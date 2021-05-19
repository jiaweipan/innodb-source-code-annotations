/******************************************************
The index tree persistent cursor

(c) 1996 Innobase Oy

Created 2/23/1996 Heikki Tuuri
*******************************************************/
/*索引树持久游标*/
#ifndef btr0pcur_h
#define btr0pcur_h

#include "univ.i"
#include "dict0dict.h"
#include "data0data.h"
#include "mtr0mtr.h"
#include "page0cur.h"
#include "btr0cur.h"
#include "btr0btr.h"
#include "btr0types.h"

/* Relative positions for a stored cursor position */ /*存储的游标位置的相对位置*/
#define BTR_PCUR_ON			1
#define BTR_PCUR_BEFORE			2
#define BTR_PCUR_AFTER			3
/* Note that if the tree is not empty, btr_pcur_store_position does not
use the following, but only uses the above three alternatives, where the
position is stored relative to a specific record: this makes implementation
of a scroll cursor easier */ /*注意，如果树不是空的，btr_pcur_store_position不使用以下方法，而只使用上述三种方法，
其中位置相对于特定的记录存储:这使得滚动光标的实现更容易*/
#define BTR_PCUR_BEFORE_FIRST_IN_TREE	4	/* in an empty tree */
#define BTR_PCUR_AFTER_LAST_IN_TREE	5	/* in an empty tree */

/******************************************************************
Allocates memory for a persistent cursor object and initializes the cursor. */
/*为持久游标对象分配内存并初始化游标。*/
btr_pcur_t*
btr_pcur_create_for_mysql(void);
/*============================*/
				/* out, own: persistent cursor */
/******************************************************************
Frees the memory for a persistent cursor object. */
/*释放持久游标对象的内存。*/
void
btr_pcur_free_for_mysql(
/*====================*/
	btr_pcur_t*	cursor);	/* in, own: persistent cursor */
/******************************************************************
Copies the stored position of a pcur to another pcur. */
/*将一个发生事件的存储位置复制到另一个发生事件。*/
void
btr_pcur_copy_stored_position(
/*==========================*/
	btr_pcur_t*	pcur_receive,	/* in: pcur which will receive the
					position info */
	btr_pcur_t*	pcur_donate);	/* in: pcur from which the info is
					copied */
/******************************************************************
Sets the old_rec_buf field to NULL. */ /*将old_rec_buf字段设置为NULL。*/
UNIV_INLINE
void
btr_pcur_init(
/*==========*/
	btr_pcur_t*	pcur);	/* in: persistent cursor */
