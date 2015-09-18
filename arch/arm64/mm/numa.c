/*
 * NUMA support, based on the x86 implementation.
 *
 * Copyright (C) 2015 Cavium Inc.
 * Author: Ganapatrao Kulkarni <gkulkarni@cavium.com>
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

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/nodemask.h>
#include <linux/sched.h>
#include <linux/topology.h>
#include <linux/of.h>
#include <linux/mmzone.h>

#include <asm/smp_plat.h>

int __initdata numa_off;
nodemask_t numa_nodes_parsed __initdata;
static int numa_distance_cnt;
static u8 *numa_distance;
static u8 dummy_numa_enabled;

struct pglist_data *node_data[MAX_NUMNODES] __read_mostly;
EXPORT_SYMBOL(node_data);

struct __node_cpu_hwid node_cpu_hwid[NR_CPUS];
static struct numa_meminfo numa_meminfo;

static __init int numa_setup(char *opt)
{
	if (!opt)
		return -EINVAL;
	if (!strncmp(opt, "off", 3)) {
		pr_info("%s\n", "NUMA turned off");
		numa_off = 1;
	}
	return 0;
}
early_param("numa", numa_setup);

cpumask_var_t node_to_cpumask_map[MAX_NUMNODES];
EXPORT_SYMBOL(node_to_cpumask_map);

int cpu_to_node_map[NR_CPUS];
EXPORT_SYMBOL(cpu_to_node_map);

/*
 * Returns a pointer to the bitmask of CPUs on Node 'node'.
 */
const struct cpumask *cpumask_of_node(int node)
{
	if (node >= nr_node_ids) {
		pr_warn("cpumask_of_node(%d): node > nr_node_ids(%d)\n",
			node, nr_node_ids);
		dump_stack();
		return cpu_none_mask;
	}
	if (node_to_cpumask_map[node] == NULL) {
		pr_warn("cpumask_of_node(%d): no node_to_cpumask_map!\n",
			node);
		dump_stack();
		return cpu_online_mask;
	}
	return node_to_cpumask_map[node];
}
EXPORT_SYMBOL(cpumask_of_node);

void numa_clear_node(int cpu)
{
	set_cpu_numa_node(cpu, NUMA_NO_NODE);
}

void map_cpu_to_node(int cpu, int nid)
{
	if (nid < 0) { /* just initialize by zero */
		cpu_to_node_map[cpu] = 0;
		return;
	}

	cpu_to_node_map[cpu] = nid;
	cpumask_set_cpu(cpu, node_to_cpumask_map[nid]);
	set_numa_node(nid);
}

/**
 * build_cpu_to_node_map - setup cpu to node and node to cpumask arrays
 *
 * Build cpu to node mapping and initialize the per node cpu masks using
 * info from the node_cpuid array handed to us by ACPI or DT.
 */
void __init build_cpu_to_node_map(void)
{
	int cpu, i, node;

	for (node = 0; node < MAX_NUMNODES; node++)
		cpumask_clear(node_to_cpumask_map[node]);

	for_each_possible_cpu(cpu) {
		node = NUMA_NO_NODE;
		for_each_possible_cpu(i) {
			if (cpu_logical_map(cpu) == node_cpu_hwid[i].cpu_hwid) {
				node = node_cpu_hwid[i].node_id;
				break;
			}
		}
		map_cpu_to_node(cpu, node);
	}
}
/*
 * Allocate node_to_cpumask_map based on number of available nodes
 * Requires node_possible_map to be valid.
 *
 * Note: cpumask_of_node() is not valid until after this is done.
 * (Use CONFIG_DEBUG_PER_CPU_MAPS to check this.)
 */
void __init setup_node_to_cpumask_map(void)
{
	unsigned int node;

	/* setup nr_node_ids if not done yet */
	if (nr_node_ids == MAX_NUMNODES)
		setup_nr_node_ids();

	/* allocate the map */
	for (node = 0; node < nr_node_ids; node++)
		alloc_bootmem_cpumask_var(&node_to_cpumask_map[node]);

	/* cpumask_of_node() will now work */
	pr_debug("Node to cpumask map for %d nodes\n", nr_node_ids);
}

/*
 *  Set the cpu to node and mem mapping
 */
void numa_store_cpu_info(int cpu)
{
	if (dummy_numa_enabled) {
		/* set to default */
		node_cpu_hwid[cpu].node_id  =  0;
		node_cpu_hwid[cpu].cpu_hwid = cpu_logical_map(cpu);
	}
	map_cpu_to_node(cpu, node_cpu_hwid[cpu].node_id);
}

