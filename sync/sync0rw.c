/******************************************************
The read-write lock (for thread synchronization)

(c) 1995 Innobase Oy

Created 9/11/1995 Heikki Tuuri
*******************************************************/
/*读写锁(用于线程，而不是数据库事务)*/
#include "sync0rw.h"
#ifdef UNIV_NONINL
#include "sync0rw.ic"
#endif

#include "os0thread.h"
#include "mem0mem.h"
#include "srv0srv.h"

ulint	rw_s_system_call_count	= 0;
ulint	rw_s_spin_wait_count	= 0;
ulint	rw_s_os_wait_count	= 0;

ulint	rw_s_exit_count		= 0;

ulint	rw_x_system_call_count	= 0;
ulint	rw_x_spin_wait_count	= 0;
ulint	rw_x_os_wait_count	= 0;

ulint	rw_x_exit_count		= 0;

/* The global list of rw-locks */
/* rw-locks的全局列表*/
rw_lock_list_t	rw_lock_list;
mutex_t		rw_lock_list_mutex;

/* The global mutex which protects debug info lists of all rw-locks.
To modify the debug info list of an rw-lock, this mutex has to be
acquired in addition to the mutex protecting the lock. */
/*保护所有rw锁的调试信息列表的全局互斥。为了修改rw-lock的调试信息列表，除了保护锁的互斥量之外，还必须获得这个互斥量。*/
mutex_t		rw_lock_debug_mutex;
os_event_t	rw_lock_debug_event;	/* If deadlock detection does not
					get immediately the mutex, it may
					wait for this event */ /*如果死锁检测没有立即得到互斥锁，它可能会等待这个事件*/
ibool		rw_lock_debug_waiters;	/* This is set to TRUE, if there may
					be waiters for the event */ /*如果事件中可能有等待者，则此设置为TRUE*/

/**********************************************************************
Creates a debug info struct. */
/*创建一个调试信息结构体。*/
static
rw_lock_debug_t*
rw_lock_debug_create(void);
/*======================*/
/**********************************************************************
Frees a debug info struct. */
/*释放一个调试信息结构体。*/
static
void
rw_lock_debug_free(
/*===============*/
	rw_lock_debug_t* info);

/**********************************************************************
Creates a debug info struct. */
/*创建一个调试信息结构体。*/
static
rw_lock_debug_t*
rw_lock_debug_create(void)
/*======================*/
{
	return((rw_lock_debug_t*) mem_alloc(sizeof(rw_lock_debug_t)));
}

/**********************************************************************
Frees a debug info struct. */
/*释放一个调试信息结构体。*/
static
void
rw_lock_debug_free(
/*===============*/
	rw_lock_debug_t* info)
{
	mem_free(info);
}

/**********************************************************************
Creates, or rather, initializes an rw-lock object in a specified memory
location (which must be appropriately aligned). The rw-lock is initialized
to the non-locked state. Explicit freeing of the rw-lock with rw_lock_free
is necessary only if the memory block containing it is freed. */
/*创建，或者更确切地说，在指定的内存位置(必须适当对齐)初始化rw-lock对象。
rw-lock初始化为非锁定状态。只有当包含rw_lock_free的内存块被释放时，才需要使用rw_lock_free来显式地释放rw-lock。*/
void
rw_lock_create_func(
/*================*/
	rw_lock_t*	lock,		/* in: pointer to memory */
	char*		cfile_name,	/* in: file name where created */
	ulint		cline)		/* in: file line where created */
{
	/* If this is the very first time a synchronization
	object is created, then the following call initializes
	the sync system. */
	/*如果这是第一次创建同步对象，那么下面的调用将初始化同步系统。*/
	mutex_create(rw_lock_get_mutex(lock));
	mutex_set_level(rw_lock_get_mutex(lock), SYNC_NO_ORDER_CHECK);

	lock->mutex.cfile_name = cfile_name;
	lock->mutex.cline = cline;

	rw_lock_set_waiters(lock, 0);
	rw_lock_set_writer(lock, RW_LOCK_NOT_LOCKED);
	lock->writer_count = 0;
	rw_lock_set_reader_count(lock, 0);

	lock->writer_is_wait_ex = FALSE;

	UT_LIST_INIT(lock->debug_list);

	lock->magic_n = RW_LOCK_MAGIC_N;
	lock->level = SYNC_LEVEL_NONE;
	
	lock->cfile_name = cfile_name;
	lock->cline = cline;

	lock->last_s_file_name = "not yet reserved";
	lock->last_x_file_name = "not yet reserved";
	lock->last_s_line = 0;
	lock->last_x_line = 0;

	mutex_enter(&rw_lock_list_mutex);

	UT_LIST_ADD_FIRST(list, rw_lock_list, lock);

	mutex_exit(&rw_lock_list_mutex);
}

