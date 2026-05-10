/* =============================================================
 * PATTERN 09 — CONCURRENCY (Priority Cooperative Scheduler)
 * =============================================================
 * CONCEPT
 * -------
 * In bare-metal embedded systems, concurrency is often achieved
 * without an RTOS through a cooperative scheduler: tasks are
 * assigned periods and priorities; each tick the scheduler picks
 * the highest-priority overdue task and runs it.  Tasks must be
 * non-blocking and short-lived.
 *
 * WHY IT MATTERS IN EMBEDDED SYSTEMS
 * ------------------------------------
 * Many embedded systems (sensor nodes, motor controllers) do not
 * need a full RTOS.  A simple scheduler gives deterministic,
 * priority-aware task dispatch with zero RTOS overhead.
 *
 * EXAMPLE SCENARIO
 * ----------------
 * An industrial IoT sensor node runs five tasks:
 *   Priority 0 (highest): Read emergency stop button (10 ms)
 *   Priority 1: Sample ADC (50 ms)
 *   Priority 2: PID control loop (100 ms)
 *   Priority 3: Update LCD display (500 ms)
 *   Priority 4 (lowest): Transmit to gateway (2000 ms)
 *
 * KEY MECHANICS
 * -------------
 *  Task { fn, period, deadline, priority, run_count }
 *  sched_tick(ms)  — advances all deadlines, runs overdue tasks in
 *                    priority order (lower number = higher priority)
 * ============================================================= */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* ── Task descriptor ───────────────────────────────────────── */
typedef void (*TaskFn)(uint32_t elapsed_ms);

typedef struct {
    TaskFn      fn;
    const char *name;
    uint32_t    period_ms;
    uint32_t    deadline_ms;   /* absolute tick when next run is due */
    uint8_t     priority;      /* 0 = highest */
    uint32_t    run_count;
    uint32_t    overrun_count; /* times task was late */
    uint32_t    last_run_ms;
} Task;

/* ── Scheduler ─────────────────────────────────────────────── */
#define MAX_TASKS 10
static Task     tasks[MAX_TASKS];
static int      task_count  = 0;
static uint32_t elapsed_ms  = 0;

void sched_add(const char *name, uint8_t priority,
               uint32_t period_ms, TaskFn fn)
{
    if (task_count >= MAX_TASKS) return;
    Task *t         = &tasks[task_count++];
    t->fn           = fn;
    t->name         = name;
    t->period_ms    = period_ms;
    t->deadline_ms  = period_ms;   /* first deadline = one period from now */
    t->priority     = priority;
    t->run_count    = 0;
    t->overrun_count = 0;
    t->last_run_ms  = 0;
    printf("  Registered [P%u] %-18s every %4u ms\n",
           priority, name, period_ms);
}

/* Run one scheduler tick advancing by ms_step milliseconds */
void sched_tick(uint32_t ms_step)
{
    elapsed_ms += ms_step;

    /*
     * Find the overdue task with the highest priority (lowest number).
     * This is a simple O(n) scan — fine for ≤16 tasks.
     */
    bool ran_any = true;
    while (ran_any) {
        ran_any = false;
        int best = -1;
        for (int i = 0; i < task_count; i++) {
            Task *t = &tasks[i];
            if (elapsed_ms >= t->deadline_ms) {
                if (best < 0 || t->priority < tasks[best].priority)
                    best = i;
            }
        }
        if (best >= 0) {
            Task *t = &tasks[best];
            if (elapsed_ms > t->deadline_ms + t->period_ms)
                t->overrun_count++;
            t->run_count++;
            t->last_run_ms = elapsed_ms;
            t->deadline_ms += t->period_ms;
            t->fn(elapsed_ms);
            ran_any = true;   /* check again — another may now be due */
        }
    }
}

/* ── Tasks ─────────────────────────────────────────────────── */
static void task_estop(uint32_t ms)
{
    /* Priority 0: safety-critical — check e-stop button */
    static int press = 0;
    press = (ms / 100) % 2;   /* simulate: every 200 ms it toggles */
    printf("    [%5u ms][P0] E-STOP check → %s\n",
           ms, press ? "PRESSED!" : "clear");
}

static void task_adc(uint32_t ms)
{
    uint32_t fake_mv = 1800 + (ms % 400);
    printf("    [%5u ms][P1] ADC sample   → %u mV\n", ms, fake_mv);
}

static void task_pid(uint32_t ms)
{
    float setpoint = 100.0f;
    float measured = 95.0f + (float)(ms % 10);
    float error    = setpoint - measured;
    printf("    [%5u ms][P2] PID loop      → err=%.1f, output=%.1f%%\n",
           ms, error, 50.0f + error * 2.0f);
}

static void task_lcd(uint32_t ms)
{
    printf("    [%5u ms][P3] LCD update    → frame #%u\n",
           ms, ms / 500);
}

static void task_tx(uint32_t ms)
{
    static uint32_t seq = 0;
    printf("    [%5u ms][P4] Gateway TX    → seq=%u, payload=16 bytes\n",
           ms, seq++);
}

/* ── Scheduler statistics ──────────────────────────────────── */
static void sched_report(void)
{
    printf("\n  ┌─────────────────────────────────────────────────────┐\n");
    printf("  │ Task               Pri  Period  Runs  Overruns      │\n");
    printf("  ├─────────────────────────────────────────────────────┤\n");
    for (int i = 0; i < task_count; i++) {
        Task *t = &tasks[i];
        printf("  │ %-18s  %u   %5u ms  %4u  %4u            │\n",
               t->name, t->priority, t->period_ms,
               t->run_count, t->overrun_count);
    }
    printf("  └─────────────────────────────────────────────────────┘\n");
}

int main(void)
{
    printf("=== CONCURRENCY — Priority Cooperative Scheduler ===\n\n");

    printf("  -- Registering tasks --\n");
    sched_add("EStopCheck",   0,   10, task_estop);
    sched_add("AdcSample",    1,   50, task_adc);
    sched_add("PidLoop",      2,  100, task_pid);
    sched_add("LcdUpdate",    3,  500, task_lcd);
    sched_add("GatewayTX",    4, 2000, task_tx);

    printf("\n  -- Simulating 2000 ms (10 ms steps) --\n");
    for (int i = 0; i < 200; i++)
        sched_tick(10);

    sched_report();
    return 0;
}
