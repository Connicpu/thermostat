/* esp_zb_fake.c - lowest-layer fake of esp-zigbee-lib / ZBOSS for the HOOTL sim.
 *
 * zb_thermostat.c is compiled unchanged against the real esp-zigbee-lib headers;
 * only the library is replaced. The fake models the parts of the stack the
 * firmware relies on: startup/commissioning signals, network steering against a
 * scripted coordinator, parent loss / leave, the scheduler alarm queue, the stack
 * lock, the attribute store, reads of the coordinator's Time cluster, and OTA
 * image transfer through the action callback. Joined state persists across
 * reboots (zb_storage). Script commands start with "zb" (see SimZbCmd). */
#include "core.h"
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

#define K(n) SimKvGet(n, 0)
#define KD(n, d) SimKvGet(n, d)

static esp_zb_core_action_callback_t s_action;
static SemaphoreHandle_t s_lock;
static TaskHandle_t s_zbTask;
static bool s_linked;          /* has a parent right now (RAM) */

/* ---------------- scheduler alarms ---------------- */
typedef void (*JobFn)(I64 a, I64 b);
#define NALARM 64
static struct { bool used; U64 at; esp_zb_callback_t cb; U8 param; JobFn job; I64 a, b; } s_al[NALARM];

static void AddAlarm(U64 at, esp_zb_callback_t cb, U8 param, JobFn job, I64 a, I64 b)
{
    for (int i = 0; i < NALARM; i++) if (!s_al[i].used) {
        s_al[i].used = true; s_al[i].at = at; s_al[i].cb = cb; s_al[i].param = param; s_al[i].job = job; s_al[i].a = a; s_al[i].b = b;
        if (s_zbTask) xTaskNotifyGive(s_zbTask);
        return;
    }
    SimEmit("ZB alarm table full");
}
void esp_zb_scheduler_alarm(esp_zb_callback_t cb, U8 param, U32 ms) { AddAlarm(SimNowMs() + ms, cb, param, NULL, 0, 0); }
static void Job(U32 ms, JobFn f, I64 a, I64 b) { AddAlarm(SimNowMs() + ms, NULL, 0, f, a, b); }

bool esp_zb_lock_acquire(TickType_t t) { return s_lock && xSemaphoreTake(s_lock, t) == pdTRUE; }
void esp_zb_lock_release(void) { xSemaphoreGive(s_lock); }

/* ---------------- signals ---------------- */
static void Signal(esp_zb_app_signal_type_t sig, esp_err_t st)
{
    U32 v = sig;
    esp_zb_app_signal_t s = {.p_app_signal = &v, .esp_err_status = st};
    SimEmit("ZB signal 0x%x status %d", sig, st);
    esp_zb_app_signal_handler(&s);
}
static void JSignal(I64 sig, I64 st) { Signal((esp_zb_app_signal_type_t)sig, (esp_err_t)st); }

const char *esp_zb_zdo_signal_to_string(esp_zb_app_signal_type_t s) { (void)s; return "signal"; }

/* ---------------- platform / init ---------------- */
esp_err_t esp_zb_platform_config(esp_zb_platform_config_t *pC) { (void)pC; return K("zb.fail.platform") ? ESP_FAIL : ESP_OK; }
void esp_zb_init(esp_zb_cfg_t *pC) { (void)pC; s_lock = xSemaphoreCreateMutex(); }
void esp_zb_sleep_enable(bool e) { (void)e; }
void esp_zb_sleep_now(void) {}
void esp_zb_set_rx_on_when_idle(bool r) { (void)r; }
void esp_zb_set_default_long_poll_interval(U32 ms) { (void)ms; }
esp_err_t esp_zb_set_primary_network_channel_set(U32 m) { (void)m; return ESP_OK; }
esp_err_t esp_zb_device_register(esp_zb_ep_list_t *pE) { (void)pE; return K("zb.fail.register") ? ESP_FAIL : ESP_OK; }
void esp_zb_core_action_handler_register(esp_zb_core_action_callback_t cb) { s_action = cb; }
U16 esp_zb_get_short_address(void) { return 0x1234; }
U16 esp_zb_get_pan_id(void) { return 0xBEEF; }
U8 esp_zb_get_current_channel(void) { return 15; }
bool esp_zb_bdb_is_factory_new(void) { return !K("zb.joined"); }
bool esp_zb_bdb_dev_joined(void) { return K("zb.joined") && s_linked; }

