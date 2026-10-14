/* periph.c - HOOTL hardware models: GPIO + trace, esp_timer, SHT40 on I2C (with a
 * simple room model), SSD1680 e-paper on SPI (framebuffer capture), NVS on a
 * persisted store, OTA slots/otadata, STWD100, task watchdog, chip reset.
 * All fault injection is "sticky" (kv store) so it survives sim reboots,
 * like a real hardware fault would. */
#include "sim.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_ota_ops.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define K(name) sim_kv_get(name, 0)
static void stwd_poll_edge(void);

const char *esp_err_to_name(esp_err_t c)
{
    switch (c) {
    case ESP_OK: return "ESP_OK";
    case ESP_FAIL: return "ESP_FAIL";
    case ESP_ERR_NO_MEM: return "ESP_ERR_NO_MEM";
    case ESP_ERR_INVALID_ARG: return "ESP_ERR_INVALID_ARG";
    case ESP_ERR_INVALID_STATE: return "ESP_ERR_INVALID_STATE";
    case ESP_ERR_INVALID_SIZE: return "ESP_ERR_INVALID_SIZE";
    case ESP_ERR_NOT_FOUND: return "ESP_ERR_NOT_FOUND";
    case ESP_ERR_TIMEOUT: return "ESP_ERR_TIMEOUT";
    case ESP_ERR_INVALID_CRC: return "ESP_ERR_INVALID_CRC";
    case ESP_ERR_NVS_NOT_FOUND: return "ESP_ERR_NVS_NOT_FOUND";
    case ESP_ERR_NVS_NOT_ENOUGH_SPACE: return "ESP_ERR_NVS_NOT_ENOUGH_SPACE";
    case ESP_ERR_NVS_NO_FREE_PAGES: return "ESP_ERR_NVS_NO_FREE_PAGES";
    case ESP_ERR_NVS_NEW_VERSION_FOUND: return "ESP_ERR_NVS_NEW_VERSION_FOUND";
    case ESP_ERR_NVS_NOT_INITIALIZED: return "ESP_ERR_NVS_NOT_INITIALIZED";
    case ESP_ERR_NVS_INVALID_LENGTH: return "ESP_ERR_NVS_INVALID_LENGTH";
    case ESP_ERR_OTA_VALIDATE_FAILED: return "ESP_ERR_OTA_VALIDATE_FAILED";
    case ESP_ERR_OTA_ROLLBACK_FAILED: return "ESP_ERR_OTA_ROLLBACK_FAILED";
    default: return "ESP_ERR_?";
    }
}

/* ================= GPIO ================= */
#define NPIN 24
static int s_out[NPIN];        /* output latch */
static int s_mode[NPIN];
static int s_ext[NPIN];        /* externally driven level for inputs */
static int s_pad_last[NPIN];

static int pad(int n)
{
    char k[32];
    snprintf(k, sizeof k, "gpio.stuck.%d", n);
    int64_t st = sim_kv_get(k, -1);
    if (st >= 0) return (int)st;
    if (s_mode[n] & GPIO_MODE_OUTPUT) return s_out[n];
    return n == 11 ? 1 : 0;   /* board: 10k pull-up on STWD100 ~EN (disarmed), pull-downs on the triac drivers */
}

static bool traced(int n) { return n == 0 || n == 1 || n == 2 || n == 8 || n == 10 || n == 11; }

static void trace(int n)
{
    if (!traced(n)) return;
    int p = pad(n);
    if (p != s_pad_last[n]) { s_pad_last[n] = p; sim_emit("GPIO %d %d", n, p); }
}

static void trace_all(void) { for (int i = 0; i < NPIN; i++) trace(i); }

esp_err_t gpio_config(const gpio_config_t *c)
{
    for (int n = 0; n < NPIN; n++) {
        if (!(c->pin_bit_mask & (1ULL << n))) continue;
        char k[32];
        snprintf(k, sizeof k, "gpio.cfgfail.%d", n);
        if (K(k)) return ESP_FAIL;
        s_mode[n] = c->mode;
        trace(n);
    }
    return ESP_OK;
}

esp_err_t gpio_set_level(gpio_num_t n, uint32_t level)
{
    if (n < 0 || n >= NPIN) return ESP_ERR_INVALID_ARG;
    char k[32];
    snprintf(k, sizeof k, "gpio.setfail.%d", n);
    if (K(k)) return ESP_FAIL;
    s_out[n] = level ? 1 : 0;
    trace(n);
    if (n == 10 || n == 11) stwd_poll_edge();
    return ESP_OK;
}

