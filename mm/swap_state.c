/*
 *  linux/mm/swap_state.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *  Swap reorganised 29.12.95, Stephen Tweedie
 *
 *  Rewritten to use page cache, (C) 1998 Stephen Tweedie
 */
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/init.h>
#include <linux/pagemap.h>
#include <linux/backing-dev.h>
#include <linux/blkdev.h>
#include <linux/pagevec.h>
#include <linux/migrate.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/swap_slots.h>

#include <asm/pgtable.h>
#include "internal.h"

/*
 * swapper_space is a fiction, retained to simplify the path through
 * vmscan's shrink_page_list.
 */
static const struct address_space_operations swap_aops = {
	.writepage	= swap_writepage,
	.set_page_dirty	= swap_set_page_dirty,
#ifdef CONFIG_MIGRATION
	.migratepage	= migrate_page,
#endif
};

struct address_space *swapper_spaces[MAX_SWAPFILES];
static unsigned int nr_swapper_spaces[MAX_SWAPFILES];

#define INC_CACHE_INFO(x)	do { swap_cache_info.x++; } while (0)

static struct {
	unsigned long add_total;
	unsigned long del_total;
	unsigned long find_success;
	unsigned long find_total;
} swap_cache_info;

unsigned long total_swapcache_pages(void)
{
	unsigned int i, j, nr;
	unsigned long ret = 0;
	struct address_space *spaces;

	rcu_read_lock();
	for (i = 0; i < MAX_SWAPFILES; i++) {
		/*
		 * The corresponding entries in nr_swapper_spaces and
		 * swapper_spaces will be reused only after at least
		 * one grace period.  So it is impossible for them
		 * belongs to different usage.
		 */
		nr = nr_swapper_spaces[i];
		spaces = rcu_dereference(swapper_spaces[i]);
		if (!nr || !spaces)
			continue;
		for (j = 0; j < nr; j++)
			ret += spaces[j].nrpages;
	}
	rcu_read_unlock();
	return ret;
}

static atomic_t swapin_readahead_hits = ATOMIC_INIT(4);

void show_swap_cache_info(void)
{
	printk("%lu pages in swap cache\n", total_swapcache_pages());
	printk("Swap cache stats: add %lu, delete %lu, find %lu/%lu\n",
		swap_cache_info.add_total, swap_cache_info.del_total,
		swap_cache_info.find_success, swap_cache_info.find_total);
	printk("Free swap  = %ldkB\n",
		get_nr_swap_pages() << (PAGE_SHIFT - 10));
	printk("Total swap = %lukB\n", total_swap_pages << (PAGE_SHIFT - 10));
}

/*
 * __add_to_swap_cache resembles add_to_page_cache_locked on swapper_space,
 * but sets SwapCache flag and private instead of mapping and index.
 */
int __add_to_swap_cache(struct page *page, swp_entry_t entry)
{
	int error;
	struct address_space *address_space;

	VM_BUG_ON_PAGE(!PageLocked(page), page);
	VM_BUG_ON_PAGE(PageSwapCache(page), page);
	VM_BUG_ON_PAGE(!PageSwapBacked(page), page);

	get_page(page);
	SetPageSwapCache(page);
	set_page_private(page, entry.val);

	address_space = swap_address_space(entry);
	spin_lock_irq(&address_space->tree_lock);
	error = radix_tree_insert(&address_space->page_tree,
				  swp_offset(entry), page);
	if (likely(!error)) {
		address_space->nrpages++;
		__inc_node_page_state(page, NR_FILE_PAGES);
		INC_CACHE_INFO(add_total);
	}
	spin_unlock_irq(&address_space->tree_lock);

	if (unlikely(error)) {
		/*
		 * Only the context which have set SWAP_HAS_CACHE flag
		 * would call add_to_swap_cache().
		 * So add_to_swap_cache() doesn't returns -EEXIST.
		 */
		VM_BUG_ON(error == -EEXIST);
		set_page_private(page, 0UL);
		ClearPageSwapCache(page);
		put_page(page);
	}

	return error;
}


int add_to_swap_cache(struct page *page, swp_entry_t entry, gfp_t gfp_mask)
{
	int error;

	error = radix_tree_maybe_preload(gfp_mask);
	if (!error) {
		error = __add_to_swap_cache(page, entry);
		radix_tree_preload_end();
	}
	return error;
}

/*
 * This must be called only on pages that have
 * been verified to be in the swap cache.
 */
