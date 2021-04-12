/******************************************************
Mutex, the basic synchronization primitive

(c) 1995 Innobase Oy

Created 9/5/1995 Heikki Tuuri
*******************************************************/
/*互斥，基本的同步原语*/
#ifndef sync0sync_h
#define sync0sync_h

#include "univ.i"
#include "sync0types.h"
#include "ut0lst.h"
#include "ut0mem.h"
#include "os0thread.h"
#include "os0sync.h"
#include "sync0arr.h"

/**********************************************************************
Initializes the synchronization data structures. */
/*初始化同步数据结构。*/
void
sync_init(void);
/*===========*/
/**********************************************************************
Frees the resources in synchronization data structures. */
/*释放同步数据结构中的资源。*/
void
sync_close(void);
/*===========*/
/**********************************************************************
Creates, or rather, initializes a mutex object to a specified memory
location (which must be appropriately aligned). The mutex is initialized
in the reset state. Explicit freeing of the mutex with mutex_free is
necessary only if the memory block containing it is freed. */
/*创建，或者更确切地说，将互斥对象初始化到指定的内存位置(必须适当对齐)。
互斥锁在reset状态初始化。只有当包含互斥锁的内存块被释放时，才需要使用mutex_free来显式释放互斥锁。*/
#define mutex_create(M)	mutex_create_func((M), IB__FILE__, __LINE__)
/*===================*/
/**********************************************************************
Creates, or rather, initializes a mutex object in a specified memory
location (which must be appropriately aligned). The mutex is initialized
in the reset state. Explicit freeing of the mutex with mutex_free is
necessary only if the memory block containing it is freed. */
/*创建，或者更确切地说，将互斥对象初始化到指定的内存位置(必须适当对齐)。
互斥锁在reset状态初始化。只有当包含互斥锁的内存块被释放时，才需要使用mutex_free来显式释放互斥锁。*/
void
mutex_create_func(
/*==============*/
	mutex_t*	mutex,		/* in: pointer to memory */
	char*		cfile_name,	/* in: file name where created */
	ulint		cline);		/* in: file line where created */
/**********************************************************************
Calling this function is obligatory only if the memory buffer containing
the mutex is freed. Removes a mutex object from the mutex list. The mutex
is checked to be in the reset state. */
/*只有当包含互斥锁的内存缓冲区被释放时，才必须调用这个函数。从互斥对象列表中移除一个互斥对象。互斥量被检查为复位状态。*/
#undef mutex_free			/* Fix for MacOS X */
void
mutex_free(
/*=======*/
	mutex_t*	mutex);	/* in: mutex */
/******************************************************************
NOTE! The following macro should be used in mutex locking, not the
corresponding function. */
/*注意!下面的宏应该用于互斥锁，而不是相应的函数。*/
#define mutex_enter(M)    mutex_enter_func((M), IB__FILE__, __LINE__)
/******************************************************************
NOTE! The following macro should be used in mutex locking, not the
corresponding function. */
/*注意!下面的宏应该用于互斥锁，而不是相应的函数。*/
/* NOTE! currently same as mutex_enter! */
/* 注意!当前相同的mutex_enter!*/
#define mutex_enter_fast(M)    	mutex_enter_func((M), IB__FILE__, __LINE__)
#define mutex_enter_fast_func  	mutex_enter_func;
/**********************************************************************
NOTE! Use the corresponding macro in the header file, not this function
directly. Locks a mutex for the current thread. If the mutex is reserved
the function spins a preset time (controlled by SYNC_SPIN_ROUNDS) waiting
for the mutex before suspending the thread. */
/*注意!在头文件中使用相应的宏，而不是直接使用这个函数。为当前线程锁定互斥锁。
如果互斥锁被保留，函数旋转一个预设的时间(由SYNC_SPIN_ROUNDS控制)来等待互斥锁，然后挂起线程。*/
UNIV_INLINE
void
mutex_enter_func(
/*=============*/
	mutex_t*	mutex,		/* in: pointer to mutex */
	char*		file_name, 	/* in: file name where locked */
	ulint		line);		/* in: line where locked */
/************************************************************************
Tries to lock the mutex for the current thread. If the lock is not acquired
immediately, returns with return value 1. */
/*尝试锁定当前线程的互斥锁。如果没有立即获得锁，则返回值为1。*/
ulint
mutex_enter_nowait(
/*===============*/
					/* out: 0 if succeed, 1 if not */
	mutex_t*	mutex,		/* in: pointer to mutex */
	char*	   	file_name, 	/* in: file name where mutex
					requested */
	ulint	   	line);		/* in: line where requested */
