/*
 * GICv3 ITS emulation
 *
 * Copyright (C) 2015 ARM Ltd.
 * Author: Andre Przywara <andre.przywara@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/cpu.h>
#include <linux/kvm.h>
#include <linux/kvm_host.h>
#include <linux/interrupt.h>
#include <linux/list.h>

#include <linux/irqchip/arm-gic-v3.h>
#include <kvm/arm_vgic.h>

#include <asm/kvm_emulate.h>
#include <asm/kvm_arm.h>
#include <asm/kvm_mmu.h>

#include "vgic.h"
#include "its-emul.h"

struct its_device {
	struct list_head dev_list;
	struct list_head itt;
	u32 device_id;
};

struct its_collection {
	struct list_head coll_list;
	u32 collection_id;
	u32 target_addr;
};

struct its_itte {
	struct list_head itte_list;
	struct its_collection *collection;
	u32 lpi;
	u32 event_id;
	bool enabled;
	unsigned long *pending;
};

#define for_each_lpi(dev, itte, kvm) \
	list_for_each_entry(dev, &(kvm)->arch.vgic.its.device_list, dev_list) \
		list_for_each_entry(itte, &(dev)->itt, itte_list)

static struct its_itte *find_itte_by_lpi(struct kvm *kvm, int lpi)
{
	struct its_device *device;
	struct its_itte *itte;

	for_each_lpi(device, itte, kvm) {
		if (itte->lpi == lpi)
			return itte;
	}
	return NULL;
}

#define BASER_BASE_ADDRESS(x) ((x) & 0xfffffffff000ULL)

/* The distributor lock is held by the VGIC MMIO handler. */
static bool handle_mmio_misc_gits(struct kvm_vcpu *vcpu,
				  struct kvm_exit_mmio *mmio,
				  phys_addr_t offset)
{
	struct vgic_its *its = &vcpu->kvm->arch.vgic.its;
	u32 reg;
	bool was_enabled;

	switch (offset & ~3) {
	case 0x00:		/* GITS_CTLR */
		/* We never defer any command execution. */
		reg = GITS_CTLR_QUIESCENT;
		if (its->enabled)
			reg |= GITS_CTLR_ENABLE;
		was_enabled = its->enabled;
		vgic_reg_access(mmio, &reg, offset & 3,
				ACCESS_READ_VALUE | ACCESS_WRITE_VALUE);
		its->enabled = !!(reg & GITS_CTLR_ENABLE);
		return !was_enabled && its->enabled;
	case 0x04:		/* GITS_IIDR */
		reg = (PRODUCT_ID_KVM << 24) | (IMPLEMENTER_ARM << 0);
		vgic_reg_access(mmio, &reg, offset & 3,
				ACCESS_READ_VALUE | ACCESS_WRITE_IGNORED);
		break;
	case 0x08:		/* GITS_TYPER */
		/*
		 * We use linear CPU numbers for redistributor addressing,
		 * so GITS_TYPER.PTA is 0.
		 * To avoid memory waste on the guest side, we keep the
		 * number of IDBits and DevBits low for the time being.
		 * This could later be made configurable by userland.
		 * Since we have all collections in linked list, we claim
		 * that we can hold all of the collection tables in our
		 * own memory and that the ITT entry size is 1 byte (the
		 * smallest possible one).
		 */
		reg = GITS_TYPER_PLPIS;
		reg |= 0xff << GITS_TYPER_HWCOLLCNT_SHIFT;
		reg |= 0x0f << GITS_TYPER_DEVBITS_SHIFT;
		reg |= 0x0f << GITS_TYPER_IDBITS_SHIFT;
		vgic_reg_access(mmio, &reg, offset & 3,
				ACCESS_READ_VALUE | ACCESS_WRITE_IGNORED);
		break;
	case 0x0c:
		/* The upper 32bits of TYPER are all 0 for the time being.
		 * Should we need more than 256 collections, we can enable
		 * some bits in here.
		 */
		vgic_reg_access(mmio, NULL, offset & 3,
				ACCESS_READ_RAZ | ACCESS_WRITE_IGNORED);
		break;
	}

	return false;
}

static bool handle_mmio_gits_idregs(struct kvm_vcpu *vcpu,
				    struct kvm_exit_mmio *mmio,
				    phys_addr_t offset)
{
	u32 reg = 0;
	int idreg = (offset & ~3) + GITS_IDREGS_BASE;

	switch (idreg) {
	case GITS_PIDR2:
		reg = GIC_PIDR2_ARCH_GICv3;
		break;
	case GITS_PIDR4:
		/* This is a 64K software visible page */
		reg = 0x40;
		break;
	/* Those are the ID registers for (any) GIC. */
	case GITS_CIDR0:
		reg = 0x0d;
		break;
	case GITS_CIDR1:
		reg = 0xf0;
		break;
	case GITS_CIDR2:
		reg = 0x05;
		break;
	case GITS_CIDR3:
		reg = 0xb1;
		break;
	}
	vgic_reg_access(mmio, &reg, offset & 3,
			ACCESS_READ_VALUE | ACCESS_WRITE_IGNORED);
	return false;
}

