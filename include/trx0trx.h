/******************************************************
The transaction
事务
(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0trx_h
#define trx0trx_h

#include "univ.i"
#include "trx0types.h"
#include "lock0types.h"
#include "usr0types.h"
#include "que0types.h"
#include "mem0mem.h"
#include "read0types.h"

extern ulint	trx_n_mysql_transactions;

/************************************************************************
Releases the search latch if trx has reserved it.如果trx保留了搜索锁存器，则释放它。 */

void
trx_search_latch_release_if_reserved(
/*=================================*/
        trx_t*     trx); /* in: transaction */
/********************************************************************
Retrieves the error_info field from a trx. 从trx中检索error_info字段。*/

void*
trx_get_error_info(
/*===============*/
		     /* out: the error info */
	trx_t*  trx); /* in: trx object */
/********************************************************************
Creates and initializes a transaction object. 创建并初始化事务对象。*/

trx_t*
trx_create(
/*=======*/
			/* out, own: the transaction */
	sess_t*	sess);	/* in: session or NULL */
/************************************************************************
Creates a transaction object for MySQL. 为MySQL创建事务对象。*/

trx_t*
trx_allocate_for_mysql(void);
/*========================*/
				/* out, own: transaction object */
/************************************************************************
Creates a transaction object for background operations by the master thread. 为主线程的后台操作创建一个事务对象。*/

trx_t*
trx_allocate_for_background(void);
/*=============================*/
				/* out, own: transaction object */
/************************************************************************
Frees a transaction object. 释放事务对象。*/

void
trx_free(
/*=====*/
	trx_t*	trx);	/* in, own: trx object */
/************************************************************************
Frees a transaction object for MySQL. */

void
trx_free_for_mysql(
/*===============*/
	trx_t*	trx);	/* in, own: trx object */
/************************************************************************
Frees a transaction object of a background operation of the master thread. 释放主线程的后台操作的事务对象。*/

void
trx_free_for_background(
/*====================*/
	trx_t*	trx);	/* in, own: trx object */
/********************************************************************
Creates trx objects for transactions and initializes the trx list of
trx_sys at database start. Rollback segment and undo log lists must
already exist when this function is called, because the lists of
transactions to be rolled back or cleaned up are built based on the
undo log lists. 为事务创建trx对象，并在数据库启动时初始化trx_sys的trx列表。
调用此函数时，回滚段和撤销日志列表必须已经存在，因为要回滚或清理的事务列表是基于撤销日志列表构建的。*/
void
trx_lists_init_at_db_start(void);
/*============================*/
/********************************************************************
Starts a new transaction. 启动一个新事务。*/

ibool
trx_start(
/*======*/
			/* out: TRUE if success, FALSE if the rollback
			segment could not support this many transactions */
	trx_t* 	trx,	/* in: transaction */
	ulint	rseg_id);/* in: rollback segment id; if ULINT_UNDEFINED
			is passed, the system chooses the rollback segment
			automatically in a round-robin fashion */
/********************************************************************
Starts a new transaction. 启动一个新事务。*/

ibool
trx_start_low(
/*==========*/
			/* out: TRUE */
	trx_t* 	trx,	/* in: transaction */
	ulint	rseg_id);/* in: rollback segment id; if ULINT_UNDEFINED
			is passed, the system chooses the rollback segment
			automatically in a round-robin fashion */
/*****************************************************************
Starts the transaction if it is not yet started. 如果事务尚未启动，则启动事务。*/
UNIV_INLINE
void
trx_start_if_not_started(
/*=====================*/
	trx_t*	trx);	/* in: transaction */
/********************************************************************
Commits a transaction. 提交一个事务。*/

void
trx_commit_off_kernel(
/*==================*/
	trx_t*	trx);	/* in: transaction */
/**************************************************************************
Does the transaction commit for MySQL.是否为MySQL提交事务。 */

ulint
trx_commit_for_mysql(
/*=================*/
			/* out: 0 or error number */
	trx_t*	trx);	/* in: trx handle */
/**************************************************************************
Marks the latest SQL statement ended. 标记最新的SQL语句结束。*/

void
trx_mark_sql_stat_end(
/*==================*/
	trx_t*	trx);	/* in: trx handle */
