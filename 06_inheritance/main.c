/* =============================================================
 * PATTERN 06 — INHERITANCE / POLYMORPHISM
 * =============================================================
 * CONCEPT
 * -------
 * C structs can simulate class inheritance by embedding a "base"
 * struct as the FIRST member of a "derived" struct.  A pointer
 * to the derived struct is safely cast to a pointer to the base
 * because both point to the same memory address.  Polymorphism
 * is achieved through the function pointers in the base struct.
 *
 * WHY IT MATTERS IN EMBEDDED SYSTEMS
 * ------------------------------------
 * A motion control board may drive DC motors, stepper motors,
 * and servo motors.  The control loop that runs them should not
 * be rewritten for each type.  With inheritance, a single
 * Actuator* array can hold all three, and the control loop calls
 * actuator->move() without knowing the underlying type.
 *
 * EXAMPLE SCENARIO
 * ----------------
 * Three actuator types share a common Actuator interface:
 *   init() / move(steps) / stop() / status()
 *
 * A single control_loop() function drives all of them identically.
 *
 * KEY MECHANICS
 * -------------
 *  struct DcMotor   { Actuator base; int channel; int rpm; };
 *  struct Stepper   { Actuator base; int step_pin; long position; };
 *  struct Servo     { Actuator base; int pwm_pin;  int angle_deg; };
 *
 *  Actuator *all[] = { &dc.base, &step.base, &servo.base };
 *  for each: all[i]->move(all[i], 200);   // polymorphic call
 * ============================================================= */

#include <stdio.h>
#include <stdint.h>

/* ── Base "class" ──────────────────────────────────────────── */
typedef struct Actuator Actuator;

struct Actuator {
    const char *type_name;
    void (*init  )(Actuator *self);
    void (*move  )(Actuator *self, int amount);   /* amount is type-specific */
    void (*stop  )(Actuator *self);
    void (*status)(const Actuator *self);
};

/* ── Derived: DC Motor ─────────────────────────────────────── */
typedef struct {
    Actuator base;           /* MUST be first member */
    uint8_t  channel;
    int      current_rpm;
    int      target_rpm;
} DcMotor;

static void dc_init(Actuator *a)
{
    DcMotor *m = (DcMotor *)a;
    m->current_rpm = 0;
    printf("    [DcMotor ch%u] initialised (H-bridge ready)\n", m->channel);
}
static void dc_move(Actuator *a, int rpm)
{
    DcMotor *m = (DcMotor *)a;
    m->target_rpm  = rpm;
    m->current_rpm = rpm;   /* instant for demo */
    printf("    [DcMotor ch%u] spinning at %d RPM\n", m->channel, rpm);
}
static void dc_stop(Actuator *a)
{
    DcMotor *m = (DcMotor *)a;
    m->current_rpm = 0;
    printf("    [DcMotor ch%u] stopped (coast)\n", m->channel);
}
static void dc_status(const Actuator *a)
{
    const DcMotor *m = (const DcMotor *)a;
    printf("    [DcMotor ch%u] RPM=%d\n", m->channel, m->current_rpm);
}

static void dc_motor_init(DcMotor *m, uint8_t channel)
{
    m->channel        = channel;
    m->current_rpm    = 0;
    m->target_rpm     = 0;
    m->base.type_name = "DcMotor";
    m->base.init      = dc_init;
    m->base.move      = dc_move;
    m->base.stop      = dc_stop;
    m->base.status    = dc_status;
}

/* ── Derived: Stepper Motor ────────────────────────────────── */
typedef struct {
    Actuator base;
    uint8_t  step_pin;
    uint8_t  dir_pin;
    long     position_steps;
} Stepper;

