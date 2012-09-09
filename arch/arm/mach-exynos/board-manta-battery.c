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
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/suspend.h>
#include <linux/pm_wakeup.h>

#include <plat/adc.h>
#include <plat/gpio-cfg.h>

#include <linux/platform_data/max17047_fuelgauge.h>
#include <linux/platform_data/bq24191_charger.h>
#include <linux/power/smb347-charger.h>
#include <linux/platform_data/android_battery.h>
#include <linux/platform_data/ds2482.h>

#include "board-manta.h"

#include <linux/slab.h>

#define TA_ADC_LOW		700
#define TA_ADC_HIGH		1750

#define ADC_NUM_SAMPLES		5
#define ADC_LIMIT_ERR_COUNT	5

#define	GPIO_USB_SEL1		EXYNOS5_GPH0(1)
#define	GPIO_TA_EN		EXYNOS5_GPG1(5)
#define	GPIO_TA_INT		EXYNOS5_GPX0(0)
#define	GPIO_TA_nCHG_LUNCHBOX	EXYNOS5_GPG1(4)
#define	GPIO_TA_nCHG_ALPHA	EXYNOS5_GPX0(4)
#define GPIO_OTG_VBUS_SENSE	EXYNOS5_GPX1(0)
#define GPIO_VBUS_POGO_5V	EXYNOS5_GPX1(2)
#define GPIO_OTG_VBUS_SENSE_FAC	EXYNOS5_GPB0(1)
#define GPIO_1WIRE_SLEEP	EXYNOS5_GPG0(0)

static int gpio_TA_nCHG = GPIO_TA_nCHG_ALPHA;

/* Temporary, need pogo driver API to share these */
#define GPIO_TA_CHECK_SEL	EXYNOS5_GPD1(5)
#define GPIO_POGO_SPDIF		EXYNOS5_GPB1(0)

enum charge_connector {
	CHARGE_CONNECTOR_POGO,
	CHARGE_CONNECTOR_USB,
	CHARGE_CONNECTOR_MAX,
};

static enum manta_charge_source charge_source;
static enum manta_charge_source manta_bat_charge_source[CHARGE_CONNECTOR_MAX];
static bool manta_bat_usb_online;
static bool manta_bat_pogo_online;
static bool manta_bat_otg_enabled;
static bool manta_bat_chg_enabled;
static bool manta_bat_chg_enable_synced;
static struct power_supply *manta_bat_smb347_mains;
static struct power_supply *manta_bat_smb347_usb;
static struct power_supply *manta_bat_smb347_battery;
static struct power_supply *manta_bat_ds2784_battery;

static struct max17047_fg_callbacks *fg_callbacks;
static struct bq24191_chg_callbacks *chg_callbacks;
static struct android_bat_callbacks *bat_callbacks;

static struct s3c_adc_client *ta_adc_client;

static struct wakeup_source manta_bat_vbus_ws;

static DEFINE_MUTEX(manta_bat_charger_detect_lock);
static DEFINE_MUTEX(manta_bat_adc_lock);

static inline int manta_source_to_android(enum manta_charge_source src)
{
	switch (src) {
	case MANTA_CHARGE_SOURCE_NONE:
		return CHARGE_SOURCE_NONE;
	case MANTA_CHARGE_SOURCE_USB:
		return CHARGE_SOURCE_USB;
	case MANTA_CHARGE_SOURCE_AC_SAMSUNG:
	case MANTA_CHARGE_SOURCE_AC_OTHER:
		return CHARGE_SOURCE_AC;
	default:
		break;
	}

	return CHARGE_SOURCE_NONE;
}

static inline int manta_bat_get_ds2784(void) {
	if (!manta_bat_ds2784_battery)
		manta_bat_ds2784_battery =
			power_supply_get_by_name("ds2784-fuelgauge");

	if (!manta_bat_ds2784_battery) {
		pr_err("%s: failed to get ds2784-fuelgauge power supply\n",
		       __func__);
		return -ENODEV;
	}

	return 0;
}

static void max17047_fg_register_callbacks(struct max17047_fg_callbacks *ptr)
{
	fg_callbacks = ptr;
	if (exynos5_manta_get_revision() >= MANTA_REV_BETA)
		fg_callbacks->get_temperature = NULL;
}

static void max17047_fg_unregister_callbacks(void)
{
	fg_callbacks = NULL;
}

static struct max17047_platform_data max17047_fg_pdata = {
	.register_callbacks = max17047_fg_register_callbacks,
	.unregister_callbacks = max17047_fg_unregister_callbacks,
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
	int hw_rev = exynos5_manta_get_revision();
	int ret;

	gpio_TA_nCHG = hw_rev >= MANTA_REV_PRE_ALPHA ? GPIO_TA_nCHG_ALPHA
		: GPIO_TA_nCHG_LUNCHBOX;

