/************************************************************************
Memory primitives

(c) 1994, 1995 Innobase Oy

Created 5/11/1994 Heikki Tuuri
*************************************************************************/
/*内存原语*/
#include "ut0mem.h"

#ifdef UNIV_NONINL
#include "ut0mem.ic"
#endif

#include "mem0mem.h"
#include "os0sync.h"

/* This struct is placed first in every allocated memory block */
/* 这个结构首先放在每个分配的内存块中 */
typedef struct ut_mem_block_struct ut_mem_block_t;

/* The total amount of memory currently allocated from the OS with malloc */
/* 当前使用malloc从操作系统分配的内存总量 */
ulint	ut_total_allocated_memory	= 0;

struct ut_mem_block_struct{
        UT_LIST_NODE_T(ut_mem_block_t) mem_block_list;
			/* mem block list node */ /* 内存块的链表节点 */ 
	ulint	size;	/* size of allocated memory */ /* 已分配内存大小 */ 
	ulint	magic_n;
};

#define UT_MEM_MAGIC_N	1601650166

/* List of all memory blocks allocated from the operating system
with malloc */
/* 从操作系统mallc分配的所有内存块的列表*/
UT_LIST_BASE_NODE_T(ut_mem_block_t)   ut_mem_block_list;

os_fast_mutex_t ut_list_mutex;  /* this protects the list */ /* 他保护这个列表 */ 

ibool  ut_mem_block_list_inited = FALSE;

/**************************************************************************
Initializes the mem block list at database startup. */
/* 在数据库启动时初始化mem块列表。*/
static
void
ut_mem_block_list_init(void)
/*========================*/
{
        os_fast_mutex_init(&ut_list_mutex);
        UT_LIST_INIT(ut_mem_block_list);
	ut_mem_block_list_inited = TRUE;
}

/**************************************************************************
Allocates memory. Sets it also to zero if UNIV_SET_MEM_TO_ZERO is
defined and set_to_zero is TRUE. */
/*分配内存。如果UNIV_SET_MEM_TO_ZERO被定义并且set_to_zero是真的，则也将其设置为0*/
void*
ut_malloc_low(
/*==========*/
	                     /* out, own: allocated memory */
        ulint   n,           /* in: number of bytes to allocate */
	ibool   set_to_zero) /* in: TRUE if allocated memory should be set
			     to zero if UNIV_SET_MEM_TO_ZERO is defined */
{
	void*	ret;

	ut_ad((sizeof(ut_mem_block_t) % 8) == 0); /* check alignment ok */

	if (!ut_mem_block_list_inited) {
	        ut_mem_block_list_init();
	}

	os_fast_mutex_lock(&ut_list_mutex);

	ret = malloc(n + sizeof(ut_mem_block_t));

	if (ret == NULL) {
		fprintf(stderr,
		"InnoDB: Fatal error: cannot allocate %lu bytes of\n"
		"InnoDB: memory with malloc! Total allocated memory\n"
		"InnoDB: by InnoDB %lu bytes. Operating system errno: %d\n"
		"InnoDB: Cannot continue operation!\n"
		"InnoDB: Check if you should increase the swap file or\n"
		"InnoDB: ulimits of your operating system.\n"
		"InnoDB: On FreeBSD check you have compiled the OS with\n"
		"InnoDB: a big enough maximum process size.\n",
		                  n, ut_total_allocated_memory, errno);

	        os_fast_mutex_unlock(&ut_list_mutex);

		exit(1);
	}		

	if (set_to_zero) {
#ifdef UNIV_SET_MEM_TO_ZERO
	        memset(ret, '\0', n + sizeof(ut_mem_block_t));
#endif
	}

	((ut_mem_block_t*)ret)->size = n + sizeof(ut_mem_block_t);
	((ut_mem_block_t*)ret)->magic_n = UT_MEM_MAGIC_N;

	ut_total_allocated_memory += n + sizeof(ut_mem_block_t);
	
	UT_LIST_ADD_FIRST(mem_block_list, ut_mem_block_list,
			                         ((ut_mem_block_t*)ret));
	os_fast_mutex_unlock(&ut_list_mutex);

	return((void*)((byte*)ret + sizeof(ut_mem_block_t)));
}

/**************************************************************************
Allocates memory. Sets it also to zero if UNIV_SET_MEM_TO_ZERO is
defined. */
/*分配内存。如果UNIV_SET_MEM_TO_ZERO为0，则也将其设置为0定义。*/
void*
ut_malloc(
/*======*/
	                /* out, own: allocated memory */
        ulint   n)      /* in: number of bytes to allocate */
{
        return(ut_malloc_low(n, TRUE));
}
/**************************************************************************
Frees a memory block allocated with ut_malloc. */
/*释放用ut_malloc分配的内存块*/
void
ut_free(
/*====*/
	void* ptr)  /* in, own: memory block */
{
        ut_mem_block_t* block;

	block = (ut_mem_block_t*)((byte*)ptr - sizeof(ut_mem_block_t));

	os_fast_mutex_lock(&ut_list_mutex);

	ut_a(block->magic_n == UT_MEM_MAGIC_N);
	ut_a(ut_total_allocated_memory >= block->size);

	ut_total_allocated_memory -= block->size;
	
	UT_LIST_REMOVE(mem_block_list, ut_mem_block_list, block);
	free(block);
	
	os_fast_mutex_unlock(&ut_list_mutex);
}

/**************************************************************************
Frees all allocated memory not freed yet. */
/*释放所有尚未释放的已分配内存。*/
void
ut_free_all_mem(void)
/*=================*/
{
        ut_mem_block_t* block;

	os_fast_mutex_lock(&ut_list_mutex);

	while ((block = UT_LIST_GET_FIRST(ut_mem_block_list))) {

		ut_a(block->magic_n == UT_MEM_MAGIC_N);
		ut_a(ut_total_allocated_memory >= block->size);

		ut_total_allocated_memory -= block->size;
	
	        UT_LIST_REMOVE(mem_block_list, ut_mem_block_list, block);
	        free(block);
	}
		                      
	os_fast_mutex_unlock(&ut_list_mutex);

	ut_a(ut_total_allocated_memory == 0);
}

/**************************************************************************
Catenates two strings into newly allocated memory. The memory must be freed
using mem_free. */
/*将两个字符串连接到新分配的内存中。必须使用mem_free释放内存。*/
char*
ut_str_catenate(
/*============*/
			/* out, own: catenated null-terminated string */
	char*	str1,	/* in: null-terminated string */
	char*	str2)	/* in: null-terminated string */
{
	ulint	len1;
	ulint	len2;
	char*	str;

	len1 = ut_strlen(str1);
	len2 = ut_strlen(str2);

	str = mem_alloc(len1 + len2 + 1);

	ut_memcpy(str, str1, len1);
	ut_memcpy(str + len1, str2, len2);

	str[len1 + len2] = '\0';

	return(str);
}
