/******************************************************
The server main program 服务器主程序

(c) 1995 Innobase Oy

Created 10/10/1995 Heikki Tuuri
*******************************************************/


#ifndef srv0srv_h
#define srv0srv_h

#include "univ.i"
#include "sync0sync.h"
#include "os0sync.h"
#include "com0com.h"
#include "que0types.h"
#include "trx0types.h"

/* Buffer which can be used in printing fatal error messages 可用于打印致命错误消息的缓冲区*/
extern char	srv_fatal_errbuf[];

/* When this event is set the lock timeout and InnoDB monitor
thread starts running 设置此事件后，锁超时，InnoDB监视器线程开始运行*/
extern os_event_t	srv_lock_timeout_thread_event;

/* Server parameters which are read from the initfile  从initfile中读取的服务器参数*/

extern char*	srv_data_home;
extern char*	srv_logs_home;
extern char*	srv_arch_dir;

extern ulint	srv_n_data_files;
extern char**	srv_data_file_names;
extern ulint*	srv_data_file_sizes;
extern ulint*   srv_data_file_is_raw_partition;

extern ibool	srv_created_new_raw;

#define SRV_NEW_RAW    1
#define SRV_OLD_RAW    2

extern char**	srv_log_group_home_dirs;

extern ulint	srv_n_log_groups;
extern ulint	srv_n_log_files;
extern ulint	srv_log_file_size;
extern ibool	srv_log_archive_on;
extern ulint	srv_log_buffer_size;
extern ibool	srv_flush_log_at_trx_commit;

extern byte	srv_latin1_ordering[256];/* The sort order table of the latin1
					character set */
extern ibool	srv_use_native_aio;		

extern ulint	srv_pool_size;
extern ulint	srv_mem_pool_size;
extern ulint	srv_lock_table_size;

extern ulint	srv_n_file_io_threads;

extern ibool	srv_archive_recovery;
extern dulint	srv_archive_recovery_limit_lsn;

extern ulint	srv_lock_wait_timeout;

extern char*    srv_unix_file_flush_method_str;
extern ulint    srv_unix_file_flush_method;
extern ulint	srv_force_recovery;
extern ulint	srv_thread_concurrency;

extern lint	srv_conc_n_threads;

extern ibool	srv_fast_shutdown;

extern ibool	srv_use_doublewrite_buf;

extern ibool    srv_set_thread_priorities;
extern int      srv_query_thread_priority;

/*-------------------------------------------*/

extern ulint	srv_n_rows_inserted;
extern ulint	srv_n_rows_updated;
extern ulint	srv_n_rows_deleted;
extern ulint	srv_n_rows_read;

extern ibool	srv_print_innodb_monitor;
extern ibool    srv_print_innodb_lock_monitor;
extern ibool    srv_print_innodb_tablespace_monitor;
extern ibool    srv_print_innodb_table_monitor;

extern ibool	srv_lock_timeout_and_monitor_active;
extern ibool	srv_error_monitor_active; 

extern ulint	srv_n_spin_wait_rounds;
extern ulint	srv_spin_wait_delay;
extern ibool	srv_priority_boost;
		
extern	ulint	srv_pool_size;
extern	ulint	srv_mem_pool_size;
extern	ulint	srv_lock_table_size;

extern	ulint	srv_sim_disk_wait_pct;
extern	ulint	srv_sim_disk_wait_len;
extern	ibool	srv_sim_disk_wait_by_yield;
extern	ibool	srv_sim_disk_wait_by_wait;

extern	ibool	srv_measure_contention;
extern	ibool	srv_measure_by_spin;
	
extern	ibool	srv_print_thread_releases;
extern	ibool	srv_print_lock_waits;
extern	ibool	srv_print_buf_io;
extern	ibool	srv_print_log_io;
extern	ibool	srv_print_parsed_sql;
extern	ibool	srv_print_latch_waits;

extern	ibool	srv_test_nocache;
extern	ibool	srv_test_cache_evict;

extern	ibool	srv_test_extra_mutexes;
extern	ibool	srv_test_sync;
extern	ulint	srv_test_n_threads;
extern	ulint	srv_test_n_loops;
extern	ulint	srv_test_n_free_rnds;
extern	ulint	srv_test_n_reserved_rnds;
extern	ulint	srv_test_n_mutexes;
extern	ulint	srv_test_array_size;

extern ulint	srv_activity_count;

extern mutex_t*	kernel_mutex_temp;/* mutex protecting the server, trx structs,
				query threads, and lock table: we allocate
				it from dynamic memory to get it to the
				same DRAM page as other hotspot semaphores 
				互斥锁保护服务器，trx结构体，查询线程，锁表:我们从动态内存中分配它，使其与其他热点信号量相同的DRAM页面*/
#define kernel_mutex (*kernel_mutex_temp)

#define SRV_MAX_N_IO_THREADS	100
				
/* Array of English strings describing the current state of an
i/o handler thread 描述i/o处理程序线程当前状态的英文字符串数组*/
extern char* srv_io_thread_op_info[];

typedef struct srv_sys_struct	srv_sys_t;

