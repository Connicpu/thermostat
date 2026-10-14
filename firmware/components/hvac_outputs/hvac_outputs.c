#include "core.h"
#include "hvac_outputs.h"
#include "board.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include <string.h>

static const char *kTag = "hvac";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static HvacState s_state; /* last commanded (after interlock and fault hold) */
static U32 s_faults;
static U8 s_cfgFault;    /* pin could not be configured: permanent until reboot */
static U8 s_driveFault;  /* latched: bit0 W, bit1 Y, bit2 G - pad did not follow / write failed */
static U8 s_mismatch[3]; /* consecutive settled readback mismatches per pin */
static I64 s_drivenAtUs; /* last time drive() changed the latches */
static I64 s_latchedAtUs;
static U8 s_retests;

static const gpio_num_t kPins[3] = {BOARD_GPIO_DRV_W, BOARD_GPIO_DRV_Y, BOARD_GPIO_DRV_G};

/**
 * Set the three pins to s: lowered pins first, then raised in the order G, W, Y, and Y is never
 * raised if G failed (no compressor without the blower). Caller holds s_lock.
 *
 * Returns bitmask (bit0 W, bit1 Y, bit2 G) of pins whose gpio_set_level() failed. An error code
 * is authoritative, so there is no debounce.
 */
static U8 Drive(HvacState s)
{
    U8 bad = 0;
    const bool v[3] = {s.w, s.y, s.g};
    /* Drop first, raise after, so a W->Y swap can never overlap even for
     * one instruction. (The MOC3063 zero-cross opto + triac would hold
     * conduction until the next zero crossing anyway.) Raise G before Y and
     * never raise Y if G could not be driven: no compressor without the blower. */
    for (int i = 0; i < 3; i++)
        if (!v[i] && gpio_set_level(kPins[i], 0) != ESP_OK)
            bad |= 1u << i;
    static const int kRaise[3] = {2, 0, 1}; /* G, W, Y */
    for (int k = 0; k < 3; k++)
    {
        int i = kRaise[k];
        if (!v[i])
            continue;
        if (i == 1 && (bad & 4u))
            continue;
        if (gpio_set_level(kPins[i], 1) != ESP_OK)
            bad |= 1u << i;
    }
    return bad;
}

/** Returns the pad levels read back through the input buffer (the pins are INPUT_OUTPUT). */
static HvacState Readback(void)
{
    return (HvacState){.w = gpio_get_level(BOARD_GPIO_DRV_W) != 0,
                       .y = gpio_get_level(BOARD_GPIO_DRV_Y) != 0,
                       .g = gpio_get_level(BOARD_GPIO_DRV_G) != 0};
}

/** Returns bitmask (bit0 W, bit1 Y, bit2 G) of outputs that differ between a and b. */
static U8 Diff(HvacState a, HvacState b)
{
    return (U8)((a.w != b.w) | ((a.y != b.y) << 1) | ((a.g != b.g) << 2));
}

/** Configure the pins, all off. See hvac_outputs.h. */
esp_err_t HvacOutputsInit(void)
{
    const gpio_num_t *pPins = kPins;
    esp_err_t ret = ESP_OK;
    for (int i = 0; i < 3; i++)
    {
        gpio_set_level(pPins[i], 0); /* latch 0 before enabling the driver */
        gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << pPins[i],
            .mode = GPIO_MODE_INPUT_OUTPUT,       /* input buffer on: lets us read the pad back */
            .pull_down_en = GPIO_PULLDOWN_ENABLE, /* belt + braces with the 10k on board */
        };
        esp_err_t e = gpio_config(&cfg);
        if (e != ESP_OK)
        {
            /* No reboot loop: the 10k pull-down keeps this triac off; latch an output fault. */
            ESP_LOGE(kTag, "EVT output fault: config pin %d failed (%s)", pPins[i], esp_err_to_name(e));
            s_cfgFault |= 1u << i;
            ret = e;
            continue;
        }
        gpio_set_level(pPins[i], 0);
        /* Keep the normal (awake) pad config during auto light sleep, so
         * an output that is on stays on and one that is off stays off. */
        gpio_sleep_sel_dis(pPins[i]);
    }
    s_state = (HvacState){0};
    s_drivenAtUs = esp_timer_get_time();
    return ret;
}