static void step_init(Actuator *a)
{
    Stepper *s = (Stepper *)a;
    s->position_steps = 0;
    printf("    [Stepper s%u/d%u] initialised (position zeroed)\n",
           s->step_pin, s->dir_pin);
}
static void step_move(Actuator *a, int steps)
{
    Stepper *s = (Stepper *)a;
    s->position_steps += steps;
    printf("    [Stepper] moved %+d steps → position=%ld\n",
           steps, s->position_steps);
}
static void step_stop(Actuator *a)
{
    Stepper *s = (Stepper *)a;
    printf("    [Stepper] hold at position=%ld\n", s->position_steps);
}
static void step_status(const Actuator *a)
{
    const Stepper *s = (const Stepper *)a;
    printf("    [Stepper] position=%ld steps\n", s->position_steps);
}

static void stepper_init(Stepper *s, uint8_t step_pin, uint8_t dir_pin)
{
    s->step_pin       = step_pin;
    s->dir_pin        = dir_pin;
    s->position_steps = 0;
    s->base.type_name = "Stepper";
    s->base.init      = step_init;
    s->base.move      = step_move;
    s->base.stop      = step_stop;
    s->base.status    = step_status;
}

/* ── Derived: Servo ────────────────────────────────────────── */
typedef struct {
    Actuator base;
    uint8_t  pwm_channel;
    int      angle_deg;    /* 0–180 */
} Servo;

static void servo_init(Actuator *a)
{
    Servo *s = (Servo *)a;
    s->angle_deg = 90;     /* centre */
    printf("    [Servo pwm%u] initialised at 90°\n", s->pwm_channel);
}
static void servo_move(Actuator *a, int angle)
{
    Servo *s = (Servo *)a;
    if (angle < 0)   angle = 0;
    if (angle > 180) angle = 180;
    s->angle_deg = angle;
    /* pulse width: 1000–2000 µs maps to 0–180° */
    int pulse_us = 1000 + (angle * 1000 / 180);
    printf("    [Servo pwm%u] angle=%d° → pulse=%d µs\n",
           s->pwm_channel, angle, pulse_us);
}
static void servo_stop(Actuator *a)
{
    Servo *s = (Servo *)a;
    printf("    [Servo pwm%u] holding at %d°\n", s->pwm_channel, s->angle_deg);
}
static void servo_status(const Actuator *a)
{
    const Servo *s = (const Servo *)a;
    printf("    [Servo pwm%u] angle=%d°\n", s->pwm_channel, s->angle_deg);
}

static void servo_init_obj(Servo *s, uint8_t pwm_channel)
{
    s->pwm_channel    = pwm_channel;
    s->angle_deg      = 90;
    s->base.type_name = "Servo";
    s->base.init      = servo_init;
    s->base.move      = servo_move;
    s->base.stop      = servo_stop;
    s->base.status    = servo_status;
}

/* ── Generic control loop — works for ANY Actuator ──────────── */
static void control_loop(Actuator **actuators, int count)
{
    printf("\n  [ControlLoop] Initialising %d actuators:\n", count);
    for (int i = 0; i < count; i++)
        actuators[i]->init(actuators[i]);

    printf("\n  [ControlLoop] Moving all to position 100:\n");
    for (int i = 0; i < count; i++)
        actuators[i]->move(actuators[i], 100);

    printf("\n  [ControlLoop] Status report:\n");
    for (int i = 0; i < count; i++)
        actuators[i]->status(actuators[i]);

    printf("\n  [ControlLoop] Stopping all:\n");
    for (int i = 0; i < count; i++)
        actuators[i]->stop(actuators[i]);
}

int main(void)
{
    printf("=== INHERITANCE / POLYMORPHISM — Actuator Control ===\n");
    printf("control_loop() calls move()/stop() without knowing the type.\n");

    DcMotor dc;
    Stepper step;
    Servo   srv;

    dc_motor_init(&dc,   2);
    stepper_init (&step, 5, 6);
    servo_init_obj(&srv, 3);

    /* All three stored as Actuator* — polymorphic array */
    Actuator *all[] = { &dc.base, &step.base, &srv.base };
    control_loop(all, 3);

    return 0;
}
