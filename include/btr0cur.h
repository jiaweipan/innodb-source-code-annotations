/******************************************************
The index tree cursor

(c) 1994-1996 Innobase Oy

Created 10/16/1994 Heikki Tuuri
*******************************************************/
/*索引树游标*/
#ifndef btr0cur_h
#define btr0cur_h

#include "univ.i"
#include "dict0dict.h"
#include "data0data.h"
#include "page0cur.h"
#include "btr0types.h"
#include "que0types.h"
#include "row0types.h"
#include "ha0ha.h"

/* Mode flags for btr_cur operations; these can be ORed */
#define BTR_NO_UNDO_LOG_FLAG	1	/* do no undo logging */
#define BTR_NO_LOCKING_FLAG	2	/* do no record lock checking */
#define BTR_KEEP_SYS_FLAG	4	/* sys fields will be found from the
					update vector or inserted entry */

#define BTR_CUR_ADAPT
#define BTR_CUR_HASH_ADAPT

/*************************************************************
Returns the page cursor component of a tree cursor. */ /*返回树游标的页面游标组件。*/
UNIV_INLINE
page_cur_t*
btr_cur_get_page_cur(
/*=================*/
				/* out: pointer to page cursor component */
	btr_cur_t*	cursor);	/* in: tree cursor */
/*************************************************************
Returns the record pointer of a tree cursor. */ /*返回树游标的记录指针。*/
UNIV_INLINE
rec_t*
btr_cur_get_rec(
/*============*/
				/* out: pointer to record */
	btr_cur_t*	cursor);	/* in: tree cursor */
/*************************************************************
Invalidates a tree cursor by setting record pointer to NULL. */ /*通过将记录指针设置为NULL使树游标失效。*/
UNIV_INLINE
void
btr_cur_invalidate(
/*===============*/
	btr_cur_t*	cursor);	/* in: tree cursor */
/*************************************************************
Returns the page of a tree cursor. */ /*返回树状游标的页面。*/
UNIV_INLINE
page_t*
btr_cur_get_page(
/*=============*/
				/* out: pointer to page */
	btr_cur_t*	cursor);	/* in: tree cursor */
/*************************************************************
Returns the tree of a cursor. */ /*返回游标树。*/
UNIV_INLINE
dict_tree_t*
btr_cur_get_tree(
/*=============*/
				/* out: tree */
	btr_cur_t*	cursor);	/* in: tree cursor */
/*************************************************************
Positions a tree cursor at a given record. */ /*将树状游标定位在给定的记录上。*/
UNIV_INLINE
void
btr_cur_position(
/*=============*/
	dict_index_t*	index, 	/* in: index */
	rec_t*		rec,	/* in: record in tree */
	btr_cur_t*	cursor);/* in: cursor */
/************************************************************************
Searches an index tree and positions a tree cursor on a given level.
NOTE: n_fields_cmp in tuple must be set so that it cannot be compared
to node pointer page number fields on the upper levels of the tree!
Note that if mode is PAGE_CUR_LE, which is used in inserts, then
cursor->up_match and cursor->low_match both will have sensible values.
If mode is PAGE_CUR_GE, then up_match will a have a sensible value. */
/*搜索索引树并在给定的级别上定位树游标。注意:元组中的n_fields_cmp必须设置，这样它就不能与树的上层的节点指针页号字段进行比较!
注意，如果mode是PAGE_CUR_LE(在插入中使用)，那么cursor->up_match和cursor->low_match都将具有合理的值。
如果mode为PAGE_CUR_GE，则up_match将有一个合理的值。*/
void
btr_cur_search_to_nth_level(
/*========================*/
	dict_index_t*	index,	/* in: index */
	ulint		level,	/* in: the tree level of search */
	dtuple_t*	tuple,	/* in: data tuple; NOTE: n_fields_cmp in
				tuple must be set so that it cannot get
				compared to the node ptr page number field! */ /*数据元组;注意:元组中的n_fields_cmp必须设置，以便不能与节点ptr页号字段进行比较!*/
	ulint		mode,	/* in: PAGE_CUR_L, ...;
				NOTE that if the search is made using a unique
				prefix of a record, mode should be PAGE_CUR_LE,
				not PAGE_CUR_GE, as the latter may end up on
				the previous page of the record! Inserts
				should always be made using PAGE_CUR_LE to
				search the position! */ /*注意，如果搜索是使用一个唯一的记录前缀，模式应该是PAGE_CUR_LE，而不是PAGE_CUR_GE，因为后者可能会结束在记录的前一页!插入应该总是使用PAGE_CUR_LE来搜索位置!*/
	ulint		latch_mode, /* in: BTR_SEARCH_LEAF, ..., ORed with
				BTR_INSERT and BTR_ESTIMATE;
				cursor->left_page is used to store a pointer
				to the left neighbor page, in the cases
				BTR_SEARCH_PREV and BTR_MODIFY_PREV;
				NOTE that if has_search_latch
				is != 0, we maybe do not have a latch set
				on the cursor page, we assume
				the caller uses his search latch
				to protect the record! */ /*cursor->left_page用于存储一个指针指向左边的邻居页面,在BTR_SEARCH_PREV BTR_MODIFY_PREV;注意,如果has_search_latch ! = 0,我们也许没有闩上设置光标页面中,我们假定调用者使用他的搜索锁保护记录!*/
	btr_cur_t*	cursor, /* in/out: tree cursor; the cursor page is
				s- or x-latched, but see also above! */
	ulint		has_search_latch,/* in: latch mode the caller
				currently has on btr_search_latch:
				RW_S_LATCH, or 0 */ /*调用者当前在btr_search_latch: RW_S_LATCH或0上的闩锁模式*/
	mtr_t*		mtr);	/* in: mtr */
