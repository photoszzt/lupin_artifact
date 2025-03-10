#ifndef BITSET_H_
#define BITSET_H_
#if defined (__KERNEL__) || defined(MODULE)
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#include "common_macro.h"

#ifndef CHAR_BIT
#  define CHAR_BIT 8  /* Normally in <limits.h> */
#endif

struct bitset_bucket_range {
    size_t from;
    size_t to;
};

static const unsigned char popcount_lookup[256] = {
    0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
    1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8
};

static force_inline unsigned int popcount_byte(unsigned char b) {
    return popcount_lookup[b];
}

/*
 * A char-backed bitset implementation
 */

static force_inline size_t bitset_sizeof(size_t elements) {
    return ((elements) + CHAR_BIT - 1u) / CHAR_BIT;
}

static uint8_t bitset_index_mask[8] = {1, 2, 4, 8, 16, 32, 64, 128};

static force_inline void bitset_set(unsigned char *bitset, size_t pos) {
    size_t bucket = pos / CHAR_BIT;
    size_t index = pos % CHAR_BIT;
    bitset[bucket] |= bitset_index_mask[index];
}

static force_inline void bitset_clear(unsigned char *bitset, size_t pos) {
    size_t bucket = pos / CHAR_BIT;
    size_t index = pos % CHAR_BIT;
    bitset[bucket] &= ~bitset_index_mask[index];
}

static force_inline unsigned int bitset_test(const unsigned char *bitset, size_t pos) {
    size_t bucket = pos / CHAR_BIT;
    size_t index = pos % CHAR_BIT;
    return bitset[bucket] & bitset_index_mask[index];
}
struct bitset_bucket_range bitset_clear_range(unsigned char *bitset, size_t from_pos, size_t to_pos);
struct bitset_bucket_range bitset_set_range(unsigned char *bitset, size_t from_pos, size_t to_pos);
size_t bitset_count_range(unsigned char *bitset, size_t from_pos, size_t to_pos);
void bitset_shift_left(unsigned char *bitset, size_t from_pos, size_t to_pos, size_t by);
void bitset_shift_right(unsigned char *bitset, size_t from_pos, size_t to_pos, size_t by);

#endif // BITSET_H_
