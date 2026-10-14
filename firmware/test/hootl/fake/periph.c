/* periph.c - HOOTL hardware models: GPIO + trace, esp_timer, SHT40 on I2C (with a
 * simple room model), SSD1680 e-paper on SPI (framebuffer capture), NVS on a
 * persisted store, OTA slots/otadata, STWD100, task watchdog, chip reset.
 * All fault injection is "sticky" (kv store) so it survives sim reboots,
 * like a real hardware fault would. */
#include "core.h"
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

#define K(name) SimKvGet(name, 0)
static void StwdPollEdge(void);

/** Name of the error codes the firmware uses (subset of the real table); "ESP_ERR_?" for others. */
const char *esp_err_to_name(esp_err_t c)
{
    switch (c)
    {
    case ESP_OK:
        return "ESP_OK";
    case ESP_FAIL:
        return "ESP_FAIL";
    case ESP_ERR_NO_MEM:
        return "ESP_ERR_NO_MEM";
    case ESP_ERR_INVALID_ARG:
        return "ESP_ERR_INVALID_ARG";
    case ESP_ERR_INVALID_STATE:
        return "ESP_ERR_INVALID_STATE";
    case ESP_ERR_INVALID_SIZE:
        return "ESP_ERR_INVALID_SIZE";
    case ESP_ERR_NOT_FOUND:
        return "ESP_ERR_NOT_FOUND";
    case ESP_ERR_TIMEOUT:
        return "ESP_ERR_TIMEOUT";
    case ESP_ERR_INVALID_CRC:
        return "ESP_ERR_INVALID_CRC";
    case ESP_ERR_NVS_NOT_FOUND:
        return "ESP_ERR_NVS_NOT_FOUND";
    case ESP_ERR_NVS_NOT_ENOUGH_SPACE:
        return "ESP_ERR_NVS_NOT_ENOUGH_SPACE";
    case ESP_ERR_NVS_NO_FREE_PAGES:
        return "ESP_ERR_NVS_NO_FREE_PAGES";
    case ESP_ERR_NVS_NEW_VERSION_FOUND:
        return "ESP_ERR_NVS_NEW_VERSION_FOUND";
    case ESP_ERR_NVS_NOT_INITIALIZED:
        return "ESP_ERR_NVS_NOT_INITIALIZED";
    case ESP_ERR_NVS_INVALID_LENGTH:
        return "ESP_ERR_NVS_INVALID_LENGTH";
    case ESP_ERR_OTA_VALIDATE_FAILED:
        return "ESP_ERR_OTA_VALIDATE_FAILED";
    case ESP_ERR_OTA_ROLLBACK_FAILED:
        return "ESP_ERR_OTA_ROLLBACK_FAILED";
    default:
        return "ESP_ERR_?";
    }
}

/* ================= GPIO ================= */
#define NPIN 24
static int s_out[NPIN]; /* output latch */
static int s_mode[NPIN];
static int s_ext[NPIN]; /* externally driven level for inputs */
static int s_padLast[NPIN];
/* C6 GPIO input path: the IN register goes through a 2-stage synchroniser, so a
 * read issued right after a write still returns the old pad level. Modelled as:
 * reads less than GPIO_SYNC_US after the last write see the previous level
 * (virtual time does not advance inside a code path, so "immediately" = stale). */
#define GPIO_SYNC_US 2
static int s_outPrev[NPIN];
static U64 s_outAt[NPIN];

/**
 * Electrical pad level of GPIO n: a stuck pin (kv gpio.stuck.<n>) wins, then the output latch
 * when the pin is an output, otherwise the board's resistors (pull-up on 11 = STWD100 ~EN, pull-downs elsewhere).
 */
static int Pad(int n)
{
    char k[32];
    snprintf(k, sizeof k, "gpio.stuck.%d", n);
    I64 st = SimKvGet(k, -1);
    if (st >= 0)
        return (int)st;
    if (s_mode[n] & GPIO_MODE_OUTPUT)
        return s_out[n];
    return n == 11 ? 1 : 0; /* board: 10k pull-up on STWD100 ~EN (disarmed), pull-downs on the triac drivers */
}

/**
 * Pins whose pad changes are traced: 0 W (heat), 1 Y (cool), 2 G (fan), 8, 10 STWD100 WDI, 11 STWD100 ~EN.
 */
static bool Traced(int n)
{
    return n == 0 || n == 1 || n == 2 || n == 8 || n == 10 || n == 11;
}

/** Emit "GPIO <n> <level>" when a traced pad changed since the last trace. */
static void Trace(int n)
{
    if (!Traced(n))
        return;
    int p = Pad(n);
    if (p != s_padLast[n])
    {
        s_padLast[n] = p;
        SimEmit("GPIO %d %d", n, p);
    }
}

/** Trace every pin (used at boot to log the reset state). */
static void TraceAll(void)
{
    for (int i = 0; i < NPIN; i++)
        Trace(i);
}

/** Fake gpio_config(): records the mode of each pin in the mask; kv gpio.cfgfail.<n> makes it fail. */
esp_err_t gpio_config(const gpio_config_t *pC)
{
    for (int n = 0; n < NPIN; n++)
    {
        if (!(pC->pin_bit_mask & (1ULL << n)))
            continue;
        char k[32];
        snprintf(k, sizeof k, "gpio.cfgfail.%d", n);
        if (K(k))
            return ESP_FAIL;
        s_mode[n] = pC->mode;
        Trace(n);
    }
    return ESP_OK;
}

/**
 * Fake gpio_set_level(): updates the output latch, remembering the previous level and the time
 * for the synchroniser model; kv gpio.setfail.<n> makes it fail. Writes to 10/11 poll the STWD100 model.
 */
esp_err_t gpio_set_level(gpio_num_t n, U32 level)
{
    if (n < 0 || n >= NPIN)
        return ESP_ERR_INVALID_ARG;
    char k[32];
    snprintf(k, sizeof k, "gpio.setfail.%d", n);
    if (K(k))
        return ESP_FAIL;
    if ((level ? 1 : 0) != s_out[n])
    {
        s_outPrev[n] = s_out[n];
        s_outAt[n] = SimNowUs();
    }
    s_out[n] = level ? 1 : 0;
    Trace(n);
    if (n == 10 || n == 11)
        StwdPollEdge();
    return ESP_OK;
}

