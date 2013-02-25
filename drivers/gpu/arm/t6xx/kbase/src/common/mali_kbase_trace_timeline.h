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



#if !defined(_KBASE_TRACE_TIMELINE_H)
#define _KBASE_TRACE_TIMELINE_H

#ifdef CONFIG_MALI_TRACE_TIMELINE

#define BASE_JM_SUBMIT_SLOTS        16

typedef struct kbase_trace_timeline
{
	atomic_t jd_atoms_in_flight;
	atomic_t jm_atoms_submitted[BASE_JM_SUBMIT_SLOTS];

	u32 owner_tgid;
} kbase_trace_timeline;

typedef enum
{
	#define KBASE_TIMELINE_TRACE_CODE(enum_val, desc, format, format_desc) enum_val
	#include "mali_kbase_trace_timeline_defs.h"
	#undef KBASE_TIMELINE_TRACE_CODE
} kbase_trace_timeline_code;

extern ssize_t show_timeline_defs(struct device *dev, struct device_attribute *attr, char *buf);

/* mali_timeline.h defines kernel tracepoints used by the KBASE_TIMELINE
   functions.
   Output is timestamped by either sched_clock() (default), local_clock(), or
   cpu_clock(), depending on /sys/kernel/debug/tracing/trace_clock */
#include "mali_timeline.h"

/* Trace number of atoms in flight for kctx (atoms either not completed, or in
   process of being returned to user */
#define KBASE_TIMELINE_ATOMS_IN_FLIGHT(kctx, count)                                 \
	do                                                                          \
	{                                                                           \
		struct timespec ts;                                                 \
		getnstimeofday(&ts);                                                \
		trace_mali_timeline_atoms_in_flight(ts.tv_sec, ts.tv_nsec,          \
                                                    (int)kctx->timeline.owner_tgid, \
                                                    count);                         \
	} while (0)

/* Trace number of atoms submitted to job slot js */
#define KBASE_TIMELINE_ATOMS_SUBMITTED(kctx, js, count)                             \
	do                                                                          \
	{                                                                           \
		struct timespec ts;                                                 \
		getnstimeofday(&ts);                                                \
		trace_mali_timeline_atoms_submitted(ts.tv_sec, ts.tv_nsec,          \
                                                    (int)kctx->timeline.owner_tgid, \
                                                    js, count);                     \
	} while (0)

/* Trace state of overall GPU power */
#define KBASE_TIMELINE_GPU_POWER(kbdev, active)                                    \
	do                                                                         \
	{                                                                          \
		struct timespec ts;                                                \
		getnstimeofday(&ts);                                               \
		trace_mali_timeline_gpu_power_active(ts.tv_sec, ts.tv_nsec,        \
		                                     SW_SET_GPU_POWER_ACTIVE, active); \
	} while (0)

/* Trace state of tiler power */
#define KBASE_TIMELINE_POWER_TILER(kbdev, bitmap)                               \
	do                                                                      \
	{                                                                       \
		struct timespec ts;                                             \
		getnstimeofday(&ts);                                            \
		trace_mali_timeline_gpu_power_active(ts.tv_sec, ts.tv_nsec,     \
		                                     SW_SET_GPU_POWER_TILER_ACTIVE, \
		                                     hweight64(bitmap));        \
	} while (0)

/* Trace number of shaders currently powered */
#define KBASE_TIMELINE_POWER_SHADER(kbdev, bitmap)                               \
	do                                                                       \
	{                                                                        \
		struct timespec ts;                                              \
		getnstimeofday(&ts);                                             \
		trace_mali_timeline_gpu_power_active(ts.tv_sec, ts.tv_nsec,      \
		                                     SW_SET_GPU_POWER_SHADER_ACTIVE, \
		                                     hweight64(bitmap));         \
	} while (0)

#else

#define KBASE_TIMELINE_ATOMS_IN_FLIGHT(kctx, count) CSTD_NOP()

#define KBASE_TIMELINE_ATOMS_SUBMITTED(kctx, js, count) CSTD_NOP()

#define KBASE_TIMELINE_GPU_POWER(kbdev, active) CSTD_NOP()

#define KBASE_TIMELINE_POWER_TILER(kbdev, bitmap) CSTD_NOP()

#define KBASE_TIMELINE_POWER_SHADER(kbdev, bitmap) CSTD_NOP()

#endif				/* CONFIG_MALI_TRACE_TIMELINE */

#endif				/* _KBASE_TRACE_TIMELINE_H */
