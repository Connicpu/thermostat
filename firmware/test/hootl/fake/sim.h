/* sim.h - internal interface between the HOOTL fakes. Not seen by firmware code. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

enum { SIM_EXIT_DONE = 0, SIM_EXIT_RESET = 3, SIM_EXIT_DEADLOCK = 4, SIM_EXIT_ABORT = 5 };

/* kernel.c */
uint64_t sim_now_us(void);
uint64_t sim_now_ms(void);
void     sim_set_now_us(uint64_t t);
void     sim_set_end_us(uint64_t t);
void     sim_kernel_run(void (*first)(void)) __attribute__((noreturn));
const char *sim_cur_task_name(void);
bool     sim_mutex_held(void *mutex);

/* sim_main.c */
void sim_log(char level, const char *tag, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void sim_emit(const char *fmt, ...) __attribute__((format(printf, 1, 2)));   /* "@<ms> " + text */
void sim_finish(int code) __attribute__((noreturn));      /* save state, exit */
void sim_reset(const char *reason) __attribute__((noreturn));   /* chip reset: save state, exit SIM_EXIT_RESET */
void sim_start_service_tasks(void);
bool sim_fault_task_create(const char *name);
uint32_t sim_rand(void);                                   /* seeded xorshift */
const char *sim_outdir(void);

/* persistent key/value store for sticky fake configuration (survives sim reboots) */
int64_t sim_kv_get(const char *k, int64_t def);
void    sim_kv_set(const char *k, int64_t v);

/* periph.c */
void sim_periph_boot(void);                               /* power-on state of all fake hardware */
bool sim_periph_cmd(char **argv, int argc);               /* script command; false = unknown */
uint64_t sim_periph_next_deadline_us(void);               /* watchdogs */
void sim_periph_check(void);
void sim_nvs_save(FILE *f);
bool sim_nvs_load_line(const char *line);

/* esp_zb_fake.c */
bool sim_zb_cmd(char **argv, int argc);
