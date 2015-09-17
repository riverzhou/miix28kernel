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

#include <linux/irqchip/arm-gic-v3.h>
#include <kvm/arm_vgic.h>

#include <asm/kvm_emulate.h>
#include <asm/kvm_arm.h>
#include <asm/kvm_mmu.h>

#include "vgic.h"
#include "its-emul.h"

static bool handle_mmio_misc_gits(struct kvm_vcpu *vcpu,
				  struct kvm_exit_mmio *mmio,
				  phys_addr_t offset)
{
	return false;
}

static bool handle_mmio_gits_idregs(struct kvm_vcpu *vcpu,
				    struct kvm_exit_mmio *mmio,
				    phys_addr_t offset)
{
	return false;
}

static bool handle_mmio_gits_cbaser(struct kvm_vcpu *vcpu,
				    struct kvm_exit_mmio *mmio,
				    phys_addr_t offset)
{
	return false;
}

static bool handle_mmio_gits_cwriter(struct kvm_vcpu *vcpu,
				     struct kvm_exit_mmio *mmio,
				     phys_addr_t offset)
{
	return false;
}

static bool handle_mmio_gits_creadr(struct kvm_vcpu *vcpu,
				    struct kvm_exit_mmio *mmio,
				    phys_addr_t offset)
{
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

	spin_lock_init(&its->lock);

	its->enabled = false;

	return -ENXIO;
}
