/******************************************************
Mini-transaction buffer

(c) 1995 Innobase Oy

Created 11/26/1995 Heikki Tuuri
*******************************************************/
/*Mini-transaction缓冲*/
#ifndef mtr0mtr_h
#define mtr0mtr_h

#include "univ.i"
#include "mem0mem.h"
#include "dyn0dyn.h"
#include "buf0types.h"
#include "sync0rw.h"
#include "ut0byte.h"
#include "mtr0types.h"
#include "page0types.h"

/* Logging modes for a mini-transaction */ /*小型事务的日志模式*/
#define MTR_LOG_ALL		21	/* default mode: log all operations
					modifying disk-based data */ /*记录所有修改基于磁盘的数据的操作*/
#define	MTR_LOG_NONE		22	/* log no operations */
/*#define	MTR_LOG_SPACE	23 */	/* log only operations modifying
					file space page allocation data
					(operations in fsp0fsp.* ) */ /*仅记录修改文件空间页面分配数据的操作(fsp0fsp. log)。*)*/
#define	MTR_LOG_SHORT_INSERTS	24	/* inserts are logged in a shorter
					form */ /*插入以较短的形式记录*/
					
/* Types for the mlock objects to store in the mtr memo; NOTE that the
first 3 values must be RW_S_LATCH, RW_X_LATCH, RW_NO_LATCH */
/*要存储在mtr memo中的mlock对象的类型;请注意,前3个必须是RW_S_LATCH、RW_X_LATCH、RW_NO_LATCH*/
#define	MTR_MEMO_PAGE_S_FIX	RW_S_LATCH
#define	MTR_MEMO_PAGE_X_FIX	RW_X_LATCH
#define	MTR_MEMO_BUF_FIX	RW_NO_LATCH
#define MTR_MEMO_MODIFY		54
#define	MTR_MEMO_S_LOCK		55
#define	MTR_MEMO_X_LOCK		56

/* Log item types: we have made them to be of the type 'byte'
for the compiler to warn if val and type parameters are switched
in a call to mlog_write_ulint. NOTE! For 1 - 8 bytes, the
flag value must give the length also! */
/*日志项类型:我们将它们设置为'byte'类型，以便编译器在调用mlog_write_ulint时，
如果val和类型参数被切换，编译器会发出警告。注意!对于1 - 8字节，标志值也必须给出长度!*/
#define	MLOG_SINGLE_REC_FLAG	128		/* if the mtr contains only
						one log record for one page,
						i.e., write_initial_log_record
						has been called only once,
						this flag is ORed to the type
						of that first log record */
/*如果mtr只包含一页日志记录，即write_initial_log_record只被调用一次，这个标志与第一个日志记录的类型有关*/
#define	MLOG_1BYTE		((byte)1) 	/* one byte is written */
#define	MLOG_2BYTES		((byte)2)	/* 2 bytes ... */
#define	MLOG_4BYTES		((byte)4)	/* 4 bytes ... */
#define	MLOG_8BYTES		((byte)8)	/* 8 bytes ... */
#define	MLOG_REC_INSERT		((byte)9)	/* record insert */
#define	MLOG_REC_CLUST_DELETE_MARK ((byte)10) 	/* mark clustered index record
						deleted */ /*标记聚集索引记录已删除*/
#define	MLOG_REC_SEC_DELETE_MARK ((byte)11) 	/* mark secondary index record
						deleted */ /*标记二级索引记录删除*/
#define MLOG_REC_UPDATE_IN_PLACE ((byte)13)	/* update of a record,
						preserves record field sizes */ /*更新一个记录，保存记录字段的大小*/
#define MLOG_REC_DELETE		((byte)14)	/* delete a record from a
						page */ /*从页面中删除一条记录*/
#define	MLOG_LIST_END_DELETE 	((byte)15)	/* delete record list end on
						index page */ /*删除记录列表结束在索引页*/
