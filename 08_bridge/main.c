/* =============================================================
 * PATTERN 08 — BRIDGE
 * =============================================================
 * CONCEPT
 * -------
 * The Bridge pattern decouples an abstraction from its
 * implementation so both can vary independently.  The abstraction
 * holds a pointer (the "bridge") to the implementation.
 *
 * Without Bridge: N log-levels × M transports = N×M subclasses.
 * With    Bridge: N + M  — levels and transports mix freely.
 *
 * WHY IT MATTERS IN EMBEDDED SYSTEMS
 * ------------------------------------
 * A logging/telemetry system has two independent axes:
 *   AXIS 1 — Log level formatters: DebugLog, InfoLog, ErrorLog
 *   AXIS 2 — Output transports:    UART, RTT (Segger), SPI Flash
 * Any formatter should work with any transport without 3×3=9 classes.
 *
 * EXAMPLE SCENARIO
 * ----------------
 *   Logger (abstraction)          Transport (implementor)
 *   ─────────────────────         ───────────────────────
 *   DebugLogger  ──┐              UartTransport
 *   InfoLogger   ──┼──(bridge)──► RttTransport
 *   ErrorLogger  ──┘              FlashTransport
 *
 * KEY MECHANICS
 * -------------
 *  Logger holds a Transport*  (the bridge pointer).
 *  Logger.log() formats the message, then calls transport->send().
 *  Swap transport at runtime without touching the Logger code.
 * ============================================================= */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════
 *  IMPLEMENTOR side — Transport
 * ═══════════════════════════════════════════════════════════ */
typedef struct Transport Transport;
struct Transport {
    const char *name;
    int  (*send)(Transport *self, const char *formatted_msg);
    void (*flush)(Transport *self);
};

/* ── Concrete transport A: UART ──────────────────────────── */
static int uart_send(Transport *t, const char *msg)
{
    (void)t;
    printf("  [UART] >> %s", msg);
    return (int)strlen(msg);
}
static void uart_flush(Transport *t) { (void)t; /* no buffering */ }

static Transport uart_transport = { "UART", uart_send, uart_flush };

/* ── Concrete transport B: Segger RTT (simulated) ─────────── */
#define RTT_BUF_SIZE 512
static char rtt_buf[RTT_BUF_SIZE];
static int  rtt_pos = 0;

static int rtt_send(Transport *t, const char *msg)
{
    (void)t;
    int n = (int)strlen(msg);
    if (rtt_pos + n < RTT_BUF_SIZE - 1) {
        memcpy(rtt_buf + rtt_pos, msg, (size_t)n);
        rtt_pos += n;
        rtt_buf[rtt_pos] = '\0';
    }
    printf("  [RTT ] >> %s", msg);
    return n;
}
static void rtt_flush(Transport *t)
{
    (void)t;
    printf("  [RTT ] flush: %d bytes in ring\n", rtt_pos);
    rtt_pos = 0;
}
static Transport rtt_transport = { "SeggerRTT", rtt_send, rtt_flush };

/* ── Concrete transport C: Flash (log to NOR flash page) ───── */
static uint32_t flash_write_addr = 0x00080000;  /* log partition start */
static int flash_send(Transport *t, const char *msg)
{
    (void)t;
    int n = (int)strlen(msg);
    printf("  [FLASH@0x%08X] %s", flash_write_addr, msg);
    flash_write_addr += (uint32_t)((n + 3) & ~3);  /* 4-byte align */
    return n;
}
static void flash_flush(Transport *t) { (void)t; }

static Transport flash_transport = { "NorFlash", flash_send, flash_flush };

/* ═══════════════════════════════════════════════════════════
 *  ABSTRACTION side — Logger
 * ═══════════════════════════════════════════════════════════ */
typedef enum { LOG_DEBUG=0, LOG_INFO, LOG_WARN, LOG_ERROR } LogLevel;

typedef struct {
    LogLevel    level;
    const char *prefix;      /* e.g. "[DEBUG]" */
    Transport  *transport;   /* the bridge pointer */
    uint32_t    seq;         /* sequence number    */
} Logger;

static void logger_init(Logger *l, LogLevel level, Transport *t)
{
    const char *prefixes[] = { "DEBUG", "INFO ", "WARN ", "ERROR" };
    l->level     = level;
    l->prefix    = prefixes[level];
    l->transport = t;
    l->seq       = 0;
}

/* The core log function — formats then delegates to transport */
static void logger_log(Logger *l, LogLevel msg_level, const char *fmt, ...)
{
    if (msg_level < l->level) return;   /* filter below threshold */

    char formatted[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(formatted, sizeof(formatted), fmt, args);
    va_end(args);

    char final[300];
    const char *levels[] = { "DBG", "INF", "WRN", "ERR" };
    snprintf(final, sizeof(final), "[%s #%03u] %s\n",
             levels[msg_level], l->seq++, formatted);

    l->transport->send(l->transport, final);
}

/* Runtime bridge swap — change transport without recreating logger */
static void logger_set_transport(Logger *l, Transport *t)
{
    printf("    (logger switching transport: %s → %s)\n",
           l->transport->name, t->name);
    l->transport->flush(l->transport);
    l->transport = t;
}

/* ── Application ───────────────────────────────────────────── */
static void run_system_diagnostics(Logger *log)
{
    logger_log(log, LOG_DEBUG, "Stack watermark: %d bytes free", 1248);
    logger_log(log, LOG_INFO,  "Sensor init complete");
    logger_log(log, LOG_INFO,  "ADC calibration: vref=3287 mV");
    logger_log(log, LOG_WARN,  "Battery voltage: 3.51 V (low threshold)");
    logger_log(log, LOG_ERROR, "I2C bus timeout on address 0x44");
}

int main(void)
{
    printf("=== BRIDGE PATTERN — Logger with Swappable Transports ===\n");
    printf("The Logger abstraction and Transport implementation vary independently.\n\n");

    Logger sys_log;

    printf("  -- Phase 1: DEBUG logger → UART transport --\n");
    logger_init(&sys_log, LOG_DEBUG, &uart_transport);
    run_system_diagnostics(&sys_log);

    printf("\n  -- Phase 2: swap to RTT (no logger change needed) --\n");
    logger_set_transport(&sys_log, &rtt_transport);
    logger_log(&sys_log, LOG_INFO,  "Now logging over RTT");
    logger_log(&sys_log, LOG_WARN,  "Heap fragmentation: 12%%");
    sys_log.transport->flush(sys_log.transport);

    printf("\n  -- Phase 3: raise level to WARN, write to Flash --\n");
    logger_set_transport(&sys_log, &flash_transport);
    sys_log.level = LOG_WARN;   /* suppress DEBUG/INFO */
    run_system_diagnostics(&sys_log);   /* only WARN+ERROR reach flash */

    printf("\n  -- Phase 4: back to UART for final report --\n");
    logger_set_transport(&sys_log, &uart_transport);
    logger_log(&sys_log, LOG_ERROR, "System entering safe-mode");

    return 0;
}
