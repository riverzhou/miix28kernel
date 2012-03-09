/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/errno.h>
#include <linux/cma.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <linux/input.h>
#include <linux/mmc/host.h>
#include <linux/persistent_ram.h>
#include <linux/platform_device.h>
#include <linux/platform_data/exynos_usb3_drd.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/serial_core.h>
#include <linux/i2c.h>

#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>

#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/regs-serial.h>
#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/sdhci.h>

#include <mach/map.h>
#include <mach/sysmmu.h>
#include <mach/exynos-ion.h>
#include <mach/dwmci.h>

#include "board-manta.h"
#include "common.h"

static struct platform_device ramconsole_device = {
	.name           = "ram_console",
	.id             = -1,
};

static struct platform_device persistent_trace_device = {
	.name           = "persistent_trace",
	.id             = -1,
};

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define MANTA_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define MANTA_ULCON_DEFAULT	S3C2410_LCON_CS8

#define MANTA_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg manta_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= MANTA_UCON_DEFAULT,
		.ulcon		= MANTA_ULCON_DEFAULT,
		.ufcon		= MANTA_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= MANTA_UCON_DEFAULT,
		.ulcon		= MANTA_ULCON_DEFAULT,
		.ufcon		= MANTA_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= MANTA_UCON_DEFAULT,
		.ulcon		= MANTA_ULCON_DEFAULT,
		.ufcon		= MANTA_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= MANTA_UCON_DEFAULT,
		.ulcon		= MANTA_ULCON_DEFAULT,
		.ufcon		= MANTA_UFCON_DEFAULT,
	},
};

static struct gpio_event_direct_entry manta_keypad_key_map[] = {
	{
		.gpio   = EXYNOS5_GPX2(7),
		.code   = KEY_POWER,
	},
	{
		.gpio   = EXYNOS5_GPX2(0),
		.code   = KEY_VOLUMEUP,
	},
	{
		.gpio   = EXYNOS5_GPX2(1),
		.code   = KEY_VOLUMEDOWN,
	}
};

static struct gpio_event_input_info manta_keypad_key_info = {
	.info.func              = gpio_event_input_func,
	.info.no_suspend        = true,
	.debounce_time.tv64	= 5 * NSEC_PER_MSEC,
	.type                   = EV_KEY,
	.keymap                 = manta_keypad_key_map,
	.keymap_size            = ARRAY_SIZE(manta_keypad_key_map)
};

static struct gpio_event_info *manta_keypad_input_info[] = {
	&manta_keypad_key_info.info,
};

static struct gpio_event_platform_data manta_keypad_data = {
	.names  = {
		"manta-keypad",
		NULL,
	},
	.info           = manta_keypad_input_info,
	.info_count     = ARRAY_SIZE(manta_keypad_input_info),
};

static struct platform_device manta_keypad_device = {
	.name   = GPIO_EVENT_DEV_NAME,
	.id     = 0,
	.dev    = {
		.platform_data = &manta_keypad_data,
	},
};

static void __init manta_gpio_power_init(void)
{
	int err = 0;

	err = gpio_request_one(EXYNOS5_GPX2(7), 0, "GPX2(7)");
	if (err) {
		printk(KERN_ERR "failed to request GPX2(7) for "
				"suspend/resume control\n");
		return;
	}
	s3c_gpio_setpull(EXYNOS5_GPX2(7), S3C_GPIO_PULL_NONE);

	gpio_free(EXYNOS5_GPX2(7));
}

/* defined in arch/arm/mach-exynos/reserve-mem.c */
extern void exynos_cma_region_reserve(struct cma_region *,
				struct cma_region *, size_t, const char *);

static void __init exynos_reserve_mem(void)
{
	static struct cma_region regions[] = {
		{
			.name = "ion",
			.size = CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE * SZ_1K,
			{
				.alignment = SZ_1M
			}
		},
		{
			.size = 0 /* END OF REGION DEFINITIONS */
		}
	};

	static const char map[] __initconst = "ion-exynos=ion;";

	exynos_cma_region_reserve(regions, NULL, 0, map);
}

