/******************************************************
The transaction
事务
(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#include "trx0trx.h"

#ifdef UNIV_NONINL
#include "trx0trx.ic"
#endif

#include "trx0undo.h"
#include "trx0rseg.h"
#include "log0log.h"
#include "que0que.h"
#include "lock0lock.h"
#include "trx0roll.h"
#include "usr0sess.h"
#include "read0read.h"
#include "srv0srv.h"
#include "thr0loc.h"
#include "btr0sea.h"


/* Copy of the prototype for innobase_mysql_print_thd: this
copy must be equal to the one in mysql/sql/ha_innobase.cc !
innobase_mysql_print_thd的原型的拷贝:这个拷贝必须等于mysql/sql/ha_innobase.cc中的一个 ! */
void innobase_mysql_print_thd(void* thd);

/* Dummy session used currently in MySQL interface 当前MySQL接口中使用的虚拟会话*/
sess_t*		trx_dummy_sess = NULL;

/* Number of transactions currently allocated for MySQL: protected by
the kernel mutex 当前分配给MySQL的事务数:由内核互斥保护*/
ulint	trx_n_mysql_transactions = 0;

/********************************************************************
Retrieves the error_info field from a trx. 从trx中检索error_info字段。*/

void*
trx_get_error_info(
/*===============*/
		     /* out: the error info */
	trx_t*  trx) /* in: trx object */
{
        return(trx->error_info);
}

/********************************************************************
Creates and initializes a transaction object. 创建并初始化事务对象。*/

trx_t*
trx_create(
/*=======*/
			/* out, own: the transaction */
	sess_t*	sess)	/* in: session or NULL */
{
	trx_t*	trx;

	ut_ad(mutex_own(&kernel_mutex));

	trx = mem_alloc(sizeof(trx_t));

	trx->magic_n = TRX_MAGIC_N;

	trx->op_info = "";
	
	trx->type = TRX_USER;
	trx->conc_state = TRX_NOT_STARTED;

	trx->dict_operation = FALSE;

	trx->mysql_thd = NULL;

	trx->n_mysql_tables_in_use = 0;
	trx->mysql_n_tables_locked = 0;

	trx->mysql_log_file_name = NULL;
	trx->mysql_log_offset = 0;
	
	trx->ignore_duplicates_in_insert = FALSE;

	mutex_create(&(trx->undo_mutex));
	mutex_set_level(&(trx->undo_mutex), SYNC_TRX_UNDO);

	trx->rseg = NULL;

	trx->undo_no = ut_dulint_zero;
	trx->last_sql_stat_start.least_undo_no = ut_dulint_zero;
	trx->insert_undo = NULL;
	trx->update_undo = NULL;
	trx->undo_no_arr = NULL;
	
	trx->error_state = DB_SUCCESS;

	trx->sess = sess;
	trx->que_state = TRX_QUE_RUNNING;
	trx->n_active_thrs = 0;

	trx->handling_signals = FALSE;

	UT_LIST_INIT(trx->signals);
	UT_LIST_INIT(trx->reply_signals);

	trx->graph = NULL;

	trx->wait_lock = NULL;
	UT_LIST_INIT(trx->wait_thrs);

	trx->lock_heap = mem_heap_create_in_buffer(256);
	UT_LIST_INIT(trx->trx_locks);

	trx->has_search_latch = FALSE;
	trx->search_latch_timeout = BTR_SEA_TIMEOUT;

	trx->declared_to_be_inside_innodb = FALSE;
	trx->n_tickets_to_enter_innodb = 0;

	trx->auto_inc_lock = NULL;
	
	trx->read_view_heap = mem_heap_create(256);
	trx->read_view = NULL;

	return(trx);
}

/************************************************************************
Creates a transaction object for MySQL.为MySQL创建事务对象。 */

trx_t*
trx_allocate_for_mysql(void)
/*========================*/
				/* out, own: transaction object */
{
	trx_t*	trx;

	mutex_enter(&kernel_mutex);
	
	/* Open a dummy session 打开一个虚拟会话*/

	if (!trx_dummy_sess) {
		trx_dummy_sess = sess_open(NULL, (byte*)"Dummy sess",
						ut_strlen("Dummy sess"));
	}
	
	trx = trx_create(trx_dummy_sess);

	trx_n_mysql_transactions++;
	
	UT_LIST_ADD_FIRST(mysql_trx_list, trx_sys->mysql_trx_list, trx);
	
	mutex_exit(&kernel_mutex);

	trx->mysql_thread_id = os_thread_get_curr_id();
	
	return(trx);
}

/************************************************************************
Creates a transaction object for background operations by the master thread. 为主线程的后台操作创建一个事务对象。*/

trx_t*
trx_allocate_for_background(void)
/*=============================*/
				/* out, own: transaction object */
{
	trx_t*	trx;

	mutex_enter(&kernel_mutex);
	
	/* Open a dummy session */

	if (!trx_dummy_sess) {
		trx_dummy_sess = sess_open(NULL, (byte*)"Dummy sess",
						ut_strlen("Dummy sess"));
	}
	
	trx = trx_create(trx_dummy_sess);

	mutex_exit(&kernel_mutex);
	
	return(trx);
}

/************************************************************************
Releases the search latch if trx has reserved it.如果trx保留了搜索锁存器，则释放它。 */

void
trx_search_latch_release_if_reserved(
/*=================================*/
        trx_t*     trx) /* in: transaction */
{
  	if (trx->has_search_latch) {
    		rw_lock_s_unlock(&btr_search_latch);

    		trx->has_search_latch = FALSE;
  	}
}

/************************************************************************
Frees a transaction object. 释放事务对象。*/

