/******************************************************
The dynamically allocated array

(c) 1996 Innobase Oy

Created 2/5/1996 Heikki Tuuri
*******************************************************/
/*
 动态分配数组
 */

#ifndef dyn0dyn_h /* 防止dyn0dyn.h被重复引用 */
#define dyn0dyn_h

#include "univ.i"
#include "ut0lst.h"
#include "mem0mem.h"

typedef struct dyn_block_struct		dyn_block_t;
typedef dyn_block_t			dyn_array_t;


/* Initial 'payload' size in bytes in a dynamic array block */
/* 动态数组块中的默认“有效负载”大小（字节） */
#define	DYN_ARRAY_DATA_SIZE	512

/*************************************************************************
Initializes a dynamic array. */
/* 初始化一个动态数组 */
UNIV_INLINE
dyn_array_t*
dyn_array_create(
/*=============*/
				/* out: initialized dyn array */ 
				/* 输出：初始化后的动态数组 */
	dyn_array_t*	arr);	/* in: pointer to a memory buffer of
				size sizeof(dyn_array_t) */
				/* 输入：指向结构体dyn_array_t大小内存缓冲区的指针 */
/****************************************************************
Frees a dynamic array. */
/* 释放一个动态数组 */
UNIV_INLINE
void
dyn_array_free(
/*===========*/
	dyn_array_t*	arr);	/* in: dyn array */ /* 输入：指向结构体dyn_array_t大小内存缓冲区的指针 */
/*************************************************************************
Makes room on top of a dyn array and returns a pointer to a buffer in it.
After copying the elements, the caller must close the buffer using
dyn_array_close. */
/*
在动态数组顶部腾出空间并返回指向该缓冲区的指针。
在复制元素之后，调用者必须使用dyn_array_close关闭缓冲区。
*/
UNIV_INLINE
byte*
dyn_array_open(
/*===========*/
				/* out: pointer to the buffer */
				/* 输出:指向缓冲区的指针 */
	dyn_array_t*	arr,	/* in: dynamic array */ /* 输入：指向结构体dyn_array_t大小内存缓冲区的指针 */
	ulint		size);	/* in: size in bytes of the buffer */ /* 输入：缓冲区字节大小*/
/*************************************************************************
Closes the buffer returned by dyn_array_open. */
/*
关闭dyn_array_open返回的缓冲区*/
UNIV_INLINE
void
dyn_array_close(
/*============*/
	dyn_array_t*	arr,	/* in: dynamic array */ /* 输入：指向结构体dyn_array_t大小内存缓冲区的指针 */
	byte*		ptr);	/* in: buffer space from ptr up was not used */ /* 输入：地址大于ptr的缓冲区空间是未使用的*/
/*************************************************************************
Makes room on top of a dyn array and returns a pointer to
the added element. The caller must copy the element to
the pointer returned. */
/*
在dyn数组顶部腾出空间并返回指向被添加元素的指针。调用者必须将元素复制到被返回的指针。*/
UNIV_INLINE
void*
dyn_array_push(
/*===========*/
				/* out: pointer to the element */ /* 输出：指向元素的指针 */
	dyn_array_t*	arr,	/* in: dynamic array */ /* 输入：指向结构体dyn_array_t大小内存缓冲区的指针 */
	ulint		size);	/* in: size in bytes of the element */ /* 输入：缓冲区字节大小*/
/****************************************************************
Returns pointer to an element in dyn array. */
/*  返回一个指向动态数组元素的指针*/
UNIV_INLINE
void*
dyn_array_get_element(
/*==================*/
				/* out: pointer to element */ /* 输出：指向元素的指针 */
	dyn_array_t*	arr,	/* in: dyn array */ /* 输入：指向结构体dyn_array_t大小内存缓冲区的指针 */
	ulint		pos);	/* in: position of element as bytes 
				from array start */ /* 元素的位置（字节）从数组开始 */
/****************************************************************
Returns the size of stored data in a dyn array. */
/*  返回被存储数据在动态数组的大小*/
UNIV_INLINE
ulint
dyn_array_get_data_size(
/*====================*/
				/* out: data size in bytes */ /* 输出：数据大小（字节） */
	dyn_array_t*	arr);	/* in: dyn array */ /* 输入：指向结构体dyn_array_t大小内存缓冲区的指针 */
