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



#ifndef _KBASE_MEM_LOWLEVEL_H
#define _KBASE_MEM_LOWLEVEL_H

#ifndef _KBASE_H_
#error "Don't include this file directly, use mali_kbase.h instead"
#endif

/**
 * Physical page allocator
 */
typedef struct kbase_phy_allocator kbase_phy_allocator;

/**
 * Dedicated physical page allocator
 */
typedef struct kbasep_phy_os_allocator kbasep_phy_os_allocator;
/**
 * OS physical page allocator
 */
typedef struct kbasep_phy_dedicated_allocator kbasep_phy_dedicated_allocator;

/**
 * @brief Initialize a physical page allocator
 *
 * The physical page allocator is responsible for allocating physical memory pages of
 * PAGE_SIZE bytes each. Pages are allocated through the OS or from a reserved
 * memory region.
 *
 * Physical page allocation through the OS
 *
 * If \a mem is 0, upto \a nr_pages of pages may be allocated through the OS for use
 * by a user process. OSs that require allocating CPU virtual address space in order
 * to allocate physical pages must observe that the CPU virtual address space is
 * allocated for the current user process and that the physical allocator must always
 * be used with this same user process.
 *
 * If \a mem is 0, and \a nr_pages is 0, a variable number of pages may be allocated
 * through the OS for use by the kernel (only limited by the available OS memory).
 * Allocated pages may be mapped into the kernel using kmap(). The use case for
 * this type of physical allocator is the allocation of physical pages for MMU page
 * tables. OSs that require allocating CPU virtual address space in order
 * to allocate physical pages must likely manage a list of fixed size virtual
 * address regions against which pages are committed as more pages are allocated.
 *
 * Physical page allocation from a reserved memory region
 *
 * If \a mem is not 0, \a mem specifies the physical start address of a physically
 * contiguous memory region, from which \a nr_pages of pages may be allocated, for
 * use by a user process. The start address is aligned to PAGE_SIZE bytes. The memory
 * region must not be in use by the OS and solely for use by the physical allocator.
 * OSs that require allocating CPU virtual address space in order to allocate physical
 * pages must observe that the CPU virtual address space is allocated for the current
 * user process and that the physical allocator must always be used with this same user
 * process.
 *
 * @param[out] allocator physical allocator to initialize
 * @param[in] mem        Set \a mem to 0 if physical pages should be allocated through the OS,
 *                       otherwise \a mem represents the physical address of a reserved
 *                       memory region from which pages should be allocated. The physical
 *                       address must be PAGE_SIZE aligned.
 * @param[in] nr_pages   maximum number of physical pages that can be allocated.
 *                       If nr_pages > 0, pages are for use in user space.
 *                       If nr_pages is 0, a variable number number of pages can be allocated
 *                       (limited by the available pages from the OS) but the pages are
 *                       for use by the kernel and \a mem must be set to 0
 *                       (to enable allocating physical pages through the OS).
 * @param[in] name		 name of the reserved memory region
 * @return MALI_ERROR_NONE if successful. Any other value indicates failure.
 */
static INLINE mali_error kbase_phy_allocator_init(kbase_phy_allocator * const allocator, phys_addr_t mem, u32 nr_pages, const char *name) CHECK_RESULT;

mali_error kbasep_phy_os_allocator_init(struct kbasep_phy_os_allocator * const allocator, phys_addr_t mem, u32 nr_pages);
mali_error kbasep_phy_dedicated_allocator_init(struct kbasep_phy_dedicated_allocator * const allocator, phys_addr_t mem, u32 nr_pages, const char *name);
mali_error kbasep_phy_dedicated_allocator_request_memory(phys_addr_t mem, u32 nr_pages, const char *name);

/**
 * @brief Terminate a physical page allocator
 *
 * Frees any resources necessary to manage the physical allocator. Any physical pages that
 * were allocated or mapped by the allocator must have been freed and unmapped earlier.
 *
 * Allocating and mapping pages using the terminated allocator is prohibited until the
 * the \a allocator is reinitailized with kbase_phy_allocator_init().
 *
 * @param[in] allocator initialized physical allocator
 */
static INLINE void kbase_phy_allocator_term(kbase_phy_allocator *allocator);

void kbasep_phy_os_allocator_term(kbasep_phy_os_allocator *allocator);
void kbasep_phy_dedicated_allocator_term(kbasep_phy_dedicated_allocator *allocator);
void kbasep_phy_dedicated_allocator_release_memory(phys_addr_t mem, u32 nr_pages);

/**
 * @brief Flags for kbase_phy_allocator_pages_alloc
 */
