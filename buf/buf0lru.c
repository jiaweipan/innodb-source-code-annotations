/******************************************************
The database buffer replacement algorithm
数据库缓冲区替换算法
(c) 1995 Innobase Oy

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#include "buf0lru.h"

#ifdef UNIV_NONINL
#include "buf0lru.ic"
#include "srv0srv.h"	/* Needed to getsrv_print_innodb_monitor */
#endif

#include "ut0byte.h"
#include "ut0lst.h"
#include "ut0rnd.h"
#include "sync0sync.h"
#include "sync0rw.h"
#include "hash0hash.h"
#include "os0sync.h"
#include "fil0fil.h"
#include "btr0btr.h"
#include "buf0buf.h"
#include "buf0flu.h"
#include "buf0rea.h"
#include "btr0sea.h"
#include "os0file.h"

/* The number of blocks from the LRU_old pointer onward, including the block
pointed to, must be 3/8 of the whole LRU list length, except that the
tolerance defined below is allowed. Note that the tolerance must be small
enough such that for even the BUF_LRU_OLD_MIN_LEN long LRU list, the
LRU_old pointer is not allowed to point to either end of the LRU list. 
从LRU_old指针开始的块的数量，包括所指向的块，必须是整个LRU列表长度的3/8，除了下面定义的公差是允许的。
注意，允许的范围必须足够小，即使是BUF_LRU_OLD_MIN_LEN长LRU列表，也不允许LRU_old指针指向LRU列表的任何一端。
*/
#define BUF_LRU_OLD_TOLERANCE	20

/* The whole LRU list length is divided by this number to determine an
initial segment in buf_LRU_get_recent_limit 整个LRU列表的长度除以这个数字来确定buf_LRU_get_recent_limit中的初始段*/

#define BUF_LRU_INITIAL_RATIO	8

/**********************************************************************
Takes a block out of the LRU list and page hash table and sets the block
state to BUF_BLOCK_REMOVE_HASH. 从LRU列表和页哈希表中取出一个块，并将块状态设置为BUF_BLOCK_REMOVE_HASH。*/
static
void
buf_LRU_block_remove_hashed_page(
/*=============================*/
	buf_block_t*	block);	/* in: block, must contain a file page and
				be in a state where it can be freed; there
				may or may not be a hash index to the page */
/**********************************************************************
Puts a file page whose has no hash index to the free list. 将一个没有哈希索引的文件页放到空闲列表中。*/
static
void
buf_LRU_block_free_hashed_page(
/*===========================*/
	buf_block_t*	block);	/* in: block, must contain a file page and
				be in a state where it can be freed */

/**********************************************************************
Gets the minimum LRU_position field for the blocks in an initial segment
(determined by BUF_LRU_INITIAL_RATIO) of the LRU list. The limit is not
guaranteed to be precise, because the ulint_clock may wrap around. */
/*获取LRU列表初始段(由BUF_LRU_INITIAL_RATIO决定)中块的最小LRU_position字段。这个限制不能保证是精确的，因为ulint_clock可能会绕圈。*/
ulint
buf_LRU_get_recent_limit(void)
/*==========================*/
			/* out: the limit; zero if could not determine it */
{
	buf_block_t*	block;
	ulint		len;
	ulint		limit;

	mutex_enter(&(buf_pool->mutex));

	len = UT_LIST_GET_LEN(buf_pool->LRU);

	if (len < BUF_LRU_OLD_MIN_LEN) {
		/* The LRU list is too short to do read-ahead LRU列表太短，无法进行预读*/

		mutex_exit(&(buf_pool->mutex));

		return(0);
	}

	block = UT_LIST_GET_FIRST(buf_pool->LRU);

	limit = block->LRU_position - len / BUF_LRU_INITIAL_RATIO;

	mutex_exit(&(buf_pool->mutex));

	return(limit);
}

/**********************************************************************
Look for a replaceable block from the end of the LRU list and put it to
the free list if found. 从LRU列表的末尾寻找一个可替换的块，如果找到，把它放到空闲列表中。*/

