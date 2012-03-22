/*
 * drivers/misc/es305.c - Audience ES305 Voice Processor driver
 *
 * Copyright (C) 2012 Google, Inc.
 * Copyright (C) 2012 Samsung Corporation.
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/platform_data/es305.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#define ES305_FIRMWARE_NAME		"es305_fw.bin"
#define MAX_CMD_SEND_SIZE		32
#define RETRY_COUNT			5

/* ES305 commands and values */
#define ES305_BOOT			0x0001
#define ES305_BOOT_ACK			0x01

#define ES305_SYNC_POLLING		0x80000000

#define ES305_RESET_IMMEDIATE		0x80020000

#define ES305_GET_DEVICE_PARAM		0x800B0000
#define ES305_PORT_A_WORD_LENGTH	0x0A00

#define ES305_SET_POWER_STATE_SLEEP	0x80100001

#define ES305_GET_MIC_SAMPLE_RATE	0x80500000
#define ES305_SET_MIC_SAMPLE_RATE	0x80510000
#define ES305_8KHZ			0x0008
#define ES305_16KHZ			0x000A
#define ES305_48KHZ			0x0030

#define ES305_DIGITAL_PASSTHROUGH	0x80520000

struct es305_data {
	struct es305_platform_data	*pdata;
	struct device			*dev;
	struct i2c_client		*client;
	struct clk			*clk;
	const struct firmware		*fw;
	u32				passthrough;
};

static int es305_send_cmd(struct es305_data *es305, u32 command, u16 *response)
{
	u8 send[4];
	u8 recv[4];
	int ret = 0;
	int retry = RETRY_COUNT;

	send[0] = (command >> 24) & 0xff;
	send[1] = (command >> 16) & 0xff;
	send[2] = (command >> 8) & 0xff;
	send[3] = command & 0xff;

	ret = i2c_master_send(es305->client, send, 4);
	if (ret < 0) {
		dev_err(es305->dev, "i2c_master_send failed\n");
		return ret;
	}

	/* The sleep command cannot be acked before the device goes to sleep */
	if (command == ES305_SET_POWER_STATE_SLEEP)
		return ret;

	usleep_range(1000, 2000);
	while (retry--) {
		ret = i2c_master_recv(es305->client, recv, 4);
		if (ret < 0) {
			dev_err(es305->dev, "i2c_master_recv failed\n");
			return ret;
		}
		/*
		 * Check that the first two bytes of the response match
		 * (the ack is in those bytes)
		 */
		if ((send[0] == recv[0]) && (send[1] == recv[1])) {
			if (response)
				*response = (recv[2] << 8) | recv[3];
			ret = 0;
			break;
		} else {
			dev_err(es305->dev,
				"incorrect ack (got 0x%.2x%.2x)\n",
				recv[0], recv[1]);
			ret = -EINVAL;
		}

		/* Wait before polling again */
		if (retry > 0)
			msleep(20);
	}

	return ret;
}

static int es305_load_firmware(struct es305_data *es305)
{
	int ret = 0;
	const u8 *i2c_cmds;
	int size;

	i2c_cmds = es305->fw->data;
	size = es305->fw->size;

	while (size > 0) {
		ret = i2c_master_send(es305->client, i2c_cmds,
				min(size, MAX_CMD_SEND_SIZE));
		if (ret < 0) {
			dev_err(es305->dev, "i2c_master_send failed\n");
			break;
		}
		size -= MAX_CMD_SEND_SIZE;
		i2c_cmds += MAX_CMD_SEND_SIZE;
	}

	return ret;
}

static int es305_reset(struct es305_data *es305)
{
	int ret = 0;
	struct es305_platform_data *pdata = es305->pdata;
	static const u8 boot[2] = {ES305_BOOT >> 8, ES305_BOOT};
	u8 ack;
	int retry = RETRY_COUNT;

	while (retry--) {
		/* Reset ES305 chip */
		gpio_set_value(pdata->gpio_reset, 0);
		usleep_range(200, 400);
		gpio_set_value(pdata->gpio_reset, 1);

		/* Delay before sending i2c commands */
		msleep(50);

		/*
		 * Send boot command and check response. The boot command
		 * is different from the others in that it's only 2 bytes,
		 * and the ack retry mechanism is different too.
		 */
		ret = i2c_master_send(es305->client, boot, 2);
		if (ret < 0) {
			dev_err(es305->dev, "i2c_master_send failed\n");
			continue;
		}
		usleep_range(1000, 2000);
		ret = i2c_master_recv(es305->client, &ack, 1);
		if (ret < 0) {
			dev_err(es305->dev, "i2c_master_recv failed\n");
			continue;
		}
		if (ack != ES305_BOOT_ACK) {
			dev_err(es305->dev, "boot ack incorrect (got 0x%.2x)\n",
									ack);
			continue;
		}

		ret = es305_load_firmware(es305);
		if (ret < 0) {
			dev_err(es305->dev, "load firmware error\n");
			continue;
		}

		/* Delay before issuing a sync command */
		msleep(120);

		ret = es305_send_cmd(es305, ES305_SYNC_POLLING, NULL);
		if (ret < 0) {
			dev_err(es305->dev, "sync error\n");
			continue;
		}

		break;
	}

	return ret;
}

