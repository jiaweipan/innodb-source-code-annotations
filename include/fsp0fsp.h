/******************************************************
File space management

(c) 1995 Innobase Oy

Created 12/18/1995 Heikki Tuuri
*******************************************************/
/*文件空间管理*/
#ifndef fsp0fsp_h
#define fsp0fsp_h

#include "univ.i"

#include "mtr0mtr.h"
#include "fut0lst.h"
#include "ut0byte.h"
#include "page0types.h"

/* If records are inserted in order, there are the following
flags to tell this (their type is made byte for the compiler
to warn if direction and hint parameters are switched in
fseg_alloc_free_page): */
/*如果按顺序插入记录，则有以下标志说明这一点
（它们的类型为byte，以便编译器在fseg_alloc_free_page页中切换方向和提示参数时发出警告）：*/
#define	FSP_UP		((byte)111)	/* alphabetically upwards */ /*按字母顺序向上 */
#define	FSP_DOWN	((byte)112)	/* alphabetically downwards */ /*按字母顺序向下*/
#define	FSP_NO_DIR	((byte)113)	/* no order */ /*无序*/

/* File space extent size in pages */ /*文件空间范围大小（以页为单位）*/
#define	FSP_EXTENT_SIZE		64

/* On a page of any file segment, data may be put starting from this offset: */
/* 在任何文件段的页面上，可以从此偏移量开始放置数据：*/
#define FSEG_PAGE_DATA		FIL_PAGE_DATA

/* File segment header which points to the inode describing the file segment */
/* 文件段头，指向描述文件段的inode*/
typedef	byte	fseg_header_t;

#define FSEG_HDR_SPACE		0	/* space id of the inode */ /*inode的空间id*/
#define FSEG_HDR_PAGE_NO	4	/* page number of the inode */ /*inode的页号*/
#define FSEG_HDR_OFFSET		8	/* byte offset of the inode */ /*inode的字节偏移量*/

#define FSEG_HEADER_SIZE	10 /**/

/**************************************************************************
Initializes the file space system. */
/*初始化文件空间系统*/
void
fsp_init(void);
/*==========*/
/**************************************************************************
Initializes the space header of a new created space. */
/*初始化新创建空间的空间标头。 */
void
fsp_header_init(
/*============*/
	ulint	space,	/* in: space id */
	ulint	size,	/* in: current size in blocks */
	mtr_t*	mtr);	/* in: mini-transaction handle */	
/**************************************************************************
Increases the space size field of a space. */
/*增加空间的“空间大小”字段。*/
void
fsp_header_inc_size(
/*================*/
	ulint	space,	/* in: space id */
	ulint	size_inc,/* in: size increment in pages */
	mtr_t*	mtr);	/* in: mini-transaction handle */	
/**************************************************************************
Creates a new segment. */
/*创建新段。*/
page_t*
fseg_create(
/*========*/
			/* out: the page where the segment header is placed,
			x-latched, NULL if could not create segment
			because of lack of space */
	ulint	space,	/* in: space id */
	ulint	page,	/* in: page where the segment header is placed: if
			this is != 0, the page must belong to another segment,
			if this is 0, a new page will be allocated and it
			will belong to the created segment */
	ulint	byte_offset, /* in: byte offset of the created segment header
			on the page */
	mtr_t*	mtr);	/* in: mtr */
/**************************************************************************
Creates a new segment. */
/*创建新段。*/
page_t*
fseg_create_general(
/*================*/
			/* out: the page where the segment header is placed,
			x-latched, NULL if could not create segment
			because of lack of space */
	ulint	space,	/* in: space id */
	ulint	page,	/* in: page where the segment header is placed: if
			this is != 0, the page must belong to another segment,
			if this is 0, a new page will be allocated and it
			will belong to the created segment */
	ulint	byte_offset, /* in: byte offset of the created segment header
			on the page */
	ibool	has_done_reservation, /* in: TRUE if the caller has
			already done the reservation for the pages
			with fsp_reserve_free_extents (at least 2 extents:
			one for the inode and, then there other for the
			segment) is no need to do the check for this
			individual operation */
	mtr_t*	mtr);	/* in: mtr */
/**************************************************************************
Calculates the number of pages reserved by a segment, and how many pages are
currently used. */
/*计算段保留的页数以及当前使用的页数。*/
ulint
fseg_n_reserved_pages(
/*==================*/
				/* out: number of reserved pages */
	fseg_header_t* 	header,	/* in: segment header */
	ulint*		used,	/* out: number of pages used (<= reserved) */
	mtr_t*		mtr);	/* in: mtr handle */
