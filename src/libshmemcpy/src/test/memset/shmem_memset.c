// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2023, Intel Corporation */

/*
 * shmem_memset.c -- unit test for doing a memset
 *
 * usage: shmem_memset file offset length
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include "unittest.h"
// #include "file.h"
// #include "ut_pmem2.h"
#include "memset_common.h"
#include "shmemcpy_arch_api.h"
#include "shmemcpy_api.h"

static void
do_memset_variants(int fd, char *dest, const char *file_name, size_t dest_off,
		size_t bytes, persist_fn p, memset_fn fn)
{
	for (unsigned long i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memset(fd, dest, file_name, dest_off, bytes,
				fn, Flags[i], p);
		if (Flags[i] & SHMEM_F_MEM_NOFLUSH)
			p(dest, bytes);
	}
}

int
main(int argc, char *argv[])
{
	int fd;
	char *dest;
	struct stat st;

	if (argc != 4)
		UT_FATAL("usage: %s file offset length", argv[0]);

	const char *thr = getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = getenv("PMEM_AVX");
	const char *avx512f = getenv("PMEM_AVX512F");
	const char *movdir64b = getenv("PMEM_MOVDIR64B");

	START(argc, argv, "pmem2_memset %s %s %s %savx %savx512f %smovdir64b",
			argv[2], argv[3],
			thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!",
			movdir64b ? "" : "!");
	util_init();

	fd = OPEN(argv[1], O_RDWR);

	FSTAT(fd, &st);
	size_t mapped_len = (size_t)st.st_size;
	dest = (char *) MMAP(NULL, mapped_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (dest == NULL)
		UT_FATAL("!could not map file: %s", argv[1]);

	size_t dest_off = strtoul(argv[2], NULL, 0);
	size_t bytes = strtoul(argv[3], NULL, 0);

	do_memset_variants(fd, dest, argv[1], dest_off, bytes,
		shmem_output_cpu_cache, shmem_memset);

	MUNMAP(dest, mapped_len);

	CLOSE(fd);

	DONE(NULL);
}
