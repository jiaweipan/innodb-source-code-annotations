/******************************************************
Database log

(c) 1995-1997 InnoDB Oy

Created 12/9/1995 Heikki Tuuri
*******************************************************/
/*数据库重做日志*/
#include "log0log.h"

#ifdef UNIV_NONINL
#include "log0log.ic"
#endif

#include "mem0mem.h"
#include "buf0buf.h"
#include "buf0flu.h"
#include "srv0srv.h"
#include "log0recv.h"
#include "fil0fil.h"
#include "dict0boot.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0sys.h"
#include "trx0trx.h"

/* Global log system variable *//*全局日志系统变量*/
log_t*	log_sys	= NULL;

ibool	log_do_write = TRUE;
ibool	log_debug_writes = FALSE;

/* Pointer to this variable is used as the i/o-message when we do i/o to an
archive */ /*当我们对存档文件进行i/o操作时，指向这个变量的指针被用作i/o消息*/
byte	log_archive_io;

/* A margin for free space in the log buffer before a log entry is catenated */
/* 在连接日志条目之前，日志缓冲区中可用空间的空白*/
#define LOG_BUF_WRITE_MARGIN 	(4 * OS_FILE_LOG_BLOCK_SIZE)

/* Margins for free space in the log buffer after a log entry is catenated */
/*日志条目被连接后，日志缓冲区中空闲空间的空白*/
#define LOG_BUF_FLUSH_RATIO	2
#define LOG_BUF_FLUSH_MARGIN	(LOG_BUF_WRITE_MARGIN + 4 * UNIV_PAGE_SIZE)

/* Margin for the free space in the smallest log group, before a new query
step which modifies the database, is started */
/*启动修改数据库的新查询步骤之前，最小日志组中可用空间的空白*/
#define LOG_CHECKPOINT_FREE_PER_THREAD	(4 * UNIV_PAGE_SIZE)
#define LOG_CHECKPOINT_EXTRA_FREE	(8 * UNIV_PAGE_SIZE)

/* This parameter controls asynchronous making of a new checkpoint; the value
should be bigger than LOG_POOL_PREFLUSH_RATIO_SYNC */
/*该参数控制新检查点的异步生成;该值应该大于LOG_POOL_PREFLUSH_RATIO_SYNC*/
#define LOG_POOL_CHECKPOINT_RATIO_ASYNC	32

/* This parameter controls synchronous preflushing of modified buffer pages */
/* 此参数控制对已修改缓冲区页的同步预刷新 */
#define LOG_POOL_PREFLUSH_RATIO_SYNC	16

/* The same ratio for asynchronous preflushing; this value should be less than
the previous *//*相同的比例用于异步预刷新;这个值应该小于之前的值*/
#define LOG_POOL_PREFLUSH_RATIO_ASYNC	8

/* Extra margin, in addition to one log file, used in archiving */
/* 除日志文件外的额外空白，用于归档*/
#define LOG_ARCHIVE_EXTRA_MARGIN	(4 * UNIV_PAGE_SIZE)

/* This parameter controls asynchronous writing to the archive */
/* 该参数控制对存档的异步写入*/
#define LOG_ARCHIVE_RATIO_ASYNC		16

/* Codes used in unlocking flush latches */
#define LOG_UNLOCK_NONE_FLUSHED_LOCK	1
#define LOG_UNLOCK_FLUSH_LOCK		2

/* States of an archiving operation */
#define	LOG_ARCHIVE_READ	1
#define	LOG_ARCHIVE_WRITE	2

/**********************************************************
Completes a checkpoint write i/o to a log file. */ /*完成对日志文件的检查点写i/o。*/
static
void
log_io_complete_checkpoint(
/*=======================*/
	log_group_t*	group);	/* in: log group */
/**********************************************************
Completes an archiving i/o. */ /*完成一个归档i/o。*/
static
void
log_io_complete_archive(void);
/*=========================*/
/********************************************************************
Tries to establish a big enough margin of free space in the log groups, such
that a new log entry can be catenated without an immediate need for a
archiving. *//*尝试在日志组中建立足够大的空闲空间，以便可以连接新的日志条目，而不需要立即进行归档。*/
static
void
log_archive_margin(void);
/*====================*/


/********************************************************************
Returns the oldest modified block lsn in the pool, or log_sys->lsn if none
exists. *//*返回池中最旧的修改过的块lsn，如果不存在则返回log_sys->lsn。*/
static
dulint
log_buf_pool_get_oldest_modification(void)
/*======================================*/
{
	dulint	lsn;

	ut_ad(mutex_own(&(log_sys->mutex)));

	lsn = buf_pool_get_oldest_modification();

	if (ut_dulint_is_zero(lsn)) {

		lsn = log_sys->lsn;
	}

	return(lsn);
}

/****************************************************************
Opens the log for log_write_low. The log must be closed with log_close and
released with log_release. */
/*打开log_write_low的日志。日志必须用log_close关闭，用log_release释放。*/
dulint
log_reserve_and_open(
/*=================*/
			/* out: start lsn of the log record */
	ulint	len)	/* in: length of data to be catenated */
{
	log_t*	log			= log_sys;
	ulint	len_upper_limit;
	ulint	archived_lsn_age;
	ulint	count			= 0;
	ulint	dummy;
loop:
	mutex_enter(&(log->mutex));
	
	/* Calculate an upper limit for the space the string may take in the
	log buffer */
    /*计算字符串在日志缓冲区中可能占用的空间的上限*/
	len_upper_limit = LOG_BUF_WRITE_MARGIN + (5 * len) / 4;

	if (log->buf_free + len_upper_limit > log->buf_size) {

		mutex_exit(&(log->mutex));

		/* Not enough free space, do a syncronous flush of the log
		buffer */ /*如果没有足够的空闲空间，请同步刷新日志缓冲区*/
		log_flush_up_to(ut_dulint_max, LOG_WAIT_ALL_GROUPS);

		count++;

		ut_ad(count < 50);

		goto loop;
	}

	if (log->archiving_state != LOG_ARCH_OFF) {
	
		archived_lsn_age = ut_dulint_minus(log->lsn, log->archived_lsn);
	
		if (archived_lsn_age + len_upper_limit
						> log->max_archived_lsn_age) {
	
			/* Not enough free archived space in log groups: do a
			synchronous archive write batch: */
	        /*日志组中没有足够的空闲归档空间:执行同步归档写批处理:*/
			mutex_exit(&(log->mutex));
	
			ut_ad(len_upper_limit <= log->max_archived_lsn_age);
	
			log_archive_do(TRUE, &dummy);
	
			count++;
	
			ut_ad(count < 50);
	
			goto loop;
		}
	}

#ifdef UNIV_LOG_DEBUG
	log->old_buf_free = log->buf_free;
	log->old_lsn = log->lsn;
#endif	
	return(log->lsn);
}

/****************************************************************
Writes to the log the string given. It is assumed that the caller holds the
log mutex. */
/*将给定的字符串写入日志。假定调用者持有日志互斥锁。*/
void
log_write_low(
/*==========*/
	byte*	str,		/* in: string */
	ulint	str_len)	/* in: string length */
{
	log_t*	log	= log_sys;
	ulint	len;
	ulint	data_len;
	byte*	log_block;

	ut_ad(mutex_own(&(log->mutex)));
part_loop:
	/* Calculate a part length */
	/* 计算数据部分长度*/
	data_len = (log->buf_free % OS_FILE_LOG_BLOCK_SIZE) + str_len;

	if (data_len <= OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE) {

	    	/* The string fits within the current log block */
	        /*该字符串适合于当前日志块*/
	    	len = str_len;
	} else {
		data_len = OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE;
		
	    	len = OS_FILE_LOG_BLOCK_SIZE
			- (log->buf_free % OS_FILE_LOG_BLOCK_SIZE)
	    		- LOG_BLOCK_TRL_SIZE;
	}

	ut_memcpy(log->buf + log->buf_free, str, len);

	str_len -= len;
	str = str + len;

	log_block = ut_align_down(log->buf + log->buf_free,
						OS_FILE_LOG_BLOCK_SIZE);
	log_block_set_data_len(log_block, data_len);

	if (data_len == OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE) {
		/* This block became full */
		log_block_set_data_len(log_block, OS_FILE_LOG_BLOCK_SIZE);
		log_block_set_checkpoint_no(log_block,
						log_sys->next_checkpoint_no);
		len += LOG_BLOCK_HDR_SIZE + LOG_BLOCK_TRL_SIZE;

		log->lsn = ut_dulint_add(log->lsn, len);

		/* Initialize the next block header and trailer */
		log_block_init(log_block + OS_FILE_LOG_BLOCK_SIZE, log->lsn);
	} else {
		log->lsn = ut_dulint_add(log->lsn, len);
	}

	log->buf_free += len;

	ut_ad(log->buf_free <= log->buf_size);

	if (str_len > 0) {
		goto part_loop;
	}
}

/****************************************************************
Closes the log. */
/*关闭日志。*/
dulint
log_close(void)
/*===========*/
			/* out: lsn */
{
	byte*	log_block;
	ulint	first_rec_group;
	dulint	oldest_lsn;
	dulint	lsn;
	log_t*	log	= log_sys;

	ut_ad(mutex_own(&(log->mutex)));

	lsn = log->lsn;
	
	log_block = ut_align_down(log->buf + log->buf_free,
						OS_FILE_LOG_BLOCK_SIZE);
	first_rec_group = log_block_get_first_rec_group(log_block);

	if (first_rec_group == 0) {
		/* We initialized a new log block which was not written
		full by the current mtr: the next mtr log record group
		will start within this block at the offset data_len */
		/*我们初始化了一个没有被当前mtr写满的新日志块:
		下一个mtr日志记录组将从偏移量data_len处的这个块中开始*/
		log_block_set_first_rec_group(log_block,
			 		log_block_get_data_len(log_block));
	}

	if (log->buf_free > log->max_buf_free) {

		log->check_flush_or_checkpoint = TRUE;
	}

	if (ut_dulint_minus(lsn, log->last_checkpoint_lsn)
					<= log->max_modified_age_async) {
		goto function_exit;
	}

	oldest_lsn = buf_pool_get_oldest_modification();

	if (ut_dulint_is_zero(oldest_lsn)
	    || (ut_dulint_minus(lsn, oldest_lsn)
					> log->max_modified_age_async)
	    || (ut_dulint_minus(lsn, log->last_checkpoint_lsn)
					> log->max_checkpoint_age_async)) {

		log->check_flush_or_checkpoint = TRUE;
	}
function_exit:

#ifdef UNIV_LOG_DEBUG
	log_check_log_recs(log->buf + log->old_buf_free,
			log->buf_free - log->old_buf_free, log->old_lsn);
#endif

	return(lsn);
}

/**********************************************************
Pads the current log block full with dummy log records. Used in producing
consistent archived log files. */
/*用虚拟日志记录填充当前日志块。用于生成一致的归档日志文件。*/
static
void
log_pad_current_log_block(void)
/*===========================*/
{
	byte	b		= MLOG_DUMMY_RECORD;
	ulint	pad_length;
	ulint	i;
	dulint	lsn;
	
	/* We retrieve lsn only because otherwise gcc crashed on HP-UX */
	/* 我们检索lsn只是因为gcc在HP-UX上崩溃了*/
	lsn = log_reserve_and_open(OS_FILE_LOG_BLOCK_SIZE);

	pad_length = OS_FILE_LOG_BLOCK_SIZE
			- (log_sys->buf_free % OS_FILE_LOG_BLOCK_SIZE)
			- LOG_BLOCK_TRL_SIZE;

	for (i = 0; i < pad_length; i++) {
		log_write_low(&b, 1);
	}

	lsn = log_sys->lsn;
		
	log_close();
	log_release();

	ut_a((ut_dulint_get_low(lsn) % OS_FILE_LOG_BLOCK_SIZE)
						== LOG_BLOCK_HDR_SIZE);
}

