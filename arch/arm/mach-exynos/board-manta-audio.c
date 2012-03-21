/*
 * Copyright (C) 2012 Google, Inc.
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
 *
 */

#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/platform_data/es305.h>

#include <mach/gpio.h>

#define GPIO_ES305_WAKEUP	EXYNOS5_GPG0(3)
#define GPIO_ES305_RESET	EXYNOS5_GPG0(4)

static struct es305_platform_data es305_pdata = {
	.gpio_wakeup = EXYNOS5_GPG0(3),
	.gpio_reset = EXYNOS5_GPG0(4),
	.passthrough_src = 1, /* port A */
	.passthrough_dst = 4, /* port D */
};

static struct i2c_board_info i2c_devs4[] __initdata = {
	{
		I2C_BOARD_INFO("audience_es305", 0x3e),
		.platform_data = &es305_pdata,
	},
};

static void manta_audio_setup_clocks(void)
{
	struct clk *xxti;
	struct clk *clkout;

	xxti = clk_get(NULL, "xxti");
	if (IS_ERR(xxti)) {
		pr_err("%s: cannot get xxti clock\n", __func__);
		return;
	}

	clkout = clk_get(NULL, "clkout");
	if (IS_ERR(clkout)) {
		pr_err("%s: cannot get clkout\n", __func__);
		clk_put(xxti);
		return;
	}

	clk_set_parent(clkout, xxti);
	clk_add_alias("system_clk", "4-003e", "clkout", NULL);
	clk_put(clkout);
	clk_put(xxti);
}

void __init exynos5_manta_audio_init(void)
{
	manta_audio_setup_clocks();

	i2c_register_board_info(4, i2c_devs4, ARRAY_SIZE(i2c_devs4));
}
