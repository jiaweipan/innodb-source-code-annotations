/*   Innobase relational database engine; Copyright (C) 2001 Innobase Oy
     
     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License 2
     as published by the Free Software Foundation in June 1991.
     
     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.
     
     You should have received a copy of the GNU General Public License 2
     along with this program (in file COPYING); if not, write to the Free
     Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */
/******************************************************
The database buffer pool high-level routines
数据库缓冲池高级例程
(c) 1995 Innobase Oy

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0buf_h
#define buf0buf_h

#include "univ.i"
#include "fil0fil.h"
#include "mtr0types.h"
#include "buf0types.h"
#include "sync0rw.h"
#include "hash0hash.h"
#include "ut0byte.h"

/* Flags for flush types */ /*用于刷新类型的标志*/
#define BUF_FLUSH_LRU		1
#define BUF_FLUSH_SINGLE_PAGE	2
#define BUF_FLUSH_LIST		3	/* An array in the pool struct
					has size BUF_FLUSH_LIST + 1: if you
					add more flush types, put them in
					the middle! */ /*池结构中的数组大小为BUF_FLUSH_LIST + 1:如果添加更多的刷新类型，请将它们放在中间!*/
/* Modes for buf_page_get_gen */
#define BUF_GET			10	/* get always */
#define	BUF_GET_IF_IN_POOL	11	/* get if in pool */
#define	BUF_GET_NOWAIT		12	/* get if can set the latch without
					waiting */
#define BUF_GET_NO_LATCH	14	/* get and bufferfix, but set no latch;
					we have separated this case, because
					it is error-prone programming not to
					set a latch, and it should be used
					with care */ /*Get和bufferfix，但是不设置闩锁;我们分隔了这个例子，因为不设置闩锁是容易出错的编程，应该小心使用它*/
/* Modes for buf_page_get_known_nowait */
#define BUF_MAKE_YOUNG	51
#define BUF_KEEP_OLD	52

extern buf_pool_t* 	buf_pool; 	/* The buffer pool of the database */
extern ibool		buf_debug_prints;/* If this is set TRUE, the program
					prints info whenever read or flush
					occurs */

/************************************************************************
Initializes the buffer pool of the database. */
/*初始化数据库的缓冲池。*/
void
buf_pool_init(
/*==========*/
	ulint	max_size,	/* in: maximum size of the pool in blocks */
	ulint	curr_size);	/* in: current size to use, must be <=
				max_size */
/*************************************************************************
Gets the current size of buffer pool in bytes. */ /*获取缓冲池的当前大小(以字节为单位)。*/
UNIV_INLINE
ulint
buf_pool_get_curr_size(void);
/*========================*/
			/* out: size in bytes */
/*************************************************************************
Gets the maximum size of buffer pool in bytes. */ /*获取缓冲池的最大大小(以字节为单位)。*/
UNIV_INLINE
ulint
buf_pool_get_max_size(void);
/*=======================*/
			/* out: size in bytes */
/************************************************************************
Gets the smallest oldest_modification lsn for any page in the pool. Returns
ut_dulint_zero if all modified pages have been flushed to disk. */
/*获取池中任何页面的最小oldest_modify lsn。如果所有修改的页面都已刷新到磁盘，则返回ut_dulint_zero。*/
UNIV_INLINE
dulint
buf_pool_get_oldest_modification(void);
/*==================================*/
				/* out: oldest modification in pool,
				ut_dulint_zero if none */
/*************************************************************************
Allocates a buffer frame. */
/*分配一个缓冲区帧。*/
buf_frame_t*
buf_frame_alloc(void);
/*==================*/
				/* out: buffer frame */
/*************************************************************************
Frees a buffer frame which does not contain a file page. */
/*释放不包含文件页的缓冲帧。*/
void
buf_frame_free(
/*===========*/
	buf_frame_t*	frame);	/* in: buffer frame */
/*************************************************************************
Copies contents of a buffer frame to a given buffer. */ /*将缓冲区帧的内容复制到给定的缓冲区。*/
UNIV_INLINE
byte*
buf_frame_copy(
/*===========*/
				/* out: buf */
	byte*		buf,	/* in: buffer to copy to */
	buf_frame_t*	frame);	/* in: buffer frame */
/******************************************************************
NOTE! The following macros should be used instead of buf_page_get_gen,
to improve debugging. Only values RW_S_LATCH and RW_X_LATCH are allowed
in LA! */ /*注意!下面的宏应该用来代替buf_page_get_gen，以改进调试。在LA!中，只允许使用RW_S_LATCH和RW_X_LATCH。*/
#define buf_page_get(SP, OF, LA, MTR)    buf_page_get_gen(\
				SP, OF, LA, NULL,\
				BUF_GET, IB__FILE__, __LINE__, MTR)
/******************************************************************
Use these macros to bufferfix a page with no latching. Remember not to
read the contents of the page unless you know it is safe. Do not modify
the contents of the page! We have separated this case, because it is
error-prone programming not to set a latch, and it should be used
with care. *//*使用这些宏来缓冲修复没有锁存的页面。记得不要阅读网页的内容，除非你知道它是安全的。
请勿修改网页内容!我们将这种情况分开，因为不设置闩锁是容易出错的编程，应该小心使用它。*/
#define buf_page_get_with_no_latch(SP, OF, MTR)    buf_page_get_gen(\
				SP, OF, RW_NO_LATCH, NULL,\
				BUF_GET_NO_LATCH, IB__FILE__, __LINE__, MTR)
