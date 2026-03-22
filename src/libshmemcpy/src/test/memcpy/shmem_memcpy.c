// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2023, Intel Corporation */

/*
 * shmem_memcpy.c -- test for doing a memcpy from libpmem2
 *
 * usage: shmem2_memcpy file destoff srcoff length
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include "unittest.h"
// #include "file.h"
// #include "ut_pmem2.h"
#include "memcpy_common.h"
#include "shmemcpy_arch_api.h"
#include "shmemcpy_api.h"

/*
 * do_memcpy_variants -- do_memcpy wrapper that tests multiple variants
 * of memcpy functions
 */
static void
do_memcpy_variants(int fd, char *dest, int dest_off, char *src, int src_off,
		    size_t bytes, size_t mapped_len, const char *file_name,
		    persist_fn p, memcpy_fn fn)
{
	for (unsigned long i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memcpy(fd, dest, dest_off, src, src_off, bytes, mapped_len,
			file_name, fn, Flags[i], p);
	}
}

int
main(int argc, char *argv[])
{
	int fd = -1;
	char *dest = NULL;
	char *src = NULL;
	char *src_orig = NULL;
	size_t mapped_len = 0;
	struct stat st;

	if (argc != 5)
		UT_FATAL("usage: %s file destoff srcoff length", argv[0]);

	const char *thr = getenv("SHMEM_MOVNT_THRESHOLD");
	const char *avx = getenv("SHMEM_AVX");
	const char *avx512f = getenv("SHMEM_AVX512F");
	const char *movdir64b = getenv("SHMEM_MOVDIR64B");

	START(argc, argv, "shmem_memcpy %s %s %s %s %savx %savx512f "\
			"%smovdir64b",
			argv[2], argv[3], argv[4], thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!",
			movdir64b ? "" : "!");
	util_init();

	fd = OPEN(argv[1], O_RDWR);
	UT_ASSERT(fd != -1);
	int dest_off = atoi(argv[2]);
	int src_off = atoi(argv[3]);
	size_t bytes = strtoul(argv[4], NULL, 0);

	FSTAT(fd, &st);
	mapped_len = (size_t)st.st_size;
	dest = (char *) MMAP(NULL, mapped_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	/* src > dst */
	if (dest == NULL)
		UT_FATAL("!could not map file: %s", argv[1]);

	shmemcpy_api_init();

	src_orig = src = dest + mapped_len / 2;
	UT_ASSERT(src > dest);

	memset(dest, 0, (2 * bytes));
	shmem_output_cpu_cache(dest, 2 * bytes);
	memset(src, 0, (2 * bytes));
	shmem_output_cpu_cache(src, 2 * bytes);

	do_memcpy_variants(fd, dest, dest_off, src, src_off, bytes,
		0, argv[1], shmem_output_cpu_cache, shmem_memmove);

	src = dest;
	dest = src_orig;

	if (dest <= src)
		UT_FATAL("cannot map files in memory order");

	do_memcpy_variants(fd, dest, dest_off, src, src_off, bytes, mapped_len,
		argv[1], shmem_output_cpu_cache, shmem_memmove);

	MUNMAP(dest, mapped_len);
	CLOSE(fd);

	DONE(NULL);
}
