/******************************************************
The database buffer read
数据库缓冲区读取
(c) 1995 Innobase Oy

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#include "buf0rea.h"

#include "fil0fil.h"
#include "mtr0mtr.h"

#include "buf0buf.h"
#include "buf0flu.h"
#include "buf0lru.h"
#include "ibuf0ibuf.h"
#include "log0recv.h"
#include "trx0sys.h"
#include "os0file.h"
#include "srv0start.h"

/* The size in blocks of the area where the random read-ahead algorithm counts
the accessed pages when deciding whether to read-ahead 当决定是否预读时，随机预读算法计算所访问页面的区域块的大小 32*/
#define	BUF_READ_AHEAD_RANDOM_AREA	BUF_READ_AHEAD_AREA

/* There must be at least this many pages in buf_pool in the area to start
a random read-ahead 在buf_pool区域中必须至少有这么多页才能启动随机预读 9*/
#define BUF_READ_AHEAD_RANDOM_THRESHOLD	(5 + BUF_READ_AHEAD_RANDOM_AREA / 8)

/* The linear read-ahead area size 线性预读区大小*/
#define	BUF_READ_AHEAD_LINEAR_AREA	BUF_READ_AHEAD_AREA

/* The linear read-ahead threshold 线性预读阈值 12*/
#define BUF_READ_AHEAD_LINEAR_THRESHOLD	(3 * BUF_READ_AHEAD_LINEAR_AREA / 8)

/* If there are buf_pool->curr_size per the number below pending reads, then
read-ahead is not done: this is to prevent flooding the buffer pool with
i/o-fixed buffer blocks 如果有buf_pool->curr_size，则不执行预读:这是为了防止使用固定i/o的缓冲区块淹没缓冲池*/
#define BUF_READ_AHEAD_PEND_LIMIT	2

