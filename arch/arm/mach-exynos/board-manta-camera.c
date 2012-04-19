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
#include <linux/clk.h>
#include <linux/gpio.h>

#include <plat/cpu.h>
#include <plat/devs.h>

#include <mach/gpio.h>
#include <mach/sysmmu.h>

#include <media/exynos_fimc_is.h>


static struct exynos5_platform_fimc_is exynos5_fimc_is_data;

#if defined CONFIG_VIDEO_S5K4E5
static struct exynos5_fimc_is_sensor_info s5k4e5 = {
	.sensor_name = "S5K4E5",
	.sensor_id = SENSOR_NAME_S5K4E5,
#if defined CONFIG_S5K4E5_POSITION_FRONT
	.sensor_position = SENSOR_POSITION_FRONT,
#elif defined CONFIG_S5K4E5_POSITION_REAR
	.sensor_position = SENSOR_POSITION_REAR,
#endif
#if defined CONFIG_S5K4E5_CSI_C
	.csi_id = CSI_ID_A,
	.flite_id = FLITE_ID_A,
	.i2c_channel = SENSOR_CONTROL_I2C0,
#elif defined CONFIG_S5K4E5_CSI_D
	.csi_id = CSI_ID_B,
	.flite_id = FLITE_ID_B,
	.i2c_channel = SENSOR_CONTROL_I2C1,
#endif
	.max_width = 2560,
	.max_height = 1920,
	.max_frame_rate = 30,

	.mipi_lanes = 2,
	.mipi_settle = 12,
	.mipi_align = 24,
};
#endif

#if defined CONFIG_VIDEO_S5K6A3
static struct exynos5_fimc_is_sensor_info s5k6a3 = {
	.sensor_name = "S5K6A3",
	.sensor_id = SENSOR_NAME_S5K6A3,
#if defined CONFIG_S5K6A3_POSITION_FRONT
	.sensor_position = SENSOR_POSITION_FRONT,
#elif defined CONFIG_S5K6A3_POSITION_REAR
	.sensor_position = SENSOR_POSITION_REAR,
#endif
#if defined CONFIG_S5K6A3_CSI_C
	.csi_id = CSI_ID_A,
	.flite_id = FLITE_ID_A,
	.i2c_channel = SENSOR_CONTROL_I2C0,
#elif defined CONFIG_S5K6A3_CSI_D
	.csi_id = CSI_ID_B,
	.flite_id = FLITE_ID_B,
	.i2c_channel = SENSOR_CONTROL_I2C1,
#endif
	.max_width = 1280,
	.max_height = 720,
	.max_frame_rate = 30,

	.mipi_lanes = 1,
	.mipi_settle = 12,
	.mipi_align = 24,
};
#endif

static struct exynos5_fimc_is_regulator_info regulator_manta = {
	.cam_core = "cam_core_1.8v",
	.cam_io = "cam_io_from_1.8v",
	.cam_af = "cam_af_2.8v",
	.cam_vt = "vt_cam_1.8v",
};

static struct exynos5_fimc_is_gpio_info gpio_manta = {
	.gpio[0] = {
		.pin = EXYNOS5_GPE0(0),
		.name = "GPE0",
		.value = 0,
		.act = GPIO_RESET,
	},
	.gpio[1] = {
		.pin = EXYNOS5_GPG1(6),
		.name = "GPG1",
		.value = 0,
		.act = GPIO_RESET,
	},
	.gpio[2] = {
		.pin = EXYNOS5_GPE0(7),
		.name = "GPE0",
		.value = (3<<28),
		.act = GPIO_PULL_NONE,
	},
	.gpio[3] = {
		.pin = EXYNOS5_GPE1(1),
		.name = "GPE1",
		.value = (3<<4),
		.act = GPIO_PULL_NONE,
	},
	.gpio[4] = {
		.pin = EXYNOS5_GPF0(0),
		.name = "GPF0",
		.value = (2<<0),
		.act = GPIO_PULL_NONE,
	},
	.gpio[5] = {
		.pin = EXYNOS5_GPF0(1),
		.name = "GPF0",
		.value = (2<<4),
		.act = GPIO_PULL_NONE,
	},
	.gpio[6] = {
		.pin = EXYNOS5_GPF0(2),
		.name = "GPF0",
		.value = (2<<8),
		.act = GPIO_PULL_NONE,
	},
	.gpio[7] = {
		.pin = EXYNOS5_GPF0(3),
		.name = "GPF0",
		.value = (2<<12),
		.act = GPIO_PULL_NONE,
	},
	.gpio[8] = {
		.pin = EXYNOS5_GPG1(0),
		.name = "GPG1",
		.value = (2<<0),
		.act = GPIO_PULL_NONE,
	},
	.gpio[9] = {
		.pin = EXYNOS5_GPG1(1),
		.name = "GPG1",
		.value = (2<<4),
		.act = GPIO_PULL_NONE,
	},
	.gpio[10] = {
		.pin = EXYNOS5_GPG2(1),
		.name = "GPG2",
		.value = (2<<4),
		.act = GPIO_PULL_NONE,
	},
	.gpio[11] = {
		.pin = EXYNOS5_GPH0(3),
		.name = "GPH0",
		.value = (2<<12),
		.act = GPIO_PULL_NONE,
	},
	.gpio[12] = {
		.pin = EXYNOS5_GPV0(3),
		.name = "GPV0",
		.value = 1,
		.act = GPIO_OUTPUT,
	},
};

static struct platform_device *camera_devices[] __initdata = {
	&exynos5_device_fimc_is,
};

static void __init manta_camera_sysmmu_init(void)
{
	platform_set_sysmmu(&SYSMMU_PLATDEV(isp).dev,
					&exynos5_device_fimc_is.dev);

}

void __init exynos5_manta_camera_init(void)
{
	manta_camera_sysmmu_init();
	platform_add_devices(camera_devices, ARRAY_SIZE(camera_devices));

	dev_set_name(&exynos5_device_fimc_is.dev, "s5p-mipi-csis.0");
	clk_add_alias("gscl_wrap0", "exynos5-fimc-is", "gscl_wrap0",
			&exynos5_device_fimc_is.dev);
	clk_add_alias("sclk_gscl_wrap0", "exynos5-fimc-is", "sclk_gscl_wrap0",
			&exynos5_device_fimc_is.dev);

	dev_set_name(&exynos5_device_fimc_is.dev, "s5p-mipi-csis.1");
	clk_add_alias("gscl_wrap1", "exynos5-fimc-is", "gscl_wrap1",
			&exynos5_device_fimc_is.dev);
	clk_add_alias("sclk_gscl_wrap1", "exynos5-fimc-is", "sclk_gscl_wrap1",
			&exynos5_device_fimc_is.dev);

	dev_set_name(&exynos5_device_fimc_is.dev, "exynos-gsc.0");
	clk_add_alias("gscl", "exynos5-fimc-is", "gscl",
			&exynos5_device_fimc_is.dev);
	dev_set_name(&exynos5_device_fimc_is.dev, "exynos5-fimc-is");

#if defined CONFIG_VIDEO_S5K6A3
	exynos5_fimc_is_data.sensor_info[s5k6a3.sensor_position] = &s5k6a3;
#endif
#if defined CONFIG_VIDEO_S5K4E5
	exynos5_fimc_is_data.sensor_info[s5k4e5.sensor_position] = &s5k4e5;
#endif
	exynos5_fimc_is_data.regulator_info = &regulator_manta;
	exynos5_fimc_is_data.gpio_info = &gpio_manta;

	exynos5_fimc_is_set_platdata(&exynos5_fimc_is_data);
}

