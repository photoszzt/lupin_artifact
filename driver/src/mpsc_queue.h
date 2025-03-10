#ifndef MPSC_H_
#define MPSC_H_
// https://github.com/dbittman/waitfree-mpsc-queue/tree/master

#include "common_macro.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mpscq;
extern const size_t mpscq_size;
#define MPSC_QUEUE_SIZE 32
void init_mpscq(struct mpscq* q);
bool mpscq_enqueue(struct mpscq* q, s64 data) WARN_UNUSED_RESULT;
s64 mpscq_dequeue(struct mpscq* q) WARN_UNUSED_RESULT;
size_t mpscq_count(struct mpscq* q) WARN_UNUSED_RESULT;
size_t mpscq_capacity(struct mpscq *q) WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif

#endif // MPSC_H_
