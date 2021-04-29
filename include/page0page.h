/******************************************************
Index page routines

(c) 1994-1996 Innobase Oy

Created 2/2/1994 Heikki Tuuri
*******************************************************/
/*索引页的例程*/
#ifndef page0page_h
#define page0page_h

#include "univ.i"

#include "page0types.h"
#include "fil0fil.h"
#include "buf0buf.h"
#include "data0data.h"
#include "dict0dict.h"
#include "rem0rec.h"
#include "fsp0fsp.h"
#include "mtr0mtr.h"

#ifdef UNIV_MATERIALIZE
#undef UNIV_INLINE
#define UNIV_INLINE
#endif

/*			PAGE HEADER
			===========

Index page header starts at the first offset left free by the FIL-module */
/*索引页头从FIL-module留下的第一个空闲偏移量开始*/
typedef	byte		page_header_t;

#define	PAGE_HEADER	FSEG_PAGE_DATA	/* index page header starts at this
				offset */ /*索引页头从这个偏移量开始*/
/*-----------------------------*/
#define PAGE_N_DIR_SLOTS 0	/* number of slots in page directory */ /*页目录中的槽数*/
#define	PAGE_HEAP_TOP	 2	/* pointer to record heap top */ /*指向记录堆顶部的指针*/
#define	PAGE_N_HEAP	 4	/* number of records in the heap */ /*堆中的记录数*/
#define	PAGE_FREE	 6	/* pointer to start of page free record list */ /*指向空闲页记录列表的起始点*/
#define	PAGE_GARBAGE	 8	/* number of bytes in deleted records */ /*删除记录的字节数*/
#define	PAGE_LAST_INSERT 10	/* pointer to the last inserted record, or
				NULL if this info has been reset by a delete,
				for example */ /*指针指向最后插入的记录，或者NULL，如果该信息被删除重置，例如*/
#define	PAGE_DIRECTION	 12	/* last insert direction: PAGE_LEFT, ... */ /*最后插入方向:PAGE_LEFT，…*/
#define	PAGE_N_DIRECTION 14	/* number of consecutive inserts to the same
				direction */ /*同一方向的连续插入数*/
#define	PAGE_N_RECS	 16	/* number of user records on the page */ /*页面中用户记录个数*/
#define PAGE_MAX_TRX_ID	 18	/* highest id of a trx which may have modified
				a record on the page; a dulint; defined only
				in secondary indexes; specifically, not in an
				ibuf tree; NOTE: this may be modified only
				when the thread has an x-latch to the page,
				and ALSO an x-latch to btr_search_latch
				if there is a hash index to the page! */ /*可能修改了页面上记录的TRX的最高id;dulint;仅在二级索引中定义;
				具体来说，不是在ibuf树中;注意:只有当线程对页面有一个x-闩锁时，才可以修改它，如果页面有一个哈希索引，
				也可以对btr_search_latch有一个x-闩锁。*/
#define PAGE_HEADER_PRIV_END 26	/* end of private data structure of the page
				header which are set in a page create */ /*在页面创建中设置的页头的私有数据结构的结束*/
/*----*/
#define	PAGE_LEVEL	 26	/* level of the node in an index tree; the
				leaf level is the level 0 */ /*索引树中节点的级别;叶级是级别0*/
#define	PAGE_INDEX_ID	 28	/* index id where the page belongs */ /*页面所属的索引id*/
#define PAGE_BTR_SEG_LEAF 36	/* file segment header for the leaf pages in
				a B-tree: defined only on the root page of a
				B-tree, but not in the root of an ibuf tree */ /*b -树中叶子页的文件段头:仅在b -树的根页面上定义，但不在ibuf树的根页面上定义*/
#define PAGE_BTR_IBUF_FREE_LIST	PAGE_BTR_SEG_LEAF
#define PAGE_BTR_IBUF_FREE_LIST_NODE PAGE_BTR_SEG_LEAF
				/* in the place of PAGE_BTR_SEG_LEAF and _TOP
				there is a free list base node if the page is
				the root page of an ibuf tree, and at the same
				place is the free list node if the page is in
				a free list */  /*如果页面是ibuf树的根页面，则在PAGE_BTR_SEG_LEAF和_TOP的位置上有一个空闲列表基节点，
				如果页面在空闲列表中，则在相同的位置上有一个空闲列表节点*/