int epd_busy_level(void);
int btn_level(int n);

int gpio_get_level(gpio_num_t n)
{
    if (n < 0 || n >= NPIN) return 0;
    if (n == 23) return epd_busy_level();
    if (n == 3 || n == 4 || n == 5) return btn_level(n);
    if (s_mode[n] & GPIO_MODE_INPUT) return pad(n);    /* readback needs the input buffer enabled */
    if (s_mode[n] & GPIO_MODE_OUTPUT) return 0;        /* output-only pad: IN register reads 0 on the C6 */
    return s_ext[n];
}

esp_err_t gpio_sleep_sel_dis(gpio_num_t n) { (void)n; return ESP_OK; }

/* ================= buttons ================= */
/* pin 3 = MODE, 4 = UP, 5 = DOWN; active low. kv btn.<pin> = 1 pressed. Bounce: random toggles until bounce_until. */
static uint64_t s_bounce_until[NPIN];
int btn_level(int n)
{
    char k[32];
    snprintf(k, sizeof k, "btn.%d", n);
    int pressed = (int)K(k);
    if (sim_now_us() < s_bounce_until[n]) pressed = (int)(sim_rand() & 1);
    return pressed ? 0 : 1;
}

static int btn_pin(const char *name)
{
    if (!strcmp(name, "mode") || !strcmp(name, "m")) return 3;
    if (!strcmp(name, "up")) return 4;
    if (!strcmp(name, "down")) return 5;
    return -1;
}

/* ================= esp_timer ================= */
struct sim_timer { esp_timer_cb_t cb; void *arg; uint64_t period, next; bool active; };
static struct sim_timer s_timers[16];
static int s_ntimers;
static TaskHandle_t s_timer_task;

int64_t esp_timer_get_time(void) { return (int64_t)sim_now_us(); }

esp_err_t esp_timer_create(const esp_timer_create_args_t *a, esp_timer_handle_t *out)
{
    if (K("fault.esp_timer") || s_ntimers >= 16) return ESP_ERR_NO_MEM;
    struct sim_timer *t = &s_timers[s_ntimers++];
    t->cb = a->callback; t->arg = a->arg;
    *out = t;
    return ESP_OK;
}
static esp_err_t start(esp_timer_handle_t t, uint64_t us, uint64_t period)
{
    t->period = period; t->next = sim_now_us() + us; t->active = true;
    if (s_timer_task) xTaskNotifyGive(s_timer_task);
    return ESP_OK;
}
esp_err_t esp_timer_start_periodic(esp_timer_handle_t t, uint64_t p) { return start(t, p, p); }
esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t us) { return start(t, us, 0); }
esp_err_t esp_timer_stop(esp_timer_handle_t t) { t->active = false; return ESP_OK; }

static void timer_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint64_t now = sim_now_us(), next = UINT64_MAX;
        for (int i = 0; i < s_ntimers; i++) {
            struct sim_timer *t = &s_timers[i];
            if (!t->active) continue;
            if (t->next <= now) {
                if (t->period) t->next += t->period; else t->active = false;
                t->cb(t->arg);
            }
            if (t->active && t->next < next) next = t->next;
        }
        now = sim_now_us();
        uint32_t wait = next == UINT64_MAX ? portMAX_DELAY : (uint32_t)((next > now ? next - now : 0) / 1000);
        ulTaskNotifyTake(pdTRUE, wait ? wait : 1);
    }
}

/* ================= SHT40 + room model ================= */
struct sim_i2c_bus { int dummy; };
struct sim_i2c_dev { uint16_t addr; };
static struct sim_i2c_bus s_bus;
static struct sim_i2c_dev s_sht;
static uint64_t s_meas_at;
static bool s_meas_pending;
static double s_room_c;           /* room temperature, °C */
static uint64_t s_room_at;
static uint16_t s_stuck_t, s_stuck_h;
static bool s_have_stuck;

