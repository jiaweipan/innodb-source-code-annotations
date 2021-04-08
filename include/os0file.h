/******************************************************
The interface to the operating system file io

(c) 1995 Innobase Oy

Created 10/21/1995 Heikki Tuuri
*******************************************************/
/*操作系统文件io接口*/
#ifndef os0file_h
#define os0file_h

#include "univ.i"

#ifdef __WIN__

/* We define always WIN_ASYNC_IO, and check at run-time whether
   the OS actually supports it: Win 95 does not, NT does. */
/*我们定义了always WIN_ASYNC_IO，并在运行时检查操作系统是否支持它：win95不支持，NT支持。*/
#define WIN_ASYNC_IO

#define UNIV_NON_BUFFERED_IO

#else

#if defined(HAVE_AIO_H) && defined(HAVE_LIBRT)
#define POSIX_ASYNC_IO
#endif

#endif

#ifdef __WIN__
#define os_file_t	HANDLE
#else
typedef int	os_file_t;
#endif

/* If this flag is TRUE, then we will use the native aio of the
OS (provided we compiled Innobase with it in), otherwise we will
use simulated aio we build below with threads */
/*如果这个标志为真，那么我们将使用操作系统的本机aio（前提是我们用它编译了Innobase），否则我们将使用下面用线程构建的模拟aio*/
extern ibool	os_aio_use_native_aio;

#define OS_FILE_SECTOR_SIZE		512

/* The next value should be smaller or equal to the smallest sector size used
on any disk. A log block is required to be a portion of disk which is written
so that if the start and the end of a block get written to disk, then the
whole block gets written. This should be true even in most cases of a crash:
if this fails for a log block, then it is equivalent to a media failure in the
log. */
/*下一个值应小于或等于任何磁盘上使用的最小扇区大小。
日志块必须是磁盘中被写入的部分，这样，如果一个块的开始和结束被写入磁盘，那么整个块就会被写入。
即使在大多数崩溃情况下也是如此：如果日志块失败，则相当于日志中的媒体失败。*/
#define OS_FILE_LOG_BLOCK_SIZE		512

/* Options for file_create */
/* 文件创建选项*/
#define	OS_FILE_OPEN			51
#define	OS_FILE_CREATE			52
#define OS_FILE_OVERWRITE		53

/* Options for file_create */
/* 文件创建选项*/
#define	OS_FILE_AIO			61
#define	OS_FILE_NORMAL			62

/* Types for file create */
/* 文件创建类型*/
#define	OS_DATA_FILE			100
#define OS_LOG_FILE			101

/* Error codes from os_file_get_last_error */
/* 来自os_file_get_last_error的错误码*/
#define	OS_FILE_NOT_FOUND		71
#define	OS_FILE_DISK_FULL		72
#define	OS_FILE_ALREADY_EXISTS		73
#define OS_FILE_AIO_RESOURCES_RESERVED	74	/* wait for OS aio resources
						to become available again */ /*等待OS aio资源再次可用*/
#define	OS_FILE_ERROR_NOT_SPECIFIED	75

/* Types for aio operations */
/* 异步IO操作类型*/
#define OS_FILE_READ	10
#define OS_FILE_WRITE	11

#define OS_FILE_LOG	256	/* This can be ORed to type */

#define OS_AIO_N_PENDING_IOS_PER_THREAD 32	/* Win NT does not allow more
						than 64 */ /*Win NT不允许超过64*/

/* Modes for aio operations */
#define OS_AIO_NORMAL	21	/* Normal asynchronous i/o not for ibuf
				pages or ibuf bitmap pages */ /*正常异步i/o不适用于ibuf页或ibuf位图页 */
#define OS_AIO_IBUF	22	/* Asynchronous i/o for ibuf pages or ibuf
				bitmap pages */ /*ibuf页或ibuf位图页的异步i/o*/
#define OS_AIO_LOG  	23	/* Asynchronous i/o for the log */ /*日志的异步i/o*/
#define OS_AIO_SYNC	24	/* Asynchronous i/o where the calling thread
				will itself wait for the i/o to complete,
				doing also the job of the i/o-handler thread;
				can be used for any pages, ibuf or non-ibuf.
				This is used to save CPU time, as we can do
				with fewer thread switches. Plain synchronous
				i/o is not as good, because it must serialize
				the file seek and read or write, causing a
				bottleneck for parallelism. */
				/*异步i/o，其中调用线程本身将等待i/o完成，同时执行i/o处理程序线程的工作；
				可用于任何页面，ibuf or non-ibuf。
				这个是用来节省CPU时间的，因为我们可以用更少的线程开关。
				纯同步i/o不如以前好，因为它必须序列化文件搜索和读写，从而导致并行性瓶颈。*/

