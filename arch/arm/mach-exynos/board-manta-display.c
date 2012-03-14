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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/pwm_backlight.h>

#include <video/platform_lcd.h>
#include <video/s5p-dp.h>

#include <plat/backlight.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/dp.h>
#include <plat/fb.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-fb-v4.h>

#include <mach/gpio.h>
#include <mach/map.h>
#include <mach/regs-clock.h>

#include "common.h"

#define GPIO_LCD_EN		EXYNOS5_GPH1(7)
#define GPIO_LCD_ID		EXYNOS5_GPD1(4)
#define GPIO_LCD_PWM_IN_18V	EXYNOS5_GPB2(0)
#define GPIO_LED_BL_RST		EXYNOS5_GPG0(5)

#define GPIO_LCDP_SCL__18V	EXYNOS5_GPD0(6)
#define GPIO_LCDP_SDA__18V	EXYNOS5_GPD0(7)

static void manta_lcd_set_power(struct plat_lcd_data *pd,
				unsigned int power)
{
	if (power) {
		gpio_set_value(GPIO_LCD_EN, 1);
		usleep_range(250000, 250000);
	} else {
		gpio_set_value(GPIO_LCD_EN, 0);
	}
}

static struct plat_lcd_data manta_lcd_data = {
	.set_power	= manta_lcd_set_power,
};

static struct platform_device manta_lcd = {
	.name	= "platform-lcd",
	.dev	= {
		.parent		= &s5p_device_fimd1.dev,
		.platform_data	= &manta_lcd_data,
	},
};

static void manta_fimd_gpio_setup_24bpp(void)
{
	u32 reg;

	/* basic fimd init */
	exynos5_fimd1_gpio_setup_24bpp();

	/* MPLL => FIMD Bus clock */
	reg = __raw_readl(EXYNOS5_CLKSRC_TOP0);
	reg = (reg & ~(0x3<<14)) | (0x0<<14);
	__raw_writel(reg, EXYNOS5_CLKSRC_TOP0);

	reg = __raw_readl(EXYNOS5_CLKDIV_TOP0);
	reg = (reg & ~(0x7<<28)) | (0x2<<28);
	__raw_writel(reg, EXYNOS5_CLKDIV_TOP0);

	/* Reference clcok selection for DPTX_PHY: pad_osc_clk_24M */
	reg = __raw_readl(S3C_VA_SYS + 0x04d4);
	reg = (reg & ~(0x1 << 0)) | (0x0 << 0);
	__raw_writel(reg, S3C_VA_SYS + 0x04d4);

	/* DPTX_PHY: XXTI */
	reg = __raw_readl(S3C_VA_SYS + 0x04d8);
	reg = (reg & ~(0x1 << 3)) | (0x0 << 3);
	__raw_writel(reg, S3C_VA_SYS + 0x04d8);
}

static struct s3c_fb_pd_win manta_fb_win2 = {
	.win_mode = {
		.left_margin	= 80,
		.right_margin	= 48,
		.upper_margin	= 37,
		.lower_margin	= 3,
		.hsync_len	= 32,
		.vsync_len	= 6,
		.xres		= 2560,
		.yres		= 1600,
	},
	.virtual_x		= 2560,
	.virtual_y		= 1600 * 2,
	.max_bpp		= 32,
	.default_bpp		= 24,
	.width			= 218,
	.height			= 136,
};

static struct s3c_fb_platdata manta_lcd1_pdata __initdata = {
	.win[0]		= &manta_fb_win2,
	.win[1]		= &manta_fb_win2,
	.win[2]		= &manta_fb_win2,
	.default_win	= 2,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
	.vidcon1	= 0,
	.setup_gpio	= manta_fimd_gpio_setup_24bpp,
};

static struct video_info manta_dp_config = {
	.name			= "WQXGA(2560x1600) LCD",

	.h_sync_polarity	= 0,
	.v_sync_polarity	= 0,
	.interlaced		= 0,

	.color_space		= COLOR_RGB,
	.dynamic_range		= VESA,
	.ycbcr_coeff		= COLOR_YCBCR601,
	.color_depth		= COLOR_8,

	.link_rate		= LINK_RATE_2_70GBPS,
	.lane_count		= LANE_COUNT4,
};

static void manta_backlight_on(void)
{
	gpio_set_value(GPIO_LED_BL_RST, 1);
}

static void manta_backlight_off(void)
{
	/* LED_BACKLIGHT_RESET: XCI1RGB_5 => GPG0_5 */
	gpio_set_value(GPIO_LED_BL_RST, 0);
}

static struct s5p_dp_platdata manta_dp_data __initdata = {
	.video_info     = &manta_dp_config,
	.phy_init       = s5p_dp_phy_init,
	.phy_exit       = s5p_dp_phy_exit,
	.backlight_on   = manta_backlight_on,
	.backlight_off  = manta_backlight_off,
};

/* LCD Backlight data */
static struct samsung_bl_gpio_info manta_bl_gpio_info = {
	.no	= GPIO_LCD_PWM_IN_18V,
	.func	= S3C_GPIO_SFN(2),
};

static struct platform_pwm_backlight_data manta_bl_data = {
	.pwm_id		= 0,
	.pwm_period_ns	= 10000,
};

static struct platform_device exynos_device_md0 = {
	.name = "exynos-mdev",
	.id = 0,
};

static struct platform_device exynos_device_md1 = {
	.name = "exynos-mdev",
	.id = 1,
};

static struct platform_device exynos_device_md2 = {
	.name = "exynos-mdev",
	.id = 2,
};

static struct platform_device *manta_display_devices[] __initdata = {
	&exynos_device_md0,
	&exynos_device_md1,
	&exynos_device_md2,

	&s5p_device_fimd1,
	&manta_lcd,
	&s5p_device_dp,
};

void __init exynos5_manta_display_init(void)
{
	/* LCD_EN , XMMC2CDN => GPC2_2 */
	gpio_request_one(GPIO_LCD_EN, GPIOF_OUT_INIT_LOW, "LCD_EN");
	/* LED_BACKLIGHT_RESET: XCI1RGB_5 => GPG0_5 */
	gpio_request_one(GPIO_LED_BL_RST, GPIOF_OUT_INIT_LOW, "LED_BL_RST");

	samsung_bl_set(&manta_bl_gpio_info, &manta_bl_data);
	s5p_fimd1_set_platdata(&manta_lcd1_pdata);
	dev_set_name(&s5p_device_fimd1.dev, "exynos5-fb.1");
	clk_add_alias("lcd", "exynos5-fb.1", "fimd", &s5p_device_fimd1.dev);
	s5p_dp_set_platdata(&manta_dp_data);

	platform_add_devices(manta_display_devices,
			     ARRAY_SIZE(manta_display_devices));

	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev, "sclk_fimd",
				  "mout_mpll_user", 267 * MHZ);
}
