/******************************************************
The wait array used in synchronization primitives

(c) 1995 Innobase Oy

Created 9/5/1995 Heikki Tuuri
*******************************************************/
/*同步原语中使用的等待数组*/
#include "sync0arr.h"
#ifdef UNIV_NONINL
#include "sync0arr.ic"
#endif

#include "sync0sync.h"
#include "sync0rw.h"
#include "os0sync.h"
#include "srv0srv.h"

/*
			WAIT ARRAY  等待数组
			==========

The wait array consists of cells each of which has an
an operating system event object created for it. The threads
waiting for a mutex, for example, can reserve a cell
in the array and suspend themselves to wait for the event
to become signaled. When using the wait array, remember to make
sure that some thread holding the synchronization object
will eventually know that there is a waiter in the array and
signal the object, to prevent infinite wait.
Why we chose to implement a wait array? First, to make
mutexes fast, we had to code our own implementation of them,
which only in usually uncommon cases resorts to using
slow operating system primitives. Then we had the choice of
assigning a unique OS event for each mutex, which would
be simpler, or using a global wait array. In some operating systems,
the global wait array solution is more efficient and flexible,
because we can do with a very small number of OS events,
say 200. In NT 3.51, allocating events seems to be a quadratic
algorithm, because 10 000 events are created fast, but
100 000 events takes a couple of minutes to create.
等待数组由单元格组成，每个单元格都为其创建了一个操作系统事件对象。
例如，等待互斥锁的线程可以在数组中保留一个单元并挂起自己，以等待事件被通知。
当使用等待数组时，记得确保持有同步对象的某个线程最终会知道数组中有一个等待器，并向该对象发出信号，以防止无限等待。
为什么我们选择实现一个等待数组?首先，为了使互斥锁更快，我们必须编写自己的互斥锁实现代码，只有在不常见的情况下才会使用速度较慢的操作系统原语。
然后我们可以选择为每个互斥锁分配一个唯一的OS事件，这将会更简单，或者使用一个全局等待数组。
在一些操作系统中，全局等待数组解决方案更有效和灵活，因为我们可以处理非常少量的操作系统事件，比如200个。
在NT 3.51中，分配事件似乎是一种二次算法，因为可以快速创建10,000个事件，但创建100,000个事件需要几分钟。
*/

/* A cell where an individual thread may wait suspended
until a resource is released. The suspending is implemented
using an operating system event semaphore. */
/*单个线程可以挂起的单元，直到资源被释放。挂起是使用操作系统事件信号量来实现的。*/
struct sync_cell_struct {
        void*           wait_object;    /* pointer to the object the
                                        thread is waiting for; if NULL
                                        the cell is free for use */ /*线程正在等待的对象的指针;如果为NULL，则该单元格可以自由使用*/
	mutex_t*	old_wait_mutex;	/* the latest wait mutex in cell */ /*cell中最新的等待互斥锁*/
	rw_lock_t*	old_wait_rw_lock;/* the latest wait rw-lock in cell */ /*cell中最新的等待rw-lock*/
        ulint		request_type;	/* lock type requested on the
        				object */ /*对象上请求的锁类型*/
	char*		file;		/* in debug version file where
					requested */
	ulint		line;		/* in debug version line where
					requested */
	os_thread_id_t	thread;		/* thread id of this waiting
					thread */ /*等待线程的线程id*/
	ibool		waiting;	/* TRUE if the thread has already
					called sync_array_event_wait
					on this cell but not yet
					sync_array_free_cell (which
					actually resets wait_object and thus
					whole cell) */ /*如果线程已经在这个单元上调用了sync_array_event_wait，
					但还没有调用sync_array_free_cell(它实际上重置了wait_object，因此整个单元)，则为TRUE。*/
	ibool		event_set;	/* TRUE if the event is set */
        os_event_t 	event;   	/* operating system event
                                        semaphore handle */
	time_t		reservation_time;/* time when the thread reserved
					the wait cell */
};