/************************************************************************
Low-level function which reads a page asynchronously from a file to the
buffer buf_pool if it is not already there, in which case does nothing.
Sets the io_fix flag and sets an exclusive lock on the buffer frame. The
flag is cleared and the x-lock released by an i/o-handler thread. */
/*一个低级函数，它将一个页面异步地从一个文件读入缓冲区buf_pool，在这种情况下什么也不做。
设置io_fix标志并设置缓冲区帧上的排他锁。标志被清除，x-lock被一个i/o处理器线程释放。*/
static
ulint
buf_read_page_low(
/*==============*/
			/* out: 1 if a read request was queued, 0 if the page
			already resided in buf_pool or if the page is in
			the doublewrite buffer blocks in which case it is never
			read into the pool */
	ibool	sync,	/* in: TRUE if synchronous aio is desired */
	ulint	mode,	/* in: BUF_READ_IBUF_PAGES_ONLY, ...,
			ORed to OS_AIO_SIMULATED_WAKE_LATER (see below
			at read-ahead functions) */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number */
{
	buf_block_t*	block;
	ulint		wake_later;

	wake_later = mode & OS_AIO_SIMULATED_WAKE_LATER;
	mode = mode & ~OS_AIO_SIMULATED_WAKE_LATER;
	
	if (trx_doublewrite && space == TRX_SYS_SPACE
		&& (   (offset >= trx_doublewrite->block1
		        && offset < trx_doublewrite->block1
		     		+ TRX_SYS_DOUBLEWRITE_BLOCK_SIZE)
		    || (offset >= trx_doublewrite->block2
		        && offset < trx_doublewrite->block2
		     		+ TRX_SYS_DOUBLEWRITE_BLOCK_SIZE))) {
		return(0);
	}

#ifdef UNIV_LOG_DEBUG
	if (space % 2 == 1) {
		/* We are updating a replicate space while holding the
		log mutex: the read must be handled before other reads
		which might incur ibuf operations and thus write to the log */

		printf("Log debug: reading replicate page in sync mode\n");

		sync = TRUE;
	}
#endif
	if (ibuf_bitmap_page(offset) || trx_sys_hdr_page(space, offset)) {

		/* Trx sys header is so low in the latching order that we play
		safe and do not leave the i/o-completion to an asynchronous
		i/o-thread. Ibuf bitmap pages must always be read with
                syncronous i/o, to make sure they do not get involved in
                thread deadlocks. */
		/*Trx sys头在锁存顺序中是如此之低，以至于我们玩得很安全，不把i/o完成留给异步i/o线程。
		必须始终使用同步i/o读取Ibuf位图页，以确保它们不会卷入线程死锁。*/
		sync = TRUE;
	}

	block = buf_page_init_for_read(mode, space, offset);

	if (block != NULL) {
		fil_io(OS_FILE_READ | wake_later,
			sync, space, offset, 0, UNIV_PAGE_SIZE,
					(void*)block->frame, (void*)block);
		if (sync) {
			/* The i/o is already completed when we arrive from
			fil_read 当从fil_read到达时，i/o已经完成*/
			buf_page_io_complete(block);
		}
		
		return(1);
	}

	return(0);
}	

/************************************************************************
Applies a random read-ahead in buf_pool if there are at least a threshold
value of accessed pages from the random read-ahead area. Does not read any
page, not even the one at the position (space, offset), if the read-ahead
mechanism is not activated. NOTE 1: the calling thread may own latches on
pages: to avoid deadlocks this function must be written such that it cannot
end up waiting for these latches! NOTE 2: the calling thread must want
access to the page given: this rule is set to prevent unintended read-aheads
performed by ibuf routines, a situation which could result in a deadlock if
the OS does not support asynchronous i/o. 
如果从随机预读区访问的页面至少有一个阈值，则在buf_pool中应用随机预读。
如果未激活预读机制，则不读取任何页，甚至不读取位置(空格、偏移量)上的页。
注意1:调用线程可能拥有页上的锁存:为了避免死锁，这个函数必须被写成不能等待这些锁存!
注意2:调用线程必须要访问给定的页面:这个规则是为了防止ibuf例程执行意外的预读操作，如果操作系统不支持异步i/o，这种情况可能会导致死锁。*/
static
ulint
buf_read_ahead_random(
/*==================*/
			/* out: number of page read requests issued; NOTE
			that if we read ibuf pages, it may happen that
			the page at the given page number does not get
			read even if we return a value > 0! */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number of a page which the current thread
			wants to access */
{
	buf_block_t*	block;
	ulint		recent_blocks	= 0;
	ulint		count;
	ulint		LRU_recent_limit;
	ulint		ibuf_mode;
	ulint		low, high;
	ulint		i;

	if (srv_startup_is_before_trx_rollback_phase) {
	        /* No read-ahead to avoid thread deadlocks 没有预读以避免线程死锁*/
	        return(0);
	}

	if (ibuf_bitmap_page(offset) || trx_sys_hdr_page(space, offset)) {

		/* If it is an ibuf bitmap page or trx sys hdr, we do
                no read-ahead, as that could break the ibuf page access
                order 如果它是一个ibuf位图页或trx sys hdr，我们不做预读，因为这会破坏ibuf页访问顺序*/

		return(0);
	}

	low  = (offset / BUF_READ_AHEAD_RANDOM_AREA)
					* BUF_READ_AHEAD_RANDOM_AREA;
	high = (offset / BUF_READ_AHEAD_RANDOM_AREA + 1)
					* BUF_READ_AHEAD_RANDOM_AREA;

	if (high > fil_space_get_size(space)) {

		high = fil_space_get_size(space);
	}

	/* Get the minimum LRU_position field value for an initial segment
	of the LRU list, to determine which blocks have recently been added
	to the start of the list. 获取LRU列表初始段的最小LRU_position字段值，以确定哪些块最近被添加到列表的开头。*/
	
	LRU_recent_limit = buf_LRU_get_recent_limit();

	mutex_enter(&(buf_pool->mutex));

	if (buf_pool->n_pend_reads >
			buf_pool->curr_size / BUF_READ_AHEAD_PEND_LIMIT) {
		mutex_exit(&(buf_pool->mutex));

		return(0);
	}	

	/* Count how many blocks in the area have been recently accessed,
	that is, reside near the start of the LRU list. 计算该区域最近访问过的块数，即位于LRU列表开头附近的块数。*/

	for (i = low; i < high; i++) {

		block = buf_page_hash_get(space, i);

		if ((block)
		    && (block->LRU_position > LRU_recent_limit)
		    && block->accessed) {

			recent_blocks++;
		}
	}

	mutex_exit(&(buf_pool->mutex));
	
	if (recent_blocks < BUF_READ_AHEAD_RANDOM_THRESHOLD) {
		/* Do nothing */

		return(0);
	}

	/* Read all the suitable blocks within the area 阅读该区域内所有合适的块*/

	if (ibuf_inside()) {
		ibuf_mode = BUF_READ_IBUF_PAGES_ONLY;
	} else {
		ibuf_mode = BUF_READ_ANY_PAGE;
	}

	count = 0;

	for (i = low; i < high; i++) {
		/* It is only sensible to do read-ahead in the non-sync aio
		mode: hence FALSE as the first parameter 只有在非同步aio模式下进行预读才明智:因此FALSE作为第一个参数*/

		if (!ibuf_bitmap_page(i)) {
			
			count += buf_read_page_low(FALSE, ibuf_mode
					| OS_AIO_SIMULATED_WAKE_LATER,
								space, i);
		}
	}

	/* In simulated aio we wake the aio handler threads only after
	queuing all aio requests, in native aio the following call does
	nothing: 在模拟aio中，我们只有在所有aio请求排队后才唤醒aio处理程序线程，在原生aio中，以下调用不做任何事情:*/
	
	os_aio_simulated_wake_handler_threads();

	if (buf_debug_prints && (count > 0)) {
	
		printf("Random read-ahead space %lu offset %lu pages %lu\n",
						space, offset, count);
	}

	return(count);
}

/************************************************************************
High-level function which reads a page asynchronously from a file to the
buffer buf_pool if it is not already there. Sets the io_fix flag and sets
an exclusive lock on the buffer frame. The flag is cleared and the x-lock
released by the i/o-handler thread. Does a random read-ahead if it seems
sensible. 这是一种高级函数，它将一个不存在的页面从文件异步读取到缓冲区buf_pool中。
设置io_fix标志并设置缓冲区帧上的排他锁。标记被清除，x-lock被i/o-handler线程释放。如果看起来合理的话，执行随机预读。*/
ulint
buf_read_page(
/*==========*/
			/* out: number of page read requests issued: this can
			be > 1 if read-ahead occurred */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number */
{
	ulint	count;
	ulint	count2;

	count = buf_read_ahead_random(space, offset);

	/* We do the i/o in the synchronous aio mode to save thread
	switches: hence TRUE 我们在同步aio模式下执行i/o以保存线程切换:因此为TRUE*/

	count2 = buf_read_page_low(TRUE, BUF_READ_ANY_PAGE, space, offset);

	/* Flush pages from the end of the LRU list if necessary 如果需要，从LRU列表的末尾刷新页面*/
	buf_flush_free_margin();

	return(count + count2);
}

/************************************************************************
Applies linear read-ahead if in the buf_pool the page is a border page of
a linear read-ahead area and all the pages in the area have been accessed.
Does not read any page if the read-ahead mechanism is not activated. Note
that the the algorithm looks at the 'natural' adjacent successor and
predecessor of the page, which on the leaf level of a B-tree are the next
and previous page in the chain of leaves. To know these, the page specified
in (space, offset) must already be present in the buf_pool. Thus, the
natural way to use this function is to call it when a page in the buf_pool
is accessed the first time, calling this function just after it has been
bufferfixed.
如果在buf_pool中，页面是线性预读区域的边界页面，并且该区域中的所有页面都已被访问，则应用线性预读。
当未激活预读机制时，不读取任何页面。请注意，该算法着眼于页面的“自然”相邻后继和前任，在b树的叶层上是叶子链的下一页和前一页。
要知道这些，在(空格，偏移量)中指定的页必须已经存在于buf_pool中。
因此，使用这个函数的自然方法是在第一次访问buf_pool中的一个页面时调用它，在它被bufferfixed之后调用这个函数。
NOTE 1: as this function looks at the natural predecessor and successor
fields on the page, what happens, if these are not initialized to any
sensible value? No problem, before applying read-ahead we check that the
area to read is within the span of the space, if not, read-ahead is not
applied. An uninitialized value may result in a useless read operation, but
only very improbably.
注意1:当这个函数查看页面上的自然前任和后继字段时，如果这些字段没有初始化为任何合理的值会发生什么?
没有问题，在应用预读之前，我们检查要读的区域是否在空间的范围内，如果不在，则不应用预读。
未初始化的值可能导致无用的读操作，但这只是非常不可能的。
NOTE 2: the calling thread may own latches on pages: to avoid deadlocks this
function must be written such that it cannot end up waiting for these
latches!注意2:调用线程可能拥有页上的锁存:为了避免死锁，这个函数必须被写成不能等待这些锁存!
NOTE 3: the calling thread must want access to the page given: this rule is
set to prevent unintended read-aheads performed by ibuf routines, a situation
which could result in a deadlock if the OS does not support asynchronous io.
注意3:调用线程必须想要访问给定的页面:该规则的设置是为了防止ibuf例程执行意外的预读操作，如果操作系统不支持异步io，这种情况可能导致死锁。 */

ulint
buf_read_ahead_linear(
/*==================*/
			/* out: number of page read requests issued */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number of a page; NOTE: the current thread
			must want access to this page (see NOTE 3 above) */
{
	buf_block_t*	block;
	buf_frame_t*	frame;
	buf_block_t*	pred_block	= NULL;
	ulint		pred_offset;
	ulint		succ_offset;
	ulint		count;
	int		asc_or_desc;
	ulint		new_offset;
	ulint		fail_count;
	ulint		ibuf_mode;
	ulint		low, high;
	ulint		i;
	
	if (srv_startup_is_before_trx_rollback_phase) {
	        /* No read-ahead to avoid thread deadlocks 没有预读以避免线程死锁*/
	        return(0);
	}

	if (ibuf_bitmap_page(offset) || trx_sys_hdr_page(space, offset)) {

		/* If it is an ibuf bitmap page or trx sys hdr, we do
                no read-ahead, as that could break the ibuf page access
                order  如果它是一个ibuf位图页或trx sys hdr，我们不做预读，因为这会破坏ibuf页访问顺序*/

		return(0);
	}

	low  = (offset / BUF_READ_AHEAD_LINEAR_AREA)
					* BUF_READ_AHEAD_LINEAR_AREA;
	high = (offset / BUF_READ_AHEAD_LINEAR_AREA + 1)
					* BUF_READ_AHEAD_LINEAR_AREA;

	if ((offset != low) && (offset != high - 1)) {
		/* This is not a border page of the area: return 这不是一个边框页的区域:返回*/

		return(0);
	}

	if (high > fil_space_get_size(space)) {
		/* The area is not whole, return 面积不全，返回*/

		return(0);
	}

	mutex_enter(&(buf_pool->mutex));

	if (buf_pool->n_pend_reads >
			buf_pool->curr_size / BUF_READ_AHEAD_PEND_LIMIT) {
		mutex_exit(&(buf_pool->mutex));

		return(0);
	}	

	/* Check that almost all pages in the area have been accessed; if
	offset == low, the accesses must be in a descending order, otherwise,
	in an ascending order.检查该区域中的几乎所有页面都已被访问;如果offset == low，则访问必须按降序进行，否则按升序进行。 */

	asc_or_desc = 1;

	if (offset == low) {
		asc_or_desc = -1;
	}

	fail_count = 0;

	for (i = low; i < high; i++) {

		block = buf_page_hash_get(space, i);
		
		if ((block == NULL) || !block->accessed) {

			/* Not accessed */
			fail_count++;

		} else if (pred_block && (ut_ulint_cmp(block->LRU_position,
				      		    pred_block->LRU_position)
			       		  != asc_or_desc)) {

			/* Accesses not in the right order 访问顺序不对*/

			fail_count++;
			pred_block = block;
		}
	}

	if (fail_count > BUF_READ_AHEAD_LINEAR_AREA -
			 BUF_READ_AHEAD_LINEAR_THRESHOLD) {
		/* Too many failures: return */

		mutex_exit(&(buf_pool->mutex));

		return(0);
	}

	/* If we got this far, we know that enough pages in the area have
	been accessed in the right order: linear read-ahead can be sensible 
	如果我们走到这里，我们就知道该区域中足够多的页面已经以正确的顺序被访问了:线性预读可以是合理的 */
	block = buf_page_hash_get(space, offset);

	if (block == NULL) {
		mutex_exit(&(buf_pool->mutex));

		return(0);
	}

	frame = block->frame;
	
	/* Read the natural predecessor and successor page addresses from
	the page; NOTE that because the calling thread may have an x-latch
	on the page, we do not acquire an s-latch on the page, this is to
	prevent deadlocks. Even if we read values which are nonsense, the
	algorithm will work. 从该页中读取自然的前辈和后继页地址;
	注意，因为调用线程可能在页面上有一个x-latch，所以我们不获取页面上的s-latch，
	这是为了防止死锁。即使我们读的值是无意义的，算法也会工作。*/ 

	pred_offset = fil_page_get_prev(frame);
	succ_offset = fil_page_get_next(frame);

	mutex_exit(&(buf_pool->mutex));
	
	if ((offset == low) && (succ_offset == offset + 1)) {

	    	/* This is ok, we can continue */
	    	new_offset = pred_offset;

	} else if ((offset == high - 1) && (pred_offset == offset - 1)) {

	    	/* This is ok, we can continue */
	    	new_offset = succ_offset;
	} else {
		/* Successor or predecessor not in the right order */

		return(0);
	}

	low  = (new_offset / BUF_READ_AHEAD_LINEAR_AREA)
					* BUF_READ_AHEAD_LINEAR_AREA;
	high = (new_offset / BUF_READ_AHEAD_LINEAR_AREA + 1)
					* BUF_READ_AHEAD_LINEAR_AREA;

	if ((new_offset != low) && (new_offset != high - 1)) {
		/* This is not a border page of the area: return */

		return(0);
	}

	if (high > fil_space_get_size(space)) {
		/* The area is not whole, return */

		return(0);
	}

	/* If we got this far, read-ahead can be sensible: do it */	    	

	if (ibuf_inside()) {
		ibuf_mode = BUF_READ_IBUF_PAGES_ONLY;
	} else {
		ibuf_mode = BUF_READ_ANY_PAGE;
	}

	count = 0;

	for (i = low; i < high; i++) {
		/* It is only sensible to do read-ahead in the non-sync
		aio mode: hence FALSE as the first parameter */

		if (!ibuf_bitmap_page(i)) {
			count += buf_read_page_low(FALSE, ibuf_mode
					| OS_AIO_SIMULATED_WAKE_LATER,
					space, i);
		}
	}

	/* In simulated aio we wake the aio handler threads only after
	queuing all aio requests, in native aio the following call does
	nothing: 在模拟aio中，我们只有在所有aio请求排队后才唤醒aio处理程序线程，在原生aio中，以下调用不做任何事情:*/
	
	os_aio_simulated_wake_handler_threads();

	/* Flush pages from the end of the LRU list if necessary 如果需要，从LRU列表的末尾刷新页面*/
	buf_flush_free_margin();

	if (buf_debug_prints && (count > 0)) {
		printf(
		"LINEAR read-ahead space %lu offset %lu pages %lu\n",
		space, offset, count);
	}

	return(count);
}

/************************************************************************
Issues read requests for pages which the ibuf module wants to read in, in
order to contract insert buffer trees. Technically, this function is like
a read-ahead function. */
/*对ibuf模块想要读入的页面发出读请求，以收缩插入缓冲区树。从技术上讲，这个函数类似于预读函数。*/
void
buf_read_ibuf_merge_pages(
/*======================*/
	ibool	sync,		/* in: TRUE if the caller wants this function
				to wait for the highest address page to get
				read in, before this function returns */ /*如果调用者希望该函数等待最高的地址页被读入，然后再返回，则为TRUE*/
	ulint	space,		/* in: space id */
	ulint*	page_nos,	/* in: array of page numbers to read, with the
				highest page number the last in the array */
	ulint	n_stored)	/* in: number of page numbers in the array */
{
	ulint	i;

	ut_ad(!ibuf_inside());
#ifdef UNIV_IBUF_DEBUG
	ut_a(n_stored < UNIV_PAGE_SIZE);
#endif	
	while (buf_pool->n_pend_reads >
			buf_pool->curr_size / BUF_READ_AHEAD_PEND_LIMIT) {
		os_thread_sleep(500000);
	}	

	for (i = 0; i < n_stored; i++) {
		if ((i + 1 == n_stored) && sync) {
			buf_read_page_low(TRUE, BUF_READ_ANY_PAGE, space,
								page_nos[i]);
		} else {
			buf_read_page_low(FALSE, BUF_READ_ANY_PAGE, space,
								page_nos[i]);
		}
	}
	
	/* Flush pages from the end of the LRU list if necessary */ /*如果需要，从LRU列表的末尾刷新页面*/
	buf_flush_free_margin();

	if (buf_debug_prints) {
		printf("Ibuf merge read-ahead space %lu pages %lu\n",
							space, n_stored);
	}
}

/************************************************************************
Issues read requests for pages which recovery wants to read in. */
/*对恢复要读取的页面发出读取请求。*/
void
buf_read_recv_pages(
/*================*/
	ibool	sync,		/* in: TRUE if the caller wants this function
				to wait for the highest address page to get
				read in, before this function returns */
	ulint	space,		/* in: space id */
	ulint*	page_nos,	/* in: array of page numbers to read, with the
				highest page number the last in the array */
	ulint	n_stored)	/* in: number of page numbers in the array */
{
	ulint	i;

	for (i = 0; i < n_stored; i++) {

		while (buf_pool->n_pend_reads >= RECV_POOL_N_FREE_BLOCKS / 2) {

			os_aio_simulated_wake_handler_threads();
			os_thread_sleep(500000);
		}

		if ((i + 1 == n_stored) && sync) {
			buf_read_page_low(TRUE, BUF_READ_ANY_PAGE, space,
								page_nos[i]);
		} else {
			buf_read_page_low(FALSE, BUF_READ_ANY_PAGE
					| OS_AIO_SIMULATED_WAKE_LATER,
					space, page_nos[i]);
		}
	}
	
	os_aio_simulated_wake_handler_threads();

	/* Flush pages from the end of the LRU list if necessary */
	buf_flush_free_margin();

	if (buf_debug_prints) {
		printf("Recovery applies read-ahead pages %lu\n", n_stored);
	}
}
