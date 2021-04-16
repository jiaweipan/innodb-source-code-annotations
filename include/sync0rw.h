/******************************************************
The read-write lock (for threads, not for database transactions)

(c) 1995 Innobase Oy

Created 9/11/1995 Heikki Tuuri
*******************************************************/
/*读写锁(用于线程，而不是数据库事务)*/
#ifndef sync0rw_h
#define sync0rw_h

#include "univ.i"
#include "ut0lst.h"
#include "sync0sync.h"
#include "os0sync.h"

/* The following undef is to prevent a name conflict with a macro
in MySQL: */
/*以下undef是为了防止MySQL宏的名字冲突:*/
#undef rw_lock_t

/* Latch types; these are used also in btr0btr.h: keep the numerical values
smaller than 30 and the order of the numerical values like below! */
/*锁类型;这些也在btr0btr.h中使用:保持数值小于30和数值的顺序如下!*/
#define RW_S_LATCH	1
#define	RW_X_LATCH	2
#define	RW_NO_LATCH	3

typedef struct rw_lock_struct		rw_lock_t;
typedef struct rw_lock_debug_struct	rw_lock_debug_t;

typedef UT_LIST_BASE_NODE_T(rw_lock_t)	rw_lock_list_t;

extern rw_lock_list_t 	rw_lock_list;
extern mutex_t		rw_lock_list_mutex;

/* The global mutex which protects debug info lists of all rw-locks.
To modify the debug info list of an rw-lock, this mutex has to be

acquired in addition to the mutex protecting the lock. */
/*保护所有rw锁的调试信息列表的全局互斥。
为了修改rw-lock的调试信息列表，除了保护锁的互斥量之外，还必须获得这个互斥量。*/
extern mutex_t		rw_lock_debug_mutex;
extern os_event_t	rw_lock_debug_event;	/* If deadlock detection does
					not get immediately the mutex it
					may wait for this event */ /*如果死锁检测没有立即得到互斥锁，它可能会等待这个事件*/
extern ibool		rw_lock_debug_waiters;	/* This is set to TRUE, if
					there may be waiters for the event */ /*如果事件中可能有等待者，则此设置为TRUE*/

extern	ulint	rw_s_system_call_count;
extern	ulint	rw_s_spin_wait_count;
extern	ulint	rw_s_exit_count;
extern	ulint	rw_s_os_wait_count;
extern	ulint	rw_x_system_call_count;
extern	ulint	rw_x_spin_wait_count;
extern	ulint	rw_x_os_wait_count;
extern	ulint	rw_x_exit_count;

/**********************************************************************
Creates, or rather, initializes an rw-lock object in a specified memory
location (which must be appropriately aligned). The rw-lock is initialized
to the non-locked state. Explicit freeing of the rw-lock with rw_lock_free
is necessary only if the memory block containing it is freed. */
/*创建，或者更确切地说，在指定的内存位置(必须适当对齐)初始化rw-lock对象。
rw-lock初始化为非锁定状态。只有当包含rw_lock_free的内存块被释放时，才需要使用rw_lock_free来显式地释放rw-lock。*/
#define rw_lock_create(L)	rw_lock_create_func((L), IB__FILE__, __LINE__)
/*=====================*/
/**********************************************************************
Creates, or rather, initializes an rw-lock object in a specified memory
location (which must be appropriately aligned). The rw-lock is initialized
to the non-locked state. Explicit freeing of the rw-lock with rw_lock_free
is necessary only if the memory block containing it is freed. */
/*创建，或者更确切地说，在指定的内存位置(必须适当对齐)初始化rw-lock对象。
rw-lock初始化为非锁定状态。只有当包含rw_lock_free的内存块被释放时，才需要使用rw_lock_free来显式地释放rw-lock*/
void
rw_lock_create_func(
/*================*/
	rw_lock_t*	lock,		/* in: pointer to memory */
	char*		cfile_name,	/* in: file name where created */
	ulint		cline);		/* in: file line where created */
