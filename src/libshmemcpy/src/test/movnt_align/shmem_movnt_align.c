// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2022, Intel Corporation */

/*
 * pmem2_movnt_align.c -- test for functions with non-temporal stores
 *
 * usage: pmem2_movnt_align file [C|F|B|S]
 *
 * C - pmem2_memcpy()
 * B - pmem2_memmove() in backward direction
 * F - pmem2_memmove() in forward direction
 * S - pmem2_memset()
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// #include "libpmem2.h"
#include "unittest.h"
#include "movnt_align_common.h"
// #include "ut_pmem2.h"
#include "shmemcpy_arch_api.h"
#include "shmemcpy_api.h"

static void
check_memmove_variants(size_t doff, size_t soff, size_t len, pmem_memmove_fn memmove_fn)
{
	for (unsigned long i = 0; i < ARRAY_SIZE(Flags); ++i)
		check_memmove(doff, soff, len, memmove_fn, Flags[i]);
}

static void
check_memcpy_variants(size_t doff, size_t soff, size_t len, pmem_memcpy_fn memcpy_fn)
{
	for (unsigned long i = 0; i < ARRAY_SIZE(Flags); ++i)
		check_memcpy(doff, soff, len, memcpy_fn, Flags[i]);
}

static void
check_memset_variants(size_t off, size_t len, pmem_memset_fn memset_fn)
{
	for (unsigned long i = 0; i < ARRAY_SIZE(Flags); ++i)
		check_memset(off, len, memset_fn, Flags[i]);
}

int
main(int argc, char *argv[])
{
	if (argc != 3)
		UT_FATAL("usage: %s file type", argv[0]);

	char type = argv[2][0];
	const char *thr = getenv("SHMEM_MOVNT_THRESHOLD");
	const char *avx = getenv("SHMEM_AVX");
	const char *avx512f = getenv("SHMEM_AVX512F");
	const char *movdir64b = getenv("SHMEM_MOVDIR64B");

	START(argc, argv, "shmem_movnt_align %c %s %savx %savx512f %smovdir64b",
			type,
			thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!",
			movdir64b ? "" : "!");
	util_init();

	size_t page_size = Ut_pagesize;
	size_t s;
	switch (type) {
	case 'C': /* memcpy */
		/* mmap with guard pages */
		Src = MMAP_ANON_ALIGNED(N_BYTES, 0);
		Dst = MMAP_ANON_ALIGNED(N_BYTES, 0);
		if (Src == NULL || Dst == NULL)
			UT_FATAL("!mmap");

		Scratch = MALLOC(N_BYTES);

		/* check memcpy with 0 size */
		check_memcpy_variants(0, 0, 0, shmem_memmove);

		/* check memcpy with unaligned size */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memcpy_variants(0, 0, N_BYTES - s, shmem_memmove);

		/* check memcpy with unaligned begin */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memcpy_variants(s, 0, N_BYTES - s, shmem_memmove);

		/* check memcpy with unaligned begin and end */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memcpy_variants(s, s, N_BYTES - 2 * s, shmem_memmove);

		MUNMAP_ANON_ALIGNED(Src, N_BYTES);
		MUNMAP_ANON_ALIGNED(Dst, N_BYTES);
		FREE(Scratch);

		break;
	case 'B': /* memmove backward */
		/* mmap with guard pages */
		Src = MMAP_ANON_ALIGNED(2 * N_BYTES - page_size, 0);
		if (Src == NULL)
			UT_FATAL("!mmap");
		Dst = Src + N_BYTES - page_size;

		/* check memmove in backward direction with 0 size */
		check_memmove_variants(0, 0, 0, shmem_memmove);

		/* check memmove in backward direction with unaligned size */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(0, 0, N_BYTES - s, shmem_memmove);

		/* check memmove in backward direction with unaligned begin */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(s, 0, N_BYTES - s, shmem_memmove);

		/*
		 * check memmove in backward direction with unaligned begin
		 * and end
		 */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(s, s, N_BYTES - 2 * s, shmem_memmove);

		MUNMAP_ANON_ALIGNED(Src, 2 * N_BYTES - page_size);
		break;
	case 'F': /* memmove forward */
		/* mmap with guard pages */
		Dst = MMAP_ANON_ALIGNED(2 * N_BYTES - page_size, 0);
		Src = Dst + N_BYTES - page_size;
		if (Src == NULL)
			UT_FATAL("!mmap");

		/* check memmove in forward direction with 0 size */
		check_memmove_variants(0, 0, 0, shmem_memmove);

		/* check memmove in forward direction with unaligned size */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(0, 0, N_BYTES - s, shmem_memmove);

		/* check memmove in forward direction with unaligned begin */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(s, 0, N_BYTES - s, shmem_memmove);

		/*
		 * check memmove in forward direction with unaligned begin
		 * and end
		 */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(s, s, N_BYTES - 2 * s, shmem_memmove);

		MUNMAP_ANON_ALIGNED(Dst, 2 * N_BYTES - page_size);

		break;
	case 'S': /* memset */
		/* mmap with guard pages */
		Dst = MMAP_ANON_ALIGNED(N_BYTES, 0);
		if (Dst == NULL)
			UT_FATAL("!mmap");

		Scratch = MALLOC(N_BYTES);

		/* check memset with 0 size */
		check_memset_variants(0, 0, shmem_memset);

		/* check memset with unaligned size */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memset_variants(0, N_BYTES - s, shmem_memset);

		/* check memset with unaligned begin */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memset_variants(s, N_BYTES - s, shmem_memset);

		/* check memset with unaligned begin and end */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memset_variants(s, N_BYTES - 2 * s, shmem_memset);

		MUNMAP_ANON_ALIGNED(Dst, N_BYTES);
		FREE(Scratch);

		break;
	default:
		UT_FATAL("!wrong type of test");
	}

	DONE(NULL);
}
