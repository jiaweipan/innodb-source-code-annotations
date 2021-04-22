/******************************************************
Data types

(c) 1996 Innobase Oy

Created 1/16/1996 Heikki Tuuri
*******************************************************/
/*数据类型*/
#ifndef data0type_h
#define data0type_h

#include "univ.i"

/* SQL data type struct */
typedef struct dtype_struct		dtype_t;

/* This variable is initialized as the standard binary variable length
data type */ /*这个变量被初始化为标准的二进制可变长度数据类型*/
extern dtype_t* 	dtype_binary;

/* Data main types of SQL data; NOTE! character data types requiring
collation transformation must have the smallest codes! All codes must be
less than 256! */ /*数据主要类型SQL数据;注意!需要排序转换的字符数据类型必须具有最小的代码!所有代码必须小于256!*/
#define	DATA_VARCHAR	1	/* character varying */ /*可变长字符串*/
#define DATA_CHAR	2	/* fixed length character */ /*定长字符*/
#define DATA_FIXBINARY	3	/* binary string of fixed length */ /*固定长度的二进制字符串*/
#define DATA_BINARY	4	/* binary string */ /*二进制字符串*/
#define DATA_BLOB	5	/* binary large object */ /*二进制大对象*/
#define	DATA_INT	6	/* integer: can be any size 1 - 8 bytes */ /*整数：可以是任何大小的1-8字节*/
#define	DATA_SYS_CHILD	7	/* address of the child page in node pointer */ /*节点指针中子页的地址*/
#define	DATA_SYS	8	/* system column */ /*系统列*/
/* Data types >= DATA_FLOAT must be compared using the whole field, not as
binary strings */ /*数据类型>= DATA_FLOAT必须使用整个字段进行比较，而不是作为二进制字符串进行比较*/
#define DATA_FLOAT	9
#define DATA_DOUBLE	10
#define DATA_DECIMAL	11	/* decimal number stored as an ASCII string */ /*作为ASCII字符串存储的十进制数*/
#define	DATA_VARMYSQL	12	/* data types for which comparisons must be */ /*必须进行比较的数据类型*/
#define	DATA_MYSQL	13	/* made by MySQL */ /*由MySQL*/
#define DATA_ERROR	111	/* error value */ /*错误值*/
#define DATA_MTYPE_MAX	255
/*-------------------------------------------*/
/* Precise data types for system columns; NOTE: the values must run
from 0 up in the order given! All codes must be less than 256! */ /*系统列的精确数据类型;注意:这些值必须按照给定的顺序从0开始运行!所有代码必须小于256!*/
#define	DATA_ROW_ID	0	/* row id: a dulint */
#define DATA_ROW_ID_LEN	6	/* stored length for row id */
#define DATA_TRX_ID	1	/* transaction id: 6 bytes */
#define DATA_TRX_ID_LEN	6
#define	DATA_ROLL_PTR	2	/* rollback data pointer: 7 bytes */
#define DATA_ROLL_PTR_LEN 7
#define DATA_MIX_ID	3	/* mixed index label: a dulint, stored in
				a row in a compressed form */ /*混合索引标签:一种dulint，以压缩形式存储在一行中*/
#define DATA_MIX_ID_LEN	9	/* maximum stored length for mix id (in a
				compressed dulint form) */ /*mix id的最大存储长度(压缩dulint形式)*/
#define	DATA_N_SYS_COLS 4 	/* number of system columns defined above */ /*上面定义的系统列数*/
#define DATA_NOT_NULL	256	/* this is ORed to the precise type when
				the column is declared as NOT NULL */ /*当列声明为NOT NULL时，这是or到精确类型*/
#define DATA_UNSIGNED	512	/* this id ORed to the precise type when
				we have an unsigned integer type */ /*当我们有一个无符号整数类型时，这个id或精确类型*/
/*-------------------------------------------*/

/* Precise types of a char or varchar data. All codes must be less than 256! */ /*char或varchar数据的精确类型。所有代码必须小于256!*/
#define DATA_ENGLISH	4	/* English language character string */ /*英文字符串*/
#define	DATA_FINNISH	5	/* Finnish */
#define DATA_PRTYPE_MAX	255

/* This many bytes we need to store the type information affecting the
alphabetical order for a single field and decide the storage size of an
SQL null*/ /*我们需要这么多字节来存储影响单个字段字母顺序的类型信息，并决定SQL null的存储大小*/
#define DATA_ORDER_NULL_TYPE_BUF_SIZE	4