/****************************************************************
Gets the first block in a dyn array. */
/*  获得动态数组的第一个数据块*/
UNIV_INLINE
dyn_block_t*
dyn_array_get_first_block(
/*======================*/
	dyn_array_t*	arr);	/* in: dyn array */ /* 输入：指向动态数组指针 */
/****************************************************************
Gets the last block in a dyn array. */
/*  获得动态数组的最后一个数据块*/
UNIV_INLINE
dyn_block_t*
dyn_array_get_last_block(
/*=====================*/
	dyn_array_t*	arr);	/* in: dyn array */ /* 输入：指向动态数组指针 */
/************************************************************************
Gets the next block in a dyn array. */
/*  获得动态数组的下一个数据块*/
UNIV_INLINE
dyn_block_t*
dyn_array_get_next_block(
/*=====================*/
				/* out: pointer to next, NULL if end of list */
	dyn_array_t*	arr,	/* in: dyn array */ /* 输入：指向动态数组指针 */
	dyn_block_t*	block);	/* in: dyn array block */ /* 输入：指向当前动态数组数据块指针 */
/************************************************************************
Gets the number of used bytes in a dyn array block. */
/*  获得动态数组数据块的已使用字节数量 */
UNIV_INLINE
ulint
dyn_block_get_used(
/*===============*/
				/* out: number of bytes used */ /* 输出：已使用数据大小（字节） */
	dyn_block_t*	block);	/* in: dyn array block */ /* 输入：指向动态数组数据块指针 */
/************************************************************************
Gets pointer to the start of data in a dyn array block. */
/*  获得指向动态数组数据块起始地址的指针 */
UNIV_INLINE
byte*
dyn_block_get_data(
/*===============*/
				/* out: pointer to data */ /* 输出：指向数据的指针 */
	dyn_block_t*	block);	/* in: dyn array block */ /* 输入：指向动态数组数据块指针 */
/************************************************************************
Gets the next block in a dyn array. */
/*  获得动态数组数据块的下一个数据块*/
UNIV_INLINE
dyn_block_t*
dyn_block_get_next(
/*===============*/
				/* out: pointer to next, NULL if end of list */ /* 输出：指向下一个数据块的指针 */
	dyn_block_t*	block);	/* in: dyn array block */ /* 输入：指向动态数组数据块指针 */
/************************************************************
Pushes n bytes to a dyn array. */
/*  将n个字节推送到动态数组。*/
UNIV_INLINE
void
dyn_push_string(
/*============*/
	dyn_array_t*	arr,	/* in: dyn array */ /* 输入：指向动态数组指针 */
	byte*		str,	/* in: string to write */ /* 输入：指向写入字符串指针 */
	ulint		len);	/* in: string length */ /* 输入：字符串长度 */

/*#################################################################*/

/* NOTE! Do not use the fields of the struct directly: the definition
appears here only for the compiler to know its size! */
/*注意！不要直接使用结构体的字段：定义出现在这里只是为了让编译器知道它的大小 */
struct dyn_block_struct{
	mem_heap_t*	heap;	/* in the first block this is != NULL 
				if dynamic allocation has been needed */ /* 如果需要动态分配，在第一个数据块它是非空的 */
	ulint		used;	/* number of data bytes used in this block */ /* 在这个数据块里已使用字节数 */
	byte		data[DYN_ARRAY_DATA_SIZE];
				/* storage for array elements */	/* 数组元素的存储区 */
	UT_LIST_BASE_NODE_T(dyn_block_t) base;
				/* linear list of dyn blocks: this node is
				used only in the first block */ /* dyn块的线性列表：此节点仅用于第一个块 */
	UT_LIST_NODE_T(dyn_block_t) list;
				/* linear list node: used in all blocks */ /* 线性列表节点：用于所有块 */
#ifdef UNIV_DEBUG
	ulint		buf_end;/* only in the debug version: if dyn array is
				opened, this is the buffer end offset, else
				this is 0 */ /*仅在调试版本中：如果打开dyn array，则这是缓冲区结束偏移量，否则这是0*/
	ulint		magic_n;
#endif
};


#ifndef UNIV_NONINL
#include "dyn0dyn.ic"
#endif

#endif 
