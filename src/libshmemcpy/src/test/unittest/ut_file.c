#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "unittest.h"

/*
 * ut_open -- an open that cannot return < 0
 */
int
ut_open(const char *file, int line, const char *func, const char *path,
    int flags, ...)
{
	va_list ap;
	int mode;

	va_start(ap, flags);
	mode = va_arg(ap, int);
	int retval = open(path, flags, mode);
	va_end(ap);

	if (retval < 0)
		ut_fatal(file, line, func, "!open: %s", path);

	return retval;
}

/*
 * ut_close -- a close that cannot return -1
 */
int
ut_close(const char *file, int line, const char *func, int fd)
{
	int retval = close(fd);

	if (retval != 0)
		ut_fatal(file, line, func, "!close: %d", fd);

	return retval;
}

/*
 * ut_lseek -- an lseek that can't return -1
 */
off_t
ut_lseek(const char *file, int line, const char *func, int fd,
    off_t offset, int whence)
{
	off_t retval = lseek(fd, offset, whence);

	if (retval == -1)
		ut_fatal(file, line, func, "!lseek: %d", fd);

	return retval;
}

/*
 * ut_read -- a read that can't return -1
 */
size_t
ut_read(const char *file, int line, const char *func, int fd,
    void *buf, size_t count)
{
	ssize_t retval = read(fd, buf, count);
	if (retval < 0)
		ut_fatal(file, line, func, "!read: %d", fd);

	return (size_t)retval;
}

/*
 * ut_mmap -- a mmap call that cannot return MAP_FAILED
 */
void *
ut_mmap(const char *file, int line, const char *func, void *addr,
    size_t length, int prot, int flags, int fd, off_t offset)
{
	void *ret_addr = mmap(addr, length, prot, flags, fd, offset);

	if (ret_addr == MAP_FAILED) {
		const char *error = "";
		ut_fatal(file, line, func,
			"!mmap: addr=0x%llx length=0x%zx prot=%d flags=%d fd=%d offset=0x%llx %s",
			(unsigned long long)addr, length, prot,
			flags, fd, (unsigned long long)offset, error);
	}
	return ret_addr;
}

/*
 * ut_munmap -- a munmap call that cannot return -1
 */
int
ut_munmap(const char *file, int line, const char *func, void *addr,
    size_t length)
{
	int retval = munmap(addr, length);

	if (retval < 0)
		ut_fatal(file, line, func, "!munmap: addr=0x%llx length=0x%zx",
		    (unsigned long long)addr, length);

	return retval;
}

/*
 * ut_stat -- a stat that cannot return -1
 */
int
ut_stat(const char *file, int line, const char *func, const char *path,
    struct stat *st_bufp)
{
	int retval = stat(path, st_bufp);

	if (retval < 0)
		ut_fatal(file, line, func, "!stat: %s", path);

	return retval;
}

/*
 * ut_fstat -- a fstat that cannot return -1
 */
int
ut_fstat(const char *file, int line, const char *func, int fd,
    struct stat *st_bufp)
{
	int retval = fstat(fd, st_bufp);

	if (retval < 0)
		ut_fatal(file, line, func, "!fstat: %d", fd);

	return retval;
}

/*
 * ut_mprotect -- a mprotect call that cannot return -1
 */
int
ut_mprotect(const char *file, int line, const char *func, void *addr,
    size_t len, int prot)
{
	int retval = mprotect(addr, len, prot);

	if (retval < 0)
		ut_fatal(file, line, func,
		    "!mprotect: addr=0x%llx length=0x%zx prot=0x%x",
		    (unsigned long long)addr, len, (unsigned int)prot);

	return retval;
}
