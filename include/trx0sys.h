/******************************************************
Transaction system
交易系统
(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0sys_h
#define trx0sys_h

#include "univ.i"

#include "trx0types.h"
#include "mtr0mtr.h"
#include "mtr0log.h"
#include "ut0byte.h"
#include "mem0mem.h"
#include "sync0sync.h"
#include "ut0lst.h"
#include "buf0buf.h"
#include "fil0fil.h"
#include "fut0lst.h"
#include "fsp0fsp.h"
#include "read0types.h"

/* The transaction system 事务系统*/
extern trx_sys_t*	trx_sys;

/* Doublewrite system Doublewrite系统*/
extern trx_doublewrite_t*	trx_doublewrite;

/********************************************************************
Creates the doublewrite buffer at a database start. The header of the
doublewrite buffer is placed on the trx system header page. 
在数据库启动时创建doublewrite缓冲区。doublewrite缓冲区的头被放置在trx系统头页上。*/
void
trx_sys_create_doublewrite_buf(void);
/*================================*/
/********************************************************************
At a database startup uses a possible doublewrite buffer to restore
half-written pages in the data files. 
在数据库启动时，使用一个可能的doublewrite缓冲区来恢复数据文件中半写的页。*/
void
trx_sys_doublewrite_restore_corrupt_pages(void);
/*===========================================*/
/*******************************************************************
Checks if a page address is the trx sys header page. 检查页面地址是否为trx sys报头页。*/
UNIV_INLINE
ibool
trx_sys_hdr_page(
/*=============*/
			/* out: TRUE if trx sys header page */
	ulint	space,	/* in: space */
	ulint	page_no);/* in: page number */
/*********************************************************************
Creates and initializes the central memory structures for the transaction
system. This is called when the database is started. 
为事务系统创建和初始化中央内存结构。这在数据库启动时调用。*/
void
trx_sys_init_at_db_start(void);
/*==========================*/
/*********************************************************************
Creates and initializes the transaction system at the database creation.
在创建数据库时创建并初始化事务系统。 */
void
trx_sys_create(void);
/*================*/
/********************************************************************
Looks for a free slot for a rollback segment in the trx system file copy. 
在trx系统文件副本中为回滚段寻找空闲槽。*/
ulint
trx_sysf_rseg_find_free(
/*====================*/
					/* out: slot index or ULINT_UNDEFINED
					if not found */
	mtr_t*		mtr);		/* in: mtr */
/*******************************************************************
Gets the pointer in the nth slot of the rseg array. 获取rseg数组的第n个槽中的指针。*/
UNIV_INLINE
trx_rseg_t*
trx_sys_get_nth_rseg(
/*=================*/
				/* out: pointer to rseg object, NULL if slot
				not in use */
	trx_sys_t*	sys,	/* in: trx system */
	ulint		n);	/* in: index of slot */
/*******************************************************************
Sets the pointer in the nth slot of the rseg array. 设置rseg数组的第n个槽位的指针。*/
UNIV_INLINE
void
trx_sys_set_nth_rseg(
/*=================*/
	trx_sys_t*	sys,	/* in: trx system */
	ulint		n,	/* in: index of slot */
	trx_rseg_t*	rseg);	/* in: pointer to rseg object, NULL if slot
				not in use */
/**************************************************************************
Gets a pointer to the transaction system file copy and x-locks its page. 获取一个指向事务系统文件副本的指针，并对其页面进行x锁。*/
UNIV_INLINE
trx_sysf_t*
trx_sysf_get(
/*=========*/
			/* out: pointer to system file copy, page x-locked */
	mtr_t*	mtr);	/* in: mtr */
/*********************************************************************
Gets the space of the nth rollback segment slot in the trx system
file copy.获取trx系统文件副本中第n个回滚段槽的空间。 */
UNIV_INLINE
ulint
trx_sysf_rseg_get_space(
/*====================*/
					/* out: space id */
	trx_sysf_t*	sys_header,	/* in: trx sys file copy */
	ulint		i,		/* in: slot index == rseg id */
	mtr_t*		mtr);		/* in: mtr */
/*********************************************************************
Gets the page number of the nth rollback segment slot in the trx system
file copy.获取trx系统文件副本中第n个回滚段槽的页码。 */
UNIV_INLINE
ulint
trx_sysf_rseg_get_page_no(
/*======================*/
					/* out: page number, FIL_NULL
					if slot unused */
	trx_sysf_t*	sys_header,	/* in: trx sys file copy */
	ulint		i,		/* in: slot index == rseg id */
	mtr_t*		mtr);		/* in: mtr */