/**********************************************************************
Unlocks a mutex owned by the current thread. */
/*解锁当前线程拥有的互斥锁。*/
UNIV_INLINE
void
mutex_exit(
/*=======*/
	mutex_t*	mutex);	/* in: pointer to mutex */
/**********************************************************************
Returns TRUE if no mutex or rw-lock is currently locked.
Works only in the debug version. */
/*如果当前没有互斥锁或rw-lock被锁定，则返回TRUE。仅在调试版本中有效。*/
ibool
sync_all_freed(void);
/*================*/
/*#####################################################################
FUNCTION PROTOTYPES FOR DEBUGGING */
/***********************************************************************
Prints wait info of the sync system. */
/*打印同步系统的等待信息。*/
void
sync_print_wait_info(void);
/*======================*/
/***********************************************************************
Prints info of the sync system. */
/*打印同步系统信息。*/
void
sync_print(void);
/*============*/
/**********************************************************************
Checks that the mutex has been initialized. */
/*检查互斥锁是否已初始化。*/
ibool
mutex_validate(
/*===========*/
	mutex_t*	mutex);
/**********************************************************************
Sets the mutex latching level field. */
/*设置互斥锁锁定级别字段。*/
void
mutex_set_level(
/*============*/
	mutex_t*	mutex,	/* in: mutex */
	ulint		level);	/* in: level */
/**********************************************************************
Adds a latch and its level in the thread level array. Allocates the memory
for the array if called first time for this OS thread. Makes the checks
against other latch levels stored in the array for this thread. */
/*在线程级数组中添加锁存器及其级别。为这个操作系统线程第一次调用数组分配内存。对该线程存储在数组中的其他锁存级别进行检查。*/
void
sync_thread_add_level(
/*==================*/
	void*	latch,	/* in: pointer to a mutex or an rw-lock */
	ulint	level);	/* in: level in the latching order; if SYNC_LEVEL_NONE,
			nothing is done */			
/**********************************************************************
Removes a latch from the thread level array if it is found there. */
/*如果在线程级数组中找到锁存器，则删除它。*/
ibool
sync_thread_reset_level(
/*====================*/
			/* out: TRUE if found from the array; it is no error
			if the latch is not found, as we presently are not
			able to determine the level for every latch
			reservation the program does */
	void*	latch);	/* in: pointer to a mutex or an rw-lock */
/**********************************************************************
Checks that the level array for the current thread is empty. */
/*检查当前线程的级别数组是否为空。*/
ibool
sync_thread_levels_empty(void);
/*==========================*/
			/* out: TRUE if empty */
/**********************************************************************
Checks that the level array for the current thread is empty. */
/*检查当前线程的级别数组是否为空。*/
ibool
sync_thread_levels_empty_gen(
/*=========================*/
					/* out: TRUE if empty except the
					exceptions specified below */
	ibool	dict_mutex_allowed);	/* in: TRUE if dictionary mutex is
					allowed to be owned by the thread,
					also purge_is_running mutex is
					allowed */ /*如果字典互斥锁允许被线程拥有，那么也允许purge_is_running互斥锁*/
/**********************************************************************
Checks that the current thread owns the mutex. Works only
in the debug version. */
/*检查当前线程是否拥有互斥锁。仅在调试版本中有效。*/
ibool
mutex_own(
/*======*/
				/* out: TRUE if owns */
	mutex_t*	mutex);	/* in: mutex */
/**********************************************************************
Gets the debug information for a reserved mutex. */
/*获取保留的互斥锁的调试信息。*/
void
mutex_get_debug_info(
/*=================*/
	mutex_t*	mutex,		/* in: mutex */
	char**		file_name,	/* out: file where requested */
	ulint*		line,		/* out: line where requested */
	os_thread_id_t* thread_id);	/* out: id of the thread which owns
					the mutex */
/**********************************************************************
Counts currently reserved mutexes. Works only in the debug version. */
/*计数当前保留的互斥对象。仅在调试版本中有效。*/
ulint
mutex_n_reserved(void);
/*==================*/
/**********************************************************************
Prints debug info of currently reserved mutexes. */
/*打印当前保留的互斥锁的调试信息。*/
void
mutex_list_print_info(void);
/*========================*/
/**********************************************************************
NOT to be used outside this module except in debugging! Gets the value
of the lock word. */
/*除调试外，不得在此模块之外使用!获取锁字的值。*/
UNIV_INLINE
ulint
mutex_get_lock_word(
/*================*/
	mutex_t*	mutex);	/* in: mutex */