void
trx_free(
/*=====*/
	trx_t*	trx)	/* in, own: trx object */
{
	ut_ad(mutex_own(&kernel_mutex));

	ut_a(trx->magic_n == TRX_MAGIC_N);

	trx->magic_n = 11112222;

	ut_a(trx->conc_state == TRX_NOT_STARTED);
	
	mutex_free(&(trx->undo_mutex));

	ut_a(trx->insert_undo == NULL); 
	ut_a(trx->update_undo == NULL); 

	ut_a(trx->n_mysql_tables_in_use == 0);
	ut_a(trx->mysql_n_tables_locked == 0);
	
	if (trx->undo_no_arr) {
		trx_undo_arr_free(trx->undo_no_arr);
	}

	ut_a(UT_LIST_GET_LEN(trx->signals) == 0);
	ut_a(UT_LIST_GET_LEN(trx->reply_signals) == 0);

	ut_a(trx->wait_lock == NULL);
	ut_a(UT_LIST_GET_LEN(trx->wait_thrs) == 0);

	ut_a(!trx->has_search_latch);
	ut_a(!trx->auto_inc_lock);

	if (trx->lock_heap) {
		mem_heap_free(trx->lock_heap);
	}

	ut_a(UT_LIST_GET_LEN(trx->trx_locks) == 0);

	if (trx->read_view_heap) {
		mem_heap_free(trx->read_view_heap);
	}

	ut_a(trx->read_view == NULL);
	
	mem_free(trx);
}

/************************************************************************
Frees a transaction object for MySQL. 释放MySQL的事务对象。*/

void
trx_free_for_mysql(
/*===============*/
	trx_t*	trx)	/* in, own: trx object */
{
	thr_local_free(trx->mysql_thread_id);

	mutex_enter(&kernel_mutex);
	
	UT_LIST_REMOVE(mysql_trx_list, trx_sys->mysql_trx_list, trx);

	trx_free(trx);

	ut_a(trx_n_mysql_transactions > 0);

	trx_n_mysql_transactions--;
	
	mutex_exit(&kernel_mutex);
}

/************************************************************************
Frees a transaction object of a background operation of the master thread. 释放主线程的后台操作的事务对象。*/

void
trx_free_for_background(
/*====================*/
	trx_t*	trx)	/* in, own: trx object */
{
	mutex_enter(&kernel_mutex);
	
	trx_free(trx);
	
	mutex_exit(&kernel_mutex);
}

/********************************************************************
Inserts the trx handle in the trx system trx list in the right position.
The list is sorted on the trx id so that the biggest id is at the list
start. This function is used at the database startup to insert incomplete
transactions to the list. 将trx句柄插入trx系统trx列表的正确位置。列表按trx id排序，以便最大的id位于列表的开始位置。
该函数用于在数据库启动时将未完成的事务插入到列表中。*/
static
void
trx_list_insert_ordered(
/*====================*/
	trx_t*	trx)	/* in: trx handle */
{
	trx_t*	trx2;

	ut_ad(mutex_own(&kernel_mutex));

	trx2 = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (trx2 != NULL) {
		if (ut_dulint_cmp(trx->id, trx2->id) >= 0) {

			ut_ad(ut_dulint_cmp(trx->id, trx2->id) == 1);
			break;
		}
		trx2 = UT_LIST_GET_NEXT(trx_list, trx2);
	}

	if (trx2 != NULL) {
		trx2 = UT_LIST_GET_PREV(trx_list, trx2);

		if (trx2 == NULL) {
			UT_LIST_ADD_FIRST(trx_list, trx_sys->trx_list, trx);
		} else {
			UT_LIST_INSERT_AFTER(trx_list, trx_sys->trx_list,
								trx2, trx);
		}
	} else {
		UT_LIST_ADD_LAST(trx_list, trx_sys->trx_list, trx);
	}		
}

/********************************************************************
Creates trx objects for transactions and initializes the trx list of
trx_sys at database start. Rollback segment and undo log lists must
already exist when this function is called, because the lists of
transactions to be rolled back or cleaned up are built based on the
undo log lists. 为事务创建trx对象，并在数据库启动时初始化trx_sys的trx列表。
调用此函数时，回滚段和撤销日志列表必须已经存在，因为要回滚或清理的事务列表是基于撤销日志列表构建的。*/
void
trx_lists_init_at_db_start(void)
/*============================*/
{
	trx_rseg_t*	rseg;
	trx_undo_t*	undo;
	trx_t*		trx;

	UT_LIST_INIT(trx_sys->trx_list);

	/* Look from the rollback segments if there exist undo logs for
	transactions 查看回滚段中是否存在事务的撤消日志*/
	
	rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);

	while (rseg != NULL) {
		undo = UT_LIST_GET_FIRST(rseg->insert_undo_list);

		while (undo != NULL) {

			trx = trx_create(NULL); 

			if (undo->state != TRX_UNDO_ACTIVE) {

				trx->conc_state = TRX_COMMITTED_IN_MEMORY;
			} else {
				trx->conc_state = TRX_ACTIVE;
			}

			trx->id = undo->trx_id;
			trx->insert_undo = undo;
			trx->rseg = rseg;

			if (undo->dict_operation) {
				trx->dict_operation = undo->dict_operation;
				trx->table_id = undo->table_id;
			}

			if (!undo->empty) {
				trx->undo_no = ut_dulint_add(undo->top_undo_no,
									1);
			}

			trx_list_insert_ordered(trx);

			undo = UT_LIST_GET_NEXT(undo_list, undo);
		}

		undo = UT_LIST_GET_FIRST(rseg->update_undo_list);

		while (undo != NULL) {
			trx = trx_get_on_id(undo->trx_id);

			if (NULL == trx) {
				trx = trx_create(NULL); 

				if (undo->state != TRX_UNDO_ACTIVE) {
					trx->conc_state =
						TRX_COMMITTED_IN_MEMORY;
				} else {
					trx->conc_state = TRX_ACTIVE;
				}

				trx->id = undo->trx_id;
				trx->rseg = rseg;
				trx_list_insert_ordered(trx);

				if (undo->dict_operation) {
					trx->dict_operation =
							undo->dict_operation;
					trx->table_id = undo->table_id;
				}
			}

			trx->update_undo = undo;

			if ((!undo->empty)
			    && (ut_dulint_cmp(undo->top_undo_no, trx->undo_no)
			        >= 0)) {

				trx->undo_no = ut_dulint_add(undo->top_undo_no,
									1);
			}
			
			undo = UT_LIST_GET_NEXT(undo_list, undo);
		}

		rseg = UT_LIST_GET_NEXT(rseg_list, rseg);
	}
}

