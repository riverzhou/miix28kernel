/*
* Copyright (C) 2012 Invensense, Inc.
* Copyright (C) 2012 Samsung Electronics Co., Ltd.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/debugfs.h>
#include <linux/platform_data/ds2784_fuelgauge.h>

#define DS2784_REG_PORT                 0x00
#define DS2784_REG_STS                  0x01
#define DS2784_REG_RAAC_MSB             0x02
#define DS2784_REG_RAAC_LSB             0x03
#define DS2784_REG_RSAC_MSB             0x04
#define DS2784_REG_RSAC_LSB             0x05
#define DS2784_REG_RARC                 0x06
#define DS2784_REG_RSRC                 0x07
#define DS2784_REG_AVG_CURR_MSB         0x08
#define DS2784_REG_AVG_CURR_LSB         0x09
#define DS2784_REG_TEMP_MSB             0x0A
#define DS2784_REG_TEMP_LSB             0x0B
#define DS2784_REG_VOLT_MSB             0x0C
#define DS2784_REG_VOLT_LSB             0x0D
#define DS2784_REG_CURR_MSB             0x0E
#define DS2784_REG_CURR_LSB             0x0F
#define DS2784_REG_ACCUMULATE_CURR_MSB  0x10
#define DS2784_REG_ACCUMULATE_CURR_LSB  0x11
#define DS2784_REG_ACCUMULATE_CURR_LSB1 0x12
#define DS2784_REG_ACCUMULATE_CURR_LSB2 0x13
#define DS2784_REG_AGE_SCALAR           0x14
#define DS2784_REG_SPECIALL_FEATURE     0x15
#define DS2784_REG_FULL_MSB             0x16
#define DS2784_REG_FULL_LSB             0x17
#define DS2784_REG_ACTIVE_EMPTY_MSB     0x18
#define DS2784_REG_ACTIVE_EMPTY_LSB     0x19
#define DS2784_REG_STBY_EMPTY_MSB       0x1A
#define DS2784_REG_STBY_EMPTY_LSB       0x1B
#define DS2784_REG_EEPROM               0x1F
#define DS2784_REG_MFG_GAIN_RSGAIN_MSB  0xB0
#define DS2784_REG_MFG_GAIN_RSGAIN_LSB  0xB1

#define DS2784_REG_CTRL                 0x60
#define DS2784_REG_ACCUMULATE_BIAS      0x61
#define DS2784_REG_AGE_CAPA_MSB         0x62
#define DS2784_REG_AGE_CAPA_LSB         0x63
#define DS2784_REG_CHARGE_VOLT          0x64
#define DS2784_REG_MIN_CHARGE_CURR      0x65
#define DS2784_REG_ACTIVE_EMPTY_VOLT    0x66
#define DS2784_REG_ACTIVE_EMPTY_CURR    0x67
#define DS2784_REG_ACTIVE_EMPTY_40      0x68
#define DS2784_REG_RSNSP                0x69
#define DS2784_REG_FULL_40_MSB          0x6A
#define DS2784_REG_FULL_40_LSB          0x6B
#define DS2784_REG_FULL_SEG_4_SLOPE     0x6C
#define DS2784_REG_FULL_SEG_3_SLOPE     0x6D
#define DS2784_REG_FULL_SEG_2_SLOPE     0x6E
#define DS2784_REG_FULL_SEG_1_SLOPE     0x6F
#define DS2784_REG_AE_SEG_4_SLOPE       0x70
#define DS2784_REG_AE_SEG_3_SLOPE       0x71
#define DS2784_REG_AE_SEG_2_SLOPE       0x72
#define DS2784_REG_AE_SEG_1_SLOPE       0x73
#define DS2784_REG_SE_SEG_4_SLOPE       0x74
#define DS2784_REG_SE_SEG_3_SLOPE       0x75
#define DS2784_REG_SE_SEG_2_SLOPE       0x76
#define DS2784_REG_SE_SEG_1_SLOPE       0x77
#define DS2784_REG_RSGAIN_MSB           0x78
#define DS2784_REG_RSGAIN_LSB           0x79
#define DS2784_REG_RSTC                 0x7A
#define DS2784_REG_CURR_OFFSET_BIAS     0x7B
#define DS2784_REG_TBP34                0x7C
#define DS2784_REG_TBP23                0x7D
#define DS2784_REG_TBP12                0x7E
#define DS2784_REG_PROTECTOR_THRESHOLD  0x7F
#define DS2784_REG_USER_EEPROM_20       0x20

#define DS2784_READ_DATA		0x69
#define DS2784_WRITE_DATA		0x6C

#define DS2483_CMD_RESET		0xF0
#define DS2483_CMD_SET_READ_PTR		0xE1
#define DS2483_CMD_CHANNEL_SELECT	0xC3
#define DS2483_CMD_SKIP_ROM		0xCC
#define DS2483_CMD_WRITE_CONFIG		0xD2
#define DS2483_CMD_1WIRE_RESET		0xB4
#define DS2483_CMD_1WIRE_SINGLE_BIT	0x87
#define DS2483_CMD_1WIRE_WRITE_BYTE	0xA5
#define DS2483_CMD_1WIRE_READ_BYTE	0x96
#define DS2483_CMD_1WIRE_TRIPLET	0x78

#define DS2483_PTR_CODE_STATUS		0xF0
#define DS2483_PTR_CODE_DATA		0xE1
#define DS2483_PTR_CODE_CONFIG		0xC3

#define DS2784_DATA_SIZE		0xB2

struct fg_status {
	int timestamp;

	int voltage_uV;		/* units of uV */
	int current_uA;		/* units of uA */
	int current_avg_uA;
	int charge_uAh;

	short temp_C;		/* units of 0.1 C */

	u8 percentage;		/* battery percentage */
	u8 charge_source;
	u8 status_reg;
	u8 battery_full;	/* battery full (don't charge) */

	u8 cooldown;		/* was overtemp */
	u8 charge_mode;
};