/*********************************************************************
Sets the space id of the nth rollback segment slot in the trx system
file copy.设置trx系统文件副本中第n个回滚段槽位的空格id。 */
UNIV_INLINE
void
trx_sysf_rseg_set_space(
/*====================*/
	trx_sysf_t*	sys_header,	/* in: trx sys file copy */
	ulint		i,		/* in: slot index == rseg id */
	ulint		space,		/* in: space id */
	mtr_t*		mtr);		/* in: mtr */
/*********************************************************************
Sets the page number of the nth rollback segment slot in the trx system
file copy. 设置trx系统文件副本中第n个回滚段槽位的页码。*/
UNIV_INLINE
void
trx_sysf_rseg_set_page_no(
/*======================*/
	trx_sysf_t*	sys_header,	/* in: trx sys file copy */
	ulint		i,		/* in: slot index == rseg id */
	ulint		page_no,	/* in: page number, FIL_NULL if
					the slot is reset to unused */
	mtr_t*		mtr);		/* in: mtr */
/*********************************************************************
Allocates a new transaction id. 分配一个新的事务id。*/
UNIV_INLINE
dulint
trx_sys_get_new_trx_id(void);
/*========================*/
			/* out: new, allocated trx id */
/*********************************************************************
Allocates a new transaction number. 分配一个新的事务号。*/
UNIV_INLINE
dulint
trx_sys_get_new_trx_no(void);
/*========================*/
			/* out: new, allocated trx number */
/*********************************************************************
Writes a trx id to an index page. In case that the id size changes in
some future version, this function should be used instead of
mach_write_...将trx id写入索引页。如果在将来的版本中id大小发生了变化，应该使用这个函数而不是mach_write_… */
UNIV_INLINE
void
trx_write_trx_id(
/*=============*/
	byte*	ptr,	/* in: pointer to memory where written */
	dulint	id);	/* in: id */
/*********************************************************************
Reads a trx id from an index page. In case that the id size changes in
some future version, this function should be used instead of
mach_read_... 从索引页中读取trx id。如果在将来的版本中id大小发生了变化，应该使用这个函数而不是mach_read_…*/
UNIV_INLINE
dulint
trx_read_trx_id(
/*============*/
			/* out: id */
	byte*	ptr);	/* in: pointer to memory from where to read */
/********************************************************************
Looks for the trx handle with the given id in trx_list.在trx_list中查找具有给定id的trx句柄。 */
UNIV_INLINE
trx_t*
trx_get_on_id(
/*==========*/
			/* out: the trx handle or NULL if not found */
	dulint	trx_id);	/* in: trx id to search for */
/********************************************************************
Returns the minumum trx id in trx list. This is the smallest id for which
the trx can possibly be active. (But, you must look at the trx->conc_state to
find out if the minimum trx id transaction itself is active, or already
committed.) 返回trx列表中trx id的最小值。这是trx可能处于活动状态的最小id。(但是，您必须查看trx->conc_state，以确定最小trx id事务本身是活动的，还是已经提交。)*/
UNIV_INLINE
dulint
trx_list_get_min_trx_id(void);
/*=========================*/
			/* out: the minimum trx id, or trx_sys->max_trx_id
			if the trx list is empty */
/********************************************************************
Checks if a transaction with the given id is active. 检查具有给定id的事务是否处于活动状态。*/
UNIV_INLINE
ibool
trx_is_active(
/*==========*/
			/* out: TRUE if active */
	dulint	trx_id);/* in: trx id of the transaction */
/********************************************************************
Checks that trx is in the trx list. 检查trx是否在trx列表中。*/

ibool
trx_in_trx_list(
/*============*/
			/* out: TRUE if is in */
	trx_t*	in_trx);/* in: trx */
/*********************************************************************
Updates the offset information about the end of the MySQL binlog entry
which corresponds to the transaction just being committed. 
更新MySQL binlog条目末尾的偏移量信息，它对应于刚刚提交的事务。*/
void
trx_sys_update_mysql_binlog_offset(
/*===============================*/
	trx_t*	trx,	/* in: transaction being committed */
	mtr_t*	mtr);	/* in: mtr */
