/* sim_main.c - HOOTL simulator entry point.
 *
 *   tstat_sim --state FILE --script FILE --until MS [--seed N] [--out DIR]
 *
 * One process = one boot of the chip. A reset (esp_restart, task watchdog,
 * STWD100, OTA rollback) saves the persistent state (NVS, otadata, Zigbee
 * NVRAM, sticky fault configuration, virtual time, script cursor) to FILE and
 * exits with kSimExitReset; the runner starts the next boot with the same
 * FILE. Output on stdout, one event per line, each prefixed "@<virtual ms> ". */
#include "core.h"
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

static const char *s_pStatePath, *s_pScriptPath, *s_pOut = ".";
static U64 s_seed = 1, s_rng;

/* ---------------- kv ---------------- */
#define KV_MAX 256
static struct { char k[40]; I64 v; } s_kv[KV_MAX];
static int s_nkv;

I64 SimKvGet(const char *pK, I64 def)
{
    for (int i = 0; i < s_nkv; i++) if (!strcmp(s_kv[i].k, pK)) return s_kv[i].v;
    return def;
}
void SimKvSet(const char *pK, I64 v)
{
    for (int i = 0; i < s_nkv; i++) if (!strcmp(s_kv[i].k, pK)) { s_kv[i].v = v; return; }
    if (s_nkv < KV_MAX) { snprintf(s_kv[s_nkv].k, sizeof s_kv[0].k, "%s", pK); s_kv[s_nkv++].v = v; }
}

U32 SimRand(void)
{
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 7; s_rng ^= s_rng << 17;
    return (U32)(s_rng >> 16);
}
const char *SimOutdir(void) { return s_pOut; }

/* ---------------- output ---------------- */
void SimEmit(const char *pFmt, ...)
{
    char buf[1024];
    va_list ap; va_start(ap, pFmt); vsnprintf(buf, sizeof buf, pFmt, ap); va_end(ap);
    printf("@%llu %s\n", (unsigned long long)SimNowMs(), buf);
    fflush(stdout);
}
void SimLog(char level, const char *pTag, const char *pFmt, ...)
{
    char buf[900];
    va_list ap; va_start(ap, pFmt); vsnprintf(buf, sizeof buf, pFmt, ap); va_end(ap);
    SimEmit("%c %s: %s", level, pTag, buf);
}

/* ---------------- script ---------------- */
#define CMD_MAX 2048
static struct { U64 tMs; char line[256]; } s_cmds[CMD_MAX];
static int s_ncmd, s_cursor;

static void LoadScript(void)
{
    FILE *pF = fopen(s_pScriptPath, "r");
    if (!pF) return;
    char line[300];
    while (s_ncmd < CMD_MAX && fgets(line, sizeof line, pF)) {
        char *pP = line; while (*pP == ' ') pP++;
        if (*pP == '#' || *pP == '\n' || !*pP) continue;
        unsigned long long t;
        int n = 0;
        if (sscanf(pP, "%llu %n", &t, &n) != 1) continue;
        line[strcspn(line, "\n")] = 0;
        s_cmds[s_ncmd].tMs = t;
        snprintf(s_cmds[s_ncmd].line, sizeof s_cmds[0].line, "%s", pP + n);
        s_ncmd++;
    }
    fclose(pF);
}

static void RunCmd(const char *pLine)
{
    char tmp[256], *argv[16];
    int argc = 0;
    snprintf(tmp, sizeof tmp, "%s", pLine);
    for (char *pT = strtok(tmp, " \t"); pT && argc < 16; pT = strtok(NULL, " \t")) argv[argc++] = pT;
    if (!argc) return;
    SimEmit("CMD %s", pLine);
    if (!strcmp(argv[0], "zb")) { if (SimZbCmd(argv + 1, argc - 1)) return; }
    else if (SimPeriphCmd(argv, argc)) return;
    SimEmit("CMDERR unknown command: %s", pLine);
    SimFinish(kSimExitAbort);
}

static U64 NextCmdMs(void) { return s_cursor < s_ncmd ? s_cmds[s_cursor].tMs : UINT64_MAX; }

static void RunDueCmds(void)
{
    while (s_cursor < s_ncmd && s_cmds[s_cursor].tMs <= SimNowMs()) {
        int i = s_cursor++;   /* advance first: a command may reset the chip */
        RunCmd(s_cmds[i].line);
    }
}

/* highest priority: script commands + watchdog models */
static void SimTask(void *pArg)
{
    (void)pArg;
    for (;;) {
        RunDueCmds();
        SimPeriphCheck();
        U64 next = NextCmdMs() == UINT64_MAX ? UINT64_MAX : NextCmdMs() * 1000u;
        U64 d = SimPeriphNextDeadlineUs();
        if (d < next) next = d;
        U64 now = SimNowUs();
        U64 wait = next == UINT64_MAX ? 3600000000ull : (next > now ? next - now : 0);
        if (wait < 1000) wait = 1000;
        if (wait > 100000) wait = 100000;   /* re-evaluate watchdog deadlines at least every 100 ms */
        vTaskDelay((TickType_t)(wait / 1000));
    }
}