/**********************************************************************
Calling this function is obligatory only if the memory buffer containing
the rw-lock is freed. Removes an rw-lock object from the global list. The
rw-lock is checked to be in the non-locked state. */
/*只有当包含rw-lock的内存缓冲区被释放时，才必须调用这个函数。
从全局列表中删除rw-lock对象。rw-lock被检查为非锁定状态。*/
void
rw_lock_free(
/*=========*/
	rw_lock_t*	lock);	/* in: rw-lock */
/**********************************************************************
Checks that the rw-lock has been initialized and that there are no
simultaneous shared and exclusive locks. */
/*检查rw-lock已经初始化，并且没有同时共享和排他锁。*/
ibool
rw_lock_validate(
/*=============*/
	rw_lock_t*	lock);
/******************************************************************
NOTE! The following macros should be used in rw s-locking, not the
corresponding function. */
/*注意!以下宏应该在rw s-locking中使用，而不是相应的函数。*/
#define rw_lock_s_lock(M)    rw_lock_s_lock_func(\
					  (M), 0, IB__FILE__, __LINE__)
/******************************************************************
NOTE! The following macros should be used in rw s-locking, not the
corresponding function. */
/*注意!以下宏应该在rw s-locking中使用，而不是相应的函数。*/
#define rw_lock_s_lock_gen(M, P)    rw_lock_s_lock_func(\
					  (M), (P), IB__FILE__, __LINE__)
/******************************************************************
NOTE! The following macros should be used in rw s-locking, not the
corresponding function. */
/*注意!以下宏应该在rw s-locking中使用，而不是相应的函数。*/
#define rw_lock_s_lock_nowait(M)    rw_lock_s_lock_func_nowait(\
					     (M), IB__FILE__, __LINE__)
/**********************************************************************
NOTE! Use the corresponding macro, not directly this function, except if
you supply the file name and line number. Lock an rw-lock in shared mode
for the current thread. If the rw-lock is locked in exclusive mode, or
there is an exclusive lock request waiting, the function spins a preset
time (controlled by SYNC_SPIN_ROUNDS), waiting for the lock, before
suspending the thread. */
/*注意!使用相应的宏，而不是直接使用这个函数，除非你提供文件名和行号。在共享模式下为当前线程锁定rw-Lock。
如果rw-lock被锁定在排他模式，或者有一个排他的锁请求正在等待，函数旋转一个预设的时间(由SYNC_SPIN_ROUNDS控制)，等待锁，然后挂起线程。*/
UNIV_INLINE
void
rw_lock_s_lock_func(
/*================*/
        rw_lock_t*   	lock,  	/* in: pointer to rw-lock */
	ulint		pass,	/* in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	char*		file_name,/* in: file name where lock requested */
	ulint		line);	/* in: line where requested */
/**********************************************************************
NOTE! Use the corresponding macro, not directly this function, except if
you supply the file name and line number. Lock an rw-lock in shared mode
for the current thread if the lock can be acquired immediately. */
/*注意!使用相应的宏，而不是直接使用这个函数，除非你提供文件名和行号。
如果可以立即获得锁，则以共享模式为当前线程锁定rw-lock。*/
UNIV_INLINE
ibool
rw_lock_s_lock_func_nowait(
/*=======================*/
				/* out: TRUE if success */
        rw_lock_t*   	lock,  	/* in: pointer to rw-lock */
	char*		file_name,/* in: file name where lock requested */
	ulint		line);	/* in: line where requested */
/**********************************************************************
NOTE! Use the corresponding macro, not directly this function! Lock an
rw-lock in exclusive mode for the current thread if the lock can be
obtained immediately. */
/*注意!使用相应的宏，而不是直接这个函数!如果可以立即获得锁，则以排他模式锁定当前线程的rw-lock。*/
UNIV_INLINE
ibool
rw_lock_x_lock_func_nowait(
/*=======================*/
				/* out: TRUE if success */
        rw_lock_t*   	lock,  	/* in: pointer to rw-lock */
	char*		file_name,/* in: file name where lock requested */
	ulint		line);	/* in: line where requested */