/* NOTE: It is allowed for a thread to wait
for an event allocated for the array without owning the
protecting mutex (depending on the case: OS or database mutex), but
all changes (set or reset) to the state of the event must be made
while owning the mutex. */
/*注意:线程可以在不拥有保护互斥锁的情况下等待分配给数组的事件(这取决于操作系统或数据库的互斥锁)，
但是所有事件状态的改变(设置或重置)都必须在拥有互斥锁的情况下进行。*/
struct sync_array_struct {
	ulint		n_reserved;	/* number of currently reserved
					cells in the wait array */ /*等待数组中当前保留的单元格数*/
	ulint		n_cells;	/* number of cells in the
					wait array *//*等待数组中的单元格数*/
	sync_cell_t*	array;		/* pointer to wait array */
	ulint		protection;	/* this flag tells which
					mutex protects the data */ /*这个标志告诉哪个互斥锁保护数据*/
	mutex_t		mutex;		/* possible database mutex
					protecting this data structure */ /*可能数据库互斥锁保护这个数据结构*/
	os_mutex_t	os_mutex;	/* Possible operating system mutex
					protecting the data structure.
					As this data structure is used in
					constructing the database mutex,
					to prevent infinite recursion
					in implementation, we fall back to
					an OS mutex. */ /*可能的操作系统互斥锁保护数据结构。
					由于这个数据结构用于构造数据库互斥锁，为了防止实现中的无限递归，我们又回到了OS互斥锁。*/
	ulint		sg_count;	/* count of how many times an
					object has been signalled */ /*计数一个物体被发出信号的次数*/
	ulint		res_count;	/* count of cell reservations
					since creation of the array */ /*自数组创建以来的单元格预订计数*/
};

/**********************************************************************
This function is called only in the debug version. Detects a deadlock
of one or more threads because of waits of semaphores. */
/*这个函数只在调试版本中被调用。检测一个或多个线程因信号量等待而死锁。*/
static
ibool
sync_array_detect_deadlock(
/*=======================*/
				/* out: TRUE if deadlock detected */
        sync_array_t*	arr,	/* in: wait array; NOTE! the caller must
        			own the mutex to array */
	sync_cell_t*	start,	/* in: cell where recursive search started */
	sync_cell_t*	cell,	/* in: cell to search */
	ulint		depth);	/* in: recursion depth */

/*********************************************************************
Gets the nth cell in array. */
/*获取数组中的第n个单元格*/
static
sync_cell_t*
sync_array_get_nth_cell(
/*====================*/
				/* out: cell */
	sync_array_t*	arr,	/* in: sync array */
	ulint		n)	/* in: index */
{
	ut_a(arr);
	ut_a(n < arr->n_cells);

	return(arr->array + n);
}

/**********************************************************************
Reserves the mutex semaphore protecting a sync array. */
/*保留互斥信号量以保护同步数组。*/
static
void
sync_array_enter(
/*=============*/
	sync_array_t*	arr)	/* in: sync wait array */
{
	ulint	protection;

	protection = arr->protection;

	if (protection == SYNC_ARRAY_OS_MUTEX) {
		os_mutex_enter(arr->os_mutex);
	} else if (protection == SYNC_ARRAY_MUTEX) {
		mutex_enter(&(arr->mutex));
	} else {
		ut_error;
	}
}

/**********************************************************************
Releases the mutex semaphore protecting a sync array. */
/*释放保护同步数组的互斥信号量。*/
static
void
sync_array_exit(
/*============*/
	sync_array_t*	arr)	/* in: sync wait array */
{
	ulint	protection;

	protection = arr->protection;

	if (protection == SYNC_ARRAY_OS_MUTEX) {
		os_mutex_exit(arr->os_mutex);
	} else if (protection == SYNC_ARRAY_MUTEX) {
		mutex_exit(&(arr->mutex));
	} else {
		ut_error;
	}
}