int EpdBusyLevel(void);
int BtnLevel(int n);

/**
 * Fake gpio_get_level(): 23 = e-paper BUSY, 3/4/5 = buttons. With the input buffer enabled
 * it returns the pad, stale within GPIO_SYNC_US of a write, and inverted for the next N reads when
 * kv gpio.glitch.<n> = N. Output-only pads read 0 as on the C6; plain inputs read s_ext.
 */
int gpio_get_level(gpio_num_t n)
{
    if (n < 0 || n >= NPIN)
        return 0;
    if (n == 23)
        return EpdBusyLevel();
    if (n == 3 || n == 4 || n == 5)
        return BtnLevel(n);
    if (s_mode[n] & GPIO_MODE_INPUT)
    { /* readback needs the input buffer enabled */
        int v = Pad(n);
        char k[32];
        snprintf(k, sizeof k, "gpio.stuck.%d", n);
        if ((s_mode[n] & GPIO_MODE_OUTPUT) && SimKvGet(k, -1) < 0 && SimNowUs() - s_outAt[n] < GPIO_SYNC_US)
            v = s_outPrev[n];                       /* synchroniser delay: stale */
        snprintf(k, sizeof k, "gpio.glitch.%d", n); /* injected: next N reads inverted */
        I64 g = SimKvGet(k, 0);
        if (g > 0)
        {
            SimKvSet(k, g - 1);
            v = !v;
            SimEmit("GLITCH read pin %d", (int)n);
        }
        return v;
    }
    if (s_mode[n] & GPIO_MODE_OUTPUT)
        return 0; /* output-only pad: IN register reads 0 on the C6 */
    return s_ext[n];
}

/** No-op; always ESP_OK. */
esp_err_t gpio_sleep_sel_dis(gpio_num_t n)
{
    (void)n;
    return ESP_OK;
}

/* ================= buttons ================= */
/* pin 3 = MODE, 4 = UP, 5 = DOWN; active low. kv btn.<pin> = 1 pressed. Bounce: random toggles until bounce_until. */
static U64 s_bounceUntil[NPIN];

/**
 * Button pad level (active low; pin 3 = MODE, 4 = UP, 5 = DOWN). kv btn.<pin> = 1 means pressed;
 * before s_bounceUntil the contact reads random levels.
 */
int BtnLevel(int n)
{
    char k[32];
    snprintf(k, sizeof k, "btn.%d", n);
    int pressed = (int)K(k);
    if (SimNowUs() < s_bounceUntil[n])
        pressed = (int)(SimRand() & 1);
    return pressed ? 0 : 1;
}

/** Pin of a script button name (mode or m, up, down); -1 when unknown. */
static int BtnPin(const char *pName)
{
    if (!strcmp(pName, "mode") || !strcmp(pName, "m"))
        return 3;
    if (!strcmp(pName, "up"))
        return 4;
    if (!strcmp(pName, "down"))
        return 5;
    return -1;
}

/* ================= esp_timer ================= */
/** Fake esp_timer: callback, period (us, 0 = one-shot), next expiry (virtual us) and armed flag. */
struct SimTimer
{
    esp_timer_cb_t cb;
    void *pArg;
    U64 period, next;
    bool active;
};
static struct SimTimer s_timers[16];
static int s_ntimers;
static TaskHandle_t s_timerTask;

/** Virtual time in us. */
I64 esp_timer_get_time(void)
{
    return (I64)SimNowUs();
}

/** Allocate one of 16 timers; kv fault.esp_timer (or a full table) gives ESP_ERR_NO_MEM. */
esp_err_t esp_timer_create(const esp_timer_create_args_t *pA, esp_timer_handle_t *pOut)
{
    if (K("fault.esp_timer") || s_ntimers >= 16)
        return ESP_ERR_NO_MEM;
    struct SimTimer *pT = &s_timers[s_ntimers++];
    pT->cb = pA->callback;
    pT->pArg = pA->arg;
    *pOut = pT;
    return ESP_OK;
}

/** Arm timer t to fire `us` from now, then every `period` us (0 = once), and wake the timer task. */
static esp_err_t Start(esp_timer_handle_t t, U64 us, U64 period)
{
    t->period = period;
    t->next = SimNowUs() + us;
    t->active = true;
    if (s_timerTask)
        xTaskNotifyGive(s_timerTask);
    return ESP_OK;
}

/** Fake esp_timer_start_periodic(): period `p` us, first expiry after one period. */
esp_err_t esp_timer_start_periodic(esp_timer_handle_t t, U64 p)
{
    return Start(t, p, p);
}

/** Fake esp_timer_start_once(): fires once after `us`. */
esp_err_t esp_timer_start_once(esp_timer_handle_t t, U64 us)
{
    return Start(t, us, 0);
}

/** Disarm the timer. */
esp_err_t esp_timer_stop(esp_timer_handle_t t)
{
    t->active = false;
    return ESP_OK;
}

/**
 * The esp_timer task (priority 22): runs expired callbacks in its own context, then sleeps until the next expiry or a new start.
 */
static void TimerTask(void *pArg)
{
    (void)pArg;
    for (;;)
    {
        U64 now = SimNowUs(), next = UINT64_MAX;
        for (int i = 0; i < s_ntimers; i++)
        {
            struct SimTimer *pT = &s_timers[i];
            if (!pT->active)
                continue;
            if (pT->next <= now)
            {
                if (pT->period)
                    pT->next += pT->period;
                else
                    pT->active = false;
                pT->cb(pT->pArg);
            }
            if (pT->active && pT->next < next)
                next = pT->next;
        }
        now = SimNowUs();
        U32 wait = next == UINT64_MAX ? portMAX_DELAY : (U32)((next > now ? next - now : 0) / 1000);
        ulTaskNotifyTake(pdTRUE, wait ? wait : 1);
    }
}

