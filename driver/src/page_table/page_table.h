#ifndef CXL_PERSISTENT_PAGE_TABLE_H
#define CXL_PERSISTENT_PAGE_TABLE_H 1

#ifdef __kernel___ || MODULE
#include <linux/types.h>
#include <linux/mm.h>
#else
#include <stdint.h>
#define PAGE_SHIFT 12
#endif

typedef uint64_t pteval_t;

#define _PAGE_BIT_PRESENT	0	/* is present */
#define _PAGE_BIT_RW		1	/* writeable */
#define _PAGE_BIT_USER		2	/* userspace addressable */
#define _PAGE_BIT_PWT		3	/* page write through */
#define _PAGE_BIT_PCD		4	/* page cache disabled */
#define _PAGE_BIT_ACCESSED	5	/* was accessed (raised by CPU) */
#define _PAGE_BIT_DIRTY		6	/* was written to (raised by CPU) */
#define _PAGE_BIT_PSE		7	/* 4 MB (or 2MB) page */
#define _PAGE_BIT_PAT		7	/* on 4KB pages */
#define _PAGE_BIT_GLOBAL	8	/* Global TLB entry PPro+ */
#define _PAGE_BIT_SOFTW1	9	/* available for programmer */
#define _PAGE_BIT_SOFTW2	10	/* " */
#define _PAGE_BIT_SOFTW3	11	/* " */
#define _PAGE_BIT_PAT_LARGE	12	/* On 2MB or 1GB pages */
#define _PAGE_BIT_SOFTW4	58	/* available for programmer */
#define _PAGE_BIT_PKEY_BIT0	59	/* Protection Keys, bit 1/4 */
#define _PAGE_BIT_PKEY_BIT1	60	/* Protection Keys, bit 2/4 */
#define _PAGE_BIT_PKEY_BIT2	61	/* Protection Keys, bit 3/4 */
#define _PAGE_BIT_PKEY_BIT3	62	/* Protection Keys, bit 4/4 */
#define _PAGE_BIT_NX		63	/* No execute: only valid after cpuid check */

#define _PAGE_BIT_SOFT5     59 /* available for programmer */
#define _PAGE_BIT_SOFT6     60 /* " */
#define _PAGE_BIT_SOFT7     61 /* " */
#define _PAGE_BIT_SOFT8     62 /* " */

#define _PAGE_PRESENT	((pteval_t)1UL << _PAGE_BIT_PRESENT)
#define _PAGE_RW        ((pteval_t)1UL << _PAGE_BIT_RW)
#define _PAGE_USER      ((pteval_t)1UL << _PAGE_BIT_USER)
#define _PAGE_PWT       ((pteval_t)1UL << _PAGE_BIT_PWT)
#define _PAGE_PCD       ((pteval_t)1UL << _PAGE_BIT_PCD)
#define _PAGE_ACCESSED  ((pteval_t)1UL << _PAGE_BIT_ACCESSED)
#define _PAGE_DIRTY     ((pteval_t)1UL << _PAGE_BIT_DIRTY)
#define _PAGE_PSE       ((pteval_t)1UL << _PAGE_BIT_PSE)
#define _PAGE_PAT       ((pteval_t)1UL << _PAGE_BIT_PAT)
#define _PAGE_GLOBAL    ((pteval_t)1UL << _PAGE_BIT_GLOBAL)
#define _PAGE_SOFTW1    ((pteval_t)1UL << _PAGE_BIT_SOFTW1)
#define _PAGE_SOFTW2    ((pteval_t)1UL << _PAGE_BIT_SOFTW2)
#define _PAGE_SOFTW3    ((pteval_t)1UL << _PAGE_BIT_SOFTW3)
#define _PAGE_PAT_LARGE ((pteval_t)1UL << _PAGE_BIT_PAT_LARGE)
#define _PAGE_SOFTW4    ((pteval_t)1UL << _PAGE_BIT_SOFTW4)
#define _PAGE_PKEY_BIT0 ((pteval_t)1UL << _PAGE_BIT_PKEY_BIT0)
#define _PAGE_PKEY_BIT1 ((pteval_t)1UL << _PAGE_BIT_PKEY_BIT1)
#define _PAGE_PKEY_BIT2 ((pteval_t)1UL << _PAGE_BIT_PKEY_BIT2)
#define _PAGE_PKEY_BIT3 ((pteval_t)1UL << _PAGE_BIT_PKEY_BIT3)
#define _PAGE_NX        ((pteval_t)1UL << _PAGE_BIT_NX)
#define _PAGE_SOFT5     ((pteval_t)1UL << _PAGE_BIT_SOFT5)
#define _PAGE_SOFT6     ((pteval_t)1UL << _PAGE_BIT_SOFT6)
#define _PAGE_SOFT7     ((pteval_t)1UL << _PAGE_BIT_SOFT7)
#define _PAGE_SOFT8     ((pteval_t)1UL << _PAGE_BIT_SOFT8)

#define PAGE_TABLE_ENTRY_COUNT 512

struct page_table {
    pteval_t entries[PAGE_TABLE_ENTRY_COUNT];
} __attribute__ ((aligned (4096)));

// A 9-bit index into a page table
// Guaranteed to only ever contain 0..512
typedef uint16_t page_table_index_t;

__always_inline page_table_index_t page_table_index_from(uint16_t index) {
    return index & 0x1ff;
}

// A 12-bit offset into a 4KiB Page
typedef uint16_t page_offset_t;
__always_inline page_offset_t page_offset_from(uint16_t offset) {
    return offset & 0xfff;
}

enum PageTableLevel {
    None = 0,
    One = 1,
    Two = 2,
    Three = 3,
    Four = 4,
};

__always_inline enum PageTableLevel nextLowerLevel(enum PageTableLevel level) {
    switch (level)
    {
    case Four:
        return Three;
    case Three:
        return Two;
    case Two:
        return One;
    case One:
        return None;
    default:
        return None;
    }
}   

__always_inline enum PageTableLevel nextHigherLevel(enum PageTableLevel level) {
    switch (level)
    {
    case One:
        return Two;
    case Two:
        return Three;
    case Three:
        return Four;
    case Four:
        return None;
    default:
        return None;
    }
}

// Returns alignment for the address space described by a table at the given level
__always_inline uint64_t table_address_space_alignment(enum PageTableLevel level) {
    return 1ull << (12 + (9 * (uint8_t)level));
}

// Returns alignment for the address space described by an entry in a table at the given level
__always_inline uint64_t entry_address_space_alignment(enum PageTableLevel level) {
    return 1ull << (12 + (9 * ((uint8_t)level - 1)));
}


#endif // CXL_PERSISTENT_PAGE_TABLE_H