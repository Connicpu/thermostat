/* main.c - Thermostat V3 application: wires sensor -> control -> outputs,
 * buttons/display, Zigbee, persistence and the external watchdog.
 *
 * Task layout
 *   app (this file, prio 6) : 250 ms loop. Sensor read, control step,
 *                             output write, UI events, Zigbee publish,
 *                             health check + STWD100 kick.
 *   display (prio 3)        : e-paper rendering/refresh (slow, blocking).
 *   zigbee (prio 5)         : ZBOSS main loop.
 *   esp_timer               : 10 ms button sampling -> queue.
 */
#include "core.h"
#include <string.h>
#include "control.h"
#include "display.h"
#include "display_policy.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_pm.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hvac_outputs.h"
#include "hw_watchdog.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "sensor.h"
#include "sensor_stuck.h"
#include "settings.h"
#include "schedule.h"
#include "ui.h"
#include "menu.h"
#include "fw_version.h"
#include "zb_thermostat.h"

static const char *kTag = "app";

#define LOOP_MS             250
#define SAVE_DELAY_MS       5000             /* coalesce NVS writes */
#define HWDT_ARM_AFTER_MS   10000            /* arm STWD100 once the loop is proven healthy */
#define OTA_VALID_AFTER_MS  60000            /* min healthy uptime before confirming a new image */
#define OTA_JOIN_TIMEOUT_MS (10 * 60 * 1000) /* roll back if never rejoined Zigbee by then */
#define BANNER_MS           4000
#define BOOT_LOOP_LIMIT     5 /* this many resets without BOOT_STABLE_MS uptime -> safe mode */
#define BOOT_STABLE_MS      (3 * 60 * 1000)
#define SAFE_MODE_RETRY_MS  (30 * 60 * 1000) /* safe mode: try a normal boot again after this */

/* Structured lines for the HOOTL scenario runner / field logs: "EVT <what> ...". */
#define EVT(...) ESP_LOGI("evt", __VA_ARGS__)

static bool s_nvsOk;            /* settings can be persisted */
static bool s_safeMode;         /* boot loop detected: outputs held off */
static bool s_clearNotSaved;    /* clear did not commit: banner says so, stay in safe mode */
static bool s_outputsOk = true; /* HvacOutputsInit() succeeded */

/* on-device menu (pure state machine in components/ui/menu.c) */
static Menu s_menu;
static U8 s_fullEpoch;      /* bumped -> display does a full refresh */
static SensorReading s_raw; /* last reading, calibration removed (Device info) */

static SemaphoreHandle_t s_mtx; /* guards s_persist */
static Persist s_persist;
static bool s_dirty;
static U64 s_dirtyAt;
static QueueHandle_t s_btnQ;
static volatile U16 s_identifyS;
static volatile int s_otaPct = -1;

/** A button event as queued from the esp_timer callback to the app task: which = UiButton, ev = BtnEvent. */
typedef struct
{
    U8 which, ev;
} BtnMsg;
static QueueHandle_t s_boostQ; /* U8 kZbBoostCmd* from Zigbee or the UP+DOWN combo */
static U8 s_boostResult;       /* kZbBoostResult* last outcome, reported to HA */
static bool s_boostActive;     /* mirror for the UI combo */

/* ---------------- shared settings ---------------- */

/** Mark the persisted settings dirty, so they are saved SAVE_DELAY_MS from now. Caller holds s_mtx. */
static void MarkDirty(void)
{
    s_dirty = true;
    s_dirtyAt = NowMs();
}

/**
 * Apply new mode/fan/main setpoints to the persisted settings (mutex-protected; any task). A
 * heat or cool setpoint that differs from the current program setpoint is stored in the CURRENT
 * program period (day or night, see schedule.h); with no clock that is the day period. Marks
 * the settings dirty, so they are saved SAVE_DELAY_MS later.
 */
static void SettingsUpdate(const CtrlSettings *pS)
{
    CtrlParams p = CTRL_PARAMS_DEFAULT();
    U16 minute = 0;
    bool known = ZbThermostatLocalMinute(&minute);
    CtrlSettings n = *pS;
    CtrlSanitizeSettings(&p, &n);
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    I16 h, c;
    SchedProgramSp(&s_persist.sched, known, minute, &h, &c);
    if (n.heatSp != h)
        SchedEditCurrent(&p, &s_persist.sched, true, known, minute, n.heatSp);
    if (n.coolSp != c)
        SchedEditCurrent(&p, &s_persist.sched, false, known, minute, n.coolSp);
    s_persist.s.mode = n.mode;
    s_persist.s.fan = n.fan;
    MarkDirty();
    xSemaphoreGive(s_mtx);
}

/**
 * Returns a snapshot of the persisted settings (mutex-protected; any task), with s.heatSp /
 * s.coolSp replaced by the current program setpoints (without the morning boost).
 */
static Persist SettingsGet(void)
{
    U16 minute = 0;
    bool known = ZbThermostatLocalMinute(&minute);
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    Persist p = s_persist;
    xSemaphoreGive(s_mtx);
    SchedProgramSp(&p.sched, known, minute, &p.s.heatSp, &p.s.coolSp);
    return p;
}

static const char *s_pBanner;
static U64 s_bannerUntil;

