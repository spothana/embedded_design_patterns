/* =============================================================
 * PATTERN 11 — MUTEX (Mutual Exclusion)
 * =============================================================
 * CONCEPT
 * -------
 * A mutex allows only one thread at a time into a critical section.
 * Unlike a spinlock, a blocked thread SLEEPS (it is put in the
 * OS wait queue), freeing the CPU for other work while waiting.
 *
 * MUTEX vs SPINLOCK
 * -----------------
 *   Spinlock  — busy-wait, no scheduler, good for ISR & short CS
 *   Mutex     — sleeping wait, needs OS, good for long CS / tasks
 *
 * WHY IT MATTERS IN EMBEDDED SYSTEMS
 * ------------------------------------
 * RTOS-based firmware commonly shares buses:
 *   • I2C bus accessed by sensor task + configuration task
 *   • SPI bus accessed by display task + flash task
 *   • UART used by debug task + telemetry task
 * Without a mutex, bus transactions interleave and corrupt data.
 *
 * EXAMPLE SCENARIO
 * ----------------
 * Two RTOS tasks share a single I2C bus:
 *   EnvironmentTask  — reads BME280 (temp/humidity) every 500 ms
 *   CalibrationTask  — reads/writes EEPROM (24LC256) every 1500 ms
 *
 * Without the mutex, an EEPROM write can be interrupted mid-stream
 * by the BME280 START condition, hanging both devices.
 *
 * KEY RULES (embedded mutex use)
 * --------------------------------
 *  1. Always unlock in the same task that locked.
 *  2. Never call blocking I/O while holding a mutex.
 *  3. Acquire mutexes in a consistent order (avoid deadlock).
 *  4. Keep the critical section as short as possible.
 * ============================================================= */

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Simulated I2C bus ─────────────────────────────────────── */
typedef struct {
    pthread_mutex_t mutex;
    bool            bus_busy;       /* guard for demonstration */
    uint32_t        transaction_id;
} I2cBus;

static I2cBus i2c1;

static void i2c_bus_init(I2cBus *bus)
{
    pthread_mutex_init(&bus->mutex, NULL);
    bus->bus_busy       = false;
    bus->transaction_id = 0;
}

static void i2c_bus_destroy(I2cBus *bus)
{
    pthread_mutex_destroy(&bus->mutex);
}

/* Returns transaction ID so task can reference its own xfer */
static uint32_t i2c_start(I2cBus *bus, uint8_t addr, const char *task)
{
    /* mutex_lock() puts this task to sleep if bus is held */
    pthread_mutex_lock(&bus->mutex);
    bus->bus_busy = true;
    uint32_t tid  = ++bus->transaction_id;
    printf("  [I2C] START  addr=0x%02X  tid=%u  [%s holds bus]\n",
           addr, tid, task);
    return tid;
}

static void i2c_write_byte(I2cBus *bus, uint8_t reg, uint8_t val, const char *task)
{
    printf("  [I2C tid=%u] WRITE  reg=0x%02X val=0x%02X  [%s]\n",
           bus->transaction_id, reg, val, task);
    usleep(5000);   /* simulate 5 ms I2C transfer */
}

static uint8_t i2c_read_byte(I2cBus *bus, uint8_t reg, const char *task)
{
    printf("  [I2C tid=%u] READ   reg=0x%02X          [%s]\n",
           bus->transaction_id, reg, task);
    usleep(3000);   /* simulate 3 ms I2C transfer */
    return 0x5A;    /* simulated data */
}

static void i2c_stop(I2cBus *bus, uint32_t tid, const char *task)
{
    printf("  [I2C] STOP   tid=%u             [%s releases bus]\n", tid, task);
    bus->bus_busy = false;
    pthread_mutex_unlock(&bus->mutex);  /* release, next waiter wakes */
}

/* ── Task 1: Environment Sensor (BME280) ────────────────────── */
typedef struct { I2cBus *bus; int iterations; } TaskArg;

static void *task_environment(void *arg)
{
    TaskArg *a   = (TaskArg *)arg;
    I2cBus  *bus = a->bus;

    for (int i = 0; i < a->iterations; i++) {
        printf("\n  >> [EnvironmentTask] waking — BME280 read #%d\n", i+1);

        uint32_t tid = i2c_start(bus, 0x76, "EnvironmentTask");
        i2c_write_byte(bus, 0xF3, 0x00, "EnvironmentTask"); /* status check */
        uint8_t tmp  = i2c_read_byte(bus, 0xFA, "EnvironmentTask"); /* temp MSB */
        uint8_t hum  = i2c_read_byte(bus, 0xFD, "EnvironmentTask"); /* hum MSB  */
        i2c_stop(bus, tid, "EnvironmentTask");

        printf("  >> [EnvironmentTask] T_raw=0x%02X  H_raw=0x%02X\n", tmp, hum);
        usleep(500000);  /* 500 ms between reads */
    }
    return NULL;
}

/* ── Task 2: EEPROM Config (24LC256) ────────────────────────── */
static void *task_calibration(void *arg)
{
    TaskArg *a   = (TaskArg *)arg;
    I2cBus  *bus = a->bus;

    for (int i = 0; i < a->iterations; i++) {
        printf("\n  >> [CalibTask] waking — EEPROM access #%d\n", i+1);

        /* Write a calibration value */
        uint32_t tid = i2c_start(bus, 0x50, "CalibTask");
        i2c_write_byte(bus, 0x00, (uint8_t)(i * 10), "CalibTask"); /* addr hi */
        i2c_write_byte(bus, 0x10, 0xCA,              "CalibTask"); /* data */
        i2c_stop(bus, tid, "CalibTask");

        usleep(5000);   /* EEPROM write cycle */

        /* Read back */
        tid = i2c_start(bus, 0x50, "CalibTask");
        uint8_t readback = i2c_read_byte(bus, 0x10, "CalibTask");
        i2c_stop(bus, tid, "CalibTask");

        printf("  >> [CalibTask] EEPROM readback=0x%02X\n", readback);
        usleep(1500000); /* 1.5 s between accesses */
    }
    return NULL;
}

int main(void)
{
    printf("=== MUTEX — Shared I2C Bus (two RTOS tasks) ===\n");
    printf("pthread_mutex ensures only one task drives the I2C bus at a time.\n");
    printf("Observe: START/STOP always come in pairs with no interleaving.\n");

    i2c_bus_init(&i2c1);

    TaskArg env_arg  = { &i2c1, 3 };
    TaskArg calib_arg = { &i2c1, 2 };

    pthread_t t_env, t_calib;
    pthread_create(&t_env,   NULL, task_environment, &env_arg);
    pthread_create(&t_calib, NULL, task_calibration, &calib_arg);

    pthread_join(t_env,   NULL);
    pthread_join(t_calib, NULL);

    printf("\n  Total I2C transactions: %u\n", i2c1.transaction_id);
    printf("  No garbled transactions — mutex preserved bus integrity.\n");

    i2c_bus_destroy(&i2c1);
    return 0;
}
