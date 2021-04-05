/***********************************************************************
Memory primitives

(c) 1994, 1995 Innobase Oy

Created 5/30/1994 Heikki Tuuri
************************************************************************/
/*内存原语*/
#ifndef ut0mem_h
#define ut0mem_h

#include "univ.i"
#include <string.h>
#include <stdlib.h>

/* The total amount of memory currently allocated from the OS with malloc */
/* 当前使用malloc从操作系统分配的内存总量 */
extern ulint	ut_total_allocated_memory;

UNIV_INLINE
void*
ut_memcpy(void* dest, void* sour, ulint n);

UNIV_INLINE
void*
ut_memmove(void* dest, void* sour, ulint n);

UNIV_INLINE
int
ut_memcmp(void* str1, void* str2, ulint n);


/**************************************************************************
Allocates memory. Sets it also to zero if UNIV_SET_MEM_TO_ZERO is
defined and set_to_zero is TRUE. */
/* 分配内存。如果UNIV_SET_MEM_TO_ZERO为0并且set_to_zero是真的，则也将其设置为0。*/
void*
ut_malloc_low(
/*==========*/
	                     /* out, own: allocated memory */
        ulint   n,           /* in: number of bytes to allocate */
	ibool   set_to_zero); /* in: TRUE if allocated memory should be set
			     to zero if UNIV_SET_MEM_TO_ZERO is defined */
/**************************************************************************
Allocates memory. Sets it also to zero if UNIV_SET_MEM_TO_ZERO is
defined. */
/*分配内存。如果UNIV_SET_MEM_TO_ZERO为0，则也将其设置为0定义。*/
void*
ut_malloc(
/*======*/
	                /* out, own: allocated memory */
        ulint   n);     /* in: number of bytes to allocate */
/**************************************************************************
Frees a memory bloock allocated with ut_malloc. */
/*释放用ut_malloc分配的内存块。 */
void
ut_free(
/*====*/
	void* ptr);  /* in, own: memory block */
/**************************************************************************
Frees all allocated memory not freed yet. */
/*释放所有尚未释放的已分配内存。 */
void
ut_free_all_mem(void);
/*=================*/

UNIV_INLINE
char*
ut_strcpy(char* dest, char* sour);

UNIV_INLINE
ulint
ut_strlen(char* str);

UNIV_INLINE
int
ut_strcmp(void* str1, void* str2);

/**************************************************************************
Catenates two strings into newly allocated memory. The memory must be freed
using mem_free. */
/*将两个字符串连接到新分配的内存中。必须使用mem_free释放内存。*/
char*
ut_str_catenate(
/*============*/
			/* out, own: catenated null-terminated string */
	char*	str1,	/* in: null-terminated string */
	char*	str2);	/* in: null-terminated string */

#ifndef UNIV_NONINL
#include "ut0mem.ic"
#endif

#endif