/******************************************************************
NOTE! The following macros should be used instead of buf_page_get_gen, to
improve debugging. Only values RW_S_LATCH and RW_X_LATCH are allowed as LA! */
/*注意!下面的宏应该用来代替buf_page_get_gen，以改进调试。只有RW_S_LATCH和RW_X_LATCH值可以作为LA!*/
#define buf_page_get_nowait(SP, OF, LA, MTR)    buf_page_get_gen(\
				SP, OF, LA, NULL,\
				BUF_GET_NOWAIT, IB__FILE__, __LINE__, MTR)
/******************************************************************
NOTE! The following macros should be used instead of
buf_page_optimistic_get_func, to improve debugging. Only values RW_S_LATCH and
RW_X_LATCH are allowed as LA! */ 
/*注意!下面的宏应该用来代替buf_page_optimistic_get_func，以改进调试。只有RW_S_LATCH和RW_X_LATCH值可以作为LA!*/
#define buf_page_optimistic_get(LA, G, MC, MTR) buf_page_optimistic_get_func(\
				LA, G, MC, IB__FILE__, __LINE__, MTR)
/************************************************************************
This is the general function used to get optimistic access to a database
page. */
/*这是用于优化访问数据库页面的通用函数。*/
ibool
buf_page_optimistic_get_func(
/*=========================*/
				/* out: TRUE if success */
	ulint		rw_latch,/* in: RW_S_LATCH, RW_X_LATCH */
	buf_frame_t*	guess,	/* in: guessed frame */
	dulint		modify_clock,/* in: modify clock value if mode is
				..._GUESS_ON_CLOCK */
	char*		file,	/* in: file name */
	ulint		line,	/* in: line where called */
	mtr_t*		mtr);	/* in: mini-transaction */
/************************************************************************
Tries to get the page, but if file io is required, releases all latches
in mtr down to the given savepoint. If io is required, this function
retrieves the page to buffer buf_pool, but does not bufferfix it or latch
it. */
/*尝试获取页面，但如果需要文件io，则释放mtr中的所有锁存到给定保存点。如果需要io，这个函数将获取页面以缓冲buf_pool，但不进行缓冲修复或闩锁。*/
UNIV_INLINE
buf_frame_t*
buf_page_get_release_on_io(
/*=======================*/
				/* out: pointer to the frame, or NULL
				if not in buffer buf_pool */
	ulint	space,		/* in: space id */
	ulint	offset,		/* in: offset of the page within space
				in units of a page */
	buf_frame_t* guess,	/* in: guessed frame or NULL */
	ulint	rw_latch,	/* in: RW_X_LATCH, RW_S_LATCH,
				or RW_NO_LATCH */
	ulint	savepoint,	/* in: mtr savepoint */
	mtr_t*	mtr);		/* in: mtr */
/************************************************************************
This is used to get access to a known database page, when no waiting can be
done. */
/*这用于在不需要等待的情况下访问已知的数据库页面。*/
ibool
buf_page_get_known_nowait(
/*======================*/
				/* out: TRUE if success */
	ulint		rw_latch,/* in: RW_S_LATCH, RW_X_LATCH */
	buf_frame_t*	guess,	/* in: the known page frame */
	ulint		mode,	/* in: BUF_MAKE_YOUNG or BUF_KEEP_OLD */
	char*		file,	/* in: file name */
	ulint		line,	/* in: line where called */
	mtr_t*		mtr);	/* in: mini-transaction */
/************************************************************************
This is the general function used to get access to a database page. */
/*这是用于访问数据库页的常规函数。 */
buf_frame_t*
buf_page_get_gen(
/*=============*/
				/* out: pointer to the frame or NULL */
	ulint		space,	/* in: space id */
	ulint		offset,	/* in: page number */
	ulint		rw_latch,/* in: RW_S_LATCH, RW_X_LATCH, RW_NO_LATCH */
	buf_frame_t*	guess,	/* in: guessed frame or NULL */
	ulint		mode,	/* in: BUF_GET, BUF_GET_IF_IN_POOL,
				BUF_GET_NO_LATCH */
	char*		file,	/* in: file name */
	ulint		line,	/* in: line where called */
	mtr_t*		mtr);	/* in: mini-transaction */
