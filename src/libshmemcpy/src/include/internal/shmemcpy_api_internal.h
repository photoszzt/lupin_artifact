/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2022, Intel Corporation */

#ifndef SHMEMCPY_API_INTERNAL_H
#define SHMEMCPY_API_INTERNAL_H 1

#include "util.h"
#include "valgrind_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * flush_empty_nolog -- (internal) do not flush the CPU cache
 */
static force_inline void
flush_empty_nolog(const void *addr, size_t len)
{
	/* NOP, but tell pmemcheck about it */
	VALGRIND_DO_FLUSH(addr, len);
}

#ifdef __cplusplus
}
#endif

#endif // SHMEMCPY_API_INTERNAL_H