/**********************************************************************
Calling this function is obligatory only if the memory buffer containing
the rw-lock is freed. Removes an rw-lock object from the global list. The
rw-lock is checked to be in the non-locked state. */
/*只有当包含rw-lock的内存缓冲区被释放时，才必须调用这个函数。
从全局列表中删除rw-lock对象。rw-lock被检查为非锁定状态。*/
void
rw_lock_free(
/*=========*/
	rw_lock_t*	lock)	/* in: rw-lock */
{
	ut_ad(rw_lock_validate(lock));
	ut_a(rw_lock_get_writer(lock) == RW_LOCK_NOT_LOCKED);
	ut_a(rw_lock_get_waiters(lock) == 0);
	ut_a(rw_lock_get_reader_count(lock) == 0);
	
	lock->magic_n = 0;

	mutex_free(rw_lock_get_mutex(lock));

	mutex_enter(&rw_lock_list_mutex);

	UT_LIST_REMOVE(list, rw_lock_list, lock);

	mutex_exit(&rw_lock_list_mutex);
}

/**********************************************************************
Checks that the rw-lock has been initialized and that there are no
simultaneous shared and exclusive locks. */
/*检查rw-lock已经初始化，并且没有同时共享和排他锁。*/
ibool
rw_lock_validate(
/*=============*/
	rw_lock_t*	lock)
{
	ut_a(lock);

	mutex_enter(rw_lock_get_mutex(lock));

	ut_a(lock->magic_n == RW_LOCK_MAGIC_N);
	ut_a((rw_lock_get_reader_count(lock) == 0)
	     || (rw_lock_get_writer(lock) != RW_LOCK_EX));
	ut_a((rw_lock_get_writer(lock) == RW_LOCK_EX)
	     || (rw_lock_get_writer(lock) == RW_LOCK_WAIT_EX)
	     || (rw_lock_get_writer(lock) == RW_LOCK_NOT_LOCKED));
	ut_a((rw_lock_get_waiters(lock) == 0)
	     || (rw_lock_get_waiters(lock) == 1));
	ut_a((lock->writer != RW_LOCK_EX) || (lock->writer_count > 0));
	     
	mutex_exit(rw_lock_get_mutex(lock));

	return(TRUE);
}

