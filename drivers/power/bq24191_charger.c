/*
 *  bq24191_charger.c
 *  TI BQ24191 Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/platform_data/bq24191_charger.h>
#include <linux/debugfs.h>

struct bq24191_chg_data {
	struct i2c_client *client;
	struct bq24191_platform_data *pdata;
	struct bq24191_chg_callbacks callbacks;
	struct dentry *debugfs_dentry;
};

/* BQ24191 Registers. */
#define BQ24191_INPUT_SOURCE_CTRL		0x00
#define BQ24191_POWERON_CONFIG			0x01
#define BQ24191_CHARGE_CURRENT_CTRL		0x02
#define BQ24191_PRECHARGE_TERMINATION		0x03
#define BQ24191_CHARGE_VOLTAGE_CTRL		0x04
#define BQ24191_TERMINATION_TIMER_CTRL		0x05
#define BQ24191_IR_COMP_THERMAL_REG		0x06
#define BQ24191_MISC_OPERATION			0x07
#define BQ24191_SYSTEM_STATUS			0x08
#define BQ24191_FAULT				0x09
#define BQ24191_VENDOR_PART_REV			0x0A

#define BQ24191_CHARGING_ENABLE			0x20
#define BQ24191_CHARGING_DONE			0x30

static int bq24191_i2c_write(struct i2c_client *client, int reg, u8 value)
{
	int ret;
	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	return ret;
}

static int bq24191_i2c_read(struct i2c_client *client, int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_read_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	return ret;
}

#ifdef DEBUG
static void bq24191_dump_regs(struct i2c_client *client)
{
	u8 data = 0;
	u32 addr = 0;
	int ret;
	for (addr = 0; addr <= 0x0A; addr++) {
		ret = bq24191_i2c_read(client, addr, &data);

		if (ret < 0) {
			dev_err(&client->dev,
				"%s: addr(0x%02x) read err(%d)\n", __func__,
				addr, ret);
		} else {
			dev_info(&client->dev,
				 "%s: addr(0x%02x) data(0x%02x)\n", __func__,
				 addr, data);
		}
	}
}
#endif

static int bq24191_set_charging_enable(struct bq24191_chg_callbacks *ptr,
				       bool en)
{
	struct bq24191_chg_data *chg;
	struct i2c_client *client;
	u8 data = 0;
	int ret;

	if (!ptr) {
		pr_err("%s: null callbacks\n", __func__);
		return -EINVAL;
	}

	chg = container_of(ptr, struct bq24191_chg_data, callbacks);
	client = chg->client;

	if (en)
		data = chg->pdata->chg_enable;
	else
		data = chg->pdata->chg_disable;

	ret = bq24191_i2c_write(client, BQ24191_POWERON_CONFIG, data);
	if (ret < 0)
		return -EIO;
	gpio_set_value(chg->pdata->gpio_ta_en, !en);

	return 0;
}

static int bq24191_set_charging_current(struct bq24191_chg_callbacks *ptr,
					int cable_type)
{
	struct bq24191_chg_data *chg;
	struct i2c_client *client;
	u8 data = 0;
	int ret;

	if (!ptr) {
		pr_err("%s: null callbacks\n", __func__);
		return -EINVAL;
	}

	chg = container_of(ptr, struct bq24191_chg_data, callbacks);
	client = chg->client;

	if (cable_type == CHARGER_AC)
		data = chg->pdata->high_current_charging;
	else
		data = chg->pdata->low_current_charging;

	ret = bq24191_i2c_write(client, BQ24191_INPUT_SOURCE_CTRL, data);
	if (ret < 0)
		return -EIO;

	return 0;
}

static void __devinit bq24191_init_register(struct i2c_client *client)
{
	int ret;

	/* Charge current set 2036mA */
	ret = bq24191_i2c_write(client, BQ24191_CHARGE_CURRENT_CTRL, 0x60);
	if (ret < 0)
		dev_err(&client->dev, "%s: err : addr(%d)\n",
			__func__, BQ24191_CHARGE_CURRENT_CTRL);

	/* Disable auto termination, Disable timer */
	ret = bq24191_i2c_write(client, BQ24191_TERMINATION_TIMER_CTRL, 0x00);
	if (ret < 0)
		dev_err(&client->dev, "%s: err : addr(%d)\n",
			__func__, BQ24191_TERMINATION_TIMER_CTRL);
}

