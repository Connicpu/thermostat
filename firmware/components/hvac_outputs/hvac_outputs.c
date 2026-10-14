#include "hvac_outputs.h"
#include "board.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "hvac";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static hvac_state_t s_state;          /* last commanded (after interlock and fault hold) */
static uint32_t s_faults;
static uint8_t s_cfg_fault;           /* pin could not be configured: permanent until reboot */
static uint8_t s_drive_fault;         /* latched: bit0 W, bit1 Y, bit2 G - pad did not follow / write failed */
static uint8_t s_mismatch[3];         /* consecutive settled readback mismatches per pin */
static int64_t s_driven_at_us;        /* last time drive() changed the latches */
static int64_t s_latched_at_us;
static uint8_t s_retests;

static const gpio_num_t PINS[3] = {BOARD_GPIO_DRV_W, BOARD_GPIO_DRV_Y, BOARD_GPIO_DRV_G};

/* returns a bitmask of pins whose write failed (an error code is authoritative: no debounce) */
static uint8_t drive(hvac_state_t s)
{
    uint8_t bad = 0;
    const bool v[3] = {s.w, s.y, s.g};
    /* Drop first, raise after, so a W->Y swap can never overlap even for
     * one instruction. (The MOC3063 zero-cross opto + triac would hold
     * conduction until the next zero crossing anyway.) Raise G before Y and
     * never raise Y if G could not be driven: no compressor without the blower. */
    for (int i = 0; i < 3; i++) if (!v[i] && gpio_set_level(PINS[i], 0) != ESP_OK) bad |= 1u << i;
    static const int RAISE[3] = {2, 0, 1};   /* G, W, Y */
    for (int k = 0; k < 3; k++) {
        int i = RAISE[k];
        if (!v[i]) continue;
        if (i == 1 && (bad & 4u)) continue;
        if (gpio_set_level(PINS[i], 1) != ESP_OK) bad |= 1u << i;
    }
    return bad;
}

/* pad levels as read back through the input buffer (pins are INPUT_OUTPUT) */
static hvac_state_t readback(void)
{
    return (hvac_state_t){.w = gpio_get_level(BOARD_GPIO_DRV_W) != 0, .y = gpio_get_level(BOARD_GPIO_DRV_Y) != 0,
                          .g = gpio_get_level(BOARD_GPIO_DRV_G) != 0};
}

static uint8_t diff(hvac_state_t a, hvac_state_t b)
{
    return (uint8_t)((a.w != b.w) | ((a.y != b.y) << 1) | ((a.g != b.g) << 2));
}

esp_err_t hvac_outputs_init(void)
{
    const gpio_num_t *pins = PINS;
    esp_err_t ret = ESP_OK;
    for (int i = 0; i < 3; i++) {
        gpio_set_level(pins[i], 0); /* latch 0 before enabling the driver */
        gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << pins[i],
            .mode = GPIO_MODE_INPUT_OUTPUT,       /* input buffer on: lets us read the pad back */
            .pull_down_en = GPIO_PULLDOWN_ENABLE, /* belt + braces with the 10k on board */
        };
        esp_err_t e = gpio_config(&cfg);
        if (e != ESP_OK) {
            /* No reboot loop: the 10k pull-down keeps this triac off; latch an output fault. */
            ESP_LOGE(TAG, "EVT output fault: config pin %d failed (%s)", pins[i], esp_err_to_name(e));
            s_cfg_fault |= 1u << i;
            ret = e;
            continue;
        }
        gpio_set_level(pins[i], 0);
        /* Keep the normal (awake) pad config during auto light sleep, so
         * an output that is on stays on and one that is off stays off. */
        gpio_sleep_sel_dis(pins[i]);
    }
    s_state = (hvac_state_t){0};
    s_driven_at_us = esp_timer_get_time();
    return ret;
}