#define	MLOG_LIST_START_DELETE 	((byte)16) 	/* delete record list start on
						index page */ /*删除记录列表从索引页开始*/
#define	MLOG_LIST_END_COPY_CREATED ((byte)17) 	/* copy record list end to a
						new created index page */ /*复制记录列表结束到新创建的索引页*/
#define	MLOG_PAGE_REORGANIZE 	((byte)18)	/* reorganize an index page */ /*重新组织索引页*/
#define MLOG_PAGE_CREATE 	((byte)19)	/* create an index page */ /*创建索引页*/
#define	MLOG_UNDO_INSERT 	((byte)20)	/* insert entry in an undo
						log */ /*在撤销日志中插入条目*/
#define MLOG_UNDO_ERASE_END	((byte)21)	/* erase an undo log page end */ /*删除undo log页面结束*/
#define	MLOG_UNDO_INIT 		((byte)22)	/* initialize a page in an
						undo log */ /*在撤消日志中初始化页面*/
#define MLOG_UNDO_HDR_DISCARD	((byte)23)	/* discard an update undo log
						header */ /*丢弃update undo日志头*/
#define	MLOG_UNDO_HDR_REUSE	((byte)24)	/* reuse an insert undo log
						header */ /*重用插入撤销日志头*/
#define MLOG_UNDO_HDR_CREATE	((byte)25)	/* create an undo log header */ /*创建undo日志头*/
#define MLOG_REC_MIN_MARK	((byte)26)	/* mark an index record as the
						predefined minimum record */ /*将索引记录标记为预定义的最小记录*/
#define MLOG_IBUF_BITMAP_INIT	((byte)27)	/* initialize an ibuf bitmap
						page */ /*初始化ibuf位图页面*/
#define	MLOG_FULL_PAGE		((byte)28)	/* full contents of a page */ /*页面的完整内容*/
#define MLOG_INIT_FILE_PAGE	((byte)29)	/* this means that a file page
						is taken into use and the prior
						contents of the page should be
						ignored: in recovery we must
						not trust the lsn values stored
						to the file page */ 
						/*这意味着文件页将被使用，并且该页之前的内容应该被忽略:在恢复过程中，我们不能信任存储在文件页中的lsn值*/
#define MLOG_WRITE_STRING	((byte)30)	/* write a string to a page */ /*将字符串写入页面*/
#define	MLOG_MULTI_REC_END	((byte)31)	/* if a single mtr writes
						log records for several pages,
						this log record ends the
						sequence of these records */ 
						/*如果一个mtr写了几个页面的日志记录，那么这个日志记录将结束这些记录的顺序*/
#define MLOG_DUMMY_RECORD	((byte)32)	/* dummy log record used to
						pad a log block full */ /*虚拟日志记录用于填充日志块*/
#define MLOG_BIGGEST_TYPE	((byte)32) 	/* biggest value (used in
						asserts) */ /*最大值(在断言中使用)*/
					
/*******************************************************************
Starts a mini-transaction and creates a mini-transaction handle 
and buffer in the memory buffer given by the caller. */
/*启动一个小事务，并在调用者给出的内存缓冲区中创建一个小事务句柄和缓冲区。*/
UNIV_INLINE
mtr_t*
mtr_start(
/*======*/
			/* out: mtr buffer which also acts as
			the mtr handle */
	mtr_t*	mtr);	/* in: memory buffer for the mtr buffer */
/*******************************************************************
Starts a mini-transaction and creates a mini-transaction handle 
and buffer in the memory buffer given by the caller. */
/*启动一个小事务，并在调用者给出的内存缓冲区中创建一个小事务句柄和缓冲区。*/
mtr_t*
mtr_start_noninline(
/*================*/
			/* out: mtr buffer which also acts as
			the mtr handle */
	mtr_t*	mtr);	/* in: memory buffer for the mtr buffer */
/*******************************************************************
Commits a mini-transaction. */
/*提交一个mini-transaction。*/
void
mtr_commit(
/*=======*/
	mtr_t*	mtr);	/* in: mini-transaction */
