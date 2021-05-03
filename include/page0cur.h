/************************************************************************
The page cursor

(c) 1994-1996 Innobase Oy

Created 10/4/1994 Heikki Tuuri
*************************************************************************/
/*页面的光标*/
#ifndef page0cur_h
#define page0cur_h

#include "univ.i"

#include "page0types.h"
#include "page0page.h"
#include "rem0rec.h"
#include "data0data.h"
#include "mtr0mtr.h"


#define PAGE_CUR_ADAPT

/* Page cursor search modes; the values must be in this order! */
/*页面光标搜索模式;值必须按此顺序排列!*/
#define	PAGE_CUR_G	1
#define	PAGE_CUR_GE	2
#define	PAGE_CUR_L	3
#define	PAGE_CUR_LE	4
#define	PAGE_CUR_DBG	5

extern ulint	page_cur_short_succ;

/*************************************************************
Gets pointer to the page frame where the cursor is positioned. */
/*获取指向游标所在页框架的指针。*/
UNIV_INLINE
page_t*
page_cur_get_page(
/*==============*/
				/* out: page */
	page_cur_t*	cur);	/* in: page cursor */
/*************************************************************
Gets the record where the cursor is positioned. */
/*获取游标所在位置的记录。*/
UNIV_INLINE
rec_t*
page_cur_get_rec(
/*=============*/
				/* out: record */
	page_cur_t*	cur);	/* in: page cursor */
/*************************************************************
Sets the cursor object to point before the first user record 
on the page. */ /*将光标对象指向页面上的第一个用户记录之前。*/
UNIV_INLINE
void
page_cur_set_before_first(
/*======================*/
	page_t*		page,	/* in: index page */
	page_cur_t*	cur);	/* in: cursor */
/*************************************************************
Sets the cursor object to point after the last user record on 
the page. */ /*将光标对象指向页面上最后一条用户记录的后面。*/
UNIV_INLINE
void
page_cur_set_after_last(
/*====================*/
	page_t*		page,	/* in: index page */
	page_cur_t*	cur);	/* in: cursor */
/*************************************************************
Returns TRUE if the cursor is before first user record on page. */
/*如果光标在页面上的第一个用户记录之前，返回TRUE。*/
UNIV_INLINE
ibool
page_cur_is_before_first(
/*=====================*/
				/* out: TRUE if at start */
	page_cur_t*	cur);	/* in: cursor */
/*************************************************************
Returns TRUE if the cursor is after last user record. */
/*如果光标在最后一条用户记录之后，返回TRUE。*/
UNIV_INLINE
ibool
page_cur_is_after_last(
/*===================*/
				/* out: TRUE if at end */
	page_cur_t*	cur);	/* in: cursor */
/**************************************************************
Positions the cursor on the given record. *//*将光标定位在给定的记录上。*/
UNIV_INLINE
void
page_cur_position(
/*==============*/
	rec_t*		rec,	/* in: record on a page */
	page_cur_t*	cur);	/* in: page cursor */
/**************************************************************
Invalidates a page cursor by setting the record pointer NULL. */ /*通过将记录指针设置为NULL使页面游标失效。*/
UNIV_INLINE
void
page_cur_invalidate(
/*================*/
	page_cur_t*	cur);	/* in: page cursor */
/**************************************************************
Moves the cursor to the next record on page. */ /*将光标移到页面上的下一条记录。*/
UNIV_INLINE
void
page_cur_move_to_next(
/*==================*/
	page_cur_t*	cur);	/* in: cursor; must not be after last */
/**************************************************************
Moves the cursor to the previous record on page. */ /*将光标移到页面上的前一条记录。*/
UNIV_INLINE
void
page_cur_move_to_prev(
/*==================*/
	page_cur_t*	cur);	/* in: cursor; must not before first */
/***************************************************************
Inserts a record next to page cursor. Returns pointer to inserted record if
succeed, i.e., enough space available, NULL otherwise. The cursor stays at
the same position. */
/*在页面光标旁边插入一条记录。如果成功，返回指向插入记录的指针，即有足够的可用空间，否则为NULL。光标保持在相同的位置。*/
UNIV_INLINE
rec_t*
page_cur_tuple_insert(
/*==================*/
				/* out: pointer to record if succeed, NULL
				otherwise */
	page_cur_t*	cursor,	/* in: a page cursor */
	dtuple_t*      	tuple,  /* in: pointer to a data tuple */
	mtr_t*		mtr);	/* in: mini-transaction handle */
/***************************************************************
Inserts a record next to page cursor. Returns pointer to inserted record if
succeed, i.e., enough space available, NULL otherwise. The cursor stays at
the same position. */
/*在页面光标旁边插入一条记录。如果成功，返回指向插入记录的指针，即有足够的可用空间，否则为NULL。光标保持在相同的位置。*/
UNIV_INLINE
rec_t*
page_cur_rec_insert(
/*================*/
				/* out: pointer to record if succeed, NULL
				otherwise */
	page_cur_t*	cursor,	/* in: a page cursor */
	rec_t*		rec,	/* in: record to insert */
	mtr_t*		mtr);	/* in: mini-transaction handle */