static int es305_sleep(struct es305_data *es305)
{
	int ret = 0;
	struct es305_platform_data *pdata = es305->pdata;

	ret = es305_send_cmd(es305, ES305_SET_POWER_STATE_SLEEP, NULL);
	if (ret < 0) {
		dev_err(es305->dev, "set power state error\n");
		return ret;
	}

	/* The clock can be disabled after the device has had time to sleep */
	msleep(20);
	clk_disable(es305->clk);
	gpio_set_value(pdata->gpio_wakeup, 1);

	return ret;
}

static int es305_set_passthrough(struct es305_data *es305, u32 path)
{
	int ret;

	ret = es305_send_cmd(es305, ES305_DIGITAL_PASSTHROUGH | path, NULL);
	if (ret < 0) {
		dev_err(es305->dev, "set passthrough error\n");
		return ret;
	}

	/* The passthrough command must be followed immediately by sleep */
	ret = es305_sleep(es305);
	if (ret < 0) {
		dev_err(es305->dev, "unable to sleep\n");
		return ret;
	}

	return ret;
}

/*
 * This is the callback function passed to request_firmware_nowait(),
 * and will be called as soon as the firmware is ready.
 */
static void es305_firmware_ready(const struct firmware *fw, void *context)
{
	struct es305_data *es305 = (struct es305_data *)context;
	int ret;

	if (!fw) {
		dev_err(es305->dev, "firmware request failed\n");
		return;
	}
	es305->fw = fw;

	clk_enable(es305->clk);

	ret = es305_reset(es305);
	if (ret < 0) {
		dev_err(es305->dev, "unable to reset device\n");
		goto err;
	}

	/* Enable passthrough if needed */
	if (es305->passthrough != 0) {
		ret = es305_set_passthrough(es305, es305->passthrough);
		if (ret < 0) {
			dev_err(es305->dev,
				"unable to enable digital passthrough\n");
			goto err;
		}
	}

err:
	release_firmware(es305->fw);
	es305->fw = NULL;
}

static int __devinit es305_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct es305_data *es305;
	struct es305_platform_data *pdata = client->dev.platform_data;
	int ret = 0;

	es305 = kzalloc(sizeof(*es305), GFP_KERNEL);
	if (es305 == NULL) {
		ret = -ENOMEM;
		goto err_kzalloc;
	}

	es305->client = client;
	i2c_set_clientdata(client, es305);

	es305->dev = &client->dev;
	dev_set_drvdata(es305->dev, es305);

	es305->pdata = pdata;

	if ((pdata->passthrough_src < 0) || (pdata->passthrough_src > 4) ||
					(pdata->passthrough_dst < 0) ||
					(pdata->passthrough_dst > 4)) {
		dev_err(es305->dev, "invalid pdata\n");
		ret = -EINVAL;
		goto err_pdata;
	} else if ((pdata->passthrough_src != 0) &&
					(pdata->passthrough_dst != 0)) {
		es305->passthrough = ((pdata->passthrough_src + 3) << 4) |
					((pdata->passthrough_dst - 1) << 2);
	}

	es305->clk = clk_get(es305->dev, "system_clk");
	if (IS_ERR(es305->clk)) {
		dev_err(es305->dev, "error getting system clock\n");
		ret = PTR_ERR(es305->clk);
		goto err_clk_get;
	}

	ret = gpio_request(pdata->gpio_wakeup, "ES305 wakeup");
	if (ret < 0) {
		dev_err(es305->dev, "error requesting wakeup gpio\n");
		goto err_gpio_wakeup;
	}
	gpio_direction_output(pdata->gpio_wakeup, 0);

	ret = gpio_request(pdata->gpio_reset, "ES305 reset");
	if (ret < 0) {
		dev_err(es305->dev, "error requesting reset gpio\n");
		goto err_gpio_reset;
	}
	gpio_direction_output(pdata->gpio_reset, 0);

	request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				ES305_FIRMWARE_NAME, es305->dev, GFP_KERNEL,
				es305, es305_firmware_ready);

	return 0;

err_gpio_reset:
	gpio_free(pdata->gpio_wakeup);
err_gpio_wakeup:
	clk_put(es305->clk);
err_clk_get:
err_pdata:
	kfree(es305);
err_kzalloc:
	return ret;
}

static int __devexit es305_remove(struct i2c_client *client)
{
	struct es305_data *es305 = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);

	gpio_free(es305->pdata->gpio_wakeup);
	gpio_free(es305->pdata->gpio_reset);
	kfree(es305);

	return 0;
}

static const struct i2c_device_id es305_id[] = {
	{"audience_es305", 0},
	{},
};

static struct i2c_driver es305_driver = {
	.driver = {
		.name = "audience_es305",
		.owner = THIS_MODULE,
	},
	.probe = es305_probe,
	.remove = __devexit_p(es305_remove),
	.id_table = es305_id,
};

static int __init es305_init(void)
{
	return i2c_add_driver(&es305_driver);
}

static void __exit es305_exit(void)
{
	i2c_del_driver(&es305_driver);
}

module_init(es305_init);
module_exit(es305_exit);

MODULE_DESCRIPTION("Audience ES305 Voice Processor driver");
MODULE_LICENSE("GPL");
