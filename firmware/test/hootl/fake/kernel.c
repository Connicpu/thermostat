/* kernel.c - deterministic virtual-time FreeRTOS subset for the HOOTL simulation.
 *
 * Every task is a pthread, but only the task holding the baton runs. A task
 * gives up the baton only when it blocks (delay, queue/semaphore wait, notify
 * wait) or yields to a higher-priority task it just woke. When no task is
 * ready, virtual time jumps to the earliest wake-up. Same inputs + seed =>
 * identical run, independent of host speed. */
#include "core.h"
#include "sim.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_TASKS 32
#define NEVER UINT64_MAX

struct SimTask {
    pthread_t th;
    pthread_cond_t cv;
    TaskFunction_t fn;
    void *pArg;
    char name[16];
    int prio, id;
    bool used, dead, ready;
    U64 wakeAt;          /* us; NEVER = no timeout */
    void *pWaitObj;            /* queue/semaphore being waited on */
    U32 notify;
    bool notifyWait;
};

struct SimQueue {
    size_t item, len, count, head;
    U8 *pBuf;
    bool isMutex;
    TaskHandle_t owner;
};

static pthread_mutex_t s_big = PTHREAD_MUTEX_INITIALIZER;
static struct SimTask s_tasks[MAX_TASKS];
static struct SimTask *s_pCur;
static U64 s_nowUs;
static U64 s_endUs = NEVER;
static int s_nextId;

U64 SimNowUs(void) { return s_nowUs; }
U64 SimNowMs(void) { return s_nowUs / 1000; }
void SimSetNowUs(U64 t) { s_nowUs = t; }
void SimSetEndUs(U64 t) { s_endUs = t; }
const char *SimCurTaskName(void) { return s_pCur ? s_pCur->name : "-"; }

static struct SimTask *Pick(void)
{
    for (;;) {
        /* highest priority ready task; round-robin among equals starting after the current one */
        struct SimTask *pBest = NULL;
        int start = s_pCur ? s_pCur->id + 1 : 0;
        for (int k = 0; k < MAX_TASKS; k++) {
            struct SimTask *pT = &s_tasks[(start + k) % MAX_TASKS];
            if (!pT->used || pT->dead || !pT->ready) continue;
            if (!pBest || pT->prio > pBest->prio) pBest = pT;
        }
        if (pBest) return pBest;
        U64 next = NEVER;
        for (int i = 0; i < MAX_TASKS; i++) {
            struct SimTask *pT = &s_tasks[i];
            if (pT->used && !pT->dead && pT->wakeAt < next) next = pT->wakeAt;
        }
        if (next == NEVER) { SimLog('E', "sim", "deadlock: every task blocked forever"); SimFinish(kSimExitDeadlock); }
        if (next >= s_endUs) { s_nowUs = s_endUs; SimFinish(kSimExitDone); }
        s_nowUs = next;
        for (int i = 0; i < MAX_TASKS; i++) {
            struct SimTask *pT = &s_tasks[i];
            if (pT->used && !pT->dead && pT->wakeAt <= s_nowUs) { pT->ready = true; pT->wakeAt = NEVER; pT->pWaitObj = NULL; pT->notifyWait = false; }
        }
    }
}

/* hand the baton to the next task and wait until it comes back to `self` */
static void SwitchAway(struct SimTask *pSelf)
{
    struct SimTask *pN = Pick();
    if (pN == pSelf) return;
    s_pCur = pN;
    pthread_cond_signal(&pN->cv);
    if (pSelf->dead) { pthread_mutex_unlock(&s_big); pthread_exit(NULL); }
    while (s_pCur != pSelf) pthread_cond_wait(&pSelf->cv, &s_big);
}

static void Block(U64 timeoutUs, void *pObj)
{
    struct SimTask *pSelf = s_pCur;
    pSelf->ready = false;
    pSelf->pWaitObj = pObj;
    pSelf->wakeAt = timeoutUs == NEVER ? NEVER : s_nowUs + timeoutUs;
    SwitchAway(pSelf);
}

static void Wake(struct SimTask *pT)
{
    pT->ready = true;
    pT->wakeAt = NEVER;
    pT->pWaitObj = NULL;
    pT->notifyWait = false;
}

static void MaybePreempt(struct SimTask *pWoken)
{
    if (pWoken && s_pCur && s_pCur->used && pWoken->prio > s_pCur->prio) SwitchAway(s_pCur);   /* we stay ready */
}

static void *Trampoline(void *pP)
{
    struct SimTask *pT = pP;
    pthread_mutex_lock(&s_big);
    while (s_pCur != pT) pthread_cond_wait(&pT->cv, &s_big);
    pT->fn(pT->pArg);
    vTaskDelete(NULL);
    return NULL;
}

