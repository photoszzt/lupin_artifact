// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2023, Intel Corporation */

/*
 * shmem_memmove.c -- test for doing a memmove
 *
 * usage:
 * shmem_memmove file b:length [d:{offset}] [s:offset] [o:{1|2} S:{overlap}]
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include "unittest.h"
// #include "ut_pmem2.h"
// #include "file.h"
#include "memmove_common.h"
#include "shmemcpy_arch_api.h"
#include "shmemcpy_api.h"

static void
do_memmove_variants(char *dst, char *src, const char *file_name,
	size_t dest_off, size_t src_off, size_t bytes, persist_fn p,
	memmove_fn fn)
{
	for (unsigned long i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memmove(dst, src, file_name, dest_off, src_off,
				bytes, fn, Flags[i], p);
	}
}

int
main(int argc, char *argv[])
{
	int fd;
	char *dst;
	char *src;
	char *src_orig;
	size_t dst_off = 0;
	size_t src_off = 0;
	size_t bytes = 0;
	int who = 0;
	size_t mapped_len;
	struct stat st;

	const char *thr = getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = getenv("PMEM_AVX");
	const char *avx512f = getenv("PMEM_AVX512F");

	START(argc, argv, "pmem2_memmove %s %s %s %s %savx %savx512f",
			argc > 2 ? argv[2] : "null",
			argc > 3 ? argv[3] : "null",
			argc > 4 ? argv[4] : "null",
			thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!");

	fd = OPEN(argv[1], O_RDWR);
	UT_ASSERT(fd != -1);

	if (argc < 3)
		USAGE();


	FSTAT(fd, &st);
	mapped_len = (size_t)st.st_size;
	dst = (char *) MMAP(NULL, mapped_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	
	if (dst == NULL)
		UT_FATAL("!could not map file: %s", argv[1]);

	for (int arg = 2; arg < argc; arg++) {
		if (strchr("dsbo",
		    argv[arg][0]) == NULL || argv[arg][1] != ':')
			UT_FATAL("op must be d: or s: or b: or o:");

		size_t val = STRTOUL(&argv[arg][2], NULL, 0);

		switch (argv[arg][0]) {
		case 'd':
			if (val <= 0)
				UT_FATAL("bad offset (%lu) with d: option",
						val);
			dst_off = val;
			break;

		case 's':
			if (val <= 0)
				UT_FATAL("bad offset (%lu) with s: option",
						val);
			src_off = val;
			break;

		case 'b':
			if (val <= 0)
				UT_FATAL("bad length (%lu) with b: option",
						val);
			bytes = val;
			break;

		case 'o':
			if (val != 1 && val != 0)
				UT_FATAL("bad val (%lu) with o: option",
						val);
			who = (int)val;
			break;
		default:
			UT_FATAL("op must be d: or s: or b: or o:");
		}
	}

	if (who == 0) {
		src_orig = src = dst + mapped_len / 2;
		UT_ASSERT(src > dst);

		do_memmove_variants(dst, src, argv[1], dst_off, src_off,
			bytes, shmem_output_cpu_cache, shmem_memmove);

		/* dest > src */
		src = dst;
		dst = src_orig;

		if (dst <= src)
			UT_FATAL("cannot map files in memory order");

		do_memmove_variants(dst, src, argv[1], dst_off, src_off,
			bytes, shmem_output_cpu_cache, shmem_memmove);
	} else {
		/* use the same buffer for source and destination */
		memset(dst, 0, bytes);
		shmem_output_cpu_cache(dst, bytes);
		do_memmove_variants(dst, dst, argv[1], dst_off, src_off,
			bytes, shmem_output_cpu_cache, shmem_memmove);
	}

	MUNMAP(dst, mapped_len);

	CLOSE(fd);

	DONE(NULL);
}