/* ================= SHT40 + room model ================= */
/** Fake I2C bus handle (no state). */
struct SimI2cBus
{
    int dummy;
};

/** Fake I2C device handle: only the address matters (0x44 = SHT40). */
struct SimI2cDev
{
    U16 addr;
};
static struct SimI2cBus s_bus;
static struct SimI2cDev s_sht;
static U64 s_measAt;
static bool s_measPending;
static F64 s_roomC; /* room temperature, °C */
static U64 s_roomAt;
static U16 s_stuckT, s_stuckH;
static bool s_haveStuck;

/**
 * Advance the room model to now. Without kv room.model the temperature is just room.t (0.01 °C).
 * With it: + room.heat while W (pad 0) is on, - room.cool while Y (pad 1) is on (0.01 °C/min), plus a
 * leak of room.leak %/min towards room.ambient (0.01 °C). The result is written back to room.t.
 */
static void RoomUpdate(void)
{
    U64 now = SimNowUs();
    F64 dtMin = (F64)(now - s_roomAt) / 60e6;
    s_roomAt = now;
    if (!K("room.model"))
    {
        s_roomC = (F64)SimKvGet("room.t", 2100) / 100.0;
        return;
    }
    /* furnace / AC at full power + leak towards ambient. Rates in 0.01 °C/min. */
    F64 amb = (F64)SimKvGet("room.ambient", 1500) / 100.0;
    F64 heat = Pad(0) ? (F64)SimKvGet("room.heat", 50) / 100.0 : 0;
    F64 cool = Pad(1) ? (F64)SimKvGet("room.cool", 40) / 100.0 : 0;
    F64 leak = (F64)SimKvGet("room.leak", 1) / 100.0; /* fraction per minute */
    s_roomC += (heat - cool + (amb - s_roomC) * leak) * dtMin;
    SimKvSet("room.t", (I64)llround(s_roomC * 100.0));
}

/** SHT40 CRC-8 (poly 0x31, init 0xFF). */
static U8 Crc8(const U8 *pD, int n)
{
    U8 c = 0xFF;
    for (int i = 0; i < n; i++)
    {
        c ^= pD[i];
        for (int b = 0; b < 8; b++)
            c = (U8)(c & 0x80 ? (c << 1) ^ 0x31 : c << 1);
    }
    return c;
}

/** Fake bus creation; fails when kv sht.busfail is set. */
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *pC, i2c_master_bus_handle_t *pOut)
{
    (void)pC;
    if (K("sht.busfail"))
        return ESP_FAIL;
    *pOut = &s_bus;
    return ESP_OK;
}

/** Fake device add: records the device address. */
esp_err_t i2c_master_bus_add_device(
    i2c_master_bus_handle_t b,
    const i2c_device_config_t *pC,
    i2c_master_dev_handle_t *pOut)
{
    (void)b;
    s_sht.addr = pC->device_address;
    *pOut = &s_sht;
    return ESP_OK;
}

/** Fake bus reset: logs it and counts kv sht.resets. */
esp_err_t i2c_master_bus_reset(i2c_master_bus_handle_t b)
{
    (void)b;
    SimEmit("I2C bus reset");
    SimKvSet("sht.resets", K("sht.resets") + 1);
    return ESP_OK;
}

/**
 * Injected SHT40 fault for one transfer, from kv sht.mode: 0 ok, 1 NAK, 2 timeout (bus stuck low:
 * blocks for timeoutMs, then ESP_ERR_TIMEOUT), 3 CRC error, 4 stuck value, 5 intermittent NAK
 * (sht.p %, default 50), 6 absent. Modes 3 and 4 are applied by the receive path.
 */
static esp_err_t ShtFault(int timeoutMs)
{
    int m = (int)K("sht.mode");
    if (m == 1 || m == 6)
        return ESP_FAIL;
    if (m == 2)
    {
        vTaskDelay(pdMS_TO_TICKS(timeoutMs));
        return ESP_ERR_TIMEOUT;
    }
    if (m == 5 && (int)(SimRand() % 100) < (int)SimKvGet("sht.p", 50))
        return ESP_FAIL;
    return ESP_OK;
}

/** SHT40 write: the 0xFD command starts a high-precision measurement. Anything not at 0x44 fails. */
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t d, const U8 *pBuf, size_t n, int timeoutMs)
{
    if (d->addr != 0x44 || n < 1)
        return ESP_FAIL;
    esp_err_t e = ShtFault(timeoutMs);
    if (e != ESP_OK)
        return e;
    if (pBuf[0] == 0xFD)
    {
        s_measPending = true;
        s_measAt = SimNowUs();
    }
    return ESP_OK;
}

/**
 * SHT40 read of a 6-byte result: NAKs until 8.3 ms after the measurement command, then returns
 * the room model temperature and room.rh (0.01 %RH) with ±2/±8 LSB noise (kv sht.noise, default on),
 * or a frozen value (mode 4) or bad CRC (mode 3). Counts kv sht.reads.
 */