/************************************************************************
Assigns a read view for a consistent read query. All the consistent reads
within the same transaction will get the same read view, which is created
when this function is first called for a new started transaction.为一致性读查询分配一个读视图。
同一事务中的所有一致读取将获得相同的读视图，该视图是在为一个新启动的事务首次调用此函数时创建的。 */
read_view_t*
trx_assign_read_view(
/*=================*/
			/* out: consistent read view */
	trx_t*	trx);	/* in: active transaction */
/***************************************************************
The transaction must be in the TRX_QUE_LOCK_WAIT state. Puts it to
the TRX_QUE_RUNNING state and releases query threads which were
waiting for a lock in the wait_thrs list. 
事务必须处于TRX_QUE_LOCK_WAIT状态。将其置为TRX_QUE_RUNNING状态，并释放wait_thrs列表中等待锁的查询线程。*/
void
trx_end_lock_wait(
/*==============*/
	trx_t*	trx);	/* in: transaction */
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
	que_thr_t**	next_thr);	/* in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread; if the parameter
					is NULL, it is ignored */
/********************************************************************
Send the reply message when a signal in the queue of the trx has
been handled. 当trx队列中的信号被处理后发送应答消息。*/

void
trx_sig_reply(
/*==========*/
	trx_t*		trx,		/* in: trx handle */
	trx_sig_t*	sig,		/* in: signal */
	que_thr_t**	next_thr);	/* in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread */
/********************************************************************
Removes the signal object from a trx signal queue. 从trx信号队列中移除信号对象。*/

void
trx_sig_remove(
/*===========*/
	trx_t*		trx,	/* in: trx handle */
	trx_sig_t*	sig);	/* in, own: signal */
/********************************************************************
Starts handling of a trx signal.开始处理trx信号。 */

void
trx_sig_start_handle(
/*=================*/
	trx_t*		trx,		/* in: trx handle */
	que_thr_t**	next_thr);	/* in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread */
/********************************************************************
Ends signal handling. If the session is in the error state, and
trx->graph_before_signal_handling != NULL, returns control to the error
handling routine of the graph (currently only returns the control to the
graph root which then sends an error message to the client).
 信号处理结束。如果会话处于错误状态，并且trx->graph_before_signal_handling != NULL，
 则将控制权返回给图形的错误处理例程(目前仅将控制权返回给图形根，然后由根向客户端发送错误消息)。*/
void
trx_end_signal_handling(
/*====================*/
	trx_t*	trx);	/* in: trx */
/*************************************************************************
Creates a commit command node struct. 创建一个提交命令节点结构。*/

commit_node_t*
commit_node_create(
/*===============*/
				/* out, own: commit node struct */
	mem_heap_t*	heap);	/* in: mem heap where created */
/***************************************************************
Performs an execution step for a commit type node in a query graph. 
为查询图中的提交类型节点执行执行步骤。*/
que_thr_t*
trx_commit_step(
/*============*/
				/* out: query thread to run next, or NULL */
	que_thr_t*	thr);	/* in: query thread */
/**************************************************************************
Prints info about a transaction to the standard output. The caller must
own the kernel mutex. 将有关事务的信息打印到标准输出。调用者必须拥有内核互斥锁。*/
void
trx_print(
/*======*/
	  trx_t* trx); /* in: transaction */


/* Signal to a transaction 向事务发送信号*/
struct trx_sig_struct{
	ulint		type;		/* signal type 信号类型*/
	ulint		state;		/* TRX_SIG_WAITING or
					TRX_SIG_BEING_HANDLED */
	ulint		sender;		/* TRX_SIG_SELF or
					TRX_SIG_OTHER_SESS */
	ibool		reply;		/* TRUE if the sender of the signal
					wants reply after the operation induced
					by the signal is completed; if this
					field is TRUE and the receiver field
					below is NULL, then a SUCCESS message
					is sent to the client of the session
					to which this trx belongs 
					如果信号的发送方在信号诱导的操作完成后需要应答，则为TRUE;如果这个字段为TRUE，并且下面的receiver字段为NULL，
					那么一个SUCCESS消息将被发送到这个trx所属的会话的客户端*/
	que_thr_t*	receiver;	/* query thread which wants the reply,
					or NULL 查询线程需要应答，或NULL*/
	trx_savept_t	savept;		/* possible rollback savepoint 可能保存点回滚*/
	UT_LIST_NODE_T(trx_sig_t)
			signals;	/* queue of pending signals to the
					transaction 事务的挂起信号队列*/
	UT_LIST_NODE_T(trx_sig_t)
			reply_signals;	/* list of signals for which the sender
					transaction is waiting a reply 发送方事务正在等待应答的信号列表*/
};

