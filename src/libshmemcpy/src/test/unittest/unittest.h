/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2023, Intel Corporation */

#ifndef _UNITTEST_H
#define _UNITTEST_H 1

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util.h"

#define UT_MAX_ERR_MSG 128

extern unsigned long Ut_pagesize;
extern unsigned long long Ut_mmap_align;

void ut_dump_backtrace(void);
void ut_sighandler(int);
void ut_register_sighandlers(void);

/*
 * unit test support...
 */
void ut_start(const char *file, int line, const char *func,
	int argc, char * const argv[], const char *fmt, ...)
	__attribute__((format(printf, 6, 7)));
void NORETURN ut_done(const char *file, int line, const char *func,
	const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));
void NORETURN ut_fatal(const char *file, int line, const char *func,
	const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));
void ut_out(const char *file, int line, const char *func,
	const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));
void ut_err(const char *file, int line, const char *func,
	const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));

/* indicate the start of the test */
#define START(argc, argv, ...)\
    ut_start(__FILE__, __LINE__, __func__, argc, argv, __VA_ARGS__)

/* normal exit from test */
#ifndef _WIN32
#define DONE(...)\
    ut_done(__FILE__, __LINE__, __func__, __VA_ARGS__)
#else
#define DONE(...)\
	for (int i = argc; i > 0; i--)\
		free(argv[i - 1]);\
	ut_done(__FILE__, __LINE__, __func__, __VA_ARGS__)
#endif

/* fatal error detected */
#define UT_FATAL(...)\
    ut_fatal(__FILE__, __LINE__, __func__, __VA_ARGS__)

/* normal output */
#define UT_OUT(...)\
    ut_out(__FILE__, __LINE__, __func__, __VA_ARGS__)

/* error output */
#define UT_ERR(...)\
    ut_err(__FILE__, __LINE__, __func__, __VA_ARGS__)

/*
 * assertions...
 */