/* The server system 服务器系统*/
extern srv_sys_t*	srv_sys;

/* Alternatives for the field flush option in Unix; see the InnoDB manual about
what these mean Unix中字段刷新选项的替代方案;请参阅InnoDB手册了解这些含义*/
#define SRV_UNIX_FDATASYNC   1
#define SRV_UNIX_O_DSYNC     2
#define SRV_UNIX_LITTLESYNC  3
#define SRV_UNIX_NOSYNC      4

/* Alternatives for srv_force_recovery. Non-zero values are intended
to help the user get a damaged database up so that he can dump intact
tables and rows with SELECT INTO OUTFILE. The database must not otherwise
be used with these options! A bigger number below means that all precautions
of lower numbers are included. 
srv_force_recovery替代品。非零值旨在帮助用户恢复损坏的数据库，以便使用SELECT INTO OUTFILE转储完整的表和行。
否则，数据库不能与这些选项一起使用!下面的数字越大，意味着所有针对较低数字的预防措施都包括在内。*/
#define SRV_FORCE_IGNORE_CORRUPT 1	/* let the server run even if it
					detects a corrupt page 让服务器运行，即使它检测到一个损坏的页面*/
#define SRV_FORCE_NO_BACKGROUND	2 	/* prevent the main thread from
					running: if a crash would occur
					in purge, this prevents it 阻止主线程运行:如果在清除过程中发生崩溃，这将阻止它*/
#define SRV_FORCE_NO_TRX_UNDO	3	/* do not run trx rollback after
					recovery 恢复后不执行TRX回滚*/
#define SRV_FORCE_NO_IBUF_MERGE	4	/* prevent also ibuf operations:
					if they would cause a crash, better
					not do them 也要防止ibuf操作:如果它们会导致崩溃，最好不要做它们*/
#define	SRV_FORCE_NO_UNDO_LOG_SCAN 5	/* do not look at undo logs when
					starting the database: InnoDB will
					treat even incomplete transactions
					as committed 启动数据库时不要查看undo日志:InnoDB会将未完成的事务视为已提交*/
#define SRV_FORCE_NO_LOG_REDO	6	/* do not do the log roll-forward
					in connection with recovery 不执行与恢复相关的日志前滚*/
					
/*************************************************************************
Boots Innobase server. */

ulint
srv_boot(void);
/*==========*/
			/* out: DB_SUCCESS or error code */
/*************************************************************************
Gets the number of threads in the system. 获取系统中的线程数。*/

ulint
srv_get_n_threads(void);
/*===================*/
/*************************************************************************
Returns the calling thread type. 返回调用线程类型。*/

ulint
srv_get_thread_type(void);
/*=====================*/
			/* out: SRV_COM, ... */
/*************************************************************************
Releases threads of the type given from suspension in the thread table.
NOTE! The server mutex has to be reserved by the caller! 从线程表中释放给定类型的线程。注意!服务器互斥锁必须由调用者保留!*/

ulint
srv_release_threads(
/*================*/
			/* out: number of threads released: this may be
			< n if not enough threads were suspended at the
			moment */
	ulint	type,	/* in: thread type */
	ulint	n);	/* in: number of threads to release */
/*************************************************************************
The master thread controlling the server. 控制服务器的主线程。*/

#ifndef __WIN__
void*
#else
ulint
#endif
srv_master_thread(
/*==============*/
			/* out: a dummy parameter */
	void*	arg);	/* in: a dummy parameter required by
			os_thread_create */
/*************************************************************************
Reads a keyword and a value from a file. 从文件中读取关键字和值。*/

ulint
srv_read_init_val(
/*==============*/
				/* out: DB_SUCCESS or error code */
	FILE*	initfile,	/* in: file pointer */
	char*	keyword,	/* in: keyword before value(s), or NULL if
				no keyword read */
	char*	str_buf,	/* in/out: buffer for a string value to read,
				buffer size must be 10000 bytes, if NULL
				then not read */
	ulint*	num_val,	/* out:	numerical value to read, if NULL
				then not read */
	ibool	print_not_err);	/* in: if TRUE, then we will not print
				error messages to console */
/***********************************************************************
Tells the Innobase server that there has been activity in the database
and wakes up the master thread if it is suspended (not sleeping). Used
in the MySQL interface. Note that there is a small chance that the master
thread stays suspended (we do not protect our operation with the kernel
mutex, for performace reasons). 
告诉Innobase服务器数据库中有活动，并在主线程挂起(不是休眠)时唤醒主线程。在MySQL界面中使用。
注意，主线程保持挂起的可能性很小(出于性能原因，我们没有使用内核互斥锁来保护我们的操作)。*/
void
srv_active_wake_master_thread(void);
/*===============================*/
/***********************************************************************
Wakes up the master thread if it is suspended or being suspended. 如果主线程挂起或正在挂起，则唤醒它。*/