/**********************************************************************
NOT to be used outside this module except in debugging! Gets the waiters
field in a mutex. */
/*除调试外，不得在此模块之外使用!获取互斥锁中的waiter字段。*/
UNIV_INLINE
ulint
mutex_get_waiters(
/*==============*/
				/* out: value to set */		
	mutex_t*	mutex);	/* in: mutex */
/**********************************************************************
Implements the memory barrier operation which makes a serialization point to
the instruction flow. This is needed because the Pentium may speculatively
execute reads before preceding writes are committed. We could also use here
any LOCKed instruction (see Intel Software Dev. Manual, Vol. 3). */
/*实现内存屏障操作，使一个序列化指向指令流。
这是必须的，因为Pentium可能会在提交写之前执行读操作。
我们也可以在这里使用任何锁定指令(见英特尔软件开发手册，第3卷)。*/
void
mutex_fence(void);
/*=============*/

/*
		LATCHING ORDER WITHIN THE DATABASE 数据库内的锁存顺序
		==================================

The mutex or latch in the central memory object, for instance, a rollback
segment object, must be acquired before acquiring the latch or latches to
the corresponding file data structure. In the latching order below, these
file page object latches are placed immediately below the corresponding
central memory object latch or mutex.
中央内存对象(例如，回滚段对象)中的互斥锁或闩锁必须在获得对应文件数据结构的闩锁或闩锁之前获得。
按照下面的锁存顺序，这些文件页对象锁存器被直接放置在相应的中央内存对象锁存器或互斥锁的下面。

Synchronization object			Notes
----------------------			-----
		
Dictionary mutex			If we have a pointer to a dictionary   
|字典互斥			object, e.g., a table, it can be
|					accessed without reserving the
|					dictionary mutex. We must have a
|					reservation, a memoryfix, to the
|					appropriate table object in this case,
|					and the table must be explicitly
|					released later.如果我们有一个指向字典对象的指针，例如一个表，它可以被访问而不保留字典互斥量。
|					在本例中，必须对适当的表对象有一个预留(memoryfix)，稍后必须显式地释放表。                     
V          
Dictionary header  字典头
|
V					
Secondary index tree latch		The tree latch protects also all
|二级索引树闩					the B-tree non-leaf pages. These
V					            can be read with the page only 树闩也保护所有B-tree非叶页。这些只能在页面上读
Secondary index non-leaf		bufferfixed to save CPU time,
|二级索引非叶					no s-latch is needed on the page.
|								Modification of a page requires an
|								x-latch on the page, however. If a
|								thread owns an x-latch to the tree,
|								it is allowed to latch non-leaf pages
|								even after it has acquired the fsp
|								latch.
V					为了节省CPU时间，页面上不需要s-latch。然而，修改页面需要在页面上使用x-latch。如果线程拥有到树的x-latch，那么即使它已经获得了fsp latch，也允许它latch非叶页。
Secondary index leaf			The latch on the secondary index leaf
|二级索引的叶子			        can be kept while accessing the
|					            clustered index, to save CPU time.
V					在访问聚集索引时，可以保留二级索引叶上的锁存器，以节省CPU时间。
Clustered index tree latch		To increase concurrency, the tree
|聚簇索引树锁存器				latch is usually released when the
|								leaf page latch has been acquired.
V					为了增加并发性，通常在获得叶页锁存器时释放树形锁存器。
Clustered index non-leaf
|聚集索引非叶
V
Clustered index leaf
|聚集索引的叶子
V
Transaction system header
|交易系统头文件
V
Transaction undo mutex			The undo log entry must be written
|事务撤销互斥					before any index page is modified.
|								Transaction undo mutex is for the undo
|								logs the analogue of the tree latch
|								for a B-tree. If a thread has the
|								trx undo mutex reserved, it is allowed
|								to latch the undo log pages in any
|								order, and also after it has acquired
|								the fsp latch. 
V								在修改任何索引页之前，必须写入撤消日志项。事务撤消互斥锁用于撤消日志，类似于B-tree的树锁存器。
								如果一个线程保留了trx撤消互斥锁，那么它可以以任何顺序闩锁撤消日志页面，也可以在它获得fsp闩锁之后。
Rollback segment mutex			The rollback segment mutex must be
|回滚段互斥 					reserved, if, e.g., a new page must
|								be added to an undo log. The rollback
|								segment and the undo logs in its
|								history list can be seen as an
|								analogue of a B-tree, and the latches
|								reserved similarly, using a version of
|								lock-coupling. If an undo log must be
|								extended by a page when inserting an
|								undo log record, this corresponds to
|								a pessimistic insert in a B-tree.
V								回滚段互斥锁必须保留，例如，如果一个新的页面必须添加到撤销日志。它的历史列表中的回滚段和撤消日志可以看作是b -树的模拟，
 								并且使用锁耦合的版本以类似的方式保留锁存。如果在插入撤销日志记录时，撤销日志必须由页面扩展，这对应于b树中的悲观插入。
Rollback segment header
|回滚段标题
V
Purge system latch
|净化系统锁
V
Undo log pages				If a thread owns the trx undo mutex,
|Undo日志页面				or for a log in the history list, the
|							rseg mutex, it is allowed to latch
|							undo log pages in any order, and even
|							after it has acquired the fsp latch.
|							If a thread does not have the
|							appropriate mutex, it is allowed to
|							latch only a single undo log page in
|							a mini-transaction.
V							如果一个线程拥有trx撤消互斥锁，或者对于历史列表中的一个日志，即rseg互斥锁，则允许它以任何顺序闩锁撤消日志页面，
 							甚至在它已经获得fsp闩锁之后。如果一个线程没有适当的互斥锁，那么它只能锁存小事务中的一个撤消日志页面。
File space management latch		If a mini-transaction must allocate
|文件空间管理闩锁				several file pages, it can do that,
|								because it keeps the x-latch to the
|								file space management in its memo.
V 								如果一个小事务必须分配几个文件页，它可以这样做，因为它在memo中保留了文件空间管理的x锁存器。
File system pages
|文件系统页面
V
Kernel mutex				If a kernel operation needs a file
|内核互斥			page allocation, it must reserve the
|					fsp x-latch before acquiring the kernel
|					mutex. 如果一个内核操作需要一个文件页分配，它必须在获得内核互斥锁之前保留fsp x-latch。
V
Search system mutex
|搜索系统互斥
V
Buffer pool mutex
|缓冲池互斥
V
Log mutex
|日志互斥
Any other latch
|其他锁
V
Memory pool mutex 内存池互斥*/