esp_err_t esp_zb_start(bool autostart)
{
    (void)autostart;
    if (K("zb.fail.start")) return ESP_FAIL;
    Job(10, JSignal, ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP, ESP_OK);
    return ESP_OK;
}

static void JSteerResult(I64 a, I64 b)
{
    (void)a; (void)b;
    if (KD("zb.coord", 1)) {
        SimKvSet("zb.joined", 1);
        s_linked = true;
        Signal(ESP_ZB_BDB_SIGNAL_STEERING, ESP_OK);
    } else {
        Signal(ESP_ZB_BDB_SIGNAL_STEERING, ESP_FAIL);
    }
}

static void JInitResult(I64 a, I64 b)
{
    (void)a; (void)b;
    I64 fails = K("zb.fail.init");
    bool fresh = !K("zb.joined");
    if (fails > 0) { SimKvSet("zb.fail.init", fails - 1); Signal(fresh ? ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START : ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT, ESP_FAIL); return; }
    if (!fresh) s_linked = KD("zb.coord", 1) != 0;   /* rejoin on reboot works only if the network is there */
    if (!fresh && !s_linked && K("zb.reboot_fail")) {
        /* variant: the stack reports the failed rejoin as a failed DEVICE_REBOOT */
        Signal(ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT, ESP_FAIL);
        return;
    }
    if (!fresh && !s_linked) {   /* stack reports the reboot, then loses the parent */
        Signal(ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT, ESP_OK);
        Job(2000, JSignal, ESP_ZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT, ESP_OK);
        return;
    }
    Signal(fresh ? ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START : ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT, ESP_OK);
}

