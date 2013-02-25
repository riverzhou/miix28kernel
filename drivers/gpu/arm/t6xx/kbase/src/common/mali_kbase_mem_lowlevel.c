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

#include <linux/io.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/dma-mapping.h>
#include <linux/mutex.h>

mali_error kbasep_phy_dedicated_allocator_init(kbasep_phy_dedicated_allocator * const allocator, phys_addr_t mem, u32 nr_pages, const char *name)
{
	KBASE_DEBUG_ASSERT(allocator);
	KBASE_DEBUG_ASSERT(nr_pages > 0);
	/* Assert if not page aligned */
	KBASE_DEBUG_ASSERT(0 == (mem & (PAGE_SIZE - 1)));

	if (!mem) {
		/* no address to manage specified */
		return MALI_ERROR_FUNCTION_FAILED;
	} else {
		u32 i;

		/* try to obtain dedicated memory */
		if (NULL == request_mem_region(mem, nr_pages << PAGE_SHIFT, name)) {
			/* requested memory not available */
			return MALI_ERROR_FUNCTION_FAILED;
		}

		allocator->base = mem;
		allocator->num_pages = nr_pages;
		allocator->free_pages = allocator->num_pages;

		mutex_init(&allocator->lock);

		allocator->free_map = kzalloc(sizeof(unsigned long) * ((nr_pages + BITS_PER_LONG - 1) / BITS_PER_LONG), GFP_KERNEL);
		if (NULL == allocator->free_map) {
			release_mem_region(mem, nr_pages << PAGE_SHIFT);
			return MALI_ERROR_OUT_OF_MEMORY;
		}

		/* correct for nr_pages not being a multiple of BITS_PER_LONG */
		for (i = nr_pages; i < ((nr_pages + BITS_PER_LONG - 1) & ~(BITS_PER_LONG - 1)); i++)
			__set_bit(i, allocator->free_map);

		return MALI_ERROR_NONE;
	}
}

void kbasep_phy_dedicated_allocator_term(kbasep_phy_dedicated_allocator *allocator)
{
	KBASE_DEBUG_ASSERT(allocator);
	KBASE_DEBUG_ASSERT(allocator->free_map);
	release_mem_region(allocator->base, allocator->num_pages << PAGE_SHIFT);
	kfree(allocator->free_map);
}

u32 kbasep_phy_dedicated_pages_alloc(kbasep_phy_dedicated_allocator *allocator, u32 nr_pages, u32 flags, phys_addr_t *pages)
{
	u32 pages_allocated;

	KBASE_DEBUG_ASSERT(pages);
	KBASE_DEBUG_ASSERT(allocator);
	KBASE_DEBUG_ASSERT(allocator->free_map);
	/* Assert for unknown flags */
	KBASE_DEBUG_ASSERT((~KBASE_PHY_PAGES_SUPPORTED_FLAGS & flags) == 0);

	mutex_lock(&allocator->lock);

	for (pages_allocated = 0; pages_allocated < MIN(nr_pages, allocator->free_pages); pages_allocated++) {
		u32 pfn;
		void *mapping;

		pfn = find_first_zero_bit(allocator->free_map, allocator->num_pages);
		/* As the free_pages test passed ffz should never fail */
		KBASE_DEBUG_ASSERT(pfn != allocator->num_pages);

		/* mark as allocated */
		__set_bit(pfn, allocator->free_map);

		/* find phys addr of the page */
		pages[pages_allocated] = allocator->base + (pfn << PAGE_SHIFT);

		/* if requested fill the page with zeros or poison value */
		if (flags & (KBASE_PHY_PAGES_FLAG_CLEAR | KBASE_PHY_PAGES_FLAG_POISON)) {
			int value = 0;

			if (flags & KBASE_PHY_PAGES_FLAG_POISON)
				value = KBASE_PHY_PAGES_POISON_VALUE;

			mapping = ioremap_wc(pages[pages_allocated], SZ_4K);

			if (NULL == mapping) {
				/* roll back */
				for (pages_allocated++; pages_allocated > 0; pages_allocated--) {
					pfn = (pages[pages_allocated - 1] - allocator->base) >> PAGE_SHIFT;
					__clear_bit(pfn, allocator->free_map);
				}
				break;
			}

			memset(mapping, value, PAGE_SIZE);

			kbase_sync_to_memory(pages[pages_allocated], mapping, PAGE_SIZE);
			iounmap(mapping);
		}
	}
	allocator->free_pages -= pages_allocated;
	mutex_unlock(&allocator->lock);

	return pages_allocated;
}

