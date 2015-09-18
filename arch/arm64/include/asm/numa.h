#ifndef _ASM_NUMA_H
#define _ASM_NUMA_H

#include <linux/nodemask.h>
#include <asm/topology.h>

#ifdef CONFIG_NUMA

#define NR_NODE_MEMBLKS		(MAX_NUMNODES * 2)
#define ZONE_ALIGN (1UL << (MAX_ORDER + PAGE_SHIFT))

/* currently, arm64 implements flat NUMA topology */
#define parent_node(node)	(node)

/* dummy definitions for pci functions */
#define pcibus_to_node(node)	0
#define cpumask_of_pcibus(bus)	0

struct __node_cpu_hwid {
	u32 node_id;    /* logical node containing this CPU */
	u64 cpu_hwid;   /* MPIDR for this CPU */
};

extern struct __node_cpu_hwid node_cpu_hwid[NR_CPUS];
extern nodemask_t numa_nodes_parsed __initdata;

const struct cpumask *cpumask_of_node(int node);
/* Mappings between node number and cpus on that node. */
extern cpumask_var_t node_to_cpumask_map[MAX_NUMNODES];

void __init arm64_numa_init(void);
int __init numa_add_memblk(u32 nodeid, u64 start, u64 end);
void numa_store_cpu_info(int cpu);
void __init build_cpu_to_node_map(void);
void __init numa_set_distance(int from, int to, int distance);
#if defined(CONFIG_ARM64_DT_NUMA)
void __init dt_numa_set_node_info(u32 cpu, u64 hwid, void *dn);
#else
static inline void dt_numa_set_node_info(u32 cpu, u64 hwid, void *dn) { }
#endif
int __init arm64_dt_numa_init(void);
#else	/* CONFIG_NUMA */
static inline void numa_store_cpu_info(int cpu)		{ }
static inline void arm64_numa_init(void)		{ }
static inline void build_cpu_to_node_map(void) { }
static inline void numa_set_distance(int from, int to, int distance) { }
static inline void dt_numa_set_node_info(u32 cpu, u64 hwid, void *dn) { }
#endif	/* CONFIG_NUMA */
#endif	/* _ASM_NUMA_H */