#define PAGE_BTR_SEG_TOP (36 + FSEG_HEADER_SIZE)
				/* file segment header for the non-leaf pages
				in a B-tree: defined only on the root page of
				a B-tree, but not in the root of an ibuf
				tree */ /*b -树中非叶页的文件段头:仅在b -树的根页上定义，但不在ibuf树的根中定义*/
/*----*/
#define PAGE_DATA	(PAGE_HEADER + 36 + 2 * FSEG_HEADER_SIZE)
				/* start of data on the page */ /*页上数据的开始*/

#define PAGE_INFIMUM	(PAGE_DATA + 1 + REC_N_EXTRA_BYTES)
				/* offset of the page infimum record on the
				page */ /*页下记录在页上的偏移量*/
#define PAGE_SUPREMUM	(PAGE_DATA + 2 + 2 * REC_N_EXTRA_BYTES + 8)
				/* offset of the page supremum record on the
				page */ /*页上的页上记录的偏移量*/
#define PAGE_SUPREMUM_END (PAGE_SUPREMUM + 9)
				/* offset of the page supremum record end on
				the page */ /*页上记录结束的偏移量*/
/*-----------------------------*/

/* Directions of cursor movement */ /*光标移动方向*/
#define	PAGE_LEFT		1
#define	PAGE_RIGHT		2
#define	PAGE_SAME_REC		3
#define	PAGE_SAME_PAGE		4
#define	PAGE_NO_DIRECTION	5

/*			PAGE DIRECTORY 页目录
			==============
*/

typedef	byte			page_dir_slot_t;
typedef page_dir_slot_t		page_dir_t;

/* Offset of the directory start down from the page end. We call the
slot with the highest file address directory start, as it points to 
the first record in the list of records. */
/*从页面结束开始的目录的偏移量。我们将具有最高文件地址目录的槽称为start，因为它指向记录列表中的第一个记录。*/
#define	PAGE_DIR		FIL_PAGE_DATA_END

/* We define a slot in the page directory as two bytes */ /*我们将页目录中的槽定义为两个字节*/
#define	PAGE_DIR_SLOT_SIZE	2

/* The offset of the physically lower end of the directory, counted from
page en, dwhen the page is empty */ /*当页为空时，从页结束算起的目录物理下端的偏移量*/
#define PAGE_EMPTY_DIR_START	(PAGE_DIR + 2 * PAGE_DIR_SLOT_SIZE)

/* The maximum and minimum number of records owned by a directory slot. The
number may drop below the minimum in the first and the last slot in the 
directory. */
/*目录槽位所拥有的最大和最小记录数。该数字可能下降到目录中第一个和最后一个插槽的最小值以下。*/
#define PAGE_DIR_SLOT_MAX_N_OWNED	8
#define	PAGE_DIR_SLOT_MIN_N_OWNED	4

/*****************************************************************
Returns the max trx id field value. */ /*返回最大trx id字段值。*/
UNIV_INLINE
dulint
page_get_max_trx_id(
/*================*/
	page_t*	page);	/* in: page */
/*****************************************************************
Sets the max trx id field value. */
/*设置最大trx id字段值。*/
void
page_set_max_trx_id(
/*================*/
	page_t*	page,	/* in: page */
	dulint	trx_id);/* in: transaction id */
/*****************************************************************
Sets the max trx id field value if trx_id is bigger than the previous
value. */ /*当trx_id大于前一个值时，设置trx id字段的最大值。*/
UNIV_INLINE
void
page_update_max_trx_id(
/*===================*/
	page_t*	page,	/* in: page */
	dulint	trx_id);	/* in: transaction id */
/*****************************************************************
Reads the given header field. */ /*读取给定的报头字段。*/
UNIV_INLINE
ulint
page_header_get_field(
/*==================*/
	page_t*	page,	/* in: page */
	ulint	field);	/* in: PAGE_N_DIR_SLOTS, ... */
