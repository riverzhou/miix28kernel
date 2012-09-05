/* arch/arm/mach-exynos/board-manta-pogo.c
 *
 * Copyright (C) 2012 Samsung Electronics.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/ktime.h>
#include <linux/device.h>

#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/switch.h>
#include <linux/wakelock.h>

#include <plat/gpio-cfg.h>
#include "board-manta.h"
#include <linux/platform_data/android_battery.h>

/* including start and stop bits */
#define CMD_READ_SZ		8
#define CMD_WRITE_SZ		16
#define RESP_READ_SZ		12
#define RESP_WRITE_SZ		5

#define RW_OFFSET		2
#define ID_OFFSET		3
#define STOP_R_OFFSET		6
#define DATA_OFFSET		6
#define STOP_W_OFFSET		14

#define START_BITS		0x3
#define STOP_BITS		0x1

/* states for dock */
#define POGO_UNDOCKED			0
#define POGO_DESK_DOCK			1

/* states for usb_audio */
#define POGO_NO_AUDIO			0
#define POGO_DIGITAL_AUDIO		2

#define DOCK_STAT_POWER_MASK		3
#define DOCK_STAT_POWER_OFFSET		2
#define DOCK_STAT_POWER_NONE		0
#define DOCK_STAT_POWER_500MA		1
#define DOCK_STAT_POWER_2A		2

#define DOCK_STAT_AUDIO_MASK		3
#define DOCK_STAT_AUDIO_OFFSET		0
#define DOCK_STAT_AUDIO_CONNECTED	1
#define DOCK_STAT_AUDIO_DISCONNECTED	0

#define MFM_DELAY_NS_DEFAULT		(1000 * 10)
#define RESPONSE_INTERVAL_NS		(1000 * 700)	/* 700us */
#define CMD_DELAY_USEC			100		/* 100us */
#define TX_ERR_DELAY_USEC(delay_ns)	((delay_ns) / 1000 * 7 + CMD_DELAY_USEC)
#define check_stop_bits(x, n) ((((x) >> ((n) - 2)) & 1) == (STOP_BITS & 1))
#define raw_resp(x, n) ((x) & ((1 << ((n) - 1)) - 1));

/* GPIO configuration */
#define GPIO_POGO_DATA_BETA		EXYNOS5_GPB0(0)
#define GPIO_POGO_DATA_POST_BETA	EXYNOS5_GPX0(5)

#define GPIO_POGO_SPDIF			EXYNOS5_GPB1(0)
#define GPIO_POGO_SEL1			EXYNOS5_GPG1(0)

static struct switch_dev dock_switch = {
	.name = "dock",
};

static struct switch_dev usb_audio_switch = {
	.name = "usb_audio",
};

struct dock_state {
	struct mutex lock;
	u32 t;
	u32 last_edge_t[2];
	u32 last_edge_i[2];
	bool level;
	bool dock_connected_unknown;

	u32 mfm_delay_ns;
	bool vbus_present;
	struct workqueue_struct *dock_wq;
	struct work_struct dock_work;
	struct wake_lock dock_work_wake_lock;
};

static struct dock_state ds = {
	.lock		= __MUTEX_INITIALIZER(ds.lock),
	.mfm_delay_ns	= MFM_DELAY_NS_DEFAULT,
};

static struct gpio manta_pogo_gpios[] = {
	{GPIO_POGO_DATA_POST_BETA, GPIOF_IN, "dock"},
	{GPIO_POGO_SEL1, GPIOF_OUT_INIT_HIGH, "pogo_sel1"},
};

#define _GPIO_DOCK		(manta_pogo_gpios[0].gpio)

#define dock_out(n) { s3c_gpio_cfgpin(_GPIO_DOCK, S3C_GPIO_OUTPUT); \
	gpio_direction_output(_GPIO_DOCK, n); }
#define dock_out2(n) gpio_set_value(_GPIO_DOCK, n)
#define dock_in() { s3c_gpio_cfgpin(_GPIO_DOCK, S3C_GPIO_INPUT); \
	gpio_direction_input(_GPIO_DOCK); }