esp_err_t i2c_master_receive(i2c_master_dev_handle_t d, U8 *pBuf, size_t n, int timeoutMs)
{
    if (d->addr != 0x44 || n != 6)
        return ESP_FAIL;
    esp_err_t e = ShtFault(timeoutMs);
    if (e != ESP_OK)
        return e;
    if (!s_measPending || SimNowUs() - s_measAt < 8300)
        return ESP_FAIL; /* NAK while measuring */
    s_measPending = false;
    RoomUpdate();
    F64 t = s_roomC, rh = (F64)SimKvGet("room.rh", 4500) / 100.0;
    U16 st = (U16)fmax(0, fmin(65535, llround((t + 45.0) * 65535.0 / 175.0)));
    U16 sh = (U16)fmax(0, fmin(65535, llround((rh + 6.0) * 65535.0 / 125.0)));
    if (SimKvGet("sht.noise", 1))
    { /* a real SHT40 is never perfectly still: +-2 LSB T, +-8 LSB RH */
        st = (U16)(st + (int)(SimRand() % 5) - 2);
        sh = (U16)(sh + (int)(SimRand() % 17) - 8);
    }
    if (K("sht.mode") == 4)
    {
        if (!s_haveStuck)
        {
            s_haveStuck = true;
            s_stuckT = (U16)SimKvGet("sht.stuck_t", st);
            s_stuckH = sh;
            SimKvSet("sht.stuck_t", st);
        }
        st = s_stuckT;
        sh = s_stuckH;
    }
    else
        s_haveStuck = false;
    pBuf[0] = st >> 8;
    pBuf[1] = st & 0xFF;
    pBuf[2] = Crc8(pBuf, 2);
    pBuf[3] = sh >> 8;
    pBuf[4] = sh & 0xFF;
    pBuf[5] = Crc8(pBuf + 3, 2);
    if (K("sht.mode") == 3)
        pBuf[2] ^= 0x5A;
    SimKvSet("sht.reads", K("sht.reads") + 1);
    return ESP_OK;
}

/* ================= SSD1680 e-paper ================= */
#define EPD_W     128
#define EPD_H     296
#define EPD_BYTES (EPD_W / 8 * EPD_H)

/** Fake SPI device handle for the e-paper (no state). */
struct SimSpiDev
{
    int dummy;
};
static struct SimSpiDev s_epd;
static U8 s_ram24[EPD_BYTES], s_ram26[EPD_BYTES], s_glass[EPD_BYTES];
static U8 s_cmd, s_updMode;
static size_t s_cursor;
static U64 s_busyUntil;
static int s_refreshes;

/**
 * e-paper BUSY: high until the current reset/refresh time has elapsed, or forever with kv epd.busy_stuck.
 */
int EpdBusyLevel(void)
{
    if (K("epd.busy_stuck"))
        return 1;
    return SimNowUs() < s_busyUntil;
}

/** Fake SPI bus init; kv epd.init_fail gives ESP_ERR_INVALID_STATE. */
esp_err_t spi_bus_initialize(spi_host_device_t h, const spi_bus_config_t *pC, spi_dma_chan_t dma)
{
    (void)h;
    (void)pC;
    (void)dma;
    return K("epd.init_fail") ? ESP_ERR_INVALID_STATE : ESP_OK;
}

/** Fake device add; always the e-paper. */
esp_err_t spi_bus_add_device(spi_host_device_t h, const spi_device_interface_config_t *pC, spi_device_handle_t *pOut)
{
    (void)h;
    (void)pC;
    *pOut = &s_epd;
    return ESP_OK;
}

/**
 * Write the panel image as <outdir>/fb_<n>.pbm and emit an "EPD refresh" line with the mode,
 * ink (black pixel count) and an FNV-1a hash of the image.
 */
static void EpdDump(const char *pWhy)
{
    char path[512];
    snprintf(path, sizeof path, "%s/fb_%03d.pbm", SimOutdir(), s_refreshes);
    FILE *pF = fopen(path, "wb");
    if (pF)
    {
        /* RAM: MSB first, 1 = white. PBM: 1 = black. Panel is portrait 128x296. */
        fprintf(pF, "P4\n%d %d\n", EPD_W, EPD_H);
        for (int i = 0; i < EPD_BYTES; i++)
            fputc(~s_glass[i] & 0xFF, pF);
        fclose(pF);
    }
    U32 crc = 2166136261u;
    int ink = 0;
    for (int i = 0; i < EPD_BYTES; i++)
    {
        crc = (crc ^ s_glass[i]) * 16777619u;
        ink += __builtin_popcount((unsigned)(~s_glass[i] & 0xFF));
    }
    SimEmit(
        "EPD refresh n=%d mode=0x%02x ink=%d crc=%08x file=%s why=%s",
        s_refreshes,
        s_updMode,
        ink,
        crc,
        path,
        pWhy);
}

/**
 * SSD1680 model. With DC (GPIO 21) low the 1-byte command is decoded: 0x12 SW reset (BUSY 3 ms),
 * 0x24/0x26 start a RAM write, 0x20 activation (copy RAM 0x24 to the glass, BUSY 0.6 s for update
 * mode 0xFF else 2 s, dump a PBM). Data bytes fill RAM or set the 0x22 update mode.
 * Faults: kv epd.spi_fail fails, epd.hang blocks forever.
 */
esp_err_t spi_device_polling_transmit(spi_device_handle_t d, spi_transaction_t *pT)
{
    (void)d;
    if (K("epd.spi_fail"))
        return ESP_FAIL;
    if (K("epd.hang"))
        vTaskDelay(portMAX_DELAY); /* SPI/DMA never completes */
    const U8 *pP = pT->tx_buffer;
    size_t n = pT->length / 8;
    bool isData = s_out[21] != 0; /* DC: 0 = command */
    if (!isData)
    {
        if (n != 1)
            return ESP_ERR_INVALID_ARG;
        s_cmd = pP[0];
        if (s_cmd == 0x12)
            s_busyUntil = SimNowUs() + 3000; /* SW reset */
        if (s_cmd == 0x24 || s_cmd == 0x26)
            s_cursor = 0;
        if (s_cmd == 0x20)
        { /* master activation */
            memcpy(s_glass, s_ram24, sizeof s_glass);
            s_busyUntil = SimNowUs() + (s_updMode == 0xFF ? 600000 : 2000000);
            s_refreshes++;
            EpdDump("refresh");
        }
        return ESP_OK;
    }
    for (size_t i = 0; i < n; i++)
    {
        if (s_cmd == 0x24 && s_cursor < EPD_BYTES)
            s_ram24[s_cursor++] = pP[i];
        else if (s_cmd == 0x26 && s_cursor < EPD_BYTES)
            s_ram26[s_cursor++] = pP[i];
        else if (s_cmd == 0x22 && i == 0)
            s_updMode = pP[i];
    }
    return ESP_OK;
}

/* ================= NVS ================= */
#define NVS_MAX 64