/* assert a condition is true at runtime */
#define UT_ASSERT_rt(cnd)\
	((void)((cnd) || (ut_fatal(__FILE__, __LINE__, __func__,\
	"assertion failure: %s", #cnd), 0)))

/* assertion with extra info printed if assertion fails at runtime */
#define UT_ASSERTinfo_rt(cnd, info) \
	((void)((cnd) || (ut_fatal(__FILE__, __LINE__, __func__,\
	"assertion failure: %s (%s)", #cnd, info), 0)))

/* assert two integer values are equal at runtime */
#define UT_ASSERTeq_rt(lhs, rhs)\
	((void)(((lhs) == (rhs)) || (ut_fatal(__FILE__, __LINE__, __func__,\
	"assertion failure: %s (0x%llx) == %s (0x%llx)", #lhs,\
	(unsigned long long)(lhs), #rhs, (unsigned long long)(rhs)), 0)))

/* assert two integer values are not equal at runtime */
#define UT_ASSERTne_rt(lhs, rhs)\
	((void)(((lhs) != (rhs)) || (ut_fatal(__FILE__, __LINE__, __func__,\
	"assertion failure: %s (0x%llx) != %s (0x%llx)", #lhs,\
	(unsigned long long)(lhs), #rhs, (unsigned long long)(rhs)), 0)))

#if defined(__CHECKER__)
#define UT_COMPILE_ERROR_ON(cond)
#define UT_ASSERT_COMPILE_ERROR_ON(cond)
#elif defined(_MSC_VER)
#define UT_COMPILE_ERROR_ON(cond) C_ASSERT(!(cond))
/* XXX - can't be done with C_ASSERT() unless we have __builtin_constant_p() */
#define UT_ASSERT_COMPILE_ERROR_ON(cond) (void)(cond)
#else
#define UT_COMPILE_ERROR_ON(cond) ((void)sizeof(char[(cond) ? -1 : 1]))
#ifndef __cplusplus
#define UT_ASSERT_COMPILE_ERROR_ON(cond) UT_COMPILE_ERROR_ON(cond)
#else /* __cplusplus */
/*
 * XXX - workaround for https://github.com/pmem/issues/issues/189
 */
#define UT_ASSERT_COMPILE_ERROR_ON(cond) UT_ASSERT_rt(!(cond))
#endif /* __cplusplus */
#endif /* _MSC_VER */

/* assert a condition is true */
#define UT_ASSERT(cnd)\
	do {\
		/*\
		 * Detect useless asserts on always true expression. Please use\
		 * UT_COMPILE_ERROR_ON(!cnd) or UT_ASSERT_rt(cnd) in such\
		 * cases.\
		 */\
		if (__builtin_constant_p(cnd))\
			UT_ASSERT_COMPILE_ERROR_ON(cnd);\
		UT_ASSERT_rt(cnd);\
	} while (0)

/* assertion with extra info printed if assertion fails */
#define UT_ASSERTinfo(cnd, info) \
	do {\
		/* See comment in UT_ASSERT. */\
		if (__builtin_constant_p(cnd))\
			UT_ASSERT_COMPILE_ERROR_ON(cnd);\
		UT_ASSERTinfo_rt(cnd, info);\
	} while (0)

/* assert two integer values are equal */
#define UT_ASSERTeq(lhs, rhs)\
	do {\
		/* See comment in UT_ASSERT. */\
		if (__builtin_constant_p(lhs) && __builtin_constant_p(rhs))\
			UT_ASSERT_COMPILE_ERROR_ON((lhs) == (rhs));\
		UT_ASSERTeq_rt(lhs, rhs);\
	} while (0)

/* assert two integer values are not equal */
#define UT_ASSERTne(lhs, rhs)\
	do {\
		/* See comment in UT_ASSERT. */\
		if (__builtin_constant_p(lhs) && __builtin_constant_p(rhs))\
			UT_ASSERT_COMPILE_ERROR_ON((lhs) != (rhs));\
		UT_ASSERTne_rt(lhs, rhs);\
	} while (0)

/* assert pointer is fits range of [start, start + size) */
#define UT_ASSERTrange(ptr, start, size)\
	((void)(((uintptr_t)(ptr) >= (uintptr_t)(start) &&\
	(uintptr_t)(ptr) < (uintptr_t)(start) + (uintptr_t)(size)) ||\
	(ut_fatal(__FILE__, __LINE__, __func__,\
	"assert failure: %s (%p) is outside range [%s (%p), %s (%p))", #ptr,\
	(void *)(ptr), #start, (void *)(start), #start"+"#size,\
	(void *)((uintptr_t)(start) + (uintptr_t)(size))), 0)))

/*
 * memory allocation...
 */
void *ut_malloc(const char *file, int line, const char *func, size_t size);
void *ut_calloc(const char *file, int line, const char *func,
    size_t nmemb, size_t size);
void ut_free(const char *file, int line, const char *func, void *ptr);
void ut_aligned_free(const char *file, int line, const char *func, void *ptr);
char *ut_strdup(const char *file, int line, const char *func,
    const char *str);
void *ut_memalign(const char *file, int line, const char *func,
    size_t alignment, size_t size);

/* a malloc() that can't return NULL */
#define MALLOC(size)\
    ut_malloc(__FILE__, __LINE__, __func__, size)

/* a malloc() of zeroed memory */
#define ZALLOC(size)\
    ut_calloc(__FILE__, __LINE__, __func__, 1, size)

/* a malloc() that returns memory with given alignment */
#define MEMALIGN(alignment, size)\
    ut_memalign(__FILE__, __LINE__, __func__, alignment, size)


/* a strdup() that can't return NULL */
#define STRDUP(str)\
    ut_strdup(__FILE__, __LINE__, __func__, str)

#define FREE(ptr)\
    ut_free(__FILE__, __LINE__, __func__, ptr)

#define ALIGNED_FREE(ptr)\
    ut_aligned_free(__FILE__, __LINE__, __func__, ptr)

/*
 * file operations
 */
int ut_open(const char *file, int line, const char *func, const char *path,
    int flags, ...);

int ut_close(const char *file, int line, const char *func, int fd);

size_t ut_read(const char *file, int line, const char *func, int fd,
    void *buf, size_t len);

off_t ut_lseek(const char *file, int line, const char *func, int fd,
    off_t offset, int whence);

int ut_stat(const char *file, int line, const char *func, const char *path,
    struct stat *st_bufp);

int ut_fstat(const char *file, int line, const char *func, int fd,
    struct stat *st_bufp);

unsigned long long
ut_strtoull(const char *file, int line, const char *func,
	const char *nptr, char **endptr, int base);

unsigned long ut_strtoul(const char *file, int line, const char *func,
    const char *nptr, char **endptr, int base);

void *ut_mmap(const char *file, int line, const char *func, void *addr,
    size_t length, int prot, int flags, int fd, off_t offset);

int ut_munmap(const char *file, int line, const char *func, void *addr,
    size_t length);

int ut_mprotect(const char *file, int line, const char *func, void *addr,
    size_t len, int prot);

void * ut_mmap_anon_aligned(const char *file, int line, const char *func,
	size_t alignment, size_t size);

int ut_munmap_anon_aligned(const char *file, int line, const char *func,
	void *start, size_t size);

/* an open() that can't return < 0 */
#define OPEN(path, ...)\
    ut_open(__FILE__, __LINE__, __func__, path, __VA_ARGS__)

/* a close() that can't return -1 */
#define CLOSE(fd)\
    ut_close(__FILE__, __LINE__, __func__, fd)

/* a read() that can't return -1 */
#define READ(fd, buf, len)\
    ut_read(__FILE__, __LINE__, __func__, fd, buf, len)

/* a lseek() that can't return -1 */
#define LSEEK(fd, offset, whence)\
    ut_lseek(__FILE__, __LINE__, __func__, fd, offset, whence)

/* a mmap() that can't return MAP_FAILED */
#define MMAP(addr, len, prot, flags, fd, offset)\
    ut_mmap(__FILE__, __LINE__, __func__, addr, len, prot, flags, fd, offset)

/* a munmap() that can't return -1 */
#define MUNMAP(addr, length)\
    ut_munmap(__FILE__, __LINE__, __func__, addr, length)

/* a mprotect() that can't return -1 */
#define MPROTECT(addr, len, prot)\
    ut_mprotect(__FILE__, __LINE__, __func__, addr, len, prot);

/*
 * A mmap() that returns anonymous memory with given alignment and guard
 * pages.
 */
#define MMAP_ANON_ALIGNED(size, alignment)\
    ut_mmap_anon_aligned(__FILE__, __LINE__, __func__, alignment, size)

#define MUNMAP_ANON_ALIGNED(start, size)\
    ut_munmap_anon_aligned(__FILE__, __LINE__, __func__, start, size)

#define STAT(path, st_bufp)\
    ut_stat(__FILE__, __LINE__, __func__, path, st_bufp)

#define FSTAT(fd, st_bufp)\
    ut_fstat(__FILE__, __LINE__, __func__, fd, st_bufp)

#define STRTOULL(nptr, endptr, base)\
    ut_strtoull(__FILE__, __LINE__, __func__, nptr, endptr, base)

#define STRTOUL(nptr, endptr, base)\
    ut_strtoul(__FILE__, __LINE__, __func__, nptr, endptr, base)

#endif // _UNITTEST_H
