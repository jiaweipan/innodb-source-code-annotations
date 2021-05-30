/******************************************************
Purge old versions
清除旧版本
(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0purge_h
#define trx0purge_h

#include "univ.i"
#include "trx0types.h"
#include "mtr0mtr.h"
#include "trx0sys.h"
#include "que0types.h"
#include "page0page.h"
#include "usr0sess.h"
#include "fil0fil.h"

/* The global data structure coordinating a purge 全局数据结构协调清除*/
extern trx_purge_t*	purge_sys;

/* A dummy undo record used as a return value when we have a whole undo log
which needs no purge 当我们有一个不需要清除的完整的撤消日志时，一个虚拟的撤消记录用作返回值*/
extern trx_undo_rec_t	trx_purge_dummy_rec;

/************************************************************************
Calculates the file address of an undo log header when we have the file
address of its history list node. 当获得历史列表节点的文件地址时，计算撤销日志头的文件地址。*/
UNIV_INLINE
fil_addr_t
trx_purge_get_log_from_hist(
/*========================*/
					/* out: file address of the log */
	fil_addr_t	node_addr);	/* in: file address of the history
					list node of the log */
/*********************************************************************
Checks if trx_id is >= purge_view: then it is guaranteed that its update
undo log still exists in the system. 检查trx_id是否为>= purge_view:然后保证它的更新undo日志仍然存在于系统中。*/

ibool
trx_purge_update_undo_must_exist(
/*=============================*/
			/* out: TRUE if is sure that it is preserved, also
			if the function returns FALSE, it is possible that
			the undo log still exists in the system */
	dulint	trx_id);/* in: transaction id */
/************************************************************************
Creates the global purge system control structure and inits the history
mutex. 创建全局清除系统控制结构并初始化历史互斥。*/

void
trx_purge_sys_create(void);
/*======================*/
/************************************************************************
Adds the update undo log as the first log in the history list. Removes the
update undo log segment from the rseg slot if it is too big for reuse. 将更新撤销日志添加为历史列表中的第一个日志。
如果更新undo日志段太大而不能重用，则从rseg槽位中删除该更新undo日志段。*/
void
trx_purge_add_update_undo_to_history(
/*=================================*/
	trx_t*	trx,		/* in: transaction */
	page_t*	undo_page,	/* in: update undo log header page,
				x-latched */
	mtr_t*	mtr);		/* in: mtr */
/************************************************************************
Fetches the next undo log record from the history list to purge. It must be
released with the corresponding release function. 从要清除的历史列表中获取下一个撤消日志记录。必须有相应的释放功能。*/
trx_undo_rec_t*
trx_purge_fetch_next_rec(
/*=====================*/
				/* out: copy of an undo log record, or
				pointer to the dummy undo log record
				&trx_purge_dummy_rec if the whole undo log
				can skipped in purge; NULL if none left */
	dulint*		roll_ptr,/* out: roll pointer to undo record */
	trx_undo_inf_t** cell,	/* out: storage cell for the record in the
				purge array */
	mem_heap_t*	heap);	/* in: memory heap where copied */
/***********************************************************************
Releases a reserved purge undo record. 释放一个保留的清除撤销记录。*/

void
trx_purge_rec_release(
/*==================*/
	trx_undo_inf_t*	cell);	/* in: storage cell */
/***********************************************************************
This function runs a purge batch. 此函数运行一个清除批处理。*/

ulint
trx_purge(void);
/*===========*/
				/* out: number of undo log pages handled in
				the batch */
/**********************************************************************
Prints information of the purge system to stderr. 将清除系统的信息打印到标准错误。*/
void
trx_purge_sys_print(void);
/*======================*/