/***********************************************************************
Creates a synchronization wait array. It is protected by a mutex
which is automatically reserved when the functions operating on it
are called. */
/*创建同步等待数组。它由互斥锁保护，当对它进行操作的函数被调用时，互斥锁会自动保留。*/
sync_array_t*
sync_array_create(
/*==============*/
				/* out, own: created wait array */
	ulint	n_cells,	/* in: number of cells in the array
				to create */
	ulint	protection)	/* in: either SYNC_ARRAY_OS_MUTEX or
				SYNC_ARRAY_MUTEX: determines the type
				of mutex protecting the data structure */
{
	sync_array_t*	arr;
	sync_cell_t*	cell_array;
	sync_cell_t*	cell;
	ulint		i;
	
	ut_a(n_cells > 0);

	/* Allocate memory for the data structures */
	arr = ut_malloc(sizeof(sync_array_t));

	cell_array = ut_malloc(sizeof(sync_cell_t) * n_cells);

	arr->n_cells = n_cells;
	arr->n_reserved = 0;
	arr->array = cell_array;
	arr->protection = protection;
	arr->sg_count = 0;
	arr->res_count = 0;

        /* Then create the mutex to protect the wait array complex */
	if (protection == SYNC_ARRAY_OS_MUTEX) {
		arr->os_mutex = os_mutex_create(NULL);
	} else if (protection == SYNC_ARRAY_MUTEX) {
		mutex_create(&(arr->mutex));
		mutex_set_level(&(arr->mutex), SYNC_NO_ORDER_CHECK);
	} else {
		ut_error;
	}

        for (i = 0; i < n_cells; i++) {
		cell = sync_array_get_nth_cell(arr, i);        	
                cell->wait_object = NULL;

                /* Create an operating system event semaphore with no name */
				/*创建一个没有名称的操作系统事件信号量*/
                cell->event = os_event_create(NULL);
		cell->event_set = FALSE; /* it is created in reset state */
	}

	return(arr);
}

/**********************************************************************
Frees the resources in a wait array. */
/*释放等待数组中的资源。*/
void
sync_array_free(
/*============*/
	sync_array_t*	arr)	/* in, own: sync wait array */
{
        ulint           i;
        sync_cell_t*   	cell;
	ulint		protection;

        ut_a(arr->n_reserved == 0);
        
	sync_array_validate(arr);
        
        for (i = 0; i < arr->n_cells; i++) {
		cell = sync_array_get_nth_cell(arr, i);        	
		os_event_free(cell->event);
        }

	protection = arr->protection;

        /* Release the mutex protecting the wait array complex */
        /* 释放保护等待数组的互斥锁*/
	if (protection == SYNC_ARRAY_OS_MUTEX) {
		os_mutex_free(arr->os_mutex);
	} else if (protection == SYNC_ARRAY_MUTEX) {
		mutex_free(&(arr->mutex));
	} else {
		ut_error;
	}

	ut_free(arr->array);
	ut_free(arr);	
}

/************************************************************************
Validates the integrity of the wait array. Checks
that the number of reserved cells equals the count variable. */
/*验证等待数组的完整性。检查保留的单元格数是否等于count变量。*/
void
sync_array_validate(
/*================*/
	sync_array_t*	arr)	/* in: sync wait array */
{
        ulint           i;
        sync_cell_t*   	cell;
        ulint           count           = 0;
        
        sync_array_enter(arr);

        for (i = 0; i < arr->n_cells; i++) {
		cell = sync_array_get_nth_cell(arr, i);        	

                if (cell->wait_object != NULL) {
                        count++;
                }
        }

        ut_a(count == arr->n_reserved);

        sync_array_exit(arr);
}

/***********************************************************************
Puts the cell event in set state. */
/*将单元格事件置于设置状态。*/
static
void
sync_cell_event_set(
/*================*/
	sync_cell_t*	cell)	/* in: array cell */
{
	os_event_set(cell->event);
	cell->event_set = TRUE;
}		