void __delete_from_swap_cache(struct page *page)
{
	swp_entry_t entry;
	struct address_space *address_space;

	VM_BUG_ON_PAGE(!PageLocked(page), page);
	VM_BUG_ON_PAGE(!PageSwapCache(page), page);
	VM_BUG_ON_PAGE(PageWriteback(page), page);

	entry.val = page_private(page);
	address_space = swap_address_space(entry);
	radix_tree_delete(&address_space->page_tree, swp_offset(entry));
	set_page_private(page, 0);
	ClearPageSwapCache(page);
	address_space->nrpages--;
	__dec_node_page_state(page, NR_FILE_PAGES);
	INC_CACHE_INFO(del_total);
}

/**
 * add_to_swap - allocate swap space for a page
 * @page: page we want to move to swap
 *
 * Allocate swap space for the page and add the page to the
 * swap cache.  Caller needs to hold the page lock. 
 */
int add_to_swap(struct page *page, struct list_head *list)
{
	swp_entry_t entry;
	int err;

	VM_BUG_ON_PAGE(!PageLocked(page), page);
	VM_BUG_ON_PAGE(!PageUptodate(page), page);

#ifdef CONFIG_HSWAP
	if (!current_is_kswapd())
		entry = get_lowest_prio_swap_page();
	else
#endif
	entry = get_swap_page();

	if (!entry.val)
		return 0;

	if (mem_cgroup_try_charge_swap(page, entry)) {
		swapcache_free(entry);
		return 0;
	}

	if (unlikely(PageTransHuge(page)))
		if (unlikely(split_huge_page_to_list(page, list))) {
			swapcache_free(entry);
			return 0;
		}

	/*
	 * Radix-tree node allocations from PF_MEMALLOC contexts could
	 * completely exhaust the page allocator. __GFP_NOMEMALLOC
	 * stops emergency reserves from being allocated.
	 *
	 * TODO: this could cause a theoretical memory reclaim
	 * deadlock in the swap out path.
	 */
	/*
	 * Add it to the swap cache.
	 */
	err = add_to_swap_cache(page, entry,
			__GFP_HIGH|__GFP_NOMEMALLOC|__GFP_NOWARN);

	if (!err) {
		return 1;
	} else {	/* -ENOMEM radix-tree allocation failure */
		/*
		 * add_to_swap_cache() doesn't return -EEXIST, so we can safely
		 * clear SWAP_HAS_CACHE flag.
		 */
		swapcache_free(entry);
		return 0;
	}
}

/*
 * This must be called only on pages that have
 * been verified to be in the swap cache and locked.
 * It will never put the page into the free list,
 * the caller has a reference on the page.
 */
void delete_from_swap_cache(struct page *page)
{
	swp_entry_t entry;
	struct address_space *address_space;

	entry.val = page_private(page);

	address_space = swap_address_space(entry);
	spin_lock_irq(&address_space->tree_lock);
	__delete_from_swap_cache(page);
	spin_unlock_irq(&address_space->tree_lock);

	swapcache_free(entry);
	put_page(page);
}

/* 
 * If we are the only user, then try to free up the swap cache. 
 * 
 * Its ok to check for PageSwapCache without the page lock
 * here because we are going to recheck again inside
 * try_to_free_swap() _with_ the lock.
 * 					- Marcelo
 */
static inline void free_swap_cache(struct page *page)
{
	if (PageSwapCache(page) && !page_mapped(page) && trylock_page(page)) {
		try_to_free_swap(page);
		unlock_page(page);
	}
}

/* 
 * Perform a free_page(), also freeing any swap cache associated with
 * this page if it is the last user of the page.
 */
void free_page_and_swap_cache(struct page *page)
{
	free_swap_cache(page);
	if (!is_huge_zero_page(page))
		put_page(page);
}

/*
 * Passed an array of pages, drop them all from swapcache and then release
 * them.  They are removed from the LRU and freed if this is their last use.
 */
void free_pages_and_swap_cache(struct page **pages, int nr)
{
	struct page **pagep = pages;
	int i;

	lru_add_drain();
	for (i = 0; i < nr; i++)
		free_swap_cache(pagep[i]);
	release_pages(pagep, nr, false);
}

/*
 * Lookup a swap entry in the swap cache. A found page will be returned
 * unlocked and with its refcount incremented - we rely on the kernel
 * lock getting page table operations atomic even if we drop the page
 * lock before returning.
 */
struct page * lookup_swap_cache(swp_entry_t entry)
{
	struct page *page;

	page = find_get_page(swap_address_space(entry), swp_offset(entry));

	if (page) {
		INC_CACHE_INFO(find_success);
		if (TestClearPageReadahead(page))
			atomic_inc(&swapin_readahead_hits);
	}

