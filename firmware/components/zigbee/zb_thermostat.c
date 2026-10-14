/* zb_thermostat.c - see zb_thermostat.h. Written against esp-zigbee-lib 1.6.x. */
#include "zb_thermostat.h"
#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "esp_zigbee_core.h"
#include "esp_zigbee_ota.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "fw_version.h"
#include "zb_time.h"

static const char *TAG = "zb";

#define EP ZB_THERMOSTAT_EP
#define SERVER ESP_ZB_ZCL_CLUSTER_SERVER_ROLE

/* ZCL char strings: first byte = length */
#define MANUFACTURER_NAME "\x06" "Connie"
#define MODEL_ID          "\x0d" "thermostat-v3"
#define SW_BUILD_ID       "\x05" FW_VERSION   /* ZCL char string: length prefix, keep FW_VERSION 5 chars */

#define RUNNING_HEAT 0x0001
#define RUNNING_COOL 0x0002
#define RUNNING_FAN  0x0004

static zb_thermostat_cbs_t s_cbs;
static int16_t  s_boost_sp;
static uint16_t s_boost_min;
static uint8_t  s_boost_dir;
static uint16_t s_swing = CTRL_SWING_DEFAULT;   /* ZB_ATTR_SWING backing store */
static ctrl_settings_t s_settings;           /* last value known to the stack */
static volatile link_state_t s_link = LINK_OFFLINE;
static bool s_started;
static sched_cfg_t s_sched;                   /* schedule attribute backing store */
static uint8_t s_sched_state;                 /* ZB_ATTR_SCHED_STATE backing store */
static volatile bool s_force_settings;        /* republish mode/setpoints (e.g. rejected AUTO write) */

/* Time sync: local seconds since midnight at s_time_at_ms (monotonic). */
static uint8_t s_time_fails;                  /* consecutive reads without a usable answer */
static bool s_time_req_open;                  /* a read is out and not yet answered usefully */
static volatile bool s_time_known;
static uint32_t s_time_local_s;               /* ZCL UTCTime-style seconds (local) at sync */
static uint64_t s_time_at_ms;
static portMUX_TYPE s_time_mux = portMUX_INITIALIZER_UNLOCKED;

static uint64_t mono_ms(void) { return (uint64_t)(esp_timer_get_time() / 1000); }

/* attribute id -> field of s_sched (NULL if not a schedule attribute) */
static void *sched_field(sched_cfg_t *c, uint16_t id)
{
    switch (id) {
    case ZB_ATTR_DAY_COOL_START:    return &c->day_cool_start;
    case ZB_ATTR_DAY_COOL_TEMP:     return &c->day_cool;
    case ZB_ATTR_NIGHT_COOL_START:  return &c->night_cool_start;
    case ZB_ATTR_NIGHT_COOL_TEMP:   return &c->night_cool;
    case ZB_ATTR_DAY_HEAT_START:    return &c->day_heat_start;
    case ZB_ATTR_DAY_HEAT_TEMP:     return &c->day_heat;
    case ZB_ATTR_NIGHT_HEAT_START:  return &c->night_heat_start;
    case ZB_ATTR_NIGHT_HEAT_TEMP:   return &c->night_heat;
    case ZB_ATTR_MORNING_BOOST_MIN: return &c->boost_min;
    case ZB_ATTR_MORNING_BOOST_OFS: return &c->boost_offset;
    default:                        return NULL;
    }
}
static bool sched_attr_signed(uint16_t id)
{
    return id == ZB_ATTR_DAY_COOL_TEMP || id == ZB_ATTR_NIGHT_COOL_TEMP || id == ZB_ATTR_DAY_HEAT_TEMP ||
           id == ZB_ATTR_NIGHT_HEAT_TEMP || id == ZB_ATTR_MORNING_BOOST_OFS;
}

/* ------------------------------------------------------------------ */
/* time of day (Time cluster client -> coordinator)                    */
/* ------------------------------------------------------------------ */

static void time_request(uint8_t unused)
{
    (void)unused;
    if (s_time_req_open && s_time_fails < 255) s_time_fails++;   /* last read: no or unusable answer */
    s_time_req_open = false;
    if (s_link == LINK_JOINED) {
        /* LocalTime (0x0007) is what we want; Time + TimeZone + DstShift is the fallback. */
        static uint16_t ids[] = {ESP_ZB_ZCL_ATTR_TIME_TIME_ID, ESP_ZB_ZCL_ATTR_TIME_TIME_STATUS_ID, ESP_ZB_ZCL_ATTR_TIME_TIME_ZONE_ID,
                                 ESP_ZB_ZCL_ATTR_TIME_DST_SHIFT_ID, ESP_ZB_ZCL_ATTR_TIME_LOCAL_TIME_ID};
        esp_zb_zcl_read_attr_cmd_t r = {0};
        r.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;   /* coordinator */
        r.zcl_basic_cmd.dst_endpoint = 1;
        r.zcl_basic_cmd.src_endpoint = EP;
        r.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
        r.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TIME;
        r.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
        r.attr_number = sizeof ids / sizeof ids[0];
        r.attr_field = ids;
        if (esp_zb_zcl_read_attr_cmd_req(&r) == ESP_OK) s_time_req_open = true;
    }
    /* hourly once known (drift + DST changes); 1, 2, 5 min, then hourly while unknown */
    esp_zb_scheduler_alarm(time_request, 0, zb_time_next_delay_ms(s_time_fails, s_time_known));
}