/*************************************************************************
Sets a data type structure. */ /*设置数据类型结构。*/
UNIV_INLINE
void
dtype_set(
/*======*/
	dtype_t*	type,	/* in: type struct to init */
	ulint		mtype,	/* in: main data type */
	ulint		prtype,	/* in: precise type */
	ulint		len,	/* in: length of type */
	ulint		prec);	/* in: precision of type */
/*************************************************************************
Copies a data type structure. */ /*复制数据类型结构。*/
UNIV_INLINE
void
dtype_copy(
/*=======*/
	dtype_t*	type1,	/* in: type struct to copy to */
	dtype_t*	type2);	/* in: type struct to copy from */
/*************************************************************************
Gets the SQL main data type. */ /*获取SQL主数据类型。*/
UNIV_INLINE
ulint
dtype_get_mtype(
/*============*/
	dtype_t*	type);
/*************************************************************************
Gets the precise data type. */ /*获取精确的数据类型。*/
UNIV_INLINE
ulint
dtype_get_prtype(
/*=============*/
	dtype_t*	type);
/*************************************************************************
Gets the type length. */ /*获取类型长度。*/
UNIV_INLINE
ulint
dtype_get_len(
/*==========*/
	dtype_t*	type);
/*************************************************************************
Gets the type precision. */ /*获取类型精度。*/
UNIV_INLINE
ulint
dtype_get_prec(
/*===========*/
	dtype_t*	type);
/*************************************************************************
Gets the padding character code for the type. */ /*获取类型的填充字符代码。*/
UNIV_INLINE
ulint
dtype_get_pad_char(
/*===============*/
				/* out: padding character code, or
				ULINT_UNDEFINED if no padding specified */
	dtype_t*	type);	/* in: typeumn */
/***************************************************************************
Returns the size of a fixed size data type, 0 if not a fixed size type. */ 
/*返回固定大小数据类型的大小，如果不是固定大小类型，则返回0。*/
UNIV_INLINE
ulint
dtype_get_fixed_size(
/*=================*/
				/* out: fixed size, or 0 */
	dtype_t*	type);	/* in: type */
/***************************************************************************
Returns a stored SQL NULL size for a type. For fixed length types it is
the fixed length of the type, otherwise 0. */
/*为类型返回存储的SQL NULL大小。对于固定长度类型，它是类型的固定长度，否则为0。 */
UNIV_INLINE
ulint
dtype_get_sql_null_size(
/*====================*/
				/* out: SQL null storage size */
	dtype_t*	type);	/* in: type */
/***************************************************************************
Returns TRUE if a type is of a fixed size. */ /*如果类型的大小固定，则返回TRUE。*/
UNIV_INLINE
ibool
dtype_is_fixed_size(
/*================*/
				/* out: TRUE if fixed size */
	dtype_t*	type);	/* in: type */
/**************************************************************************
Stores to a type the information which determines its alphabetical
ordering. */ /*将决定其字母顺序的信息存储到类型中。*/
UNIV_INLINE
void
dtype_store_for_order_and_null_size(
/*================================*/
	byte*		buf,	/* in: buffer for DATA_ORDER_NULL_TYPE_BUF_SIZE
				bytes */
	dtype_t*	type);	/* in: type struct */
/**************************************************************************
Reads of a type the stored information which determines its alphabetical
ordering. */ /*读取一种类型的存储信息，这些信息决定了它的字母顺序。*/
UNIV_INLINE
void
dtype_read_for_order_and_null_size(
/*===============================*/
	dtype_t*	type,	/* in: type struct */
	byte*		buf);	/* in: buffer for type order info */
/*************************************************************************
Validates a data type structure. */
/*验证数据类型结构。*/
ibool
dtype_validate(
/*===========*/
				/* out: TRUE if ok */
	dtype_t*	type);	/* in: type struct to validate */
/*************************************************************************
Prints a data type structure. */

void
dtype_print(
/*========*/
	dtype_t*	type);	/* in: type */

/* Structure for an SQL data type */
/*SQL数据类型的结构*/
struct dtype_struct{
	ulint	mtype;		/* main data type */
	ulint	prtype;		/* precise type; MySQL data type */

	/* remaining two fields do not affect alphabetical ordering: */
     /*其余两个字段不影响字母顺序：*/
	ulint	len;		/* length */
	ulint	prec;		/* precision */
};

#ifndef UNIV_NONINL
#include "data0type.ic"
#endif

#endif
