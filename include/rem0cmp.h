/***********************************************************************
Comparison services for records

(c) 1994-2001 Innobase Oy

Created 7/1/1994 Heikki Tuuri
************************************************************************/
/*纪录比对服务*/
#ifndef rem0cmp_h
#define rem0cmp_h

#include "univ.i"
#include "data0data.h"
#include "data0type.h"
#include "dict0dict.h"
#include "rem0rec.h"

/*****************************************************************
Returns TRUE if two types are equal for comparison purposes. */
/*为便于比较，如果两种类型相等，则返回TRUE。*/
ibool
cmp_types_are_equal(
/*================*/
				/* out: TRUE if the types are considered
				equal in comparisons */
	dtype_t*	type1,	/* in: type 1 */
	dtype_t*	type2);	/* in: type 2 */
/*****************************************************************
This function is used to compare two data fields for which we know the
data type. */ /*此函数用于比较已知数据类型的两个数据字段。*/
UNIV_INLINE
int
cmp_data_data(
/*==========*/	
				/* out: 1, 0, -1, if data1 is greater, equal, 
				less than data2, respectively */
	dtype_t*	cur_type,/* in: data type of the fields */
	byte*		data1,	/* in: data field (== a pointer to a memory
				buffer) */
	ulint		len1,	/* in: data field length or UNIV_SQL_NULL */
	byte*		data2,	/* in: data field (== a pointer to a memory
				buffer) */
	ulint		len2);	/* in: data field length or UNIV_SQL_NULL */
/*****************************************************************
This function is used to compare two dfields where at least the first
has its data type field set. */
/*此函数用于比较至少第一个字段具有数据类型字段集的两个字段。*/
UNIV_INLINE
int
cmp_dfield_dfield(
/*==============*/	
				/* out: 1, 0, -1, if dfield1 is greater, equal, 
				less than dfield2, respectively */
	dfield_t*	dfield1,/* in: data field; must have type field set */
	dfield_t*	dfield2);/* in: data field */
/*****************************************************************
This function is used to compare a data tuple to a physical record.
Only dtuple->n_fields_cmp first fields are taken into account for
the the data tuple! If we denote by n = n_fields_cmp, then rec must
have either m >= n fields, or it must differ from dtuple in some of
the m fields rec has. If rec has an externally stored field we do not
compare it but return with value 0 if such a comparison should be
made. */
/*这个函数用于比较数据元组和物理记录。只有dtuple->n_fields_cmp第一个字段被考虑为数据元组!如果我们用n = n_fields_cmp表示，
那么rec必须有m个>= n个字段，或者它必须与rec的m个字段中的某些dtuple不同。
如果rec有一个外部存储的字段，我们不会比较它，但如果需要进行比较，则返回值为0。*/
UNIV_INLINE
int
cmp_dtuple_rec_with_match(
/*======================*/	
				/* out: 1, 0, -1, if dtuple is greater, equal, 
				less than rec, respectively, when only the 
				common first fields are compared, or
				until the first externally stored field in
				rec */
	dtuple_t*	dtuple,	/* in: data tuple */
	rec_t*		rec,	/* in: physical record which differs from
				dtuple in some of the common fields, or which
				has an equal number or more fields than
				dtuple */
	ulint*	 	matched_fields, /* in/out: number of already completely 
				matched fields; when function returns,
				contains the value for current comparison */
	ulint*	  	matched_bytes); /* in/out: number of already matched 
				bytes within the first field not completely
				matched; when function returns, contains the
				value for current comparison */
/******************************************************************
Compares a data tuple to a physical record. */
/*将数据元组与物理记录进行比较。*/
int
cmp_dtuple_rec(
/*===========*/
				/* out: 1, 0, -1, if dtuple is greater, equal, 
				less than rec, respectively; see the comments
				for cmp_dtuple_rec_with_match */
	dtuple_t* 	dtuple,	/* in: data tuple */
	rec_t*	  	rec);	/* in: physical record */
/******************************************************************
Checks if a dtuple is a prefix of a record. The last field in dtuple
is allowed to be a prefix of the corresponding field in the record. */
/*检查一个dtuple是否是一个记录的前缀。dtuple中的最后一个字段可以作为记录中相应字段的前缀。*/
ibool
cmp_dtuple_is_prefix_of_rec(
/*========================*/
				/* out: TRUE if prefix */
	dtuple_t* 	dtuple,	/* in: data tuple */
	rec_t*	  	rec);	/* in: physical record */
/******************************************************************
Compares a prefix of a data tuple to a prefix of a physical record for
equality. If there are less fields in rec than parameter n_fields, FALSE
is returned. NOTE that n_fields_cmp of dtuple does not affect this
comparison. */
/*比较数据元组的前缀与物理记录的前缀是否相等。如果rec中的字段比参数n_fields少，则返回FALSE。注意dtuple的n_fields_cmp不影响这个比较。*/
ibool
cmp_dtuple_rec_prefix_equal(
/*========================*/
				/* out: TRUE if equal */
	dtuple_t*	dtuple,	/* in: data tuple */
	rec_t*		rec,	/* in: physical record */
	ulint		n_fields); /* in: number of fields which should be 
				compared; must not exceed the number of 
				fields in dtuple */
/*****************************************************************
This function is used to compare two physical records. Only the common
first fields are compared, and if an externally stored field is
encountered, then 0 is returned. */
/*比较两条物理记录。只比较常见的第一个字段，如果遇到外部存储的字段，则返回0。*/
int
cmp_rec_rec_with_match(
/*===================*/	
				/* out: 1, 0 , -1 if rec1 is greater, equal,
				less, respectively, than rec2; only the common
				first fields are compared */
	rec_t*		rec1,	/* in: physical record */
	rec_t*		rec2,	/* in: physical record */
	dict_index_t*	index,	/* in: data dictionary index */
	ulint*	 	matched_fields, /* in/out: number of already completely 
				matched fields; when the function returns,
				contains the value the for current
				comparison */
	ulint*	  	matched_bytes);/* in/out: number of already matched 
				bytes within the first field not completely
				matched; when the function returns, contains
				the value for the current comparison */
/*****************************************************************
This function is used to compare two physical records. Only the common
first fields are compared. */
/*比较两条物理记录。只比较常用的第一个字段。*/
UNIV_INLINE
int
cmp_rec_rec(
/*========*/	
				/* out: 1, 0 , -1 if rec1 is greater, equal,
				less, respectively, than rec2; only the common
				first fields are compared */
	rec_t*		rec1,	/* in: physical record */
	rec_t*		rec2,	/* in: physical record */
	dict_index_t*	index);	/* in: data dictionary index */


#ifndef UNIV_NONINL
#include "rem0cmp.ic"
#endif

#endif
