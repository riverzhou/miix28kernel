/*
 *  manta_battery.c
 *  Manta Battery Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *  <himihee.seo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/types.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/platform_data/manta_battery.h>

struct manta_bat_data {
	struct manta_bat_platform_data *pdata;
	struct manta_bat_callbacks callbacks;

	struct device		*dev;

	struct power_supply	psy_bat;
	struct power_supply	psy_usb;
	struct power_supply	psy_ac;

	struct wake_lock	vbus_wake_lock;
	struct wake_lock	monitor_wake_lock;
	struct wake_lock	cable_wake_lock;

	int			cable_type;

	/* Battery Temperature (C) */
	int			batt_temp;
	int			batt_current;
	unsigned int		batt_health;
	unsigned int		batt_vcell;
	unsigned int		batt_soc;
	unsigned int		charging_status;

	struct workqueue_struct *monitor_wqueue;
	struct delayed_work	monitor_work;
	struct work_struct	cable_work;

	void (*get_init_cable_state)(struct power_supply *psy);
	bool		slow_poll;
	ktime_t		last_poll;
};

static char *supply_list[] = {
	"battery",
};

static enum power_supply_property manta_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static enum power_supply_property manta_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int manta_bat_get_property(struct power_supply *ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct manta_bat_data *battery = container_of(ps, struct manta_bat_data,
			psy_bat);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = battery->charging_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = battery->batt_health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = battery->batt_temp;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		/* battery is always online */
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		/* TODO : update now */
		val->intval = battery->batt_vcell;
		if (val->intval == -1)
			return -EINVAL;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = battery->batt_soc;
		if (val->intval == -1)
			return -EINVAL;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int manta_usb_get_property(struct power_supply *ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct manta_bat_data *battery = container_of(ps, struct manta_bat_data,
			psy_usb);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the USB charger is connected */
	val->intval = (battery->cable_type == CABLE_TYPE_USB);

	return 0;
}

static int manta_ac_get_property(struct power_supply *ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct manta_bat_data *battery = container_of(ps, struct manta_bat_data,
			psy_ac);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the AC charger is connected */
	val->intval = (battery->cable_type == CABLE_TYPE_AC);

	return 0;
}

static void manta_bat_get_temp(struct manta_bat_data *battery)
{
	int batt_temp = 25000;
	int health = battery->batt_health;

	if (battery->pdata->get_temperature)
		battery->pdata->get_temperature(&batt_temp);

	if (batt_temp >= battery->pdata->temp_high_threshold) {
		if (health != POWER_SUPPLY_HEALTH_OVERHEAT &&
				health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) {
			pr_info("battery overheat(%d>=%d),charging unavailable\n",
				batt_temp, battery->pdata->temp_high_threshold);
			battery->batt_health = POWER_SUPPLY_HEALTH_OVERHEAT;
		}
	} else if (batt_temp <= battery->pdata->temp_high_recovery &&
			batt_temp >= battery->pdata->temp_low_recovery) {
		if (health == POWER_SUPPLY_HEALTH_OVERHEAT ||
				health == POWER_SUPPLY_HEALTH_COLD) {
			pr_info("battery recovery(%d,%d~%d),charging available\n",
				batt_temp, battery->pdata->temp_low_recovery,
				battery->pdata->temp_high_recovery);
			battery->batt_health = POWER_SUPPLY_HEALTH_GOOD;
		}
	} else if (batt_temp <= battery->pdata->temp_low_threshold) {
		if (health != POWER_SUPPLY_HEALTH_COLD &&
				health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) {
			pr_info("battery cold(%d <= %d), charging unavailable\n",
				batt_temp, battery->pdata->temp_low_threshold);
			battery->batt_health = POWER_SUPPLY_HEALTH_COLD;
		}
	}

	battery->batt_temp = batt_temp/1000;
}

static void manta_bat_update_data(struct manta_bat_data *battery)
{
	int ret;
	int v;

	if (battery->pdata->get_voltage_now) {
		ret = battery->pdata->get_voltage_now();
		battery->batt_vcell = ret >= 0 ? ret : -1;
	}

	if (battery->pdata->get_capacity) {
		ret = battery->pdata->get_capacity();
		battery->batt_soc = ret >= 0 ? ret : -1;
	}

	if (battery->pdata->get_current_now) {
		ret = battery->pdata->get_current_now(&v);

		if (!ret)
			battery->batt_current = v;
	}

	manta_bat_get_temp(battery);
}

static int manta_bat_enable_charging(struct manta_bat_data *battery,
					bool enable)
{
	if (enable && (battery->batt_health != POWER_SUPPLY_HEALTH_GOOD)) {
		battery->charging_status =
		    POWER_SUPPLY_STATUS_NOT_CHARGING;

		pr_info("%s: Battery is NOT good!\n", __func__);
		return -EPERM;
	}

	if (enable) { /* Enable charging */
		if (battery->pdata && battery->pdata->set_charging_current)
			battery->pdata->set_charging_current
			(battery->cable_type);
	}

	if (battery->pdata && battery->pdata->set_charging_enable)
		battery->pdata->set_charging_enable(enable);

	pr_info("%s: enable(%d), cable(%d)\n", __func__,
		enable, battery->cable_type);

	return 0;
}

static void manta_bat_change_cable_status(struct manta_bat_callbacks *ptr,
						int cable_type)
{
	struct manta_bat_data *battery;

	if (!ptr) {
		pr_err("%s: ptr is NULL\n", __func__);
		return;
	}

	battery = container_of(ptr, struct manta_bat_data, callbacks);
	wake_lock(&battery->cable_wake_lock);
	battery->cable_type = cable_type;

	pr_info("%s: cable was changed(%d)\n", __func__,
				battery->cable_type);

	queue_work(battery->monitor_wqueue, &battery->cable_work);
}

static void manta_bat_cable_work(struct work_struct *work)
{
	struct manta_bat_data *battery = container_of(work,
		struct manta_bat_data, cable_work);

	/* already cable updated*/
	switch (battery->cable_type) {
	case CABLE_TYPE_NONE:
		battery->charging_status = POWER_SUPPLY_STATUS_DISCHARGING;
		manta_bat_enable_charging(battery, false);
		if (battery->batt_health == POWER_SUPPLY_HEALTH_OVERVOLTAGE)
			battery->batt_health = POWER_SUPPLY_HEALTH_GOOD;
		wake_lock_timeout(&battery->vbus_wake_lock, HZ * 2);
		break;
	case CABLE_TYPE_USB:
		battery->charging_status = POWER_SUPPLY_STATUS_CHARGING;
		manta_bat_enable_charging(battery, true);
		wake_lock(&battery->vbus_wake_lock);
		break;
	case CABLE_TYPE_AC:
		battery->charging_status = POWER_SUPPLY_STATUS_CHARGING;
		manta_bat_enable_charging(battery, true);
		wake_lock_timeout(&battery->vbus_wake_lock, HZ * 2);
		break;
	default:
		pr_err("%s: Invalid cable type\n", __func__);
		break;
	}

	power_supply_changed(&battery->psy_ac);
	power_supply_changed(&battery->psy_usb);

	/* To notify framework layer, remaning 2 sec */
	wake_lock_timeout(&battery->cable_wake_lock, HZ * 2);
}

static void manta_bat_monitor_work(struct work_struct *work)
{
	struct manta_bat_data *battery;
	battery = container_of(work, struct manta_bat_data, monitor_work.work);

	wake_lock(&battery->monitor_wake_lock);

	manta_bat_update_data(battery);

	switch (battery->charging_status) {
	case POWER_SUPPLY_STATUS_FULL:
	case POWER_SUPPLY_STATUS_CHARGING:
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		break;
	default:
		wake_unlock(&battery->monitor_wake_lock);
		pr_info("%s: Undefined Battery Status\n", __func__);
		return;
	}

	pr_info("BAT : l(%d) v(%d) c(%d) t(%d) h(%d) status(%d) cable(%d)\n",
		battery->batt_soc, battery->batt_vcell/1000,
		battery->batt_current, battery->batt_temp, battery->batt_health,
		battery->charging_status, battery->cable_type);

	power_supply_changed(&battery->psy_bat);

	queue_delayed_work(battery->monitor_wqueue,
		&battery->monitor_work, msecs_to_jiffies(30000));

	wake_unlock(&battery->monitor_wake_lock);

	return;
}

static __devinit int manta_bat_probe(struct platform_device *pdev)
{
	struct manta_bat_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct manta_bat_data *battery;
	int ret = 0;

	dev_info(&pdev->dev, "Manta Battery Driver\n");

	battery = kzalloc(sizeof(*battery), GFP_KERNEL);
	if (!battery)
		return -ENOMEM;


	battery->pdata = pdata;
	if (!battery->pdata) {
		pr_err("%s : No platform data\n", __func__);
		ret = -EINVAL;
		goto err_pdata;
	}

	battery->dev = &pdev->dev;
	platform_set_drvdata(pdev, battery);
	battery->batt_health = POWER_SUPPLY_HEALTH_GOOD;

	battery->psy_bat.name = "battery",
	battery->psy_bat.type = POWER_SUPPLY_TYPE_BATTERY,
	battery->psy_bat.properties = manta_battery_props,
	battery->psy_bat.num_properties = ARRAY_SIZE(manta_battery_props),
	battery->psy_bat.get_property = manta_bat_get_property,

	battery->psy_usb.name = "usb",
	battery->psy_usb.type = POWER_SUPPLY_TYPE_USB,
	battery->psy_usb.supplied_to = supply_list,
	battery->psy_usb.num_supplicants = ARRAY_SIZE(supply_list),
	battery->psy_usb.properties = manta_power_props,
	battery->psy_usb.num_properties = ARRAY_SIZE(manta_power_props),
	battery->psy_usb.get_property = manta_usb_get_property,

	battery->psy_ac.name = "ac",
	battery->psy_ac.type = POWER_SUPPLY_TYPE_MAINS,
	battery->psy_ac.supplied_to = supply_list,
	battery->psy_ac.num_supplicants = ARRAY_SIZE(supply_list),
	battery->psy_ac.properties = manta_power_props,
	battery->psy_ac.num_properties = ARRAY_SIZE(manta_power_props),
	battery->psy_ac.get_property = manta_ac_get_property;

	battery->batt_vcell = -1;
	battery->batt_soc = -1;

	wake_lock_init(&battery->vbus_wake_lock, WAKE_LOCK_SUSPEND,
			"vbus_present");
	wake_lock_init(&battery->monitor_wake_lock, WAKE_LOCK_SUSPEND,
			"manta-battery-monitor");
	wake_lock_init(&battery->cable_wake_lock, WAKE_LOCK_SUSPEND,
			"manta-battery-cable");

	/* init power supplier framework */
	ret = power_supply_register(&pdev->dev, &battery->psy_bat);
	if (ret) {
		dev_err(battery->dev, "%s: failed to register psy_bat\n",
				__func__);
		goto err_wake_lock;
	}

	ret = power_supply_register(&pdev->dev, &battery->psy_usb);
	if (ret) {
		dev_err(battery->dev, "%s: failed to register psy_usb\n",
				__func__);
		goto err_supply_unreg_bat;
	}

	ret = power_supply_register(&pdev->dev, &battery->psy_ac);
	if (ret) {
		dev_err(battery->dev, "%s: failed to register psy_ac\n",
				__func__);
		goto err_supply_unreg_usb;
	}

	battery->monitor_wqueue =
		create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!battery->monitor_wqueue) {
		dev_err(battery->dev, "%s: fail to create workqueue\n",
				__func__);
		goto err_supply_unreg_ac;
	}

	INIT_DELAYED_WORK_DEFERRABLE(&battery->monitor_work,
		manta_bat_monitor_work);
	INIT_WORK(&battery->cable_work, manta_bat_cable_work);

	battery->callbacks.change_cable_status = manta_bat_change_cable_status;
	if (battery->pdata && battery->pdata->register_callbacks)
		battery->pdata->register_callbacks(&battery->callbacks);

	/* get initial cable status */
	battery->cable_type = battery->pdata->get_init_cable_state();
	wake_lock(&battery->cable_wake_lock);
	queue_work(battery->monitor_wqueue, &battery->cable_work);

	queue_delayed_work(battery->monitor_wqueue,
		&battery->monitor_work, msecs_to_jiffies(0));

	return 0;

err_supply_unreg_ac:
	power_supply_unregister(&battery->psy_ac);
err_supply_unreg_usb:
	power_supply_unregister(&battery->psy_usb);
err_supply_unreg_bat:
	power_supply_unregister(&battery->psy_bat);
err_wake_lock:
	wake_lock_destroy(&battery->vbus_wake_lock);
	wake_lock_destroy(&battery->monitor_wake_lock);
	wake_lock_destroy(&battery->cable_wake_lock);
err_pdata:
	kfree(battery);

	return ret;
}