/**
 * Latch an output fault for the pins in `bad`: logs an EVT line once per newly faulted pin and
 * records the time of the first latch (start of the HVAC_RETEST_MS wait).
 *
 * `bad` is the bitmask of faulted pins (bit0 W, bit1 Y, bit2 G); pad levels seen (for the log);
 * `cmd` is what was commanded (for the log); `pWhy` is a short reason for the log; `now` is
 * esp_timer time, µs.
 */
static void Latch(U8 bad, HvacState pad, HvacState cmd, const char *pWhy, I64 now)
{
    if ((bad & ~s_driveFault) != 0)
        ESP_LOGE(
            kTag,
            "EVT output fault: %s; pads W=%d Y=%d G=%d, commanded W=%d Y=%d G=%d (mask 0x%x) - all off, latched",
            pWhy,
            pad.w,
            pad.y,
            pad.g,
            cmd.w,
            cmd.y,
            cmd.g,
            bad);
    if (!s_driveFault)
        s_latchedAtUs = now;
    s_driveFault |= bad;
}

/** Verify the previous command, re-test a latched fault, then interlock and drive. See hvac_outputs.h. */
HvacState HvacOutputsSet(HvacState req)
{
    const I64 now = esp_timer_get_time();

    /* 1. Verify the PREVIOUS command, now that the pads have settled (the C6 input
     *    path has a synchroniser delay, and the opto LED / pull-downs need time):
     *    only when the last change is at least HVAC_SETTLE_US old, and only fault
     *    after HVAC_MISMATCH_N consecutive mismatches, so one stale read cannot
     *    latch a fault until reboot. */
    if (now - s_drivenAtUs >= HVAC_SETTLE_US)
    {
        HvacState pad = Readback();
        U8 m = Diff(pad, s_state) & (U8)~s_cfgFault;
        U8 bad = 0;
        for (int i = 0; i < 3; i++)
        {
            if (m & (1u << i))
            {
                if (s_mismatch[i] < 255)
                    s_mismatch[i]++;
            }
            else
                s_mismatch[i] = 0;
            if (s_mismatch[i] >= HVAC_MISMATCH_N)
                bad |= 1u << i;
        }
        if (bad)
            Latch(bad, pad, s_state, "readback", now);

        /* 2. Timed re-test. Everything has been held off since the latch, and
         *    HVAC_RETEST_MS exceeds every min-off / changeover time, so letting
         *    control drive again cannot short-cycle. Only retried while the pads
         *    agree with "all off"; gives up after HVAC_RETEST_MAX tries. */
        if (s_driveFault && !m && s_retests < HVAC_RETEST_MAX && now - s_latchedAtUs >= (I64)HVAC_RETEST_MS * 1000)
        {
            s_retests++;
            ESP_LOGW(kTag, "EVT output retest %u/%u (mask 0x%x)", s_retests, HVAC_RETEST_MAX, s_driveFault);
            s_driveFault = 0;
            memset(s_mismatch, 0, sizeof s_mismatch);
        }
    }

    /* 3. Drive the new command. */
    bool fault = false;
    HvacState ok = HvacInterlock(req, &fault);
    if (s_driveFault || s_cfgFault)
        ok = (HvacState){0}; /* output fault: hold everything off */
    portENTER_CRITICAL(&s_lock);
    bool changed = Diff(ok, s_state) != 0;
    U8 bad = Drive(ok);
    s_state = ok;
    if (fault)
        s_faults++;
    portEXIT_CRITICAL(&s_lock);
    if (changed || bad)
        s_drivenAtUs = now;
    if (fault)
        ESP_LOGE(kTag, "INTERLOCK: W+Y requested together - both dropped");
    if (bad)
    {
        Latch(bad, ok, ok, "gpio write failed", now);
        portENTER_CRITICAL(&s_lock);
        Drive((HvacState){0}); /* best effort: everything we can still drive goes low */
        s_state = (HvacState){0};
        portEXIT_CRITICAL(&s_lock);
    }
    return s_state;
}

/** See hvac_outputs.h. */
U8 HvacOutputsDriveFault(void)
{
    return s_driveFault | s_cfgFault;
}

/** See hvac_outputs.h. */
void HvacOutputsAllOff(void)
{
    HvacOutputsSet((HvacState){0});
}

/** See hvac_outputs.h. */
U32 HvacOutputsInterlockFaults(void)
{
    return s_faults;
}