#define TRX_MAGIC_N	91118598

/* The transaction handle; every session has a trx object which is freed only
when the session is freed; in addition there may be session-less transactions
rolling back after a database recovery */
/*事务处理;每个会话都有一个TRX对象，该对象只有在会话被释放时才被释放;此外，在数据库恢复之后可能会出现无会话的事务回滚*/
struct trx_struct{
	ulint		magic_n;
	/* All the next fields are protected by the kernel mutex, except the
	undo logs which are protected by undo_mutex 所有接下来的字段都受到内核互斥锁的保护，只有undo日志受到undo_mutex的保护*/
	char*		op_info;	/* English text describing the
					current operation, or an empty
					string 描述当前操作的英文文本，或一个空字符串*/
	ulint		type;		/* TRX_USER, TRX_PURGE */
	ulint		conc_state;	/* state of the trx from the point
					of view of concurrency control:从并发控制的角度看TRX的状态:
					TRX_ACTIVE, TRX_COMMITTED_IN_MEMORY,
					... */
	dulint		id;		/* transaction id */
	dulint		no;		/* transaction serialization number ==
					max trx id when the transaction is 
					moved to COMMITTED_IN_MEMORY state */
	ibool		dict_operation;	/* TRUE if the trx is used to create
					a table, create an index, or drop a
					table 如果trx用于创建表、创建索引或删除表，则为TRUE*/
	dulint		table_id;	/* table id if the preceding field is
					TRUE 表id，如果前面的字段是TRUE*/
	/*------------------------------*/
        void*           mysql_thd;      /* MySQL thread handle corresponding
                                        to this trx, or NULL 对应于这个trx的MySQL线程句柄，或者NULL*/
	char*		mysql_log_file_name;
					/* If MySQL binlog is used, this field
					contains a pointer to the latest file
					name; this is NULL if binlog is not
					used 如果使用MySQL binlog，这个字段包含一个指向最新文件名的指针;如果不使用binlog，则为NULL*/
	ib_longlong	mysql_log_offset;/* If MySQL binlog is used, this field
					contains the end offset of the binlog
					entry 如果使用MySQL binlog，该字段包含binlog表项的结束偏移量*/
	os_thread_id_t	mysql_thread_id;/* id of the MySQL thread associated
					with this transaction object 与这个事务对象关联的MySQL线程的id*/
	/*------------------------------*/
	ulint		n_mysql_tables_in_use; /* number of Innobase tables
					used in the processing of the current
					SQL statement in MySQL 在MySQL中处理当前SQL语句时使用的Innobase表的个数*/
        ulint           mysql_n_tables_locked;
                                        /* how many tables the current SQL
					statement uses, except those
					in consistent read 当前SQL语句使用了多少表，除了那些一致读取*/
        ibool           has_search_latch;
			                /* TRUE if this trx has latched the
			                search system latch in S-mode 如果这个trx在s模式下锁存了搜索系统锁存，则为TRUE*/
	ulint		search_latch_timeout;
					/* If we notice that someone is
					waiting for our S-lock on the search
					latch to be released, we wait in
					row0sel.c for BTR_SEA_TIMEOUT new
					searches until we try to keep
					the search latch again over
					calls from MySQL; this is intended
					to reduce contention on the search
					latch 如果我们注意到有人在等待搜索锁存的S-lock被释放，我们会在row0sel.c中等待BTR_SEA_TIMEOUT的新搜索，
					直到我们试图在MySQL调用时再次保持搜索锁存;这是为了减少搜索锁存器上的争用*/
	/*------------------------------*/
	ibool		declared_to_be_inside_innodb;
					/* this is TRUE if we have declared
					this transaction in
					srv_conc_enter_innodb to be inside the
					InnoDB engine 如果我们在srv_conc_enter_innodb中声明了这个事务在InnoDB引擎中，这是TRUE*/
	ulint		n_tickets_to_enter_innodb;
					/* this can be > 0 only when
					declared_to_... is TRUE; when we come
					to srv_conc_innodb_enter, if the value
					here is > 0, we decrement this by 1 
					只有当declared_to_…是真的;当我们进入srv_conc_innodb_enter时，如果这里的值是> 0，我们将其减1*/ 
	/*------------------------------*/
	lock_t*		auto_inc_lock;	/* possible auto-inc lock reserved by
					the transaction; note that it is also
					in the lock list trx_locks 可能由事务保留的auto-inc锁;注意，它也在锁列表trx_locks中*/
        ibool           ignore_duplicates_in_insert;
                                        /* in an insert roll back only insert
                                        of the latest row in case
                                        of a duplicate key error 在插入回滚中，在出现重复的键错误时，只插入最近一行*/
	UT_LIST_NODE_T(trx_t)
			trx_list;	/* list of transactions 交易列表*/
	UT_LIST_NODE_T(trx_t)
			mysql_trx_list;	/* list of transactions created for
					MySQL MySQL创建的事务列表*/
	/*------------------------------*/
	mutex_t		undo_mutex;	/* mutex protecting the fields in this
					section (down to undo_no_arr), EXCEPT
					last_sql_stat_start, which can be
					accessed only when we know that there
					cannot be any activity in the undo
					logs! 除了last_sql_stat_start，只有当我们知道undo日志中没有任何活动时才能访问它!*/
	dulint		undo_no;	/* next undo log record number to
					assign 接下来撤销要分配的日志记录号*/
	trx_savept_t	last_sql_stat_start;
					/* undo_no when the last sql statement
					was started: in case of an error, trx
					is rolled back down to this undo
					number; see note at undo_mutex! 在最后一个SQL语句启动时撤销:在出现错误的情况下，TRX被回滚到这个撤销编号;参见undo_mutex!*/
	trx_rseg_t*	rseg;		/* rollback segment assigned to the
					transaction, or NULL if not assigned
					yet 回滚段分配给事务，如果没有分配则为NULL*/
	trx_undo_t*	insert_undo;	/* pointer to the insert undo log, or 
					NULL if no inserts performed yet 指针指向插入undo日志，如果还没有执行插入，则为NULL*/
	trx_undo_t* 	update_undo;	/* pointer to the update undo log, or
					NULL if no update performed yet 指针指向更新undo日志，如果没有执行更新，则为NULL*/
	dulint		roll_limit;	/* least undo number to undo during
					a rollback 回滚期间要撤销的最少撤销编号*/
	ulint		pages_undone;	/* number of undo log pages undone
					since the last undo log truncation 自上次撤消日志截断以来撤消的撤消日志页的数目*/
	trx_undo_arr_t*	undo_no_arr;	/* array of undo numbers of undo log
					records which are currently processed
					by a rollback operation 当前由回滚操作处理的undo日志记录的undo数目的数组*/
	/*------------------------------*/
	ulint		error_state;	/* 0 if no error, otherwise error
					number 0表示没有错误，否则错误编号*/
	void*		error_info;	/* if the error number indicates a
					duplicate key error, a pointer to
					the problematic index is stored here 如果错误号指示重复的键错误，则指向有问题的索引的指针会存储在这里*/
	sess_t*		sess;		/* session of the trx, NULL if none 如果没有，则为NULL*/
 	ulint		que_state;	/* TRX_QUE_RUNNING, TRX_QUE_LOCK_WAIT,
					... */
	que_t*		graph;		/* query currently run in the session,
					or NULL if none; NOTE that the query
					belongs to the session, and it can
					survive over a transaction commit, if
					it is a stored procedure with a COMMIT
					WORK statement, for instance 查询当前在会话中运行，如果没有则为NULL;
					注意，查询属于会话，如果它是一个带有commit WORK语句的存储过程，那么它可以通过事务提交存活下来*/
	ulint		n_active_thrs;	/* number of active query threads 活动查询线程数*/
	ibool		handling_signals;/* this is TRUE as long as the trx
					is handling signals 这是TRUE，只要trx正在处理信号*/
	que_t*		graph_before_signal_handling;
					/* value of graph when signal handling
					for this trx started: this is used to
					return control to the original query
					graph for error processing 启动此TRX的信号处理时图的值:用于将控制返回到原始查询图进行错误处理*/
	trx_sig_t	sig;		/* one signal object can be allocated
					in this space, avoiding mem_alloc 可以在这个空间中分配一个信号对象，避免mem_alloc*/
	UT_LIST_BASE_NODE_T(trx_sig_t)
			signals;	/* queue of processed or pending
					signals to the trx 发送到TRX的已处理或未决信号的队列*/
	UT_LIST_BASE_NODE_T(trx_sig_t)
			reply_signals;	/* list of signals sent by the query
					threads of this trx for which a thread
					is waiting for a reply; if this trx is
					killed, the reply requests in the list
					must be canceled 由该TRX的查询线程发送的信号列表，其中有一个线程正在等待应答;如果这个TRX被杀死，则必须取消列表中的应答请求*/
	/*------------------------------*/
	lock_t*		wait_lock;	/* if trx execution state is
					TRX_QUE_LOCK_WAIT, this points to
					the lock request, otherwise this is
					NULL 如果trx的执行状态是TRX_QUE_LOCK_WAIT，则指向锁请求，否则为NULL*/
	UT_LIST_BASE_NODE_T(que_thr_t)
			wait_thrs;	/* query threads belonging to this
					trx that are in the QUE_THR_LOCK_WAIT
					state 属于这个trx且处于QUE_THR_LOCK_WAIT状态的查询线程*/
	ulint		deadlock_mark;	/* a mark field used in deadlock
					checking algorithm 死锁检查算法中使用的标记字段*/
	/*------------------------------*/
	mem_heap_t*	lock_heap;	/* memory heap for the locks of the
					transaction 用于事务锁的内存堆*/
	UT_LIST_BASE_NODE_T(lock_t) 
			trx_locks;	/* locks reserved by the transaction 事务保留的锁*/
	/*------------------------------*/
	mem_heap_t*	read_view_heap;	/* memory heap for the read view 读取视图的内存堆*/
	read_view_t*	read_view;	/* consistent read view or NULL 一致性读视图或NULL*/
};