/***********************************************************************
Puts the cell event in reset state. */
/*将单元格事件置于重置状态。*/
static
void
sync_cell_event_reset(
/*==================*/
	sync_cell_t*	cell)	/* in: array cell */
{
	os_event_reset(cell->event);
	cell->event_set = FALSE;
}		

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
        ulint*   	index)  /* out: index of the reserved cell */
{
        sync_cell_t*   	cell;
        ulint           i;
        
        ut_a(object);
        ut_a(index);

        sync_array_enter(arr);

        arr->res_count++;

	/* Reserve a new cell. */
        for (i = 0; i < arr->n_cells; i++) {
		cell = sync_array_get_nth_cell(arr, i);        	

                if (cell->wait_object == NULL) {

                        /* Make sure the event is reset */
			if (cell->event_set) {	
                        	sync_cell_event_reset(cell);
			}

			cell->reservation_time = time(NULL);
			cell->thread = os_thread_get_curr_id();

			cell->wait_object = object;

			if (type == SYNC_MUTEX) {
				cell->old_wait_mutex = object;
			} else {
				cell->old_wait_rw_lock = object;
			}
				
			cell->request_type = type;
			cell->waiting = FALSE;
			
			cell->file = file;
			cell->line = line;
			
			arr->n_reserved++;

			*index = i;

			sync_array_exit(arr);
                        
                        return;
                }
        }

        ut_error; /* No free cell found */

	return;
}

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
        ulint   	index)  /* in: index of the reserved cell */
{
        sync_cell_t*   	cell;
        os_event_t 	event;
        
        ut_a(arr);

        sync_array_enter(arr);

	cell = sync_array_get_nth_cell(arr, index);        	

	ut_a(cell->wait_object);
	ut_a(!cell->waiting);
	ut_ad(os_thread_get_curr_id() == cell->thread);

       	event = cell->event;
       	cell->waiting = TRUE;

#ifdef UNIV_SYNC_DEBUG
       	
	/* We use simple enter to the mutex below, because if
	we cannot acquire it at once, mutex_enter would call
	recursively sync_array routines, leading to trouble.
	rw_lock_debug_mutex freezes the debug lists. */
    /*我们对下面的互斥量使用简单的enter，因为如果我们不能立即获取它，mutex_enter会递归地调用sync_array例程，从而导致麻烦。rw_lock_debug_mutex冻结调试列表。*/
	rw_lock_debug_mutex_enter();

	if (TRUE == sync_array_detect_deadlock(arr, cell, cell, 0)) {

		printf("########################################\n");
		ut_error;
	}		

	rw_lock_debug_mutex_exit();
#endif
        sync_array_exit(arr);

        os_event_wait(event);

        sync_array_free_cell(arr, index);
}

/**********************************************************************
Reports info of a wait array cell. */
/*报告等待数组单元格的信息。*/
static
void
sync_array_cell_print(
/*==================*/
	FILE*		file,	/* in: file where to print */
	sync_cell_t*	cell)	/* in: sync cell */
{
	mutex_t*	mutex;
	rw_lock_t*	rwlock;
	char*		str	 = NULL;
	ulint		type;

	type = cell->request_type;

	fprintf(file,
"--Thread %lu has waited at %s line %lu for %.2f seconds the semaphore:\n",
			(ulint)cell->thread, cell->file, cell->line,
			difftime(time(NULL), cell->reservation_time));

	if (type == SYNC_MUTEX) {
		/* We use old_wait_mutex in case the cell has already
		been freed meanwhile */
		mutex = cell->old_wait_mutex;

		fprintf(file,
		"Mutex at %lx created file %s line %lu, lock var %lu\n",
			(ulint)mutex, mutex->cfile_name, mutex->cline,
							mutex->lock_word);
		fprintf(file,
		"Last time reserved in file %s line %lu, waiters flag %lu\n",
			mutex->file_name, mutex->line, mutex->waiters);

	} else if (type == RW_LOCK_EX || type == RW_LOCK_SHARED) {

		if (type == RW_LOCK_EX) {
			fprintf(file, "X-lock on");
		} else {
			fprintf(file, "S-lock on");
		}

		rwlock = cell->old_wait_rw_lock;

		fprintf(file, " RW-latch at %lx created in file %s line %lu\n",
			(ulint)rwlock, rwlock->cfile_name, rwlock->cline);
		if (rwlock->writer != RW_LOCK_NOT_LOCKED) {
			fprintf(file,
			"a writer (thread id %lu) has reserved it in mode",
					(ulint)rwlock->writer_thread);
			if (rwlock->writer == RW_LOCK_EX) {
				fprintf(file, " exclusive\n");
			} else {
				fprintf(file, " wait exclusive\n");
 			}
		}
		
		fprintf(file, "number of readers %lu, waiters flag %lu\n",
				rwlock->reader_count, rwlock->waiters);
	
		fprintf(file, "Last time read locked in file %s line %lu\n",
			rwlock->last_s_file_name, rwlock->last_s_line);
		fprintf(file, "Last time write locked in file %s line %lu\n",
			rwlock->last_x_file_name, rwlock->last_x_line);
	} else {
		ut_error;
	}

        if (!cell->waiting) {
          	fprintf(file, "wait has ended\n");
	}

        if (cell->event_set) {
             	fprintf(file, "wait is ending\n");
	}
}