/** Show a short message in the status bar for BANNER_MS (app task only). */
static void Banner(const char *pB)
{
    s_pBanner = pB;
    s_bannerUntil = NowMs() + BANNER_MS;
}

/* ---------------- Zigbee callbacks (Zigbee task) ---------------- */

/**
 * Zigbee onSettings callback (Zigbee task). Starts from the live program setpoints, so a mode/fan
 * write can never copy a stale setpoint into a period that just started, then applies mode, fan and
 * only the one setpoint that was written.
 */
static void ZbOnSettings(const CtrlSettings *pS, U8 what)
{
    /* Start from the live program setpoints so a mode/fan write can never copy a
     * stale setpoint into a period that just started; then apply the one field written. */
    Persist p = SettingsGet();
    CtrlSettings n = p.s;
    n.mode = pS->mode;
    n.fan = pS->fan;
    if (what == kZbSetHeatSp)
        n.heatSp = pS->heatSp;
    if (what == kZbSetCoolSp)
        n.coolSp = pS->coolSp;
    SettingsUpdate(&n);
}

/**
 * Zigbee onCalibration callback: store LocalTemperatureCalibration (0.1 °C, clamped to ±2.5 °C) and mark dirty.
 */
static void ZbOnCalib(I8 c)
{
    if (c > 25)
        c = 25;
    if (c < -25)
        c = -25;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_persist.calibTenths = c;
    MarkDirty();
    xSemaphoreGive(s_mtx);
}

/**
 * Zigbee onBoostParams callback: store the boost preferences (already clamped by the Zigbee layer) and mark dirty.
 */
static void ZbOnBoostParams(I16 sp, U16 minutes, U8 dir)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_persist.boostSp = sp;
    s_persist.boostMinutes = minutes;
    s_persist.boostDir = dir;
    MarkDirty();
    xSemaphoreGive(s_mtx);
}

/** Zigbee onSched callback: store the sanitized program schedule and mark dirty. */
static void ZbOnSched(const SchedCfg *pC)
{
    CtrlParams prm = CTRL_PARAMS_DEFAULT();
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_persist.sched = *pC;
    SchedSanitize(&prm, &s_persist.sched);
    MarkDirty();
    xSemaphoreGive(s_mtx);
}

/**
 * Zigbee onBoostCommand callback: queue the kZbBoostCmd* for the app task (dropped if the queue is full).
 */
static void ZbOnBoostCmd(U8 cmd)
{
    xQueueSend(s_boostQ, &cmd, 0);
}

/** Returns the kZbBoostResult* code reported to HA for why a boost ended. */
static U8 EndToResult(CtrlBoostEnd e)
{
    switch (e)
    {
    case kBoostEndReached:
        return kZbBoostResultReached;
    case kBoostEndExpired:
        return kZbBoostResultExpired;
    case kBoostEndCancelled:
        return kZbBoostResultCancelled;
    case kBoostEndModeChanged:
        return kZbBoostResultModeChanged;
    case kBoostEndFault:
        return kZbBoostResultFault;
    case kBoostEndLimit:
        return kZbBoostResultLimit;
    default:
        return kZbBoostResultNone;
    }
}

/** Returns the kZbBoostResult* code reported to HA for a CtrlBoostStart() result. */
static U8 StartToResult(CtrlBoostResult r)
{
    switch (r)
    {
    case kBoostOk:
        return kZbBoostResultStarted;
    case kBoostErrFault:
        return kZbBoostResultRejectedFault;
    case kBoostErrAtTarget:
        return kZbBoostResultRejectedAtTarget;
    case kBoostErrNoDirection:
        return kZbBoostResultRejectedNoDirection;
    default:
        return kZbBoostResultRejectedArg;
    }
}

/**
 * Execute a queued boost command (app task): cancel, or start to-target / timed with the persisted
 * boost preferences. Records the result for HA and shows a banner when a start is rejected.
 */
static void HandleBoostCmd(Ctrl *pCtl, U8 cmd, U64 now)
{
    Persist p = SettingsGet();
    CtrlBoostResult r;
    switch (cmd)
    {
    case kZbBoostCmdCancel:
        if (CtrlGetBoostStatus(pCtl, now).kind != kBoostNone)
        {
            CtrlBoostCancel(pCtl);
            s_boostResult = kZbBoostResultCancelled;
        }
        return;
    case kZbBoostCmdStartTarget:
        r = CtrlBoostStart(pCtl, &p.s, kBoostToTarget, (CtrlBoostDir)p.boostDir, p.boostSp, 0, now);
        break;
    case kZbBoostCmdStartTimed:
        r = CtrlBoostStart(pCtl, &p.s, kBoostTimed, (CtrlBoostDir)p.boostDir, 0, p.boostMinutes, now);
        break;
    default:
        r = kBoostErrArg;
        break;
    }
    s_boostResult = StartToResult(r);
    ESP_LOGI(kTag, "boost cmd %u -> result %u", cmd, s_boostResult);
    if (r != kBoostOk)
        Banner(
            r == kBoostErrAtTarget      ? "Boost: already there"
            : r == kBoostErrFault       ? "Boost: sensor fault"
            : r == kBoostErrNoDirection ? "Boost: set mode HEAT/COOL"
                                        : "Boost: invalid");
}

/** Zigbee onIdentify callback: blink the LED for `secs` seconds. */
static void ZbOnIdentify(U16 secs)
{
    s_identifyS = secs;
}
static volatile U64 s_otaPctAt;

