/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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

#include <linux/bh1721fvc.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>

static struct i2c_board_info __initdata manta_sensors_i2c1_boardinfo[] = {
	{
		I2C_BOARD_INFO("bmp182", 0x77),
	},
};

#define GPIO_ALS_NRST	EXYNOS5_GPH1(2)

static struct bh1721fvc_platform_data bh1721fvc_pdata = {
	.reset_pin = GPIO_ALS_NRST,
};

static struct i2c_board_info __initdata manta_sensors_i2c2_boardinfo[] = {
	{
		I2C_BOARD_INFO("bh1721fvc", 0x23),
		.platform_data = &bh1721fvc_pdata,
	},
};

void __init exynos5_manta_sensors_init(void)
{
	i2c_register_board_info(1, manta_sensors_i2c1_boardinfo,
		ARRAY_SIZE(manta_sensors_i2c1_boardinfo));

	s3c_gpio_setpull(GPIO_ALS_NRST, S3C_GPIO_PULL_NONE);
	i2c_register_board_info(2, manta_sensors_i2c2_boardinfo,
		ARRAY_SIZE(manta_sensors_i2c2_boardinfo));
}