/**
 * numa_add_memblk_to - Add one numa_memblk to a numa_meminfo
 */

static int __init numa_add_memblk_to(int nid, u64 start, u64 end,
				     struct numa_meminfo *mi)
{
	/* ignore zero length blks */
	if (start == end)
		return 0;

	/* whine about and ignore invalid blks */
	if (start > end || nid < 0 || nid >= MAX_NUMNODES) {
		pr_warn("NUMA: Warning: invalid memblk node %d [mem %#010Lx-%#010Lx]\n",
				nid, start, end - 1);
		return 0;
	}

	if (mi->nr_blks >= NR_NODE_MEMBLKS) {
		pr_err("NUMA: too many memblk ranges\n");
		return -EINVAL;
	}

	pr_info("NUMA: Adding memblock %d [0x%llx - 0x%llx] on node %d\n",
			mi->nr_blks, start, end, nid);
	mi->blk[mi->nr_blks].start = start;
	mi->blk[mi->nr_blks].end = end;
	mi->blk[mi->nr_blks].nid = nid;
	mi->nr_blks++;
	return 0;
}

/**
 * numa_add_memblk - Add one numa_memblk to numa_meminfo
 * @nid: NUMA node ID of the new memblk
 * @start: Start address of the new memblk
 * @end: End address of the new memblk
 *
 * Add a new memblk to the default numa_meminfo.
 *
 * RETURNS:
 * 0 on success, -errno on failure.
 */
#define MAX_PHYS_ADDR	((phys_addr_t)~0)

int __init numa_add_memblk(u32 nid, u64 base, u64 end)
{
	const u64 phys_offset = __pa(PAGE_OFFSET);

	base &= PAGE_MASK;
	end &= PAGE_MASK;

	if (base > MAX_PHYS_ADDR) {
		pr_warn("NUMA: Ignoring memory block 0x%llx - 0x%llx\n",
				base, base + end);
		return -ENOMEM;
	}

	if (base + end > MAX_PHYS_ADDR) {
		pr_info("NUMA: Ignoring memory range 0x%lx - 0x%llx\n",
				ULONG_MAX, base + end);
		end = MAX_PHYS_ADDR - base;
	}

	if (base + end < phys_offset) {
		pr_warn("NUMA: Ignoring memory block 0x%llx - 0x%llx\n",
			   base, base + end);
		return -ENOMEM;
	}
	if (base < phys_offset) {
		pr_info("NUMA: Ignoring memory range 0x%llx - 0x%llx\n",
			   base, phys_offset);
		end -= phys_offset - base;
		base = phys_offset;
	}

	return numa_add_memblk_to(nid, base, base + end, &numa_meminfo);
}
EXPORT_SYMBOL(numa_add_memblk);

/* Initialize NODE_DATA for a node on the local memory */
static void __init setup_node_data(int nid, u64 start, u64 end)
{
	const size_t nd_size = roundup(sizeof(pg_data_t), PAGE_SIZE);
	u64 nd_pa;
	void *nd;
	int tnid;

	start = roundup(start, ZONE_ALIGN);

	pr_info("Initmem setup node %d [mem %#010Lx-%#010Lx]\n",
	       nid, start, end - 1);

	/*
	 * Allocate node data.  Try node-local memory and then any node.
	 */
	nd_pa = memblock_alloc_nid(nd_size, SMP_CACHE_BYTES, nid);
	if (!nd_pa) {
		nd_pa = __memblock_alloc_base(nd_size, SMP_CACHE_BYTES,
					      MEMBLOCK_ALLOC_ACCESSIBLE);
		if (!nd_pa) {
			pr_err("Cannot find %zu bytes in node %d\n",
			       nd_size, nid);
			return;
		}
	}
	nd = __va(nd_pa);

	/* report and initialize */
	pr_info("  NODE_DATA [mem %#010Lx-%#010Lx]\n",
	       nd_pa, nd_pa + nd_size - 1);
	tnid = early_pfn_to_nid(nd_pa >> PAGE_SHIFT);
	if (tnid != nid)
		pr_info("    NODE_DATA(%d) on node %d\n", nid, tnid);

	node_data[nid] = nd;
	memset(NODE_DATA(nid), 0, sizeof(pg_data_t));
	NODE_DATA(nid)->node_id = nid;
	NODE_DATA(nid)->node_start_pfn = start >> PAGE_SHIFT;
	NODE_DATA(nid)->node_spanned_pages = (end - start) >> PAGE_SHIFT;

	node_set_online(nid);
}