ibool
buf_LRU_search_and_free_block(
/*==========================*/
				/* out: TRUE if freed */
	ulint	n_iterations)	/* in: how many times this has been called
				repeatedly without result: a high value
				means that we should search farther */
{
	buf_block_t*	block;
	ibool		freed;

	mutex_enter(&(buf_pool->mutex));
	
	freed = FALSE;
	
	block = UT_LIST_GET_LAST(buf_pool->LRU);

	while (block != NULL) {

		if (buf_flush_ready_for_replace(block)) {

			if (buf_debug_prints) {
				printf(
				"Putting space %lu page %lu to free list\n",
					block->space, block->offset);
			}
			
			buf_LRU_block_remove_hashed_page(block);

			mutex_exit(&(buf_pool->mutex));

			btr_search_drop_page_hash_index(block->frame);

			mutex_enter(&(buf_pool->mutex));

			ut_a(block->buf_fix_count == 0);

			buf_LRU_block_free_hashed_page(block);

			freed = TRUE;

			break;
		}

		block = UT_LIST_GET_PREV(LRU, block);
	}

	if (buf_pool->LRU_flush_ended > 0) {
		buf_pool->LRU_flush_ended--;
	}
 
	if (!freed) {
		buf_pool->LRU_flush_ended = 0;
	}

	mutex_exit(&(buf_pool->mutex));
	
	return(freed);
}
	
/**********************************************************************
Tries to remove LRU flushed blocks from the end of the LRU list and put them
to the free list. This is beneficial for the efficiency of the insert buffer
operation, as flushed pages from non-unique non-clustered indexes are here
taken out of the buffer pool, and their inserts redirected to the insert
buffer. Otherwise, the flushed blocks could get modified again before read
operations need new buffer blocks, and the i/o work done in flushing would be
wasted. 尝试从LRU列表的末尾删除LRU刷新的块，并将它们放到空闲列表中。
这有利于提高插入缓冲区操作的效率，因为非惟一非聚集索引中的刷新页将从缓冲池中取出，并将它们的插入重定向到插入缓冲区。
否则，在读取操作需要新的缓冲区块之前，可能会再次修改刷新的块，并且在刷新中完成的i/o工作将被浪费。*/
void
buf_LRU_try_free_flushed_blocks(void)
/*=================================*/
{
	mutex_enter(&(buf_pool->mutex));

	while (buf_pool->LRU_flush_ended > 0) {

		mutex_exit(&(buf_pool->mutex));

		buf_LRU_search_and_free_block(0);
		
		mutex_enter(&(buf_pool->mutex));
	}

	mutex_exit(&(buf_pool->mutex));
}	

/**********************************************************************
Returns a free block from buf_pool. The block is taken off the free list.
If it is empty, blocks are moved from the end of the LRU list to the free
list. 从buf_pool返回一个空闲块。该区块从空闲列表中删除。如果它是空的，块将从LRU列表的末尾移动到空闲列表。*/

buf_block_t*
buf_LRU_get_free_block(void)
/*========================*/
				/* out: the free control block */
{
	buf_block_t*	block		= NULL;
	ibool		freed;
	ulint		n_iterations	= 0;
	ibool		mon_value_was;
	ibool		started_monitor	= FALSE;
loop:
	mutex_enter(&(buf_pool->mutex));

	if (buf_pool->LRU_flush_ended > 0) {
		mutex_exit(&(buf_pool->mutex));

		buf_LRU_try_free_flushed_blocks();
		
		mutex_enter(&(buf_pool->mutex));
	}
	
	/* If there is a block in the free list, take it 如果空闲列表中有块，就把它拿走*/
	if (UT_LIST_GET_LEN(buf_pool->free) > 0) {
		
		block = UT_LIST_GET_FIRST(buf_pool->free);
		UT_LIST_REMOVE(free, buf_pool->free, block);
		block->state = BUF_BLOCK_READY_FOR_USE;

		mutex_exit(&(buf_pool->mutex));

		if (started_monitor) {
			srv_print_innodb_monitor = mon_value_was;
		}	

		return(block);
	}
	
	/* If no block was in the free list, search from the end of the LRU
	list and try to free a block there 如果在空闲列表中没有块，从LRU列表的末尾搜索并尝试在那里释放一个块*/

	mutex_exit(&(buf_pool->mutex));

	freed = buf_LRU_search_and_free_block(n_iterations);

	if (freed > 0) {
		goto loop;
	}

	if (n_iterations > 30) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
		"InnoDB: Warning: difficult to find free blocks from\n"
		"InnoDB: the buffer pool (%lu search iterations)! Consider\n"
		"InnoDB: increasing the buffer pool size.\n",
						n_iterations);
		fprintf(stderr,
		"InnoDB: It is also possible that in your Unix version\n"
		"InnoDB: fsync is very slow, or completely frozen inside\n"
		"InnoDB: the OS kernel. Then upgrading to a newer version\n"
		"InnoDB: of your operating system may help. Look at the\n"
		"InnoDB: number of fsyncs in diagnostic info below.\n");

		fprintf(stderr,
		"InnoDB: Pending flushes (fsync) log: %lu; buffer pool: %lu\n",
	       			fil_n_pending_log_flushes,
				fil_n_pending_tablespace_flushes);
		fprintf(stderr,
	"InnoDB: %lu OS file reads, %lu OS file writes, %lu OS fsyncs\n",
			os_n_file_reads, os_n_file_writes, os_n_fsyncs);

		fprintf(stderr,
		"InnoDB: Starting InnoDB Monitor to print further\n"
		"InnoDB: diagnostics to the standard output.\n");

		mon_value_was = srv_print_innodb_monitor;
		started_monitor = TRUE;
		srv_print_innodb_monitor = TRUE;
	}

	/* No free block was found: try to flush the LRU list 没有发现空闲块:请尝试刷新LRU列表*/

	buf_flush_free_margin();

	os_aio_simulated_wake_handler_threads();

	if (n_iterations > 10) {

		os_thread_sleep(500000);
	}

	n_iterations++;

	goto loop;	
}	

