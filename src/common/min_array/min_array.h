#ifndef MIN_ARRAY_H_
#define MIN_ARRAY_H_

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#if defined(__KERNEL__) || defined(MODULE)
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#pragma GCC diagnostic pop
#include "binary_tree_array.h"
#include "atomic_int_op.h"
#include "common_macro.h"
#include "process_state.h"
#include "template.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MINARR_MAX_PROCESS_SHIFT
#error "need to define MINARR_MAX_PROCESS_SHIFT"
#endif

static const size_t MINARR_MAX_NUM_PROCESSES = ((size_t)1 << MINARR_MAX_PROCESS_SHIFT);
static const size_t NUM_ARR_NODES = (((size_t)1 << (MINARR_MAX_PROCESS_SHIFT + 1)) - 1);

#define min_array(max_proc_shift) struct JOIN(min_array, max_proc_shift)
min_array(MINARR_MAX_PROCESS_SHIFT) {
    DEFINE_BINARY_TREE(min_arr_tree, struct process_state, MINARR_MAX_PROCESS_SHIFT) min_arr_tree;
};

#define find_min_f(max_num_proc) JOIN(find_min, max_num_proc)
static force_inline uint64_t find_min_f(MINARR_MAX_PROCESS_SHIFT)(min_array(MINARR_MAX_PROCESS_SHIFT) *min_array)
{
    return (uint64_t)load_uint64_acquire(&min_array->min_arr_tree.nodes[0].state);
}

#define set_leaf_f(max_num_proc) JOIN(set_leaf, max_num_proc)
static force_inline void set_leaf_f(MINARR_MAX_PROCESS_SHIFT)(min_array(MINARR_MAX_PROCESS_SHIFT) *min_array, uint16_t process_id, uint64_t set_to)
{
    // PRINT_INFO("get_leaf arr idx: %d for id %u\n", NUM_NODES-MINARR_MAX_NUM_PROCESSES+process_id, process_id);
    store_uint64_release(&min_array->min_arr_tree.nodes[NUM_ARR_NODES-MINARR_MAX_NUM_PROCESSES+process_id].state, set_to);
}

#define get_leaf_arr_f(max_num_proc) JOIN(get_leaf_arr, max_num_proc)
static force_inline struct process_state* get_leaf_arr_f(MINARR_MAX_PROCESS_SHIFT)(min_array(MINARR_MAX_PROCESS_SHIFT) *min_array)
{
    return &min_array->min_arr_tree.nodes[NUM_ARR_NODES-MINARR_MAX_NUM_PROCESSES];
}

#define get_leaf_tree_pos_f(max_num_proc) JOIN(get_leaf_tree_pos, max_num_proc)
static force_inline void get_leaf_tree_pos_f(MINARR_MAX_PROCESS_SHIFT)(uint16_t process_id, struct tree_pos *pos)
{

    pos->index = (size_t)process_id + NUM_ARR_NODES - MINARR_MAX_NUM_PROCESSES + 1;
    pos->depth = MINARR_MAX_PROCESS_SHIFT + 1;
}

#define min_arr_update_f(max_num_proc) JOIN(min_arr_update, max_num_proc)
#define init_min_array_f(max_num_proc) JOIN(init_min_array, max_num_proc)
#define refresh_f(max_num_proc) JOIN(refresh, max_num_proc)
void min_arr_update_f(MINARR_MAX_PROCESS_SHIFT)(min_array(MINARR_MAX_PROCESS_SHIFT) *min_arr, uint16_t process_id, uint64_t state);

static inline void init_min_array_f(MINARR_MAX_PROCESS_SHIFT)(min_array(MINARR_MAX_PROCESS_SHIFT) *min_arr)
{
    uint16_t i;
    for (i = 0; i < (uint16_t)MINARR_MAX_NUM_PROCESSES; i++) {
        min_arr_update_f(MINARR_MAX_PROCESS_SHIFT)(min_arr, i, default_process_state(i));
    }
}

#ifdef __cplusplus
}
#endif

#endif // MIN_ARRAY_H_

#ifdef MIN_ARR_IMPL
#undef MIN_ARR_IMPL

#ifdef __cplusplus
extern "C" {
#endif

static void refresh_f(MINARR_MAX_PROCESS_SHIFT)(min_array(MINARR_MAX_PROCESS_SHIFT) *min_arr, struct tree_pos *pos)
{
    struct tree_pos left_child_pos;
    struct tree_pos right_child_pos;
    uint64_t cur_val, left_child_val, right_child_val, min_val;

    get_tree_left_child(pos, &left_child_pos);
    get_tree_right_child(pos, &right_child_pos);
    cur_val = get_process_state(&min_arr->min_arr_tree.nodes[pos->index-1]);
    left_child_val = get_process_state(&min_arr->min_arr_tree.nodes[left_child_pos.index-1]);
    right_child_val = get_process_state(&min_arr->min_arr_tree.nodes[right_child_pos.index-1]);
    min_val = MIN(left_child_val, right_child_val);
    atomic_cmpxchg_process_state(&min_arr->min_arr_tree.nodes[pos->index-1], &cur_val, min_val);
}

void min_arr_update_f(MINARR_MAX_PROCESS_SHIFT)(min_array(MINARR_MAX_PROCESS_SHIFT) *min_arr, uint16_t process_id, uint64_t state)
{
    struct tree_pos pos;
    get_leaf_tree_pos_f(MINARR_MAX_PROCESS_SHIFT)(process_id, &pos);
    set_leaf_f(MINARR_MAX_PROCESS_SHIFT)(min_arr, process_id, state);
    // PRINT_INFO("leaf node pos for %u: index: %lu, depth: %lu, update state to %lu, min_arr leaf: %lu\n",
    //     process_id, pos.index, pos.depth, state, min_arr->min_arr_tree.nodes[pos.index-1].state);
    while (!tree_pos_is_root(&pos)) {
        update_tree_parent(&pos);
        // PRINT_INFO("parent node: %lu\n", pos.index);
        refresh_f(MINARR_MAX_PROCESS_SHIFT)(min_arr, &pos);
        refresh_f(MINARR_MAX_PROCESS_SHIFT)(min_arr, &pos);
        // PRINT_INFO("%lx\n", min_arr->min_arr_tree.nodes[pos.index-1].state);
    }
}

#ifdef __cplusplus
}
#endif

#endif // MIN_ARR_IMPL
