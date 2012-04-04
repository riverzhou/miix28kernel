/* linux/arch/arm/mach-exynos/manta-pm.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/mfd/max77686.h>
#include <linux/regulator/machine.h>

#include <asm/system_misc.h>

#include <mach/regs-pmu.h>

#include "board-manta.h"
#include "common.h"

#define REBOOT_MODE_PREFIX	0x12345670
#define REBOOT_MODE_NONE	0
#define REBOOT_MODE_RECOVERY	4
#define REBOOT_MODE_FAST_BOOT	7

#define REBOOT_MODE_NO_LPM	0x12345678
#define REBOOT_MODE_LPM		0

static struct regulator_consumer_supply ldo3_supply[] = {
	REGULATOR_SUPPLY("vcc_1.8v", NULL),
	REGULATOR_SUPPLY("AVDD2", NULL),
	REGULATOR_SUPPLY("CPVDD", NULL),
	REGULATOR_SUPPLY("DBVDD1", NULL),
	REGULATOR_SUPPLY("DBVDD2", NULL),
	REGULATOR_SUPPLY("DBVDD3", NULL)
};

static struct regulator_consumer_supply ldo4_supply[] = {
	REGULATOR_SUPPLY("vcc_2.8v", NULL),
};

static struct regulator_consumer_supply ldo8_supply[] = {
	REGULATOR_SUPPLY("vmipi_1.0v", NULL),
};

static struct regulator_consumer_supply ldo9_supply[] = {
	REGULATOR_SUPPLY("touch_vdd_1.8v", NULL),
};

static struct regulator_consumer_supply ldo10_supply[] = {
	REGULATOR_SUPPLY("vmipi_1.8v", NULL),
};

static struct regulator_consumer_supply ldo11_supply[] = {
	REGULATOR_SUPPLY("vabb1_1.9v", NULL),
};

static struct regulator_consumer_supply ldo12_supply[] = {
	REGULATOR_SUPPLY("votg_3.0v", NULL),
};

static struct regulator_consumer_supply ldo14_supply[] = {
	REGULATOR_SUPPLY("vabb02_1.8v", NULL),
};

static struct regulator_consumer_supply ldo15_supply[] = {
	REGULATOR_SUPPLY("vhsic_1.0v", NULL),
};

static struct regulator_consumer_supply ldo16_supply[] = {
	REGULATOR_SUPPLY("vhsic_1.8v", NULL),
};

static struct regulator_consumer_supply ldo18_supply[] = {
	REGULATOR_SUPPLY("cam_io_from_1.8v", NULL),
};

static struct regulator_consumer_supply ldo19_supply[] = {
	REGULATOR_SUPPLY("vt_cam_1.8v", NULL),
};

static struct regulator_consumer_supply ldo20_supply[] = {
	REGULATOR_SUPPLY("vmem_vdd_1.8v", NULL),
};

static struct regulator_consumer_supply ldo21_supply[] = {
	REGULATOR_SUPPLY("vtf_2.8v", NULL),
};

static struct regulator_consumer_supply ldo22_supply[] = {
	REGULATOR_SUPPLY("cam_core_1.8v", NULL),
};

static struct regulator_consumer_supply ldo23_supply[] = {
	REGULATOR_SUPPLY("touch_avdd", NULL),
};

static struct regulator_consumer_supply ldo24_supply[] = {
	REGULATOR_SUPPLY("cam_af_2.8v", NULL),
};

static struct regulator_consumer_supply ldo25_supply[] = {
	REGULATOR_SUPPLY("vadc_3.3v", NULL),
};

static struct regulator_consumer_supply ldo26_supply[] = {
	REGULATOR_SUPPLY("irda_3.3v", NULL),
};

static struct regulator_consumer_supply max77686_buck1 =
REGULATOR_SUPPLY("vdd_mif", NULL);

static struct regulator_consumer_supply max77686_buck2 =
REGULATOR_SUPPLY("vdd_arm", NULL);

static struct regulator_consumer_supply max77686_buck3 =
REGULATOR_SUPPLY("vdd_int", NULL);

static struct regulator_consumer_supply max77686_buck4 =
REGULATOR_SUPPLY("vdd_g3d", NULL);

static struct regulator_consumer_supply max77686_buck9 =
REGULATOR_SUPPLY("cam_isp_core", NULL);

static struct regulator_consumer_supply max77686_enp32khz[] = {
	REGULATOR_SUPPLY("lpo_in", "bcm47511"),
	REGULATOR_SUPPLY("lpo", "bcm4334_bluetooth"),
};

#define REGULATOR_INIT(_ldo, _name, _min_uV, _max_uV, _always_on, _ops_mask, \
		       _disabled)					\
	static struct regulator_init_data _ldo##_init_data = {		\
		.constraints = {					\
			.name	= _name,				\
			.min_uV = _min_uV,				\
			.max_uV = _max_uV,				\
			.always_on	= _always_on,			\
			.boot_on	= _always_on,			\
			.apply_uV	= 1,				\
			.valid_ops_mask = _ops_mask,			\
			.state_mem	= {				\
				.disabled =				\
					(_disabled == -1 ? 0 : _disabled),\
				.enabled =				\
					(_disabled == -1 ? 0 : !(_disabled)),\
			},						\
		},							\
		.num_consumer_supplies = ARRAY_SIZE(_ldo##_supply),	\
		.consumer_supplies = &_ldo##_supply[0],			\
	};

REGULATOR_INIT(ldo3, "VCC_1.8V_AP", 1800000, 1800000, 1, 0, 0);
REGULATOR_INIT(ldo4, "VCC_2.8V_AP", 2800000, 2800000, 1, 0, 0);
REGULATOR_INIT(ldo8, "VMIPI_1.0V", 1000000, 1000000, 1,
	       REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo9, "TOUCH_VDD_1.8V", 1800000, 1800000, 0,
	       REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo10, "VMIPI_1.8V", 1800000, 1800000, 1,
	       REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo11, "VABB1_1.8V", 1800000, 1800000, 1,
	       REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo12, "VUOTG_3.0V", 3000000, 3000000, 1,
	       REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo14, "VABB02_1.8V", 1800000, 1800000, 0,
	       REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo15, "VHSIC_1.0V", 1000000, 1000000, 1,
	       REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo16, "VHSIC_1.8V", 1800000, 1800000, 0,
	       REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo18, "CAM_IO_FROM_1.8V", 1800000, 1800000, 0,
	       REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo19, "VT_CAM_1.8V", 1800000, 1800000, 0,
	       REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo20, "VMEM_VDD_1.8V", 1800000, 1800000, 0,
	       REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo21, "VTF_2.8V", 2800000, 2800000, 0,
	       REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo22, "CAM_CORE_1.8v", 1800000, 1800000, 0,
	       REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo23, "TSP_AVDD_2.8V", 2800000, 2800000, 0,
	       REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo24, "CAM_AF_2.8V", 2800000, 2800000, 0,
	       REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo25, "VADC_3.3V", 3300000, 3300000, 1,
	       REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo26, "IRDA_3.3V", 3300000, 3300000, 0,
	       REGULATOR_CHANGE_STATUS, 1);

static struct regulator_init_data max77686_buck1_data = {
	.constraints = {
			.name = "vdd_mif range",
			.min_uV = 850000,
			.max_uV = 1100000,
			.always_on = 1,
			.boot_on = 1,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
			},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_buck1,
};

static struct regulator_init_data max77686_buck2_data = {
	.constraints = {
			.name = "vdd_arm range",
			.min_uV = 850000,
			.max_uV = 1500000,
			.always_on = 1,
			.boot_on = 1,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
			},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_buck2,
};

static struct regulator_init_data max77686_buck3_data = {
	.constraints = {
			.name = "vdd_int range",
			.min_uV = 850000,
			.max_uV = 1300000,
			.always_on = 1,
			.boot_on = 1,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
			},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_buck3,
};

static struct regulator_init_data max77686_buck4_data = {
	.constraints = {
			.name = "vdd_g3d range",
			.min_uV = 850000,
			.max_uV = 1150000,
			.boot_on = 1,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
			REGULATOR_CHANGE_STATUS,
			},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_buck4,
};

static struct regulator_init_data max77686_buck9_data = {
	.constraints = {
			.name = "cam_isp_core",
			.min_uV = 1000000,
			.max_uV = 1200000,
			.apply_uV = 1,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
			REGULATOR_CHANGE_STATUS,
			.state_mem = {
				      .disabled = 1,
				      },
			},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_buck9,
};

static struct regulator_init_data max77686_enp32khz_data = {
	.constraints = {
			.name = "32KHZ_PMIC",
			.always_on = 1,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.state_mem = {
				      .enabled = 1,
				      .disabled = 0,
				      },
			},
	.num_consumer_supplies = ARRAY_SIZE(max77686_enp32khz),
	.consumer_supplies = max77686_enp32khz,
};

static struct max77686_regulator_data max77686_regulators[] = {
	{MAX77686_BUCK1, &max77686_buck1_data,},
	{MAX77686_BUCK2, &max77686_buck2_data,},
	{MAX77686_BUCK3, &max77686_buck3_data,},
	{MAX77686_BUCK4, &max77686_buck4_data,},
	{MAX77686_BUCK9, &max77686_buck9_data,},
	{MAX77686_LDO3, &ldo3_init_data,},
	{MAX77686_LDO4, &ldo4_init_data,},
	{MAX77686_LDO8, &ldo8_init_data,},
	{MAX77686_LDO9, &ldo9_init_data,},
	{MAX77686_LDO10, &ldo10_init_data,},
	{MAX77686_LDO11, &ldo11_init_data,},
	{MAX77686_LDO12, &ldo12_init_data,},
	{MAX77686_LDO14, &ldo14_init_data,},
	{MAX77686_LDO15, &ldo15_init_data,},
	{MAX77686_LDO16, &ldo16_init_data,},
	{MAX77686_LDO18, &ldo18_init_data,},
	{MAX77686_LDO19, &ldo19_init_data,},
	{MAX77686_LDO20, &ldo20_init_data,},
	{MAX77686_LDO21, &ldo21_init_data,},
	{MAX77686_LDO22, &ldo22_init_data,},
	{MAX77686_LDO23, &ldo23_init_data,},
	{MAX77686_LDO24, &ldo24_init_data,},
	{MAX77686_LDO25, &ldo25_init_data,},
	{MAX77686_LDO26, &ldo26_init_data,},
	{MAX77686_P32KH, &max77686_enp32khz_data,},
};

struct max77686_opmode_data max77686_opmode_data[MAX77686_REG_MAX] = {
	[MAX77686_LDO3] = {MAX77686_LDO3, MAX77686_OPMODE_NORMAL},
	[MAX77686_LDO8] = {MAX77686_LDO8, MAX77686_OPMODE_STANDBY},
	[MAX77686_LDO10] = {MAX77686_LDO10, MAX77686_OPMODE_STANDBY},
	[MAX77686_LDO11] = {MAX77686_LDO11, MAX77686_OPMODE_STANDBY},
	[MAX77686_LDO12] = {MAX77686_LDO12, MAX77686_OPMODE_STANDBY},
	[MAX77686_LDO14] = {MAX77686_LDO14, MAX77686_OPMODE_STANDBY},
	[MAX77686_LDO15] = {MAX77686_LDO15, MAX77686_OPMODE_STANDBY},
	[MAX77686_LDO16] = {MAX77686_LDO16, MAX77686_OPMODE_STANDBY},
	[MAX77686_BUCK1] = {MAX77686_BUCK1, MAX77686_OPMODE_STANDBY},
	[MAX77686_BUCK2] = {MAX77686_BUCK2, MAX77686_OPMODE_STANDBY},
	[MAX77686_BUCK3] = {MAX77686_BUCK3, MAX77686_OPMODE_STANDBY},
	[MAX77686_BUCK4] = {MAX77686_BUCK4, MAX77686_OPMODE_STANDBY},
};

static struct max77686_platform_data manta_max77686_info = {
	.num_regulators = ARRAY_SIZE(max77686_regulators),
	.regulators = max77686_regulators,
	.irq_gpio = EXYNOS5_GPX0(2),	/* AP_PMIC_IRQ */
	.irq_base = MANTA_IRQ_BOARD_PMIC_START,
	.wakeup = 1,

	.opmode_data = max77686_opmode_data,
	.ramp_rate = MAX77686_RAMP_RATE_27MV,
	.has_full_constraints = 1,

	.buck234_gpio_dvs = {
			     EXYNOS5_GPV0(7),	/* GPIO_PMIC_DVS1, */
			     EXYNOS5_GPV0(6),	/* GPIO_PMIC_DVS2, */
			     EXYNOS5_GPV0(5),	/* GPIO_PMIC_DVS3, */
			     },
	.buck234_gpio_selb = {
			      EXYNOS5_GPV0(4),	/* GPIO_BUCK2_SEL, */
			      EXYNOS5_GPV0(1),	/* GPIO_BUCK3_SEL, */
			      EXYNOS5_GPV0(0),	/* GPIO_BUCK4_SEL, */
			      },

	/*for future work after DVS Table */
	.buck2_voltage[0] = 1100000,	/* 1.1V */
	.buck2_voltage[1] = 1100000,	/* 1.1V */
	.buck2_voltage[2] = 1100000,	/* 1.1V */
	.buck2_voltage[3] = 1100000,	/* 1.1V */
	.buck2_voltage[4] = 1100000,	/* 1.1V */
	.buck2_voltage[5] = 1100000,	/* 1.1V */
	.buck2_voltage[6] = 1100000,	/* 1.1V */
	.buck2_voltage[7] = 1100000,	/* 1.1V */

	.buck3_voltage[0] = 1100000,	/* 1.1V */
	.buck3_voltage[1] = 1100000,	/* 1.1V */
	.buck3_voltage[2] = 1100000,	/* 1.1V */
	.buck3_voltage[3] = 1100000,	/* 1.1V */
	.buck3_voltage[4] = 1100000,	/* 1.1V */
	.buck3_voltage[5] = 1100000,	/* 1.1V */
	.buck3_voltage[6] = 1100000,	/* 1.1V */
	.buck3_voltage[7] = 1100000,	/* 1.1V */

	.buck4_voltage[0] = 1100000,	/* 1.1V */
	.buck4_voltage[1] = 1100000,	/* 1.1V */
	.buck4_voltage[2] = 1100000,	/* 1.1V */
	.buck4_voltage[3] = 1100000,	/* 1.1V */
	.buck4_voltage[4] = 1100000,	/* 1.1V */
	.buck4_voltage[5] = 1100000,	/* 1.1V */
	.buck4_voltage[6] = 1100000,	/* 1.1V */
	.buck4_voltage[7] = 1100000,	/* 1.1V */
};

