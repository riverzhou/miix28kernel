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

#include <linux/kernel.h>
#include <linux/i2c.h>

static struct i2c_board_info __initdata manta_sensors_i2c1_boardinfo[] = {
	{
		I2C_BOARD_INFO("bmp182", 0x77),
	},
};

void __init exynos5_manta_sensors_init(void)
{
	i2c_register_board_info(1, manta_sensors_i2c1_boardinfo,
		ARRAY_SIZE(manta_sensors_i2c1_boardinfo));
}