/**********************************************************************
Assigns a rollback segment to a transaction in a round-robin fashion.
Skips the SYSTEM rollback segment if another is available.
以循环方式将回滚段分配给事务。如果有其他可用的系统回滚段，则跳过SYSTEM回滚段。 */
UNIV_INLINE
ulint
trx_assign_rseg(void)
/*=================*/
			/* out: assigned rollback segment id */
{
	trx_rseg_t*	rseg	= trx_sys->latest_rseg;

	ut_ad(mutex_own(&kernel_mutex));
loop:
	/* Get next rseg in a round-robin fashion 以循环方式获得下一个rseg*/

	rseg = UT_LIST_GET_NEXT(rseg_list, rseg);

	if (rseg == NULL) {
		rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);
	}

	/* If it is the SYSTEM rollback segment, and there exist others, skip
	it 如果是SYSTEM回滚段，且存在其他段，请跳过该段*/

	if ((rseg->id == TRX_SYS_SYSTEM_RSEG_ID) 
			&& (UT_LIST_GET_LEN(trx_sys->rseg_list) > 1)) {
		goto loop;
	}			

	trx_sys->latest_rseg = rseg;

	return(rseg->id);
}

/********************************************************************
Starts a new transaction. 启动一个新事务。*/

ibool
trx_start_low(
/*==========*/
			/* out: TRUE */
	trx_t* 	trx,	/* in: transaction */
	ulint	rseg_id)/* in: rollback segment id; if ULINT_UNDEFINED
			is passed, the system chooses the rollback segment
			automatically in a round-robin fashion */
{
	trx_rseg_t*	rseg;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(trx->rseg == NULL);

	if (trx->type == TRX_PURGE) {
		trx->id = ut_dulint_zero;
		trx->conc_state = TRX_ACTIVE;

		return(TRUE);
	}

	ut_ad(trx->conc_state != TRX_ACTIVE);
	
	if (rseg_id == ULINT_UNDEFINED) {

		rseg_id = trx_assign_rseg();
	}

	rseg = trx_sys_get_nth_rseg(trx_sys, rseg_id);

	trx->id = trx_sys_get_new_trx_id();

	/* The initial value for trx->no: ut_dulint_max is used in
	read_view_open_now: 在read_view_open_now中使用trx->no: ut_dulint_max的初始值:*/

	trx->no = ut_dulint_max;

	trx->rseg = rseg;

	trx->conc_state = TRX_ACTIVE;

	UT_LIST_ADD_FIRST(trx_list, trx_sys->trx_list, trx);

	return(TRUE);
}

/********************************************************************
Starts a new transaction. 启动一个新事务。*/

ibool
trx_start(
/*======*/
			/* out: TRUE */
	trx_t* 	trx,	/* in: transaction */
	ulint	rseg_id)/* in: rollback segment id; if ULINT_UNDEFINED
			is passed, the system chooses the rollback segment
			automatically in a round-robin fashion */
{
	ibool	ret;
	
	mutex_enter(&kernel_mutex);

	ret = trx_start_low(trx, rseg_id);

	mutex_exit(&kernel_mutex);

	return(ret);
}

/********************************************************************
Commits a transaction. 提交一个事务。*/

