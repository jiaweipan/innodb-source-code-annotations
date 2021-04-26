/************************************************************************
SQL data field and tuple

(c) 1994-1996 Innobase Oy

Created 5/30/1994 Heikki Tuuri
*************************************************************************/
/*SQL数据字段和元组*/
#ifndef data0data_h
#define data0data_h

#include "univ.i"

#include "data0types.h"
#include "data0type.h"
#include "mem0mem.h"
#include "dict0types.h"

typedef struct big_rec_struct		big_rec_t;

/* Some non-inlined functions used in the MySQL interface: */ /*MySQL接口中使用的一些非内联函数:*/
void 
dfield_set_data_noninline(
	dfield_t* 	field,	/* in: field */
	void*		data,	/* in: data */
	ulint		len);	/* in: length or UNIV_SQL_NULL */
void* 
dfield_get_data_noninline(
	dfield_t* field);	/* in: field */
ulint
dfield_get_len_noninline(
	dfield_t* field);	/* in: field */
ulint 
dtuple_get_n_fields_noninline(
	dtuple_t* 	tuple);	/* in: tuple */
dfield_t* 
dtuple_get_nth_field_noninline(
	dtuple_t* 	tuple,	/* in: tuple */
	ulint		n);	/* in: index of field */

/*************************************************************************
Gets pointer to the type struct of SQL data field. */ /*获取指向SQL数据字段的类型结构的指针。*/
UNIV_INLINE
dtype_t*
dfield_get_type(
/*============*/
				/* out: pointer to the type struct */
	dfield_t*	field);	/* in: SQL data field */
/*************************************************************************
Sets the type struct of SQL data field. */ /*设置指向SQL数据字段的类型结构的指针。*/
UNIV_INLINE
void
dfield_set_type(
/*============*/
	dfield_t*	field,	/* in: SQL data field */
	dtype_t*	type);	/* in: pointer to data type struct */
/*************************************************************************
Gets pointer to the data in a field. */ /*获取指向字段中的数据的指针。*/
UNIV_INLINE
void* 
dfield_get_data(
/*============*/
				/* out: pointer to data */
	dfield_t* field);	/* in: field */
/*************************************************************************
Gets length of field data. */  /*获取字段数据的长度。*/
UNIV_INLINE
ulint
dfield_get_len(
/*===========*/
				/* out: length of data; UNIV_SQL_NULL if 
				SQL null data */
	dfield_t* field);	/* in: field */
/*************************************************************************
Sets length in a field. */ /*设置字段的长度。*/
UNIV_INLINE
void 
dfield_set_len(
/*===========*/
	dfield_t* 	field,	/* in: field */
	ulint		len);	/* in: length or UNIV_SQL_NULL */
/*************************************************************************
Sets pointer to the data and length in a field. */ /*设置指向字段中的数据和长度的指针。*/
UNIV_INLINE
void 
dfield_set_data(
/*============*/
	dfield_t* 	field,	/* in: field */
	void*		data,	/* in: data */
	ulint		len);	/* in: length or UNIV_SQL_NULL */
/**************************************************************************
Writes an SQL null field full of zeros. */ /*写一个充满0的SQL空字段。*/
UNIV_INLINE
void
data_write_sql_null(
/*================*/
	byte*	data,	/* in: pointer to a buffer of size len */
	ulint	len);	/* in: SQL null size in bytes */
/*************************************************************************
Copies the data and len fields. */ /*复制数据和len字段。*/
UNIV_INLINE
void 
dfield_copy_data(
/*=============*/
	dfield_t* 	field1,	/* in: field to copy to */
	dfield_t*	field2);/* in: field to copy from */
/*************************************************************************
Copies a data field to another. */ /*将一个数据字段复制到另一个。*/
UNIV_INLINE
void
dfield_copy(
/*========*/
	dfield_t*	field1,	/* in: field to copy to */
	dfield_t*	field2);/* in: field to copy from */
/*************************************************************************
Tests if data length and content is equal for two dfields. */ /*测试两个字段的数据长度和内容是否相等。*/
UNIV_INLINE
ibool
dfield_datas_are_binary_equal(
/*==========================*/
				/* out: TRUE if equal */
	dfield_t*	field1,	/* in: field */
	dfield_t*	field2);/* in: field */
/*************************************************************************
Tests if dfield data length and content is equal to the given. */ /*测试dfield数据长度和内容是否等于给定的。*/
UNIV_INLINE
ibool
dfield_data_is_binary_equal(
/*========================*/
				/* out: TRUE if equal */
	dfield_t*	field,	/* in: field */
	ulint		len,	/* in: data length or UNIV_SQL_NULL */
	byte*		data);	/* in: data */
