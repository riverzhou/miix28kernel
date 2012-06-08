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

#define MXT_FIRMWARE_FOR_NV	"maxtouch_nv.fw"

static struct regulator *touch_dvdd;
static struct regulator *touch_avdd;

static struct mxt_platform_data atmel_mxt_ts_pdata = {
	.x_line         = 32,
	.y_line         = 52,
	.x_size         = 4095,
	.y_size         = 4095,
	.orient         = MXT_DIAGONAL,
	.irqflags       = IRQF_TRIGGER_LOW | IRQF_ONESHOT,
	.boot_address   = 0x26,
	.firmware_name  = MXT_FIRMWARE_FOR_NV,
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