struct ds2784_data {
	struct	i2c_client	*client;
	struct	device		*ds2784_dev;
	struct	ds2784_platform_data *pdata;
	struct	ds2784_fg_callbacks callbacks;
	struct	delayed_work	work;
	char	raw_data[DS2784_DATA_SIZE];	/* raw DS2784 data */
	struct	fg_status	status;
	struct	mutex		access_lock;
	struct dentry		*dentry;
};

static int write_i2c(struct i2c_client *client, int w1, int w2, int len)
{
	char buf[10];
	int ret;

	buf[0] = w1;
	buf[1] = w2;

	ret = i2c_master_send(client, buf, len);
	if (ret < 0)
		dev_err(&client->dev, "i2c_master_send failed\n");

	return ret;
}

static int read_i2c(struct i2c_client *client, int len)
{
	char buf[10] = {0, };
	int ret;

	ret = i2c_master_recv(client, buf, len);
	if (ret < 0)
		dev_err(&client->dev, "i2c_master_recv failed\n");

	return ret;
}

static int ds2784_read_byte(struct i2c_client *client, char *buf)
{
	int ret;

	ret = i2c_master_recv(client, buf, 1);

	return ret;
}

static int ds2784_read_data(struct ds2784_data *data, int start, int count)
{
	int i;

	mutex_lock(&data->access_lock);

	write_i2c(data->client, DS2483_CMD_1WIRE_RESET, 0, 1);
	mdelay(5);
	read_i2c(data->client, 1);
	write_i2c(data->client,
		DS2483_CMD_1WIRE_WRITE_BYTE, DS2483_CMD_SKIP_ROM, 2);
	mdelay(5);
	read_i2c(data->client, 1);
	write_i2c(data->client,
		DS2483_CMD_1WIRE_WRITE_BYTE, DS2784_READ_DATA, 2);
	mdelay(5);
	read_i2c(data->client, 1);
	write_i2c(data->client, DS2483_CMD_1WIRE_WRITE_BYTE, start, 2);
	mdelay(5);
	read_i2c(data->client, 1);

	for (i = 0; i < count; i++) {
		write_i2c(data->client, DS2483_CMD_1WIRE_READ_BYTE, 0, 1);
		mdelay(5);
		read_i2c(data->client, 1);
		write_i2c(data->client,
			DS2483_CMD_SET_READ_PTR, DS2483_PTR_CODE_DATA, 2);
		ds2784_read_byte(data->client, &data->raw_data[start + i]);
		pr_debug("%s: raw_data[%d] : 0x%x\n",
			__func__, start + i, data->raw_data[start + i]);
		read_i2c(data->client, 1);
	}
	mutex_unlock(&data->access_lock);

	return 1;
}