/*************************************************************************
Gets number of fields in a data tuple. */ /*获取数据元组中的字段数。*/
UNIV_INLINE
ulint 
dtuple_get_n_fields(
/*================*/
				/* out: number of fields */
	dtuple_t* 	tuple);	/* in: tuple */
/*************************************************************************
Gets nth field of a tuple. */ /*获取元组的第n个字段。*/
UNIV_INLINE
dfield_t* 
dtuple_get_nth_field(
/*=================*/
				/* out: nth field */
	dtuple_t* 	tuple,	/* in: tuple */
	ulint		n);	/* in: index of field */
/*************************************************************************
Gets info bits in a data tuple. */ /*获取数据元组中的信息位。*/
UNIV_INLINE
ulint
dtuple_get_info_bits(
/*=================*/
				/* out: info bits */
	dtuple_t* 	tuple);	/* in: tuple */
/*************************************************************************
Sets info bits in a data tuple. */ /*在数据元组中设置信息位。*/
UNIV_INLINE
void
dtuple_set_info_bits(
/*=================*/
	dtuple_t* 	tuple,		/* in: tuple */
	ulint		info_bits);	/* in: info bits */
/*************************************************************************
Gets number of fields used in record comparisons. */ /*获取记录比较中使用的字段数。*/
UNIV_INLINE
ulint
dtuple_get_n_fields_cmp(
/*====================*/
				/* out: number of fields used in comparisons
				in rem0cmp.* */
	dtuple_t*	tuple);	/* in: tuple */
/*************************************************************************
Gets number of fields used in record comparisons. */ /*获取记录比较中使用的字段数。*/
UNIV_INLINE
void
dtuple_set_n_fields_cmp(
/*====================*/
	dtuple_t*	tuple,		/* in: tuple */
	ulint		n_fields_cmp);	/* in: number of fields used in
					comparisons in rem0cmp.* */
/**************************************************************
Creates a data tuple to a memory heap. The default value for number
of fields used in record comparisons for this tuple is n_fields. */
/*在内存堆中创建一个数据元组。这个元组在记录比较中使用的字段数的默认值是n_fields。*/
UNIV_INLINE
dtuple_t*
dtuple_create(
/*==========*/
	 	 		/* out, own: created tuple */
	mem_heap_t*	heap,	/* in: memory heap where the tuple
				is created */
	ulint		n_fields); /* in: number of fields */	

/*************************************************************************
Creates a dtuple for use in MySQL. */
/*创建一个dtuple在MySQL中使用。*/
dtuple_t*
dtuple_create_for_mysql(
/*====================*/
			/* out, own created dtuple */
	void** heap,    /* out: created memory heap */
	ulint n_fields); /* in: number of fields */
/*************************************************************************
Frees a dtuple used in MySQL. */
/*释放MySQL中使用的dtuple。*/
void
dtuple_free_for_mysql(
/*==================*/
	void* heap);
/*************************************************************************
Sets number of fields used in a tuple. Normally this is set in
dtuple_create, but if you want later to set it smaller, you can use this. */ 
/*设置元组中使用的字段数。通常这是在dtuple_create中设置的，但是如果您想稍后将它设置得更小，可以使用它。*/
void
dtuple_set_n_fields(
/*================*/
	dtuple_t*	tuple,		/* in: tuple */
	ulint		n_fields);	/* in: number of fields */
/**************************************************************
The following function returns the sum of data lengths of a tuple. The space
occupied by the field structs or the tuple struct is not counted. */
/*下面的函数返回一个元组的数据长度之和。字段结构或元组结构所占用的空间不被计算。*/
UNIV_INLINE
ulint
dtuple_get_data_size(
/*=================*/
				/* out: sum of data lens */
	dtuple_t*	tuple);	/* in: typed data tuple */
/****************************************************************
Returns TRUE if lengths of two dtuples are equal and respective data fields
in them are equal when compared with collation in char fields (not as binary
strings). */
/*如果两个dtuple的长度相等，并且它们各自的数据字段在与char字段(而不是二进制字符串)的排序时相等，则返回TRUE。*/
ibool
dtuple_datas_are_ordering_equal(
/*============================*/
				/* out: TRUE if length and fieds are equal
				when compared with cmp_data_data:
				NOTE: in character type fields some letters
				are identified with others! (collation) */
	dtuple_t*	tuple1,	/* in: tuple 1 */
	dtuple_t*	tuple2);/* in: tuple 2 */
/****************************************************************
Folds a prefix given as the number of fields of a tuple. */
/*将指定为元组字段数的前缀折叠。*/
UNIV_INLINE
ulint
dtuple_fold(
/*========*/
				/* out: the folded value */
	dtuple_t*	tuple,	/* in: the tuple */
	ulint		n_fields,/* in: number of complete fields to fold */
	ulint		n_bytes,/* in: number of bytes to fold in an
				incomplete last field */
	dulint		tree_id);/* in: index tree id */
