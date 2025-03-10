#include <stdio.h>
#include <stdalign.h>
#include <stddef.h>

#include "buddy_alloc.h"
#include "log.h"

#ifndef PRINTF
#define PRINTF printf
#endif

#include "print_offset_size.h"

void main(void) {
    print_size();
}