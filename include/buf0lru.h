/******************************************************
The database buffer pool LRU replacement algorithm
数据库缓冲区替换算法
(c) 1995 Innobase Oy

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0lru_h
#define buf0lru_h

#include "univ.i"
#include "ut0byte.h"
#include "buf0types.h"

/**********************************************************************
Tries to remove LRU flushed blocks from the end of the LRU list and put them
to the free list. This is beneficial for the efficiency of the insert buffer
operation, as flushed pages from non-unique non-clustered indexes are here
taken out of the buffer pool, and their inserts redirected to the insert
buffer. Otherwise, the flushed blocks could get modified again before read
operations need new buffer blocks, and the i/o work done in flushing would be
wasted. */
/*尝试从LRU列表的末尾删除LRU刷新的块，并将它们放到空闲列表中。这有利于提高插入缓冲区操作的效率，
因为非惟一非聚集索引中的刷新页将从缓冲池中取出，并将它们的插入重定向到插入缓冲区。
否则，在读取操作需要新的缓冲区块之前，可能会再次修改刷新的块，并且在刷新中完成的i/o工作将被浪费。*/
void
buf_LRU_try_free_flushed_blocks(void);
/*==================================*/

/*#######################################################################
These are low-level functions
#########################################################################*/

/* Minimum LRU list length for which the LRU_old pointer is defined  定义LRU_old指针的最小LRU列表长度*/

#define BUF_LRU_OLD_MIN_LEN	80

#define BUF_LRU_FREE_SEARCH_LEN		(5 + 2 * BUF_READ_AHEAD_AREA)

/**********************************************************************
Gets the minimum LRU_position field for the blocks in an initial segment
(determined by BUF_LRU_INITIAL_RATIO) of the LRU list. The limit is not
guaranteed to be precise, because the ulint_clock may wrap around. 
获取LRU列表初始段(由BUF_LRU_INITIAL_RATIO决定)中块的最小LRU_position字段。这个限制不能保证是精确的，因为ulint_clock可能会绕圈。*/
ulint
buf_LRU_get_recent_limit(void);
/*==========================*/
			/* out: the limit; zero if could not determine it */
/**********************************************************************
Returns a free block from the buf_pool. The block is taken off the
free list. If it is empty, blocks are moved from the end of the
LRU list to the free list.从buf_pool返回一个空闲块。该区块从空闲列表中删除。如果它是空的，块将从LRU列表的末尾移动到空闲列表。 */
buf_block_t*
buf_LRU_get_free_block(void);
/*=========================*/
				/* out: the free control block */
/**********************************************************************
Puts a block back to the free list.将一个块放回空闲列表。 */

void
buf_LRU_block_free_non_file_page(
/*=============================*/
	buf_block_t*	block);	/* in: block, must not contain a file page */
/**********************************************************************
Adds a block to the LRU list.向LRU列表添加一个块。 */

void
buf_LRU_add_block(
/*==============*/
	buf_block_t*	block,	/* in: control block */
	ibool		old);	/* in: TRUE if should be put to the old
				blocks in the LRU list, else put to the
				start; if the LRU list is very short, added to
				the start regardless of this parameter */
/**********************************************************************
Moves a block to the start of the LRU list. 移动一个块到LRU列表的开头。*/

void
buf_LRU_make_block_young(
/*=====================*/
	buf_block_t*	block);	/* in: control block */
/**********************************************************************
Moves a block to the end of the LRU list. 移动一个块到LRU列表的末尾。*/

void
buf_LRU_make_block_old(
/*===================*/
	buf_block_t*	block);	/* in: control block */
/**********************************************************************
Look for a replaceable block from the end of the LRU list and put it to
the free list if found. 从LRU列表的末尾寻找一个可替换的块，如果找到，把它放到空闲列表中。*/

ibool
buf_LRU_search_and_free_block(
/*==========================*/
				/* out: TRUE if freed */
	ulint	n_iterations);	/* in: how many times this has been called
				repeatedly without result: a high value
				means that we should search farther */
/**************************************************************************
Validates the LRU list. 验证LRU列表。*/

ibool
buf_LRU_validate(void);
/*==================*/
/**************************************************************************
Prints the LRU list. 打印LRU列表。*/

void
buf_LRU_print(void);
/*===============*/

#ifndef UNIV_NONINL
#include "buf0lru.ic"
#endif

#endif
