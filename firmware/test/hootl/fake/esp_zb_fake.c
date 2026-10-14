/* esp_zb_fake.c - lowest-layer fake of esp-zigbee-lib / ZBOSS for the HOOTL sim.
 *
 * zb_thermostat.c is compiled unchanged against the real esp-zigbee-lib headers;
 * only the library is replaced. The fake models the parts of the stack the
 * firmware relies on: startup/commissioning signals, network steering against a
 * scripted coordinator, parent loss / leave, the scheduler alarm queue, the stack
 * lock, the attribute store, reads of the coordinator's Time cluster, and OTA
 * image transfer through the action callback. Joined state persists across
 * reboots (zb_storage). Script commands start with "zb" (see sim_zb_cmd). */
#include "sim.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_zigbee_core.h"
#include "esp_zigbee_ota.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define K(n) sim_kv_get(n, 0)
#define KD(n, d) sim_kv_get(n, d)

static esp_zb_core_action_callback_t s_action;
static SemaphoreHandle_t s_lock;
static TaskHandle_t s_zb_task;
static bool s_linked;          /* has a parent right now (RAM) */

/* ---------------- scheduler alarms ---------------- */
typedef void (*job_fn)(int64_t a, int64_t b);
#define NALARM 64
static struct { bool used; uint64_t at; esp_zb_callback_t cb; uint8_t param; job_fn job; int64_t a, b; } s_al[NALARM];

static void add_alarm(uint64_t at, esp_zb_callback_t cb, uint8_t param, job_fn job, int64_t a, int64_t b)
{
    for (int i = 0; i < NALARM; i++) if (!s_al[i].used) {
        s_al[i].used = true; s_al[i].at = at; s_al[i].cb = cb; s_al[i].param = param; s_al[i].job = job; s_al[i].a = a; s_al[i].b = b;
        if (s_zb_task) xTaskNotifyGive(s_zb_task);
        return;
    }
    sim_emit("ZB alarm table full");
}
void esp_zb_scheduler_alarm(esp_zb_callback_t cb, uint8_t param, uint32_t ms) { add_alarm(sim_now_ms() + ms, cb, param, NULL, 0, 0); }
static void job(uint32_t ms, job_fn f, int64_t a, int64_t b) { add_alarm(sim_now_ms() + ms, NULL, 0, f, a, b); }

bool esp_zb_lock_acquire(TickType_t t) { return s_lock && xSemaphoreTake(s_lock, t) == pdTRUE; }
void esp_zb_lock_release(void) { xSemaphoreGive(s_lock); }

/* ---------------- signals ---------------- */
static void signal(esp_zb_app_signal_type_t sig, esp_err_t st)
{
    uint32_t v = sig;
    esp_zb_app_signal_t s = {.p_app_signal = &v, .esp_err_status = st};
    sim_emit("ZB signal 0x%x status %d", sig, st);
    esp_zb_app_signal_handler(&s);
}
static void j_signal(int64_t sig, int64_t st) { signal((esp_zb_app_signal_type_t)sig, (esp_err_t)st); }

const char *esp_zb_zdo_signal_to_string(esp_zb_app_signal_type_t s) { (void)s; return "signal"; }

/* ---------------- platform / init ---------------- */
esp_err_t esp_zb_platform_config(esp_zb_platform_config_t *c) { (void)c; return K("zb.fail.platform") ? ESP_FAIL : ESP_OK; }
void esp_zb_init(esp_zb_cfg_t *c) { (void)c; s_lock = xSemaphoreCreateMutex(); }
void esp_zb_sleep_enable(bool e) { (void)e; }
void esp_zb_sleep_now(void) {}
void esp_zb_set_rx_on_when_idle(bool r) { (void)r; }
void esp_zb_set_default_long_poll_interval(uint32_t ms) { (void)ms; }
esp_err_t esp_zb_set_primary_network_channel_set(uint32_t m) { (void)m; return ESP_OK; }
esp_err_t esp_zb_device_register(esp_zb_ep_list_t *e) { (void)e; return K("zb.fail.register") ? ESP_FAIL : ESP_OK; }
void esp_zb_core_action_handler_register(esp_zb_core_action_callback_t cb) { s_action = cb; }
uint16_t esp_zb_get_short_address(void) { return 0x1234; }
uint16_t esp_zb_get_pan_id(void) { return 0xBEEF; }
uint8_t esp_zb_get_current_channel(void) { return 15; }
bool esp_zb_bdb_is_factory_new(void) { return !K("zb.joined"); }
bool esp_zb_bdb_dev_joined(void) { return K("zb.joined") && s_linked; }