/*
 * Set nodes, which have memory in @mi, in *@nodemask.
 */
static void __init numa_nodemask_from_meminfo(nodemask_t *nodemask,
					      const struct numa_meminfo *mi)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mi->blk); i++)
		if (mi->blk[i].start != mi->blk[i].end &&
		    mi->blk[i].nid != NUMA_NO_NODE)
			node_set(mi->blk[i].nid, *nodemask);
}

/*
 * Sanity check to catch more bad NUMA configurations (they are amazingly
 * common).  Make sure the nodes cover all memory.
 */
static bool __init numa_meminfo_cover_memory(const struct numa_meminfo *mi)
{
	u64 numaram, totalram;
	int i;

	numaram = 0;
	for (i = 0; i < mi->nr_blks; i++) {
		u64 s = mi->blk[i].start >> PAGE_SHIFT;
		u64 e = mi->blk[i].end >> PAGE_SHIFT;

		numaram += e - s;
		numaram -= __absent_pages_in_range(mi->blk[i].nid, s, e);
		if ((s64)numaram < 0)
			numaram = 0;
	}

	totalram = max_pfn - absent_pages_in_range(0, max_pfn);

	/* We seem to lose 3 pages somewhere. Allow 1M of slack. */
	if ((s64)(totalram - numaram) >= (1 << (20 - PAGE_SHIFT))) {
		pr_err("NUMA: nodes only cover %lluMB of your %lluMB Total RAM. Not used.\n",
		       (numaram << PAGE_SHIFT) >> 20,
		       (totalram << PAGE_SHIFT) >> 20);
		return false;
	}
	return true;
}

/**
 * numa_reset_distance - Reset NUMA distance table
 *
 * The current table is freed.  The next numa_set_distance() call will
 * create a new one.
 */
void __init numa_reset_distance(void)
{
	size_t size = numa_distance_cnt * numa_distance_cnt *
		sizeof(numa_distance[0]);

	/* numa_distance could be 1LU marking allocation failure, test cnt */
	if (numa_distance_cnt)
		memblock_free(__pa(numa_distance), size);
	numa_distance_cnt = 0;
	numa_distance = NULL;	/* enable table creation */
}

static int __init numa_alloc_distance(void)
{
	nodemask_t nodes_parsed;
	size_t size;
	int i, j, cnt = 0;
	u64 phys;

	/* size the new table and allocate it */
	nodes_parsed = numa_nodes_parsed;
	numa_nodemask_from_meminfo(&nodes_parsed, &numa_meminfo);

	for_each_node_mask(i, nodes_parsed)
		cnt = i;
	cnt++;
	size = cnt * cnt * sizeof(numa_distance[0]);

	phys = memblock_find_in_range(0, PFN_PHYS(max_pfn),
				      size, PAGE_SIZE);
	if (!phys) {
		pr_warn("NUMA: Warning: can't allocate distance table!\n");
		/* don't retry until explicitly reset */
		numa_distance = (void *)1LU;
		return -ENOMEM;
	}
	memblock_reserve(phys, size);

	numa_distance = __va(phys);
	numa_distance_cnt = cnt;

	/* fill with the default distances */
	for (i = 0; i < cnt; i++)
		for (j = 0; j < cnt; j++)
			numa_distance[i * cnt + j] = i == j ?
				LOCAL_DISTANCE : REMOTE_DISTANCE;
	pr_debug("NUMA: Initialized distance table, cnt=%d\n", cnt);

	return 0;
}

/**
 * numa_set_distance - Set NUMA distance from one NUMA to another
 * @from: the 'from' node to set distance
 * @to: the 'to'  node to set distance
 * @distance: NUMA distance
 *
 * Set the distance from node @from to @to to @distance.  If distance table
 * doesn't exist, one which is large enough to accommodate all the currently
 * known nodes will be created.
 *
 * If such table cannot be allocated, a warning is printed and further
 * calls are ignored until the distance table is reset with
 * numa_reset_distance().
 *
 * If @from or @to is higher than the highest known node or lower than zero
 * at the time of table creation or @distance doesn't make sense, the call
 * is ignored.
 * This is to allow simplification of specific NUMA config implementations.
 */
