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



#include <kbase/src/common/mali_kbase.h>

#define CREATE_TRACE_POINTS

#ifdef CONFIG_MALI_TRACE_TIMELINE
#include "mali_timeline.h"

struct kbase_trace_timeline_desc
{
	char *enum_str;
	char *desc;
	char *format;
	char *format_desc;
};

struct kbase_trace_timeline_desc kbase_trace_timeline_desc_table[] =
{
	#define KBASE_TIMELINE_TRACE_CODE(enum_val, desc, format, format_desc) { #enum_val, desc, format, format_desc }
	#include "mali_kbase_trace_timeline_defs.h"
	#undef KBASE_TIMELINE_TRACE_CODE
	,
	{ NULL, NULL, NULL, NULL }
};

ssize_t show_timeline_defs(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i = 0;

	while (NULL != kbase_trace_timeline_desc_table[i].enum_str)
	{
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%s#%s#%s#%s\n", kbase_trace_timeline_desc_table[i].enum_str, kbase_trace_timeline_desc_table[i].desc, kbase_trace_timeline_desc_table[i].format, kbase_trace_timeline_desc_table[i].format_desc);

		i++;
	}

	if (PAGE_SIZE == ret) {
		/* we attempted to write more than a page full - truncate */
		buf[PAGE_SIZE - 2] = '\n';
		buf[PAGE_SIZE - 1] = '\0';
		ret = PAGE_SIZE - 1;
	}
	return ret;
}

#endif /* CONFIG_MALI_TRACE_TIMELINE */