#define dock_read() gpio_get_value(_GPIO_DOCK)
#define dock_irq() s3c_gpio_cfgpin(_GPIO_DOCK, S3C_GPIO_SFN(0xF))

static u32 pogo_read_fast_timer(void)
{
	return ktime_to_ns(ktime_get());
}

static u16 make_cmd(int id, bool write, int data)
{
	u16 cmd = (id & 0x7) << ID_OFFSET | write << RW_OFFSET | START_BITS;
	cmd |= STOP_BITS << (write ? STOP_W_OFFSET : STOP_R_OFFSET);
	if (write)
		cmd |= (data & 0xFF) << DATA_OFFSET;
	return cmd;
}

static int dock_get_edge(struct dock_state *s, u32 timeout, u32 tmin, u32 tmax)
{
	bool lin;
	bool in = s->level;
	u32 t;
	do {
		lin = in;
		in = dock_read();
		t = pogo_read_fast_timer();
		if (in != lin) {
			s->last_edge_t[in] = t;
			s->last_edge_i[in] = 0;
			s->level = in;
			if ((s32) (t - tmin) < 0 || (s32) (t - tmax) > 0)
				return -1;
			return 1;
		}
	} while ((s32) (t - timeout) < 0);
	return 0;
}

static bool dock_sync(struct dock_state *s, u32 timeout)
{
	u32 t;

	s->level = dock_read();
	t = pogo_read_fast_timer();

	if (!dock_get_edge(s, t + timeout, 0, 0))
		return false;
	s->last_edge_i[s->level] = 2;
	return !!dock_get_edge(s, s->last_edge_t[s->level] + s->mfm_delay_ns *
			4, 0, 0);
}

static int dock_get_next_bit(struct dock_state *s)
{
	u32 i = s->last_edge_i[!s->level] + ++s->last_edge_i[s->level];
	u32 target = s->last_edge_t[!s->level] + s->mfm_delay_ns * i;
	u32 timeout = target + s->mfm_delay_ns / 2;
	u32 tmin = target - s->mfm_delay_ns / 4;
	u32 tmax = target + s->mfm_delay_ns / 4;
	return dock_get_edge(s, timeout, tmin, tmax);
}

static u32 dock_get_bits(struct dock_state *s, int count, int *errp)
{
	u32 data = 0;
	u32 m = 1;
	int ret;
	int err = 0;
	while (count--) {
		ret = dock_get_next_bit(s);
		if (ret)
			data |= m;
		if (ret < 0)
			err++;
		m <<= 1;
	}
	if (errp)
		*errp = err;
	return data;
}

static int dock_send_bits(struct dock_state *s, u32 data, int count, int period)
{
	u32 t, t0, to;

	dock_out2(s->level);
	t = to = 0;
	t0 = pogo_read_fast_timer();

	while (count--) {
		if (data & 1)
			dock_out2((s->level = !s->level));

		data >>= 1;

		to += s->mfm_delay_ns;
		do {
			t = pogo_read_fast_timer() - t0;
		} while (t < to);
		if (t - to > period / 4) {
			pr_debug("dock: to = %d, t = %d\n", to, t);
			return -EIO;
		}
	}
	return 0;
}

static u32 mfm_encode(u16 data, int count, bool p)
{
	u32 mask;
	u32 mfm = 0;
	u32 clock = ~data & ~(data << 1 | !!p);
	for (mask = 1UL << (count - 1); mask; mask >>= 1) {
		mfm |= (data & mask);
		mfm <<= 1;
		mfm |= (clock & mask);
	}
	return mfm;
}

static u32 mfm_decode(u32 mfm)
{
	u32 data = 0;
	u32 clock = 0;
	u32 mask = 1;
	while (mfm) {
		if (mfm & 1)
			clock |= mask;
		mfm >>= 1;
		if (mfm & 1)
			data |= mask;
		mfm >>= 1;
		mask <<= 1;
	}
	return data;
}