/**
 * Zigbee onOtaProgress callback: remember the percentage (-1 = finished/aborted) and when it arrived, for the banner.
 */
static void ZbOnOta(int pct)
{
    s_otaPct = pct;
    s_otaPctAt = NowMs();
}
#define OTA_STALE_MS (5 * 60 * 1000) /* no OTA progress for this long: the transfer is dead */

/** Zigbee beforeOtaReboot callback (Zigbee task): all outputs off before the restart. */
static void ZbBeforeReboot(void)
{
    HvacOutputsAllOff();
}

/* ---------------- buttons (esp_timer task -> queue) ---------------- */

/** UI callback (esp_timer task): queue the event for the app task; dropped if the queue is full. */
static void OnButton(UiButton which, BtnEvent ev)
{
    BtnMsg m = {(U8)which, (U8)ev};
    xQueueSend(s_btnQ, &m, 0);
}

/**
 * Main-screen key handling. UP/DOWN nudge the active mode's setpoint by one display unit (with
 * the mode Off, a banner explains how to change it). UP+DOWN toggles a timed boost and undoes
 * the nudge that the first of the two presses caused (if within 1.5 s).
 *
 * Returns true if the display should refresh promptly.
 */
static bool HandleHomeKey(const BtnMsg *pM)
{
    Persist p = SettingsGet();
    CtrlSettings s = p.s;
    int dir = pM->which == kUiBtnUp ? 1 : -1;
    const bool f = p.fahrenheit;
    static CtrlSettings s_beforeNudge;
    static U64 s_nudgeAt;

    switch (pM->which)
    {
    case kUiBtnCombo:
    {
        /* UP+DOWN together: toggle a timed boost. Undo the setpoint nudge the
         * first of the two presses caused. */
        if (NowMs() - s_nudgeAt < 1500)
            SettingsUpdate(&s_beforeNudge);
        U8 cmd = s_boostActive ? kZbBoostCmdCancel : kZbBoostCmdStartTimed;
        xQueueSend(s_boostQ, &cmd, 0);
        return true;
    }
    case kUiBtnUp:
    case kUiBtnDown:
        if (pM->ev != kBtnEvPress && pM->ev != kBtnEvRepeat)
            return false;
        if (UiComboActive())
            return false;
        if (pM->ev == kBtnEvPress)
        {
            s_beforeNudge = s;
            s_nudgeAt = NowMs();
        }
        switch (s.mode)
        {
        case kCtrlModeHeat:
            s.heatSp = MenuStepTemp(s.heatSp, dir, f);
            SettingsUpdate(&s);
            break;
        case kCtrlModeCool:
            s.coolSp = MenuStepTemp(s.coolSp, dir, f);
            SettingsUpdate(&s);
            break;
        default:
            Banner("Mode is OFF - M: menu > Mode");
            break;
        }
        return true;
    default:
        return false;
    }
    return false;
}

/**
 * Factory reset (app task): outputs off, settings erased, "FACTORY RESET" shown for ~3 s while both
 * watchdogs are fed, then the Zigbee network data is erased and the chip restarts. Does not return.
 */
static void FactoryReset(void)
{
    ESP_LOGW(kTag, "factory reset");
    HvacOutputsAllOff();
    SettingsErase();
    DisplayModel dm = {0};
    dm.pBanner = "FACTORY RESET";
    dm.fullEpoch = (U8)(s_fullEpoch + 1);
    DisplayPost(&dm, true);
    for (int i = 0; i < 12; i++)
    { /* ~3 s for the panel to show it; keep the dogs fed */
        vTaskDelay(pdMS_TO_TICKS(250));
        esp_task_wdt_reset();
        if (HwWatchdogArmed())
            HwWatchdogKick();
    }
    ZbThermostatFactoryReset(); /* erases Zigbee NVRAM and restarts */
    esp_restart();
}

/** Apply one edit function to the persisted settings under the mutex and mark them dirty. */
static void PersistEdit(void (*fn)(Persist *, I32), I32 v)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    fn(&s_persist, v);
    MarkDirty();
    xSemaphoreGive(s_mtx);
}

/** PersistEdit() setter: boost-to-target setpoint, 0.01 °C. */
static void SetBoostSp(Persist *pP, I32 v)
{
    pP->boostSp = (I16)v;
}

/** PersistEdit() setter: timed boost length, minutes. */
static void SetBoostMin(Persist *pP, I32 v)
{
    pP->boostMinutes = (U16)v;
}

/** PersistEdit() setter: swing, 0.01 °C (clamped). */
static void SetSwing(Persist *pP, I32 v)
{
    pP->swing = CtrlClampSwing(v);
}

/** Zigbee onSwing callback: store the swing (0.01 °C, clamped) and mark dirty. */
static void ZbOnSwing(I16 sw)
{
    PersistEdit(SetSwing, sw);
}

/** PersistEdit() setter: display units, non-zero = °F. */
static void SetUnits(Persist *pP, I32 v)
{
    pP->fahrenheit = v ? 1 : 0;
}

/**
 * Returns the menu's live data, built from a settings snapshot plus boost, link, firmware, raw
 * sensor and uptime state.
 */
