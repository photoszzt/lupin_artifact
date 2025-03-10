#ifndef _COMMON_MACRO_H
#define _COMMON_MACRO_H

#ifndef force_inline
#define force_inline __attribute__((always_inline)) inline
#endif

#if defined(__KERNEL__) || defined(MODULE)
#include <asm/bug.h>
#include <linux/compiler.h>
#include <linux/dev_printk.h>
#include <linux/pci.h>
#include <linux/printk.h>
#include <linux/slab.h>

#define LOG(level, ...) \
  (void)level;          \
  pr_info(__VA_ARGS__)
#define ASSERTne(a, b) WARN_ON(a == b)
#define ASSERT(cnd)    WARN_ON(cnd)

/* Print macros */
#define DEV_ERR(pdev, ...) dev_err((&((struct pci_dev*)pdev)->dev), __VA_ARGS__)
#define DEV_INFO(pdev, ...) \
  dev_info((&((struct pci_dev*)pdev)->dev), __VA_ARGS__)
#define PRINT_INFO(...) pr_info(__VA_ARGS__)
#define PRINT_ERR(...)  pr_err(__VA_ARGS__)
#define PRINT_WARN(...) pr_warn(__VA_ARGS__)

// disable valgrind in kernel
#define VALGRIND_ADD_TO_TX(a, b)               ((void)0)
#define VALGRIND_REMOVE_FROM_TX(a, b)          ((void)0)
#define VALGRIND_MAKE_MEM_DEFINED(a, b)        ((void)0)
#define VALGRIND_SET_CLEAN(a, b)               ((void)0)
#define VALGRIND_ANNOTATE_NEW_MEMORY(a, b)     ((void)0)
#define VALGRIND_HG_DRD_DISABLE_CHECKING(a, b) ((void)0)

#define Free                       kfree
#define ERR                        pr_err
#define Realloc(ptr, size)         krealloc(ptr, size, GFP_KERNEL)
#define Malloc(size)               kmalloc(size, GFP_KERNEL)
#define Zalloc(size)               kzalloc(size, GFP_KERNEL)
#define Vmalloc_array(nmemb, size) kvmalloc_array(nmemb, size, GFP_KERNEL)
#define Vfree(ptr)                 kvfree(ptr)

/* Unsigned integers.  */
#define PRIu64 "llu"
#define PRIx64 "llx"

#if BITS_PER_LONG == 64
#ifndef __INT64_C
#define __INT64_C(c)  c##L
#endif

#ifndef __UINT64_C
#define __UINT64_C(c) c##UL
#endif

#else
#ifndef __INT64_C
#define __INT64_C(c)  c##LL
#endif

#ifndef __UINT64_C
#define __UINT64_C(c) c##ULL
#endif

#endif

#ifndef INT64_MAX
#define INT64_MAX (__INT64_C(9223372036854775807))
#endif
#ifndef UINT8_MAX
#define UINT8_MAX (255)
#endif
#ifndef UINT16_MAX
#define UINT16_MAX (65535)
#endif

#ifdef DEBUG_ASSERT
#define BUG_ON_ASSERT(condition) BUG_ON(condition)
#else
#define BUG_ON_ASSERT(condition)
#endif

#else  // !__KERNEL__ && !MODULE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * compiler_barrier -- issues a compiler barrier
 */
// static force_inline void
// compiler_barrier(void)
// {
// 	__asm__ volatile("" ::: "memory");
// }

#ifndef COMPILER_BARRIER
#define COMPILER_BARRIER() __asm__ volatile("" ::: "memory")
#endif

/*
 * Macro calculates number of elements in given table
 */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#define Free                       free
#define Realloc(ptr, size)         realloc(ptr, size)
#define Malloc(size)               malloc(size)
#define Zalloc(size)               calloc(1, size)
#define Vmalloc_array(nmemb, size) calloc(nmemb, size)
#define Vfree(ptr)                 free(ptr)

#define ALIGN_DOWN(size, align) ((size) & ~((align)-1))