/*****************************************************************
Sets the given header field. */ /*设置给定的报头字段。*/
UNIV_INLINE
void
page_header_set_field(
/*==================*/
	page_t*	page,	/* in: page */
	ulint	field,	/* in: PAGE_N_DIR_SLOTS, ... */
	ulint	val);	/* in: value */
/*****************************************************************
Returns the pointer stored in the given header field. */ /*返回存储在给定报头字段中的指针。*/
UNIV_INLINE
byte*
page_header_get_ptr(
/*================*/
			/* out: pointer or NULL */
	page_t*	page,	/* in: page */
	ulint	field);	/* in: PAGE_FREE, ... */
/*****************************************************************
Sets the pointer stored in the given header field. */ /*设置存储在给定报头字段中的指针。*/
UNIV_INLINE
void
page_header_set_ptr(
/*================*/
	page_t*	page,	/* in: page */
	ulint	field,	/* in: PAGE_FREE, ... */
	byte*	ptr);	/* in: pointer or NULL*/
/*****************************************************************
Resets the last insert info field in the page header. Writes to mlog
about this operation. */ /*重置页眉中的最后一个插入信息字段。将此操作写入mlog。*/
UNIV_INLINE
void
page_header_reset_last_insert(
/*==========================*/
	page_t*	page,	/* in: page */
	mtr_t*	mtr);	/* in: mtr */
/****************************************************************
Gets the first record on the page. */ /*获取页上的第一个记录。*/
UNIV_INLINE
rec_t*
page_get_infimum_rec(
/*=================*/
			/* out: the first record in record list */
	page_t*	page);	/* in: page which must have record(s) */
/****************************************************************
Gets the last record on the page. */ /*获取页面上的最后一条记录。*/
UNIV_INLINE
rec_t*
page_get_supremum_rec(
/*==================*/
			/* out: the last record in record list */
	page_t*	page);	/* in: page which must have record(s) */
/****************************************************************
Returns the middle record of record list. If there are an even number
of records in the list, returns the first record of upper half-list. */
/*返回记录列表的中间记录。如果列表中有偶数条记录，则返回上半个列表的第一个记录。*/
rec_t*
page_get_middle_rec(
/*================*/
			/* out: middle record */
	page_t*	page);	/* in: page */
/*****************************************************************
Compares a data tuple to a physical record. Differs from the function
cmp_dtuple_rec_with_match in the way that the record must reside on an
index page, and also page infimum and supremum records can be given in
the parameter rec. These are considered as the negative infinity and
the positive infinity in the alphabetical order. */
/*将数据元组与物理记录进行比较。与cmp_dtuple_rec_with_match函数不同的是，
该记录必须驻留在一个索引页上，而且页面下限值和上限值可以在参数rec中给出。
它们被认为是按字母顺序的负无穷大和正无穷大。*/
UNIV_INLINE
int
page_cmp_dtuple_rec_with_match(
/*===========================*/	
				/* out: 1, 0, -1, if dtuple is greater, equal, 
				less than rec, respectively, when only the 
				common first fields are compared */
	dtuple_t*	dtuple,	/* in: data tuple */
	rec_t*		rec,	/* in: physical record on a page; may also 
				be page infimum or supremum, in which case 
				matched-parameter values below are not 
				affected */
	ulint*	 	matched_fields, /* in/out: number of already completely 
				matched fields; when function returns
				contains the value for current comparison */
	ulint*	  	matched_bytes); /* in/out: number of already matched 
				bytes within the first field not completely
				matched; when function returns contains the
				value for current comparison */
/*****************************************************************
Gets the number of user records on page (the infimum and supremum records
are not user records). */ /*获取页面上用户记录的数量(下限值和上限值记录不是用户记录)。*/
UNIV_INLINE
ulint
page_get_n_recs(
/*============*/
			/* out: number of user records */
	page_t*	page);	/* in: index page */
/*******************************************************************
Returns the number of records before the given record in chain.
The number includes infimum and supremum records. */
/*返回链中给定记录之前的记录数。该数字包括下限值和上限值记录。*/
ulint
page_rec_get_n_recs_before(
/*=======================*/
			/* out: number of records */
	rec_t*	rec);	/* in: the physical record */