static struct i2c_board_info i2c_devs5[] __initdata = {
	{
		I2C_BOARD_INFO("max77686", (0x12 >> 1)),
		.platform_data = &manta_max77686_info,
	},
};

static void manta_power_off(void)
{
	local_irq_disable();

	writel(readl(EXYNOS_PS_HOLD_CONTROL) & ~BIT(8),
		EXYNOS_PS_HOLD_CONTROL);

	exynos5_restart(0, 0);
}

static void manta_reboot(char str, const char *cmd)
{
	local_irq_disable();

	writel(REBOOT_MODE_NO_LPM, EXYNOS_INFORM2); /* Don't enter lpm mode */
	writel(REBOOT_MODE_PREFIX | REBOOT_MODE_NONE, EXYNOS_INFORM3);

	if (cmd) {
		if (!strcmp(cmd, "recovery"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_RECOVERY,
			       EXYNOS_INFORM3);
		else if (!strcmp(cmd, "bootloader"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_FAST_BOOT,
			       EXYNOS_INFORM2);
	}

	exynos5_restart(str, cmd); /* S/W reset: INFORM0~3:  Keep its value */
}

void __init exynos5_manta_power_init(void)
{
	pm_power_off = manta_power_off;
	arm_pm_restart = manta_reboot;

	i2c_register_board_info(5, i2c_devs5, ARRAY_SIZE(i2c_devs5));
}
