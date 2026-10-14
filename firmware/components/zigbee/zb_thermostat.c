/* zb_thermostat.c - see zb_thermostat.h. Written against esp-zigbee-lib 1.6.x. */
#include "core.h"
#include "zb_thermostat.h"
#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_zigbee_core.h"
#include "esp_zigbee_ota.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "fw_version.h"
#include "zb_time.h"
#include "zb_retry.h"

static const char *kTag = "zb";

#define EP     ZB_THERMOSTAT_EP
#define SERVER ESP_ZB_ZCL_CLUSTER_SERVER_ROLE

/* ZCL char strings: first byte = length */
#define MANUFACTURER_NAME \
    "\x06"                \
    "Connie"
#define MODEL_ID \
    "\x0d"       \
    "thermostat-v3"
#define SW_BUILD_ID "\x05" FW_VERSION /* ZCL char string: length prefix, keep FW_VERSION 5 chars */

#define RUNNING_HEAT 0x0001
#define RUNNING_COOL 0x0002
#define RUNNING_FAN  0x0004

static ZbThermostatCbs s_cbs;
static I16 s_boostSp;
static U16 s_boostMin;
static U8 s_boostDir;
static U16 s_swing = CTRL_SWING_DEFAULT; /* kZbAttrSwing backing store */
static CtrlSettings s_settings;          /* last value known to the stack */
static volatile LinkState s_link = kLinkOffline;
static bool s_started;
static SchedCfg s_sched;              /* schedule attribute backing store */
static U8 s_schedState;               /* kZbAttrSchedState backing store */
static volatile bool s_forceSettings; /* republish mode/setpoints (e.g. rejected AUTO write) */

/* Radio failure handling. The thermostat never depends on Zigbee: a failure
 * here only changes what ZbThermostatLink() reports. */
static volatile bool s_radioFault; /* stack could not be started */
static volatile U64 s_aliveMs;     /* Zigbee task heartbeat (stall detection) */
static ZbRetry s_retry;            /* (re)join backoff, Zigbee task only */
static bool s_steerPending;        /* a (re)join attempt is scheduled or running */
#define HEARTBEAT_MS 5000

/* Time sync: local seconds since midnight at s_timeAtMs (monotonic). */
static U8 s_timeFails;     /* consecutive reads without a usable answer */
static bool s_timeReqOpen; /* a read is out and not yet answered usefully */
static volatile bool s_timeKnown;
static U32 s_timeLocalS; /* ZCL UTCTime-style seconds (local) at sync */
static U64 s_timeAtMs;
static portMUX_TYPE s_timeMux = portMUX_INITIALIZER_UNLOCKED;

/** Returns monotonic time in ms (esp_timer). */
static U64 MonoMs(void)
{
    return (U64)(esp_timer_get_time() / 1000);
}

/**
 * Returns a pointer to the SchedCfg field backing a schedule attribute id (I16 or U16, see
 * SchedAttrSigned()), or NULL if id is not a schedule attribute.
 */
static void *SchedField(SchedCfg *pC, U16 id)
{
    switch (id)
    {
    case kZbAttrDayCoolStart:
        return &pC->dayCoolStart;
    case kZbAttrDayCoolTemp:
        return &pC->dayCool;
    case kZbAttrNightCoolStart:
        return &pC->nightCoolStart;
    case kZbAttrNightCoolTemp:
        return &pC->nightCool;
    case kZbAttrDayHeatStart:
        return &pC->dayHeatStart;
    case kZbAttrDayHeatTemp:
        return &pC->dayHeat;
    case kZbAttrNightHeatStart:
        return &pC->nightHeatStart;
    case kZbAttrNightHeatTemp:
        return &pC->nightHeat;
    case kZbAttrMorningBoostMin:
        return &pC->boostMin;
    case kZbAttrMorningBoostOfs:
        return &pC->boostOffset;
    default:
        return NULL;
    }
}

/** Returns true if the schedule attribute is a signed temperature (S16), false for U16 times/minutes. */
static bool SchedAttrSigned(U16 id)
{
    return id == kZbAttrDayCoolTemp || id == kZbAttrNightCoolTemp || id == kZbAttrDayHeatTemp ||
           id == kZbAttrNightHeatTemp || id == kZbAttrMorningBoostOfs;
}

/* ------------------------------------------------------------------ */
/* time of day (Time cluster client -> coordinator)                    */
/* ------------------------------------------------------------------ */

/**
 * Zigbee scheduler alarm: if joined, read Time, TimeStatus, TimeZone, DstShift and LocalTime from
 * the coordinator, then reschedule itself (ZbTimeNextDelayMs()). A previous read that got no usable
 * answer counts as a failure. Runs in the Zigbee task.
 */
static void TimeRequest(U8 unused)
{
    (void)unused;
    if (s_timeReqOpen && s_timeFails < 255)
        s_timeFails++; /* last read: no or unusable answer */
    s_timeReqOpen = false;
    if (s_link == kLinkJoined)
    {
        /* LocalTime (0x0007) is what we want; Time + TimeZone + DstShift is the fallback. */
        static U16 s_ids[] = {
            ESP_ZB_ZCL_ATTR_TIME_TIME_ID,
            ESP_ZB_ZCL_ATTR_TIME_TIME_STATUS_ID,
            ESP_ZB_ZCL_ATTR_TIME_TIME_ZONE_ID,
            ESP_ZB_ZCL_ATTR_TIME_DST_SHIFT_ID,
            ESP_ZB_ZCL_ATTR_TIME_LOCAL_TIME_ID};
        esp_zb_zcl_read_attr_cmd_t r = {0};
        r.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000; /* coordinator */
        r.zcl_basic_cmd.dst_endpoint = 1;
        r.zcl_basic_cmd.src_endpoint = EP;
        r.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
        r.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TIME;
        r.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
        r.attr_number = sizeof s_ids / sizeof s_ids[0];
        r.attr_field = s_ids;
        if (esp_zb_zcl_read_attr_cmd_req(&r) == ESP_OK)
            s_timeReqOpen = true;
    }
    /* hourly once known (drift + DST changes); 1, 2, 5 min, then hourly while unknown */
    esp_zb_scheduler_alarm(TimeRequest, 0, ZbTimeNextDelayMs(s_timeFails, s_timeKnown));
}

