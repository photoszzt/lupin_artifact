// following: pmdk/src/examples/libpmem2/ringbuf/ringbuf.c
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wsign-conversion"

#include <linux/types.h>
#include <linux/compiler.h>
#pragma GCC diagnostic pop

#include "output_cacheline.h"
#include "circular_queue.h"
#include "atomic_int_op.h"
#include "uapi/vcxl_udef.h"
#include "vcxl_def.h"
#include "common_macro.h"

#define RINGBUF_POS_PERSIST_BIT (1ULL << 63)

// #define JJ_AB_MAX_PROC_SHIFT 6
// #include "jj_ab_spin/jj_abortable_spinlock.h"
// #define wsize 64
// #include "jj_ab_spin/jj_ab_wport_spinlock.h"
#include "tatas/tatas_spinlock.h"

struct pos {
    _Atomic_uint64_t read;
    _Atomic_uint64_t write;
};

/* Cache line size is 64 bytes */
struct circular_queue {
    struct pos pos;
    // struct jj_wport_spin(wsize) lock;
    struct ttas lock;
    uint64_t data[CQUEUE_SIZE];
};
const size_t circular_queue_size = sizeof(struct circular_queue);

static force_inline void cq_lock_init(struct circular_queue* cq)
{
//	jj_wport_spin_init_f(wsize)(&cq->lock);
	ttas_init(&cq->lock);
}

static force_inline unsigned long cq_lock_irqsave(struct circular_queue* cq, uint16_t machine_id)
{
	unsigned long flags = 0;
	// jj_wport_spin_lock_irqsave(&cq->lock, machine_id, flags);
	ttas_lock_irqsave(&cq->lock, machine_id, flags);
	return flags;
}

static force_inline void cq_unlock_irqrestore(struct circular_queue* cq, uint16_t machine_id, unsigned long flags)
{
	// jj_wport_spin_unlock_irqrestore_f(wsize)(&cq->lock, machine_id, flags);
	ttas_unlock_irqrestore(&cq->lock, machine_id, flags);
}

void drain_circular_queue(struct circular_queue* cq, uint16_t machine_id)
{
	uint64_t data;
	unsigned long flags = cq_lock_irqsave(cq, machine_id);
	while (circular_queue_dequeue(cq, &data, machine_id) != -1) {
	}
	cq_unlock_irqrestore(cq, machine_id, flags);
}

_Static_assert(VCXL_MAX_SUPPORTED_MACHINES * sizeof(struct circular_queue) <= FUTEX_ADDR_QUEUE_SIZE,
	"vcxl queue size larger than reservation");

void init_circular_queue(struct circular_queue *q)
{
	if (q == NULL) {
		return;
	}
	store_uint64_release(&q->pos.read, 0);
	store_uint64_release(&q->pos.write, 0);
	cq_lock_init(q);
	memset(&q->data[0], 0, sizeof(uint64_t) * CQUEUE_SIZE);
    shmem_output_cacheline(q, sizeof(struct circular_queue));
}

/*
 * circular_queue_store_position -- atomically updates a circular queue position
 */
static void
circular_queue_store_position(_Atomic_uint64_t *pos, uint64_t val)
{
	/*
	 * Ordinarily, an atomic store becomes globally visible prior to being
	 * persistent. This is a problem since applications have to make sure
	 * that they never make progress on data that isn't yet persistent.
	 * In this example, this is addressed by using the MSB of the value as a
	 * "possibly-not-yet-persistent flag".
	 * First, a value is stored with that flag set, persisted, and then
	 * stored again with the flag cleared. Any threads that load
	 * that variable need to first check the flag and, if set, persist it
	 * before proceeding.
	 * This ensures that the loaded variable is always persistent.
	 *
	 * However, if the map can be persistently written to with byte
	 * granularity (i.e., the system is eADR equipped), then data
	 * visibility is the same as data persistence. This eliminates the
	 * need for the persistent flag algorithm.
	 */
	// if (rbuf->granularity == PMEM2_GRANULARITY_BYTE) {
	// 	__atomic_store_n(pos, val, __ATOMIC_RELEASE);
	// } else {
        /*
        __atomic_store_n(pos, val | RINGBUF_POS_PERSIST_BIT,
				__ATOMIC_RELEASE);
		rbuf->persist(pos, sizeof(val));
        */
#if defined(BYTE_GRANULAR) || defined(DRAM)
	store_uint64_release(pos, val);
#elif defined(CACHELINE_GRANULAR)
	store_uint64_release(pos, val | RINGBUF_POS_PERSIST_BIT);
    shmem_output_cacheline(pos, sizeof(val));

	store_uint64_release(pos, val);
    shmem_output_cacheline(pos, sizeof(val));
#else
#error "unrecognize protection granularity"
#endif
	// __atomic_store_n(pos, val, __ATOMIC_RELEASE);
	// rbuf->persist(pos, sizeof(val));
	// }
}

/*
 * curcular_queue_load_position -- atomically loads the circular queue positions
 */
static void
circular_queue_load_position(const struct circular_queue *c,
	uint64_t *read, uint64_t *write)
{
	uint64_t w;
	uint64_t r;

	w = load_uint64_acquire(&c->pos.write);
	r = load_uint64_acquire(&c->pos.read);

	/* on systems with byte store granularity, this will never be true */
	if (w & RINGBUF_POS_PERSIST_BIT || r & RINGBUF_POS_PERSIST_BIT) {
		/*
		 * We could store the value with the persist bit cleared,
		 * helping other threads make progress. But, in this case,
		 * it's likely that the coordination required to do that safely
		 * would be more costly than the current approach.
		 */
        shmem_output_cacheline(&c->pos, sizeof(c->pos));
		w &= ~RINGBUF_POS_PERSIST_BIT;
		r &= ~RINGBUF_POS_PERSIST_BIT;
	}

	*read = r;
	*write = w;
}

/*
 * circular_queue_enqueue -- atomically appends a new entry to the circular queue.
 * This function fails if the circular queue is full.
 */
int circular_queue_enqueue(struct circular_queue *q, uint64_t data, uint16_t proc_id)
{
    uint64_t r, w, next_w;
	unsigned long flags;

	// spin_lock_irqsave(&q->lock, flags);
	flags = cq_lock_irqsave(q, proc_id);
    circular_queue_load_position(q, &r, &w);
    next_w = (w + 1) % CQUEUE_SIZE;
    if (next_w == r) {
		cq_unlock_irqrestore(q, proc_id, flags);
        return -1;
    }
    q->data[w] = data;
	shmem_output_cacheline(&q->data[w], sizeof(uint64_t));
    circular_queue_store_position(&q->pos.write, next_w);
	cq_unlock_irqrestore(q, proc_id, flags);
    return 0;
}

/*
 * circular_queue_dequeue -- atomically removes one entry from the circular queue.
 * This function fails if the circular queue is empty.
 */
int circular_queue_dequeue(struct circular_queue *q, uint64_t *data, uint16_t proc_id)
{
    uint64_t r, w;
	unsigned long flags;

	flags = cq_lock_irqsave(q, proc_id);
    circular_queue_load_position(q, &r, &w);
    if (w == r) { /* empty */
		cq_unlock_irqrestore(q, proc_id, flags);
        return -1;
    }
    *data = q->data[r];
	circular_queue_store_position(&q->pos.read, (r + 1) % CQUEUE_SIZE);
	cq_unlock_irqrestore(q, proc_id, flags);
    return 0;
}