/******************************************************************
Initializes and opens a persistent cursor to an index tree. It should be
closed with btr_pcur_close. */ /*初始化并打开指向索引树的持久游标。它应该用btr_pcur_close关闭。*/
UNIV_INLINE
void
btr_pcur_open(
/*==========*/
	dict_index_t*	index,	/* in: index */
	dtuple_t*	tuple,	/* in: tuple on which search done */
	ulint		mode,	/* in: PAGE_CUR_L, ...;
				NOTE that if the search is made using a unique
				prefix of a record, mode should be
				PAGE_CUR_LE, not PAGE_CUR_GE, as the latter
				may end up on the previous page from the
				record! */
	ulint		latch_mode,/* in: BTR_SEARCH_LEAF, ... */
	btr_pcur_t*	cursor, /* in: memory buffer for persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/******************************************************************
Opens an persistent cursor to an index tree without initializing the
cursor. */ /*在不初始化游标的情况下将持久游标打开到索引树。*/
UNIV_INLINE
void
btr_pcur_open_with_no_init(
/*=======================*/
	dict_index_t*	index,	/* in: index */
	dtuple_t*	tuple,	/* in: tuple on which search done */
	ulint		mode,	/* in: PAGE_CUR_L, ...;
				NOTE that if the search is made using a unique
				prefix of a record, mode should be
				PAGE_CUR_LE, not PAGE_CUR_GE, as the latter
				may end up on the previous page of the
				record! */
	ulint		latch_mode,/* in: BTR_SEARCH_LEAF, ...;
				NOTE that if has_search_latch != 0 then
				we maybe do not acquire a latch on the cursor
				page, but assume that the caller uses his
				btr search latch to protect the record! */
	btr_pcur_t*	cursor, /* in: memory buffer for persistent cursor */
	ulint		has_search_latch,/* in: latch mode the caller
				currently has on btr_search_latch:
				RW_S_LATCH, or 0 */
	mtr_t*		mtr);	/* in: mtr */
/*********************************************************************
Opens a persistent cursor at either end of an index. */ /*在索引的两端打开持久游标。*/
UNIV_INLINE
void
btr_pcur_open_at_index_side(
/*========================*/
	ibool		from_left,	/* in: TRUE if open to the low end,
					FALSE if to the high end */
	dict_index_t*	index,		/* in: index */
	ulint		latch_mode,	/* in: latch mode */
	btr_pcur_t*	pcur,		/* in: cursor */
	ibool		do_init,	/* in: TRUE if should be initialized */
	mtr_t*		mtr);		/* in: mtr */
/******************************************************************
Gets the up_match value for a pcur after a search. */ /*获取搜索后的pcur的up_match值。*/
UNIV_INLINE
ulint
btr_pcur_get_up_match(
/*==================*/
				/* out: number of matched fields at the cursor
				or to the right if search mode was PAGE_CUR_GE,
				otherwise undefined */
	btr_pcur_t*	cursor); /* in: memory buffer for persistent cursor */
/******************************************************************
Gets the low_match value for a pcur after a search. */ /*获取搜索后的pcur的low_match值。*/
UNIV_INLINE
ulint
btr_pcur_get_low_match(
/*===================*/
				/* out: number of matched fields at the cursor
				or to the right if search mode was PAGE_CUR_LE,
				otherwise undefined */
	btr_pcur_t*	cursor); /* in: memory buffer for persistent cursor */
/******************************************************************
If mode is PAGE_CUR_G or PAGE_CUR_GE, opens a persistent cursor on the first
user record satisfying the search condition, in the case PAGE_CUR_L or
PAGE_CUR_LE, on the last user record. If no such user record exists, then
in the first case sets the cursor after last in tree, and in the latter case
before first in tree. The latching mode must be BTR_SEARCH_LEAF or
BTR_MODIFY_LEAF. */
/*如果mode为PAGE_CUR_G或PAGE_CUR_GE，则在满足搜索条件的第一个用户记录(在PAGE_CUR_L或PAGE_CUR_LE的情况下)上打开持久游标。
如果不存在这样的用户记录，那么在第一种情况下将光标设置在树中的last之后，在后一种情况下将光标设置在树中的first之前。
锁定模式必须为BTR_SEARCH_LEAF或BTR_MODIFY_LEAF。*/
void
btr_pcur_open_on_user_rec(
/*======================*/
	dict_index_t*	index,		/* in: index */
	dtuple_t*	tuple,		/* in: tuple on which search done */
	ulint		mode,		/* in: PAGE_CUR_L, ... */
	ulint		latch_mode,	/* in: BTR_SEARCH_LEAF or
					BTR_MODIFY_LEAF */
	btr_pcur_t*	cursor, 	/* in: memory buffer for persistent
					cursor */
	mtr_t*		mtr);		/* in: mtr */
/**************************************************************************
Positions a cursor at a randomly chosen position within a B-tree. */ /*将光标定位在b -树中随机选择的位置。*/
UNIV_INLINE
void
btr_pcur_open_at_rnd_pos(
/*=====================*/
	dict_index_t*	index,		/* in: index */
	ulint		latch_mode,	/* in: BTR_SEARCH_LEAF, ... */
	btr_pcur_t*	cursor,		/* in/out: B-tree pcur */
	mtr_t*		mtr);		/* in: mtr */
/******************************************************************
Frees the possible old_rec_buf buffer of a persistent cursor and sets the
latch mode of the persistent cursor to BTR_NO_LATCHES. */ /*释放持久游标的可能的old_rec_buf缓冲区，并将持久游标的锁存模式设置为BTR_NO_LATCHES。*/
UNIV_INLINE
void
btr_pcur_close(
/*===========*/
	btr_pcur_t*	cursor);	/* in: persistent cursor */
/******************************************************************
The position of the cursor is stored by taking an initial segment of the
record the cursor is positioned on, before, or after, and copying it to the
cursor data structure, or just setting a flag if the cursor id before the
first in an EMPTY tree, or after the last in an EMPTY tree. NOTE that the
page where the cursor is positioned must not be empty if the index tree is
not totally empty! */
/*光标的位置存储在一个初始段记录的光标定位,之前或之后,并将它复制到光标数据结构,或者只是设置一个标志如果光标id在第一个空树之前,或之后的最后一个空树。
注意，如果索引树不是完全为空，则游标所在的页面一定不能为空!*/
void
btr_pcur_store_position(
/*====================*/
	btr_pcur_t*	cursor, /* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/******************************************************************
Restores the stored position of a persistent cursor bufferfixing the page and
obtaining the specified latches. If the cursor position was saved when the
(1) cursor was positioned on a user record: this function restores the position
to the last record LESS OR EQUAL to the stored record;
(2) cursor was positioned on a page infimum record: restores the position to
the last record LESS than the user record which was the successor of the page
infimum;
(3) cursor was positioned on the page supremum: restores to the first record
GREATER than the user record which was the predecessor of the supremum.
(4) cursor was positioned before the first or after the last in an empty tree:
restores to before first or after the last in the tree. */
/*恢复持久游标缓冲区的存储位置，修复该页并获得指定的锁存。如果光标位置保存(1)时,光标定位在一个用户记录:此功能恢复的位置到最后小于等于存储记录;
(2)游标定位在一个页面下确界记录:恢复过去记录不到位置的用户记录页面下确界的接班人;
(3)光标被定位到页面上界:恢复到第一个记录大于该上界的前任用户记录。
(4)游标被定位在一个空树中的第一个之前或最后一个之后:恢复到树中的第一个之前或最后一个之后。*/
ibool
btr_pcur_restore_position(
/*======================*/
					/* out: TRUE if the cursor position
					was stored when it was on a user record
					and it can be restored on a user record
					whose ordering fields are identical to
					the ones of the original user record */
	ulint		latch_mode,	/* in: BTR_SEARCH_LEAF, ... */
	btr_pcur_t*	cursor, 	/* in: detached persistent cursor */
	mtr_t*		mtr);		/* in: mtr */
/******************************************************************
If the latch mode of the cursor is BTR_LEAF_SEARCH or BTR_LEAF_MODIFY,
releases the page latch and bufferfix reserved by the cursor.
NOTE! In the case of BTR_LEAF_MODIFY, there should not exist changes
made by the current mini-transaction to the data protected by the
cursor latch, as then the latch must not be released until mtr_commit. */
/*如果游标的锁存模式为BTR_LEAF_SEARCH或BTR_LEAF_MODIFY，则释放该游标保留的页面锁存和缓冲修复。
在BTR_LEAF_MODIFY的情况下，当前的小事务不应该对游标锁存保护的数据进行更改，因为在mtr_commit之前，锁存不能被释放。*/
void
btr_pcur_release_leaf(
/*==================*/
	btr_pcur_t*	cursor, /* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Gets the rel_pos field for a cursor whose position has been stored. */ /*获取已存储位置的游标的rel_pos字段。*/
UNIV_INLINE
ulint
btr_pcur_get_rel_pos(
/*=================*/
				/* out: BTR_PCUR_ON, ... */
	btr_pcur_t*	cursor);/* in: persistent cursor */
/*************************************************************
Sets the mtr field for a pcur. */ /*设置pcur的mtr字段。*/
UNIV_INLINE
void
btr_pcur_set_mtr(
/*=============*/
	btr_pcur_t*	cursor,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in, own: mtr */
/*************************************************************
Gets the mtr field for a pcur. */ /*获取pcur的mtr字段。*/
UNIV_INLINE
mtr_t*
btr_pcur_get_mtr(
/*=============*/
				/* out: mtr */
	btr_pcur_t*	cursor);	/* in: persistent cursor */
/******************************************************************
Commits the pcur mtr and sets the pcur latch mode to BTR_NO_LATCHES,
that is, the cursor becomes detached. If there have been modifications
to the page where pcur is positioned, this can be used instead of
btr_pcur_release_leaf. Function btr_pcur_store_position should be used
before calling this, if restoration of cursor is wanted later. */
/*提交pcur mtr并将pcur闩锁模式设置为BTR_NO_LATCHES，也就是说，游标被分离。
如果对pcur所在的页面进行了修改，则可以使用它代替btr_pcur_release_leaf。如果稍后需要恢复游标，在调用此函数之前应该使用函数btr_pcur_store_position。*/
UNIV_INLINE
void
btr_pcur_commit(
/*============*/
	btr_pcur_t*	pcur);	/* in: persistent cursor */
/******************************************************************
Differs from btr_pcur_commit in that we can specify the mtr to commit. */ /*与btr_pcur_commit的不同之处在于，我们可以指定mtr来提交。*/
UNIV_INLINE
void
btr_pcur_commit_specify_mtr(
/*========================*/
	btr_pcur_t*	pcur,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr to commit */
/******************************************************************
Tests if a cursor is detached: that is the latch mode is BTR_NO_LATCHES. */ /*测试游标是否分离:即闩锁模式为BTR_NO_LATCHES。*/
UNIV_INLINE
ibool
btr_pcur_is_detached(
/*=================*/
				/* out: TRUE if detached */
	btr_pcur_t*	pcur);	/* in: persistent cursor */
/*************************************************************
Moves the persistent cursor to the next record in the tree. If no records are
left, the cursor stays 'after last in tree'. */ /*将持久游标移动到树中的下一条记录。如果没有记录留下，游标将停留在“最后一个树”之后。*/
UNIV_INLINE
ibool
btr_pcur_move_to_next(
/*==================*/
				/* out: TRUE if the cursor was not after last
				in tree */
	btr_pcur_t*	cursor,	/* in: persistent cursor; NOTE that the
				function may release the page latch */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Moves the persistent cursor to the previous record in the tree. If no records
are left, the cursor stays 'before first in tree'. */
/*将持久游标移动到树中的前一条记录。如果没有记录留下，游标将停留在“树的第一个之前”。*/
ibool
btr_pcur_move_to_prev(
/*==================*/
				/* out: TRUE if the cursor was not before first
				in tree */
	btr_pcur_t*	cursor,	/* in: persistent cursor; NOTE that the
				function may release the page latch */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Moves the persistent cursor to the next user record in the tree. If no user
records are left, the cursor ends up 'after last in tree'. */ /*将持久光标移动到树中的下一条用户记录。如果没有用户记录留下，游标将在“最后一个树”中结束。*/
UNIV_INLINE
ibool
btr_pcur_move_to_next_user_rec(
/*===========================*/
				/* out: TRUE if the cursor moved forward,
				ending on a user record */
	btr_pcur_t*	cursor,	/* in: persistent cursor; NOTE that the
				function may release the page latch */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Moves the persistent cursor to the first record on the next page.
Releases the latch on the current page, and bufferunfixes it.
Note that there must not be modifications on the current page,
as then the x-latch can be released only in mtr_commit. */
/*将持久光标移动到下一页上的第一个记录。释放当前页面上的锁存，bufferunch修复它。注意，不能对当前页面进行修改，因为x-latch只能在mtr_commit中被释放。*/
void
btr_pcur_move_to_next_page(
/*=======================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor; must be on the
				last record of the current page */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Moves the persistent cursor backward if it is on the first record
of the page. Releases the latch on the current page, and bufferunfixes
it. Note that to prevent a possible deadlock, the operation first
stores the position of the cursor, releases the leaf latch, acquires
necessary latches and restores the cursor position again before returning.
The alphabetical position of the cursor is guaranteed to be sensible
on return, but it may happen that the cursor is not positioned on the
last record of any page, because the structure of the tree may have
changed while the cursor had no latches. */
/*如果持久光标位于该页的第一个记录上，则将其向后移动。释放当前页面上的锁存，bufferunch修复它。
请注意，为了防止可能的死锁，该操作首先存储游标的位置，释放叶锁，获取必要的锁存，并在返回之前再次恢复游标位置。
游标的字母位置保证在返回时是合理的，但是可能游标没有定位在任何页面的最后一条记录上，因为树的结构可能发生了变化，而游标没有锁存。*/
void
btr_pcur_move_backward_from_page(
/*=============================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor, must be on the
				first record of the current page */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Returns the btr cursor component of a persistent cursor. */ /*返回持久游标的btr游标组件。*/
UNIV_INLINE
btr_cur_t*
btr_pcur_get_btr_cur(
/*=================*/
				/* out: pointer to btr cursor component */
	btr_pcur_t*	cursor);	/* in: persistent cursor */
/*************************************************************
Returns the page cursor component of a persistent cursor. */ /*返回持久游标的页面游标组件。*/
UNIV_INLINE
page_cur_t*
btr_pcur_get_page_cur(
/*==================*/
				/* out: pointer to page cursor component */
	btr_pcur_t*	cursor);	/* in: persistent cursor */
/*************************************************************
Returns the page of a persistent cursor. */ /*返回持久游标的页。*/
UNIV_INLINE
page_t*
btr_pcur_get_page(
/*==============*/
				/* out: pointer to the page */
	btr_pcur_t*	cursor);/* in: persistent cursor */
/*************************************************************
Returns the record of a persistent cursor. */ /*返回持久游标的记录。*/
UNIV_INLINE
rec_t*
btr_pcur_get_rec(
/*=============*/
				/* out: pointer to the record */
	btr_pcur_t*	cursor);/* in: persistent cursor */
/*************************************************************
Checks if the persistent cursor is on a user record. */ /*检查持久光标是否在用户记录上。*/
UNIV_INLINE
ibool
btr_pcur_is_on_user_rec(
/*====================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Checks if the persistent cursor is after the last user record on 
a page. */ /*检查持久光标是否位于页面上的最后一条用户记录之后。*/
UNIV_INLINE
ibool
btr_pcur_is_after_last_on_page(
/*===========================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Checks if the persistent cursor is before the first user record on 
a page. */ /*检查持久光标是否在页上的第一个用户记录之前。*/
UNIV_INLINE
ibool
btr_pcur_is_before_first_on_page(
/*=============================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Checks if the persistent cursor is before the first user record in
the index tree. */ /*检查持久游标是否在索引树中的第一个用户记录之前。*/
UNIV_INLINE
ibool
btr_pcur_is_before_first_in_tree(
/*=============================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Checks if the persistent cursor is after the last user record in
the index tree. */ /*检查持久游标是否位于索引树中的最后一条用户记录之后。*/
UNIV_INLINE
ibool
btr_pcur_is_after_last_in_tree(
/*===========================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Moves the persistent cursor to the next record on the same page. */ /*将持久光标移到同一页上的下一个记录。*/
UNIV_INLINE
void
btr_pcur_move_to_next_on_page(
/*==========================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Moves the persistent cursor to the previous record on the same page. */ /*将持久光标移动到同一页上的前一条记录。*/
UNIV_INLINE
void
btr_pcur_move_to_prev_on_page(
/*==========================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */


/* The persistent B-tree cursor structure. This is used mainly for SQL
selects, updates, and deletes. */
/*持久的b -树游标结构。这主要用于SQL选择、更新和删除。*/
struct btr_pcur_struct{
	btr_cur_t	btr_cur;	/* a B-tree cursor */
	ulint		latch_mode;	/* see FIXME note below!
					BTR_SEARCH_LEAF, BTR_MODIFY_LEAF,
					BTR_MODIFY_TREE, or BTR_NO_LATCHES,
					depending on the latching state of
					the page and tree where the cursor is
					positioned; the last value means that
					the cursor is not currently positioned:
					we say then that the cursor is
					detached; it can be restored to
					attached if the old position was
					stored in old_rec */ /*参见下面的FIXME注意事项!BTR_SEARCH_LEAF、BTR_MODIFY_LEAF、BTR_MODIFY_TREE或BTR_NO_LATCHES，
					这取决于游标所在页面和树的锁存状态;最后一个值表示游标当前没有定位:我们说then游标已经分离;
					如果旧位置存储在old_rec中，则可以将其恢复为attached*/
	ulint		old_stored;	/* BTR_PCUR_OLD_STORED
					or BTR_PCUR_OLD_NOT_STORED */
	rec_t*		old_rec;	/* if cursor position is stored,
					contains an initial segment of the
					latest record cursor was positioned
					either on, before, or after */ /*如果存储了游标位置，则包含游标定位的最新记录的初始段*/
	ulint		rel_pos;	/* BTR_PCUR_ON, BTR_PCUR_BEFORE, or
					BTR_PCUR_AFTER, depending on whether
					cursor was on, before, or after the
					old_rec record */ /*BTR_PCUR_ON、BTR_PCUR_BEFORE或BTR_PCUR_AFTER，这取决于光标是在old_rec记录之前还是之后*/
	dulint		modify_clock;	/* the modify clock value of the
					buffer block when the cursor position
					was stored */ /*存储光标位置时缓冲区的修改时钟值*/
	ulint		pos_state;	/* see FIXME note below!
					BTR_PCUR_IS_POSITIONED,
					BTR_PCUR_WAS_POSITIONED,
					BTR_PCUR_NOT_POSITIONED */
	ulint		search_mode;	/* PAGE_CUR_G, ... */
	/*-----------------------------*/
	/* NOTE that the following fields may possess dynamically allocated
	memory which should be freed if not needed anymore! */
    /*注意，以下字段可能占用动态分配的内存，如果不再需要，应该释放内存!*/
	mtr_t*		mtr;		/* NULL, or this field may contain
					a mini-transaction which holds the
					latch on the cursor page */
	byte*		old_rec_buf;	/* NULL, or a dynamically allocated
					buffer for old_rec */
	ulint		buf_size;	/* old_rec_buf size if old_rec_buf
					is not NULL */
};

#define BTR_PCUR_IS_POSITIONED	1997660512	/* FIXME: currently, the state
						can be BTR_PCUR_IS_POSITIONED,
						though it really should be
						BTR_PCUR_WAS_POSITIONED,
						because we have no obligation
						to commit the cursor with
						mtr; similarly latch_mode may
						be out of date */ /*FIXME:目前，状态可以是btr_pcur_is_positions，虽然它实际上应该是btr_pcur_was_positions，
						因为我们没有义务用mtr提交游标;类似地，latch_mode可能已经过时*/
#define BTR_PCUR_WAS_POSITIONED	1187549791
#define BTR_PCUR_NOT_POSITIONED 1328997689

#define BTR_PCUR_OLD_STORED	908467085
#define BTR_PCUR_OLD_NOT_STORED	122766467

#ifndef UNIV_NONINL
#include "btr0pcur.ic"
#endif
				
#endif