/*****************************************************************
Gets the number of dir slots in directory. */ /*获取目录中dir插槽的数量。*/
UNIV_INLINE
ulint
page_dir_get_n_slots(
/*=================*/
			/* out: number of slots */
	page_t*	page);	/* in: index page */
/*****************************************************************
Gets pointer to nth directory slot. */ /*获取指向第n个目录槽的指针。*/
UNIV_INLINE
page_dir_slot_t*
page_dir_get_nth_slot(
/*==================*/
			/* out: pointer to dir slot */
	page_t*	page,	/* in: index page */
	ulint	n);	/* in: position */
/******************************************************************
Used to check the consistency of a record on a page. */ /*用于检查某页上记录的一致性。*/
UNIV_INLINE
ibool
page_rec_check(
/*===========*/
			/* out: TRUE if succeed */
	rec_t*	rec);	/* in: record */
/*******************************************************************
Gets the record pointed to by a directory slot. */ /*获取由目录槽指向的记录。*/
UNIV_INLINE
rec_t*
page_dir_slot_get_rec(
/*==================*/
					/* out: pointer to record */
	page_dir_slot_t*	slot);	/* in: directory slot */
/*******************************************************************
This is used to set the record offset in a directory slot. */ /*这用于设置目录槽中的记录偏移量。*/
UNIV_INLINE
void
page_dir_slot_set_rec(
/*==================*/
	page_dir_slot_t* slot,	/* in: directory slot */
	rec_t*		 rec);	/* in: record on the page */
/*******************************************************************
Gets the number of records owned by a directory slot. */ /*获取目录槽拥有的记录数。*/
UNIV_INLINE
ulint
page_dir_slot_get_n_owned(
/*======================*/
					/* out: number of records */
	page_dir_slot_t* 	slot);	/* in: page directory slot */
/*******************************************************************
This is used to set the owned records field of a directory slot. */ /*这用于设置目录槽的拥有记录字段。*/
UNIV_INLINE
void
page_dir_slot_set_n_owned(
/*======================*/
	page_dir_slot_t*	slot,	/* in: directory slot */
	ulint			n);	/* in: number of records owned 
					by the slot */
/****************************************************************
Calculates the space reserved for directory slots of a given
number of records. The exact value is a fraction number
n * PAGE_DIR_SLOT_SIZE / PAGE_DIR_SLOT_MIN_N_OWNED, and it is
rounded upwards to an integer. */ /*计算给定记录数量的目录槽保留的空间。
确切的值是小数n * PAGE_DIR_SLOT_SIZE / PAGE_DIR_SLOT_MIN_N_OWNED，向上取整。*/
UNIV_INLINE
ulint
page_dir_calc_reserved_space(
/*=========================*/
	ulint	n_recs);	/* in: number of records */
/*******************************************************************
Looks for the directory slot which owns the given record. */
/*查找拥有给定记录的目录槽。*/
UNIV_INLINE
ulint
page_dir_find_owner_slot(
/*=====================*/
				/* out: the directory slot number */
	rec_t*	rec);		/* in: the physical record */
/****************************************************************
Gets the pointer to the next record on the page. */
/*获取指向页上下一个记录的指针。*/
UNIV_INLINE
rec_t*
page_rec_get_next(
/*==============*/
			/* out: pointer to next record */
	rec_t*	rec);	/* in: pointer to record, must not be page
			supremum */
/****************************************************************
Sets the pointer to the next record on the page. */ 
/*设置指向该页上下一个记录的指针。*/
UNIV_INLINE
void
page_rec_set_next(
/*==============*/
	rec_t*	rec,	/* in: pointer to record, must not be
			page supremum */
	rec_t*	next);	/* in: pointer to next record, must not
			be page infimum */
/****************************************************************
Gets the pointer to the previous record. *//*获取指向前一条记录的指针。*/
UNIV_INLINE
rec_t*
page_rec_get_prev(
/*==============*/
			/* out: pointer to previous record */
	rec_t*	rec);	/* in: pointer to record, must not be page
			infimum */
