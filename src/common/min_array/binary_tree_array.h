#ifndef BINARY_TREE_ARRAY_H_
#define BINARY_TREE_ARRAY_H_

#include "common_macro.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A perfect binary tree stored in an array; power of 2 leaf nodes */

/* how many leaf nodes; 2^LEAF_ORDER is the number of nodes */
#define DEFINE_BINARY_TREE(name, node_type, order) \
    struct name { \
        node_type nodes[(1 << (order + 1)) - 1]; \
    }

struct tree_pos {
    size_t index;
    size_t depth;
};

#ifdef __cplusplus
#define INVALID_TREE_POS tree_pos{ 0, 0 }
#else
#define INVALID_TREE_POS ((struct tree_pos){ 0, 0 })
#endif

/*
 * Navigation functions
 */

/* Returns a position at the root of a buddy allocation tree */
static force_inline struct tree_pos binary_tree_root(void) {
    struct tree_pos identity = { 1, 1 };
    return identity;
}

static force_inline bool tree_pos_is_root(struct tree_pos *pos)
{
    return pos->depth == 1 && pos->index == 1;
}

/* Returns the left child node position. Does not check if that is a valid position */
static force_inline struct tree_pos tree_left_child(struct tree_pos pos)
{
    pos.index *= 2;
    pos.depth++;
    return pos;
}

static force_inline void get_tree_left_child(struct tree_pos *pos, struct tree_pos *pos_out)
{
    pos_out->index = pos->index * 2;
    pos_out->depth = pos->depth + 1;
}

/* Returns the right child node position. Does not check if that is a valid position */
static force_inline struct tree_pos tree_right_child(struct tree_pos pos)
{
    pos.index *= 2;
    pos.index++;
    pos.depth++;
    return pos;
}

static force_inline void get_tree_right_child(struct tree_pos *pos, struct tree_pos *pos_out)
{
    pos_out->index = pos->index * 2;
    pos_out->index += 1;
    pos_out->depth = pos->depth + 1;
}

/* Returns the current sibling node position. Does not check if that is a valid position */
static force_inline struct tree_pos tree_sibling(struct tree_pos pos)
{
    pos.index ^= 1;
    return pos;
}

/* Returns the parent node position or an invalid position if there is no parent node */
static force_inline struct tree_pos tree_parent(struct tree_pos pos)
{
    pos.index /= 2;
    pos.depth--;
    return pos;
}

static force_inline void update_tree_parent(struct tree_pos *pos)
{
    pos->index /= 2;
    pos->depth--;
}

/* Returns the right adjacent node position or an invalid position if there is no right adjacent node */
static force_inline struct tree_pos tree_right_adjacent(struct tree_pos pos)
{
    if (((pos.index + 1) ^ pos.index) > pos.index) {
        return INVALID_TREE_POS;
    }
    pos.index++;
    return pos;
}

#ifdef __cplusplus
}
#endif

#endif // BINARY_TREE_ARRAY_H_