BaseType_t xTaskCreate(TaskFunction_t fn, const char *pName, U32 stack, void *pArg, UBaseType_t prio, TaskHandle_t *pOut)
{
    (void)stack;
    struct SimTask *pT = NULL;
    for (int i = 0; i < MAX_TASKS; i++) if (!s_tasks[i].used) { pT = &s_tasks[i]; pT->id = i; break; }
    if (!pT) return pdFAIL;
    if (SimFaultTaskCreate(pName)) return pdFAIL;
    pT->used = true; pT->dead = false; pT->ready = true; pT->wakeAt = NEVER;
    pT->fn = fn; pT->pArg = pArg; pT->prio = (int)prio;
    snprintf(pT->name, sizeof pT->name, "%s", pName);
    pthread_cond_init(&pT->cv, NULL);
    s_nextId++;
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 1 << 20);
    pthread_create(&pT->th, &a, Trampoline, pT);
    pthread_detach(pT->th);
    if (pOut) *pOut = pT;
    MaybePreempt(pT);
    return pdPASS;
}

void vTaskDelete(TaskHandle_t h)
{
    struct SimTask *pT = h ? h : s_pCur;
    pT->dead = true;
    pT->ready = false;
    if (pT == s_pCur) SwitchAway(pT);   /* does not return for the current task */
}

void vTaskDelay(TickType_t ticks)
{
    Block((U64)ticks * 1000u, NULL);
}

void vTaskDelayUntil(TickType_t *pPrev, TickType_t inc)
{
    U64 target = ((U64)*pPrev + inc) * 1000u;
    *pPrev += inc;
    if (target > s_nowUs) Block(target - s_nowUs, NULL);
    else Block(0, NULL);   /* still let others run */
}

TickType_t xTaskGetTickCount(void) { return (TickType_t)(s_nowUs / 1000u); }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return s_pCur; }

U32 ulTaskNotifyTake(BaseType_t clear, TickType_t wait)
{
    struct SimTask *pSelf = s_pCur;
    if (!pSelf->notify && wait) {
        pSelf->notifyWait = true;
        Block(wait == portMAX_DELAY ? NEVER : (U64)wait * 1000u, NULL);
    }
    U32 v = pSelf->notify;
    if (clear) pSelf->notify = 0; else if (v) pSelf->notify--;
    return v;
}

BaseType_t xTaskNotifyGive(TaskHandle_t t)
{
    t->notify++;
    if (t->notifyWait && !t->dead) { Wake(t); MaybePreempt(t); }
    return pdPASS;
}

/* ---------------- queues / mutexes ---------------- */

QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t item)
{
    struct SimQueue *pQ = calloc(1, sizeof *pQ);
    pQ->item = item; pQ->len = len;
    pQ->pBuf = calloc(len ? len : 1, item ? item : 1);
    return pQ;
}

static void WakeWaiters(void *pObj)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        struct SimTask *pT = &s_tasks[i];
        if (pT->used && !pT->dead && !pT->ready && pT->pWaitObj == pObj) { Wake(pT); MaybePreempt(pT); return; }
    }
}

BaseType_t xQueueSend(QueueHandle_t q, const void *pItem, TickType_t wait)
{
    U64 deadline = wait == portMAX_DELAY ? NEVER : s_nowUs + (U64)wait * 1000u;
    while (q->count == q->len) {
        if (s_nowUs >= deadline) return pdFAIL;
        Block(deadline == NEVER ? NEVER : deadline - s_nowUs, q);
    }
    memcpy(q->pBuf + ((q->head + q->count) % q->len) * q->item, pItem, q->item);
    q->count++;
    WakeWaiters(q);
    return pdPASS;
}

BaseType_t xQueueReceive(QueueHandle_t q, void *pItem, TickType_t wait)
{
    U64 deadline = wait == portMAX_DELAY ? NEVER : s_nowUs + (U64)wait * 1000u;
    while (q->count == 0) {
        if (s_nowUs >= deadline) return pdFAIL;
        Block(deadline == NEVER ? NEVER : deadline - s_nowUs, q);
    }
    memcpy(pItem, q->pBuf + q->head * q->item, q->item);
    q->head = (q->head + 1) % q->len;
    q->count--;
    WakeWaiters(q);
    return pdPASS;
}

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    struct SimQueue *pQ = xQueueCreate(1, 0);
    pQ->isMutex = true;
    return pQ;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait)
{
    U64 deadline = wait == portMAX_DELAY ? NEVER : s_nowUs + (U64)wait * 1000u;
    while (s->owner) {
        if (s_nowUs >= deadline) return pdFAIL;
        Block(deadline == NEVER ? NEVER : deadline - s_nowUs, s);
    }
    s->owner = s_pCur;
    return pdPASS;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t s)
{
    s->owner = NULL;
    WakeWaiters(s);
    return pdPASS;
}

bool SimMutexHeld(void *pM) { return pM && ((struct SimQueue *)pM)->owner != NULL; }

/* ---------------- start ---------------- */

static void MainTask(void *pArg) { ((void (*)(void))pArg)(); }

void SimKernelRun(void (*first)(void))
{
    pthread_mutex_lock(&s_big);
    struct SimTask boot = {.used = false};
    s_pCur = &boot;   /* placeholder: hand the baton to the real tasks */
    pthread_cond_init(&boot.cv, NULL);
    xTaskCreate(MainTask, "main", 0, (void *)first, 1, NULL);
    SimStartServiceTasks();
    struct SimTask *pN = Pick();
    s_pCur = pN;
    pthread_cond_signal(&pN->cv);
    for (;;) pthread_cond_wait(&boot.cv, &s_big);   /* never handed back; process ends via SimFinish() */
}