/****************************************************************
Writes to the database log the full contents of the pages that this mtr is
the first to modify in the buffer pool. This function is called when the
database is in the online backup state. */
/*将此mtr在缓冲池中首先修改的页面的全部内容写入数据库日志。当数据库处于联机备份状态时，调用此函数。*/
void
mtr_log_write_backup_entries(
/*=========================*/
	mtr_t*	mtr,		/* in: mini-transaction */
	dulint	backup_lsn);	/* in: online backup lsn */
/**************************************************************
Sets and returns a savepoint in mtr. */
/*设置并返回mtr中的保存点。*/
UNIV_INLINE
ulint
mtr_set_savepoint(
/*==============*/
			/* out: savepoint */
	mtr_t*	mtr);	/* in: mtr */
/**************************************************************
Releases the latches stored in an mtr memo down to a savepoint.
NOTE! The mtr must not have made changes to buffer pages after the
savepoint, as these can be handled only by mtr_commit. */
/*释放锁存在mtr备忘录到一个保存点。注意!mtr必须没有在保存点之后对缓冲区页面进行更改，
因为这些更改只能由mtr_commit处理。*/
void
mtr_rollback_to_savepoint(
/*======================*/
	mtr_t*	mtr,		/* in: mtr */
	ulint	savepoint);	/* in: savepoint */
/**************************************************************
Releases the (index tree) s-latch stored in an mtr memo after a
savepoint. */
/*在保存点之后释放存储在mtr备忘录中的(索引树)s锁存器。*/
UNIV_INLINE
void
mtr_release_s_latch_at_savepoint(
/*=============================*/
	mtr_t*		mtr,		/* in: mtr */
	ulint		savepoint,	/* in: savepoint */
	rw_lock_t* 	lock);		/* in: latch to release */
/*******************************************************************
Gets the logging mode of a mini-transaction. */
/*获取小型事务的日志记录模式。*/
UNIV_INLINE
ulint
mtr_get_log_mode(
/*=============*/
			/* out: logging mode: MTR_LOG_NONE, ... */
	mtr_t*	mtr);	/* in: mtr */
/*******************************************************************
Changes the logging mode of a mini-transaction. */
/*更改小事务的日志记录模式。*/
UNIV_INLINE
ulint
mtr_set_log_mode(
/*=============*/
			/* out: old mode */
	mtr_t*	mtr,	/* in: mtr */
	ulint	mode);	/* in: logging mode: MTR_LOG_NONE, ... */
/************************************************************
Reads 1 - 4 bytes from a file page buffered in the buffer pool. */
/*从缓冲池中缓冲的文件页读取1 - 4个字节。*/
ulint
mtr_read_ulint(
/*===========*/
			/* out: value read */
	byte*	ptr,	/* in: pointer from where to read */
	ulint	type,	/* in: MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/************************************************************
Reads 8 bytes from a file page buffered in the buffer pool. */
/*从缓冲池中缓冲的文件页读取8个字节。*/
dulint
mtr_read_dulint(
/*===========*/
			/* out: value read */
	byte*	ptr,	/* in: pointer from where to read */
	ulint	type,	/* in: MLOG_8BYTES */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/*************************************************************************
This macro locks an rw-lock in s-mode. */
/*这个宏在s模式下锁定rw-lock。*/
#define mtr_s_lock(B, MTR)	mtr_s_lock_func((B), IB__FILE__, __LINE__,\
						(MTR))
/*************************************************************************
This macro locks an rw-lock in x-mode. */
/*这个宏在x模式下锁定了rw-lock。*/
#define mtr_x_lock(B, MTR)	mtr_x_lock_func((B), IB__FILE__, __LINE__,\
						(MTR))