static MenuCtx BuildMenuCtx(const Persist *pP, U64 now)
{
    return (MenuCtx){
        .mode = pP->s.mode,
        .fan = pP->s.fan,
        .boostActive = s_boostActive,
        .boostSp = pP->boostSp,
        .boostMinutes = pP->boostMinutes,
        .swing = pP->swing,
        .calib = (I16)pP->calibTenths * 10,
        .sched = pP->sched,
        .fahrenheit = pP->fahrenheit,
        .link = ZbThermostatLink(),
        .pFwVersion = FW_VERSION,
        .rawValid = s_raw.valid,
        .rawTemp = s_raw.tempC100,
        .rawRh = s_raw.rhC100,
        .uptimeS = (U32)(now / 1000),
        .safeMode = s_safeMode,
    };
}

/* ---------------- boot-loop guard ---------------- */

/**
 * Read/modify the boot-loop counter (NVS namespace "boot", key "n").
 *
 * Counts boots that did not reach BOOT_STABLE_MS of uptime. Whatever keeps resetting the chip
 * (watchdog, panic, brown-out), after BOOT_LOOP_LIMIT of them the thermostat stays up in safe
 * mode instead: outputs off, STWD100 not armed, the fault on screen and on Zigbee. Deliberate
 * software restarts (ESP_RST_SW: OTA reboot/rollback, factory reset, safe-mode retry, clearing
 * safe mode from the menu) are not counted. Power-on resets ARE counted, because an STWD100
 * reset pulls CHIP_EN and reports as ESP_RST_POWERON, so 5 quick power cycles also enter safe
 * mode (it ends by itself after SAFE_MODE_RETRY_MS). The counter lives in its own NVS namespace
 * and survives a factory reset.
 *
 * `op` is 1 = increment (saturating), 0 = clear, -1 = read only, 0x100|n = set to n.
 *
 * Returns the counter after the operation. Increment, read and set return that count even when
 * the write does not commit, so a boot that has already been counted still enters safe mode.
 * A clear returns 0 when the write commits and -1 when NVS cannot be opened or the write does
 * not commit. Opening NVS failing on any other op returns 0.
 */
static int BootCountRw(int op)
{
    nvs_handle_t h;
    U8 n = 0;
    if (!s_nvsOk || nvs_open("boot", NVS_READWRITE, &h) != ESP_OK)
        return op == 0 ? -1 : 0;
    if (nvs_get_u8(h, "n", &n) != ESP_OK)
        n = 0;
    if (op >= 0x100)
        n = (U8)(op & 0xFF);
    else if (op == 1)
        n = (U8)(n < 255 ? n + 1 : 255);
    else if (op == 0)
        n = 0;
    if (op != -1)
    {
        esp_err_t e = nvs_set_u8(h, "n", n);
        if (e == ESP_OK)
            e = nvs_commit(h);
        nvs_close(h);
        if (op == 0 && e != ESP_OK)
            return -1;
        return (int)n;
    }
    nvs_close(h);
    return (int)n;
}

/**
 * Write the settings now when force is set, otherwise once SAVE_DELAY_MS has passed since the
 * change. A failed write stays dirty and is tried again a minute later.
 */
static void SaveSettings(bool force)
{
    U64 now = NowMs();
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    bool save = s_dirty && (force || now >= s_dirtyAt + SAVE_DELAY_MS);
    if (save)
        s_dirty = false;
    Persist snap = s_persist;
    xSemaphoreGive(s_mtx);
    if (save && s_nvsOk && !SettingsSave(&snap))
    {
        /* write failed (flash full / worn / error): keep running, retry in a minute */
        EVT("EVT fault nvs write: settings not saved, retrying");
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        s_dirty = true;
        s_dirtyAt = now + 60000 - SAVE_DELAY_MS;
        xSemaphoreGive(s_mtx);
    }
}

/**
 * Carry out a menu action (app task): settings edits, boost commands (queued), pairing, factory
 * reset, or clearing safe mode. Bumps the full-refresh epoch when the menu asks for one.
 *
 * Returns true if the display should refresh promptly.
 */
