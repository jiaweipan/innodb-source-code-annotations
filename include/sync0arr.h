/******************************************************
The wait array used in synchronization primitives

(c) 1995 Innobase Oy

Created 9/5/1995 Heikki Tuuri
*******************************************************/
/*同步原语中使用的等待数组*/
#ifndef sync0arr_h
#define sync0arr_h

#include "univ.i"
#include "ut0lst.h"
#include "ut0mem.h"
#include "os0thread.h"

typedef struct sync_cell_struct        	sync_cell_t;
typedef struct sync_array_struct	sync_array_t;

#define SYNC_ARRAY_OS_MUTEX	1
#define SYNC_ARRAY_MUTEX	2

/***********************************************************************
Creates a synchronization wait array. It is protected by a mutex
which is automatically reserved when the functions operating on it
are called. */
/*同步原语中使用的等待数组*/
sync_array_t*
sync_array_create(
/*==============*/
				/* out, own: created wait array */
	ulint	n_cells,	/* in: number of cells in the array
				to create */
	ulint	protection);	/* in: either SYNC_ARRAY_OS_MUTEX or
				SYNC_ARRAY_MUTEX: determines the type
				of mutex protecting the data structure */
/**********************************************************************
Frees the resources in a wait array. */
/*释放等待数组中的资源。*/
void
sync_array_free(
/*============*/
	sync_array_t*	arr);	/* in, own: sync wait array */
/**********************************************************************
Reserves a wait array cell for waiting for an object.
The event of the cell is reset to nonsignalled state. */
/*为等待对象保留一个等待数组单元格。单元的事件被重置为无信号状态。*/
void
sync_array_reserve_cell(
/*====================*/
        sync_array_t*	arr,	/* in: wait array */
        void*   	object, /* in: pointer to the object to wait for */
        ulint		type,	/* in: lock request type */
        char*		file,	/* in: file where requested */
        ulint		line,	/* in: line where requested */
        ulint*   	index); /* out: index of the reserved cell */
/**********************************************************************
This function should be called when a thread starts to wait on
a wait array cell. In the debug version this function checks
if the wait for a semaphore will result in a deadlock, in which
case prints info and asserts. */
/*当线程开始在等待数组单元格上等待时，应该调用这个函数。
在调试版本中，该函数检查对信号量的等待是否会导致死锁，在这种情况下打印信息和断言。*/
void
sync_array_wait_event(
/*==================*/
        sync_array_t*	arr,	/* in: wait array */
        ulint   	index);  /* in: index of the reserved cell */
/**********************************************************************
Frees the cell. NOTE! sync_array_wait_event frees the cell
automatically! */
/*释放细胞。注意!sync_array_wait_event自动释放单元格!*/
void
sync_array_free_cell(
/*=================*/
	sync_array_t*	arr,	/* in: wait array */
        ulint    	index);  /* in: index of the cell in array */
/**************************************************************************
Looks for the cells in the wait array which refer
to the wait object specified,
and sets their corresponding events to the signaled state. In this
way releases the threads waiting for the object to contend for the object.
It is possible that no such cell is found, in which case does nothing. */
/*在等待数组中查找引用指定的等待对象的单元格，并将它们对应的事件设置为有信号的状态。
通过这种方式释放等待对象竞争对象的线程。有可能没有找到这样的细胞，在这种情况下，什么也不能做。*/
void
sync_array_signal_object(
/*=====================*/
	sync_array_t*	arr,	/* in: wait array */
	void*		object);/* in: wait object */
/**************************************************************************
If the wakeup algorithm does not work perfectly at semaphore relases,
this function will do the waking (see the comment in mutex_exit). This
function should be called about every 1 second in the server. */
/*如果唤醒算法在释放信号量时不能完美地工作，这个函数将执行唤醒(参见mutex_exit中的注释)。这个函数应该在服务器上每1秒调用一次。*/
void
sync_arr_wake_threads_if_sema_free(void);
/*====================================*/
/**************************************************************************
Prints warnings of long semaphore waits to stderr. Currently > 120 sec. */
/*向标准错误输出长信号量等待的警告。目前> 120秒*/
void
sync_array_print_long_waits(void);
/*=============================*/
/************************************************************************
Validates the integrity of the wait array. Checks
that the number of reserved cells equals the count variable. */
/*验证等待数组的完整性。检查保留的单元格数是否等于count变量。*/
void
sync_array_validate(
/*================*/
	sync_array_t*	arr);	/* in: sync wait array */
/**************************************************************************
Prints info of the wait array. */
/*打印等待数组的信息。*/
void
sync_array_print_info(
/*==================*/
	sync_array_t*	arr);	/* in: wait array */


#ifndef UNIV_NONINL
#include "sync0arr.ic"
#endif

#endif