/***************************************************************
Inserts a record next to page cursor. Returns pointer to inserted record if
succeed, i.e., enough space available, NULL otherwise. The record to be
inserted can be in a data tuple or as a physical record. The other parameter
must then be NULL. The cursor stays at the same position. */
/*在页面光标旁边插入一条记录。如果成功，返回指向插入记录的指针，即有足够的可用空间，否则为NULL。要插入的记录可以在一个数据元组中，也可以作为一个物理记录。另一个参数必须为NULL。光标保持在相同的位置。*/
rec_t*
page_cur_insert_rec_low(
/*====================*/
				/* out: pointer to record if succeed, NULL
				otherwise */
	page_cur_t*	cursor,	/* in: a page cursor */
	dtuple_t*      	tuple,  /* in: pointer to a data tuple or NULL */
	ulint		data_size,/* in: data size of tuple */
	rec_t*      	rec,  	/* in: pointer to a physical record or NULL */
	mtr_t*		mtr);	/* in: mini-transaction handle */
/*****************************************************************
Copies records from page to a newly created page, from a given record onward,
including that record. Infimum and supremum records are not copied. */
/*从一个给定记录(包括该记录)开始，将记录从一个页复制到一个新创建的页。下位记录和上位记录不被复制。*/
void
page_copy_rec_list_end_to_created_page(
/*===================================*/
	page_t*	new_page,	/* in: index page to copy to */
	page_t*	page,		/* in: index page */
	rec_t*	rec,		/* in: first record to copy */
	mtr_t*	mtr);		/* in: mtr */
/***************************************************************
Deletes a record at the page cursor. The cursor is moved to the 
next record after the deleted one. */
/*删除页光标处的记录。光标移动到删除记录后的下一条记录。*/
void
page_cur_delete_rec(
/*================*/
	page_cur_t*  	cursor,		/* in: a page cursor */
	mtr_t*		mtr);		/* in: mini-transaction handle */
/********************************************************************
Searches the right position for a page cursor. *//*搜索页面光标的正确位置。*/
UNIV_INLINE
ulint
page_cur_search(
/*============*/
				/* out: number of matched fields on the left */
	page_t*		page,	/* in: index page */
	dtuple_t*	tuple,	/* in: data tuple */
	ulint		mode,	/* in: PAGE_CUR_L, PAGE_CUR_LE, PAGE_CUR_G,
				or PAGE_CUR_GE */
	page_cur_t*	cursor);/* out: page cursor */
/********************************************************************
Searches the right position for a page cursor. */
/*搜索页面光标的正确位置。*/
void
page_cur_search_with_match(
/*=======================*/
	page_t*		page,	/* in: index page */
	dtuple_t*	tuple,	/* in: data tuple */
	ulint		mode,	/* in: PAGE_CUR_L, PAGE_CUR_LE, PAGE_CUR_G,
				or PAGE_CUR_GE */
	ulint*		iup_matched_fields,
				/* in/out: already matched fields in upper
				limit record */
	ulint*		iup_matched_bytes,
				/* in/out: already matched bytes in a field
				not yet completely matched */
	ulint*		ilow_matched_fields,
				/* in/out: already matched fields in lower
				limit record */
	ulint*		ilow_matched_bytes,
				/* in/out: already matched bytes in a field
				not yet completely matched */
	page_cur_t*	cursor); /* out: page cursor */ 
/***************************************************************
Positions a page cursor on a randomly chosen user record on a page. If there
are no user records, sets the cursor on the infimum record. */
/*将页面光标定位在页面上随机选择的用户记录上。如果没有用户记录，将光标设置在下一条记录上。*/
void
page_cur_open_on_rnd_user_rec(
/*==========================*/
	page_t*		page,	/* in: page */
	page_cur_t*	cursor);/* in/out: page cursor */
/***************************************************************
Parses a log record of a record insert on a page. */
/*解析页上插入的记录的日志记录。*/
byte*
page_cur_parse_insert_rec(
/*======================*/
			/* out: end of log record or NULL */
	ibool	is_short,/* in: TRUE if short inserts */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr);	/* in: mtr or NULL */
/**************************************************************
Parses a log record of copying a record list end to a new created page. */
/*解析将记录列表复制到新创建页面的日志记录。*/
byte*
page_parse_copy_rec_list_to_created_page(
/*=====================================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr);	/* in: mtr or NULL */
/***************************************************************
Parses log record of a record delete on a page. */
/*在页面上解析删除记录的日志记录。*/
byte*
page_cur_parse_delete_rec(
/*======================*/
			/* out: pointer to record end or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr);	/* in: mtr or NULL */

/* Index page cursor */
/*索引页光标*/
struct page_cur_struct{
	byte*	rec;	/* pointer to a record on page */
};

#ifndef UNIV_NONINL
#include "page0cur.ic"
#endif

#endif 
