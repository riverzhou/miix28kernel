/*
 * bq24191_charger.h
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
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

#ifndef __BQ24191_CHARGER_H
#define __BQ24191_CHARGER_H

enum {
	CHARGER_BATTERY = 0,
	CHARGER_AC,
	CHARGER_USB,
	CHARGER_DOCK,
};

struct bq24191_chg_callbacks {
	int (*set_charging_current) (struct bq24191_chg_callbacks *, int);
	int (*set_charging_enable) (struct bq24191_chg_callbacks *, bool);
};

struct bq24191_platform_data {
	void (*register_callbacks)(struct bq24191_chg_callbacks *);
	void (*unregister_callbacks)(void);
	u8 high_current_charging;
	u8 low_current_charging;
	u8 chg_enable;
	u8 chg_disable;
	int gpio_ta_nchg;
	int gpio_ta_en;
};

#endif /* __BQ24191_CHARGER_H */
