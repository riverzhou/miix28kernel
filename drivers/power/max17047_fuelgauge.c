/*
 * max17047_fuelgauge.c
 *
 * Copyright (C) 2011-2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/rtc.h>
#include <linux/platform_data/max17047_fuelgauge.h>

struct max17047_fg_data {
	struct i2c_client *client;
	struct max17047_platform_data *pdata;
	struct max17047_fg_callbacks callbacks;
};

/* MAX17047 Registers. */
#define MAX17047_REG_STATUS		0x00
#define MAX17047_REG_VALRT_TH		0x01
#define MAX17047_REG_TALRT_TH		0x02
#define MAX17047_REG_SALRT_TH		0x03
#define MAX17047_REG_SOC_REP		0x06
#define MAX17047_REG_VCELL		0x09
#define MAX17047_REG_TEMPERATURE	0x08
#define MAX17047_REG_CURRENT		0x0A
#define MAX17047_REG_AVGVCELL		0x19
#define MAX17047_REG_CONFIG		0x1D
#define MAX17047_REG_VERSION		0x21
#define MAX17047_REG_LEARNCFG		0x28
#define MAX17047_REG_FILTERCFG		0x29
#define MAX17047_REG_MISCCFG		0x2B
#define MAX17047_REG_CGAIN		0x2E
#define MAX17047_REG_RCOMP		0x38
#define MAX17047_REG_VFOCV		0xFB
#define MAX17047_REG_SOC_VF		0xFF

static int max17047_i2c_write(struct i2c_client *client, u8 reg, u16 value)
{
	int ret = i2c_smbus_write_word_data(client, reg, value);

	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d, reg: 0x%02x, data: 0x%04x\n",
			__func__, ret, reg, value);
		return ret;
	}
	return 0;
}

static int max17047_i2c_read(struct i2c_client *client, u8 reg, u8 *data)
{
	int ret = i2c_smbus_read_i2c_block_data(client, reg, 2, data);

	if (ret != 2) {
		dev_err(&client->dev, "%s: err %d, reg: 0x%02x\n",
				__func__, ret, reg);
		return ret < 0 ? ret : -EIO;
	}
	return 0;
}

static int max17047_i2c_read_16register(struct i2c_client *client,
					u8 addr, u16 *r_data)
{
	u8 data[32];
	int i = 0;
	int ret = i2c_smbus_read_i2c_block_data(client,	addr, sizeof(data),
						data);

	if (ret != sizeof(data)) {
		dev_err(&client->dev, "%s: err %d, reg: 0x%02x\n",
			__func__, ret, addr);
		return ret < 0 ? ret : -EIO;
	}

	for (i = 0; i < 16; i++)
		r_data[i] = (data[(i<<1) + 1] << 8) | data[(i<<1)];

	return 0;
}

static void max17047_dump_register(struct max17047_fg_data *fuelgauge_data)
{
	struct i2c_client *client = fuelgauge_data->client;
	u16 data[0x10];
	int i;

	struct timespec ts;
	struct rtc_time tm;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);

	dev_info(&client->dev, "%s: %d/%d/%d %02d:%02d\n", __func__,
					tm.tm_mday,
					tm.tm_mon + 1,
					tm.tm_year + 1900,
					tm.tm_hour,
					tm.tm_min);

	for (i = 0; i < 0x10; i++) {
		max17047_i2c_read_16register(client, i*16, data);

		pr_info("%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,",
			data[0x00], data[0x01], data[0x02], data[0x03],
			data[0x04], data[0x05], data[0x06], data[0x07]);
		pr_cont("%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,",
			data[0x08], data[0x09], data[0x0a], data[0x0b],
			data[0x0c], data[0x0d], data[0x0e], data[0x0f]);

		if (i == 4)
			i = 13;

		schedule();
	}
}

static int max17047_get_temperature(struct max17047_fg_callbacks *ptr,
				    int *temp_now)
{
	struct max17047_fg_data *fg_data;
	struct i2c_client *client;
	u8 data[2];
	s32 temperature;
	int ret;

	if (!ptr) {
		pr_err("%s: null callbacks\n", __func__);
		return -EINVAL;
	}

	fg_data = container_of(ptr, struct max17047_fg_data, callbacks);
	client = fg_data->client;

	ret = max17047_i2c_read(client, MAX17047_REG_TEMPERATURE, data);
	if (ret < 0)
		return -EIO;

	temperature = (s8)data[1] * 1000;
	if (temperature > 0)
		temperature += data[0] * 39 / 10;

	dev_dbg(&client->dev, "%s: temperature (0x%02x%02x, %d)\n", __func__,
		data[1], data[0], temperature);

	*temp_now = temperature;
	return 0;
}