	INC_CACHE_INFO(find_total);
	return page;
}

struct page *__read_swap_cache_async(swp_entry_t entry, gfp_t gfp_mask,
			struct vm_area_struct *vma, unsigned long addr,
			bool *new_page_allocated)
{
	struct page *found_page, *new_page = NULL;
	struct address_space *swapper_space = swap_address_space(entry);
	int err;
	*new_page_allocated = false;

	do {
		/*
		 * First check the swap cache.  Since this is normally
		 * called after lookup_swap_cache() failed, re-calling
		 * that would confuse statistics.
		 */
		found_page = find_get_page(swapper_space, swp_offset(entry));
		if (found_page)
			break;

		/*
		 * Just skip read ahead for unused swap slot.
		 * During swap_off when swap_slot_cache is disabled,
		 * we have to handle the race between putting
		 * swap entry in swap cache and marking swap slot
		 * as SWAP_HAS_CACHE.  That's done in later part of code or
		 * else swap_off will be aborted if we return NULL.
		 */
		if (!__swp_swapcount(entry) && swap_slot_cache_enabled)
			break;

		/*
		 * Get a new page to read into from swap.
		 */
		if (!new_page) {
			new_page = alloc_page_vma(gfp_mask, vma, addr);
			if (!new_page)
				break;		/* Out of memory */
		}

		/*
		 * call radix_tree_preload() while we can wait.
		 */
		err = radix_tree_maybe_preload(gfp_mask & GFP_RECLAIM_MASK);
		if (err)
			break;

		/*
		 * Swap entry may have been freed since our caller observed it.
		 */
		err = swapcache_prepare(entry);
		if (err == -EEXIST) {
			radix_tree_preload_end();
			/*
			 * We might race against get_swap_page() and stumble
			 * across a SWAP_HAS_CACHE swap_map entry whose page
			 * has not been brought into the swapcache yet, while
			 * the other end is scheduled away waiting on discard
			 * I/O completion at scan_swap_map().
			 *
			 * In order to avoid turning this transitory state
			 * into a permanent loop around this -EEXIST case
			 * if !CONFIG_PREEMPT and the I/O completion happens
			 * to be waiting on the CPU waitqueue where we are now
			 * busy looping, we just conditionally invoke the
			 * scheduler here, if there are some more important
			 * tasks to run.
			 *
			 * cond_resched() may not work if the process is RT.
			 * We need a usleep_range() give up CPU to another task.
			 */
			usleep_range(500, 1000);
			continue;
		}
		if (err) {		/* swp entry is obsolete ? */
			radix_tree_preload_end();
			break;
		}

		/* May fail (-ENOMEM) if radix-tree node allocation failed. */
		__SetPageLocked(new_page);
		__SetPageSwapBacked(new_page);
		err = __add_to_swap_cache(new_page, entry);
		if (likely(!err)) {
			radix_tree_preload_end();
			/*
			 * Initiate read into locked page and return.
			 */
			SetPageWorkingset(new_page);
			lru_cache_add_anon(new_page);
			*new_page_allocated = true;
			return new_page;
		}
		radix_tree_preload_end();
		__ClearPageLocked(new_page);
		/*
		 * add_to_swap_cache() doesn't return -EEXIST, so we can safely
		 * clear SWAP_HAS_CACHE flag.
		 */
		swapcache_free(entry);
	} while (err != -ENOMEM);

	if (new_page)
		put_page(new_page);
	return found_page;
}

/*
 * Locate a page of swap in physical memory, reserving swap cache space
 * and reading the disk if it is not already cached.
 * A failure return means that either the page allocation failed or that
 * the swap entry is no longer in use.
 */
struct page *read_swap_cache_async(swp_entry_t entry, gfp_t gfp_mask,
			struct vm_area_struct *vma, unsigned long addr)
{
	bool page_was_allocated;
	struct page *retpage = __read_swap_cache_async(entry, gfp_mask,
			vma, addr, &page_was_allocated);

	if (page_was_allocated)
		swap_readpage(retpage);

	return retpage;
}