void SimStartServiceTasks(void) { xTaskCreate(SimTask, "sim", 0, NULL, 24, NULL); }

bool SimFaultTaskCreate(const char *pName)
{
    char k[40];
    snprintf(k, sizeof k, "fault.task.%s", pName);
    return SimKvGet(k, 0) != 0;
}

/* ---------------- state ---------------- */
static void SaveState(const char *pReason)
{
    FILE *pF = fopen(s_pStatePath, "w");
    if (!pF) { perror("state"); _exit(kSimExitAbort); }
    fprintf(pF, "now_us %llu\ncursor %d\nreset %s\n", (unsigned long long)SimNowUs(), s_cursor, pReason);
    for (int i = 0; i < s_nkv; i++) fprintf(pF, "kv %s %lld\n", s_kv[i].k, (long long)s_kv[i].v);
    SimNvsSave(pF);
    fclose(pF);
}

static char s_resetReason[32] = "POWERON";

/* What the C6 reports after each kind of sim reset. The STWD100 WDO pulls CHIP_EN,
 * which the chip cannot tell from a power-on. */
esp_reset_reason_t esp_reset_reason(void)
{
    static const struct { const char *pN; esp_reset_reason_t r; } kM[] = {
        {"POWERON", ESP_RST_POWERON}, {"STWD100", ESP_RST_POWERON}, {"SW", ESP_RST_SW},
        {"OTA_ROLLBACK", ESP_RST_SW}, {"PANIC", ESP_RST_PANIC}, {"TASK_WDT", ESP_RST_TASK_WDT},
    };
    for (unsigned i = 0; i < sizeof kM / sizeof kM[0]; i++) if (!strcmp(kM[i].pN, s_resetReason)) return kM[i].r;
    return ESP_RST_UNKNOWN;
}

static void LoadState(void)
{
    FILE *pF = fopen(s_pStatePath, "r");
    if (!pF) return;
    char line[2200];
    while (fgets(line, sizeof line, pF)) {
        line[strcspn(line, "\n")] = 0;
        unsigned long long u; int c; char k[64]; long long v;
        if (sscanf(line, "now_us %llu", &u) == 1) SimSetNowUs(u + 50000);   /* reset + ROM boot: 50 ms, outputs low (pull-downs) */
        else if (sscanf(line, "cursor %d", &c) == 1) s_cursor = c;
        else if (sscanf(line, "reset %31s", s_resetReason) == 1) {}
        else if (sscanf(line, "kv %63s %lld", k, &v) == 2) SimKvSet(k, v);
        else SimNvsLoadLine(line);
    }
    fclose(pF);
}

void SimFinish(int code)
{
    SaveState(code == kSimExitDone ? "END" : "ABORT");
    SimEmit("EXIT %d", code);
    fflush(stdout);
    _exit(code);
}

void SimReset(const char *pReason)
{
    SimEmit("RESET %s", pReason);
    SaveState(pReason);
    fflush(stdout);
    _exit(kSimExitReset);
}

void SimAbort(const char *pWhy, const char *pFile, int line)
{
    SimEmit("PANIC %s at %s:%d", pWhy, pFile, line);
    SimReset("PANIC");
}

static void Boot(void)
{
    RunDueCmds();          /* everything scheduled up to now: pre-boot fault setup */
    app_main();
}

int main(int argc, char **pArgv)
{
    U64 until = 0;
    for (int i = 1; i + 1 < argc; i += 2) {
        if (!strcmp(pArgv[i], "--state")) s_pStatePath = pArgv[i + 1];
        else if (!strcmp(pArgv[i], "--script")) s_pScriptPath = pArgv[i + 1];
        else if (!strcmp(pArgv[i], "--until")) until = strtoull(pArgv[i + 1], NULL, 10);
        else if (!strcmp(pArgv[i], "--seed")) s_seed = strtoull(pArgv[i + 1], NULL, 10);
        else if (!strcmp(pArgv[i], "--out")) s_pOut = pArgv[i + 1];
    }
    if (!s_pStatePath || !s_pScriptPath || !until) {
        fprintf(stderr, "usage: tstat_sim --state FILE --script FILE --until MS [--seed N] [--out DIR]\n");
        return 2;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    LoadState();
    LoadScript();
    I64 boots = SimKvGet("sim.boots", 0) + 1;
    SimKvSet("sim.boots", boots);
    s_rng = s_seed * 0x9E3779B97F4A7C15ull + (U64)boots * 0xBF58476D1CE4E5B9ull + 1;
    SimSetEndUs(until * 1000ull);
    if (SimNowMs() >= until) { SimEmit("EXIT 0"); return 0; }
    SimEmit("BOOT n=%lld reason=%s", (long long)boots, s_resetReason);
    SimPeriphBoot();
    SimKernelRun(Boot);
}