void
trx_commit_off_kernel(
/*==================*/
	trx_t*	trx)	/* in: transaction */
{
	page_t*		update_hdr_page;
	dulint		lsn;
	trx_rseg_t*	rseg;
	trx_undo_t*	undo;
	ibool		must_flush_log	= FALSE;
	mtr_t		mtr;
	
	ut_ad(mutex_own(&kernel_mutex));

	rseg = trx->rseg;
	
	if (trx->insert_undo != NULL || trx->update_undo != NULL) {

		mutex_exit(&kernel_mutex);

		mtr_start(&mtr);
		
		must_flush_log = TRUE;

		/* Change the undo log segment states from TRX_UNDO_ACTIVE
		to some other state: these modifications to the file data
		structure define the transaction as committed in the file
		based world, at the serialization point of the log sequence
		number lsn obtained below. 将undo log段状态从TRX_UNDO_ACTIVE更改为其他一些状态:
		对文件数据结构的这些修改将事务定义为在基于文件的世界中提交的，在下面获得的日志序列号lsn的序列化点。*/
		mutex_enter(&(rseg->mutex));
			
		if (trx->insert_undo != NULL) {
			trx_undo_set_state_at_finish(trx, trx->insert_undo,
									&mtr);
		}

		undo = trx->update_undo;

		if (undo) {
			mutex_enter(&kernel_mutex);
#ifdef notdefined
			/* ########## There is a bug here: purge and rollback
			need the whole stack of old record versions even if no
			consistent read would need them!! This is because they
			decide on the basis of the old versions when we can
			remove delete marked secondary index records! 
			这里有一个bug:清除和回滚需要旧记录版本的整个堆栈，即使没有一致的读取需要它们!!这是因为他们决定在旧版本的基础上，我们可以删除删除标记的二级索引记录!*/
			if (!undo->del_marks && (undo->size == 1)
			    && (UT_LIST_GET_LEN(trx_sys->view_list) == 1)) {

			    	/* There is no need to save the update undo
			    	log: discard it; note that &mtr gets committed
			    	while we must hold the kernel mutex and
				therefore this optimization may add to the
				contention of the kernel mutex. 
				不需要保存更新的undo日志:丢弃它;注意&mtr在我们必须持有内核互斥锁时被提交，因此这个优化可能会增加内核互斥锁的争用。*/
			    	lsn = trx_undo_update_cleanup_by_discard(trx,
									&mtr);
				mutex_exit(&(rseg->mutex));

			    	goto shortcut;
			}
#endif
			trx->no = trx_sys_get_new_trx_no();
			
			mutex_exit(&kernel_mutex);

			/* It is not necessary to obtain trx->undo_mutex here
			because only a single OS thread is allowed to do the
			transaction commit for this transaction. 
			这里不需要获取trx->undo_mutex，因为只有一个操作系统线程被允许对这个事务进行事务提交。*/	
			update_hdr_page = trx_undo_set_state_at_finish(trx,
								undo, &mtr);

			/* We have to do the cleanup for the update log while
			holding the rseg mutex because update log headers
			have to be put to the history list in the order of
			the trx number. 我们必须在保留rseg互斥锁的同时对更新日志进行清理，因为更新日志头必须按trx号的顺序放到历史列表中。*/

			trx_undo_update_cleanup(trx, update_hdr_page, &mtr);
		}

		mutex_exit(&(rseg->mutex));

		/* Update the latest MySQL binlog name and offset info
		in trx sys header if MySQL binlogging is on 
		更新最新的MySQL binlog名称和偏移信息in trx sys header ，如果MySQL binlog是打开的*/

		if (trx->mysql_log_file_name) {
			trx_sys_update_mysql_binlog_offset(trx, &mtr);
		}
		
		/* If we did not take the shortcut, the following call
		commits the mini-transaction, making the whole transaction
		committed in the file-based world at this log sequence number;
		otherwise, we get the commit lsn from the call of
		trx_undo_update_cleanup_by_discard above.
		NOTE that transaction numbers, which are assigned only to
		transactions with an update undo log, do not necessarily come
		in exactly the same order as commit lsn's, if the transactions
		have different rollback segments. To get exactly the same
		order we should hold the kernel mutex up to this point,
		adding to to the contention of the kernel mutex. However, if
		a transaction T2 is able to see modifications made by
		a transaction T1, T2 will always get a bigger transaction
		number and a bigger commit lsn than T1. 
		如果我们没有采用这种捷径，下面的调用将提交迷你事务，使整个事务在基于文件的世界中以这个日志序号提交;
		否则，我们将从上面的trx_undo_update_cleanup_by_discard调用中获得提交lsn。
		注意，如果事务有不同的回滚段，则事务号(只分配给带有更新撤销日志的事务)不一定与提交lsn的顺序完全相同。
		为了获得完全相同的顺序，我们应该一直保存内核互斥锁，这就增加了内核互斥锁的争用。
		但是，如果事务T2能够看到事务T1所做的修改，那么事务T2总是会得到比事务T1更大的事务号和提交lsn。*/
		/*--------------*/
 		mtr_commit(&mtr);
 		/*--------------*/
 		lsn = mtr.end_lsn;

		mutex_enter(&kernel_mutex);
	}

	ut_ad(trx->conc_state == TRX_ACTIVE);
	ut_ad(mutex_own(&kernel_mutex));
	
	/* The following assignment makes the transaction committed in memory
	and makes its changes to data visible to other transactions.
	NOTE that there is a small discrepancy from the strict formal
	visibility rules here: a human user of the database can see
	modifications made by another transaction T even before the necessary
	log segment has been flushed to the disk. If the database happens to
	crash before the flush, the user has seen modifications from T which
	will never be a committed transaction. However, any transaction T2
	which sees the modifications of the committing transaction T, and
	which also itself makes modifications to the database, will get an lsn
	larger than the committing transaction T. In the case where the log
	flush fails, and T never gets committed, also T2 will never get
	committed. 下面的分配使事务提交到内存中，并使其对数据的更改对其他事务可见。
	注意，这里与严格的正式可见性规则有一点差异:数据库的人类用户可以看到另一个事务T所做的修改，甚至在必要的日志段被刷新到磁盘之前。
	如果数据库在刷新之前崩溃，用户看到了来自T的修改，这永远不会是一个已提交的事务。
	然而,任何事务T2将修改提交的事务,和也使修改数据库,将得到一个lsn大于提交事务T的情况下日志刷新失败,T2和T从来没有提交,也永远不会提交。*/

	/*--------------------------------------*/
	trx->conc_state = TRX_COMMITTED_IN_MEMORY;
	/*--------------------------------------*/

	lock_release_off_kernel(trx);

	if (trx->read_view) {
		read_view_close(trx->read_view);

		mem_heap_empty(trx->read_view_heap);
		trx->read_view = NULL;
	}

/*	printf("Trx %lu commit finished\n", ut_dulint_get_low(trx->id)); */

	if (must_flush_log) {

		mutex_exit(&kernel_mutex);
	
		if (trx->insert_undo != NULL) {

			trx_undo_insert_cleanup(trx);
		}

		/* NOTE that we could possibly make a group commit more
		efficient here: call os_thread_yield here to allow also other
		trxs to come to commit! 注意，我们可以让一个组提交更有效率:在这里调用os_thread_yield来允许其他trxs也来提交!*/

		/* We now flush the log, as the transaction made changes to
		the database, making the transaction committed on disk. It is
		enough that any one of the log groups gets written to disk. 
		当事务对数据库进行更改时，我们现在刷新日志，使事务在磁盘上提交。只要将任何一个日志组写入磁盘就足够了。*/
		/*-------------------------------------*/

		/* Only in some performance tests the variable srv_flush..
		will be set to FALSE: 只有在某些性能测试中，变量srv_flush..将被设置为FALSE:*/

		if (srv_flush_log_at_trx_commit) {
		
 			log_flush_up_to(lsn, LOG_WAIT_ONE_GROUP);
 		}

		/*-------------------------------------*/
	
		mutex_enter(&kernel_mutex);
	}

	trx->conc_state = TRX_NOT_STARTED;
	trx->rseg = NULL;
	trx->undo_no = ut_dulint_zero;
	trx->last_sql_stat_start.least_undo_no = ut_dulint_zero;

	ut_ad(UT_LIST_GET_LEN(trx->wait_thrs) == 0);
	ut_ad(UT_LIST_GET_LEN(trx->trx_locks) == 0);

	UT_LIST_REMOVE(trx_list, trx_sys->trx_list, trx);
}

