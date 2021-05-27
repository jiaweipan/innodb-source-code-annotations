/******************************************************
Rollback segment
回滚段
(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0rseg_h
#define trx0rseg_h

#include "univ.i"
#include "trx0types.h"
#include "trx0sys.h"

/**********************************************************************
Gets a rollback segment header.获取回滚段标头。 */
UNIV_INLINE
trx_rsegf_t*
trx_rsegf_get(
/*==========*/
				/* out: rollback segment header, page
				x-latched */
	ulint	space,		/* in: space where placed */
	ulint	page_no,	/* in: page number of the header */
	mtr_t*	mtr);		/* in: mtr */
/**********************************************************************
Gets a newly created rollback segment header. 获取一个新创建的回滚段标头。*/
UNIV_INLINE
trx_rsegf_t*
trx_rsegf_get_new(
/*==============*/
				/* out: rollback segment header, page
				x-latched */
	ulint	space,		/* in: space where placed */
	ulint	page_no,	/* in: page number of the header */
	mtr_t*	mtr);		/* in: mtr */
/*******************************************************************
Gets the file page number of the nth undo log slot. 获取第n个撤消日志槽的文件页号。*/
UNIV_INLINE
ulint
trx_rsegf_get_nth_undo(
/*===================*/
				/* out: page number of the undo log segment */
	trx_rsegf_t*	rsegf,	/* in: rollback segment header */
	ulint		n,	/* in: index of slot */
	mtr_t*		mtr);	/* in: mtr */
/*******************************************************************
Sets the file page number of the nth undo log slot.设置第n个undo日志槽位的文件页号。 */
UNIV_INLINE
void
trx_rsegf_set_nth_undo(
/*===================*/
	trx_rsegf_t*	rsegf,	/* in: rollback segment header */
	ulint		n,	/* in: index of slot */
	ulint		page_no,/* in: page number of the undo log segment */
	mtr_t*		mtr);	/* in: mtr */
/********************************************************************
Looks for a free slot for an undo log segment.为撤消日志段寻找空闲槽位。 */
UNIV_INLINE
ulint
trx_rsegf_undo_find_free(
/*=====================*/
				/* out: slot index or ULINT_UNDEFINED if not
				found */
	trx_rsegf_t*	rsegf,	/* in: rollback segment header */
	mtr_t*		mtr);	/* in: mtr */
/**********************************************************************
Looks for a rollback segment, based on the rollback segment id. 根据回滚段id查找回滚段。*/

trx_rseg_t*
trx_rseg_get_on_id(
/*===============*/
			/* out: rollback segment */
	ulint	id);	/* in: rollback segment id */
/********************************************************************
Creates a rollback segment header. This function is called only when
a new rollback segment is created in the database. 创建回滚段标头。只有在数据库中创建了一个新的回滚段时，才会调用这个函数。*/

ulint
trx_rseg_header_create(
/*===================*/
				/* out: page number of the created segment,
				FIL_NULL if fail */
	ulint	space,		/* in: space id */
	ulint	max_size,	/* in: max size in pages */
	ulint*	slot_no,	/* out: rseg id == slot number in trx sys */
	mtr_t*	mtr);		/* in: mtr */
/*************************************************************************
Creates the memory copies for rollback segments and initializes the
rseg list and array in trx_sys at a database startup. 为回滚段创建内存副本，并在数据库启动时在trx_sys中初始化rseg列表和数组。*/

void
trx_rseg_list_and_array_init(
/*=========================*/
	trx_sysf_t*	sys_header,	/* in: trx system header */
	mtr_t*		mtr);		/* in: mtr */
/********************************************************************
Creates a new rollback segment to the database. 为数据库创建一个新的回滚段。*/

trx_rseg_t*
trx_rseg_create(
/*============*/
				/* out: the created segment object, NULL if
				fail */
	ulint	space,		/* in: space id */
	ulint	max_size,	/* in: max size in pages */
	ulint*	id,		/* out: rseg id */
	mtr_t*	mtr);		/* in: mtr */


/* Number of undo log slots in a rollback segment file copy 回滚段文件副本中的undo log槽数*/
#define TRX_RSEG_N_SLOTS	1024

/* Maximum number of transactions supported by a single rollback segment 单个回滚段支持的最大事务数*/
#define TRX_RSEG_MAX_N_TRXS	(TRX_RSEG_N_SLOTS / 2)

