/*
 * Copyright (C) 2012 Google, Inc.
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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

#ifndef __MACH_EXYNOS_BOARD_MANTA_H
#define __MACH_EXYNOS_BOARD_MANTA_H

#include <mach/irqs.h>

/* board IRQ allocations */
#define MANTA_IRQ_BOARD_PMIC_START	IRQ_BOARD_START

void exynos5_manta_audio_init(void);
void exynos5_manta_display_init(void);
void exynos5_manta_input_init(void);
void exynos5_manta_power_init(void);
void exynos5_manta_battery_init(void);
void exynos5_manta_wlan_init(void);

int manta_stmpe811_read_adc_data(u8 channel);

#endif
