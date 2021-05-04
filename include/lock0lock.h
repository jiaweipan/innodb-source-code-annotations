/******************************************************
The transaction lock system

(c) 1996 Innobase Oy

Created 5/7/1996 Heikki Tuuri
*******************************************************/
/*事务锁系统*/
#ifndef lock0lock_h
#define lock0lock_h

#include "univ.i"
#include "trx0types.h"
#include "rem0types.h"
#include "dict0types.h"
#include "que0types.h"
#include "page0types.h"
#include "lock0types.h"
#include "read0types.h"
#include "hash0hash.h"

extern ibool	lock_print_waits;

/*************************************************************************
Gets the size of a lock struct. */
/*获取锁结构体的大小。*/
ulint
lock_get_size(void);
/*===============*/
			/* out: size in bytes */
/*************************************************************************
Creates the lock system at database start. */
/*在数据库启动时创建锁系统。*/
void
lock_sys_create(
/*============*/
	ulint	n_cells);	/* in: number of slots in lock hash table */
/*************************************************************************
Checks if some transaction has an implicit x-lock on a record in a secondary
index. */
/*检查某个事务是否对二级索引中的记录具有隐式x-锁。*/
trx_t*
lock_sec_rec_some_has_impl_off_kernel(
/*==================================*/
				/* out: transaction which has the x-lock, or
				NULL */
	rec_t*		rec,	/* in: user record */
	dict_index_t*	index);	/* in: secondary index */
/*************************************************************************
Checks if some transaction has an implicit x-lock on a record in a clustered
index. */ /*检查某个事务是否对聚集索引中的记录具有隐式x-锁。*/
UNIV_INLINE
trx_t*
lock_clust_rec_some_has_impl(
/*=========================*/
				/* out: transaction which has the x-lock, or
				NULL */
	rec_t*		rec,	/* in: user record */
	dict_index_t*	index);	/* in: clustered index */
/*****************************************************************
Resets the lock bits for a single record. Releases transactions
waiting for lock requests here. */
/*重置单个记录的锁定位。在这里释放等待锁请求的事务。*/
void
lock_rec_reset_and_release_wait(
/*============================*/
	rec_t*	rec);	/* in: record whose locks bits should be reset */
/*****************************************************************
Makes a record to inherit the locks of another record as gap type
locks, but does not reset the lock bits of the other record. Also
waiting lock requests on rec are inherited as GRANTED gap locks. */
/*创建一个记录以继承另一个记录的锁作为间隙类型锁，但不重置另一个记录的锁位。在rec上等待的锁请求也被继承为GRANTED间隙锁。*/
void
lock_rec_inherit_to_gap(
/*====================*/
	rec_t*	heir,	/* in: record which inherits */
	rec_t*	rec);	/* in: record from which inherited; does NOT reset
			the locks on this record */
/*****************************************************************
Updates the lock table when we have reorganized a page. NOTE: we copy
also the locks set on the infimum of the page; the infimum may carry
locks if an update of a record is occurring on the page, and its locks
were temporarily stored on the infimum. */
/*在重新组织页面后更新锁表。注意:我们也复制了设置在页面底部的锁;
如果页面上的记录正在发生更新，且其锁临时存储在下限值上，则下限值可以携带锁。*/
void
lock_move_reorganize_page(
/*======================*/
	page_t*	page,		/* in: old index page */
	page_t*	new_page);	/* in: reorganized page */
/*****************************************************************
Moves the explicit locks on user records to another page if a record
list end is moved to another page. */
/*如果记录列表的结尾被移到另一个页面，则将用户记录上的显式锁移到另一个页面。*/
void
lock_move_rec_list_end(
/*===================*/
	page_t*	new_page,	/* in: index page to move to */
	page_t*	page,		/* in: index page */
	rec_t*	rec);		/* in: record on page: this is the
				first record moved */
/*****************************************************************
Moves the explicit locks on user records to another page if a record
list start is moved to another page. */
/*如果记录列表开始移动到另一个页面，则将用户记录上的显式锁移动到另一个页面。*/
void
lock_move_rec_list_start(
/*=====================*/
	page_t*	new_page,	/* in: index page to move to */
	page_t*	page,		/* in: index page */
	rec_t*	rec,		/* in: record on page: this is the
				first record NOT copied */
	rec_t*	old_end);	/* in: old previous-to-last record on
				new_page before the records were copied */
/*****************************************************************
Updates the lock table when a page is split to the right. */
/*当页面向右拆分时更新锁表。*/
void
lock_update_split_right(
/*====================*/
	page_t*	right_page,	/* in: right page */
	page_t*	left_page);	/* in: left page */
