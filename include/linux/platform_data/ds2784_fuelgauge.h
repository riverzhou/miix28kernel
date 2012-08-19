/*
 * ds2784_fuelgauge.h
 *
 * Copyright (C) 2011 Samsung Electronics
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

#ifndef __DS2784_BATTERY_H_
#define __DS2784_BATTERY_H_

struct ds2784_fg_callbacks {
	int (*get_capacity) (struct ds2784_fg_callbacks *callbacks);
	int (*get_voltage_now) (struct ds2784_fg_callbacks *callbacks);
	int (*get_current_now) (struct ds2784_fg_callbacks *, int *);
	int (*get_temperature) (struct ds2784_fg_callbacks *, int *);
};

struct ds2784_platform_data {
	void (*register_callbacks)(struct ds2784_fg_callbacks *);
	void (*unregister_callbacks)(void);
	void *w1_slave;
};

#endif