esp_err_t esp_zb_start(bool autostart)
{
    (void)autostart;
    if (K("zb.fail.start")) return ESP_FAIL;
    job(10, j_signal, ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP, ESP_OK);
    return ESP_OK;
}

static void j_steer_result(int64_t a, int64_t b)
{
    (void)a; (void)b;
    if (KD("zb.coord", 1)) {
        sim_kv_set("zb.joined", 1);
        s_linked = true;
        signal(ESP_ZB_BDB_SIGNAL_STEERING, ESP_OK);
    } else {
        signal(ESP_ZB_BDB_SIGNAL_STEERING, ESP_FAIL);
    }
}

static void j_init_result(int64_t a, int64_t b)
{
    (void)a; (void)b;
    int64_t fails = K("zb.fail.init");
    bool fresh = !K("zb.joined");
    if (fails > 0) { sim_kv_set("zb.fail.init", fails - 1); signal(fresh ? ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START : ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT, ESP_FAIL); return; }
    if (!fresh) s_linked = KD("zb.coord", 1) != 0;   /* rejoin on reboot works only if the network is there */
    if (!fresh && !s_linked) {   /* stack reports the reboot, then loses the parent */
        signal(ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT, ESP_OK);
        job(2000, j_signal, ESP_ZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT, ESP_OK);
        return;
    }
    signal(fresh ? ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START : ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT, ESP_OK);
}

