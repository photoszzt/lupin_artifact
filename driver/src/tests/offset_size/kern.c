#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#define BUDDY_HEADER 1
#include <linux/types.h>
#include "buddy_alloc.h"

#include "log.h"
#define PRINTF pr_info
#include "print_offset_size.h"

MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
MODULE_AUTHOR("");
MODULE_DESCRIPTION("");


static int __init test_init(void)
{
    print_size();
    return 0;
}

static void __exit test_exit(void)
{

}

module_init(test_init);
module_exit(test_exit);
