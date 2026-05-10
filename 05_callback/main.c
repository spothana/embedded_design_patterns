/* =============================================================
 * PATTERN 05 — CALLBACK
 * =============================================================
 * CONCEPT
 * -------
 * A Callback is a function pointer registered with a lower-level
 * module.  When an event occurs, the lower-level module "calls
 * back" the registered function without knowing what it does.
 * This inverts the dependency: the driver does not depend on the
 * application — it is the other way around.
 *
 * WHY IT MATTERS IN EMBEDDED SYSTEMS
 * ------------------------------------
 * Hardware events (timer overflow, ADC done, DMA complete, UART
 * byte received) need to notify application code without coupling
 * the driver layer to any particular application.  Callbacks are
 * the standard embedded C idiom for this — they are essentially
 * how every RTOS, HAL, and bare-metal ISR framework works.
 *
 * EXAMPLE SCENARIO
 * ----------------
 * A hardware timer module fires at configurable intervals.
 * Application modules register callbacks for specific periods:
 *   • LED blinker  — 500 ms
 *   • Watchdog pet — 800 ms
 *   • Data uplink  — 5000 ms
 * The timer has no idea what those functions do; it just fires them.
 *
 * KEY MECHANICS
 * -------------
 *  typedef void (*TimerCallback)(void *ctx);
 *  timer_register(period_ms, callback, context_ptr);
 *  timer_tick()  ← called from ISR or main loop
 * ============================================================= */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── Callback type — takes a user-supplied context pointer ─── */
typedef void (*TimerCallback)(void *ctx);

/* ── Timer module internals ────────────────────────────────── */
#define MAX_CALLBACKS 8

typedef struct {
    TimerCallback fn;
    void         *ctx;
    uint32_t      period_ms;
    uint32_t      remaining_ms;
    bool          active;
    char          label[24];
} TimerEntry;

static TimerEntry  timer_table[MAX_CALLBACKS];
static int         timer_count = 0;
static uint32_t    elapsed_ms  = 0;

void timer_module_init(void)
{
    memset(timer_table, 0, sizeof(timer_table));
    timer_count = 0;
    elapsed_ms  = 0;
}

int timer_register(const char *label, uint32_t period_ms,
                   TimerCallback fn, void *ctx)
{
    if (timer_count >= MAX_CALLBACKS || fn == NULL) return -1;
    TimerEntry *e = &timer_table[timer_count++];
    strncpy(e->label, label, sizeof(e->label)-1);
    e->period_ms    = period_ms;
    e->remaining_ms = period_ms;
    e->fn           = fn;
    e->ctx          = ctx;
    e->active       = true;
    printf("  [Timer] registered %-16s every %4u ms\n", label, period_ms);
    return 0;
}

/* Called every 1 ms from a SysTick ISR (simulated here) */
void timer_tick_ms(uint32_t ms_step)
{
    elapsed_ms += ms_step;
    for (int i = 0; i < timer_count; i++) {
        TimerEntry *e = &timer_table[i];
        if (!e->active) continue;
        if (e->remaining_ms <= ms_step) {
            e->remaining_ms = e->period_ms;
            e->fn(e->ctx);          /* ← the callback fires here */
        } else {
            e->remaining_ms -= ms_step;
        }
    }
}

/* ── Application callbacks ─────────────────────────────────── */

/* LED blinker — context is a toggle state */
static void cb_led_blink(void *ctx)
{
    bool *on = (bool *)ctx;
    *on = !(*on);
    printf("    [t=%5u ms] LED blink → %s\n", elapsed_ms, *on ? "ON " : "OFF");
}

/* Watchdog — no context needed */
static void cb_watchdog_pet(void *ctx)
{
    (void)ctx;
    printf("    [t=%5u ms] Watchdog petted\n", elapsed_ms);
}

/* Data uplink — context is a packet counter */
static void cb_data_uplink(void *ctx)
{
    uint32_t *pkt = (uint32_t *)ctx;
    (*pkt)++;
    printf("    [t=%5u ms] Data uplink — packet #%u sent\n",
           elapsed_ms, *pkt);
}

/* Voltage check — context is a threshold (mV) */
static void cb_voltage_check(void *ctx)
{
    uint32_t threshold_mv = *(uint32_t *)ctx;
    uint32_t fake_mv = 3600 - (elapsed_ms / 20); /* simulate draining battery */
    const char *status = fake_mv >= threshold_mv ? "OK" : "LOW!";
    printf("    [t=%5u ms] Voltage check: %u mV [%s]\n",
           elapsed_ms, fake_mv, status);
}

int main(void)
{
    printf("=== CALLBACK PATTERN — Timer Event Dispatcher ===\n\n");

    timer_module_init();

    /* Application context variables */
    bool     led_state    = false;
    uint32_t pkt_count    = 0;
    uint32_t vth_mv       = 3500;

    /* Register callbacks — timer knows nothing about what they do */
    printf("  -- Registering callbacks --\n");
    timer_register("LED_Blink",     500,  cb_led_blink,      &led_state);
    timer_register("WDT_Pet",       800,  cb_watchdog_pet,   NULL);
    timer_register("DataUplink",   2000,  cb_data_uplink,    &pkt_count);
    timer_register("VoltageCheck", 1500,  cb_voltage_check,  &vth_mv);

    /* Simulate 5 seconds of SysTick */
    printf("\n  -- Simulating 5000 ms (advancing in 500 ms steps) --\n");
    for (int i = 0; i < 10; i++)
        timer_tick_ms(500);

    printf("\n  -- Final state --\n");
    printf("  LED: %s | Packets sent: %u\n",
           led_state ? "ON" : "OFF", pkt_count);

    return 0;
}
