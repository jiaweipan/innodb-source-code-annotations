/******************************************************
The database buffer pool flush algorithm
数据库缓冲池刷新算法
(c) 1995 Innobase Oy

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0flu_h
#define buf0flu_h

#include "univ.i"
#include "buf0types.h"
#include "ut0byte.h"
#include "mtr0types.h"

/************************************************************************
Updates the flush system data structures when a write is completed. 当写入完成时更新刷新系统数据结构。*/

void
buf_flush_write_complete(
/*=====================*/
	buf_block_t*	block);	/* in: pointer to the block in question */
/*************************************************************************
Flushes pages from the end of the LRU list if there is too small
a margin of replaceable pages there. 
如果LRU列表末尾的可替换页面的空白太小，则刷新该列表末尾的页面。*/
void
buf_flush_free_margin(void);
/*=======================*/
/***********************************************************************
This utility flushes dirty blocks from the end of the LRU list or flush_list.
NOTE 1: in the case of an LRU flush the calling thread may own latches to
pages: to avoid deadlocks, this function must be written so that it cannot
end up waiting for these latches! NOTE 2: in the case of a flush list flush,
the calling thread is not allowed to own any latches on pages! 
这个实用程序从LRU列表或flush_list的末尾刷新脏块。注1:在LRU刷新的情况下，调用线程可能拥有对页面的锁存:
为了避免死锁，必须编写这个函数，使它不能等待这些锁存!注2:在刷新列表刷新的情况下，调用线程不允许拥有页上的任何锁存!*/
ulint
buf_flush_batch(
/*============*/
				/* out: number of blocks for which the write
				request was queued */
	ulint	flush_type,	/* in: BUF_FLUSH_LRU or BUF_FLUSH_LIST; if
				BUF_FLUSH_LIST, then the caller must not own
				any latches on pages */
	ulint	min_n,		/* in: wished minimum mumber of blocks flushed
				(it is not guaranteed that the actual number
				is that big, though) */
	dulint	lsn_limit);	/* in the case BUF_FLUSH_LIST all blocks whose
				oldest_modification is smaller than this
				should be flushed (if their number does not
				exceed min_n), otherwise ignored */
/**********************************************************************
Waits until a flush batch of the given type ends 等待给定类型的刷新批处理结束*/

void
buf_flush_wait_batch_end(
/*=====================*/
	ulint	type);	/* in: BUF_FLUSH_LRU or BUF_FLUSH_LIST */
/************************************************************************
This function should be called at a mini-transaction commit, if a page was
modified in it. Puts the block to the list of modified blocks, if it not
already in it. 如果在迷你事务提交中修改了页面，则应该在提交时调用此函数。如果该块还没有在其中，则将其放入已修改块的列表中。*/
UNIV_INLINE
void
buf_flush_note_modification(
/*========================*/
	buf_block_t*	block,	/* in: block which is modified */
	mtr_t*		mtr);	/* in: mtr */
/************************************************************************
This function should be called when recovery has modified a buffer page.当恢复修改了缓冲页时，应该调用此函数。 */
UNIV_INLINE
void
buf_flush_recv_note_modification(
/*=============================*/
	buf_block_t*	block,		/* in: block which is modified */
	dulint		start_lsn,	/* in: start lsn of the first mtr in a
					set of mtr's */
	dulint		end_lsn);	/* in: end lsn of the last mtr in the
					set of mtr's */
/************************************************************************
Returns TRUE if the file page block is immediately suitable for replacement,
i.e., transition FILE_PAGE => NOT_USED allowed. 如果文件页块立即适合替换，即转换FILE_PAGE => NOT_USED允许，则返回TRUE。*/
ibool
buf_flush_ready_for_replace(
/*========================*/
				/* out: TRUE if can replace immediately */
	buf_block_t*	block);	/* in: buffer control block, must be in state
				BUF_BLOCK_FILE_PAGE and in the LRU list */
/**********************************************************************
Validates the flush list. 验证刷新列表。*/

ibool
buf_flush_validate(void);
/*====================*/
		/* out: TRUE if ok */

/* When buf_flush_free_margin is called, it tries to make this many blocks
available to replacement in the free list and at the end of the LRU list (to
make sure that a read-ahead batch can be read efficiently in a single
sweep). 当调用buf_flush_free_margin时，它试图在空闲列表和LRU列表的末尾让这些块可用来替换(以确保在一次清除中可以有效地读取预读批处理)。*/

#define BUF_FLUSH_FREE_BLOCK_MARGIN 	(5 + BUF_READ_AHEAD_AREA)
#define BUF_FLUSH_EXTRA_MARGIN 		(BUF_FLUSH_FREE_BLOCK_MARGIN / 4 + 100)

#ifndef UNIV_NONINL
#include "buf0flu.ic"
#endif
				
#endif