/**************************************************************************
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize
file space fragmentation. */
/*从段中分配单个空闲页。此函数实现智能分配策略，该策略尝试最小化文件空间碎片。*/
ulint
fseg_alloc_free_page(
/*=================*/
				/* out: the allocated page offset
				FIL_NULL if no page could be allocated */
	fseg_header_t*	seg_header, /* in: segment header */
	ulint		hint,	/* in: hint of which page would be desirable */ /*暗示哪一页是可取的*/
	byte		direction, /* in: if the new page is needed because
				of an index page split, and records are
				inserted there in order, into which
				direction they go alphabetically: FSP_DOWN,
				FSP_UP, FSP_NO_DIR */ 
				/*如果由于索引页拆分而需要新页，
				并且记录按顺序插入，
				按字母顺序插入到哪个方向：FSP_DOWN、FSP_UP、FSP_NO_DIR */
	mtr_t*		mtr);	/* in: mtr handle */
/**************************************************************************
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize file space
fragmentation. */
/*从段中分配单个空闲页。此函数实现智能分配策略，该策略尝试最小化文件空间碎片。 */
ulint
fseg_alloc_free_page_general(
/*=========================*/
				/* out: allocated page offset, FIL_NULL if no
				page could be allocated */
	fseg_header_t*	seg_header,/* in: segment header */
	ulint		hint,	/* in: hint of which page would be desirable */
	byte		direction,/* in: if the new page is needed because
				of an index page split, and records are
				inserted there in order, into which
				direction they go alphabetically: FSP_DOWN,
				FSP_UP, FSP_NO_DIR */
	ibool		has_done_reservation, /* in: TRUE if the caller has
				already done the reservation for the page
				with fsp_reserve_free_extents, then there
				is no need to do the check for this individual
				page */
				/*如果调用者已经用fsp_reserve_free_extents对页面进行了保留，
				那么就不需要对这个单独的页面进行检查*/
	mtr_t*		mtr);	/* in: mtr handle */
/**************************************************************************
Reserves free pages from a tablespace. All mini-transactions which may
use several pages from the tablespace should call this function beforehand
and reserve enough free extents so that they certainly will be able
to do their operation, like a B-tree page split, fully. Reservations
must be released with function fil_space_release_free_extents!

The alloc_type below has the following meaning: FSP_NORMAL means an
operation which will probably result in more space usage, like an
insert in a B-tree; FSP_UNDO means allocation to undo logs: if we are
deleting rows, then this allocation will in the long run result in
less space usage (after a purge); FSP_CLEANING means allocation done
in a physical record delete (like in a purge) or other cleaning operation
which will result in less space usage in the long run. We prefer the latter
two types of allocation: when space is scarce, FSP_NORMAL allocations
will not succeed, but the latter two allocations will succeed, if possible.
The purpose is to avoid dead end where the database is full but the
user cannot free any space because these freeing operations temporarily
reserve some space. */ 
/*从表空间保留空闲页。所有可能使用表空间中多个页面的小型事务都应该事先调用此函数，
并保留足够的空闲扩展数据块，以便它们肯定能够完全执行其操作（如B树页面分割）。
必须使用功能fil_space_release_free_extents释放预订！
下面的alloc_type具有以下含义：
FSP_NORMAL表示可能会导致更多空间使用的操作，如B树中的insert；
FSP_UNDO表示分配以撤消日志：如果我们删除行，那么从长远来看，这种分配将导致更少的空间使用（在清除之后）；
FSP_CLEANING清理是指在物理记录删除（如清除）或其他清理操作中进行的分配，从长远来看，这将减少空间使用。
我们更喜欢后两种分配：当空间不足时，FSP_NORMAL分配将不会成功，但后两种分配将成功，如果有可能。
那个其目的是避免数据库已满但用户无法释放任何空间的死角，因为这些释放操作暂时保留了一些空间*/
ibool
fsp_reserve_free_extents(
/*=====================*/
			/* out: TRUE if we were able to make the reservation */
	ulint	space,	/* in: space id */
	ulint	n_ext,	/* in: number of extents to reserve */
	ulint	alloc_type,/* in: FSP_NORMAL, FSP_UNDO, or FSP_CLEANING */
	mtr_t*	mtr);	/* in: mtr */
/**************************************************************************
This function should be used to get information on how much we still
will be able to insert new data to the database without running out the
tablespace. Only free extents are taken into account and we also subtract
the safety margin required by the above function fsp_reserve_free_extents. */
/*这个函数应该用来获取关于在不耗尽表空间的情况下，我们还能向数据库插入多少新数据的信息。
只考虑自由范围，我们还减去上述函数fsp_reserve_free_extents所需的安全裕度。*/
ulint
fsp_get_available_space_in_free_extents(
/*====================================*/
			/* out: available space in kB */
	ulint	space);	/* in: space id */
/**************************************************************************
Frees a single page of a segment. */
/*释放段的单个页面。 */
void
fseg_free_page(
/*===========*/
	fseg_header_t*	seg_header, /* in: segment header */
	ulint		space,	/* in: space id */
	ulint		page,	/* in: page offset */
	mtr_t*		mtr);	/* in: mtr handle */
/***********************************************************************
Frees a segment. The freeing is performed in several mini-transactions,
so that there is no danger of bufferfixing too many buffer pages. */
/*释放段。释放是在几个小事务中执行的，这样就不会有修复过多缓冲页的危险。*/
void
fseg_free(
/*======*/
	ulint	space,	/* in: space id */
	ulint	page_no,/* in: page number where the segment header is
			placed */
	ulint	offset);/* in: byte offset of the segment header on that
			page */