	s3c_gpio_cfgpin(GPIO_TA_INT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TA_INT, S3C_GPIO_PULL_NONE);

	if (hw_rev > MANTA_REV_ALPHA) {
		s3c_gpio_cfgpin(GPIO_OTG_VBUS_SENSE, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_OTG_VBUS_SENSE, S3C_GPIO_PULL_NONE);

		s3c_gpio_cfgpin(GPIO_VBUS_POGO_5V, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_VBUS_POGO_5V, S3C_GPIO_PULL_NONE);

		s3c_gpio_cfgpin(GPIO_OTG_VBUS_SENSE_FAC, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_OTG_VBUS_SENSE_FAC, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_pd_cfg(GPIO_OTG_VBUS_SENSE_FAC,
					S5P_GPIO_PD_PREV_STATE);
		s5p_gpio_set_pd_pull(GPIO_OTG_VBUS_SENSE_FAC,
					S5P_GPIO_PD_UPDOWN_DISABLE);
	}

#if 1 /* temporary, move to pogo driver */
	if (hw_rev >= MANTA_REV_DOGFOOD02) {
		s3c_gpio_cfgpin(GPIO_TA_CHECK_SEL, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TA_CHECK_SEL, S3C_GPIO_PULL_NONE);
		ret = gpio_request_one(GPIO_TA_CHECK_SEL, GPIOF_OUT_INIT_LOW,
				       "ta_check_sel");
		if (ret)
			pr_err("%s: cannot request gpio%d\n", __func__,
			       GPIO_TA_CHECK_SEL);
	}
#endif /* temporary */

	s3c_gpio_cfgpin(gpio_TA_nCHG, S3C_GPIO_INPUT);
	s3c_gpio_setpull(gpio_TA_nCHG, hw_rev >= MANTA_REV_PRE_ALPHA ?
			 S3C_GPIO_PULL_NONE : S3C_GPIO_PULL_UP);

	s3c_gpio_cfgpin(GPIO_TA_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TA_EN, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_pd_cfg(GPIO_TA_EN, S5P_GPIO_PD_PREV_STATE);
	s5p_gpio_set_pd_pull(GPIO_TA_EN, S5P_GPIO_PD_UPDOWN_DISABLE);

	s3c_gpio_cfgpin(GPIO_USB_SEL1, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_USB_SEL1, S3C_GPIO_PULL_NONE);
	ret = gpio_request_one(GPIO_USB_SEL1, GPIOF_OUT_INIT_HIGH, "usb_sel1");
	if (ret)
		pr_err("%s: cannot request gpio%d\n", __func__, GPIO_USB_SEL1);
}

static int read_ta_adc(enum charge_connector conn, int ta_check_sel)
{
	int adc_max = -1;
	int adc_min = 1 << 11;
	int adc_total = 0;
	int i, j;
	int ret;
	int hw_rev = exynos5_manta_get_revision();

	mutex_lock(&manta_bat_adc_lock);

	/* switch to check adc */
	if (conn == CHARGE_CONNECTOR_USB)
		gpio_set_value(GPIO_USB_SEL1, 0);
	else
		manta_pogo_switch_set(0);

	/* temporary, need pogo driver API */
	if (hw_rev >= MANTA_REV_DOGFOOD02) {
		if (ta_check_sel) {
			s3c_gpio_cfgpin(GPIO_POGO_SPDIF, S3C_GPIO_INPUT);
			s3c_gpio_setpull(GPIO_POGO_SPDIF, S3C_GPIO_PULL_NONE);
		}

		gpio_set_value(GPIO_TA_CHECK_SEL, ta_check_sel);
	}

	msleep(100);

	for (i = 0; i < ADC_NUM_SAMPLES; i++) {
		if (exynos5_manta_get_revision() <= MANTA_REV_LUNCHBOX)
			ret = manta_stmpe811_read_adc_data(6);
		else
			ret = s3c_adc_read(ta_adc_client, 0);

		if (ret == -ETIMEDOUT) {
			for (j = 0; j < ADC_LIMIT_ERR_COUNT; j++) {
				msleep(20);
				if (exynos5_manta_get_revision() <=
				    MANTA_REV_LUNCHBOX)
					ret = manta_stmpe811_read_adc_data(6);
				else
					ret = s3c_adc_read(ta_adc_client, 0);

				if (ret > 0)
					break;
			}
			if (j >= ADC_LIMIT_ERR_COUNT) {
				pr_err("%s: Retry count exceeded\n", __func__);
				goto out;
			}
		} else if (ret < 0) {
			pr_err("%s: Failed read adc value : %d\n",
				__func__, ret);
			goto out;
		}

		if (ret > adc_max)
			adc_max = ret;
		if (ret < adc_min)
			adc_min = ret;

		adc_total += ret;
	}

	ret = (adc_total - adc_max - adc_min) / (ADC_NUM_SAMPLES - 2);

out:
	msleep(50);

	/* switch back to normal */
	if (conn == CHARGE_CONNECTOR_USB)
		gpio_set_value(GPIO_USB_SEL1, 1);
	else
		manta_pogo_switch_set(1);

	/* temporary, need pogo driver API */
	if (hw_rev >= MANTA_REV_DOGFOOD02) {
		gpio_set_value(GPIO_TA_CHECK_SEL, 1); /* TODO: restore */
		if (ta_check_sel)
			s3c_gpio_cfgpin(GPIO_POGO_SPDIF, S3C_GPIO_SFN(0x4));
	}

	mutex_unlock(&manta_bat_adc_lock);
	return ret;
}

int manta_bat_otg_enable(bool enable)
{
	int ret;
	union power_supply_propval value;

	if (!manta_bat_smb347_usb)
		manta_bat_smb347_usb = power_supply_get_by_name("smb347-usb");

	if (!manta_bat_smb347_usb) {
		pr_err("%s: failed to get smb347-usb power supply\n",
		       __func__);
		return -ENODEV;
	}

	mutex_lock(&manta_bat_charger_detect_lock);

	if (manta_bat_otg_enabled == enable) {
		mutex_unlock(&manta_bat_charger_detect_lock);
		return 0;
	}

	value.intval = enable ? 1 : 0;
	ret = manta_bat_smb347_usb->set_property(manta_bat_smb347_usb,
						 POWER_SUPPLY_PROP_USB_OTG,
						 &value);
	if (ret)
		pr_err("%s: failed to set smb347-usb OTG mode\n",
		       __func__);
	else
		manta_bat_otg_enabled = enable;

	mutex_unlock(&manta_bat_charger_detect_lock);
	return ret;
}

static enum manta_charge_source check_samsung_charger(
	enum charge_connector conn)
{
	int ta_adc;
	enum manta_charge_source charge_source;

	if (conn == CHARGE_CONNECTOR_POGO) {
		/* todo: check dock int */
		ta_adc = read_ta_adc(conn, 0);
		pr_debug("%s: ta_adc conn=%d ta_check=0 val=%d\n", __func__,
			 conn, ta_adc);
	} else {
		/* temporary until use APSD */
		ta_adc = read_ta_adc(conn, 0);
		pr_debug("%s: ta_adc conn=%d ta_check=0 val=%d\n", __func__,
			 conn, ta_adc);
	}

	if (ta_adc > TA_ADC_LOW && ta_adc < TA_ADC_HIGH) {
		ta_adc = read_ta_adc(conn, 1);
		pr_debug("%s: ta_adc conn=%d ta_check=1 val=%d\n",
			 __func__, conn, ta_adc);
		charge_source = ta_adc > TA_ADC_LOW && ta_adc < TA_ADC_HIGH ?
			MANTA_CHARGE_SOURCE_AC_SAMSUNG :
			MANTA_CHARGE_SOURCE_AC_OTHER;
	} else {
		charge_source = MANTA_CHARGE_SOURCE_USB;
	}

	return charge_source;
}

static enum manta_charge_source
detect_charge_source(enum charge_connector conn, bool online)
{
	enum manta_charge_source charge_source, dock_charge_source;

	if (!online) {
		if (conn == CHARGE_CONNECTOR_POGO)
			manta_pogo_set_vbus(online);
		return MANTA_CHARGE_SOURCE_NONE;
	}

	charge_source = check_samsung_charger(conn);

	if (conn == CHARGE_CONNECTOR_POGO &&
	    charge_source == MANTA_CHARGE_SOURCE_USB) {
		dock_charge_source = manta_pogo_set_vbus(online);
		if ((int) dock_charge_source >= 0)
			return dock_charge_source;
	}

	return charge_source;
}

static int update_charging_status(bool usb_connected, bool pogo_connected)
{
	int i;
	int ret = 0;

	if (manta_bat_usb_online != usb_connected) {
		manta_bat_usb_online = usb_connected;
		manta_bat_charge_source[CHARGE_CONNECTOR_USB] =
			detect_charge_source(CHARGE_CONNECTOR_USB,
					     usb_connected);
		manta_otg_set_usb_state(
			manta_bat_charge_source[CHARGE_CONNECTOR_USB] ==
			MANTA_CHARGE_SOURCE_USB);
		ret = 1;
	}

	if (manta_bat_pogo_online != pogo_connected) {
		manta_bat_pogo_online = pogo_connected;
		manta_bat_charge_source[CHARGE_CONNECTOR_POGO] =
			detect_charge_source(CHARGE_CONNECTOR_POGO,
					     pogo_connected);
		ret = 1;
	}

	/* Find the highest-priority charging source */
	for (i = 0; i < CHARGE_CONNECTOR_MAX; i++)
		if (manta_bat_charge_source[i] != MANTA_CHARGE_SOURCE_NONE)
			break;

	if (i < CHARGE_CONNECTOR_MAX)
		charge_source = manta_bat_charge_source[i];
	else
		charge_source = MANTA_CHARGE_SOURCE_NONE;

	return ret;
}

static void manta_bat_set_charging_enable(int en)
{
	union power_supply_propval value;

	manta_bat_chg_enabled = en;

	if (exynos5_manta_get_revision() >= MANTA_REV_PRE_ALPHA) {
		if (!manta_bat_smb347_battery)
			manta_bat_smb347_battery =
				power_supply_get_by_name("smb347-battery");

		if (!manta_bat_smb347_battery)
			return;

		value.intval = en ? 1 : 0;
		manta_bat_smb347_battery->set_property(
			manta_bat_smb347_battery,
			POWER_SUPPLY_PROP_CHARGE_ENABLED,
			&value);
		manta_bat_chg_enable_synced = true;

	} else if (chg_callbacks && chg_callbacks->set_charging_enable) {
		chg_callbacks->set_charging_enable(chg_callbacks, en);
	}
}

static void manta_bat_sync_charge_enable(void)
{
	union power_supply_propval chg_enabled = {0,};

	if (!manta_bat_smb347_battery)
		return;

	manta_bat_smb347_battery->get_property(
		manta_bat_smb347_battery,
		POWER_SUPPLY_PROP_CHARGE_ENABLED,
		&chg_enabled);

	if (chg_enabled.intval != manta_bat_chg_enabled) {
		if (manta_bat_chg_enable_synced) {
			pr_info("%s: charger changed enable state to %d\n",
				__func__, chg_enabled.intval);
			manta_bat_chg_enabled = chg_enabled.intval;
		}

		manta_bat_set_charging_enable(manta_bat_chg_enabled);
	}
}

static void change_charger_status(void)
{
	int ta_int;
	union power_supply_propval pogo_connected = {0,};
	union power_supply_propval usb_connected = {0,};
	int status_change = 0;
	int hw_rev = exynos5_manta_get_revision();
	int ret;

	mutex_lock(&manta_bat_charger_detect_lock);
	ta_int = hw_rev <= MANTA_REV_ALPHA ? gpio_get_value(GPIO_TA_INT) :
			gpio_get_value(GPIO_OTG_VBUS_SENSE) |
			gpio_get_value(GPIO_VBUS_POGO_5V);

	if (exynos5_manta_get_revision() >= MANTA_REV_PRE_ALPHA &&
	    (!manta_bat_smb347_mains || !manta_bat_smb347_usb ||
	     !manta_bat_smb347_battery)) {
		manta_bat_smb347_mains =
			power_supply_get_by_name("smb347-mains");
		manta_bat_smb347_usb =
			power_supply_get_by_name("smb347-usb");
		manta_bat_smb347_battery =
			power_supply_get_by_name("smb347-battery");

		if (!manta_bat_smb347_mains || !manta_bat_smb347_usb ||
		    !manta_bat_smb347_battery)
			pr_err("%s: failed to get power supplies\n", __func__);
	}

	if (exynos5_manta_get_revision() > MANTA_REV_ALPHA) {
		if (!manta_bat_otg_enabled)
			usb_connected.intval = gpio_get_value(GPIO_OTG_VBUS_SENSE);
		pogo_connected.intval = gpio_get_value(GPIO_VBUS_POGO_5V);

		if (manta_bat_smb347_usb &&
		    usb_connected.intval != manta_bat_usb_online) {
			ret = manta_bat_smb347_usb->set_property(
				manta_bat_smb347_usb,
				POWER_SUPPLY_PROP_ONLINE,
				&usb_connected);
			if (ret)
				pr_err("%s: failed to change smb347-usb online\n",
				       __func__);
		}

		if (manta_bat_smb347_mains &&
		    pogo_connected.intval != manta_bat_pogo_online) {
			ret = manta_bat_smb347_mains->set_property(
				manta_bat_smb347_mains,
				POWER_SUPPLY_PROP_ONLINE,
				&pogo_connected);
			if (ret)
				pr_err("%s: failed to change smb347-mains online\n",
				       __func__);
		}
	}

	if (ta_int) {
		if (exynos5_manta_get_revision() <= MANTA_REV_ALPHA) {
			if (manta_bat_smb347_mains)
				manta_bat_smb347_mains->get_property(
					manta_bat_smb347_mains,
					POWER_SUPPLY_PROP_ONLINE,
					&pogo_connected);

			if (manta_bat_smb347_usb)
				manta_bat_smb347_usb->get_property(
					manta_bat_smb347_usb,
					POWER_SUPPLY_PROP_ONLINE,
					&usb_connected);
		}

		if (exynos5_manta_get_revision() >= MANTA_REV_PRE_ALPHA) {
			status_change =
				update_charging_status(usb_connected.intval,
						       pogo_connected.intval);
			manta_bat_sync_charge_enable();
		} else {
			status_change = update_charging_status(true, false);
		}
	} else {
		status_change = update_charging_status(false, false);
	}

	pr_debug("%s: ta_int(%d), charge_source(%d)", __func__,
		 ta_int, charge_source);

	if (status_change && bat_callbacks &&
	    bat_callbacks->charge_source_changed)
		bat_callbacks->charge_source_changed(
			bat_callbacks, manta_source_to_android(charge_source));

	if (status_change) {
		if (manta_bat_charge_source[CHARGE_CONNECTOR_USB] ==
		    MANTA_CHARGE_SOURCE_USB)
			__pm_stay_awake(&manta_bat_vbus_ws);
		else
			__pm_wakeup_event(&manta_bat_vbus_ws, 500);
	}

	mutex_unlock(&manta_bat_charger_detect_lock);
}

static char *exynos5_manta_supplicant[] = { "manta-board" };

static struct smb347_charger_platform_data smb347_chg_pdata = {
	.use_mains = true,
	.use_usb = true,
	.enable_control = SMB347_CHG_ENABLE_PIN_ACTIVE_LOW,
	.usb_mode_pin_ctrl = false,
	.max_charge_current = 2500000,
	.max_charge_voltage = 4300000,
	.disable_automatic_recharge = true,
	.pre_charge_current = 200000,
	.termination_current = 250000,
	.pre_to_fast_voltage = 2600000,
	.mains_current_limit = 2000000,
	.usb_hc_current_limit = 1800000,
	.irq_gpio = GPIO_TA_nCHG_ALPHA,
	.disable_stat_interrupts = true,
	.en_gpio = GPIO_TA_EN,
	.supplied_to = exynos5_manta_supplicant,
	.num_supplicants = ARRAY_SIZE(exynos5_manta_supplicant),
};

static struct bq24191_platform_data bq24191_chg_pdata = {
	.register_callbacks = bq24191_chg_register_callbacks,
	.unregister_callbacks = bq24191_chg_unregister_callbacks,
	.high_current_charging = 0x06,	/* input current limit 2A */
	.low_current_charging = 0x32,	/* input current linit 500mA */
	.chg_enable = 0x1d,
	.chg_disable = 0x0d,
	.gpio_ta_nchg = GPIO_TA_nCHG_LUNCHBOX,
	.gpio_ta_en = GPIO_TA_EN,
};

static void manta_bat_register_callbacks(struct android_bat_callbacks *ptr)
{
	bat_callbacks = ptr;
}

static void manta_bat_unregister_callbacks(void)
{
	bat_callbacks = NULL;
}

static int manta_bat_poll_charge_source(void)
{
	change_charger_status();
	return manta_source_to_android(charge_source);
}

static void exynos5_manta_set_mains_current(void)
{
	int ret;
	union power_supply_propval value;

	if (!manta_bat_smb347_mains)
		manta_bat_smb347_mains =
			power_supply_get_by_name("smb347-mains");

	if (!manta_bat_smb347_mains) {
		pr_err("%s: failed to get smb347-mains power supply\n",
		       __func__);
		return;
	}

	value.intval =
		manta_bat_charge_source[CHARGE_CONNECTOR_POGO] ==
		   MANTA_CHARGE_SOURCE_USB ? 500000 : 2000000;

	ret = manta_bat_smb347_mains->set_property(manta_bat_smb347_mains,
					 POWER_SUPPLY_PROP_CURRENT_MAX,
					 &value);
	if (ret)
		pr_err("%s: failed to set smb347-mains current limit\n",
		       __func__);
}

static void exynos5_manta_set_usb_hc(void)
{
	int ret;
	union power_supply_propval value;

	if (!manta_bat_smb347_usb)
		manta_bat_smb347_usb = power_supply_get_by_name("smb347-usb");

	if (!manta_bat_smb347_usb) {
		pr_err("%s: failed to get smb347-usb power supply\n",
		       __func__);
		return;
	}

	value.intval =
		manta_bat_charge_source[CHARGE_CONNECTOR_USB] ==
		MANTA_CHARGE_SOURCE_USB ? 0 : 1;
	ret = manta_bat_smb347_usb->set_property(manta_bat_smb347_usb,
						 POWER_SUPPLY_PROP_USB_HC,
						 &value);
	if (ret)
		pr_err("%s: failed to set smb347-usb USB/HC mode\n",
		       __func__);
}

static void manta_bat_set_charging_current(
	int android_charge_source)
{
	if (exynos5_manta_get_revision() >= MANTA_REV_PRE_ALPHA) {
		exynos5_manta_set_mains_current();
		exynos5_manta_set_usb_hc();
	}
}

static int manta_bat_get_capacity(void)
{
	int hw_rev = exynos5_manta_get_revision();
	union power_supply_propval soc;
	int ret = -ENXIO;

	if (hw_rev >= MANTA_REV_BETA) {
		if (manta_bat_get_ds2784())
			return ret;

		ret = manta_bat_ds2784_battery->get_property(
			manta_bat_ds2784_battery, POWER_SUPPLY_PROP_CAPACITY,
			&soc);

		if (ret >= 0)
			ret = soc.intval;
	} else {
		if (fg_callbacks && fg_callbacks->get_capacity)
			ret = fg_callbacks->get_capacity(fg_callbacks);
	}

	return ret;
}

static int manta_bat_get_temperature(int *temp_now)
{
	int hw_rev = exynos5_manta_get_revision();
	union power_supply_propval temp;
	int ret = -ENXIO;

	if (hw_rev >= MANTA_REV_BETA) {
		if (manta_bat_get_ds2784())
			return ret;

		ret = manta_bat_ds2784_battery->get_property(
			manta_bat_ds2784_battery, POWER_SUPPLY_PROP_TEMP,
			&temp);

		if (ret >= 0)
			*temp_now = temp.intval;
	} else {
		if (fg_callbacks && fg_callbacks->get_temperature)
			ret = fg_callbacks->get_temperature(fg_callbacks,
							    temp_now);

		if (ret >= 0)
			*temp_now /= 1000;
	}

	return ret;
}

static int manta_bat_get_voltage_now(void)
{
	int hw_rev = exynos5_manta_get_revision();
	union power_supply_propval vcell;
	int ret = -ENXIO;

	if (hw_rev >= MANTA_REV_BETA) {
		if (manta_bat_get_ds2784())
			return ret;

		ret = manta_bat_ds2784_battery->get_property(
			manta_bat_ds2784_battery, POWER_SUPPLY_PROP_VOLTAGE_NOW,
			&vcell);

		if (ret >= 0)
			ret = vcell.intval;
	} else {
		if (fg_callbacks && fg_callbacks->get_voltage_now)
			ret = fg_callbacks->get_voltage_now(fg_callbacks);
	}

	return ret;
}

static int manta_bat_get_current_now(int *i_current)
{
	int hw_rev = exynos5_manta_get_revision();
	union power_supply_propval inow;
	int ret = -ENXIO;

	if (hw_rev >= MANTA_REV_BETA) {
		if (manta_bat_get_ds2784())
			return ret;

		ret = manta_bat_ds2784_battery->get_property(
			manta_bat_ds2784_battery, POWER_SUPPLY_PROP_CURRENT_NOW,
			&inow);

		if (ret >= 0)
			*i_current = inow.intval;
	} else {
		if (fg_callbacks && fg_callbacks->get_current_now)
			ret = fg_callbacks->get_current_now(fg_callbacks,
							    i_current);
	}

	return ret;
}

static irqreturn_t ta_int_intr(int irq, void *arg)
{
	change_charger_status();
	return IRQ_HANDLED;
}

static struct android_bat_platform_data android_battery_pdata = {
	.register_callbacks = manta_bat_register_callbacks,
	.unregister_callbacks = manta_bat_unregister_callbacks,

	.poll_charge_source = manta_bat_poll_charge_source,

	.set_charging_current = manta_bat_set_charging_current,
	.set_charging_enable = manta_bat_set_charging_enable,

	.get_capacity = manta_bat_get_capacity,
	.get_temperature = manta_bat_get_temperature,
	.get_voltage_now = manta_bat_get_voltage_now,
	.get_current_now = manta_bat_get_current_now,

	.temp_high_threshold = 500,	/* 50c */
	.temp_high_recovery = 430,	/* 43c */
	.temp_low_recovery = 30,		/* 3c */
	.temp_low_threshold = 0,		/* 0c */
};

static struct platform_device android_device_battery = {
	.name = "android-battery",
	.id = -1,
	.dev.platform_data = &android_battery_pdata,
};

static struct platform_device *manta_battery_devices[] __initdata = {
	&android_device_battery,
};

static char *manta_charge_source_str(enum manta_charge_source charge_source)
{
	switch (charge_source) {
	case MANTA_CHARGE_SOURCE_NONE:
		return "none";
	case MANTA_CHARGE_SOURCE_AC_SAMSUNG:
		return "ac-samsung";
	case MANTA_CHARGE_SOURCE_AC_OTHER:
		return "ac-other";
	case MANTA_CHARGE_SOURCE_USB:
		return "usb";
	default:
		break;
	}

	return "?";
}


static int manta_power_debug_dump(struct seq_file *s, void *unused)
{
	if (exynos5_manta_get_revision() > MANTA_REV_ALPHA) {
		seq_printf(s, "ta_en=%d ta_nchg=%d ta_int=%d usbin=%d, dcin=%d\n",
			gpio_get_value(GPIO_TA_EN),
			gpio_get_value(gpio_TA_nCHG),
			gpio_get_value(GPIO_TA_INT),
			gpio_get_value(GPIO_OTG_VBUS_SENSE),
			gpio_get_value(GPIO_VBUS_POGO_5V));
	} else {
		seq_printf(s, "ta_en=%d ta_nchg=%d ta_int=%d\n",
			gpio_get_value(GPIO_TA_EN),
			gpio_get_value(gpio_TA_nCHG),
			gpio_get_value(GPIO_TA_INT));
	}

	seq_printf(s, "usb: type=%s; pogo: type=%s adc=%d,%d\n",
		   manta_bat_otg_enabled ? "otg" :
		   manta_charge_source_str(
			   manta_bat_charge_source[CHARGE_CONNECTOR_USB]),
		   manta_charge_source_str(
			   manta_bat_charge_source[CHARGE_CONNECTOR_POGO]),
		   read_ta_adc(CHARGE_CONNECTOR_POGO, 0),
		   read_ta_adc(CHARGE_CONNECTOR_POGO, 1));
	return 0;
}

static int manta_power_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, manta_power_debug_dump, inode->i_private);
}

