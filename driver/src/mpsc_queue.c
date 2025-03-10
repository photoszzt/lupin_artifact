#include <linux/types.h>
#include <linux/atomic.h>
#include "mpsc_queue.h"
#include "atomic_int_op.h"
#include "output_cacheline.h"


struct mpscq {
    atomic64_t buffer[MPSC_QUEUE_SIZE];
    atomic64_t count;
    atomic64_t head;
    uint64_t tail;
};
_Static_assert(sizeof(struct mpscq) == 280, "mpsc queue size is not 280");
const size_t mpscq_size = sizeof(struct mpscq);

void init_mpscq(struct mpscq* q)
{
    int i;
    if (q == NULL) {
        pr_err("passed in mpscq is nullptr\n");
        return;
    }
    q->count = (atomic64_t)ATOMIC64_INIT(0);
    q->head = (atomic64_t)ATOMIC64_INIT(0);
    q->tail = 0;
    for (i = 0; i < MPSC_QUEUE_SIZE; i++) {
        q->buffer[i] = (atomic64_t)ATOMIC64_INIT(-1);
    }
    shmem_output_cacheline(q, sizeof(struct mpscq));
}

bool mpscq_enqueue(struct mpscq* q, s64 data)
{
    s64 head;
    s64 count = atomic64_fetch_add(1, &q->count);
#ifdef CACHELINE_GRANULAR
    shmem_output_cacheline(&q->count, sizeof(s64));
#endif
    if (count >= MPSC_QUEUE_SIZE) {
        atomic64_fetch_sub(1, &q->count);
#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(&q->count, sizeof(s64));
#endif
        return false;
    }
    /* increment the head, which gives us 'exclusive' access to that elem */
    head = atomic64_fetch_add(1, &q->head);
#ifdef CACHELINE_GRANULAR
    shmem_output_cacheline(&q->head, sizeof(s64));
#endif
    atomic64_xchg(&q->buffer[head % MPSC_QUEUE_SIZE], data);
#ifdef CACHELINE_GRANULAR
    shmem_output_cacheline(&q->buffer[head % MPSC_QUEUE_SIZE], sizeof(s64));
#endif
    return true;
}

s64 mpscq_dequeue(struct mpscq* q)
{
    s64 ret = atomic64_xchg(&q->buffer[q->tail], -1);
#ifdef CACHELINE_GRANULAR
    shmem_output_cacheline(&q->buffer[q->tail], sizeof(s64));
#endif
    if (ret == -1) {
        return -1;
    }
    if (++q->tail >= MPSC_QUEUE_SIZE) {
        q->tail = 0;
#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(&q->tail, sizeof(s64));
#endif
    }
    atomic64_fetch_sub(1, &q->count);
#ifdef CACHELINE_GRANULAR
    shmem_output_cacheline(&q->count, sizeof(s64));
#endif
    return ret;
}

size_t inline mpscq_count(struct mpscq* q)
{
    return atomic64_read_acquire(&q->count);
}

size_t inline mpscq_capacity(struct mpscq *q)
{
    return MPSC_QUEUE_SIZE;
}
