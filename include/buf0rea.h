/******************************************************
The database buffer read
数据库缓冲区读取
(c) 1995 Innobase Oy

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0rea_h
#define buf0rea_h

#include "univ.i"
#include "buf0types.h"

/************************************************************************
High-level function which reads a page asynchronously from a file to the
buffer buf_pool if it is not already there. Sets the io_fix flag and sets
an exclusive lock on the buffer frame. The flag is cleared and the x-lock
released by the i/o-handler thread. Does a random read-ahead if it seems
sensible. 这是一种高级函数，它将一个不存在的页面从文件异步读取到缓冲区buf_pool中。
设置io_fix标志并设置缓冲区帧上的排他锁。标记被清除，x-lock被i/o-handler线程释放。如果看起来合理的话，执行随机预读。*/
ulint
buf_read_page(
/*==========*/
			/* out: number of page read requests issued: this can
			be > 1 if read-ahead occurred */
	ulint	space,	/* in: space id */
	ulint	offset);/* in: page number */
/************************************************************************
Applies linear read-ahead if in the buf_pool the page is a border page of
a linear read-ahead area and all the pages in the area have been accessed.
Does not read any page if the read-ahead mechanism is not activated. Note
that the the algorithm looks at the 'natural' adjacent successor and
predecessor of the page, which on the leaf level of a B-tree are the next
and previous page in the chain of leaves. To know these, the page specified
in (space, offset) must already be present in the buf_pool. Thus, the
natural way to use this function is to call it when a page in the buf_pool
is accessed the first time, calling this function just after it has been
bufferfixed.
如果在buf_pool中，页面是线性预读区域的边界页面，并且该区域中的所有页面都已被访问，则应用线性预读。
当未激活预读机制时，不读取任何页面。请注意，该算法着眼于页面的“自然”相邻后继和前任，在b树的叶层上是叶子链的下一页和前一页。
要知道这些，在(空格，偏移量)中指定的页必须已经存在于buf_pool中。
因此，使用这个函数的自然方法是在第一次访问buf_pool中的一个页面时调用它，在它被bufferfixed之后调用这个函数。
NOTE 1: as this function looks at the natural predecessor and successor
fields on the page, what happens, if these are not initialized to any
sensible value? No problem, before applying read-ahead we check that the
area to read is within the span of the space, if not, read-ahead is not
applied. An uninitialized value may result in a useless read operation, but
only very improbably.
注意1:当这个函数查看页面上的自然前任和后继字段时，如果这些字段没有初始化为任何合理的值会发生什么?
没有问题，在应用预读之前，我们检查要读的区域是否在空间的范围内，如果不在，则不应用预读。
未初始化的值可能导致无用的读操作，但这只是非常不可能的。
NOTE 2: the calling thread may own latches on pages: to avoid deadlocks this
function must be written such that it cannot end up waiting for these
latches!注意2:调用线程可能拥有页上的锁存:为了避免死锁，这个函数必须被写成不能等待这些锁存!
NOTE 3: the calling thread must want access to the page given: this rule is
set to prevent unintended read-aheads performed by ibuf routines, a situation
which could result in a deadlock if the OS does not support asynchronous io. 
注意3:调用线程必须想要访问给定的页面:该规则的设置是为了防止ibuf例程执行意外的预读操作，如果操作系统不支持异步io，这种情况可能导致死锁。*/

ulint
buf_read_ahead_linear(
/*==================*/
			/* out: number of page read requests issued */
	ulint	space,	/* in: space id */
	ulint	offset);/* in: page number of a page; NOTE: the current thread
			must want access to this page (see NOTE 3 above) */
/************************************************************************
Issues read requests for pages which the ibuf module wants to read in, in
order to contract insert buffer trees. Technically, this function is like
a read-ahead function. 对ibuf模块想要读入的页面发出读请求，以收缩插入缓冲区树。从技术上讲，这个函数类似于预读函数。*/

void
buf_read_ibuf_merge_pages(
/*======================*/
	ibool	sync,		/* in: TRUE if the caller wants this function
				to wait for the highest address page to get
				read in, before this function returns */
	ulint	space,		/* in: space id */
	ulint*	page_nos,	/* in: array of page numbers to read, with
				the highest page number last in the array */
	ulint	n_stored);	/* in: number of page numbers in the array */
/************************************************************************
Issues read requests for pages which recovery wants to read in. 对恢复希望读取的页面发出读取请求。*/

void
buf_read_recv_pages(
/*================*/
	ibool	sync,		/* in: TRUE if the caller wants this function
				to wait for the highest address page to get
				read in, before this function returns 如果调用者希望该函数等待最高的地址页被读入，然后再返回，则为TRUE*/
	ulint	space,		/* in: space id */
	ulint*	page_nos,	/* in: array of page numbers to read, with the
				highest page number the last in the array 要读取的页码数组，最高页码在数组中的最后一个*/
	ulint	n_stored);	/* in: number of page numbers in the array 数组中的页码数*/

/* The size in pages of the area which the read-ahead algorithms read if
invoked 如果被调用，预读算法读取的区域的页大小*/

#define	BUF_READ_AHEAD_AREA	ut_min(32, buf_pool->curr_size / 16)

/* Modes used in read-ahead */
#define BUF_READ_IBUF_PAGES_ONLY	131
#define BUF_READ_ANY_PAGE		132

#endif
