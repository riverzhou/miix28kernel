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
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/power_supply.h>
#include <linux/debugfs.h>
#include <linux/platform_data/ds2784_fuelgauge.h>

#include "../w1/w1.h"

#define W1_DS2784_READ_DATA		0x69
#define W1_DS2784_WRITE_DATA		0x6C

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

struct fuelgauge_status {
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

struct ds2784_info {
	struct	device			*dev;
	struct	ds2784_platform_data	*pdata;
	struct power_supply		bat;
	struct	delayed_work		work;
	char				raw[DS2784_DATA_SIZE];
	struct	fuelgauge_status	status;
	struct	w1_slave *w1_slave;
	bool				inited;
	struct dentry		*dentry;
};

static int w1_ds2784_io(struct w1_slave *sl, char *buf, int addr,
				size_t count, int io)
{
	if (!sl)
		return 0;

	mutex_lock(&sl->master->mutex);

	if (addr > DS2784_DATA_SIZE || addr < 0) {
		count = 0;
		goto out;
	}
	if (addr + count > DS2784_DATA_SIZE)
		count = DS2784_DATA_SIZE - addr;

	if (!w1_reset_select_slave(sl)) {
		if (!io) {
			w1_write_8(sl->master, W1_DS2784_READ_DATA);
			w1_write_8(sl->master, addr);
			count = w1_read_block(sl->master, buf, count);
		} else {
			w1_write_8(sl->master, W1_DS2784_WRITE_DATA);
			w1_write_8(sl->master, addr);
			w1_write_block(sl->master, buf, count);
		}
	}

out:
	mutex_unlock(&sl->master->mutex);

	return count;
}

static int w1_ds2784_read(struct w1_slave *sl, char *buf,
				int addr, size_t count)
{
	return w1_ds2784_io(sl, buf, addr, count, 0);
}

static int w1_ds2784_write(struct w1_slave *sl, char *buf,
				int addr, size_t count)
{
	return w1_ds2784_io(sl, buf, addr, count, 1);
}

static int ds2784_get_soc(struct ds2784_info *di, int *soc)
{
	int ret;

	ret = w1_ds2784_read(di->w1_slave,
			di->raw + DS2784_REG_RARC, DS2784_REG_RARC, 1);

	if (ret < 0)
		return ret;

	if (di->raw[DS2784_REG_RARC] == 0xff)
		di->status.percentage = 42;
	else
		di->status.percentage =	di->raw[DS2784_REG_RARC];

	pr_debug("%s: level : %d\n", __func__, di->status.percentage);
	*soc = di->status.percentage;
	return 0;
}

static int ds2784_get_vcell(struct ds2784_info *di, int *vcell)
{
	short n;
	int ret;

	ret = w1_ds2784_read(di->w1_slave, di->raw + DS2784_REG_VOLT_MSB,
			     DS2784_REG_VOLT_MSB, 2);

	if (ret < 0)
		return ret;

	if (di->raw[DS2784_REG_VOLT_LSB] == 0xff &&
			di->raw[DS2784_REG_VOLT_MSB] == 0xff) {
		di->status.voltage_uV = 4242000;
	} else {
		n = (((di->raw[DS2784_REG_VOLT_MSB] << 8) |
			(di->raw[DS2784_REG_VOLT_LSB])) >> 5);

		di->status.voltage_uV = n * 4886;
	}

	pr_debug("%s: voltage : %d\n", __func__, di->status.voltage_uV);
	*vcell = di->status.voltage_uV;
	return 0;
}

static int ds2784_get_current_now(struct ds2784_info *di, int *i_current)
{
	short n;
	int ret;

	if (!di->raw[DS2784_REG_RSNSP]) {
		ret = w1_ds2784_read(di->w1_slave, di->raw + DS2784_REG_RSNSP,
				     DS2784_REG_RSNSP, 1);
		if (ret < 0)
			dev_err(di->dev, "error %d reading RSNSP\n", ret);
	}

	ret = w1_ds2784_read(di->w1_slave,
			di->raw + DS2784_REG_CURR_MSB, DS2784_REG_CURR_MSB, 2);

	if (ret < 0)
		return ret;

	n = ((di->raw[DS2784_REG_CURR_MSB] << 8) |
			(di->raw[DS2784_REG_CURR_LSB]));

	di->status.current_uA = (n * 15625) / 10000 * di->raw[DS2784_REG_RSNSP];
	pr_debug("%s: current : %d\n", __func__, di->status.current_uA);

	*i_current = di->status.current_uA / 1000;
	return 0;
}

static int ds2784_get_temperature(struct ds2784_info *di, int *temp_now)
{
	short n;
	int ret;

	ret = w1_ds2784_read(di->w1_slave,
			di->raw + DS2784_REG_TEMP_MSB, DS2784_REG_TEMP_MSB, 2);

	if (ret < 0)
		return ret;

	n = (((di->raw[DS2784_REG_TEMP_MSB] << 8) |
			(di->raw[DS2784_REG_TEMP_LSB])) >> 5);

	if (di->raw[DS2784_REG_TEMP_MSB] & (1 << 7))
		n |= 0xf800;

	di->status.temp_C = (n * 10) / 8;
	pr_debug("%s: temp : %d\n", __func__, di->status.temp_C);

	*temp_now = di->status.temp_C;
	return 0;
}

static int ds2784_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	int ret = 0;
	struct ds2784_info *di = container_of(psy, struct ds2784_info, bat);

