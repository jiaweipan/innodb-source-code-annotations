/******************************************************
The memory management

(c) 1994, 1995 Innobase Oy

Created 6/9/1994 Heikki Tuuri
*******************************************************/
/*内存管理*/
#ifndef mem0mem_h
#define mem0mem_h

#include "univ.i"
#include "ut0mem.h"
#include "ut0byte.h"
#include "ut0ut.h"
#include "ut0rnd.h"
#include "sync0sync.h"
#include "ut0lst.h"
#include "mach0data.h"

/* -------------------- MEMORY HEAPS ----------------------------- */

/* The info structure stored at the beginning of a heap block */
/* 存储在堆块开头的信息结构 */
typedef struct mem_block_info_struct mem_block_info_t;

/* A block of a memory heap consists of the info structure
followed by an area of memory */
/* 内存堆的块由信息结构和内存区域组成*/
typedef mem_block_info_t	mem_block_t;

/* A memory heap is a nonempty linear list of memory blocks */
/* 内存堆是内存块的非空线性列表 */
typedef mem_block_t	mem_heap_t;

/* Types of allocation for memory heaps: DYNAMIC means allocation from the
dynamic memory pool of the C compiler, BUFFER means allocation from the index
page buffer pool; the latter method is used for very big heaps */
/*内存堆的分配类型：动态意味着从C编译器的动态内存池分配，缓冲意味着从索引页缓冲池分配；后一种方法用于非常大的堆*/
#define MEM_HEAP_DYNAMIC	0	/* the most common type */ /*最常见的类型*/
#define MEM_HEAP_BUFFER		1
#define MEM_HEAP_BTR_SEARCH	2	/* this flag can be ORed to the
					previous */

/* The following start size is used for the first block in the memory heap if
the size is not specified, i.e., 0 is given as the parameter in the call of
create. The standard size is the maximum size of the blocks used for
allocations of small buffers. */
/*以下起始大小用于内存堆中的第一个块，如果未指定大小，即在调用中给定0作为参数创建。
标准大小是用于小缓冲区分配的最大尺寸。 */

#define MEM_BLOCK_START_SIZE            64
#define MEM_BLOCK_STANDARD_SIZE         8192

/* If a memory heap is allowed to grow into the buffer pool, the following
is the maximum size for a single allocated buffer: */
/* 如果允许内存堆增长到缓冲池中，则单个已分配缓冲区的最大大小：*/
#define MEM_MAX_ALLOC_IN_BUF		(UNIV_PAGE_SIZE - 200)

/**********************************************************************
Initializes the memory system. */
/*初始化内存系统*/
void
mem_init(
/*=====*/
	ulint	size);	/* in: common pool size in bytes */
/******************************************************************
Use this macro instead of the corresponding function! Macro for memory
heap creation. */
/*使用此宏而不是相应的函数！内存宏堆创建。*/
#define mem_heap_create(N)    mem_heap_create_func(\
						(N), NULL, MEM_HEAP_DYNAMIC,\
						IB__FILE__, __LINE__)
/******************************************************************
Use this macro instead of the corresponding function! Macro for memory
heap creation. */
/*使用此宏而不是相应的函数！内存宏堆创建。*/
#define mem_heap_create_in_buffer(N)	mem_heap_create_func(\
						(N), NULL, MEM_HEAP_BUFFER,\
						IB__FILE__, __LINE__)
/******************************************************************
Use this macro instead of the corresponding function! Macro for memory
heap creation. */
/*使用此宏而不是相应的函数！内存宏堆创建。*/
#define mem_heap_create_in_btr_search(N) mem_heap_create_func(\
					(N), NULL, MEM_HEAP_BTR_SEARCH |\
						MEM_HEAP_BUFFER,\
						IB__FILE__, __LINE__)
/******************************************************************
Use this macro instead of the corresponding function! Macro for fast
memory heap creation. An initial block of memory B is given by the
caller, N is its size, and this memory block is not freed by
mem_heap_free. See the parameter comment in mem_heap_create_func below. */
/*使用此宏而不是相应的函数！快速宏内存堆创建。
内存B的初始块由调用者，N是它的大小，这个内存块不是由内存堆空闲。
请参阅下面 mem_heap_create_func中的参数注释。*/
#define mem_heap_fast_create(N, B)	mem_heap_create_func(\
						(N), (B), MEM_HEAP_DYNAMIC,\
						IB__FILE__, __LINE__)

/******************************************************************
Use this macro instead of the corresponding function! Macro for memory
heap freeing. */

#define mem_heap_free(heap) mem_heap_free_func(\
					  (heap), IB__FILE__, __LINE__)