static int bq24191_debug_dump(struct seq_file *s, void *unused)
{
	struct bq24191_chg_data *chg = s->private;
	struct i2c_client *client = chg->client;
	u8 v;
	u8 v2;
	u8 v3;

	if (bq24191_i2c_read(client, BQ24191_SYSTEM_STATUS, &v) >= 0)
		seq_printf(s, "stat: vbus=%d chrg=%d dpm=%d pg=%d therm=%d"
			   " vsys=%d\n",
			   v >> 6, (v & 0x30) >> 4, !!(v & 0x8), !!(v & 0x4),
			   !!(v & 0x2), v & 0x1);
	if (bq24191_i2c_read(client, BQ24191_FAULT, &v) >= 0)
		seq_printf(s, "fault: wdog=%d otg=%d chrg=%d bat=%d ntc=%d\n",
			   !!(v & 0x80), !!(v & 0x40), (v & 0x30) >> 4,
			   !!(v & 0x8), v & 0x7);
	if (bq24191_i2c_read(client, BQ24191_POWERON_CONFIG, &v) >= 0)
		seq_printf(s, "cfg=%x ce=%d stat=%d sysmin=%x\n",
			   (v & 0x30) >> 4,
			   gpio_get_value(chg->pdata->gpio_ta_en),
			   gpio_get_value(chg->pdata->gpio_ta_nchg),
			   (v & 0xe) >> 1);
	if (bq24191_i2c_read(client, BQ24191_INPUT_SOURCE_CTRL, &v) >= 0 &&
	    bq24191_i2c_read(client, BQ24191_CHARGE_CURRENT_CTRL, &v2) >= 0 &&
	    bq24191_i2c_read(client, BQ24191_CHARGE_VOLTAGE_CTRL, &v3) >= 0)
		seq_printf(s, "vindpm=%x iinlim=%x ichg=%x vreg=%x\n",
			   (v & 0x78) >> 3, v & 0x7, v2 >> 2, v3 & 0xfc >> 2);
	return 0;
}

static int bq24191_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, bq24191_debug_dump, inode->i_private);
}

static const struct file_operations bq24191_debug_fops = {
	.open = bq24191_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __devinit bq24191_charger_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct bq24191_chg_data *chg;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	dev_info(&client->dev, "bq24191 Charger Driver Loading\n");

	chg = kzalloc(sizeof(*chg), GFP_KERNEL);
	if (!chg)
		return -ENOMEM;

	chg->client = client;
	chg->pdata = client->dev.platform_data;
	if (!chg->pdata) {
		dev_err(&client->dev, "%s: No platform data supplied\n",
			__func__);
		ret = -EINVAL;
		goto err_pdata;
	}

	i2c_set_clientdata(client, chg);
	bq24191_init_register(client);

	chg->callbacks.set_charging_current = bq24191_set_charging_current;
	chg->callbacks.set_charging_enable = bq24191_set_charging_enable;
	if (chg->pdata && chg->pdata->register_callbacks)
		chg->pdata->register_callbacks(&chg->callbacks);

	chg->debugfs_dentry = debugfs_create_file("bq24191", S_IRUGO, NULL,
						  chg, &bq24191_debug_fops);
	if (IS_ERR_OR_NULL(chg->debugfs_dentry))
		dev_err(&client->dev,
			"failed to create bq24191 debugfs entry\n");

	return 0;

err_pdata:
	kfree(chg);
	i2c_set_clientdata(client, NULL);
	return ret;
}

static int __devexit bq24191_charger_remove(struct i2c_client *client)
{
	struct bq24191_chg_data *chg = i2c_get_clientdata(client);

	if (chg->pdata && chg->pdata->unregister_callbacks)
		chg->pdata->unregister_callbacks();
	if (!IS_ERR_OR_NULL(chg->debugfs_dentry))
		debugfs_remove(chg->debugfs_dentry);
	kfree(chg);
	return 0;
}

static const struct i2c_device_id bq24191_charger_id[] = {
	{"bq24191-charger", 0},
	{}
};

static struct i2c_driver bq24191_charger_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "bq24191-charger",
		   },
	.id_table = bq24191_charger_id,
	.probe = bq24191_charger_i2c_probe,
	.remove = __devexit_p(bq24191_charger_remove),
	.command = NULL,
};

MODULE_DEVICE_TABLE(i2c, bq24191_charger_id);

static int __init bq24191_charger_init(void)
{
	int ret = i2c_add_driver(&bq24191_charger_i2c_driver);

	if (ret < 0)
		pr_err("%s: failed to add bq24191 i2c drv\n", __func__);

	return ret;
}

static void __exit bq24191_charger_exit(void)
{
	i2c_del_driver(&bq24191_charger_i2c_driver);
}

module_init(bq24191_charger_init);
module_exit(bq24191_charger_exit);

MODULE_AUTHOR("Mihee Seo <himihee.seo@samsung.com>");
MODULE_DESCRIPTION("bq24191 charger driver");
MODULE_LICENSE("GPL");
