/******************************************************
Recovery

(c) 1997 Innobase Oy

Created 9/20/1997 Heikki Tuuri
*******************************************************/
/*重做日志恢复*/
#ifndef log0recv_h
#define log0recv_h

#include "univ.i"
#include "ut0byte.h"
#include "page0types.h"
#include "hash0hash.h"
#include "log0log.h"

/***********************************************************************
Returns TRUE if recovery is currently running. */ /*如果恢复正在运行，则返回TRUE。*/
UNIV_INLINE
ibool
recv_recovery_is_on(void);
/*=====================*/
/***********************************************************************
Returns TRUE if recovery from backup is currently running. */ /*如果当前正在运行备份恢复，则返回TRUE。*/
UNIV_INLINE
ibool
recv_recovery_from_backup_is_on(void);
/*=================================*/
/****************************************************************************
Applies the hashed log records to the page, if the page lsn is less than the
lsn of a log record. This can be called when a buffer page has just been
read in, or also for a page already in the buffer pool. */
/*如果页面lsn小于某条日志记录的lsn，则将散列日志记录应用到该页面。
这可以在刚刚读入缓冲页时调用，也可以在缓冲池中已经读入的页调用。*/
void
recv_recover_page(
/*==============*/
	ibool	just_read_in,	/* in: TRUE if the i/o-handler calls this for
				a freshly read page */
	page_t*	page,		/* in: buffer page */
	ulint	space,		/* in: space id */
	ulint	page_no);	/* in: page number */
/************************************************************
Recovers from a checkpoint. When this function returns, the database is able
to start processing of new user transactions, but the function
recv_recovery_from_checkpoint_finish should be called later to complete
the recovery and free the resources used in it. */
/*从检查点恢复。当这个函数返回时，数据库能够开始处理新的用户事务，
但是稍后应该调用recv_recovery_from_checkpoint_finish函数来完成恢复并释放其中使用的资源。*/
ulint
recv_recovery_from_checkpoint_start(
/*================================*/
				/* out: error code or DB_SUCCESS */
	ulint	type,		/* in: LOG_CHECKPOINT or LOG_ARCHIVE */
	dulint	limit_lsn,	/* in: recover up to this lsn if possible */
	dulint	min_flushed_lsn,/* in: min flushed lsn from data files */
	dulint	max_flushed_lsn);/* in: max flushed lsn from data files */
/************************************************************
Completes recovery from a checkpoint. */
/*从检查点完成恢复。*/
void
recv_recovery_from_checkpoint_finish(void);
/*======================================*/
/***********************************************************
Scans log from a buffer and stores new log data to the parsing buffer. Parses
and hashes the log records if new data found. */
/*从缓冲区扫描日志，并将新的日志数据存储到解析缓冲区。如果发现新数据，则解析并散列日志记录。*/
ibool
recv_scan_log_recs(
/*===============*/
				/* out: TRUE if limit_lsn has been reached, or
				not able to scan any more in this log group */
	ibool	store_to_hash,	/* in: TRUE if the records should be stored
				to the hash table; this is set FALSE if just
				debug checking is needed */
	byte*	buf,		/* in: buffer containing a log segment or
				garbage */
	ulint	len,		/* in: buffer length */
	dulint	start_lsn,	/* in: buffer start lsn */
	dulint*	contiguous_lsn,	/* in/out: it is known that all log groups
				contain contiguous log data up to this lsn */ /*已知所有日志组都包含到该lsn为止的连续日志数据*/
	dulint*	group_scanned_lsn);/* out: scanning succeeded up to this lsn */
/**********************************************************
Resets the logs. The contents of log files will be lost! */
/*重置日志。日志文件的内容将丢失!*/
void
recv_reset_logs(
/*============*/
	dulint	lsn,		/* in: reset to this lsn rounded up to
				be divisible by OS_FILE_LOG_BLOCK_SIZE,
				after which we add LOG_BLOCK_HDR_SIZE */
	ulint	arch_log_no,	/* in: next archived log file number */
	ibool	new_logs_created);/* in: TRUE if resetting logs is done
				at the log creation; FALSE if it is done
				after archive recovery */