/*********************************************************************
NOTE: Use the corresponding macros instead of this function. Creates a
memory heap which allocates memory from dynamic space. For debugging
purposes, takes also the file name and line as argument in the debug
version. */
/*注意：使用相应的宏而不是此函数。
创建从动态空间分配内存的内存堆。
出于调试目的，还将文件名和行作为调试版本中的参数。*/
UNIV_INLINE
mem_heap_t*
mem_heap_create_func(
/*=================*/
				/* out, own: memory heap */
	ulint	n,		/* in: desired start block size,
				this means that a single user buffer
				of size n will fit in the block, 
				0 creates a default size block;
				if init_block is not NULL, n tells
				its size in bytes */
				/*所需的起始块大小，这意味着大小为n的单个用户缓冲区将适合该块，
				0将创建默认大小的块；如果init_block块不为空，则n以字节表示其大小*/
	void*	init_block,	/* in: if very fast creation is
				wanted, the caller can reserve some
				memory from its stack, for example,
				and pass it as the the initial block
				to the heap: then no OS call of malloc
				is needed at the creation. CAUTION:
				the caller must make sure the initial
				block is not unintentionally erased
				(if allocated in the stack), before
				the memory heap is explicitly freed. */
				/*例如，如果需要非常快的创建，调用方可以从堆栈中保留一些内存，并将其作为初始块传递给堆：
				那么在创建时不需要malloc的OS调用。
				注意：调用者在内存堆被显式释放之前，必须确保初始块没有被无意中擦除（如果在堆栈中分配）。 */
	ulint	type,		/* in: MEM_HEAP_DYNAMIC or MEM_HEAP_BUFFER */ 
	char*   file_name,	/* in: file name where created */
	ulint	line		/* in: line where created */
	);
/*********************************************************************
NOTE: Use the corresponding macro instead of this function.
Frees the space occupied by a memory heap. */
/*注意：使用相应的宏而不是此函数。释放内存堆占用的空间。*/
UNIV_INLINE
void
mem_heap_free_func(
/*===============*/
	mem_heap_t*   	heap,		/* in, own: heap to be freed */
	char*  		file_name,	/* in: file name where freed */
	ulint   	line		/* in: line where freed */
);
/*******************************************************************
Allocates n bytes of memory from a memory heap. */
/*从内存堆中分配n个字节的内存。*/
UNIV_INLINE
void*
mem_heap_alloc(
/*===========*/
				/* out: allocated storage, NULL if
				did not succeed */
	mem_heap_t*   	heap, 	/* in: memory heap */
	ulint           n);	/* in: number of bytes; if the heap is allowed
				to grow into the buffer pool, this must be
				<= MEM_MAX_ALLOC_IN_BUF */
/*********************************************************************
Returns a pointer to the heap top. */
/*返回指向堆顶的指针。*/
UNIV_INLINE
byte*
mem_heap_get_heap_top(
/*==================*/     
				/* out: pointer to the heap top */
	mem_heap_t*   	heap); 	/* in: memory heap */
/*********************************************************************
Frees the space in a memory heap exceeding the pointer given. The
pointer must have been acquired from mem_heap_get_heap_top. The first
memory block of the heap is not freed. */
/*释放内存堆中超过给定指针的空间。
指针必须是从mem_heap_get_heap_top获取的。
第一个堆的内存块未释放。 */
UNIV_INLINE
void
mem_heap_free_heap_top(
/*===================*/
	mem_heap_t*   	heap,	/* in: heap from which to free */
	byte*		old_top);/* in: pointer to old top of heap */
/*********************************************************************
Empties a memory heap. The first memory block of the heap is not freed. */
/*清空内存堆。堆的第一个内存块没有被释放。*/
UNIV_INLINE
void
mem_heap_empty(
/*===========*/
	mem_heap_t*   	heap);	/* in: heap to empty */
/*********************************************************************
Returns a pointer to the topmost element in a memory heap.
The size of the element must be given. */
/*返回指向内存堆中最顶层元素的指针。必须给出元素的大小。*/
UNIV_INLINE
void*
mem_heap_get_top(
/*=============*/     
				/* out: pointer to the topmost element */
	mem_heap_t*   	heap, 	/* in: memory heap */
	ulint           n);     /* in: size of the topmost element */
/*********************************************************************
Frees the topmost element in a memory heap.
The size of the element must be given. */
/*释放内存堆中最顶层的元素。必须给出元素的大小。*/
UNIV_INLINE
void
mem_heap_free_top(
/*==============*/     
	mem_heap_t*   	heap, 	/* in: memory heap */
	ulint           n);     /* in: size of the topmost element */
/*********************************************************************
Returns the space in bytes occupied by a memory heap. */
/*返回内存堆占用的空间（字节）。*/
UNIV_INLINE
ulint
mem_heap_get_size(
/*==============*/
	mem_heap_t*	heap);  	/* in: heap */
/******************************************************************
Use this macro instead of the corresponding function!
Macro for memory buffer allocation */
/*使用此宏而不是相应的函数！内存缓冲区分配宏*/
#define mem_alloc(N)    mem_alloc_func((N), IB__FILE__, __LINE__)
/******************************************************************
Use this macro instead of the corresponding function!
Macro for memory buffer allocation */
/*使用此宏而不是相应的函数！内存缓冲区分配宏*/
#define mem_alloc_noninline(N)    mem_alloc_func_noninline(\
					  (N), IB__FILE__, __LINE__)
