/************************************************************************
Record manager

(c) 1994-1996 Innobase Oy

Created 5/30/1994 Heikki Tuuri
*************************************************************************/

#ifndef rem0rec_h
#define rem0rec_h

#include "univ.i"
#include "data0data.h"
#include "rem0types.h"
#include "mtr0types.h"

/* Maximum values for various fields (for non-blob tuples) */ 
/*不同字段的最大值(对于非blob元组)*/
#define REC_MAX_N_FIELDS	(1024 - 1)
#define REC_MAX_HEAP_NO		(2 * 8192 - 1)
#define REC_MAX_N_OWNED		(16 - 1)

/* Flag denoting the predefined minimum record: this bit is ORed in the 4
info bits of a record */
/*标记预定义的最小记录:该位或一个记录的4个信息位*/
#define REC_INFO_MIN_REC_FLAG	0x10

/* Number of extra bytes in a record, in addition to the data and the
offsets */
/*记录中除了数据和偏移量之外的额外字节数*/
#define REC_N_EXTRA_BYTES	6

/**********************************************************
The following function is used to get the offset of the
next chained record on the same page. */
/*下面的函数用于获取同一页上下一个链接记录的偏移量。*/
UNIV_INLINE
ulint 
rec_get_next_offs(
/*==============*/
			/* out: the page offset of the next 
			chained record */
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function is used to set the next record offset field
of the record. */
/*下面的函数用于设置记录的下一个记录偏移字段。*/
UNIV_INLINE
void
rec_set_next_offs(
/*==============*/
	rec_t*	rec,	/* in: physical record */
	ulint	next);	/* in: offset of the next record */
/**********************************************************
The following function is used to get the number of fields
in the record. */
/*下面的函数用于获取记录中的字段数。*/
UNIV_INLINE
ulint
rec_get_n_fields(
/*=============*/
			/* out: number of data fields */
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function is used to get the number of records
owned by the previous directory record. */
/*以下函数用于获取前一个目录记录拥有的记录数。*/
UNIV_INLINE
ulint
rec_get_n_owned(
/*============*/
			/* out: number of owned records */
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function is used to set the number of owned
records. */ /*设置拥有的记录数。*/
UNIV_INLINE
void
rec_set_n_owned(
/*============*/
	rec_t*	rec,		/* in: physical record */
	ulint	n_owned);	/* in: the number of owned */
/**********************************************************
The following function is used to retrieve the info bits of
a record. */ /*下面的函数用于检索记录的信息位。*/
UNIV_INLINE
ulint
rec_get_info_bits(
/*==============*/
			/* out: info bits */
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function is used to set the info bits of a record. */
/*下面的功能用于设置记录的信息位。*/
UNIV_INLINE
void
rec_set_info_bits(
/*==============*/
	rec_t*	rec,	/* in: physical record */
	ulint	bits);	/* in: info bits */
/**********************************************************
Gets the value of the deleted falg in info bits. */
/*获取信息位中已删除的falg的值。*/
UNIV_INLINE
ibool
rec_info_bits_get_deleted_flag(
/*===========================*/
				/* out: TRUE if deleted flag set */
	ulint	info_bits);	/* in: info bits from a record */
/**********************************************************
The following function tells if record is delete marked. */
/*下面的函数告诉记录是否被删除标记。*/
UNIV_INLINE
ibool
rec_get_deleted_flag(
/*=================*/
			/* out: TRUE if delete marked */
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function is used to set the deleted bit. */
/*以下函数用于设置删除位。*/
UNIV_INLINE
void
rec_set_deleted_flag(
/*=================*/
	rec_t*	rec,	/* in: physical record */
	ibool	flag);	/* in: TRUE if delete marked */
/**********************************************************
The following function is used to get the order number
of the record in the heap of the index page. */
/*下面的函数用于获取索引页堆中记录的订单号。*/
UNIV_INLINE
ulint
rec_get_heap_no(
/*=============*/
			/* out: heap order number */
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function is used to set the heap number
field in the record. */
/*下面的函数用于设置记录中的堆数字段。*/
UNIV_INLINE
void
rec_set_heap_no(
/*=============*/
	rec_t*	rec,	/* in: physical record */
	ulint	heap_no);/* in: the heap number */
/**********************************************************
The following function is used to test whether the data offsets
in the record are stored in one-byte or two-byte format. */
/*下面的函数用于测试记录中的数据偏移是以单字节还是双字节格式存储的。*/
UNIV_INLINE
ibool
rec_get_1byte_offs_flag(
/*====================*/
			/* out: TRUE if 1-byte form */
	rec_t*	rec);	/* in: physical record */
/****************************************************************
The following function is used to get a pointer to the nth
data field in the record. */
/*下面的函数用于获取指向记录中第n个数据字段的指针。*/
byte*
rec_get_nth_field(
/*==============*/
 			/* out: pointer to the field, NULL if SQL null */
 	rec_t*	rec, 	/* in: record */
 	ulint	n,	/* in: index of the field */
	ulint*	len);	/* out: length of the field; UNIV_SQL_NULL 
			if SQL null */
/****************************************************************
Gets the physical size of a field. Also an SQL null may have a field of
size > 0, if the data type is of a fixed size. */
/*获取字段的物理大小。如果数据类型的大小是固定的，那么SQL null可能有一个大小为> 0的字段。*/
UNIV_INLINE
ulint
rec_get_nth_field_size(
/*===================*/
			/* out: field size in bytes */
 	rec_t*	rec, 	/* in: record */
 	ulint	n);	/* in: index of the field */
/***************************************************************
Gets the value of the ith field extern storage bit. If it is TRUE
it means that the field is stored on another page. */
/*获取第i个字段扩展存储位的值。如果为TRUE，则表示该字段存储在另一个页面上。*/
UNIV_INLINE
ibool
rec_get_nth_field_extern_bit(
/*=========================*/
			/* in: TRUE or FALSE */
	rec_t*	rec,	/* in: record */
	ulint	i);	/* in: ith field */
/**********************************************************
Returns TRUE if the extern bit is set in any of the fields
of rec. */ /*如果rec的任何字段中设置了外部位，则返回TRUE。*/
UNIV_INLINE
ibool
rec_contains_externally_stored_field(
/*=================================*/
			/* out: TRUE if a field is stored externally */
	rec_t*	rec);	/* in: record */
/***************************************************************
Sets the value of the ith field extern storage bit. */
/*设置第i个字段扩展存储位的值。*/
void
rec_set_nth_field_extern_bit(
/*=========================*/
	rec_t*	rec,	/* in: record */
	ulint	i,	/* in: ith field */
	ibool	val,	/* in: value to set */
	mtr_t*	mtr);	/* in: mtr holding an X-latch to the page where
			rec is, or NULL; in the NULL case we do not
			write to log about the change */
/***************************************************************
Sets TRUE the extern storage bits of fields mentioned in an array. */
/*将数组中提到的字段的外部存储位设置为TRUE。*/
void
rec_set_field_extern_bits(
/*======================*/
	rec_t*	rec,		/* in: record */
	ulint*	vec,		/* in: array of field numbers */
	ulint	n_fields,	/* in: number of fields numbers */
	mtr_t*	mtr);		/* in: mtr holding an X-latch to the page
				where rec is, or NULL; in the NULL case we
				do not write to log about the change */
/****************************************************************
The following function is used to get a copy of the nth
data field in the record to a buffer. */
/*下面的函数用于将记录中的第n个数据字段复制到缓冲区。*/
UNIV_INLINE
void
rec_copy_nth_field(
/*===============*/
 	void*	buf,	/* in: pointer to the buffer */
 	rec_t*	rec, 	/* in: record */
 	ulint	n,	/* in: index of the field */
	ulint*	len);	/* out: length of the field; UNIV_SQL_NULL if SQL 
			null */
/*************************************************************** 
This is used to modify the value of an already existing field in 
a physical record. The previous value must have exactly the same 
size as the new value. If len is UNIV_SQL_NULL then the field is 
treated as SQL null. */
/*这用于修改物理记录中已经存在的字段的值。前一个值的大小必须与新值完全相同。如果len是UNIV_SQL_NULL，则该字段被视为SQL null。*/
UNIV_INLINE
void
rec_set_nth_field(
/*==============*/
	rec_t*	rec, 	/* in: record */
	ulint	n,	/* in: index of the field */
	void*	data,	/* in: pointer to the data if not SQL null */
	ulint	len);	/* in: length of the data or UNIV_SQL_NULL. 
			If not SQL null, must have the same length as the
			previous value. If SQL null, previous value must be
			SQL null. */
/************************************************************** 
The following function returns the data size of a physical
record, that is the sum of field lengths. SQL null fields
are counted as length 0 fields. The value returned by the function
is the distance from record origin to record end in bytes. */
/*下面的函数返回物理记录的数据大小，即字段长度之和。SQL空字段被计算为长度为0的字段。函数返回的值是从记录起始点到记录结束点的距离，以字节为单位。*/
UNIV_INLINE
ulint
rec_get_data_size(
/*==============*/
			/* out: size */
	rec_t*	rec);	/* in: physical record */
/************************************************************** 
Returns the total size of record minus data size of record.
The value returned by the function is the distance from record 
start to record origin in bytes. */
/*返回记录的总大小减去记录的数据大小。函数返回的值是从记录开始到记录起源的距离，以字节为单位。*/
UNIV_INLINE
ulint
rec_get_extra_size(
/*===============*/
			/* out: size */
	rec_t*	rec);	/* in: physical record */
/************************************************************** 
Returns the total size of a physical record.  */
/*返回物理记录的总大小。*/
UNIV_INLINE
ulint
rec_get_size(
/*=========*/
			/* out: size */
	rec_t*	rec);	/* in: physical record */
/**************************************************************
Returns a pointer to the start of the record. */
/*返回一个指向记录开头的指针。*/
UNIV_INLINE
byte*
rec_get_start(
/*==========*/
			/* out: pointer to start */
	rec_t*	rec);	/* in: pointer to record */
/**************************************************************
Returns a pointer to the end of the record. */
/*返回指向记录末尾的指针。*/
UNIV_INLINE
byte*
rec_get_end(
/*========*/
			/* out: pointer to end */
	rec_t*	rec);	/* in: pointer to record */
/*******************************************************************
Copies a physical record to a buffer. */
/*将物理记录复制到缓冲区。*/
UNIV_INLINE
rec_t*
rec_copy(
/*=====*/
			/* out: pointer to the origin of the copied record */
	void*	buf,	/* in: buffer */
	rec_t*	rec);	/* in: physical record */
/******************************************************************
Copies the first n fields of a physical record to a new physical record in
a buffer. */
/*将一个物理记录的前n个字段复制到缓冲区中的一个新的物理记录。*/
rec_t*
rec_copy_prefix_to_buf(
/*===================*/
				/* out, own: copied record */
	rec_t*	rec,		/* in: physical record */
	ulint	n_fields,	/* in: number of fields to copy */
	byte**	buf,		/* in/out: memory buffer for the copied prefix,
				or NULL */
	ulint*	buf_size);	/* in/out: buffer size */
/****************************************************************
Folds a prefix of a physical record to a ulint. */
/*将物理记录的前缀折叠到ulint。*/
UNIV_INLINE
ulint
rec_fold(
/*=====*/
				/* out: the folded value */
	rec_t*	rec,		/* in: the physical record */
	ulint	n_fields,	/* in: number of complete fields to fold */
	ulint	n_bytes,	/* in: number of bytes to fold in an
				incomplete last field */
	dulint	tree_id);	/* in: index tree id */
/*************************************************************
Builds a physical record out of a data tuple and stores it beginning from
address destination. */
/*从数据元组构建一个物理记录，并从地址目的地开始存储它。*/
UNIV_INLINE
rec_t* 	
rec_convert_dtuple_to_rec(
/*======================*/			
				/* out: pointer to the origin of physical
				record */
	byte*	destination,	/* in: start address of the physical record */
	dtuple_t* dtuple);	/* in: data tuple */
/*************************************************************
Builds a physical record out of a data tuple and stores it beginning from
address destination. */
/*从数据元组构建一个物理记录，并从地址目的地开始存储它。*/
rec_t* 	
rec_convert_dtuple_to_rec_low(
/*==========================*/			
				/* out: pointer to the origin of physical
				record */
	byte*	destination,	/* in: start address of the physical record */
	dtuple_t* dtuple,	/* in: data tuple */
	ulint	data_size);	/* in: data size of dtuple */
/**************************************************************
Returns the extra size of a physical record if we know its
data size and number of fields. */
/*如果知道物理记录的数据大小和字段数，则返回该记录的额外大小。*/
UNIV_INLINE
ulint
rec_get_converted_extra_size(
/*=========================*/
				/* out: extra size */
	ulint	data_size,	/* in: data size */
	ulint	n_fields);	/* in: number of fields */
/**************************************************************
The following function returns the size of a data tuple when converted to
a physical record. */
/*下面的函数返回数据元组转换为物理记录时的大小。*/
UNIV_INLINE
ulint
rec_get_converted_size(
/*===================*/
				/* out: size */
	dtuple_t*	dtuple);/* in: data tuple */
/******************************************************************
Copies the first n fields of a physical record to a data tuple.
The fields are copied to the memory heap. */
/*将物理记录的前n个字段复制到一个数据元组中。字段被复制到内存堆中。*/
void
rec_copy_prefix_to_dtuple(
/*======================*/
	dtuple_t*	tuple,		/* in: data tuple */
	rec_t*		rec,		/* in: physical record */
	ulint		n_fields,	/* in: number of fields to copy */
	mem_heap_t*	heap);		/* in: memory heap */
/*******************************************************************
Validates the consistency of a physical record. */
/*验证物理记录的一致性。*/
ibool
rec_validate(
/*=========*/
			/* out: TRUE if ok */
	rec_t*	rec);	/* in: physical record */
/*******************************************************************
Prints a physical record. */
/*打印物理记录。*/
void
rec_print(
/*======*/
	rec_t*	rec);	/* in: physical record */
/*******************************************************************
Prints a physical record to a buffer. */
/*将物理记录打印到缓冲区。*/
ulint
rec_sprintf(
/*========*/
			/* out: printed length in bytes */
	char*	buf,	/* in: buffer to print to */
	ulint	buf_len,/* in: buffer length */
	rec_t*	rec);	/* in: physical record */

#define REC_INFO_BITS		6	/* This is single byte bit-field */

/* Maximum lengths for the data in a physical record if the offsets
are given in one byte (resp. two byte) format. */
/*如果偏移量以一个字节为单位给出，则物理记录中数据的最大长度。两个字节)格式*/
#define REC_1BYTE_OFFS_LIMIT	0x7F
#define REC_2BYTE_OFFS_LIMIT	0x7FFF

/* The data size of record must be smaller than this because we reserve
two upmost bits in a two byte offset for special purposes */
/*记录的数据大小必须小于这个值，因为我们在两个字节的偏移量中保留了两个最上面的位，用于特殊目的*/
#define REC_MAX_DATA_SIZE	(16 * 1024)

#ifndef UNIV_NONINL
#include "rem0rec.ic"
#endif

#endif