static int dock_command(struct dock_state *s, u16 cmd, int len, int retlen)
{
	u32 mfm;
	int count;
	u32 data = cmd;
	int tx, ret;
	int err = -1;
	unsigned long flags;

	dock_out(s->level = 0);

	mfm = mfm_encode(data, len, false);
	count = len * 2;

	pr_debug("%s: data 0x%x mfm 0x%x\n", __func__, cmd, mfm);

	local_irq_save(flags);
	tx = dock_send_bits(s, mfm, count, s->mfm_delay_ns);

	if (!tx) {
		dock_in();
		if (dock_sync(s, RESPONSE_INTERVAL_NS)) {
			ret = dock_get_bits(s, retlen * 2, &err);
		} else {
			pr_debug("%s: response sync error\n", __func__);
			ret = -1;
		}
		dock_out(0);
	}
	local_irq_restore(flags);

	udelay(tx < 0 ? TX_ERR_DELAY_USEC(s->mfm_delay_ns) : CMD_DELAY_USEC);

	if (tx < 0) {
		pr_debug("dock_command: %x: transmission err %d\n", cmd, tx);
		return tx;
	}
	if (ret < 0) {
		pr_debug("dock_command: %x: no response\n", cmd);
		return ret;
	}
	data = mfm_decode(ret);
	mfm = mfm_encode(data, retlen, true);
	if (mfm != ret || err || !check_stop_bits(data, retlen)) {
		pr_debug("dock_command: %x: bad response, data %x, mfm %x %x, err %d\n",
						cmd, data, mfm, ret, err);
		return -EIO;
	}

	return raw_resp(data, retlen);
}

static int dock_command_retry(struct dock_state *s, u16 cmd, size_t len,
		size_t retlen)
{
	int retry = 20;
	int ret;
	while (retry--) {
		ret = dock_command(s, cmd, len, retlen);
		if (ret >= 0)
			return ret;
		if (retry != 19)
			usleep_range(10000, 11000);
	}
	s->dock_connected_unknown = true;
	return -EIO;
}

static int dock_send_cmd(struct dock_state *s, int id, bool write, int data)
{
	int ret;
	u16 cmd = make_cmd(id, write, data);

	ret = dock_command_retry(s, cmd, write ? CMD_WRITE_SZ : CMD_READ_SZ,
			write ? RESP_WRITE_SZ - 2 : RESP_READ_SZ - 2);

	if (ret < 0)
		pr_warning("%s: failed, cmd: %x, write: %d, data: %x\n",
			__func__, cmd, write, data);

	return ret;
}

static int dock_read_multi(struct dock_state *s, int addr, u8 *data, size_t len)
{
	int ret;
	int i;
	u8 suml, sumr = -1;

	int retry = 20;
	while (retry--) {
		suml = 0;
		for (i = 0; i <= len; i++) {
			ret = dock_send_cmd(s, addr + i, false, 0);
			if (ret < 0)
				return ret;
			if (i < len) {
				data[i] = ret;
				suml += ret;
			} else
				sumr = ret;
		}
		if (sumr == suml)
			return 0;

		pr_warning("dock_read_multi(%x): bad checksum, %x != %x\n",
			   addr, sumr, suml);
	}
	return -EIO;
}

static int dock_write_multi(struct dock_state *s, int addr, u8 *data,
		size_t len)
{
	int ret;
	int i;
	u8 sum;

	int retry = 20;
	while (retry--) {
		sum = 0;
		for (i = 0; i < len; i++) {
			sum += data[i];
			ret = dock_send_cmd(s, addr + i, true, data[i]);
			if (ret < 0)
				return ret;
		}
		ret = dock_send_cmd(s, addr + len, true, sum);
		if (ret < 0)
			return ret;
		/* check sum error */
		if (ret == 0)
			continue;
		return 0;
	}
	return -EIO;
}

void manta_pogo_switch_set(int value)
{
	gpio_set_value(GPIO_POGO_SEL1, value);
}


static int dock_acquire(struct dock_state *s)
{
	mutex_lock(&s->lock);
	manta_pogo_switch_set(1);
	dock_in();
	if (dock_read()) {
		/* Allow some time for the dock pull-down resistor to discharge
		 * the line capacitance.
		 */
		usleep_range(1000, 2000);
		if (dock_read()) {
			mutex_unlock(&s->lock);
			return -ENOENT;
		}
	}

	pr_debug("%s: acquired dock\n", __func__);

	dock_out(0);
	s->level = false;

	return 0;
}

