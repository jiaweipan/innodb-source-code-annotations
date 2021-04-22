/******************************************************
Mini-transaction logging routines

(c) 1995 Innobase Oy

Created 12/7/1995 Heikki Tuuri
*******************************************************/
/*Mini-transaction日志记录程序*/
#ifndef mtr0log_h
#define mtr0log_h

#include "univ.i"
#include "mtr0mtr.h"

/************************************************************
Writes 1 - 4 bytes to a file page buffered in the buffer pool.
Writes the corresponding log record to the mini-transaction log. */
/*向缓冲池中缓冲的文件页写入1 - 4个字节。将相应的日志记录写入小事务日志。*/
void
mlog_write_ulint(
/*=============*/
	byte*	ptr,	/* in: pointer where to write */
	ulint	val,	/* in: value to write */
	byte	type,	/* in: MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/************************************************************
Writes 8 bytes to a file page buffered in the buffer pool.
Writes the corresponding log record to the mini-transaction log. */
/*向缓冲池中缓冲的文件页写入8个字节。将相应的日志记录写入小事务日志。*/
void
mlog_write_dulint(
/*==============*/
	byte*	ptr,	/* in: pointer where to write */
	dulint	val,	/* in: value to write */
	byte	type,	/* in: MLOG_8BYTES */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/************************************************************
Writes a string to a file page buffered in the buffer pool. Writes the
corresponding log record to the mini-transaction log. */
/*将字符串写入缓冲池中缓冲的文件页。将相应的日志记录写入小事务日志。*/
void
mlog_write_string(
/*==============*/
	byte*	ptr,	/* in: pointer where to write */
	byte*	str,	/* in: string to write */
	ulint	len,	/* in: string length */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/************************************************************
Writes initial part of a log record consisting of one-byte item
type and four-byte space and page numbers. */
/*写入由单字节项类型、四字节空间和页码组成的日志记录的初始部分。*/
void
mlog_write_initial_log_record(
/*==========================*/
	byte*	ptr,	/* in: pointer to (inside) a buffer frame
			holding the file page where modification
			is made */
	byte	type,	/* in: log item type: MLOG_1BYTE, ... */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/************************************************************
Catenates 1 - 4 bytes to the mtr log. */ /*将1 - 4字节连接到mtr日志。*/
UNIV_INLINE
void
mlog_catenate_ulint(
/*================*/
	mtr_t*	mtr,	/* in: mtr */
	ulint	val,	/* in: value to write */
	ulint	type);	/* in: MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES */
/************************************************************
Catenates n bytes to the mtr log. */
/*将n个字节连接到mtr日志。*/
void
mlog_catenate_string(
/*=================*/
	mtr_t*	mtr,	/* in: mtr */
	byte*	str,	/* in: string to write */
	ulint	len);	/* in: string length */
/************************************************************
Catenates a compressed ulint to mlog. */ /*将一个压缩的ulint连接到mlog。*/
UNIV_INLINE
void
mlog_catenate_ulint_compressed(
/*===========================*/
	mtr_t*	mtr,	/* in: mtr */
	ulint	val);	/* in: value to write */
/************************************************************
Catenates a compressed dulint to mlog. */ /*将一个压缩的dulint连接到mlog。*/
UNIV_INLINE
void
mlog_catenate_dulint_compressed(
/*============================*/
	mtr_t*	mtr,	/* in: mtr */
	dulint	val);	/* in: value to write */
/************************************************************
Opens a buffer to mlog. It must be closed with mlog_close. */ /*打开一个缓冲区mlog。它必须用mlog_close关闭。*/
UNIV_INLINE
byte*
mlog_open(
/*======*/
			/* out: buffer, NULL if log mode MTR_LOG_NONE */
	mtr_t*	mtr,	/* in: mtr */
	ulint	size);	/* in: buffer size in bytes */
/************************************************************
Closes a buffer opened to mlog. */ /*关闭对mlog打开的缓冲区。*/
UNIV_INLINE
void
mlog_close(
/*=======*/
	mtr_t*	mtr,	/* in: mtr */
	byte*	ptr);	/* in: buffer space from ptr up was not used */
/************************************************************
Writes the initial part of a log record. */ /*写入日志记录的初始部分。*/
UNIV_INLINE
byte*
mlog_write_initial_log_record_fast(
/*===============================*/
			/* out: new value of log_ptr */
	byte*	ptr,	/* in: pointer to (inside) a buffer frame holding the
			file page where modification is made */
	byte	type,	/* in: log item type: MLOG_1BYTE, ... */
	byte*	log_ptr,/* in: pointer to mtr log which has been opened */
	mtr_t*	mtr);	/* in: mtr */
/****************************************************************
Writes the contents of a mini-transaction log, if any, to the database log. */
/*将小事务日志(如果有的话)的内容写入数据库日志。*/
dulint
mlog_write(
/*=======*/
	dyn_array_t*	mlog,		/* in: mlog */
	ibool*		modifications);	/* out: TRUE if there were 
					log items to write */
/************************************************************
Parses an initial log record written by mlog_write_initial_log_record. */
/*解析由mlog_write_initial_log_record写入的初始日志记录。*/
byte*
mlog_parse_initial_log_record(
/*==========================*/
			/* out: parsed record end, NULL if not a complete
			record */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	byte*	type,	/* out: log record type: MLOG_1BYTE, ... */
	ulint*	space,	/* out: space id */
	ulint*	page_no);/* out: page number */
/************************************************************
Parses a log record written by mlog_write_ulint or mlog_write_dulint. */
/*解析mlog_write_ulint或mlog_write_dulint写的日志记录。*/
byte*
mlog_parse_nbytes(
/*==============*/
			/* out: parsed record end, NULL if not a complete
			record */
	ulint	type,	/* in: log record type: MLOG_1BYTE, ... */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	byte*	page);	/* in: page where to apply the log record, or NULL */
/************************************************************
Parses a log record written by mlog_write_string. */
/*解析mlog_write_string写入的日志记录。*/
byte*
mlog_parse_string(
/*==============*/
			/* out: parsed record end, NULL if not a complete
			record */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	byte*	page);	/* in: page where to apply the log record, or NULL */


/* Insert, update, and maybe other functions may use this value to define an
extra mlog buffer size for variable size data */
/*插入、更新和其他函数可能会使用这个值来为可变大小的数据定义一个额外的mlog缓冲区大小*/
#define MLOG_BUF_MARGIN	256

#ifndef UNIV_NONINL
#include "mtr0log.ic"
#endif

#endif
