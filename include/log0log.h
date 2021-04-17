/******************************************************
Database log

(c) 1995 Innobase Oy

Created 12/9/1995 Heikki Tuuri
*******************************************************/
/*数据库重做日志*/
#ifndef log0log_h
#define log0log_h

#include "univ.i"
#include "ut0byte.h"
#include "sync0sync.h"
#include "sync0rw.h"

typedef struct log_struct	log_t;
typedef struct log_group_struct	log_group_t;

extern	ibool	log_do_write;
extern 	ibool	log_debug_writes;

/* Wait modes for log_flush_up_to */
#define LOG_NO_WAIT		91
#define LOG_WAIT_ONE_GROUP	92
#define	LOG_WAIT_ALL_GROUPS	93
#define LOG_MAX_N_GROUPS	32

/****************************************************************
Writes to the log the string given. The log must be released with
log_release. */
/*将给定的字符串写入日志。日志必须通过log_release释放。*/
UNIV_INLINE
dulint
log_reserve_and_write_fast(
/*=======================*/
			/* out: end lsn of the log record, ut_dulint_zero if
			did not succeed */
	byte*	str,	/* in: string */
	ulint	len,	/* in: string length */
	dulint*	start_lsn,/* out: start lsn of the log record */
	ibool*	success);/* out: TRUE if success */
/***************************************************************************
Releases the log mutex. */
/*释放日志互斥锁。*/
UNIV_INLINE
void
log_release(void);
/*=============*/
/***************************************************************************
Checks if there is need for a log buffer flush or a new checkpoint, and does
this if yes. Any database operation should call this when it has modified
more than about 4 pages. NOTE that this function may only be called when the
OS thread owns no synchronization objects except the dictionary mutex. */
/*检查是否需要刷新日志缓冲区或新的检查点，如果需要，则执行此操作。
任何数据库操作在修改超过4个页面时都应该调用这个函数。
注意，只有当操作系统线程除了字典互斥锁之外没有其他同步对象时，才可以调用这个函数。*/
UNIV_INLINE
void
log_free_check(void);
/*================*/
/****************************************************************
Opens the log for log_write_low. The log must be closed with log_close and
released with log_release. */
/*打开log_write_low的日志。日志必须用log_close关闭，用log_release释放。*/
dulint
log_reserve_and_open(
/*=================*/
			/* out: start lsn of the log record */
	ulint	len);	/* in: length of data to be catenated */
/****************************************************************
Writes to the log the string given. It is assumed that the caller holds the
log mutex. */
/*将给定的字符串写入日志。假定调用者持有日志互斥锁。*/
void
log_write_low(
/*==========*/
	byte*	str,		/* in: string */
	ulint	str_len);	/* in: string length */
/****************************************************************
Closes the log. */
/*关闭日志。*/
dulint
log_close(void);
/*===========*/
			/* out: lsn */
/****************************************************************
Gets the current lsn. */
/*获取当前lsn。*/
UNIV_INLINE
dulint
log_get_lsn(void);
/*=============*/
			/* out: current lsn */
/****************************************************************************
Gets the online backup lsn. */ /*获取在线备份的lsn。*/
UNIV_INLINE
dulint
log_get_online_backup_lsn_low(void);
/*===============================*/
/****************************************************************************
Gets the online backup state. */ /*获取在线备份状态。*/
UNIV_INLINE
ibool
log_get_online_backup_state_low(void);
/*=================================*/
				/* out: online backup state, the caller must
				own the log_sys mutex */
/**********************************************************
Initializes the log. */
/*初始化日志。*/
void
log_init(void);
/*==========*/
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
	ulint	archive_space_id);	/* in: space id of the file space
					which contains some archived log
					files for this group; currently, only
					for the first log group this is
					used */
/**********************************************************
Completes an i/o to a log file. */
/*完成对日志文件的一次i/o操作。*/
void
log_io_complete(
/*============*/
	log_group_t*	group);	/* in: log group */
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
	ulint	wait);	/* in: LOG_NO_WAIT, LOG_WAIT_ONE_GROUP,
			or LOG_WAIT_ALL_GROUPS */