void
srv_wake_master_thread(void);
/*========================*/
/*************************************************************************
Puts an OS thread to wait if there are too many concurrent threads
(>= srv_thread_concurrency) inside InnoDB. The threads wait in a FIFO queue. 
如果InnoDB内部有太多并发线程(>= srv_thread_concurrency)，则设置OS线程等待。线程在FIFO队列中等待。*/
void
srv_conc_enter_innodb(
/*==================*/
	trx_t*	trx);	/* in: transaction object associated with the
			thread */
/*************************************************************************
This lets a thread enter InnoDB regardless of the number of threads inside
InnoDB. This must be called when a thread ends a lock wait.
这让一个线程进入InnoDB，而不管InnoDB内部有多少线程。当线程结束锁等待时必须调用此函数。 */
void
srv_conc_force_enter_innodb(
/*========================*/
	trx_t*	trx);	/* in: transaction object associated with the
			thread */
/*************************************************************************
This must be called when a thread exits InnoDB in a lock wait or at the
end of an SQL statement. 当线程在锁等待或SQL语句结束时退出InnoDB时，必须调用此函数。*/

void
srv_conc_force_exit_innodb(
/*=======================*/
	trx_t*	trx);	/* in: transaction object associated with the
			thread */
/*************************************************************************
This must be called when a thread exits InnoDB. 当线程退出InnoDB时必须调用这个函数。*/

void
srv_conc_exit_innodb(
/*=================*/
	trx_t*	trx);	/* in: transaction object associated with the
			thread */
/*******************************************************************
Puts a MySQL OS thread to wait for a lock to be released. 设置一个MySQL OS线程来等待锁被释放。*/

ibool
srv_suspend_mysql_thread(
/*=====================*/
				/* out: TRUE if the lock wait timeout was
				exceeded */
	que_thr_t*	thr);	/* in: query thread associated with
				the MySQL OS thread */
/************************************************************************
Releases a MySQL OS thread waiting for a lock to be released, if the
thread is already suspended. 释放一个正在等待锁被释放的MySQL OS线程，如果这个线程已经挂起。*/

void
srv_release_mysql_thread_if_suspended(
/*==================================*/
	que_thr_t*	thr);	/* in: query thread associated with the
				MySQL OS thread  */
/*************************************************************************
A thread which wakes up threads whose lock wait may have lasted too long.
This also prints the info output by various InnoDB monitors.
 唤醒锁等待时间过长的线程的线程。它还打印各种InnoDB监视器的信息输出。*/
#ifndef __WIN__
void*
#else
ulint
#endif
srv_lock_timeout_and_monitor_thread(
/*================================*/
			/* out: a dummy parameter */
	void*	arg);	/* in: a dummy parameter required by
			os_thread_create */
/*************************************************************************
A thread which prints warnings about semaphore waits which have lasted
too long. These can be used to track bugs which cause hangs.
 打印信号量等待时间过长警告的线程。这些可以用于跟踪导致挂起的bug。*/
#ifndef __WIN__
void*
#else
ulint
#endif
srv_error_monitor_thread(
/*=====================*/
			/* out: a dummy parameter */
	void*	arg);	/* in: a dummy parameter required by
			os_thread_create */


/* Types for the threads existing in the system. Threads of types 4 - 9
are called utility threads. Note that utility threads are mainly disk
bound, except that version threads 6 - 7 may also be CPU bound, if
cleaning versions from the buffer pool. 
系统中现有线程的类型。类型为4 - 9的线程称为实用线程。注意，实用程序线程主要是磁盘绑定的，除了版本线程6 - 7也可能是CPU绑定的，如果从缓冲池清理版本。*/
#define	SRV_COM		1	/* threads serving communication and queries 提供通信和查询服务的线程*/
#define	SRV_CONSOLE	2	/* thread serving console 线程服务控制台*/
#define	SRV_WORKER	3	/* threads serving parallelized queries and
				queries released from lock wait 服务于并行查询和从锁中释放的查询的线程等待*/
#define SRV_BUFFER	4	/* thread flushing dirty buffer blocks,
				not currently in use 线程刷新当前未使用的脏缓冲区块*/
#define SRV_RECOVERY	5	/* threads finishing a recovery,
				not currently in use 正在完成恢复的线程，当前未使用*/
#define SRV_INSERT	6	/* thread flushing the insert buffer to disk,
				not currently in use 将当前未使用的插入缓冲区刷新到磁盘的线程*/
#define SRV_MASTER	7      	/* the master thread, (whose type number must
				be biggest) 主线程，(其类型号必须是最大的)*/

/* Thread slot in the thread table 线程表中的线程槽*/
typedef struct srv_slot_struct	srv_slot_t;

/* Thread table is an array of slots 线程表是一个槽数组*/
typedef srv_slot_t	srv_table_t;

/* The server system struct 服务器系统结构*/
struct srv_sys_struct{
	os_event_t	operational;	/* created threads must wait for the
					server to become operational by
					waiting for this event 创建的线程必须通过等待此事件来等待服务器操作*/
	com_endpoint_t*	endpoint;	/* the communication endpoint of the
					server 服务器的通信端点*/

	srv_table_t*	threads;	/* server thread table */
	UT_LIST_BASE_NODE_T(que_thr_t)
			tasks;		/* task queue */
};

extern ulint	srv_n_threads_active[];

#endif