/** Fake NVS entry (namespace, key, up to 512 value bytes); persisted in the sim state file. */
static struct
{
    bool used;
    char ns[16], key[16];
    U8 v[512];
    size_t n;
} s_nvs[NVS_MAX];
static bool s_nvsInited;

/** Open NVS handles (handle = index + 1); an empty ns marks a free slot. */
static struct
{
    char ns[16];
    bool rw;
} s_handles[8];

/** Write every NVS entry to the state file as `nvs <ns> <key> <hex>` ("-" for empty). */
void SimNvsSave(FILE *pF)
{
    for (int i = 0; i < NVS_MAX; i++)
    {
        if (!s_nvs[i].used)
            continue;
        fprintf(pF, "nvs %s %s ", s_nvs[i].ns, s_nvs[i].key);
        for (size_t j = 0; j < s_nvs[i].n; j++)
            fprintf(pF, "%02x", s_nvs[i].v[j]);
        fprintf(pF, "%s\n", s_nvs[i].n ? "" : "-");
    }
}

/** Index of entry ns/key, creating an empty one when `create` is set. Returns -1 when missing or full. */
static int NvsFind(const char *pNs, const char *pKey, bool create)
{
    for (int i = 0; i < NVS_MAX; i++)
        if (s_nvs[i].used && !strcmp(s_nvs[i].ns, pNs) && !strcmp(s_nvs[i].key, pKey))
            return i;
    if (!create)
        return -1;
    for (int i = 0; i < NVS_MAX; i++)
        if (!s_nvs[i].used)
        {
            s_nvs[i].used = true;
            snprintf(s_nvs[i].ns, 16, "%s", pNs);
            snprintf(s_nvs[i].key, 16, "%s", pKey);
            s_nvs[i].n = 0;
            return i;
        }
    return -1;
}

/** Set entry ns/key from a hex string ("-" = empty value). */
static void NvsPutHex(const char *pNs, const char *pKey, const char *pHex)
{
    int i = NvsFind(pNs, pKey, true);
    if (i < 0)
        return;
    s_nvs[i].n = 0;
    if (!strcmp(pHex, "-"))
        return;
    for (size_t j = 0; pHex[2 * j] && pHex[2 * j + 1] && j < sizeof s_nvs[0].v; j++)
    {
        unsigned b;
        sscanf(pHex + 2 * j, "%2x", &b);
        s_nvs[i].v[j] = (U8)b;
        s_nvs[i].n = j + 1;
    }
}

/** Restore one `nvs ...` state-file line. Returns false if the line is not an NVS entry. */
bool SimNvsLoadLine(const char *pLine)
{
    char ns[16], key[16], hex[1100];
    if (sscanf(pLine, "nvs %15s %15s %1099s", ns, key, hex) != 3)
        return false;
    NvsPutHex(ns, key, hex);
    return true;
}

/**
 * Fake nvs_flash_init(), kv nvs.init: 0 ok, 1 NO_FREE_PAGES and 2 NEW_VERSION_FOUND until erased,
 * 3 partition missing (NOT_FOUND, the erase fails too), 4 generic failure.
 */
esp_err_t nvs_flash_init(void)
{
    switch (K("nvs.init"))
    {
    case 1:
        return ESP_ERR_NVS_NO_FREE_PAGES;
    case 2:
        return ESP_ERR_NVS_NEW_VERSION_FOUND;
    case 3:
        return ESP_ERR_NOT_FOUND;
    case 4:
        return ESP_FAIL;
    default:
        s_nvsInited = true;
        return ESP_OK;
    }
}

/** Erase every entry and clear nvs.init modes 1/2; modes 3 and 4 make it fail. */
esp_err_t nvs_flash_erase(void)
{
    int m = (int)K("nvs.init");
    if (m == 3)
        return ESP_ERR_NOT_FOUND;
    if (m == 4)
        return ESP_FAIL;
    memset(s_nvs, 0, sizeof s_nvs);
    SimKvSet("nvs.init", 0);
    SimEmit("NVS erased");
    return ESP_OK;
}

/**
 * Fake nvs_open(): needs nvs_flash_init(); kv nvs.open_fail fails it; read-only opens of an empty
 * namespace give ESP_ERR_NVS_NOT_FOUND; at most 8 open handles.
 */
esp_err_t nvs_open(const char *pNs, nvs_open_mode_t mode, nvs_handle_t *pOut)
{
    if (!s_nvsInited)
        return ESP_ERR_NVS_NOT_INITIALIZED;
    if (K("nvs.open_fail"))
        return ESP_FAIL;
    if (mode == NVS_READONLY)
    {
        bool any = false;
        for (int i = 0; i < NVS_MAX; i++)
            if (s_nvs[i].used && !strcmp(s_nvs[i].ns, pNs))
                any = true;
        if (!any)
            return ESP_ERR_NVS_NOT_FOUND;
    }
    for (int h = 0; h < 8; h++)
        if (!s_handles[h].ns[0])
        {
            snprintf(s_handles[h].ns, 16, "%s", pNs);
            s_handles[h].rw = mode == NVS_READWRITE;
            *pOut = (nvs_handle_t)(h + 1);
            return ESP_OK;
        }
    return ESP_ERR_NO_MEM;
}

/** Free the handle slot. */
void nvs_close(nvs_handle_t h)
{
    if (h >= 1 && h <= 8)
        s_handles[h - 1].ns[0] = 0;
}

/** Namespace of handle h ("" when invalid). */
static const char *Hns(nvs_handle_t h)
{
    return (h >= 1 && h <= 8) ? s_handles[h - 1].ns : "";
}

/**
 * Fake nvs_get_blob(): size query with pOut == NULL; INVALID_LENGTH when *pLen is too small. kv nvs.read_fail fails it.
 */