static void on_time_resp(const esp_zb_zcl_cmd_read_attr_resp_message_t *m)
{
    bool have_utc = false, have_local = false, have_status = false;
    uint8_t status = 0;
    uint32_t utc = 0, local = 0;
    int32_t tz = 0, dst = 0;
    for (const esp_zb_zcl_read_attr_resp_variable_t *v = m->variables; v; v = v->next) {
        if (v->status != ESP_ZB_ZCL_STATUS_SUCCESS || !v->attribute.data.value) continue;
        const void *d = v->attribute.data.value;
        switch (v->attribute.id) {
        case ESP_ZB_ZCL_ATTR_TIME_TIME_ID:       memcpy(&utc, d, 4); have_utc = utc != 0 && utc != 0xFFFFFFFFu; break;
        case ESP_ZB_ZCL_ATTR_TIME_TIME_STATUS_ID: status = *(const uint8_t *)d; have_status = true; break;
        case ESP_ZB_ZCL_ATTR_TIME_TIME_ZONE_ID:  memcpy(&tz, d, 4); break;
        case ESP_ZB_ZCL_ATTR_TIME_DST_SHIFT_ID:  memcpy(&dst, d, 4); break;
        case ESP_ZB_ZCL_ATTR_TIME_LOCAL_TIME_ID: memcpy(&local, d, 4); have_local = local != 0 && local != 0xFFFFFFFFu; break;
        default: break;
        }
    }
    if (!have_local && have_utc) { local = (uint32_t)((int64_t)utc + tz + dst); have_local = true; }
    if (!have_local || !zb_time_acceptable(have_status, status, local) ||
        (have_utc && !zb_time_acceptable(have_status, status, utc))) {
        ESP_LOGW(TAG, "time sync: coordinator returned no usable time (status 0x%02x)", status);
        return;   /* counted as a failure at the next attempt */
    }
    s_time_req_open = false;
    s_time_fails = 0;
    portENTER_CRITICAL(&s_time_mux);
    s_time_local_s = local;
    s_time_at_ms = mono_ms();
    s_time_known = true;
    portEXIT_CRITICAL(&s_time_mux);
    ESP_LOGI(TAG, "time sync: local %02lu:%02lu", (unsigned long)(local / 3600 % 24), (unsigned long)(local / 60 % 60));
}

/* one self-rescheduling poll chain, started on the first join */
static void time_sync_start(void)
{
    static bool running;
    if (running) return;
    running = true;
    esp_zb_scheduler_alarm(time_request, 0, 5000);
}

bool zb_thermostat_local_minute(uint16_t *minute)
{
    portENTER_CRITICAL(&s_time_mux);
    bool known = s_time_known;
    uint64_t secs = (uint64_t)s_time_local_s + (mono_ms() - s_time_at_ms) / 1000u;
    portEXIT_CRITICAL(&s_time_mux);
    if (known) *minute = (uint16_t)(secs % 86400u / 60u);
    return known;
}

/* ------------------------------------------------------------------ */
/* commissioning                                                      */
/* ------------------------------------------------------------------ */