static void room_update(void)
{
    uint64_t now = sim_now_us();
    double dt_min = (double)(now - s_room_at) / 60e6;
    s_room_at = now;
    if (!K("room.model")) { s_room_c = (double)sim_kv_get("room.t", 2100) / 100.0; return; }
    /* furnace / AC at full power + leak towards ambient. Rates in 0.01 °C/min. */
    double amb = (double)sim_kv_get("room.ambient", 1500) / 100.0;
    double heat = pad(0) ? (double)sim_kv_get("room.heat", 50) / 100.0 : 0;
    double cool = pad(1) ? (double)sim_kv_get("room.cool", 40) / 100.0 : 0;
    double leak = (double)sim_kv_get("room.leak", 1) / 100.0;   /* fraction per minute */
    s_room_c += (heat - cool + (amb - s_room_c) * leak) * dt_min;
    sim_kv_set("room.t", (int64_t)llround(s_room_c * 100.0));
}

static uint8_t crc8(const uint8_t *d, int n)
{
    uint8_t c = 0xFF;
    for (int i = 0; i < n; i++) { c ^= d[i]; for (int b = 0; b < 8; b++) c = (uint8_t)(c & 0x80 ? (c << 1) ^ 0x31 : c << 1); }
    return c;
}

esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *c, i2c_master_bus_handle_t *out)
{
    (void)c;
    if (K("sht.busfail")) return ESP_FAIL;
    *out = &s_bus;
    return ESP_OK;
}
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t b, const i2c_device_config_t *c, i2c_master_dev_handle_t *out)
{
    (void)b;
    s_sht.addr = c->device_address;
    *out = &s_sht;
    return ESP_OK;
}
esp_err_t i2c_master_bus_reset(i2c_master_bus_handle_t b) { (void)b; sim_emit("I2C bus reset"); sim_kv_set("sht.resets", K("sht.resets") + 1); return ESP_OK; }

/* sht.mode: 0 ok, 1 nak, 2 timeout (bus stuck low), 3 crc, 4 stuck value, 5 intermittent nak (sht.p %), 6 absent */
static esp_err_t sht_fault(int timeout_ms)
{
    int m = (int)K("sht.mode");
    if (m == 1 || m == 6) return ESP_FAIL;
    if (m == 2) { vTaskDelay(pdMS_TO_TICKS(timeout_ms)); return ESP_ERR_TIMEOUT; }
    if (m == 5 && (int)(sim_rand() % 100) < (int)sim_kv_get("sht.p", 50)) return ESP_FAIL;
    return ESP_OK;
}

esp_err_t i2c_master_transmit(i2c_master_dev_handle_t d, const uint8_t *buf, size_t n, int timeout_ms)
{
    if (d->addr != 0x44 || n < 1) return ESP_FAIL;
    esp_err_t e = sht_fault(timeout_ms);
    if (e != ESP_OK) return e;
    if (buf[0] == 0xFD) { s_meas_pending = true; s_meas_at = sim_now_us(); }
    return ESP_OK;
}

esp_err_t i2c_master_receive(i2c_master_dev_handle_t d, uint8_t *buf, size_t n, int timeout_ms)
{
    if (d->addr != 0x44 || n != 6) return ESP_FAIL;
    esp_err_t e = sht_fault(timeout_ms);
    if (e != ESP_OK) return e;
    if (!s_meas_pending || sim_now_us() - s_meas_at < 8300) return ESP_FAIL;   /* NAK while measuring */
    s_meas_pending = false;
    room_update();
    double t = s_room_c, rh = (double)sim_kv_get("room.rh", 4500) / 100.0;
    uint16_t st = (uint16_t)fmax(0, fmin(65535, llround((t + 45.0) * 65535.0 / 175.0)));
    uint16_t sh = (uint16_t)fmax(0, fmin(65535, llround((rh + 6.0) * 65535.0 / 125.0)));
    if (sim_kv_get("sht.noise", 1)) {   /* a real SHT40 is never perfectly still: +-2 LSB T, +-8 LSB RH */
        st = (uint16_t)(st + (int)(sim_rand() % 5) - 2);
        sh = (uint16_t)(sh + (int)(sim_rand() % 17) - 8);
    }
    if (K("sht.mode") == 4) {
        if (!s_have_stuck) { s_have_stuck = true; s_stuck_t = (uint16_t)sim_kv_get("sht.stuck_t", st); s_stuck_h = sh; sim_kv_set("sht.stuck_t", st); }
        st = s_stuck_t; sh = s_stuck_h;
    } else s_have_stuck = false;
    buf[0] = st >> 8; buf[1] = st & 0xFF; buf[2] = crc8(buf, 2);
    buf[3] = sh >> 8; buf[4] = sh & 0xFF; buf[5] = crc8(buf + 3, 2);
    if (K("sht.mode") == 3) buf[2] ^= 0x5A;
    sim_kv_set("sht.reads", K("sht.reads") + 1);
    return ESP_OK;
}