/*********************************************************************
Prints to stderr the MySQL binlog offset info in the trx system header if
the magic number shows it valid. 
打印到stderr的MySQL binlog偏移量信息在trx系统头如果魔术数字显示它有效。*/
void
trx_sys_print_mysql_binlog_offset(void);
/*===================================*/

/* The automatically created system rollback segment has this id 自动创建的系统回滚段具有此id*/
#define TRX_SYS_SYSTEM_RSEG_ID	0

/* Max number of rollback segments: the number of segment specification slots
in the transaction system array; rollback segment id must fit in one byte,
therefore 256 回滚段的最大数量:事务系统数组中的段规范槽的数量;回滚段id必须符合一个字节，因此256*/
#define	TRX_SYS_N_RSEGS		256

/* Space id and page no where the trx system file copy resides trx系统文件副本所在的空间id和页no*/
#define	TRX_SYS_SPACE	0	/* the SYSTEM tablespace 系统表空间*/
#define	TRX_SYS_PAGE_NO	FSP_TRX_SYS_PAGE_NO

/* The offset of the transaction system header on the page 页上事务系统标头的偏移量*/
#define	TRX_SYS		FSEG_PAGE_DATA

/* Transaction system header; protected by trx_sys->mutex 交易系统头文件;保护trx_sys->mutex*/
/*-------------------------------------------------------------*/
#define	TRX_SYS_TRX_ID_STORE	0	/* the maximum trx id or trx number
					modulo TRX_SYS_TRX_ID_UPDATE_MARGIN
					written to a file page by any
					transaction; the assignment of
					transaction ids continues from this
					number rounded up by .._MARGIN plus
					.._MARGIN when the database is
					started 任何事务写入文件页的TRX_SYS_TRX_ID_UPDATE_MARGIN的最大trx id或trx编号模;事务id的分配从这个数字四舍五入到.._MARGIN + . .数据库启动时的_MARGIN*/
#define TRX_SYS_FSEG_HEADER	8	/* segment header for the tablespace
					segment the trx system is created
					into TRX系统创建的表空间段的段头*/
#define	TRX_SYS_RSEGS		(8 + FSEG_HEADER_SIZE)	
					/* the start of the array of rollback
					segment specification slots 回滚段规范槽数组的开始*/
/*-------------------------------------------------------------*/

#define TRX_SYS_MYSQL_LOG_NAME_LEN	32
#define TRX_SYS_MYSQL_LOG_MAGIC_N	873422344

/* The offset of the MySQL binlog offset info on the trx system header page 
trx系统头页上MySQL binlog偏移量信息的偏移量*/
#define TRX_SYS_MYSQL_LOG_INFO		(UNIV_PAGE_SIZE - 300)
#define	TRX_SYS_MYSQL_LOG_MAGIC_N_FLD	0	/* magic number which shows
						if we have valid data in the
						MySQL binlog info; the value
						is ..._MAGIC_N if yes 
						魔术数字，显示我们是否有有效的数据在MySQL binlog信息;值是…_MAGIC_N如果是*/
#define TRX_SYS_MYSQL_LOG_NAME		4	/* MySQL log file name MySQL日志文件名*/
#define TRX_SYS_MYSQL_LOG_OFFSET_HIGH	(4 + TRX_SYS_MYSQL_LOG_NAME_LEN)
						/* high 4 bytes of the offset
						within that file 该文件中偏移量的高4字节*/
#define TRX_SYS_MYSQL_LOG_OFFSET_LOW	(8 + TRX_SYS_MYSQL_LOG_NAME_LEN)
						/* low 4 bytes of the offset
						within that file 该文件中低4字节的偏移量*/

/* The offset of the doublewrite buffer header on the trx system header page 
trx系统报头页上的doublewrite缓冲区报头的偏移量*/
#define TRX_SYS_DOUBLEWRITE		(UNIV_PAGE_SIZE - 200)
/*-------------------------------------------------------------*/
#define TRX_SYS_DOUBLEWRITE_FSEG 	0	/* fseg header of the fseg
						containing the doublewrite
						buffer 包含doublewrite缓冲区的Fseg的头文件*/
#define TRX_SYS_DOUBLEWRITE_MAGIC 	FSEG_HEADER_SIZE
						/* 4-byte magic number which
						shows if we already have
						created the doublewrite
						buffer 4字节的幻数，用来显示是否已经创建了doublewrite缓冲区*/