#define KBASE_PHY_PAGES_FLAG_DEFAULT (0)	/** Default allocation flag */
#define KBASE_PHY_PAGES_FLAG_CLEAR   (1 << 0)	/** Clear the pages after allocation */
#define KBASE_PHY_PAGES_FLAG_POISON  (1 << 1)	/** Fill the memory with a poison value */

#define KBASE_PHY_PAGES_SUPPORTED_FLAGS (KBASE_PHY_PAGES_FLAG_DEFAULT|KBASE_PHY_PAGES_FLAG_CLEAR|KBASE_PHY_PAGES_FLAG_POISON)

#define KBASE_PHY_PAGES_POISON_VALUE  0xFD /** Value to fill the memory with when KBASE_PHY_PAGES_FLAG_POISON is set */

/**
 * @brief Allocate physical pages
 *
 * Allocates \a nr_pages physical pages of PAGE_SIZE each using the physical
 * allocator \a allocator and stores the physical address of each allocated page
 * in the \a pages array.
 *
 * If the physical allocator was initialized to allocate pages for use by a user
 * process, the pages need to be allocated in the same user space context as the
 * physical allocator was initialized in.
 *
 * This function may block and cannot be used from ISR context.
 *
 * @param[in] allocator initialized physical allocator
 * @param[in] nr_pages  number of physical pages to allocate
 * @param[in] flags     allocation flags - refer to KBASE_PHY_PAGES_FLAG_*
 * @param[out] pages    array of \a nr_pages elements storing the physical
 *                      address of an allocated page
 * @return The number of pages successfully allocated,
 * which might be lower than requested, including zero pages.
 */
static INLINE u32 kbase_phy_allocator_pages_alloc(kbase_phy_allocator *allocator, u32 nr_pages, u32 flags, phys_addr_t *pages) CHECK_RESULT;

u32 kbasep_phy_os_pages_alloc(kbasep_phy_os_allocator *allocator, u32 nr_pages, u32 flags, phys_addr_t *pages);
u32 kbasep_phy_dedicated_pages_alloc(kbasep_phy_dedicated_allocator *allocator, u32 nr_pages, u32 flags, phys_addr_t *pages);

/**
 * @brief Free physical pages
 *
 * Frees physical pages previously allocated by kbase_phy_pages_alloc(). The same
 * arguments used for the allocation need to be specified when freeing them.
 *
 * Freeing individual pages of a set of pages allocated by kbase_phy_pages_alloc()
 * is not allowed.
 *
 * If the physical allocator was initialized to allocate pages for use by a user
 * process, the pages need to be freed in the same user space context as the
 * physical allocator was initialized in.
 *
 * The contents of the \a pages array is undefined after kbase_phy_pages_free has
 * freed the pages.
 *
 * @param[in] allocator initialized physical allocator
 * @param[in] nr_pages  number of physical pages to free (as used during the allocation)
 * @param[in] pages     array of \a nr_pages storing the physical address of an
 *                      allocated page (as used during the allocation).
 */
static INLINE void kbase_phy_allocator_pages_free(kbase_phy_allocator *allocator, u32 nr_pages, phys_addr_t *pages);

void kbasep_phy_os_pages_free(kbasep_phy_os_allocator *allocator, u32 nr_pages, phys_addr_t *pages);
void kbasep_phy_dedicated_pages_free(kbasep_phy_dedicated_allocator *allocator, u32 nr_pages, phys_addr_t *pages);

struct kbasep_phy_dedicated_allocator {
	/* lock to protect the free map management */
	struct mutex lock;

	phys_addr_t base;
	u32 num_pages;
	u32 free_pages;

	unsigned long *free_map;
};

struct kbasep_phy_os_allocator {
};

typedef enum kbasep_phy_allocator_type {
	KBASEP_PHY_ALLOCATOR_OS,
	KBASEP_PHY_ALLOCATOR_DEDICATED
} kbasep_phy_allocator_type;

struct kbase_phy_allocator {
	kbasep_phy_allocator_type type;
	union {
		struct kbasep_phy_dedicated_allocator dedicated;
		struct kbasep_phy_os_allocator os;
	} data;
};

static INLINE mali_error kbase_phy_allocator_init(kbase_phy_allocator * const allocator, phys_addr_t mem, u32 nr_pages, const char *name)
{
	KBASE_DEBUG_ASSERT(allocator);
	if (mem == 0) {
		allocator->type = KBASEP_PHY_ALLOCATOR_OS;
		return kbasep_phy_os_allocator_init(&allocator->data.os, mem, nr_pages);
	} else {
		allocator->type = KBASEP_PHY_ALLOCATOR_DEDICATED;
		return kbasep_phy_dedicated_allocator_init(&allocator->data.dedicated, mem, nr_pages, name);
	}
}

