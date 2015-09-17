/*
 * GICv3 ITS emulation definitions
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

#ifndef __KVM_ITS_EMUL_H__
#define __KVM_ITS_EMUL_H__

#include <linux/kvm.h>
#include <linux/kvm_host.h>

#include <asm/kvm_emulate.h>
#include <asm/kvm_arm.h>
#include <asm/kvm_mmu.h>

#include "vgic.h"

#define INTERRUPT_ID_BITS_ITS 16
#define VITS_NR_LPIS (1U << INTERRUPT_ID_BITS_ITS)

void vgic_enable_lpis(struct kvm_vcpu *vcpu);
int vits_init(struct kvm *kvm);
void vits_destroy(struct kvm *kvm);

int vits_inject_msi(struct kvm *kvm, struct kvm_msi *msi);

bool vits_queue_lpis(struct kvm_vcpu *vcpu);
void vits_unqueue_lpi(struct kvm_vcpu *vcpu, int irq);

#define E_ITS_MOVI_UNMAPPED_INTERRUPT		0x010107
#define E_ITS_MOVI_UNMAPPED_COLLECTION		0x010109
#define E_ITS_CLEAR_UNMAPPED_INTERRUPT		0x010507
#define E_ITS_MAPC_PROCNUM_OOR			0x010902
#define E_ITS_MAPTI_UNMAPPED_DEVICE		0x010a04
#define E_ITS_MAPTI_PHYSICALID_OOR		0x010a06
#define E_ITS_INV_UNMAPPED_INTERRUPT		0x010c07
#define E_ITS_INVALL_UNMAPPED_COLLECTION	0x010d09
#define E_ITS_MOVALL_PROCNUM_OOR		0x010e01
#define E_ITS_DISCARD_UNMAPPED_INTERRUPT	0x010f07

#endif