/****************************************************************
TRUE if the record is a user record on the page. */ /*如果该记录是页面上的用户记录，则为TRUE。*/
UNIV_INLINE
ibool
page_rec_is_user_rec(
/*=================*/
			/* out: TRUE if a user record */
	rec_t*	rec);	/* in: record */
/****************************************************************
TRUE if the record is the supremum record on a page. */ /*如果该记录是页面上的最高记录，则为TRUE。*/
UNIV_INLINE
ibool
page_rec_is_supremum(
/*=================*/
			/* out: TRUE if the supremum record */
	rec_t*	rec);	/* in: record */
/****************************************************************
TRUE if the record is the infimum record on a page. */ /*如果该记录是页上的下位记录，则为TRUE。*/
UNIV_INLINE
ibool
page_rec_is_infimum(
/*================*/
			/* out: TRUE if the infimum record */
	rec_t*	rec);	/* in: record */
/****************************************************************
TRUE if the record is the first user record on the page. */ /*如果该记录是页面上的第一个用户记录，则为TRUE。*/
UNIV_INLINE
ibool
page_rec_is_first_user_rec(
/*=======================*/
			/* out: TRUE if first user record */
	rec_t*	rec);	/* in: record */
/****************************************************************
TRUE if the record is the last user record on the page. */ /*如果该记录是页面上的最后一条用户记录，则为TRUE。*/
UNIV_INLINE
ibool
page_rec_is_last_user_rec(
/*======================*/
			/* out: TRUE if last user record */
	rec_t*	rec);	/* in: record */
/*******************************************************************
Looks for the record which owns the given record. */ /*查找拥有给定记录的记录。*/
UNIV_INLINE
rec_t*
page_rec_find_owner_rec(
/*====================*/
			/* out: the owner record */
	rec_t*	rec);	/* in: the physical record */
/***************************************************************************
This is a low-level operation which is used in a database index creation
to update the page number of a created B-tree to a data dictionary
record. */
/*这是一种低级操作，用于创建数据库索引，将已创建的b -树的页码更新为数据字典记录。*/
void
page_rec_write_index_page_no(
/*=========================*/
	rec_t*	rec,	/* in: record to update */
	ulint	i,	/* in: index of the field to update */
	ulint	page_no,/* in: value to write */
	mtr_t*	mtr);	/* in: mtr */
/****************************************************************
Returns the maximum combined size of records which can be inserted on top
of record heap. *//*返回可以插入到记录堆顶部的记录的最大组合大小。*/
UNIV_INLINE
ulint
page_get_max_insert_size(
/*=====================*/
			/* out: maximum combined size for inserted records */
	page_t*	page,	/* in: index page */
	ulint	n_recs);	/* in: number of records */
/****************************************************************
Returns the maximum combined size of records which can be inserted on top
of record heap if page is first reorganized. */
/*如果页面首先被重新组织，则返回可以插入到记录堆顶部的记录的最大组合大小。*/
UNIV_INLINE
ulint
page_get_max_insert_size_after_reorganize(
/*======================================*/
			/* out: maximum combined size for inserted records */
	page_t*	page,	/* in: index page */
	ulint	n_recs);/* in: number of records */
/*****************************************************************
Calculates free space if a page is emptied. */ /*如果页面被清空，则计算可用空间。*/
UNIV_INLINE
ulint
page_get_free_space_of_empty(void);
/*==============================*/
				/* out: free space */
/****************************************************************
Returns the sum of the sizes of the records in the record list
excluding the infimum and supremum records. */
/*返回记录列表中除下限值和上限值记录外的所有记录的大小之和。*/
UNIV_INLINE
ulint
page_get_data_size(
/*===============*/
			/* out: data in bytes */
	page_t*	page);	/* in: index page */
/****************************************************************
Allocates a block of memory from an index page. */
/*从索引页分配一块内存。*/
byte*
page_mem_alloc(
/*===========*/
			/* out: pointer to start of allocated 
			buffer, or NULL if allocation fails */
	page_t*	page,	/* in: index page */
	ulint	need,	/* in: number of bytes needed */
	ulint*	heap_no);/* out: this contains the heap number
			of the allocated record if allocation succeeds */
