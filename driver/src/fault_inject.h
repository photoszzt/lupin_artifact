#ifndef FAULT_INJECT_H_
#define FAULT_INJECT_H_

#ifdef INJECT_CRASH
#include <asm/bug.h>
extern int crash_fault;
#define crash_here(num, fmt, ...)                   \
	do {                                        \
		barrier();                          \
		if (crash_fault == num) {           \
			pr_err(fmt, ##__VA_ARGS__); \
			BUG();                      \
		}                                   \
		barrier();                          \
	} while (0)
#else
#define crash_here(num, fmt, ...)
#endif

#endif // FAULT_INJECT_H_
