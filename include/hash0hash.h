/******************************************************
The simple hash table utility

(c) 1997 Innobase Oy

Created 5/20/1997 Heikki Tuuri
*******************************************************/
/*简单哈希表公用程序*/
#ifndef hash0hash_h
#define hash0hash_h

#include "univ.i"
#include "mem0mem.h"
#include "sync0sync.h"

typedef struct hash_table_struct hash_table_t;
typedef struct hash_cell_struct hash_cell_t;

typedef void*	hash_node_t;

/*****************************************************************
Creates a hash table with >= n array cells. The actual number
of cells is chosen to be a prime number slightly bigger than n. */
/*创建包含>=n个数组单元格的哈希表。实际数量细胞数被选为比n稍大的素数。*/
hash_table_t*
hash_create(
/*========*/
			/* out, own: created table */
	ulint	n);	/* in: number of array cells */
/*****************************************************************
Creates a mutex array to protect a hash table. */
/*创建互斥数组以保护哈希表。*/
void
hash_create_mutexes(
/*================*/
	hash_table_t*	table,		/* in: hash table */
	ulint		n_mutexes,	/* in: number of mutexes */
	ulint		sync_level);	/* in: latching order level of the
					mutexes: used in the debug version */
/*****************************************************************
Frees a hash table. */
/*释放哈希表。*/
void
hash_table_free(
/*============*/
	hash_table_t*	table);	/* in, own: hash table */
/******************************************************************
Calculates the hash value from a folded value. */
/*从折叠值计算哈希值。*/
UNIV_INLINE
ulint
hash_calc_hash(
/*===========*/
				/* out: hashed value */
	ulint		fold,	/* in: folded value */
	hash_table_t*	table);	/* in: hash table */
/***********************************************************************
Inserts a struct to a hash table. */
/*将结构插入哈希表。*/
#define HASH_INSERT(TYPE, NAME, TABLE, FOLD, DATA)\
{\
	hash_cell_t*	cell3333;\
	TYPE*		struct3333;\
\
	ut_ad(!(TABLE)->mutexes || mutex_own(hash_get_mutex(TABLE, FOLD)));\
\
	(DATA)->NAME = NULL;\
\
	cell3333 = hash_get_nth_cell(TABLE, hash_calc_hash(FOLD, TABLE));\
\
	if (cell3333->node == NULL) {\
		cell3333->node = DATA;\
	} else {\
		struct3333 = cell3333->node;\
\
		while (struct3333->NAME != NULL) {\
\
			struct3333 = struct3333->NAME;\
		}\
\
		struct3333->NAME = DATA;\
	}\
}

/***********************************************************************
Deletes a struct from a hash table. */
/*从哈希表中删除结构。*/
#define HASH_DELETE(TYPE, NAME, TABLE, FOLD, DATA)\
{\
	hash_cell_t*	cell3333;\
	TYPE*		struct3333;\
\
	ut_ad(!(TABLE)->mutexes || mutex_own(hash_get_mutex(TABLE, FOLD)));\
\
	cell3333 = hash_get_nth_cell(TABLE, hash_calc_hash(FOLD, TABLE));\
\
	if (cell3333->node == DATA) {\
		cell3333->node = DATA->NAME;\
	} else {\
		struct3333 = cell3333->node;\
\
		while (struct3333->NAME != DATA) {\
\
			ut_ad(struct3333)\
			struct3333 = struct3333->NAME;\
		}\
\
		struct3333->NAME = DATA->NAME;\
	}\
}

/***********************************************************************
Gets the first struct in a hash chain, NULL if none. */
/*获取哈希链中的第一个结构，如果没有，则为NULL。*/
#define HASH_GET_FIRST(TABLE, HASH_VAL)\
	(hash_get_nth_cell(TABLE, HASH_VAL)->node)

/***********************************************************************
Gets the next struct in a hash chain, NULL if none. */
/*获取哈希链中的下一个结构，如果没有则为NULL。*/
#define HASH_GET_NEXT(NAME, DATA)	((DATA)->NAME)