static int __devexit manta_bat_remove(struct platform_device *pdev)
{
	struct manta_bat_data *battery = platform_get_drvdata(pdev);

	flush_workqueue(battery->monitor_wqueue);
	destroy_workqueue(battery->monitor_wqueue);

	power_supply_unregister(&battery->psy_bat);
	power_supply_unregister(&battery->psy_usb);
	power_supply_unregister(&battery->psy_ac);

	wake_lock_destroy(&battery->vbus_wake_lock);
	wake_lock_destroy(&battery->monitor_wake_lock);
	wake_lock_destroy(&battery->cable_wake_lock);

	kfree(battery);

	return 0;
}

static int manta_bat_suspend(struct device *dev)
{
	struct manta_bat_data *battery = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&battery->monitor_work);

	return 0;
}

static int manta_bat_resume(struct device *dev)
{
	struct manta_bat_data *battery = dev_get_drvdata(dev);

	queue_delayed_work(battery->monitor_wqueue,
		&battery->monitor_work, msecs_to_jiffies(0));

	return 0;
}

static const struct dev_pm_ops manta_bat_pm_ops = {
	.suspend	= manta_bat_suspend,
	.resume = manta_bat_resume,
};

static struct platform_driver manta_bat_driver = {
	.driver = {
		.name = "manta-battery",
		.owner = THIS_MODULE,
		.pm = &manta_bat_pm_ops,
	},
	.probe = manta_bat_probe,
	.remove = __devexit_p(manta_bat_remove),
};

static int __init manta_bat_init(void)
{
	return platform_driver_register(&manta_bat_driver);
}

static void __exit manta_bat_exit(void)
{
	platform_driver_unregister(&manta_bat_driver);
}

late_initcall(manta_bat_init);
module_exit(manta_bat_exit);

MODULE_DESCRIPTION("Manta battery driver");
MODULE_AUTHOR("<ms925.kim@samsung.com>");
MODULE_AUTHOR("<joshua.chang@samsung.com>");
MODULE_LICENSE("GPL");

