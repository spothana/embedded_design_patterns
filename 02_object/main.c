/* =============================================================
 * PATTERN 02 — OBJECT PATTERN
 * =============================================================
 * CONCEPT
 * -------
 * C has no classes.  The Object Pattern simulates encapsulation by
 * bundling data and the functions that operate on it inside a single
 * struct.  An "init" function acts as the constructor; methods are
 * function pointers stored in the struct.
 *
 * WHY IT MATTERS IN EMBEDDED SYSTEMS
 * ------------------------------------
 * A microcontroller often has multiple identical peripherals (four
 * PWM channels, three timers, two ADCs).  Without the Object Pattern
 * you end up with one global variable per channel and a spaghetti of
 * channel numbers passed to every function.  With it, each channel
 * is a self-contained object that carries its own state and methods.
 *
 * EXAMPLE SCENARIO
 * ----------------
 * A motor controller board has four independent PWM channels.
 * Each channel needs: frequency, duty cycle, enable/disable, and a
 * way to report its status.  We model each as a PwmChannel object.
 *
 * KEY MECHANICS
 * -------------
 *  • PwmChannel struct holds config + runtime state + fn pointers
 *  • pwm_init() wires the function pointers (the "constructor")
 *  • Methods receive `self` — the idiomatic C substitute for `this`
 *
 *  PwmChannel ch1, ch2;          // two independent objects
 *  pwm_init(&ch1, 0, 1000);      // channel 0, 1 kHz
 *  ch1.set_duty(&ch1, 75);       // 75 % duty
 *  ch1.enable(&ch1);
 * ============================================================= */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Object definition ─────────────────────────────────────── */
typedef struct PwmChannel PwmChannel;

struct PwmChannel {
    /* --- data (private by convention) --- */
    uint8_t  channel_id;
    uint32_t frequency_hz;
    uint8_t  duty_percent;   /* 0–100 */
    bool     enabled;
    uint32_t total_pulses;   /* runtime counter */

    /* --- methods --- */
    void (*enable   )(PwmChannel *self);
    void (*disable  )(PwmChannel *self);
    void (*set_duty )(PwmChannel *self, uint8_t percent);
    void (*set_freq )(PwmChannel *self, uint32_t hz);
    void (*tick     )(PwmChannel *self);   /* call every ms to count pulses */
    void (*status   )(const PwmChannel *self);
};

/* ── Method implementations ────────────────────────────────── */
static void pwm_enable(PwmChannel *self)
{
    self->enabled = true;
    printf("  [PWM ch%u] ENABLED  (freq=%u Hz, duty=%u%%)\n",
           self->channel_id, self->frequency_hz, self->duty_percent);
}

static void pwm_disable(PwmChannel *self)
{
    self->enabled = false;
    printf("  [PWM ch%u] DISABLED\n", self->channel_id);
}

static void pwm_set_duty(PwmChannel *self, uint8_t percent)
{
    if (percent > 100) percent = 100;
    self->duty_percent = percent;
    printf("  [PWM ch%u] duty → %u%%\n", self->channel_id, percent);
}

static void pwm_set_freq(PwmChannel *self, uint32_t hz)
{
    self->frequency_hz = hz;
    printf("  [PWM ch%u] freq → %u Hz\n", self->channel_id, hz);
}

static void pwm_tick(PwmChannel *self)
{
    /*
     * In a real driver this fires from a timer ISR.
     * Here we just simulate pulse counting.
     */
    if (self->enabled)
        self->total_pulses += self->frequency_hz / 1000; /* pulses per ms */
}

static void pwm_status(const PwmChannel *self)
{
    printf("  [PWM ch%u] freq=%u Hz | duty=%u%% | %s | pulses=%u\n",
           self->channel_id,
           self->frequency_hz,
           self->duty_percent,
           self->enabled ? "RUNNING" : "stopped",
           self->total_pulses);
}

/* ── Constructor ───────────────────────────────────────────── */
void pwm_init(PwmChannel *self, uint8_t channel_id, uint32_t frequency_hz)
{
    self->channel_id   = channel_id;
    self->frequency_hz = frequency_hz;
    self->duty_percent = 50;       /* default 50 % */
    self->enabled      = false;
    self->total_pulses = 0;

    /* wire function pointers */
    self->enable    = pwm_enable;
    self->disable   = pwm_disable;
    self->set_duty  = pwm_set_duty;
    self->set_freq  = pwm_set_freq;
    self->tick      = pwm_tick;
    self->status    = pwm_status;
}

/* ── Application ───────────────────────────────────────────── */
int main(void)
{
    printf("=== OBJECT PATTERN — PWM Channel Objects ===\n");
    printf("Each PwmChannel carries its own state and methods.\n\n");

    /* Four independent objects — no global channel variables */
    PwmChannel ch0, ch1, ch2, ch3;
    pwm_init(&ch0, 0, 20000);   /* servo: 20 kHz */
    pwm_init(&ch1, 1, 1000);    /* LED dimmer: 1 kHz */
    pwm_init(&ch2, 2, 25000);   /* fan: 25 kHz */
    pwm_init(&ch3, 3, 440);     /* buzzer: 440 Hz */

    printf("  -- Configure --\n");
    ch0.set_duty(&ch0, 7);       /* servo ~center */
    ch1.set_duty(&ch1, 80);      /* LED bright */
    ch2.set_duty(&ch2, 60);      /* fan 60% */
    ch3.set_duty(&ch3, 50);

    printf("\n  -- Enable --\n");
    ch0.enable(&ch0);
    ch1.enable(&ch1);
    ch2.enable(&ch2);
    ch3.enable(&ch3);

    /* Simulate 100 ms of ticks */
    for (int ms = 0; ms < 100; ms++) {
        ch0.tick(&ch0);
        ch1.tick(&ch1);
        ch2.tick(&ch2);
        ch3.tick(&ch3);
    }

    printf("\n  -- Adjust at runtime --\n");
    ch1.set_duty(&ch1, 20);      /* dim LED */
    ch2.set_freq(&ch2, 30000);   /* fan faster */
    ch3.disable(&ch3);           /* silence buzzer */

    printf("\n  -- Status report --\n");
    ch0.status(&ch0);
    ch1.status(&ch1);
    ch2.status(&ch2);
    ch3.status(&ch3);

    return 0;
}