/**********************************************************************
Lock an rw-lock in shared mode for the current thread. If the rw-lock is
locked in exclusive mode, or there is an exclusive lock request waiting,
the function spins a preset time (controlled by SYNC_SPIN_ROUNDS), waiting
for the lock, before suspending the thread. */
/*在共享模式下为当前线程锁定rw-Lock。如果rw-lock被锁定在排他模式，
或者有一个排他的锁请求正在等待，函数旋转一个预设的时间(由SYNC_SPIN_ROUNDS控制)，等待锁，然后挂起线程。*/
void
rw_lock_s_lock_spin(
/*================*/
        rw_lock_t*   	lock,  	/* in: pointer to rw-lock */
	ulint		pass,	/* in: pass value; != 0, if the lock
				will be passed to another thread to unlock */
	char*		file_name, /* in: file name where lock requested */
	ulint		line)	/* in: line where requested */
{
        ulint    index;	/* index of the reserved wait cell */
        ulint    i;   	/* spin round count */
        
        ut_ad(rw_lock_validate(lock));

lock_loop:
	rw_s_spin_wait_count++;

	/* Spin waiting for the writer field to become free */
        i = 0;

        while (rw_lock_get_writer(lock) != RW_LOCK_NOT_LOCKED
						&& i < SYNC_SPIN_ROUNDS) {
        	if (srv_spin_wait_delay) {
        		ut_delay(ut_rnd_interval(0, srv_spin_wait_delay));
        	}

        	i++;
        }

	if (i == SYNC_SPIN_ROUNDS) {
		os_thread_yield();
	}

	if (srv_print_latch_waits) {
		printf(
	"Thread %lu spin wait rw-s-lock at %lx cfile %s cline %lu rnds %lu\n",
			os_thread_get_curr_id(), (ulint)lock,
				lock->cfile_name, lock->cline, i);
	}

	mutex_enter(rw_lock_get_mutex(lock));

        /* We try once again to obtain the lock */

	if (TRUE == rw_lock_s_lock_low(lock, pass, file_name, line)) {
		mutex_exit(rw_lock_get_mutex(lock));

		return; /* Success */
	} else {
		/* If we get here, locking did not succeed, we may
		suspend the thread to wait in the wait array */

		rw_s_system_call_count++;

        	sync_array_reserve_cell(sync_primary_wait_array,
				lock, RW_LOCK_SHARED,
				file_name, line,
				&index);

		rw_lock_set_waiters(lock, 1);

		mutex_exit(rw_lock_get_mutex(lock));

		if (srv_print_latch_waits) {
			printf(
		"Thread %lu OS wait rw-s-lock at %lx cfile %s cline %lu\n",
				os_thread_get_curr_id(), (ulint)lock,
				lock->cfile_name, lock->cline);
		}

		rw_s_system_call_count++;
		rw_s_os_wait_count++;

       	 	sync_array_wait_event(sync_primary_wait_array, index);

        	goto lock_loop;
	}        
}

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
所有权被转移是因为我们希望当前线程能够获得存储在mtr中的第二个x锁存器。这反过来又需要通过索引页操作的调试检查。*/
void
rw_lock_x_lock_move_ownership(
/*==========================*/
	rw_lock_t*	lock)	/* in: lock which was x-locked in the
				buffer read */
{
	ut_ad(rw_lock_is_locked(lock, RW_LOCK_EX));

	mutex_enter(&(lock->mutex));

	lock->writer_thread = os_thread_get_curr_id();

	lock->pass = 0;

	mutex_exit(&(lock->mutex));
}

/**********************************************************************
Low-level function for acquiring an exclusive lock. */
/*用于获取独占锁的低级函数。*/
UNIV_INLINE
ulint
rw_lock_x_lock_low(
/*===============*/
				/* out: RW_LOCK_NOT_LOCKED if did
				not succeed, RW_LOCK_EX if success,
				RW_LOCK_WAIT_EX, if got wait reservation */
        rw_lock_t*   	lock,  	/* in: pointer to rw-lock */
	ulint		pass,	/* in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	char*		file_name,/* in: file name where lock requested */
	ulint		line)	/* in: line where requested */
{
	ut_ad(mutex_own(rw_lock_get_mutex(lock)));

	if (rw_lock_get_writer(lock) == RW_LOCK_NOT_LOCKED) {

		if (rw_lock_get_reader_count(lock) == 0) {
			
			rw_lock_set_writer(lock, RW_LOCK_EX);
			lock->writer_thread = os_thread_get_curr_id();
			lock->writer_count++;
			lock->pass = pass;
			
			#ifdef UNIV_SYNC_DEBUG
			rw_lock_add_debug_info(lock, pass, RW_LOCK_EX,
							file_name, line);
			#endif
			lock->last_x_file_name = file_name;
			lock->last_x_line = line;
		
			/* Locking succeeded, we may return */
			return(RW_LOCK_EX);
		} else {
			/* There are readers, we have to wait */
			rw_lock_set_writer(lock, RW_LOCK_WAIT_EX);
			lock->writer_thread = os_thread_get_curr_id();
			lock->pass = pass;
			lock->writer_is_wait_ex = TRUE;

			#ifdef UNIV_SYNC_DEBUG
			rw_lock_add_debug_info(lock, pass, RW_LOCK_WAIT_EX,
							file_name, line);
			#endif

			return(RW_LOCK_WAIT_EX);
		}

	} else if ((rw_lock_get_writer(lock) == RW_LOCK_WAIT_EX)
		   && (lock->writer_thread == os_thread_get_curr_id())) {

		if (rw_lock_get_reader_count(lock) == 0) {

			rw_lock_set_writer(lock, RW_LOCK_EX);
			lock->writer_count++;
			lock->pass = pass;
			lock->writer_is_wait_ex = FALSE;

			#ifdef UNIV_SYNC_DEBUG
			rw_lock_remove_debug_info(lock, pass, RW_LOCK_WAIT_EX);
			rw_lock_add_debug_info(lock, pass, RW_LOCK_EX,
							file_name, line);
			#endif
		
			lock->last_x_file_name = file_name;
			lock->last_x_line = line;

			/* Locking succeeded, we may return */
			return(RW_LOCK_EX);
		}

		return(RW_LOCK_WAIT_EX);

	} else if ((rw_lock_get_writer(lock) == RW_LOCK_EX)
		   && (lock->writer_thread == os_thread_get_curr_id())
		   && (lock->pass == 0)
		   && (pass == 0)) {

		lock->writer_count++;

		#ifdef UNIV_SYNC_DEBUG
		rw_lock_add_debug_info(lock, pass, RW_LOCK_EX, file_name,
									line);
		#endif
		
		lock->last_x_file_name = file_name;
		lock->last_x_line = line;

		/* Locking succeeded, we may return */
		return(RW_LOCK_EX);
	}

	/* Locking did not succeed */
	return(RW_LOCK_NOT_LOCKED);
}