/**********************************************************************
Releases a shared mode lock. */
/*释放共享模式锁。*/
UNIV_INLINE
void
rw_lock_s_unlock_func(
/*==================*/
	rw_lock_t*	lock	/* in: rw-lock */
#ifdef UNIV_SYNC_DEBUG
	,ulint		pass	/* in: pass value; != 0, if the lock may have
				been passed to another thread to unlock */
#endif
	);
/***********************************************************************
Releases a shared mode lock. */
/*释放共享模式锁。*/
#ifdef UNIV_SYNC_DEBUG
#define rw_lock_s_unlock(L)    rw_lock_s_unlock_func(L, 0)
#else
#define rw_lock_s_unlock(L)    rw_lock_s_unlock_func(L)
#endif
/***********************************************************************
Releases a shared mode lock. */
/*释放共享模式锁。*/
#ifdef UNIV_SYNC_DEBUG
#define rw_lock_s_unlock_gen(L, P)    rw_lock_s_unlock_func(L, P)
#else
#define rw_lock_s_unlock_gen(L, P)    rw_lock_s_unlock_func(L)
#endif
/******************************************************************
NOTE! The following macro should be used in rw x-locking, not the
corresponding function. */
/*注意!下面的宏应该在rw x-locking中使用，而不是相应的函数。*/
#define rw_lock_x_lock(M)    rw_lock_x_lock_func(\
					  (M), 0, IB__FILE__, __LINE__)
/******************************************************************
NOTE! The following macro should be used in rw x-locking, not the
corresponding function. */
/*注意!下面的宏应该在rw x-locking中使用，而不是相应的函数。*/
#define rw_lock_x_lock_gen(M, P)    rw_lock_x_lock_func(\
					  (M), (P), IB__FILE__, __LINE__)
/******************************************************************
NOTE! The following macros should be used in rw x-locking, not the
corresponding function. */
/*注意!以下宏应该在rw x-locking中使用，而不是相应的函数。*/
#define rw_lock_x_lock_nowait(M)    rw_lock_x_lock_func_nowait(\
					     (M), IB__FILE__, __LINE__)
/**********************************************************************
NOTE! Use the corresponding macro, not directly this function! Lock an
rw-lock in exclusive mode for the current thread. If the rw-lock is locked
in shared or exclusive mode, or there is an exclusive lock request waiting,
the function spins a preset time (controlled by SYNC_SPIN_ROUNDS), waiting
for the lock, before suspending the thread. If the same thread has an x-lock
on the rw-lock, locking succeed, with the following exception: if pass != 0,
only a single x-lock may be taken on the lock. NOTE: If the same thread has
an s-lock, locking does not succeed! */
/*注意!使用相应的宏，而不是直接这个函数!在独占模式下为当前线程锁定rw-Lock。
如果rw-lock被锁定在共享或排他模式，或者有一个排他的锁请求正在等待，函数旋转一个预设的时间(由SYNC_SPIN_ROUNDS控制)，
等待锁，然后挂起线程。如果同一个线程在rw-lock上有一个x-lock，那么锁定成功，
但是有以下的例外情况:如果pass != 0，那么只有一个x-lock可以在这个锁上执行。
注意:如果同一个线程有s-lock，锁定不会成功!*/
void
rw_lock_x_lock_func(
/*================*/
        rw_lock_t*   	lock,  	/* in: pointer to rw-lock */
	ulint		pass,	/* in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	char*		file_name,/* in: file name where lock requested */
	ulint		line);	/* in: line where requested */
/**********************************************************************
Releases an exclusive mode lock. */
/*释放独占模式锁。*/
UNIV_INLINE
void
rw_lock_x_unlock_func(
/*==================*/
	rw_lock_t*	lock	/* in: rw-lock */
#ifdef UNIV_SYNC_DEBUG
	,ulint		pass	/* in: pass value; != 0, if the lock may have
				been passed to another thread to unlock */
#endif
	);
