/* =============================================================
 * ring_buffer.h  — PUBLIC API  (callers only see this file)
 * =============================================================
 * CONCEPT: OPAQUE PATTERN
 * -----------------------
 * The Opaque Pattern hides the internal layout of a module's
 * data structure from every file that uses it.  The public header
 * forward-declares the type (so pointers compile) but never reveals
 * the struct body.  The body lives exclusively in the .c file.
 *
 * WHY IT MATTERS IN EMBEDDED SYSTEMS
 * ------------------------------------
 * Ring (circular) buffers are used everywhere in embedded firmware:
 * ISR → task data handoff, UART RX queues, ADC sample pipelines.
 * Exposing the internal head/tail/count variables invites bugs —
 * application code might manipulate them directly and corrupt the
 * buffer.  The Opaque Pattern makes that physically impossible.
 *
 * BENEFIT: changing the internal representation (e.g. switching
 * from head+tail to head+count) never forces a recompile of any
 * file that only includes ring_buffer.h.
 * ============================================================= */
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Forward declaration only — the struct body is in ring_buffer.c */
typedef struct RingBuffer RingBuffer;

/* Lifecycle */
RingBuffer *rb_create(size_t capacity);
void        rb_destroy(RingBuffer *rb);

/* Operations */
bool    rb_push  (RingBuffer *rb, uint8_t byte);
bool    rb_pop   (RingBuffer *rb, uint8_t *out);
bool    rb_peek  (const RingBuffer *rb, uint8_t *out);
size_t  rb_count (const RingBuffer *rb);
size_t  rb_space (const RingBuffer *rb);
bool    rb_is_full (const RingBuffer *rb);
bool    rb_is_empty(const RingBuffer *rb);
void    rb_flush (RingBuffer *rb);

#endif /* RING_BUFFER_H */