/********************************************************************
Advances the smallest lsn for which there are unflushed dirty blocks in the
buffer pool and also may make a new checkpoint. NOTE: this function may only
be called if the calling thread owns no synchronization objects! */
/*推进缓冲池中有未刷新脏块的最小lsn，还可能创建新的检查点。注意:这个函数只能在调用线程没有同步对象的情况下调用!*/
ibool
log_preflush_pool_modified_pages(
/*=============================*/
				/* out: FALSE if there was a flush batch of
				the same type running, which means that we
				could not start this flush batch */
	dulint	new_oldest,	/* in: try to advance oldest_modified_lsn
				at least to this lsn */
	ibool	sync);		/* in: TRUE if synchronous operation is
				desired */
/**********************************************************
Makes a checkpoint. Note that this function does not flush dirty
blocks from the buffer pool: it only checks what is lsn of the oldest
modification in the pool, and writes information about the lsn in
log files. Use log_make_checkpoint_at to flush also the pool. */
/*创建一个检查点。注意，该函数不会清除缓冲池中的脏块:它只检查缓冲池中最旧修改的lsn，
并将有关lsn的信息写入日志文件。也可以使用log_make_checkpoint_at来刷新池。*/
ibool
log_checkpoint(
/*===========*/
				/* out: TRUE if success, FALSE if a checkpoint
				write was already running */
	ibool	sync,		/* in: TRUE if synchronous operation is
				desired */
	ibool	write_always);	/* in: the function normally checks if the
				the new checkpoint would have a greater
				lsn than the previous one: if not, then no
				physical write is done; by setting this
				parameter TRUE, a physical write will always be
				made to log files */
/********************************************************************
Makes a checkpoint at a given lsn or later. */
/*在给定的lsn或稍后时间建立检查点。*/
void
log_make_checkpoint_at(
/*===================*/
	dulint	lsn,		/* in: make a checkpoint at this or a later
				lsn, if ut_dulint_max, makes a checkpoint at
				the latest lsn */
	ibool	write_always);	/* in: the function normally checks if the
				the new checkpoint would have a greater
				lsn than the previous one: if not, then no
				physical write is done; by setting this
				parameter TRUE, a physical write will always be
				made to log files */
/********************************************************************
Makes a checkpoint at the latest lsn and writes it to first page of each
data file in the database, so that we know that the file spaces contain
all modifications up to that lsn. This can only be called at database
shutdown. This function also writes all log in log files to the log archive. */
/*在最新的lsn上创建一个检查点，并将其写入数据库中每个数据文件的第一页，这样我们就知道文件空间包含该lsn之前的所有修改。
这只能在数据库关闭时调用。此函数还将日志文件中的所有日志写入日志归档。*/
void
logs_empty_and_mark_files_at_shutdown(void);
/*=======================================*/
/**********************************************************
Reads a checkpoint info from a log group header to log_sys->checkpoint_buf. */
/*从日志组头中读取检查点信息到log_sys->checkpoint_buf。*/
void
log_group_read_checkpoint_info(
/*===========================*/
	log_group_t*	group,	/* in: log group */
	ulint		field);	/* in: LOG_CHECKPOINT_1 or LOG_CHECKPOINT_2 */
/***********************************************************************
Gets info from a checkpoint about a log group. */
/*从检查点获取关于日志组的信息。*/
void
log_checkpoint_get_nth_group_info(
/*==============================*/
	byte*	buf,	/* in: buffer containing checkpoint info */
	ulint	n,	/* in: nth slot */
	ulint*	file_no,/* out: archived file number */
	ulint*	offset);/* out: archived file offset */
/**********************************************************
Writes checkpoint info to groups. */
/*向组写入检查点信息。*/
void
log_groups_write_checkpoint_info(void);
/*==================================*/
/************************************************************************
Starts an archiving operation. */
/*启动存档操作。*/
ibool
log_archive_do(
/*===========*/
			/* out: TRUE if succeed, FALSE if an archiving
			operation was already running */
	ibool	sync,	/* in: TRUE if synchronous operation is desired */
	ulint*	n_bytes);/* out: archive log buffer size, 0 if nothing to
			archive */
/********************************************************************
Writes the log contents to the archive up to the lsn when this function was
called, and stops the archiving. When archiving is started again, the archived
log file numbers start from a number one higher, so that the archiving will
not write again to the archived log files which exist when this function
returns. */
/*当调用此函数时，将日志内容写入归档到lsn，并停止归档。
当再次开始归档时，归档的日志文件号从高1号开始，这样当这个函数返回时归档的日志文件就不会再次写入。*/
ulint
log_archive_stop(void);
/*==================*/
				/* out: DB_SUCCESS or DB_ERROR */
