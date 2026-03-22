#include <execinfo.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "unittest.h"

#define SIZE 100

/*
 * ut_dump_backtrace -- dump stacktrace to error log using libc's backtrace
 */
void
ut_dump_backtrace(void)
{
	// NOLINTBEGIN(cert-sig30-c)
	int j, nptrs;
	void *buffer[SIZE];
	char **strings;

	nptrs = backtrace(buffer, SIZE);

	strings = backtrace_symbols(buffer, nptrs);
	if (strings == NULL) {
		UT_ERR("!backtrace_symbols");
		return;
	}

	for (j = 0; j < nptrs; j++)
		UT_ERR("%d: %s", j, strings[j]);

	free(strings);
	// NOLINTEND(cert-sig30-c)
}

/*
 * ut_sighandler -- fatal signal handler
 */
void
ut_sighandler(int sig)
{
	// NOLINTBEGIN(cert-sig30-c)
	/*
	 * Usually SIGABRT is a result of ASSERT() or FATAL().
	 * We don't need backtrace, as the reason of the failure
	 * is logged in debug traces.
	 */
	if (sig != SIGABRT) {
		UT_ERR("\n");
		UT_ERR("Signal %d, backtrace:", sig);
		ut_dump_backtrace();
		UT_ERR("\n");
	}
	exit(128 + sig);
	// NOLINTEND(cert-sig30-c)
}

/*
 * ut_register_sighandlers -- register signal handlers for various fatal signals
 */
void
ut_register_sighandlers(void)
{
	sig_t ret = signal(SIGSEGV, ut_sighandler);
	if (ret == SIG_ERR) {
		UT_FATAL("!fail to setup SIGSEGV handler: %s", strerror(errno));
	}
	ret = signal(SIGABRT, ut_sighandler);
	if (ret == SIG_ERR) {
		UT_FATAL("!fail to setup SIGABRT handler: %s", strerror(errno));
	}
	ret = signal(SIGILL, ut_sighandler);
	if (ret == SIG_ERR) {
		UT_FATAL("!fail to setup SIGILL handler: %s", strerror(errno));
	}
	ret = signal(SIGFPE, ut_sighandler);
	if (ret == SIG_ERR) {
		UT_FATAL("!fail to setup SIGFPE handler: %s", strerror(errno));
	}
	ret = signal(SIGINT, ut_sighandler);
	if (ret == SIG_ERR) {
		UT_FATAL("!fail to setup SIGINT handler: %s", strerror(errno));
	}
}