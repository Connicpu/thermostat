/* sim.h - internal interface between the HOOTL fakes. Not seen by firmware code. */
#pragma once
#include "core.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

enum { kSimExitDone = 0, kSimExitReset = 3, kSimExitDeadlock = 4, kSimExitAbort = 5 };

/* kernel.c */
U64 SimNowUs(void);
U64 SimNowMs(void);
void     SimSetNowUs(U64 t);
void     SimSetEndUs(U64 t);
void     SimKernelRun(void (*first)(void)) __attribute__((noreturn));
const char *SimCurTaskName(void);
bool     SimMutexHeld(void *pMutex);

/* sim_main.c */
void SimLog(char level, const char *pTag, const char *pFmt, ...) __attribute__((format(printf, 3, 4)));
void SimEmit(const char *pFmt, ...) __attribute__((format(printf, 1, 2)));   /* "@<ms> " + text */
void SimFinish(int code) __attribute__((noreturn));      /* save state, exit */
void SimReset(const char *pReason) __attribute__((noreturn));   /* chip reset: save state, exit kSimExitReset */
void SimStartServiceTasks(void);
bool SimFaultTaskCreate(const char *pName);
U32 SimRand(void);                                   /* seeded xorshift */
const char *SimOutdir(void);

/* persistent key/value store for sticky fake configuration (survives sim reboots) */
I64 SimKvGet(const char *pK, I64 def);
void    SimKvSet(const char *pK, I64 v);

/* periph.c */
void SimPeriphBoot(void);                               /* power-on state of all fake hardware */
bool SimPeriphCmd(char **pArgv, int argc);               /* script command; false = unknown */
U64 SimPeriphNextDeadlineUs(void);               /* watchdogs */
void SimPeriphCheck(void);
void SimNvsSave(FILE *pF);
bool SimNvsLoadLine(const char *pLine);

/* esp_zb_fake.c */
bool SimZbCmd(char **pArgv, int argc);
