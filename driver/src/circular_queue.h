#ifndef _CIRCULAR_QUEUE_H
#define _CIRCULAR_QUEUE_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wsign-conversion"

#include <linux/types.h>
#include <linux/build_bug.h>

#pragma GCC diagnostic pop

struct circular_queue;

extern const size_t circular_queue_size;

#define CQUEUE_SIZE 128
void init_circular_queue(struct circular_queue *q);
int circular_queue_enqueue(struct circular_queue *q, uint64_t data, uint16_t proc_id);
int circular_queue_dequeue(struct circular_queue *q, uint64_t *data, uint16_t proc_id);
void drain_circular_queue(struct circular_queue* cq, uint16_t machine_id);

#endif // _CIRCULAR_QUEUE_H