/*************************************************************************
NOTE! Use the macro above!
Locks a lock in s-mode. */
/*注意!使用上面的宏!锁定s模式下的锁。*/
UNIV_INLINE
void
mtr_s_lock_func(
/*============*/
	rw_lock_t*	lock,	/* in: rw-lock */
	char*		file,	/* in: file name */
	ulint		line,	/* in: line number */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************************
NOTE! Use the macro above!
Locks a lock in x-mode. */
/*注意!使用上面的宏!在x模式下锁定一个锁。*/
UNIV_INLINE
void
mtr_x_lock_func(
/*============*/
	rw_lock_t*	lock,	/* in: rw-lock */
	char*		file,	/* in: file name */
	ulint		line,	/* in: line number */
	mtr_t*		mtr);	/* in: mtr */

/*******************************************************
Releases an object in the memo stack. */
/*释放备忘录堆栈中的对象。*/
void
mtr_memo_release(
/*=============*/
	mtr_t*	mtr,	/* in: mtr */
	void*	object,	/* in: object */
	ulint	type);	/* in: object type: MTR_MEMO_S_LOCK, ... */
/****************************************************************
Parses a log record which contains the full contents of a page. */
/*解析包含页面全部内容的日志记录。*/
byte*
mtr_log_parse_full_page(
/*====================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page);	/* in: page or NULL */
/**************************************************************
Checks if memo contains the given item. */
/*检查备忘录是否包含给定的项目。*/
UNIV_INLINE
ibool
mtr_memo_contains(
/*==============*/
			/* out: TRUE if contains */
	mtr_t*	mtr,	/* in: mtr */
	void*	object,	/* in: object to search */
	ulint	type);	/* in: type of object */
/*************************************************************
Prints info of an mtr handle. */
void
mtr_print(
/*======*/
	mtr_t*	mtr);	/* in: mtr */
/*######################################################################*/

#define	MTR_BUF_MEMO_SIZE	200	/* number of slots in memo */

/*******************************************************************
Returns the log object of a mini-transaction buffer. */
/*返回小事务缓冲区的日志对象。*/
UNIV_INLINE
dyn_array_t*
mtr_get_log(
/*========*/
			/* out: log */
	mtr_t*	mtr);	/* in: mini-transaction */
/*******************************************************
Pushes an object to an mtr memo stack. */
/*将对象推入mtr备忘录堆栈。*/
UNIV_INLINE
void
mtr_memo_push(
/*==========*/
	mtr_t*	mtr,	/* in: mtr */
	void*	object,	/* in: object */
	ulint	type);	/* in: object type: MTR_MEMO_S_LOCK, ... */


/* Type definition of a mini-transaction memo stack slot. */
/*小型交易备忘录堆栈槽的类型定义。*/
typedef	struct mtr_memo_slot_struct	mtr_memo_slot_t;
struct mtr_memo_slot_struct{
	ulint	type;	/* type of the stored object (MTR_MEMO_S_LOCK, ...) */
	void*	object;	/* pointer to the object */
};

/* Mini-transaction handle and buffer */
struct mtr_struct{
	ulint		state;	/* MTR_ACTIVE, MTR_COMMITTING, MTR_COMMITTED */
	dyn_array_t	memo;	/* memo stack for locks etc. */
	dyn_array_t	log;	/* mini-transaction log */
	ibool		modifications;
				/* TRUE if the mtr made modifications to
				buffer pool pages */
	ulint		n_log_recs;
				/* count of how many page initial log records
				have been written to the mtr log */
	ulint		log_mode; /* specifies which operations should be
				logged; default value MTR_LOG_ALL */
	dulint		start_lsn;/* start lsn of the possible log entry for
				this mtr */
	dulint		end_lsn;/* end lsn of the possible log entry for
				this mtr */
	ulint		magic_n;
};

#define	MTR_MAGIC_N		54551

#define MTR_ACTIVE		12231
#define MTR_COMMITTING		56456
#define MTR_COMMITTED		34676
	
#ifndef UNIV_NONINL
#include "mtr0mtr.ic"
#endif

#endif