/***********************************************************************
Moves the LRU_old pointer so that the length of the old blocks list
is inside the allowed limits. 移动LRU_old指针，使旧块列表的长度在允许的范围内。*/
UNIV_INLINE
void
buf_LRU_old_adjust_len(void)
/*========================*/
{
	ulint	old_len;
	ulint	new_len;

	ut_ad(buf_pool->LRU_old);
	ut_ad(mutex_own(&(buf_pool->mutex)));
	ut_ad(3 * (BUF_LRU_OLD_MIN_LEN / 8) > BUF_LRU_OLD_TOLERANCE + 5);

	for (;;) {
		old_len = buf_pool->LRU_old_len;
		new_len = 3 * (UT_LIST_GET_LEN(buf_pool->LRU) / 8);

		/* Update the LRU_old pointer if necessary 如果需要，更新LRU_old指针*/
	
		if (old_len < new_len - BUF_LRU_OLD_TOLERANCE) {
		
			buf_pool->LRU_old = UT_LIST_GET_PREV(LRU,
							buf_pool->LRU_old);
			(buf_pool->LRU_old)->old = TRUE;
			buf_pool->LRU_old_len++;

		} else if (old_len > new_len + BUF_LRU_OLD_TOLERANCE) {

			(buf_pool->LRU_old)->old = FALSE;
			buf_pool->LRU_old = UT_LIST_GET_NEXT(LRU,
							buf_pool->LRU_old);
			buf_pool->LRU_old_len--;
		} else {
			ut_ad(buf_pool->LRU_old); /* Check that we did not
						fall out of the LRU list 检查我们没有掉出LRU列表*/
			return;
		}
	}
}

/***********************************************************************
Initializes the old blocks pointer in the LRU list.
This function should be called when the LRU list grows to
BUF_LRU_OLD_MIN_LEN length. 
初始化LRU列表中的旧块指针。当LRU列表的长度增加到BUF_LRU_OLD_MIN_LEN时，应该调用这个函数。*/
static
void
buf_LRU_old_init(void)
/*==================*/
{
	buf_block_t*	block;

	ut_ad(UT_LIST_GET_LEN(buf_pool->LRU) == BUF_LRU_OLD_MIN_LEN);

	/* We first initialize all blocks in the LRU list as old and then use
	the adjust function to move the LRU_old pointer to the right
	position 我们首先将LRU列表中的所有块初始化为old，然后使用adjust函数将LRU_old指针移动到正确的位置*/

	block = UT_LIST_GET_FIRST(buf_pool->LRU);

	while (block != NULL) {
		block->old = TRUE;
		block = UT_LIST_GET_NEXT(LRU, block);
	}

	buf_pool->LRU_old = UT_LIST_GET_FIRST(buf_pool->LRU);
	buf_pool->LRU_old_len = UT_LIST_GET_LEN(buf_pool->LRU);
	
	buf_LRU_old_adjust_len();
}	    	