/**********************************************************************
Looks for a cell with the given thread id. */
/*查找具有给定线程id的单元格。*/
static
sync_cell_t*
sync_array_find_thread(
/*===================*/
				/* out: pointer to cell or NULL
				if not found */
        sync_array_t*	arr,	/* in: wait array */
	os_thread_id_t	thread)	/* in: thread id */
{
        ulint           i;
        sync_cell_t*   	cell;

        for (i = 0; i < arr->n_cells; i++) {

		cell = sync_array_get_nth_cell(arr, i);        	

                if ((cell->wait_object != NULL)
		    && (cell->thread == thread)) {

		    	return(cell);	/* Found */
                }
        }

	return(NULL);	/* Not found */
}

/**********************************************************************
Recursion step for deadlock detection. */
/*用于死锁检测的递归步骤。*/
static
ibool
sync_array_deadlock_step(
/*=====================*/
				/* out: TRUE if deadlock detected */
        sync_array_t*	arr,	/* in: wait array; NOTE! the caller must
        			own the mutex to array */
	sync_cell_t*	start,	/* in: cell where recursive search
				started */
	os_thread_id_t	thread,	/* in: thread to look at */
	ulint		pass,	/* in: pass value */
	ulint		depth)	/* in: recursion depth */
{
	sync_cell_t*	new;
	ibool		ret;

	depth++;

	if (pass != 0) {
		/* If pass != 0, then we do not know which threads are
		responsible of releasing the lock, and no deadlock can
		be detected. */

		return(FALSE);
	}
			    	
	new = sync_array_find_thread(arr, thread);

	if (new == start) {
		/* Stop running of other threads */

		ut_dbg_stop_threads = TRUE;

		/* Deadlock */
		printf("########################################\n");
		printf("DEADLOCK of threads detected!\n");

		return(TRUE);

	} else if (new) {
		ret = sync_array_detect_deadlock(arr, start, new, depth);

		if (ret) {
			return(TRUE);
		}
	}
	return(FALSE);
}

/**********************************************************************
This function is called only in the debug version. Detects a deadlock
of one or more threads because of waits of semaphores. */
/*这个函数只在调试版本中被调用。检测一个或多个线程因信号量等待而死锁。*/
static
ibool
sync_array_detect_deadlock(
/*=======================*/
				/* out: TRUE if deadlock detected */
        sync_array_t*	arr,	/* in: wait array; NOTE! the caller must
        			own the mutex to array */
	sync_cell_t*	start,	/* in: cell where recursive search started */
	sync_cell_t*	cell,	/* in: cell to search */
	ulint		depth)	/* in: recursion depth */
{
	mutex_t*	mutex;
	rw_lock_t*	lock;
	os_thread_id_t	thread;
	ibool		ret;
	rw_lock_debug_t* debug;
	
        ut_a(arr && start && cell);
	ut_ad(cell->wait_object);
	ut_ad(os_thread_get_curr_id() == start->thread);
	ut_ad(depth < 100);
	
	depth++;
	
	if (cell->event_set || !cell->waiting) {

		return(FALSE); /* No deadlock here */
	}
	
	if (cell->request_type == SYNC_MUTEX) {

		mutex = cell->wait_object;

		if (mutex_get_lock_word(mutex) != 0) {

			thread = mutex->thread_id;

			/* Note that mutex->thread_id above may be
			also OS_THREAD_ID_UNDEFINED, because the
			thread which held the mutex maybe has not
			yet updated the value, or it has already
			released the mutex: in this case no deadlock
			can occur, as the wait array cannot contain
			a thread with ID_UNDEFINED value. */

			ret = sync_array_deadlock_step(arr, start, thread, 0,
								depth);
			if (ret) {
				printf(
			"Mutex %lx owned by thread %lu file %s line %lu\n",
					(ulint)mutex, mutex->thread_id,
					mutex->file_name, mutex->line);
				sync_array_cell_print(stdout, cell);
				return(TRUE);
			}
		}

		return(FALSE); /* No deadlock */

	} else if (cell->request_type == RW_LOCK_EX) {

	    lock = cell->wait_object;

	    debug = UT_LIST_GET_FIRST(lock->debug_list);

	    while (debug != NULL) {

		thread = debug->thread_id;

		if (((debug->lock_type == RW_LOCK_EX)
	             && (thread != cell->thread))
	            || ((debug->lock_type == RW_LOCK_WAIT_EX)
			&& (thread != cell->thread))
	            || (debug->lock_type == RW_LOCK_SHARED)) {

			/* The (wait) x-lock request can block infinitely
			only if someone (can be also cell thread) is holding
			s-lock, or someone (cannot be cell thread) (wait)
			x-lock, and he is blocked by start thread */

			ret = sync_array_deadlock_step(arr, start, thread,
							debug->pass,
							depth);
			if (ret) {
				printf("rw-lock %lx ", (ulint) lock);
				rw_lock_debug_print(debug);
				sync_array_cell_print(stdout, cell);

				return(TRUE);
			}
		}

		debug = UT_LIST_GET_NEXT(list, debug);
	    }

	    return(FALSE);

	} else if (cell->request_type == RW_LOCK_SHARED) {

	    lock = cell->wait_object;
	    debug = UT_LIST_GET_FIRST(lock->debug_list);

	    while (debug != NULL) {

		thread = debug->thread_id;

		if ((debug->lock_type == RW_LOCK_EX)
	            || (debug->lock_type == RW_LOCK_WAIT_EX)) {

			/* The s-lock request can block infinitely only if
			someone (can also be cell thread) is holding (wait)
			x-lock, and he is blocked by start thread */

			ret = sync_array_deadlock_step(arr, start, thread,
							debug->pass,
							depth);
			if (ret) {
				printf("rw-lock %lx ", (ulint) lock);
				rw_lock_debug_print(debug);
				sync_array_cell_print(stdout, cell);

				return(TRUE);
			}
		}

		debug = UT_LIST_GET_NEXT(list, debug);
	    }

	    return(FALSE);

	} else {
		ut_error;
	}

	return(TRUE); 	/* Execution never reaches this line: for compiler
			fooling only */
}