/************************************************************************
Assigns a read view for a consistent read query. All the consistent reads
within the same transaction will get the same read view, which is created
when this function is first called for a new started transaction. 
为一致性读查询分配一个读视图。同一事务中的所有一致读取将获得相同的读视图，该视图是在为一个新启动的事务首次调用此函数时创建的。*/
read_view_t*
trx_assign_read_view(
/*=================*/
			/* out: consistent read view */
	trx_t*	trx)	/* in: active transaction */
{
	ut_ad(trx->conc_state == TRX_ACTIVE);

	if (trx->read_view) {
		return(trx->read_view);
	}
	
	mutex_enter(&kernel_mutex);

	if (!trx->read_view) {
		trx->read_view = read_view_open_now(trx, trx->read_view_heap);
	}

	mutex_exit(&kernel_mutex);
	
	return(trx->read_view);
}

/********************************************************************
Commits a transaction. NOTE that the kernel mutex is temporarily released.提交一个事务。说明内核互斥被暂时释放。 */
static
void
trx_handle_commit_sig_off_kernel(
/*=============================*/
	trx_t*		trx,		/* in: transaction */
	que_thr_t**	next_thr)	/* in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread 如果传入的值是一个指向NULL指针的指针，则调用函数可以开始运行一个新的查询线程*/
{
	trx_sig_t*	sig;
	trx_sig_t*	next_sig;
	
	ut_ad(mutex_own(&kernel_mutex));

	trx->que_state = TRX_QUE_COMMITTING;

	trx_commit_off_kernel(trx);

	ut_ad(UT_LIST_GET_LEN(trx->wait_thrs) == 0);

	/* Remove all TRX_SIG_COMMIT signals from the signal queue and send
	reply messages to them 从信号队列中删除所有TRX_SIG_COMMIT信号，并向它们发送回复消息*/

	sig = UT_LIST_GET_FIRST(trx->signals);

	while (sig != NULL) {
		next_sig = UT_LIST_GET_NEXT(signals, sig);

		if (sig->type == TRX_SIG_COMMIT) {

			trx_sig_reply(trx, sig, next_thr);
			trx_sig_remove(trx, sig);
		}

		sig = next_sig;
	}

	trx->que_state = TRX_QUE_RUNNING;
}

/***************************************************************
The transaction must be in the TRX_QUE_LOCK_WAIT state. Puts it to
the TRX_QUE_RUNNING state and releases query threads which were
waiting for a lock in the wait_thrs list. */
/*事务必须处于TRX_QUE_LOCK_WAIT状态。将其置为TRX_QUE_RUNNING状态，并释放wait_thrs列表中等待锁的查询线程。*/
void
trx_end_lock_wait(
/*==============*/
	trx_t*	trx)	/* in: transaction */
{
	que_thr_t*	thr;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(trx->que_state == TRX_QUE_LOCK_WAIT);
	
	thr = UT_LIST_GET_FIRST(trx->wait_thrs);

	while (thr != NULL) {
		que_thr_end_wait_no_next_thr(thr);

		UT_LIST_REMOVE(trx_thrs, trx->wait_thrs, thr);
			
		thr = UT_LIST_GET_FIRST(trx->wait_thrs);
	}

	trx->que_state = TRX_QUE_RUNNING;
}

/***************************************************************
Moves the query threads in the lock wait list to the SUSPENDED state and puts
the transaction to the TRX_QUE_RUNNING state. 
将锁等待列表中的查询线程移到SUSPENDED状态，并将事务移到TRX_QUE_RUNNING状态。*/
static
void
trx_lock_wait_to_suspended(
/*=======================*/
	trx_t*	trx)	/* in: transaction in the TRX_QUE_LOCK_WAIT state */
{
	que_thr_t*	thr;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(trx->que_state == TRX_QUE_LOCK_WAIT);
	
	thr = UT_LIST_GET_FIRST(trx->wait_thrs);

	while (thr != NULL) {
		thr->state = QUE_THR_SUSPENDED;
	
		UT_LIST_REMOVE(trx_thrs, trx->wait_thrs, thr);
			
		thr = UT_LIST_GET_FIRST(trx->wait_thrs);
	}

	trx->que_state = TRX_QUE_RUNNING;
}

/***************************************************************
Moves the query threads in the sig reply wait list of trx to the SUSPENDED
state.将trx的sig应答等待列表中的查询线程移动到SUSPENDED状态。 */
static
void
trx_sig_reply_wait_to_suspended(
/*============================*/
	trx_t*	trx)	/* in: transaction */
{
	trx_sig_t*	sig;
	que_thr_t*	thr;

	ut_ad(mutex_own(&kernel_mutex));
	
	sig = UT_LIST_GET_FIRST(trx->reply_signals);

	while (sig != NULL) {
		thr = sig->receiver;

		ut_ad(thr->state == QUE_THR_SIG_REPLY_WAIT);
		
		thr->state = QUE_THR_SUSPENDED;

		sig->receiver = NULL;
		sig->reply = FALSE;
	
		UT_LIST_REMOVE(reply_signals, trx->reply_signals, sig);
			
		sig = UT_LIST_GET_FIRST(trx->reply_signals);
	}
}

