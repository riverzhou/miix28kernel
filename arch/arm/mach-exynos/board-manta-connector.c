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
 *
 */

#define pr_fmt(fmt) "manta_otg %s: " fmt, __func__

#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/usb/gadget.h>
#include <linux/usb/otg.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/usb-phy.h>

#include "board-manta.h"

#define GPIO_USB_VBUS	EXYNOS5_GPX1(0)
#define GPIO_USB_ID	EXYNOS5_GPX1(1)

struct manta_otg {
	struct usb_otg		otg;
	struct usb_phy		phy;
	struct delayed_work	work;
	bool			usb_connected;

	/* HACK: s5p phy interface requires passing a pdev pointer */
	struct platform_device	pdev;
};
static struct manta_otg manta_otg;


static int manta_phy_init(struct usb_phy *phy)
{
	struct manta_otg *motg = container_of(phy, struct manta_otg, phy);

	if (phy->last_event == USB_EVENT_ID)
		return s5p_usb_phy_init(&motg->pdev, S5P_USB_PHY_HOST);
	else
		return s5p_usb_phy_init(&motg->pdev, S5P_USB_PHY_DEVICE);
}

static void manta_phy_shutdown(struct usb_phy *phy)
{
	struct manta_otg *motg = container_of(phy, struct manta_otg, phy);
	s5p_usb_phy_exit(&motg->pdev, S5P_USB_PHY_DEVICE);
}

static int manta_otg_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	otg->host = host;
	if (!host)
		otg->phy->state = OTG_STATE_UNDEFINED;
	return 0;
}

static int manta_otg_set_peripheral(struct usb_otg *otg,
				    struct usb_gadget *gadget)
{
	struct manta_otg *motg = container_of(otg, struct manta_otg, otg);

	otg->gadget = gadget;

	if (otg->phy->state == OTG_STATE_UNDEFINED)
		queue_delayed_work(system_nrt_wq, &motg->work, 0);

	if (!gadget)
		otg->phy->state = OTG_STATE_UNDEFINED;

	return 0;
}

static int manta_otg_set_vbus(struct usb_otg *otg, bool enabled)
{
	pr_debug("vbus %s\n", enabled ? "on" : "off");
	return manta_bat_otg_enable(enabled);
}

static void manta_otg_work(struct work_struct *work)
{
	struct manta_otg *motg = container_of(work, struct manta_otg, work.work);
	enum usb_otg_state prev_state = motg->phy.state;
	int id, vbus;

	vbus = motg->usb_connected;
	id = gpio_get_value(GPIO_USB_ID);

	pr_debug("vbus=%d id=%d\n", vbus, id);

	if (!id) {
		if (prev_state == OTG_STATE_A_HOST)
			return;

		if (!motg->otg.host)
			return;

		motg->phy.state = OTG_STATE_A_HOST;
		motg->phy.last_event = USB_EVENT_ID;
		atomic_notifier_call_chain(&motg->phy.notifier,
					USB_EVENT_ID,
					motg->otg.gadget);
		otg_set_vbus(&motg->otg, 1);
	} else if (vbus) {
		if (prev_state == OTG_STATE_B_PERIPHERAL)
			return;

		if (!motg->otg.gadget)
			return;

		motg->phy.state = OTG_STATE_B_PERIPHERAL;
		motg->phy.last_event = USB_EVENT_VBUS;
		atomic_notifier_call_chain(&motg->phy.notifier,
					USB_EVENT_VBUS,
					motg->otg.gadget);
		usb_gadget_vbus_connect(motg->otg.gadget);
	} else {
		if (prev_state == OTG_STATE_B_IDLE)
			return;

		if (prev_state == OTG_STATE_B_PERIPHERAL && motg->otg.gadget)
			usb_gadget_vbus_disconnect(motg->otg.gadget);

		if (prev_state == OTG_STATE_A_HOST && motg->otg.host)
			otg_set_vbus(&motg->otg, 0);

		motg->phy.state = OTG_STATE_B_IDLE;
		motg->phy.last_event = USB_EVENT_NONE;
		atomic_notifier_call_chain(&motg->phy.notifier,
					USB_EVENT_NONE,
					motg->otg.gadget);
	}

	pr_info("%s -> %s\n", otg_state_string(prev_state),
			      otg_state_string(motg->phy.state));
}

static irqreturn_t manta_otg_irq(int irq, void *data)
{
	struct manta_otg *motg = data;
	queue_delayed_work(system_nrt_wq, &motg->work, 20);
	return IRQ_HANDLED;
}

void manta_otg_set_usb_state(bool connected)
{
	struct manta_otg *motg = &manta_otg;
	motg->usb_connected = connected;
	queue_delayed_work(system_nrt_wq, &motg->work, 20);
}

void exynos5_manta_connector_init(void)
{
	struct manta_otg *motg = &manta_otg;
	struct device *dev = &motg->pdev.dev;

	INIT_DELAYED_WORK(&motg->work, manta_otg_work);
	ATOMIC_INIT_NOTIFIER_HEAD(&motg->phy.notifier);

	device_initialize(dev);
	dev_set_name(dev, "%s", "manta_otg");
	if (device_add(dev)) {
		dev_err(dev, "%s: cannot reg device\n", __func__);
		return;
	}
	dev_set_drvdata(dev, motg);

	motg->phy.dev		= dev;
	motg->phy.label		= "manta_otg";
	motg->phy.otg		= &motg->otg;
	motg->phy.init		= manta_phy_init;
	motg->phy.shutdown	= manta_phy_shutdown;

	motg->otg.phy		 = &motg->phy;
	motg->otg.set_host	 = manta_otg_set_host;
	motg->otg.set_peripheral = manta_otg_set_peripheral;
	motg->otg.set_vbus	 = manta_otg_set_vbus;

	usb_set_transceiver(&motg->phy);
}

static int __init manta_connector_init(void)
{
	struct manta_otg *motg = &manta_otg;
	int ret;

	s3c_gpio_cfgpin(GPIO_USB_ID, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_USB_ID, S3C_GPIO_PULL_UP);

	ret = request_irq(gpio_to_irq(GPIO_USB_ID), manta_otg_irq,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
			IRQF_ONESHOT, "usb_id", motg);
	if (ret)
		pr_err("request_irq ID failed: %d\n", ret);

	return ret;
}
device_initcall(manta_connector_init);