/* ================= SSD1680 e-paper ================= */
#define EPD_W 128
#define EPD_H 296
#define EPD_BYTES (EPD_W / 8 * EPD_H)
struct sim_spi_dev { int dummy; };
static struct sim_spi_dev s_epd;
static uint8_t s_ram24[EPD_BYTES], s_ram26[EPD_BYTES], s_glass[EPD_BYTES];
static uint8_t s_cmd, s_upd_mode;
static size_t s_cursor;
static uint64_t s_busy_until;
static int s_refreshes;

int epd_busy_level(void)
{
    if (K("epd.busy_stuck")) return 1;
    return sim_now_us() < s_busy_until;
}

esp_err_t spi_bus_initialize(spi_host_device_t h, const spi_bus_config_t *c, spi_dma_chan_t dma)
{
    (void)h; (void)c; (void)dma;
    return K("epd.init_fail") ? ESP_ERR_INVALID_STATE : ESP_OK;
}
esp_err_t spi_bus_add_device(spi_host_device_t h, const spi_device_interface_config_t *c, spi_device_handle_t *out)
{
    (void)h; (void)c;
    *out = &s_epd;
    return ESP_OK;
}

static void epd_dump(const char *why)
{
    char path[512];
    snprintf(path, sizeof path, "%s/fb_%03d.pbm", sim_outdir(), s_refreshes);
    FILE *f = fopen(path, "wb");
    if (f) {
        /* RAM: MSB first, 1 = white. PBM: 1 = black. Panel is portrait 128x296. */
        fprintf(f, "P4\n%d %d\n", EPD_W, EPD_H);
        for (int i = 0; i < EPD_BYTES; i++) fputc(~s_glass[i] & 0xFF, f);
        fclose(f);
    }
    uint32_t crc = 2166136261u; int ink = 0;
    for (int i = 0; i < EPD_BYTES; i++) { crc = (crc ^ s_glass[i]) * 16777619u; ink += __builtin_popcount((unsigned)(~s_glass[i] & 0xFF)); }
    sim_emit("EPD refresh n=%d mode=0x%02x ink=%d crc=%08x file=%s why=%s", s_refreshes, s_upd_mode, ink, crc, path, why);
}

esp_err_t spi_device_polling_transmit(spi_device_handle_t d, spi_transaction_t *t)
{
    (void)d;
    if (K("epd.spi_fail")) return ESP_FAIL;
    if (K("epd.hang")) vTaskDelay(portMAX_DELAY);       /* SPI/DMA never completes */
    const uint8_t *p = t->tx_buffer;
    size_t n = t->length / 8;
    bool is_data = s_out[21] != 0;   /* DC: 0 = command */
    if (!is_data) {
        if (n != 1) return ESP_ERR_INVALID_ARG;
        s_cmd = p[0];
        if (s_cmd == 0x12) s_busy_until = sim_now_us() + 3000;      /* SW reset */
        if (s_cmd == 0x24 || s_cmd == 0x26) s_cursor = 0;
        if (s_cmd == 0x20) {                                        /* master activation */
            memcpy(s_glass, s_ram24, sizeof s_glass);
            s_busy_until = sim_now_us() + (s_upd_mode == 0xFF ? 600000 : 2000000);
            s_refreshes++;
            epd_dump("refresh");
        }
        return ESP_OK;
    }
    for (size_t i = 0; i < n; i++) {
        if (s_cmd == 0x24 && s_cursor < EPD_BYTES) s_ram24[s_cursor++] = p[i];
        else if (s_cmd == 0x26 && s_cursor < EPD_BYTES) s_ram26[s_cursor++] = p[i];
        else if (s_cmd == 0x22 && i == 0) s_upd_mode = p[i];
    }
    return ESP_OK;
}

/* ================= NVS ================= */
#define NVS_MAX 64
static struct { bool used; char ns[16], key[16]; uint8_t v[512]; size_t n; } s_nvs[NVS_MAX];
static bool s_nvs_inited;
static struct { char ns[16]; bool rw; } s_handles[8];

void sim_nvs_save(FILE *f)
{
    for (int i = 0; i < NVS_MAX; i++) {
        if (!s_nvs[i].used) continue;
        fprintf(f, "nvs %s %s ", s_nvs[i].ns, s_nvs[i].key);
        for (size_t j = 0; j < s_nvs[i].n; j++) fprintf(f, "%02x", s_nvs[i].v[j]);
        fprintf(f, "%s\n", s_nvs[i].n ? "" : "-");
    }
}

