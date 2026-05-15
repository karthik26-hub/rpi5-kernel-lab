// SPDX-License-Identifier: GPL-2.0
/*
 * hello.c - RPi5 Track 1, E1: Hello World Kernel Module
 *
 * Demonstrates: module_init/exit, module parameters, printk log levels
 * Build: make
 * Load:  sudo insmod hello.ko debug_level=1
 * Check: dmesg | tail
 * Remove: sudo rmmod hello
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/utsname.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Karthik Nambiar");
MODULE_DESCRIPTION("RPi5 Track1 E1 - Hello World LKM");
MODULE_VERSION("1.0");

/* Module parameter: pass debug_level=1 at insmod time */
static int debug_level = 0;
module_param(debug_level, int, 0644);
MODULE_PARM_DESC(debug_level, "Debug verbosity level (0=off, 1=verbose)");

static int __init hello_init(void)
{
    printk(KERN_INFO "hello: module loaded (debug_level=%d)\n", debug_level);

    if (debug_level >= 1) {
        printk(KERN_DEBUG "hello: [DEBUG] running on kernel %s\n",
               utsname()->release);
        printk(KERN_DEBUG "hello: [DEBUG] module load complete\n");
    }

    return 0;  /* 0 = success; non-zero aborts loading */
}

static void __exit hello_exit(void)
{
    printk(KERN_INFO "hello: module unloaded\n");
}

module_init(hello_init);
module_exit(hello_exit);