#define OS_AIO_SIMULATED_WAKE_LATER	512 /* This can be ORed to mode
				in the call of os_aio(...),
				if the caller wants to post several i/o
				requests in a batch, and only after that
 				wake the i/o-handler thread; this has
				effect only in simulated aio */ 
				/*如果调用者希望在一个批处理中发布几个i/o请求，
				并且只有在这之后才唤醒i/o处理程序线程，
				那么在调用os_aio（…）时，这可以被OR为mode；这只在模拟的aio中有效*/
#define OS_WIN31     1
#define OS_WIN95     2	
#define OS_WINNT     3

extern ulint	os_n_file_reads;
extern ulint	os_n_file_writes;
extern ulint	os_n_fsyncs;

/***************************************************************************
Gets the operating system version. Currently works only on Windows. */

ulint
os_get_os_version(void);
/*===================*/
                  /* out: OS_WIN95, OS_WIN31, OS_WINNT (2000 == NT) */
/********************************************************************
Opens an existing file or creates a new. */
/*打开现有文件或创建新文件。*/
os_file_t
os_file_create(
/*===========*/
			/* out, own: handle to the file, not defined if error,
			error number can be retrieved with os_get_last_error */
			/*文件句柄，未定义如果有错误，可以用 os_get_last_error检索错误号*/
	char*	name,	/* in: name of the file or path as a null-terminated
			string */ /* 文件或路径的名称，以null结尾的字符串 */
	ulint	create_mode,/* in: OS_FILE_OPEN if an existing file is opened
			(if does not exist, error), or OS_FILE_CREATE if a new
			file is created (if exists, error), OS_FILE_OVERWRITE
			if a new file is created or an old overwritten */
	ulint	purpose,/* in: OS_FILE_AIO, if asynchronous, non-buffered i/o
			is desired, OS_FILE_NORMAL, if any normal file */
	ulint	type,	/* in: OS_DATA_FILE or OS_LOG_FILE */
	ibool*	success);/* out: TRUE if succeed, FALSE if error */
/***************************************************************************
Closes a file handle. In case of error, error number can be retrieved with
os_file_get_last_error. */
/*关闭文件句柄。如果出现错误，可以使用os_file_get_last_error检索错误号。 */
ibool
os_file_close(
/*==========*/
				/* out: TRUE if success */
	os_file_t	file);	/* in, own: handle to a file */
/***************************************************************************
Gets a file size. */
/*获取文件大小。*/
ibool
os_file_get_size(
/*=============*/
				/* out: TRUE if success */
	os_file_t	file,	/* in: handle to a file */
	ulint*		size,	/* out: least significant 32 bits of file
				size */ /*文件大小的最低有效32位*/
	ulint*		size_high);/* out: 最大有效32位大小 */
/***************************************************************************
Sets a file size. This function can be used to extend or truncate a file. */
/*设置文件大小。此函数可用于扩展或截断文件。*/
ibool
os_file_set_size(
/*=============*/
				/* out: TRUE if success */
	char*		name,	/* in: name of the file or path as a
				null-terminated string */
	os_file_t	file,	/* in: handle to a file */
	ulint		size,	/* in: least significant 32 bits of file
				size */
	ulint		size_high);/* in: most significant 32 bits of size */
/***************************************************************************
Flushes the write buffers of a given file to the disk. */
/*将给定文件的写入缓冲区刷新到磁盘。 */
ibool
os_file_flush(
/*==========*/
				/* out: TRUE if success */
	os_file_t	file);	/* in, own: handle to a file */
/***************************************************************************
Retrieves the last error number if an error occurs in a file io function.
The number should be retrieved before any other OS calls (because they may
overwrite the error number). If the number is not known to this program,
the OS error number + 100 is returned. */
/*如果文件io中发生错误，则检索最后一个错误号功能应该在任何其他操作系统调用之前检索号码（因为它们可能会覆盖错误号码）。
如果这个程序不知道这个号码，返回操作系统错误号+100。*/
ulint
os_file_get_last_error(void);
/*========================*/
		/* out: error number, or OS error number + 100 */