static void dock_release(struct dock_state *s)
{
	dock_in();
	mutex_unlock(&s->lock);
	pr_debug("%s: released dock\n", __func__);
}

enum {
	DOCK_STATUS = 0x1,
	DOCK_ID_ADDR = 0x2,
};

static int dock_check_status(struct dock_state *s, int *charge_source)
{
	int ret = 0;
	int dock_stat, power;

	if (!s->vbus_present || dock_acquire(s))
		goto no_dock;

	dock_out(0);

	if (s->dock_connected_unknown) {
		/* force a new dock notification if a command failed */
		switch_set_state(&dock_switch, POGO_UNDOCKED);
		switch_set_state(&usb_audio_switch, POGO_NO_AUDIO);
		s->dock_connected_unknown = false;
	}

	pr_debug("%s: sending status command\n", __func__);

	dock_stat = dock_send_cmd(s, DOCK_STATUS, false, 0);
	dock_release(s);

	pr_debug("%s: Dock status %02x\n", __func__, dock_stat);
	if (dock_stat >= 0) {
		switch_set_state(&dock_switch, POGO_DESK_DOCK);
		switch_set_state(&usb_audio_switch,
				(dock_stat & DOCK_STAT_AUDIO_CONNECTED) ?
				POGO_DIGITAL_AUDIO : POGO_NO_AUDIO);

		if (charge_source) {
			power = (dock_stat >> DOCK_STAT_POWER_OFFSET) &
					DOCK_STAT_POWER_MASK;
			switch (power) {
			case DOCK_STAT_POWER_500MA:
				*charge_source = CHARGE_SOURCE_USB;
				break;
			case DOCK_STAT_POWER_2A:
				*charge_source = CHARGE_SOURCE_AC;
				break;
			default:
				pr_warn("%s: unknown dock power state %d, default to USB\n",
							__func__, power);
				*charge_source = CHARGE_SOURCE_USB;
			}
		}

		dock_irq();
		goto done;
	}
no_dock:
	ret = -ENOENT;
	switch_set_state(&dock_switch, POGO_UNDOCKED);
	switch_set_state(&usb_audio_switch, POGO_NO_AUDIO);
done:
	wake_unlock(&s->dock_work_wake_lock);

	return ret;
}

int manta_pogo_set_vbus(bool status)
{
	struct dock_state *s = &ds;
	int ret, charge_source;

	s->vbus_present = status;
	pr_debug("%s: status %d\n", __func__, status ? 1 : 0);

	if (status) {
		wake_lock(&s->dock_work_wake_lock);
		ret = dock_check_status(s, &charge_source);
		if (ret < 0)
			return ret;
	} else {
		dock_in();
		switch_set_state(&dock_switch, POGO_UNDOCKED);
		switch_set_state(&usb_audio_switch, POGO_NO_AUDIO);
		s->dock_connected_unknown = false;
		charge_source = CHARGE_SOURCE_NONE;
	}

	return charge_source;
}

static void dock_work_proc(struct work_struct *work)
{
	struct dock_state *s = container_of(work, struct dock_state,
			dock_work);

	dock_check_status(s, NULL);
}

static irqreturn_t pogo_data_interrupt(int irq, void *data)
{
	struct dock_state *s = data;
	pr_debug("%s\n", __func__);

	wake_lock(&s->dock_work_wake_lock);
	queue_work(s->dock_wq, &s->dock_work);

	return IRQ_HANDLED;
}

#ifdef DEBUG
static ssize_t dev_attr_vbus_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ds.vbus_present ? 1 : 0);
}

static ssize_t dev_attr_vbus_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	if (size) {
		manta_pogo_set_vbus(buf[0] == '1');
		return size;
	} else
		return -EINVAL;
}
static DEVICE_ATTR(vbus, S_IRUGO | S_IWUSR, dev_attr_vbus_show,
		dev_attr_vbus_store);
