/* =============================================================
 * PATTERN 01 — FACTORY METHOD
 * =============================================================
 * CONCEPT
 * -------
 * The Factory Method pattern defines an interface for creating an
 * object but lets a factory function decide which concrete type to
 * instantiate.  Callers work through the interface and never need
 * to know which concrete implementation they received.
 *
 * WHY IT MATTERS IN EMBEDDED SYSTEMS
 * ------------------------------------
 * Microcontrollers typically have several communication buses
 * (UART, I2C, SPI).  Higher-level code (e.g. a data-logger) should
 * not be littered with #ifdefs deciding which bus to use.  A factory
 * function reads a compile-time or runtime configuration and hands
 * back the right driver — the logger just calls send().
 *
 * EXAMPLE SCENARIO
 * ----------------
 * A data-logger must send packets over whichever bus is wired on the
 * board.  We model three bus drivers behind a single CommsDriver
 * interface and provide a factory that creates the right one.
 *
 * KEY MECHANICS
 * -------------
 *  • CommsDriver  — the interface (struct of function pointers)
 *  • UartDriver / I2cDriver / SpiDriver — concrete implementations
 *  • comms_create() — the factory; only place that knows concrete types
 *
 *                    ┌─────────────────┐
 *                    │   CommsDriver   │  ← interface
 *                    │  open()         │
 *                    │  send()         │
 *                    │  close()        │
 *                    └────────┬────────┘
 *           ┌─────────────────┼─────────────────┐
 *      UartDriver        I2cDriver          SpiDriver
 * ============================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── Interface ─────────────────────────────────────────────── */
typedef enum { BUS_UART, BUS_I2C, BUS_SPI } BusType;

typedef struct CommsDriver CommsDriver;
struct CommsDriver {
    BusType type;
    int     (*open )(CommsDriver *self, unsigned int speed_hz);
    int     (*send )(CommsDriver *self, const uint8_t *data, size_t len);
    void    (*close)(CommsDriver *self);
    void    (*destroy)(CommsDriver *self);
};

/* ── Concrete: UART ────────────────────────────────────────── */
typedef struct { CommsDriver base; unsigned int baud; } UartDriver;

static int uart_open(CommsDriver *self, unsigned int speed_hz) {
    UartDriver *d = (UartDriver *)self;
    d->baud = speed_hz;
    printf("  [UART] opened at %u baud\n", d->baud);
    return 0;
}
static int uart_send(CommsDriver *self, const uint8_t *data, size_t len) {
    (void)self;
    /* Check if all bytes are printable text */
    int all_print = 1;
    for (size_t i = 0; i < len; i++)
        if (data[i] < 0x20 || data[i] > 0x7E) { all_print = 0; break; }

    if (all_print) {
        printf("  [UART] sending %zu bytes: \"", len);
        for (size_t i = 0; i < len; i++) putchar((char)data[i]);
        printf("\"\n");
    } else {
        printf("  [UART] sending %zu bytes: [ ", len);
        for (size_t i = 0; i < len; i++) printf("0x%02X ", data[i]);
        printf("]\n");
    }
    return (int)len;
}
static void uart_close  (CommsDriver *self) { (void)self; printf("  [UART] closed\n"); }
static void uart_destroy(CommsDriver *self) { free(self); }

/* ── Concrete: I2C ─────────────────────────────────────────── */
typedef struct { CommsDriver base; uint8_t slave_addr; } I2cDriver;

static int i2c_open(CommsDriver *self, unsigned int speed_hz) {
    (void)self; printf("  [I2C]  opened at %u kHz, slave=0x%02X\n",
                        speed_hz/1000, ((I2cDriver*)self)->slave_addr);
    return 0;
}
static int i2c_send(CommsDriver *self, const uint8_t *data, size_t len) {
    (void)self;
    printf("  [I2C]  sending %zu bytes: [ ", len);
    for (size_t i = 0; i < len; i++) printf("0x%02X ", data[i]);
    printf("]\n");
    return (int)len;
}
static void i2c_close  (CommsDriver *self) { (void)self; printf("  [I2C]  closed\n"); }
static void i2c_destroy(CommsDriver *self) { free(self); }

/* ── Concrete: SPI ─────────────────────────────────────────── */
typedef struct { CommsDriver base; uint8_t cs_pin; } SpiDriver;

static int spi_open(CommsDriver *self, unsigned int speed_hz) {
    printf("  [SPI]  opened at %u kHz, CS=pin%u\n",
           speed_hz / 1000, ((SpiDriver*)self)->cs_pin);
    return 0;
}
static int spi_send(CommsDriver *self, const uint8_t *data, size_t len) {
    (void)self;
    printf("  [SPI]  shifting %zu bytes: [ ", len);
    for (size_t i = 0; i < len; i++) printf("%02X ", data[i]);
    printf("]\n");
    return (int)len;
}
static void spi_close  (CommsDriver *self) { (void)self; printf("  [SPI]  closed\n"); }
static void spi_destroy(CommsDriver *self) { free(self); }

/* ── Factory ───────────────────────────────────────────────── */
/*
 * comms_create() is the only function that knows the concrete types.
 * The caller receives a CommsDriver pointer and uses only the interface.
 */
CommsDriver *comms_create(BusType bus)
{
    switch (bus) {
        case BUS_UART: {
            UartDriver *d = calloc(1, sizeof(UartDriver));
            if (!d) return NULL;
            d->base.type    = BUS_UART;
            d->base.open    = uart_open;
            d->base.send    = uart_send;
            d->base.close   = uart_close;
            d->base.destroy = uart_destroy;
            d->baud         = 9600; /* default */
            return &d->base;
        }
        case BUS_I2C: {
            I2cDriver *d = calloc(1, sizeof(I2cDriver));
            if (!d) return NULL;
            d->base.type    = BUS_I2C;
            d->base.open    = i2c_open;
            d->base.send    = i2c_send;
            d->base.close   = i2c_close;
            d->base.destroy = i2c_destroy;
            d->slave_addr   = 0x50;
            return &d->base;
        }
        case BUS_SPI: {
            SpiDriver *d = calloc(1, sizeof(SpiDriver));
            if (!d) return NULL;
            d->base.type    = BUS_SPI;
            d->base.open    = spi_open;
            d->base.send    = spi_send;
            d->base.close   = spi_close;
            d->base.destroy = spi_destroy;
            d->cs_pin       = 4;
            return &d->base;
        }
    }
    return NULL;
}

/* ── Application: data-logger uses only the interface ─────── */
static void data_logger_run(CommsDriver *bus, BusType t)
{
    const char *names[] = { "UART", "I2C", "SPI" };
    printf("\n  >> Running data-logger over %s:\n", names[t]);

    bus->open(bus, 115200);

    uint8_t packet[] = { 0xAA, 0x01, 0x23, 0x45, 0x55 };
    bus->send(bus, packet, sizeof(packet));

    const char *text = "TEMP=23.5C";
    bus->send(bus, (const uint8_t *)text, strlen(text));

    bus->close(bus);
}

int main(void)
{
    printf("=== FACTORY METHOD — Communication Bus Driver ===\n");
    printf("The data-logger never sees UartDriver/I2cDriver/SpiDriver.\n");
    printf("It only uses CommsDriver* returned by comms_create().\n");

    BusType configs[] = { BUS_UART, BUS_I2C, BUS_SPI };
    for (int i = 0; i < 3; i++) {
        CommsDriver *bus = comms_create(configs[i]);
        data_logger_run(bus, configs[i]);
        bus->destroy(bus);
    }
    return 0;
}
