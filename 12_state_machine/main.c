/* =============================================================
 * PATTERN 12 — BEHAVIORAL STATE MACHINE
 * =============================================================
 * CONCEPT
 * -------
 * The State Machine (State Pattern) assigns each system state its
 * own set of behaviours.  The context holds a pointer to the
 * current state.  Events are dispatched to the current state's
 * handler.  Transitions are explicit state pointer swaps.
 *
 * This eliminates sprawling if/switch chains whose conditions depend
 * on multiple flags — each state is self-contained and independent.
 *
 * WHY IT MATTERS IN EMBEDDED SYSTEMS
 * ------------------------------------
 * Real embedded products are fundamentally state machines:
 *   • Battery charger: IDLE → CC_CHARGE → CV_CHARGE → DONE → FAULT
 *   • Motor controller: STOPPED → ACCEL → CRUISE → DECEL → STOPPED
 *   • BLE device: ADV → CONNECTING → CONNECTED → BONDING → SLEEPING
 * Implementing these with nested ifs leads to bugs that are nearly
 * impossible to test.  Explicit FSMs are auditable and safe.
 *
 * EXAMPLE SCENARIO
 * ----------------
 * A Li-Ion battery charger controller implements the standard
 * CC/CV (constant-current / constant-voltage) charging algorithm:
 *
 *  ┌────────┐  plug-in ┌──────────┐ Vbat≥Vcc ┌──────────┐
 *  │  IDLE  │─────────►│CC_CHARGE │──────────►│CV_CHARGE │
 *  └────────┘          └──────────┘           └──────────┘
 *       ▲                   │ fault                 │ Ibat<Imin
 *       │                   ▼                       ▼
 *       │             ┌──────────┐           ┌──────────┐
 *       └─────────────│  FAULT   │◄──────────│   DONE   │
 *        unplug       └──────────┘  unplug   └──────────┘
 *
 * KEY MECHANICS
 * -------------
 *  • Each State is a struct: { name, on_enter, on_event, on_exit }
 *  • ChargerFSM holds: State *current, ChargerData data
 *  • fsm_send_event() dispatches to current->on_event()
 *  • Transitions call fsm_transition() which fires exit/enter hooks
 * ============================================================= */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Events ────────────────────────────────────────────────── */
typedef enum {
    EVT_PLUG_IN,       /* charger connected to battery pack */
    EVT_UNPLUG,        /* charger disconnected              */
    EVT_VBAT_HIGH,     /* battery reached CC→CV threshold   */
    EVT_IBAT_LOW,      /* tail current reached — fully done */
    EVT_OVER_TEMP,     /* temperature fault detected        */
    EVT_TIMER_TICK,    /* periodic 1-second tick            */
} Event;

static const char *event_names[] = {
    "PLUG_IN", "UNPLUG", "VBAT_HIGH", "IBAT_LOW", "OVER_TEMP", "TIMER_TICK"
};

/* ── Charger physical data (shared across states) ──────────── */
typedef struct {
    float    vbat_mv;       /* battery voltage  */
    float    ibat_ma;       /* charge current   */
    float    temp_c;        /* cell temperature */
    uint32_t charge_sec;    /* seconds in current phase */
    uint32_t total_sec;     /* total charge time */
    bool     plugged_in;
} ChargerData;

/* ── Forward declarations ──────────────────────────────────── */
typedef struct ChargerFSM ChargerFSM;
typedef struct State      State;

struct State {
    const char *name;
    void (*on_enter)(ChargerFSM *fsm);
    void (*on_event)(ChargerFSM *fsm, Event e);
    void (*on_exit )(ChargerFSM *fsm);
};

struct ChargerFSM {
    const State *current;
    ChargerData  data;
    uint32_t     event_count;
};

/* ── Transition helper ─────────────────────────────────────── */
static void fsm_transition(ChargerFSM *fsm, const State *next)
{
    printf("      [FSM] %s → %s\n", fsm->current->name, next->name);
    if (fsm->current->on_exit)  fsm->current->on_exit(fsm);
    fsm->current = next;
    fsm->data.charge_sec = 0;
    if (fsm->current->on_enter) fsm->current->on_enter(fsm);
}

/* ── Forward-declare all states ────────────────────────────── */
static const State st_idle;
static const State st_cc;
static const State st_cv;
static const State st_done;
static const State st_fault;

/* ─────────────────── IDLE ─────────────────────────────────── */
static void idle_enter(ChargerFSM *fsm)
{
    printf("    Charger IDLE — outputs off\n");
    fsm->data.ibat_ma = 0;
}
static void idle_event(ChargerFSM *fsm, Event e)
{
    if (e == EVT_PLUG_IN) {
        fsm->data.plugged_in = true;
        fsm->data.vbat_mv    = 3600.0f;  /* simulated starting voltage */
        fsm->data.ibat_ma    = 0;
        fsm->data.temp_c     = 25.0f;
        fsm_transition(fsm, &st_cc);
    }
}
static const State st_idle = { "IDLE", idle_enter, idle_event, NULL };