/**************************************************************************
Frees part of a segment. This function can be used to free a segment
by repeatedly calling this function in different mini-transactions.
Doing the freeing in a single mini-transaction might result in
too big a mini-transaction. */
/*释放段的一部分。此函数可以通过在不同的mini中重复调用此函数来释放段-交易。
做什么单个迷你事务中的释放可能会导致迷你事务过大。 */
ibool
fseg_free_step(
/*===========*/
				/* out: TRUE if freeing completed */
	fseg_header_t*	header,	/* in, own: segment header; NOTE: if the header
				resides on the first page of the frag list
				of the segment, this pointer becomes obsolete
				after the last freeing step */
				/*段标题；注意：如果标题位于段的frag列表的第一页，则在最后一个释放步骤之后，此指针将被废弃*/
	mtr_t*		mtr);	/* in: mtr */
/**************************************************************************
Frees part of a segment. Differs from fseg_free_step because this function
leaves the header page unfreed. */
/*释放段的一部分。与fseg_free_step不同，因为此函数使页眉页保持未展开状态。 */
ibool
fseg_free_step_not_header(
/*======================*/
				/* out: TRUE if freeing completed, except the
				header page */
	fseg_header_t*	header,	/* in: segment header which must reside on
				the first fragment page of the segment */
				/*必须位于段的第一个片段页上的段头*/
	mtr_t*		mtr);	/* in: mtr */
/***************************************************************************
Checks if a page address is an extent descriptor page address. */
/*检查页地址是否是扩展描述符页地址。*/
UNIV_INLINE
ibool
fsp_descr_page(
/*===========*/
			/* out: TRUE if a descriptor page */
	ulint	page_no);/* in: page number */
/***************************************************************
Parses a redo log record of a file page init. */
/*解析文件页初始化的重做日志记录。*/
byte*
fsp_parse_init_file_page(
/*=====================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page);	/* in: page or NULL */
/***********************************************************************
Validates the file space system and its segments. */
/*验证文件空间系统及其段。*/
ibool
fsp_validate(
/*=========*/
			/* out: TRUE if ok */
	ulint	space);	/* in: space id */
/***********************************************************************
Prints info of a file space. */
/*打印文件空间的信息。*/
void
fsp_print(
/*======*/
	ulint	space);	/* in: space id */
/***********************************************************************
Validates a segment. */
/*验证段。*/
ibool
fseg_validate(
/*==========*/
				/* out: TRUE if ok */
	fseg_header_t*	header, /* in: segment header */
	mtr_t*		mtr2);	/* in: mtr */
/***********************************************************************
Writes info of a segment. */
/*写入段的信息。*/
void
fseg_print(
/*=======*/
	fseg_header_t*	header, /* in: segment header */
	mtr_t*		mtr);	/* in: mtr */

/* Flags for fsp_reserve_free_extents */
#define FSP_NORMAL	1000000
#define	FSP_UNDO	2000000
#define FSP_CLEANING	3000000

/* Number of pages described in a single descriptor page: currently each page
description takes less than 1 byte; a descriptor page is repeated every
this many file pages */
/*单个描述符页面中描述的页面数：当前每个页面描述占用的字节数不到1个；描述符页面每隔这么多个文件页面重复一次*/
#define XDES_DESCRIBED_PER_PAGE		UNIV_PAGE_SIZE

/* The space low address page map, and also offsets for extent descriptor and
bitmap pages which are repeated always after XDES_DESCRIBED_PER_PAGE more
pages: */
/*空间低地址页映射，以及范围描述符和位图页的偏移量，这些页总是在 XDES_DESCRIBED_PER_PAGE more pages之后重复：*/
/*--------------------------------------*/
#define FSP_XDES_OFFSET			0
#define FSP_IBUF_BITMAP_OFFSET		1
				/* The ibuf bitmap pages are the ones whose
				page number is the number above plus a
				multiple of XDES_DESCRIBED_PER_PAGE */
				/*ibuf位图页的页码是上面的数字加上每页所描述的XDES的倍数*/
#define FSP_FIRST_INODE_PAGE_NO		2
#define FSP_IBUF_HEADER_PAGE_NO		3
#define FSP_IBUF_TREE_ROOT_PAGE_NO	4
				/* The ibuf tree root page number in each
				tablespace; its fseg inode is on the page
				number FSP_FIRST_INODE_PAGE_NO */
				/*每个表空间中的ibuf树根页码；其fseg inode位于页码FSP_FIRST_INODE_PAGE_NO上*/
#define FSP_TRX_SYS_PAGE_NO		5
#define	FSP_FIRST_RSEG_PAGE_NO		6
#define FSP_DICT_HDR_PAGE_NO		7
/*--------------------------------------*/

#ifndef UNIV_NONINL
#include "fsp0fsp.ic"
#endif

#endif
