/* =============================================================
 * PATTERN 03 — OPAQUE PATTERN
 * =============================================================
 * CONCEPT
 * -------
 * The Opaque Pattern hides a struct's internals behind a pointer.
 * The public header only forward-declares "typedef struct Foo Foo;".
 * The actual struct body lives only in the .c file.
 *
 * WHY IT MATTERS IN EMBEDDED SYSTEMS
 * ------------------------------------
 * Embedded modules (ring buffers, FIFOs, driver handles) must
 * not have their internals poked from outside.  This pattern gives
 * us C-level enforcement: the compiler rejects any attempt to
 * dereference the opaque pointer because the size is unknown.
 *
 * TRY ADDING THIS TO main() — it will NOT compile:
 *   uart_rx->count = 99;   // error: dereferencing pointer to incomplete type
 *
 * EXAMPLE SCENARIO
 * ----------------
 * A UART interrupt handler pushes received bytes into a ring buffer.
 * The application reads them out one at a time.  The ring buffer's
 * internal head/tail/count are completely hidden.
 *
 * FILES
 * -----
 *   ring_buffer.h  — public API   (only forward declaration of struct)
 *   ring_buffer.c  — private impl (struct body lives here)
 *   main.c         — user code    (cannot access internals)
 * ============================================================= */

#include <stdio.h>
#include <string.h>
#include "ring_buffer.h"

/* Simulate a UART ISR pushing bytes into the buffer */
static void simulate_uart_isr(RingBuffer *rx, const char *incoming)
{
    printf("  [ISR] receiving: \"%s\"\n", incoming);
    for (size_t i = 0; i < strlen(incoming); i++) {
        if (!rb_push(rx, (uint8_t)incoming[i]))
            printf("  [ISR] OVERFLOW — byte '%c' dropped!\n", incoming[i]);
    }
}

int main(void)
{
    printf("=== OPAQUE PATTERN — UART RX Ring Buffer ===\n");
    printf("main() can call rb_push/pop/count but CANNOT touch\n");
    printf("rb->head, rb->tail, or rb->buf — compiler forbids it.\n\n");

    /* Create a small 8-byte RX FIFO */
    RingBuffer *uart_rx = rb_create(8);

    printf("  Buffer capacity=8, count=%zu, space=%zu\n\n",
           rb_count(uart_rx), rb_space(uart_rx));

    /* Simulate ISR feeding data */
    simulate_uart_isr(uart_rx, "Hello!");
    printf("  After ISR:  count=%zu, full=%s\n\n",
           rb_count(uart_rx), rb_is_full(uart_rx) ? "YES" : "no");

    /* Application reads bytes */
    printf("  Application draining buffer:\n  [APP] \"");
    uint8_t byte;
    while (rb_pop(uart_rx, &byte))
        putchar((char)byte);
    printf("\"\n\n");

    /* Demonstrate overflow protection */
    simulate_uart_isr(uart_rx, "Overflow_Test!!");  /* 16 bytes into 8-slot buf */
    printf("  Drained: \"");
    while (rb_pop(uart_rx, &byte))
        putchar((char)byte);
    printf("\" (only 8 bytes survived)\n\n");

    /* Flush */
    simulate_uart_isr(uart_rx, "ABCD");
    printf("  Before flush: count=%zu\n", rb_count(uart_rx));
    rb_flush(uart_rx);
    printf("  After  flush: count=%zu\n", rb_count(uart_rx));

    rb_destroy(uart_rx);
    return 0;
}