/***********************************************************************
Requests a synchronous read operation. */
/*请求同步读取操作。*/
ibool
os_file_read(
/*=========*/
				/* out: TRUE if request was
				successful, FALSE if fail */
	os_file_t	file,	/* in: handle to a file */
	void*		buf,	/* in: buffer where to read */
	ulint		offset,	/* in: least significant 32 bits of file
				offset where to read */
	ulint		offset_high,/* in: most significant 32 bits of
				offset */
	ulint		n);	/* in: number of bytes to read */	
/***********************************************************************
Requests a synchronous write operation. */
/*请求同步写入操作。*/
ibool
os_file_write(
/*==========*/
				/* out: TRUE if request was
				successful, FALSE if fail */
	char*		name,	/* in: name of the file or path as a
				null-terminated string */
	os_file_t	file,	/* in: handle to a file */
	void*		buf,	/* in: buffer from which to write */
	ulint		offset,	/* in: least significant 32 bits of file
				offset where to write */
	ulint		offset_high,/* in: most significant 32 bits of
				offset */
	ulint		n);	/* in: number of bytes to write */	
/****************************************************************************
Initializes the asynchronous io system. Creates separate aio array for
non-ibuf read and write, a third aio array for the ibuf i/o, with just one
segment, two aio arrays for log reads and writes with one segment, and a
synchronous aio array of the specified size. The combined number of segments
in the three first aio arrays is the parameter n_segments given to the
function. The caller must create an i/o handler thread for each segment in
the four first arrays, but not for the sync aio array. */
/*初始化异步io系统。为non-ibuf 读写创建单独的aio数组，
为ibuf i/o创建第三个aio数组（只有一个段），
为日志读写创建两个aio数组（一个段），以及指定大小的同步aio数组。
前三个aio数组中的段的组合数是给定给函数的参数n_segments。
调用者必须为前四个数组中的每个段创建一个i/o处理程序线程，但不能为sync aio数组创建。*/
void
os_aio_init(
/*========*/
	ulint	n,		/* in: maximum number of pending aio operations
				allowed; n must be divisible by n_segments */ /*允许的挂起aio操作的最大数目；n必须可被n_segments整除*/
	ulint	n_segments,	/* in: combined number of segments in the four
				first aio arrays; must be >= 4 */ /*前四个aio数组中的段的组合数；必须大于等于4*/
	ulint	n_slots_sync);	/* in: number of slots in the sync aio array */ /*同步aio数组中的插槽数*/
/***********************************************************************
Requests an asynchronous i/o operation. */
/*请求异步i/o操作。 */
ibool
os_aio(
/*===*/
				/* out: TRUE if request was queued
				successfully, FALSE if fail */
	ulint		type,	/* in: OS_FILE_READ or OS_FILE_WRITE */
	ulint		mode,	/* in: OS_AIO_NORMAL, ..., possibly ORed
				to OS_AIO_SIMULATED_WAKE_LATER: the
				last flag advises this function not to wake
				i/o-handler threads, but the caller will
				do the waking explicitly later, in this
				way the caller can post several requests in
				a batch; NOTE that the batch must not be
				so big that it exhausts the slots in aio
				arrays! NOTE that a simulated batch
				may introduce hidden chances of deadlocks,
				because i/os are not actually handled until
				all have been posted: use with great
				caution! */
				/*OS_AIO_NORMAL，…，可能 ORed to OS_AIO_SIMULATED_WAKE_LATER：
				最后一个标志建议此函数不要唤醒i/o处理程序线程，但调用方稍后将显式地进行唤醒，
				这样调用方可以在一个批中发布多个请求；请注意，批不能太大，以至于耗尽AIO数组中的插槽！
				注意，一个模拟的批处理可能会引入隐藏的死锁机会，因为直到所有的i/o都被发布之后才真正被处理：请谨慎使用！*/
	char*		name,	/* in: name of the file or path as a
				null-terminated string */
				/*文件或路径的名称，以null结尾的字符串*/
	os_file_t	file,	/* in: handle to a file */
	void*		buf,	/* in: buffer where to read or from which
				to write */
	ulint		offset,	/* in: least significant 32 bits of file
				offset where to read or write */
	ulint		offset_high, /* in: most significant 32 bits of
				offset */
	ulint		n,	/* in: number of bytes to read or write */	
	void*		message1,/* in: messages for the aio handler (these
				can be used to identify a completed aio
				operation); if mode is OS_AIO_SYNC, these
				are ignored */
				/*aio处理程序的消息（可用于标识已完成的aio操作）；如果模式为OS_AIO_SYNC，则忽略这些消息 */
	void*		message2);
