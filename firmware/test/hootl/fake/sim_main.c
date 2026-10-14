/* sim_main.c - HOOTL simulator entry point.
 *
 *   tstat_sim --state FILE --script FILE --until MS [--seed N] [--out DIR]
 *
 * One process = one boot of the chip. A reset (esp_restart, task watchdog,
 * STWD100, OTA rollback) saves the persistent state (NVS, otadata, Zigbee
 * NVRAM, sticky fault configuration, virtual time, script cursor) to FILE and
 * exits with SIM_EXIT_RESET; the runner starts the next boot with the same
 * FILE. Output on stdout, one event per line, each prefixed "@<virtual ms> ". */
#include "sim.h"
#include "esp_system.h"
#include "esp_err.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void);

static const char *s_state_path, *s_script_path, *s_out = ".";
static uint64_t s_seed = 1, s_rng;

/* ---------------- kv ---------------- */
#define KV_MAX 256
static struct { char k[40]; int64_t v; } s_kv[KV_MAX];
static int s_nkv;

int64_t sim_kv_get(const char *k, int64_t def)
{
    for (int i = 0; i < s_nkv; i++) if (!strcmp(s_kv[i].k, k)) return s_kv[i].v;
    return def;
}
void sim_kv_set(const char *k, int64_t v)
{
    for (int i = 0; i < s_nkv; i++) if (!strcmp(s_kv[i].k, k)) { s_kv[i].v = v; return; }
    if (s_nkv < KV_MAX) { snprintf(s_kv[s_nkv].k, sizeof s_kv[0].k, "%s", k); s_kv[s_nkv++].v = v; }
}

uint32_t sim_rand(void)
{
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 7; s_rng ^= s_rng << 17;
    return (uint32_t)(s_rng >> 16);
}
const char *sim_outdir(void) { return s_out; }

/* ---------------- output ---------------- */
void sim_emit(const char *fmt, ...)
{
    char buf[1024];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    printf("@%llu %s\n", (unsigned long long)sim_now_ms(), buf);
    fflush(stdout);
}
void sim_log(char level, const char *tag, const char *fmt, ...)
{
    char buf[900];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    sim_emit("%c %s: %s", level, tag, buf);
}

/* ---------------- script ---------------- */
#define CMD_MAX 2048
static struct { uint64_t t_ms; char line[256]; } s_cmds[CMD_MAX];
static int s_ncmd, s_cursor;

static void load_script(void)
{
    FILE *f = fopen(s_script_path, "r");
    if (!f) return;
    char line[300];
    while (s_ncmd < CMD_MAX && fgets(line, sizeof line, f)) {
        char *p = line; while (*p == ' ') p++;
        if (*p == '#' || *p == '\n' || !*p) continue;
        unsigned long long t;
        int n = 0;
        if (sscanf(p, "%llu %n", &t, &n) != 1) continue;
        line[strcspn(line, "\n")] = 0;
        s_cmds[s_ncmd].t_ms = t;
        snprintf(s_cmds[s_ncmd].line, sizeof s_cmds[0].line, "%s", p + n);
        s_ncmd++;
    }
    fclose(f);
}

static void run_cmd(const char *line)
{
    char tmp[256], *argv[16];
    int argc = 0;
    snprintf(tmp, sizeof tmp, "%s", line);
    for (char *t = strtok(tmp, " \t"); t && argc < 16; t = strtok(NULL, " \t")) argv[argc++] = t;
    if (!argc) return;
    sim_emit("CMD %s", line);
    if (!strcmp(argv[0], "zb")) { if (sim_zb_cmd(argv + 1, argc - 1)) return; }
    else if (sim_periph_cmd(argv, argc)) return;
    sim_emit("CMDERR unknown command: %s", line);
    sim_finish(SIM_EXIT_ABORT);
}

static uint64_t next_cmd_ms(void) { return s_cursor < s_ncmd ? s_cmds[s_cursor].t_ms : UINT64_MAX; }

static void run_due_cmds(void)
{
    while (s_cursor < s_ncmd && s_cmds[s_cursor].t_ms <= sim_now_ms()) {
        int i = s_cursor++;   /* advance first: a command may reset the chip */
        run_cmd(s_cmds[i].line);
    }
}

/* highest priority: script commands + watchdog models */
static void sim_task(void *arg)
{
    (void)arg;
    for (;;) {
        run_due_cmds();
        sim_periph_check();
        uint64_t next = next_cmd_ms() == UINT64_MAX ? UINT64_MAX : next_cmd_ms() * 1000u;
        uint64_t d = sim_periph_next_deadline_us();
        if (d < next) next = d;
        uint64_t now = sim_now_us();
        uint64_t wait = next == UINT64_MAX ? 3600000000ull : (next > now ? next - now : 0);
        if (wait < 1000) wait = 1000;
        if (wait > 100000) wait = 100000;   /* re-evaluate watchdog deadlines at least every 100 ms */
        vTaskDelay((TickType_t)(wait / 1000));
    }
}