esp_err_t esp_zb_bdb_start_top_level_commissioning(U8 mode)
{
    if (mode == ESP_ZB_BDB_MODE_INITIALIZATION) { Job(50, JInitResult, 0, 0); return ESP_OK; }
    if (mode == ESP_ZB_BDB_MODE_NETWORK_STEERING) {
        I64 n = K("zb.steer_attempts") + 1;
        SimKvSet("zb.steer_attempts", n);
        SimEmit("ZB steering attempt %lld", (long long)n);
        Job(3000, JSteerResult, 0, 0);
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

void esp_zb_factory_reset(void)
{
    SimKvSet("zb.joined", 0);
    SimEmit("ZB factory reset (zb_storage erased)");
    esp_restart();
}

/* ---------------- data model: allocation only, values tracked by (cluster, attr) ---------------- */
esp_zb_attribute_list_t *esp_zb_zcl_attr_list_create(U16 id) { (void)id; return calloc(1, sizeof(esp_zb_attribute_list_t)); }
#define MK(name, cfgt) esp_zb_attribute_list_t *name(cfgt *pCfg) { (void)pCfg; return calloc(1, sizeof(esp_zb_attribute_list_t)); }
MK(esp_zb_basic_cluster_create, esp_zb_basic_cluster_cfg_t)
MK(esp_zb_identify_cluster_create, esp_zb_identify_cluster_cfg_t)
MK(esp_zb_thermostat_cluster_create, esp_zb_thermostat_cluster_cfg_t)
MK(esp_zb_fan_control_cluster_create, esp_zb_fan_control_cluster_cfg_t)
MK(esp_zb_temperature_meas_cluster_create, esp_zb_temperature_meas_cluster_cfg_t)
MK(esp_zb_humidity_meas_cluster_create, esp_zb_humidity_meas_cluster_cfg_t)
MK(esp_zb_time_cluster_create, esp_zb_time_cluster_cfg_t)
MK(esp_zb_ota_cluster_create, esp_zb_ota_cluster_cfg_t)
esp_err_t esp_zb_basic_cluster_add_attr(esp_zb_attribute_list_t *pL, U16 id, void *pV) { (void)pL; (void)id; (void)pV; return ESP_OK; }
esp_err_t esp_zb_thermostat_cluster_add_attr(esp_zb_attribute_list_t *pL, U16 id, void *pV) { (void)pL; (void)id; (void)pV; return ESP_OK; }
esp_err_t esp_zb_ota_cluster_add_attr(esp_zb_attribute_list_t *pL, U16 id, void *pV) { (void)pL; (void)id; (void)pV; return ESP_OK; }
esp_zb_cluster_list_t *esp_zb_zcl_cluster_list_create(void) { return calloc(1, sizeof(esp_zb_cluster_list_t)); }
#define ADD(name) esp_err_t name(esp_zb_cluster_list_t *pCl, esp_zb_attribute_list_t *pL, U8 r) { (void)pCl; (void)pL; (void)r; return ESP_OK; }
ADD(esp_zb_cluster_list_add_basic_cluster) ADD(esp_zb_cluster_list_add_identify_cluster) ADD(esp_zb_cluster_list_add_thermostat_cluster)
ADD(esp_zb_cluster_list_add_fan_control_cluster) ADD(esp_zb_cluster_list_add_temperature_meas_cluster)
ADD(esp_zb_cluster_list_add_humidity_meas_cluster) ADD(esp_zb_cluster_list_add_custom_cluster)
ADD(esp_zb_cluster_list_add_time_cluster) ADD(esp_zb_cluster_list_add_ota_cluster)
esp_zb_ep_list_t *esp_zb_ep_list_create(void) { return calloc(1, sizeof(esp_zb_ep_list_t)); }
esp_err_t esp_zb_ep_list_add_ep(esp_zb_ep_list_t *pE, esp_zb_cluster_list_t *pC, esp_zb_endpoint_config_t cfg) { (void)pE; (void)pC; (void)cfg; return ESP_OK; }

#define NATTR 96
static struct { U16 cl, id; U8 size; I64 v; bool set; } s_attr[NATTR];
static int AttrIdx(U16 cl, U16 id)
{
    for (int i = 0; i < NATTR; i++) if (s_attr[i].size && s_attr[i].cl == cl && s_attr[i].id == id) return i;
    for (int i = 0; i < NATTR; i++) if (!s_attr[i].size) { s_attr[i].cl = cl; s_attr[i].id = id; s_attr[i].size = 2; return i; }
    return 0;
}
static U8 TypeSize(U8 t) { return (t == ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM || t == ESP_ZB_ZCL_ATTR_TYPE_U8 || t == ESP_ZB_ZCL_ATTR_TYPE_S8) ? 1 : 2; }

esp_err_t esp_zb_cluster_add_manufacturer_attr(esp_zb_attribute_list_t *pL, U16 cl, U16 id, U16 mc, U8 type, U8 acc, void *pV)
{
    (void)pL; (void)mc; (void)acc; (void)pV;
    int i = AttrIdx(cl, id);
    s_attr[i].size = TypeSize(type);
    return ESP_OK;
}

static bool IsU8Attr(U16 cl, U16 id)
{
    return (cl == ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT && id == ESP_ZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID) ||
           (cl == ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL && id == ESP_ZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_ID);
}
static bool IsSigned(U16 cl, U16 id)
{
    return cl == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT || (cl == ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT && id != ESP_ZB_ZCL_ATTR_THERMOSTAT_THERMOSTAT_RUNNING_STATE_ID);
}

static esp_zb_zcl_status_t Store(U16 cl, U16 id, const void *pV)
{
    if (!SimMutexHeld(s_lock)) SimEmit("ZB WARN attribute 0x%04x/0x%04x set without the stack lock", cl, id);
    int i = AttrIdx(cl, id);
    if (IsU8Attr(cl, id)) s_attr[i].size = 1;
    I64 val;
    if (s_attr[i].size == 1) val = *(const U8 *)pV;
    else val = IsSigned(cl, id) ? *(const I16 *)pV : *(const U16 *)pV;
    if (!s_attr[i].set || s_attr[i].v != val) SimEmit("ZB attr 0x%04x/0x%04x = %lld", cl, id, (long long)val);
    s_attr[i].v = val; s_attr[i].set = true;
    return ESP_ZB_ZCL_STATUS_SUCCESS;
}
esp_zb_zcl_status_t esp_zb_zcl_set_attribute_val(U8 ep, U16 cl, U8 role, U16 id, void *pV, bool chk)
{ (void)ep; (void)role; (void)chk; return Store(cl, id, pV); }
esp_zb_zcl_status_t esp_zb_zcl_set_manufacturer_attribute_val(U8 ep, U16 cl, U8 role, U16 mc, U16 id, void *pV, bool chk)
{ (void)ep; (void)role; (void)mc; (void)chk; return Store(cl, id, pV); }

esp_err_t esp_zb_zcl_report_attr_cmd_req(esp_zb_zcl_report_attr_cmd_t *pR)
{
    if (!s_linked) return ESP_ERR_INVALID_STATE;
    SimEmit("ZB report 0x%04x/0x%04x", pR->clusterID, pR->attributeID);
    return ESP_OK;
}

/* ---------------- Time cluster reads ---------------- */
/* kv zb.time.mode: 0 = coordinator does not answer, 1 = answers.
 * zb.time.epoch0 = UTC (s since 2000) at virtual t=0; utc = epoch0 + now/1000.
 * zb.time.status, zb.time.tz, zb.time.dst; zb.time.local_ofs: -1 = LocalTime not supported. */
static void JTimeResp(I64 a, I64 b)
{
    (void)a; (void)b;
    if (!s_linked || !K("zb.time.mode")) return;
    static U32 s_utc, s_local; static U8 s_status; static I32 s_tz, s_dst;
    s_utc = (U32)(K("zb.time.epoch0") + (I64)(SimNowMs() / 1000));
    s_status = (U8)KD("zb.time.status", 0x02);
    s_tz = (I32)K("zb.time.tz"); s_dst = (I32)K("zb.time.dst");
    bool hasLocal = KD("zb.time.local", 1) != 0;
    s_local = (U32)((I64)s_utc + s_tz + s_dst);
    esp_zb_zcl_read_attr_resp_variable_t v[5];
    memset(v, 0, sizeof v);
    struct { U16 id; void *pP; } f[5] = {
        {ESP_ZB_ZCL_ATTR_TIME_TIME_ID, &s_utc}, {ESP_ZB_ZCL_ATTR_TIME_TIME_STATUS_ID, &s_status},
        {ESP_ZB_ZCL_ATTR_TIME_TIME_ZONE_ID, &s_tz}, {ESP_ZB_ZCL_ATTR_TIME_DST_SHIFT_ID, &s_dst}, {ESP_ZB_ZCL_ATTR_TIME_LOCAL_TIME_ID, &s_local}};
    int n = hasLocal ? 5 : 4;
    for (int i = 0; i < n; i++) {
        v[i].status = ESP_ZB_ZCL_STATUS_SUCCESS;
        v[i].attribute.id = f[i].id;
        v[i].attribute.data.value = f[i].pP;
        v[i].next = i + 1 < n ? &v[i + 1] : NULL;
    }
    esp_zb_zcl_cmd_read_attr_resp_message_t m;
    memset(&m, 0, sizeof m);
    m.info.cluster = ESP_ZB_ZCL_CLUSTER_ID_TIME;
    m.info.status = ESP_ZB_ZCL_STATUS_SUCCESS;
    m.variables = v;
    SimEmit("ZB time response utc=%u status=0x%02x local=%s", s_utc, s_status, hasLocal ? "yes" : "no");
    s_action(ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID, &m);
}

U8 esp_zb_zcl_read_attr_cmd_req(esp_zb_zcl_read_attr_cmd_t *pR)
{
    SimEmit("ZB read 0x%04x (%d attrs)", pR->clusterID, pR->attr_number);
    if (pR->clusterID == ESP_ZB_ZCL_CLUSTER_ID_TIME) Job(500, JTimeResp, 0, 0);
    return 0;
}

/* ---------------- OTA client transfer ---------------- */
/* zb ota <size> [abortAt=<block>] [bad=1 (not an ESP image)] [tag=1 (wrong element tag)] [stallAt=<block>] */
static struct { bool on; U32 size, off; int block, abortAt, stallAt; bool bad, badtag; } s_ota;

static esp_err_t OtaMsg(esp_zb_zcl_ota_upgrade_status_t st, const U8 *pP, U16 n)
{
    esp_zb_zcl_ota_upgrade_value_message_t m;
    memset(&m, 0, sizeof m);
    m.info.status = ESP_ZB_ZCL_STATUS_SUCCESS;
    m.upgrade_status = st;
    m.ota_header.image_size = s_ota.size;
    m.ota_header.file_version = 0x00030100;
    m.payload_size = n;
    m.payload = (U8 *)pP;
    return s_action(ESP_ZB_CORE_OTA_UPGRADE_VALUE_CB_ID, &m);
}

static void JOtaBlock(I64 a, I64 b)
{
    (void)a; (void)b;
    if (!s_ota.on) return;
    if (!s_linked) { SimEmit("ZB ota: link lost, aborting transfer"); OtaMsg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_ABORT, NULL, 0); s_ota.on = false; return; }
    if (s_ota.block == s_ota.abortAt) { SimEmit("ZB ota: server aborted at block %d", s_ota.block); OtaMsg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_ABORT, NULL, 0); s_ota.on = false; return; }
    if (s_ota.block == s_ota.stallAt) { SimEmit("ZB ota: server stopped sending at block %d", s_ota.block); return; }
    U8 buf[223];
    U16 n = (U16)(s_ota.size - s_ota.off < sizeof buf ? s_ota.size - s_ota.off : sizeof buf);
    for (U16 i = 0; i < n; i++) buf[i] = (U8)(s_ota.off + i);
    if (s_ota.off == 0) {
        U16 tag = s_ota.badtag ? 5 : 0; U32 len = s_ota.size - 6;
        memcpy(buf, &tag, 2); memcpy(buf + 2, &len, 4);
        buf[6] = s_ota.bad ? 0x00 : 0xE9;
    }
    esp_err_t e = OtaMsg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE, buf, n);
    s_ota.off += n; s_ota.block++;
    if (e != ESP_OK) { SimEmit("ZB ota: client rejected block -> abort"); OtaMsg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_ABORT, NULL, 0); s_ota.on = false; return; }
    if (s_ota.off < s_ota.size) { Job(100, JOtaBlock, 0, 0); return; }
    s_ota.on = false;
    /* esp-zigbee-lib 1.6 client order after the last block: APPLY, CHECK, FINISH */
    OtaMsg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_APPLY, NULL, 0);
    if (OtaMsg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_CHECK, NULL, 0) != ESP_OK) { OtaMsg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_ABORT, NULL, 0); return; }
    OtaMsg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_FINISH, NULL, 0);   /* reboots on success */
}