/************************************************************************
Looks for a struct in a hash table. */
/*在哈希表中查找结构。*/
#define HASH_SEARCH(NAME, TABLE, FOLD, DATA, TEST)\
{\
\
	ut_ad(!(TABLE)->mutexes || mutex_own(hash_get_mutex(TABLE, FOLD)));\
\
	(DATA) = HASH_GET_FIRST(TABLE, hash_calc_hash(FOLD, TABLE));\
\
	while ((DATA) != NULL) {\
		if (TEST) {\
			break;\
		} else {\
			(DATA) = HASH_GET_NEXT(NAME, DATA);\
		}\
	}\
}

/****************************************************************
Gets the nth cell in a hash table. */
/*获取哈希表中的第n个单元格。 */
UNIV_INLINE
hash_cell_t*
hash_get_nth_cell(
/*==============*/
				/* out: pointer to cell */
	hash_table_t* 	table,	/* in: hash table */
	ulint 		n);	/* in: cell index */
/*****************************************************************
Returns the number of cells in a hash table. */
UNIV_INLINE
ulint
hash_get_n_cells(
/*=============*/
				/* out: number of cells */
	hash_table_t*	table);	/* in: table */
/***********************************************************************
Deletes a struct which is stored in the heap of the hash table, and compacts
the heap. The fold value must be stored in the struct NODE in a field named
'fold'. */
/*删除存储在哈希表堆中的结构，并压缩堆。fold值必须存储在struct节点中名为“fold”的字段中。 */
#define HASH_DELETE_AND_COMPACT(TYPE, NAME, TABLE, NODE)\
{\
	TYPE*		node111;\
	TYPE*		top_node111;\
	hash_cell_t*	cell111;\
	ulint		fold111;\
\
	fold111 = (NODE)->fold;\
\
	HASH_DELETE(TYPE, NAME, TABLE, fold111, NODE);\
\
	top_node111 = (TYPE*)mem_heap_get_top(\
				hash_get_heap(TABLE, fold111),\
							sizeof(TYPE));\
\
	/* If the node to remove is not the top node in the heap, compact the\
	heap of nodes by moving the top node in the place of NODE. */\
\	/*如果要移除的节点不是堆中的顶部节点，则通过将顶部节点移动到节点所在的位置来压缩节点堆。*/\
\  
	if (NODE != top_node111) {\
\
		/* Copy the top node in place of NODE */ /*复制顶部节点以代替节点*/\
\        
		*(NODE) = *top_node111;\
\
		cell111 = hash_get_nth_cell(TABLE,\
				hash_calc_hash(top_node111->fold, TABLE));\
\
		/* Look for the pointer to the top node, to update it */ /*查找指向顶部节点的指针以更新它*/\
\
		if (cell111->node == top_node111) {\
			/* The top node is the first in the chain */ /*顶部节点是链中的第一个节点*/\
\
			cell111->node = NODE;\
		} else {\
			/* We have to look for the predecessor of the top\
			node */ /*我们必须寻找顶层节点的前置*/\
			node111 = cell111->node;\
\
			while (top_node111 != HASH_GET_NEXT(NAME, node111)) {\
\
				node111 = HASH_GET_NEXT(NAME, node111);\
			}\
\
			/* Now we have the predecessor node */ /*现在我们有了前置节点*/\
\
			node111->NAME = NODE;\
		}\
	}\
\
	/* Free the space occupied by the top node */ /*释放顶部节点占用的空间 */ \
\
	mem_heap_free_top(hash_get_heap(TABLE, fold111), sizeof(TYPE));\
}

/***********************************************************************
Calculates the number of stored structs in a hash table. */
/*计算哈希表中存储的结构数。 */
#define HASH_GET_N_NODES(TYPE, NAME, TABLE, N)\
{\
	hash_cell_t*	cell3333;\
	TYPE*		struct3333;\
	ulint		i3333;\
\
	(N) = 0;\
\
	for (i3333 = 0; i3333 < hash_get_n_cells(TABLE); i3333++) {\
\
		cell3333 = hash_get_nth_cell(TABLE, i3333);\
\
		struct3333 = cell3333->node;\
\
		while (struct3333) {\
\
			(N) = (N) + 1;\
\
			struct = HASH_GET_NEXT(NAME, struct3333);\
		}\
	}\
}