static bool ApplyMenuAction(const MenuAction *pA)
{
    U8 cmd;
    Persist p = SettingsGet();
    switch (pA->kind)
    {
    case kMaSetMode:
        p.s.mode = (CtrlMode)pA->value;
        SettingsUpdate(&p.s);
        break;
    case kMaSetFan:
        p.s.fan = (CtrlFan)pA->value;
        SettingsUpdate(&p.s);
        break;
    case kMaBoostTarget:
        PersistEdit(SetBoostSp, pA->value);
        cmd = kZbBoostCmdStartTarget;
        xQueueSend(s_boostQ, &cmd, 0);
        break;
    case kMaBoostTimed:
        PersistEdit(SetBoostMin, pA->value);
        cmd = kZbBoostCmdStartTimed;
        xQueueSend(s_boostQ, &cmd, 0);
        break;
    case kMaBoostCancel:
        cmd = kZbBoostCmdCancel;
        xQueueSend(s_boostQ, &cmd, 0);
        break;
    case kMaSetSwing:
        PersistEdit(SetSwing, pA->value);
        break;
    case kMaSetOffset: /* same store as a Zigbee LocalTemperatureCalibration write */
        ZbOnCalib((I8)pA->value);
        break;
    case kMaSetSched:
    { /* persisted like any setting; ZbThermostatPublish() reports the change to HA */
        CtrlParams prm = CTRL_PARAMS_DEFAULT();
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        SchedSet(&s_persist.sched, (SchedItem)pA->item, pA->value);
        SchedSanitize(&prm, &s_persist.sched);
        MarkDirty();
        xSemaphoreGive(s_mtx);
        break;
    }
    case kMaSetUnits:
        PersistEdit(SetUnits, pA->value);
        break;
    case kMaPair:
        Banner(ZbThermostatLink() == kLinkJoined ? "Already paired (reset to re-pair)" : "Pairing...");
        ZbThermostatPair();
        break;
    case kMaFactoryReset:
        FactoryReset();
        break;
    case kMaClearSafeMode: /* counter back to 0, pending settings written, then a normal boot */
    {
        int n = BootCountRw(0);
        if (n == 0)
        {
            ESP_LOGW(kTag, "EVT safe mode: cleared from the menu");
            SaveSettings(true);
            esp_restart();
        }
        else
        {
            /* Stay up. The banner and the log say the clear was not saved. */
            ESP_LOGW(kTag, "EVT safe mode: clear not saved");
            s_clearNotSaved = true;
        }
        break;
    }
    default:
        break;
    }
    if (pA->fullRefresh)
        s_fullEpoch++;
    return pA->redraw || pA->fullRefresh;
}

/**
 * Route one button event (app task) through the menu; main-screen keys go to HandleHomeKey().
 * While UP+DOWN are held together inside the menu, their single events are ignored.
 *
 * Returns true if the display should refresh promptly.
 */
static bool HandleButton(const BtnMsg *pM)
{
    static const MenuKey kKeys[] = {kMenuKeyUp, kMenuKeyDown, kMenuKeyM, kMenuKeyCombo};
    Persist p = SettingsGet();
    MenuCtx c = BuildMenuCtx(&p, NowMs());
    /* while UP+DOWN are held together their single events are not menu moves */
    if ((pM->which == kUiBtnUp || pM->which == kUiBtnDown) && UiComboActive() && !MenuIsHome(&s_menu))
        return false;
    MenuAction a = MenuOnKey(&s_menu, &c, kKeys[pM->which], (BtnEvent)pM->ev, NowMs());
    if (a.kind == kMaHomeKey)
        return HandleHomeKey(pM);
    return ApplyMenuAction(&a);
}

/* ---------------- LED ---------------- */

/**
 * Drive the status LED: 2 Hz while identifying, fast blink on a control fault, a short blip while pairing, otherwise off (no heat near the sensor).
 */
static void LedUpdate(U64 now, const CtrlOutput *pO, LinkState link)
{
    bool on;
    if (s_identifyS)
        on = (now / 250) & 1; /* identify: 2 Hz */
    else if (pO->fault != kCtrlFaultNone)
        on = (now / 125) & 1; /* fault: fast */
    else if (link == kLinkPairing)
        on = (now % 2000) < 100; /* pairing: blip */
    else
        on = false; /* keep heat off the sensor */
    UiLed(on);
}

/* ---------------- main loop ---------------- */

/**
 * App task (priority 6), every LOOP_MS (250 ms): sensor read (every CONFIG_THERMO_SENSOR_PERIOD_MS,
 * with stuck detection) -> buttons, menu and boost commands -> schedule and control step -> outputs
 * -> Zigbee publish -> display -> delayed NVS save -> LED -> health check and watchdog kicks ->
 * boot-loop guard -> OTA image confirmation or rollback. Emits "EVT ..." log lines on every state
 * change (used by HOOTL).
 */