/**********************************************************************
Removes a block from the LRU list. 从LRU列表中移除一个块。*/
UNIV_INLINE
void
buf_LRU_remove_block(
/*=================*/
	buf_block_t*	block)	/* in: control block */
{
	ut_ad(buf_pool);
	ut_ad(block);
	ut_ad(mutex_own(&(buf_pool->mutex)));
		
	/* If the LRU_old pointer is defined and points to just this block,
	move it backward one step 如果定义了LRU_old指针并只指向这个块，则将它向后移动一步*/

	if (block == buf_pool->LRU_old) {

		/* Below: the previous block is guaranteed to exist, because
		the LRU_old pointer is only allowed to differ by the
		tolerance value from strict 3/8 of the LRU list length. 
		下面:前面的块保证存在，因为LRU_old指针只允许与严格的LRU列表长度的3/8的公差值不同。*/
		buf_pool->LRU_old = UT_LIST_GET_PREV(LRU, block);
		(buf_pool->LRU_old)->old = TRUE;

		buf_pool->LRU_old_len++;
		ut_ad(buf_pool->LRU_old);
	}

	/* Remove the block from the LRU list 从LRU列表中移除该块*/
	UT_LIST_REMOVE(LRU, buf_pool->LRU, block);

	/* If the LRU list is so short that LRU_old not defined, return 如果LRU列表太短以至于LRU_old没有定义，则返回*/
	if (UT_LIST_GET_LEN(buf_pool->LRU) < BUF_LRU_OLD_MIN_LEN) {

		buf_pool->LRU_old = NULL;

		return;
	}

	ut_ad(buf_pool->LRU_old);	

	/* Update the LRU_old_len field if necessary 如果需要，更新LRU_old_len字段*/
	if (block->old) {

		buf_pool->LRU_old_len--;
	}

	/* Adjust the length of the old block list if necessary 如有必要，调整旧的阻止列表的长度*/
	buf_LRU_old_adjust_len();
}	    	

/**********************************************************************
Adds a block to the LRU list end. 将一个块添加到LRU列表的末尾。*/
UNIV_INLINE
void
buf_LRU_add_block_to_end_low(
/*=========================*/
	buf_block_t*	block)	/* in: control block */
{
	buf_block_t*	last_block;
	
	ut_ad(buf_pool);
	ut_ad(block);
	ut_ad(mutex_own(&(buf_pool->mutex)));

	block->old = TRUE;

	last_block = UT_LIST_GET_LAST(buf_pool->LRU);

	if (last_block) {
		block->LRU_position = last_block->LRU_position;
	} else {
		block->LRU_position = buf_pool_clock_tic();
	}			

	UT_LIST_ADD_LAST(LRU, buf_pool->LRU, block);

	if (UT_LIST_GET_LEN(buf_pool->LRU) >= BUF_LRU_OLD_MIN_LEN) {

		buf_pool->LRU_old_len++;
	}

	if (UT_LIST_GET_LEN(buf_pool->LRU) > BUF_LRU_OLD_MIN_LEN) {

		ut_ad(buf_pool->LRU_old);

		/* Adjust the length of the old block list if necessary 如有必要，调整旧的阻止列表的长度*/

		buf_LRU_old_adjust_len();

	} else if (UT_LIST_GET_LEN(buf_pool->LRU) == BUF_LRU_OLD_MIN_LEN) {

		/* The LRU list is now long enough for LRU_old to become
		defined: init it 现在LRU列表已经足够长，可以定义LRU_old:初始化它*/

		buf_LRU_old_init();
	}
}	    	

