/* =============================================================
 * PATTERN 10 — SPINLOCK
 * =============================================================
 * CONCEPT
 * -------
 * A spinlock is a busy-wait synchronisation primitive.  A thread
 * trying to acquire a held lock loops continuously ("spins") until
 * the lock is released.  Unlike a mutex, the thread never sleeps.
 *
 * CRITICAL RULE: lock → critical section → unlock.
 * NEVER return or longjmp out of a critical section!
 *
 * WHY IT MATTERS IN EMBEDDED SYSTEMS
 * ------------------------------------
 * On bare-metal or in ISR context, you cannot sleep/block.
 * Spinlocks protect very short critical sections accessed from
 * both ISR and task context, such as:
 *   • DMA transfer queue shared between DMA-complete ISR and app
 *   • Ring buffer head/tail update in UART ISR
 *   • CAN mailbox flag between CAN ISR and protocol task
 *
 * WHEN TO PREFER SPINLOCK OVER MUTEX
 * ------------------------------------
 *   ✓ Critical section is < ~10 instructions
 *   ✓ Used from ISR context (cannot use a sleeping mutex in ISR)
 *   ✗ Long critical sections (wastes CPU — use mutex instead)
 *   ✗ Recursive locking (spinlocks are not reentrant)
 *
 * EXAMPLE SCENARIO
 * ----------------
 * A DMA transfer queue holds pending SPI DMA jobs.  The DMA-complete
 * ISR enqueues the next job; the application thread dequeues.
 * Without a spinlock the queue head/tail could be corrupted by
 * interleaved ISR and thread access.
 *
 * NOTE: We use __sync_lock_test_and_set (GCC atomic builtin).
 * On Cortex-M you would use LDREX/STREX or __disable_irq/__enable_irq.
 * ============================================================= */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── Spinlock ──────────────────────────────────────────────── */
typedef struct {
    volatile int locked;
} Spinlock;

static inline void spinlock_init   (Spinlock *sl) { sl->locked = 0; }

static inline void spinlock_acquire(Spinlock *sl)
{
    /*
     * __sync_lock_test_and_set atomically:
     *   temp = sl->locked;
     *   sl->locked = 1;
     *   return temp;
     * We loop while it was already 1 (held by someone else).
     */
    while (__sync_lock_test_and_set(&sl->locked, 1))
        ;  /* spin — on real MCU: add NOP or WFE for power */
}

static inline void spinlock_release(Spinlock *sl)
{
    __sync_lock_release(&sl->locked);   /* atomic store of 0 */
}

/* ── DMA transfer job ──────────────────────────────────────── */
typedef struct {
    uint32_t    src_addr;
    uint32_t    dst_addr;
    uint16_t    length;
    uint8_t     channel;
    const char *label;
} DmaJob;

/* ── Lock-protected circular job queue ─────────────────────── */
#define QUEUE_SIZE 8

typedef struct {
    DmaJob   jobs[QUEUE_SIZE];
    int      head;
    int      tail;
    int      count;
    Spinlock lock;
    uint32_t enqueue_calls;
    uint32_t dequeue_calls;
    uint32_t dropped;
} DmaQueue;

static DmaQueue dma_q;

static void dma_queue_init(DmaQueue *q)
{
    memset(q, 0, sizeof(*q));
    spinlock_init(&q->lock);
}

/* Called from ISR — must be fast, no sleeping */
static bool dma_enqueue(DmaQueue *q, DmaJob job)
{
    spinlock_acquire(&q->lock);

    bool ok = false;
    if (q->count < QUEUE_SIZE) {
        q->jobs[q->head] = job;
        q->head = (q->head + 1) % QUEUE_SIZE;
        q->count++;
        ok = true;
    } else {
        q->dropped++;
    }
    q->enqueue_calls++;

    spinlock_release(&q->lock);
    return ok;
}

/* Called from application task */
static bool dma_dequeue(DmaQueue *q, DmaJob *out)
{
    spinlock_acquire(&q->lock);

    bool ok = false;
    if (q->count > 0) {
        *out    = q->jobs[q->tail];
        q->tail = (q->tail + 1) % QUEUE_SIZE;
        q->count--;
        ok = true;
    }
    q->dequeue_calls++;

    spinlock_release(&q->lock);
    return ok;
}

/* ── Simulate ISR enqueueing DMA jobs ──────────────────────── */
static void simulate_isr_batch(const char *scenario, DmaJob *jobs, int n)
{
    printf("\n  [ISR] %s — enqueueing %d jobs:\n", scenario, n);
    for (int i = 0; i < n; i++) {
        bool ok = dma_enqueue(&dma_q, jobs[i]);
        printf("    enqueue [%s] → %s (queue depth=%d)\n",
               jobs[i].label, ok ? "OK" : "DROPPED", dma_q.count);
    }
}

/* ── Application drains the queue ──────────────────────────── */
static void app_drain_queue(void)
{
    printf("\n  [App]  Draining DMA queue:\n");
    DmaJob job;
    while (dma_dequeue(&dma_q, &job)) {
        printf("    execute DMA: ch%u  0x%08X→0x%08X  len=%u  [%s]\n",
               job.channel, job.src_addr, job.dst_addr,
               job.length, job.label);
    }
    printf("    Queue empty.\n");
}

int main(void)
{
    printf("=== SPINLOCK — DMA Transfer Queue ===\n");
    printf("ISR and App share the queue; spinlock prevents corruption.\n");

    dma_queue_init(&dma_q);

    /* Batch 1: ADC DMA → RAM */
    DmaJob adc_jobs[] = {
        { 0x40012400, 0x20001000, 64,  0, "ADC1_CH0→RAM" },
        { 0x40012404, 0x20001040, 64,  0, "ADC1_CH1→RAM" },
        { 0x40012408, 0x20001080, 128, 0, "ADC1_CH2→RAM" },
    };
    simulate_isr_batch("ADC complete", adc_jobs, 3);
    app_drain_queue();

    /* Batch 2: fill queue beyond capacity */
    DmaJob spi_jobs[] = {
        { 0x20002000, 0x40013800, 32, 1, "TX_frame_1" },
        { 0x20002020, 0x40013800, 32, 1, "TX_frame_2" },
        { 0x20002040, 0x40013800, 32, 1, "TX_frame_3" },
        { 0x20002060, 0x40013800, 32, 1, "TX_frame_4" },
        { 0x20002080, 0x40013800, 32, 1, "TX_frame_5" },
        { 0x200020A0, 0x40013800, 32, 1, "TX_frame_6" },
        { 0x200020C0, 0x40013800, 32, 1, "TX_frame_7" },
        { 0x200020E0, 0x40013800, 32, 1, "TX_frame_8" },
        { 0x20002100, 0x40013800, 32, 1, "TX_frame_9" }, /* should drop */
        { 0x20002120, 0x40013800, 32, 1, "TX_frame_10"},  /* should drop */
    };
    simulate_isr_batch("SPI TX burst (10 jobs, queue fits 8)", spi_jobs, 10);
    app_drain_queue();

    printf("\n  Statistics:\n");
    printf("    Total enqueues:  %u\n", dma_q.enqueue_calls);
    printf("    Total dequeues:  %u\n", dma_q.dequeue_calls);
    printf("    Dropped (full):  %u\n", dma_q.dropped);

    return 0;
}