#define TRX_SYS_DOUBLEWRITE_BLOCK1	(4 + FSEG_HEADER_SIZE)
						/* page number of the
						first page in the first
						sequence of 64
						(= FSP_EXTENT_SIZE) consecutive
						pages in the doublewrite
						buffer doublewrite缓冲区中连续64个(= FSP_EXTENT_SIZE)页的第一个序列的第一个页的页码*/
#define TRX_SYS_DOUBLEWRITE_BLOCK2	(8 + FSEG_HEADER_SIZE)
						/* page number of the
						first page in the second
						sequence of 64 consecutive
						pages in the doublewrite
						buffer 双写缓冲区中连续64页的第二序列中的第一页的页码*/
#define TRX_SYS_DOUBLEWRITE_REPEAT	12	/* we repeat the above 3
						numbers so that if the trx
						sys header is half-written
						to disk, we still may be able
						to recover the information 我们重复上述3个数字，以便如果TRX系统头被半写入磁盘，我们仍然可以恢复信息*/
/*-------------------------------------------------------------*/
#define TRX_SYS_DOUBLEWRITE_MAGIC_N	536853855

#define TRX_SYS_DOUBLEWRITE_BLOCK_SIZE	FSP_EXTENT_SIZE	

/* Doublewrite control struct Doublewrite控制结构*/
struct trx_doublewrite_struct{
	mutex_t	mutex;		/* mutex protecting the first_free field and
				write_buf 保护first_free字段和write_buf的互斥锁*/
	ulint	block1;		/* the page number of the first
				doublewrite block (64 pages) 第一个doublewrite块的页码(64页)*/
	ulint	block2;		/* page number of the second block 第2块的页码*/
	ulint	first_free;	/* first free position in write_buf measured
				in units of UNIV_PAGE_SIZE write_buf中的第一个自由位置，单位为UNIV_PAGE_SIZE*/
	byte*	write_buf; 	/* write buffer used in writing to the
				doublewrite buffer, aligned to an
				address divisible by UNIV_PAGE_SIZE
				(which is required by Windows aio) */ /*写入doublewrite缓冲区，对齐到UNIV_PAGE_SIZE可整除的地址(Windows aio要求)*/
	byte*	write_buf_unaligned; /* pointer to write_buf, but unaligned */
	buf_block_t**
		buf_block_arr;	/* array to store pointers to the buffer
				blocks which have been cached to write_buf 指向已缓存到write_buf的缓冲区块的指针数组*/
};

/* The transaction system central memory data structure; protected by the
kernel mutex 事务系统中央存储数据结构;由内核互斥锁保护*/
struct trx_sys_struct{
	dulint		max_trx_id;	/* The smallest number not yet
					assigned as a transaction id or
					transaction number 尚未分配为事务id或事务号的最小数字*/
	UT_LIST_BASE_NODE_T(trx_t) trx_list;
					/* List of active and committed in
					memory transactions, sorted on trx id,
					biggest first 内存事务中活动和提交的列表，按trx id排序，最大优先*/
	UT_LIST_BASE_NODE_T(trx_t) mysql_trx_list;
					/* List of transactions created
					for MySQL 为MySQL创建的事务列表*/
	UT_LIST_BASE_NODE_T(trx_rseg_t) rseg_list;
					/* List of rollback segment objects 回滚段对象列表*/
	trx_rseg_t*	latest_rseg;	/* Latest rollback segment in the
					round-robin assignment of rollback
					segments to transactions 将回滚段轮循分配给事务中的最新回滚段*/
	trx_rseg_t*	rseg_array[TRX_SYS_N_RSEGS];
					/* Pointer array to rollback segments;
					NULL if slot not in use 指向回滚段的指针数组;如果槽位不使用则为NULL*/
	UT_LIST_BASE_NODE_T(read_view_t) view_list;
					/* List of read views sorted on trx no,
					biggest first 读视图列表按trx排序不，最大的第一个*/
};

/* When a trx id which is zero modulo this number (which must be a power of
two) is assigned, the field TRX_SYS_TRX_ID_STORE on the transaction system
page is updated 当分配的trx id对这个数字取零模(必须是2的幂)时，事务系统页面上的字段TRX_SYS_TRX_ID_STORE将被更新*/
#define TRX_SYS_TRX_ID_WRITE_MARGIN	256

#ifndef UNIV_NONINL
#include "trx0sys.ic"
#endif

#endif 
