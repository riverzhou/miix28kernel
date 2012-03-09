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
#include <linux/i2c.h>
#include <linux/i2c/atmel_mxt_ts.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#define GPIO_TOUCH_CHG		EXYNOS5_GPG1(2)
#define GPIO_TOUCH_RESET	EXYNOS5_GPG1(3)

static struct regulator *touch_dvdd;
static struct regulator *touch_avdd;

#define MXT1664S_MAX_MT_FINGERS		10
#define MXT1664S_CHRGTIME_BATT		60
#define MXT1664S_THRESHOLD_BATT		70
#define MXT1664S_THRESHOLD_CHRG		70
#define MXT1664S_CALCFG_BATT		210

static const u8 mxt_config[] = {
	/* GEN_POWERCONFIG_T7 */
	7, 1,
	255, 255, 50,

	/* GEN_ACQUISITIONCONFIG_T8 */
	8, 1,
	MXT1664S_CHRGTIME_BATT, 0, 10, 10, 0, 0, 20, 35, 0, 0,

	/* TOUCH_MULTITOUCHSCREEN_T9 */
	9, 1,
	139, 0, 0, 32, 52, 0, 0x40, MXT1664S_THRESHOLD_BATT, 2, 1,
	0, 5, 2, 0, MXT1664S_MAX_MT_FINGERS, 10, 10, 20, 0x3F, 6,
	0xFF, 9, 0, 0, 0, 0, 0, 0, 0, 0,
	15, 15, 42, 42, 0,

	/* TOUCH_KEYARRAY_T15 */
	15, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0,

	/* SPT_COMCONFIG_T18 */
	18, 1,
	0, 0,

	/* PROCI_ONETOUCHGESTUREPROCESSOR_T24 */
	24, 1,
	0, 4, 255, 3, 63, 100, 100, 1, 10, 20,
	40, 75, 2, 2, 100, 100, 25, 25, 0,

	/* PROCI_TWOTOUCHGESTUREPROCESSOR_T27 */
	27, 1,
	0, 1, 0, 244, 3, 35, 0,

	/* PROCI_GRIPSUPPRESSION_T40 */
	40, 1,
	0, 20, 20, 20, 20,

	/* PROCI_TOUCHSUPPRESSION_T42 */
	42, 1,
	3, 60, 100, 60, 0, 20, 0, 0, 0, 0,

	/* SPT_DIGITIZER_T43 */
	43, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0,

	/* SPT_CTECONFIG_T46 */
	46, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0,

	/* PROCI_STYLUS_T47 */
	47, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

	/* PROCG_NOISESUPPRESSION_T48 */
	48, 1,
	3, 128, MXT1664S_CALCFG_BATT, 0, 0, 0, 0, 0, 0, 0,
	48, 25, 0, 6, 6, 0, 0, 64, 4, 64,
	0, 0, 25, 0, 0, 0, 0, 20, 0, 100,
	0, 10, 10, 0, 48, MXT1664S_THRESHOLD_CHRG, 0, 50, 1, 0,
	MXT1664S_MAX_MT_FINGERS, 10, 10, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0,

	/* ADAPTIVE_T55 */
	55, 1,
	0, 0, 0, 0, 0, 0,

	/* PROCI_SHIELDLESS_T56 */
	56, 1,
	3, 0, 1, 24, 21, 21, 22, 22, 22, 23,
	23, 24, 24, 25, 25, 25, 25, 25, 25, 25,
	24, 24, 24, 24, 24, 24, 24, 23, 24, 23,
	23, 23, 22, 22, 21, 21, 2, 255, 1, 2,
	0, 5,

	/* SPT_GENERICDATA_T57 */
	57, 1,
	227, 25, 0,
};

static struct mxt_platform_data atmel_mxt_ts_pdata = {
	.config		= mxt_config,
	.config_length	= ARRAY_SIZE(mxt_config),
	.x_line         = 32,
	.y_line         = 52,
	.x_size         = 2560,
	.y_size         = 1600,
	.blen           = 0x40,
	.threshold      = 0x46,
	.orient         = MXT_DIAGONAL,
	.irqflags       = IRQF_TRIGGER_LOW | IRQF_ONESHOT,
};

static struct i2c_board_info i2c_devs3[] __initdata = {
	{
		I2C_BOARD_INFO("atmel_mxt_ts", 0x4a),
		.platform_data	= &atmel_mxt_ts_pdata,
	},
};

void __init exynos5_manta_input_init(void)
{
	gpio_request(GPIO_TOUCH_CHG, "TSP_INT");
	s3c_gpio_cfgpin(GPIO_TOUCH_CHG, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_TOUCH_CHG, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(GPIO_TOUCH_CHG);

	gpio_request(GPIO_TOUCH_RESET, "TSP_RST");
	s3c_gpio_cfgpin(GPIO_TOUCH_RESET, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TOUCH_RESET, S3C_GPIO_PULL_NONE);
}

/*
 * TODO: Remove the device_initcall level function
 * Because we need to get regulators to enable the touchscreen, we can't
 * register the touch i2c device during arch_init. So, instead, add a
 * device_initcall that runs after regulators are present that just turns on
 * the regulators and leaves them on.
 *
 * When we get dynamic power management of the touch IC, we can move this back
 * to the arch_initcall function above.
 *
 * */
static int __init exynos5_manta_touch_init(void)
{
	struct i2c_adapter *i2c_adap3;
	int ret;
	int i;

	touch_dvdd = regulator_get(NULL, "touch_vdd_1.8v");
	if (IS_ERR(touch_dvdd)) {
		pr_err("%s: can't get touch_vdd_1.8v\n", __func__);
		return PTR_ERR(touch_dvdd);
	}

	touch_avdd = regulator_get(NULL, "touch_avdd");
	if (IS_ERR(touch_avdd)) {
		pr_err("%s: can't get touch_avdd\n", __func__);
		ret = PTR_ERR(touch_avdd);
		goto err_reg_get_avdd;
	}

	gpio_set_value(GPIO_TOUCH_RESET, 0);
	regulator_enable(touch_dvdd);
	regulator_enable(touch_avdd);
	usleep_range(1000, 1500);
	gpio_set_value(GPIO_TOUCH_RESET, 1);
	msleep(300);

	i2c_devs3[0].irq = gpio_to_irq(GPIO_TOUCH_CHG);

	/* now add the devices directly to the adapter since it was previously
	 * registered */
	i2c_adap3 = i2c_get_adapter(3);
	if (!i2c_adap3) {
		pr_err("%s: failed to get adapter i2c3\n", __func__);
		ret = -ENODEV;
		goto err_get_adap;
	}

	for (i = 0; i < ARRAY_SIZE(i2c_devs3); i++)
		i2c_new_device(i2c_adap3, &i2c_devs3[i]);

	return 0;

err_get_adap:
	regulator_put(touch_dvdd);
err_reg_get_avdd:
	regulator_put(touch_avdd);
	return ret;
}
device_initcall(exynos5_manta_touch_init);
