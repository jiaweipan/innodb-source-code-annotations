/*******************************************************************
Byte utilities

(c) 1994, 1995 Innobase Oy

Created 5/11/1994 Heikki Tuuri
********************************************************************/

#include "ut0byte.h"

#ifdef UNIV_NONINL
#include "ut0byte.ic"
#endif

#include "ut0sort.h"

/* Zero value for a dulint */ /*一个dulint的零值 */
dulint	ut_dulint_zero 		= {0, 0};

/* Maximum value for a dulint */ /*一个dulint的最大值 */
dulint	ut_dulint_max 		= {0xFFFFFFFF, 0xFFFFFFFF};

/****************************************************************
Sort function for dulint arrays. */ 
/* dulint数组的排序函数。 */
void
ut_dulint_sort(dulint* arr, dulint* aux_arr, ulint low, ulint high)
/*===============================================================*/
{
	UT_SORT_FUNCTION_BODY(ut_dulint_sort, arr, aux_arr, low, high,
				ut_dulint_cmp);
}

/****************************************************************
Copies a string to a memory location, setting characters to lower case. */
/* 将字符串复制到内存位置，将字符设置为小写 */
void
ut_cpy_in_lower_case(
/*=================*/
        char*   dest,  /* in: destination */
	char*   source,/* in: source */
        ulint   len)   /* in: string length */
{
        ulint i;

	for (i = 0; i < len; i++) {
	        dest[i] = tolower(source[i]);
	}
}

/****************************************************************
Compares two strings when converted to lower case. */
/* 转换为小写时比较两个字符串 */
int
ut_cmp_in_lower_case(
/*=================*/
		       /* out: -1, 0, 1 if str1 < str2, str1 == str2,
		       str1 > str2, respectively */
       char*   str1,   /* in: string1 */
       char*   str2,   /* in: string2 */
       ulint   len)    /* in: length of both strings */
{
       ulint i;

       for (i = 0; i < len; i++) {
	       if (tolower(str1[i]) < tolower(str2[i])) {
		       return(-1);
	       }

	       if (tolower(str1[i]) > tolower(str2[i])) {
	               return(1);
	       }
       }

       return(0);
}