static void JOtaStart(I64 a, I64 b)
{
    (void)a; (void)b;
    esp_zb_zcl_ota_upgrade_query_image_resp_message_t q;
    memset(&q, 0, sizeof q);
    q.info.status = ESP_ZB_ZCL_STATUS_SUCCESS;
    q.file_version = 0x00030100; q.image_size = s_ota.size;
    s_action(ESP_ZB_CORE_OTA_UPGRADE_QUERY_IMAGE_RESP_CB_ID, &q);
    if (OtaMsg(ESP_ZB_ZCL_OTA_UPGRADE_STATUS_START, NULL, 0) != ESP_OK) { SimEmit("ZB ota: client refused start"); s_ota.on = false; return; }
    Job(100, JOtaBlock, 0, 0);
}

static void JOtaNone(I64 a, I64 b)
{
    (void)a; (void)b;
    esp_zb_zcl_ota_upgrade_query_image_resp_message_t q;
    memset(&q, 0, sizeof q);
    q.info.status = ESP_ZB_ZCL_STATUS_NO_IMAGE_AVAILABLE;
    SimEmit("ZB ota: query -> NO_IMAGE_AVAILABLE");
    s_action(ESP_ZB_CORE_OTA_UPGRADE_QUERY_IMAGE_RESP_CB_ID, &q);
}

