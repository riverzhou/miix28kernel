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
#include <plat/sdhci.h>
#include <mach/map.h>

#include <linux/random.h>
#include <linux/jiffies.h>

#define GPIO_WLAN_PMENA		EXYNOS5_GPV1(0)
#define GPIO_WLAN_IRQ		EXYNOS5_GPX2(5)

static struct resource manta_wifi_resources[] = {
	[0] = {
		.name	= "bcmdhd_wlan_irq",
		.start	= GPIO_WLAN_IRQ,
		.end	= GPIO_WLAN_IRQ,
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL |
			  IORESOURCE_IRQ_SHAREABLE,
	},
};

static void (*wifi_status_cb)(struct platform_device *, int state);
static struct platform_device *manta_wifi_dev;

static int exynos5_manta_wlan_ext_cd_init(
				void (*notify_func)(struct platform_device *,
				int state))
{
	manta_wifi_dev = &s3c_device_hsmmc3;
	wifi_status_cb = notify_func;
	return 0;
}

static int exynos5_manta_wlan_ext_cd_cleanup(
				void (*notify_func)(struct platform_device *,
				int state))
{
	wifi_status_cb = NULL;
	return 0;
}

static int manta_wifi_set_carddetect(int val)
{
	pr_debug("%s: %d\n", __func__, val);
	if (wifi_status_cb)
		wifi_status_cb(manta_wifi_dev, val);
	else
		pr_warning("%s: Nobody to notify\n", __func__);
	return 0;
}

static int manta_wifi_power_state;

static int manta_wifi_power(int on)
{
	int ret = 0;

	pr_debug("%s: %d\n", __func__, on);

	msleep(300);
	gpio_set_value(GPIO_WLAN_PMENA, on);
	msleep(200);

	manta_wifi_power_state = on;
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

__setup("androidboot.macaddr=", manta_mac_addr_setup);

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
	{"",   "XY", 4},  /* universal */
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
	.host_caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_SD_HIGHSPEED,
	.pm_caps		= MMC_PM_KEEP_POWER | MMC_PM_IGNORE_PM_NOTIFY,
	.cfg_gpio		= exynos5_setup_sdhci3_cfg_gpio,
	.ext_cd_init		= exynos5_manta_wlan_ext_cd_init,
	.ext_cd_cleanup		= exynos5_manta_wlan_ext_cd_cleanup,
};

static void __init manta_wlan_gpio(void)
{
	int gpio;

	pr_debug("%s: start\n", __func__);

	/* Setup wlan Power Enable */
	gpio = GPIO_WLAN_PMENA;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);

	/* Setup wlan IRQ */
	gpio = GPIO_WLAN_IRQ;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);

	manta_wifi_resources[0].start = gpio_to_irq(gpio);
	manta_wifi_resources[0].end = gpio_to_irq(gpio);
}

void __init exynos5_manta_wlan_init(void)
{
	pr_debug("%s: start\n", __func__);

	s3c_sdhci3_set_platdata(&manta_hsmmc3_pdata);
	platform_device_register(&s3c_device_hsmmc3);
	manta_wlan_gpio();
	platform_device_register(&manta_wifi_device);
}