static void latch(uint8_t bad, hvac_state_t pad, hvac_state_t cmd, const char *why, int64_t now)
{
    if ((bad & ~s_drive_fault) != 0)
        ESP_LOGE(TAG, "EVT output fault: %s; pads W=%d Y=%d G=%d, commanded W=%d Y=%d G=%d (mask 0x%x) - all off, latched",
                 why, pad.w, pad.y, pad.g, cmd.w, cmd.y, cmd.g, bad);
    if (!s_drive_fault) s_latched_at_us = now;
    s_drive_fault |= bad;
}

hvac_state_t hvac_outputs_set(hvac_state_t req)
{
    const int64_t now = esp_timer_get_time();

    /* 1. Verify the PREVIOUS command, now that the pads have settled (the C6 input
     *    path has a synchroniser delay, and the opto LED / pull-downs need time):
     *    only when the last change is at least HVAC_SETTLE_US old, and only fault
     *    after HVAC_MISMATCH_N consecutive mismatches, so one stale read cannot
     *    latch a fault until reboot. */
    if (now - s_driven_at_us >= HVAC_SETTLE_US) {
        hvac_state_t pad = readback();
        uint8_t m = diff(pad, s_state) & (uint8_t)~s_cfg_fault;
        uint8_t bad = 0;
        for (int i = 0; i < 3; i++) {
            if (m & (1u << i)) { if (s_mismatch[i] < 255) s_mismatch[i]++; }
            else s_mismatch[i] = 0;
            if (s_mismatch[i] >= HVAC_MISMATCH_N) bad |= 1u << i;
        }
        if (bad) latch(bad, pad, s_state, "readback", now);

        /* 2. Timed re-test. Everything has been held off since the latch, and
         *    HVAC_RETEST_MS exceeds every min-off / changeover time, so letting
         *    control drive again cannot short-cycle. Only retried while the pads
         *    agree with "all off"; gives up after HVAC_RETEST_MAX tries. */
        if (s_drive_fault && !m && s_retests < HVAC_RETEST_MAX &&
            now - s_latched_at_us >= (int64_t)HVAC_RETEST_MS * 1000) {
            s_retests++;
            ESP_LOGW(TAG, "EVT output retest %u/%u (mask 0x%x)", s_retests, HVAC_RETEST_MAX, s_drive_fault);
            s_drive_fault = 0;
            memset(s_mismatch, 0, sizeof s_mismatch);
        }
    }

    /* 3. Drive the new command. */
    bool fault = false;
    hvac_state_t ok = hvac_interlock(req, &fault);
    if (s_drive_fault || s_cfg_fault) ok = (hvac_state_t){0};   /* output fault: hold everything off */
    portENTER_CRITICAL(&s_lock);
    bool changed = diff(ok, s_state) != 0;
    uint8_t bad = drive(ok);
    s_state = ok;
    if (fault) s_faults++;
    portEXIT_CRITICAL(&s_lock);
    if (changed || bad) s_driven_at_us = now;
    if (fault) ESP_LOGE(TAG, "INTERLOCK: W+Y requested together - both dropped");
    if (bad) {
        latch(bad, ok, ok, "gpio write failed", now);
        portENTER_CRITICAL(&s_lock);
        drive((hvac_state_t){0});        /* best effort: everything we can still drive goes low */
        s_state = (hvac_state_t){0};
        portEXIT_CRITICAL(&s_lock);
    }
    return s_state;
}

uint8_t hvac_outputs_drive_fault(void) { return s_drive_fault | s_cfg_fault; }

hvac_state_t hvac_outputs_get(void)
{
    portENTER_CRITICAL(&s_lock);
    hvac_state_t s = s_state;
    portEXIT_CRITICAL(&s_lock);
    return s;
}

void hvac_outputs_all_off(void)
{
    hvac_outputs_set((hvac_state_t){0});
}

uint32_t hvac_outputs_interlock_faults(void)
{
    return s_faults;
}
