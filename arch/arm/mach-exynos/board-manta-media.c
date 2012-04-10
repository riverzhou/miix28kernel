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

#include <linux/errno.h>
#include <linux/platform_device.h>

#include <plat/cpu.h>
#include <plat/devs.h>

#include <mach/exynos-ion.h>
#include <mach/exynos-mfc.h>
#include <mach/sysmmu.h>

static struct platform_device *media_devices[] __initdata = {
	&s5p_device_mfc,
};

static struct s5p_mfc_platdata manta_mfc_pd = {
	.clock_rate = 300 * MHZ,
};

static void __init manta_media_sysmmu_init(void)
{
	platform_set_sysmmu(&SYSMMU_PLATDEV(mfc_lr).dev, &s5p_device_mfc.dev);
}

void __init exynos5_manta_media_init(void)
{
	manta_media_sysmmu_init();
	s5p_mfc_set_platdata(&manta_mfc_pd);

	platform_add_devices(media_devices, ARRAY_SIZE(media_devices));
}

