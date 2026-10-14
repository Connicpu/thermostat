/* kernel.c - deterministic virtual-time FreeRTOS subset for the HOOTL simulation.
 *
 * Every task is a pthread, but only the task holding the baton runs. A task
 * gives up the baton only when it blocks (delay, queue/semaphore wait, notify
 * wait) or yields to a higher-priority task it just woke. When no task is
 * ready, virtual time jumps to the earliest wake-up. Same inputs + seed =>
 * identical run, independent of host speed. */
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

struct sim_task {
    pthread_t th;
    pthread_cond_t cv;
    TaskFunction_t fn;
    void *arg;
    char name[16];
    int prio, id;
    bool used, dead, ready;
    uint64_t wake_at;          /* us; NEVER = no timeout */
    void *wait_obj;            /* queue/semaphore being waited on */
    uint32_t notify;
    bool notify_wait;
};

struct sim_queue {
    size_t item, len, count, head;
    uint8_t *buf;
    bool is_mutex;
    TaskHandle_t owner;
};

static pthread_mutex_t s_big = PTHREAD_MUTEX_INITIALIZER;
static struct sim_task s_tasks[MAX_TASKS];
static struct sim_task *s_cur;
static uint64_t s_now_us;
static uint64_t s_end_us = NEVER;
static int s_next_id;

uint64_t sim_now_us(void) { return s_now_us; }
uint64_t sim_now_ms(void) { return s_now_us / 1000; }
void sim_set_now_us(uint64_t t) { s_now_us = t; }
void sim_set_end_us(uint64_t t) { s_end_us = t; }
const char *sim_cur_task_name(void) { return s_cur ? s_cur->name : "-"; }

static struct sim_task *pick(void)
{
    for (;;) {
        /* highest priority ready task; round-robin among equals starting after the current one */
        struct sim_task *best = NULL;
        int start = s_cur ? s_cur->id + 1 : 0;
        for (int k = 0; k < MAX_TASKS; k++) {
            struct sim_task *t = &s_tasks[(start + k) % MAX_TASKS];
            if (!t->used || t->dead || !t->ready) continue;
            if (!best || t->prio > best->prio) best = t;
        }
        if (best) return best;
        uint64_t next = NEVER;
        for (int i = 0; i < MAX_TASKS; i++) {
            struct sim_task *t = &s_tasks[i];
            if (t->used && !t->dead && t->wake_at < next) next = t->wake_at;
        }
        if (next == NEVER) { sim_log('E', "sim", "deadlock: every task blocked forever"); sim_finish(SIM_EXIT_DEADLOCK); }
        if (next >= s_end_us) { s_now_us = s_end_us; sim_finish(SIM_EXIT_DONE); }
        s_now_us = next;
        for (int i = 0; i < MAX_TASKS; i++) {
            struct sim_task *t = &s_tasks[i];
            if (t->used && !t->dead && t->wake_at <= s_now_us) { t->ready = true; t->wake_at = NEVER; t->wait_obj = NULL; t->notify_wait = false; }
        }
    }
}

/* hand the baton to the next task and wait until it comes back to `self` */
static void switch_away(struct sim_task *self)
{
    struct sim_task *n = pick();
    if (n == self) return;
    s_cur = n;
    pthread_cond_signal(&n->cv);
    if (self->dead) { pthread_mutex_unlock(&s_big); pthread_exit(NULL); }
    while (s_cur != self) pthread_cond_wait(&self->cv, &s_big);
}

static void block(uint64_t timeout_us, void *obj)
{
    struct sim_task *self = s_cur;
    self->ready = false;
    self->wait_obj = obj;
    self->wake_at = timeout_us == NEVER ? NEVER : s_now_us + timeout_us;
    switch_away(self);
}

static void wake(struct sim_task *t)
{
    t->ready = true;
    t->wake_at = NEVER;
    t->wait_obj = NULL;
    t->notify_wait = false;
}

static void maybe_preempt(struct sim_task *woken)
{
    if (woken && s_cur && s_cur->used && woken->prio > s_cur->prio) switch_away(s_cur);   /* we stay ready */
}

static void *trampoline(void *p)
{
    struct sim_task *t = p;
    pthread_mutex_lock(&s_big);
    while (s_cur != t) pthread_cond_wait(&t->cv, &s_big);
    t->fn(t->arg);
    vTaskDelete(NULL);
    return NULL;
}

BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack, void *arg, UBaseType_t prio, TaskHandle_t *out)
{
    (void)stack;
    struct sim_task *t = NULL;
    for (int i = 0; i < MAX_TASKS; i++) if (!s_tasks[i].used) { t = &s_tasks[i]; t->id = i; break; }
    if (!t) return pdFAIL;
    if (sim_fault_task_create(name)) return pdFAIL;
    t->used = true; t->dead = false; t->ready = true; t->wake_at = NEVER;
    t->fn = fn; t->arg = arg; t->prio = (int)prio;
    snprintf(t->name, sizeof t->name, "%s", name);
    pthread_cond_init(&t->cv, NULL);
    s_next_id++;
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 1 << 20);
    pthread_create(&t->th, &a, trampoline, t);
    pthread_detach(t->th);
    if (out) *out = t;
    maybe_preempt(t);
    return pdPASS;
}

void vTaskDelete(TaskHandle_t h)
{
    struct sim_task *t = h ? h : s_cur;
    t->dead = true;
    t->ready = false;
    if (t == s_cur) switch_away(t);   /* does not return for the current task */
}

void vTaskDelay(TickType_t ticks)
{
    block((uint64_t)ticks * 1000u, NULL);
}

void vTaskDelayUntil(TickType_t *prev, TickType_t inc)
{
    uint64_t target = ((uint64_t)*prev + inc) * 1000u;
    *prev += inc;
    if (target > s_now_us) block(target - s_now_us, NULL);
    else block(0, NULL);   /* still let others run */
}

TickType_t xTaskGetTickCount(void) { return (TickType_t)(s_now_us / 1000u); }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return s_cur; }

uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t wait)
{
    struct sim_task *self = s_cur;
    if (!self->notify && wait) {
        self->notify_wait = true;
        block(wait == portMAX_DELAY ? NEVER : (uint64_t)wait * 1000u, NULL);
    }
    uint32_t v = self->notify;
    if (clear) self->notify = 0; else if (v) self->notify--;
    return v;
}

BaseType_t xTaskNotifyGive(TaskHandle_t t)
{
    t->notify++;
    if (t->notify_wait && !t->dead) { wake(t); maybe_preempt(t); }
    return pdPASS;
}

/* ---------------- queues / mutexes ---------------- */

QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t item)
{
    struct sim_queue *q = calloc(1, sizeof *q);
    q->item = item; q->len = len;
    q->buf = calloc(len ? len : 1, item ? item : 1);
    return q;
}

static void wake_waiters(void *obj)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        struct sim_task *t = &s_tasks[i];
        if (t->used && !t->dead && !t->ready && t->wait_obj == obj) { wake(t); maybe_preempt(t); return; }
    }
}

BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t wait)
{
    uint64_t deadline = wait == portMAX_DELAY ? NEVER : s_now_us + (uint64_t)wait * 1000u;
    while (q->count == q->len) {
        if (s_now_us >= deadline) return pdFAIL;
        block(deadline == NEVER ? NEVER : deadline - s_now_us, q);
    }
    memcpy(q->buf + ((q->head + q->count) % q->len) * q->item, item, q->item);
    q->count++;
    wake_waiters(q);
    return pdPASS;
}

BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t wait)
{
    uint64_t deadline = wait == portMAX_DELAY ? NEVER : s_now_us + (uint64_t)wait * 1000u;
    while (q->count == 0) {
        if (s_now_us >= deadline) return pdFAIL;
        block(deadline == NEVER ? NEVER : deadline - s_now_us, q);
    }
    memcpy(item, q->buf + q->head * q->item, q->item);
    q->head = (q->head + 1) % q->len;
    q->count--;
    wake_waiters(q);
    return pdPASS;
}

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    struct sim_queue *q = xQueueCreate(1, 0);
    q->is_mutex = true;
    return q;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait)
{
    uint64_t deadline = wait == portMAX_DELAY ? NEVER : s_now_us + (uint64_t)wait * 1000u;
    while (s->owner) {
        if (s_now_us >= deadline) return pdFAIL;
        block(deadline == NEVER ? NEVER : deadline - s_now_us, s);
    }
    s->owner = s_cur;
    return pdPASS;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t s)
{
    s->owner = NULL;
    wake_waiters(s);
    return pdPASS;
}

bool sim_mutex_held(void *m) { return m && ((struct sim_queue *)m)->owner != NULL; }

/* ---------------- start ---------------- */

static void main_task(void *arg) { ((void (*)(void))arg)(); }

void sim_kernel_run(void (*first)(void))
{
    pthread_mutex_lock(&s_big);
    struct sim_task boot = {.used = false};
    s_cur = &boot;   /* placeholder: hand the baton to the real tasks */
    pthread_cond_init(&boot.cv, NULL);
    xTaskCreate(main_task, "main", 0, (void *)first, 1, NULL);
    sim_start_service_tasks();
    struct sim_task *n = pick();
    s_cur = n;
    pthread_cond_signal(&n->cv);
    for (;;) pthread_cond_wait(&boot.cv, &s_big);   /* never handed back; process ends via sim_finish() */
}