/***********************************************************************
Sets types of fields binary in a tuple. */
/*设置元组中二进制字段的类型。*/
UNIV_INLINE
void
dtuple_set_types_binary(
/*====================*/
	dtuple_t*	tuple,	/* in: data tuple */
	ulint		n);	/* in: number of fields to set */
/**************************************************************
Checks that a data field is typed. Asserts an error if not. */
/*检查数据字段是否已输入。如果不是，则断言错误。*/
ibool
dfield_check_typed(
/*===============*/
				/* out: TRUE if ok */
	dfield_t*	field);	/* in: data field */
/**************************************************************
Checks that a data tuple is typed. Asserts an error if not. */
/*检查数据元组是否有类型。如果不是，则断言错误。*/
ibool
dtuple_check_typed(
/*===============*/
				/* out: TRUE if ok */
	dtuple_t*	tuple);	/* in: tuple */
/**************************************************************
Validates the consistency of a tuple which must be complete, i.e,
all fields must have been set. */
/*验证元组的一致性，必须是完整的，即所有字段必须已设置。*/
ibool
dtuple_validate(
/*============*/
				/* out: TRUE if ok */
	dtuple_t*	tuple);	/* in: tuple */
/*****************************************************************
Pretty prints a dfield value according to its data type. */
/*Pretty根据数据类型打印一个dfield值。*/
void
dfield_print(
/*=========*/
	dfield_t*	dfield);/* in: dfield */
/*****************************************************************
Pretty prints a dfield value according to its data type. Also the hex string
is printed if a string contains non-printable characters. */ 
/*Pretty根据数据类型打印dfield值。如果字符串包含不可打印的字符，也会打印十六进制字符串。*/
void
dfield_print_also_hex(
/*==================*/
	dfield_t*	dfield);	 /* in: dfield */
/**************************************************************
The following function prints the contents of a tuple. */
/*下面的函数打印元组的内容。*/
void
dtuple_print(
/*=========*/
	dtuple_t*	tuple);	/* in: tuple */
/**************************************************************
The following function prints the contents of a tuple to a buffer. */
/*下面的函数将元组的内容打印到缓冲区。*/
ulint
dtuple_sprintf(
/*===========*/
				/* out: printed length in bytes */
	char*		buf,	/* in: print buffer */
	ulint		buf_len,/* in: buf length in bytes */
	dtuple_t*	tuple);	/* in: tuple */
/******************************************************************
Moves parts of long fields in entry to the big record vector so that
the size of tuple drops below the maximum record size allowed in the
database. Moves data only from those fields which are not necessary
to determine uniquely the insertion place of the tuple in the index. */
/*将条目中的部分长字段移动到大记录向量，以便元组的大小降到数据库中允许的最大记录大小以下。
仅从那些不需要唯一地确定元组在索引中的插入位置的字段中移动数据。*/
big_rec_t*
dtuple_convert_big_rec(
/*===================*/
				/* out, own: created big record vector,
				NULL if we are not able to shorten
				the entry enough, i.e., if there are
				too many short fields in entry */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: index entry */
	ulint*		ext_vec,/* in: array of externally stored fields,
				or NULL: if a field already is externally
				stored, then we cannot move it to the vector
				this function returns */
	ulint		n_ext_vec);/* in: number of elements is ext_vec */
/******************************************************************
Puts back to entry the data stored in vector. Note that to ensure the
fields in entry can accommodate the data, vector must have been created
from entry with dtuple_convert_big_rec. */
/*将存储在vector中的数据放回条目。请注意，为了确保entry中的字段可以容纳数据，
必须使用dtuple_convert_big)rec从entry创建vector。*/
void
dtuple_convert_back_big_rec(
/*========================*/
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: entry whose data was put to vector */
	big_rec_t*	vector);/* in, own: big rec vector; it is
				freed in this function */
/******************************************************************
Frees the memory in a big rec vector. */
/*释放大rec向量中的内存。*/
void
dtuple_big_rec_free(
/*================*/
	big_rec_t*	vector);	/* in, own: big rec vector; it is
				freed in this function */
/***************************************************************
Generates a random tuple. */
/*生成一个随机元组。 */
dtuple_t*
dtuple_gen_rnd_tuple(
/*=================*/
				/* out: pointer to the tuple */
	mem_heap_t*	heap);	/* in: memory heap where generated */
/*******************************************************************
Generates a test tuple for sort and comparison tests. */
/*为排序和比较测试生成测试元组。*/
void
dtuple_gen_test_tuple(
/*==================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 3 fields */
	ulint		i);	/* in: a number, 0 <= i < 512 */