/**********************************************************************
NOTE! Use the corresponding macro, not directly this function! Lock an
rw-lock in exclusive mode for the current thread. If the rw-lock is locked
in shared or exclusive mode, or there is an exclusive lock request waiting,
the function spins a preset time (controlled by SYNC_SPIN_ROUNDS), waiting
for the lock before suspending the thread. If the same thread has an x-lock
on the rw-lock, locking succeed, with the following exception: if pass != 0,
only a single x-lock may be taken on the lock. NOTE: If the same thread has
an s-lock, locking does not succeed! */
/*注意!使用相应的宏，而不是直接这个函数!在独占模式下为当前线程锁定rw- Lock。
如果rw-lock被锁定在共享或排他模式，或者有一个排他的锁请求正在等待，函数旋转一个预设的时间(由SYNC_SPIN_ROUNDS控制)，
在挂起线程之前等待锁。如果同一个线程在rw-lock上有一个x-lock，那么锁定成功，但是有以下的例外情况:
如果pass != 0，那么只有一个x-lock可以在这个锁上执行。注意:如果同一个线程有s-lock，锁定不会成功!*/
void
rw_lock_x_lock_func(
/*================*/
        rw_lock_t*   	lock,  	/* in: pointer to rw-lock */
	ulint		pass,	/* in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	char*		file_name,/* in: file name where lock requested */
	ulint		line)	/* in: line where requested */
{
        ulint	index;  /* index of the reserved wait cell */
        ulint	state;	/* lock state acquired */
        ulint	i;	/* spin round count */
        
        ut_ad(rw_lock_validate(lock));

lock_loop:
        /* Acquire the mutex protecting the rw-lock fields */
	mutex_enter_fast(&(lock->mutex));

	state = rw_lock_x_lock_low(lock, pass, file_name, line);
		
	mutex_exit(&(lock->mutex));
        
	if (state == RW_LOCK_EX) {

		return;	/* Locking succeeded */

	} else if (state == RW_LOCK_NOT_LOCKED) {

 		/* Spin waiting for the writer field to become free */
		i = 0;

        	while (rw_lock_get_writer(lock) != RW_LOCK_NOT_LOCKED 
               					&& i < SYNC_SPIN_ROUNDS) {
        		if (srv_spin_wait_delay) {
				ut_delay(ut_rnd_interval(0,
							srv_spin_wait_delay));
        		}
        		
        		i++;
        	}
		if (i == SYNC_SPIN_ROUNDS) {
			os_thread_yield();
		}
        } else if (state == RW_LOCK_WAIT_EX) {

 		/* Spin waiting for the reader count field to become zero */
		i = 0;

        	while (rw_lock_get_reader_count(lock) != 0 
               					&& i < SYNC_SPIN_ROUNDS) {
        		if (srv_spin_wait_delay) {
				ut_delay(ut_rnd_interval(0,
							srv_spin_wait_delay));
        		}

			i++;
        	}
		if (i == SYNC_SPIN_ROUNDS) {
			os_thread_yield();
		}
        } else {
		i = 0; /* Eliminate a compiler warning */
		ut_error;
	}	

	if (srv_print_latch_waits) {
		printf(
	"Thread %lu spin wait rw-x-lock at %lx cfile %s cline %lu rnds %lu\n",
			os_thread_get_curr_id(), (ulint)lock,
					lock->cfile_name, lock->cline, i);
	}

	rw_x_spin_wait_count++;

        /* We try once again to obtain the lock. Acquire the mutex protecting
	the rw-lock fields */

	mutex_enter(rw_lock_get_mutex(lock));

	state = rw_lock_x_lock_low(lock, pass, file_name, line);

	if (state == RW_LOCK_EX) {
		mutex_exit(rw_lock_get_mutex(lock));

		return;	/* Locking succeeded */
	}

	rw_x_system_call_count++;

        sync_array_reserve_cell(sync_primary_wait_array,
				lock, RW_LOCK_EX,
				file_name, line,
				&index);

	rw_lock_set_waiters(lock, 1);

	mutex_exit(rw_lock_get_mutex(lock));

	if (srv_print_latch_waits) {
		printf(
		"Thread %lu OS wait for rw-x-lock at %lx cfile %s cline %lu\n",
		os_thread_get_curr_id(), (ulint)lock, lock->cfile_name,
							lock->cline);
	}

	rw_x_system_call_count++;
	rw_x_os_wait_count++;

        sync_array_wait_event(sync_primary_wait_array, index);

        goto lock_loop;
}