/**********************************************************************
Determines if we can wake up the thread waiting for a sempahore. */
/*确定是否可以唤醒等待信号量的线程。*/
static
ibool
sync_arr_cell_can_wake_up(
/*======================*/
	sync_cell_t*	cell)	/* in: cell to search */
{
	mutex_t*	mutex;
	rw_lock_t*	lock;
	
	if (cell->request_type == SYNC_MUTEX) {

		mutex = cell->wait_object;

		if (mutex_get_lock_word(mutex) == 0) {

			return(TRUE);
		}

	} else if (cell->request_type == RW_LOCK_EX) {

	    	lock = cell->wait_object;

	    	if (rw_lock_get_reader_count(lock) == 0
		    && rw_lock_get_writer(lock) == RW_LOCK_NOT_LOCKED) {

			return(TRUE);
		}

	    	if (rw_lock_get_reader_count(lock) == 0
		    && rw_lock_get_writer(lock) == RW_LOCK_WAIT_EX
		    && lock->writer_thread == cell->thread) {

			return(TRUE);
		}

	} else if (cell->request_type == RW_LOCK_SHARED) {
	    	lock = cell->wait_object;

		if (rw_lock_get_writer(lock) == RW_LOCK_NOT_LOCKED) {
		
			return(TRUE);
		}
	}

	return(FALSE);
}

/**********************************************************************
Frees the cell. NOTE! sync_array_wait_event frees the cell
automatically! */
/*释放单元格。注意!sync_array_wait_event自动释放单元格!*/
void
sync_array_free_cell(
/*=================*/
	sync_array_t*	arr,	/* in: wait array */
        ulint    	index)  /* in: index of the cell in array */
{
        sync_cell_t*   	cell;
        
        sync_array_enter(arr);

        cell = sync_array_get_nth_cell(arr, index);

        ut_a(cell->wait_object != NULL);

	cell->wait_object =  NULL;

	ut_a(arr->n_reserved > 0);
	arr->n_reserved--;

        sync_array_exit(arr);
}