/****************************************************************************
Waits until there are no pending writes in os_aio_write_array. There can
be other, synchronous, pending writes. */
/*等待直到os_aio_write_array中没有挂起的写入。可以有其他的、同步的、挂起的写入。*/
void
os_aio_wait_until_no_pending_writes(void);
/*=====================================*/
/**************************************************************************
Wakes up simulated aio i/o-handler threads if they have something to do. */
/*如果模拟的aio i/o-handler 线程有事要做，则唤醒它们。*/
void
os_aio_simulated_wake_handler_threads(void);
/*=======================================*/

#ifdef WIN_ASYNC_IO
/**************************************************************************
This function is only used in Windows asynchronous i/o.
Waits for an aio operation to complete. This function is used to wait the
for completed requests. The aio array of pending requests is divided
into segments. The thread specifies which segment or slot it wants to wait
for. NOTE: this function will also take care of freeing the aio slot,
therefore no other thread is allowed to do the freeing! */
/*此函数仅用于Windows异步i/o。
等待aio操作完成。此函数用于等待已完成的请求。未决请求的aio数组被划分为多个段。
线程指定要等待的段或槽。注意：此函数还将负责释放aio插槽，因此不允许其他线程进行释放！*/
ibool
os_aio_windows_handle(
/*==================*/
				/* out: TRUE if the aio operation succeeded */
	ulint	segment,	/* in: the number of the segment in the aio
				arrays to wait for; segment 0 is the ibuf
				i/o thread, segment 1 the log i/o thread,
				then follow the non-ibuf read threads, and as
				the last are the non-ibuf write threads; if
				this is ULINT_UNDEFINED, then it means that
				sync aio is used, and this parameter is
				ignored */
				/*aio数组中要等待的段数；
				段0是ibuf i/o线程，段1是日志i/o线程，然后跟随非ibuf读线程，最后是非ibuf写线程；
				如果未定义此值，则表示使用了同步aio，并且忽略此参数*/
	ulint	pos,		/* this parameter is used only in sync aio:
				wait for the aio slot at this position */  
				/*他的参数仅用于同步aio：在此位置等待aio插槽*/
	void**	message1,	/* out: the messages passed with the aio
				request; note that also in the case where
				the aio operation failed, these output
				parameters are valid and can be used to
				restart the operation, for example */
				/*与aio请求一起传递的消息；
				请注意，在aio操作失败的情况下，这些输出参数也是有效的，
				例如，可以用于重新启动操作 */
	void**	message2,
	ulint*	type);		/* out: OS_FILE_WRITE or ..._READ */
#endif
#ifdef POSIX_ASYNC_IO
/**************************************************************************
This function is only used in Posix asynchronous i/o. Waits for an aio
operation to complete. */
/*此函数仅用于Posix异步i/o。等待aio要完成的操作。*/
ibool
os_aio_posix_handle(
/*================*/
				/* out: TRUE if the aio operation succeeded */
	ulint	array_no,	/* in: array number 0 - 3 */
	void**	message1,	/* out: the messages passed with the aio
				request; note that also in the case where
				the aio operation failed, these output
				parameters are valid and can be used to
				restart the operation, for example */
	void**	message2);
#endif
/**************************************************************************
Does simulated aio. This function should be called by an i/o-handler
thread. */
/*模拟aio。此函数应由i/o-handler 线程调用。  */
ibool
os_aio_simulated_handle(
/*====================*/
				/* out: TRUE if the aio operation succeeded */
	ulint	segment,	/* in: the number of the segment in the aio
				arrays to wait for; segment 0 is the ibuf
				i/o thread, segment 1 the log i/o thread,
				then follow the non-ibuf read threads, and as
				the last are the non-ibuf write threads */
	void**	message1,	/* out: the messages passed with the aio
				request; note that also in the case where
				the aio operation failed, these output
				parameters are valid and can be used to
				restart the operation, for example */
	void**	message2,
	ulint*	type);		/* out: OS_FILE_WRITE or ..._READ */
/**************************************************************************
Validates the consistency of the aio system. */
/*验证aio系统的一致性。*/
ibool
os_aio_validate(void);
/*=================*/
				/* out: TRUE if ok */
/**************************************************************************
Prints info of the aio arrays. */

void
os_aio_print(void);
/*==============*/
/**************************************************************************
Checks that all slots in the system have been freed, that is, there are
no pending io operations. */
/*检查系统中的所有插槽是否已释放，即没有挂起的io操作*/
ibool
os_aio_all_slots_free(void);
/*=======================*/
				/* out: TRUE if all free */
#endif 