/**
 * Handle the coordinator's Time read response: use LocalTime, or Time + TimeZone + DstShift if
 * LocalTime is missing, and accept it only if ZbTimeAcceptable(). On success, store it with the
 * monotonic timestamp under s_timeMux and reset the failure count; otherwise log it and let the
 * next TimeRequest() count the failure.
 */
static void OnTimeResp(const esp_zb_zcl_cmd_read_attr_resp_message_t *pM)
{
    bool haveUtc = false, haveLocal = false, haveStatus = false;
    U8 status = 0;
    U32 utc = 0, local = 0;
    I32 tz = 0, dst = 0;
    for (const esp_zb_zcl_read_attr_resp_variable_t *pV = pM->variables; pV; pV = pV->next)
    {
        if (pV->status != ESP_ZB_ZCL_STATUS_SUCCESS || !pV->attribute.data.value)
            continue;
        const void *pD = pV->attribute.data.value;
        switch (pV->attribute.id)
        {
        case ESP_ZB_ZCL_ATTR_TIME_TIME_ID:
            memcpy(&utc, pD, 4);
            haveUtc = utc != 0 && utc != 0xFFFFFFFFu;
            break;
        case ESP_ZB_ZCL_ATTR_TIME_TIME_STATUS_ID:
            status = *(const U8 *)pD;
            haveStatus = true;
            break;
        case ESP_ZB_ZCL_ATTR_TIME_TIME_ZONE_ID:
            memcpy(&tz, pD, 4);
            break;
        case ESP_ZB_ZCL_ATTR_TIME_DST_SHIFT_ID:
            memcpy(&dst, pD, 4);
            break;
        case ESP_ZB_ZCL_ATTR_TIME_LOCAL_TIME_ID:
            memcpy(&local, pD, 4);
            haveLocal = local != 0 && local != 0xFFFFFFFFu;
            break;
        default:
            break;
        }
    }
    if (!haveLocal && haveUtc)
    {
        local = (U32)((I64)utc + tz + dst);
        haveLocal = true;
    }
    if (!haveLocal || !ZbTimeAcceptable(haveStatus, status, local) ||
        (haveUtc && !ZbTimeAcceptable(haveStatus, status, utc)))
    {
        ESP_LOGW(kTag, "time sync: coordinator returned no usable time (status 0x%02x)", status);
        return; /* counted as a failure at the next attempt */
    }
    s_timeReqOpen = false;
    s_timeFails = 0;
    portENTER_CRITICAL(&s_timeMux);
    s_timeLocalS = local;
    s_timeAtMs = MonoMs();
    s_timeKnown = true;
    portEXIT_CRITICAL(&s_timeMux);
    ESP_LOGI(
        kTag,
        "time sync: local %02lu:%02lu",
        (unsigned long)(local / 3600 % 24),
        (unsigned long)(local / 60 % 60));
}

/** Start the self-rescheduling time poll chain (first read after 5 s). Only the first call does anything. */
static void TimeSyncStart(void)
{
    static bool s_running;
    if (s_running)
        return;
    s_running = true;
    esp_zb_scheduler_alarm(TimeRequest, 0, 5000);
}

/** Extrapolate from the last sync under the spinlock. See zb_thermostat.h. */
bool ZbThermostatLocalMinute(U16 *pMinute)
{
    portENTER_CRITICAL(&s_timeMux);
    bool known = s_timeKnown;
    U64 secs = (U64)s_timeLocalS + (MonoMs() - s_timeAtMs) / 1000u;
    portEXIT_CRITICAL(&s_timeMux);
    if (known)
        *pMinute = (U16)(secs % 86400u / 60u);
    return known;
}

/* ------------------------------------------------------------------ */
/* commissioning                                                      */
/* ------------------------------------------------------------------ */

/**
 * Zigbee scheduler alarm: start BDB commissioning in `mode`. For network steering, does nothing if
 * already joined again and otherwise shows pairing (or keeps "lost parent"). Clears s_steerPending
 * if the start fails.
 */
static void CommissioningCb(U8 mode)
{
    if (mode == ESP_ZB_BDB_MODE_NETWORK_STEERING)
    {
        s_steerPending = false;
        if (esp_zb_bdb_dev_joined())
            return; /* already back (e.g. stack rejoined by itself) */
        if (s_link != kLinkLost)
            s_link = kLinkPairing;
        s_steerPending = true;
    }
    if (esp_zb_bdb_start_top_level_commissioning(mode) != ESP_OK)
    {
        ESP_LOGE(kTag, "commissioning start failed");
        s_steerPending = false;
    }
}

/**
 * Schedule the next (re)join attempt after delayMs. At most one is outstanding
 * (s_steerPending).
 *
 * `pWhy` is the reason, for the EVT log line.
 */
static void ScheduleSteering(U32 delayMs, const char *pWhy)
{
    if (s_steerPending)
        return;
    s_steerPending = true;
    ESP_LOGI(kTag, "EVT zb retry in %lu ms (%s, attempt %u)", (unsigned long)delayMs, pWhy, (unsigned)s_retry.fails);
    esp_zb_scheduler_alarm(CommissioningCb, ESP_ZB_BDB_MODE_NETWORK_STEERING, delayMs);
}

/**
 * Zigbee scheduler alarm, every HEARTBEAT_MS: stamp s_aliveMs so ZbThermostatLink() can detect a stalled stack.
 */
static void Heartbeat(U8 unused)
{
    (void)unused;
    s_aliveMs = MonoMs();
    esp_zb_scheduler_alarm(Heartbeat, 0, HEARTBEAT_MS);
}

/**
 * esp-zigbee application signal handler (vendor callback, Zigbee task). Drives the link state and
 * the join backoff: init, then rejoin or start steering when factory-new; steering result;
 * leave; parent lost; and the light sleep request (esp_zb_sleep_now()).
 */