/*******************************************************************
Generates a test tuple for B-tree speed tests. */
/*为b树速度测试生成测试元组。*/
void
dtuple_gen_test_tuple3(
/*===================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 3 fields */
	ulint		i,	/* in: a number < 1000000 */
	ulint		type,	/* in: DTUPLE_TEST_FIXED30, ... */
	byte*		buf);	/* in: a buffer of size >= 8 bytes */
/*******************************************************************
Generates a test tuple for B-tree speed tests. */
/*为b树速度测试生成测试元组。*/
void
dtuple_gen_search_tuple3(
/*=====================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 1 or 2 fields */
	ulint		i,	/* in: a number < 1000000 */
	byte*		buf);	/* in: a buffer of size >= 8 bytes */
/*******************************************************************
Generates a test tuple for TPC-A speed test. */
/*为TPC-A速度测试生成测试元组。*/
void
dtuple_gen_test_tuple_TPC_A(
/*========================*/
	dtuple_t*	tuple,	/* in/out: a tuple with >= 3 fields */
	ulint		i,	/* in: a number < 10000 */
	byte*		buf);	/* in: a buffer of size >= 16 bytes */
/*******************************************************************
Generates a test tuple for B-tree speed tests. */
/*为b树速度测试生成测试元组。*/
void
dtuple_gen_search_tuple_TPC_A(
/*==========================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 1 field */
	ulint		i,	/* in: a number < 10000 */
	byte*		buf);	/* in: a buffer of size >= 16 bytes */
/*******************************************************************
Generates a test tuple for TPC-C speed test. */
/*为TPC-C速度测试生成测试元组。*/
void
dtuple_gen_test_tuple_TPC_C(
/*========================*/
	dtuple_t*	tuple,	/* in/out: a tuple with >= 12 fields */
	ulint		i,	/* in: a number < 100000 */
	byte*		buf);	/* in: a buffer of size >= 16 bytes */
/*******************************************************************
Generates a test tuple for B-tree speed tests. */
/*为b树速度测试生成测试元组。*/
void
dtuple_gen_search_tuple_TPC_C(
/*==========================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 1 field */
	ulint		i,	/* in: a number < 100000 */
	byte*		buf);	/* in: a buffer of size >= 16 bytes */

/* Types of the third field in dtuple_gen_test_tuple3 */	/*dtuple_gen_test_tuple3中第三个字段的类型*/
#define DTUPLE_TEST_FIXED30	1
#define DTUPLE_TEST_RND30	2
#define DTUPLE_TEST_RND3500	3
#define DTUPLE_TEST_FIXED2000	4
#define DTUPLE_TEST_FIXED3	5

/*######################################################################*/

/* Structure for an SQL data field */ /*SQL数据字段的结构*/
struct dfield_struct{
	void*		data;	/* pointer to data */
	ulint		len;	/* data length; UNIV_SQL_NULL if SQL null; */
	dtype_t		type;	/* type of data */
	ulint		col_no;	/* when building index entries, the column
				number can be stored here */
};

struct dtuple_struct {
	ulint		info_bits;	/* info bits of an index record:
					the default is 0; this field is used
					if an index record is built from
					a data tuple */
	ulint		n_fields;	/* number of fields in dtuple */
	ulint		n_fields_cmp;	/* number of fields which should
					be used in comparison services
					of rem0cmp.*; the index search
					is performed by comparing only these
					fields, others are ignored; the
					default value in dtuple creation is
					the same value as n_fields */ /*在rem0cmp.*的比较服务中应使用的字段数;
					索引搜索仅通过比较这些字段来执行，其他字段被忽略;dtuple创建的默认值与n_fields相同*/
	dfield_t*	fields;		/* fields */
	UT_LIST_NODE_T(dtuple_t) tuple_list;
					/* data tuples can be linked into a
					list using this field */ /*可以使用此字段将数据元组链接到列表中*/
	ulint		magic_n;	
};
#define	DATA_TUPLE_MAGIC_N	65478679

/* A slot for a field in a big rec vector */
/*在一个大的矩形矢量中，用于一个字段的槽*/ 
typedef struct big_rec_field_struct 	big_rec_field_t;
struct big_rec_field_struct {
	ulint		field_no;	/* field number in record */
	ulint		len;		/* stored data len */
	byte*		data;		/* stored data */
};

/* Storage format for overflow data in a big record, that is, a record
which needs external storage of data fields */
/*大记录中溢出数据的存储格式，即需要外部存储数据字段的记录*/
struct big_rec_struct {
	mem_heap_t*	heap;		/* memory heap from which allocated */
	ulint		n_fields;	/* number of stored fields */
	big_rec_field_t* fields;	/* stored fields */
};
	
#ifndef UNIV_NONINL
#include "data0data.ic"
#endif

#endif
