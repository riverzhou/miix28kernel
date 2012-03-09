/* linux/arch/arm/mach-exynos/board-manta-battery.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/platform_device.h>

#include <plat/gpio-cfg.h>

#include <linux/platform_data/max17047_fuelgauge.h>

#include "board-manta.h"

#define	GPIO_FUEL_SCL_18V	EXYNOS5_GPD0(1)
#define	GPIO_FUEL_SDA_18V	EXYNOS5_GPD0(0)

static struct max17047_fg_callbacks *fg_callbacks;

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

static struct platform_device *manta_battery_devices[] __initdata = {
	&max17047_device_fuelgauge,
};

void __init exynos5_manta_battery_init(void)
{
	platform_add_devices(manta_battery_devices,
		ARRAY_SIZE(manta_battery_devices));

	i2c_register_board_info(9, max17047_brdinfo_fuelgauge,
		ARRAY_SIZE(max17047_brdinfo_fuelgauge));
}