#endif

static ssize_t dev_attr_delay_ns_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ds.mfm_delay_ns);
}

static ssize_t dev_attr_delay_ns_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	if (size) {
		ds.mfm_delay_ns = simple_strtoul(buf, NULL, 10);
		return size;
	} else
		return -EINVAL;
}
static DEVICE_ATTR(delay_ns, S_IRUGO | S_IWUSR, dev_attr_delay_ns_show,
		dev_attr_delay_ns_store);

static ssize_t dev_attr_dock_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	u8 dock_id[4];

	ret = dock_acquire(&ds);
	if (ret < 0)
		goto fail;
	ret = dock_read_multi(&ds, DOCK_ID_ADDR, dock_id, 4);
	dock_release(&ds);
	if (ret < 0)
		goto fail;

	ret = sprintf(buf, "%02x:%02x:%02x:%02x\n\n",
		dock_id[0], dock_id[1], dock_id[2], dock_id[3]);
fail:
	dock_irq();
	return ret;
}

static ssize_t dev_attr_dock_id_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, i;
	int val[4];
	u8 dock_id[4];

	if (size < 11 || sscanf(buf, "%2x:%2x:%2x:%2x", &val[0], &val[1],
		&val[2], &val[3]) != 4)
		return -EINVAL;

	for (i = 0; i < 4; i++)
		dock_id[i] = val[i];

	ret = dock_acquire(&ds);
	if (ret < 0)
		goto fail;
	ret = dock_write_multi(&ds, DOCK_ID_ADDR, dock_id, 4);
	dock_release(&ds);
	if (ret < 0)
		goto fail;

	ret = size;
fail:
	dock_irq();
	return ret;
}
static DEVICE_ATTR(dock_id, S_IRUGO | S_IWUSR, dev_attr_dock_id_show,
		dev_attr_dock_id_store);

void __init exynos5_manta_pogo_init(void)
{
	struct dock_state *s = &ds;
	int ret;

	if (exynos5_manta_get_revision() <= MANTA_REV_BETA)
		manta_pogo_gpios[0].gpio = GPIO_POGO_DATA_BETA;

	wake_lock_init(&s->dock_work_wake_lock, WAKE_LOCK_SUSPEND, "dock");

	INIT_WORK(&s->dock_work, dock_work_proc);
	s->dock_wq = create_singlethread_workqueue("dock");

	s3c_gpio_cfgpin(GPIO_POGO_SEL1, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_POGO_SEL1, S3C_GPIO_PULL_NONE);
	ret = gpio_request_array(manta_pogo_gpios,
				 ARRAY_SIZE(manta_pogo_gpios));
	if (ret)
		pr_err("%s: cannot request gpios\n", __func__);

	if (switch_dev_register(&dock_switch) == 0) {
		ret = device_create_file(dock_switch.dev, &dev_attr_delay_ns);
		WARN_ON(ret);
		ret = device_create_file(dock_switch.dev, &dev_attr_dock_id);
		WARN_ON(ret);
#ifdef DEBUG
		ret = device_create_file(dock_switch.dev, &dev_attr_vbus);
		WARN_ON(ret);
#endif
	}

	WARN_ON(switch_dev_register(&usb_audio_switch));
}

int __init pogo_data_irq_late_init(void)
{
	int ret, data_irq;

	dock_irq();
	s3c_gpio_setpull(_GPIO_DOCK, S3C_GPIO_PULL_NONE);

	s5p_register_gpio_interrupt(_GPIO_DOCK);
	data_irq = gpio_to_irq(_GPIO_DOCK);

	ret = request_threaded_irq(data_irq, NULL, pogo_data_interrupt,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "dock", &ds);
	if (ret < 0)
		pr_err("%s: failed to request irq %d, rc: %d\n", __func__,
			data_irq, ret);

	ret = enable_irq_wake(data_irq);
	if (ret)
		pr_warn("%s: failed to enable irq_wake for POGO_DATA\n",
			__func__);

	return ret;
}

late_initcall(pogo_data_irq_late_init);