/**********************************************************
Calculates the data capacity of a log group, when the log file headers are not
included. */
/*计算不包含日志文件头的日志组的数据容量。*/
ulint
log_group_get_capacity(
/*===================*/
				/* out: capacity in bytes */
	log_group_t*	group)	/* in: log group */
{
	ut_ad(mutex_own(&(log_sys->mutex)));

	return((group->file_size - LOG_FILE_HDR_SIZE) * group->n_files); 
}

/**********************************************************
Calculates the offset within a log group, when the log file headers are not
included. */
/*当不包括日志文件头时，计算日志组中的偏移量。*/
UNIV_INLINE
ulint
log_group_calc_size_offset(
/*=======================*/
				/* out: size offset (<= offset) */
	ulint		offset,	/* in: real offset within the log group */
	log_group_t*	group)	/* in: log group */
{
	ut_ad(mutex_own(&(log_sys->mutex)));

	return(offset - LOG_FILE_HDR_SIZE * (1 + offset / group->file_size));
}

/**********************************************************
Calculates the offset within a log group, when the log file headers are
included. */
/*当包含日志文件头时，计算日志组内的偏移量。*/
UNIV_INLINE
ulint
log_group_calc_real_offset(
/*=======================*/
				/* out: real offset (>= offset) */
	ulint		offset,	/* in: size offset within the log group */
	log_group_t*	group)	/* in: log group */
{
	ut_ad(mutex_own(&(log_sys->mutex)));

	return(offset + LOG_FILE_HDR_SIZE
		* (1 + offset / (group->file_size - LOG_FILE_HDR_SIZE)));
}

/**********************************************************
Calculates the offset of an lsn within a log group. */
/*计算lsn在日志组内的偏移量。*/
static
ulint
log_group_calc_lsn_offset(
/*======================*/
				/* out: offset within the log group */
	dulint		lsn,	/* in: lsn, must be within 4 GB of group->lsn */
	log_group_t*	group)	/* in: log group */
{
	dulint	gr_lsn;
	ulint	gr_lsn_size_offset;
	ulint	difference;
	ulint	group_size;
	ulint	offset;
	
	ut_ad(mutex_own(&(log_sys->mutex)));

	gr_lsn = group->lsn;

	gr_lsn_size_offset = log_group_calc_size_offset(group->lsn_offset,
									group);
	group_size = log_group_get_capacity(group);

	if (ut_dulint_cmp(lsn, gr_lsn) >= 0) {
			
		difference = ut_dulint_minus(lsn, gr_lsn);
	} else {
		difference = ut_dulint_minus(gr_lsn, lsn);

		difference = difference % group_size;

		difference = group_size - difference;
	}

	offset = (gr_lsn_size_offset + difference) % group_size;

	return(log_group_calc_real_offset(offset, group));
}

/************************************************************
Sets the field values in group to correspond to a given lsn. For this function
to work, the values must already be correctly initialized to correspond to
some lsn, for instance, a checkpoint lsn. */
/*将group中的字段值设置为对应于给定的lsn。要使该函数工作，这些值必须已经正确初始化，以对应于某些lsn，例如，检查点lsn。*/
void
log_group_set_fields(
/*=================*/
	log_group_t*	group,	/* in: group */
	dulint		lsn)	/* in: lsn for which the values should be
				set */
{
	group->lsn_offset = log_group_calc_lsn_offset(lsn, group);
	group->lsn = lsn;
}

/*********************************************************************
Calculates the recommended highest values for lsn - last_checkpoint_lsn,
lsn - buf_get_oldest_modification(), and lsn - max_archive_lsn_age. */
/*计算lsn - last_checkpoint_lsn、lsn - buf_get_oldest_modification()和lsn - max_archive_lsn_age的推荐最大值。*/
static
ibool
log_calc_max_ages(void)
/*===================*/
			/* out: error value FALSE if the smallest log group is
			too small to accommodate the number of OS threads in
			the database server */
{
	log_group_t*	group;
	ulint		n_threads;
	ulint		margin;
	ulint		free;
	ibool		success		= TRUE;
	ulint		smallest_capacity;	
	ulint		archive_margin;
	ulint		smallest_archive_margin;

	ut_ad(!mutex_own(&(log_sys->mutex)));

	n_threads = srv_get_n_threads();

	mutex_enter(&(log_sys->mutex));

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	ut_ad(group);

	smallest_capacity = ULINT_MAX;
	smallest_archive_margin = ULINT_MAX;

	while (group) {
		if (log_group_get_capacity(group) < smallest_capacity) {

			smallest_capacity = log_group_get_capacity(group);
		}

		archive_margin = log_group_get_capacity(group)
				- (group->file_size - LOG_FILE_HDR_SIZE)
				- LOG_ARCHIVE_EXTRA_MARGIN;

		if (archive_margin < smallest_archive_margin) {

			smallest_archive_margin = archive_margin;
		}
	
		group = UT_LIST_GET_NEXT(log_groups, group);
	}
	
	/* For each OS thread we must reserve so much free space in the
	smallest log group that it can accommodate the log entries produced
	by single query steps: running out of free log space is a serious
	system error which requires rebooting the database. */
	/*对于每个操作系统线程，我们必须在最小的日志组中预留足够的空闲空间，
	以容纳单个查询步骤产生的日志条目:空闲日志空间耗尽是一个严重的系统错误，需要重新启动数据库。*/
	free = LOG_CHECKPOINT_FREE_PER_THREAD * n_threads
						+ LOG_CHECKPOINT_EXTRA_FREE;
	if (free >= smallest_capacity / 2) {
		success = FALSE;

		goto failure;
	} else {
		margin = smallest_capacity - free;
	}

	margin = ut_min(margin, log_sys->adm_checkpoint_interval);

	log_sys->max_modified_age_async = margin
				- margin / LOG_POOL_PREFLUSH_RATIO_ASYNC;
	log_sys->max_modified_age_sync = margin
				- margin / LOG_POOL_PREFLUSH_RATIO_SYNC;

	log_sys->max_checkpoint_age_async = margin - margin
					/ LOG_POOL_CHECKPOINT_RATIO_ASYNC;
	log_sys->max_checkpoint_age = margin;

	log_sys->max_archived_lsn_age = smallest_archive_margin;

	log_sys->max_archived_lsn_age_async = smallest_archive_margin
						- smallest_archive_margin /
						  LOG_ARCHIVE_RATIO_ASYNC;
failure:
	mutex_exit(&(log_sys->mutex));

	if (!success) {
		fprintf(stderr,
	  "Error: log file group too small for the number of threads\n");
	}

	return(success);
}

/**********************************************************
Initializes the log. */
/*初始化重做日志*/
void
log_init(void)
/*==========*/
{
	byte*	buf;

	log_sys = mem_alloc(sizeof(log_t));

	mutex_create(&(log_sys->mutex));
	mutex_set_level(&(log_sys->mutex), SYNC_LOG);

	mutex_enter(&(log_sys->mutex));

	/* Start the lsn from one log block from zero: this way every
	log record has a start lsn != zero, a fact which we will use */
	/*从一个日志块开始，从0开始lsn:这样，每条日志记录都有一个起始lsn != 0，我们将使用这个事实*/
	log_sys->lsn = LOG_START_LSN;

	ut_a(LOG_BUFFER_SIZE >= 16 * OS_FILE_LOG_BLOCK_SIZE);
	ut_a(LOG_BUFFER_SIZE >= 4 * UNIV_PAGE_SIZE);

	buf = ut_malloc(LOG_BUFFER_SIZE + OS_FILE_LOG_BLOCK_SIZE);
	log_sys->buf = ut_align(buf, OS_FILE_LOG_BLOCK_SIZE);

	log_sys->buf_size = LOG_BUFFER_SIZE;

	memset(log_sys->buf, '\0', LOG_BUFFER_SIZE);

	log_sys->max_buf_free = log_sys->buf_size / LOG_BUF_FLUSH_RATIO
				- LOG_BUF_FLUSH_MARGIN;
	log_sys->check_flush_or_checkpoint = TRUE;
	UT_LIST_INIT(log_sys->log_groups);

	log_sys->n_log_ios = 0;	

	log_sys->n_log_ios_old = log_sys->n_log_ios;
	log_sys->last_printout_time = time(NULL);
	/*----------------------------*/
	
	log_sys->buf_next_to_write = 0;

	log_sys->written_to_some_lsn = log_sys->lsn;
	log_sys->written_to_all_lsn = log_sys->lsn;
	
	log_sys->n_pending_writes = 0;

	log_sys->no_flush_event = os_event_create(NULL);

	os_event_set(log_sys->no_flush_event);

	log_sys->one_flushed_event = os_event_create(NULL);

	os_event_set(log_sys->one_flushed_event);

	/*----------------------------*/
	log_sys->adm_checkpoint_interval = ULINT_MAX;

	log_sys->next_checkpoint_no = ut_dulint_zero;
	log_sys->last_checkpoint_lsn = log_sys->lsn;
	log_sys->n_pending_checkpoint_writes = 0; 

	rw_lock_create(&(log_sys->checkpoint_lock));
	rw_lock_set_level(&(log_sys->checkpoint_lock), SYNC_NO_ORDER_CHECK);

	log_sys->checkpoint_buf = ut_align(
				mem_alloc(2 * OS_FILE_LOG_BLOCK_SIZE),
						OS_FILE_LOG_BLOCK_SIZE);
	memset(log_sys->checkpoint_buf, '\0', OS_FILE_LOG_BLOCK_SIZE);
	/*----------------------------*/

	log_sys->archiving_state = LOG_ARCH_ON;
	log_sys->archived_lsn = log_sys->lsn;
	log_sys->next_archived_lsn = ut_dulint_zero;

	log_sys->n_pending_archive_ios = 0;

	rw_lock_create(&(log_sys->archive_lock));
	rw_lock_set_level(&(log_sys->archive_lock), SYNC_NO_ORDER_CHECK);

	log_sys->archive_buf = ut_align(
				ut_malloc(LOG_ARCHIVE_BUF_SIZE
					  + OS_FILE_LOG_BLOCK_SIZE),
						OS_FILE_LOG_BLOCK_SIZE);
	log_sys->archive_buf_size = LOG_ARCHIVE_BUF_SIZE;

	memset(log_sys->archive_buf, '\0', LOG_ARCHIVE_BUF_SIZE);

	log_sys->archiving_on = os_event_create(NULL);

	/*----------------------------*/
	
	log_sys->online_backup_state = FALSE;

	/*----------------------------*/
	
	log_block_init(log_sys->buf, log_sys->lsn);
	log_block_set_first_rec_group(log_sys->buf, LOG_BLOCK_HDR_SIZE);

	log_sys->buf_free = LOG_BLOCK_HDR_SIZE;
	log_sys->lsn = ut_dulint_add(LOG_START_LSN, LOG_BLOCK_HDR_SIZE);
	
	mutex_exit(&(log_sys->mutex));

#ifdef UNIV_LOG_DEBUG
	recv_sys_create();
	recv_sys_init();

	recv_sys->parse_start_lsn = log_sys->lsn;
	recv_sys->scanned_lsn = log_sys->lsn;
	recv_sys->scanned_checkpoint_no = 0;
	recv_sys->recovered_lsn = log_sys->lsn;
	recv_sys->limit_lsn = ut_dulint_max;
#endif
}