/************************************************************************
Initializes a page to the buffer buf_pool. The page is usually not read
from a file even if it cannot be found in the buffer buf_pool. This is one
of the functions which perform to a block a state transition NOT_USED =>
FILE_PAGE (the other is buf_page_init_for_read above). */
/*将一个页面初始化为缓冲区buf_pool。即使在缓冲区buf_pool中找不到该页，也通常不会从文件中读取该页。
这是执行阻塞状态转换NOT_USED =>FILE_PAGE的函数之一(另一个是上面的buf_page_init_for_read)。*/
buf_frame_t*
buf_page_create(
/*============*/
			/* out: pointer to the frame, page bufferfixed */
	ulint	space,	/* in: space id */
	ulint	offset,	/* in: offset of the page within space in units of
			a page */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/************************************************************************
Decrements the bufferfix count of a buffer control block and releases
a latch, if specified. */ /*如果指定，则递减缓冲区控制块的bufferfix计数并释放一个闩锁。*/
UNIV_INLINE
void
buf_page_release(
/*=============*/
	buf_block_t*	block,		/* in: buffer block */
	ulint		rw_latch,	/* in: RW_S_LATCH, RW_X_LATCH,
					RW_NO_LATCH */
	mtr_t*		mtr);		/* in: mtr */
/************************************************************************
Moves a page to the start of the buffer pool LRU list. This high-level
function can be used to prevent an important page from from slipping out of
the buffer pool. */
/*移动一页到缓冲池LRU列表的开头。这个高级函数可用于防止重要页面从缓冲池中滑出。*/
void
buf_page_make_young(
/*=================*/
	buf_frame_t*	frame);	/* in: buffer frame of a file page */
/************************************************************************
Returns TRUE if the page can be found in the buffer pool hash table. NOTE
that it is possible that the page is not yet read from disk, though. */
/*如果可以在缓冲池哈希表中找到该页，则返回TRUE。注意，有可能该页还没有从磁盘读取。*/
ibool
buf_page_peek(
/*==========*/
			/* out: TRUE if found from page hash table,
			NOTE that the page is not necessarily yet read
			from disk! */
	ulint	space,	/* in: space id */
	ulint	offset);/* in: page number */
/************************************************************************
Returns the buffer control block if the page can be found in the buffer
pool. NOTE that it is possible that the page is not yet read
from disk, though. This is a very low-level function: use with care! */
/*如果可以在缓冲池中找到该页，则返回缓冲控制块。注意，有可能该页还没有从磁盘读取。这是一个非常低级的函数:小心使用!*/
buf_block_t*
buf_page_peek_block(
/*================*/
			/* out: control block if found from page hash table,
			otherwise NULL; NOTE that the page is not necessarily
			yet read from disk! */
	ulint	space,	/* in: space id */
	ulint	offset);/* in: page number */
/************************************************************************
Sets file_page_was_freed TRUE if the page is found in the buffer pool.
This function should be called when we free a file page and want the
debug version to check that it is not accessed any more unless
reallocated. */
/*如果在缓冲池中找到页面，则设置file_page_was_freed为TRUE。当释放文件页并希望调试版本检查该文件页是否不再被访问(除非重新分配)时，应该调用此函数。*/
buf_block_t*
buf_page_set_file_page_was_freed(
/*=============================*/
			/* out: control block if found from page hash table,
			otherwise NULL */
	ulint	space,	/* in: space id */
	ulint	offset);	/* in: page number */
/************************************************************************
Sets file_page_was_freed FALSE if the page is found in the buffer pool.
This function should be called when we free a file page and want the
debug version to check that it is not accessed any more unless
reallocated. */
/*如果在缓冲池中找到页面，则将file_page_was_freed设置为FALSE。当释放文件页并希望调试版本检查该文件页是否不再被访问(除非重新分配)时，应该调用此函数。*/
buf_block_t*
buf_page_reset_file_page_was_freed(
/*===============================*/
			/* out: control block if found from page hash table,
			otherwise NULL */
	ulint	space,	/* in: space id */
	ulint	offset);	/* in: page number */
/************************************************************************
Recommends a move of a block to the start of the LRU list if there is danger
of dropping from the buffer pool. NOTE: does not reserve the buffer pool
mutex. */ /*如果有从缓冲池中删除的危险，建议将块移动到LRU列表的开头。注意:不保留缓冲池互斥。*/
UNIV_INLINE
ibool
buf_block_peek_if_too_old(
/*======================*/
				/* out: TRUE if should be made younger */
	buf_block_t*	block);	/* in: block to make younger */
/************************************************************************
Returns the current state of is_hashed of a page. FALSE if the page is
not in the pool. NOTE that this operation does not fix the page in the
pool if it is found there. */
/*返回页面的is_hash的当前状态。如果页不在池中，则为FALSE。注意，如果在池中找到页面，此操作不会将其固定在池中。*/
ibool
buf_page_peek_if_search_hashed(
/*===========================*/
			/* out: TRUE if page hash index is built in search
			system */
	ulint	space,	/* in: space id */
	ulint	offset);/* in: page number */
/************************************************************************
Gets the youngest modification log sequence number for a frame.
Returns zero if not file page or no modification occurred yet. */
/*获取帧的最新修改日志序号。如果未发生文件页或未发生任何修改，则返回零。*/
UNIV_INLINE
dulint
buf_frame_get_newest_modification(
/*==============================*/
				/* out: newest modification to page */
	buf_frame_t*	frame);	/* in: pointer to a frame */
/************************************************************************
Increments the modify clock of a frame by 1. The caller must (1) own the
pool mutex and block bufferfix count has to be zero, (2) or own an x-lock
on the block. */ 
/*增加帧的修改时钟1。调用者必须(1)拥有池互斥锁，并且块缓冲修复计数必须为零，(2)或者在块上拥有一个x-lock。*/
UNIV_INLINE
dulint
buf_frame_modify_clock_inc(
/*=======================*/
				/* out: new value */
	buf_frame_t*	frame);	/* in: pointer to a frame */
/************************************************************************
Returns the value of the modify clock. The caller must have an s-lock 
or x-lock on the block. *//*返回已修改时钟的值。调用者必须在区块上有s-lock或x-lock。*/
UNIV_INLINE
dulint
buf_frame_get_modify_clock(
/*=======================*/
				/* out: value */
	buf_frame_t*	frame);	/* in: pointer to a frame */
/************************************************************************
Calculates a page checksum which is stored to the page when it is written
to a file. Note that we must be careful to calculate the same value
on 32-bit and 64-bit architectures. */
/*计算写入文件时存储到页面中的页面校验和。注意，我们必须小心地在32位和64位体系结构上计算相同的值。*/
ulint
buf_calc_page_checksum(
/*===================*/
		       /* out: checksum */
	byte*   page); /* in: buffer page */
/************************************************************************
Checks if a page is corrupt. */
/*检查页面是否损坏。*/
ibool
buf_page_is_corrupted(
/*==================*/
				/* out: TRUE if corrupted */
	byte*	read_buf);	/* in: a database page */
/**************************************************************************
Gets the page number of a pointer pointing within a buffer frame containing
a file page. */ /*获取指向包含文件页的缓冲帧内的指针的页码。*/
UNIV_INLINE
ulint
buf_frame_get_page_no(
/*==================*/
			/* out: page number */
	byte*	ptr);	/* in: pointer to within a buffer frame */
/**************************************************************************
Gets the space id of a pointer pointing within a buffer frame containing a
file page. */ /*获取指向包含文件页的缓冲帧内的指针的空间id。*/
UNIV_INLINE
ulint
buf_frame_get_space_id(
/*===================*/
			/* out: space id */
	byte*	ptr);	/* in: pointer to within a buffer frame */
/**************************************************************************
Gets the space id, page offset, and byte offset within page of a
pointer pointing to a buffer frame containing a file page. */ /*获取指向包含文件页的缓冲区帧的指针的页内的空间id、页偏移量和字节偏移量。*/
UNIV_INLINE
void
buf_ptr_get_fsp_addr(
/*=================*/
	byte*		ptr,	/* in: pointer to a buffer frame */
	ulint*		space,	/* out: space id */
	fil_addr_t*	addr);	/* out: page offset and byte offset */
/**************************************************************************
Gets the hash value of the page the pointer is pointing to. This can be used
in searches in the lock hash table. */ /*获取指针所指向的页面的哈希值。这可以在锁散列表的搜索中使用。*/
UNIV_INLINE
ulint
buf_frame_get_lock_hash_val(
/*========================*/
			/* out: lock hash value */
	byte*	ptr);	/* in: pointer to within a buffer frame */
/**************************************************************************
Gets the mutex number protecting the page record lock hash chain in the lock
table. */ /*获取保护锁表中页记录锁散列链的互斥锁号。*/
UNIV_INLINE
mutex_t*
buf_frame_get_lock_mutex(
/*=====================*/
			/* out: mutex */
	byte*	ptr);	/* in: pointer to within a buffer frame */
/***********************************************************************
Gets the frame the pointer is pointing to. */ /*获取指针所指向的帧。*/
UNIV_INLINE
buf_frame_t*
buf_frame_align(
/*============*/
			/* out: pointer to block */
	byte*	ptr);	/* in: pointer to a frame */
/***********************************************************************
Checks if a pointer points to the block array of the buffer pool (blocks, not
the frames). */ /*检查指针是否指向缓冲池的块数组(块，而不是帧)。*/
UNIV_INLINE
ibool
buf_pool_is_block(
/*==============*/
			/* out: TRUE if pointer to block */
	void*	ptr);	/* in: pointer to memory */
/*************************************************************************
Validates the buffer pool data structure. */
/*验证缓冲池数据结构。*/
ibool
buf_validate(void);
/*==============*/
/************************************************************************
Prints a page to stderr. */
/*将页面打印到标准错误。*/
void
buf_page_print(
/*===========*/
	byte*	read_buf);	/* in: a database page */
/*************************************************************************
Prints info of the buffer pool data structure. */
/*打印缓冲池数据结构的信息。*/
void
buf_print(void);
/*===========*/
/*************************************************************************
Returns the number of pending buf pool ios. */
/*返回挂起的buf池ios的数量。*/
ulint
buf_get_n_pending_ios(void);
/*=======================*/
/*************************************************************************
Prints info of the buffer i/o. */
/*打印缓冲区i/o的信息。*/
void
buf_print_io(void);
/*==============*/
/*************************************************************************
Checks that all file pages in the buffer are in a replaceable state. */
/*检查缓冲区中的所有文件页是否处于可替换状态。*/
ibool
buf_all_freed(void);
/*===============*/
/*************************************************************************
Checks that there currently are no pending i/o-operations for the buffer
pool. */
/*检查缓冲池当前没有挂起的i/o操作。*/
ibool
buf_pool_check_no_pending_io(void);
/*==============================*/
				/* out: TRUE if there is no pending i/o */
/*************************************************************************
Invalidates the file pages in the buffer pool when an archive recovery is
completed. All the file pages buffered must be in a replaceable state when
this function is called: not latched and not modified. */
/*当存档恢复完成时，使缓冲池中的文件页无效。当调用此函数时，所有缓冲的文件页必须处于可替换状态:未锁存和未修改。*/
void
buf_pool_invalidate(void);
/*=====================*/

/*========================================================================
--------------------------- LOWER LEVEL ROUTINES -------------------------
=========================================================================*/

/*************************************************************************
Adds latch level info for the rw-lock protecting the buffer frame. This
should be called in the debug version after a successful latching of a
page if we know the latching order level of the acquired latch. If
UNIV_SYNC_DEBUG is not defined, compiles to an empty function. */ 
/*为保护缓冲帧的rw-lock添加闩锁级别信息。如果我们知道获得的闩锁的闩锁顺序级别，那么在页面成功闩锁之后，
应该在调试版本中调用这个函数。如果UNIV_SYNC_DEBUG未定义，则编译为空函数。*/
UNIV_INLINE
void
buf_page_dbg_add_level(
/*===================*/
	buf_frame_t*	frame,	/* in: buffer page where we have acquired
				a latch */
	ulint		level);	/* in: latching order level */
/*************************************************************************
Gets a pointer to the memory frame of a block. */ /*获取指向块的内存帧的指针。*/
UNIV_INLINE
buf_frame_t*
buf_block_get_frame(
/*================*/
				/* out: pointer to the frame */
	buf_block_t*	block);	/* in: pointer to the control block */
/*************************************************************************
Gets the space id of a block. */ /*获取块的空间id。*/
UNIV_INLINE
ulint
buf_block_get_space(
/*================*/
				/* out: space id */
	buf_block_t*	block);	/* in: pointer to the control block */
/*************************************************************************
Gets the page number of a block. */ /*获取块的页码。*/
UNIV_INLINE
ulint
buf_block_get_page_no(
/*==================*/
				/* out: page number */
	buf_block_t*	block);	/* in: pointer to the control block */
/***********************************************************************
Gets the block to whose frame the pointer is pointing to. */ /*获取指针所指向的帧的块。*/
UNIV_INLINE
buf_block_t*
buf_block_align(
/*============*/
			/* out: pointer to block */
	byte*	ptr);	/* in: pointer to a frame */
/************************************************************************
This function is used to get info if there is an io operation
going on on a buffer page. */ /*该函数用于在缓冲区页面上进行io操作时获取信息。*/
UNIV_INLINE
ibool
buf_page_io_query(
/*==============*/
				/* out: TRUE if io going on */
	buf_block_t*	block);	/* in: pool block, must be bufferfixed */
/***********************************************************************
Accessor function for block array. *//*块数组的访问函数。*/
UNIV_INLINE
buf_block_t*
buf_pool_get_nth_block(
/*===================*/
				/* out: pointer to block */
	buf_pool_t*	pool,	/* in: pool */
	ulint		i);	/* in: index of the block */
/************************************************************************
Function which inits a page for read to the buffer buf_pool. If the page is
already in buf_pool, does nothing. Sets the io_fix flag to BUF_IO_READ and
sets a non-recursive exclusive lock on the buffer frame. The io-handler must
take care that the flag is cleared and the lock released later. This is one
of the functions which perform the state transition NOT_USED => FILE_PAGE to
a block (the other is buf_page_create). */ 
/*初始化buffer buf_pool的读页面的函数。如果页面已经在buf_pool中，则不执行任何操作。
设置io_fix标志为BUF_IO_READ，并设置缓冲区帧上的非递归排他锁。io处理程序必须注意清除标志并稍后释放锁。
这是执行状态转换NOT_USED => FILE_PAGE到一个块的函数之一(另一个是buf_page_create)。*/
buf_block_t*
buf_page_init_for_read(
/*===================*/
			/* out: pointer to the block */
	ulint	mode,	/* in: BUF_READ_IBUF_PAGES_ONLY, ... */
	ulint	space,	/* in: space id */
	ulint	offset);/* in: page number */
/************************************************************************
Completes an asynchronous read or write request of a file page to or from
the buffer pool. */
/*完成对缓冲池的文件页的异步读或写请求。*/
void
buf_page_io_complete(
/*=================*/
	buf_block_t*	block);	/* in: pointer to the block in question */
/************************************************************************
Calculates a folded value of a file page address to use in the page hash
table. */  /*计算要在页哈希表中使用的文件页地址的折叠值。*/
UNIV_INLINE
ulint
buf_page_address_fold(
/*==================*/
			/* out: the folded value */
	ulint	space,	/* in: space id */
	ulint	offset);/* in: offset of the page within space */
/**********************************************************************
Returns the control block of a file page, NULL if not found. *//*返回文件页的控制块，如果没有找到则为NULL。*/
UNIV_INLINE
buf_block_t*
buf_page_hash_get(
/*==============*/
			/* out: block, NULL if not found */
	ulint	space,	/* in: space id */
	ulint	offset);/* in: offset of the page within space */
/***********************************************************************
Increments the pool clock by one and returns its new value. Remember that
in the 32 bit version the clock wraps around at 4 billion! */
/*将池时钟增加1并返回其新值。记住，在32位的版本中，时钟在40亿左右!*/
UNIV_INLINE
ulint
buf_pool_clock_tic(void);
/*====================*/
			/* out: new clock value */
/*************************************************************************
Gets the current length of the free list of buffer blocks. */
/*获取缓冲区块的空闲列表的当前长度。*/
ulint
buf_get_free_list_len(void);
/*=======================*/


			
/* The buffer control block structure */
/* 缓冲区控制块结构*/
struct buf_block_struct{

	/* 1. General fields */

	ulint		state;		/* state of the control block:
					BUF_BLOCK_NOT_USED, ... */
	byte*		frame;		/* pointer to buffer frame which
					is of size UNIV_PAGE_SIZE, and
					aligned to an address divisible by
					UNIV_PAGE_SIZE */ /*指向缓冲区帧的指针，其大小为UNIV_PAGE_SIZE，并对齐到一个能被UNIV_PAGE_SIZE整除的地址*/
	ulint		space;		/* space id of the page */ /*页面的空格id*/
	ulint		offset;		/* page number within the space */ /*页码在空格内*/
	ulint		lock_hash_val;	/* hashed value of the page address
					in the record lock hash table */ /*记录锁哈希表中页面地址的哈希值*/
	mutex_t*	lock_mutex;	/* mutex protecting the chain in the
					record lock hash table */ /*保护记录锁散列表中的链的互斥锁*/
	rw_lock_t	lock;		/* read-write lock of the buffer
					frame */ /*缓冲区帧的读写锁*/
	rw_lock_t	read_lock;	/* rw-lock reserved when a page read
					to the frame is requested; a thread
					can wait for this rw-lock if it wants
					to wait for the read to complete;
					the usual way is to wait for lock,
					but if the thread just wants a
					bufferfix and no latch on the page,
					then it can wait for this rw-lock */ /*读写锁(Rw-lock)在请求读到帧的页面时保留;如果一个线程想要等待读操作完成，
					它可以等待这个rw-lock;通常的方法是等待锁，但是如果线程只想要一个bufferfix而页面上没有闩锁，那么它可以等待这个rw-lock*/
	buf_block_t*	hash;		/* node used in chaining to the page
					hash table 用于链接到页哈希表的节点*/
	/* 2. Page flushing fields 2. 页面刷新字段 */

	UT_LIST_NODE_T(buf_block_t) flush_list;
					/* node of the modified, not yet
					flushed blocks list 已修改的尚未刷新块列表的节点*/
	dulint		newest_modification;
					/* log sequence number of the youngest
					modification to this block, zero if
					not modified 最新修改此块的日志序号，如果没有修改则为零*/
	dulint		oldest_modification;
					/* log sequence number of the START of
					the log entry written of the oldest
					modification to this block which has
					not yet been flushed on disk; zero if
					all modifications are on disk 对磁盘上尚未刷新的块进行的最老的修改所写入的日志条目的START日志序号;如果所有修改都在磁盘上，则为零*/
	ulint		flush_type;	/* if this block is currently being
					flushed to disk, this tells the
					flush_type: BUF_FLUSH_LRU or
					BUF_FLUSH_LIST 如果此块当前正在被刷新到磁盘，则告知flush_type: BUF_FLUSH_LRU或BUF_FLUSH_LIST*/

	/* 3. LRU replacement algorithm fields 3.LRU替换算法字段*/

	UT_LIST_NODE_T(buf_block_t) free;
					/* node of the free block list 空闲块列表的节点*/
	UT_LIST_NODE_T(buf_block_t) LRU;
					/* node of the LRU list LRU列表节点*/
	ulint		LRU_position;	/* value which monotonically
					decreases (or may stay constant if
					the block is in the old blocks) toward
					the end of the LRU list, if the pool
					ulint_clock has not wrapped around:
					NOTE that this value can only be used
					in heuristic algorithms, because of
					the possibility of a wrap-around! 如果池ulint_clock没有被包围，
					这个值就会单调地减少(或者如果块在旧的块中就会保持不变)到LRU列表的末尾:注意，这个值只能在启发式算法中使用，因为可能会被包围!*/
	ulint		freed_page_clock;/* the value of freed_page_clock
					buffer pool when this block was
					last time put to the head of the
					LRU list --freed_page_clock缓冲池的值，当这个block最后一次被放到LRU列表的头时*/
	ibool		old;		/* TRUE if the block is in the old
					blocks in the LRU list  如果该块在LRU列表中的旧块中，则为TRUE*/
	ibool		accessed;	/* TRUE if the page has been accessed
					while in the buffer pool: read-ahead
					may read in pages which have not been
					accessed yet --如果页在缓冲池中已被访问，则为TRUE:预读可以读取尚未被访问的页*/
	ulint		buf_fix_count;	/* count of how manyfold this block
					is currently bufferfixed --这个块当前被缓冲固定了多少倍*/
	ulint		io_fix;		/* if a read is pending to the frame,
					io_fix is BUF_IO_READ, in the case
					of a write BUF_IO_WRITE, otherwise 0 如果读是挂起的帧，io_fix是BUF_IO_READ，如果写是BUF_IO_WRITE，否则为0*/
	/* 4. Optimistic search field 4. 乐观的搜索框*/

	dulint		modify_clock;	/* this clock is incremented every
					time a pointer to a record on the
					page may become obsolete; this is
					used in the optimistic cursor
					positioning: if the modify clock has
					not changed, we know that the pointer
					is still valid; this field may be
					changed if the thread (1) owns the
					pool mutex and the page is not
					bufferfixed, or (2) the thread has an
					x-latch on the block 每当指向页面上某个记录的指针可能过时时，这个时钟就会加1;
					这在乐观游标定位中使用:如果修改后的时钟没有改变，我们知道指针仍然有效;
					如果线程(1)拥有池互斥锁，而页面没有缓冲固定，或者(2)线程在块上有一个x-latch，这个字段可能会改变*/

	/* 5. Hash search fields: NOTE that these fields are protected by
	btr_search_mutex 哈希搜索字段:注意这些字段受到btr_search_mutex的保护*/
	
	ulint		n_hash_helps;	/* counter which controls building
					of a new hash index for the page 计数器，用于控制为页面构建新的哈希索引*/
	ulint		n_fields;	/* recommended prefix length for hash
					search: number of full fields 哈希搜索推荐的前缀长度:完整字段的数量*/
	ulint		n_bytes;	/* recommended prefix: number of bytes
					in an incomplete field 推荐前缀:不完整字段的字节数*/
	ulint		side;		/* BTR_SEARCH_LEFT_SIDE or
					BTR_SEARCH_RIGHT_SIDE, depending on
					whether the leftmost record of several
					records with the same prefix should be
					indexed in the hash index -- BTR_SEARCH_LEFT_SIDE或BTR_SEARCH_RIGHT_SIDE，
					取决于是否应该在哈希索引中索引几个具有相同前缀的记录的最左边的记录*/
	ibool		is_hashed;	/* TRUE if hash index has already been
					built on this page; note that it does
					not guarantee that the index is
					complete, though: there may have been
					hash collisions, record deletions,
					etc. */ /*如果已在该页上建立哈希索引，则为TRUE;请注意，它不能保证索引是完整的，但是:可能存在散列冲突、记录删除等。*/
	ulint		curr_n_fields;	/* prefix length for hash indexing:
					number of full fields 哈希索引的前缀长度:完整字段的数量*/
	ulint		curr_n_bytes;	/* number of bytes in hash indexing 哈希索引中的字节数*/
	ulint		curr_side;	/* BTR_SEARCH_LEFT_SIDE or
					BTR_SEARCH_RIGHT_SIDE in hash
					indexing --BTR_SEARCH_LEFT_SIDE或BTR_SEARCH_RIGHT_SIDE散列索引*/
	/* 6. Debug fields 调试领域*/

	rw_lock_t	debug_latch;	/* in the debug version, each thread
					which bufferfixes the block acquires
					an s-latch here; so we can use the
					debug utilities in sync0rw 在调试版本中，每个缓冲区修复块的线程在这里获得一个s-latch;因此，我们可以使用sync0rw中的调试工具*/
        ibool           file_page_was_freed;
                                        /* this is set to TRUE when fsp
                                        frees a page in buffer pool 当fsp释放缓冲池中的页面时，这个值设置为TRUE*/
};

/* The buffer pool structure. NOTE! The definition appears here only for
other modules of this directory (buf) to see it. Do not use from outside! */
/*缓冲池结构。注意!这里的定义仅供此目录(buf)的其他模块查看。请勿从外部使用!*/
struct buf_pool_struct{

	/* 1. General fields */
    /* 1. 通用字段*/
	mutex_t		mutex;		/* mutex protecting the buffer pool
					struct and control blocks, except the
					read-write lock in them */ /*保护缓冲池结构和控制块(除了其中的读写锁)的互斥锁*/
	byte*		frame_mem;	/* pointer to the memory area which
					was allocated for the frames */ /*指向分配给帧的内存区域的指针*/
	byte*		frame_zero;	/* pointer to the first buffer frame:
					this may differ from frame_mem, because
					this is aligned by the frame size */ /*指向第一个缓冲区帧的指针:它可能不同于frame_mem，因为它是按帧大小对齐的*/
	byte*		high_end;	/* pointer to the end of the
					buffer pool */ /*指向缓冲池末端的指针*/
	buf_block_t*	blocks;		/* array of buffer control blocks */ /*缓冲区控制块的数组*/
	ulint		max_size;	/* number of control blocks ==
					maximum pool size in pages */ /*控制块数量==页面中的最大池大小*/
	ulint		curr_size;	/* current pool size in pages */ /*页面中的当前池大小*/
	hash_table_t*	page_hash;	/* hash table of the file pages */ /*文件页的哈希表*/

	ulint		n_pend_reads;	/* number of pending read operations */ /*挂起的读操作个数*/

	time_t		last_printout_time; /* when buf_print was last time
					called */ /*上次调用buf_print时*/
	ulint		n_pages_read;	/* number read operations */ /*读取操作数*/
	ulint		n_pages_written;/* number write operations */ /*写入操作数*/
	ulint		n_pages_created;/* number of pages created in the pool
					with no read */ /*池中创建的没有读取的页面数*/
	ulint		n_page_gets;	/* number of page gets performed;
					also successful seraches through
					the adaptive hash index are
					counted as page gets; this field
					is NOT protected by the buffer
					pool mutex */ /*执行的页面数;通过自适应哈希索引成功的搜索也被计算为页面获取;这个字段不受缓冲池互斥锁的保护*/
	ulint		n_page_gets_old;/* n_page_gets when buf_print was
					last time called: used to calculate
					hit rate */ /*上次调用buf_print时的N_page_gets:用于计算命中率*/
	ulint		n_pages_read_old;/* n_pages_read when buf_print was
					last time called */ /*上次调用buf_print时的N_pages_read*/
	ulint		n_pages_written_old;/* number write operations */ /*写操作数*/
	ulint		n_pages_created_old;/* number of pages created in
					the pool with no read */ /*池中创建的没有读取的页面数*/
	/* 2. Page flushing algorithm fields */
    /*2. 页面刷新算法字段*/
	UT_LIST_BASE_NODE_T(buf_block_t) flush_list;
					/* base node of the modified block
					list */ /*修改后的块列表的基本节点*/
	ibool		init_flush[BUF_FLUSH_LIST + 1];
					/* this is TRUE when a flush of the
					given type is being initialized */ /*当初始化给定类型的刷新时，此值为TRUE*/
	ulint		n_flush[BUF_FLUSH_LIST + 1];
					/* this is the number of pending
					writes in the given flush type */ /*这是给定刷新类型中挂起的写操作的数量*/
	os_event_t	no_flush[BUF_FLUSH_LIST + 1];
					/* this is in the set state when there
					is no flush batch of the given type
					running */ /*当没有运行给定类型的刷新批处理时，将处于设置状态*/
	ulint		ulint_clock;	/* a sequence number used to count
					time. NOTE! This counter wraps
					around at 4 billion (if ulint ==
					32 bits)! */ /*用于计算时间的序列号。注意!这个计数器在40亿左右包装(如果ulint ==32位)!*/
	ulint		freed_page_clock;/* a sequence number used to count the
					number of buffer blocks removed from
					the end of the LRU list; NOTE that
					this counter may wrap around at 4
					billion! */ /*一个序列号，用于计算从LRU列表末尾移除的缓冲区块的数量;注意，这个计数器可能在40亿左右!*/
	ulint		LRU_flush_ended;/* when an LRU flush ends for a page,
					this is incremented by one; this is
					set to zero when a buffer block is
					allocated */  /*当一个页面的LRU刷新结束时，这个值加1;当分配缓冲区块时，该值被设置为0*/

	/* 3. LRU replacement algorithm fields */
    /* 3.LRU替换算法字段*/
	UT_LIST_BASE_NODE_T(buf_block_t) free;
					/* base node of the free block list */ /*空闲块列表的基本节点*/
	UT_LIST_BASE_NODE_T(buf_block_t) LRU;
					/* base node of the LRU list */ /*LRU列表的基本节点*/
	buf_block_t*	LRU_old; 	/* pointer to the about 3/8 oldest
					blocks in the LRU list; NULL if LRU
					length less than BUF_LRU_OLD_MIN_LEN */ /*指向LRU列表中3/8个最老的块的指针;如果LRU长度小于BUF_LRU_OLD_MIN_LEN，则为NULL*/
	ulint		LRU_old_len;	/* length of the LRU list from
					the block to which LRU_old points
					onward, including that block;
					see buf0lru.c for the restrictions
					on this value; not defined if
					LRU_old == NULL */ /*LRU链表从LRU_old所指向的块开始的长度，包括那个块，这个值的限制见buf0lru.c;如果LRU_old == NULL，则没有定义*/
};

/* States of a control block */ /*控制块的状态*/
#define	BUF_BLOCK_NOT_USED	211	/* is in the free list */ /*是否在免费列表中*/
#define BUF_BLOCK_READY_FOR_USE	212	/* when buf_get_free_block returns
					a block, it is in this state */ /*当buf_get_free_block返回一个block时，它就处于这种状态*/
#define	BUF_BLOCK_FILE_PAGE	213	/* contains a buffered file page */ /*包含一个缓冲的文件页*/
#define	BUF_BLOCK_MEMORY	214	/* contains some main memory object */ /*包含一些主内存对象*/
#define BUF_BLOCK_REMOVE_HASH	215	/* hash index should be removed
					before putting to the free list */ /*散列索引应该在放入空闲列表之前被删除*/

/* Io_fix states of a control block; these must be != 0 */ /*控制块的Io_fix状态;它们一定是!= 0*/
#define BUF_IO_READ		561
#define BUF_IO_WRITE		562

/************************************************************************
Let us list the consistency conditions for different control block states.
让我们列出不同控制块状态的一致性条件。
NOT_USED:	is in free list, not in LRU list, not in flush list, nor
		page hash table 在空闲列表中，不在LRU列表中，不在刷新列表中，也不在页哈希表中
READY_FOR_USE:	is not in free list, LRU list, or flush list, nor page
		hash table  不是在空闲列表，LRU列表，刷新列表，或页哈希表
MEMORY:		is not in free list, LRU list, or flush list, nor page
		hash table  不是在空闲列表，LRU列表，刷新列表，或页哈希表
FILE_PAGE:	space and offset are defined, is in page hash table
		if io_fix == BUF_IO_WRITE,  如果io_fix == BUF_IO_WRITE，则在页哈希表中定义了空间和偏移量
			pool: no_flush[block->flush_type] is in reset state,
			pool: n_flush[block->flush_type] > 0			
		
		(1) if buf_fix_count == 0, then
			is in LRU list, not in free list
			is in flush list,
				if and only if oldest_modification > 0
			is x-locked,
				if and only if io_fix == BUF_IO_READ
			is s-locked,
				if and only if io_fix == BUF_IO_WRITE
						
		(2) if buf_fix_count > 0, then
			is not in LRU list, not in free list
			is in flush list,
				if and only if oldest_modification > 0
			if io_fix == BUF_IO_READ,		
				is x-locked
			if io_fix == BUF_IO_WRITE,
				is s-locked
			
State transitions:

NOT_USED => READY_FOR_USE
READY_FOR_USE => MEMORY
READY_FOR_USE => FILE_PAGE
MEMORY => NOT_USED
FILE_PAGE => NOT_USED	NOTE: This transition is allowed if and only if 
				(1) buf_fix_count == 0,
				(2) oldest_modification == 0, and
				(3) io_fix == 0.
*/

#ifndef UNIV_NONINL
#include "buf0buf.ic"
#endif

#endif
