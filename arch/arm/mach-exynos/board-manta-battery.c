/* linux/arch/arm/mach-exynos/board-manta-battery.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>

#include <plat/gpio-cfg.h>

#include <linux/platform_data/max17047_fuelgauge.h>
#include <linux/platform_data/bq24191_charger.h>
#include <linux/power/smb347-charger.h>
#include <linux/platform_data/manta_battery.h>

#include "board-manta.h"

#define TA_ADC_LOW		800
#define TA_ADC_HIGH		1750

#define	GPIO_USB_SEL1		EXYNOS5_GPH0(1)

#define	GPIO_TA_EN		EXYNOS5_GPG1(5)
#define	GPIO_TA_INT		EXYNOS5_GPX0(0)
#define	GPIO_TA_nCHG_LUNCHBOX	EXYNOS5_GPG1(4)
#define	GPIO_TA_nCHG_ALPHA	EXYNOS5_GPX0(4)

static int gpio_TA_nCHG = GPIO_TA_nCHG_ALPHA;
static int cable_type;

static struct max17047_fg_callbacks *fg_callbacks;
static struct bq24191_chg_callbacks *chg_callbacks;
static struct manta_bat_callbacks *bat_callbacks;

static void max17047_fg_register_callbacks(struct max17047_fg_callbacks *ptr)
{
	fg_callbacks = ptr;
}

static void max17047_fg_unregister_callbacks(void)
{
	fg_callbacks = NULL;
}

static struct max17047_platform_data max17047_fg_pdata = {
	.register_callbacks = max17047_fg_register_callbacks,
	.unregister_callbacks = max17047_fg_unregister_callbacks,
};

static void bq24191_chg_register_callbacks(struct bq24191_chg_callbacks *ptr)
{
	chg_callbacks = ptr;
}

static void bq24191_chg_unregister_callbacks(void)
{
	chg_callbacks = NULL;
}

static void charger_gpio_init(void)
{
	int hw_rev = exynos5_manta_get_revision();

	gpio_TA_nCHG = hw_rev >= MANTA_REV_PRE_ALPHA ? GPIO_TA_nCHG_ALPHA
		: GPIO_TA_nCHG_LUNCHBOX;

	s3c_gpio_cfgpin(GPIO_TA_INT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TA_INT, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(gpio_TA_nCHG, S3C_GPIO_INPUT);
	s3c_gpio_setpull(gpio_TA_nCHG, hw_rev >= MANTA_REV_PRE_ALPHA ?
			 S3C_GPIO_PULL_NONE : S3C_GPIO_PULL_UP);

	s3c_gpio_cfgpin(GPIO_TA_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TA_EN, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_TA_EN, 0);
}

static int read_ta_adc(void)
{
	int vol1, vol2;
	int result;

	/* usb switch to check adc */
	s3c_gpio_cfgpin(GPIO_USB_SEL1, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_USB_SEL1, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_USB_SEL1, 0);

	msleep(100);

	vol1 = manta_stmpe811_read_adc_data(6);
	vol2 = manta_stmpe811_read_adc_data(6);

	if (vol1 >= 0 && vol2 >= 0)
		result = (vol1 + vol2) / 2;
	else
		result = -1;

	msleep(50);

	/* usb switch to normal */
	gpio_set_value(GPIO_USB_SEL1, 1);
	return result;
}

static int check_samsung_charger(void)
{
	int result = false;
	int ta_adc;

	ta_adc = read_ta_adc();

	/* ADC range was recommended by HW */
	result = ta_adc > TA_ADC_LOW && ta_adc < TA_ADC_HIGH;
	pr_debug("%s : adc(%d), return(%d)\n", __func__, ta_adc, result);
	return result;
}

static void change_cable_status(void)
{
	int ta_int = gpio_get_value(GPIO_TA_INT);
	int is_ta;

	if (ta_int) {
		is_ta = check_samsung_charger();
		if (is_ta == true)
			cable_type = CABLE_TYPE_AC;
		else
			cable_type = CABLE_TYPE_USB;
	} else {
		cable_type = CABLE_TYPE_NONE;
	}

	pr_debug("%s: ta_int(%d), cable_type(%d)", __func__,
		 ta_int, cable_type);

	if (bat_callbacks && bat_callbacks->change_cable_status)
		bat_callbacks->change_cable_status(bat_callbacks, cable_type);
}

static struct smb347_charger_platform_data smb347_chg_pdata = {
	.use_mains = true,
	.use_usb = true,
	.enable_control = SMB347_CHG_ENABLE_PIN_ACTIVE_LOW,
	.max_charge_current = 2000000,
	.max_charge_voltage = 4200000,
	.pre_charge_current = 200000,
	.termination_current = 150000,
	.pre_to_fast_voltage = 2600000,
	.mains_current_limit = 1800000,
	.usb_hc_current_limit = 1500000,
	.irq_gpio = GPIO_TA_nCHG_ALPHA,
};

static struct bq24191_platform_data bq24191_chg_pdata = {
	.register_callbacks = bq24191_chg_register_callbacks,
	.unregister_callbacks = bq24191_chg_unregister_callbacks,
	.high_current_charging = 0x06,	/* input current limit 2A */
	.low_current_charging = 0x32,	/* input current linit 500mA */
	.chg_enable = 0x1d,
	.chg_disable = 0x0d,
	.gpio_ta_nchg = GPIO_TA_nCHG_LUNCHBOX,
	.gpio_ta_en = GPIO_TA_EN,
};

static void manta_bat_register_callbacks(struct manta_bat_callbacks *ptr)
{
	bat_callbacks = ptr;
}