/************************************************************
Creates the recovery system. */
/*创建恢复系统。*/
void
recv_sys_create(void);
/*=================*/
/************************************************************
Inits the recovery system for a recovery operation. */
/*为恢复操作初始化恢复系统。*/
void
recv_sys_init(void);
/*===============*/
/***********************************************************************
Empties the hash table of stored log records, applying them to appropriate
pages. */
/*清空存储的日志记录的散列表，将它们应用到适当的页面。*/
void
recv_apply_hashed_log_recs(
/*=======================*/
	ibool	allow_ibuf);	/* in: if TRUE, also ibuf operations are
				allowed during the application; if FALSE,
				no ibuf operations are allowed, and after
				the application all file pages are flushed to
				disk and invalidated in buffer pool: this
				alternative means that no new log records
				can be generated during the application */
/************************************************************
Recovers from archived log files, and also from log files, if they exist. */
/*从归档日志文件中恢复，如果日志文件存在，也可以从日志文件中恢复。*/
ulint
recv_recovery_from_archive_start(
/*=============================*/
				/* out: error code or DB_SUCCESS */
	dulint	min_flushed_lsn,/* in: min flushed lsn field from the
				data files */
	dulint	limit_lsn,	/* in: recover up to this lsn if possible */
	ulint	first_log_no);	/* in: number of the first archived log file
				to use in the recovery; the file will be
				searched from INNOBASE_LOG_ARCH_DIR specified
				in server config file */
/************************************************************
Completes recovery from archive. */
/*完成归档恢复。*/
void
recv_recovery_from_archive_finish(void);
/*===================================*/
/***********************************************************************
Checks that a replica of a space is identical to the original space. */
/*检查空间的副本是否与原始空间完全相同。*/
void
recv_compare_spaces(
/*================*/
	ulint	space1,	/* in: space id */
	ulint	space2,	/* in: space id */
	ulint	n_pages);/* in: number of pages */
/***********************************************************************
Checks that a replica of a space is identical to the original space. Disables
ibuf operations and flushes and invalidates the buffer pool pages after the
test. This function can be used to check the recovery before dict or trx
systems are initialized. */
/*检查空间的副本是否与原始空间完全相同。在测试后禁用ibuf操作并刷新缓冲池页并使之失效。
此函数可用于在dict或trx系统初始化之前检查恢复情况。*/
void
recv_compare_spaces_low(
/*====================*/
	ulint	space1,	/* in: space id */
	ulint	space2,	/* in: space id */
	ulint	n_pages);/* in: number of pages */

/* Block of log record data */ /*日志记录数据块*/
typedef struct recv_data_struct	recv_data_t;
struct recv_data_struct{
	recv_data_t*	next;	/* pointer to the next block or NULL */
				/* the log record data is stored physically
				immediately after this struct, max amount
				RECV_DATA_BLOCK_SIZE bytes of it */ /*RECV_DATA_BLOCK_SIZE的最大值RECV_DATA_BLOCK_SIZE的最大字节数*/
};

/* Stored log record struct */ /*存储的日志记录结构*/
typedef struct recv_struct	recv_t;
struct recv_struct{
	byte		type;	/* log record type */
	ulint		len;	/* log record body length in bytes */
	recv_data_t*	data;	/* chain of blocks containing the log record
				body */ /*包含日志记录主体的区块链*/
	dulint		start_lsn;/* start lsn of the log segment written by
				the mtr which generated this log record: NOTE
				that this is not necessarily the start lsn of
				this log record */ /*start lsn产生此日志的mtr写入的日志段的起始lsn:注意，不一定是该日志的起始lsn*/
	dulint		end_lsn;/* end lsn of the log segment written by
				the mtr which generated this log record: NOTE
				that this is not necessarily the end lsn of
				this log record */ /*此日志记录的mtr写入的日志段的结束lsn:注意，不一定是此日志记录的结束lsn*/
	UT_LIST_NODE_T(recv_t)
			rec_list;/* list of log records for this page */
};

/* Hashed page file address struct *//*散列页面文件地址结构*/
typedef struct recv_addr_struct	recv_addr_t;
struct recv_addr_struct{
	ulint		state;	/* RECV_NOT_PROCESSED, RECV_BEING_PROCESSED,
				or RECV_PROCESSED */
	ulint		space;	/* space id */
	ulint		page_no;/* page number */
	UT_LIST_BASE_NODE_T(recv_t)
			rec_list;/* list of log records for this page */
	hash_node_t	addr_hash;
};