/*********************************************************************
Checks the compatibility of a new signal with the other signals in the
queue. 检查新信号与队列中其他信号的兼容性*/
static
ibool
trx_sig_is_compatible(
/*==================*/
			/* out: TRUE if the signal can be queued */
	trx_t*	trx,	/* in: trx handle */
	ulint	type,	/* in: signal type */
	ulint	sender)	/* in: TRX_SIG_SELF or TRX_SIG_OTHER_SESS */
{
	trx_sig_t*	sig;

	ut_ad(mutex_own(&kernel_mutex));

	if (UT_LIST_GET_LEN(trx->signals) == 0) {

		return(TRUE);
	}
	
	if (sender == TRX_SIG_SELF) {
		if (type == TRX_SIG_ERROR_OCCURRED) {

			return(TRUE);

		} else if (type == TRX_SIG_BREAK_EXECUTION) {

			return(TRUE);
		} else {
			return(FALSE);
		}
	}

	ut_ad(sender == TRX_SIG_OTHER_SESS);

	sig = UT_LIST_GET_FIRST(trx->signals);

	if (type == TRX_SIG_COMMIT) {
		while (sig != NULL) {

			if (sig->type == TRX_SIG_TOTAL_ROLLBACK) {

				return(FALSE);
			}

			sig = UT_LIST_GET_NEXT(signals, sig);
		}

 		return(TRUE);

	} else if (type == TRX_SIG_TOTAL_ROLLBACK) {
		while (sig != NULL) {

			if (sig->type == TRX_SIG_COMMIT) {

				return(FALSE);
			}

			sig = UT_LIST_GET_NEXT(signals, sig);
		}

		return(TRUE);

	} else if (type == TRX_SIG_BREAK_EXECUTION) {

		return(TRUE);		
	} else {
		ut_error;

		return(FALSE);
	}
}

/********************************************************************
Sends a signal to a trx object. 向trx对象发送一个信号。*/

ibool
trx_sig_send(
/*=========*/
					/* out: TRUE if the signal was
					successfully delivered */
	trx_t*		trx,		/* in: trx handle */
	ulint		type,		/* in: signal type */
	ulint		sender,		/* in: TRX_SIG_SELF or
					TRX_SIG_OTHER_SESS */
	ibool		reply,		/* in: TRUE if the sender of the signal
					wants reply after the operation induced
					by the signal is completed; if type
					is TRX_SIG_END_WAIT, this must be
					FALSE */
	que_thr_t*	receiver_thr,	/* in: query thread which wants the
					reply, or NULL */
	trx_savept_t* 	savept,		/* in: possible rollback savepoint, or
					NULL */
	que_thr_t**	next_thr)	/* in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread; if the parameter
					is NULL, it is ignored */
{
	trx_sig_t*	sig;
	trx_t*		receiver_trx;

	ut_ad(trx);
	ut_ad(mutex_own(&kernel_mutex));

	if (!trx_sig_is_compatible(trx, type, sender)) {
		/* The signal is not compatible with the other signals in
		the queue: do nothing 该信号与队列中的其他信号不兼容:不做任何事情*/

		ut_a(0);
		
		/* sess_raise_error_low(trx, 0, 0, NULL, NULL, NULL, NULL,
						"Incompatible signal"); */
		return(FALSE);
	}

	/* Queue the signal object 将信号对象排队*/

	if (UT_LIST_GET_LEN(trx->signals) == 0) {

		/* The signal list is empty: the 'sig' slot must be unused
		(we improve performance a bit by avoiding mem_alloc) 信号列表为空:“sig”槽必须未使用(我们通过避免mem_alloc来提高性能)*/
		sig = &(trx->sig);		
 	} else {
		/* It might be that the 'sig' slot is unused also in this
		case, but we choose the easy way of using mem_alloc 可能在这种情况下'sig'槽也没有使用，但是我们选择使用mem_alloc的简单方法 */
		 
		sig = mem_alloc(sizeof(trx_sig_t));
	}

	UT_LIST_ADD_LAST(signals, trx->signals, sig);

	sig->type = type;
	sig->state = TRX_SIG_WAITING;
	sig->sender = sender;
	sig->reply = reply;
	sig->receiver = receiver_thr;

	if (savept) {
		sig->savept = *savept;
	}

	if (receiver_thr) {
		receiver_trx = thr_get_trx(receiver_thr);

		UT_LIST_ADD_LAST(reply_signals, receiver_trx->reply_signals,
									sig);
	}

	if (trx->sess->state == SESS_ERROR) {
	
		trx_sig_reply_wait_to_suspended(trx);
	}

	if ((sender != TRX_SIG_SELF) || (type == TRX_SIG_BREAK_EXECUTION)) {

		/* The following call will add a TRX_SIG_ERROR_OCCURRED
		signal to the end of the queue, if the session is not yet
		in the error state: 可能在这种情况下'sig'槽也没有使用，但是我们选择使用mem_alloc的简单方法*/

		ut_a(0);

		sess_raise_error_low(trx, 0, 0, NULL, NULL, NULL, NULL,
		  "Signal from another session, or a break execution signal");
	}

	/* If there were no other signals ahead in the queue, try to start
	handling of the signal 如果队列中没有其他信号，尝试开始处理信号*/

	if (UT_LIST_GET_FIRST(trx->signals) == sig) {
	
		trx_sig_start_handle(trx, next_thr);
	}

	return(TRUE);
}