/* ---------------- remote writes ---------------- */
static void JWrite(I64 packed, I64 val)
{
    U16 cl = (U16)(packed >> 24), id = (U16)(packed >> 8); int sz = (int)(packed & 0xFF);
    static U8 s_buf[2];
    if (sz == 1) s_buf[0] = (U8)val; else { U16 v = (U16)val; memcpy(s_buf, &v, 2); }
    esp_zb_zcl_set_attr_value_message_t m;
    memset(&m, 0, sizeof m);
    m.info.status = ESP_ZB_ZCL_STATUS_SUCCESS;
    m.info.dst_endpoint = 1;
    m.info.cluster = cl;
    m.attribute.id = id;
    m.attribute.data.value = s_buf;
    m.attribute.data.size = (U16)sz;
    /* the real stack stores the written value before calling the app */
    Store(cl, id, s_buf);
    SimEmit("ZB remote write 0x%04x/0x%04x = %lld", cl, id, (long long)val);
    s_action(ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID, &m);
}

static void JLost(I64 a, I64 b) { (void)a; (void)b; s_linked = false; Signal(ESP_ZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT, ESP_OK); }
static void JLeave(I64 a, I64 b) { (void)a; (void)b; s_linked = false; SimKvSet("zb.joined", 0); Signal(ESP_ZB_ZDO_SIGNAL_LEAVE, ESP_OK); }

