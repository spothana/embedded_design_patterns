/* =============================================================
 * PATTERN 04 — SINGLETON
 * =============================================================
 * CONCEPT
 * -------
 * The Singleton ensures that a resource or configuration exists
 * as exactly ONE instance for the lifetime of the program.  A
 * getter function returns a pointer to that single instance; the
 * first call initialises it, all later calls return the same object.
 *
 * WHY IT MATTERS IN EMBEDDED SYSTEMS
 * ------------------------------------
 * Some resources must exist exactly once:
 *   • ADC calibration lookup table (computed once at boot)
 *   • Global event log / fault register
 *   • Hardware abstraction layer initialisation state
 * Having multiple copies wastes RAM and causes inconsistencies.
 *
 * EXAMPLE SCENARIO
 * ----------------
 * The ADC calibration table maps raw 12-bit ADC codes to millivolts.
 * It is computed once from a reference measurement at power-on.
 * Every module (thermistor, battery monitor, light sensor) uses the
 * same table — they must all see the same calibrated values.
 *
 * THREAD SAFETY NOTE
 * ------------------
 * On bare-metal with a single core, static initialisation is safe.
 * On an RTOS, protect the first-call initialisation with a mutex.
 * ============================================================= */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>    /* for illustrative gain computation */

#define ADC_BITS        12
#define ADC_FULL_SCALE  4095

/* ── The Singleton type ────────────────────────────────────── */
typedef struct {
    bool     initialised;
    float    vref_mv;        /* reference voltage measured at boot  */
    float    gain;           /* counts → millivolts multiplier      */
    float    offset_mv;      /* systematic offset correction        */
    uint32_t init_timestamp; /* ms tick at which calibration ran    */
    uint32_t use_count;      /* how many conversions have been done */
} AdcCalibration;

/* ── Singleton accessor ────────────────────────────────────── */
/*
 * The static local variable is zero-initialised by C at program start
 * and lives until the program exits.  No two calls ever create a
 * second instance — the compiler guarantees this.
 */
AdcCalibration *adc_cal_get(void)
{
    static AdcCalibration instance;   /* THE single instance */
    return &instance;
}

/* ── "Constructor" — safe to call multiple times ───────────── */
void adc_cal_init(float measured_vref_mv, uint32_t timestamp_ms)
{
    AdcCalibration *cal = adc_cal_get();

    if (cal->initialised) {
        printf("  [AdcCal] Already initialised at t=%u ms — ignoring re-init\n",
               cal->init_timestamp);
        return;
    }

    cal->vref_mv        = measured_vref_mv;
    cal->gain           = measured_vref_mv / (float)ADC_FULL_SCALE;
    cal->offset_mv      = -0.5f;          /* ±0.5 mV systematic offset */
    cal->init_timestamp = timestamp_ms;
    cal->use_count      = 0;
    cal->initialised    = true;

    printf("  [AdcCal] Calibrated: Vref=%.2f mV, gain=%.4f mV/LSB, offset=%.2f mV\n",
           cal->vref_mv, cal->gain, cal->offset_mv);
}

/* ── Convert a raw ADC reading to millivolts ───────────────── */
float adc_raw_to_mv(uint16_t raw)
{
    AdcCalibration *cal = adc_cal_get();
    if (!cal->initialised) {
        printf("  [AdcCal] WARNING: not initialised — using defaults!\n");
        return raw * (3300.0f / ADC_FULL_SCALE);
    }
    cal->use_count++;
    return (raw * cal->gain) + cal->offset_mv;
}

/* ── Simulated sensor modules — each uses the same singleton ── */
static void thermistor_read(uint16_t raw)
{
    float mv = adc_raw_to_mv(raw);
    /* Simplified: 10 mV/°C, 0°C = 500 mV */
    float temp_c = (mv - 500.0f) / 10.0f;
    printf("  [Thermistor] raw=%u → %.2f mV → %.1f °C\n", raw, mv, temp_c);
}

static void battery_monitor_read(uint16_t raw)
{
    /* Voltage divider: actual = reading * 2 */
    float mv = adc_raw_to_mv(raw) * 2.0f;
    printf("  [Battery]    raw=%u → %.0f mV bus voltage\n", raw, mv);
}

static void light_sensor_read(uint16_t raw)
{
    float mv  = adc_raw_to_mv(raw);
    float pct = (mv / 3300.0f) * 100.0f;
    printf("  [LightSensor]raw=%u → %.1f%% illumination\n", raw, pct);
}

int main(void)
{
    printf("=== SINGLETON — ADC Calibration Table ===\n\n");

    printf("  -- Boot: power-supply module measures Vref --\n");
    adc_cal_init(3287.5f, 42);    /* measured Vref = 3287.5 mV at t=42 ms */

    printf("\n  -- Another module tries to re-init (should be silently ignored) --\n");
    adc_cal_init(3300.0f, 500);   /* should be ignored */

    printf("\n  -- Three independent modules use the same calibration --\n");
    thermistor_read(1500);
    battery_monitor_read(2048);
    light_sensor_read(3200);

    printf("\n  -- Singleton statistics --\n");
    AdcCalibration *cal = adc_cal_get();
    printf("  Conversions performed: %u\n", cal->use_count);
    printf("  Calibrated at:         t=%u ms\n", cal->init_timestamp);

    /* Prove it is one instance */
    AdcCalibration *p1 = adc_cal_get();
    AdcCalibration *p2 = adc_cal_get();
    printf("  adc_cal_get() == adc_cal_get() ? %s\n",
           p1 == p2 ? "YES  ← same address" : "NO  ← BUG");

    return 0;
}