/********************************************************************
Starts again archiving which has been stopped. */
/*重新开始已经停止的存档。*/
ulint
log_archive_start(void);
/*===================*/
			/* out: DB_SUCCESS or DB_ERROR */
/********************************************************************
Stop archiving the log so that a gap may occur in the archived log files. */
/*停止对日志的归档，归档的日志文件可能会出现间隙。*/
ulint
log_archive_noarchivelog(void);
/*==========================*/
			/* out: DB_SUCCESS or DB_ERROR */
/********************************************************************
Start archiving the log so that a gap may occur in the archived log files. */
/*开始归档日志，以便归档的日志文件中可能出现间隙。*/
ulint
log_archive_archivelog(void);
/*========================*/
			/* out: DB_SUCCESS or DB_ERROR */
/**********************************************************
Generates an archived log file name. */
/*生成归档日志文件名。*/
void
log_archived_file_name_gen(
/*=======================*/
	char*	buf,	/* in: buffer where to write */
	ulint	id,	/* in: group id */
	ulint	file_no);/* in: file number */
/**********************************************************
Switches the database to the online backup state. */
/*将数据库切换到联机备份状态。*/
ulint
log_switch_backup_state_on(void);
/*============================*/
			/* out: DB_SUCCESS or DB_ERROR */
/**********************************************************
Switches the online backup state off. */
/*关闭在线备份状态。*/
ulint
log_switch_backup_state_off(void);
/*=============================*/
			/* out: DB_SUCCESS or DB_ERROR */
/************************************************************************
Checks that there is enough free space in the log to start a new query step.
Flushes the log buffer or makes a new checkpoint if necessary. NOTE: this
function may only be called if the calling thread owns no synchronization
objects! */
/*检查日志中是否有足够的空闲空间来启动一个新的查询步骤。
如有必要，刷新日志缓冲区或创建新的检查点。注意:这个函数只能在调用线程没有同步对象的情况下调用!*/
void
log_check_margins(void);
/*===================*/
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
	dulint		end_lsn);	/* in: read area end */
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
	ulint		new_data_offset);/* in: start offset of new data in
					buf: this parameter is used to decide
					if we have to write a new log file
					header */
/************************************************************
Sets the field values in group to correspond to a given lsn. For this function
to work, the values must already be correctly initialized to correspond to
some lsn, for instance, a checkpoint lsn. */
/*将group中的字段值设置为对应于给定的lsn。要使该函数工作，这些值必须已经正确初始化，以对应于某些lsn，例如，检查点lsn。*/
void
log_group_set_fields(
/*=================*/
	log_group_t*	group,	/* in: group */
	dulint		lsn);	/* in: lsn for which the values should be
				set */
/**********************************************************
Calculates the data capacity of a log group, when the log file headers are not
included. */
/*计算不包含日志文件头的日志组的数据容量。*/
ulint
log_group_get_capacity(
/*===================*/
				/* out: capacity in bytes */
	log_group_t*	group);	/* in: log group */
/****************************************************************
Gets a log block flush bit. */ /*获取日志块刷新位。*/
UNIV_INLINE
ibool
log_block_get_flush_bit(
/*====================*/
				/* out: TRUE if this block was the first
				to be written in a log flush */
	byte*	log_block);	/* in: log block */
/****************************************************************
Gets a log block number stored in the header. */ /*获取存储在报头中的日志块号。*/
UNIV_INLINE
ulint
log_block_get_hdr_no(
/*=================*/
				/* out: log block number stored in the block
				header */
	byte*	log_block);	/* in: log block */
/****************************************************************
Gets a log block data length. */ /*获取日志块数据长度。*/
UNIV_INLINE
ulint
log_block_get_data_len(
/*===================*/
				/* out: log block data length measured as a
				byte offset from the block start */
	byte*	log_block);	/* in: log block */
/****************************************************************
Sets the log block data length. */ /*设置日志块数据长度。*/
UNIV_INLINE
void
log_block_set_data_len(
/*===================*/
	byte*	log_block,	/* in: log block */
	ulint	len);		/* in: data length */