esp_err_t esp_zb_bdb_start_top_level_commissioning(uint8_t mode)
{
    if (mode == ESP_ZB_BDB_MODE_INITIALIZATION) { job(50, j_init_result, 0, 0); return ESP_OK; }
    if (mode == ESP_ZB_BDB_MODE_NETWORK_STEERING) {
        int64_t n = K("zb.steer_attempts") + 1;
        sim_kv_set("zb.steer_attempts", n);
        sim_emit("ZB steering attempt %lld", (long long)n);
        job(3000, j_steer_result, 0, 0);
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

void esp_zb_factory_reset(void)
{
    sim_kv_set("zb.joined", 0);
    sim_emit("ZB factory reset (zb_storage erased)");
    esp_restart();
}

/* ---------------- data model: allocation only, values tracked by (cluster, attr) ---------------- */
esp_zb_attribute_list_t *esp_zb_zcl_attr_list_create(uint16_t id) { (void)id; return calloc(1, sizeof(esp_zb_attribute_list_t)); }
#define MK(name, cfgt) esp_zb_attribute_list_t *name(cfgt *c) { (void)c; return calloc(1, sizeof(esp_zb_attribute_list_t)); }
MK(esp_zb_basic_cluster_create, esp_zb_basic_cluster_cfg_t)
MK(esp_zb_identify_cluster_create, esp_zb_identify_cluster_cfg_t)
MK(esp_zb_thermostat_cluster_create, esp_zb_thermostat_cluster_cfg_t)
MK(esp_zb_fan_control_cluster_create, esp_zb_fan_control_cluster_cfg_t)
MK(esp_zb_temperature_meas_cluster_create, esp_zb_temperature_meas_cluster_cfg_t)
MK(esp_zb_humidity_meas_cluster_create, esp_zb_humidity_meas_cluster_cfg_t)
MK(esp_zb_time_cluster_create, esp_zb_time_cluster_cfg_t)
MK(esp_zb_ota_cluster_create, esp_zb_ota_cluster_cfg_t)
esp_err_t esp_zb_basic_cluster_add_attr(esp_zb_attribute_list_t *l, uint16_t id, void *v) { (void)l; (void)id; (void)v; return ESP_OK; }
esp_err_t esp_zb_thermostat_cluster_add_attr(esp_zb_attribute_list_t *l, uint16_t id, void *v) { (void)l; (void)id; (void)v; return ESP_OK; }
esp_err_t esp_zb_ota_cluster_add_attr(esp_zb_attribute_list_t *l, uint16_t id, void *v) { (void)l; (void)id; (void)v; return ESP_OK; }
esp_zb_cluster_list_t *esp_zb_zcl_cluster_list_create(void) { return calloc(1, sizeof(esp_zb_cluster_list_t)); }
#define ADD(name) esp_err_t name(esp_zb_cluster_list_t *cl, esp_zb_attribute_list_t *l, uint8_t r) { (void)cl; (void)l; (void)r; return ESP_OK; }
ADD(esp_zb_cluster_list_add_basic_cluster) ADD(esp_zb_cluster_list_add_identify_cluster) ADD(esp_zb_cluster_list_add_thermostat_cluster)
ADD(esp_zb_cluster_list_add_fan_control_cluster) ADD(esp_zb_cluster_list_add_temperature_meas_cluster)
ADD(esp_zb_cluster_list_add_humidity_meas_cluster) ADD(esp_zb_cluster_list_add_custom_cluster)
ADD(esp_zb_cluster_list_add_time_cluster) ADD(esp_zb_cluster_list_add_ota_cluster)
esp_zb_ep_list_t *esp_zb_ep_list_create(void) { return calloc(1, sizeof(esp_zb_ep_list_t)); }
esp_err_t esp_zb_ep_list_add_ep(esp_zb_ep_list_t *e, esp_zb_cluster_list_t *c, esp_zb_endpoint_config_t cfg) { (void)e; (void)c; (void)cfg; return ESP_OK; }

#define NATTR 96
static struct { uint16_t cl, id; uint8_t size; int64_t v; bool set; } s_attr[NATTR];
static int attr_idx(uint16_t cl, uint16_t id)
{
    for (int i = 0; i < NATTR; i++) if (s_attr[i].size && s_attr[i].cl == cl && s_attr[i].id == id) return i;
    for (int i = 0; i < NATTR; i++) if (!s_attr[i].size) { s_attr[i].cl = cl; s_attr[i].id = id; s_attr[i].size = 2; return i; }
    return 0;
}
static uint8_t type_size(uint8_t t) { return (t == ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM || t == ESP_ZB_ZCL_ATTR_TYPE_U8 || t == ESP_ZB_ZCL_ATTR_TYPE_S8) ? 1 : 2; }

esp_err_t esp_zb_cluster_add_manufacturer_attr(esp_zb_attribute_list_t *l, uint16_t cl, uint16_t id, uint16_t mc, uint8_t type, uint8_t acc, void *v)
{
    (void)l; (void)mc; (void)acc; (void)v;
    int i = attr_idx(cl, id);
    s_attr[i].size = type_size(type);
    return ESP_OK;
}

static bool is_u8_attr(uint16_t cl, uint16_t id)
{
    return (cl == ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT && id == ESP_ZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID) ||
           (cl == ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL && id == ESP_ZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_ID);
}
static bool is_signed(uint16_t cl, uint16_t id)
{
    return cl == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT || (cl == ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT && id != ESP_ZB_ZCL_ATTR_THERMOSTAT_THERMOSTAT_RUNNING_STATE_ID);
}

static esp_zb_zcl_status_t store(uint16_t cl, uint16_t id, const void *v)
{
    if (!sim_mutex_held(s_lock)) sim_emit("ZB WARN attribute 0x%04x/0x%04x set without the stack lock", cl, id);
    int i = attr_idx(cl, id);
    if (is_u8_attr(cl, id)) s_attr[i].size = 1;
    int64_t val;
    if (s_attr[i].size == 1) val = *(const uint8_t *)v;
    else val = is_signed(cl, id) ? *(const int16_t *)v : *(const uint16_t *)v;
    if (!s_attr[i].set || s_attr[i].v != val) sim_emit("ZB attr 0x%04x/0x%04x = %lld", cl, id, (long long)val);
    s_attr[i].v = val; s_attr[i].set = true;
    return ESP_ZB_ZCL_STATUS_SUCCESS;
}
esp_zb_zcl_status_t esp_zb_zcl_set_attribute_val(uint8_t ep, uint16_t cl, uint8_t role, uint16_t id, void *v, bool chk)
{ (void)ep; (void)role; (void)chk; return store(cl, id, v); }
esp_zb_zcl_status_t esp_zb_zcl_set_manufacturer_attribute_val(uint8_t ep, uint16_t cl, uint8_t role, uint16_t mc, uint16_t id, void *v, bool chk)
{ (void)ep; (void)role; (void)mc; (void)chk; return store(cl, id, v); }

esp_err_t esp_zb_zcl_report_attr_cmd_req(esp_zb_zcl_report_attr_cmd_t *r)
{
    if (!s_linked) return ESP_ERR_INVALID_STATE;
    sim_emit("ZB report 0x%04x/0x%04x", r->clusterID, r->attributeID);
    return ESP_OK;
}

/* ---------------- Time cluster reads ---------------- */
/* kv zb.time.mode: 0 = coordinator does not answer, 1 = answers.
 * zb.time.epoch0 = UTC (s since 2000) at virtual t=0; utc = epoch0 + now/1000.
 * zb.time.status, zb.time.tz, zb.time.dst; zb.time.local_ofs: -1 = LocalTime not supported. */
static void j_time_resp(int64_t a, int64_t b)
{
    (void)a; (void)b;
    if (!s_linked || !K("zb.time.mode")) return;
    static uint32_t utc, local; static uint8_t status; static int32_t tz, dst;
    utc = (uint32_t)(K("zb.time.epoch0") + (int64_t)(sim_now_ms() / 1000));
    status = (uint8_t)KD("zb.time.status", 0x02);
    tz = (int32_t)K("zb.time.tz"); dst = (int32_t)K("zb.time.dst");
    bool has_local = KD("zb.time.local", 1) != 0;
    local = (uint32_t)((int64_t)utc + tz + dst);
    esp_zb_zcl_read_attr_resp_variable_t v[5];
    memset(v, 0, sizeof v);
    struct { uint16_t id; void *p; } f[5] = {
        {ESP_ZB_ZCL_ATTR_TIME_TIME_ID, &utc}, {ESP_ZB_ZCL_ATTR_TIME_TIME_STATUS_ID, &status},
        {ESP_ZB_ZCL_ATTR_TIME_TIME_ZONE_ID, &tz}, {ESP_ZB_ZCL_ATTR_TIME_DST_SHIFT_ID, &dst}, {ESP_ZB_ZCL_ATTR_TIME_LOCAL_TIME_ID, &local}};
    int n = has_local ? 5 : 4;
    for (int i = 0; i < n; i++) {
        v[i].status = ESP_ZB_ZCL_STATUS_SUCCESS;
        v[i].attribute.id = f[i].id;
        v[i].attribute.data.value = f[i].p;
        v[i].next = i + 1 < n ? &v[i + 1] : NULL;
    }
    esp_zb_zcl_cmd_read_attr_resp_message_t m;
    memset(&m, 0, sizeof m);
    m.info.cluster = ESP_ZB_ZCL_CLUSTER_ID_TIME;
    m.info.status = ESP_ZB_ZCL_STATUS_SUCCESS;
    m.variables = v;
    sim_emit("ZB time response utc=%u status=0x%02x local=%s", utc, status, has_local ? "yes" : "no");
    s_action(ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID, &m);
}

uint8_t esp_zb_zcl_read_attr_cmd_req(esp_zb_zcl_read_attr_cmd_t *r)
{
    sim_emit("ZB read 0x%04x (%d attrs)", r->clusterID, r->attr_number);
    if (r->clusterID == ESP_ZB_ZCL_CLUSTER_ID_TIME) job(500, j_time_resp, 0, 0);
    return 0;
}

/* ---------------- OTA client transfer ---------------- */
/* zb ota <size> [abort_at=<block>] [bad=1 (not an ESP image)] [tag=1 (wrong element tag)] [stall_at=<block>] */
static struct { bool on; uint32_t size, off; int block, abort_at, stall_at; bool bad, badtag; } s_ota;

static esp_err_t ota_msg(esp_zb_zcl_ota_upgrade_status_t st, const uint8_t *p, uint16_t n)
{
    esp_zb_zcl_ota_upgrade_value_message_t m;
    memset(&m, 0, sizeof m);
    m.info.status = ESP_ZB_ZCL_STATUS_SUCCESS;
    m.upgrade_status = st;
    m.ota_header.image_size = s_ota.size;
    m.ota_header.file_version = 0x00030100;
    m.payload_size = n;
    m.payload = (uint8_t *)p;
    return s_action(ESP_ZB_CORE_OTA_UPGRADE_VALUE_CB_ID, &m);
}

static void j_ota_block(int64_t a, int64_t b)
{
    (void)a; (void)b;
    if (!s_ota.on) return;
    if (!s_linked) { sim_emit("ZB ota: link lost, aborting transfer"); ota_msg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_ABORT, NULL, 0); s_ota.on = false; return; }
    if (s_ota.block == s_ota.abort_at) { sim_emit("ZB ota: server aborted at block %d", s_ota.block); ota_msg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_ABORT, NULL, 0); s_ota.on = false; return; }
    if (s_ota.block == s_ota.stall_at) { sim_emit("ZB ota: server stopped sending at block %d", s_ota.block); return; }
    uint8_t buf[223];
    uint16_t n = (uint16_t)(s_ota.size - s_ota.off < sizeof buf ? s_ota.size - s_ota.off : sizeof buf);
    for (uint16_t i = 0; i < n; i++) buf[i] = (uint8_t)(s_ota.off + i);
    if (s_ota.off == 0) {
        uint16_t tag = s_ota.badtag ? 5 : 0; uint32_t len = s_ota.size - 6;
        memcpy(buf, &tag, 2); memcpy(buf + 2, &len, 4);
        buf[6] = s_ota.bad ? 0x00 : 0xE9;
    }
    esp_err_t e = ota_msg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE, buf, n);
    s_ota.off += n; s_ota.block++;
    if (e != ESP_OK) { sim_emit("ZB ota: client rejected block -> abort"); ota_msg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_ABORT, NULL, 0); s_ota.on = false; return; }
    if (s_ota.off < s_ota.size) { job(100, j_ota_block, 0, 0); return; }
    s_ota.on = false;
    if (ota_msg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_CHECK, NULL, 0) != ESP_OK) { ota_msg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_ABORT, NULL, 0); return; }
    ota_msg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_FINISH, NULL, 0);   /* reboots on success */
}