/* The control structure used in the purge operation 在清洗操作中使用的控制结构*/
struct trx_purge_struct{
	ulint		state;		/* Purge system state 清洗系统状态*/
	sess_t*		sess;		/* System session running the purge
					query 运行清除查询的系统会话*/
	trx_t*		trx;		/* System transaction running the purge
					query: this trx is not in the trx list
					of the trx system and it never ends 运行清除查询的系统事务:此trx不在trx系统的trx列表中，它永远不会结束*/
	que_t*		query;		/* The query graph which will do the
					parallelized purge operation 将执行并行清除操作的查询图*/
	rw_lock_t	purge_is_running;/* Purge operation set an x-latch here
					while it is accessing a table: this
					prevents dropping of the table 清除操作在访问表时设置一个x锁存器:这可以防止删除表*/
	rw_lock_t	latch;		/* The latch protecting the purge view.
					A purge operation must acquire an
					x-latch here for the instant at which
					it changes the purge view: an undo
					log operation can prevent this by
					obtaining an s-latch here. 保护清除视图的闩锁。清除操作必须在它更改清除视图的瞬间在这里获得x-latch:
					撤消日志操作可以通过在这里获得s-latch来防止这种情况。*/
	read_view_t*	view;		/* The purge will not remove undo logs
					which are >= this view (purge view) 清除不会删除>=这个视图(清除视图)的撤销日志。*/
	mutex_t		mutex;		/* Mutex protecting the fields below 保护下面字段的互斥锁*/
	ulint		n_pages_handled;/* Approximate number of undo log
					pages processed in purge 清除中处理的撤销日志页面的大致数目*/
	ulint		handle_limit;	/* Target of how many pages to get
					processed in the current purge 当前清除中要处理多少页的目标*/
	/*------------------------------*/
	/* The following two fields form the 'purge pointer' which advances
	during a purge, and which is used in history list truncation 
	以下两个字段构成了“清除指针”，它在清除过程中前进，并在历史列表截断中使用*/
	dulint		purge_trx_no;	/* Purge has advanced past all
					transactions whose number is less
					than this 清除已经超过了所有少于这个数目的交易*/
	dulint		purge_undo_no;	/* Purge has advanced past all records
					whose undo number is less than this 清除已超过所有撤销编号小于此的记录*/
	/*-----------------------------*/
	ibool		next_stored;	/* TRUE if the info of the next record
					to purge is stored below: if yes, then
					the transaction number and the undo
					number of the record are stored in
					purge_trx_no and purge_undo_no above 如果下面存储了要清除的下一条记录的信息，则为TRUE:
					如果是，则该记录的事务号和撤销号存储在上面的purge_trx_no和purge_undo_no中*/
	trx_rseg_t*	rseg;		/* Rollback segment for the next undo
					record to purge 要清除的下一个撤消记录的回滚段*/
	ulint		page_no;	/* Page number for the next undo
					record to purge, page number of the
					log header, if dummy record 页号为下一个要清除的撤销记录，页号为日志头，如果是假记录*/
	ulint		offset;		/* Page offset for the next undo
					record to purge, 0 if the dummy
					record 要清除的下一个撤销记录的页偏移量，如果是假记录，则为0*/
	ulint		hdr_page_no;	/* Header page of the undo log where
					the next record to purge belongs 要清除的下一条记录所属的撤销日志的标题页*/
	ulint		hdr_offset;	/* Header byte offset on the page 页上的头字节偏移量*/
	/*-----------------------------*/
	trx_undo_arr_t*	arr;		/* Array of transaction numbers and
					undo numbers of the undo records
					currently under processing in purge 当前正在清除中处理的撤消记录的事务号和撤消号的数组*/
	mem_heap_t*	heap;		/* Temporary storage used during a
					purge: can be emptied after purge
					completes 在清洗过程中使用的临时存储器:可以在清洗完成后清空*/
};

#define TRX_PURGE_ON		1	/* purge operation is running 正在进行清除操作*/
#define TRX_STOP_PURGE		2	/* purge operation is stopped, or
					it should be stopped 清洗操作停止，或应停止*/
#ifndef UNIV_NONINL
#include "trx0purge.ic"
#endif

#endif 