/*********************************************************************
Opens a cursor at either end of an index. */
/*在索引的两端打开游标。*/
void
btr_cur_open_at_index_side(
/*=======================*/
	ibool		from_left,	/* in: TRUE if open to the low end,
					FALSE if to the high end */
	dict_index_t*	index,		/* in: index */
	ulint		latch_mode,	/* in: latch mode */
	btr_cur_t*	cursor,		/* in: cursor */
	mtr_t*		mtr);		/* in: mtr */
/**************************************************************************
Positions a cursor at a randomly chosen position within a B-tree. */
/*将光标定位在b -树中随机选择的位置。*/
void
btr_cur_open_at_rnd_pos(
/*====================*/
	dict_index_t*	index,		/* in: index */
	ulint		latch_mode,	/* in: BTR_SEARCH_LEAF, ... */
	btr_cur_t*	cursor,		/* in/out: B-tree cursor */
	mtr_t*		mtr);		/* in: mtr */
/*****************************************************************
Tries to perform an insert to a page in an index tree, next to cursor.
It is assumed that mtr holds an x-latch on the page. The operation does
not succeed if there is too little space on the page. If there is just
one record on the page, the insert will always succeed; this is to
prevent trying to split a page with just one record. */
/*尝试对索引树中游标旁边的页面执行插入操作。假设mtr在页面上持有一个x锁存器。
如果页面空间过小，操作不会成功。如果页面上只有一条记录，则插入总是成功的;这是为了防止试图用一个记录分割一个页面。*/
ulint
btr_cur_optimistic_insert(
/*======================*/
				/* out: DB_SUCCESS, DB_WAIT_LOCK,
				DB_FAIL, or error number */
	ulint		flags,	/* in: undo logging and locking flags: if not
				zero, the parameters index and thr should be
				specified */
	btr_cur_t*	cursor,	/* in: cursor on page after which to insert;
				cursor stays valid */
	dtuple_t*	entry,	/* in: entry to insert */
	rec_t**		rec,	/* out: pointer to inserted record if
				succeed */
	big_rec_t**	big_rec,/* out: big rec vector whose fields have to
				be stored externally by the caller, or
				NULL */
	que_thr_t*	thr,	/* in: query thread or NULL */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Performs an insert on a page of an index tree. It is assumed that mtr
holds an x-latch on the tree and on the cursor page. If the insert is
made on the leaf level, to avoid deadlocks, mtr must also own x-latches
to brothers of page, if those brothers exist. */
/*在索引树的页面上执行插入操作。假设mtr在树和游标页上持有一个x-latch。
如果插入是在叶级进行的，那么为了避免死锁，mtr还必须拥有对page的兄弟的x-latches，如果这些兄弟存在的话。*/
ulint
btr_cur_pessimistic_insert(
/*=======================*/
				/* out: DB_SUCCESS or error number */
	ulint		flags,	/* in: undo logging and locking flags: if not
				zero, the parameter thr should be
				specified; if no undo logging is specified,
				then the caller must have reserved enough
				free extents in the file space so that the
				insertion will certainly succeed */
	btr_cur_t*	cursor,	/* in: cursor after which to insert;
				cursor stays valid */
	dtuple_t*	entry,	/* in: entry to insert */
	rec_t**		rec,	/* out: pointer to inserted record if
				succeed */
	big_rec_t**	big_rec,/* out: big rec vector whose fields have to
				be stored externally by the caller, or
				NULL */
	que_thr_t*	thr,	/* in: query thread or NULL */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Updates a secondary index record when the update causes no size
changes in its fields. The only case when this function is currently
called is that in a char field characters change to others which
are identified in the collation order. */
/*当更新不会导致二级索引记录的字段大小发生变化时，更新二级索引记录。当前调用此函数的唯一情况是，在char字段中，字符改变为按排序顺序标识的其他字符。*/
ulint
btr_cur_update_sec_rec_in_place(
/*============================*/
				/* out: DB_SUCCESS or error number */
	btr_cur_t*	cursor,	/* in: cursor on the record to update;
				cursor stays valid and positioned on the
				same record */
	upd_t*		update,	/* in: update vector */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Updates a record when the update causes no size changes in its fields. */
/*当更新未导致记录的字段大小发生变化时，更新记录。*/
ulint
btr_cur_update_in_place(
/*====================*/
				/* out: DB_SUCCESS or error number */
	ulint		flags,	/* in: undo logging and locking flags */
	btr_cur_t*	cursor,	/* in: cursor on the record to update;
				cursor stays valid and positioned on the
				same record */
	upd_t*		update,	/* in: update vector */
	ulint		cmpl_info,/* in: compiler info on secondary index
				updates */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Tries to update a record on a page in an index tree. It is assumed that mtr
holds an x-latch on the page. The operation does not succeed if there is too
little space on the page or if the update would result in too empty a page,
so that tree compression is recommended. */
/*尝试更新索引树中页上的记录。假设mtr在页面上持有一个x锁存器。
如果页面上的空间太少，或者更新将导致页面太空，则操作不会成功，因此建议使用树压缩。*/
ulint
btr_cur_optimistic_update(
/*======================*/
				/* out: DB_SUCCESS, or DB_OVERFLOW if the
				updated record does not fit, DB_UNDERFLOW
				if the page would become too empty */
	ulint		flags,	/* in: undo logging and locking flags */
	btr_cur_t*	cursor,	/* in: cursor on the record to update;
				cursor stays valid and positioned on the
				same record */
	upd_t*		update,	/* in: update vector; this must also
				contain trx id and roll ptr fields */
	ulint		cmpl_info,/* in: compiler info on secondary index
				updates */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Performs an update of a record on a page of a tree. It is assumed
that mtr holds an x-latch on the tree and on the cursor page. If the
update is made on the leaf level, to avoid deadlocks, mtr must also
own x-latches to brothers of page, if those brothers exist. */
/*对树的页上的记录执行更新。假设mtr在树和游标页上持有一个x-latch。
如果在叶级进行更新，为了避免死锁，mtr还必须拥有对page兄弟的x-latches，如果这些兄弟存在的话。*/
ulint
btr_cur_pessimistic_update(
/*=======================*/
				/* out: DB_SUCCESS or error code */
	ulint		flags,	/* in: undo logging, locking, and rollback
				flags */
	btr_cur_t*	cursor,	/* in: cursor on the record to update */
	big_rec_t**	big_rec,/* out: big rec vector whose fields have to
				be stored externally by the caller, or NULL */
	upd_t*		update,	/* in: update vector; this is allowed also
				contain trx id and roll ptr fields, but
				the values in update vector have no effect */
	ulint		cmpl_info,/* in: compiler info on secondary index
				updates */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr);	/* in: mtr */
/***************************************************************
Marks a clustered index record deleted. Writes an undo log record to
undo log on this delete marking. Writes in the trx id field the id
of the deleting transaction, and in the roll ptr field pointer to the
undo log record created. */
/*标记已删除的聚集索引记录。在此删除标记上写入一条撤消日志记录以撤消日志。
在trx id字段中写入删除事务的id，在roll ptr字段中写入指向创建的undo日志记录的指针。*/
ulint
btr_cur_del_mark_set_clust_rec(
/*===========================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT, or error
				number */
	ulint		flags,	/* in: undo logging and locking flags */
	btr_cur_t*	cursor,	/* in: cursor */
	ibool		val,	/* in: value to set */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr);	/* in: mtr */
/***************************************************************
Sets a secondary index record delete mark to TRUE or FALSE. */
/*将二级索引记录删除标记设置为TRUE或FALSE。*/
ulint
btr_cur_del_mark_set_sec_rec(
/*=========================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT, or error
				number */
	ulint		flags,	/* in: locking flag */
	btr_cur_t*	cursor,	/* in: cursor */
	ibool		val,	/* in: value to set */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr);	/* in: mtr */
/***************************************************************
Sets a secondary index record delete mark to FALSE. This function is
only used by the insert buffer insert merge mechanism. */
/*将二级索引记录删除标记设置为FALSE。此函数仅用于插入缓冲区插入合并机制。*/
void
btr_cur_del_unmark_for_ibuf(
/*========================*/
	rec_t*	rec,	/* in: record to delete unmark */
	mtr_t*	mtr);	/* in: mtr */
/*****************************************************************
Tries to compress a page of the tree on the leaf level. It is assumed
that mtr holds an x-latch on the tree and on the cursor page. To avoid
deadlocks, mtr must also own x-latches to brothers of page, if those
brothers exist. NOTE: it is assumed that the caller has reserved enough
free extents so that the compression will always succeed if done! */
/*试图在叶级上压缩树的一页。假设mtr在树和游标页上持有一个x-latch。
为了避免死锁，mtr也必须拥有对page兄弟的x-latches，如果这些兄弟存在的话。
注意:假设调用者保留了足够的空闲区段，这样压缩就总是成功!*/
void
btr_cur_compress(
/*=============*/
	btr_cur_t*	cursor,	/* in: cursor on the page to compress;
				cursor does not stay valid */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Tries to compress a page of the tree if it seems useful. It is assumed
that mtr holds an x-latch on the tree and on the cursor page. To avoid
deadlocks, mtr must also own x-latches to brothers of page, if those
brothers exist. NOTE: it is assumed that the caller has reserved enough
free extents so that the compression will always succeed if done! */
/*试图压缩树中的一页，如果它看起来有用。假设mtr在树和游标页上持有一个x-latch。
为了避免死锁，mtr也必须拥有对page兄弟的x-latches，如果这些兄弟存在的话。
注意:假设调用者保留了足够的空闲区段，这样压缩就总是成功!*/
ibool
btr_cur_compress_if_useful(
/*=======================*/
				/* out: TRUE if compression occurred */
	btr_cur_t*	cursor,	/* in: cursor on the page to compress;
				cursor does not stay valid if compression
				occurs */
	mtr_t*		mtr);	/* in: mtr */
/***********************************************************
Removes the record on which the tree cursor is positioned. It is assumed
that the mtr has an x-latch on the page where the cursor is positioned,
but no latch on the whole tree. */
/*删除树光标所定位的记录。假设mtr在游标所在的页面上有一个x-latch，但是在整个树上没有latch。*/
ibool
btr_cur_optimistic_delete(
/*======================*/
				/* out: TRUE if success, i.e., the page
				did not become too empty */
	btr_cur_t*	cursor,	/* in: cursor on the record to delete;
				cursor stays valid: if deletion succeeds,
				on function exit it points to the successor
				of the deleted record */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Removes the record on which the tree cursor is positioned. Tries
to compress the page if its fillfactor drops below a threshold
or if it is the only page on the level. It is assumed that mtr holds
an x-latch on the tree and on the cursor page. To avoid deadlocks,
mtr must also own x-latches to brothers of page, if those brothers
exist. */
/*删除树光标所定位的记录。如果页面的填充因子低于阈值，或者它是该级别上唯一的页面，则尝试压缩该页。
假设mtr在树和游标页上持有一个x-latch。为了避免死锁，mtr也必须拥有对page兄弟的x-latches，如果这些兄弟存在的话。*/
ibool
btr_cur_pessimistic_delete(
/*=======================*/
				/* out: TRUE if compression occurred */
	ulint*		err,	/* out: DB_SUCCESS or DB_OUT_OF_FILE_SPACE;
				the latter may occur because we may have
				to update node pointers on upper levels,
				and in the case of variable length keys
				these may actually grow in size */
	ibool		has_reserved_extents, /* in: TRUE if the
				caller has already reserved enough free
				extents so that he knows that the operation
				will succeed */
	btr_cur_t*	cursor,	/* in: cursor on the record to delete;
				if compression does not occur, the cursor
				stays valid: it points to successor of
				deleted record on function exit */
	ibool		in_rollback,/* in: TRUE if called in rollback */
	mtr_t*		mtr);	/* in: mtr */
/***************************************************************
Parses a redo log record of updating a record in-place. */
/*解析原地更新记录的重做日志记录。*/
byte*
btr_cur_parse_update_in_place(
/*==========================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page);	/* in: page or NULL */
/***************************************************************
Parses a redo log record of updating a record, but not in-place. */
/*解析更新记录的重做日志记录，但不是原地更新。*/
byte*
btr_cur_parse_opt_update(
/*=====================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr);	/* in: mtr or NULL */
/********************************************************************
Parses the redo log record for delete marking or unmarking of a clustered
index record. */
/*解析重做日志记录以删除标记或不标记聚集索引记录。*/
byte*
btr_cur_parse_del_mark_set_clust_rec(
/*=================================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page);	/* in: page or NULL */	
/********************************************************************
Parses the redo log record for delete marking or unmarking of a secondary
index record. */
/*解析重做日志记录以删除标记或不标记二级索引记录。*/
byte*
btr_cur_parse_del_mark_set_sec_rec(
/*===============================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page);	/* in: page or NULL */	
/***********************************************************************
Estimates the number of rows in a given index range. */
/*估计给定索引范围内的行数。*/
ulint
btr_estimate_n_rows_in_range(
/*=========================*/
				/* out: estimated number of rows */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	tuple1,	/* in: range start, may also be empty tuple */
	ulint		mode1,	/* in: search mode for range start */
	dtuple_t*	tuple2,	/* in: range end, may also be empty tuple */
	ulint		mode2);	/* in: search mode for range end */
/***********************************************************************
Estimates the number of different key values in a given index, for
each n-column prefix of the index where n <= dict_index_get_n_unique(index).
The estimates are stored in the array index->stat_n_diff_key_vals. */
/*对于n <= dict_index_get_n_unique(index)的索引的每个n列前缀，估计给定索引中不同键值的数量。估计值存储在数组索引>stat_n_diff_key_vals中。*/
void
btr_estimate_number_of_different_key_vals(
/*======================================*/
	dict_index_t*	index);	/* in: index */
/***********************************************************************
Marks not updated extern fields as not-owned by this record. The ownership
is transferred to the updated record which is inserted elsewhere in the
index tree. In purge only the owner of externally stored field is allowed
to free the field. */
/*将未更新的外部字段标记为不属于此记录。所有权转移到插入到索引树其他地方的更新记录。在清除中，只有外部存储字段的所有者才允许释放该字段。*/
void
btr_cur_mark_extern_inherited_fields(
/*=================================*/
	rec_t*	rec,	/* in: record in a clustered index */
	upd_t*	update,	/* in: update vector */
	mtr_t*	mtr);	/* in: mtr */
/***********************************************************************
The complement of the previous function: in an update entry may inherit
some externally stored fields from a record. We must mark them as inherited
in entry, so that they are not freed in a rollback. */
/*前一个函数的补充:在一个更新条目中，可以从一个记录继承一些外部存储的字段。我们必须在条目中将它们标记为继承的，这样它们就不会在回滚中被释放。*/
void
btr_cur_mark_dtuple_inherited_extern(
/*=================================*/
	dtuple_t*	entry,		/* in: updated entry to be inserted to
					clustered index */
	ulint*		ext_vec,	/* in: array of extern fields in the
					original record */
	ulint		n_ext_vec,	/* in: number of elements in ext_vec */
	upd_t*		update);	/* in: update vector */
/***********************************************************************
Marks all extern fields in a record as owned by the record. This function
should be called if the delete mark of a record is removed: a not delete
marked record always owns all its extern fields. */
/*将记录中的所有外部字段标记为该记录所拥有。如果一个记录的删除标记被删除，这个函数应该被调用:一个没有删除标记的记录总是拥有它所有的外部字段。*/
void
btr_cur_unmark_extern_fields(
/*=========================*/
	rec_t*	rec,	/* in: record in a clustered index */
	mtr_t*	mtr);	/* in: mtr */
/***********************************************************************
Marks all extern fields in a dtuple as owned by the record. */
/*将dtuple中的所有extern字段标记为该记录所有。*/
void
btr_cur_unmark_dtuple_extern_fields(
/*================================*/
	dtuple_t*	entry,		/* in: clustered index entry */
	ulint*		ext_vec,	/* in: array of numbers of fields
					which have been stored externally */
	ulint		n_ext_vec);	/* in: number of elements in ext_vec */
/***********************************************************************
Stores the fields in big_rec_vec to the tablespace and puts pointers to
them in rec. The fields are stored on pages allocated from leaf node
file segment of the index tree. */
/*将big_rec_vec中的字段存储到表空间中，并将指向它们的指针存储在rec中。字段存储在索引树的叶节点文件段分配的页面中。*/
ulint
btr_store_big_rec_extern_fields(
/*============================*/
					/* out: DB_SUCCESS or error */
	dict_index_t*	index,		/* in: index of rec; the index tree
					MUST be X-latched */
	rec_t*		rec,		/* in: record */
	big_rec_t*	big_rec_vec,	/* in: vector containing fields
					to be stored externally */
	mtr_t*		local_mtr);	/* in: mtr containing the latch to
					rec and to the tree */
/***********************************************************************
Frees the space in an externally stored field to the file space
management if the field in data is owned the externally stored field,
in a rollback we may have the additional condition that the field must
not be inherited. */
/*如果数据中的字段属于外部存储的字段，则释放外部存储字段中的空间给文件空间管理，在回滚中，我们可能会有一个附加条件，即该字段不能被继承。*/
void
btr_free_externally_stored_field(
/*=============================*/
	dict_index_t*	index,		/* in: index of the data, the index
					tree MUST be X-latched */
	byte*		data,		/* in: internally stored data
					+ reference to the externally
					stored part */
	ulint		local_len,	/* in: length of data */
	ibool		do_not_free_inherited,/* in: TRUE if called in a
					rollback and we do not want to free
					inherited fields */
	mtr_t*		local_mtr);	/* in: mtr containing the latch to
					data an an X-latch to the index
					tree */
/***************************************************************
Frees the externally stored fields for a record. */
/*释放外部存储的记录字段。*/
void
btr_rec_free_externally_stored_fields(
/*==================================*/
	dict_index_t*	index,	/* in: index of the data, the index
				tree MUST be X-latched */
	rec_t*		rec,	/* in: record */
	ibool		do_not_free_inherited,/* in: TRUE if called in a
				rollback and we do not want to free
				inherited fields */
	mtr_t*		mtr);	/* in: mini-transaction handle which contains
				an X-latch to record page and to the index
				tree */
/***********************************************************************
Copies an externally stored field of a record to mem heap. */
/*将记录的外部存储字段复制到内存堆。*/
byte*
btr_rec_copy_externally_stored_field(
/*=================================*/
				/* out: the field copied to heap */
	rec_t*		rec,	/* in: record */
	ulint		no,	/* in: field number */
	ulint*		len,	/* out: length of the field */
	mem_heap_t*	heap);	/* in: mem heap */
/***********************************************************************
Copies an externally stored field of a record to mem heap. Parameter
data contains a pointer to 'internally' stored part of the field:
possibly some data, and the reference to the externally stored part in
the last 20 bytes of data. */
/*将记录的外部存储字段复制到内存堆。参数data包含一个指向“内部”存储的字段部分的指针:
可能是一些数据，以及在数据的最后20个字节中对外部存储部分的引用。*/
byte*
btr_copy_externally_stored_field(
/*=============================*/
				/* out: the whole field copied to heap */
	ulint*		len,	/* out: length of the whole field */
	byte*		data,	/* in: 'internally' stored part of the
				field containing also the reference to
				the external part */
	ulint		local_len,/* in: length of data */
	mem_heap_t*	heap);	/* in: mem heap */
/***********************************************************************
Stores the positions of the fields marked as extern storage in the update
vector, and also those fields who are marked as extern storage in rec
and not mentioned in updated fields. We use this function to remember
which fields we must mark as extern storage in a record inserted for an
update. */
/*在更新向量中存储标记为extern storage的字段的位置，以及在rec中标记为extern storage而在updated fields中没有提到的字段的位置。
我们使用这个函数来记住在为更新而插入的记录中哪些字段必须标记为外部存储。*/
ulint
btr_push_update_extern_fields(
/*==========================*/
				/* out: number of values stored in ext_vect */
	ulint*	ext_vect,	/* in: array of ulints, must be preallocated
				to have place for all fields in rec */
	rec_t*	rec,		/* in: record */
	upd_t*	update);	/* in: update vector */
	

/*######################################################################*/

/* In the pessimistic delete, if the page data size drops below this
limit, merging it to a neighbor is tried */
/*在悲观删除中，如果页面数据大小低于此限制，则尝试将其合并到邻居*/
#define BTR_CUR_PAGE_COMPRESS_LIMIT	(UNIV_PAGE_SIZE / 2)

/* A slot in the path array. We store here info on a search path down the
tree. Each slot contains data on a single level of the tree. */
/*路径阵列中的槽位。我们在树下的搜索路径上存储信息。每个插槽包含树的单个级别上的数据。*/
typedef struct btr_path_struct	btr_path_t;
struct btr_path_struct{
	ulint	nth_rec;	/* index of the record
				where the page cursor stopped on
				this level (index in alphabetical
				order); value ULINT_UNDEFINED
				denotes array end */ /*页面光标在该级别上停止的记录的索引(按字母顺序索引);ULINT_UNDEFINED表示数组结束*/
	ulint	n_recs;		/* number of records on the page */ /*页面记录个数*/
};

#define BTR_PATH_ARRAY_N_SLOTS	250	/* size of path array (in slots) */ /*路径数组的大小(以槽为单位)*/

/* The tree cursor: the definition appears here only for the compiler
to know struct size! */
/*树游标:这里的定义只是为了让编译器知道结构体的大小!*/
struct btr_cur_struct {
	dict_index_t*	index;		/* index where positioned */ /*索引在定位*/
	page_cur_t	page_cur;	/* page cursor */ /*页面光标*/
	page_t*		left_page;	/* this field is used to store a pointer
					to the left neighbor page, in the cases
					BTR_SEARCH_PREV and BTR_MODIFY_PREV */ /*BTR_SEARCH_PREV和BTR_MODIFY_PREV的情况下，该字段用于存储指向左邻居页面的指针*/
	/*------------------------------*/
	que_thr_t*	thr;		/* this field is only used when
					btr_cur_search_... is called for an
					index entry insertion: the calling
					query thread is passed here to be
					used in the insert buffer */ /*该字段仅在btr_cur_search_…在插入索引项时调用:调用查询线程在插入缓冲区中被传递*/
	/*------------------------------*/
	/* The following fields are used in btr_cur_search... to pass
	information: */ /*以下字段用于btr_cur_search…通过信息:*/
	ulint		flag;		/* BTR_CUR_HASH, BTR_CUR_HASH_FAIL,
					BTR_CUR_BINARY, or
					BTR_CUR_INSERT_TO_IBUF */
	ulint		tree_height;	/* Tree height if the search is done
					for a pessimistic insert or update
					operation */ /*如果执行了悲观插入或更新操作的搜索，则为树的高度*/
	ulint		up_match;	/* If the search mode was PAGE_CUR_LE,
					the number of matched fields to the
					the first user record to the right of
					the cursor record after
					btr_cur_search_...;
					for the mode PAGE_CUR_GE, the matched
					fields to the first user record AT THE
					CURSOR or to the right of it;
					NOTE that the up_match and low_match
					values may exceed the correct values
					for comparison to the adjacent user
					record if that record is on a
					different leaf page! (See the note in
					row_ins_duplicate_key.) */ /*如果搜索模式为PAGE_CUR_LE，则搜索模式为PAGE_CUR_GE时，btr_cur_search_…
					注意，up_match和low_match的值可能会超过与相邻用户记录进行比较的正确值，如果相邻用户记录位于不同的叶子页面上!
					(参见row_ins_duplicate_key中的注释。)*/
	ulint		up_bytes;	/* number of matched bytes to the
					right at the time cursor positioned;
					only used internally in searches: not
					defined after the search */ /*光标定位时向右匹配的字节数;只在内部搜索中使用:在搜索之后没有定义*/
	ulint		low_match;	/* if search mode was PAGE_CUR_LE,
					the number of matched fields to the
					first user record AT THE CURSOR or
					to the left of it after
					btr_cur_search_...;
					NOT defined for PAGE_CUR_GE or any
					other search modes; see also the NOTE
					in up_match! */ /*如果搜索模式为PAGE_CUR_LE，则匹配到the CURSOR处或btr_cur_search_后的第一个用户记录的字段数;请参见up_match!*/
	ulint		low_bytes;	/* number of matched bytes to the
					right at the time cursor positioned;
					only used internally in searches: not
					defined after the search */ /*游标定位时向右匹配的字节数;仅在搜索中内部使用:搜索后未定义*/
	ulint		n_fields;	/* prefix length used in a hash
					search if hash_node != NULL */ /*如果hash_node != NULL，则在哈希搜索中使用的前缀长度*/
	ulint		n_bytes;	/* hash prefix bytes if hash_node !=
					NULL */
	ulint		fold;		/* fold value used in the search if
					flag is BTR_CUR_HASH */
	/*------------------------------*/
	btr_path_t*	path_arr;	/* in estimating the number of
					rows in range, we store in this array
					information of the path through
					the tree */ /*在估计范围内的行数时，我们在这个数组中存储通过树的路径信息*/
};

/* Values for the flag documenting the used search method */ /*记录所使用的搜索方法的标志的值*/
#define BTR_CUR_HASH		1	/* successful shortcut using the hash
					index */ /*使用散列索引的成功快捷方式*/
#define BTR_CUR_HASH_FAIL	2	/* failure using hash, success using
					binary search: the misleading hash
					reference is stored in the field
					hash_node, and might be necessary to
					update */ /*失败使用哈希，成功使用二进制搜索:误导性的哈希引用存储在字段hash_node中，可能需要更新*/
#define BTR_CUR_BINARY		3	/* success using the binary search */ /*成功使用二分查找*/
#define BTR_CUR_INSERT_TO_IBUF	4	/* performed the intended insert to
					the insert buffer */ /*对插入缓冲区执行预期的插入*/

/* If pessimistic delete fails because of lack of file space,
there is still a good change of success a little later: try this many times,
and sleep this many microseconds in between */ /*如果悲观删除因为文件空间不足而失败，那么稍晚一点也会成功:尝试这多次，并在其间睡眠这几微秒*/
#define BTR_CUR_RETRY_DELETE_N_TIMES	100
#define BTR_CUR_RETRY_SLEEP_TIME	50000

/* The reference in a field of which data is stored on a different page */ /*数据存储在不同页面的字段中的引用*/
/*--------------------------------------*/
#define BTR_EXTERN_SPACE_ID		0	/* space id where stored */ /*存储空间id*/
#define BTR_EXTERN_PAGE_NO		4	/* page no where stored */ /*存储页号*/
#define BTR_EXTERN_OFFSET		8	/* offset of BLOB header
						on that page */ /*该页上BLOB头的偏移量*/
#define BTR_EXTERN_LEN			12	/* 8 bytes containing the
						length of the externally
						stored part of the BLOB.
						The 2 highest bits are
						reserved to the flags below. */ /*8个字节，包含BLOB外部存储部分的长度。最高的2位保留给下面的标志。*/
/*--------------------------------------*/
#define BTR_EXTERN_FIELD_REF_SIZE	20

/* The highest bit of BTR_EXTERN_LEN (i.e., the highest bit of the byte
at lowest address) is set to 1 if this field does not 'own' the externally
stored field; only the owner field is allowed to free the field in purge!
If the 2nd highest bit is 1 then it means that the externally stored field
was inherited from an earlier version of the row. In rollback we are not
allowed to free an inherited external field. */
/*如果该字段不“拥有”外部存储的字段，则BTR_EXTERN_LEN的最高位(即最低地址的字节的最高位)被设置为1;
在清除中，只有所有者字段允许释放字段!如果第2位是1，则意味着外部存储的字段继承自该行的早期版本。
在回滚中，不允许释放继承的外部字段。*/
#define BTR_EXTERN_OWNER_FLAG		128
#define BTR_EXTERN_INHERITED_FLAG	64

extern ulint	btr_cur_n_non_sea;

#ifndef UNIV_NONINL
#include "btr0cur.ic"
#endif
				
#endif
