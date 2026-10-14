/* sim.h - internal interface between the HOOTL fakes. Not seen by firmware code. */
#pragma once
#include "core.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

enum
{
    kSimExitDone = 0,
    kSimExitReset = 3,
    kSimExitDeadlock = 4,
    kSimExitAbort = 5
};

/* kernel.c */
/** Virtual time in us. Only advances when every task is blocked (kernel.c). */
U64 SimNowUs(void);

/** Virtual time in ms. */
U64 SimNowMs(void);

/** Set virtual time (us); used when restoring state at boot. */
void SimSetNowUs(U64 t);

/** Set the end of the run (us); the kernel exits with kSimExitDone when time reaches it. */
void SimSetEndUs(U64 t);

/**
 * Start the deterministic scheduler: creates the "main" task (priority 1) running first(), and the
 * sim service task, then hands over control. Never returns; the process ends through SimFinish()/SimReset().
 */
void SimKernelRun(void (*first)(void)) __attribute__((noreturn));

/** Name of the running task ("-" before the kernel starts). */
const char *SimCurTaskName(void);

/** Whether the mutex created by xSemaphoreCreateMutex() is currently held by any task. */
bool SimMutexHeld(void *pMutex);

/* sim_main.c */
/** Log a firmware/sim message as `@<ms> <level> <tag>: <text>`. */
void SimLog(char level, const char *pTag, const char *pFmt, ...) __attribute__((format(printf, 3, 4)));

/** Emit one event line to stdout, prefixed `@<virtual ms> `. */
void SimEmit(const char *pFmt, ...) __attribute__((format(printf, 1, 2))); /* "@<ms> " + text */
/**
 * End the run: save the state file (reason END for kSimExitDone, else ABORT), emit `EXIT <code>`
 * and exit with `code`.
 */
void SimFinish(int code) __attribute__((noreturn)); /* save state, exit */
/**
 * Chip reset: emit `RESET <reason>`, save state with that reason and exit kSimExitReset; the runner boots again.
 */
void SimReset(const char *pReason) __attribute__((noreturn)); /* chip reset: save state, exit kSimExitReset */
/** Create the sim service task (script commands and watchdog models, priority 24). */
void SimStartServiceTasks(void);

/** Fault injection for xTaskCreate(): true if kv fault.task.<name> is set. */
bool SimFaultTaskCreate(const char *pName);

/** Deterministic xorshift PRNG, seeded from --seed and the boot number. */
U32 SimRand(void); /* seeded xorshift */
/** Output directory for artifacts (framebuffer PBMs), from --out (default "."). */
const char *SimOutdir(void);

/* persistent key/value store for sticky fake configuration (survives sim reboots) */
/** Value of sticky kv key pK, or `def` when unset. */
I64 SimKvGet(const char *pK, I64 def);

/** Set sticky kv key pK (at most 256 keys; silently dropped when full). */
void SimKvSet(const char *pK, I64 v);

/* periph.c */
/** Power-on state of all fake hardware, including the bootloader's OTA slot choice. */
void SimPeriphBoot(void); /* power-on state of all fake hardware */
/** Run a hardware script command. Returns false for an unknown command. */
bool SimPeriphCmd(char **pArgv, int argc); /* script command; false = unknown */
/** Next virtual time (us) at which a watchdog model could fire; UINT64_MAX if none is armed. */
U64 SimPeriphNextDeadlineUs(void); /* watchdogs */
/** Reset the chip if a watchdog model expired (STWD100 or task watchdog). */
void SimPeriphCheck(void);

/** Append the fake NVS contents to the state file. */
void SimNvsSave(FILE *pF);

/** Restore one NVS line from the state file. Returns false if the line is not an NVS entry. */
bool SimNvsLoadLine(const char *pLine);

/* esp_zb_fake.c */
/** Run a `zb ...` script command (without the "zb" word). Returns false for an unknown command. */
bool SimZbCmd(char **pArgv, int argc);