void esp_zb_app_signal_handler(esp_zb_app_signal_t *pSigS)
{
    U32 *pP = pSigS->p_app_signal;
    esp_err_t st = pSigS->esp_err_status;
    esp_zb_app_signal_type_t sig = *pP;

    switch (sig)
    {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (st != ESP_OK)
        {
            U32 d = ZbRetryFailed(&s_retry);
            ESP_LOGW(kTag, "stack init failed (%s), retrying in %lu ms", esp_err_to_name(st), (unsigned long)d);
            if (!esp_zb_bdb_is_factory_new())
                s_link = kLinkLost; /* joined before: show "lost parent", not "offline" */
            esp_zb_scheduler_alarm(CommissioningCb, ESP_ZB_BDB_MODE_INITIALIZATION, d);
            break;
        }
        if (esp_zb_bdb_is_factory_new())
        {
            ESP_LOGI(kTag, "factory new: start network steering (pairing)");
            s_link = kLinkPairing;
            s_steerPending = true;
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        }
        else
        {
            ESP_LOGI(kTag, "rejoined existing network");
            s_link = kLinkJoined;
            ZbRetryJoined(&s_retry, MonoMs());
            TimeSyncStart();
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (st == ESP_OK)
        {
            ESP_LOGI(
                kTag,
                "joined PAN 0x%04hx ch %d addr 0x%04hx",
                esp_zb_get_pan_id(),
                esp_zb_get_current_channel(),
                esp_zb_get_short_address());
            s_steerPending = false;
            s_link = kLinkJoined;
            ZbRetryJoined(&s_retry, MonoMs());
            TimeSyncStart();
        }
        else
        {
            /* Keep trying with backoff (zb_retry.h). The thermostat works fine unpaired. */
            s_steerPending = false;
            if (s_link != kLinkLost)
                s_link = kLinkPairing;
            ScheduleSteering(ZbRetryFailed(&s_retry), "steering failed");
        }
        break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE:
        /* Removed from the network (e.g. by the coordinator): go back to pairing, slowly. */
        ESP_LOGW(kTag, "left network");
        s_link = kLinkOffline;
        ScheduleSteering(ZbRetryLost(&s_retry, MonoMs()), "left network");
        break;
    case ESP_ZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT:
        ESP_LOGW(kTag, "parent lost");
        s_link = kLinkLost;
        ScheduleSteering(ZbRetryLost(&s_retry, MonoMs()), "parent lost");
        break;
    case ESP_ZB_COMMON_SIGNAL_CAN_SLEEP:
        esp_zb_sleep_now(); /* light sleep until the next poll / timer / GPIO */
        break;
    default:
        ESP_LOGD(kTag, "signal %s (0x%x) %s", esp_zb_zdo_signal_to_string(sig), sig, esp_err_to_name(st));
        break;
    }
}

/** Radio fault, stall check, then the tracked state. See zb_thermostat.h. */
LinkState ZbThermostatLink(void)
{
    if (s_radioFault)
        return kLinkFault;
    if (s_started && ZbRetryStalled(s_aliveMs, MonoMs()))
        return kLinkFault; /* task dead or stuck */
    return s_link;
}

/* Never wait for the stack forever: a stalled Zigbee task must not freeze the UI/control loop. */
#define LOCK_WAIT_MS 1000

/** See zb_thermostat.h. */
void ZbThermostatPair(void)
{
    if (!s_started || ZbThermostatLink() == kLinkFault)
        return;
    if (!esp_zb_lock_acquire(pdMS_TO_TICKS(LOCK_WAIT_MS)))
    {
        ESP_LOGE(kTag, "pair: stack busy");
        return;
    }
    if (!esp_zb_bdb_dev_joined())
    {
        s_link = kLinkPairing;
        s_retry.fails = 0; /* user asked: start the backoff over */
        s_steerPending = true;
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
    }
    esp_zb_lock_release();
}

/** See zb_thermostat.h. */
void ZbThermostatFactoryReset(void)
{
    if (!s_started)
        return;
    if (!esp_zb_lock_acquire(pdMS_TO_TICKS(LOCK_WAIT_MS)))
    {
        ESP_LOGE(kTag, "factory reset: Zigbee stack not responding, network data kept");
        return; /* caller restarts anyway */
    }
    esp_zb_factory_reset(); /* erases zb_storage and reboots */
    esp_zb_lock_release();
}

/* ------------------------------------------------------------------ */
/* remote attribute writes                                            */
/* ------------------------------------------------------------------ */

/**
 * Handle a remote attribute write (Zigbee task, stack lock held). Thermostat setpoints/mode and
 * fan mode go to onSettings. An unsupported system mode is refused and the real one
 * republished. Calibration and identify are passed on. Boost cluster: parameters are clamped
 * and passed to onBoostParams; the command goes to onBoostCommand; swing and schedule writes
 * are clamped, written back so a read-back shows the stored value, and passed on.
 *
 * Returns ESP_OK, or ESP_ERR_INVALID_ARG for a failed message.
 */
static esp_err_t OnSetAttr(const esp_zb_zcl_set_attr_value_message_t *pM)
{
    ESP_RETURN_ON_FALSE(pM && pM->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, kTag, "bad msg");
    if (pM->info.dst_endpoint != EP || !pM->attribute.data.value)
        return ESP_OK;
    const void *pV = pM->attribute.data.value;
    CtrlSettings s = s_settings;
    U8 what = kZbSetModeFan;
    bool changed = false;

    switch (pM->info.cluster)
    {
    case ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT:
        switch (pM->attribute.id)
        {
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID:
            s.heatSp = *(const I16 *)pV;
            what = kZbSetHeatSp;
            changed = true;
            break;
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ID:
            s.coolSp = *(const I16 *)pV;
            what = kZbSetCoolSp;
            changed = true;
            break;
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID:
        {
            U8 mode = *(const U8 *)pV;
            if (CtrlModeValid(mode))
            {
                s.mode = (CtrlMode)mode;
                changed = true;
            }
            else
            { /* e.g. Auto / Heat-Cool: not supported, put the real mode back */
                ESP_LOGW(kTag, "unsupported system mode %u ignored", mode);
                s_forceSettings = true;
            }
            break;
        }
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_LOCAL_TEMPERATURE_CALIBRATION_ID:
            if (s_cbs.onCalibration)
                s_cbs.onCalibration(*(const I8 *)pV);
            break;
        }
        break;
    case ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL:
        if (pM->attribute.id == ESP_ZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_ID)
        {
            U8 f = *(const U8 *)pV;
            /* Single-speed blower: anything other than Auto means On. */
            s.fan = (f == ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_AUTO || f == ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_OFF)
                        ? kCtrlFanAuto
                        : kCtrlFanOn;
            changed = true;
        }
        break;
    case ZB_BOOST_CLUSTER_ID:
    {
        I16 sp = s_boostSp;
        U16 mins = s_boostMin;
        U8 dir = s_boostDir;
        switch (pM->attribute.id)
        {
        case kZbBoostAttrSetpoint:
            sp = *(const I16 *)pV;
            break;
        case kZbBoostAttrDuration:
            mins = *(const U16 *)pV;
            break;
        case kZbBoostAttrMode:
            dir = *(const U8 *)pV;
            break;
        case kZbBoostAttrCommand:
            if (s_cbs.onBoostCommand)
                s_cbs.onBoostCommand(*(const U8 *)pV);
            return ESP_OK;
        case kZbAttrSwing:
        {
            U16 raw = *(const U16 *)pV;
            I16 sw = CtrlClampSwing(raw > 1000 ? 1000 : raw);
            if (sw != (I16)raw)
            { /* store the clamped value so a read-back shows reality */
                s_swing = (U16)sw;
                esp_zb_zcl_set_manufacturer_attribute_val(
                    EP,
                    ZB_BOOST_CLUSTER_ID,
                    SERVER,
                    ZB_BOOST_MANUF_CODE,
                    kZbAttrSwing,
                    &s_swing,
                    false);
            }
            s_swing = (U16)sw;
            if (s_cbs.onSwing)
                s_cbs.onSwing(sw);
            return ESP_OK;
        }
        default:
        {
            void *pF = SchedField(&s_sched, pM->attribute.id);
            if (!pF)
                return ESP_OK;
            SchedCfg c = s_sched;
            pF = SchedField(&c, pM->attribute.id);
            if (SchedAttrSigned(pM->attribute.id))
                *(I16 *)pF = *(const I16 *)pV;
            else
                *(U16 *)pF = *(const U16 *)pV;
            CtrlParams prm = CTRL_PARAMS_DEFAULT();
            SchedSanitize(&prm, &c);
            s_sched = c;
            /* store the clamped value so a read-back shows reality */
            esp_zb_zcl_set_manufacturer_attribute_val(
                EP,
                ZB_BOOST_CLUSTER_ID,
                SERVER,
                ZB_BOOST_MANUF_CODE,
                pM->attribute.id,
                SchedField(&s_sched, pM->attribute.id),
                false);
            if (s_cbs.onSched)
                s_cbs.onSched(&s_sched);
            return ESP_OK;
        }
        }
        if (mins < BOOST_TIMED_MIN_MIN)
            mins = BOOST_TIMED_MIN_MIN;
        if (mins > BOOST_TIMED_MAX_MIN)
            mins = BOOST_TIMED_MAX_MIN;
        if (dir > kBoostDirCool)
            dir = kBoostDirAuto;
        s_boostSp = sp;
        s_boostMin = mins;
        s_boostDir = dir;
        if (s_cbs.onBoostParams)
            s_cbs.onBoostParams(sp, mins, dir);
        return ESP_OK;
    }
    case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY:
        if (pM->attribute.id == ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID && s_cbs.onIdentify)
            s_cbs.onIdentify(*(const U16 *)pV);
        break;
    }
    if (changed)
    {
        /* The stack attribute now holds the written value: remember it so a second
         * write before the app re-publishes (e.g. mode then fan in one burst) starts
         * from it instead of the stale copy and does not undo the first write. */
        s_settings = s;
        if (s_cbs.onSettings)
            s_cbs.onSettings(&s, what);
    }
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* OTA upgrade client                                                 */
/* ------------------------------------------------------------------ */

#define OTA_ELEMENT_HEADER_LEN 6
#define OTA_TAG_UPGRADE_IMAGE  0x0000
static const esp_partition_t *s_pOtaPart;
static esp_ota_handle_t s_ota;
static bool s_otaTagSeen;
static U32 s_otaOff, s_otaTotal;

/**
 * Strip the 6-byte OTA element header (tag + length) from the first chunk of the image,
 * checking that it is an upgrade image whose length matches the total. Later chunks pass
 * through unchanged.
 *
 * `total` is the image size from the OTA header; `pP`/`n` are the chunk; `pOut`/`pOutlen`
 * receive the start and length of the payload to write.
 *
 * Returns ESP_OK, ESP_ERR_INVALID_ARG (bad tag/element) or ESP_ERR_INVALID_SIZE.
 */
static esp_err_t OtaElement(U32 total, const U8 *pP, U16 n, const U8 **pOut, U16 *pOutlen)
{
    if (!s_otaTagSeen)
    {
        ESP_RETURN_ON_FALSE(pP && n > OTA_ELEMENT_HEADER_LEN, ESP_ERR_INVALID_ARG, kTag, "bad element");
        U16 tag;
        U32 len;
        memcpy(&tag, pP, 2);
        memcpy(&len, pP + 2, 4);
        ESP_RETURN_ON_FALSE(tag == OTA_TAG_UPGRADE_IMAGE, ESP_ERR_INVALID_ARG, kTag, "unsupported tag %u", tag);
        ESP_RETURN_ON_FALSE(len + OTA_ELEMENT_HEADER_LEN == total, ESP_ERR_INVALID_SIZE, kTag, "bad length");
        s_otaTagSeen = true;
        *pOut = pP + OTA_ELEMENT_HEADER_LEN;
        *pOutlen = n - OTA_ELEMENT_HEADER_LEN;
    }
    else
    {
        *pOut = pP;
        *pOutlen = n;
    }
    return ESP_OK;
}

/** Abort any open OTA write handle and reset the transfer state. */
static void OtaReset(void)
{
    if (s_ota)
        esp_ota_abort(s_ota);
    s_ota = 0;
    s_otaOff = s_otaTotal = 0;
    s_otaTagSeen = false;
}

/**
 * OTA client state machine (Zigbee task): START opens the next OTA partition, RECEIVE writes payload
 * (reporting progress in %), CHECK verifies the byte count, FINISH validates, sets the boot partition,
 * calls beforeOtaReboot and restarts, and ABORT resets. Any error resets the transfer and reports
 * progress -1.
 */
static esp_err_t OnOta(const esp_zb_zcl_ota_upgrade_value_message_t *pM)
{
    esp_err_t ret = ESP_OK;
    if (pM->info.status != ESP_ZB_ZCL_STATUS_SUCCESS)
        return ESP_OK;
    switch (pM->upgrade_status)
    {
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_START:
        ESP_LOGI(kTag, "OTA start");
        OtaReset(); /* drop any handle left by an earlier failed transfer */
        s_pOtaPart = esp_ota_get_next_update_partition(NULL);
        ESP_RETURN_ON_FALSE(s_pOtaPart, ESP_FAIL, kTag, "no OTA partition");
        ret = esp_ota_begin(s_pOtaPart, OTA_WITH_SEQUENTIAL_WRITES, &s_ota);
        if (ret != ESP_OK)
            s_ota = 0;
        break;
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE:
    {
        s_otaTotal = pM->ota_header.image_size;
        s_otaOff += pM->payload_size;
        const U8 *pD;
        U16 n;
        if (pM->payload_size && pM->payload)
        {
            ret = OtaElement(s_otaTotal, pM->payload, pM->payload_size, &pD, &n);
            if (ret == ESP_OK)
                ret = esp_ota_write(s_ota, pD, n);
            if (ret != ESP_OK)
                ESP_LOGE(kTag, "OTA write failed: %s", esp_err_to_name(ret));
        }
        if (s_cbs.onOtaProgress && s_otaTotal)
            s_cbs.onOtaProgress((int)((U64)s_otaOff * 100 / s_otaTotal));
        break;
    }
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_CHECK:
        ret = (s_otaOff == s_otaTotal) ? ESP_OK : ESP_FAIL;
        break;
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_FINISH:
        ret = esp_ota_end(s_ota);
        s_ota = 0; /* esp_ota_end frees the handle even on failure */
        if (ret != ESP_OK)
        {
            ESP_LOGE(kTag, "ota end: %s", esp_err_to_name(ret));
            break;
        }
        ret = esp_ota_set_boot_partition(s_pOtaPart);
        if (ret != ESP_OK)
        {
            ESP_LOGE(kTag, "set boot: %s", esp_err_to_name(ret));
            break;
        }
        ESP_LOGW(kTag, "OTA done (v0x%08lx), rebooting", (unsigned long)pM->ota_header.file_version);
        if (s_cbs.beforeOtaReboot)
            s_cbs.beforeOtaReboot();
        esp_restart();
        break;
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_ABORT:
        ESP_LOGW(kTag, "OTA aborted");
        OtaReset();
        if (s_cbs.onOtaProgress)
            s_cbs.onOtaProgress(-1);
        break;
    default:
        break;
    }
    if (ret != ESP_OK)
    {
        OtaReset();
        if (s_cbs.onOtaProgress)
            s_cbs.onOtaProgress(-1);
    }
    return ret;
}

/**
 * esp-zigbee core action callback: dispatch attribute writes, OTA events and Time read responses; approve offered OTA images.
 */
static esp_err_t ActionHandler(esp_zb_core_action_callback_id_t id, const void *pMsg)
{
    switch (id)
    {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        return OnSetAttr((const esp_zb_zcl_set_attr_value_message_t *)pMsg);
    case ESP_ZB_CORE_OTA_UPGRADE_VALUE_CB_ID:
        return OnOta((const esp_zb_zcl_ota_upgrade_value_message_t *)pMsg);
    case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
    {
        const esp_zb_zcl_cmd_read_attr_resp_message_t *pR = pMsg;
        if (pR->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_TIME)
            OnTimeResp(pR);
        return ESP_OK;
    }
    case ESP_ZB_CORE_OTA_UPGRADE_QUERY_IMAGE_RESP_CB_ID:
    {
        const esp_zb_zcl_ota_upgrade_query_image_resp_message_t *pR = pMsg;
        if (pR->info.status == ESP_ZB_ZCL_STATUS_SUCCESS)
            ESP_LOGI(
                kTag,
                "OTA image available: v0x%08lx, %lu bytes",
                (unsigned long)pR->file_version,
                (unsigned long)pR->image_size);
        return ESP_OK; /* approve */
    }
    default:
        ESP_LOGD(kTag, "action 0x%x", id);
        return ESP_OK;
    }
}

/* ------------------------------------------------------------------ */
/* data model                                                         */
/* ------------------------------------------------------------------ */

/* any failure building the data model = radio fault (no reboot loop) */
#define TRY(x)                                                 \
    do                                                         \
    {                                                          \
        esp_err_t _e = (x);                                    \
        if (_e != ESP_OK)                                      \
        {                                                      \
            ESP_LOGE(kTag, "%s: %s", #x, esp_err_to_name(_e)); \
            return NULL;                                       \
        }                                                      \
    } while (0)

/**
 * Build endpoint 1: Basic, Identify, Thermostat, Fan Control, Temperature and Humidity
 * Measurement, the manufacturer Boost cluster (boost, swing and schedule attributes backed by
 * this file's statics) and the Time and OTA clients.
 *
 * `pInit` is the initial mode, fan and setpoints; `calib` is the LocalTemperatureCalibration, 0.1 °C.
 *
 * Returns the endpoint list, or NULL if any cluster could not be added (treated as a radio
 * fault).
 */
static esp_zb_ep_list_t *BuildEndpoint(const CtrlSettings *pInit, I8 calib)
{
    esp_zb_cluster_list_t *pCl = esp_zb_zcl_cluster_list_create();

    /* Basic: mains powered (0x01 = mains single phase; closest for 24 VAC) */
    esp_zb_basic_cluster_cfg_t basicCfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = 0x01};
    esp_zb_attribute_list_t *pBasic = esp_zb_basic_cluster_create(&basicCfg);
    esp_zb_basic_cluster_add_attr(pBasic, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(pBasic, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, MODEL_ID);
    esp_zb_basic_cluster_add_attr(pBasic, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, SW_BUILD_ID);
    TRY(esp_zb_cluster_list_add_basic_cluster(pCl, pBasic, SERVER));

    esp_zb_identify_cluster_cfg_t idCfg = {.identify_time = 0};
    TRY(esp_zb_cluster_list_add_identify_cluster(pCl, esp_zb_identify_cluster_create(&idCfg), SERVER));

    /* Thermostat */
    esp_zb_thermostat_cluster_cfg_t thCfg = {
        .local_temperature = (I16)0x8000, /* invalid until first reading */
        .occupied_cooling_setpoint = pInit->coolSp,
        .occupied_heating_setpoint = pInit->heatSp,
        .control_sequence_of_operation = 0x04, /* cooling and heating (no Auto: writes of 1 are refused) */
        .system_mode = (U8)pInit->mode,
    };
    esp_zb_attribute_list_t *pTh = esp_zb_thermostat_cluster_create(&thCfg);
    CtrlParams p = CTRL_PARAMS_DEFAULT();
    I16 minSp = p.minSp, maxSp = p.maxSp;
    U16 running = 0;
    esp_zb_thermostat_cluster_add_attr(pTh, ESP_ZB_ZCL_ATTR_THERMOSTAT_ABS_MIN_HEAT_SETPOINT_LIMIT_ID, &minSp);
    esp_zb_thermostat_cluster_add_attr(pTh, ESP_ZB_ZCL_ATTR_THERMOSTAT_ABS_MAX_HEAT_SETPOINT_LIMIT_ID, &maxSp);
    esp_zb_thermostat_cluster_add_attr(pTh, ESP_ZB_ZCL_ATTR_THERMOSTAT_ABS_MIN_COOL_SETPOINT_LIMIT_ID, &minSp);
    esp_zb_thermostat_cluster_add_attr(pTh, ESP_ZB_ZCL_ATTR_THERMOSTAT_ABS_MAX_COOL_SETPOINT_LIMIT_ID, &maxSp);
    esp_zb_thermostat_cluster_add_attr(pTh, ESP_ZB_ZCL_ATTR_THERMOSTAT_MIN_HEAT_SETPOINT_LIMIT_ID, &minSp);
    esp_zb_thermostat_cluster_add_attr(pTh, ESP_ZB_ZCL_ATTR_THERMOSTAT_MAX_HEAT_SETPOINT_LIMIT_ID, &maxSp);
    esp_zb_thermostat_cluster_add_attr(pTh, ESP_ZB_ZCL_ATTR_THERMOSTAT_MIN_COOL_SETPOINT_LIMIT_ID, &minSp);
    esp_zb_thermostat_cluster_add_attr(pTh, ESP_ZB_ZCL_ATTR_THERMOSTAT_MAX_COOL_SETPOINT_LIMIT_ID, &maxSp);
    esp_zb_thermostat_cluster_add_attr(pTh, ESP_ZB_ZCL_ATTR_THERMOSTAT_LOCAL_TEMPERATURE_CALIBRATION_ID, &calib);
    esp_zb_thermostat_cluster_add_attr(pTh, ESP_ZB_ZCL_ATTR_THERMOSTAT_THERMOSTAT_RUNNING_STATE_ID, &running);
    TRY(esp_zb_cluster_list_add_thermostat_cluster(pCl, pTh, SERVER));

    /* Fan control: On/Auto */
    esp_zb_fan_control_cluster_cfg_t fanCfg = {
        .fan_mode = (U8)pInit->fan,
        .fan_mode_sequence = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_SEQUENCE_ON_AUTO,
    };
    TRY(esp_zb_cluster_list_add_fan_control_cluster(pCl, esp_zb_fan_control_cluster_create(&fanCfg), SERVER));

    esp_zb_temperature_meas_cluster_cfg_t tCfg = {
        .measured_value = (I16)0x8000,
        .min_value = -4500,
        .max_value = 13000,
    };
    TRY(esp_zb_cluster_list_add_temperature_meas_cluster(pCl, esp_zb_temperature_meas_cluster_create(&tCfg), SERVER));

    esp_zb_humidity_meas_cluster_cfg_t hCfg = {.measured_value = 0xFFFF, .min_value = 0, .max_value = 10000};
    TRY(esp_zb_cluster_list_add_humidity_meas_cluster(pCl, esp_zb_humidity_meas_cluster_create(&hCfg), SERVER));

    /* Manufacturer-specific Boost cluster (see zb_boost.h). */
    esp_zb_attribute_list_t *pBc = esp_zb_zcl_attr_list_create(ZB_BOOST_CLUSTER_ID);
    static U8 s_bCmd = 0, s_bState = 0, s_bResult = 0;
    static U16 s_bRem = 0;
    const U8 rw = ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_MANUF_SPEC;
    const U8 ro = ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING | ESP_ZB_ZCL_ATTR_MANUF_SPEC;
    /* TRY() logs #x, the call's text. Breaking right after '(' would add a space to those log
     * strings (and to .rodata), so this block keeps its original layout. */
    /* clang-format off */
    TRY(esp_zb_cluster_add_manufacturer_attr(pBc, ZB_BOOST_CLUSTER_ID, kZbBoostAttrSetpoint, ZB_BOOST_MANUF_CODE,
                                             ESP_ZB_ZCL_ATTR_TYPE_S16, rw, &s_boostSp));
    TRY(esp_zb_cluster_add_manufacturer_attr(pBc, ZB_BOOST_CLUSTER_ID, kZbBoostAttrDuration, ZB_BOOST_MANUF_CODE,
                                             ESP_ZB_ZCL_ATTR_TYPE_U16, rw, &s_boostMin));
    TRY(esp_zb_cluster_add_manufacturer_attr(pBc, ZB_BOOST_CLUSTER_ID, kZbBoostAttrMode, ZB_BOOST_MANUF_CODE,
                                             ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, rw, &s_boostDir));
    TRY(esp_zb_cluster_add_manufacturer_attr(pBc, ZB_BOOST_CLUSTER_ID, kZbBoostAttrCommand, ZB_BOOST_MANUF_CODE,
                                             ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, rw, &s_bCmd));
    TRY(esp_zb_cluster_add_manufacturer_attr(pBc, ZB_BOOST_CLUSTER_ID, kZbBoostAttrState, ZB_BOOST_MANUF_CODE,
                                             ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, ro, &s_bState));
    TRY(esp_zb_cluster_add_manufacturer_attr(pBc, ZB_BOOST_CLUSTER_ID, kZbBoostAttrRemaining, ZB_BOOST_MANUF_CODE,
                                             ESP_ZB_ZCL_ATTR_TYPE_U16, ro, &s_bRem));
    TRY(esp_zb_cluster_add_manufacturer_attr(pBc, ZB_BOOST_CLUSTER_ID, kZbBoostAttrLastResult, ZB_BOOST_MANUF_CODE,
                                             ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, ro, &s_bResult));
    TRY(esp_zb_cluster_add_manufacturer_attr(pBc, ZB_BOOST_CLUSTER_ID, kZbAttrSwing, ZB_BOOST_MANUF_CODE,
                                             ESP_ZB_ZCL_ATTR_TYPE_U16, rw, &s_swing));
    for (U16 id = ZB_SCHED_ATTR_FIRST; id <= ZB_SCHED_ATTR_LAST; id++)
        TRY(esp_zb_cluster_add_manufacturer_attr(pBc, ZB_BOOST_CLUSTER_ID, id, ZB_BOOST_MANUF_CODE,
                                                 SchedAttrSigned(id) ? ESP_ZB_ZCL_ATTR_TYPE_S16 : ESP_ZB_ZCL_ATTR_TYPE_U16,
                                                 rw, SchedField(&s_sched, id)));
    TRY(esp_zb_cluster_add_manufacturer_attr(pBc, ZB_BOOST_CLUSTER_ID, kZbAttrSchedState, ZB_BOOST_MANUF_CODE,
                                             ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, ro, &s_schedState));
    TRY(esp_zb_cluster_list_add_custom_cluster(pCl, pBc, SERVER));

    /* Time client: we read the coordinator's clock for the program schedule. */
    esp_zb_time_cluster_cfg_t timeCfg = {0};
    TRY(esp_zb_cluster_list_add_time_cluster(pCl, esp_zb_time_cluster_create(&timeCfg),
                                             ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));
    /* clang-format on */

    /* OTA upgrade client */
    esp_zb_ota_cluster_cfg_t otaCfg = {
        .ota_upgrade_file_version = FW_OTA_FILE_VERSION,
        .ota_upgrade_downloaded_file_ver = FW_OTA_FILE_VERSION,
        .ota_upgrade_manufacturer = CONFIG_THERMO_OTA_MANUFACTURER,
        .ota_upgrade_image_type = CONFIG_THERMO_OTA_IMAGE_TYPE,
    };
    esp_zb_attribute_list_t *pOta = esp_zb_ota_cluster_create(&otaCfg);
    esp_zb_zcl_ota_upgrade_client_variable_t otaVar = {
        .timer_query = ESP_ZB_ZCL_OTA_UPGRADE_QUERY_TIMER_COUNT_DEF, /* minutes (1 day) */
        .hw_version = CONFIG_THERMO_HW_VERSION,
        .max_data_size = 223,
    };
    U16 srvAddr = 0xffff;
    U8 srvEp = 0xff;
    esp_zb_ota_cluster_add_attr(pOta, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_CLIENT_DATA_ID, &otaVar);
    esp_zb_ota_cluster_add_attr(pOta, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ADDR_ID, &srvAddr);
    esp_zb_ota_cluster_add_attr(pOta, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ENDPOINT_ID, &srvEp);
    TRY(esp_zb_cluster_list_add_ota_cluster(pCl, pOta, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));

    esp_zb_ep_list_t *pEps = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t epCfg = {
        .endpoint = EP,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_THERMOSTAT_DEVICE_ID,
        .app_device_version = 0,
    };
    TRY(esp_zb_ep_list_add_ep(pEps, pCl, epCfg));
    return pEps;
}

static CtrlSettings s_init;
static I8 s_initCalib;

/**
 * Zigbee task: init the stack as a sleepy end device, register the endpoint and action handler,
 * start the stack and run its main loop forever. On failure the radio fault is recorded and the
 * task deletes itself (no reboot).
 */
static void ZbTask(void *pArg)
{
    esp_zb_cfg_t cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,
        .install_code_policy = false,
        .nwk_cfg.zed_cfg =
            {
                .ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN,
                .keep_alive = 4000,
            },
    };
    esp_zb_sleep_enable(true);
    esp_zb_init(&cfg);
    esp_zb_set_rx_on_when_idle(false); /* sleepy end device */
    esp_zb_set_default_long_poll_interval(CONFIG_THERMO_ZB_LONG_POLL_MS);
    esp_zb_ep_list_t *pEps = BuildEndpoint(&s_init, s_initCalib);
    esp_err_t err = pEps ? esp_zb_device_register(pEps) : ESP_FAIL;
    if (err == ESP_OK)
    {
        esp_zb_core_action_handler_register(ActionHandler);
        esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);
        err = esp_zb_start(false);
    }
    if (err != ESP_OK)
    {
        /* No reboot: the radio stays off, the thermostat keeps working locally and shows the fault. */
        ESP_LOGE(kTag, "EVT zb radio fault: stack start failed (%s)", esp_err_to_name(err));
        s_radioFault = true;
        vTaskDelete(NULL);
        return;
    }
    s_aliveMs = MonoMs();
    esp_zb_scheduler_alarm(Heartbeat, 0, HEARTBEAT_MS);
    s_started = true;
    esp_zb_stack_main_loop();
}

/** See zb_thermostat.h. */
esp_err_t ZbThermostatStart(const ZbThermostatCbs *pCbs, const ZbThermostatInit *pInit)
{
    s_cbs = *pCbs;
    s_settings = s_init = pInit->s;
    s_initCalib = pInit->calib;
    s_boostSp = pInit->boostSp;
    s_boostMin = pInit->boostMinutes;
    s_boostDir = pInit->boostDir;
    s_swing = (U16)CtrlClampSwing(pInit->swing);
    s_sched = pInit->sched;
    esp_zb_platform_config_t pc = {
        .radio_config = {.radio_mode = ZB_RADIO_MODE_NATIVE},
        .host_config = {.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE},
    };
    esp_err_t err = esp_zb_platform_config(&pc);
    if (err == ESP_OK && xTaskCreate(ZbTask, "zigbee", 6144, NULL, 5, NULL) != pdPASS)
        err = ESP_ERR_NO_MEM;
    if (err != ESP_OK)
    {
        ESP_LOGE(kTag, "EVT zb radio fault: %s", esp_err_to_name(err));
        s_radioFault = true;
    }
    return err;
}

/* ------------------------------------------------------------------ */
/* local -> stack                                                     */
/* ------------------------------------------------------------------ */

/** Set a standard server attribute on endpoint 1 (no report); a failure is logged. */
static void SetAttr(U16 cluster, U16 attr, void *pV)
{
    esp_zb_zcl_status_t st = esp_zb_zcl_set_attribute_val(EP, cluster, SERVER, attr, pV, false);
    if (st != ESP_ZB_ZCL_STATUS_SUCCESS)
        ESP_LOGW(kTag, "set 0x%04x/0x%04x: %d", cluster, attr, st);
}

/**
 * Send an explicit attribute report for a Boost cluster attribute to the coordinator (ZHA does not configure reporting for it).
 */
static void ReportBoostAttr(U16 attr)
{
    esp_zb_zcl_report_attr_cmd_t r = {0};
    r.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000; /* coordinator (ZHA) */
    r.zcl_basic_cmd.dst_endpoint = 1;
    r.zcl_basic_cmd.src_endpoint = EP;
    r.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    r.clusterID = ZB_BOOST_CLUSTER_ID;
    r.manuf_specific = 1;
    r.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
    r.manuf_code = ZB_BOOST_MANUF_CODE;
    r.attributeID = attr;
    esp_err_t e = esp_zb_zcl_report_attr_cmd_req(&r);
    if (e != ESP_OK)
        ESP_LOGW(kTag, "boost report 0x%04x: %s", attr, esp_err_to_name(e));
}

/** Skip if unchanged; update attributes; report boost/schedule changes. See zb_thermostat.h. */
void ZbThermostatPublish(const ZbThermostatState *pSt)
{
    if (!s_started)
        return;
    static ZbThermostatState s_last;
    static bool s_haveLast;
    if (s_haveLast && !s_forceSettings && memcmp(&s_last, pSt, sizeof *pSt) == 0)
        return;
    if (!esp_zb_lock_acquire(pdMS_TO_TICKS(100)))
        return; /* never block the control loop for long */

    I16 t = pSt->tempValid ? pSt->temp : (I16)0x8000;
    U16 h = pSt->rhValid ? pSt->rh : 0xFFFF;
    U16 running = (pSt->w ? RUNNING_HEAT : 0) | (pSt->y ? RUNNING_COOL : 0) | (pSt->g ? RUNNING_FAN : 0);
    U8 mode = (U8)pSt->s.mode, fan = (U8)pSt->s.fan;
    I16 hs = pSt->s.heatSp, cs = pSt->s.coolSp;

    SetAttr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_LOCAL_TEMPERATURE_ID, &t);
    SetAttr(ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &t);
    SetAttr(ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, &h);
    SetAttr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_THERMOSTAT_RUNNING_STATE_ID, &running);
    if (s_forceSettings || memcmp(&pSt->s, &s_settings, sizeof s_settings) != 0)
    {
        s_forceSettings = false;
        SetAttr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID, &mode);
        SetAttr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID, &hs);
        SetAttr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ID, &cs);
        SetAttr(ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL, ESP_ZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_ID, &fan);
        s_settings = pSt->s;
    }
    /* Boost status: write the attributes and push an explicit report to the
     * coordinator whenever they change (ZHA does not configure reporting for
     * a quirk-added manufacturer cluster by itself). */
    struct
    {
        U16 id;
        void *pV;
        U8 wasChanged;
    } battrs[] = {
        {kZbBoostAttrState, (void *)&pSt->boostState, !s_haveLast || pSt->boostState != s_last.boostState},
        {kZbBoostAttrRemaining,
         (void *)&pSt->boostRemainingMin,
         !s_haveLast || pSt->boostRemainingMin != s_last.boostRemainingMin},
        {kZbBoostAttrLastResult,
         (void *)&pSt->boostLastResult,
         !s_haveLast || pSt->boostLastResult != s_last.boostLastResult},
    };
    /* Settings changed on the device itself: update the attribute store and report.
     * Compare with what the app published last time (not with the attribute),
     * so a stale snapshot can never undo a write that just arrived from HA. */
    bool spCh = s_haveLast && pSt->boostSp != s_last.boostSp && pSt->boostSp != s_boostSp;
    bool minCh = s_haveLast && pSt->boostMinutes != s_last.boostMinutes && pSt->boostMinutes != s_boostMin;
    bool swCh = s_haveLast && pSt->swing != s_last.swing && (U16)pSt->swing != s_swing;
    if (spCh)
        s_boostSp = pSt->boostSp;
    if (minCh)
        s_boostMin = pSt->boostMinutes;
    if (swCh)
        s_swing = (U16)pSt->swing;
    struct
    {
        U16 id;
        void *pV;
        U8 wasChanged;
    } sattrs[] = {
        {kZbBoostAttrSetpoint, &s_boostSp, spCh},
        {kZbBoostAttrDuration, &s_boostMin, minCh},
        {kZbAttrSwing, &s_swing, swCh},
    };
    /* schedule: same rule (local changes, e.g. setpoint edits of the current period) */
    for (U16 id = ZB_SCHED_ATTR_FIRST; id <= ZB_SCHED_ATTR_LAST; id++)
    {
        SchedCfg *pLc = (SchedCfg *)&s_last.sched, *pNc = (SchedCfg *)&pSt->sched;
        U16 a = *(U16 *)SchedField(pNc, id), b = *(U16 *)SchedField(pLc, id);
        U16 *pCur = SchedField(&s_sched, id);
        if (!s_haveLast || a == b || a == *pCur)
            continue;
        *pCur = a;
        esp_zb_zcl_set_manufacturer_attribute_val(
            EP,
            ZB_BOOST_CLUSTER_ID,
            SERVER,
            ZB_BOOST_MANUF_CODE,
            id,
            pCur,
            false);
        if (s_link == kLinkJoined)
            ReportBoostAttr(id);
    }
    if (!s_haveLast || pSt->schedState != s_last.schedState)
    {
        s_schedState = pSt->schedState;
        esp_zb_zcl_set_manufacturer_attribute_val(
            EP,
            ZB_BOOST_CLUSTER_ID,
            SERVER,
            ZB_BOOST_MANUF_CODE,
            kZbAttrSchedState,
            &s_schedState,
            false);
        if (s_link == kLinkJoined)
            ReportBoostAttr(kZbAttrSchedState);
    }
    for (size_t i = 0; i < sizeof sattrs / sizeof sattrs[0]; i++)
    {
        if (!sattrs[i].wasChanged)
            continue;
        esp_zb_zcl_set_manufacturer_attribute_val(
            EP,
            ZB_BOOST_CLUSTER_ID,
            SERVER,
            ZB_BOOST_MANUF_CODE,
            sattrs[i].id,
            sattrs[i].pV,
            false);
        if (s_link == kLinkJoined)
            ReportBoostAttr(sattrs[i].id);
    }
    for (size_t i = 0; i < sizeof battrs / sizeof battrs[0]; i++)
    {
        if (!battrs[i].wasChanged)
            continue;
        esp_zb_zcl_set_manufacturer_attribute_val(
            EP,
            ZB_BOOST_CLUSTER_ID,
            SERVER,
            ZB_BOOST_MANUF_CODE,
            battrs[i].id,
            battrs[i].pV,
            false);
        if (s_link == kLinkJoined)
            ReportBoostAttr(battrs[i].id);
    }
    esp_zb_lock_release();
    s_last = *pSt;
    s_haveLast = true;
}