/* ─────────────── CC CHARGE (Constant Current) ─────────────── */
static void cc_enter(ChargerFSM *fsm)
{
    fsm->data.ibat_ma = 1000.0f;   /* 1 A constant current */
    printf("    CC phase: Ibat=%.0f mA, Vbat=%.0f mV\n",
           fsm->data.ibat_ma, fsm->data.vbat_mv);
}
static void cc_event(ChargerFSM *fsm, Event e)
{
    switch (e) {
        case EVT_TIMER_TICK:
            fsm->data.charge_sec++;
            fsm->data.total_sec++;
            fsm->data.vbat_mv += 2.5f;   /* voltage rises during CC */
            fsm->data.temp_c  += 0.1f;
            printf("    [CC  t=%us] Vbat=%.0f mV  Ibat=%.0f mA  T=%.1f°C\n",
                   fsm->data.charge_sec, fsm->data.vbat_mv,
                   fsm->data.ibat_ma, fsm->data.temp_c);
            break;
        case EVT_VBAT_HIGH:
            printf("    Vbat reached CV threshold (4200 mV) → switching to CV\n");
            fsm_transition(fsm, &st_cv);
            break;
        case EVT_OVER_TEMP:
            fsm_transition(fsm, &st_fault);
            break;
        case EVT_UNPLUG:
            fsm_transition(fsm, &st_idle);
            break;
        default: break;
    }
}
static void cc_exit(ChargerFSM *fsm)
{
    printf("    CC phase ended after %u s\n", fsm->data.charge_sec);
}
static const State st_cc = { "CC_CHARGE", cc_enter, cc_event, cc_exit };

/* ─────────────── CV CHARGE (Constant Voltage) ─────────────── */
static void cv_enter(ChargerFSM *fsm)
{
    fsm->data.ibat_ma = 900.0f;   /* current starts falling */
    printf("    CV phase: Vbat held at 4200 mV, Ibat tapering\n");
}
static void cv_event(ChargerFSM *fsm, Event e)
{
    switch (e) {
        case EVT_TIMER_TICK:
            fsm->data.charge_sec++;
            fsm->data.total_sec++;
            fsm->data.ibat_ma -= 50.0f;   /* taper current */
            if (fsm->data.ibat_ma < 0) fsm->data.ibat_ma = 0;
            printf("    [CV  t=%us] Vbat=4200 mV  Ibat=%.0f mA  T=%.1f°C\n",
                   fsm->data.charge_sec, fsm->data.ibat_ma, fsm->data.temp_c);
            break;
        case EVT_IBAT_LOW:
            printf("    Ibat below 50 mA → charge complete\n");
            fsm_transition(fsm, &st_done);
            break;
        case EVT_OVER_TEMP:
            fsm_transition(fsm, &st_fault);
            break;
        case EVT_UNPLUG:
            fsm_transition(fsm, &st_idle);
            break;
        default: break;
    }
}
static void cv_exit(ChargerFSM *fsm)
{
    printf("    CV phase ended after %u s\n", fsm->data.charge_sec);
}
static const State st_cv = { "CV_CHARGE", cv_enter, cv_event, cv_exit };

/* ─────────────── DONE ─────────────────────────────────────── */
static void done_enter(ChargerFSM *fsm)
{
    printf("    ✓ Battery FULLY CHARGED in %u s total\n", fsm->data.total_sec);
    fsm->data.ibat_ma = 0;
}
static void done_event(ChargerFSM *fsm, Event e)
{
    if (e == EVT_UNPLUG) fsm_transition(fsm, &st_idle);
}
static const State st_done = { "DONE", done_enter, done_event, NULL };

/* ─────────────── FAULT ────────────────────────────────────── */
static void fault_enter(ChargerFSM *fsm)
{
    printf("    ✗ FAULT — outputs off, alert LED blinking\n");
    fsm->data.ibat_ma = 0;
}
static void fault_event(ChargerFSM *fsm, Event e)
{
    if (e == EVT_UNPLUG) {
        printf("    Fault cleared by unplug\n");
        fsm_transition(fsm, &st_idle);
    }
}
static const State st_fault = { "FAULT", fault_enter, fault_event, NULL };

/* ── FSM dispatcher ────────────────────────────────────────── */
static void fsm_send(ChargerFSM *fsm, Event e)
{
    fsm->event_count++;
    /* Print the event name so event_names[] earns its keep */
    printf("  [EVENT] %s  (state: %s)\n", event_names[e], fsm->current->name);
    fsm->current->on_event(fsm, e);
}

/* ── Scenario runner ───────────────────────────────────────── */
int main(void)
{
    printf("=== BEHAVIORAL STATE MACHINE — Li-Ion Battery Charger ===\n\n");

    ChargerFSM charger = { .current = &st_idle };
    charger.current->on_enter(&charger);   /* enter initial state */

    printf("\n  -- Scenario A: Normal full charge cycle --\n");
    fsm_send(&charger, EVT_PLUG_IN);

    /* Simulate CC phase (4 ticks then voltage threshold) */
    for (int i = 0; i < 4; i++)
        fsm_send(&charger, EVT_TIMER_TICK);
    fsm_send(&charger, EVT_VBAT_HIGH);     /* transition CC→CV */

    /* Simulate CV taper (current drops each tick, then done) */
    for (int i = 0; i < 6; i++)
        fsm_send(&charger, EVT_TIMER_TICK);
    fsm_send(&charger, EVT_IBAT_LOW);      /* current < 50 mA → DONE */

    fsm_send(&charger, EVT_UNPLUG);        /* back to IDLE */

    printf("\n  -- Scenario B: Fault during CC phase --\n");
    fsm_send(&charger, EVT_PLUG_IN);
    fsm_send(&charger, EVT_TIMER_TICK);
    fsm_send(&charger, EVT_TIMER_TICK);
    fsm_send(&charger, EVT_OVER_TEMP);     /* thermal runaway → FAULT */
    fsm_send(&charger, EVT_TIMER_TICK);    /* ignored in FAULT state */
    fsm_send(&charger, EVT_UNPLUG);        /* clears fault → IDLE */

    printf("\n  Total events processed: %u\n", charger.event_count);
    return 0;
}
