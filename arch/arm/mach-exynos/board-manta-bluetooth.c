/*
 * Copyright (C) 2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/wakelock.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include <asm/mach-types.h>

#include <mach/gpio.h>
#include <plat/gpio-cfg.h>

#define GPIO_BT_WAKE		EXYNOS5_GPH1(3)
#define GPIO_BT_HOST_WAKE	EXYNOS5_GPX2(6)
#define GPIO_BTREG_ON		EXYNOS5_GPH0(0)

#define GPIO_BT_UART_RXD	EXYNOS5_GPA0(0)
#define GPIO_BT_UART_TXD	EXYNOS5_GPA0(1)
#define GPIO_BT_UART_CTS	EXYNOS5_GPA0(2)
#define GPIO_BT_UART_RTS	EXYNOS5_GPA0(3)

static struct rfkill *bt_rfkill;

static DEFINE_MUTEX(manta_bt_wlan_sync);

struct gpio_init_data {
	uint num;
	uint cfg;
	uint pull;
	uint drv;
};

static struct gpio_init_data manta_init_bt_gpios[] = {
	/* BT_UART_RXD */
	{GPIO_BT_UART_RXD, S3C_GPIO_SFN(2), S3C_GPIO_PULL_UP,
							S5P_GPIO_DRVSTR_LV2},
	/* BT_UART_TXD */
	{GPIO_BT_UART_TXD, S3C_GPIO_SFN(2), S3C_GPIO_PULL_NONE,
							S5P_GPIO_DRVSTR_LV2},
	/* BT_UART_CTS */
	{GPIO_BT_UART_CTS, S3C_GPIO_SFN(2), S3C_GPIO_PULL_NONE,
							S5P_GPIO_DRVSTR_LV2},
	/* BT_UART_RTS */
	{GPIO_BT_UART_RTS, S3C_GPIO_SFN(2), S3C_GPIO_PULL_NONE,
							S5P_GPIO_DRVSTR_LV2},
	/* BT_HOST_WAKE */
	{GPIO_BT_HOST_WAKE, S3C_GPIO_INPUT, S3C_GPIO_PULL_NONE,
							S5P_GPIO_DRVSTR_LV1}
};

static struct platform_device bcm43241_bluetooth_platform_device = {
	.name		= "bcm43241_bluetooth",
	.id		= -1,
};

static struct platform_device *manta_bt_devs[] __initdata = {
	&bcm43241_bluetooth_platform_device,
};

void __init exynos5_manta_bt_init(void)
{
	int i;
	int gpio;

	for (i = 0; i < ARRAY_SIZE(manta_init_bt_gpios); i++) {
		gpio = manta_init_bt_gpios[i].num;
		s3c_gpio_cfgpin(gpio, manta_init_bt_gpios[i].cfg);
		s3c_gpio_setpull(gpio, manta_init_bt_gpios[i].pull);
		s5p_gpio_set_drvstr(gpio, manta_init_bt_gpios[i].drv);
	}

	platform_add_devices(manta_bt_devs, ARRAY_SIZE(manta_bt_devs));
}

void bt_wlan_lock(void)
{
	mutex_lock(&manta_bt_wlan_sync);
}

void bt_wlan_unlock(void)
{
	mutex_unlock(&manta_bt_wlan_sync);
}

static int bcm43241_bt_rfkill_set_power(void *data, bool blocked)
{
	/* rfkill_ops callback. Turn transmitter on when blocked is false */
	bt_wlan_lock();
	msleep(300);
	if (!blocked) {
		pr_info("[BT] Bluetooth Power On.\n");
		gpio_set_value(GPIO_BTREG_ON, 1);
	} else {
		pr_info("[BT] Bluetooth Power Off.\n");
		gpio_set_value(GPIO_BTREG_ON, 0);
	}
	msleep(50);
	bt_wlan_unlock();
	return 0;
}

static const struct rfkill_ops bcm43241_bt_rfkill_ops = {
	.set_block = bcm43241_bt_rfkill_set_power,
};

static int bcm43241_bluetooth_probe(struct platform_device *pdev)
{
	int rc;

	rc = gpio_request(GPIO_BTREG_ON, "bcm43241_bten_gpio");
	if (unlikely(rc)) {
		pr_err("[BT] GPIO_BTREG_ON request failed.\n");
		goto err_gpio_btreg_on;
	}

	rc = gpio_request(GPIO_BT_WAKE, "bcm43241_btwake_gpio");
	if (unlikely(rc)) {
		pr_err("[BT] GPIO_BT_WAKE request failed.\n");
		goto err_gpio_bt_wake;
	}

	rc = gpio_request(GPIO_BT_HOST_WAKE, "bcm43241_bthostwake_gpio");
	if (unlikely(rc)) {
		pr_err("[BT] GPIO_BT_HOST_WAKE request failed.\n");
		goto err_gpio_bt_host_wake;
	}

	gpio_direction_input(GPIO_BT_HOST_WAKE);
	gpio_direction_output(GPIO_BT_WAKE, 0);
	gpio_direction_output(GPIO_BTREG_ON, 0);

	bt_rfkill = rfkill_alloc("bcm43241 Bluetooth", &pdev->dev,
				RFKILL_TYPE_BLUETOOTH, &bcm43241_bt_rfkill_ops,
				NULL);
	if (unlikely(!bt_rfkill)) {
		pr_err("[BT] bt_rfkill alloc failed.\n");
		rc =  -ENOMEM;
		goto err_rfkill_alloc;
	}

	rfkill_init_sw_state(bt_rfkill, false);
	rc = rfkill_register(bt_rfkill);
	if (unlikely(rc)) {
		pr_err("[BT] bt_rfkill register failed.\n");
		rc = -1;
		goto err_rfkill_register;
	}
	rfkill_set_sw_state(bt_rfkill, true);

	return rc;

err_rfkill_register:
	rfkill_destroy(bt_rfkill);
err_rfkill_alloc:
	gpio_free(GPIO_BT_HOST_WAKE);
err_gpio_bt_host_wake:
	gpio_free(GPIO_BT_WAKE);
err_gpio_bt_wake:
	gpio_free(GPIO_BTREG_ON);
err_gpio_btreg_on:
	return rc;
}

static int bcm43241_bluetooth_remove(struct platform_device *pdev)
{
	rfkill_unregister(bt_rfkill);
	rfkill_destroy(bt_rfkill);

	gpio_free(GPIO_BT_HOST_WAKE);
	gpio_free(GPIO_BT_WAKE);
	gpio_free(GPIO_BTREG_ON);

	return 0;
}

static struct platform_driver bcm43241_bluetooth_platform_driver = {
	.probe = bcm43241_bluetooth_probe,
	.remove = bcm43241_bluetooth_remove,
	.driver = {
		   .name = "bcm43241_bluetooth",
		   .owner = THIS_MODULE,
	},
};

static int __init bcm43241_bluetooth_init(void)
{
	return platform_driver_register(&bcm43241_bluetooth_platform_driver);
}

static void __exit bcm43241_bluetooth_exit(void)
{
	platform_driver_unregister(&bcm43241_bluetooth_platform_driver);
}


module_init(bcm43241_bluetooth_init);
module_exit(bcm43241_bluetooth_exit);

MODULE_ALIAS("platform:bcm43241");
MODULE_DESCRIPTION("bcm43241_bluetooth");
MODULE_LICENSE("GPL");