/***********************************************************************
Releases an exclusive mode lock. */
/*释放独占模式锁。*/
#ifdef UNIV_SYNC_DEBUG
#define rw_lock_x_unlock(L)    rw_lock_x_unlock_func(L, 0)
#else
#define rw_lock_x_unlock(L)    rw_lock_x_unlock_func(L)
#endif
/***********************************************************************
Releases an exclusive mode lock. */
/*释放独占模式锁。*/
#ifdef UNIV_SYNC_DEBUG
#define rw_lock_x_unlock_gen(L, P)    rw_lock_x_unlock_func(L, P)
#else
#define rw_lock_x_unlock_gen(L, P)    rw_lock_x_unlock_func(L)
#endif
/**********************************************************************
Low-level function which locks an rw-lock in s-mode when we know that it
is possible and none else is currently accessing the rw-lock structure.
Then we can do the locking without reserving the mutex. */
/*当我们知道有可能并且没有其他人正在访问rw-lock结构时，在s模式下锁定rw-lock的低级功能。
然后我们可以在不保留互斥量的情况下进行锁定。*/
UNIV_INLINE
void
rw_lock_s_lock_direct(
/*==================*/
        rw_lock_t*   	lock  	/* in: pointer to rw-lock */
	,char*		file_name, /* in: file name where lock requested */
	ulint		line	/* in: line where requested */
);
/**********************************************************************
Low-level function which locks an rw-lock in x-mode when we know that it
is not locked and none else is currently accessing the rw-lock structure.
Then we can do the locking without reserving the mutex. */
/*当我们知道rw-lock没有被锁定并且没有其他人正在访问rw-lock结构时，在x模式下锁定rw-lock的低级函数。
然后我们可以在不保留互斥量的情况下进行锁定。*/
UNIV_INLINE
void
rw_lock_x_lock_direct(
/*==================*/
        rw_lock_t*   	lock  	/* in: pointer to rw-lock */
	,char*		file_name, /* in: file name where lock requested */
	ulint		line	/* in: line where requested */
);
/**********************************************************************
This function is used in the insert buffer to move the ownership of an
x-latch on a buffer frame to the current thread. The x-latch was set by
the buffer read operation and it protected the buffer frame while the
read was done. The ownership is moved because we want that the current
thread is able to acquire a second x-latch which is stored in an mtr.
This, in turn, is needed to pass the debug checks of index page
operations. */
/*这个函数在插入缓冲区中用于将缓冲区帧上x锁存器的所有权移动到当前线程。
x锁存器是由缓冲区读取操作设置的，当读取完成时，它保护缓冲区帧。
所有权被转移是因为我们希望当前线程能够获得存储在mtr中的第二个x锁存器。
这反过来又需要通过索引页操作的调试检查。*/
void
rw_lock_x_lock_move_ownership(
/*==========================*/
	rw_lock_t*	lock);	/* in: lock which was x-locked in the
				buffer read */
/**********************************************************************
Releases a shared mode lock when we know there are no waiters and none
else will access the lock during the time this function is executed. */
/*当我们知道没有等待者且在此函数执行期间没有其他人访问该锁时，释放共享模式锁。*/
UNIV_INLINE
void
rw_lock_s_unlock_direct(
/*====================*/
	rw_lock_t*	lock);	/* in: rw-lock */
/**********************************************************************
Releases an exclusive mode lock when we know there are no waiters, and
none else will access the lock durint the time this function is executed. */
/*当我们知道没有等待者，并且在此函数执行期间没有其他人访问该锁时，释放排他模式锁。*/
UNIV_INLINE
void
rw_lock_x_unlock_direct(
/*====================*/
	rw_lock_t*	lock);	/* in: rw-lock */
/**********************************************************************
Sets the rw-lock latching level field. */
/*设置rw-lock锁定级别字段。*/
void
rw_lock_set_level(
/*==============*/
	rw_lock_t*	lock,	/* in: rw-lock */
	ulint		level);	/* in: level */
/**********************************************************************
Returns the value of writer_count for the lock. Does not reserve the lock
mutex, so the caller must be sure it is not changed during the call. */
/*返回锁的writer_count值。不保留锁互斥，因此调用者必须确保在调用过程中没有更改锁互斥。*/
UNIV_INLINE
ulint
rw_lock_get_x_lock_count(
/*=====================*/
				/* out: value of writer_count */
	rw_lock_t*	lock);	/* in: rw-lock */