esp_err_t nvs_get_blob(nvs_handle_t h, const char *pKey, void *pOut, size_t *pLen)
{
    if (K("nvs.read_fail"))
        return ESP_FAIL;
    int i = NvsFind(Hns(h), pKey, false);
    if (i < 0)
        return ESP_ERR_NVS_NOT_FOUND;
    if (!pOut)
    {
        *pLen = s_nvs[i].n;
        return ESP_OK;
    }
    if (*pLen < s_nvs[i].n)
    {
        *pLen = s_nvs[i].n;
        return ESP_ERR_NVS_INVALID_LENGTH;
    }
    memcpy(pOut, s_nvs[i].v, s_nvs[i].n);
    *pLen = s_nvs[i].n;
    return ESP_OK;
}

/** Common write check: a read-write handle, and the nvs.write_fail / nvs.full faults. */
static esp_err_t WriteCheck(nvs_handle_t h)
{
    if (h < 1 || h > 8 || !s_handles[h - 1].rw)
        return ESP_ERR_INVALID_ARG;
    if (K("nvs.write_fail"))
        return ESP_FAIL;
    if (K("nvs.full"))
        return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    return ESP_OK;
}

/** Fake nvs_set_blob() (up to 512 bytes), logged as "NVS set". Stored immediately; commit is not needed. */
esp_err_t nvs_set_blob(nvs_handle_t h, const char *pKey, const void *pV, size_t len)
{
    esp_err_t e = WriteCheck(h);
    if (e != ESP_OK)
        return e;
    int i = NvsFind(Hns(h), pKey, true);
    if (i < 0 || len > sizeof s_nvs[0].v)
        return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    memcpy(s_nvs[i].v, pV, len);
    s_nvs[i].n = len;
    SimEmit("NVS set %s/%s %zu bytes", Hns(h), pKey, len);
    return ESP_OK;
}

/** Fake nvs_get_u8(): the entry must be exactly 1 byte. */
esp_err_t nvs_get_u8(nvs_handle_t h, const char *pKey, U8 *pOut)
{
    size_t n = 1;
    if (K("nvs.read_fail"))
        return ESP_FAIL;
    int i = NvsFind(Hns(h), pKey, false);
    if (i < 0)
        return ESP_ERR_NVS_NOT_FOUND;
    if (s_nvs[i].n != 1)
        return ESP_ERR_NVS_INVALID_LENGTH;
    (void)n;
    *pOut = s_nvs[i].v[0];
    return ESP_OK;
}

/** Stored as a 1-byte blob. */
esp_err_t nvs_set_u8(nvs_handle_t h, const char *pKey, U8 v)
{
    return nvs_set_blob(h, pKey, &v, 1);
}

/** Remove one key. */
esp_err_t nvs_erase_key(nvs_handle_t h, const char *pKey)
{
    esp_err_t e = WriteCheck(h);
    if (e != ESP_OK)
        return e;
    int i = NvsFind(Hns(h), pKey, false);
    if (i < 0)
        return ESP_ERR_NVS_NOT_FOUND;
    s_nvs[i].used = false;
    return ESP_OK;
}

/** Remove every key in the handle's namespace. */
esp_err_t nvs_erase_all(nvs_handle_t h)
{
    esp_err_t e = WriteCheck(h);
    if (e != ESP_OK)
        return e;
    for (int i = 0; i < NVS_MAX; i++)
        if (s_nvs[i].used && !strcmp(s_nvs[i].ns, Hns(h)))
            s_nvs[i].used = false;
    SimEmit("NVS erase_all %s", Hns(h));
    return ESP_OK;
}

/** No-op unless kv nvs.commit_fail is set. */
esp_err_t nvs_commit(nvs_handle_t h)
{
    (void)h;
    return K("nvs.commit_fail") ? ESP_FAIL : ESP_OK;
}

/* ================= OTA ================= */
/* kv: ota.run (slot running), ota.boot (slot to boot), ota.st0/1 (esp_ota_img_states_t; default VALID for 0,
 * UNDEFINED for 1), ota.img0/1 (1 = bootable image). Faults: ota.nopart, ota.begin_fail, ota.write_fail_after
 * (bytes, 0 = off), ota.setboot_fail. */
static const esp_partition_t kPart[2] = {{"ota_0", 0x20000, 0x300000, 0}, {"ota_1", 0x320000, 0x300000, 1}};
static U32 s_otaWritten;
static U8 s_otaFirst;
static bool s_otaOpen;

/** otadata state of slot i (kv ota.st0/1; default VALID for slot 0 and UNDEFINED for slot 1). */
static I64 Ost(int i)
{
    return SimKvGet(i ? "ota.st1" : "ota.st0", i ? ESP_OTA_IMG_UNDEFINED : ESP_OTA_IMG_VALID);
}

/** Set the otadata state of slot i. */
static void SetOst(int i, I64 v)
{
    SimKvSet(i ? "ota.st1" : "ota.st0", v);
}

/** Whether slot i holds a bootable image (kv ota.img0/1; default slot 0 only). */
static I64 Oimg(int i)
{
    return SimKvGet(i ? "ota.img1" : "ota.img0", i ? 0 : 1);
}

/**
 * The bootloader's rollback logic (CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE), run on every sim boot:
 * a slot still PENDING_VERIFY is ABORTED and the other slot is booted if usable; NEW becomes
 * PENDING_VERIFY; an INVALID/ABORTED/empty slot falls back to the other one. Sets ota.boot/ota.run.
 */
static void OtaBootloader(void)
{
    int b = (int)SimKvGet("ota.boot", 0), o = !b;
    if (Ost(b) == ESP_OTA_IMG_PENDING_VERIFY)
    {
        SimEmit("BOOTLOADER slot %d still PENDING_VERIFY: marking ABORTED, rolling back", b);
        SetOst(b, ESP_OTA_IMG_ABORTED);
        if (Oimg(o) && (Ost(o) == ESP_OTA_IMG_VALID || Ost(o) == ESP_OTA_IMG_UNDEFINED))
            b = o;
    }
    else if (Ost(b) == ESP_OTA_IMG_NEW)
    {
        SetOst(b, ESP_OTA_IMG_PENDING_VERIFY);
    }
    else if (Ost(b) == ESP_OTA_IMG_INVALID || Ost(b) == ESP_OTA_IMG_ABORTED || !Oimg(b))
    {
        if (Oimg(o))
            b = o;
    }
    SimKvSet("ota.boot", b);
    SimKvSet("ota.run", b);
    SimEmit("BOOTLOADER running slot %d state %lld", b, (long long)Ost(b));
}

