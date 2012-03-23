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
#include <linux/mfd/wm8994/gpio.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/platform_data/es305.h>
#include <linux/platform_device.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>

#include <mach/gpio.h>

#include <plat/devs.h>

#define GPIO_ES305_WAKEUP	EXYNOS5_GPG0(3)
#define GPIO_ES305_RESET	EXYNOS5_GPG0(4)
#define GPIO_CODEC_LDO_EN	EXYNOS5_GPH1(1)

static struct es305_platform_data es305_pdata = {
	.gpio_wakeup = GPIO_ES305_WAKEUP,
	.gpio_reset = GPIO_ES305_RESET,
	.passthrough_src = 1, /* port A */
	.passthrough_dst = 4, /* port D */
};

static struct i2c_board_info i2c_devs4[] __initdata = {
	{
		I2C_BOARD_INFO("audience_es305", 0x3e),
		.platform_data = &es305_pdata,
	},
};

static struct regulator_consumer_supply vbatt_supplies[] = {
	REGULATOR_SUPPLY("LDO1VDD", "7-001a"),
	REGULATOR_SUPPLY("SPKVDD1", "7-001a"),
	REGULATOR_SUPPLY("SPKVDD2", "7-001a"),
};

static struct regulator_init_data vbatt_initdata = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies = ARRAY_SIZE(vbatt_supplies),
	.consumer_supplies = vbatt_supplies,
};

static struct fixed_voltage_config vbatt_config = {
	.init_data = &vbatt_initdata,
	.microvolts = 3700000,
	.supply_name = "VBATT",
	.gpio = -EINVAL,
};

static struct platform_device vbatt_device = {
	.name = "reg-fixed-voltage",
	.id = -1,
	.dev = {
		.platform_data = &vbatt_config,
	},
};

static struct regulator_consumer_supply wm1811_ldo1_supplies[] = {
	REGULATOR_SUPPLY("AVDD1", "7-001a"),
};

static struct regulator_init_data wm1811_ldo1_initdata = {
	.constraints = {
		.name = "WM1811 LDO1",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(wm1811_ldo1_supplies),
	.consumer_supplies = wm1811_ldo1_supplies,
};

static struct regulator_consumer_supply wm1811_ldo2_supplies[] = {
	REGULATOR_SUPPLY("DCVDD", "7-001a"),
};

static struct regulator_init_data wm1811_ldo2_initdata = {
	.constraints = {
		.name = "WM1811 LDO2",
		.always_on = true,  /* Actually status changed by LDO1 */
	},
	.num_consumer_supplies = ARRAY_SIZE(wm1811_ldo2_supplies),
	.consumer_supplies = wm1811_ldo2_supplies,
};

static struct wm8994_pdata wm1811_pdata = {
	.gpio_defaults = {
		[0] = WM8994_GP_FN_IRQ, /* GPIO1 IRQ output, CMOS mode */
		[7] = WM8994_GPN_DIR |
				WM8994_GP_FN_PIN_SPECIFIC, /* DACDAT3 */
		[8] = WM8994_CONFIGURE_GPIO |
				WM8994_GP_FN_PIN_SPECIFIC, /* ADCDAT3 */
		[9] = WM8994_CONFIGURE_GPIO |
				WM8994_GP_FN_PIN_SPECIFIC, /* LRCLK3 */
		[10] = WM8994_CONFIGURE_GPIO |
				WM8994_GP_FN_PIN_SPECIFIC, /* BCLK3 */
	},

	/* The enable is shared but assign it to LDO1 for software */
	.ldo = {
		{
			.enable = GPIO_CODEC_LDO_EN,
			.init_data = &wm1811_ldo1_initdata,
		},
		{
			.init_data = &wm1811_ldo2_initdata,
		},
	},

	/* Regulated mode at highest output voltage */
	.micbias = {0x2f, 0x2f},
	.micd_lvl_sel = 0xff,

	.ldo_ena_always_driven = true,
};

static struct i2c_board_info i2c_devs7[] __initdata = {
	{
		I2C_BOARD_INFO("wm1811", (0x34 >> 1)), /* Audio codec */
		.platform_data = &wm1811_pdata,
		.irq = IRQ_EINT(29),
	},
};

static struct platform_device manta_i2s_device = {
	.name	= "manta-i2s",
	.id	= -1,
};

static struct platform_device *manta_audio_devices[] __initdata = {
	&vbatt_device,
	&samsung_asoc_dma,
	&samsung_asoc_idma,
	&exynos5_device_srp,
	&exynos5_device_i2s0,
	&exynos5_device_pcm0,
	&exynos5_device_spdif,
	&manta_i2s_device,
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
	clk_add_alias("system_clk", "manta-i2s", "clkout", NULL);
	clk_put(clkout);
	clk_put(xxti);
}

void __init exynos5_manta_audio_init(void)
{
	manta_audio_setup_clocks();

	i2c_register_board_info(4, i2c_devs4, ARRAY_SIZE(i2c_devs4));
	i2c_register_board_info(7, i2c_devs7, ARRAY_SIZE(i2c_devs7));

	platform_add_devices(manta_audio_devices,
				ARRAY_SIZE(manta_audio_devices));
}