/****************************************************************
Gets the mutex index for a fold value in a hash table. */
/*获取哈希表中fold值的互斥索引。*/
UNIV_INLINE
ulint
hash_get_mutex_no(
/*==============*/
				/* out: mutex number */
	hash_table_t* 	table,	/* in: hash table */
	ulint 		fold);	/* in: fold */
/****************************************************************
Gets the nth heap in a hash table. */
/*获取哈希表中的第n个堆。*/
UNIV_INLINE
mem_heap_t*
hash_get_nth_heap(
/*==============*/
				/* out: mem heap */
	hash_table_t* 	table,	/* in: hash table */
	ulint 		i);	/* in: index of the heap */
/****************************************************************
Gets the heap for a fold value in a hash table. */
/*获取哈希表中fold值的堆。*/
UNIV_INLINE
mem_heap_t*
hash_get_heap(
/*==========*/
				/* out: mem heap */
	hash_table_t* 	table,	/* in: hash table */
	ulint 		fold);	/* in: fold */
/****************************************************************
Gets the nth mutex in a hash table. */
/*获取哈希表中的第n个互斥体。*/
UNIV_INLINE
mutex_t*
hash_get_nth_mutex(
/*===============*/
				/* out: mutex */
	hash_table_t* 	table,	/* in: hash table */
	ulint 		i);	/* in: index of the mutex */
/****************************************************************
Gets the mutex for a fold value in a hash table. */
/*获取哈希表中fold值的互斥锁。*/
UNIV_INLINE
mutex_t*
hash_get_mutex(
/*===========*/
				/* out: mutex */
	hash_table_t* 	table,	/* in: hash table */
	ulint 		fold);	/* in: fold */
/****************************************************************
Reserves the mutex for a fold value in a hash table. */
/*为哈希表中的fold值占用互斥锁。*/
void
hash_mutex_enter(
/*=============*/
	hash_table_t* 	table,	/* in: hash table */
	ulint 		fold);	/* in: fold */
/****************************************************************
Releases the mutex for a fold value in a hash table. */
/*为哈希表中的fold值释放互斥锁。*/
void
hash_mutex_exit(
/*============*/
	hash_table_t* 	table,	/* in: hash table */
	ulint 		fold);	/* in: fold */
/****************************************************************
Reserves all the mutexes of a hash table, in an ascending order. */
/*按升序占用哈希表的所有mutex。*/
void
hash_mutex_enter_all(
/*=================*/
	hash_table_t* 	table);	/* in: hash table */
/****************************************************************
Releases all the mutexes of a hash table. */
/*释放哈希表的所有mutex。*/
void
hash_mutex_exit_all(
/*================*/
	hash_table_t* 	table);	/* in: hash table */


struct hash_cell_struct{
	void*	node;	/* hash chain node, NULL if none */
};

/* The hash table structure */
/* 哈希表结构*/
struct hash_table_struct {
	ulint		n_cells;/* number of cells in the hash table *//*哈希表中的单元格数*/
	hash_cell_t*	array;	/* pointer to cell array */ /*指向单元格数组的指针*/
	ulint		n_mutexes;/* if mutexes != NULL, then the number of
				mutexes, must be a power of 2 *//*如果互斥！=NULL，则互斥锁的数目必须是2的幂 */
	mutex_t*	mutexes;/* NULL, or an array of mutexes used to
				protect segments of the hash table */ /*NULL，或用于保护哈希表段的互斥量数组*/
	mem_heap_t**	heaps;	/* if this is non-NULL, hash chain nodes for
				external chaining can be allocated from these
				memory heaps; there are then n_mutexes many of
				these heaps */ /*如果该值为非空，则可以从这些内存堆中分配用于外部链接的哈希链节点；然后这些堆中会有n_mutexes个 */
	mem_heap_t*	heap;
	ulint		magic_n;
};

#define HASH_TABLE_MAGIC_N	76561114

#ifndef UNIV_NONINL
#include "hash0hash.ic"
#endif

#endif
