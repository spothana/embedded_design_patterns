/* =============================================================
 * ring_buffer.c — PRIVATE implementation
 * =============================================================
 * The full struct definition lives here ONLY.
 * No other translation unit can access head / tail / count /
 * buf directly — the compiler enforces this because those files
 * only ever see the forward declaration from ring_buffer.h.
 * ============================================================= */
#include "ring_buffer.h"
#include <stdlib.h>
#include <string.h>

/* Full definition — invisible outside this file */
struct RingBuffer {
    uint8_t *buf;       /* heap-allocated storage          */
    size_t   capacity;  /* maximum bytes the buffer holds  */
    size_t   head;      /* next write position             */
    size_t   tail;      /* next read position              */
    size_t   count;     /* bytes currently stored          */
};

RingBuffer *rb_create(size_t capacity)
{
    if (capacity == 0) return NULL;
    RingBuffer *rb = malloc(sizeof(RingBuffer));
    if (!rb) return NULL;
    rb->buf = malloc(capacity);
    if (!rb->buf) { free(rb); return NULL; }
    rb->capacity = capacity;
    rb->head     = 0;
    rb->tail     = 0;
    rb->count    = 0;
    return rb;
}

void rb_destroy(RingBuffer *rb)
{
    if (!rb) return;
    free(rb->buf);
    free(rb);
}

bool rb_push(RingBuffer *rb, uint8_t byte)
{
    if (rb->count == rb->capacity) return false;   /* full */
    rb->buf[rb->head] = byte;
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count++;
    return true;
}

bool rb_pop(RingBuffer *rb, uint8_t *out)
{
    if (rb->count == 0) return false;              /* empty */
    *out    = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count--;
    return true;
}

bool rb_peek(const RingBuffer *rb, uint8_t *out)
{
    if (rb->count == 0) return false;
    *out = rb->buf[rb->tail];
    return true;
}

size_t rb_count    (const RingBuffer *rb) { return rb->count; }
size_t rb_space    (const RingBuffer *rb) { return rb->capacity - rb->count; }
bool   rb_is_full  (const RingBuffer *rb) { return rb->count == rb->capacity; }
bool   rb_is_empty (const RingBuffer *rb) { return rb->count == 0; }
void   rb_flush    (RingBuffer *rb)       { rb->head = rb->tail = rb->count = 0; }
