#ifndef CXL_ADDR_H
#define CXL_ADDR_H
#include <linux/overflow.h>

#ifdef __kernel___ || MODULE
#include <linux/types.h>
#else
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#define __always_inline __attribute__((always_inline))
# define unlikely(x)	__builtin_expect(!!(x), 0)
#endif


__always_inline uint64_t align_down(uint64_t addr, uint64_t alignment) {
#ifdef __kernel___ || MODULE
    BUG_ON(alignment != 0 && !(alignment & (alignment - 1)));
#else
    if (alignment != 0 && !(alignment & (alignment - 1))) {
        fprintf(stderr, "Invalid alignment: %lu\n", alignment);
        abort();
    }
#endif
    return addr & ~(alignment - 1);
}

__always_inline uint64_t align_up(uint64_t addr, uint64_t alignment) {
#ifdef __kernel___ || MODULE
    BUG_ON(alignment != 0 && !(alignment & (alignment - 1)));
#else
    if (alignment != 0 && !(alignment & (alignment - 1))) {
        fprintf(stderr, "Invalid alignment: %lu\n", alignment);
        abort();
    }
#endif
    uint64_t mask = alignment - 1;
    if (addr & mask == 0) {
        // already aligned
        return addr;
    }
    uint64_t aligned;
    uint64_t tmp = addr | mask;
    uint64_t tmp2 = 1;
    if (unlikely(check_add_overflow(tmp, tmp2, &aligned))) {
#ifdef __kernel___ || MODULE
        BUG();
#else
        fprintf(stderr, "Overflow in align_up\n");
        abort();
#endif
    }
    return aligned;
}

#endif // CXL_ADDR_H