/****************************************************************
Puts a record to free list. */
/*将一个记录放到空闲列表中。*/
UNIV_INLINE
void
page_mem_free(
/*==========*/
	page_t*	page,	/* in: index page */
	rec_t*	rec);	/* in: pointer to the (origin of) record */
/**************************************************************
The index page creation function. */
/*索引页创建功能。*/
page_t* 
page_create(
/*========*/
					/* out: pointer to the page */
	buf_frame_t*	frame,		/* in: a buffer frame where the page is
					created */
	mtr_t*		mtr);		/* in: mini-transaction handle */
/*****************************************************************
Differs from page_copy_rec_list_end, because this function does not
touch the lock table and max trx id on page. */
/*不同于page_copy_rec_list_end，因为这个函数不接触锁表和页上的最大trx id。*/
void
page_copy_rec_list_end_no_locks(
/*============================*/
	page_t*	new_page,	/* in: index page to copy to */
	page_t*	page,		/* in: index page */
	rec_t*	rec,		/* in: record on page */
	mtr_t*	mtr);		/* in: mtr */
/*****************************************************************
Copies records from page to new_page, from the given record onward,
including that record. Infimum and supremum records are not copied.
The records are copied to the start of the record list on new_page. */
/*从给定记录开始(包括该记录)，将记录从page复制到new_page。下位记录和上位记录不被复制。记录被复制到new_page上的记录列表的开头。*/
void
page_copy_rec_list_end(
/*===================*/
	page_t*	new_page,	/* in: index page to copy to */
	page_t*	page,		/* in: index page */
	rec_t*	rec,		/* in: record on page */
	mtr_t*	mtr);		/* in: mtr */
/*****************************************************************
Copies records from page to new_page, up to the given record, NOT
including that record. Infimum and supremum records are not copied.
The records are copied to the end of the record list on new_page. */
/*将记录从page复制到new_page，直到给定的记录，不包括该记录。下位记录和上位记录不被复制。记录被复制到new_page上记录列*/
void
page_copy_rec_list_start(
/*=====================*/
	page_t*	new_page,	/* in: index page to copy to */
	page_t*	page,		/* in: index page */
	rec_t*	rec,		/* in: record on page */
	mtr_t*	mtr);		/* in: mtr */
/*****************************************************************
Deletes records from a page from a given record onward, including that record.
The infimum and supremum records are not deleted. */
/*从一个给定记录开始从一个页面删除记录，包括该记录。下限值和上限值不被删除。*/
void
page_delete_rec_list_end(
/*=====================*/
	page_t*	page,	/* in: index page */
	rec_t*	rec,	/* in: record on page */
	ulint	n_recs,	/* in: number of records to delete, or ULINT_UNDEFINED
			if not known */
	ulint	size,	/* in: the sum of the sizes of the records in the end
			of the chain to delete, or ULINT_UNDEFINED if not
			known */
	mtr_t*	mtr);	/* in: mtr */
/*****************************************************************
Deletes records from page, up to the given record, NOT including
that record. Infimum and supremum records are not deleted. */
/*从页面删除记录，直到给定的记录，不包括该记录。下位记录和上位记录不会被删除。*/
void
page_delete_rec_list_start(
/*=======================*/
	page_t*	page,	/* in: index page */
	rec_t*	rec,	/* in: record on page */
	mtr_t*	mtr);	/* in: mtr */
/*****************************************************************
Moves record list end to another page. Moved records include
split_rec. */
/*移动记录列表到另一个页面。移动的记录包括split_rec。*/
void
page_move_rec_list_end(
/*===================*/
	page_t*	new_page,	/* in: index page where to move */
	page_t*	page,		/* in: index page */
	rec_t*	split_rec,	/* in: first record to move */
	mtr_t*	mtr);		/* in: mtr */
/*****************************************************************
Moves record list start to another page. Moved records do not include
split_rec. */
/*移动记录列表开始到另一个页面。移动的记录不包括split_rec。*/
void
page_move_rec_list_start(
/*=====================*/
	page_t*	new_page,	/* in: index page where to move */
	page_t*	page,		/* in: index page */
	rec_t*	split_rec,	/* in: first record not to move */
	mtr_t*	mtr);		/* in: mtr */
