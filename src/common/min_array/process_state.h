#ifndef PROCESS_STATE_H_
#define PROCESS_STATE_H_

#include "atomic_int_op.h"
#include "common_macro.h"
#include "persist_cas.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * bit 0 - 15 bit is the process id
 * bit 16 - 62 bit is the counter
 * bit 63 is the "dirty" bit for distinguish whether it's flushed or not; it's not set in BYTE_GRANULAR
 */
struct process_state {
    _Atomic_uint64_t state;
};

#define COUNTER_MASK    (((1UL << 47) - 1) << 16)
#define COUNTER_INF     ((1UL << 47) - 1)
#define PROCESS_ID_MASK 0xffff

static force_inline uint64_t default_process_state(uint16_t process_id)
{
    return COUNTER_MASK | (uint64_t) process_id;
}

static force_inline uint64_t compose_process_state(uint64_t token, uint16_t process_id)
{
    return token << 16 | (uint16_t) process_id;
}

static force_inline uint64_t get_process_state(struct process_state* state)
{
    return pcas_read_uint64(&state->state);
}

static force_inline bool atomic_cmpxchg_process_state(struct process_state* state, uint64_t* expect, uint64_t set_to)
{
    return persist_cas_uint64(&state->state, expect, set_to);
}

static force_inline uint64_t get_counter(uint64_t state)
{
    return (state & COUNTER_MASK) >> 16;
}

static force_inline uint16_t get_process_id(uint64_t state)
{
    return state & PROCESS_ID_MASK;
}

#ifdef __cplusplus
}
#endif

#endif // PROCESS_STATE_H_