/****************************************************************
Gets a log block number stored in the trailer. */ /*获取存储在trailer中的日志块号。*/
UNIV_INLINE
ulint
log_block_get_trl_no(
/*=================*/
				/* out: log block number stored in the block
				trailer */
	byte*	log_block);	/* in: log block */
/****************************************************************
Gets a log block first mtr log record group offset. *//*获取日志块第一个mtr日志记录组偏移量。*/
UNIV_INLINE
ulint
log_block_get_first_rec_group(
/*==========================*/
				/* out: first mtr log record group byte offset
				from the block start, 0 if none */
	byte*	log_block);	/* in: log block */
/****************************************************************
Sets the log block first mtr log record group offset. */ /*设置日志块第一个mtr日志记录组偏移量。*/
UNIV_INLINE
void
log_block_set_first_rec_group(
/*==========================*/
	byte*	log_block,	/* in: log block */
	ulint	offset);	/* in: offset, 0 if none */
/****************************************************************
Gets a log block checkpoint number field (4 lowest bytes). */ /*获取日志块检查点编号字段(最低4个字节)。*/
UNIV_INLINE
ulint
log_block_get_checkpoint_no(
/*========================*/
				/* out: checkpoint no (4 lowest bytes) */
	byte*	log_block);	/* in: log block */
/****************************************************************
Initializes a log block in the log buffer. */ /*初始化日志缓冲区中的日志块。*/
UNIV_INLINE
void
log_block_init(
/*===========*/
	byte*	log_block,	/* in: pointer to the log buffer */
	dulint	lsn);		/* in: lsn within the log block */
/****************************************************************
Converts a lsn to a log block number. */ /*将lsn转换为日志块号。*/
UNIV_INLINE
ulint
log_block_convert_lsn_to_no(
/*========================*/
			/* out: log block number, it is > 0 and <= 1G */
	dulint	lsn);	/* in: lsn of a byte within the block */
/**********************************************************
Prints info of the log. */
/*打印日志信息。*/
void
log_print(void);
/*===========*/

extern log_t*	log_sys;

/* Values used as flags */ /*作为标志的值*/
#define LOG_FLUSH	7652559
#define LOG_CHECKPOINT	78656949
#define LOG_ARCHIVE	11122331
#define LOG_RECOVER	98887331

/* The counting of lsn's starts from this value: this must be non-zero */ 
/* lsn的计数从这个值开始:这个值必须不为零*/
#define LOG_START_LSN	ut_dulint_create(0, 16 * OS_FILE_LOG_BLOCK_SIZE)

#define LOG_BUFFER_SIZE 	(srv_log_buffer_size * UNIV_PAGE_SIZE)
#define LOG_ARCHIVE_BUF_SIZE	(srv_log_buffer_size * UNIV_PAGE_SIZE / 4)

/* Offsets of a log block header */ /*日志块报头的偏移量*/
#define	LOG_BLOCK_HDR_NO	0	/* block number which must be > 0 and
					is allowed to wrap around at 2G; the
					highest bit is set to 1 if this is the
					first log block in a log flush write
					segment */ /*块号，必须为> 0，并且允许在2G处绕圈;如果这是日志刷新写段中的第一个日志块，则最高位设置为1*/
#define LOG_BLOCK_FLUSH_BIT_MASK 0x80000000
					/* mask used to get the highest bit in
					the preceding field */ /*用于获取前面字段中最高位的掩码*/
#define	LOG_BLOCK_HDR_DATA_LEN	4	/* number of bytes of log written to
					this block */ /*写入此块的日志的字节数*/
#define	LOG_BLOCK_FIRST_REC_GROUP 6	/* offset of the first start of an
					mtr log record group in this log block,
					0 if none; if the value is the same
					as LOG_BLOCK_HDR_DATA_LEN, it means
					that the first rec group has not yet
					been catenated to this log block, but
					if it will, it will start at this
					offset; an archive recovery can
					start parsing the log records starting
					from this offset in this log block,
					if value not 0 */ /*此日志块中mtr日志记录组的第一个开始的偏移量，如果没有则为0;
					如果该值与LOG_BLOCK_HDR_DATA_LEN相同，这意味着第一个rec组还没有被连接到这个日志块，
					但是如果它将被连接，它将从这个偏移量开始;如果值不为0，归档恢复可以从这个日志块中的这个偏移量开始解析日志记录*/
