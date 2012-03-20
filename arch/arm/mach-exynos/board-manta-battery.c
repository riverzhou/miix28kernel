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
#include <linux/i2c-gpio.h>
#include <linux/platform_device.h>

#include <plat/gpio-cfg.h>

#include <linux/platform_data/max17047_fuelgauge.h>
#include <linux/platform_data/bq24191_charger.h>
#include <linux/platform_data/manta_battery.h>

#include "board-manta.h"

#define	GPIO_FUEL_SCL_18V	EXYNOS5_GPD0(1)
#define	GPIO_FUEL_SDA_18V	EXYNOS5_GPD0(0)

#define	GPIO_USB_SEL1		EXYNOS5_GPH0(1)

#define	GPIO_TA_EN		EXYNOS5_GPG1(5)
#define	GPIO_TA_INT	EXYNOS5_GPX0(0)
#define	GPIO_TA_nCHG		EXYNOS5_GPG1(4)
#define	GPIO_CHG_SCL_18V	EXYNOS5_GPE0(2)
#define	GPIO_CHG_SDA_18V	EXYNOS5_GPE0(1)

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

static struct i2c_gpio_platform_data max17047_i2c_data_fuelgauge = {
	.sda_pin = GPIO_FUEL_SDA_18V,
	.scl_pin = GPIO_FUEL_SCL_18V,
};

static struct platform_device max17047_device_fuelgauge = {
	.name = "i2c-gpio",
	.id = 9,
	.dev.platform_data = &max17047_i2c_data_fuelgauge,
};

static struct max17047_platform_data max17047_fg_pdata = {
	.register_callbacks = max17047_fg_register_callbacks,
	.unregister_callbacks = max17047_fg_unregister_callbacks,
};

static struct i2c_board_info max17047_brdinfo_fuelgauge[] __initdata = {
	{
		I2C_BOARD_INFO("max17047-fuelgauge", 0x36),
		.platform_data	= &max17047_fg_pdata,
	},
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
	s3c_gpio_cfgpin(GPIO_TA_INT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TA_INT, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_TA_nCHG, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TA_nCHG, S3C_GPIO_PULL_UP);

	s3c_gpio_cfgpin(GPIO_TA_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TA_EN, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_TA_EN, 0);
}

static int check_samsung_charger(void)
{
	int result = false;
	int vol1, vol2, vol_avg;

	/* usb switch to check adc */
	s3c_gpio_cfgpin(GPIO_USB_SEL1, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_USB_SEL1, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_USB_SEL1, 0);

	msleep(100);

	vol1 = manta_stmpe811_read_adc_data(6);
	vol2 = manta_stmpe811_read_adc_data(6);

	if (vol1 >= 0 && vol2 >= 0) {
		vol_avg = (vol1 + vol2) / 2;

		/* ADC range was recommended by HW */
		result = vol_avg > 1000 && vol_avg < 1500;
	}

	msleep(50);

	/* usb switch to normal */
	gpio_set_value(GPIO_USB_SEL1, 1);

	pr_debug("%s : adc1(%d), adc2(%d), return(%d)\n", __func__,
							vol1, vol2, result);
	return result;
}

static void bq24191_change_cable_status(int gpio_ta_int)
{
	int is_ta;

	if (gpio_ta_int == 1) {
		is_ta = check_samsung_charger();
		if (is_ta == true)
			cable_type = CABLE_TYPE_AC;
		else
			cable_type = CABLE_TYPE_USB;
	} else {
		cable_type = CABLE_TYPE_NONE;
	}

	pr_debug("%s: ta_int(%d), cable_type(%d)", __func__,
		gpio_ta_int, cable_type);

	if (bat_callbacks && bat_callbacks->change_cable_status)
		bat_callbacks->change_cable_status(bat_callbacks, cable_type);
}

static struct i2c_gpio_platform_data bq24191_i2c_data_charger = {
	.sda_pin = GPIO_CHG_SDA_18V,
	.scl_pin = GPIO_CHG_SCL_18V,
};

static struct platform_device bq24191_device_charger = {
	.name = "i2c-gpio",
	.id = 10,
	.dev.platform_data = &bq24191_i2c_data_charger,
};

static struct bq24191_platform_data bq24191_chg_pdata = {
	.register_callbacks = bq24191_chg_register_callbacks,
	.unregister_callbacks = bq24191_chg_unregister_callbacks,
	.change_cable_status = bq24191_change_cable_status,
	.high_current_charging = 0x36,	/* input current limit 2A */
	.low_current_charging = 0x32,	/* input current linit 500mA */
	.chg_enable = 0x1d,
	.chg_disable = 0x0d,
	.gpio_ta_int = GPIO_TA_INT,
	.gpio_ta_nchg = GPIO_TA_nCHG,
	.gpio_ta_en = GPIO_TA_EN,
};

static struct i2c_board_info bq24191_brdinfo_charger[] __initdata = {
	{
		I2C_BOARD_INFO("bq24191-charger", 0x6a),
		.platform_data	= &bq24191_chg_pdata,
	},
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

static int manta_bat_get_temperature(void)
{
	if (fg_callbacks && fg_callbacks->get_temperature)
		return fg_callbacks->get_temperature(fg_callbacks);
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
	&max17047_device_fuelgauge,
	&bq24191_device_charger,
	&manta_device_battery,
};

void __init exynos5_manta_battery_init(void)
{
	charger_gpio_init();
	bq24191_brdinfo_charger[0].irq = gpio_to_irq(GPIO_TA_INT);

	platform_add_devices(manta_battery_devices,
		ARRAY_SIZE(manta_battery_devices));

	i2c_register_board_info(9, max17047_brdinfo_fuelgauge,
		ARRAY_SIZE(max17047_brdinfo_fuelgauge));

	i2c_register_board_info(10, bq24191_brdinfo_charger,
		ARRAY_SIZE(bq24191_brdinfo_charger));
}

