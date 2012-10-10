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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <asm/mach-types.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <linux/if.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <linux/mmc/host.h>
#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <mach/dwmci.h>
#include <mach/map.h>

#include <linux/random.h>
#include <linux/jiffies.h>

#define GPIO_WLAN_PMENA		EXYNOS5_GPV1(0)
#define GPIO_WLAN_IRQ		EXYNOS5_GPX2(5)

#define WLAN_SDIO_CMD		EXYNOS5_GPC2(1)
#define WLAN_SDIO_DATA0	EXYNOS5_GPC2(3)
#define WLAN_SDIO_DATA1	EXYNOS5_GPC2(4)
#define WLAN_SDIO_DATA2	EXYNOS5_GPC2(5)
#define WLAN_SDIO_DATA3	EXYNOS5_GPC2(6)

extern void bt_wlan_lock(void);
extern void bt_wlan_unlock(void);

static struct resource manta_wifi_resources[] = {
	[0] = {
		.name	= "bcmdhd_wlan_irq",
		.start	= GPIO_WLAN_IRQ,
		.end	= GPIO_WLAN_IRQ,
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL |
			  IORESOURCE_IRQ_SHAREABLE,
	},
};

struct wifi_gpio_sleep_data {
	uint num;
	uint cfg;
	uint pull;
};

static struct wifi_gpio_sleep_data manta_sleep_wifi_gpios[] = {
	/* WLAN_SDIO_CMD */
	{WLAN_SDIO_CMD, S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},
	/* WLAN_SDIO_D(0) */
	{WLAN_SDIO_DATA0, S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},
	/* WLAN_SDIO_D(1) */
	{WLAN_SDIO_DATA1, S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},
	/* WLAN_SDIO_D(2) */
	{WLAN_SDIO_DATA2, S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},
	/* WLAN_SDIO_D(3) */
	{WLAN_SDIO_DATA3, S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},
};

static void (*wifi_status_cb)(struct platform_device *, int state);

static int exynos5_manta_wlan_ext_cd_init(
			void (*notify_func)(struct platform_device *, int))
{
	wifi_status_cb = notify_func;
	return 0;
}

static int exynos5_manta_wlan_ext_cd_cleanup(
			void (*notify_func)(struct platform_device *, int))
{
	wifi_status_cb = NULL;
	return 0;
}

static int manta_wifi_set_carddetect(int val)
{
	pr_debug("%s: %d\n", __func__, val);

	if (wifi_status_cb)
		wifi_status_cb(&exynos5_device_dwmci1, val);
	else
		pr_warning("%s: Nobody to notify\n", __func__);

	return 0;
}

static int manta_wifi_power_state;

static int manta_wifi_power(int on)
{
	int ret = 0;

	bt_wlan_lock();
	pr_debug("%s: %d\n", __func__, on);

	if (on)
		msleep(600);
	else
		msleep(50);
	gpio_set_value(GPIO_WLAN_PMENA, on);
	if (on)
		msleep(500);
	else
		msleep(50);

	manta_wifi_power_state = on;
	bt_wlan_unlock();
	return ret;
}

static int manta_wifi_reset_state;

static int manta_wifi_reset(int on)
{
	pr_debug("%s: do nothing\n", __func__);
	manta_wifi_reset_state = on;
	return 0;
}

static unsigned char manta_mac_addr[IFHWADDRLEN] = { 0, 0x90, 0x4c, 0, 0, 0 };

static int __init manta_mac_addr_setup(char *str)
{
	char macstr[IFHWADDRLEN*3];
	char *macptr = macstr;
	char *token;
	int i = 0;

	if (!str)
		return 0;
	pr_debug("wlan MAC = %s\n", str);
	if (strlen(str) >= sizeof(macstr))
		return 0;
	strcpy(macstr, str);

	while ((token = strsep(&macptr, ":")) != NULL) {
		unsigned long val;
		int res;

		if (i >= IFHWADDRLEN)
			break;
		res = kstrtoul(token, 0x10, &val);
		if (res < 0)
			return 0;
		manta_mac_addr[i++] = (u8)val;
	}

	return 1;
}