/*****************************************************************
Updates the lock table when a page is merged to the right. */
/*当页面合并到右侧时更新锁表。*/
void
lock_update_merge_right(
/*====================*/
	rec_t*	orig_succ,	/* in: original successor of infimum
				on the right page before merge */ /*合并前右页上的下位的原始后继者*/
	page_t*	left_page);	/* in: merged index page which will be
				discarded */ /*将被丢弃的合并索引页*/
/*****************************************************************
Updates the lock table when the root page is copied to another in
btr_root_raise_and_insert. Note that we leave lock structs on the
root page, even though they do not make sense on other than leaf
pages: the reason is that in a pessimistic update the infimum record
of the root page will act as a dummy carrier of the locks of the record
to be updated. */
/*当根页被复制到btr_root_raise_and_insert中的另一个页时更新锁表。
注意,我们将锁结构在根页面,即使他们不理解其他比叶页面:悲观的原因是更新页面的下确界记录根将作为一个虚拟载波锁的记录被更新。*/
void
lock_update_root_raise(
/*===================*/
	page_t*	new_page,	/* in: index page to which copied */
	page_t*	root);		/* in: root page */
/*****************************************************************
Updates the lock table when a page is copied to another and the original page
is removed from the chain of leaf pages, except if page is the root! */
/*当一个页被复制到另一个页并且原始页从叶页链中删除时，更新锁表，除非page是根页!*/
void
lock_update_copy_and_discard(
/*=========================*/
	page_t*	new_page,	/* in: index page to which copied */
	page_t*	page);		/* in: index page; NOT the root! */
/*****************************************************************
Updates the lock table when a page is split to the left. */
/*当一个页面被拆分到左边时更新锁表。*/
void
lock_update_split_left(
/*===================*/
	page_t*	right_page,	/* in: right page */
	page_t*	left_page);	/* in: left page */
/*****************************************************************
Updates the lock table when a page is merged to the left. */
/*当一个页面合并到左边时更新锁表。*/
void
lock_update_merge_left(
/*===================*/
	page_t*	left_page,	/* in: left page to which merged */
	rec_t*	orig_pred,	/* in: original predecessor of supremum
				on the left page before merge */
	page_t*	right_page);	/* in: merged index page which will be
				discarded */
/*****************************************************************
Resets the original locks on heir and replaces them with gap type locks
inherited from rec. */
/*重置继承对象上的原始锁，并用从rec继承的间隙类型锁替换它们。*/
void
lock_rec_reset_and_inherit_gap_locks(
/*=================================*/
	rec_t*	heir,	/* in: heir record */
	rec_t*	rec);	/* in: record */
/*****************************************************************
Updates the lock table when a page is discarded. */
/*当一个页被丢弃时更新锁表。*/
void
lock_update_discard(
/*================*/
	rec_t*	heir,	/* in: record which will inherit the locks */
	page_t*	page);	/* in: index page which will be discarded */
/*****************************************************************
Updates the lock table when a new user record is inserted. */
/*插入新用户记录时更新锁表。*/
void
lock_update_insert(
/*===============*/
	rec_t*	rec);	/* in: the inserted record */
/*****************************************************************
Updates the lock table when a record is removed. */
/*当记录被删除时更新锁表。*/
void
lock_update_delete(
/*===============*/
	rec_t*	rec);	/* in: the record to be removed */
/*************************************************************************
Stores on the page infimum record the explicit locks of another record.
This function is used to store the lock state of a record when it is
updated and the size of the record changes in the update. The record
is in such an update moved, perhaps to another page. The infimum record
acts as a dummy carrier record, taking care of lock releases while the
actual record is being moved. */
/*页下的存储记录另一个记录的显式锁。这个函数用于存储记录更新时的锁状态，以及记录的大小在更新中发生变化。
记录在这样的更新中移动，可能是到另一个页面。最低值记录充当一个虚拟的载体记录，当实际记录被移动时，它负责锁的释放。*/
void
lock_rec_store_on_page_infimum(
/*===========================*/
	rec_t*	rec);	/* in: record whose lock state is stored
			on the infimum record of the same page; lock
			bits are reset on the record */
/*************************************************************************
Restores the state of explicit lock requests on a single record, where the
state was stored on the infimum of the page. */
/*恢复单个记录上显式锁请求的状态，该状态存储在页面的下端。*/
void
lock_rec_restore_from_page_infimum(
/*===============================*/
	rec_t*	rec,	/* in: record whose lock state is restored */
	page_t*	page);	/* in: page (rec is not necessarily on this page)
			whose infimum stored the lock state; lock bits are
			reset on the infimum */ 