#define TRX_MAX_N_THREADS	32	/* maximum number of concurrent
					threads running a single operation of
					a transaction, e.g., a parallel query 运行事务的单个操作(例如并行查询)的最大并发线程数*/
/* Transaction types 事务类型*/
#define	TRX_USER		1	/* normal user transaction 普通用户事务*/
#define	TRX_PURGE		2	/* purge transaction: this is not
					inserted to the trx list of trx_sys
					and no rollback segment is assigned to
					this 清除事务:该事务不会被插入trx_sys的TRX列表中，并且没有回滚段被分配给该事务*/
/* Transaction concurrency states 事务并发状态*/
#define	TRX_NOT_STARTED		1
#define	TRX_ACTIVE		2
#define	TRX_COMMITTED_IN_MEMORY	3

/* Transaction execution states when trx state is TRX_ACTIVE 当trx状态为TRX_ACTIVE时，事务执行状态*/
#define TRX_QUE_RUNNING		1	/* transaction is running 事务运行*/
#define TRX_QUE_LOCK_WAIT	2	/* transaction is waiting for a lock 事务正在等待锁*/
#define TRX_QUE_ROLLING_BACK	3	/* transaction is rolling back 事务正在回滚*/
#define TRX_QUE_COMMITTING	4	/* transaction is committing 事务被提交*/