#define LOG_BLOCK_CHECKPOINT_NO	8	/* 4 lower bytes of the value of
					log_sys->next_checkpoint_no when the
					log block was last written to: if the
					block has not yet been written full,
					this value is only updated before a
					log buffer flush */ /* log_sys->next_checkpoint_no上次写入时的低4字节数:
					如果该块还没有写满，该值只在刷新日志缓冲区之前更新*/
#define LOG_BLOCK_HDR_SIZE	12	/* size of the log block header in
					bytes */ /*日志块头的大小(以字节为单位)*/

/* Offsets of a log block trailer from the end of the block */ /*日志块从块的末尾开始的偏移量*/
#define	LOG_BLOCK_TRL_NO	4	/* log block number */ /*日志块编号*/
#define	LOG_BLOCK_TRL_SIZE	4	/* trailer size in bytes */ /*尾部大小(以字节为单位)*/

/* Offsets for a checkpoint field */ /*检查点字段的偏移量*/
#define LOG_CHECKPOINT_NO		0
#define LOG_CHECKPOINT_LSN		8
#define LOG_CHECKPOINT_OFFSET		16
#define LOG_CHECKPOINT_LOG_BUF_SIZE	20
#define	LOG_CHECKPOINT_ARCHIVED_LSN	24
#define	LOG_CHECKPOINT_GROUP_ARRAY	32

/* For each value < LOG_MAX_N_GROUPS the following 8 bytes: */ /*对于< LOG_MAX_N_GROUPS的每个值，以下8个字节:*/

#define LOG_CHECKPOINT_ARCHIVED_FILE_NO	0
#define LOG_CHECKPOINT_ARCHIVED_OFFSET	4

#define	LOG_CHECKPOINT_ARRAY_END	(LOG_CHECKPOINT_GROUP_ARRAY\
							+ LOG_MAX_N_GROUPS * 8)
#define LOG_CHECKPOINT_CHECKSUM_1 	LOG_CHECKPOINT_ARRAY_END
#define LOG_CHECKPOINT_CHECKSUM_2 	(4 + LOG_CHECKPOINT_ARRAY_END)
#define LOG_CHECKPOINT_SIZE		(8 + LOG_CHECKPOINT_ARRAY_END)

/* Offsets of a log file header */ /*日志文件头的偏移量*/
#define LOG_GROUP_ID		0	/* log group number */ /*日志组数*/
#define LOG_FILE_START_LSN	4	/* lsn of the start of data in this
					log file */ /*该日志文件中数据开始的lsn*/
#define LOG_FILE_NO		12	/* 4-byte archived log file number */ /*4字节的归档日志文件号*/
#define	LOG_FILE_ARCH_COMPLETED	OS_FILE_LOG_BLOCK_SIZE
					/* this 4-byte field is TRUE when
					the writing of an archived log file
					has been completed */ /*当已完成对归档日志文件的写入时，这个4字节的字段为TRUE*/
#define LOG_FILE_END_LSN	(OS_FILE_LOG_BLOCK_SIZE + 4)
					/* lsn where the archived log file
					at least extends: actually the
					archived log file may extend to a
					later lsn, as long as it is within the
					same log block as this lsn; this field
					is defined only when an archived log
					file has been completely written */ /*归档日志文件至少扩展的lsn:实际上，归档日志文件可以扩展到后面的lsn，
					只要它与这个lsn在同一个日志块内;仅当归档日志文件已完全写入时，才定义此字段*/
#define LOG_CHECKPOINT_1	OS_FILE_LOG_BLOCK_SIZE
#define LOG_CHECKPOINT_2	(3 * OS_FILE_LOG_BLOCK_SIZE)
#define LOG_FILE_HDR_SIZE	(4 * OS_FILE_LOG_BLOCK_SIZE)

#define LOG_GROUP_OK		301
#define LOG_GROUP_CORRUPTED	302