/* The rollback segment memory object 回滚段内存对象*/
struct trx_rseg_struct{
	/*--------------------------------------------------------*/
	ulint		id;	/* rollback segment id == the index of 
				its slot in the trx system file copy 回滚段id ==其槽位在TRX系统文件副本中的索引*/
	mutex_t		mutex;	/* mutex protecting the fields in this
				struct except id; NOTE that the latching
				order must always be kernel mutex ->
				rseg mutex 保护该结构中除id以外的字段的互斥锁;注意，锁存顺序必须始终是内核互斥量->rseg互斥量*/
	ulint		space;	/* space where the rollback segment is 
				header is placed 回滚段头部所在的空间被放置*/
	ulint		page_no;/* page number of the rollback segment
				header 回滚段标头的页号*/
	ulint		max_size;/* maximum allowed size in pages 页面中允许的最大大小*/
	ulint		curr_size;/* current size in pages 当前页面大小*/
	/*--------------------------------------------------------*/
	/* Fields for update undo logs 用于更新undo日志的字段*/
	UT_LIST_BASE_NODE_T(trx_undo_t) update_undo_list;
					/* List of update undo logs 更新undo日志列表*/
	UT_LIST_BASE_NODE_T(trx_undo_t) update_undo_cached;
					/* List of update undo log segments
					cached for fast reuse 为快速重用而缓存的更新undo日志段列表*/
	/*--------------------------------------------------------*/
	/* Fields for insert undo logs 插入undo日志字段*/
	UT_LIST_BASE_NODE_T(trx_undo_t) insert_undo_list;
					/* List of insert undo logs 插入undo日志列表*/
	UT_LIST_BASE_NODE_T(trx_undo_t) insert_undo_cached;
					/* List of insert undo log segments
					cached for fast reuse 为快速重用而缓存的插入undo日志段列表*/
	/*--------------------------------------------------------*/
	ulint		last_page_no;	/* Page number of the last not yet
					purged log header in the history list;
					FIL_NULL if all list purged 历史记录列表中最后一个尚未清除的日志头的页号;如果所有列表被清除，则FIL_NULL*/
	ulint		last_offset;	/* Byte offset of the last not yet
					purged log header 最后一个尚未清除的日志头的字节偏移量*/
	dulint		last_trx_no;	/* Transaction number of the last not
					yet purged log 最后一个尚未清除日志的事务号*/
	ibool		last_del_marks;	/* TRUE if the last not yet purged log
					needs purging 如果最后一个尚未清除的日志需要清除，则为TRUE*/
	/*--------------------------------------------------------*/
	UT_LIST_NODE_T(trx_rseg_t) rseg_list;
					/* the list of the rollback segment
					memory objects 回滚段内存对象的列表*/
};

/* Undo log segment slot in a rollback segment header Undo回滚段头中的日志段槽位*/
/*-------------------------------------------------------------*/
#define	TRX_RSEG_SLOT_PAGE_NO	0	/* Page number of the header page of
					an undo log segment undo日志段报头页号*/
/*-------------------------------------------------------------*/
/* Slot size 槽尺寸*/
#define TRX_RSEG_SLOT_SIZE	4

/* The offset of the rollback segment header on its page 回滚段标头在其页面上的偏移量*/
#define	TRX_RSEG		FSEG_PAGE_DATA

/* Transaction rollback segment header 事务回滚段头*/
/*-------------------------------------------------------------*/
#define	TRX_RSEG_MAX_SIZE	0	/* Maximum allowed size for rollback
					segment in pages 页中回滚段允许的最大大小*/
#define	TRX_RSEG_HISTORY_SIZE	4	/* Number of file pages occupied
					by the logs in the history list 历史列表中日志占用的文件页数*/
#define	TRX_RSEG_HISTORY	8	/* The update undo logs for committed
					transactions 已提交事务的更新撤销日志*/
#define	TRX_RSEG_FSEG_HEADER	(8 + FLST_BASE_NODE_SIZE)
					/* Header for the file segment where
					this page is placed 放置此页的文件段的头*/
#define TRX_RSEG_UNDO_SLOTS	(8 + FLST_BASE_NODE_SIZE + FSEG_HEADER_SIZE)
					/* Undo log segment slots Undo日志段插槽*/
/*-------------------------------------------------------------*/

#ifndef UNIV_NONINL
#include "trx0rseg.ic"
#endif

#endif 