/*
 * Find all enabled and pending LPIs and queue them into the list
 * registers.
 * The dist lock is held by the caller.
 */
bool vits_queue_lpis(struct kvm_vcpu *vcpu)
{
	struct vgic_its *its = &vcpu->kvm->arch.vgic.its;
	struct its_device *device;
	struct its_itte *itte;
	bool ret = true;

	if (!vgic_has_its(vcpu->kvm))
		return true;
	if (!its->enabled || !vcpu->kvm->arch.vgic.lpis_enabled)
		return true;

	spin_lock(&its->lock);
	for_each_lpi(device, itte, vcpu->kvm) {
		if (!itte->enabled || !test_bit(vcpu->vcpu_id, itte->pending))
			continue;

		if (!itte->collection)
			continue;

		if (itte->collection->target_addr != vcpu->vcpu_id)
			continue;

		__clear_bit(vcpu->vcpu_id, itte->pending);

		ret &= vgic_queue_irq(vcpu, 0, itte->lpi);
	}

	spin_unlock(&its->lock);
	return ret;
}

/* Called with the distributor lock held by the caller. */
void vits_unqueue_lpi(struct kvm_vcpu *vcpu, int lpi)
{
	struct vgic_its *its = &vcpu->kvm->arch.vgic.its;
	struct its_itte *itte;

	spin_lock(&its->lock);

	/* Find the right ITTE and put the pending state back in there */
	itte = find_itte_by_lpi(vcpu->kvm, lpi);
	if (itte)
		__set_bit(vcpu->vcpu_id, itte->pending);

	spin_unlock(&its->lock);
}

static int vits_handle_command(struct kvm_vcpu *vcpu, u64 *its_cmd)
{
	return -ENODEV;
}

static bool handle_mmio_gits_cbaser(struct kvm_vcpu *vcpu,
				    struct kvm_exit_mmio *mmio,
				    phys_addr_t offset)
{
	struct vgic_its *its = &vcpu->kvm->arch.vgic.its;
	int mode = ACCESS_READ_VALUE;

	mode |= its->enabled ? ACCESS_WRITE_IGNORED : ACCESS_WRITE_VALUE;

	vgic_handle_base_register(vcpu, mmio, offset, &its->cbaser, mode);

	/* Writing CBASER resets the read pointer. */
	if (mmio->is_write)
		its->creadr = 0;

	return false;
}

static int its_cmd_buffer_size(struct kvm *kvm)
{
	struct vgic_its *its = &kvm->arch.vgic.its;

	return ((its->cbaser & 0xff) + 1) << 12;
}

static gpa_t its_cmd_buffer_base(struct kvm *kvm)
{
	struct vgic_its *its = &kvm->arch.vgic.its;

	return BASER_BASE_ADDRESS(its->cbaser);
}

/*
 * By writing to CWRITER the guest announces new commands to be processed.
 * Since we cannot read from guest memory inside the ITS spinlock, we
 * iterate over the command buffer (with the lock dropped) until the read
 * pointer matches the write pointer. Other VCPUs writing this register in the
 * meantime will just update the write pointer, leaving the command
 * processing to the first instance of the function.
 */
static bool handle_mmio_gits_cwriter(struct kvm_vcpu *vcpu,
				     struct kvm_exit_mmio *mmio,
				     phys_addr_t offset)
{
	struct vgic_its *its = &vcpu->kvm->arch.vgic.its;
	gpa_t cbaser = its_cmd_buffer_base(vcpu->kvm);
	u64 cmd_buf[4];
	u32 reg;
	bool finished;

	/* The upper 32 bits are RES0 */
	if ((offset & ~3) == 0x04) {
		vgic_reg_access(mmio, &reg, offset & 3,
				ACCESS_READ_RAZ | ACCESS_WRITE_IGNORED);
		return false;
	}

	reg = its->cwriter & 0xfffe0;
	vgic_reg_access(mmio, &reg, offset & 3,
			ACCESS_READ_VALUE | ACCESS_WRITE_VALUE);
	if (!mmio->is_write)
		return false;

	reg &= 0xfffe0;
	if (reg > its_cmd_buffer_size(vcpu->kvm))
		return false;

	spin_lock(&its->lock);

	/*
	 * If there is still another VCPU handling commands, let this
	 * one pick up the new CWRITER and process our new commands as well.
	 */
	finished = (its->cwriter != its->creadr);
	its->cwriter = reg;

	spin_unlock(&its->lock);

	while (!finished) {
		int ret = kvm_read_guest(vcpu->kvm, cbaser + its->creadr,
					 cmd_buf, 32);
		if (ret) {
			/*
			 * Gah, we are screwed. Reset CWRITER to that command
			 * that we have finished processing and return.
			 */
			spin_lock(&its->lock);
			its->cwriter = its->creadr;
			spin_unlock(&its->lock);
			break;
		}
		vits_handle_command(vcpu, cmd_buf);

		spin_lock(&its->lock);
		its->creadr += 32;
		if (its->creadr == its_cmd_buffer_size(vcpu->kvm))
			its->creadr = 0;
		finished = (its->creadr == its->cwriter);
		spin_unlock(&its->lock);
	}

	return false;
}