/********************************************************************
Splits a directory slot which owns too many records. */
/*分割拥有过多记录的目录槽。*/
void
page_dir_split_slot(
/*================*/
	page_t*	page, 		/* in: the index page in question */
	ulint	slot_no); 	/* in: the directory slot */
/*****************************************************************
Tries to balance the given directory slot with too few records
with the upper neighbor, so that there are at least the minimum number 
of records owned by the slot; this may result in the merging of 
two slots. */
/*尝试与上层邻居平衡记录过少的给定目录槽，以便至少有最小数量的记录属于该槽;这可能导致两个槽的合并。*/
void
page_dir_balance_slot(
/*==================*/
	page_t*	page,		/* in: index page */
	ulint	slot_no); 	/* in: the directory slot */
/**************************************************************
Parses a log record of a record list end or start deletion. */
/*解析记录列表结束或开始删除的日志记录。*/
byte*
page_parse_delete_rec_list(
/*=======================*/
			/* out: end of log record or NULL */
	byte	type,	/* in: MLOG_LIST_END_DELETE or MLOG_LIST_START_DELETE */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */	
	mtr_t*	mtr);	/* in: mtr or NULL */
/***************************************************************
Parses a redo log record of creating a page. */
/*解析创建页面的重做日志记录。*/
byte*
page_parse_create(
/*==============*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr);	/* in: mtr or NULL */
/****************************************************************
Prints record contents including the data relevant only in
the index page context. */
/*打印记录内容，包括仅在索引页上下文中相关的数据。*/
void
page_rec_print(
/*===========*/
	rec_t*	rec);
/*******************************************************************
This is used to print the contents of the directory for
debugging purposes. */
/*它用于打印目录的内容，以供调试之用。*/
void
page_dir_print(
/*===========*/
	page_t*	page,	/* in: index page */
	ulint	pr_n);	/* in: print n first and n last entries */
/*******************************************************************
This is used to print the contents of the page record list for
debugging purposes. */
/*它用于打印页面记录列表的内容，以供调试之用。*/
void
page_print_list(
/*============*/
	page_t*	page,	/* in: index page */
	ulint	pr_n);	/* in: print n first and n last entries */
/*******************************************************************
Prints the info in a page header. */
/*打印页眉中的信息。*/
void
page_header_print(
/*==============*/
	page_t*	page);
/*******************************************************************
This is used to print the contents of the page for
debugging purposes. */
/*它用于打印页面的内容以供调试之用。*/
void
page_print(
/*======*/
	page_t*	page,	/* in: index page */
	ulint	dn,	/* in: print dn first and last entries in directory */
	ulint	rn);	/* in: print rn first and last records on page */
/*******************************************************************
The following is used to validate a record on a page. This function
differs from rec_validate as it can also check the n_owned field and
the heap_no field. */
/*下面的代码用于验证页面上的记录。这个函数与rec_validate不同，因为它也可以检查n_owned字段和heap_no字段。*/
ibool
page_rec_validate(
/*==============*/
			/* out: TRUE if ok */
	rec_t* 	rec);	/* in: record on the page */
/*******************************************************************
This function checks the consistency of an index page. */
/*这个函数检查索引页的一致性。*/
ibool
page_validate(
/*==========*/
				/* out: TRUE if ok */
	page_t*		page,	/* in: index page */
	dict_index_t*	index);	/* in: data dictionary index containing
				the page record type definition */
/*******************************************************************
Looks in the page record list for a record with the given heap number. */
/*在页记录列表中查找具有给定堆号的记录。*/
rec_t*
page_find_rec_with_heap_no(
/*=======================*/
			/* out: record, NULL if not found */
	page_t*	page,	/* in: index page */
	ulint	heap_no);/* in: heap number */

#ifdef UNIV_MATERIALIZE
#undef UNIV_INLINE
#define UNIV_INLINE  UNIV_INLINE_ORIGINAL
#endif

#ifndef UNIV_NONINL
#include "page0page.ic"
#endif

#endif 