static void exynos_dwmci_cfg_gpio(int width)
{
	unsigned int gpio;

	for (gpio = EXYNOS5_GPC0(0); gpio < EXYNOS5_GPC0(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	}

	switch (width) {
	case 8:
		for (gpio = EXYNOS5_GPC1(3); gpio <= EXYNOS5_GPC1(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(4));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
		}
	case 4:
		for (gpio = EXYNOS5_GPC0(3); gpio <= EXYNOS5_GPC0(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
		}
		break;
	case 1:
		gpio = EXYNOS5_GPC0(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	default:
		break;
	}
}

static struct dw_mci_board exynos_dwmci_pdata __initdata = {
	.num_slots		= 1,
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION |
				  DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 66 * 1000 * 1000,
	.caps			= MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR |
				  MMC_CAP_8_BIT_DATA | MMC_CAP_CMD23,
	.fifo_depth             = 0x80,
	.detect_delay_ms	= 200,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos_dwmci_cfg_gpio,
};

#ifdef CONFIG_S3C_DEV_HSMMC3
void exynos5_setup_sdhci3_cfg_gpio(struct platform_device *dev, int width)
{
	unsigned int gpio;

	/* Set all the necessary GPC3[0:1] pins to special-function 2 */
	for (gpio = EXYNOS5_GPC3(0); gpio < EXYNOS5_GPC3(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}

	for (gpio = EXYNOS5_GPC3(3); gpio <= EXYNOS5_GPC3(6); gpio++) {
		/* Data pin GPC3[3:6] to special-function 2 */
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}
}

static struct s3c_sdhci_platdata manta_hsmmc3_pdata __initdata = {
	.cd_type		= S3C_SDHCI_CD_EXTERNAL,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
	.cfg_gpio		= exynos5_setup_sdhci3_cfg_gpio,
};
#endif

static struct platform_device *manta_devices[] __initdata = {
	&ramconsole_device,
	&persistent_trace_device,
	&s3c_device_rtc,
	&s3c_device_i2c3,
	&s3c_device_i2c5,
	&manta_keypad_device,
	&exynos5_device_dwmci,
#ifdef CONFIG_S3C_DEV_HSMMC3
	&s3c_device_hsmmc3,
#endif
	&exynos_device_ion,
	&exynos_device_ss_udc,
};

static struct exynos_usb3_drd_pdata manta_ss_udc_pdata;

static void __init manta_ss_udc_init(void)
{
	struct exynos_usb3_drd_pdata *pdata = &manta_ss_udc_pdata;

	exynos_ss_udc_set_platdata(pdata);

	gpio_request_one(EXYNOS5_GPH0(1), GPIOF_INIT_HIGH, "usb_sel");
	gpio_request_one(EXYNOS5_GPC2(2), GPIOF_INIT_HIGH, "usb3.0_en");
}

static void __init manta_map_io(void)
{
	clk_xusbxti.rate = 24000000;
	clk_xxti.rate = 24000000;
	exynos_init_io(NULL, 0);
	s3c24xx_init_clocks(clk_xusbxti.rate);
	s3c24xx_init_uarts(manta_uartcfgs, ARRAY_SIZE(manta_uartcfgs));
}

static void __init manta_sysmmu_init(void)
{
}

static struct persistent_ram_descriptor manta_prd[] __initdata = {
	{
		.name = "ram_console",
		.size = SZ_2M,
	},
#ifdef CONFIG_PERSISTENT_TRACER
	{
		.name = "persistent_trace",
		.size = SZ_1M,
	},
#endif
};

static struct persistent_ram manta_pr __initdata = {
	.descs = manta_prd,
	.num_descs = ARRAY_SIZE(manta_prd),
	.start = PLAT_PHYS_OFFSET + SZ_1G + SZ_512M,
#ifdef CONFIG_PERSISTENT_TRACER
	.size = 3 * SZ_1M,
#else
	.size = SZ_2M,
#endif
};

static void __init manta_init_early(void)
{
	persistent_ram_early_init(&manta_pr);
}

static void __init manta_machine_init(void)
{
	manta_sysmmu_init();
	exynos_ion_set_platdata();
	exynos_dwmci_set_platdata(&exynos_dwmci_pdata);
#ifdef CONFIG_S3C_DEV_HSMMC3
	s3c_sdhci3_set_platdata(&manta_hsmmc3_pdata);
#endif
	s3c_i2c3_set_platdata(NULL);
	s3c_i2c5_set_platdata(NULL);

	manta_gpio_power_init();

	manta_ss_udc_init();

	platform_add_devices(manta_devices, ARRAY_SIZE(manta_devices));

	exynos5_manta_power_init();
	exynos5_manta_display_init();
	exynos5_manta_input_init();
	exynos5_manta_battery_init();
}

MACHINE_START(MANTA, "Manta")
	.atag_offset	= 0x100,
	.init_early	= manta_init_early,
	.init_irq	= exynos5_init_irq,
	.map_io		= manta_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= manta_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos5_restart,
	.reserve	= exynos_reserve_mem,
MACHINE_END