static void j_ota_start(int64_t a, int64_t b)
{
    (void)a; (void)b;
    esp_zb_zcl_ota_upgrade_query_image_resp_message_t q;
    memset(&q, 0, sizeof q);
    q.info.status = ESP_ZB_ZCL_STATUS_SUCCESS;
    q.file_version = 0x00030100; q.image_size = s_ota.size;
    s_action(ESP_ZB_CORE_OTA_UPGRADE_QUERY_IMAGE_RESP_CB_ID, &q);
    if (ota_msg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_START, NULL, 0) != ESP_OK) { sim_emit("ZB ota: client refused start"); s_ota.on = false; return; }
    job(100, j_ota_block, 0, 0);
}

static void j_ota_none(int64_t a, int64_t b)
{
    (void)a; (void)b;
    esp_zb_zcl_ota_upgrade_query_image_resp_message_t q;
    memset(&q, 0, sizeof q);
    q.info.status = ESP_ZB_ZCL_STATUS_NO_IMAGE_AVAILABLE;
    sim_emit("ZB ota: query -> NO_IMAGE_AVAILABLE");
    s_action(ESP_ZB_CORE_OTA_UPGRADE_QUERY_IMAGE_RESP_CB_ID, &q);
}

/* ---------------- remote writes ---------------- */
static void j_write(int64_t packed, int64_t val)
{
    uint16_t cl = (uint16_t)(packed >> 24), id = (uint16_t)(packed >> 8); int sz = (int)(packed & 0xFF);
    static uint8_t buf[2];
    if (sz == 1) buf[0] = (uint8_t)val; else { uint16_t v = (uint16_t)val; memcpy(buf, &v, 2); }
    esp_zb_zcl_set_attr_value_message_t m;
    memset(&m, 0, sizeof m);
    m.info.status = ESP_ZB_ZCL_STATUS_SUCCESS;
    m.info.dst_endpoint = 1;
    m.info.cluster = cl;
    m.attribute.id = id;
    m.attribute.data.value = buf;
    m.attribute.data.size = (uint16_t)sz;
    /* the real stack stores the written value before calling the app */
    store(cl, id, buf);
    sim_emit("ZB remote write 0x%04x/0x%04x = %lld", cl, id, (long long)val);
    s_action(ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID, &m);
}