static int nvs_find(const char *ns, const char *key, bool create)
{
    for (int i = 0; i < NVS_MAX; i++) if (s_nvs[i].used && !strcmp(s_nvs[i].ns, ns) && !strcmp(s_nvs[i].key, key)) return i;
    if (!create) return -1;
    for (int i = 0; i < NVS_MAX; i++) if (!s_nvs[i].used) {
        s_nvs[i].used = true; snprintf(s_nvs[i].ns, 16, "%s", ns); snprintf(s_nvs[i].key, 16, "%s", key); s_nvs[i].n = 0; return i;
    }
    return -1;
}

static void nvs_put_hex(const char *ns, const char *key, const char *hex)
{
    int i = nvs_find(ns, key, true);
    if (i < 0) return;
    s_nvs[i].n = 0;
    if (!strcmp(hex, "-")) return;
    for (size_t j = 0; hex[2 * j] && hex[2 * j + 1] && j < sizeof s_nvs[0].v; j++) {
        unsigned b; sscanf(hex + 2 * j, "%2x", &b); s_nvs[i].v[j] = (uint8_t)b; s_nvs[i].n = j + 1;
    }
}

bool sim_nvs_load_line(const char *line)
{
    char ns[16], key[16], hex[1100];
    if (sscanf(line, "nvs %15s %15s %1099s", ns, key, hex) != 3) return false;
    nvs_put_hex(ns, key, hex);
    return true;
}

/* nvs.init: 0 ok, 1 no_free_pages until erased, 2 new_version until erased, 3 partition missing (NOT_FOUND, erase fails too), 4 generic fail */
esp_err_t nvs_flash_init(void)
{
    switch (K("nvs.init")) {
    case 1: return ESP_ERR_NVS_NO_FREE_PAGES;
    case 2: return ESP_ERR_NVS_NEW_VERSION_FOUND;
    case 3: return ESP_ERR_NOT_FOUND;
    case 4: return ESP_FAIL;
    default: s_nvs_inited = true; return ESP_OK;
    }
}
esp_err_t nvs_flash_erase(void)
{
    int m = (int)K("nvs.init");
    if (m == 3) return ESP_ERR_NOT_FOUND;
    if (m == 4) return ESP_FAIL;
    memset(s_nvs, 0, sizeof s_nvs);
    sim_kv_set("nvs.init", 0);
    sim_emit("NVS erased");
    return ESP_OK;
}
esp_err_t nvs_open(const char *ns, nvs_open_mode_t mode, nvs_handle_t *out)
{
    if (!s_nvs_inited) return ESP_ERR_NVS_NOT_INITIALIZED;
    if (K("nvs.open_fail")) return ESP_FAIL;
    if (mode == NVS_READONLY) {
        bool any = false;
        for (int i = 0; i < NVS_MAX; i++) if (s_nvs[i].used && !strcmp(s_nvs[i].ns, ns)) any = true;
        if (!any) return ESP_ERR_NVS_NOT_FOUND;
    }
    for (int h = 0; h < 8; h++) if (!s_handles[h].ns[0]) {
        snprintf(s_handles[h].ns, 16, "%s", ns); s_handles[h].rw = mode == NVS_READWRITE; *out = (nvs_handle_t)(h + 1); return ESP_OK;
    }
    return ESP_ERR_NO_MEM;
}
void nvs_close(nvs_handle_t h) { if (h >= 1 && h <= 8) s_handles[h - 1].ns[0] = 0; }
static const char *hns(nvs_handle_t h) { return (h >= 1 && h <= 8) ? s_handles[h - 1].ns : ""; }

esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *len)
{
    if (K("nvs.read_fail")) return ESP_FAIL;
    int i = nvs_find(hns(h), key, false);
    if (i < 0) return ESP_ERR_NVS_NOT_FOUND;
    if (!out) { *len = s_nvs[i].n; return ESP_OK; }
    if (*len < s_nvs[i].n) { *len = s_nvs[i].n; return ESP_ERR_NVS_INVALID_LENGTH; }
    memcpy(out, s_nvs[i].v, s_nvs[i].n);
    *len = s_nvs[i].n;
    return ESP_OK;
}
static esp_err_t write_check(nvs_handle_t h)
{
    if (h < 1 || h > 8 || !s_handles[h - 1].rw) return ESP_ERR_INVALID_ARG;
    if (K("nvs.write_fail")) return ESP_FAIL;
    if (K("nvs.full")) return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *v, size_t len)
{
    esp_err_t e = write_check(h);
    if (e != ESP_OK) return e;
    int i = nvs_find(hns(h), key, true);
    if (i < 0 || len > sizeof s_nvs[0].v) return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    memcpy(s_nvs[i].v, v, len); s_nvs[i].n = len;
    sim_emit("NVS set %s/%s %zu bytes", hns(h), key, len);
    return ESP_OK;
}
esp_err_t nvs_get_u8(nvs_handle_t h, const char *key, uint8_t *out)
{
    size_t n = 1;
    if (K("nvs.read_fail")) return ESP_FAIL;
    int i = nvs_find(hns(h), key, false);
    if (i < 0) return ESP_ERR_NVS_NOT_FOUND;
    if (s_nvs[i].n != 1) return ESP_ERR_NVS_INVALID_LENGTH;
    (void)n;
    *out = s_nvs[i].v[0];
    return ESP_OK;
}
esp_err_t nvs_set_u8(nvs_handle_t h, const char *key, uint8_t v) { return nvs_set_blob(h, key, &v, 1); }
esp_err_t nvs_erase_key(nvs_handle_t h, const char *key)
{
    esp_err_t e = write_check(h);
    if (e != ESP_OK) return e;
    int i = nvs_find(hns(h), key, false);
    if (i < 0) return ESP_ERR_NVS_NOT_FOUND;
    s_nvs[i].used = false;
    return ESP_OK;
}
esp_err_t nvs_erase_all(nvs_handle_t h)
{
    esp_err_t e = write_check(h);
    if (e != ESP_OK) return e;
    for (int i = 0; i < NVS_MAX; i++) if (s_nvs[i].used && !strcmp(s_nvs[i].ns, hns(h))) s_nvs[i].used = false;
    sim_emit("NVS erase_all %s", hns(h));
    return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) { (void)h; return K("nvs.commit_fail") ? ESP_FAIL : ESP_OK; }

/* ================= OTA ================= */
/* kv: ota.run (slot running), ota.boot (slot to boot), ota.st0/1 (esp_ota_img_states_t; default VALID for 0,
 * UNDEFINED for 1), ota.img0/1 (1 = bootable image). Faults: ota.nopart, ota.begin_fail, ota.write_fail_after
 * (bytes, 0 = off), ota.setboot_fail. */
static const esp_partition_t s_part[2] = {{"ota_0", 0x20000, 0x300000, 0}, {"ota_1", 0x320000, 0x300000, 1}};
static uint32_t s_ota_written; static uint8_t s_ota_first; static bool s_ota_open;
static int64_t ost(int i) { return sim_kv_get(i ? "ota.st1" : "ota.st0", i ? ESP_OTA_IMG_UNDEFINED : ESP_OTA_IMG_VALID); }
static void set_ost(int i, int64_t v) { sim_kv_set(i ? "ota.st1" : "ota.st0", v); }
static int64_t oimg(int i) { return sim_kv_get(i ? "ota.img1" : "ota.img0", i ? 0 : 1); }

/* bootloader with CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE */
static void ota_bootloader(void)
{
    int b = (int)sim_kv_get("ota.boot", 0), o = !b;
    if (ost(b) == ESP_OTA_IMG_PENDING_VERIFY) {
        sim_emit("BOOTLOADER slot %d still PENDING_VERIFY: marking ABORTED, rolling back", b);
        set_ost(b, ESP_OTA_IMG_ABORTED);
        if (oimg(o) && (ost(o) == ESP_OTA_IMG_VALID || ost(o) == ESP_OTA_IMG_UNDEFINED)) b = o;
    } else if (ost(b) == ESP_OTA_IMG_NEW) {
        set_ost(b, ESP_OTA_IMG_PENDING_VERIFY);
    } else if (ost(b) == ESP_OTA_IMG_INVALID || ost(b) == ESP_OTA_IMG_ABORTED || !oimg(b)) {
        if (oimg(o)) b = o;
    }
    sim_kv_set("ota.boot", b);
    sim_kv_set("ota.run", b);
    sim_emit("BOOTLOADER running slot %d state %lld", b, (long long)ost(b));
}