/**********************************************************************
Adds a block to the LRU list.向LRU列表添加一个块。 */
UNIV_INLINE
void
buf_LRU_add_block_low(
/*==================*/
	buf_block_t*	block,	/* in: control block */
	ibool		old)	/* in: TRUE if should be put to the old blocks
				in the LRU list, else put to the start; if the
				LRU list is very short, the block is added to
				the start, regardless of this parameter 
				如果应该放在LRU列表中的旧块上，则为TRUE，否则放在开头;如果LRU列表很短，则将块添加到起始位置，与该参数无关*/
{
	ulint	cl;
	
	ut_ad(buf_pool);
	ut_ad(block);
	ut_ad(mutex_own(&(buf_pool->mutex)));

	block->old = old;
	cl = buf_pool_clock_tic();

	if (!old || (UT_LIST_GET_LEN(buf_pool->LRU) < BUF_LRU_OLD_MIN_LEN)) {

		UT_LIST_ADD_FIRST(LRU, buf_pool->LRU, block);

		block->LRU_position = cl;		
		block->freed_page_clock = buf_pool->freed_page_clock;
	} else {
		UT_LIST_INSERT_AFTER(LRU, buf_pool->LRU, buf_pool->LRU_old,
								block);
		buf_pool->LRU_old_len++;

		/* We copy the LRU position field of the previous block
		to the new block 我们将前一个块的LRU位置字段复制到新块中*/

		block->LRU_position = (buf_pool->LRU_old)->LRU_position;
	}

	if (UT_LIST_GET_LEN(buf_pool->LRU) > BUF_LRU_OLD_MIN_LEN) {

		ut_ad(buf_pool->LRU_old);

		/* Adjust the length of the old block list if necessary 如有必要，调整旧的阻止列表的长度*/

		buf_LRU_old_adjust_len();

	} else if (UT_LIST_GET_LEN(buf_pool->LRU) == BUF_LRU_OLD_MIN_LEN) {

		/* The LRU list is now long enough for LRU_old to become
		defined: init it 现在LRU列表已经足够长，可以定义LRU_old:初始化它*/

		buf_LRU_old_init();
	}	
}	    	

/**********************************************************************
Adds a block to the LRU list.向LRU列表添加一个块。 */

void
buf_LRU_add_block(
/*==============*/
	buf_block_t*	block,	/* in: control block */
	ibool		old)	/* in: TRUE if should be put to the old
				blocks in the LRU list, else put to the start;
				if the LRU list is very short, the block is
				added to the start, regardless of this
				parameter */
{
	buf_LRU_add_block_low(block, old);
}

/**********************************************************************
Moves a block to the start of the LRU list. 移动一个块到LRU列表的开头。*/

void
buf_LRU_make_block_young(
/*=====================*/
	buf_block_t*	block)	/* in: control block */
{
	buf_LRU_remove_block(block);
	buf_LRU_add_block_low(block, FALSE);
}

/**********************************************************************
Moves a block to the end of the LRU list. 移动一个块到LRU列表的末尾。*/

void
buf_LRU_make_block_old(
/*===================*/
	buf_block_t*	block)	/* in: control block */
{
	buf_LRU_remove_block(block);
	buf_LRU_add_block_to_end_low(block);
}

/**********************************************************************
Puts a block back to the free list. 将一个块放回空闲列表。*/

void
buf_LRU_block_free_non_file_page(
/*=============================*/
	buf_block_t*	block)	/* in: block, must not contain a file page */
{
	ut_ad(mutex_own(&(buf_pool->mutex)));
	ut_ad(block);
	
	ut_ad((block->state == BUF_BLOCK_MEMORY)
	      || (block->state == BUF_BLOCK_READY_FOR_USE));

	block->state = BUF_BLOCK_NOT_USED;

#ifdef UNIV_DEBUG	
	/* Wipe contents of page to reveal possible stale pointers to it */
	memset(block->frame, '\0', UNIV_PAGE_SIZE);
#endif	
	UT_LIST_ADD_FIRST(free, buf_pool->free, block);
}

/**********************************************************************
Takes a block out of the LRU list and page hash table and sets the block
state to BUF_BLOCK_REMOVE_HASH. 从LRU列表和页哈希表中取出一个块，并将块状态设置为BUF_BLOCK_REMOVE_HASH。*/
static
void
buf_LRU_block_remove_hashed_page(
/*=============================*/
	buf_block_t*	block)	/* in: block, must contain a file page and
				be in a state where it can be freed; there
				may or may not be a hash index to the page */
{
	ut_ad(mutex_own(&(buf_pool->mutex)));
	ut_ad(block);
	
	ut_ad(block->state == BUF_BLOCK_FILE_PAGE);

	ut_a(block->io_fix == 0);
	ut_a(block->buf_fix_count == 0);
	ut_a(ut_dulint_cmp(block->oldest_modification, ut_dulint_zero) == 0);

	buf_LRU_remove_block(block);

	buf_pool->freed_page_clock += 1;

 	buf_frame_modify_clock_inc(block->frame);
		
	HASH_DELETE(buf_block_t, hash, buf_pool->page_hash,
			buf_page_address_fold(block->space, block->offset),
			block);

	block->state = BUF_BLOCK_REMOVE_HASH;
}