void kbasep_phy_dedicated_pages_free(kbasep_phy_dedicated_allocator *allocator, u32 nr_pages, phys_addr_t *pages)
{
	u32 i;

	KBASE_DEBUG_ASSERT(pages);
	KBASE_DEBUG_ASSERT(allocator);
	KBASE_DEBUG_ASSERT(allocator->free_map);

	mutex_lock(&allocator->lock);

	for (i = 0; i < nr_pages; i++) {
		if (0 != pages[i]) {
			u32 pfn;

			KBASE_DEBUG_ASSERT(pages[i] >= allocator->base);
			KBASE_DEBUG_ASSERT(pages[i] < allocator->base + (allocator->num_pages << PAGE_SHIFT));

			pfn = (pages[i] - allocator->base) >> PAGE_SHIFT;
			__clear_bit(pfn, allocator->free_map);

			allocator->free_pages++;

			pages[i] = 0;
		}
	}

	mutex_unlock(&allocator->lock);
}

mali_error kbasep_phy_os_allocator_init(kbasep_phy_os_allocator * const allocator, phys_addr_t mem, u32 nr_pages)
{
	KBASE_DEBUG_ASSERT(NULL != allocator);

	return MALI_ERROR_NONE;
}

void kbasep_phy_os_allocator_term(kbasep_phy_os_allocator *allocator)
{
	KBASE_DEBUG_ASSERT(NULL != allocator);
	/* Nothing needed */
}

u32 kbasep_phy_os_pages_alloc(kbasep_phy_os_allocator *allocator, u32 nr_pages, u32 flags, phys_addr_t *pages)
{
	int i;
	/* Assert for unknown flags */
	KBASE_DEBUG_ASSERT((~KBASE_PHY_PAGES_SUPPORTED_FLAGS & flags) == 0);
	KBASE_DEBUG_ASSERT(NULL != allocator);

	for (i = 0; i < nr_pages; i++) {
		struct page *p;
		void *mp;

		p = alloc_page(GFP_HIGHUSER);
		if (NULL == p)
			break;

		if (flags & (KBASE_PHY_PAGES_FLAG_CLEAR | KBASE_PHY_PAGES_FLAG_POISON)) {
			int value = 0;

			if (flags & KBASE_PHY_PAGES_FLAG_POISON)
				value = KBASE_PHY_PAGES_POISON_VALUE;

			mp = kmap(p);
			if (NULL == mp) {
				__free_page(p);
				break;
			}

			memset(mp, value, PAGE_SIZE);	/* instead of __GFP_ZERO, so we can do cache maintenance */
			kbase_sync_to_memory(PFN_PHYS(page_to_pfn(p)), mp, PAGE_SIZE);
			kunmap(p);
		}

		pages[i] = PFN_PHYS(page_to_pfn(p));
	}

	return i;
}

void kbasep_phy_os_pages_free(kbasep_phy_os_allocator *allocator, u32 nr_pages, phys_addr_t *pages)
{
	int i;

	KBASE_DEBUG_ASSERT(NULL != allocator);

	for (i = 0; i < nr_pages; i++) {
		if (0 != pages[i]) {
			__free_page(pfn_to_page(PFN_DOWN(pages[i])));
			pages[i] = (phys_addr_t) 0;
		}
	}
}

void kbase_sync_to_memory(phys_addr_t paddr, void *vaddr, size_t sz)
{
#ifdef CONFIG_ARM
	dmac_flush_range(vaddr, vaddr + sz);
	outer_flush_range(paddr, paddr + sz);
#elif defined(CONFIG_X86)
	struct scatterlist scl = { 0, };
	sg_set_page(&scl, pfn_to_page(PFN_DOWN(paddr)), sz, paddr & (PAGE_SIZE - 1));
	dma_sync_sg_for_cpu(NULL, &scl, 1, DMA_TO_DEVICE);
	mb();			/* for outer_sync (if needed) */
#else
#error Implement cache maintenance for your architecture here
#endif
}

void kbase_sync_to_cpu(phys_addr_t paddr, void *vaddr, size_t sz)
{
#ifdef CONFIG_ARM
	dmac_flush_range(vaddr, vaddr + sz);
	outer_flush_range(paddr, paddr + sz);
#elif defined(CONFIG_X86)
	struct scatterlist scl = { 0, };
	sg_set_page(&scl, pfn_to_page(PFN_DOWN(paddr)), sz, paddr & (PAGE_SIZE - 1));
	dma_sync_sg_for_cpu(NULL, &scl, 1, DMA_FROM_DEVICE);
#else
#error Implement cache maintenance for your architecture here
#endif
}