/** Running slot (kv ota.run). */
const esp_partition_t *esp_ota_get_running_partition(void)
{
    return &kPart[SimKvGet("ota.run", 0)];
}

/** The other slot; NULL with kv ota.nopart. */
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *pS)
{
    (void)pS;
    if (K("ota.nopart"))
        return NULL;
    return &kPart[!SimKvGet("ota.run", 0)];
}

/** otadata state of the slot; ESP_ERR_NOT_FOUND when UNDEFINED. */
esp_err_t esp_ota_get_state_partition(const esp_partition_t *pP, esp_ota_img_states_t *pSt)
{
    I64 v = Ost(pP->index);
    if (v == ESP_OTA_IMG_UNDEFINED)
        return ESP_ERR_NOT_FOUND;
    *pSt = (esp_ota_img_states_t)v;
    return ESP_OK;
}

/** Mark the running slot VALID. */
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void)
{
    SetOst((int)SimKvGet("ota.run", 0), ESP_OTA_IMG_VALID);
    SimEmit("OTA mark valid slot %lld", (long long)SimKvGet("ota.run", 0));
    return ESP_OK;
}

/** Whether the other slot has an image that isn't INVALID/ABORTED. */
bool esp_ota_check_rollback_is_possible(void)
{
    int o = !(int)SimKvGet("ota.run", 0);
    return Oimg(o) && Ost(o) != ESP_OTA_IMG_INVALID && Ost(o) != ESP_OTA_IMG_ABORTED;
}

/**
 * Mark the running slot INVALID and reboot into the other one; fails if there is no usable image. Does not return on success.
 */
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void)
{
    int r = (int)SimKvGet("ota.run", 0), o = !r;
    if (!Oimg(o) || Ost(o) == ESP_OTA_IMG_INVALID || Ost(o) == ESP_OTA_IMG_ABORTED)
    {
        SimEmit("OTA rollback impossible: no valid image in slot %d", o);
        return ESP_ERR_OTA_ROLLBACK_FAILED;
    }
    SetOst(r, ESP_OTA_IMG_INVALID);
    SimKvSet("ota.boot", o);
    SimReset("OTA_ROLLBACK");
}

/** Start writing an image; kv ota.begin_fail gives ESP_ERR_OTA_PARTITION_CONFLICT. */
esp_err_t esp_ota_begin(const esp_partition_t *pP, size_t size, esp_ota_handle_t *pOut)
{
    (void)size;
    if (!pP)
        return ESP_ERR_INVALID_ARG;
    if (K("ota.begin_fail"))
        return ESP_ERR_OTA_PARTITION_CONFLICT;
    s_otaWritten = 0;
    s_otaOpen = true;
    s_otaFirst = 0;
    *pOut = 1;
    SimEmit("OTA begin slot %d", pP->index);
    return ESP_OK;
}

/** Count image bytes and remember the first one; kv ota.write_fail_after = N fails past N bytes. */
esp_err_t esp_ota_write(esp_ota_handle_t h, const void *pD, size_t n)
{
    if (h != 1 || !s_otaOpen)
        return ESP_ERR_INVALID_ARG;
    I64 lim = K("ota.write_fail_after");
    if (lim && s_otaWritten + n > (U64)lim)
        return ESP_FAIL;
    if (!s_otaWritten && n)
        s_otaFirst = ((const U8 *)pD)[0];
    s_otaWritten += (U32)n;
    return ESP_OK;
}

/** Validate the image (first byte 0xE9, at least 1024 bytes) and mark the other slot as holding an image. */
esp_err_t esp_ota_end(esp_ota_handle_t h)
{
    if (h != 1 || !s_otaOpen)
        return ESP_ERR_INVALID_ARG;
    s_otaOpen = false;
    if (s_otaFirst != 0xE9 || s_otaWritten < 1024)
    {
        SimEmit("OTA end: image invalid");
        return ESP_ERR_OTA_VALIDATE_FAILED;
    }
    SimKvSet(SimKvGet("ota.run", 0) ? "ota.img0" : "ota.img1", 1);
    SimEmit("OTA end ok %u bytes", s_otaWritten);
    return ESP_OK;
}

/** Close the image write. */
esp_err_t esp_ota_abort(esp_ota_handle_t h)
{
    (void)h;
    s_otaOpen = false;
    SimEmit("OTA abort");
    return ESP_OK;
}

/** Select slot for the next boot and mark it NEW; kv ota.setboot_fail fails it. */
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *pP)
{
    if (K("ota.setboot_fail"))
        return ESP_FAIL;
    SimKvSet("ota.boot", pP->index);
    SetOst(pP->index, ESP_OTA_IMG_NEW);
    return ESP_OK;
}

/* ================= reset, watchdogs ================= */
/** Software reset (reason SW). Does not return. */
void esp_restart(void)
{
    SimReset("SW");
}

static TaskHandle_t s_twdtTask;
static U64 s_twdtFed;
static U64 s_hangAt = UINT64_MAX;

/**
 * Watch task t (or the caller). Also arms the injected app hang: kv hang.boots = N hangs the next N
 * boots hang.after ms (default 2000) after this call.
 */
esp_err_t esp_task_wdt_add(TaskHandle_t t)
{
    s_twdtTask = t ? t : xTaskGetCurrentTaskHandle();
    s_twdtFed = SimNowUs();
    /* injected app hang: read here (after the t<=boot script commands ran) */
    I64 hb = K("hang.boots");
    if (hb > 0)
    {
        SimKvSet("hang.boots", hb - 1);
        s_hangAt = SimNowUs() + (U64)SimKvGet("hang.after", 2000) * 1000u;
    }
    return ESP_OK;
}

