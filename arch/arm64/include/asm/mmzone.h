#ifndef __ASM_ARM64_MMZONE_H_
#define __ASM_ARM64_MMZONE_H_

#ifdef CONFIG_NUMA

#include <linux/mmdebug.h>
#include <asm/smp.h>
#include <linux/types.h>
#include <asm/numa.h>

extern struct pglist_data *node_data[];

#define NODE_DATA(nid)		(node_data[nid])


struct numa_memblk {
	u64			start;
	u64			end;
	int			nid;
};

struct numa_meminfo {
	int			nr_blks;
	struct numa_memblk	blk[NR_NODE_MEMBLKS];
};

void __init numa_remove_memblk_from(int idx, struct numa_meminfo *mi);
int __init numa_cleanup_meminfo(struct numa_meminfo *mi);
void __init numa_reset_distance(void);

#endif /* CONFIG_NUMA */
#endif /* __ASM_ARM64_MMZONE_H_ */
