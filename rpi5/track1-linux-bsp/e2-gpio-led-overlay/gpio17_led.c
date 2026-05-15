// SPDX-License-Identifier: GPL-2.0
/*
 * gpio17_led.c - RPi5 Track 1, E2: External LED driver on GPIO 17
 *
 * Kernel 6.12 API: gpio_device_find_by_label() + gpio_device_get_desc()
 * RPi5 RP1 chip label: "pinctrl-rp1"
 *
 * Build:  make clean && make
 * Load:   sudo insmod gpio17_led.ko
 *         sudo insmod gpio17_led.ko blink_on_load=1
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>
#include <linux/utsname.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Karthik Nambiar");
MODULE_DESCRIPTION("RPi5 Track1 E2 - External LED on GPIO17 via RP1 (kernel 6.12)");
MODULE_VERSION("1.0");

#define LED_NAME    "gpio17-led"
#define GPIO_LINE   17
#define CHIP_LABEL  "pinctrl-rp1"

static int blink_on_load = 0;
module_param(blink_on_load, int, 0644);
MODULE_PARM_DESC(blink_on_load, "Flash LED 3x on load to confirm wiring");

static struct gpio_desc *led_gpio;
static struct gpio_device *led_gdev;

static void gpio17_led_set(struct led_classdev *cdev,
                            enum led_brightness brightness)
{
    gpiod_set_value(led_gpio, brightness ? 1 : 0);
}

static struct led_classdev gpio17_led = {
    .name           = LED_NAME,
    .brightness     = LED_OFF,
    .brightness_set = gpio17_led_set,
    .flags          = LED_CORE_SUSPENDRESUME,
};

static int __init gpio17_led_init(void)
{
    int ret, i;

    printk(KERN_INFO "gpio17_led: loading on kernel %s\n", utsname()->release);

    /* Step 1: find the RP1 GPIO device by label */
    led_gdev = gpio_device_find_by_label(CHIP_LABEL);
    if (!led_gdev) {
        printk(KERN_ERR "gpio17_led: gpio_device '%s' not found\n", CHIP_LABEL);
        return -ENODEV;
    }
    printk(KERN_INFO "gpio17_led: found gpio_device '%s'\n", CHIP_LABEL);

    /* Step 2: get descriptor for line 17 */
    led_gpio = gpio_device_get_desc(led_gdev, GPIO_LINE);
    if (IS_ERR(led_gpio)) {
        printk(KERN_ERR "gpio17_led: gpio_device_get_desc(%d) failed: %ld\n",
               GPIO_LINE, PTR_ERR(led_gpio));
        gpio_device_put(led_gdev);
        return PTR_ERR(led_gpio);
    }

    /* Step 3: request and configure as output low */
    ret = gpiod_direction_output(led_gpio, 0);
    if (ret) {
        printk(KERN_ERR "gpio17_led: gpiod_direction_output failed: %d\n", ret);
        gpio_device_put(led_gdev);
        return ret;
    }

    /* Step 4: register with LED subsystem */
    ret = led_classdev_register(NULL, &gpio17_led);
    if (ret) {
        printk(KERN_ERR "gpio17_led: led_classdev_register failed: %d\n", ret);
        gpiod_set_value(led_gpio, 0);
        gpio_device_put(led_gdev);
        return ret;
    }

    /* Step 5: optional 3x blink to confirm wiring */
    if (blink_on_load) {
        for (i = 0; i < 3; i++) {
            gpiod_set_value(led_gpio, 1);
            msleep(200);
            gpiod_set_value(led_gpio, 0);
            msleep(200);
        }
        printk(KERN_INFO "gpio17_led: blink-on-load done\n");
    }

    printk(KERN_INFO "gpio17_led: /sys/class/leds/%s ready on %s line %d\n",
           LED_NAME, CHIP_LABEL, GPIO_LINE);
    return 0;
}

static void __exit gpio17_led_exit(void)
{
    led_classdev_unregister(&gpio17_led);
    gpiod_set_value(led_gpio, 0);
    gpio_device_put(led_gdev);
    printk(KERN_INFO "gpio17_led: unloaded, GPIO%d low\n", GPIO_LINE);
}

module_init(gpio17_led_init);
module_exit(gpio17_led_exit);