/** Feed the task watchdog; once the injected hang time has passed the watched task blocks here forever. */
esp_err_t esp_task_wdt_reset(void)
{
    if (xTaskGetCurrentTaskHandle() == s_twdtTask && SimNowUs() >= s_hangAt)
    {
        SimEmit("HANG app task (injected)");
        vTaskDelay(portMAX_DELAY);
    }
    s_twdtFed = SimNowUs();
    return ESP_OK;
}

/* STWD100: ~EN low = armed; reset if no WDI edge for tWD (1.6 s typ). */
static int s_wdiLast;
static U64 s_wdiEdge;
static bool s_wdArmed;
#define STWD_TWD_US 1600000u

/** Sample the STWD100 pins: ~EN (11) low arms it (restarting the timeout), any WDI (10) edge feeds it. */
static void StwdPoll(void)
{
    int en = Pad(11), wdi = Pad(10);
    bool armed = en == 0;
    if (armed && !s_wdArmed)
        s_wdiEdge = SimNowUs();
    s_wdArmed = armed;
    if (wdi != s_wdiLast)
    {
        s_wdiLast = wdi;
        s_wdiEdge = SimNowUs();
    }
}

/**
 * Next virtual time (us) at which a watchdog could fire: STWD100 tWD 1.6 s after the last edge, task WDT 5 s after the last feed.
 */
U64 SimPeriphNextDeadlineUs(void)
{
    U64 d = UINT64_MAX;
    if (s_wdArmed)
        d = s_wdiEdge + STWD_TWD_US;
    if (s_twdtTask && s_twdtFed + 5000000u < d)
        d = s_twdtFed + 5000000u;
    return d;
}

/**
 * Reset the chip if the armed STWD100 saw no WDI edge for 1.6 s (STWD100) or the task watchdog was not fed for 5 s (TASK_WDT).
 */
void SimPeriphCheck(void)
{
    StwdPoll();
    U64 now = SimNowUs();
    if (s_wdArmed && now - s_wdiEdge >= STWD_TWD_US)
        SimReset("STWD100");
    if (s_twdtTask && now - s_twdtFed >= 5000000u)
    {
        SimEmit("task_wdt: app task did not reset the watchdog");
        SimReset("TASK_WDT");
    }
}

/** Poll the STWD100 right after a write to its pins, so no edge is missed. */
static void StwdPollEdge(void)
{
    StwdPoll();
}

/** Poll the STWD100 before a script command changes anything. */
static void GpioHook(void)
{
    StwdPoll();
}

/* ================= boot ================= */
/**
 * Power-on state of the fake hardware: outputs low, the bootloader picks the OTA slot, the reset
 * pad levels are traced, the room model starts from room.t, and the esp_timer task is created.
 */
void SimPeriphBoot(void)
{
    memset(s_out, 0, sizeof s_out);
    for (int i = 0; i < NPIN; i++)
        s_padLast[i] = -1;
    OtaBootloader();
    TraceAll(); /* reset state: every traced pad low (pull-downs), unless stuck */
    s_roomC = (F64)SimKvGet("room.t", 2100) / 100.0;
    s_roomAt = SimNowUs();
    xTaskCreate(TimerTask, "esp_timer", 0, NULL, 22, &s_timerTask);
}

/* ================= script commands ================= */
/** Parse a script number (decimal, 0x hex or 0 octal). */
static I64 Num(const char *pS)
{
    return strtoll(pS, NULL, 0);
}

/**
 * Run a hardware script command: set <kv> <n>, temp <0.01 °C>, btn <mode|m|up|down>
 * <press|release|bounce ms>, nvs <ns> <key> <hex>, ota pending <other_valid|...>, power cycle,
 * dump, end.
 *
 * Returns false for an unknown command.
 */
bool SimPeriphCmd(char **pA, int n)
{
    GpioHook();
    if (!strcmp(pA[0], "set") && n == 3)
    {
        SimKvSet(pA[1], Num(pA[2]));
        if (!strncmp(pA[1], "room.t", 6))
            RoomUpdate();
        return true;
    }
    if (!strcmp(pA[0], "temp") && n == 2)
    {
        RoomUpdate();
        SimKvSet("room.t", Num(pA[1]));
        s_roomC = Num(pA[1]) / 100.0;
        return true;
    }
    if (!strcmp(pA[0], "btn") && n >= 3)
    {
        int p = BtnPin(pA[1]);
        if (p < 0)
            return false;
        char k[16];
        snprintf(k, sizeof k, "btn.%d", p);
        if (!strcmp(pA[2], "press"))
            SimKvSet(k, 1);
        else if (!strcmp(pA[2], "release"))
            SimKvSet(k, 0);
        else if (!strcmp(pA[2], "bounce") && n == 4)
            s_bounceUntil[p] = SimNowUs() + (U64)Num(pA[3]) * 1000u;
        else
            return false;
        return true;
    }
    if (!strcmp(pA[0], "nvs") && n == 4)
    {
        NvsPutHex(pA[1], pA[2], pA[3]);
        return true;
    }
    if (!strcmp(pA[0], "ota") && n == 3 && !strcmp(pA[1], "pending"))
    {
        /* just "installed" an update in the running slot: bootloader would have made it PENDING_VERIFY */
        int r = (int)SimKvGet("ota.run", 0), o = !r;
        SetOst(r, ESP_OTA_IMG_PENDING_VERIFY);
        SimKvSet(r ? "ota.img1" : "ota.img0", 1);
        SimKvSet(o ? "ota.img1" : "ota.img0", !strcmp(pA[2], "other_valid"));
        SetOst(o, !strcmp(pA[2], "other_valid") ? ESP_OTA_IMG_VALID : ESP_OTA_IMG_INVALID);
        return true;
    }
    if (!strcmp(pA[0], "power") && n == 2 && !strcmp(pA[1], "cycle"))
        SimReset("POWERON");
    if (!strcmp(pA[0], "dump"))
    {
        EpdDump("dump");
        return true;
    }
    if (!strcmp(pA[0], "end"))
        SimFinish(kSimExitDone);
    return false;
}
