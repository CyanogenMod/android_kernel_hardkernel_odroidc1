/*
 * ODROID sysfs support for extra feature enhancement
 *
 * Copyright (C) 2014, Hardkernel Co,.Ltd
 * Author: Charles Park <charles.park@hardkernel.com>
 * Author: Dongjin Kim <tobetter@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/sysfs.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/hrtimer.h>
#include <asm/setup.h>

MODULE_AUTHOR("Hardkernel Co,.Ltd");
MODULE_DESCRIPTION("SYSFS driver for ODROID hardware");
MODULE_LICENSE("GPL");

static struct hrtimer input_timer;
static struct input_dev *input_dev;
static int keycode[] = { KEY_POWER, };
static int key_release_seconds = 0;

static ssize_t set_poweroff_trigger(struct class *class,
                struct class_attribute *attr, const char *buf, size_t count)
{
        unsigned int val;

        if (!(sscanf(buf, "%d\n", &val)))
                return  -EINVAL;

        // Emulate power button by software
        if ((val != 0) && (val < 5)) {
                if (!key_release_seconds) {
                        key_release_seconds = val;
                        input_report_key(input_dev, KEY_POWER, 1);

                        hrtimer_start(&input_timer,
                                        ktime_set(key_release_seconds, 0),
                                        HRTIMER_MODE_REL);

                        input_sync(input_dev);
                }
        }

        return count;
}

static const char *product;
static const char *serialno;
static const char *mac_addr;

static ssize_t show_product(struct class *class,
                struct class_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", product);
}

static ssize_t show_serialno(struct class *class,
                struct class_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", serialno);
}

static ssize_t show_mac_addr(struct class *class,
                struct class_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", mac_addr);
}

static struct class_attribute odroid_class_attrs[] = {
        __ATTR(poweroff_trigger, 0222, NULL, set_poweroff_trigger),
        __ATTR(product, 0444, show_product, NULL),
        __ATTR(serialno, 0444, show_serialno, NULL),
        __ATTR(mac_addr, 0444, show_mac_addr, NULL),
        __ATTR_NULL,
};

static struct class odroid_class = {
        .name = "odroid",
        .owner = THIS_MODULE,
        .class_attrs = odroid_class_attrs,
};

static enum hrtimer_restart input_timer_function(struct hrtimer *timer)
{
        key_release_seconds = 0;
        input_report_key(input_dev, KEY_POWER, 0);
        input_sync(input_dev);

        return HRTIMER_NORESTART;
}

static int odroid_sysfs_probe(struct platform_device *pdev)
{
        int error = 0;
#ifdef CONFIG_USE_OF
    struct device_node *node;
#endif

#if defined(SLEEP_DISABLE_FLAG)
#if defined(CONFIG_HAS_WAKELOCK)
        wake_lock(&sleep_wake_lock);
#endif
#endif
        //------------------------------------------------------------------------
        // virtual key init (Power Off Key)
        //------------------------------------------------------------------------
        input_dev = input_allocate_device();
        if (!input_dev) {
                error = -ENOMEM;
                goto err_out;
        }

        input_dev->name = "vt-input";
        input_dev->phys = "vt-input/input0";
        input_dev->id.bustype = BUS_HOST;
        input_dev->id.vendor = 0x16B4;
        input_dev->id.product = 0x0701;
        input_dev->id.version = 0x0001;
        input_dev->keycode = keycode;
        input_dev->keycodesize = sizeof(keycode[0]);
        input_dev->keycodemax = ARRAY_SIZE(keycode);

        set_bit(EV_KEY, input_dev->evbit);
        set_bit(KEY_POWER & KEY_MAX, input_dev->keybit);

        error = input_register_device(input_dev);
        if (error) {
                input_free_device(input_dev);
                goto err_out;
        }

        printk("%s input driver registered!!\n", "Virtual-Key");

        hrtimer_init(&input_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        input_timer.function = input_timer_function;

#ifdef CONFIG_USE_OF
	if (pdev->dev.of_node) {
		node = pdev->dev.of_node;
		of_property_read_string(node, "product", &product);
		of_property_read_string(node, "serialno", &serialno);
		of_property_read_string(node, "mac_addr", &mac_addr);
	}
#endif

err_out:
        return error;
}

static  int odroid_sysfs_remove(struct platform_device *pdev)
{
#if defined(SLEEP_DISABLE_FLAG)
#if defined(CONFIG_HAS_WAKELOCK)
        wake_unlock(&sleep_wake_lock);
#endif
#endif
        return 0;
}

static int odroid_sysfs_suspend(struct platform_device *dev, pm_message_t state)
{
#if defined(DEBUG_PM_MSG)
        printk("%s\n", __FUNCTION__);
#endif

        return 0;
}

static int odroid_sysfs_resume(struct platform_device *dev)
{
#if defined(DEBUG_PM_MSG)
        printk("%s\n", __FUNCTION__);
#endif

        return  0;
}

#if defined(CONFIG_OF)
static const struct of_device_id odroid_sysfs_dt[] = {
        { .compatible = "odroid-sysfs" },
        { },
};
MODULE_DEVICE_TABLE(of, odroid_sysfs_dt);
#endif

static struct platform_driver odroid_sysfs_driver = {
        .driver = {
                .name = "odroid-sysfs",
                .owner = THIS_MODULE,
#if defined(CONFIG_OF)
                .of_match_table = of_match_ptr(odroid_sysfs_dt),
#endif
        },
        .probe = odroid_sysfs_probe,
        .remove = odroid_sysfs_remove,
        .suspend = odroid_sysfs_suspend,
        .resume = odroid_sysfs_resume,
};

static int __init odroid_sysfs_init(void)
{
        int error = class_register(&odroid_class);
        if (0 > error)
                return error;

        printk("--------------------------------------------------------\n");
#if defined(SLEEP_DISABLE_FLAG)
#if defined(CONFIG_HAS_WAKELOCK)
        printk("%s(%d) : Sleep Disable Flag SET!!(Wake_lock_init)\n",
                        __FUNCTION__, __LINE__);

        wake_lock_init(&sleep_wake_lock, WAKE_LOCK_SUSPEND, "sleep_wake_lock");
#endif
#else
        printk("%s(%d) : Sleep Enable !! \n", __FUNCTION__, __LINE__);
#endif
        printk("--------------------------------------------------------\n");

        return platform_driver_register(&odroid_sysfs_driver);
}

static void __exit odroid_sysfs_exit(void)
{
#if defined(SLEEP_DISABLE_FLAG)
#if defined(CONFIG_HAS_WAKELOCK)
        wake_lock_destroy(&sleep_wake_lock);
#endif
#endif
        platform_driver_unregister(&odroid_sysfs_driver);
        class_unregister(&odroid_class);
}

module_init(odroid_sysfs_init);
module_exit(odroid_sysfs_exit);