/*************************************************************************
Returns TRUE if there are explicit record locks on a page. */
/*如果页面上有显式记录锁，则返回TRUE。*/
ibool
lock_rec_expl_exist_on_page(
/*========================*/
			/* out: TRUE if there are explicit record locks on
			the page */
	ulint	space,	/* in: space id */
	ulint	page_no);/* in: page number */
/*************************************************************************
Checks if locks of other transactions prevent an immediate insert of
a record. If they do, first tests if the query thread should anyway
be suspended for some reason; if not, then puts the transaction and
the query thread to the lock wait state and inserts a waiting request
for a gap x-lock to the lock queue. */
/*检查其他事务的锁是否阻止了记录的立即插入。
如果有，首先测试查询线程是否应该出于某种原因挂起;
如果不是，则将事务和查询线程置于锁等待状态，并向锁队列插入一个间隙x-lock的等待请求。*/
ulint
lock_rec_insert_check_and_lock(
/*===========================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	rec_t*		rec,	/* in: record after which to insert */
	dict_index_t*	index,	/* in: index */
	que_thr_t*	thr,	/* in: query thread */
	ibool*		inherit);/* out: set to TRUE if the new inserted
				record maybe should inherit LOCK_GAP type
				locks from the successor record */
/*************************************************************************
Checks if locks of other transactions prevent an immediate modify (update,
delete mark, or delete unmark) of a clustered index record. If they do,
first tests if the query thread should anyway be suspended for some
reason; if not, then puts the transaction and the query thread to the
lock wait state and inserts a waiting request for a record x-lock to the
lock queue. */
/*检查其他事务的锁是否阻止对聚集索引记录的立即修改(更新、删除标记或删除未标记)。
如果有，首先测试查询线程是否应该出于某种原因挂起;如果没有，则将事务和查询线程置于锁等待状态，并向锁队列插入一个记录x-lock的等待请求。*/
ulint
lock_clust_rec_modify_check_and_lock(
/*=================================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	rec_t*		rec,	/* in: record which should be modified */
	dict_index_t*	index,	/* in: clustered index */
	que_thr_t*	thr);	/* in: query thread */
/*************************************************************************
Checks if locks of other transactions prevent an immediate modify
(delete mark or delete unmark) of a secondary index record. */
/*检查其他事务的锁是否阻止对二级索引记录的立即修改(删除标记或删除未标记)。*/
ulint
lock_sec_rec_modify_check_and_lock(
/*===============================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	rec_t*		rec,	/* in: record which should be modified;
				NOTE: as this is a secondary index, we
				always have to modify the clustered index
				record first: see the comment below */
	dict_index_t*	index,	/* in: secondary index */
	que_thr_t*	thr);	/* in: query thread */
/*************************************************************************
Checks if locks of other transactions prevent an immediate read, or passing
over by a read cursor, of a clustered index record. If they do, first tests
if the query thread should anyway be suspended for some reason; if not, then
puts the transaction and the query thread to the lock wait state and inserts a
waiting request for a record lock to the lock queue. Sets the requested mode
lock on the record. */
/*检查其他事务的锁是否阻止对聚集索引记录的立即读取或读取游标传递。
如果有，首先测试查询线程是否应该出于某种原因挂起;如果没有，则将事务和查询线程置于锁等待状态，并向锁队列插入一个记录锁的等待请求。
对记录设置请求的模式锁定。*/
ulint
lock_clust_rec_read_check_and_lock(
/*===============================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	rec_t*		rec,	/* in: user record or page supremum record
				which should be read or passed over by a read
				cursor */
	dict_index_t*	index,	/* in: clustered index */
	ulint		mode,	/* in: mode of the lock which the read cursor
				should set on records: LOCK_S or LOCK_X; the
				latter is possible in SELECT FOR UPDATE */
	que_thr_t*	thr);	/* in: query thread */
/*************************************************************************
Like the counterpart for a clustered index above, but now we read a
secondary index record. */
/*就像上面聚集索引的对应对象一样，但是现在我们读取一个二级索引记录。*/
ulint
lock_sec_rec_read_check_and_lock(
/*=============================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	rec_t*		rec,	/* in: user record or page supremum record
				which should be read or passed over by a read
				cursor */
	dict_index_t*	index,	/* in: secondary index */
	ulint		mode,	/* in: mode of the lock which the read cursor
				should set on records: LOCK_S or LOCK_X; the
				latter is possible in SELECT FOR UPDATE */
	que_thr_t*	thr);	/* in: query thread */