/**********************************************************************
Inits a log group to the log system. */
/*向日志系统初始化日志组。*/
void
log_group_init(
/*===========*/
	ulint	id,			/* in: group id */
	ulint	n_files,		/* in: number of log files */
	ulint	file_size,		/* in: log file size in bytes */
	ulint	space_id,		/* in: space id of the file space
					which contains the log files of this
					group */
	ulint	archive_space_id)	/* in: space id of the file space
					which contains some archived log
					files for this group; currently, only
					for the first log group this is
					used */
{
	ulint	i;
	
	log_group_t*	group;

	group = mem_alloc(sizeof(log_group_t));

	group->id = id;
	group->n_files = n_files;
	group->file_size = file_size;
	group->space_id = space_id;
	group->state = LOG_GROUP_OK;
	group->lsn = LOG_START_LSN;
	group->lsn_offset = LOG_FILE_HDR_SIZE;
	group->n_pending_writes = 0;

	group->file_header_bufs = mem_alloc(sizeof(byte*) * n_files);
	group->archive_file_header_bufs = mem_alloc(sizeof(byte*) * n_files);

	for (i = 0; i < n_files; i++) {
		*(group->file_header_bufs + i) = ut_align(
			mem_alloc(LOG_FILE_HDR_SIZE + OS_FILE_LOG_BLOCK_SIZE),
						OS_FILE_LOG_BLOCK_SIZE);
		*(group->archive_file_header_bufs + i) = ut_align(
			mem_alloc(LOG_FILE_HDR_SIZE + OS_FILE_LOG_BLOCK_SIZE),
						OS_FILE_LOG_BLOCK_SIZE);
	}
	
	group->archive_space_id = archive_space_id;

	group->archived_file_no = 0;
	group->archived_offset = 0;

	group->checkpoint_buf = ut_align(
				mem_alloc(2 * OS_FILE_LOG_BLOCK_SIZE),
						OS_FILE_LOG_BLOCK_SIZE);
	
	UT_LIST_ADD_LAST(log_groups, log_sys->log_groups, group);

	ut_a(log_calc_max_ages());
}	
	
/**********************************************************************
Does the unlockings needed in flush i/o completion. */
/*完成flush i/o所需的解锁。*/
UNIV_INLINE
void
log_flush_do_unlocks(
/*=================*/
	ulint	code)	/* in: any ORed combination of LOG_UNLOCK_FLUSH_LOCK
			and LOG_UNLOCK_NONE_FLUSHED_LOCK */
{
	ut_ad(mutex_own(&(log_sys->mutex)));

	/* NOTE that we must own the log mutex when doing the setting of the
	events: this is because transactions will wait for these events to
	be set, and at that moment the log flush they were waiting for must
	have ended. If the log mutex were not reserved here, the i/o-thread
	calling this function might be preempted for a while, and when it
	resumed execution, it might be that a new flush had been started, and
	this function would erroneously signal the NEW flush as completed.
		Thus, the changes in the state of these events are performed
	atomically in conjunction with the changes in the state of
	log_sys->n_pending_writes etc. */ 
	/*注意，在设置事件时，我们必须拥有日志互斥锁:这是因为事务将等待这些事件被设置，
	而在那一刻，它们所等待的日志刷新一定已经结束。如果这里没有保留日志互斥锁，
	调用这个函数的i/o线程可能会被抢占一段时间，当它恢复执行时，可能是启动了一个新的刷新，
	而这个函数将错误地发出新刷新已经完成的信号。因此，这些事件状态的更改与log_sys->n_pending_writes等状态的更改一起以原子方式执行。*/

	if (code & LOG_UNLOCK_NONE_FLUSHED_LOCK) {
		os_event_set(log_sys->one_flushed_event);
	}

	if (code & LOG_UNLOCK_FLUSH_LOCK) {
		os_event_set(log_sys->no_flush_event);
	}
}

/**********************************************************************
Checks if a flush is completed for a log group and does the completion
routine if yes. */
/*检查日志组的刷新是否完成，如果完成，则执行完成例程。*/
UNIV_INLINE
ulint
log_group_check_flush_completion(
/*=============================*/
				/* out: LOG_UNLOCK_NONE_FLUSHED_LOCK or 0 */
	log_group_t*	group)	/* in: log group */
{
	ut_ad(mutex_own(&(log_sys->mutex)));	

	if (!log_sys->one_flushed && (group->n_pending_writes == 0)) {

		if (log_debug_writes) {
			printf("Log flushed first to group %lu\n", group->id);
		}
	
		log_sys->written_to_some_lsn = log_sys->flush_lsn;
		log_sys->one_flushed = TRUE;

		return(LOG_UNLOCK_NONE_FLUSHED_LOCK);
	}

	if (log_debug_writes && (group->n_pending_writes == 0)) {

		printf("Log flushed to group %lu\n", group->id);
	}

	return(0);
}

/**********************************************************
Checks if a flush is completed and does the completion routine if yes. */
/*检查刷新是否完成，如果完成则执行完成例程。*/
static
ulint
log_sys_check_flush_completion(void)
/*================================*/
			/* out: LOG_UNLOCK_FLUSH_LOCK or 0 */
{
	ulint	move_start;
	ulint	move_end;

	ut_ad(mutex_own(&(log_sys->mutex)));

	if (log_sys->n_pending_writes == 0) {
	
		log_sys->written_to_all_lsn = log_sys->flush_lsn;
		log_sys->buf_next_to_write = log_sys->flush_end_offset;

		if (log_sys->flush_end_offset > log_sys->max_buf_free / 2) {
			/* Move the log buffer content to the start of the
			buffer */
			/*将日志缓冲区的内容移动到缓冲区的开头*/
			move_start = ut_calc_align_down(
						log_sys->flush_end_offset,
						OS_FILE_LOG_BLOCK_SIZE);
			move_end = ut_calc_align(log_sys->buf_free,
						OS_FILE_LOG_BLOCK_SIZE);

			ut_memmove(log_sys->buf, log_sys->buf + move_start,
						move_end - move_start);
			log_sys->buf_free -= move_start;
		
			log_sys->buf_next_to_write -= move_start;
		}

		return(LOG_UNLOCK_FLUSH_LOCK);
	}

	return(0);
}

/**********************************************************
Completes an i/o to a log file. */
/*完成对日志文件的一次i/o操作。*/
void
log_io_complete(
/*============*/
	log_group_t*	group)	/* in: log group or a dummy pointer */
{
	ulint	unlock;

	if ((byte*)group == &log_archive_io) {
		/* It was an archive write */

		log_io_complete_archive();

		return;
	}

	if ((ulint)group & 0x1) {
		/* It was a checkpoint write */
		group = (log_group_t*)((ulint)group - 1);

		if (srv_unix_file_flush_method != SRV_UNIX_O_DSYNC
		   && srv_unix_file_flush_method != SRV_UNIX_NOSYNC) {
		
		        fil_flush(group->space_id);
		}

		log_io_complete_checkpoint(group);

		return;
	}

	if (srv_unix_file_flush_method != SRV_UNIX_O_DSYNC
	    && srv_unix_file_flush_method != SRV_UNIX_NOSYNC) {

	        fil_flush(group->space_id);
	}

	mutex_enter(&(log_sys->mutex));

	ut_ad(group->n_pending_writes > 0);
	ut_ad(log_sys->n_pending_writes > 0);
	
	group->n_pending_writes--;
	log_sys->n_pending_writes--;

	unlock = log_group_check_flush_completion(group);
	unlock = unlock | log_sys_check_flush_completion();
	
	log_flush_do_unlocks(unlock);

	mutex_exit(&(log_sys->mutex));
}

/**********************************************************
Writes a log file header to a log file space. */
/*将日志文件头写入日志文件空间。*/
static
void
log_group_file_header_flush(
/*========================*/
	ulint		type,		/* in: LOG_FLUSH or LOG_RECOVER */
	log_group_t*	group,		/* in: log group */
	ulint		nth_file,	/* in: header to the nth file in the
					log file space */ /*日志文件空间中的第n个文件*/
	dulint		start_lsn)	/* in: log file data starts at this
					lsn */
{
	byte*	buf;
	ulint	dest_offset;
	ibool	sync;

	ut_ad(mutex_own(&(log_sys->mutex)));

	ut_a(nth_file < group->n_files);

	buf = *(group->file_header_bufs + nth_file);

	mach_write_to_4(buf + LOG_GROUP_ID, group->id);
	mach_write_to_8(buf + LOG_FILE_START_LSN, start_lsn);

	dest_offset = nth_file * group->file_size;

	sync = FALSE;

	if (type == LOG_RECOVER) {

		sync = TRUE;
	}
	
	if (log_debug_writes) {
		printf(
		"Writing log file header to group %lu file %lu\n", group->id,
								nth_file);
	}

	if (log_do_write) {
		if (type == LOG_FLUSH) {
			log_sys->n_pending_writes++;
			group->n_pending_writes++;
		}

		log_sys->n_log_ios++;	
		
		fil_io(OS_FILE_WRITE | OS_FILE_LOG, sync, group->space_id,
				dest_offset / UNIV_PAGE_SIZE,
				dest_offset % UNIV_PAGE_SIZE,
				OS_FILE_LOG_BLOCK_SIZE,
				buf, group);
	}
}

