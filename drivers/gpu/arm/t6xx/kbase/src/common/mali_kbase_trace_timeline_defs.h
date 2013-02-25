/*
 *
 * (C) COPYRIGHT 2012 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */



/* ***** IMPORTANT: THIS IS NOT A NORMAL HEADER FILE         *****
 * *****            DO NOT INCLUDE DIRECTLY                  *****
 * *****            THE LACK OF HEADER GUARDS IS INTENTIONAL ***** */

	KBASE_TIMELINE_TRACE_CODE(CTX_SET_NR_ATOMS_IN_FLIGHT,     "CTX: Atoms in flight",    "%d,%d",    "instance,value"),
	KBASE_TIMELINE_TRACE_CODE(SW_SET_GPU_SLOT_ACTIVE,         "SW: GPU slot active",     "%d,%d,%d", "tgid,instance,value"),
	KBASE_TIMELINE_TRACE_CODE(SW_SET_GPU_POWER_ACTIVE,        "SW: GPU power active",    "%d,%d",    "instance,value"),
	KBASE_TIMELINE_TRACE_CODE(SW_SET_GPU_POWER_TILER_ACTIVE,  "SW: GPU tiler powered",   "%d,%d",    "instance,value"),
	KBASE_TIMELINE_TRACE_CODE(SW_SET_GPU_POWER_SHADER_ACTIVE, "SW: GPU shaders powered", "%d,%d",    "instance,value")