/* Latching order levels */
#define SYNC_NO_ORDER_CHECK	3000	/* this can be used to suppress
					latching order checking */ /*这可以用来抑制锁存顺序检查*/
#define	SYNC_LEVEL_NONE		2000	/* default: level not defined */
#define SYNC_DICT		1000
#define SYNC_DICT_AUTOINC_MUTEX	999
#define	SYNC_FOREIGN_KEY_CHECK	998
#define	SYNC_PURGE_IS_RUNNING	997
#define SYNC_DICT_HEADER	995
#define SYNC_IBUF_HEADER	914
#define SYNC_IBUF_PESS_INSERT_MUTEX 912
#define SYNC_IBUF_MUTEX		910	/* ibuf mutex is really below
					SYNC_FSP_PAGE: we assign value this
					high only to get the program to pass
					the debug checks */ /*ibuf互斥量实际上低于SYNC_FSP_PAGE:我们将值赋得这么高，只是为了让程序通过调试检查*/
/*-------------------------------*/
#define	SYNC_INDEX_TREE		900
#define SYNC_TREE_NODE_NEW	892
#define SYNC_TREE_NODE_FROM_HASH 891
#define SYNC_TREE_NODE		890
#define	SYNC_PURGE_SYS		810
#define	SYNC_PURGE_LATCH	800
#define	SYNC_TRX_UNDO		700
#define SYNC_RSEG		600
#define SYNC_RSEG_HEADER_NEW	591
#define SYNC_RSEG_HEADER	590
#define SYNC_TRX_UNDO_PAGE	570
#define SYNC_EXTERN_STORAGE	500
#define	SYNC_FSP		400
#define	SYNC_FSP_PAGE		395
/*------------------------------------- Insert buffer headers */ 
/*------------------------------------- ibuf_mutex */
/*------------------------------------- Insert buffer trees */
#define	SYNC_IBUF_BITMAP_MUTEX	351
#define	SYNC_IBUF_BITMAP	350
/*-------------------------------*/
#define	SYNC_KERNEL		300
#define SYNC_REC_LOCK		299
#define	SYNC_TRX_LOCK_HEAP	298
#define SYNC_TRX_SYS_HEADER	290
#define SYNC_LOG		170
#define SYNC_RECV		168
#define	SYNC_SEARCH_SYS		160	/* NOTE that if we have a memory
					heap that can be extended to the
					buffer pool, its logical level is
					SYNC_SEARCH_SYS, as memory allocation
					can call routines there! Otherwise
					the level is SYNC_MEM_HASH. */
					 /*注意，如果我们有一个可以扩展到缓冲池的内存堆，它的逻辑级别是SYNC_SEARCH_SYS，
					因为内存分配可以在那里调用例程!否则级别为SYNC_MEM_HASH。*/