static void commissioning_cb(uint8_t mode)
{
    if (esp_zb_bdb_start_top_level_commissioning(mode) != ESP_OK)
        ESP_LOGE(TAG, "commissioning start failed");
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *sig_s)
{
    uint32_t *p = sig_s->p_app_signal;
    esp_err_t st = sig_s->esp_err_status;
    esp_zb_app_signal_type_t sig = *p;

    switch (sig) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (st != ESP_OK) {
            ESP_LOGW(TAG, "stack init failed (%s), retrying", esp_err_to_name(st));
            esp_zb_scheduler_alarm(commissioning_cb, ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
            break;
        }
        if (esp_zb_bdb_is_factory_new()) {
            ESP_LOGI(TAG, "factory new: start network steering (pairing)");
            s_link = LINK_PAIRING;
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            ESP_LOGI(TAG, "rejoined existing network");
            s_link = LINK_JOINED;
            time_sync_start();
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (st == ESP_OK) {
            ESP_LOGI(TAG, "joined PAN 0x%04hx ch %d addr 0x%04hx", esp_zb_get_pan_id(),
                     esp_zb_get_current_channel(), esp_zb_get_short_address());
            s_link = LINK_JOINED;
            time_sync_start();
        } else {
            /* Keep trying, slowly. The thermostat works fine unpaired. */
            s_link = LINK_PAIRING;
            esp_zb_scheduler_alarm(commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 10000);
        }
        break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE:
        ESP_LOGW(TAG, "left network");
        s_link = LINK_OFFLINE;
        break;
    case ESP_ZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT:
        s_link = LINK_LOST;
        break;
    case ESP_ZB_COMMON_SIGNAL_CAN_SLEEP:
        esp_zb_sleep_now();   /* light sleep until the next poll / timer / GPIO */
        break;
    default:
        ESP_LOGD(TAG, "signal %s (0x%x) %s", esp_zb_zdo_signal_to_string(sig), sig, esp_err_to_name(st));
        break;
    }
}

link_state_t zb_thermostat_link(void) { return s_link; }

void zb_thermostat_pair(void)
{
    if (!s_started) return;
    esp_zb_lock_acquire(portMAX_DELAY);
    if (!esp_zb_bdb_dev_joined()) {
        s_link = LINK_PAIRING;
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
    }
    esp_zb_lock_release();
}

void zb_thermostat_factory_reset(void)
{
    if (!s_started) return;
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_factory_reset();   /* erases zb_storage and reboots */
    esp_zb_lock_release();
}

/* ------------------------------------------------------------------ */
/* remote attribute writes                                            */
/* ------------------------------------------------------------------ */

static esp_err_t on_set_attr(const esp_zb_zcl_set_attr_value_message_t *m)
{
    ESP_RETURN_ON_FALSE(m && m->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "bad msg");
    if (m->info.dst_endpoint != EP || !m->attribute.data.value) return ESP_OK;
    const void *v = m->attribute.data.value;
    ctrl_settings_t s = s_settings;
    uint8_t what = ZB_SET_MODE_FAN;
    bool changed = false;

    switch (m->info.cluster) {
    case ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT:
        switch (m->attribute.id) {
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID:
            s.heat_sp = *(const int16_t *)v; what = ZB_SET_HEAT_SP; changed = true; break;
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ID:
            s.cool_sp = *(const int16_t *)v; what = ZB_SET_COOL_SP; changed = true; break;
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID: {
            uint8_t mode = *(const uint8_t *)v;
            if (ctrl_mode_valid(mode)) { s.mode = (ctrl_mode_t)mode; changed = true; }
            else {   /* e.g. Auto / Heat-Cool: not supported, put the real mode back */
                ESP_LOGW(TAG, "unsupported system mode %u ignored", mode);
                s_force_settings = true;
            }
            break;
        }
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_LOCAL_TEMPERATURE_CALIBRATION_ID:
            if (s_cbs.on_calibration) s_cbs.on_calibration(*(const int8_t *)v);
            break;
        }
        break;
    case ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL:
        if (m->attribute.id == ESP_ZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_ID) {
            uint8_t f = *(const uint8_t *)v;
            /* Single-speed blower: anything other than Auto means On. */
            s.fan = (f == ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_AUTO || f == ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_OFF)
                        ? CTRL_FAN_AUTO : CTRL_FAN_ON;
            changed = true;
        }
        break;
    case ZB_BOOST_CLUSTER_ID: {
        int16_t sp = s_boost_sp; uint16_t mins = s_boost_min; uint8_t dir = s_boost_dir;
        switch (m->attribute.id) {
        case ZB_BOOST_ATTR_SETPOINT: sp = *(const int16_t *)v; break;
        case ZB_BOOST_ATTR_DURATION: mins = *(const uint16_t *)v; break;
        case ZB_BOOST_ATTR_MODE:     dir = *(const uint8_t *)v; break;
        case ZB_BOOST_ATTR_COMMAND:
            if (s_cbs.on_boost_command) s_cbs.on_boost_command(*(const uint8_t *)v);
            return ESP_OK;
        case ZB_ATTR_SWING: {
            uint16_t raw = *(const uint16_t *)v;
            int16_t sw = ctrl_clamp_swing(raw > 1000 ? 1000 : raw);
            if (sw != (int16_t)raw) {   /* store the clamped value so a read-back shows reality */
                s_swing = (uint16_t)sw;
                esp_zb_zcl_set_manufacturer_attribute_val(EP, ZB_BOOST_CLUSTER_ID, SERVER, ZB_BOOST_MANUF_CODE, ZB_ATTR_SWING,
                                                          &s_swing, false);
            }
            s_swing = (uint16_t)sw;
            if (s_cbs.on_swing) s_cbs.on_swing(sw);
            return ESP_OK;
        }
        default: {
            void *f = sched_field(&s_sched, m->attribute.id);
            if (!f) return ESP_OK;
            sched_cfg_t c = s_sched;
            f = sched_field(&c, m->attribute.id);
            if (sched_attr_signed(m->attribute.id)) *(int16_t *)f = *(const int16_t *)v;
            else                                    *(uint16_t *)f = *(const uint16_t *)v;
            ctrl_params_t prm = CTRL_PARAMS_DEFAULT();
            sched_sanitize(&prm, &c);
            s_sched = c;
            /* store the clamped value so a read-back shows reality */
            esp_zb_zcl_set_manufacturer_attribute_val(EP, ZB_BOOST_CLUSTER_ID, SERVER, ZB_BOOST_MANUF_CODE,
                                                      m->attribute.id, sched_field(&s_sched, m->attribute.id), false);
            if (s_cbs.on_sched) s_cbs.on_sched(&s_sched);
            return ESP_OK;
        }
        }
        if (mins < BOOST_TIMED_MIN_MIN) mins = BOOST_TIMED_MIN_MIN;
        if (mins > BOOST_TIMED_MAX_MIN) mins = BOOST_TIMED_MAX_MIN;
        if (dir > BOOST_DIR_COOL) dir = BOOST_DIR_AUTO;
        s_boost_sp = sp; s_boost_min = mins; s_boost_dir = dir;
        if (s_cbs.on_boost_params) s_cbs.on_boost_params(sp, mins, dir);
        return ESP_OK;
    }
    case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY:
        if (m->attribute.id == ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID && s_cbs.on_identify)
            s_cbs.on_identify(*(const uint16_t *)v);
        break;
    }
    if (changed && s_cbs.on_settings) s_cbs.on_settings(&s, what);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* OTA upgrade client                                                 */
/* ------------------------------------------------------------------ */

#define OTA_ELEMENT_HEADER_LEN 6
#define OTA_TAG_UPGRADE_IMAGE  0x0000
static const esp_partition_t *s_ota_part;
static esp_ota_handle_t s_ota;
static bool s_ota_tag_seen;
static uint32_t s_ota_off, s_ota_total;

static esp_err_t ota_element(uint32_t total, const uint8_t *p, uint16_t n, const uint8_t **out, uint16_t *outlen)
{
    if (!s_ota_tag_seen) {
        ESP_RETURN_ON_FALSE(p && n > OTA_ELEMENT_HEADER_LEN, ESP_ERR_INVALID_ARG, TAG, "bad element");
        uint16_t tag; uint32_t len;
        memcpy(&tag, p, 2); memcpy(&len, p + 2, 4);
        ESP_RETURN_ON_FALSE(tag == OTA_TAG_UPGRADE_IMAGE, ESP_ERR_INVALID_ARG, TAG, "unsupported tag %u", tag);
        ESP_RETURN_ON_FALSE(len + OTA_ELEMENT_HEADER_LEN == total, ESP_ERR_INVALID_SIZE, TAG, "bad length");
        s_ota_tag_seen = true;
        *out = p + OTA_ELEMENT_HEADER_LEN;
        *outlen = n - OTA_ELEMENT_HEADER_LEN;
    } else {
        *out = p;
        *outlen = n;
    }
    return ESP_OK;
}

static void ota_reset(void)
{
    if (s_ota) esp_ota_abort(s_ota);
    s_ota = 0;
    s_ota_off = s_ota_total = 0;
    s_ota_tag_seen = false;
}

static esp_err_t on_ota(const esp_zb_zcl_ota_upgrade_value_message_t *m)
{
    esp_err_t ret = ESP_OK;
    if (m->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) return ESP_OK;
    switch (m->upgrade_status) {
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_START:
        ESP_LOGI(TAG, "OTA start");
        ota_reset();   /* drop any handle left by an earlier failed transfer */
        s_ota_part = esp_ota_get_next_update_partition(NULL);
        ESP_RETURN_ON_FALSE(s_ota_part, ESP_FAIL, TAG, "no OTA partition");
        ret = esp_ota_begin(s_ota_part, OTA_WITH_SEQUENTIAL_WRITES, &s_ota);
        if (ret != ESP_OK) s_ota = 0;
        break;
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE: {
        s_ota_total = m->ota_header.image_size;
        s_ota_off += m->payload_size;
        const uint8_t *d; uint16_t n;
        if (m->payload_size && m->payload) {
            ret = ota_element(s_ota_total, m->payload, m->payload_size, &d, &n);
            if (ret == ESP_OK) ret = esp_ota_write(s_ota, d, n);
            if (ret != ESP_OK) ESP_LOGE(TAG, "OTA write failed: %s", esp_err_to_name(ret));
        }
        if (s_cbs.on_ota_progress && s_ota_total)
            s_cbs.on_ota_progress((int)((uint64_t)s_ota_off * 100 / s_ota_total));
        break;
    }
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_CHECK:
        ret = (s_ota_off == s_ota_total) ? ESP_OK : ESP_FAIL;
        break;
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_FINISH:
        ret = esp_ota_end(s_ota);
        s_ota = 0;   /* esp_ota_end frees the handle even on failure */
        if (ret != ESP_OK) { ESP_LOGE(TAG, "ota end: %s", esp_err_to_name(ret)); break; }
        ret = esp_ota_set_boot_partition(s_ota_part);
        if (ret != ESP_OK) { ESP_LOGE(TAG, "set boot: %s", esp_err_to_name(ret)); break; }
        ESP_LOGW(TAG, "OTA done (v0x%08lx), rebooting", (unsigned long)m->ota_header.file_version);
        if (s_cbs.before_ota_reboot) s_cbs.before_ota_reboot();
        esp_restart();
        break;
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_ABORT:
        ESP_LOGW(TAG, "OTA aborted");
        ota_reset();
        if (s_cbs.on_ota_progress) s_cbs.on_ota_progress(-1);
        break;
    default:
        break;
    }
    if (ret != ESP_OK) {
        ota_reset();
        if (s_cbs.on_ota_progress) s_cbs.on_ota_progress(-1);
    }
    return ret;
}

static esp_err_t action_handler(esp_zb_core_action_callback_id_t id, const void *msg)
{
    switch (id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        return on_set_attr((const esp_zb_zcl_set_attr_value_message_t *)msg);
    case ESP_ZB_CORE_OTA_UPGRADE_VALUE_CB_ID:
        return on_ota((const esp_zb_zcl_ota_upgrade_value_message_t *)msg);
    case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID: {
        const esp_zb_zcl_cmd_read_attr_resp_message_t *r = msg;
        if (r->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_TIME) on_time_resp(r);
        return ESP_OK;
    }
    case ESP_ZB_CORE_OTA_UPGRADE_QUERY_IMAGE_RESP_CB_ID: {
        const esp_zb_zcl_ota_upgrade_query_image_resp_message_t *r = msg;
        if (r->info.status == ESP_ZB_ZCL_STATUS_SUCCESS)
            ESP_LOGI(TAG, "OTA image available: v0x%08lx, %lu bytes", (unsigned long)r->file_version,
                     (unsigned long)r->image_size);
        return ESP_OK;   /* approve */
    }
    default:
        ESP_LOGD(TAG, "action 0x%x", id);
        return ESP_OK;
    }
}

/* ------------------------------------------------------------------ */
/* data model                                                         */
/* ------------------------------------------------------------------ */

static esp_zb_ep_list_t *build_endpoint(const ctrl_settings_t *init, int8_t calib)
{
    esp_zb_cluster_list_t *cl = esp_zb_zcl_cluster_list_create();

    /* Basic: mains powered (0x01 = mains single phase; closest for 24 VAC) */
    esp_zb_basic_cluster_cfg_t basic_cfg = {.zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE, .power_source = 0x01};
    esp_zb_attribute_list_t *basic = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, MODEL_ID);
    esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, SW_BUILD_ID);
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cl, basic, SERVER));

    esp_zb_identify_cluster_cfg_t id_cfg = {.identify_time = 0};
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cl, esp_zb_identify_cluster_create(&id_cfg), SERVER));

    /* Thermostat */
    esp_zb_thermostat_cluster_cfg_t th_cfg = {
        .local_temperature = (int16_t)0x8000,              /* invalid until first reading */
        .occupied_cooling_setpoint = init->cool_sp,
        .occupied_heating_setpoint = init->heat_sp,
        .control_sequence_of_operation = 0x04,             /* cooling and heating (no Auto: writes of 1 are refused) */
        .system_mode = (uint8_t)init->mode,
    };
    esp_zb_attribute_list_t *th = esp_zb_thermostat_cluster_create(&th_cfg);
    ctrl_params_t p = CTRL_PARAMS_DEFAULT();
    int16_t min_sp = p.min_sp, max_sp = p.max_sp;
    uint16_t running = 0;
    esp_zb_thermostat_cluster_add_attr(th, ESP_ZB_ZCL_ATTR_THERMOSTAT_ABS_MIN_HEAT_SETPOINT_LIMIT_ID, &min_sp);
    esp_zb_thermostat_cluster_add_attr(th, ESP_ZB_ZCL_ATTR_THERMOSTAT_ABS_MAX_HEAT_SETPOINT_LIMIT_ID, &max_sp);
    esp_zb_thermostat_cluster_add_attr(th, ESP_ZB_ZCL_ATTR_THERMOSTAT_ABS_MIN_COOL_SETPOINT_LIMIT_ID, &min_sp);
    esp_zb_thermostat_cluster_add_attr(th, ESP_ZB_ZCL_ATTR_THERMOSTAT_ABS_MAX_COOL_SETPOINT_LIMIT_ID, &max_sp);
    esp_zb_thermostat_cluster_add_attr(th, ESP_ZB_ZCL_ATTR_THERMOSTAT_MIN_HEAT_SETPOINT_LIMIT_ID, &min_sp);
    esp_zb_thermostat_cluster_add_attr(th, ESP_ZB_ZCL_ATTR_THERMOSTAT_MAX_HEAT_SETPOINT_LIMIT_ID, &max_sp);
    esp_zb_thermostat_cluster_add_attr(th, ESP_ZB_ZCL_ATTR_THERMOSTAT_MIN_COOL_SETPOINT_LIMIT_ID, &min_sp);
    esp_zb_thermostat_cluster_add_attr(th, ESP_ZB_ZCL_ATTR_THERMOSTAT_MAX_COOL_SETPOINT_LIMIT_ID, &max_sp);
    esp_zb_thermostat_cluster_add_attr(th, ESP_ZB_ZCL_ATTR_THERMOSTAT_LOCAL_TEMPERATURE_CALIBRATION_ID, &calib);
    esp_zb_thermostat_cluster_add_attr(th, ESP_ZB_ZCL_ATTR_THERMOSTAT_THERMOSTAT_RUNNING_STATE_ID, &running);
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_thermostat_cluster(cl, th, SERVER));

    /* Fan control: On/Auto */
    esp_zb_fan_control_cluster_cfg_t fan_cfg = {
        .fan_mode = (uint8_t)init->fan,
        .fan_mode_sequence = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_SEQUENCE_ON_AUTO,
    };
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_fan_control_cluster(cl, esp_zb_fan_control_cluster_create(&fan_cfg), SERVER));

    esp_zb_temperature_meas_cluster_cfg_t t_cfg = {
        .measured_value = (int16_t)0x8000, .min_value = -4500, .max_value = 13000,
    };
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_temperature_meas_cluster(cl, esp_zb_temperature_meas_cluster_create(&t_cfg), SERVER));

    esp_zb_humidity_meas_cluster_cfg_t h_cfg = {.measured_value = 0xFFFF, .min_value = 0, .max_value = 10000};
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_humidity_meas_cluster(cl, esp_zb_humidity_meas_cluster_create(&h_cfg), SERVER));

    /* Manufacturer-specific Boost cluster (see zb_boost.h). */
    esp_zb_attribute_list_t *bc = esp_zb_zcl_attr_list_create(ZB_BOOST_CLUSTER_ID);
    static uint8_t b_cmd = 0, b_state = 0, b_result = 0;
    static uint16_t b_rem = 0;
    const uint8_t RW = ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_MANUF_SPEC;
    const uint8_t RO = ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING | ESP_ZB_ZCL_ATTR_MANUF_SPEC;
    ESP_ERROR_CHECK(esp_zb_cluster_add_manufacturer_attr(bc, ZB_BOOST_CLUSTER_ID, ZB_BOOST_ATTR_SETPOINT, ZB_BOOST_MANUF_CODE,
                                                         ESP_ZB_ZCL_ATTR_TYPE_S16, RW, &s_boost_sp));
    ESP_ERROR_CHECK(esp_zb_cluster_add_manufacturer_attr(bc, ZB_BOOST_CLUSTER_ID, ZB_BOOST_ATTR_DURATION, ZB_BOOST_MANUF_CODE,
                                                         ESP_ZB_ZCL_ATTR_TYPE_U16, RW, &s_boost_min));
    ESP_ERROR_CHECK(esp_zb_cluster_add_manufacturer_attr(bc, ZB_BOOST_CLUSTER_ID, ZB_BOOST_ATTR_MODE, ZB_BOOST_MANUF_CODE,
                                                         ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, RW, &s_boost_dir));
    ESP_ERROR_CHECK(esp_zb_cluster_add_manufacturer_attr(bc, ZB_BOOST_CLUSTER_ID, ZB_BOOST_ATTR_COMMAND, ZB_BOOST_MANUF_CODE,
                                                         ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, RW, &b_cmd));
    ESP_ERROR_CHECK(esp_zb_cluster_add_manufacturer_attr(bc, ZB_BOOST_CLUSTER_ID, ZB_BOOST_ATTR_STATE, ZB_BOOST_MANUF_CODE,
                                                         ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, RO, &b_state));
    ESP_ERROR_CHECK(esp_zb_cluster_add_manufacturer_attr(bc, ZB_BOOST_CLUSTER_ID, ZB_BOOST_ATTR_REMAINING, ZB_BOOST_MANUF_CODE,
                                                         ESP_ZB_ZCL_ATTR_TYPE_U16, RO, &b_rem));
    ESP_ERROR_CHECK(esp_zb_cluster_add_manufacturer_attr(bc, ZB_BOOST_CLUSTER_ID, ZB_BOOST_ATTR_LAST_RESULT, ZB_BOOST_MANUF_CODE,
                                                         ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, RO, &b_result));
    ESP_ERROR_CHECK(esp_zb_cluster_add_manufacturer_attr(bc, ZB_BOOST_CLUSTER_ID, ZB_ATTR_SWING, ZB_BOOST_MANUF_CODE,
                                                         ESP_ZB_ZCL_ATTR_TYPE_U16, RW, &s_swing));
    for (uint16_t id = ZB_SCHED_ATTR_FIRST; id <= ZB_SCHED_ATTR_LAST; id++)
        ESP_ERROR_CHECK(esp_zb_cluster_add_manufacturer_attr(bc, ZB_BOOST_CLUSTER_ID, id, ZB_BOOST_MANUF_CODE,
                                                             sched_attr_signed(id) ? ESP_ZB_ZCL_ATTR_TYPE_S16 : ESP_ZB_ZCL_ATTR_TYPE_U16,
                                                             RW, sched_field(&s_sched, id)));
    ESP_ERROR_CHECK(esp_zb_cluster_add_manufacturer_attr(bc, ZB_BOOST_CLUSTER_ID, ZB_ATTR_SCHED_STATE, ZB_BOOST_MANUF_CODE,
                                                         ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, RO, &s_sched_state));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_custom_cluster(cl, bc, SERVER));

    /* Time client: we read the coordinator's clock for the program schedule. */
    esp_zb_time_cluster_cfg_t time_cfg = {0};
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_time_cluster(cl, esp_zb_time_cluster_create(&time_cfg),
                                                         ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));

    /* OTA upgrade client */
    esp_zb_ota_cluster_cfg_t ota_cfg = {
        .ota_upgrade_file_version = FW_OTA_FILE_VERSION,
        .ota_upgrade_downloaded_file_ver = FW_OTA_FILE_VERSION,
        .ota_upgrade_manufacturer = CONFIG_THERMO_OTA_MANUFACTURER,
        .ota_upgrade_image_type = CONFIG_THERMO_OTA_IMAGE_TYPE,
    };
    esp_zb_attribute_list_t *ota = esp_zb_ota_cluster_create(&ota_cfg);
    esp_zb_zcl_ota_upgrade_client_variable_t ota_var = {
        .timer_query = ESP_ZB_ZCL_OTA_UPGRADE_QUERY_TIMER_COUNT_DEF,   /* minutes (1 day) */
        .hw_version = CONFIG_THERMO_HW_VERSION,
        .max_data_size = 223,
    };
    uint16_t srv_addr = 0xffff;
    uint8_t srv_ep = 0xff;
    esp_zb_ota_cluster_add_attr(ota, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_CLIENT_DATA_ID, &ota_var);
    esp_zb_ota_cluster_add_attr(ota, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ADDR_ID, &srv_addr);
    esp_zb_ota_cluster_add_attr(ota, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ENDPOINT_ID, &srv_ep);
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_ota_cluster(cl, ota, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));

    esp_zb_ep_list_t *eps = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t ep_cfg = {
        .endpoint = EP,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_THERMOSTAT_DEVICE_ID,
        .app_device_version = 0,
    };
    ESP_ERROR_CHECK(esp_zb_ep_list_add_ep(eps, cl, ep_cfg));
    return eps;
}