/*************************************************************************
Checks that a record is seen in a consistent read. */
/*检查是否在一致的读取中看到一条记录。*/
ibool
lock_clust_rec_cons_read_sees(
/*==========================*/
				/* out: TRUE if sees, or FALSE if an earlier
				version of the record should be retrieved */
	rec_t*		rec,	/* in: user record which should be read or
				passed over by a read cursor */
	dict_index_t*	index,	/* in: clustered index */
	read_view_t*	view);	/* in: consistent read view */
/*************************************************************************
Checks that a non-clustered index record is seen in a consistent read. */
/*检查非聚集索引记录是否出现在一致的读取中。*/
ulint
lock_sec_rec_cons_read_sees(
/*========================*/
				/* out: TRUE if certainly sees, or FALSE if an
				earlier version of the clustered index record
				might be needed: NOTE that a non-clustered
				index page contains so little information on
				its modifications that also in the case FALSE,
				the present version of rec may be the right,
				but we must check this from the clustered
				index record */
	rec_t*		rec,	/* in: user record which should be read or
				passed over by a read cursor */
	dict_index_t*	index,	/* in: non-clustered index */
	read_view_t*	view);	/* in: consistent read view */
/*************************************************************************
Locks the specified database table in the mode given. If the lock cannot
be granted immediately, the query thread is put to wait. */
/*以给定的模式锁定指定的数据库表。如果不能立即授予锁，则将查询线程置于等待状态。*/
ulint
lock_table(
/*=======*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	dict_table_t*	table,	/* in: database table in dictionary cache */
	ulint		mode,	/* in: lock mode */
	que_thr_t*	thr);	/* in: query thread */
/*************************************************************************
Checks if there are any locks set on the table. */
/*检查table上是否有锁。*/
ibool
lock_is_on_table(
/*=============*/
				/* out: TRUE if there are lock(s) */
	dict_table_t*	table);	/* in: database table in dictionary cache */
/*************************************************************************
Releases an auto-inc lock a transaction possibly has on a table.
Releases possible other transactions waiting for this lock. */
/*释放一个事务可能在表上拥有的auto-inc锁。释放可能等待此锁的其他事务。*/
void
lock_table_unlock_auto_inc(
/*=======================*/
	trx_t*	trx);	/* in: transaction */
/*************************************************************************
Releases transaction locks, and releases possible other transactions waiting
because of these locks. */
/*释放事务锁，并释放可能因这些锁而等待的其他事务。*/
void
lock_release_off_kernel(
/*====================*/
	trx_t*	trx);	/* in: transaction */
/*************************************************************************
Cancels a waiting lock request and releases possible other transactions
waiting behind it. */
/*取消一个等待的锁请求并释放在它后面等待的其他可能的事务。*/
void
lock_cancel_waiting_and_release(
/*============================*/
	lock_t*	lock);	/* in: waiting lock request */
/*************************************************************************
Resets all locks, both table and record locks, on a table to be dropped.
No lock is allowed to be a wait lock. */
/*重置要删除的表上的所有锁，包括表锁和记录锁。不允许任何锁是等待锁。*/
void
lock_reset_all_on_table(
/*====================*/
	dict_table_t*	table);	/* in: table to be dropped */
/*************************************************************************
Calculates the fold value of a page file address: used in inserting or
searching for a lock in the hash table. */
/*计算页面文件地址的折叠值:用于在哈希表中插入或搜索锁。*/
UNIV_INLINE
ulint
lock_rec_fold(
/*===========*/
			/* out: folded value */
	ulint	space,	/* in: space */
	ulint	page_no);/* in: page number */
/*************************************************************************
Calculates the hash value of a page file address: used in inserting or
searching for a lock in the hash table. */
/*计算页面文件地址的哈希值:用于在哈希表中插入或搜索锁。*/
UNIV_INLINE
ulint
lock_rec_hash(
/*==========*/
			/* out: hashed value */
	ulint	space,	/* in: space */
	ulint	page_no);/* in: page number */
/*************************************************************************
Gets the mutex protecting record locks on a given page address. */
/*获取保护给定页面地址上的记录锁的互斥锁。*/
mutex_t*
lock_rec_get_mutex_for_addr(
/*========================*/
	ulint	space,	/* in: space id */
	ulint	page_no);/* in: page number */