static void manta_bat_unregister_callbacks(void)
{
	bat_callbacks = NULL;
}

static int manta_bat_get_init_cable_state(void)
{
	return cable_type;
}

static void manta_bat_set_charging_current(int cable_type)
{
	if (chg_callbacks && chg_callbacks->set_charging_current)
		chg_callbacks->set_charging_current(chg_callbacks, cable_type);
}

static void manta_bat_set_charging_enable(int en)
{
	if (chg_callbacks && chg_callbacks->set_charging_enable)
		chg_callbacks->set_charging_enable(chg_callbacks, en);
}

static int manta_bat_get_capacity(void)
{
	if (fg_callbacks && fg_callbacks->get_capacity)
		return fg_callbacks->get_capacity(fg_callbacks);
	return -ENXIO;
}

static int manta_bat_get_temperature(int *temp_now)
{
	if (fg_callbacks && fg_callbacks->get_temperature)
		return fg_callbacks->get_temperature(fg_callbacks,
						     temp_now);
	return -ENXIO;
}

static int manta_bat_get_voltage_now(void)
{
	if (fg_callbacks && fg_callbacks->get_voltage_now)
		return fg_callbacks->get_voltage_now(fg_callbacks);
	return -ENXIO;
}

static int manta_bat_get_current_now(int *i_current)
{
	int ret = -ENXIO;

	if (fg_callbacks && fg_callbacks->get_current_now)
		ret = fg_callbacks->get_current_now(fg_callbacks, i_current);
	return ret;
}

static irqreturn_t ta_int_intr(int irq, void *arg)
{
	change_cable_status();
	return IRQ_HANDLED;
}

static struct manta_bat_platform_data manta_battery_pdata = {
	.register_callbacks = manta_bat_register_callbacks,
	.unregister_callbacks = manta_bat_unregister_callbacks,

	.get_init_cable_state = manta_bat_get_init_cable_state,

	.set_charging_current = manta_bat_set_charging_current,
	.set_charging_enable = manta_bat_set_charging_enable,

	.get_capacity = manta_bat_get_capacity,
	.get_temperature = manta_bat_get_temperature,
	.get_voltage_now = manta_bat_get_voltage_now,
	.get_current_now = manta_bat_get_current_now,

	.temp_high_threshold = 50000,	/* 50c */
	.temp_high_recovery = 42000,	/* 42c */
	.temp_low_recovery = 2000,		/* 2c */
	.temp_low_threshold = 0,		/* 0c */
};

static struct platform_device manta_device_battery = {
	.name = "manta-battery",
	.id = -1,
	.dev.platform_data = &manta_battery_pdata,
};

static struct platform_device *manta_battery_devices[] __initdata = {
	&manta_device_battery,
};

static int manta_power_debug_dump(struct seq_file *s, void *unused)
{
	seq_printf(s, "ta_en=%d ta_nchg=%d ta_int=%d ta_adc=%d cable=%d\n",
		   gpio_get_value(GPIO_TA_EN), gpio_get_value(gpio_TA_nCHG),
		   gpio_get_value(GPIO_TA_INT), read_ta_adc(), cable_type);
	return 0;
}

static int manta_power_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, manta_power_debug_dump, inode->i_private);
}

static const struct file_operations manta_power_debug_fops = {
	.open = manta_power_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct i2c_board_info i2c_devs2_common[] __initdata = {
	{
		I2C_BOARD_INFO("max17047-fuelgauge", 0x36),
		.platform_data	= &max17047_fg_pdata,
	},
};

static struct i2c_board_info i2c_devs2_lunchbox[] __initdata = {
	{
		I2C_BOARD_INFO("bq24191-charger", 0x6a),
		.platform_data	= &bq24191_chg_pdata,
	},
};

static struct i2c_board_info i2c_devs2_prealpha[] __initdata = {
	{
		I2C_BOARD_INFO("smb347", 0x0c >> 1),
		.platform_data  = &smb347_chg_pdata,
	},
};

void __init exynos5_manta_battery_init(void)
{
	int hw_rev = exynos5_manta_get_revision();

	charger_gpio_init();

	platform_add_devices(manta_battery_devices,
		ARRAY_SIZE(manta_battery_devices));

	i2c_register_board_info(2, i2c_devs2_common,
				ARRAY_SIZE(i2c_devs2_common));

	if (hw_rev  >= MANTA_REV_PRE_ALPHA)
		i2c_register_board_info(2, i2c_devs2_prealpha,
					ARRAY_SIZE(i2c_devs2_prealpha));
	else
		i2c_register_board_info(2, i2c_devs2_lunchbox,
					ARRAY_SIZE(i2c_devs2_lunchbox));

	if (IS_ERR_OR_NULL(debugfs_create_file("manta-power", S_IRUGO, NULL,
					       NULL, &manta_power_debug_fops)))
		pr_err("failed to create manta-power debugfs entry\n");
}

static int __init exynos5_manta_battery_late_init(void)
{
	int ret;

	ret = request_threaded_irq(gpio_to_irq(GPIO_TA_INT), NULL,
				   ta_int_intr,
				   IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				   IRQF_ONESHOT, "ta_int", NULL);
	if (ret) {
		pr_err("%s: ta_int register failed, ret=%d\n",
		       __func__, ret);
	} else {
		ret = enable_irq_wake(gpio_to_irq(GPIO_TA_INT));
		if (ret)
			pr_warn("%s: failed to enable irq_wake for ta_int\n",
				__func__);
	}

	/* Poll initial cable state */
	change_cable_status();
	return 0;
}

device_initcall(exynos5_manta_battery_late_init);