/********************************************************************
Ends signal handling. If the session is in the error state, and
trx->graph_before_signal_handling != NULL, then returns control to the error
handling routine of the graph (currently just returns the control to the
graph root which then will send an error message to the client). 
信号处理结束。如果会话处于错误状态，并且trx->graph_before_signal_handling != NULL，
则将控制权返回给图形的错误处理例程(当前只是将控制权返回给图形根，然后将错误消息发送给客户端)。*/
void
trx_end_signal_handling(
/*====================*/
	trx_t*	trx)	/* in: trx */
{
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(trx->handling_signals == TRUE);

	trx->handling_signals = FALSE;

	trx->graph = trx->graph_before_signal_handling;

	if (trx->graph && (trx->sess->state == SESS_ERROR)) {
			
		que_fork_error_handle(trx, trx->graph);
	}
}

/********************************************************************
Starts handling of a trx signal. 开始处理trx信号。*/

void
trx_sig_start_handle(
/*=================*/
	trx_t*		trx,		/* in: trx handle */
	que_thr_t**	next_thr)	/* in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread; if the parameter
					is NULL, it is ignored */
{
	trx_sig_t*	sig;
	ulint		type;
loop:
	/* We loop in this function body as long as there are queued signals
	we can process immediately 只要有可以立即处理的排队信号，我们就在函数体中进行循环*/

	ut_ad(trx);
	ut_ad(mutex_own(&kernel_mutex));

	if (trx->handling_signals && (UT_LIST_GET_LEN(trx->signals) == 0)) {

		trx_end_signal_handling(trx);
	
		return;
	}

	if (trx->conc_state == TRX_NOT_STARTED) {

		trx_start_low(trx, ULINT_UNDEFINED);
	}

	/* If the trx is in a lock wait state, moves the waiting query threads
	to the suspended state 如果trx处于锁等待状态，则将等待的查询线程移动到挂起状态*/

	if (trx->que_state == TRX_QUE_LOCK_WAIT) {
	
		trx_lock_wait_to_suspended(trx);
	}

	/* If the session is in the error state and this trx has threads
	waiting for reply from signals, moves these threads to the suspended
	state, canceling wait reservations; note that if the transaction has
	sent a commit or rollback signal to itself, and its session is not in
	the error state, then nothing is done here. 
	如果会话处于错误状态，并且该trx有线程在等待信号的应答，则将这些线程移到挂起状态，取消等待预留;
	注意，如果事务已经向自己发送了提交或回滚信号，并且它的会话没有处于错误状态，那么这里什么也不做。*/
	if (trx->sess->state == SESS_ERROR) {
		trx_sig_reply_wait_to_suspended(trx);
	}
	
	/* If there are no running query threads, we can start processing of a
	signal, otherwise we have to wait until all query threads of this
	transaction are aware of the arrival of the signal. 
	如果没有正在运行的查询线程，我们可以开始处理一个信号，否则我们必须等待，直到该事务的所有查询线程都知道信号的到达。*/
	if (trx->n_active_thrs > 0) {

		return;
	}

	if (trx->handling_signals == FALSE) {
		trx->graph_before_signal_handling = trx->graph;

		trx->handling_signals = TRUE;
	}

	sig = UT_LIST_GET_FIRST(trx->signals);
	type = sig->type;

	if (type == TRX_SIG_COMMIT) {

		trx_handle_commit_sig_off_kernel(trx, next_thr);

	} else if ((type == TRX_SIG_TOTAL_ROLLBACK)
				|| (type == TRX_SIG_ROLLBACK_TO_SAVEPT)) { 

		trx_rollback(trx, sig, next_thr);

		/* No further signals can be handled until the rollback
		completes, therefore we return 回滚完成之前不能处理任何其他信号，因此返回*/

		return;

	} else if (type == TRX_SIG_ERROR_OCCURRED) {

		trx_rollback(trx, sig, next_thr);

		/* No further signals can be handled until the rollback
		completes, therefore we return 回滚完成之前不能处理任何其他信号，因此返回*/

		return;

	} else if (type == TRX_SIG_BREAK_EXECUTION) {

		trx_sig_reply(trx, sig, next_thr);
		trx_sig_remove(trx, sig);
	} else {
		ut_error;
	}

	goto loop;
}			

/********************************************************************
Send the reply message when a signal in the queue of the trx has been
handled. 当trx队列中的信号被处理后发送应答消息。*/

void
trx_sig_reply(
/*==========*/
	trx_t*		trx,		/* in: trx handle */
	trx_sig_t*	sig,		/* in: signal */
	que_thr_t**	next_thr)	/* in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread */
{
	trx_t*	receiver_trx;

	ut_ad(trx && sig);
	ut_ad(mutex_own(&kernel_mutex));

	if (sig->reply && (sig->receiver != NULL)) {

		ut_ad((sig->receiver)->state == QUE_THR_SIG_REPLY_WAIT);

		receiver_trx = thr_get_trx(sig->receiver);

		UT_LIST_REMOVE(reply_signals, receiver_trx->reply_signals,
									sig);
		ut_ad(receiver_trx->sess->state != SESS_ERROR);
									
		que_thr_end_wait(sig->receiver, next_thr);

		sig->reply = FALSE;
		sig->receiver = NULL;

	} else if (sig->reply) {
		/* In this case the reply should be sent to the client of
		the session of the transaction 在这种情况下，应该将应答发送到事务会话的客户端*/

		sig->reply = FALSE;
		sig->receiver = NULL;

		sess_srv_msg_send_simple(trx->sess, SESS_SRV_SUCCESS,
						SESS_NOT_RELEASE_KERNEL);
	}
}

/********************************************************************
Removes a signal object from the trx signal queue. 从trx信号队列中移除一个信号对象。*/