void __init numa_set_distance(int from, int to, int distance)
{
	if (!numa_distance && numa_alloc_distance() < 0)
		return;

	if (from >= numa_distance_cnt || to >= numa_distance_cnt ||
			from < 0 || to < 0) {
		pr_warn_once("NUMA: Warning: node ids are out of bound, from=%d to=%d distance=%d\n",
			    from, to, distance);
		return;
	}

	if ((u8)distance != distance ||
	    (from == to && distance != LOCAL_DISTANCE)) {
		pr_warn_once("NUMA: Warning: invalid distance parameter, from=%d to=%d distance=%d\n",
			     from, to, distance);
		return;
	}

	numa_distance[from * numa_distance_cnt + to] = distance;
}
EXPORT_SYMBOL(numa_set_distance);

int __node_distance(int from, int to)
{
	if (from >= numa_distance_cnt || to >= numa_distance_cnt)
		return from == to ? LOCAL_DISTANCE : REMOTE_DISTANCE;
	return numa_distance[from * numa_distance_cnt + to];
}
EXPORT_SYMBOL(__node_distance);

static int __init numa_register_memblks(struct numa_meminfo *mi)
{
	unsigned long uninitialized_var(pfn_align);
	int i, nid;

	/* Account for nodes with cpus and no memory */
	node_possible_map = numa_nodes_parsed;
	numa_nodemask_from_meminfo(&node_possible_map, mi);
	if (WARN_ON(nodes_empty(node_possible_map)))
		return -EINVAL;

	for (i = 0; i < mi->nr_blks; i++) {
		struct numa_memblk *mb = &mi->blk[i];

		memblock_set_node(mb->start, mb->end - mb->start,
				  &memblock.memory, mb->nid);
	}

	/*
	 * If sections array is gonna be used for pfn -> nid mapping, check
	 * whether its granularity is fine enough.
	 */
#ifdef NODE_NOT_IN_PAGE_FLAGS
	pfn_align = node_map_pfn_alignment();
	if (pfn_align && pfn_align < PAGES_PER_SECTION) {
		pr_warn("Node alignment %lluMB < min %lluMB, rejecting NUMA config\n",
		       PFN_PHYS(pfn_align) >> 20,
		       PFN_PHYS(PAGES_PER_SECTION) >> 20);
		return -EINVAL;
	}
#endif
	if (!numa_meminfo_cover_memory(mi))
		return -EINVAL;

	/* Finally register nodes. */
	for_each_node_mask(nid, node_possible_map) {
		u64 start = PFN_PHYS(max_pfn);
		u64 end = 0;

		for (i = 0; i < mi->nr_blks; i++) {
			if (nid != mi->blk[i].nid)
				continue;
			start = min(mi->blk[i].start, start);
			end = max(mi->blk[i].end, end);
		}

		if (start < end)
			setup_node_data(nid, start, end);
	}

	/* Dump memblock with node info and return. */
	memblock_dump_all();
	return 0;
}

static int __init numa_init(int (*init_func)(void))
{
	int ret, i;

	nodes_clear(numa_nodes_parsed);
	nodes_clear(node_possible_map);
	nodes_clear(node_online_map);

	ret = init_func();
	if (ret < 0)
		return ret;

	ret = numa_register_memblks(&numa_meminfo);
	if (ret < 0)
		return ret;

	for (i = 0; i < nr_cpu_ids; i++)
		numa_clear_node(i);

	setup_node_to_cpumask_map();
	build_cpu_to_node_map();
	return 0;
}

/**
 * dummy_numa_init - Fallback dummy NUMA init
 *
 * Used if there's no underlying NUMA architecture, NUMA initialization
 * fails, or NUMA is disabled on the command line.
 *
 * Must online at least one node and add memory blocks that cover all
 * allowed memory.  This function must not fail.
 */
static int __init dummy_numa_init(void)
{
	pr_info("%s\n", "No NUMA configuration found");
	pr_info("Faking a node at [mem %#018Lx-%#018Lx]\n",
	       0LLU, PFN_PHYS(max_pfn) - 1);
	node_set(0, numa_nodes_parsed);
	numa_add_memblk(0, 0, PFN_PHYS(max_pfn));
	dummy_numa_enabled = 1;

	return 0;
}

/**
 * arm64_numa_init - Initialize NUMA
 *
 * Try each configured NUMA initialization method until one succeeds.  The
 * last fallback is dummy single node config encomapssing whole memory and
 * never fails.
 */
void __init arm64_numa_init(void)
{
	int (*init_func)(void);

	if (IS_ENABLED(CONFIG_ARM64_DT_NUMA))
		init_func = arm64_dt_numa_init;
	else
		init_func = NULL;

	if (!numa_off && init_func) {
		if (!numa_init(init_func))
			return;
	}

	numa_init(dummy_numa_init);
}