static void j_lost(int64_t a, int64_t b) { (void)a; (void)b; s_linked = false; signal(ESP_ZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT, ESP_OK); }
static void j_leave(int64_t a, int64_t b) { (void)a; (void)b; s_linked = false; sim_kv_set("zb.joined", 0); signal(ESP_ZB_ZDO_SIGNAL_LEAVE, ESP_OK); }

/* ---------------- main loop ---------------- */
void esp_zb_stack_main_loop(void)
{
    s_zb_task = xTaskGetCurrentTaskHandle();
    for (;;) {
        if (K("zb.die")) { sim_emit("ZB task exits (injected)"); s_zb_task = NULL; vTaskDelete(NULL); }
        if (K("zb.stall")) { sim_emit("ZB task stalls holding the stack lock (injected)"); xSemaphoreTake(s_lock, portMAX_DELAY); vTaskDelay(portMAX_DELAY); }
        uint64_t now = sim_now_ms(), next = UINT64_MAX;
        int due = -1;
        for (int i = 0; i < NALARM; i++) if (s_al[i].used) {
            if (s_al[i].at <= now && (due < 0 || s_al[i].at < s_al[due].at)) due = i;
            if (s_al[i].at < next) next = s_al[i].at;
        }
        if (due >= 0) {
            typeof(s_al[0]) a = s_al[due];
            s_al[due].used = false;
            xSemaphoreTake(s_lock, portMAX_DELAY);
            if (a.cb) a.cb(a.param); else a.job(a.a, a.b);
            xSemaphoreGive(s_lock);
            continue;
        }
        uint32_t wait = next == UINT64_MAX ? 1000 : (uint32_t)(next - now);
        ulTaskNotifyTake(pdTRUE, wait > 1000 ? 1000 : wait);   /* poll kv injections at least once a second */
    }
}