/************************************************************************
Accessor functions for rw lock. */
/*rw锁的访问函数。*/
UNIV_INLINE
ulint
rw_lock_get_waiters(
/*================*/
	rw_lock_t*	lock);
UNIV_INLINE
ulint
rw_lock_get_writer(
/*===============*/
	rw_lock_t*	lock);
UNIV_INLINE
ulint
rw_lock_get_reader_count(
/*=====================*/
	rw_lock_t*	lock);
/**********************************************************************
Checks if the thread has locked the rw-lock in the specified mode, with
the pass value == 0. */
/*检查线程是否在指定的模式下锁定了rw-lock, pass值== 0。*/
ibool
rw_lock_own(
/*========*/
	rw_lock_t*	lock,		/* in: rw-lock */
	ulint		lock_type);	/* in: lock type */
/**********************************************************************
Checks if somebody has locked the rw-lock in the specified mode. */
/*检查是否有人在指定模式下锁定了rw-lock。*/
ibool
rw_lock_is_locked(
/*==============*/
	rw_lock_t*	lock,		/* in: rw-lock */
	ulint		lock_type);	/* in: lock type: RW_LOCK_SHARED,
					RW_LOCK_EX */
/*******************************************************************
Prints debug info of an rw-lock. */
/*打印rw-lock的调试信息。*/
void
rw_lock_print(
/*==========*/
	rw_lock_t*	lock);	/* in: rw-lock */
/*******************************************************************
Prints debug info of currently locked rw-locks. */
/*打印当前锁定的rw-locks的调试信息。*/
void
rw_lock_list_print_info(void);
/*=========================*/
/*******************************************************************
Returns the number of currently locked rw-locks.
Works only in the debug version. */
/*返回当前锁定的rw-locks的数量。仅在调试版本中有效。*/
ulint
rw_lock_n_locked(void);
/*==================*/

/*#####################################################################*/

/**********************************************************************
Acquires the debug mutex. We cannot use the mutex defined in sync0sync,
because the debug mutex is also acquired in sync0arr while holding the OS
mutex protecting the sync array, and the ordinary mutex_enter might
recursively call routines in sync0arr, leading to a deadlock on the OS
mutex. */
/*获取调试互斥锁。我们不能使用sync0sync中定义的互斥锁，因为在持有操作系统的互斥锁以保护同步数组的同时，
调试互斥锁也在sync0arr中获得，普通的mutex_enter可能递归地调用sync0arr中的例程，导致操作系统的互斥锁。*/
void
rw_lock_debug_mutex_enter(void);
/*==========================*/
/**********************************************************************
Releases the debug mutex. */
/*释放调试互斥。*/
void
rw_lock_debug_mutex_exit(void);
/*==========================*/
/*************************************************************************
Prints info of a debug struct. */
/*打印调试结构体的信息。*/
void
rw_lock_debug_print(
/*================*/
	rw_lock_debug_t*	info);	/* in: debug struct */