/**********************************************************************
Acquires the debug mutex. We cannot use the mutex defined in sync0sync,
because the debug mutex is also acquired in sync0arr while holding the OS
mutex protecting the sync array, and the ordinary mutex_enter might
recursively call routines in sync0arr, leading to a deadlock on the OS
mutex. */
/*获取调试互斥锁。我们不能使用sync0sync中定义的互斥锁，因为在持有操作系统的互斥锁以保护同步数组的同时，
调试互斥锁也在sync0arr中获得，普通的mutex_enter可能递归地调用sync0arr中的例程，导致操作系统的互斥锁。*/
void
rw_lock_debug_mutex_enter(void)
/*==========================*/
{
loop:
	if (0 == mutex_enter_nowait(&rw_lock_debug_mutex,
			IB__FILE__, __LINE__)) {
		return;
	}

	os_event_reset(rw_lock_debug_event);

	rw_lock_debug_waiters = TRUE;

	if (0 == mutex_enter_nowait(&rw_lock_debug_mutex,
			IB__FILE__, __LINE__)) {
		return;
	}

	os_event_wait(rw_lock_debug_event);

	goto loop;	
}

/**********************************************************************
Releases the debug mutex. */
/*释放调试互斥。*/
void
rw_lock_debug_mutex_exit(void)
/*==========================*/
{
	mutex_exit(&rw_lock_debug_mutex);

	if (rw_lock_debug_waiters) {
		rw_lock_debug_waiters = FALSE;
		os_event_set(rw_lock_debug_event);
	}
}

/**********************************************************************
Inserts the debug information for an rw-lock. */
/*为rw-lock插入调试信息。*/
void
rw_lock_add_debug_info(
/*===================*/
	rw_lock_t*	lock,		/* in: rw-lock */
	ulint		pass,		/* in: pass value */
	ulint		lock_type,	/* in: lock type */
	char*		file_name,	/* in: file where requested */
	ulint		line)		/* in: line where requested */
{
	rw_lock_debug_t*	info;

	ut_ad(lock);
	ut_ad(file_name);

	info = rw_lock_debug_create();

	rw_lock_debug_mutex_enter();

	info->file_name = file_name;
	info->line 	= line;
	info->lock_type = lock_type;
	info->thread_id = os_thread_get_curr_id();
	info->pass	= pass;

	UT_LIST_ADD_FIRST(list, lock->debug_list, info);	

	rw_lock_debug_mutex_exit();

	if ((pass == 0) && (lock_type != RW_LOCK_WAIT_EX)) {
		sync_thread_add_level(lock, lock->level);
	}
}	