void sim_start_service_tasks(void) { xTaskCreate(sim_task, "sim", 0, NULL, 24, NULL); }

bool sim_fault_task_create(const char *name)
{
    char k[40];
    snprintf(k, sizeof k, "fault.task.%s", name);
    return sim_kv_get(k, 0) != 0;
}

/* ---------------- state ---------------- */
static void save_state(const char *reason)
{
    FILE *f = fopen(s_state_path, "w");
    if (!f) { perror("state"); _exit(SIM_EXIT_ABORT); }
    fprintf(f, "now_us %llu\ncursor %d\nreset %s\n", (unsigned long long)sim_now_us(), s_cursor, reason);
    for (int i = 0; i < s_nkv; i++) fprintf(f, "kv %s %lld\n", s_kv[i].k, (long long)s_kv[i].v);
    sim_nvs_save(f);
    fclose(f);
}

static char s_reset_reason[32] = "POWERON";

/* What the C6 reports after each kind of sim reset. The STWD100 WDO pulls CHIP_EN,
 * which the chip cannot tell from a power-on. */
esp_reset_reason_t esp_reset_reason(void)
{
    static const struct { const char *n; esp_reset_reason_t r; } M[] = {
        {"POWERON", ESP_RST_POWERON}, {"STWD100", ESP_RST_POWERON}, {"SW", ESP_RST_SW},
        {"OTA_ROLLBACK", ESP_RST_SW}, {"PANIC", ESP_RST_PANIC}, {"TASK_WDT", ESP_RST_TASK_WDT},
    };
    for (unsigned i = 0; i < sizeof M / sizeof M[0]; i++) if (!strcmp(M[i].n, s_reset_reason)) return M[i].r;
    return ESP_RST_UNKNOWN;
}

static void load_state(void)
{
    FILE *f = fopen(s_state_path, "r");
    if (!f) return;
    char line[2200];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = 0;
        unsigned long long u; int c; char k[64]; long long v;
        if (sscanf(line, "now_us %llu", &u) == 1) sim_set_now_us(u + 50000);   /* reset + ROM boot: 50 ms, outputs low (pull-downs) */
        else if (sscanf(line, "cursor %d", &c) == 1) s_cursor = c;
        else if (sscanf(line, "reset %31s", s_reset_reason) == 1) {}
        else if (sscanf(line, "kv %63s %lld", k, &v) == 2) sim_kv_set(k, v);
        else sim_nvs_load_line(line);
    }
    fclose(f);
}

void sim_finish(int code)
{
    save_state(code == SIM_EXIT_DONE ? "END" : "ABORT");
    sim_emit("EXIT %d", code);
    fflush(stdout);
    _exit(code);
}

void sim_reset(const char *reason)
{
    sim_emit("RESET %s", reason);
    save_state(reason);
    fflush(stdout);
    _exit(SIM_EXIT_RESET);
}

void sim_abort(const char *why, const char *file, int line)
{
    sim_emit("PANIC %s at %s:%d", why, file, line);
    sim_reset("PANIC");
}

static void boot(void)
{
    run_due_cmds();          /* everything scheduled up to now: pre-boot fault setup */
    app_main();
}

int main(int argc, char **argv)
{
    uint64_t until = 0;
    for (int i = 1; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--state")) s_state_path = argv[i + 1];
        else if (!strcmp(argv[i], "--script")) s_script_path = argv[i + 1];
        else if (!strcmp(argv[i], "--until")) until = strtoull(argv[i + 1], NULL, 10);
        else if (!strcmp(argv[i], "--seed")) s_seed = strtoull(argv[i + 1], NULL, 10);
        else if (!strcmp(argv[i], "--out")) s_out = argv[i + 1];
    }
    if (!s_state_path || !s_script_path || !until) {
        fprintf(stderr, "usage: tstat_sim --state FILE --script FILE --until MS [--seed N] [--out DIR]\n");
        return 2;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    load_state();
    load_script();
    int64_t boots = sim_kv_get("sim.boots", 0) + 1;
    sim_kv_set("sim.boots", boots);
    s_rng = s_seed * 0x9E3779B97F4A7C15ull + (uint64_t)boots * 0xBF58476D1CE4E5B9ull + 1;
    sim_set_end_us(until * 1000ull);
    if (sim_now_ms() >= until) { sim_emit("EXIT 0"); return 0; }
    sim_emit("BOOT n=%lld reason=%s", (long long)boots, s_reset_reason);
    sim_periph_boot();
    sim_kernel_run(boot);
}