void
trx_sig_remove(
/*===========*/
	trx_t*		trx,	/* in: trx handle */
	trx_sig_t*	sig)	/* in, own: signal */
{
	ut_ad(trx && sig);
	ut_ad(mutex_own(&kernel_mutex));

	ut_ad(sig->reply == FALSE);
	ut_ad(sig->receiver == NULL);

	UT_LIST_REMOVE(signals, trx->signals, sig);
	sig->type = 0;	/* reset the field to catch possible bugs 重置字段以捕获可能的bug*/

	if (sig != &(trx->sig)) {
		mem_free(sig);
	}
}

/*************************************************************************
Creates a commit command node struct. 创建一个提交命令节点结构。*/

commit_node_t*
commit_node_create(
/*===============*/
				/* out, own: commit node struct */
	mem_heap_t*	heap)	/* in: mem heap where created */
{
	commit_node_t*	node;

	node = mem_heap_alloc(heap, sizeof(commit_node_t));
	node->common.type  = QUE_NODE_COMMIT;
	node->state = COMMIT_NODE_SEND;
	
	return(node);
}

/***************************************************************
Performs an execution step for a commit type node in a query graph. 为查询图中的提交类型节点执行执行步骤。*/

que_thr_t*
trx_commit_step(
/*============*/
				/* out: query thread to run next, or NULL */
	que_thr_t*	thr)	/* in: query thread */
{
	commit_node_t*	node;
	que_thr_t*	next_thr;
	ibool		success;
	
	node = thr->run_node;

	ut_ad(que_node_get_type(node) == QUE_NODE_COMMIT);

	if (thr->prev_node == que_node_get_parent(node)) {
		node->state = COMMIT_NODE_SEND;
	}

	if (node->state == COMMIT_NODE_SEND) {
		mutex_enter(&kernel_mutex);

		node->state = COMMIT_NODE_WAIT;

		next_thr = NULL;
		
		thr->state = QUE_THR_SIG_REPLY_WAIT;

		/* Send the commit signal to the transaction 向事务发送提交信号*/
		
		success = trx_sig_send(thr_get_trx(thr), TRX_SIG_COMMIT,
					TRX_SIG_SELF, TRUE, thr, NULL,
					&next_thr);
		
		mutex_exit(&kernel_mutex);

		if (!success) {
			/* Error in delivering the commit signal 提交提交信号时出错*/
			que_thr_handle_error(thr, DB_ERROR, NULL, 0);
		}

		return(next_thr);
	}

	ut_ad(node->state == COMMIT_NODE_WAIT);
		
	node->state = COMMIT_NODE_SEND;
	
	thr->run_node = que_node_get_parent(node);

	return(thr);
}

/**************************************************************************
Does the transaction commit for MySQL. 是否为MySQL提交事务。*/

ulint
trx_commit_for_mysql(
/*=================*/
			/* out: 0 or error number */
	trx_t*	trx)	/* in: trx handle */
{
	/* Because we do not do the commit by sending an Innobase
	sig to the transaction, we must here make sure that trx has been
	started. 因为我们不会通过向事务发送Innobase签名来进行提交，所以我们必须确保trx已经启动。*/

	ut_a(trx);

	trx->op_info = "committing";
	
	trx_start_if_not_started(trx);

	mutex_enter(&kernel_mutex);

	trx_commit_off_kernel(trx);

	mutex_exit(&kernel_mutex);

	trx->op_info = "";
	
	return(0);
}

/**************************************************************************
Marks the latest SQL statement ended. 标记最新的SQL语句结束。*/

void
trx_mark_sql_stat_end(
/*==================*/
	trx_t*	trx)	/* in: trx handle */
{
	ut_a(trx);

	if (trx->conc_state == TRX_NOT_STARTED) {
		trx->undo_no = ut_dulint_zero;
	}

	trx->last_sql_stat_start.least_undo_no = trx->undo_no;
}

/**************************************************************************
Prints info about a transaction to the standard output. The caller must
own the kernel mutex. 将有关事务的信息打印到标准输出。调用者必须拥有内核互斥锁。*/

void
trx_print(
/*======*/
	trx_t*	trx)	/* in: transaction */
{
  	printf("TRANSACTION %lu %lu, OS thread id %lu",
		ut_dulint_get_high(trx->id),
	 	ut_dulint_get_low(trx->id),
	 	(ulint)trx->mysql_thread_id);

	if (ut_strlen(trx->op_info) > 0) {
		printf(" %s", trx->op_info);
	}
	
  	if (trx->type != TRX_USER) {
    		printf(" purge trx");
  	}
  	
  	switch (trx->conc_state) {
  		case TRX_NOT_STARTED:         printf(", not started"); break;
  		case TRX_ACTIVE:              printf(", active"); break;
  		case TRX_COMMITTED_IN_MEMORY: printf(", committed in memory");
									break;
  		default: printf(" state %lu", trx->conc_state);
  	}

  	switch (trx->que_state) {
  		case TRX_QUE_RUNNING:         printf(", runs or sleeps"); break;
  		case TRX_QUE_LOCK_WAIT:       printf(", lock wait"); break;
  		case TRX_QUE_ROLLING_BACK:    printf(", rolling back"); break;
  		case TRX_QUE_COMMITTING:      printf(", committing"); break;
  		default: printf(" que state %lu", trx->que_state);
  	}

  	if (0 < UT_LIST_GET_LEN(trx->trx_locks)) {
  		printf(", has %lu lock struct(s)",
				UT_LIST_GET_LEN(trx->trx_locks));
	}

  	if (trx->has_search_latch) {
  		printf(", holds adaptive hash latch");
  	}

	if (ut_dulint_cmp(trx->undo_no, ut_dulint_zero) != 0) {
		printf(", undo log entries %lu",
			ut_dulint_get_low(trx->undo_no));
	}
	
  	printf("\n");

  	if (trx->mysql_thd != NULL) {
    		innobase_mysql_print_thd(trx->mysql_thd);
  	}  
}
