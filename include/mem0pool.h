/******************************************************
The lowest-level memory management

(c) 1994, 1995 Innobase Oy

Created 6/9/1994 Heikki Tuuri
*******************************************************/
/* 最低级别的内存管理 */
#ifndef mem0pool_h
#define mem0pool_h

#include "univ.i"
#include "os0file.h"
#include "ut0lst.h"

typedef struct mem_area_struct	mem_area_t;
typedef struct mem_pool_struct	mem_pool_t;

/* The common memory pool */
/* 公共内存池 */
extern mem_pool_t*	mem_comm_pool;

/* Memory area header */
/* 内存区头 */
struct mem_area_struct{
	ulint		size_and_free;	/* memory area size is obtained by
					anding with ~MEM_AREA_FREE; area in
					a free list if ANDing with
					MEM_AREA_FREE results in nonzero */ 
	UT_LIST_NODE_T(mem_area_t)
			free_list;	/* free list node */
};

/* Each memory area takes this many extra bytes for control information */
/* 每一个内存区域都会占用这么多额外的字节作为控制信息 */
#define MEM_AREA_EXTRA_SIZE	(ut_calc_align(sizeof(struct mem_area_struct),\
                                              UNIV_MEM_ALIGNMENT))

/************************************************************************
Creates a memory pool. */
/* 创建内存池。 */
mem_pool_t*
mem_pool_create(
/*============*/
			/* out: memory pool */
	ulint	size);	/* in: pool size in bytes */
/************************************************************************
Allocates memory from a pool. NOTE: This low-level function should only be used in mem0mem.*! */
/* 从池中分配内存。注意：此低级函数只应用于mem0mem.*！  */
void*
mem_area_alloc(
/*===========*/
				/* out, own: allocated memory buffer */
	ulint		size,	/* in: allocated size in bytes; for optimum
				space usage, the size should be a power of 2
				minus MEM_AREA_EXTRA_SIZE */
	mem_pool_t*	pool);	/* in: memory pool */
/************************************************************************
Frees memory to a pool. */
/* 释放内存到内存池*/
void
mem_area_free(
/*==========*/
	void*		ptr,	/* in, own: pointer to allocated memory
				buffer */
	mem_pool_t*	pool);	/* in: memory pool */
/************************************************************************
Returns the amount of reserved memory. */
/* 返回保留内存量。*/
ulint
mem_pool_get_reserved(
/*==================*/
				/* out: reserved mmeory in bytes */
	mem_pool_t*	pool);	/* in: memory pool */
/************************************************************************
Reserves the mem pool mutex. */
/* 保留内存池互斥。*/
void
mem_pool_mutex_enter(void);
/*======================*/
/************************************************************************
Releases the mem pool mutex. */
/* 释放内存池互斥。*/
void
mem_pool_mutex_exit(void);
/*=====================*/
/************************************************************************
Validates a memory pool. */
/* 验证内存池。*/
ibool
mem_pool_validate(
/*==============*/
				/* out: TRUE if ok */
	mem_pool_t*	pool);	/* in: memory pool */
/************************************************************************
Prints info of a memory pool. */
/* 打印内存池的信息。*/
void
mem_pool_print_info(
/*================*/
	FILE*	        outfile,/* in: output file to write to */
	mem_pool_t*	pool);	/* in: memory pool */


#ifndef UNIV_NONINL
#include "mem0pool.ic"
#endif

#endif 
