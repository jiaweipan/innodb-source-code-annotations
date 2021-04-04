/**********************************************************************
Utilities for byte operations

(c) 1994, 1995 Innobase Oy

Created 1/20/1994 Heikki Tuuri
***********************************************************************/
/* 字节操作工具集 */

#ifndef ut0byte_h /* 防止ut0byte.h被重复引用 */
#define ut0byte_h


#include "univ.i"

/* Type definition for a 64-bit unsigned integer, which works also
in 32-bit machines. NOTE! Access the fields only with the accessor
functions. This definition appears here only for the compiler to
know the size of a dulint. */
/* 64位无符号整数的类型定义，该定义也适用在32位机器中。
注意！仅使用访问器访问字段功能。
此定义仅出现在此处供编译器使用知道一个dulint的大小。*/
typedef	struct dulint_struct	dulint;
struct dulint_struct{
	ulint	high;	/* most significant 32 bits */ /*最高有效32位*/
	ulint	low;	/* least significant 32 bits */ /*最低有效32位*/
};

/* Zero value for a dulint */ /*一个dulint的零值 */
extern dulint	ut_dulint_zero;

/* Maximum value for a dulint */ /*一个dulint的最大值 */
extern dulint	ut_dulint_max; 

/***********************************************************
Creates a 64-bit dulint out of two ulints. */
/*从两个ulint中创建64位dulint。*/
UNIV_INLINE
dulint
ut_dulint_create(
/*=============*/
			/* out: created dulint */ /*输出：被创建的dulint */
	ulint	high,	/* in: high-order 32 bits */   /*输入：高阶32位 */
	ulint	low);	/* in: low-order 32 bits */  /*输入：低阶32位 */
/***********************************************************
Gets the high-order 32 bits of a dulint. */
/*从一个dulint中获得高阶32位。*/
UNIV_INLINE
ulint
ut_dulint_get_high(
/*===============*/
			/* out: 32 bits in ulint */ /*输出：32位 ulint */
	dulint	d);	/* in: dulint */  /*输入：dulint */
/***********************************************************
Gets the low-order 32 bits of a dulint. */
/*从一个dulint中获得低阶32位。*/
UNIV_INLINE
ulint
ut_dulint_get_low(
/*==============*/
			/* out: 32 bits in ulint */ /*输出：32位 ulint */
	dulint	d);	/* in: dulint */  /*输入：dulint */
/***********************************************************
Tests if a dulint is zero. */
/*测试一个dulint是否是0。*/
UNIV_INLINE
ibool
ut_dulint_is_zero(
/*==============*/
			/* out: TRUE if zero */ /*输出：如果是0返回TRUE */
	dulint	a);	/* in: dulint */ /*输入：dulint */
/***********************************************************
Compares two dulints. */
/*对比两个dulints。*/
UNIV_INLINE
int
ut_dulint_cmp(
/*==========*/
			/* out: -1 if a < b, 0 if a == b,
			1 if a > b */ 
	dulint	a,	/* in: dulint */
	dulint	b);	/* in: dulint */
/***********************************************************
Calculates the max of two dulints. */
/*计算两个dulint的最大值 。*/
UNIV_INLINE
dulint
ut_dulint_get_max(
/*==============*/
			/* out: max(a, b) */
	dulint	a,	/* in: dulint */
	dulint	b);	/* in: dulint */
/***********************************************************
Calculates the min of two dulints. */
/*计算两个dulint的最小值 。*/
UNIV_INLINE
dulint
ut_dulint_get_min(
/*==============*/
			/* out: min(a, b) */
	dulint	a,	/* in: dulint */
	dulint	b);	/* in: dulint */
/***********************************************************
Adds a ulint to a dulint. */
/*向dulint添加ulint。*/
UNIV_INLINE
dulint
ut_dulint_add(
/*==========*/
			/* out: sum a + b */
	dulint	a,	/* in: dulint */
	ulint	b);	/* in: ulint */
/***********************************************************
Subtracts a ulint from a dulint. */
/*从dulint中减去ulint。*/
UNIV_INLINE
dulint
ut_dulint_subtract(
/*===============*/
			/* out: a - b */
	dulint	a,	/* in: dulint */
	ulint	b);	/* in: ulint, b <= a */
/***********************************************************
Subtracts a dulint from another. NOTE that the difference must be positive
and smaller that 4G. */
/*从另一个中减去一个。请注意，差值必须为正比4G还小*/
UNIV_INLINE
ulint
ut_dulint_minus(
/*============*/
			/* out: a - b */
	dulint	a,	/* in: dulint; NOTE a must be >= b and at most
			2 to power 32 - 1 greater */
	dulint	b);	/* in: dulint */