/**********************************************************************
Puts a file page whose has no hash index to the free list. 将一个没有哈希索引的文件页放到空闲列表中。*/
static
void
buf_LRU_block_free_hashed_page(
/*===========================*/
	buf_block_t*	block)	/* in: block, must contain a file page and
				be in a state where it can be freed */
{
	ut_ad(mutex_own(&(buf_pool->mutex)));
	ut_ad(block->state == BUF_BLOCK_REMOVE_HASH);

	block->state = BUF_BLOCK_MEMORY;

	buf_LRU_block_free_non_file_page(block);
}
				
/**************************************************************************
Validates the LRU list. 验证LRU列表。*/

ibool
buf_LRU_validate(void)
/*==================*/
{
	buf_block_t*	block;
	ulint		old_len;
	ulint		new_len;
	ulint		LRU_pos;
	
	ut_ad(buf_pool);
	mutex_enter(&(buf_pool->mutex));

	if (UT_LIST_GET_LEN(buf_pool->LRU) >= BUF_LRU_OLD_MIN_LEN) {

		ut_a(buf_pool->LRU_old);
		old_len = buf_pool->LRU_old_len;
		new_len = 3 * (UT_LIST_GET_LEN(buf_pool->LRU) / 8);
		ut_a(old_len >= new_len - BUF_LRU_OLD_TOLERANCE);
		ut_a(old_len <= new_len + BUF_LRU_OLD_TOLERANCE);
	}
		
	UT_LIST_VALIDATE(LRU, buf_block_t, buf_pool->LRU);

	block = UT_LIST_GET_FIRST(buf_pool->LRU);

	old_len = 0;

	while (block != NULL) {

		ut_a(block->state == BUF_BLOCK_FILE_PAGE);

		if (block->old) {
			old_len++;
		}

		if (buf_pool->LRU_old && (old_len == 1)) {
			ut_a(buf_pool->LRU_old == block);
		}

		LRU_pos	= block->LRU_position;

		block = UT_LIST_GET_NEXT(LRU, block);

		if (block) {
			/* If the following assert fails, it may
			not be an error: just the buf_pool clock
			has wrapped around */
			ut_a(LRU_pos >= block->LRU_position);
		}
	}

	if (buf_pool->LRU_old) {
		ut_a(buf_pool->LRU_old_len == old_len);
	} 

	UT_LIST_VALIDATE(free, buf_block_t, buf_pool->free);

	block = UT_LIST_GET_FIRST(buf_pool->free);

	while (block != NULL) {
		ut_a(block->state == BUF_BLOCK_NOT_USED);

		block = UT_LIST_GET_NEXT(free, block);
	}

	mutex_exit(&(buf_pool->mutex));
	return(TRUE);
}

/**************************************************************************
Prints the LRU list. 打印LRU列表。*/

void
buf_LRU_print(void)
/*===============*/
{
	buf_block_t*	block;
	buf_frame_t*	frame;
	ulint		len;
	
	ut_ad(buf_pool);
	mutex_enter(&(buf_pool->mutex));

	printf("Pool ulint clock %lu\n", buf_pool->ulint_clock);

	block = UT_LIST_GET_FIRST(buf_pool->LRU);

	len = 0;

	while (block != NULL) {

		printf("BLOCK %lu ", block->offset);

		if (block->old) {
			printf("old ");
		}

		if (block->buf_fix_count) {
			printf("buffix count %lu ", block->buf_fix_count);
		}

		if (block->io_fix) {
			printf("io_fix %lu ", block->io_fix);
		}

		if (ut_dulint_cmp(block->oldest_modification,
				ut_dulint_zero) > 0) {
			printf("modif. ");
		}

		printf("LRU pos %lu ", block->LRU_position);
		
		frame = buf_block_get_frame(block);

		printf("type %lu ", fil_page_get_type(frame));
		printf("index id %lu ", ut_dulint_get_low(
					btr_page_get_index_id(frame)));

		block = UT_LIST_GET_NEXT(LRU, block);
		len++;
		if (len % 10 == 0) {
			printf("\n");
		}
	}

	mutex_exit(&(buf_pool->mutex));
}