/*************************************************************************
Validates the lock queue on a single record. */
/*在单个记录上验证锁队列。*/
ibool
lock_rec_queue_validate(
/*====================*/
				/* out: TRUE if ok */
	rec_t*		rec,	/* in: record to look at */
	dict_index_t*	index);	/* in: index, or NULL if not known */
/*************************************************************************
Prints info of a table lock. */
/*打印表锁的信息。*/
void
lock_table_print(
/*=============*/
	lock_t*	lock);	/* in: table type lock */
/*************************************************************************
Prints info of a record lock. */
/*打印记录锁的信息。*/
void
lock_rec_print(
/*===========*/
	lock_t*	lock);	/* in: record type lock */
/*************************************************************************
Prints info of locks for all transactions. */
/*打印所有事务的锁信息。*/
void
lock_print_info(void);
/*=================*/
/*************************************************************************
Validates the lock queue on a table. */
/*验证表上的锁队列。*/
ibool
lock_table_queue_validate(
/*======================*/
				/* out: TRUE if ok */
	dict_table_t*	table);	/* in: table */
/*************************************************************************
Validates the record lock queues on a page. */
/*验证页上的记录锁定队列。*/
ibool
lock_rec_validate_page(
/*===================*/
			/* out: TRUE if ok */
	ulint	space,	/* in: space id */
	ulint	page_no);/* in: page number */
/*************************************************************************
Validates the lock system. */
/*验证锁系统。*/
ibool
lock_validate(void);
/*===============*/
			/* out: TRUE if ok */

/* The lock system */
extern lock_sys_t*	lock_sys;

/* Lock modes and types */
#define	LOCK_NONE	0	/* this flag is used elsewhere to note
				consistent read */ /*此标志用于其他地方以表示一致的读取*/
#define	LOCK_IS		2	/* intention shared */ /*意向共享*/
#define	LOCK_IX		3	/* intention exclusive */ /*意向排他*/
#define	LOCK_S		4	/* shared */ /*共享*/
#define	LOCK_X		5	/* exclusive */ /*排它*/
#define	LOCK_AUTO_INC	6	/* locks the auto-inc counter of a table
				in an exclusive mode */ /*以独占模式锁定表的auto-inc计数器*/
#define LOCK_MODE_MASK	0xF	/* mask used to extract mode from the
				type_mode field in a lock */ /*用于从锁中的type_mode字段提取模式的掩码*/
#define LOCK_TABLE	16	/* these type values should be so high that */ /*这些类型值应该很高*/
#define	LOCK_REC	32	/* they can be ORed to the lock mode */  /*它们可以是ORed到锁定模式*/
#define LOCK_TYPE_MASK	0xF0	/* mask used to extract lock type from the
				type_mode field in a lock */ /*用于从锁中的type_mode字段提取锁类型的掩码*/
#define LOCK_WAIT	256	/* this wait bit should be so high that
				it can be ORed to the lock mode and type;
				when this bit is set, it means that the
				lock has not yet been granted, it is just
				waiting for its turn in the wait queue */ /*
				这个等待位应该是如此之高，以至于它可以 ORed 锁定模式和类型;
				当设置了这个位时，它意味着锁还没有被授予，它只是在等待轮到自己的等待队列*/
#define LOCK_GAP	512	/* this gap bit should be so high that
				it can be ORed to the other flags;
				when this bit is set, it means that the
				lock holds only on the gap before the record;
				for instance, an x-lock on the gap does not
				give permission to modify the record on which
				the bit is set; locks of this type are created
				when records are removed from the index chain
				of records */ /*这个间隙位应该是如此之高，以至于它可以或到其他旗帜;
				当设置这个位时，它意味着锁只持有记录之前的间隙;例如，gap上的x-lock不允许修改位所设置的记录;
				当从记录的索引链中删除记录时，将创建这种类型的锁*/

/* When lock bits are reset, the following flags are available: */ /*当锁定位被重置时，以下标志可用:*/
#define LOCK_RELEASE_WAIT	1
#define LOCK_NOT_RELEASE_WAIT	2

/* Lock operation struct */ /*锁定操作结构*/
typedef struct lock_op_struct	lock_op_t;
struct lock_op_struct{
	dict_table_t*	table;	/* table to be locked */
	ulint		mode;	/* lock mode */
};

#define LOCK_OP_START		1
#define LOCK_OP_COMPLETE	2

/* The lock system struct */
struct lock_sys_struct{
	hash_table_t*	rec_hash;	/* hash table of the record locks */
};

/* The lock system */
extern lock_sys_t*	lock_sys;


#ifndef UNIV_NONINL
#include "lock0lock.ic"
#endif

#endif 