/* Log group consists of a number of log files, each of the same size; a log
group is implemented as a space in the sense of the module fil0fil. */
/*日志组由许多日志文件组成，每个文件大小相同;日志组实现为模块fil0fil意义上的空间。*/
struct log_group_struct{
	/* The following fields are protected by log_sys->mutex */ /*以下字段受到log_sys->>mutex的保护*/
	ulint		id;		/* log group id */ /*重做日志组ID*/
	ulint		n_files;	/* number of files in the group */ /*重做日志组中重做文件的数量*/
	ulint		file_size;	/* individual log file size in bytes,
					including the log file header */ /*每个重做日志文件大小，包含文件头*/
	ulint		space_id;	/* file space which implements the log
					group */  /*对应的文件空间用来实现重做文件组*/
	ulint		state;		/* LOG_GROUP_OK or
					LOG_GROUP_CORRUPTED */ /*重做日志组的状态*/
	dulint		lsn;		/* lsn used to fix coordinates within
					the log group */ /*每个重做日志组已经写入到重做日志文件的LSN*/
	ulint		lsn_offset;	/* the offset of the above lsn */ /*上述对应重做日志的偏移量*/
	ulint		n_pending_writes;/* number of currently pending flush
					writes for this log group */ /*此日志组等待写入的操作数目*/
	byte**		file_header_bufs;/* buffers for each file header in the
					group */ /*每个重做日志文件的log file header*/
	/*-----------------------------*/
	byte**		archive_file_header_bufs;/* buffers for each file
					header in the group */  /*每个重做归档文件的log file header*/
	ulint		archive_space_id;/* file space which implements the log
					group archive */ /*归档重做日志ID*/
	ulint		archived_file_no;/* file number corresponding to
					log_sys->archived_lsn */   /*归档重做日志编号*/
	ulint		archived_offset;/* file offset corresponding to
					log_sys->archived_lsn, 0 if we have
					not yet written to the archive file
					number archived_file_no */ /*已经归档到文件的偏移量*/
	ulint		next_archived_file_no;/* during an archive write,
					until the write is completed, we
					store the next value for
					archived_file_no here: the write
					completion function then sets the new
					value to ..._file_no */ /*在一次归档写入过程中，直到写入完成，我们将archiived_file_no的下一个值存储在这里:
					写入完成函数然后将新值设置为_file_no*/
	ulint		next_archived_offset; /* like the preceding field */
	/*-----------------------------*/
	dulint		scanned_lsn;	/* used only in recovery: recovery scan
					succeeded up to this lsn in this log
					group */ /*仅在恢复中使用:恢复扫描成功，直到该日志组中的lsn*/
	byte*		checkpoint_buf;	/* checkpoint header is written from
					this buffer to the group */ /*保存检查点值的缓存*/
	UT_LIST_NODE_T(log_group_t)
			log_groups;	/* list of log groups */ /*重做日志组列表*/
};	

struct log_struct{
	byte		pad[64];	/* padding to prevent other memory
					update hotspots from residing on the
					same memory cache line */ /*填充使得log_struct对象可以放在同一个cache line 中，减少竞争*/
	dulint		lsn;		/* log sequence number */ /*重做日志缓冲的LSN*/
	ulint		buf_free;	/* first free offset within the log
					buffer */ /*当前已经写入到重做日志缓冲的位置*/
	mutex_t		mutex;		/* mutex protecting the log */ /*互斥结构。保护重做日志对象*/
	byte*		buf;		/* log buffer */ /*重做日志缓冲空间*/
	ulint		buf_size;	/* log buffer size in bytes */ /*重做日志缓冲空间大小*/
	ulint		max_buf_free;	/* recommended maximum value of
					buf_free, after which the buffer is
					flushed */ /*最大可使用空间*/
	ulint		old_buf_free;	/* value of buf free when log was
					last time opened; only in the debug
					version */ /*上次写入重做日志的位置，仅在DEBUG模式实用*/
	dulint		old_lsn;	/* value of lsn when log was last time
					opened; only in the debug version */ /*上次写入重做日志的LSN，仅在DEBUG模式使用*/
	ibool		check_flush_or_checkpoint;
					/* this is set to TRUE when there may
					be need to flush the log buffer, or
					preflush buffer pool pages, or make
					a checkpoint; this MUST be TRUE when
					lsn - last_checkpoint_lsn >
					max_checkpoint_age; this flag is
					peeked at by log_free_check(), which
					does not reserve the log mutex */ 
					/*当可能需要刷新日志缓冲区或预刷新缓冲池页面，或创建检查点时，将此设置为TRUE;
					当lsn - last_checkpoint_lsn > max_checkpoint_age;这个标志由log_free_check()查看，它不保留日志互斥锁*/
	UT_LIST_BASE_NODE_T(log_group_t)
			log_groups;	/* log groups */ /*重做日志组链表*/