/**************************************************************************
Looks for the cells in the wait array which refer to the wait object
specified, and sets their corresponding events to the signaled state. In this
way releases the threads waiting for the object to contend for the object.
It is possible that no such cell is found, in which case does nothing. */
/*在等待数组中查找引用指定的等待对象的单元格，并将它们对应的事件设置为有信号的状态。
通过这种方式释放等待对象竞争对象的线程。有可能没有找到这样的单元，在这种情况下，什么也不能做。*/
void
sync_array_signal_object(
/*=====================*/
	sync_array_t*	arr,	/* in: wait array */
	void*		object)	/* in: wait object */
{
        sync_cell_t*   	cell;
        ulint           count;
        ulint           i;

        sync_array_enter(arr);

	arr->sg_count++;

	i = 0;
	count = 0;

        while (count < arr->n_reserved) {

        	cell = sync_array_get_nth_cell(arr, i);

                if (cell->wait_object != NULL) {

                        count++;
                        if (cell->wait_object == object) {

                        	sync_cell_event_set(cell);
                        }
                }

                i++;
        }

        sync_array_exit(arr);
}

/**************************************************************************
If the wakeup algorithm does not work perfectly at semaphore relases,
this function will do the waking (see the comment in mutex_exit). This
function should be called about every 1 second in the server. */
/*如果唤醒算法在释放信号量时不能完美地工作，这个函数将执行唤醒(参见mutex_exit中的注释)。这个函数应该在服务器上每1秒调用一次。*/
void
sync_arr_wake_threads_if_sema_free(void)
/*====================================*/
{
	sync_array_t*	arr 	= sync_primary_wait_array;
        sync_cell_t*   	cell;
        ulint           count;
        ulint           i;

        sync_array_enter(arr);

	i = 0;
	count = 0;

        while (count < arr->n_reserved) {

        	cell = sync_array_get_nth_cell(arr, i);

                if (cell->wait_object != NULL) {

                        count++;

                        if (sync_arr_cell_can_wake_up(cell)) {

                        	sync_cell_event_set(cell);
                        }
                }

                i++;
        }

        sync_array_exit(arr);
}

/**************************************************************************
Prints warnings of long semaphore waits to stderr. Currently > 120 sec. */
/*向标准错误输出长信号量等待的警告。目前> 120秒。*/
void
sync_array_print_long_waits(void)
/*=============================*/
{
        sync_cell_t*   	cell;
        ibool		old_val;
	ibool		noticed = FALSE;
	ulint           i;

        for (i = 0; i < sync_primary_wait_array->n_cells; i++) {

        	cell = sync_array_get_nth_cell(sync_primary_wait_array, i);

                if (cell->wait_object != NULL
		    && difftime(time(NULL), cell->reservation_time) > 240) {

		    	fprintf(stderr,
				"InnoDB: Warning: a long semaphore wait:\n");
			sync_array_cell_print(stderr, cell);

			noticed = TRUE;
                }

                if (cell->wait_object != NULL
		    && difftime(time(NULL), cell->reservation_time) > 420) {

		    	fprintf(stderr,
"InnoDB: Error: semaphore wait has lasted > 420 seconds\n"
"InnoDB: We intentionally crash the server, because it appears to be hung.\n"
		    	);

		    	ut_a(0);
                }
       	}

	if (noticed) {
		fprintf(stderr,
"InnoDB: ###### Starts InnoDB Monitor for 30 secs to print diagnostic info:\n");
        	old_val = srv_print_innodb_monitor;

        	srv_print_innodb_monitor = TRUE;
		os_event_set(srv_lock_timeout_thread_event);

        	os_thread_sleep(30000000);

        	srv_print_innodb_monitor = old_val;
		fprintf(stderr,
"InnoDB: ###### Diagnostic info printed to the standard output\n");
	}
}

/**************************************************************************
Prints info of the wait array. */
/*打印等待数组的信息。*/
static
void
sync_array_output_info(
/*===================*/
	sync_array_t*	arr)	/* in: wait array; NOTE! caller must own the
				mutex */
{
        sync_cell_t*   	cell;
        ulint           count;
	ulint           i;

	printf("OS WAIT ARRAY INFO: reservation count %ld, signal count %ld\n",
						arr->res_count, arr->sg_count);
	i = 0;
	count = 0;

        while (count < arr->n_reserved) {

        	cell = sync_array_get_nth_cell(arr, i);

                if (cell->wait_object != NULL) {
                        count++;
			sync_array_cell_print(stdout, cell);
                }

                i++;
       	}
}

/**************************************************************************
Prints info of the wait array. */
/*打印等待数组的信息。*/
void
sync_array_print_info(
/*==================*/
	sync_array_t*	arr)	/* in: wait array */
{
        sync_array_enter(arr);

	sync_array_output_info(arr);
        
        sync_array_exit(arr);
}