static void ds2784_parse_data(u8 *raw, struct fg_status *s)
{
	short n;

	/* Get status reg */
	s->status_reg = raw[DS2784_REG_STS];

	/* Get Level */
	s->percentage = raw[DS2784_REG_RARC];

	/* Get Voltage: Unit=4.886mV, range is 0V to 4.99V */
	n = (((raw[DS2784_REG_VOLT_MSB] << 8) |
				(raw[DS2784_REG_VOLT_LSB])) >> 5);

	s->voltage_uV = n * 4886;

	/* Get Current: Unit= 1.5625uV x Rsnsp(67)=104.68 */
	n = ((raw[DS2784_REG_CURR_MSB]) << 8) |
		raw[DS2784_REG_CURR_LSB];
	s->current_uA = ((n * 15625) / 10000) * 67;

	n = ((raw[DS2784_REG_AVG_CURR_MSB]) << 8) |
		raw[DS2784_REG_AVG_CURR_LSB];
	s->current_avg_uA = ((n * 15625) / 10000) * 67;

	/* Get Temperature:
	 * 11 bit signed result in Unit=0.125 degree C.
	 * Convert to integer tenths of degree C.
	 */
	n = ((raw[DS2784_REG_TEMP_MSB] << 8) |
			(raw[DS2784_REG_TEMP_LSB])) >> 5;

	s->temp_C = (n * 10) / 8;

	/* RAAC is in units of 1.6mAh */
	s->charge_uAh = ((raw[DS2784_REG_RAAC_MSB] << 8) |
			raw[DS2784_REG_RAAC_LSB]) * 1600;
}

static int ds2784_get_soc(struct ds2784_fg_callbacks *ptr)
{
	struct ds2784_data *ds2784_data;

	ds2784_data = container_of(ptr, struct ds2784_data, callbacks);

	ds2784_read_data(ds2784_data, DS2784_REG_RARC, 1);

	if (ds2784_data->raw_data[DS2784_REG_RARC] == 0xff)
		ds2784_data->status.percentage = 42;
	else
		ds2784_data->status.percentage =
			ds2784_data->raw_data[DS2784_REG_RARC];

	pr_debug("%s: level : %d\n", __func__,
			ds2784_data->status.percentage);
	return ds2784_data->status.percentage;
}

static int ds2784_get_vcell(struct ds2784_fg_callbacks *ptr)
{
	struct ds2784_data *ds2784_data;
	short n;

	ds2784_data = container_of(ptr, struct ds2784_data, callbacks);

	ds2784_read_data(ds2784_data, DS2784_REG_VOLT_MSB, 2);

	if (ds2784_data->raw_data[DS2784_REG_VOLT_LSB] == 0xff &&
	    ds2784_data->raw_data[DS2784_REG_VOLT_MSB] == 0xff) {
		ds2784_data->status.voltage_uV = 4242000;
	} else {
		n = (((ds2784_data->raw_data[DS2784_REG_VOLT_MSB] << 8) |
		      (ds2784_data->raw_data[DS2784_REG_VOLT_LSB])) >> 5);

		ds2784_data->status.voltage_uV = n * 4886;
	}

	pr_debug("%s: voltage : %d\n", __func__,
			ds2784_data->status.voltage_uV);

	return ds2784_data->status.voltage_uV;
}

static int ds2784_get_current(struct ds2784_fg_callbacks *ptr,
			int *i_current)
{
	struct ds2784_data *ds2784_data;
	short n;

	ds2784_data = container_of(ptr, struct ds2784_data, callbacks);

	ds2784_read_data(ds2784_data, DS2784_REG_CURR_MSB, 2);
	n = ((ds2784_data->raw_data[DS2784_REG_CURR_MSB] << 8) |
			(ds2784_data->raw_data[DS2784_REG_CURR_LSB]));

	ds2784_data->status.current_uA = ((n * 15625) / 1000) * 67;
	pr_debug("%s: current : %d\n", __func__,
			ds2784_data->status.current_uA);

	*i_current = ds2784_data->status.current_uA / 10000;
	return 0;
}

static int ds2784_get_temperature(struct ds2784_fg_callbacks *ptr,
			int *temp_now)
{
	struct ds2784_data *ds2784_data;
	short n;

	ds2784_data = container_of(ptr, struct ds2784_data, callbacks);

	ds2784_read_data(ds2784_data, DS2784_REG_TEMP_MSB, 2);
	n = (ds2784_data->raw_data[DS2784_REG_TEMP_MSB] << 8 |
	     ds2784_data->raw_data[DS2784_REG_TEMP_LSB]) >> 5;

	if (ds2784_data->raw_data[DS2784_REG_TEMP_MSB] & (1 << 7))
		n |= 0xf800;

	ds2784_data->status.temp_C = (n * 10) / 8;
	pr_debug("%s: temp : %d\n", __func__,
			ds2784_data->status.temp_C);

	*temp_now = ds2784_data->status.temp_C * 1000;
	return 0;
}