/*******************************************************************
NOTE: Use the corresponding macro instead of this function.
Allocates a single buffer of memory from the dynamic memory of
the C compiler. Is like malloc of C. The buffer must be freed 
with mem_free. */
/*注意：使用相应的宏而不是此函数。
从C编译器的动态内存中分配一个内存缓冲区。
类似于C的malloc。缓冲区必须用mem_free释放。*/
UNIV_INLINE
void*
mem_alloc_func(
/*===========*/
				/* out, own: free storage, NULL
				if did not succeed */
	ulint 	n,              /* in: desired number of bytes */
	char*  	file_name,  	/* in: file name where created */
	ulint 	line            /* in: line where created */
);
/*******************************************************************
NOTE: Use the corresponding macro instead of this function.
Allocates a single buffer of memory from the dynamic memory of
the C compiler. Is like malloc of C. The buffer must be freed 
with mem_free. */
/*注意：使用相应的宏而不是此函数。
从C编译器的动态内存中分配一个内存缓冲区。
类似于C的malloc。缓冲区必须用mem_free释放。*/
void*
mem_alloc_func_noninline(
/*=====================*/
				/* out, own: free storage, NULL if did not
				succeed */
	ulint   n,              /* in: desired number of bytes */
	char*  	file_name,	/* in: file name where created */
	ulint   line		/* in: line where created */
	);
/******************************************************************
Use this macro instead of the corresponding function!
Macro for memory buffer freeing */
/*使用此宏而不是相应的函数！用于释放内存缓冲区的宏 */
#define mem_free(PTR)   mem_free_func((PTR), IB__FILE__, __LINE__)
/*******************************************************************
NOTE: Use the corresponding macro instead of this function.
Frees a single buffer of storage from
the dynamic memory of C compiler. Similar to free of C. */
/*注意：使用相应的宏而不是此函数。从C编译器的动态内存中释放一个存储缓冲区。类似于C的free。*/
UNIV_INLINE
void
mem_free_func(
/*==========*/
	void*   ptr,    	/* in, own: buffer to be freed */
	char*  	file_name,  	/* in: file name where created */
	ulint 	line            /* in: line where created */
);
/*******************************************************************
Implements realloc. */
/*实现 realloc */
UNIV_INLINE
void*
mem_realloc(
/*========*/
			/* out, own: free storage, NULL if did not succeed */
	void*	buf,	/* in: pointer to an old buffer */
	ulint   n,	/* in: desired number of bytes */
	char*  	file_name,/* in: file name where called */
	ulint 	line);  /* in: line where called */
#ifdef MEM_PERIODIC_CHECK
/**********************************************************************
Goes through the list of all allocated mem blocks, checks their magic
numbers, and reports possible corruption. */
/*检查所有分配内存块的列表，检查它们的幻数，并报告可能的损坏。*/
void
mem_validate_all_blocks(void);
/*=========================*/
#endif

/*#######################################################################*/
	
/* The info header of a block in a memory heap */
/* 内存堆中块的信息头 */
struct mem_block_info_struct {
	ulint   magic_n;/* magic number for debugging */
	char	file_name[8];/* file name where the mem heap was created */
	ulint	line;	/* line number where the mem heap was created */
	UT_LIST_BASE_NODE_T(mem_block_t) base; /* In the first block in the
			the list this is the base node of the list of blocks;
			in subsequent blocks this is undefined */
	UT_LIST_NODE_T(mem_block_t) list; /* This contains pointers to next
			and prev in the list. The first block allocated
			to the heap is also the first block in this list,
			though it also contains the base node of the list. */
	ulint   len;    /* physical length of this block in bytes */
	ulint 	type; 	/* type of heap: MEM_HEAP_DYNAMIC, or
			MEM_HEAP_BUF possibly ORed to MEM_HEAP_BTR_SEARCH */
	ibool	init_block; /* TRUE if this is the first block used in fast
			creation of a heap: the memory will be freed
			by the creator, not by mem_heap_free */
	ulint   free;   /* offset in bytes of the first free position for
			user data in the block */
	ulint   start;  /* the value of the struct field 'free' at the 
			creation of the block */
	byte* 	free_block;
			/* if the MEM_HEAP_BTR_SEARCH bit is set in type,
			and this is the heap root, this can contain an
			allocated buffer frame, which can be appended as a
			free block to the heap, if we need more space;
			otherwise, this is NULL */
#ifdef MEM_PERIODIC_CHECK	
	UT_LIST_NODE_T(mem_block_t) mem_block_list;
			/* List of all mem blocks allocated; protected
			by the mem_comm_pool mutex */
#endif
};

#define MEM_BLOCK_MAGIC_N	764741555
#define MEM_FREED_BLOCK_MAGIC_N	547711122

/* Header size for a memory heap block */
/* 内存堆块的头大小*/
#define MEM_BLOCK_HEADER_SIZE   ut_calc_align(sizeof(mem_block_info_t),\
							UNIV_MEM_ALIGNMENT)
#include "mem0dbg.h"

#ifndef UNIV_NONINL
#include "mem0mem.ic"
#endif

#endif 