/**********************************************************
Writes a buffer to a log file group. */
/*向日志文件组写入缓冲区。*/
void
log_group_write_buf(
/*================*/
	ulint		type,		/* in: LOG_FLUSH or LOG_RECOVER */
	log_group_t*	group,		/* in: log group */
	byte*		buf,		/* in: buffer */
	ulint		len,		/* in: buffer len; must be divisible
					by OS_FILE_LOG_BLOCK_SIZE */
	dulint		start_lsn,	/* in: start lsn of the buffer; must
					be divisible by
					OS_FILE_LOG_BLOCK_SIZE */
	ulint		new_data_offset)/* in: start offset of new data in
					buf: this parameter is used to decide
					if we have to write a new log file
					header *//*buf中新数据的起始偏移量:这个参数用于决定我们是否必须写入一个新的日志文件头*/
{
	ulint	write_len;
	ibool	sync;
	ibool	write_header;
	ulint	next_offset;

	ut_ad(mutex_own(&(log_sys->mutex)));
	ut_ad(len % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_ad(ut_dulint_get_low(start_lsn) % OS_FILE_LOG_BLOCK_SIZE == 0);

	sync = FALSE;

	if (type == LOG_RECOVER) {

		sync = TRUE;
	}

	if (new_data_offset == 0) {
		write_header = TRUE;
	} else {
		write_header = FALSE;
	}
loop:
	if (len == 0) {

		return;
	}

	next_offset = log_group_calc_lsn_offset(start_lsn, group);

	if ((next_offset % group->file_size == LOG_FILE_HDR_SIZE)
	   						&& write_header) {
		/* We start to write a new log file instance in the group */
        /* 我们开始在组中写入一个新的日志文件实例*/
		log_group_file_header_flush(type, group,
				next_offset / group->file_size, start_lsn);
	}

	if ((next_offset % group->file_size) + len > group->file_size) {

		write_len = group->file_size - (next_offset % group->file_size);
	} else {
		write_len = len;
	}
	
	if (log_debug_writes) {
		printf(
		"Writing log file segment to group %lu offset %lu len %lu\n",
					group->id, next_offset, write_len);
	}

	if (log_do_write) {
		if (type == LOG_FLUSH) {
			log_sys->n_pending_writes++;
			group->n_pending_writes++;
		}

		log_sys->n_log_ios++;	

		fil_io(OS_FILE_WRITE | OS_FILE_LOG, sync, group->space_id,
			next_offset / UNIV_PAGE_SIZE,
			next_offset % UNIV_PAGE_SIZE, write_len, buf, group);
	}

	if (write_len < len) {
		start_lsn = ut_dulint_add(start_lsn, write_len);
		len -= write_len;
		buf += write_len;

		write_header = TRUE;

		goto loop;
	}
}

/**********************************************************
This function is called, e.g., when a transaction wants to commit. It checks
that the log has been flushed to disk up to the last log entry written by the
transaction. If there is a flush running, it waits and checks if the flush
flushed enough. If not, starts a new flush. */
/*这个函数被调用，例如，当一个事务想要提交时。它检查日志是否已刷新到磁盘，直到事务写入的最后一个日志条目。
如果有一个刷新正在运行，它将等待并检查刷新是否足够。如果没有，开始新的flush。*/
void
log_flush_up_to(
/*============*/
	dulint	lsn,	/* in: log sequence number up to which the log should
			be flushed, ut_dulint_max if not specified */
	ulint	wait)	/* in: LOG_NO_WAIT, LOG_WAIT_ONE_GROUP,
			or LOG_WAIT_ALL_GROUPS */
{
	log_group_t*	group;
	ulint		start_offset;
	ulint		end_offset;
	ulint		area_start;
	ulint		area_end;
	ulint		loop_count;

	if (recv_no_ibuf_operations) {
		/* Recovery is running and no operations on the log files are
		allowed yet (the variable name .._no_ibuf_.. is misleading) */
		/*恢复正在运行，不允许对日志文件进行任何操作(变量名.._no_ibuf_..是误导)*/
		return;
	}

	loop_count = 0;
loop:
	loop_count++;

	ut_ad(loop_count < 5);

	if (loop_count > 2) {
/*		printf("Log loop count %lu\n", loop_count); */
	}
	
	mutex_enter(&(log_sys->mutex));

	if ((ut_dulint_cmp(log_sys->written_to_all_lsn, lsn) >= 0)
	    	|| ((ut_dulint_cmp(log_sys->written_to_some_lsn, lsn) >= 0)
	        			&& (wait != LOG_WAIT_ALL_GROUPS))) {

		mutex_exit(&(log_sys->mutex));

		return;
	}
	
	if (log_sys->n_pending_writes > 0) {
		/* A flush is running */

		if (ut_dulint_cmp(log_sys->flush_lsn, lsn) >= 0) {
			/* The flush will flush enough: wait for it to
			complete  */
			/*冲水将足够冲水:等待它完成*/
			goto do_waits;
		}
		
		mutex_exit(&(log_sys->mutex));

		/* Wait for the flush to complete and try to start a new
		flush */
        /*等待刷新完成并尝试开始新的刷新*/
		os_event_wait(log_sys->no_flush_event);

		goto loop;
	}

	if (log_sys->buf_free == log_sys->buf_next_to_write) {
		/* Nothing to flush */

		mutex_exit(&(log_sys->mutex));

		return;
	}

	if (log_debug_writes) {
		printf("Flushing log from %lu %lu up to lsn %lu %lu\n",
			ut_dulint_get_high(log_sys->written_to_all_lsn),
			ut_dulint_get_low(log_sys->written_to_all_lsn),
					ut_dulint_get_high(log_sys->lsn),
					ut_dulint_get_low(log_sys->lsn));
	}

	os_event_reset(log_sys->no_flush_event);
	os_event_reset(log_sys->one_flushed_event);

	start_offset = log_sys->buf_next_to_write;
	end_offset = log_sys->buf_free;

	area_start = ut_calc_align_down(start_offset, OS_FILE_LOG_BLOCK_SIZE);
	area_end = ut_calc_align(end_offset, OS_FILE_LOG_BLOCK_SIZE);

	ut_ad(area_end - area_start > 0);

	log_sys->flush_lsn = log_sys->lsn;
	log_sys->one_flushed = FALSE;
	
	log_block_set_flush_bit(log_sys->buf + area_start, TRUE);
	log_block_set_checkpoint_no(
			log_sys->buf + area_end - OS_FILE_LOG_BLOCK_SIZE,
			log_sys->next_checkpoint_no);
	
	/* Copy the last, incompletely written, log block a log block length
	up, so that when the flush operation writes from the log buffer, the
	segment to write will not be changed by writers to the log */
	/*将最后一个未写完的日志块的长度向上复制，这样当刷新操作从日志缓冲区写入时，写入者不会更改要写的段到日志*/
	ut_memcpy(log_sys->buf + area_end,
			log_sys->buf + area_end - OS_FILE_LOG_BLOCK_SIZE,
			OS_FILE_LOG_BLOCK_SIZE);

	log_sys->buf_free += OS_FILE_LOG_BLOCK_SIZE;
	log_sys->flush_end_offset = log_sys->buf_free;

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (group) {
		log_group_write_buf(LOG_FLUSH, group,
			log_sys->buf + area_start,
			area_end - area_start,
			ut_dulint_align_down(log_sys->written_to_all_lsn,
						OS_FILE_LOG_BLOCK_SIZE),
			start_offset - area_start);

		log_group_set_fields(group, log_sys->flush_lsn);
						
		group = UT_LIST_GET_NEXT(log_groups, group);
	}

do_waits:
	mutex_exit(&(log_sys->mutex));

	if (wait == LOG_WAIT_ONE_GROUP) {
		os_event_wait(log_sys->one_flushed_event);
	} else if (wait == LOG_WAIT_ALL_GROUPS) {
		os_event_wait(log_sys->no_flush_event);
	} else {
		ut_ad(wait == LOG_NO_WAIT);
	}			
}

/********************************************************************
Tries to establish a big enough margin of free space in the log buffer, such
that a new log entry can be catenated without an immediate need for a flush. */
/*尝试在日志缓冲区中建立足够大的空闲空间，以便可以连接新的日志条目，而不需要立即刷新。*/
static
void
log_flush_margin(void)
/*==================*/
{
	ibool	do_flush	= FALSE;
	log_t*	log		= log_sys;

	mutex_enter(&(log->mutex));

	if (log->buf_free > log->max_buf_free) {
		
		if (log->n_pending_writes > 0) {
			/* A flush is running: hope that it will provide enough
			free space */ /*正在刷新:希望它能提供足够的空闲空间*/
		} else {
			do_flush = TRUE;
		}
	}

	mutex_exit(&(log->mutex));

	if (do_flush) {
		log_flush_up_to(ut_dulint_max, LOG_NO_WAIT);
	}
}

/********************************************************************
Advances the smallest lsn for which there are unflushed dirty blocks in the
buffer pool. NOTE: this function may only be called if the calling thread owns
no synchronization objects! */
/*推进缓冲池中有未刷新脏块的最小lsn。注意:这个函数只能在调用线程没有同步对象的情况下调用!*/
ibool
log_preflush_pool_modified_pages(
/*=============================*/
				/* out: FALSE if there was a flush batch of
				the same type running, which means that we
				could not start this flush batch */
	dulint	new_oldest,	/* in: try to advance oldest_modified_lsn
				at least to this lsn */
	ibool	sync)		/* in: TRUE if synchronous operation is
				desired */
{
	ulint	n_pages;

	if (recv_recovery_on) {
		/* If the recovery is running, we must first apply all
		log records to their respective file pages to get the
		right modify lsn values to these pages: otherwise, there
		might be pages on disk which are not yet recovered to the
		current lsn, and even after calling this function, we could
		not know how up-to-date the disk version of the database is,
		and we could not make a new checkpoint on the basis of the
		info on the buffer pool only. */
	    /*如果正在运行恢复，我们必须首先将所有日志记录应用到它们各自的文件页面，以获得这些页面正确的修改lsn值:
		否则,可能存在磁盘上的页面,还没有恢复到当前的lsn,甚至在调用这个函数,我们无法知道最新的磁盘版本的数据库,
		我们不能新建一个检查点的信息的基础上缓冲池。*/
		recv_apply_hashed_log_recs(TRUE);
	}

	n_pages = buf_flush_batch(BUF_FLUSH_LIST, ULINT_MAX, new_oldest);

	if (sync) {
		buf_flush_wait_batch_end(BUF_FLUSH_LIST);
	}

	if (n_pages == ULINT_UNDEFINED) {

		return(FALSE);
	}

	return(TRUE);
}

/**********************************************************
Completes a checkpoint. */ /*完成一个检查点。*/
static
void
log_complete_checkpoint(void)
/*=========================*/
{
	ut_ad(mutex_own(&(log_sys->mutex)));
	ut_ad(log_sys->n_pending_checkpoint_writes == 0);

	log_sys->next_checkpoint_no
			= ut_dulint_add(log_sys->next_checkpoint_no, 1);

	log_sys->last_checkpoint_lsn = log_sys->next_checkpoint_lsn;

	rw_lock_x_unlock_gen(&(log_sys->checkpoint_lock), LOG_CHECKPOINT);
}

/**********************************************************
Completes an asynchronous checkpoint info write i/o to a log file. */ /*完成异步检查点信息写i/o到日志文件。*/
static
void
log_io_complete_checkpoint(
/*=======================*/
	log_group_t*	group)	/* in: log group */
{
	mutex_enter(&(log_sys->mutex));

	ut_ad(log_sys->n_pending_checkpoint_writes > 0);
	
	log_sys->n_pending_checkpoint_writes--;

	if (log_debug_writes) {
		printf("Checkpoint info written to group %lu\n", group->id);
	}

	if (log_sys->n_pending_checkpoint_writes == 0) {
		log_complete_checkpoint();
	}	
	
	mutex_exit(&(log_sys->mutex));
}

/***********************************************************************
Writes info to a checkpoint about a log group. */
/*将信息写入关于日志组的检查点。*/
static
void
log_checkpoint_set_nth_group_info(
/*==============================*/
	byte*	buf,	/* in: buffer for checkpoint info */
	ulint	n,	/* in: nth slot */
	ulint	file_no,/* in: archived file number */
	ulint	offset)	/* in: archived file offset */
{
	ut_ad(n < LOG_MAX_N_GROUPS);

	mach_write_to_4(buf + LOG_CHECKPOINT_GROUP_ARRAY
			+ 8 * n + LOG_CHECKPOINT_ARCHIVED_FILE_NO, file_no);
	mach_write_to_4(buf + LOG_CHECKPOINT_GROUP_ARRAY
			+ 8 * n + LOG_CHECKPOINT_ARCHIVED_OFFSET, offset);
}

/***********************************************************************
Gets info from a checkpoint about a log group. */
/*从检查点获取关于日志组的信息。*/
void
log_checkpoint_get_nth_group_info(
/*==============================*/
	byte*	buf,	/* in: buffer containing checkpoint info */
	ulint	n,	/* in: nth slot */
	ulint*	file_no,/* out: archived file number */
	ulint*	offset)	/* out: archived file offset */
{
	ut_ad(n < LOG_MAX_N_GROUPS);

	*file_no = mach_read_from_4(buf + LOG_CHECKPOINT_GROUP_ARRAY
				+ 8 * n + LOG_CHECKPOINT_ARCHIVED_FILE_NO);
	*offset = mach_read_from_4(buf + LOG_CHECKPOINT_GROUP_ARRAY
				+ 8 * n + LOG_CHECKPOINT_ARCHIVED_OFFSET);
}

/**********************************************************
Writes the checkpoint info to a log group header. */
/*将检查点信息写入日志组标头。*/
static
void
log_group_checkpoint(
/*=================*/
	log_group_t*	group)	/* in: log group */
{
	log_group_t*	group2;
	dulint	archived_lsn;
	dulint	next_archived_lsn;
	ulint	write_offset;
	ulint	fold;
	byte*	buf;
	ulint	i;

	ut_ad(mutex_own(&(log_sys->mutex)));
	ut_ad(LOG_CHECKPOINT_SIZE <= OS_FILE_LOG_BLOCK_SIZE);
	
	buf = group->checkpoint_buf;
	
	mach_write_to_8(buf + LOG_CHECKPOINT_NO, log_sys->next_checkpoint_no);
	mach_write_to_8(buf + LOG_CHECKPOINT_LSN, log_sys->next_checkpoint_lsn);

	mach_write_to_4(buf + LOG_CHECKPOINT_OFFSET,
			log_group_calc_lsn_offset(
					log_sys->next_checkpoint_lsn, group));
								
	mach_write_to_4(buf + LOG_CHECKPOINT_LOG_BUF_SIZE, log_sys->buf_size);

	if (log_sys->archiving_state == LOG_ARCH_OFF) {
		archived_lsn = ut_dulint_max;
	} else {
		archived_lsn = log_sys->archived_lsn;

		if (0 != ut_dulint_cmp(archived_lsn,
					log_sys->next_archived_lsn)) {
			next_archived_lsn = log_sys->next_archived_lsn;
			/* For debugging only */
		}	
	}
		
	mach_write_to_8(buf + LOG_CHECKPOINT_ARCHIVED_LSN, archived_lsn);

	for (i = 0; i < LOG_MAX_N_GROUPS; i++) {
		log_checkpoint_set_nth_group_info(buf, i, 0, 0);
	}
	
	group2 = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (group2) {
		log_checkpoint_set_nth_group_info(buf, group2->id,
						group2->archived_file_no,
						group2->archived_offset);

		group2 = UT_LIST_GET_NEXT(log_groups, group2);
	}

	fold = ut_fold_binary(buf, LOG_CHECKPOINT_CHECKSUM_1);
	mach_write_to_4(buf + LOG_CHECKPOINT_CHECKSUM_1, fold);

	fold = ut_fold_binary(buf + LOG_CHECKPOINT_LSN,
			LOG_CHECKPOINT_CHECKSUM_2 - LOG_CHECKPOINT_LSN);
	mach_write_to_4(buf + LOG_CHECKPOINT_CHECKSUM_2, fold);

	/* We alternate the physical place of the checkpoint info in the first
	log file */ /*我们在第一个日志文件中替换检查点信息的物理位置*/
	
	if (ut_dulint_get_low(log_sys->next_checkpoint_no) % 2 == 0) {
		write_offset = LOG_CHECKPOINT_1;
	} else {
		write_offset = LOG_CHECKPOINT_2;
	}
					
	if (log_do_write) {
		if (log_sys->n_pending_checkpoint_writes == 0) {
	
			rw_lock_x_lock_gen(&(log_sys->checkpoint_lock),
							LOG_CHECKPOINT);
		}

		log_sys->n_pending_checkpoint_writes++;

		log_sys->n_log_ios++;
		
		/* We send as the last parameter the group machine address
		added with 1, as we want to distinguish between a normal log
		file write and a checkpoint field write */
		/*我们将用1添加的组机器地址作为最后一个参数发送，因为我们想要区分普通的日志文件写入和检查点字段写入*/
		
		fil_io(OS_FILE_WRITE | OS_FILE_LOG, FALSE, group->space_id,
				write_offset / UNIV_PAGE_SIZE,
				write_offset % UNIV_PAGE_SIZE,
				OS_FILE_LOG_BLOCK_SIZE,
				buf, ((byte*)group + 1));

		ut_ad(((ulint)group & 0x1) == 0);
	}
}

/**********************************************************
Reads a checkpoint info from a log group header to log_sys->checkpoint_buf. */
/*从日志组头中读取检查点信息到log_sys->checkpoint_buf。*/
void
log_group_read_checkpoint_info(
/*===========================*/
	log_group_t*	group,	/* in: log group */
	ulint		field)	/* in: LOG_CHECKPOINT_1 or LOG_CHECKPOINT_2 */
{
	ut_ad(mutex_own(&(log_sys->mutex)));

	log_sys->n_log_ios++;
	
	fil_io(OS_FILE_READ | OS_FILE_LOG, TRUE, group->space_id,
			field / UNIV_PAGE_SIZE, field % UNIV_PAGE_SIZE,
			OS_FILE_LOG_BLOCK_SIZE, log_sys->checkpoint_buf, NULL);
}

/**********************************************************
Writes checkpoint info to groups. */
/*向组写入检查点信息。*/
void
log_groups_write_checkpoint_info(void)
/*==================================*/
{
	log_group_t*	group;

	ut_ad(mutex_own(&(log_sys->mutex)));

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (group) {
		log_group_checkpoint(group);

		group = UT_LIST_GET_NEXT(log_groups, group);
	}
}

/**********************************************************
Makes a checkpoint. Note that this function does not flush dirty
blocks from the buffer pool: it only checks what is lsn of the oldest
modification in the pool, and writes information about the lsn in
log files. Use log_make_checkpoint_at to flush also the pool. */
/*制作一个检查点。注意，该函数不会清除缓冲池中的脏块:它只检查缓冲池中最旧修改的lsn，并将有关lsn的信息写入日志文件。
也可以使用log_make_checkpoint_at来刷新池。*/
ibool
log_checkpoint(
/*===========*/
				/* out: TRUE if success, FALSE if a checkpoint
				write was already running */
	ibool	sync,		/* in: TRUE if synchronous operation is
				desired */
	ibool	write_always)	/* in: the function normally checks if the
				the new checkpoint would have a greater
				lsn than the previous one: if not, then no
				physical write is done; by setting this
				parameter TRUE, a physical write will always be
				made to log files */
				/*该函数通常检查新检查点的lsn是否大于前一个检查点:如果不是，
				则不进行物理写;通过将此参数设置为TRUE，将始终对日志文件进行物理写操作*/
{
	dulint	oldest_lsn;

	if (recv_recovery_is_on()) {
		recv_apply_hashed_log_recs(TRUE);
	}

	if (srv_unix_file_flush_method != SRV_UNIX_NOSYNC) {
	        fil_flush_file_spaces(FIL_TABLESPACE);
	}

	mutex_enter(&(log_sys->mutex));

	oldest_lsn = log_buf_pool_get_oldest_modification();

	mutex_exit(&(log_sys->mutex));

	/* Because log also contains headers and dummy log records,
	if the buffer pool contains no dirty buffers, oldest_lsn
	gets the value log_sys->lsn from the previous function,
	and we must make sure that the log is flushed up to that
	lsn. If there are dirty buffers in the buffer pool, then our
	write-ahead-logging algorithm ensures that the log has been flushed
	up to oldest_lsn. */
	/*因为log还包含头部和虚拟日志记录，如果缓冲池不包含脏缓冲区，oldest_lsn将从前面的函数中获取log_sys->lsn的值，
	我们必须确保日志被刷新到该lsn。如果缓冲池中有脏缓冲区，那么我们的write- advance -logging算法将确保日志已经刷新到oldest_lsn。*/
	log_flush_up_to(oldest_lsn, LOG_WAIT_ALL_GROUPS);

	mutex_enter(&(log_sys->mutex));

	if (!write_always && ut_dulint_cmp(
			log_sys->last_checkpoint_lsn, oldest_lsn) >= 0) {

		mutex_exit(&(log_sys->mutex));

		return(TRUE);
	}

	ut_ad(ut_dulint_cmp(log_sys->written_to_all_lsn, oldest_lsn) >= 0);
	
	if (log_sys->n_pending_checkpoint_writes > 0) {
		/* A checkpoint write is running */

		mutex_exit(&(log_sys->mutex));

		if (sync) {
			/* Wait for the checkpoint write to complete */
			rw_lock_s_lock(&(log_sys->checkpoint_lock));
			rw_lock_s_unlock(&(log_sys->checkpoint_lock));
		}

		return(FALSE);
	}

	log_sys->next_checkpoint_lsn = oldest_lsn;

	if (log_debug_writes) {
		printf("Making checkpoint no %lu at lsn %lu %lu\n",
			ut_dulint_get_low(log_sys->next_checkpoint_no),
			ut_dulint_get_high(oldest_lsn),
			ut_dulint_get_low(oldest_lsn));
	}

	log_groups_write_checkpoint_info();

	mutex_exit(&(log_sys->mutex));

	if (sync) {
		/* Wait for the checkpoint write to complete */
		rw_lock_s_lock(&(log_sys->checkpoint_lock));
		rw_lock_s_unlock(&(log_sys->checkpoint_lock));
	}

	return(TRUE);
}

/********************************************************************
Makes a checkpoint at a given lsn or later. */
/*在给定的lsn或稍后时间建立检查点。*/
void
log_make_checkpoint_at(
/*===================*/
	dulint	lsn,		/* in: make a checkpoint at this or a later
				lsn, if ut_dulint_max, makes a checkpoint at
				the latest lsn */
	ibool	write_always)	/* in: the function normally checks if the
				the new checkpoint would have a greater
				lsn than the previous one: if not, then no
				physical write is done; by setting this
				parameter TRUE, a physical write will always be
				made to log files */
{
	ibool	success;

	/* Preflush pages synchronously */

	success = FALSE;
	
	while (!success) {
		success = log_preflush_pool_modified_pages(lsn, TRUE);
	}

	success = FALSE;
	
	while (!success) {
		success = log_checkpoint(TRUE, write_always);
	}
}

/********************************************************************
Tries to establish a big enough margin of free space in the log groups, such
that a new log entry can be catenated without an immediate need for a
checkpoint. NOTE: this function may only be called if the calling thread
owns no synchronization objects! */
/*尝试在日志组中建立足够大的空闲空间，以便可以在不立即需要检查点的情况下连接新的日志条目。
注意:这个函数只能在调用线程没有同步对象的情况下调用!*/
static
void
log_checkpoint_margin(void)
/*=======================*/
{
	log_t*	log		= log_sys;
	ulint	age;
	ulint	checkpoint_age;
	ulint	advance;
	dulint	oldest_lsn;
	dulint	new_oldest;
	ibool	do_preflush;
	ibool	sync;
	ibool	checkpoint_sync;
	ibool	do_checkpoint;
	ibool	success;
loop:
	sync = FALSE;
	checkpoint_sync = FALSE;
	do_preflush = FALSE;
	do_checkpoint = FALSE;

	mutex_enter(&(log->mutex));

	if (log->check_flush_or_checkpoint == FALSE) {
		mutex_exit(&(log->mutex));

		return;
	}	

	oldest_lsn = log_buf_pool_get_oldest_modification();

	age = ut_dulint_minus(log->lsn, oldest_lsn);
	
	if (age > log->max_modified_age_sync) {

		/* A flush is urgent: we have to do a synchronous preflush */
		/* 刷新是紧急的:我们必须做一个同步的预刷新*/
		sync = TRUE;
	
		advance = 2 * (age - log->max_modified_age_sync);

		new_oldest = ut_dulint_add(oldest_lsn, advance);

		do_preflush = TRUE;

	} else if (age > log->max_modified_age_async) {

		/* A flush is not urgent: we do an asynchronous preflush */
		/* flush不是紧急的:我们执行异步的预flush*/
		advance = age - log->max_modified_age_async;

		new_oldest = ut_dulint_add(oldest_lsn, advance);

		do_preflush = TRUE;
	}

	checkpoint_age = ut_dulint_minus(log->lsn, log->last_checkpoint_lsn);

	if (checkpoint_age > log->max_checkpoint_age) {
		/* A checkpoint is urgent: we do it synchronously */
	    /* 检查点是紧急的:我们同步进行*/
		checkpoint_sync = TRUE;

		do_checkpoint = TRUE;

	} else if (checkpoint_age > log->max_checkpoint_age_async) {
		/* A checkpoint is not urgent: do it asynchronously */
        /* 检查点不是紧急的:异步执行*/
		do_checkpoint = TRUE;

		log->check_flush_or_checkpoint = FALSE;
	} else {
		log->check_flush_or_checkpoint = FALSE;
	}
	
	mutex_exit(&(log->mutex));

	if (do_preflush) {
		success = log_preflush_pool_modified_pages(new_oldest, sync);

		/* If the flush succeeded, this thread has done its part
		and can proceed. If it did not succeed, there was another
		thread doing a flush at the same time. If sync was FALSE,
		the flush was not urgent, and we let this thread proceed.
		Otherwise, we let it start from the beginning again. */
		/*如果刷新成功，这个线程已经完成了它的部分，可以继续执行。
		如果没有成功，则同时有另一个线程进行刷新。如果sync为FALSE，
		则刷新不是紧急的，我们让这个线程继续执行。否则，我们就从头再来。*/
		if (sync && !success) {
			mutex_enter(&(log->mutex));

			log->check_flush_or_checkpoint = TRUE;
	
			mutex_exit(&(log->mutex));
			goto loop;
		}
	}

	if (do_checkpoint) {
		log_checkpoint(checkpoint_sync, FALSE);

		if (checkpoint_sync) {

			goto loop;
		}
	}
}

/**********************************************************
Reads a specified log segment to a buffer. */
/*将指定的日志段读入缓冲区。*/
void
log_group_read_log_seg(
/*===================*/
	ulint		type,		/* in: LOG_ARCHIVE or LOG_RECOVER */
	byte*		buf,		/* in: buffer where to read */
	log_group_t*	group,		/* in: log group */
	dulint		start_lsn,	/* in: read area start */
	dulint		end_lsn)	/* in: read area end */
{
	ulint	len;
	ulint	source_offset;
	ibool	sync;

	ut_ad(mutex_own(&(log_sys->mutex)));

	sync = FALSE;

	if (type == LOG_RECOVER) {
		sync = TRUE;
	}
loop:
	source_offset = log_group_calc_lsn_offset(start_lsn, group);

	len = ut_dulint_minus(end_lsn, start_lsn);

	ut_ad(len != 0);

	if ((source_offset % group->file_size) + len > group->file_size) {

		len = group->file_size - (source_offset % group->file_size);
	}

	if (type == LOG_ARCHIVE) {
	
		log_sys->n_pending_archive_ios++;
	}

	log_sys->n_log_ios++;
	
	fil_io(OS_FILE_READ | OS_FILE_LOG, sync, group->space_id,
		source_offset / UNIV_PAGE_SIZE, source_offset % UNIV_PAGE_SIZE,
		len, buf, &log_archive_io);

	start_lsn = ut_dulint_add(start_lsn, len);
	buf += len;

	if (ut_dulint_cmp(start_lsn, end_lsn) != 0) {

		goto loop;
	}
}

/**********************************************************
Generates an archived log file name. */
/*生成归档日志文件名。*/
void
log_archived_file_name_gen(
/*=======================*/
	char*	buf,	/* in: buffer where to write */
	ulint	id,	/* in: group id */
	ulint	file_no)/* in: file number */
{
	UT_NOT_USED(id);	/* Currently we only archive the first group */
	
	sprintf(buf, "%sib_arch_log_%010lu", srv_arch_dir, file_no);
}

/**********************************************************
Writes a log file header to a log file space. */
/*将日志文件头写入日志文件空间。*/
static
void
log_group_archive_file_header_write(
/*================================*/
	log_group_t*	group,		/* in: log group */
	ulint		nth_file,	/* in: header to the nth file in the
					archive log file space */
	ulint		file_no,	/* in: archived file number */
	dulint		start_lsn)	/* in: log file data starts at this
					lsn */
{
	byte*	buf;
	ulint	dest_offset;

	ut_ad(mutex_own(&(log_sys->mutex)));

	ut_a(nth_file < group->n_files);

	buf = *(group->archive_file_header_bufs + nth_file);

	mach_write_to_4(buf + LOG_GROUP_ID, group->id);
	mach_write_to_8(buf + LOG_FILE_START_LSN, start_lsn);
	mach_write_to_4(buf + LOG_FILE_NO, file_no);

	mach_write_to_4(buf + LOG_FILE_ARCH_COMPLETED, FALSE);

	dest_offset = nth_file * group->file_size;

	log_sys->n_log_ios++;
	
	fil_io(OS_FILE_WRITE | OS_FILE_LOG, TRUE, group->archive_space_id,
				dest_offset / UNIV_PAGE_SIZE,
				dest_offset % UNIV_PAGE_SIZE,
				2 * OS_FILE_LOG_BLOCK_SIZE,
				buf, &log_archive_io);
}

/**********************************************************
Writes a log file header to a completed archived log file. */
/*将日志文件头写入已完成的归档日志文件。*/
static
void
log_group_archive_completed_header_write(
/*=====================================*/
	log_group_t*	group,		/* in: log group */
	ulint		nth_file,	/* in: header to the nth file in the
					archive log file space */
	dulint		end_lsn)	/* in: end lsn of the file */
{
	byte*	buf;
	ulint	dest_offset;

	ut_ad(mutex_own(&(log_sys->mutex)));
	ut_a(nth_file < group->n_files);

	buf = *(group->archive_file_header_bufs + nth_file);

	mach_write_to_4(buf + LOG_FILE_ARCH_COMPLETED, TRUE);
	mach_write_to_8(buf + LOG_FILE_END_LSN, end_lsn);

	dest_offset = nth_file * group->file_size + LOG_FILE_ARCH_COMPLETED;

	log_sys->n_log_ios++;
	
	fil_io(OS_FILE_WRITE | OS_FILE_LOG, TRUE, group->archive_space_id,
				dest_offset / UNIV_PAGE_SIZE,
				dest_offset % UNIV_PAGE_SIZE,
				OS_FILE_LOG_BLOCK_SIZE,
				buf + LOG_FILE_ARCH_COMPLETED,
				&log_archive_io);
}

/**********************************************************
Does the archive writes for a single log group. */ /*执行单个日志组的归档写入操作。*/
static
void
log_group_archive(
/*==============*/
	log_group_t*	group)	/* in: log group */
{
	os_file_t file_handle;
	dulint	start_lsn;
	dulint	end_lsn;
	char	name[100];
	byte*	buf;
	ulint	len;
	ibool	ret;
	ulint	next_offset;
	ulint	n_files;
	ulint	open_mode;
	
	ut_ad(mutex_own(&(log_sys->mutex)));

	start_lsn = log_sys->archived_lsn;

	ut_ad(ut_dulint_get_low(start_lsn) % OS_FILE_LOG_BLOCK_SIZE == 0);

	end_lsn = log_sys->next_archived_lsn;

	ut_ad(ut_dulint_get_low(end_lsn) % OS_FILE_LOG_BLOCK_SIZE == 0);

	buf = log_sys->archive_buf;

	n_files = 0;

	next_offset = group->archived_offset;
loop:				
	if ((next_offset % group->file_size == 0)
	    || (fil_space_get_size(group->archive_space_id) == 0)) {

		/* Add the file to the archive file space; create or open the
		file */

		if (next_offset % group->file_size == 0) {
			open_mode = OS_FILE_CREATE;
		} else {
			open_mode = OS_FILE_OPEN;
		}
	
		log_archived_file_name_gen(name, group->id,
					group->archived_file_no + n_files);
		fil_reserve_right_to_open();

		file_handle = os_file_create(name, open_mode, OS_FILE_AIO,
						OS_DATA_FILE, &ret);

		if (!ret && (open_mode == OS_FILE_CREATE)) {
			file_handle = os_file_create(name, OS_FILE_OPEN,
					OS_FILE_AIO, OS_DATA_FILE, &ret);
		}

		if (!ret) {
		  fprintf(stderr,
		   "InnoDB: Cannot create or open archive log file %s.\n",
			  name);
		  fprintf(stderr, "InnoDB: Cannot continue operation.\n"
       		  "InnoDB: Check that the log archive directory exists,\n"
			  "InnoDB: you have access rights to it, and\n"
			  "InnoDB: there is space available.\n");
		  exit(1);
		}

		if (log_debug_writes) {
			printf("Created archive file %s\n", name);
		}

		ret = os_file_close(file_handle);
	
		ut_a(ret);
	
		fil_release_right_to_open();
	
		/* Add the archive file as a node to the space */
		/* 将归档文件作为节点添加到空间中*/
		fil_node_create(name, group->file_size / UNIV_PAGE_SIZE,
						group->archive_space_id);

		if (next_offset % group->file_size == 0) {
			log_group_archive_file_header_write(group, n_files,
					group->archived_file_no + n_files,
					start_lsn);

			next_offset += LOG_FILE_HDR_SIZE;
		}				
	}

	len = ut_dulint_minus(end_lsn, start_lsn);

	if (group->file_size < (next_offset % group->file_size) + len) {

		len = group->file_size - (next_offset % group->file_size);
	}
	
	if (log_debug_writes) {
		printf(
		"Archiving starting at lsn %lu %lu, len %lu to group %lu\n",
					ut_dulint_get_high(start_lsn),
					ut_dulint_get_low(start_lsn),
					len, group->id);
	}

	log_sys->n_pending_archive_ios++;

	log_sys->n_log_ios++;
	
	fil_io(OS_FILE_WRITE | OS_FILE_LOG, FALSE, group->archive_space_id,
		next_offset / UNIV_PAGE_SIZE, next_offset % UNIV_PAGE_SIZE,
		ut_calc_align(len, OS_FILE_LOG_BLOCK_SIZE), buf,
							&log_archive_io);

	start_lsn = ut_dulint_add(start_lsn, len);
	next_offset += len;
	buf += len;

	if (next_offset % group->file_size == 0) {
		n_files++;
	}

	if (ut_dulint_cmp(end_lsn, start_lsn) != 0) {
	
		goto loop;
	}

	group->next_archived_file_no = group->archived_file_no + n_files;
	group->next_archived_offset = next_offset % group->file_size;

	ut_ad(group->next_archived_offset % OS_FILE_LOG_BLOCK_SIZE == 0);
}

/*********************************************************
(Writes to the archive of each log group.) Currently, only the first
group is archived. */ /*(写入到每个日志组的存档。)目前，只有第一组存档。*/
static
void
log_archive_groups(void)
/*====================*/
{
	log_group_t*	group;

	ut_ad(mutex_own(&(log_sys->mutex)));

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	log_group_archive(group);
}

/*********************************************************
Completes the archiving write phase for (each log group), currently,
the first log group. */
/*完成(每个日志组)的归档写入阶段，目前是第一个日志组。*/
static
void
log_archive_write_complete_groups(void)
/*===================================*/
{
	log_group_t*	group;
	ulint		end_offset;
	ulint		trunc_files;
	ulint		n_files;
	dulint		start_lsn;
	dulint		end_lsn;
	ulint		i;

	ut_ad(mutex_own(&(log_sys->mutex)));

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	group->archived_file_no = group->next_archived_file_no;
	group->archived_offset = group->next_archived_offset;

	/* Truncate from the archive file space all but the last
	file, or if it has been written full, all files */
    /*从归档文件空间中截断除最后一个文件以外的所有文件，或者如果最后一个文件已写满，则截断所有文件*/
	n_files = (UNIV_PAGE_SIZE
			    * fil_space_get_size(group->archive_space_id))
			    				/ group->file_size;
	ut_ad(n_files > 0);
		
	end_offset = group->archived_offset;

	if (end_offset % group->file_size == 0) {
		
		trunc_files = n_files;
	} else {
		trunc_files = n_files - 1;
	}

	if (log_debug_writes && trunc_files) {
		printf("Complete file(s) archived to group %lu\n",
								group->id);
	}

	/* Calculate the archive file space start lsn */
	/* 计算归档文件空间的起始lsn*/
	start_lsn = ut_dulint_subtract(log_sys->next_archived_lsn,
				end_offset - LOG_FILE_HDR_SIZE
				+ trunc_files
				  * (group->file_size - LOG_FILE_HDR_SIZE));
	end_lsn = start_lsn;
	
	for (i = 0; i < trunc_files; i++) {

		end_lsn = ut_dulint_add(end_lsn,
					group->file_size - LOG_FILE_HDR_SIZE);

		/* Write a notice to the headers of archived log
		files that the file write has been completed */
        /*向归档日志文件的头写入通知，说明文件写入已完成
		*/
		log_group_archive_completed_header_write(group, i, end_lsn);
	}
		
	fil_space_truncate_start(group->archive_space_id,
					trunc_files * group->file_size);

	if (log_debug_writes) {
		printf("Archiving writes completed\n");
	}
}

/**********************************************************
Completes an archiving i/o. */ /*完成一个归档i/o。*/
static
void
log_archive_check_completion_low(void)
/*==================================*/
{
	ut_ad(mutex_own(&(log_sys->mutex)));

	if (log_sys->n_pending_archive_ios == 0
			&& log_sys->archiving_phase == LOG_ARCHIVE_READ) {

		if (log_debug_writes) {
			printf("Archiving read completed\n");
		}

	    	/* Archive buffer has now been read in: start archive writes */
			/* 归档缓冲区现在已经被读取:开始归档写入*/
		log_sys->archiving_phase = LOG_ARCHIVE_WRITE;

		log_archive_groups();
	}	

	if (log_sys->n_pending_archive_ios == 0
			&& log_sys->archiving_phase == LOG_ARCHIVE_WRITE) {

	     	log_archive_write_complete_groups();

		log_sys->archived_lsn = log_sys->next_archived_lsn;

		rw_lock_x_unlock_gen(&(log_sys->archive_lock), LOG_ARCHIVE);
	}	
}

/**********************************************************
Completes an archiving i/o. */  /*完成一个归档i/o。*/
static
void
log_io_complete_archive(void)
/*=========================*/
{
	log_group_t*	group;

	mutex_enter(&(log_sys->mutex));

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	mutex_exit(&(log_sys->mutex));

	fil_flush(group->archive_space_id);
	
	mutex_enter(&(log_sys->mutex));
	
	ut_ad(log_sys->n_pending_archive_ios > 0);
	
	log_sys->n_pending_archive_ios--;

	log_archive_check_completion_low();

	mutex_exit(&(log_sys->mutex));
}

/************************************************************************
Starts an archiving operation. */
/*启动存档操作。*/
ibool
log_archive_do(
/*===========*/
			/* out: TRUE if succeed, FALSE if an archiving
			operation was already running */
	ibool	sync,	/* in: TRUE if synchronous operation is desired */
	ulint*	n_bytes)/* out: archive log buffer size, 0 if nothing to
			archive */
{
	ibool	calc_new_limit;
	dulint	start_lsn;
	dulint	limit_lsn;
	
	calc_new_limit = TRUE;
loop:	
	mutex_enter(&(log_sys->mutex));

	if (log_sys->archiving_state == LOG_ARCH_OFF) {
		mutex_exit(&(log_sys->mutex));

		*n_bytes = 0;

		return(TRUE);

	} else if (log_sys->archiving_state == LOG_ARCH_STOPPED
	           || log_sys->archiving_state == LOG_ARCH_STOPPING2) {

		mutex_exit(&(log_sys->mutex));
		
		os_event_wait(log_sys->archiving_on);

		mutex_enter(&(log_sys->mutex));

		goto loop;
	}
	
	start_lsn = log_sys->archived_lsn;
	
	if (calc_new_limit) {
		ut_ad(log_sys->archive_buf_size % OS_FILE_LOG_BLOCK_SIZE == 0);

		limit_lsn = ut_dulint_add(start_lsn,
						log_sys->archive_buf_size);

		*n_bytes = log_sys->archive_buf_size;
						
		if (ut_dulint_cmp(limit_lsn, log_sys->lsn) >= 0) {

			limit_lsn = ut_dulint_align_down(log_sys->lsn,
						OS_FILE_LOG_BLOCK_SIZE);
		}
	}

	if (ut_dulint_cmp(log_sys->archived_lsn, limit_lsn) >= 0) {

		mutex_exit(&(log_sys->mutex));

		*n_bytes = 0;
		
		return(TRUE);
	}

	if (ut_dulint_cmp(log_sys->written_to_all_lsn, limit_lsn) < 0) {

		mutex_exit(&(log_sys->mutex));
	
		log_flush_up_to(limit_lsn, LOG_WAIT_ALL_GROUPS);

		calc_new_limit = FALSE;

		goto loop;
	}
	
	if (log_sys->n_pending_archive_ios > 0) {
		/* An archiving operation is running */

		mutex_exit(&(log_sys->mutex));

		if (sync) {
			rw_lock_s_lock(&(log_sys->archive_lock));
			rw_lock_s_unlock(&(log_sys->archive_lock));
		}

		*n_bytes = log_sys->archive_buf_size;

		return(FALSE);
	}		

	rw_lock_x_lock_gen(&(log_sys->archive_lock), LOG_ARCHIVE);

	log_sys->archiving_phase = LOG_ARCHIVE_READ;

	log_sys->next_archived_lsn = limit_lsn;

	if (log_debug_writes) {
		printf("Archiving from lsn %lu %lu to lsn %lu %lu\n",
			ut_dulint_get_high(log_sys->archived_lsn),
			ut_dulint_get_low(log_sys->archived_lsn),
			ut_dulint_get_high(limit_lsn),
			ut_dulint_get_low(limit_lsn));
	}

	/* Read the log segment to the archive buffer */
	
	log_group_read_log_seg(LOG_ARCHIVE, log_sys->archive_buf,
				UT_LIST_GET_FIRST(log_sys->log_groups),
				start_lsn, limit_lsn);

	mutex_exit(&(log_sys->mutex));

	if (sync) {
		rw_lock_s_lock(&(log_sys->archive_lock));
		rw_lock_s_unlock(&(log_sys->archive_lock));
	}

	*n_bytes = log_sys->archive_buf_size;
	
	return(TRUE);
}

/********************************************************************
Writes the log contents to the archive at least up to the lsn when this
function was called. *//*调用此函数时，将日志内容至少写到lsn。*/
static
void
log_archive_all(void)
/*=================*/
{
	dulint	present_lsn;
	ulint	dummy;
	
	mutex_enter(&(log_sys->mutex));

	if (log_sys->archiving_state == LOG_ARCH_OFF) {
		mutex_exit(&(log_sys->mutex));

		return;
	}

	present_lsn = log_sys->lsn;

	mutex_exit(&(log_sys->mutex));

	log_pad_current_log_block();
	
	for (;;) {
		mutex_enter(&(log_sys->mutex));

		if (ut_dulint_cmp(present_lsn, log_sys->archived_lsn) <= 0) {
			
			mutex_exit(&(log_sys->mutex));

			return;
		}
		
		mutex_exit(&(log_sys->mutex));

		log_archive_do(TRUE, &dummy);
	}
}	

/*********************************************************
Closes the possible open archive log file (for each group) the first group,
and if it was open, increments the group file count by 2, if desired. */
/*关闭第一个组可能打开的归档日志文件(针对每个组)，如果它是打开的，将组文件计数增加2(如果需要的话)。*/
static
void
log_archive_close_groups(
/*=====================*/
	ibool	increment_file_count)	/* in: TRUE if we want to increment
					the file count */
{
	log_group_t*	group;
	ulint		trunc_len;

	ut_ad(mutex_own(&(log_sys->mutex)));

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	trunc_len = UNIV_PAGE_SIZE
			    * fil_space_get_size(group->archive_space_id);

	if (trunc_len > 0) {
		ut_a(trunc_len == group->file_size);
			    
		/* Write a notice to the headers of archived log
		files that the file write has been completed */
		/*向归档日志文件的头写入通知，说明文件写入已完成*/
		log_group_archive_completed_header_write(group,
						0, log_sys->archived_lsn);

		fil_space_truncate_start(group->archive_space_id,
								trunc_len);
		if (increment_file_count) {
			group->archived_offset = 0;
			group->archived_file_no += 2;
		}

		if (log_debug_writes) {
			printf(
			"Incrementing arch file no to %lu in log group %lu\n",
				group->archived_file_no + 2, group->id);
		}
	}
}

/********************************************************************
Writes the log contents to the archive up to the lsn when this function was
called, and stops the archiving. When archiving is started again, the archived
log file numbers start from 2 higher, so that the archiving will
not write again to the archived log files which exist when this function
returns. */
/*当调用此函数时，将日志内容写入归档到lsn，并停止归档。
当再次开始归档时，归档的日志文件号从更高的2开始，这样当这个函数返回时归档的日志文件不会再次写入*/
ulint
log_archive_stop(void)
/*==================*/
				/* out: DB_SUCCESS or DB_ERROR */
{
	ibool	success;

	mutex_enter(&(log_sys->mutex));

	if (log_sys->archiving_state != LOG_ARCH_ON) {

		mutex_exit(&(log_sys->mutex));

		return(DB_ERROR);
	}	

	log_sys->archiving_state = LOG_ARCH_STOPPING;
	
	mutex_exit(&(log_sys->mutex));
	
	log_archive_all();

	mutex_enter(&(log_sys->mutex));

	log_sys->archiving_state = LOG_ARCH_STOPPING2;
	os_event_reset(log_sys->archiving_on);

	mutex_exit(&(log_sys->mutex));

	/* Wait for a possible archiving operation to end */
	/* 等待可能的存档操作结束*/
	rw_lock_s_lock(&(log_sys->archive_lock));
	rw_lock_s_unlock(&(log_sys->archive_lock));

	mutex_enter(&(log_sys->mutex));

	/* Close all archived log files, incrementing the file count by 2,
	if appropriate */
	/* 关闭所有存档的日志文件，如果合适的话，将文件计数增加2*/
	log_archive_close_groups(TRUE);
	
	mutex_exit(&(log_sys->mutex));

	/* Make a checkpoint, so that if recovery is needed, the file numbers
	of new archived log files will start from the right value */
	/* 创建一个检查点，以便在需要恢复时，新归档日志文件的文件号将从正确的值开始*/
	success = FALSE;
	
	while (!success) {
		success = log_checkpoint(TRUE, TRUE);
	}

	mutex_enter(&(log_sys->mutex));

	log_sys->archiving_state = LOG_ARCH_STOPPED;
	
	mutex_exit(&(log_sys->mutex));

	return(DB_SUCCESS);
}

/********************************************************************
Starts again archiving which has been stopped. */
/*重新开始已经停止的存档。*/
ulint
log_archive_start(void)
/*===================*/
			/* out: DB_SUCCESS or DB_ERROR */
{
	mutex_enter(&(log_sys->mutex));

	if (log_sys->archiving_state != LOG_ARCH_STOPPED) {

		mutex_exit(&(log_sys->mutex));

		return(DB_ERROR);
	}	
	
	log_sys->archiving_state = LOG_ARCH_ON;

	os_event_set(log_sys->archiving_on);

	mutex_exit(&(log_sys->mutex));

	return(DB_SUCCESS);
}

/********************************************************************
Stop archiving the log so that a gap may occur in the archived log files. */
/*停止对日志的归档，归档的日志文件可能会出现间隙。*/
ulint
log_archive_noarchivelog(void)
/*==========================*/
			/* out: DB_SUCCESS or DB_ERROR */
{
loop:
	mutex_enter(&(log_sys->mutex));

	if (log_sys->archiving_state == LOG_ARCH_STOPPED
	    || log_sys->archiving_state == LOG_ARCH_OFF) {

		log_sys->archiving_state = LOG_ARCH_OFF;
	
		os_event_set(log_sys->archiving_on);

		mutex_exit(&(log_sys->mutex));

		return(DB_SUCCESS);
	}	

	mutex_exit(&(log_sys->mutex));

	log_archive_stop();

	os_thread_sleep(500000);
	
	goto loop;	
}

/********************************************************************
Start archiving the log so that a gap may occur in the archived log files. */
/*开始归档日志，以便归档的日志文件中可能出现间隙。*/
ulint
log_archive_archivelog(void)
/*========================*/
			/* out: DB_SUCCESS or DB_ERROR */
{
	mutex_enter(&(log_sys->mutex));

	if (log_sys->archiving_state == LOG_ARCH_OFF) {

		log_sys->archiving_state = LOG_ARCH_ON;

		log_sys->archived_lsn = ut_dulint_align_down(log_sys->lsn,
						OS_FILE_LOG_BLOCK_SIZE);	
		mutex_exit(&(log_sys->mutex));

		return(DB_SUCCESS);
	}	

	mutex_exit(&(log_sys->mutex));

	return(DB_ERROR);	
}

/********************************************************************
Tries to establish a big enough margin of free space in the log groups, such
that a new log entry can be catenated without an immediate need for
archiving. */
/*尝试在日志组中建立足够大的空闲空间，以便可以连接新的日志条目，而不需要立即归档。*/
static
void
log_archive_margin(void)
/*====================*/
{
	log_t*	log		= log_sys;
	ulint	age;
	ibool	sync;
	ulint	dummy;
loop:
	mutex_enter(&(log->mutex));

	if (log->archiving_state == LOG_ARCH_OFF) {
		mutex_exit(&(log->mutex));

		return;
	}

	age = ut_dulint_minus(log->lsn, log->archived_lsn);
	
	if (age > log->max_archived_lsn_age) {

		/* An archiving is urgent: we have to do synchronous i/o */

		sync = TRUE;
	
	} else if (age > log->max_archived_lsn_age_async) {

		/* An archiving is not urgent: we do asynchronous i/o */
	
		sync = FALSE;
	} else {
		/* No archiving required yet */

		mutex_exit(&(log->mutex));

		return;
	}

	mutex_exit(&(log->mutex));

	log_archive_do(sync, &dummy);

	if (sync == TRUE) {
		/* Check again that enough was written to the archive */

		goto loop;
	}
}

/************************************************************************
Checks that there is enough free space in the log to start a new query step.
Flushes the log buffer or makes a new checkpoint if necessary. NOTE: this
function may only be called if the calling thread owns no synchronization
objects! */
/*检查日志中是否有足够的空闲空间来启动一个新的查询步骤。如有必要，刷新日志缓冲区或创建新的检查点。
注意:这个函数只能在调用线程没有同步对象的情况下调用!*/
void
log_check_margins(void)
/*===================*/
{
loop:
	log_flush_margin();

	log_checkpoint_margin();

	log_archive_margin();

	mutex_enter(&(log_sys->mutex));
	
	if (log_sys->check_flush_or_checkpoint) {

		mutex_exit(&(log_sys->mutex));

		goto loop;
	}

	mutex_exit(&(log_sys->mutex));
}

/**********************************************************
Switches the database to the online backup state. */
/*将数据库切换到联机备份状态。*/
ulint
log_switch_backup_state_on(void)
/*============================*/
			/* out: DB_SUCCESS or DB_ERROR */
{
	dulint	backup_lsn;
	
	mutex_enter(&(log_sys->mutex));

	if (log_sys->online_backup_state) {

		/* The database is already in that state */

		mutex_exit(&(log_sys->mutex));

		return(DB_ERROR);
	}

	log_sys->online_backup_state = TRUE;

	backup_lsn = log_sys->lsn;

	log_sys->online_backup_lsn = backup_lsn;

	mutex_exit(&(log_sys->mutex));

	/* log_checkpoint_and_mark_file_spaces(); */

	return(DB_SUCCESS);
}

/**********************************************************
Switches the online backup state off. */
/*关闭在线备份状态。*/
ulint
log_switch_backup_state_off(void)
/*=============================*/
			/* out: DB_SUCCESS or DB_ERROR */
{
	mutex_enter(&(log_sys->mutex));

	if (!log_sys->online_backup_state) {

		/* The database is already in that state */

		mutex_exit(&(log_sys->mutex));

		return(DB_ERROR);
	}

	log_sys->online_backup_state = FALSE;

	mutex_exit(&(log_sys->mutex));

	return(DB_SUCCESS);
}

/********************************************************************
Makes a checkpoint at the latest lsn and writes it to first page of each
data file in the database, so that we know that the file spaces contain
all modifications up to that lsn. This can only be called at database
shutdown. This function also writes all log in log files to the log archive. */
/*在最新的lsn上创建一个检查点，并将其写入数据库中每个数据文件的第一页，这样我们就知道文件空间包含该lsn之前的所有修改。
这只能在数据库关闭时调用。此函数还将日志文件中的所有日志写入日志归档。*/
void
logs_empty_and_mark_files_at_shutdown(void)
/*=======================================*/
{
	dulint	lsn;
	ulint	arch_log_no;

	ut_print_timestamp(stderr);
	fprintf(stderr, "  InnoDB: Starting shutdown...\n");

	/* Wait until the master thread and all other operations are idle: our
	algorithm only works if the server is idle at shutdown */
	/*等待，直到主线程和所有其他操作空闲:我们的算法只有在服务器在关闭时空闲时才能工作*/
	srv_shutdown_state = SRV_SHUTDOWN_CLEANUP;
loop:
	os_thread_sleep(100000);

	mutex_enter(&kernel_mutex);

	if (trx_n_mysql_transactions > 0
			|| UT_LIST_GET_LEN(trx_sys->trx_list) > 0) {
		
		mutex_exit(&kernel_mutex);
		
		goto loop;
	}

	if (srv_n_threads_active[SRV_MASTER] != 0) {

		mutex_exit(&kernel_mutex);

		goto loop;
	}

	mutex_exit(&kernel_mutex);
	
	mutex_enter(&(log_sys->mutex));

	if (log_sys->n_pending_archive_ios
			+ log_sys->n_pending_checkpoint_writes
			+ log_sys->n_pending_writes > 0) {
				
		mutex_exit(&(log_sys->mutex));

		goto loop;
	}	
	
	mutex_exit(&(log_sys->mutex));

	if (!buf_pool_check_no_pending_io()) {

		goto loop;
	}

	log_archive_all();

	log_make_checkpoint_at(ut_dulint_max, TRUE);

	mutex_enter(&(log_sys->mutex));

	lsn = log_sys->lsn;

	if (ut_dulint_cmp(lsn, log_sys->last_checkpoint_lsn) != 0
	   || (srv_log_archive_on
		&& ut_dulint_cmp(lsn,
	    ut_dulint_add(log_sys->archived_lsn, LOG_BLOCK_HDR_SIZE)) != 0)) {

	    	mutex_exit(&(log_sys->mutex));

	    	goto loop;
	}    

	arch_log_no =
		UT_LIST_GET_FIRST(log_sys->log_groups)->archived_file_no;
		
	if (0 == UT_LIST_GET_FIRST(log_sys->log_groups)->archived_offset) {
	
		arch_log_no--;
	}
	
	log_archive_close_groups(TRUE);

	mutex_exit(&(log_sys->mutex));

	fil_flush_file_spaces(FIL_TABLESPACE);
	fil_flush_file_spaces(FIL_LOG);

	/* The following fil_write_... will pass the buffer pool: therefore
	it is essential that the buffer pool has been completely flushed
	to disk! */
	/*以下fil_write_……将传递缓冲池:因此缓冲池必须完全刷新到磁盘!*/
	if (!buf_all_freed()) {

		goto loop;
	}

	if (srv_lock_timeout_and_monitor_active) {

		goto loop;
	}

	/* We now suspend also the InnoDB error monitor thread */
	/*现在我们还挂起了InnoDB错误监视线程*/
	srv_shutdown_state = SRV_SHUTDOWN_LAST_PHASE;

	if (srv_error_monitor_active) {

		goto loop;
	}

	fil_write_flushed_lsn_to_data_files(lsn, arch_log_no);	

	fil_flush_file_spaces(FIL_TABLESPACE);

	ut_print_timestamp(stderr);
	fprintf(stderr, "  InnoDB: Shutdown completed\n");
}

/**********************************************************
Checks by parsing that the catenated log segment for a single mtr is
consistent. */
/*过解析单个mtr的连接日志段是否一致来检查。*/
ibool
log_check_log_recs(
/*===============*/
	byte*	buf,		/* in: pointer to the start of the log segment
				in the log_sys->buf log buffer */
	ulint	len,		/* in: segment length in bytes */
	dulint	buf_start_lsn)	/* in: buffer start lsn */
{
	dulint	contiguous_lsn;
	dulint	scanned_lsn;
	byte*	start;
	byte*	end;
	byte*	buf1;
	byte*	scan_buf;

	ut_ad(mutex_own(&(log_sys->mutex)));

	if (len == 0) {

		return(TRUE);
	}
	
	start = ut_align_down(buf, OS_FILE_LOG_BLOCK_SIZE);
	end = ut_align(buf + len, OS_FILE_LOG_BLOCK_SIZE);

	buf1 = mem_alloc((end - start) + OS_FILE_LOG_BLOCK_SIZE);
	scan_buf = ut_align(buf1, OS_FILE_LOG_BLOCK_SIZE);

	ut_memcpy(scan_buf, start, end - start);
	
	recv_scan_log_recs(FALSE, scan_buf, end - start,
				ut_dulint_align_down(buf_start_lsn,
						OS_FILE_LOG_BLOCK_SIZE),
			&contiguous_lsn, &scanned_lsn);

	ut_a(ut_dulint_cmp(scanned_lsn, ut_dulint_add(buf_start_lsn, len))
									== 0);
	ut_a(ut_dulint_cmp(recv_sys->recovered_lsn, scanned_lsn) == 0);

	mem_free(buf1);
			
	return(TRUE);
}

/**********************************************************
Prints info of the log. */
/*打印日志信息。*/
void
log_print(void)
/*===========*/
{
	double	time_elapsed;
	time_t	current_time;

	mutex_enter(&(log_sys->mutex));

	printf("Log sequence number %lu %lu\n"
	       "Log flushed up to   %lu %lu\n"
	       "Last checkpoint at  %lu %lu\n",
			ut_dulint_get_high(log_sys->lsn),
			ut_dulint_get_low(log_sys->lsn),
			ut_dulint_get_high(log_sys->written_to_some_lsn),
			ut_dulint_get_low(log_sys->written_to_some_lsn),
			ut_dulint_get_high(log_sys->last_checkpoint_lsn),
			ut_dulint_get_low(log_sys->last_checkpoint_lsn));

	current_time = time(NULL);
			
	time_elapsed = difftime(current_time, log_sys->last_printout_time);

	printf(
	"%lu pending log writes, %lu pending chkp writes\n"
	"%lu log i/o's done, %.2f log i/o's/second\n",
	log_sys->n_pending_writes,
	log_sys->n_pending_checkpoint_writes,
	log_sys->n_log_ios,
	(log_sys->n_log_ios - log_sys->n_log_ios_old) / time_elapsed);

	log_sys->n_log_ios_old = log_sys->n_log_ios;
	log_sys->last_printout_time = current_time;

	mutex_exit(&(log_sys->mutex));
}