#define	SYNC_BUF_POOL		150
#define	SYNC_BUF_BLOCK		149
#define SYNC_DOUBLEWRITE	140
#define	SYNC_ANY_LATCH		135
#define SYNC_THR_LOCAL		133
#define	SYNC_MEM_HASH		131
#define	SYNC_MEM_POOL		130

/* Codes used to designate lock operations */ /*用于指定锁操作的代码*/
#define RW_LOCK_NOT_LOCKED 	350
#define RW_LOCK_EX		351
#define RW_LOCK_EXCLUSIVE	351
#define RW_LOCK_SHARED		352
#define RW_LOCK_WAIT_EX		353
#define SYNC_MUTEX		354

/* NOTE! The structure appears here only for the compiler to know its size.
Do not use its fields directly! The structure used in the spin lock
implementation of a mutual exclusion semaphore. */
/*注意!这个结构出现在这里只是为了让编译器知道它的大小。不要直接使用它的字段!互斥信号量的自旋锁实现中使用的结构。*/
struct mutex_struct {
	ulint	lock_word;	/* This ulint is the target of the atomic
				test-and-set instruction in Win32 */ /*这个ulint是Win32中原子测试和设置指令的目标*/
#ifndef _WIN32
	os_fast_mutex_t
		os_fast_mutex;	/* In other systems we use this OS mutex
				in place of lock_word */ /*在其他系统中，我们使用这个OS互斥锁来代替lock_word*/.
#endif
	ulint	waiters;	/* This ulint is set to 1 if there are (or
				may be) threads waiting in the global wait
				array for this mutex to be released.
				Otherwise, this is 0. */ 
				/*如果在全局等待数组中有(或者可能有)正在等待释放这个互斥锁的线程，
				那么这个ulint值就被设置为1。否则，这是0。*/
	UT_LIST_NODE_T(mutex_t)	list; /* All allocated mutexes are put into
				a list.	Pointers to the next and prev. */ /*链表*/
	os_thread_id_t thread_id; /* Debug version: The thread id of the
				thread which locked the mutex. */ /*当前持有该mutex的线程ID*/
	char*	file_name;	/* Debug version: File name where the mutex
				was locked */
	ulint	line;		/* Debug version: Line where the mutex was
				locked */
	ulint	level;		/* Debug version: level in the global latching
				order; default SYNC_LEVEL_NONE */
	char*	cfile_name;	/* File name where mutex created */
	ulint	cline;		/* Line where created */
	ulint	magic_n;
};

#define MUTEX_MAGIC_N	(ulint)979585

/* The global array of wait cells for implementation of the databases own
mutexes and read-write locks. Appears here for debugging purposes only! */
/*用于实现数据库的互斥锁和读写锁的等待单元的全局数组。仅供调试之用!*/
extern sync_array_t*	sync_primary_wait_array;

/* Constant determining how long spin wait is continued before suspending
the thread. A value 600 rounds on a 1995 100 MHz Pentium seems to correspond
to 20 microseconds. */
/*确定在挂起线程之前旋转等待的持续时间。值600轮在1995 100兆赫的奔腾似乎对应20微秒。*/
#define	SYNC_SPIN_ROUNDS	srv_n_spin_wait_rounds

#define SYNC_INFINITE_TIME	((ulint)(-1))

/* Means that a timeout elapsed when waiting */
/* 意味着等待时超时*/
#define SYNC_TIME_EXCEEDED	(ulint)1

/* The number of system calls made in this module. Intended for performance
monitoring. */
/*在此模块中进行的系统调用的数量。用于性能监视。*/
extern 	ulint	mutex_system_call_count;
extern	ulint	mutex_exit_count;

/* Latching order checks start when this is set TRUE */
/*当设置为TRUE时，就开始进行锁定顺序检查*/
extern ibool	sync_order_checks_on;

/* This variable is set to TRUE when sync_init is called */
/* 当调用sync_init时，这个变量被设置为TRUE*/
extern ibool	sync_initialized;

#ifndef UNIV_NONINL
#include "sync0sync.ic"
#endif

#endif
