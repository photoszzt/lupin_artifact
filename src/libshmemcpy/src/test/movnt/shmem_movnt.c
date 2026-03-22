// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2022, Intel Corporation */

/*
 * shmem_movnt.c -- test for MOVNT threshold
 *
 * usage: shmem_movnt
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include "unittest.h"
// #include "ut_pmem2.h"
#include "shmemcpy_arch_api.h"
#include "shmemcpy_api.h"

int
main(int argc, char *argv[])
{
	int fd;
	char *dst;
	char *src;
	struct stat st;

	if (argc != 2)
		UT_FATAL("usage: %s file", argv[0]);

	const char *thr = getenv("SHMEM_MOVNT_THRESHOLD");
	const char *avx = getenv("SHMEM_AVX");
	const char *avx512f = getenv("SHMEM_AVX512F");
	const char *movdir64b = getenv("SHMEM_MOVDIR64B");

	START(argc, argv, "shmem_movnt %s %savx %savx512f %smovdir64b",
			thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!",
			movdir64b ? "" : "!");
	util_init();

	fd = OPEN(argv[1], O_RDWR);
	UT_ASSERT(fd != -1);

	FSTAT(fd, &st);
	size_t mapped_len = (size_t)st.st_size;
	dst = (char *) MMAP(NULL, mapped_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	src = MEMALIGN(64, 8192);
	dst = MEMALIGN(64, 8192);

	memset(src, 0x88, 8192);
	memset(dst, 0, 8192);

	for (size_t size = 1; size <= 4096; size *= 2) {
		memset(dst, 0, 4096);
		shmem_memmove(dst, src, size, SHMEM_F_MEM_NODRAIN);
		UT_ASSERTeq(memcmp(src, dst, size), 0);
		UT_ASSERTeq(dst[size], 0);
	}

	for (size_t size = 1; size <= 4096; size *= 2) {
		memset(dst, 0, 4096);
		shmem_memset(dst, 0x77, size, SHMEM_F_MEM_NODRAIN);
		UT_ASSERTeq(dst[0], 0x77);
		UT_ASSERTeq(dst[size - 1], 0x77);
		UT_ASSERTeq(dst[size], 0);
	}

	ALIGNED_FREE(dst);
	ALIGNED_FREE(src);

	MUNMAP(dst, mapped_len);

	CLOSE(fd);

	DONE(NULL);
}
