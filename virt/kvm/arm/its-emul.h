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

void vgic_enable_lpis(struct kvm_vcpu *vcpu);
int vits_init(struct kvm *kvm);
void vits_destroy(struct kvm *kvm);

bool vits_queue_lpis(struct kvm_vcpu *vcpu);
void vits_unqueue_lpi(struct kvm_vcpu *vcpu, int irq);

#endif