/* ---------------- main loop ---------------- */
void esp_zb_stack_main_loop(void)
{
    s_zbTask = xTaskGetCurrentTaskHandle();
    for (;;) {
        if (K("zb.die")) { SimEmit("ZB task exits (injected)"); s_zbTask = NULL; vTaskDelete(NULL); }
        if (K("zb.stall")) { SimEmit("ZB task stalls holding the stack lock (injected)"); xSemaphoreTake(s_lock, portMAX_DELAY); vTaskDelay(portMAX_DELAY); }
        U64 now = SimNowMs(), next = UINT64_MAX;
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
        U32 wait = next == UINT64_MAX ? 1000 : (U32)(next - now);
        ulTaskNotifyTake(pdTRUE, wait > 1000 ? 1000 : wait);   /* poll kv injections at least once a second */
    }
}

/* ---------------- script ---------------- */
static I64 Num(const char *pS) { return strtoll(pS, NULL, 0); }
static I64 Opt(char **pA, int n, const char *pK, I64 d)
{
    size_t l = strlen(pK);
    for (int i = 0; i < n; i++) if (!strncmp(pA[i], pK, l) && pA[i][l] == '=') return Num(pA[i] + l + 1);
    return d;
}

bool SimZbCmd(char **pA, int n)
{
    if (n < 1) return false;
    if (!strcmp(pA[0], "coord") && n == 2) { SimKvSet("zb.coord", !strcmp(pA[1], "up")); return true; }
    if (!strcmp(pA[0], "joined") && n == 2) { SimKvSet("zb.joined", Num(pA[1])); return true; }
    if (!strcmp(pA[0], "lost")) { Job(0, JLost, 0, 0); return true; }
    if (!strcmp(pA[0], "leave")) { Job(0, JLeave, 0, 0); return true; }
    if (!strcmp(pA[0], "stall")) { SimKvSet("zb.stall", 1); if (s_zbTask) xTaskNotifyGive(s_zbTask); return true; }
    if (!strcmp(pA[0], "die")) { SimKvSet("zb.die", 1); if (s_zbTask) xTaskNotifyGive(s_zbTask); return true; }
    if (!strcmp(pA[0], "heal")) { SimKvSet("zb.stall", 0); SimKvSet("zb.die", 0); return true; }
    if (!strcmp(pA[0], "fail") && n >= 2) {
        char k[40]; snprintf(k, sizeof k, "zb.fail.%s", pA[1]);
        SimKvSet(k, n >= 3 ? Num(pA[2]) : 1);
        return true;
    }
    if (!strcmp(pA[0], "write") && n == 5) {
        int sz = !strcmp(pA[3], "u8") || !strcmp(pA[3], "s8") ? 1 : 2;
        I64 packed = (Num(pA[1]) << 24) | (Num(pA[2]) << 8) | sz;
        Job(0, JWrite, packed, Num(pA[4]));
        return true;
    }
    if (!strcmp(pA[0], "time") && n >= 2) {
        if (!strcmp(pA[1], "off")) { SimKvSet("zb.time.mode", 0); return true; }
        /* zb time <utc-at-t0> [status=] [tz=] [dst=] [local=0|1] */
        SimKvSet("zb.time.mode", 1);
        SimKvSet("zb.time.epoch0", Num(pA[1]));
        SimKvSet("zb.time.status", Opt(pA, n, "status", 0x02));
        SimKvSet("zb.time.tz", Opt(pA, n, "tz", 0));
        SimKvSet("zb.time.dst", Opt(pA, n, "dst", 0));
        SimKvSet("zb.time.local", Opt(pA, n, "local", 1));
        return true;
    }
    if (!strcmp(pA[0], "ota") && n >= 2) {
        if (!strcmp(pA[1], "none")) { Job(0, JOtaNone, 0, 0); return true; }
        memset(&s_ota, 0, sizeof s_ota);
        s_ota.on = true; s_ota.size = (U32)Num(pA[1]);
        s_ota.abortAt = (int)Opt(pA, n, "abort_at", -1);
        s_ota.stallAt = (int)Opt(pA, n, "stall_at", -1);
        s_ota.bad = Opt(pA, n, "bad", 0); s_ota.badtag = Opt(pA, n, "tag", 0);
        Job(0, JOtaStart, 0, 0);
        return true;
    }
    return false;
}