/* Recovery system data structure */ /*恢复系统数据结构*/
typedef struct recv_sys_struct	recv_sys_t;
struct recv_sys_struct{
	mutex_t		mutex;	/* mutex protecting the fields apply_log_recs,
				n_addrs, and the state field in each recv_addr
				struct */ /*互斥锁保护每个recv_addr结构体中的字段apply_log_recs、n_addr和state*/
	ibool		apply_log_recs;
				/* this is TRUE when log rec application to
				pages is allowed; this flag tells the
				i/o-handler if it should do log record
				application *//*当允许日志rec应用程序到页面时是这样的;
				这个标志告诉i/o处理程序是否应该执行日志记录应用程序*/
	ibool		apply_batch_on;
				/* this is TRUE when a log rec application
				batch is running */ /*当日志rec应用程序批处理运行时是这样的*/
	dulint		lsn;	/* log sequence number */
	ulint		last_log_buf_size;
				/* size of the log buffer when the database
				last time wrote to the log */ /*数据库最后一次写入日志时日志缓冲区的大小*/
	byte*		last_block;
				/* possible incomplete last recovered log
				block */ /*可能上次恢复的日志块不完整*/
	byte*		last_block_buf_start;
				/* the nonaligned start address of the
				preceding buffer */ /*前一个缓冲区的未对齐起始地址*/
	byte*		buf;	/* buffer for parsing log records */ /*用于解析日志记录的缓冲区*/
	ulint		len;	/* amount of data in buf */ /*buf中的数据量*/
	dulint		parse_start_lsn;
				/* this is the lsn from which we were able to
				start parsing log records and adding them to
				the hash table; ut_dulint_zero if a suitable
				start point not found yet *//*在这个lsn中，我们可以开始解析日志记录并将它们添加到哈希表中;
				如果还没有找到合适的起点，则使用ut_dulint_0*/
	dulint		scanned_lsn;
				/* the log data has been scanned up to this
				lsn */ /*日志数据已经扫描到这个lsn*/
	ulint		scanned_checkpoint_no;
				/* the log data has been scanned up to this
				checkpoint number (lowest 4 bytes) */ /*日志数据已经扫描到这个检查点编号(最低4字节)*/
	ulint		recovered_offset;
				/* start offset of non-parsed log records in
				buf */ /*buf中未解析日志记录的起始偏移量*/
	dulint		recovered_lsn;
				/* the log records have been parsed up to
				this lsn */ /*日志记录已经解析到这个lsn*/
	dulint		limit_lsn;/* recovery should be made at most up to this
				lsn */ /*应该最多恢复到这个lsn*/
	log_group_t*	archive_group;
				/* in archive recovery: the log group whose
				archive is read */ /*归档恢复:要读取归档的日志组*/
	mem_heap_t*	heap;	/* memory heap of log records and file
				addresses*/ /*日志记录和文件地址的内存堆*/
	hash_table_t*	addr_hash;/* hash table of file addresses of pages *//*页的文件地址的哈希表*/
	ulint		n_addrs;/* number of not processed hashed file
				addresses in the hash table */ /*哈希表中未处理的哈希文件地址的数目*/
};

extern recv_sys_t*	recv_sys;
extern ibool		recv_recovery_on;
extern ibool		recv_no_ibuf_operations;
extern ibool		recv_needed_recovery;

/* States of recv_addr_struct */
#define RECV_NOT_PROCESSED	71
#define RECV_BEING_READ		72
#define RECV_BEING_PROCESSED	73
#define RECV_PROCESSED		74

/* The number which is added to a space id to obtain the replicate space
in the debug version: spaces with an odd number as the id are replicate
spaces */ /*在调试版本中，添加到空间id中以获得复制空间的数字:id为奇数的空间为复制空间*/
#define RECV_REPLICA_SPACE_ADD	1

/* This many blocks must be left free in the buffer pool when we scan
the log and store the scanned log records in the buffer pool: we will
use these free blocks to read in pages when we start applying the
log records to the database. */
/*当我们扫描日志并将扫描到的日志记录存储在缓冲池中时，这许多块必须留在缓冲池中空闲:
当我们开始将日志记录应用到数据库中时，我们将使用这些空闲块来读取页面。*/
#define RECV_POOL_N_FREE_BLOCKS	 (ut_min(256, buf_pool_get_curr_size() / 8))

#ifndef UNIV_NONINL
#include "log0recv.ic"
#endif

#endif