static ctrl_settings_t s_init;
static int8_t s_init_calib;

static void zb_task(void *arg)
{
    esp_zb_cfg_t cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,
        .install_code_policy = false,
        .nwk_cfg.zed_cfg = {
            .ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN,
            .keep_alive = 4000,
        },
    };
    esp_zb_sleep_enable(true);
    esp_zb_init(&cfg);
    esp_zb_set_rx_on_when_idle(false);              /* sleepy end device */
    esp_zb_set_default_long_poll_interval(CONFIG_THERMO_ZB_LONG_POLL_MS);
    ESP_ERROR_CHECK(esp_zb_device_register(build_endpoint(&s_init, s_init_calib)));
    esp_zb_core_action_handler_register(action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));
    s_started = true;
    esp_zb_stack_main_loop();
}

esp_err_t zb_thermostat_start(const zb_thermostat_cbs_t *cbs, const zb_thermostat_init_t *init)
{
    s_cbs = *cbs;
    s_settings = s_init = init->s;
    s_init_calib = init->calib;
    s_boost_sp = init->boost_sp;
    s_boost_min = init->boost_minutes;
    s_boost_dir = init->boost_dir;
    s_swing = (uint16_t)ctrl_clamp_swing(init->swing);
    s_sched = init->sched;
    esp_zb_platform_config_t pc = {
        .radio_config = {.radio_mode = ZB_RADIO_MODE_NATIVE},
        .host_config = {.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE},
    };
    ESP_RETURN_ON_ERROR(esp_zb_platform_config(&pc), TAG, "platform config");
    return xTaskCreate(zb_task, "zigbee", 6144, NULL, 5, NULL) == pdPASS ? ESP_OK : ESP_FAIL;
}