static INLINE void kbase_phy_allocator_term(kbase_phy_allocator *allocator)
{
	KBASE_DEBUG_ASSERT(allocator);
	if (allocator->type == KBASEP_PHY_ALLOCATOR_OS)
		kbasep_phy_os_allocator_term(&allocator->data.os);
	else
		kbasep_phy_dedicated_allocator_term(&allocator->data.dedicated);
}

static INLINE u32 kbase_phy_allocator_pages_alloc(kbase_phy_allocator *allocator, u32 nr_pages, u32 flags, phys_addr_t *pages)
{
	KBASE_DEBUG_ASSERT(allocator);
	KBASE_DEBUG_ASSERT(pages);
	if (allocator->type != KBASEP_PHY_ALLOCATOR_OS && allocator->type != KBASEP_PHY_ALLOCATOR_DEDICATED)
		return 0;

	if (allocator->type == KBASEP_PHY_ALLOCATOR_OS)
		return kbasep_phy_os_pages_alloc(&allocator->data.os, nr_pages, flags, pages);
	else
		return kbasep_phy_dedicated_pages_alloc(&allocator->data.dedicated, nr_pages, flags, pages);
}

static INLINE void kbase_phy_allocator_pages_free(kbase_phy_allocator *allocator, u32 nr_pages, phys_addr_t *pages)
{
	KBASE_DEBUG_ASSERT(allocator);
	KBASE_DEBUG_ASSERT(pages);
	if (allocator->type == KBASEP_PHY_ALLOCATOR_OS)
		kbasep_phy_os_pages_free(&allocator->data.os, nr_pages, pages);
	else
		kbasep_phy_dedicated_pages_free(&allocator->data.dedicated, nr_pages, pages);
}

/**
 * A pointer to a cache synchronization function, either kbase_sync_to_cpu()
 * or kbase_sync_to_memory().
 */
typedef void (*kbase_sync_kmem_fn) (phys_addr_t, void *, size_t);

/**
 * @brief Synchronize a memory area for other system components usage
 *
 * Performs the necessary memory coherency operations on a given memory area,
 * such that after the call, changes in memory are correctly seen by other
 * system components. Any change made to memory after that call may not be seen
 * by other system components.
 *
 * In effect:
 * - all CPUs will perform a cache clean operation on their inner & outer data caches
 * - any write buffers are drained (including that of outer cache controllers)
 *
 * This function waits until all operations have completed.
 *
 * The area is restricted to one page or less and must not cross a page boundary.
 * The offset within the page is aligned to cache line size and size is ensured
 * to be a multiple of the cache line size.
 *
 * Both physical and virtual address of the area need to be provided to support OS
 * cache flushing APIs that either use the virtual or the physical address. When
 * called from OS specific code it is allowed to only provide the address that
 * is actually used by the specific OS and leave the other address as 0.
 *
 * @param[in] paddr  physical address
 * @param[in] vaddr  CPU virtual address valid in the current user VM or the kernel VM
 * @param[in] sz     size of the area, <= PAGE_SIZE.
 */
void kbase_sync_to_memory(phys_addr_t paddr, void *vaddr, size_t sz);

/**
 * @brief Synchronize a memory area for CPU usage
 *
 * Performs the necessary memory coherency operations on a given memory area,
 * such that after the call, changes in memory are correctly seen by any CPU.
 * Any change made to this area by any CPU before this call may be lost.
 *
 * In effect:
 * - all CPUs will perform a cache clean & invalidate operation on their inner &
 *   outer data caches.
 *
 * @note Stricly only an invalidate operation is required but by cleaning the cache
 * too we prevent loosing changes made to the memory area due to software bugs. By
 * having these changes cleaned from the cache it allows us to catch the memory
 * area getting corrupted with the help of watch points. In correct operation the
 * clean & invalidate operation would not be more expensive than an invalidate
 * operation. Also note that for security reasons, it is dangerous to expose a
 * cache 'invalidate only' operation to user space.
 *
 * - any read buffers are flushed (including that of outer cache controllers)
 *
 * This function waits until all operations have completed.
 *
 * The area is restricted to one page or less and must not cross a page boundary.
 * The offset within the page is aligned to cache line size and size is ensured
 * to be a multiple of the cache line size.
 *
 * Both physical and virtual address of the area need to be provided to support OS
 * cache flushing APIs that either use the virtual or the physical address. When
 * called from OS specific code it is allowed to only provide the address that
 * is actually used by the specific OS and leave the other address as 0.
 *
 * @param[in] paddr  physical address
 * @param[in] vaddr  CPU virtual address valid in the current user VM or the kernel VM
 * @param[in] sz     size of the area, <= PAGE_SIZE.
 */
void kbase_sync_to_cpu(phys_addr_t paddr, void *vaddr, size_t sz);

#endif				/* _KBASE_LOWLEVEL_H */