static int max17047_get_vcell(struct max17047_fg_callbacks *ptr)
{
	struct max17047_fg_data *fg_data;
	struct i2c_client *client;
	u8 data[2];
	int ret;
	u32 vcell;

	if (!ptr) {
		pr_err("%s: null callbacks\n", __func__);
		return -EINVAL;
	}

	fg_data = container_of(ptr, struct max17047_fg_data, callbacks);
	client = fg_data->client;

	ret = max17047_i2c_read(client, MAX17047_REG_VCELL, data);
	if (ret < 0)
		return -EIO;

	vcell = ((data[0] >> 3) + (data[1] << 5)) * 625;

	dev_dbg(&client->dev, "%s: VCELL(0x%02x%02x, %d)\n", __func__,
		 data[1], data[0], vcell);
	return vcell;
}

static int max17047_get_current(struct max17047_fg_callbacks *ptr,
				int *i_current)
{
	struct max17047_fg_data *fg_data;
	struct i2c_client *client;
	u8 data[2];
	int ret;
	s16 cur;

	if (!ptr) {
		pr_err("%s: null callbacks\n", __func__);
		return -EINVAL;
	}

	fg_data = container_of(ptr, struct max17047_fg_data, callbacks);
	client = fg_data->client;

	ret = max17047_i2c_read(client, MAX17047_REG_CURRENT, data);
	if (ret < 0)
		return -EIO;

	cur = ((data[1] << 8) | data[0]);
	*i_current = cur * 15625 / 100000;

	dev_dbg(&client->dev, "%s: CURRENT(0x%02x%02x, %d)\n", __func__,
						data[1], data[0], *i_current);
	return 0;
}

static int max17047_get_soc(struct max17047_fg_callbacks *ptr)
{
	struct max17047_fg_data *fg_data;
	struct i2c_client *client;
	u8 data[2];
	int ret;

	if (!ptr) {
		pr_err("%s: null callbacks\n", __func__);
		return -EINVAL;
	}

	fg_data = container_of(ptr, struct max17047_fg_data, callbacks);
	client = fg_data->client;

	ret = max17047_i2c_read(client, MAX17047_REG_SOC_REP, data);
	if (ret < 0)
		return -EIO;

	dev_dbg(&client->dev, "%s: SOC(0x%02x%02x, %d)\n", __func__,
		 data[1], data[0], data[1]);
	return data[1];
}

static int __devinit max17047_fuelgauge_i2c_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17047_fg_data *fg_data;
	int ret;

	dev_info(&client->dev, "max17047 Fuel gauge\n");

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	fg_data = kzalloc(sizeof(*fg_data), GFP_KERNEL);
	if (!fg_data)
		return -ENOMEM;

	fg_data->client = client;
	fg_data->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, fg_data);

	if (!fg_data->pdata) {
		dev_err(&client->dev, "%s : No platform data supplied\n",
			__func__);
		ret = -EINVAL;
		goto err_pdata;
	}

	fg_data->callbacks.get_capacity = max17047_get_soc;
	fg_data->callbacks.get_voltage_now = max17047_get_vcell;
	fg_data->callbacks.get_current_now = max17047_get_current;
	fg_data->callbacks.get_temperature = max17047_get_temperature;
	if (fg_data->pdata->register_callbacks)
		fg_data->pdata->register_callbacks(&fg_data->callbacks);

	max17047_dump_register(fg_data);
	return 0;

err_pdata:
	kfree(fg_data);
	i2c_set_clientdata(client, NULL);
	return ret;
}

static int __devexit max17047_fuelgauge_remove(struct i2c_client *client)
{
	struct max17047_fg_data *fg_data = i2c_get_clientdata(client);

	if (fg_data->pdata && fg_data->pdata->unregister_callbacks)
		fg_data->pdata->unregister_callbacks();

	kfree(fg_data);
	return 0;
}

static const struct i2c_device_id max17047_fuelgauge_id[] = {
	{"max17047-fuelgauge", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, max17047_fuelgauge_id);

static struct i2c_driver max17047_i2c_driver = {
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "max17047-fuelgauge",
	},
	.probe		= max17047_fuelgauge_i2c_probe,
	.remove		= __devexit_p(max17047_fuelgauge_remove),
	.id_table	= max17047_fuelgauge_id,
};

static int __init max17047_fuelgauge_init(void)
{
	int ret = i2c_add_driver(&max17047_i2c_driver);

	if (ret < 0)
		pr_err("%s: failed to add max17047 i2c driver\n", __func__);

	return ret;
}

static void __exit max17047_fuelgauge_exit(void)
{
	i2c_del_driver(&max17047_i2c_driver);
}

module_init(max17047_fuelgauge_init);
module_exit(max17047_fuelgauge_exit);

MODULE_AUTHOR("Mihee Seo <himihee.seo@samsung.com>");
MODULE_DESCRIPTION("max17047 Fuel gauge driver");
MODULE_LICENSE("GPL");