	/* The fields involved in the log buffer flush */ /*涉及到日志缓冲区刷新的字段*/

	ulint		buf_next_to_write;/* first offset in the log buffer
					where the byte content may not exist
					written to file, e.g., the start
					offset of a log record catenated
					later; this is advanced when a flush
					operation is completed to all the log
					groups */ /*在日志缓冲区中第一个可能不存在的字节内容写入文件的偏移量，
					例如，一个日志记录的开始偏移量。当完成对所有日志组的刷新操作时，这是高级的.
					(表示从这个位置开始的重做日志缓冲还没被写入到文件)*/
	dulint		written_to_some_lsn;
					/* first log sequence number not yet
					written to any log group; for this to
					be advanced, it is enough that the
					write i/o has been completed for any
					one log group */ /*尚未写入任何日志组的第一个日志序列号;为了进行进一步的改进，对于任何一个日志组，只要已经完成了写i/o就足够了
					(重做日志缓冲至少已经被写入到一个重做日志文件组的LSN)*/
	dulint		written_to_all_lsn;
					/* first log sequence number not yet
					written to some log group; for this to
					be advanced, it is enough that the
					write i/o has been completed for all
					log groups */ /*重做日志缓冲已经被写入到所有重做日志文件组的LSN*/
	dulint		flush_lsn;	/* end lsn for the current flush */ /*已经写入到重做日志文件的LSN*/
	ulint		flush_end_offset;/* the data in buffer ha been flushed
					up to this offset when the current
					flush ends: this field will then
					be copied to buf_next_to_write */ /*已经写入到重做日志文件的偏移量*/
	ulint		n_pending_writes;/* number of currently pending flush
					writes */ /*当前挂起的刷新写的数目*/
	os_event_t	no_flush_event;	/* this event is in the reset state
					when a flush is running; a thread
					should wait for this without owning
					the log mutex, but NOTE that to set or
					reset this event, the thread MUST own
					the log mutex! */ /*等待所有异步I/O写入到重做日志文件组的事件*/
	ibool		one_flushed;	/* during a flush, this is first FALSE
					and becomes TRUE when one log group
					has been flushed *//*写入重做文件组时首先设置为false。若有一个重做日志文件*/
	os_event_t	one_flushed_event;/* this event is reset when the
					flush has not yet completed for any
					log group; e.g., this means that a
					transaction has been committed when
					this is set; a thread should wait
					for this without owning the log mutex,
					but NOTE that to set or reset this
					event, the thread MUST own the log
					mutex! */ /*当任何日志组的刷新尚未完成时，将重置此事件;例如，当设置此参数时，意味着一个事务已经被提交;
					线程应该等待这个事件，而不需要拥有日志互斥量，但是注意，要设置或重置这个事件，线程必须拥有日志互斥量!
					（等待一个异步I/O写入到重做日志文件组的事件）*/
	ulint		n_log_ios;	/* number of log i/os initiated thus
					far */ /**迄今为止发起的日志i/o数*/
	ulint		n_log_ios_old;	/* number of log i/o's at the
					previous printout */ /*上次打印输出的日志i/o的数目*/
	time_t		last_printout_time;/* when log_print was last time
					called */ /*上次打印的时间*/