/* NOTE! The structure appears here only for the compiler to know its size.
Do not use its fields directly! The structure used in the spin lock
implementation of a read-write lock. Several threads may have a shared lock
simultaneously in this lock, but only one writer may have an exclusive lock,
in which case no shared locks are allowed. To prevent starving of a writer
blocked by readers, a writer may queue for the lock by setting the writer
field. Then no new readers are allowed in. */
/*注意!这个结构出现在这里只是为了让编译器知道它的大小。
不要直接使用它的字段!在读写锁的自旋锁实现中使用的结构。
多个线程可以在这个锁中同时拥有一个共享锁，但是只有一个写入器可以拥有排他锁，在这种情况下不允许使用共享锁。
为了防止被reader阻塞的写入器挨饿，写入器可以通过设置writer字段来排队获取锁。那就不允许新的读者进入。*/
struct rw_lock_struct {
	ulint	reader_count;	/* Number of readers who have locked this
				lock in the shared mode */ /*在共享模式下锁定该锁的读者数量*/
	ulint	writer; 	/* This field is set to RW_LOCK_EX if there
				is a writer owning the lock (in exclusive
				mode), RW_LOCK_WAIT_EX if a writer is
				queueing for the lock, and
				RW_LOCK_NOT_LOCKED, otherwise. */ /*如果有一个写入者拥有这个锁(排他模式下)，这个字段被设置为RW_LOCK_EX，
				如果一个写入者正在排队等待这个锁，则设置为RW_LOCK_WAIT_EX，否则设置为RW_LOCK_NOT_LOCKED。*/
	os_thread_id_t	writer_thread;
				/* Thread id of a possible writer thread */ /*线程可能写入线程的id*/
	ulint	writer_count;	/* Number of times the same thread has
				recursively locked the lock in the exclusive
				mode */ /*同一线程在排他模式下递归锁定锁的次数*/
	mutex_t	mutex;		/* The mutex protecting rw_lock_struct */ /*保护rw_lock_struct的互斥锁*/
	ulint	pass; 		/* Default value 0. This is set to some
				value != 0 given by the caller of an x-lock
				operation, if the x-lock is to be passed to
				another thread to unlock (which happens in
				asynchronous i/o). */ /*默认值0。如果x-lock要被传递给另一个线程去解锁(这在异步i/o中发生)，
				则x-lock操作的调用者将它设置为某个值!= 0。*/
	ulint	waiters;	/* This ulint is set to 1 if there are
				waiters (readers or writers) in the global
				wait array, waiting for this rw_lock.
				Otherwise, == 0. */ /*如果全局等待数组中有等待者(读者或写者)在等待rw_lock，则该ulint值设置为1。否则,= = 0。*/
	ibool	writer_is_wait_ex;
				/* This is TRUE if the writer field is
				RW_LOCK_WAIT_EX; this field is located far
				from the memory update hotspot fields which
				are at the start of this struct, thus we can
				peek this field without causing much memory
				bus traffic */ /*如果writer字段是RW_LOCK_WAIT_EX，则为真;这个字段离内存更新热点字段很远，
				热点字段位于这个结构的开头，因此我们可以在不引起太多内存总线流量的情况下查看这个字段*/
	UT_LIST_NODE_T(rw_lock_t) list;
				/* All allocated rw locks are put into a
				list */ /*所有分配的rw锁都放入一个列表中*/
	UT_LIST_BASE_NODE_T(rw_lock_debug_t) debug_list;
				/* In the debug version: pointer to the debug
				info list of the lock */ /*在调试版本:指向锁的调试信息列表的指针*/
	ulint	level;		/* Debug version: level in the global latching
				order; default SYNC_LEVEL_NONE */ /*调试版本:在全局锁存顺序中的级别;默认SYNC_LEVEL_NONE*/
	char*	cfile_name;	/* File name where lock created */
	ulint	cline;		/* Line where created */
	char*	last_s_file_name;/* File name where last time s-locked */
	char*	last_x_file_name;/* File name where last time x-locked */
	ulint	last_s_line;	/* Line number where last time s-locked */
	ulint	last_x_line;	/* Line number where last time x-locked */
	ulint	magic_n;
};

#define	RW_LOCK_MAGIC_N	22643

/* The structure for storing debug info of an rw-lock */
/*用于存储rw-lock调试信息的结构*/
struct	rw_lock_debug_struct {

	os_thread_id_t thread_id;  /* The thread id of the thread which
				locked the rw-lock */ /*锁定rw-lock的线程id*/
	ulint	pass;		/* Pass value given in the lock operation */ /*传递锁操作中给定的值*/
	ulint	lock_type;	/* Type of the lock: RW_LOCK_EX,
				RW_LOCK_SHARED, RW_LOCK_WAIT_EX */
	char*	file_name;	/* File name where the lock was obtained */
	ulint	line;		/* Line where the rw-lock was locked */
	UT_LIST_NODE_T(rw_lock_debug_t) list;
				/* Debug structs are linked in a two-way
				list */
};

#ifndef UNIV_NONINL
#include "sync0rw.ic"
#endif

#endif