/************************************************************
Rounds a dulint downward to a multiple of a power of 2. */
/* 将dulint向下舍入为2的幂的倍数。 */
UNIV_INLINE
dulint
ut_dulint_align_down(
/*=================*/
				/* out: rounded value */ 
	dulint   n,        	/* in: number to be rounded */
	ulint    align_no);  	/* in: align by this number which must be a
				power of 2 */ /* 按这个数字对齐，这个数字必须是2的幂 */
/************************************************************
Rounds a dulint upward to a multiple of a power of 2. */
/* 将dulint向上舍入为2的幂的倍数。 */
UNIV_INLINE
dulint
ut_dulint_align_up(
/*===============*/
				/* out: rounded value */
	dulint   n,        	/* in: number to be rounded */
	ulint    align_no);  	/* in: align by this number which must be a
				power of 2 */ /* 按这个数字对齐，这个数字必须是2的幂 */
/***********************************************************
Increments a dulint variable by 1. */
/* 将dulint变量递增1 */
#define UT_DULINT_INC(D)\
{\
	if ((D).low == 0xFFFFFFFF) {\
		(D).high = (D).high + 1;\
		(D).low = 0;\
	} else {\
		(D).low = (D).low + 1;\
	}\
}
/***********************************************************
Tests if two dulints are equal. */
/* 测试两个dulint是否相等。 */
#define UT_DULINT_EQ(D1, D2)	(((D1).low == (D2).low)\
						&& ((D1).high == (D2).high))
/****************************************************************
Sort function for dulint arrays. */
/* dulint数组的排序函数。 */
void
ut_dulint_sort(dulint* arr, dulint* aux_arr, ulint low, ulint high);
/*===============================================================*/
/************************************************************
The following function calculates the value of an integer n rounded
to the least product of align_no which is >= n. align_no has to be a
power of 2. */
/* 下面的函数计算整数n的值到align_no的最小乘积>=n。align_no必须是2次幂*/
UNIV_INLINE
ulint
ut_calc_align(
/*==========*/
				/* out: rounded value */
	ulint    n,             /* in: number to be rounded */
	ulint    align_no);     /* in: align by this number */
/************************************************************
The following function calculates the value of an integer n rounded
to the biggest product of align_no which is <= n. align_no has to be a
power of 2. */
/*下面的函数计算整数n的值到align_no的最大乘积<=n。align_no必须是2次幂。*/
UNIV_INLINE
ulint
ut_calc_align_down(
/*===============*/
				/* out: rounded value */
	ulint    n,          	/* in: number to be rounded */
	ulint    align_no);	/* in: align by this number */
/*************************************************************
The following function rounds up a pointer to the nearest aligned address. */
/*下面的函数向上舍入指向最近对齐地址的指针。*/
UNIV_INLINE
void*
ut_align(
/*=====*/
				/* out: aligned pointer */
	void*   ptr,            /* in: pointer */
	ulint   align_no);     	/* in: align by this number */
/*************************************************************
The following function rounds down a pointer to the nearest
aligned address. */
/*下面的函数将指针向下舍入到最近的对齐的地址。*/
UNIV_INLINE
void*
ut_align_down(
/*==========*/
				/* out: aligned pointer */
	void*   ptr,            /* in: pointer */
	ulint   align_no);      /* in: align by this number */
/*********************************************************************
Gets the nth bit of a ulint. */
/* 得到一个ulint的第n个比特。*/
UNIV_INLINE
ibool
ut_bit_get_nth(
/*===========*/
			/* out: TRUE if nth bit is 1; 0th bit is defined to
			be the least significant */
	ulint	a,	/* in: ulint */
	ulint	n);	/* in: nth bit requested */
/*********************************************************************
Sets the nth bit of a ulint. */
/* 设置一个ulint的第n个比特。*/
UNIV_INLINE
ulint
ut_bit_set_nth(
/*===========*/
			/* out: the ulint with the bit set as requested */
	ulint	a,	/* in: ulint */ /* */
	ulint	n,	/* in: nth bit requested */
	ibool	val);	/* in: value for the bit to set */
/****************************************************************
Copies a string to a memory location, setting characters to lower case. */
/* 将字符串复制到内存位置，将字符设置为小写 */
void
ut_cpy_in_lower_case(
/*=================*/
        char*   dest,    /* in: destination */
	char*   source,  /* in: source */
        ulint   len);     /* in: string length */
/****************************************************************
Compares two strings when converted to lower case. */
/* 转换为小写时比较两个字符串 */
int
ut_cmp_in_lower_case(
/*=================*/
		        /* out: -1, 0, 1 if str1 < str2, str1 == str2,
			str1 > str2, respectively */
       char*   str1,    /* in: string1 */
       char*   str2,    /* in: string2 */
       ulint   len);     /* in: length of both strings */


#ifndef UNIV_NONINL
#include "ut0byte.ic"
#endif

#endif
