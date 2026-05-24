![License](https://img.shields.io/github/license/spothana/embedded_design_patterns)
![Stars](https://img.shields.io/github/stars/spothana/embedded_design_patterns)

# Embedded Design Patterns — Study Guide

A hands-on C + CMake project covering every design pattern in embedded systems.

Every pattern uses an embedded-systems scenario chosen to
make the pattern's purpose immediately obvious to a firmware developer.
Each `main.c` is a self-contained lesson: read top-to-bottom, run it,
then modify it.

---

## Quick start

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

make run_all          # run every demo in order

# or run individually
./01_factory/01_factory
./12_state_machine/12_state_machine
```

---

## Patterns at a glance

| # | Pattern | Scenario | Key Concept |
|---|---------|----------|-------------|
| 01 | **Factory Method** | UART / I2C / SPI driver | Create objects without knowing their concrete type |
| 02 | **Object** | PWM channel (4 independent channels) | Bundle data + methods in a struct; struct-as-class |
| 03 | **Opaque** | UART RX ring buffer | Hide struct internals; compiler-enforced encapsulation |
| 04 | **Singleton** | ADC calibration table | One and only one instance; shared by all modules |
| 05 | **Callback** | Timer event dispatcher | Invert dependency; driver calls app without knowing it |
| 06 | **Inheritance** | DC motor / stepper / servo | Polymorphic array via embedded base struct |
| 07 | **Virtual API** | NOR Flash chip swap (W25Q64 → AT25SF) | Stable vtable isolates app from hardware changes |
| 08 | **Bridge** | Logger × Transport (UART / RTT / Flash) | Two independent variation axes; bridge pointer |
| 09 | **Concurrency** | Priority cooperative scheduler (5 tasks) | Bare-metal multitasking without RTOS |
| 10 | **Spinlock** | DMA transfer queue (ISR ↔ task) | Atomic busy-wait; correct for ISR context |
| 11 | **Mutex** | Shared I2C bus (env task + calib task) | Sleeping wait; correct for long critical sections |
| 12 | **State Machine** | Li-Ion battery charger (CC→CV→DONE) | Self-contained states; enter/event/exit hooks |

---

## Pattern deep-dives

### 01 · Factory Method — UART / I2C / SPI Driver
**Problem:** A data-logger must work over any bus fitted on the board.
**Solution:** `comms_create(BUS_UART / BUS_I2C / BUS_SPI)` returns a
`CommsDriver*`. The logger calls `open() / send() / close()` — it never
sees `UartDriver`, `I2cDriver`, or `SpiDriver` directly.
**Key takeaway:** Only the factory function knows the concrete types.

### 02 · Object Pattern — PWM Channel
**Problem:** Four PWM channels need independent state (frequency, duty,
enable flag, pulse counter). Global variables per channel is a mess.
**Solution:** `PwmChannel` struct carries all state + function pointers.
`pwm_init()` wires the methods. `ch1.set_duty(&ch1, 75)` is idiomatic C
for `ch1.set_duty(75)` in C++.
**Key takeaway:** Struct + init function = C class.

### 03 · Opaque Pattern — Ring Buffer
**Problem:** Exposing `head`, `tail`, `count` lets application code
corrupt the buffer directly.
**Solution:** `ring_buffer.h` has only `typedef struct RingBuffer RingBuffer;`.
The struct body is in `ring_buffer.c`. The compiler rejects
`rb->count = 99;` from application code with "incomplete type".
**Key takeaway:** Move the struct body to the .c file to enforce encapsulation.

### 04 · Singleton — ADC Calibration Table
**Problem:** ADC calibration is computed once at boot. Every sensor
module must see the same calibration values.
**Solution:** `static AdcCalibration instance;` inside `adc_cal_get()`
lives forever and is zero-initialised once. All calls return the same address.
**Key takeaway:** `static` local inside a getter = thread-safe (bare-metal) singleton.

### 05 · Callback — Timer Dispatcher
**Problem:** The timer driver must notify five different application
functions at different intervals without depending on them.
**Solution:** `timer_register(period_ms, fn, ctx)` stores a function
pointer and context per entry. `timer_tick_ms()` fires them when due.
**Key takeaway:** `typedef void (*TimerCallback)(void *ctx)` — always add
a `ctx` pointer so the callback can reach its own data without globals.

### 06 · Inheritance / Polymorphism — Actuator Control
**Problem:** `control_loop()` must call `init/move/stop` on DC motors,
steppers, and servos without knowing which type it has.
**Solution:** Each concrete struct embeds `Actuator base` as its first member.
`Actuator *all[] = { &dc.base, &step.base, &srv.base }` — casting is safe
because C guarantees the address of the first member equals the struct address.
**Key takeaway:** First-member embedding = C inheritance; function pointers = virtual dispatch.

### 07 · Virtual API (Adapter) — Flash Chip Swap
**Problem:** PCB rev2 replaced the W25Q64 NOR flash with an AT25SF641
that has a different SPI command set. The bootloader must not change.
**Solution:** `FlashDriver` vtable is the stable contract. `w25q64_create()`
and `at25sf_create()` fill the same vtable differently.
**Key takeaway:** Define the interface first, implement second. HW changes = new adapter only.

### 08 · Bridge — Logger × Transport
**Problem:** 3 log levels × 3 transports = 9 combinations. Subclassing
each causes an explosion.
**Solution:** `Logger` (abstraction) holds a `Transport*` (bridge pointer).
Swap `logger_set_transport()` at runtime — no logger code changes.
**Key takeaway:** When you have NxM subclass explosion, separate the axes and bridge them.

### 09 · Concurrency Scheduler — IoT Sensor Node
**Problem:** Five tasks need to run at different rates without an RTOS.
**Solution:** Each `Task` stores `{fn, period, deadline, priority}`.
`sched_tick()` finds the highest-priority overdue task and runs it.
**Key takeaway:** Tasks must be short and non-blocking. State between calls lives in static variables or passed context.

### 10 · Spinlock — DMA Queue
**Problem:** The DMA-complete ISR and the application task both touch
the job queue. A context switch mid-update corrupts head/tail.
**Solution:** `__sync_lock_test_and_set` atomically grabs the lock before
the critical section. The ISR spins for a handful of cycles.
**Key takeaway:** Spinlock in ISR — always. Keep the critical section to ≤10 instructions.

### 11 · Mutex — Shared I2C Bus
**Problem:** Two RTOS tasks share one I2C bus. Without locking, an
EEPROM write can be interrupted mid-transaction by a sensor read,
corrupting both transfers.
**Solution:** `pthread_mutex_lock/unlock` wraps each complete I2C transaction.
The waiting task sleeps — CPU is free for other tasks.
**Key takeaway:** Mutex for sleeping wait (tasks). Spinlock for busy-wait (ISR).

### 12 · Behavioral State Machine — Battery Charger
**Problem:** A charger with five states and six events requires dozens
of inter-tangled `if` statements that are impossible to verify.
**Solution:** Each `State` is `{name, on_enter, on_event, on_exit}`.
`fsm_transition()` calls exit on the old state, swaps the pointer, calls
enter on the new state. Adding a state = adding one struct and its handlers.
**Key takeaway:** Self-transitions, guards, and entry/exit actions are the three tools
of every practical embedded FSM.

---

## Design pattern relationships

```
Creational              Structural             Behavioural / Concurrency
──────────              ──────────             ─────────────────────────
Factory Method          Callback               Concurrency Scheduler
Object                  Inheritance            Spinlock
Opaque                  Virtual API            Mutex
Singleton               Bridge                 State Machine
```

---

## File layout

```
embedded_patterns/
├── CMakeLists.txt
├── README.md
├── 01_factory/        main.c
├── 02_object/         main.c
├── 03_opaque/         ring_buffer.h  ring_buffer.c  main.c
├── 04_singleton/      main.c
├── 05_callback/       main.c
├── 06_inheritance/    main.c
├── 07_virtual_api/    main.c
├── 08_bridge/         main.c
├── 09_scheduler/      main.c
├── 10_spinlock/       main.c
├── 11_mutex/          main.c
└── 12_state_machine/  main.c
```

---

## Study tips

1. **Read the block comment at the top of each `main.c`** before the code.
   It states the problem, why it matters, and the key mechanics.
2. **Break it intentionally.** Remove a spinlock, skip `mutex_unlock`, or
   add a second singleton init — see what breaks and why.
3. **Combine patterns.** The Factory returns Objects. Objects use Callbacks.
   A State Machine uses a Singleton configuration table. Real firmware does all of this.