	if (!di->inited)
		return -ENODEV;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = ds2784_get_vcell(di, &val->intval);
		break;

	case POWER_SUPPLY_PROP_TEMP:
		ret = ds2784_get_temperature(di, &val->intval);
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = "DS2784";
		break;

	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "Maxim/Dallas";
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = ds2784_get_current_now(di, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		ret = ds2784_get_soc(di, &val->intval);
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static enum power_supply_property ds2784_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
};

static int ds2784_debugfs_show(struct seq_file *s, void *unused)
{
	struct ds2784_info *di = s->private;
	u8 reg;

	w1_ds2784_read(di->w1_slave, di->raw, 0x00, 0x1C);
	w1_ds2784_read(di->w1_slave, di->raw + 0x20, 0x20, 0x10);
	w1_ds2784_read(di->w1_slave, di->raw + 0x60, 0x60, 0x20);
	w1_ds2784_read(di->w1_slave, di->raw + 0xb0, 0xb0, 0x02);

	for (reg = 0x0; reg <= 0xb1; reg++) {
		if ((reg >= 0x1c && reg <= 0x1f) ||
			(reg >= 0x38 && reg <= 0x5f) ||
			(reg >= 0x90 && reg <= 0xaf))
				continue;

		if (!(reg & 0x7))
			seq_printf(s, "\n0x%02x:", reg);

		seq_printf(s, "\t0x%02x", di->raw[reg]);
	}
	seq_printf(s, "\n");
	return 0;
}

static int ds2784_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, ds2784_debugfs_show, inode->i_private);
}

static const struct file_operations ds2784_debugfs_fops = {
	.open = ds2784_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __devinit ds2784_probe(struct platform_device *pdev)
{
	struct ds2784_info *di;
	int ret;

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		pr_err("%s:failed to allocate memory for module data\n",
			__func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, di);
	di->pdata = pdev->dev.platform_data;
	di->w1_slave = di->pdata->w1_slave;
	di->dev = &pdev->dev;
	di->bat.name		= dev_name(&pdev->dev);
	di->bat.type		= POWER_SUPPLY_TYPE_BATTERY;
	di->bat.properties	= ds2784_props;
	di->bat.num_properties	= ARRAY_SIZE(ds2784_props);
	di->bat.get_property	= ds2784_get_property;

	ret = power_supply_register(&pdev->dev, &di->bat);
	if (ret) {
		dev_err(di->dev, "failed to register battery power supply\n");
		kfree(di);
		return ret;
	}

	di->dentry = debugfs_create_file("ds2784", S_IRUGO, NULL, di,
					 &ds2784_debugfs_fops);
	di->inited = true;
	return 0;
}

static int __devexit ds2784_remove(struct platform_device *pdev)
{
	struct ds2784_info *di = platform_get_drvdata(pdev);

	power_supply_unregister(&di->bat);
	debugfs_remove(di->dentry);
	kfree(di);
	return 0;
}

static struct platform_driver ds2784_driver = {
	.probe = ds2784_probe,
	.remove   = __devexit_p(ds2784_remove),
	.driver = {
		.owner = THIS_MODULE,
		.name = "ds2784-fuelgauge",
	},
};

module_platform_driver(ds2784_driver);

MODULE_AUTHOR("Samsung");
MODULE_DESCRIPTION("ds2784 driver");
MODULE_LICENSE("GPL");