/* ---------------- script ---------------- */
static int64_t num(const char *s) { return strtoll(s, NULL, 0); }
static int64_t opt(char **a, int n, const char *k, int64_t d)
{
    size_t l = strlen(k);
    for (int i = 0; i < n; i++) if (!strncmp(a[i], k, l) && a[i][l] == '=') return num(a[i] + l + 1);
    return d;
}

bool sim_zb_cmd(char **a, int n)
{
    if (n < 1) return false;
    if (!strcmp(a[0], "coord") && n == 2) { sim_kv_set("zb.coord", !strcmp(a[1], "up")); return true; }
    if (!strcmp(a[0], "joined") && n == 2) { sim_kv_set("zb.joined", num(a[1])); return true; }
    if (!strcmp(a[0], "lost")) { job(0, j_lost, 0, 0); return true; }
    if (!strcmp(a[0], "leave")) { job(0, j_leave, 0, 0); return true; }
    if (!strcmp(a[0], "stall")) { sim_kv_set("zb.stall", 1); if (s_zb_task) xTaskNotifyGive(s_zb_task); return true; }
    if (!strcmp(a[0], "die")) { sim_kv_set("zb.die", 1); if (s_zb_task) xTaskNotifyGive(s_zb_task); return true; }
    if (!strcmp(a[0], "heal")) { sim_kv_set("zb.stall", 0); sim_kv_set("zb.die", 0); return true; }
    if (!strcmp(a[0], "fail") && n >= 2) {
        char k[40]; snprintf(k, sizeof k, "zb.fail.%s", a[1]);
        sim_kv_set(k, n >= 3 ? num(a[2]) : 1);
        return true;
    }
    if (!strcmp(a[0], "write") && n == 5) {
        int sz = !strcmp(a[3], "u8") || !strcmp(a[3], "s8") ? 1 : 2;
        int64_t packed = (num(a[1]) << 24) | (num(a[2]) << 8) | sz;
        job(0, j_write, packed, num(a[4]));
        return true;
    }
    if (!strcmp(a[0], "time") && n >= 2) {
        if (!strcmp(a[1], "off")) { sim_kv_set("zb.time.mode", 0); return true; }
        /* zb time <utc-at-t0> [status=] [tz=] [dst=] [local=0|1] */
        sim_kv_set("zb.time.mode", 1);
        sim_kv_set("zb.time.epoch0", num(a[1]));
        sim_kv_set("zb.time.status", opt(a, n, "status", 0x02));
        sim_kv_set("zb.time.tz", opt(a, n, "tz", 0));
        sim_kv_set("zb.time.dst", opt(a, n, "dst", 0));
        sim_kv_set("zb.time.local", opt(a, n, "local", 1));
        return true;
    }
    if (!strcmp(a[0], "ota") && n >= 2) {
        if (!strcmp(a[1], "none")) { job(0, j_ota_none, 0, 0); return true; }
        memset(&s_ota, 0, sizeof s_ota);
        s_ota.on = true; s_ota.size = (uint32_t)num(a[1]);
        s_ota.abort_at = (int)opt(a, n, "abort_at", -1);
        s_ota.stall_at = (int)opt(a, n, "stall_at", -1);
        s_ota.bad = opt(a, n, "bad", 0); s_ota.badtag = opt(a, n, "tag", 0);
        job(0, j_ota_start, 0, 0);
        return true;
    }
    return false;
}