static unsigned long swapin_nr_pages(unsigned long offset)
{
	static unsigned long prev_offset;
	unsigned int pages, max_pages, last_ra;
	static atomic_t last_readahead_pages;

	max_pages = 1 << READ_ONCE(page_cluster);
	if (max_pages <= 1)
		return 1;

	/*
	 * This heuristic has been found to work well on both sequential and
	 * random loads, swapping to hard disk or to SSD: please don't ask
	 * what the "+ 2" means, it just happens to work well, that's all.
	 */
	pages = atomic_xchg(&swapin_readahead_hits, 0) + 2;
	if (pages == 2) {
		/*
		 * We can have no readahead hits to judge by: but must not get
		 * stuck here forever, so check for an adjacent offset instead
		 * (and don't even bother to check whether swap type is same).
		 */
		if (offset != prev_offset + 1 && offset != prev_offset - 1)
			pages = 1;
		prev_offset = offset;
	} else {
		unsigned int roundup = 4;
		while (roundup < pages)
			roundup <<= 1;
		pages = roundup;
	}

	if (pages > max_pages)
		pages = max_pages;

	/* Don't shrink readahead too fast */
	last_ra = atomic_read(&last_readahead_pages) / 2;
	if (pages < last_ra)
		pages = last_ra;
	atomic_set(&last_readahead_pages, pages);

	return pages;
}

/**
 * swapin_readahead - swap in pages in hope we need them soon
 * @entry: swap entry of this memory
 * @gfp_mask: memory allocation flags
 * @vma: user vma this address belongs to
 * @addr: target address for mempolicy
 *
 * Returns the struct page for entry and addr, after queueing swapin.
 *
 * Primitive swap readahead code. We simply read an aligned block of
 * (1 << page_cluster) entries in the swap area. This method is chosen
 * because it doesn't cost us any seek time.  We also make sure to queue
 * the 'original' request together with the readahead ones...
 *
 * This has been extended to use the NUMA policies from the mm triggering
 * the readahead.
 *
 * Caller must hold down_read on the vma->vm_mm if vma is not NULL.
 * This is needed to ensure the VMA will not be freed in our back. In the case
 * of the speculative page fault handler, this cannot happen, even if we don't
 * hold the mmap_sem. Callees are assumed to take care of reading VMA's fields
 * using READ_ONCE() to read consistent values.
 */
struct page *swapin_readahead(swp_entry_t entry, gfp_t gfp_mask,
			struct vm_area_struct *vma, unsigned long addr)
{
	struct page *page;
	unsigned long entry_offset = swp_offset(entry);
	unsigned long offset = entry_offset;
	unsigned long start_offset, end_offset;
	unsigned long mask = is_swap_fast(entry) ? 0 :
				(1UL << page_cluster) - 1;
	struct blk_plug plug;

	mask = is_swap_fast(entry) ? 0 : swapin_nr_pages(offset) - 1;
	if (!mask)
		goto skip;

	/* Read a page_cluster sized and aligned cluster around offset. */
	start_offset = offset & ~mask;
	end_offset = offset | mask;
	if (!start_offset)	/* First page is swap header. */
		start_offset++;

	blk_start_plug(&plug);
	for (offset = start_offset; offset <= end_offset ; offset++) {
		/* Ok, do the async read-ahead now */
		page = read_swap_cache_async(swp_entry(swp_type(entry), offset),
						gfp_mask, vma, addr);
		if (!page)
			continue;
		if (offset != entry_offset)
			SetPageReadahead(page);
		put_page(page);
	}
	blk_finish_plug(&plug);

	lru_add_drain();	/* Push any new pages onto the LRU now */
skip:
	return read_swap_cache_async(entry, gfp_mask, vma, addr);
}

int init_swap_address_space(unsigned int type, unsigned long nr_pages)
{
	struct address_space *spaces, *space;
	unsigned int i, nr;

	nr = DIV_ROUND_UP(nr_pages, SWAP_ADDRESS_SPACE_PAGES);
	spaces = kvzalloc(sizeof(struct address_space) * nr, GFP_KERNEL);
	if (!spaces)
		return -ENOMEM;
	for (i = 0; i < nr; i++) {
		space = spaces + i;
		INIT_RADIX_TREE(&space->page_tree, GFP_ATOMIC|__GFP_NOWARN);
		atomic_set(&space->i_mmap_writable, 0);
		space->a_ops = &swap_aops;
		/* swap cache doesn't use writeback related tags */
		mapping_set_no_writeback_tags(space);
		spin_lock_init(&space->tree_lock);
	}
	nr_swapper_spaces[type] = nr;
	rcu_assign_pointer(swapper_spaces[type], spaces);

	return 0;
}

void exit_swap_address_space(unsigned int type)
{
	struct address_space *spaces;

	spaces = swapper_spaces[type];
	nr_swapper_spaces[type] = 0;
	rcu_assign_pointer(swapper_spaces[type], NULL);
	synchronize_rcu();
	kvfree(spaces);
}