const esp_partition_t *esp_ota_get_running_partition(void) { return &s_part[sim_kv_get("ota.run", 0)]; }
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *s)
{
    (void)s;
    if (K("ota.nopart")) return NULL;
    return &s_part[!sim_kv_get("ota.run", 0)];
}
esp_err_t esp_ota_get_state_partition(const esp_partition_t *p, esp_ota_img_states_t *st)
{
    int64_t v = ost(p->index);
    if (v == ESP_OTA_IMG_UNDEFINED) return ESP_ERR_NOT_FOUND;
    *st = (esp_ota_img_states_t)v;
    return ESP_OK;
}
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void)
{
    set_ost((int)sim_kv_get("ota.run", 0), ESP_OTA_IMG_VALID);
    sim_emit("OTA mark valid slot %lld", (long long)sim_kv_get("ota.run", 0));
    return ESP_OK;
}
bool esp_ota_check_rollback_is_possible(void)
{
    int o = !(int)sim_kv_get("ota.run", 0);
    return oimg(o) && ost(o) != ESP_OTA_IMG_INVALID && ost(o) != ESP_OTA_IMG_ABORTED;
}
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void)
{
    int r = (int)sim_kv_get("ota.run", 0), o = !r;
    if (!oimg(o) || ost(o) == ESP_OTA_IMG_INVALID || ost(o) == ESP_OTA_IMG_ABORTED) {
        sim_emit("OTA rollback impossible: no valid image in slot %d", o);
        return ESP_ERR_OTA_ROLLBACK_FAILED;
    }
    set_ost(r, ESP_OTA_IMG_INVALID);
    sim_kv_set("ota.boot", o);
    sim_reset("OTA_ROLLBACK");
}
esp_err_t esp_ota_begin(const esp_partition_t *p, size_t size, esp_ota_handle_t *out)
{
    (void)size;
    if (!p) return ESP_ERR_INVALID_ARG;
    if (K("ota.begin_fail")) return ESP_ERR_OTA_PARTITION_CONFLICT;
    s_ota_written = 0; s_ota_open = true; s_ota_first = 0;
    *out = 1;
    sim_emit("OTA begin slot %d", p->index);
    return ESP_OK;
}
esp_err_t esp_ota_write(esp_ota_handle_t h, const void *d, size_t n)
{
    if (h != 1 || !s_ota_open) return ESP_ERR_INVALID_ARG;
    int64_t lim = K("ota.write_fail_after");
    if (lim && s_ota_written + n > (uint64_t)lim) return ESP_FAIL;
    if (!s_ota_written && n) s_ota_first = ((const uint8_t *)d)[0];
    s_ota_written += (uint32_t)n;
    return ESP_OK;
}
esp_err_t esp_ota_end(esp_ota_handle_t h)
{
    if (h != 1 || !s_ota_open) return ESP_ERR_INVALID_ARG;
    s_ota_open = false;
    if (s_ota_first != 0xE9 || s_ota_written < 1024) { sim_emit("OTA end: image invalid"); return ESP_ERR_OTA_VALIDATE_FAILED; }
    sim_kv_set(sim_kv_get("ota.run", 0) ? "ota.img0" : "ota.img1", 1);
    sim_emit("OTA end ok %u bytes", s_ota_written);
    return ESP_OK;
}
esp_err_t esp_ota_abort(esp_ota_handle_t h) { (void)h; s_ota_open = false; sim_emit("OTA abort"); return ESP_OK; }
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *p)
{
    if (K("ota.setboot_fail")) return ESP_FAIL;
    sim_kv_set("ota.boot", p->index);
    set_ost(p->index, ESP_OTA_IMG_NEW);
    return ESP_OK;
}

/* ================= reset, watchdogs ================= */
void esp_restart(void) { sim_reset("SW"); }

static TaskHandle_t s_twdt_task;
static uint64_t s_twdt_fed;
static uint64_t s_hang_at = UINT64_MAX;

esp_err_t esp_task_wdt_add(TaskHandle_t t)
{
    s_twdt_task = t ? t : xTaskGetCurrentTaskHandle();
    s_twdt_fed = sim_now_us();
    /* injected app hang: read here (after the t<=boot script commands ran) */
    int64_t hb = K("hang.boots");
    if (hb > 0) { sim_kv_set("hang.boots", hb - 1); s_hang_at = sim_now_us() + (uint64_t)sim_kv_get("hang.after", 2000) * 1000u; }
    return ESP_OK;
}
esp_err_t esp_task_wdt_reset(void)
{
    if (xTaskGetCurrentTaskHandle() == s_twdt_task && sim_now_us() >= s_hang_at) {
        sim_emit("HANG app task (injected)");
        vTaskDelay(portMAX_DELAY);
    }
    s_twdt_fed = sim_now_us();
    return ESP_OK;
}