/* Types of a trx signal trx信号的类型*/
#define TRX_SIG_NO_SIGNAL		100
#define TRX_SIG_TOTAL_ROLLBACK		1
#define TRX_SIG_ROLLBACK_TO_SAVEPT	2
#define TRX_SIG_COMMIT			3
#define	TRX_SIG_ERROR_OCCURRED		4
#define TRX_SIG_BREAK_EXECUTION		5

/* Sender types of a signal 信号的发送方类型*/
#define TRX_SIG_SELF		1	/* sent by the session itself, or
					by an error occurring within this
					session 由会话本身发送，或由会话内发生的错误发送*/
#define TRX_SIG_OTHER_SESS	2	/* sent by another session (which
					must hold rights to this) 由另一个会话发送(该会话必须拥有此权限)*/
/* Signal states 信号状态*/
#define	TRX_SIG_WAITING		1
#define TRX_SIG_BEING_HANDLED	2
					
/* Commit command node in a query graph 查询图中的提交命令节点*/
struct commit_node_struct{
	que_common_t	common;	/* node type: QUE_NODE_COMMIT 节点类型:QUE_NODE_COMMIT*/
	ulint		state;	/* node execution state 节点执行状态*/
};

/* Commit node states 提交节点状态*/
#define COMMIT_NODE_SEND	1
#define COMMIT_NODE_WAIT	2


#ifndef UNIV_NONINL
#include "trx0trx.ic"
#endif

#endif 