/**********************************************************************
Removes a debug information struct for an rw-lock. */
/*移除rw-lock的调试信息结构。*/
void
rw_lock_remove_debug_info(
/*======================*/
	rw_lock_t*	lock,		/* in: rw-lock */
	ulint		pass,		/* in: pass value */
	ulint		lock_type)	/* in: lock type */
{
	rw_lock_debug_t*	info;

	ut_ad(lock);

	if ((pass == 0) && (lock_type != RW_LOCK_WAIT_EX)) {
		sync_thread_reset_level(lock);
	}

	rw_lock_debug_mutex_enter();

	info = UT_LIST_GET_FIRST(lock->debug_list);

	while (info != NULL) {
		if ((pass == info->pass)
		    && ((pass != 0)
			|| (info->thread_id == os_thread_get_curr_id()))
		    && (info->lock_type == lock_type)) {

		    	/* Found! */
		    	UT_LIST_REMOVE(list, lock->debug_list, info);
			rw_lock_debug_mutex_exit();

		    	rw_lock_debug_free(info);

		    	return;
		}

		info = UT_LIST_GET_NEXT(list, info);
	}

	ut_error;
}

/**********************************************************************
Sets the rw-lock latching level field. */
/*设置rw-lock锁定级别字段。*/
void
rw_lock_set_level(
/*==============*/
	rw_lock_t*	lock,	/* in: rw-lock */
	ulint		level)	/* in: level */
{
	lock->level = level;
}

/**********************************************************************
Checks if the thread has locked the rw-lock in the specified mode, with
the pass value == 0. */
/*检查线程是否在指定的模式下锁定了rw-lock, pass值== 0。*/
ibool
rw_lock_own(
/*========*/
					/* out: TRUE if locked */
	rw_lock_t*	lock,		/* in: rw-lock */
	ulint		lock_type)	/* in: lock type */
{
	rw_lock_debug_t*	info;

	ut_ad(lock);
	ut_ad(rw_lock_validate(lock));

#ifndef UNIV_SYNC_DEBUG
	ut_error;
#endif	
	mutex_enter(&(lock->mutex));

	info = UT_LIST_GET_FIRST(lock->debug_list);

	while (info != NULL) {

		if ((info->thread_id == os_thread_get_curr_id())
		    && (info->pass == 0)
		    && (info->lock_type == lock_type)) {

			mutex_exit(&(lock->mutex));
		    	/* Found! */

		    	return(TRUE);
		}

		info = UT_LIST_GET_NEXT(list, info);
	}
	mutex_exit(&(lock->mutex));

	return(FALSE);
}

/**********************************************************************
Checks if somebody has locked the rw-lock in the specified mode. */
/*检查是否有人在指定模式下锁定了rw-lock。*/
ibool
rw_lock_is_locked(
/*==============*/
					/* out: TRUE if locked */
	rw_lock_t*	lock,		/* in: rw-lock */
	ulint		lock_type)	/* in: lock type: RW_LOCK_SHARED,
					RW_LOCK_EX */
{
	ibool	ret	= FALSE;

	ut_ad(lock);
	ut_ad(rw_lock_validate(lock));
	
	mutex_enter(&(lock->mutex));

	if (lock_type == RW_LOCK_SHARED) {
		if (lock->reader_count > 0) {
			ret = TRUE;
		}
	} else if (lock_type == RW_LOCK_EX) {
		if (lock->writer == RW_LOCK_EX) {
			ret = TRUE;
		}
	} else {
		ut_error;
	}

	mutex_exit(&(lock->mutex));

	return(ret);
}