static const struct file_operations manta_power_debug_fops = {
	.open = manta_power_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int manta_power_adc_debug_dump(struct seq_file *s, void *unused)
{
	seq_printf(s, "usb=%d,%d pogo=%d,%d\n",
		   read_ta_adc(CHARGE_CONNECTOR_USB, 0),
		   read_ta_adc(CHARGE_CONNECTOR_USB, 1),
		   read_ta_adc(CHARGE_CONNECTOR_POGO, 0),
		   read_ta_adc(CHARGE_CONNECTOR_POGO, 1));
	return 0;
}

static int manta_power_adc_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, manta_power_adc_debug_dump, inode->i_private);
}

static const struct file_operations manta_power_adc_debug_fops = {
	.open = manta_power_adc_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct ds2482_platform_data ds2483_pdata = {
	.slpz_gpio = -1,
};

static struct i2c_board_info i2c_devs2_prebeta[] __initdata = {
	{
		I2C_BOARD_INFO("max17047-fuelgauge", 0x36),
		.platform_data	= &max17047_fg_pdata,
	},
};

static struct i2c_board_info i2c_devs2_beta[] __initdata = {
	{
		I2C_BOARD_INFO("ds2482", 0x30 >> 1),
		.platform_data = &ds2483_pdata,
	},
};

static struct i2c_board_info i2c_devs2_lunchbox[] __initdata = {
	{
		I2C_BOARD_INFO("bq24191-charger", 0x6a),
		.platform_data	= &bq24191_chg_pdata,
	},
};

static struct i2c_board_info i2c_devs2_prealpha[] __initdata = {
	{
		I2C_BOARD_INFO("smb347", 0x0c >> 1),
		.platform_data  = &smb347_chg_pdata,
	},
};

void __init exynos5_manta_battery_init(void)
{
	int hw_rev = exynos5_manta_get_revision();

	charger_gpio_init();

	platform_add_devices(manta_battery_devices,
		ARRAY_SIZE(manta_battery_devices));

	if (hw_rev >= MANTA_REV_DOGFOOD02) {
		s3c_gpio_cfgpin(GPIO_1WIRE_SLEEP, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_1WIRE_SLEEP, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_pd_cfg(GPIO_1WIRE_SLEEP, S5P_GPIO_PD_OUTPUT0);
		s5p_gpio_set_pd_pull(GPIO_1WIRE_SLEEP,
				     S5P_GPIO_PD_UPDOWN_DISABLE);
		ds2483_pdata.slpz_gpio = GPIO_1WIRE_SLEEP;
	}

	if (hw_rev >= MANTA_REV_BETA)
		i2c_register_board_info(2, i2c_devs2_beta,
				ARRAY_SIZE(i2c_devs2_beta));
	else
		i2c_register_board_info(2, i2c_devs2_prebeta,
				ARRAY_SIZE(i2c_devs2_prebeta));

	if (hw_rev  >= MANTA_REV_PRE_ALPHA)
		i2c_register_board_info(2, i2c_devs2_prealpha,
					ARRAY_SIZE(i2c_devs2_prealpha));
	else
		i2c_register_board_info(2, i2c_devs2_lunchbox,
					ARRAY_SIZE(i2c_devs2_lunchbox));

	if (exynos5_manta_get_revision() >= MANTA_REV_PRE_ALPHA)
		ta_adc_client = s3c_adc_register(&android_device_battery,
						 NULL, NULL, 0);

	if (IS_ERR_OR_NULL(debugfs_create_file("manta-power", S_IRUGO, NULL,
					       NULL, &manta_power_debug_fops)))
		pr_err("failed to create manta-power debugfs entry\n");

	if (IS_ERR_OR_NULL(debugfs_create_file("manta-power-adc", S_IRUGO, NULL,
					       NULL,
					       &manta_power_adc_debug_fops)))
		pr_err("failed to create manta-power-adc debugfs entry\n");
}

static void exynos5_manta_power_changed(struct power_supply *psy)
{
	change_charger_status();
}

static int exynos5_manta_power_get_property(struct power_supply *psy,
	    enum power_supply_property psp,
	    union power_supply_propval *val)
{
	return -EINVAL;
}

static struct power_supply exynos5_manta_power_supply = {
	.name = "manta-board",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.external_power_changed = exynos5_manta_power_changed,
	.get_property = exynos5_manta_power_get_property,
};

static int exynos5_manta_battery_pm_event(struct notifier_block *notifier,
					  unsigned long pm_event,
					  void *unused)
{
	int hw_rev = exynos5_manta_get_revision();

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		if (hw_rev <= MANTA_REV_ALPHA) {
			disable_irq(gpio_to_irq(GPIO_TA_INT));
		} else {
			disable_irq(gpio_to_irq(GPIO_OTG_VBUS_SENSE));
			disable_irq(gpio_to_irq(GPIO_VBUS_POGO_5V));
		}
		break;

	case PM_POST_SUSPEND:
		if (hw_rev <= MANTA_REV_ALPHA) {
			enable_irq(gpio_to_irq(GPIO_TA_INT));
		} else {
			enable_irq(gpio_to_irq(GPIO_OTG_VBUS_SENSE));
			enable_irq(gpio_to_irq(GPIO_VBUS_POGO_5V));
		}
		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_manta_battery_pm_notifier_block = {
	.notifier_call = exynos5_manta_battery_pm_event,
};

static int __init exynos5_manta_battery_late_init(void)
{
	int ret;
	int hw_rev = exynos5_manta_get_revision();

	ret = power_supply_register(NULL, &exynos5_manta_power_supply);
	if (ret)
		pr_err("%s: failed to register power supply\n", __func__);

	if (hw_rev <= MANTA_REV_ALPHA) {
		ret = request_threaded_irq(gpio_to_irq(GPIO_TA_INT), NULL,
				ta_int_intr,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT, "ta_int", NULL);
		if (ret) {
			pr_err("%s: ta_int register failed, ret=%d\n",
					__func__, ret);
		} else {
			ret = enable_irq_wake(gpio_to_irq(GPIO_TA_INT));
			if (ret)
				pr_warn("%s: failed to enable irq_wake for ta_int\n",
					__func__);
		}
	} else {
		ret = request_threaded_irq(gpio_to_irq(GPIO_OTG_VBUS_SENSE),
				NULL, ta_int_intr,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT, "usb_vbus", NULL);
		if (ret) {
			pr_err("%s: usb_vbus irq register failed, ret=%d\n",
				__func__, ret);
		} else {
			ret = enable_irq_wake(gpio_to_irq(GPIO_OTG_VBUS_SENSE));
			if (ret)
				pr_warn("%s: failed to enable irq_wake for usb_vbus\n",
					__func__);
		}

		ret = request_threaded_irq(gpio_to_irq(GPIO_VBUS_POGO_5V), NULL,
				ta_int_intr,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT, "pogo_vbus", NULL);
		if (ret) {
			pr_err("%s: pogo_vbus irq register failed, ret=%d\n",
					__func__, ret);
		} else {
			ret = enable_irq_wake(gpio_to_irq(GPIO_VBUS_POGO_5V));
			if (ret)
				pr_warn("%s: failed to enable irq_wake for pogo_vbus\n",
						__func__);
		}
	}

	ret = register_pm_notifier(&exynos5_manta_battery_pm_notifier_block);
	if (ret)
		pr_warn("%s: failed to register PM notifier; ret=%d\n",
			__func__, ret);

	wakeup_source_init(&manta_bat_vbus_ws, "vbus");

	/* Poll initial charger state */
	change_charger_status();
	return 0;
}

late_initcall(exynos5_manta_battery_late_init);