__setup("androidboot.wifimacaddr=", manta_mac_addr_setup);

static int manta_wifi_get_mac_addr(unsigned char *buf)
{
	uint rand_mac;

	if (!buf)
		return -EFAULT;

	if ((manta_mac_addr[4] == 0) && (manta_mac_addr[5] == 0)) {
		srandom32((uint)jiffies);
		rand_mac = random32();
		manta_mac_addr[3] = (unsigned char)rand_mac;
		manta_mac_addr[4] = (unsigned char)(rand_mac >> 8);
		manta_mac_addr[5] = (unsigned char)(rand_mac >> 16);
	}
	memcpy(buf, manta_mac_addr, IFHWADDRLEN);
	return 0;
}

/* Customized Locale table : OPTIONAL feature */
#define WLC_CNTRY_BUF_SZ	4
struct cntry_locales_custom {
	char iso_abbrev[WLC_CNTRY_BUF_SZ];
	char custom_locale[WLC_CNTRY_BUF_SZ];
	int  custom_locale_rev;
};

static struct cntry_locales_custom manta_wifi_translate_custom_table[] = {
/* Table should be filled out based on custom platform regulatory requirement */
	{"",   "XY", 7},  /* universal */
	{"US", "US", 69}, /* input ISO "US" to : US regrev 69 */
	{"CA", "US", 69}, /* input ISO "CA" to : US regrev 69 */
	{"EU", "EU", 5},  /* European union countries */
	{"AT", "EU", 5},
	{"BE", "EU", 5},
	{"BG", "EU", 5},
	{"CY", "EU", 5},
	{"CZ", "EU", 5},
	{"DK", "EU", 5},
	{"EE", "EU", 5},
	{"FI", "EU", 5},
	{"FR", "EU", 5},
	{"DE", "EU", 5},
	{"GR", "EU", 5},
	{"HU", "EU", 5},
	{"IE", "EU", 5},
	{"IT", "EU", 5},
	{"LV", "EU", 5},
	{"LI", "EU", 5},
	{"LT", "EU", 5},
	{"LU", "EU", 5},
	{"MT", "EU", 5},
	{"NL", "EU", 5},
	{"PL", "EU", 5},
	{"PT", "EU", 5},
	{"RO", "EU", 5},
	{"SK", "EU", 5},
	{"SI", "EU", 5},
	{"ES", "EU", 5},
	{"SE", "EU", 5},
	{"GB", "EU", 5},  /* input ISO "GB" to : EU regrev 05 */
	{"IL", "IL", 0},
	{"CH", "CH", 0},
	{"TR", "TR", 0},
	{"NO", "NO", 0},
	{"KR", "KR", 25},
	{"AU", "XY", 3},
	{"CN", "CN", 0},
	{"TW", "XY", 3},
	{"AR", "XY", 3},
	{"MX", "XY", 3},
	{"JP", "EU", 0},
	{"BR", "KR", 25}
};

static void *manta_wifi_get_country_code(char *ccode)
{
	int size = ARRAY_SIZE(manta_wifi_translate_custom_table);
	int i;

	if (!ccode)
		return NULL;

	for (i = 0; i < size; i++)
		if (strcmp(ccode,
			manta_wifi_translate_custom_table[i].iso_abbrev) == 0)
			return &manta_wifi_translate_custom_table[i];
	return &manta_wifi_translate_custom_table[0];
}

static struct wifi_platform_data manta_wifi_control = {
	.set_power		= manta_wifi_power,
	.set_reset		= manta_wifi_reset,
	.set_carddetect		= manta_wifi_set_carddetect,
	.mem_prealloc		= NULL,
	.get_mac_addr		= manta_wifi_get_mac_addr,
	.get_country_code	= manta_wifi_get_country_code,
};