static int ds2784_read_status(struct ds2784_data *data)
{
	int ret = 0;
	int start, count;

	if (data->raw_data[DS2784_REG_RSNSP] == 0x0) {
		start = 0;
		count = DS2784_DATA_SIZE;
	} else {
		start = DS2784_REG_PORT;
		count = DS2784_REG_CURR_LSB - start + 1;
	}

	ds2784_read_data(data, start, count);
	ds2784_parse_data(data->raw_data, &data->status);

	pr_debug("batt: %3d%%, %d mV, %d mA (%d avg), %s%d.%d C, %d mAh\n",
			data->status.percentage,
			data->status.voltage_uV / 1000,
			data->status.current_uA / 1000,
			data->status.current_avg_uA / 1000,
			data->status.temp_C < 0 ? "-" : "",
			abs(data->status.temp_C) / 10,
			abs(data->status.temp_C) % 10,
			data->status.charge_uAh / 1000);

	return ret;
}

static int ds2784_debugfs_show(struct seq_file *s, void *unused)
{
	struct ds2784_data *data = s->private;
	u8 reg;

	ds2784_read_data(data, 0x00, 0x1c);
	ds2784_read_data(data, 0x20, 0x10);
	ds2784_read_data(data, 0x60, 0x20);
	ds2784_read_data(data, 0xb0, 0x02);

	for (reg = 0x0; reg <= 0xb1; reg++) {
		if ((reg >= 0x1c && reg <= 0x1f) ||
		    (reg >= 0x38 && reg <= 0x5f) ||
		    (reg >= 0x90 && reg <= 0xaf))
			continue;

		if (!(reg & 0x7))
			seq_printf(s, "\n0x%02x:", reg);

		seq_printf(s, "\t0x%02x", data->raw_data[reg]);
	}
	seq_printf(s, "\n");
	return 0;
}

static int ds2784_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, ds2784_debugfs_show, inode->i_private);
}

static const struct file_operations ds2784_debugfs_fops = {
	.open		= ds2784_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int ds2784_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	int ret;
	struct ds2784_data *ds2784_data;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "need I2C_FUNC_I2C\n");
		return -ENODEV;
	}

	ds2784_data = kzalloc(sizeof(*ds2784_data), GFP_KERNEL);
	if (ds2784_data == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	ds2784_data->client = client;
	ds2784_data->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, ds2784_data);

	mutex_init(&ds2784_data->access_lock);

	ds2784_data->callbacks.get_capacity = ds2784_get_soc;
	ds2784_data->callbacks.get_voltage_now = ds2784_get_vcell;
	ds2784_data->callbacks.get_current_now = ds2784_get_current;
	ds2784_data->callbacks.get_temperature = ds2784_get_temperature;
	if (ds2784_data->pdata->register_callbacks)
		ds2784_data->pdata->register_callbacks(&ds2784_data->callbacks);

	ds2784_data->dentry =
		debugfs_create_file("ds2784", S_IRUGO, NULL, ds2784_data,
				    &ds2784_debugfs_fops);
	return 0;

err_exit:
	kfree(ds2784_data);
	return ret;
}

static int ds2784_remove(struct i2c_client *client)
{
	struct ds2784_data *ds2784_data;

	ds2784_data = i2c_get_clientdata(client);
	debugfs_remove(ds2784_data->dentry);
	kfree(ds2784_data);

	return 0;
}

static const struct i2c_device_id ds2784_id[] = {
	{"ds2784", 0},
};

static struct i2c_driver ds2784_driver = {
	.id_table = ds2784_id,
	.probe = ds2784_probe,
	.remove = ds2784_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "ds2784",
	},
};

static int __init ds2784_data_init(void)
{
	return i2c_add_driver(&ds2784_driver);
}
module_init(ds2784_data_init);

static void __exit ds2784_data_exit(void)
{
	i2c_del_driver(&ds2784_driver);
}
module_exit(ds2784_data_exit);

MODULE_AUTHOR("Samsung");
MODULE_DESCRIPTION("ds2784 driver");
MODULE_LICENSE("GPL");
