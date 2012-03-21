/*
 *  manta_battery.h
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_MANTA_BATTERY_H
#define _LINUX_MANTA_BATTERY_H

enum {
	CABLE_TYPE_NONE = 0,
	CABLE_TYPE_AC,
	CABLE_TYPE_USB,
	CABLE_TYPE_DOCK,
};

struct manta_bat_callbacks {
	void (*change_cable_status)
		(struct manta_bat_callbacks *, int);
};

struct manta_bat_platform_data {
	void (*register_callbacks)(struct manta_bat_callbacks *);
	void (*unregister_callbacks)(void);

	/* communication with charger */
	void (*set_charging_current) (int);
	void (*set_charging_enable) (int);

	/* communication with fuelgauge */
	int (*get_capacity) (void);
	int (*get_temperature) (int *);
	int (*get_voltage_now)(void);
	int (*get_current_now)(int *);

	int temp_high_threshold;
	int temp_high_recovery;
	int temp_low_recovery;
	int temp_low_threshold;

	int (*get_init_cable_state) (void);
};

#endif