static struct platform_device manta_wifi_device = {
	.name		= "bcmdhd_wlan",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(manta_wifi_resources),
	.resource	= manta_wifi_resources,
	.dev		= {
		.platform_data = &manta_wifi_control,
	},
};

static void exynos5_setup_wlan_cfg_gpio(int width)
{
	unsigned int gpio;

	/* Set all the necessary GPC2[0:1] pins to special-function 2 */
	for (gpio = EXYNOS5_GPC2(0); gpio < EXYNOS5_GPC2(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	}

	for (gpio = EXYNOS5_GPC2(3); gpio <= EXYNOS5_GPC2(6); gpio++) {
		/* Data pin GPC2[3:6] to special-function 2 */
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	}
}

static struct dw_mci_board exynos_wlan_pdata __initdata = {
	.num_slots		= 1,
	.cd_type                = DW_MCI_CD_EXTERNAL,
	.quirks			= DW_MCI_QUIRK_HIGHSPEED |
				  DW_MCI_QUIRK_IDMAC_DTO,
	.bus_hz			= 50 * 1000 * 1000,
	.max_bus_hz		= 200 * 1000 * 1000,
	.caps			= MMC_CAP_UHS_SDR104 |
				  MMC_CAP_4_BIT_DATA | MMC_CAP_SD_HIGHSPEED,
	.caps2			= MMC_CAP2_BROKEN_VOLTAGE,
	.pm_caps		= MMC_PM_KEEP_POWER | MMC_PM_IGNORE_PM_NOTIFY,
	.fifo_depth		= 0x80,
	.detect_delay_ms	= 0,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos5_setup_wlan_cfg_gpio,
	.ext_cd_init		= exynos5_manta_wlan_ext_cd_init,
	.ext_cd_cleanup		= exynos5_manta_wlan_ext_cd_cleanup,
	.sdr_timing		= 0x03040002,
	.ddr_timing		= 0x03030002,
	.clk_drv 		= 2,
};

static struct platform_device *manta_wlan_devs[] __initdata = {
	&exynos5_device_dwmci1,
	&manta_wifi_device,
};

static void __init manta_wlan_gpio(void)
{
	int gpio;
	int i;

	pr_debug("%s: start\n", __func__);

	/* Setup wlan Power Enable */
	gpio = GPIO_WLAN_PMENA;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
	/* Keep power state during suspend */
	s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_PREV_STATE);
	gpio_set_value(gpio, 0);

	/* Setup wlan IRQ */
	gpio = GPIO_WLAN_IRQ;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);

	manta_wifi_resources[0].start = gpio_to_irq(gpio);
	manta_wifi_resources[0].end = gpio_to_irq(gpio);

	/* Setup sleep GPIO for wifi */
	for (i = 0; i < ARRAY_SIZE(manta_sleep_wifi_gpios); i++) {
		gpio = manta_sleep_wifi_gpios[i].num;
		s5p_gpio_set_pd_cfg(gpio, manta_sleep_wifi_gpios[i].cfg);
		s5p_gpio_set_pd_pull(gpio, manta_sleep_wifi_gpios[i].pull);
	}

}

void __init exynos5_manta_wlan_init(void)
{
	pr_debug("%s: start\n", __func__);

	exynos_dwmci_set_platdata(&exynos_wlan_pdata, 1);
	dev_set_name(&exynos5_device_dwmci1.dev, "exynos4-sdhci.1");
	clk_add_alias("dwmci", "dw_mmc.1", "hsmmc", &exynos5_device_dwmci1.dev);
	clk_add_alias("sclk_dwmci", "dw_mmc.1", "sclk_mmc",
		      &exynos5_device_dwmci1.dev);
	manta_wlan_gpio();
	platform_add_devices(manta_wlan_devs, ARRAY_SIZE(manta_wlan_devs));
}