	/* Fields involved in checkpoints */ /*检查点涉及的字段*/
	ulint		max_modified_age_async;
					/* when this recommended value for lsn
					- buf_pool_get_oldest_modification()
					is exceeded, we start an asynchronous
					preflush of pool pages */ /*当超过lsn- buf_pool_get_oldest_modify()的推荐值时，我们启动池页的异步预刷*/
	ulint		max_modified_age_sync;
					/* when this recommended value for lsn
					- buf_pool_get_oldest_modification()
					is exceeded, we start a synchronous
					preflush of pool pages */ /*当超过lsn- buf_pool_get_oldest_modification()的推荐值时，我们启动池页的同步预刷*/
	ulint		adm_checkpoint_interval;
					/* administrator-specified checkpoint
					interval in terms of log growth in
					bytes; the interval actually used by
					the database can be smaller */ /*管理员指定的检查点间隔(以字节为单位的日志增长);数据库实际使用的间隔可以更小*/
	ulint		max_checkpoint_age_async;
					/* when this checkpoint age is exceeded
					we start an asynchronous writing of a
					new checkpoint */ /*当超过这个检查点年龄时，我们开始异步写一个新的检查点*/
	ulint		max_checkpoint_age;
					/* this is the maximum allowed value
					for lsn - last_checkpoint_lsn when a
					new query step is started */ /*这是一个新的查询步骤开始时lsn - last_checkpoint_lsn允许的最大值*/
	dulint		next_checkpoint_no;
					/* next checkpoint number */ /*下一个检查点数量*/
	dulint		last_checkpoint_lsn;
					/* latest checkpoint lsn */ /*上一次检查点时的LSN*/
	dulint		next_checkpoint_lsn;
					/* next checkpoint lsn */ /*下一次检查点时的LSN*/
	ulint		n_pending_checkpoint_writes;
					/* number of currently pending
					checkpoint writes */ /*正在进行写入检查点值操作的异步I/O数量*/
	rw_lock_t	checkpoint_lock;/* this latch is x-locked when a
					checkpoint write is running; a thread
					should wait for this without owning
					the log mutex */ /*当一个检查点写正在运行时，这个锁存器是x锁的;线程应该在不拥有日志互斥量的情况下等待此操作*/
	byte*		checkpoint_buf;	/* checkpoint header is read to this
					buffer */ /*检查点报头被读取到这个缓冲区*/
	/* Fields involved in archiving */ /*涉及归档的字段*/
	ulint		archiving_state;/* LOG_ARCH_ON, LOG_ARCH_STOPPING
					LOG_ARCH_STOPPED, LOG_ARCH_OFF *//*是否重做日志归档*/
	dulint		archived_lsn;	/* archiving has advanced to this lsn */ /*已经归档的重做日志LSN*/
	ulint		max_archived_lsn_age_async;
					/* recommended maximum age of
					archived_lsn, before we start
					asynchronous copying to the archive */ /*在开始异步复制到归档之前，建议archiived_lsn的最大年龄*/
	ulint		max_archived_lsn_age;
					/* maximum allowed age for
					archived_lsn */ /*同步执行归档操作的距离。*/
	dulint		next_archived_lsn;/* during an archive write,
					until the write is completed, we
					store the next value for
					archived_lsn here: the write
					completion function then sets the new
					value to archived_lsn */ /*在归档写入期间，直到写入完成，我们将archiived_lsn的下一个值存储在这里:
					然后写入完成函数将新值设置为archiived_lsn*/
	ulint		archiving_phase;/* LOG_ARCHIVE_READ or
					LOG_ARCHIVE_WRITE */ 
	ulint		n_pending_archive_ios;
					/* number of currently pending reads
					or writes in archiving */ /*异步归档操作的I/O数量*/
	rw_lock_t	archive_lock;	/* this latch is x-locked when an
					archive write is running; a thread
					should wait for this without owning
					the log mutex */ /*当存档写入运行时，这个锁存器是x锁的;线程应该在不拥有日志互斥量的情况下等待此操作*/
	ulint		archive_buf_size;/* size of archive_buf */ /*归档重做日志大小*/
	byte*		archive_buf;	/* log segment is written to the
					archive from this buffer */ /*归档重做日志的内存空间*/
	os_event_t	archiving_on;	/* if archiving has been stopped,
					a thread can wait for this event to
					become signaled */ /*如果归档已经停止，线程可以等待这个事件被通知*/ 
	/* Fields involved in online backups */ /*涉及在线备份的字段*/
	ibool		online_backup_state;
					/* TRUE if the database is in the
					online backup state */ /*如果数据库处于在线备份状态，则为“TRUE”*/
	dulint		online_backup_lsn;
					/* lsn when the state was changed to
					the online backup state */ /*当lsn状态变为在线备份状态时*/
};

#define LOG_ARCH_ON		71
#define LOG_ARCH_STOPPING	72
#define LOG_ARCH_STOPPING2	73
#define LOG_ARCH_STOPPED	74
#define LOG_ARCH_OFF		75

#ifndef UNIV_NONINL
#include "log0log.ic"
#endif

#endif