static void AppTask(void *pArg)
{
    CtrlParams prm = CTRL_PARAMS_DEFAULT();
    Ctrl ctl;
    CtrlInit(&ctl, &prm, NowMs());
    esp_task_wdt_add(NULL);

    SensorReading rd = {0};
    U64 nextSensor = 0, boot = NowMs(), lastIdentTick = boot;
    bool otaMarked = false, bootStable = false;
    HvacState evtOut = {0};
    bool evtOutInit = false, evtDispFault = false, evtOutFault = false, evtTime = false;
    int evtLink = -1, evtFault = -1;
    CtrlSettings evtS = {0};
    bool evtSInit = false;

    for (;;)
    {
        TickType_t wake = xTaskGetTickCount();
        U64 now = NowMs();
        Persist p = SettingsGet();
        bool urgent = false;

        /* 1. sensor */
        if (now >= nextSensor)
        {
            nextSensor = now + CONFIG_THERMO_SENSOR_PERIOD_MS;
            rd = SensorRead((I16)(CONFIG_THERMO_SELF_HEAT_OFFSET_C100 + p.calibTenths * 10));
            static SensorStuck s_stuck;
            static bool s_stuckEvt;
            bool frozen = rd.valid && SensorStuckUpdate(&s_stuck, rd.tempC100, rd.rhC100, now);
            if (frozen != s_stuckEvt)
            {
                EVT("EVT fault sensor frozen=%d", frozen);
                s_stuckEvt = frozen;
            }
            if (frozen)
                rd.valid = false; /* a frozen sensor is no sensor: failsafe */
            CtrlInput in = {.valid = rd.valid, .temp = rd.tempC100};
            CtrlFeedSensor(&ctl, &in, now);
            s_raw = rd;
            s_raw.tempC100 = (I16)(rd.tempC100 - (CONFIG_THERMO_SELF_HEAT_OFFSET_C100 + p.calibTenths * 10));
        }

        /* 2. buttons */
        BtnMsg m;
        while (xQueueReceive(s_btnQ, &m, 0) == pdTRUE)
            urgent |= HandleButton(&m);
        {
            MenuAction ma = MenuTick(&s_menu, now);
            urgent |= ApplyMenuAction(&ma);
        } /* 10 s idle */
        /* Device info shows the reading with calibration taken back off. */
        static bool s_sawInfo;
        if (s_menu.st == kMsInfo && !s_sawInfo && s_raw.valid)
            EVT("EVT raw temp=%d", (int)s_raw.tempC100);
        s_sawInfo = s_menu.st == kMsInfo;
        p = SettingsGet();
        U8 bcmd;
        while (xQueueReceive(s_boostQ, &bcmd, 0) == pdTRUE)
        {
            HandleBoostCmd(&ctl, bcmd, now);
            urgent = true;
        }

        if (ctl.p.hysteresis != p.swing)
            CtrlSetSwing(&ctl, p.swing);

        /* 3. control + outputs (the interlock lives in HvacOutputsSet) */
        /*    program schedule -> effective setpoints (incl. morning heat boost) */
        U16 minute = 0;
        U64 minuteStart = 0;
        bool timeKnown = ZbThermostatLocalClock(&minute, &minuteStart);
        CtrlSettings eff = p.s;
        SchedState sst = SchedApply(&prm, &p.sched, timeKnown, minute, &eff);
        CtrlOutput o = CtrlStep(&ctl, &eff, now);
        HvacState req = {.w = o.w, .y = o.y, .g = o.g};
        if (s_safeMode)
            req = (HvacState){0};
        HvacState act = HvacOutputsSet(req);
        bool outFault = !s_outputsOk || HvacOutputsDriveFault() != 0;
        if (!evtOutInit || act.w != evtOut.w || act.y != evtOut.y || act.g != evtOut.g)
        {
            EVT("EVT out W=%d Y=%d G=%d", act.w, act.y, act.g);
            evtOut = act;
            evtOutInit = true;
        }
        if (outFault && !evtOutFault)
        {
            EVT("EVT fault output mask=0x%x", HvacOutputsDriveFault());
            evtOutFault = true;
        }
        if (!outFault && evtOutFault)
        {
            EVT("EVT fault output cleared");
            evtOutFault = false;
        }
        if ((int)o.fault != evtFault)
        {
            EVT("EVT ctrl fault=%s", CtrlFaultStr(o.fault));
            evtFault = (int)o.fault;
        }
        if (!evtSInit || memcmp(&evtS, &eff, sizeof eff) != 0)
        {
            EVT("EVT setpoint mode=%d heat=%d cool=%d fan=%d sched=%d",
                eff.mode,
                eff.heatSp,
                eff.coolSp,
                eff.fan,
                (int)sst);
            evtS = eff;
            evtSInit = true;
        }
        if (timeKnown != evtTime)
        {
            EVT("EVT time known=%d minute=%u", timeKnown, minute);
            evtTime = timeKnown;
        }
        CtrlBoostStatus bs = CtrlGetBoostStatus(&ctl, now);
        if (s_boostActive && bs.kind == kBoostNone && bs.lastEnd != kBoostEndCancelled)
        {
            s_boostResult = EndToResult(bs.lastEnd); /* ended by itself */
            urgent = true;
        }
        s_boostActive = bs.kind != kBoostNone;
        U16 bmin = (U16)((bs.remainingS + 59) / 60);
        U8 bstate = kZbBoostStateOff;
        if (bs.kind == kBoostToTarget)
            bstate = bs.dir == kCtrlModeHeat ? kZbBoostStateHeatToTarget : kZbBoostStateCoolToTarget;
        if (bs.kind == kBoostTimed)
            bstate = bs.dir == kCtrlModeHeat ? kZbBoostStateHeatTimed : kZbBoostStateCoolTimed;

        /* 4. Zigbee */
        ZbThermostatState zs = {
            .tempValid = rd.valid && o.fault != kCtrlFaultTempRange,
            .temp = rd.tempC100,
            .rhValid = rd.valid,
            .rh = rd.rhC100,
            .s = p.s,
            .w = act.w,
            .y = act.y,
            .g = act.g,
            .boostState = bstate,
            .boostRemainingMin = bmin,
            .boostLastResult = s_boostResult,
            .boostSp = p.boostSp,
            .boostMinutes = p.boostMinutes,
            .swing = p.swing,
            .calib = p.calibTenths,
            .sched = p.sched,
            .schedState = (U8)sst,
        };
        ZbThermostatPublish(&zs);
        LinkState link = ZbThermostatLink();
        if ((int)link != evtLink)
        {
            EVT("EVT link %d", (int)link);
            evtLink = (int)link;
        }

        /* 5. display */
        static char s_otaBanner[24];
        DisplayModel dm;
        memset(&dm, 0, sizeof dm);
        dm.tempValid = zs.tempValid;
        /* quantise so sensor noise does not cause refreshes */
        dm.temp = (I16)((rd.tempC100 + (rd.tempC100 >= 0 ? 5 : -5)) / 10 * 10);
        dm.rhValid = rd.valid;
        dm.rh = (U16)((rd.rhC100 + 50) / 100 * 100);
        dm.s = p.s;
        dm.w = act.w;
        dm.y = act.y;
        dm.g = act.g;
        dm.waiting = o.waiting;
        dm.fault = o.fault;
        dm.link = link;
        dm.fahrenheit = p.fahrenheit;
        {
            MenuCtx mc = BuildMenuCtx(&p, now);
            if (!MenuIsHome(&s_menu))
                mc.rawTemp = (I16)(s_raw.tempC100 / 10 * 10); /* 0.1 steps: fewer refreshes */
            MenuGetView(&s_menu, &mc, &dm.view);
        }
        dm.fullEpoch = s_fullEpoch;
        dm.swing = p.swing;
        dm.schedState = (U8)sst;
        dm.clockKnown = timeKnown;
        dm.localMinute = timeKnown ? minute : 0;
        dm.minuteStartMs = timeKnown ? minuteStart : 0;
        dm.boost = bs.kind;
        dm.boostDir = bs.dir;
        dm.boostSp = bs.sp;
        dm.boostMinLeft = bs.kind == kBoostTimed ? bmin : 0;
        if (s_otaPct >= 0 && now - s_otaPctAt >= OTA_STALE_MS)
        {
            EVT("EVT ota stalled: no progress for %d min, banner cleared", OTA_STALE_MS / 60000);
            s_otaPct = -1;
        }
        if (s_otaPct >= 0)
        {
            snprintf(s_otaBanner, sizeof s_otaBanner, "Updating firmware %d%%", s_otaPct);
            dm.pBanner = s_otaBanner;
        }
        else if (s_safeMode)
        {
            dm.pBanner = s_clearNotSaved ? "SAFE MODE: clear not saved" : "SAFE MODE: repeated resets";
        }
        else if (outFault)
        {
            dm.pBanner = "OUTPUT FAULT - service";
        }
        else if (s_pBanner && now < s_bannerUntil)
        {
            dm.pBanner = s_pBanner;
        }
        else if (!s_nvsOk)
        {
            dm.pBanner = "Storage fault: not saved";
        }
        DisplayPost(&dm, urgent);

        /* 6. persistence */
        SaveSettings(false);

        /* 7. LED / identify */
        if (s_identifyS && now - lastIdentTick >= 1000)
        {
            s_identifyS--;
            lastIdentTick = now;
        }
        if (!s_identifyS)
            lastIdentTick = now;
        LedUpdate(now, &o, link);

        /* 8. health check -> watchdogs. Kick only if every check passed.
         *    The display is NOT part of it: a dead panel must not stop heating
         *    (or reboot-loop the thermostat); it is reported instead.
         *    `now` was sampled at the top of this loop. SensorRead() runs after that
         *    sample, and a failing read (I2C NAK and bus reset) takes long enough for
         *    the display task to stamp s_alive with a later time. A stamp ahead of
         *    `now` is not a stall: unsigned subtraction would wrap. */
        bool dispFault = !DisplayRunning() || DisplayHeartbeatStalled(DisplayLastAliveMs(), now);
        if (dispFault != evtDispFault)
        {
            EVT("EVT display fault=%d", dispFault);
            evtDispFault = dispFault;
        }
        bool healthy = HvacOutputsInterlockFaults() == 0;
        if (healthy)
        {
            esp_task_wdt_reset();
#if CONFIG_THERMO_HW_WDT_ENABLE
            if (!s_safeMode && !HwWatchdogArmed() && now - boot >= HWDT_ARM_AFTER_MS)
            {
                ESP_LOGI(kTag, "arming STWD100");
                HwWatchdogArm();
            }
            if (HwWatchdogArmed())
                HwWatchdogKick();
#endif
        }
        else
        {
            ESP_LOGE(kTag, "unhealthy: outputs off, letting watchdog bite");
            HvacOutputsAllOff();
        }

        /* 8b. boot-loop guard */
        if (!bootStable && !s_safeMode && healthy && now - boot >= BOOT_STABLE_MS)
        {
            bootStable = true;
            BootCountRw(0);
        }
        if (s_safeMode && now - boot >= SAFE_MODE_RETRY_MS)
        {
            ESP_LOGW(kTag, "EVT safe mode: retrying a normal boot");
            /* exactly one trial: one more counted reset goes straight back to safe mode */
            BootCountRw(0x100 | (BOOT_LOOP_LIMIT - 1));
            HvacOutputsAllOff();
            esp_restart();
        }

        /* 9. OTA rollback protection: confirm the image only once it is healthy
         *    AND back on the Zigbee network; if it never rejoins, roll back. */
        if (!otaMarked)
        {
            esp_ota_img_states_t st;
            const esp_partition_t *pRun = esp_ota_get_running_partition();
            bool pending = esp_ota_get_state_partition(pRun, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY;
            if (!pending)
            {
                otaMarked = true; /* factory/already-confirmed image: nothing to do */
            }
            else if (
                healthy && o.fault == kCtrlFaultNone && now - boot >= OTA_VALID_AFTER_MS &&
                ZbThermostatLink() == kLinkJoined)
            {
                ESP_LOGI(kTag, "new image healthy and joined: confirming");
                esp_ota_mark_app_valid_cancel_rollback();
                otaMarked = true;
            }
            else if (now - boot >= OTA_JOIN_TIMEOUT_MS)
            {
                esp_err_t rb = ESP_ERR_OTA_ROLLBACK_FAILED;
                /* Check first: forcing the outputs off and then carrying on would bypass
                 * the compressor/heat minimum off times. */
                if (esp_ota_check_rollback_is_possible())
                {
                    ESP_LOGE(kTag, "new image not confirmed within %d s: rolling back", OTA_JOIN_TIMEOUT_MS / 1000);
                    HvacOutputsAllOff();
                    rb = esp_ota_mark_app_invalid_rollback_and_reboot(); /* returns only on failure */
                }
                /* No valid image in the other slot: keep running this one instead of
                 * retrying every loop. */
                ESP_LOGE(kTag, "rollback failed (%s): keeping current image", esp_err_to_name(rb));
                esp_ota_mark_app_valid_cancel_rollback(); /* keep this image bootable after the next reset */
                otaMarked = true;
            }
        }

        vTaskDelayUntil(&wake, pdMS_TO_TICKS(LOOP_MS));
    }
}

/**
 * ESP-IDF entry point: outputs off and the external watchdog configured (disarmed) first, then NVS,
 * power management, boot-loop accounting, settings, sensor, display and buttons. Then the app task,
 * and finally Zigbee, so local control never waits for the radio. No failure here aborts or
 * reboot-loops; faults are logged and shown instead.
 */
void app_main(void)
{
    /* Outputs first: guarantee a known-off state before anything else. */
    s_outputsOk = HvacOutputsInit() == ESP_OK; /* failure latches an output fault (all off) */
    HwWatchdogInit();                          /* disarmed until the main loop proves itself */

    /* Nothing below may abort: a fault here must not turn into a reboot loop. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    s_nvsOk = err == ESP_OK;
    if (!s_nvsOk)
        EVT("EVT fault nvs init %s: running on defaults, nothing is saved", esp_err_to_name(err));

#if CONFIG_PM_ENABLE
    esp_pm_config_t pm = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .light_sleep_enable = true,
    };
    if (esp_pm_configure(&pm) != ESP_OK)
        ESP_LOGW(kTag, "power management not configured");
#endif

    esp_reset_reason_t rr = esp_reset_reason();
    int boots = BootCountRw(rr == ESP_RST_SW ? -1 : 1);
    EVT("EVT boot reason=%d count=%u", (int)rr, (unsigned)boots);
    s_safeMode = boots >= BOOT_LOOP_LIMIT;
    if (s_safeMode)
        EVT("EVT safe mode: %u resets without %d min of stable uptime", (unsigned)boots, BOOT_STABLE_MS / 60000);

    s_mtx = xSemaphoreCreateMutex();
    s_btnQ = xQueueCreate(16, sizeof(BtnMsg));
    s_boostQ = xQueueCreate(4, sizeof(U8));
    SettingsLoad(&s_persist);
    /* no clock yet at boot: mirror the day program */
    SchedProgramSp(&s_persist.sched, false, 0, &s_persist.s.heatSp, &s_persist.s.coolSp);
    MenuInit(&s_menu);
    ESP_LOGI(
        kTag,
        "settings: mode %d heat %d cool %d fan %d calib %d",
        s_persist.s.mode,
        s_persist.s.heatSp,
        s_persist.s.coolSp,
        s_persist.s.fan,
        s_persist.calibTenths);
    EVT("EVT cfg loaded mode=%d heat=%d cool=%d fan=%d",
        s_persist.s.mode,
        s_persist.s.heatSp,
        s_persist.s.coolSp,
        s_persist.s.fan);

    if (SensorInit() != ESP_OK)
        ESP_LOGE(kTag, "EVT fault sensor init (control stays in failsafe)");
    if (DisplayInit() != ESP_OK)
        ESP_LOGE(kTag, "EVT fault display init (control continues without a screen)");
    if (UiInit(OnButton) != ESP_OK)
        ESP_LOGE(kTag, "EVT fault buttons init (Zigbee control only)");

    /* Local control first: it never waits for Zigbee. */
    if (xTaskCreate(AppTask, "app", 4096, NULL, 6, NULL) != pdPASS)
    {
        ESP_LOGE(kTag, "app task: out of memory");
        esp_restart();
    }

    ZbThermostatCbs cbs = {
        .onSettings = ZbOnSettings,
        .onCalibration = ZbOnCalib,
        .onIdentify = ZbOnIdentify,
        .onOtaProgress = ZbOnOta,
        .beforeOtaReboot = ZbBeforeReboot,
        .onBoostParams = ZbOnBoostParams,
        .onBoostCommand = ZbOnBoostCmd,
        .onSwing = ZbOnSwing,
        .onSched = ZbOnSched,
    };
    ZbThermostatInit zi = {
        .s = s_persist.s,
        .calib = s_persist.calibTenths,
        .boostSp = s_persist.boostSp,
        .boostMinutes = s_persist.boostMinutes,
        .boostDir = s_persist.boostDir,
        .swing = s_persist.swing,
        .sched = s_persist.sched,
    };
    if (ZbThermostatStart(&cbs, &zi) != ESP_OK)
        ESP_LOGE(kTag, "Zigbee unavailable: local control only (radio fault shown)");
}