/* ------------------------------------------------------------------ */
/* local -> stack                                                     */
/* ------------------------------------------------------------------ */

static void set_attr(uint16_t cluster, uint16_t attr, void *v)
{
    esp_zb_zcl_status_t st = esp_zb_zcl_set_attribute_val(EP, cluster, SERVER, attr, v, false);
    if (st != ESP_ZB_ZCL_STATUS_SUCCESS) ESP_LOGW(TAG, "set 0x%04x/0x%04x: %d", cluster, attr, st);
}

static void report_boost_attr(uint16_t attr)
{
    esp_zb_zcl_report_attr_cmd_t r = {0};
    r.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;   /* coordinator (ZHA) */
    r.zcl_basic_cmd.dst_endpoint = 1;
    r.zcl_basic_cmd.src_endpoint = EP;
    r.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    r.clusterID = ZB_BOOST_CLUSTER_ID;
    r.manuf_specific = 1;
    r.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
    r.manuf_code = ZB_BOOST_MANUF_CODE;
    r.attributeID = attr;
    esp_err_t e = esp_zb_zcl_report_attr_cmd_req(&r);
    if (e != ESP_OK) ESP_LOGW(TAG, "boost report 0x%04x: %s", attr, esp_err_to_name(e));
}

void zb_thermostat_publish(const zb_thermostat_state_t *st)
{
    if (!s_started) return;
    static zb_thermostat_state_t last;
    static bool have_last;
    if (have_last && !s_force_settings && memcmp(&last, st, sizeof *st) == 0) return;
    if (!esp_zb_lock_acquire(pdMS_TO_TICKS(100))) return;   /* never block the control loop for long */

    int16_t t = st->temp_valid ? st->temp : (int16_t)0x8000;
    uint16_t h = st->rh_valid ? st->rh : 0xFFFF;
    uint16_t running = (st->w ? RUNNING_HEAT : 0) | (st->y ? RUNNING_COOL : 0) | (st->g ? RUNNING_FAN : 0);
    uint8_t mode = (uint8_t)st->s.mode, fan = (uint8_t)st->s.fan;
    int16_t hs = st->s.heat_sp, cs = st->s.cool_sp;

    set_attr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_LOCAL_TEMPERATURE_ID, &t);
    set_attr(ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &t);
    set_attr(ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, &h);
    set_attr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_THERMOSTAT_RUNNING_STATE_ID, &running);
    if (s_force_settings || memcmp(&st->s, &s_settings, sizeof s_settings) != 0) {
        s_force_settings = false;
        set_attr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID, &mode);
        set_attr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID, &hs);
        set_attr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ID, &cs);
        set_attr(ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL, ESP_ZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_ID, &fan);
        s_settings = st->s;
    }
    /* Boost status: write the attributes and push an explicit report to the
     * coordinator whenever they change (ZHA does not configure reporting for
     * a quirk-added manufacturer cluster by itself). */
    struct { uint16_t id; void *v; uint8_t was_changed; } battrs[] = {
        {ZB_BOOST_ATTR_STATE, (void *)&st->boost_state, !have_last || st->boost_state != last.boost_state},
        {ZB_BOOST_ATTR_REMAINING, (void *)&st->boost_remaining_min,
         !have_last || st->boost_remaining_min != last.boost_remaining_min},
        {ZB_BOOST_ATTR_LAST_RESULT, (void *)&st->boost_last_result,
         !have_last || st->boost_last_result != last.boost_last_result},
    };
    /* Settings changed on the device itself: update the attribute store and report.
     * Compare with what the app published last time (not with the attribute),
     * so a stale snapshot can never undo a write that just arrived from HA. */
    bool sp_ch = have_last && st->boost_sp != last.boost_sp && st->boost_sp != s_boost_sp;
    bool min_ch = have_last && st->boost_minutes != last.boost_minutes && st->boost_minutes != s_boost_min;
    bool sw_ch = have_last && st->swing != last.swing && (uint16_t)st->swing != s_swing;
    if (sp_ch) s_boost_sp = st->boost_sp;
    if (min_ch) s_boost_min = st->boost_minutes;
    if (sw_ch) s_swing = (uint16_t)st->swing;
    struct { uint16_t id; void *v; uint8_t was_changed; } sattrs[] = {
        {ZB_BOOST_ATTR_SETPOINT, &s_boost_sp, sp_ch},
        {ZB_BOOST_ATTR_DURATION, &s_boost_min, min_ch},
        {ZB_ATTR_SWING, &s_swing, sw_ch},
    };
    /* schedule: same rule (local changes, e.g. setpoint edits of the current period) */
    for (uint16_t id = ZB_SCHED_ATTR_FIRST; id <= ZB_SCHED_ATTR_LAST; id++) {
        sched_cfg_t *lc = (sched_cfg_t *)&last.sched, *nc = (sched_cfg_t *)&st->sched;
        uint16_t a = *(uint16_t *)sched_field(nc, id), b = *(uint16_t *)sched_field(lc, id);
        uint16_t *cur = sched_field(&s_sched, id);
        if (!have_last || a == b || a == *cur) continue;
        *cur = a;
        esp_zb_zcl_set_manufacturer_attribute_val(EP, ZB_BOOST_CLUSTER_ID, SERVER, ZB_BOOST_MANUF_CODE, id, cur, false);
        if (s_link == LINK_JOINED) report_boost_attr(id);
    }
    if (!have_last || st->sched_state != last.sched_state) {
        s_sched_state = st->sched_state;
        esp_zb_zcl_set_manufacturer_attribute_val(EP, ZB_BOOST_CLUSTER_ID, SERVER, ZB_BOOST_MANUF_CODE, ZB_ATTR_SCHED_STATE,
                                                  &s_sched_state, false);
        if (s_link == LINK_JOINED) report_boost_attr(ZB_ATTR_SCHED_STATE);
    }
    for (size_t i = 0; i < sizeof sattrs / sizeof sattrs[0]; i++) {
        if (!sattrs[i].was_changed) continue;
        esp_zb_zcl_set_manufacturer_attribute_val(EP, ZB_BOOST_CLUSTER_ID, SERVER, ZB_BOOST_MANUF_CODE, sattrs[i].id,
                                                  sattrs[i].v, false);
        if (s_link == LINK_JOINED) report_boost_attr(sattrs[i].id);
    }
    for (size_t i = 0; i < sizeof battrs / sizeof battrs[0]; i++) {
        if (!battrs[i].was_changed) continue;
        esp_zb_zcl_set_manufacturer_attribute_val(EP, ZB_BOOST_CLUSTER_ID, SERVER, ZB_BOOST_MANUF_CODE, battrs[i].id,
                                                  battrs[i].v, false);
        if (s_link == LINK_JOINED) report_boost_attr(battrs[i].id);
    }
    esp_zb_lock_release();
    last = *st;
    have_last = true;
}