static bool handle_mmio_gits_creadr(struct kvm_vcpu *vcpu,
				    struct kvm_exit_mmio *mmio,
				    phys_addr_t offset)
{
	struct vgic_its *its = &vcpu->kvm->arch.vgic.its;
	u32 reg;

	switch (offset & ~3) {
	case 0x00:
		reg = its->creadr & 0xfffe0;
		vgic_reg_access(mmio, &reg, offset & 3,
				ACCESS_READ_VALUE | ACCESS_WRITE_IGNORED);
		break;
	case 0x04:
		vgic_reg_access(mmio, &reg, offset & 3,
				ACCESS_READ_RAZ | ACCESS_WRITE_IGNORED);
		break;
	}
	return false;
}

static const struct vgic_io_range vgicv3_its_ranges[] = {
	{
		.base		= GITS_CTLR,
		.len		= 0x10,
		.bits_per_irq	= 0,
		.handle_mmio	= handle_mmio_misc_gits,
	},
	{
		.base		= GITS_CBASER,
		.len		= 0x08,
		.bits_per_irq	= 0,
		.handle_mmio	= handle_mmio_gits_cbaser,
	},
	{
		.base		= GITS_CWRITER,
		.len		= 0x08,
		.bits_per_irq	= 0,
		.handle_mmio	= handle_mmio_gits_cwriter,
	},
	{
		.base		= GITS_CREADR,
		.len		= 0x08,
		.bits_per_irq	= 0,
		.handle_mmio	= handle_mmio_gits_creadr,
	},
	{
		/* We don't need any memory from the guest. */
		.base		= GITS_BASER,
		.len		= 0x40,
		.bits_per_irq	= 0,
		.handle_mmio	= handle_mmio_raz_wi,
	},
	{
		.base		= GITS_IDREGS_BASE,
		.len		= 0x30,
		.bits_per_irq	= 0,
		.handle_mmio	= handle_mmio_gits_idregs,
	},
};

/* This is called on setting the LPI enable bit in the redistributor. */
void vgic_enable_lpis(struct kvm_vcpu *vcpu)
{
}

int vits_init(struct kvm *kvm)
{
	struct vgic_dist *dist = &kvm->arch.vgic;
	struct vgic_its *its = &dist->its;

	dist->pendbaser = kmalloc(sizeof(u64) * dist->nr_cpus, GFP_KERNEL);
	if (!dist->pendbaser)
		return -ENOMEM;

	spin_lock_init(&its->lock);

	INIT_LIST_HEAD(&its->device_list);
	INIT_LIST_HEAD(&its->collection_list);

	its->enabled = false;

	return -ENXIO;
}

void vits_destroy(struct kvm *kvm)
{
	struct vgic_dist *dist = &kvm->arch.vgic;
	struct vgic_its *its = &dist->its;
	struct its_device *dev;
	struct its_itte *itte;
	struct list_head *dev_cur, *dev_temp;
	struct list_head *cur, *temp;

	if (!vgic_has_its(kvm))
		return;

	if (!its->device_list.next)
		return;

	spin_lock(&its->lock);
	list_for_each_safe(dev_cur, dev_temp, &its->device_list) {
		dev = container_of(dev_cur, struct its_device, dev_list);
		list_for_each_safe(cur, temp, &dev->itt) {
			itte = (container_of(cur, struct its_itte, itte_list));
			list_del(cur);
			kfree(itte);
		}
		list_del(dev_cur);
		kfree(dev);
	}

	list_for_each_safe(cur, temp, &its->collection_list) {
		list_del(cur);
		kfree(container_of(cur, struct its_collection, coll_list));
	}

	kfree(dist->pendbaser);

	its->enabled = false;
	spin_unlock(&its->lock);
}