#define DEV_ERR(dev, ...) \
  (void)dev;              \
  fprintf(stderr, __VA_ARGS__)
#define DEV_INFO(dev, ...) \
  (void)dev;               \
  fprintf(stdout, __VA_ARGS__)
#define PRINT_INFO(...) fprintf(stdout, "[INFO]" __VA_ARGS__)
#define PRINT_ERR(...)  fprintf(stderr, "[ERR]" __VA_ARGS__)
#define PRINT_WARN(...) fprintf(stderr, "[WARN]" __VA_ARGS__)

#if !defined(likely)
#if defined(__GNUC__)
#define likely(x) __builtin_expect(!!(x), 1)
#else
#define likely(x) (!!(x))
#endif
#endif

#ifndef unlikely
#if defined(__GNUC__)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define unlikely(x) (!!(x))
#endif
#endif

#ifndef BUG_ON
#ifdef NDEBUG
#define BUG_ON(cond) \
  do {               \
    if (cond) {      \
    }                \
  } while (0)
#else
#define BUG_ON(cond) assert(!(cond))
#endif
#endif
#define BUG() BUG_ON(1)

#ifdef DEBUG_ASSERT
#define BUG_ON_ASSERT(condition) assert(!(condition))
#else
#define BUG_ON_ASSERT(condition)
#endif

#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef CPU_PAUSE
#define CPU_PAUSE() asm volatile("pause\n" : : : "memory")
#endif

#if defined(__x86_64) || defined(_M_X64) || defined(__aarch64__) || \
    defined(__riscv) || defined(__loongarch64)
#define CACHELINE_SIZE 64ULL
#elif defined(__PPC64__)
#define CACHELINE_SIZE 128ULL
#else
#error unable to recognize architecture at compile time
#endif

/* macro for counting the number of varargs (up to 9) */
#define COUNT(...)                                         COUNT_I(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define COUNT_I(_, _9, _8, _7, _6, _5, _4, _3, _2, X, ...) X

/* concatenation macro */
#define GLUE(A, B)   GLUE_I(A, B)
#define GLUE_I(A, B) A##B

/* macro for suppresing errors from unused variables (up to 9) */
#define SUPPRESS_UNUSED(...) \
  GLUE(SUPPRESS_ARG_, COUNT(__VA_ARGS__))(__VA_ARGS__)
#define SUPPRESS_ARG_1(X) (void)X
#define SUPPRESS_ARG_2(X, ...) \
  SUPPRESS_ARG_1(X);           \
  SUPPRESS_ARG_1(__VA_ARGS__)
#define SUPPRESS_ARG_3(X, ...) \
  SUPPRESS_ARG_1(X);           \
  SUPPRESS_ARG_2(__VA_ARGS__)
#define SUPPRESS_ARG_4(X, ...) \
  SUPPRESS_ARG_1(X);           \
  SUPPRESS_ARG_3(__VA_ARGS__)
#define SUPPRESS_ARG_5(X, ...) \
  SUPPRESS_ARG_1(X);           \
  SUPPRESS_ARG_4(__VA_ARGS__)
#define SUPPRESS_ARG_6(X, ...) \
  SUPPRESS_ARG_1(X);           \
  SUPPRESS_ARG_5(__VA_ARGS__)
#define SUPPRESS_ARG_7(X, ...) \
  SUPPRESS_ARG_1(X);           \
  SUPPRESS_ARG_6(__VA_ARGS__)
#define SUPPRESS_ARG_8(X, ...) \
  SUPPRESS_ARG_1(X);           \
  SUPPRESS_ARG_7(__VA_ARGS__)
#define SUPPRESS_ARG_9(X, ...) \
  SUPPRESS_ARG_1(X);           \
  SUPPRESS_ARG_8(__VA_ARGS__)

#ifndef XSTR
#define XSTR(x) STR(x)
#endif
#ifndef STR
#define STR(x) #x
#endif

#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))

#endif  // _COMMON_MACRO_H