/* STWD100: ~EN low = armed; reset if no WDI edge for tWD (1.6 s typ). */
static int s_wdi_last;
static uint64_t s_wdi_edge;
static bool s_wd_armed;
#define STWD_TWD_US 1600000u

static void stwd_poll(void)
{
    int en = pad(11), wdi = pad(10);
    bool armed = en == 0;
    if (armed && !s_wd_armed) s_wdi_edge = sim_now_us();
    s_wd_armed = armed;
    if (wdi != s_wdi_last) { s_wdi_last = wdi; s_wdi_edge = sim_now_us(); }
}

uint64_t sim_periph_next_deadline_us(void)
{
    uint64_t d = UINT64_MAX;
    if (s_wd_armed) d = s_wdi_edge + STWD_TWD_US;
    if (s_twdt_task && s_twdt_fed + 5000000u < d) d = s_twdt_fed + 5000000u;
    return d;
}

void sim_periph_check(void)
{
    stwd_poll();
    uint64_t now = sim_now_us();
    if (s_wd_armed && now - s_wdi_edge >= STWD_TWD_US) sim_reset("STWD100");
    if (s_twdt_task && now - s_twdt_fed >= 5000000u) { sim_emit("task_wdt: app task did not reset the watchdog"); sim_reset("TASK_WDT"); }
}

static void stwd_poll_edge(void) { stwd_poll(); }
static void gpio_hook(void) { stwd_poll(); }

/* ================= boot ================= */
void sim_periph_boot(void)
{
    memset(s_out, 0, sizeof s_out);
    for (int i = 0; i < NPIN; i++) s_pad_last[i] = -1;
    ota_bootloader();
    trace_all();       /* reset state: every traced pad low (pull-downs), unless stuck */
    s_room_c = (double)sim_kv_get("room.t", 2100) / 100.0;
    s_room_at = sim_now_us();
    xTaskCreate(timer_task, "esp_timer", 0, NULL, 22, &s_timer_task);
}

/* ================= script commands ================= */
static int64_t num(const char *s) { return strtoll(s, NULL, 0); }

bool sim_periph_cmd(char **a, int n)
{
    gpio_hook();
    if (!strcmp(a[0], "set") && n == 3) { sim_kv_set(a[1], num(a[2])); if (!strncmp(a[1], "room.t", 6)) room_update(); return true; }
    if (!strcmp(a[0], "temp") && n == 2) { room_update(); sim_kv_set("room.t", num(a[1])); s_room_c = num(a[1]) / 100.0; return true; }
    if (!strcmp(a[0], "btn") && n >= 3) {
        int p = btn_pin(a[1]);
        if (p < 0) return false;
        char k[16]; snprintf(k, sizeof k, "btn.%d", p);
        if (!strcmp(a[2], "press")) sim_kv_set(k, 1);
        else if (!strcmp(a[2], "release")) sim_kv_set(k, 0);
        else if (!strcmp(a[2], "bounce") && n == 4) s_bounce_until[p] = sim_now_us() + (uint64_t)num(a[3]) * 1000u;
        else return false;
        return true;
    }
    if (!strcmp(a[0], "nvs") && n == 4) { nvs_put_hex(a[1], a[2], a[3]); return true; }
    if (!strcmp(a[0], "ota") && n == 3 && !strcmp(a[1], "pending")) {
        /* just "installed" an update in the running slot: bootloader would have made it PENDING_VERIFY */
        int r = (int)sim_kv_get("ota.run", 0), o = !r;
        set_ost(r, ESP_OTA_IMG_PENDING_VERIFY);
        sim_kv_set(r ? "ota.img1" : "ota.img0", 1);
        sim_kv_set(o ? "ota.img1" : "ota.img0", !strcmp(a[2], "other_valid"));
        set_ost(o, !strcmp(a[2], "other_valid") ? ESP_OTA_IMG_VALID : ESP_OTA_IMG_INVALID);
        return true;
    }
    if (!strcmp(a[0], "power") && n == 2 && !strcmp(a[1], "cycle")) sim_reset("POWERON");
    if (!strcmp(a[0], "dump")) { epd_dump("dump"); return true; }
    if (!strcmp(a[0], "end")) sim_finish(SIM_EXIT_DONE);
    return false;
}
