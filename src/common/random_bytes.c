#include "random_bytes.h"

int wait_and_get_random_bytes(void *buf, size_t nbytes)
{
#if defined(__KERNEL__) || defined(MODULE)
    return get_random_bytes_wait(buf, nbytes);
#else
    size_t offset = 0;
    ssize_t ret;

    while (nbytes > 0) {
        do {
            ret = getrandom((char *)buf + offset, nbytes, 0);
        } while (ret == -1 && errno == EINTR);
        if (ret < 0) {
            return (int)ret;
        }
        // ret >= 0 here
        offset += (size_t)ret;
        nbytes -= (size_t)ret;
    }
    assert(nbytes == 0);
    return 0;
#endif
}
