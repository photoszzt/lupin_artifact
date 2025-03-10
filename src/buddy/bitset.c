#if defined(__KERNEL__) || defined(MODULE)
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#include "bitset.h"

static const uint8_t bitset_char_mask[8][8] = {
    {1, 3, 7, 15, 31, 63, 127, 255},
    {0, 2, 6, 14, 30, 62, 126, 254},
    {0, 0, 4, 12, 28, 60, 124, 252},
    {0, 0, 0,  8, 24, 56, 120, 248},
    {0, 0, 0,  0, 16, 48, 112, 240},
    {0, 0, 0,  0,  0, 32,  96, 224},
    {0, 0, 0,  0,  0,  0,  64, 192},
    {0, 0, 0,  0,  0,  0,   0, 128},
};

/**
 * @brief Clear a range of bits in a bitset.
 *
 * @param bitset
 * @param from_pos start position (inclusive)
 * @param to_pos end position (inclusive)
 */
struct bitset_bucket_range bitset_clear_range(unsigned char *bitset, size_t from_pos, size_t to_pos) {
    size_t from_bucket = from_pos / CHAR_BIT;
    size_t to_bucket = to_pos / CHAR_BIT;

    size_t from_index = from_pos % CHAR_BIT;
    size_t to_index = to_pos % CHAR_BIT;

    if (from_bucket == to_bucket) {
        bitset[from_bucket] &= ~bitset_char_mask[from_index][to_index];
    } else {
        bitset[from_bucket] &= ~bitset_char_mask[from_index][7];
        bitset[to_bucket] &= ~bitset_char_mask[0][to_index];
        while(++from_bucket != to_bucket) {
            bitset[from_bucket] = 0;
        }
    }
    return (struct bitset_bucket_range) {from_bucket, from_bucket};
}

/**
 * @brief Set a range of bits in a bitset.
 *
 * @param bitset
 * @param from_pos start position (inclusive)
 * @param to_pos end position (inclusive)
 */
struct bitset_bucket_range bitset_set_range(unsigned char *bitset, size_t from_pos, size_t to_pos) {
    size_t from_bucket = from_pos / CHAR_BIT;
    size_t to_bucket = to_pos / CHAR_BIT;

    size_t from_index = from_pos % CHAR_BIT;
    size_t to_index = to_pos % CHAR_BIT;

    if (from_bucket == to_bucket) {
        bitset[from_bucket] |= bitset_char_mask[from_index][to_index];
    } else {
        bitset[from_bucket] |= bitset_char_mask[from_index][7];
        bitset[to_bucket] |= bitset_char_mask[0][to_index];
        while(++from_bucket != to_bucket) {
            bitset[from_bucket] = 255u;
        }
    }
    return (struct bitset_bucket_range) {from_bucket, from_bucket};
}

size_t bitset_count_range(unsigned char *bitset, size_t from_pos, size_t to_pos) {
    size_t result;

    size_t from_bucket = from_pos / CHAR_BIT;
    size_t to_bucket = to_pos / CHAR_BIT;

    size_t from_index = from_pos % CHAR_BIT;
    size_t to_index = to_pos % CHAR_BIT;

    if (from_bucket == to_bucket) {
        return popcount_byte(bitset[from_bucket] & bitset_char_mask[from_index][to_index]);
    }

    result = popcount_byte(bitset[from_bucket] & bitset_char_mask[from_index][7])
        + popcount_byte(bitset[to_bucket]  & bitset_char_mask[0][to_index]);
    while(++from_bucket != to_bucket) {
        result += popcount_byte(bitset[from_bucket]);
    }
    return result;
}

void bitset_shift_left(unsigned char *bitset, size_t from_pos, size_t to_pos, size_t by) {
    size_t length = to_pos - from_pos;
    size_t i = 0;
    for(i = 0; i < length; i++) {
        size_t at = from_pos + i;
        if (bitset_test(bitset, at)) {
            bitset_set(bitset, at-by);
        } else {
            bitset_clear(bitset, at-by);
        }
    }
    bitset_clear_range(bitset, length, length+by-1);

}

void bitset_shift_right(unsigned char *bitset, size_t from_pos, size_t to_pos, size_t by) {
    ssize_t length = (ssize_t) to_pos - (ssize_t) from_pos;
    while (length >= 0) {
        size_t at = from_pos + (size_t) length;
        if (bitset_test(bitset, at)) {
            bitset_set(bitset, at+by);
        } else {
            bitset_clear(bitset, at+by);
        }
        length -= 1;
    }
    bitset_clear_range(bitset, from_pos, from_pos+by-1);
}