/*******************************************************************
Prints debug info of currently locked rw-locks. */
/*打印当前锁定的rw-locks的调试信息。*/
void
rw_lock_list_print_info(void)
/*=========================*/
{
#ifndef UNIV_SYNC_DEBUG
#else
	rw_lock_t*	lock;
	ulint		count		= 0;
	rw_lock_debug_t* info;
	
	mutex_enter(&rw_lock_list_mutex);

	printf("-------------\n");
	printf("RW-LATCH INFO\n");
	printf("-------------\n");

	lock = UT_LIST_GET_FIRST(rw_lock_list);

	while (lock != NULL) {

		count++;

		mutex_enter(&(lock->mutex));

		if ((rw_lock_get_writer(lock) != RW_LOCK_NOT_LOCKED)
		    || (rw_lock_get_reader_count(lock) != 0)
		    || (rw_lock_get_waiters(lock) != 0)) {

			printf("RW-LOCK: %lx ", (ulint)lock);

			if (rw_lock_get_waiters(lock)) {
				printf(" Waiters for the lock exist\n");
			} else {
				printf("\n");
			}
		    
			info = UT_LIST_GET_FIRST(lock->debug_list);
			while (info != NULL) {	
				rw_lock_debug_print(info);
				info = UT_LIST_GET_NEXT(list, info);
			}
		}

		mutex_exit(&(lock->mutex));
		lock = UT_LIST_GET_NEXT(list, lock);
	}

	printf("Total number of rw-locks %ld\n", count);
	mutex_exit(&rw_lock_list_mutex);
#endif
}

/*******************************************************************
Prints debug info of an rw-lock. */
/*打印rw-lock的调试信息。*/
void
rw_lock_print(
/*==========*/
	rw_lock_t*	lock)	/* in: rw-lock */
{
#ifndef UNIV_SYNC_DEBUG
	printf(
	   "Sorry, cannot give rw-lock info in non-debug version!\n");
#else
	ulint		count		= 0;
	rw_lock_debug_t* info;
	
	printf("-------------\n");
	printf("RW-LATCH INFO\n");
	printf("RW-LATCH: %lx ", (ulint)lock);

	if ((rw_lock_get_writer(lock) != RW_LOCK_NOT_LOCKED)
	    || (rw_lock_get_reader_count(lock) != 0)
	    || (rw_lock_get_waiters(lock) != 0)) {

		if (rw_lock_get_waiters(lock)) {
			printf(" Waiters for the lock exist\n");
		} else {
			printf("\n");
		}
		    
		info = UT_LIST_GET_FIRST(lock->debug_list);
		while (info != NULL) {	
			rw_lock_debug_print(info);
			info = UT_LIST_GET_NEXT(list, info);
		}
	}
#endif
}

/*************************************************************************
Prints info of a debug struct. */
/*打印调试结构体的信息。*/
void
rw_lock_debug_print(
/*================*/
	rw_lock_debug_t*	info)	/* in: debug struct */
{
	ulint	rwt;

	rwt 	  = info->lock_type;	
			
	printf("Locked: thread %ld file %s line %ld  ",
		    	info->thread_id, info->file_name, info->line);
	if (rwt == RW_LOCK_SHARED) {
		printf("S-LOCK");
	} else if (rwt == RW_LOCK_EX) {
		printf("X-LOCK");
	} else if (rwt == RW_LOCK_WAIT_EX) {
		printf("WAIT X-LOCK");
	} else {
		ut_error;
	}
	if (info->pass != 0) {
		printf(" pass value %lu", info->pass);
	}
	printf("\n");
}

/*******************************************************************
Returns the number of currently locked rw-locks. Works only in the debug
version. */
/*返回当前锁定的rw-locks的数量。仅在调试版本中有效。*/
ulint
rw_lock_n_locked(void)
/*==================*/
{
#ifndef UNIV_SYNC_DEBUG
	printf(
	   "Sorry, cannot give rw-lock info in non-debug version!\n");
	ut_error;
	return(0);
#else
	rw_lock_t*	lock;
	ulint		count		= 0;
	
	mutex_enter(&rw_lock_list_mutex);

	lock = UT_LIST_GET_FIRST(rw_lock_list);

	while (lock != NULL) {
		mutex_enter(rw_lock_get_mutex(lock));

		if ((rw_lock_get_writer(lock) != RW_LOCK_NOT_LOCKED)
				|| (rw_lock_get_reader_count(lock) != 0)) {
			count++;
		}

		mutex_exit(rw_lock_get_mutex(lock));
		lock = UT_LIST_GET_NEXT(list, lock);
	}

	mutex_exit(&rw_lock_list_mutex);

	return(count);
#endif
}
