/*   Innobase relational database engine; Copyright (C) 2001 Innobase Oy
     
     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License 2
     as published by the Free Software Foundation in June 1991.
     
     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.
     
     You should have received a copy of the GNU General Public License 2
     along with this program (in file COPYING); if not, write to the Free
     Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */
/******************************************************
The database buffer buf_pool
数据库缓冲区buf_pool
(c) 1995 Innobase Oy

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#include "buf0buf.h"

#ifdef UNIV_NONINL
#include "buf0buf.ic"
#endif

#include "mem0mem.h"
#include "btr0btr.h"
#include "fil0fil.h"
#include "lock0lock.h"
#include "btr0sea.h"
#include "ibuf0ibuf.h"
#include "dict0dict.h"
#include "log0recv.h"
#include "trx0undo.h"
#include "srv0srv.h"

/*
		IMPLEMENTATION OF THE BUFFER POOL 缓冲池的实现
		=================================

Performance improvement: 性能改进:
------------------------
Thread scheduling in NT may be so slow that the OS wait mechanism should
not be used even in waiting for disk reads to complete.
Rather, we should put waiting query threads to the queue of
waiting jobs, and let the OS thread do something useful while the i/o
is processed. In this way we could remove most OS thread switches in
an i/o-intensive benchmark like TPC-C.
NT中的线程调度可能非常慢，甚至在等待磁盘读取完成时也不应该使用OS等待机制。
相反，我们应该把等待的查询线程放到等待作业的队列中，让OS线程在处理i/o时做一些有用的事情。
通过这种方式，我们可以在像TPC-C这样的i/o密集型基准测试中删除大多数OS线程开关。
A possibility is to put a user space thread library between the database
and NT. User space thread libraries might be very fast.
一种可能性是将用户空间线程库放在数据库和NT之间。用户空间线程库可能非常快。
SQL Server 7.0 can be configured to use 'fibers' which are lightweight
threads in NT. These should be studied.
SQL Server 7.0可以配置为使用'fibers'，这是NT中的轻量级线程。我们应该研究一下。
		Buffer frames and blocks 缓冲帧和块
		------------------------
Following the terminology of Gray and Reuter, we call the memory
blocks where file pages are loaded buffer frames. For each buffer
frame there is a control block, or shortly, a block, in the buffer
control array. The control info which does not need to be stored
in the file along with the file page, resides in the control block.
按照Gray和Reuter的术语，我们将加载文件页的内存块称为缓冲帧。
对于每个缓冲帧，缓冲控制数组中都有一个控制块。不需要与文件页一起存储在文件中的控制信息驻留在控制块中。
		Buffer pool struct 缓冲池结构
		------------------
The buffer buf_pool contains a single mutex which protects all the
control data structures of the buf_pool. The content of a buffer frame is
protected by a separate read-write lock in its control block, though.
These locks can be locked and unlocked without owning the buf_pool mutex.
The OS events in the buf_pool struct can be waited for without owning the
buf_pool mutex.
缓冲区buf_pool包含一个互斥锁，它保护buf_pool的所有控制数据结构。
但是，缓冲帧的内容由其控制块中的一个独立的读写锁保护。这些锁可以在不拥有buf_pool互斥锁的情况下被锁定和解锁。
buf_pool结构中的操作系统事件可以等待而不拥有buf_pool互斥锁。
The buf_pool mutex is a hot-spot in main memory, causing a lot of
memory bus traffic on multiprocessor systems when processors
alternately access the mutex. On our Pentium, the mutex is accessed
maybe every 10 microseconds. We gave up the solution to have mutexes
for each control block, for instance, because it seemed to be
complicated.
buf_pool互斥锁是主存中的一个热点，在多处理器系统中，当处理器交替访问互斥锁时，会导致大量内存总线流量。
在我们的奔腾上，互斥锁可能每10微秒被访问一次。例如，我们放弃了为每个控制块使用互斥锁的解决方案，因为它看起来很复杂。
A solution to reduce mutex contention of the buf_pool mutex is to
create a separate mutex for the page hash table. On Pentium,
accessing the hash table takes 2 microseconds, about half
of the total buf_pool mutex hold time.
减少buf_pool互斥锁争用的一个解决方案是为页哈希表创建一个单独的互斥锁。在Pentium上，访问哈希表需要2微秒，大约占buf_pool互斥锁保持时间的一半。
		Control blocks 控制块
		--------------

The control block contains, for instance, the bufferfix count
which is incremented when a thread wants a file page to be fixed
in a buffer frame. The bufferfix operation does not lock the
contents of the frame, however. For this purpose, the control
block contains a read-write lock.
例如，控制块包含bufferfix计数，当线程希望在缓冲帧中固定文件页时，该计数将增加。但是bufferfix操作并不锁定帧的内容。为此，控制块包含一个读写锁。
The buffer frames have to be aligned so that the start memory
address of a frame is divisible by the universal page size, which
is a power of two.
缓冲帧必须对齐，以便帧的起始内存地址能被通用页大小(2的幂)整除。
We intend to make the buffer buf_pool size on-line reconfigurable,
that is, the buf_pool size can be changed without closing the database.
Then the database administarator may adjust it to be bigger
at night, for example. The control block array must
contain enough control blocks for the maximum buffer buf_pool size
which is used in the particular database.
If the buf_pool size is cut, we exploit the virtual memory mechanism of
the OS, and just refrain from using frames at high addresses. Then the OS
can swap them to disk.
我们打算使缓冲区buf_pool的大小在线重新配置，也就是说，buf_pool的大小可以在不关闭数据库的情况下改变。
例如，数据库管理员可能会在晚上将其调整为更大。控制块数组必须包含足够的控制块，以满足特定数据库中使用的最大缓冲区buf_pool大小。
如果buf_pool的大小被削减，我们就利用操作系统的虚拟内存机制，并且避免在高位地址使用帧。然后操作系统可以将它们交换到磁盘。
The control blocks containing file pages are put to a hash table
according to the file address of the page.
We could speed up the access to an individual page by using
"pointer swizzling": we could replace the page references on
non-leaf index pages by direct pointers to the page, if it exists
in the buf_pool. We could make a separate hash table where we could
chain all the page references in non-leaf pages residing in the buf_pool,
using the page reference as the hash key,
and at the time of reading of a page update the pointers accordingly.
Drawbacks of this solution are added complexity and,
possibly, extra space required on non-leaf pages for memory pointers.
A simpler solution is just to speed up the hash table mechanism
in the database, using tables whose size is a power of 2.
包含文件页的控制块根据该页的文件地址放入哈希表中。我们可以使用“指针混合”来加速对单个页面的访问:
如果页面存在于buf_pool中，我们可以用指向非叶索引页面的直接指针来替代对该页面的引用。
我们可以创建一个单独的哈希表，在这个哈希表中，我们可以将驻留在buf_pool中的非叶页中的所有页引用连接起来，
使用页引用作为哈希键，并在读取一个页时相应地更新指针。这种解决方案的缺点是增加了复杂性，可能还需要在非叶页上为内存指针提供额外的空间。
一种更简单的解决方案是使用大小为2的幂的表来加速数据库中的哈希表机制。
		Lists of blocks 块列表
		---------------

There are several lists of control blocks. The free list contains
blocks which are currently not used.
有几个控制块列表。空闲列表包含当前未使用的块。
The LRU-list contains all the blocks holding a file page
except those for which the bufferfix count is non-zero.
The pages are in the LRU list roughly in the order of the last
access to the page, so that the oldest pages are at the end of the
list. We also keep a pointer to near the end of the LRU list,
which we can use when we want to artificially age a page in the
buf_pool. This is used if we know that some page is not needed
again for some time: we insert the block right after the pointer,
causing it to be replaced sooner than would noramlly be the case.
Currently this aging mechanism is used for read-ahead mechanism
of pages, and it can also be used when there is a scan of a full
table which cannot fit in the memory. Putting the pages near the
of the LRU list, we make sure that most of the buf_pool stays in the
main memory, undisturbed.
LRU-list包含包含文件页的所有块，bufferfix计数不为零的块除外。
这些页面在LRU列表中大致是按照最后一次访问页面的顺序排列的，因此最老的页面位于列表的末尾。
我们还保留了一个指向LRU列表末尾的指针，当我们想在buf_pool中人为地老化一个页面时，可以使用它。
如果我们知道某个页面在一段时间内不再需要了，就会使用这种方法:我们将块插入到指针的后面，导致它比通常情况下更早地被替换。
目前这种老化机制用于页的预读机制，也可以用于扫描一个满表，但内存不能容纳它。
将页面放在LRU列表的附近，我们可以确保buf_pool的大部分保持在主存中，不受干扰。
The chain of modified blocks contains the blocks
holding file pages that have been modified in the memory
but not written to disk yet. The block with the oldest modification
which has not yet been written to disk is at the end of the chain.
被修改的块链包含保存在内存中被修改但尚未写入磁盘的文件页的块。修改时间最久且尚未写入磁盘的区块位于链的末端。
		Loading a file page 加载文件页面
		-------------------

First, a victim block for replacement has to be found in the
buf_pool. It is taken from the free list or searched for from the
end of the LRU-list. An exclusive lock is reserved for the frame,
the io_fix field is set in the block fixing the block in buf_pool,
and the io-operation for loading the page is queued. The io-handler thread
releases the X-lock on the frame and resets the io_fix field
when the io operation completes.
首先，必须在buf_pool中找到一个用于替换的受害者块。它从空闲列表中获取，或者从lru -列表的末尾搜索。
一个排他锁被保留给帧，io_fix字段被设置在buf_pool中固定块的块中，加载页面的io操作被排队。
io-handler线程释放帧上的X-lock，并在io操作完成时重置io_fix字段。
A thread may request the above operation using the buf_page_get-
function. It may then continue to request a lock on the frame.
The lock is granted when the io-handler releases the x-lock.
线程可以使用buf_page_get函数请求上述操作。然后它可能继续请求帧上的锁。当io处理程序释放x-lock时授予锁。
		Read-ahead 预读
		----------

The read-ahead mechanism is intended to be intelligent and
isolated from the semantically higher levels of the database
index management. From the higher level we only need the
information if a file page has a natural successor or
predecessor page. On the leaf level of a B-tree index,
these are the next and previous pages in the natural
order of the pages.
预读机制旨在实现智能，并与语义上更高级别的数据库索引管理隔离。
在较高层，只有当文件页具有自然的后继或前身页时，我们才需要这些信息。在b -树索引的叶级上，这些是按页面的自然顺序排列的下一页和下一页。
Let us first explain the read-ahead mechanism when the leafs
of a B-tree are scanned in an ascending or descending order.
When a read page is the first time referenced in the buf_pool,
the buffer manager checks if it is at the border of a so-called
linear read-ahead area. The tablespace is divided into these
areas of size 64 blocks, for example. So if the page is at the
border of such an area, the read-ahead mechanism checks if
all the other blocks in the area have been accessed in an
ascending or descending order. If this is the case, the system
looks at the natural successor or predecessor of the page,
checks if that is at the border of another area, and in this case
issues read-requests for all the pages in that area. Maybe
we could relax the condition that all the pages in the area
have to be accessed: if data is deleted from a table, there may
appear holes of unused pages in the area.
让我们首先解释当b树的叶子按升序或降序扫描时的预读机制。
当一个读页第一次在buf_pool中被引用时，缓冲区管理器检查它是否位于所谓的线性预读区边界。
例如，表空间被划分为大小为64块的区域。因此，如果页面位于这样一个区域的边界，那么预读机制将检查该区域中的所有其他块是否已按升序或降序访问。
如果是这种情况，系统将查看页面的自然后继或前身，检查它是否位于另一个区域的边界，并在这种情况下对该区域中的所有页面发出读取请求。
也许我们可以放宽必须访问该区域内所有页面的条件:如果从表中删除数据，该区域内可能会出现未使用页面的漏洞。
A different read-ahead mechanism is used when there appears
to be a random access pattern to a file.
If a new page is referenced in the buf_pool, and several pages
of its random access area (for instance, 32 consecutive pages
in a tablespace) have recently been referenced, we may predict
that the whole area may be needed in the near future, and issue
the read requests for the whole area.
当出现对文件的随机访问模式时，将使用不同的预读机制。
如果buf_pool中引用一个新页面,和几页随机访问的区域(例如,连续32页表空间)最近被引用,我们可以预计,整个地区可能需要在不久的将来,整个地区的读请求和问题。 */

buf_pool_t*	buf_pool = NULL; /* The buffer buf_pool of the database */ /*数据库的缓冲区buf_pool*/

ulint		buf_dbg_counter	= 0; /* This is used to insert validation
					operations in excution in the
					debug version 这用于在调试版本中插入执行中的验证操作*/
ibool		buf_debug_prints = FALSE; /* If this is set TRUE,
					the program prints info whenever
					read-ahead or flush occurs 如果设置为TRUE，当预读或刷新发生时，程序打印信息*/

/************************************************************************
Calculates a page checksum which is stored to the page when it is written
to a file. Note that we must be careful to calculate the same value
on 32-bit and 64-bit architectures. 
计算写入文件时存储到页面中的页面校验和。注意，我们必须小心地在32位和64位体系结构上计算相同的值。*/
ulint
buf_calc_page_checksum(
/*===================*/
		       /* out: checksum */
	byte*    page) /* in: buffer page */
{
  	ulint checksum;

  	checksum = ut_fold_binary(page, FIL_PAGE_FILE_FLUSH_LSN);
  		+ ut_fold_binary(page + FIL_PAGE_DATA,
				UNIV_PAGE_SIZE - FIL_PAGE_DATA
				- FIL_PAGE_END_LSN);
  	checksum = checksum & 0xFFFFFFFF;

  	return(checksum);
}

/************************************************************************
Checks if a page is corrupt.检查页面是否损坏。 */

ibool
buf_page_is_corrupted(
/*==================*/
				/* out: TRUE if corrupted */
	byte*	read_buf)	/* in: a database page */
{
	ulint	checksum;

	checksum = buf_calc_page_checksum(read_buf);

	/* Note that InnoDB initializes empty pages to zero, and
	early versions of InnoDB did not store page checksum to
	the 4 most significant bytes of the page lsn field at the
	end of a page: 注意InnoDB会将空页面初始化为0，
	早期的InnoDB版本不会将页面校验和存储在页面末尾的page lsn字段中最重要的4个字节:*/
	if ((mach_read_from_4(read_buf + FIL_PAGE_LSN + 4)
		    		!= mach_read_from_4(read_buf + UNIV_PAGE_SIZE
					- FIL_PAGE_END_LSN + 4))
		|| (checksum != mach_read_from_4(read_buf
                                        + UNIV_PAGE_SIZE
					- FIL_PAGE_END_LSN)
		    && mach_read_from_4(read_buf + FIL_PAGE_LSN)
			    	!= mach_read_from_4(read_buf
                                        + UNIV_PAGE_SIZE
						- FIL_PAGE_END_LSN))) {
		return(TRUE);
	}

	return(FALSE);
}

/************************************************************************
Prints a page to stderr. 将页面打印到标准错误。*/

void
buf_page_print(
/*===========*/
	byte*	read_buf)	/* in: a database page */
{
	dict_index_t*	index;
	ulint		checksum;
	char*		buf;
	
	buf = mem_alloc(4 * UNIV_PAGE_SIZE);

	ut_sprintf_buf(buf, read_buf, UNIV_PAGE_SIZE);

	fprintf(stderr,
	"InnoDB: Page dump in ascii and hex (%lu bytes):\n%s",
					UNIV_PAGE_SIZE, buf);
	fprintf(stderr, "InnoDB: End of page dump\n");

	mem_free(buf);

	checksum = buf_calc_page_checksum(read_buf);

	fprintf(stderr, "InnoDB: Page checksum %lu stored checksum %lu\n",
			checksum, mach_read_from_4(read_buf
                                        + UNIV_PAGE_SIZE
					- FIL_PAGE_END_LSN)); 
	fprintf(stderr,
	"InnoDB: Page lsn %lu %lu, low 4 bytes of lsn at page end %lu\n",
		mach_read_from_4(read_buf + FIL_PAGE_LSN),
		mach_read_from_4(read_buf + FIL_PAGE_LSN + 4),
		mach_read_from_4(read_buf + UNIV_PAGE_SIZE
					- FIL_PAGE_END_LSN + 4));
	if (mach_read_from_2(read_buf + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_TYPE)
	    == TRX_UNDO_INSERT) {
	    	fprintf(stderr,
			"InnoDB: Page may be an insert undo log page\n");
	} else if (mach_read_from_2(read_buf + TRX_UNDO_PAGE_HDR
						+ TRX_UNDO_PAGE_TYPE)
	    	== TRX_UNDO_UPDATE) {
	    	fprintf(stderr,
			"InnoDB: Page may be an update undo log page\n");
	}

	if (fil_page_get_type(read_buf) == FIL_PAGE_INDEX) {
	    	fprintf(stderr,
			"InnoDB: Page may be an index page ");

		fprintf(stderr,
			"where index id is %lu %lu\n",
			ut_dulint_get_high(btr_page_get_index_id(read_buf)),
			ut_dulint_get_low(btr_page_get_index_id(read_buf)));

		index = dict_index_find_on_id_low(
					btr_page_get_index_id(read_buf));
		if (index) {
			fprintf(stderr, "InnoDB: and table %s index %s\n",
						index->table_name,
						index->name);
		}
	}
}

/************************************************************************
Initializes a buffer control block when the buf_pool is created.在创建buf_pool时初始化缓冲区控制块。 */
static
void
buf_block_init(
/*===========*/
	buf_block_t*	block,	/* in: pointer to control block */
	byte*		frame)	/* in: pointer to buffer frame */
{
	block->state = BUF_BLOCK_NOT_USED;
	
	block->frame = frame;

	block->modify_clock = ut_dulint_zero;
	
	block->file_page_was_freed = FALSE;

	rw_lock_create(&(block->lock));
	ut_ad(rw_lock_validate(&(block->lock)));

	rw_lock_create(&(block->read_lock));
	rw_lock_set_level(&(block->read_lock), SYNC_NO_ORDER_CHECK);
	
	rw_lock_create(&(block->debug_latch));
	rw_lock_set_level(&(block->debug_latch), SYNC_NO_ORDER_CHECK);
}

/************************************************************************
Creates a buffer buf_pool object. 创建缓冲区buf_pool对象。*/
static
buf_pool_t*
buf_pool_create(
/*============*/
				/* out, own: buf_pool object, NULL if not
				enough memory */
	ulint	max_size,	/* in: maximum size of the buf_pool in
				blocks */
	ulint	curr_size)	/* in: current size to use, must be <=
				max_size, currently must be equal to
				max_size */
{
	byte*		frame;
	ulint		i;
	buf_block_t*	block;
	
	ut_a(max_size == curr_size);
	
	buf_pool = mem_alloc(sizeof(buf_pool_t));

	/* 1. Initialize general fields 初始化通用字段
	   ---------------------------- */
	mutex_create(&(buf_pool->mutex));
	mutex_set_level(&(buf_pool->mutex), SYNC_BUF_POOL);

	mutex_enter(&(buf_pool->mutex));
	
	buf_pool->frame_mem = ut_malloc(UNIV_PAGE_SIZE * (max_size + 1));

	if (buf_pool->frame_mem == NULL) {

		return(NULL);
	}

	buf_pool->blocks = ut_malloc(sizeof(buf_block_t) * max_size);

	if (buf_pool->blocks == NULL) {

		return(NULL);
	}

	buf_pool->max_size = max_size;
	buf_pool->curr_size = curr_size;

	/* Align pointer to the first frame 对齐指针到第一帧*/

	frame = ut_align(buf_pool->frame_mem, UNIV_PAGE_SIZE);
	buf_pool->frame_zero = frame;

	buf_pool->high_end = frame + UNIV_PAGE_SIZE * curr_size;

	/* Init block structs and assign frames for them 初始化块结构并为它们分配帧*/
	for (i = 0; i < max_size; i++) {

		block = buf_pool_get_nth_block(buf_pool, i);
		buf_block_init(block, frame);
		frame = frame + UNIV_PAGE_SIZE;
	}
	
	buf_pool->page_hash = hash_create(2 * max_size);

	buf_pool->n_pend_reads = 0;

	buf_pool->last_printout_time = time(NULL);

	buf_pool->n_pages_read = 0;
	buf_pool->n_pages_written = 0;
	buf_pool->n_pages_created = 0;

	buf_pool->n_page_gets = 0;
	buf_pool->n_page_gets_old = 0;
	buf_pool->n_pages_read_old = 0;
	buf_pool->n_pages_written_old = 0;
	buf_pool->n_pages_created_old = 0;
	
	/* 2. Initialize flushing fields 初始化冲洗字段
	   ---------------------------- */
	UT_LIST_INIT(buf_pool->flush_list);

	for (i = BUF_FLUSH_LRU; i <= BUF_FLUSH_LIST; i++) {
		buf_pool->n_flush[i] = 0;
		buf_pool->init_flush[i] = FALSE;
		buf_pool->no_flush[i] = os_event_create(NULL);
	}

	buf_pool->LRU_flush_ended = 0;

	buf_pool->ulint_clock = 1;
	buf_pool->freed_page_clock = 0;
	
	/* 3. Initialize LRU fields 初始化LRU字段
	   ---------------------------- */
	UT_LIST_INIT(buf_pool->LRU);

	buf_pool->LRU_old = NULL;

	/* Add control blocks to the free list  向空闲列表中添加控制块*/
	UT_LIST_INIT(buf_pool->free);
	for (i = 0; i < curr_size; i++) {

		block = buf_pool_get_nth_block(buf_pool, i);

		/* Wipe contents of page to eliminate a Purify warning 擦去页面的内容以消除Purify警告*/
		memset(block->frame, '\0', UNIV_PAGE_SIZE);

		UT_LIST_ADD_FIRST(free, buf_pool->free, block);
	}

	mutex_exit(&(buf_pool->mutex));

	btr_search_sys_create(curr_size * UNIV_PAGE_SIZE / sizeof(void*) / 64);

	return(buf_pool);
}	

/************************************************************************
Initializes the buffer buf_pool of the database. 初始化数据库的缓冲区buf_pool。*/

void
buf_pool_init(
/*==========*/
	ulint	max_size,	/* in: maximum size of the buf_pool in blocks */
	ulint	curr_size)	/* in: current size to use, must be <=
				max_size */
{
	ut_a(buf_pool == NULL);

	buf_pool_create(max_size, curr_size);

	ut_ad(buf_validate());
}

/************************************************************************
Allocates a buffer block. 分配一个缓冲区块。*/
UNIV_INLINE
buf_block_t*
buf_block_alloc(void)
/*=================*/
				/* out, own: the allocated block */
{
	buf_block_t*	block;

	block = buf_LRU_get_free_block();

	return(block);
}

/************************************************************************
Moves to the block to the start of the LRU list if there is a danger
that the block would drift out of the buffer pool. 如果块有漂移出缓冲池的危险，则移动到LRU列表的开头。*/
UNIV_INLINE
void
buf_block_make_young(
/*=================*/
	buf_block_t*	block)	/* in: block to make younger */
{
	if (buf_pool->freed_page_clock >= block->freed_page_clock 
				+ 1 + (buf_pool->curr_size / 1024)) {

		/* There has been freeing activity in the LRU list:
		best to move to the head of the LRU list
		LRU列表中已经有了释放活动:最好移到LRU列表的头部 */
		buf_LRU_make_block_young(block);
	}
}

/************************************************************************
Moves a page to the start of the buffer pool LRU list. This high-level
function can be used to prevent an important page from from slipping out of
the buffer pool.我们首先将LRU列表中的所有块初始化为old，然后使用adjust函数将LRU_old指针移动到正确的位置 */

void
buf_page_make_young(
/*=================*/
	buf_frame_t*	frame)	/* in: buffer frame of a file page */
{
	buf_block_t*	block;
	
	mutex_enter(&(buf_pool->mutex));

	block = buf_block_align(frame);

	ut_ad(block->state == BUF_BLOCK_FILE_PAGE);

	buf_LRU_make_block_young(block);

	mutex_exit(&(buf_pool->mutex));
}

/************************************************************************
Frees a buffer block which does not contain a file page. 释放不包含文件页的缓冲区块。*/
UNIV_INLINE
void
buf_block_free(
/*===========*/
	buf_block_t*	block)	/* in, own: block to be freed */
{
	ut_ad(block->state != BUF_BLOCK_FILE_PAGE);

	mutex_enter(&(buf_pool->mutex));

	buf_LRU_block_free_non_file_page(block);

	mutex_exit(&(buf_pool->mutex));
}

/*************************************************************************
Allocates a buffer frame. */
/*分配一个缓冲区帧。*/
buf_frame_t*
buf_frame_alloc(void)
/*=================*/
				/* out: buffer frame */
{
	return(buf_block_alloc()->frame);
}

/*************************************************************************
Frees a buffer frame which does not contain a file page. */
/*释放不包含文件页的缓冲帧。*/
void
buf_frame_free(
/*===========*/
	buf_frame_t*	frame)	/* in: buffer frame */
{
	buf_block_free(buf_block_align(frame));
}
	
/************************************************************************
Returns the buffer control block if the page can be found in the buffer
pool. NOTE that it is possible that the page is not yet read
from disk, though. This is a very low-level function: use with care! 
如果可以在缓冲池中找到该页，则返回缓冲控制块。注意，有可能该页还没有从磁盘读取。这是一个非常低级的函数:小心使用!*/
buf_block_t*
buf_page_peek_block(
/*================*/
			/* out: control block if found from page hash table,
			otherwise NULL; NOTE that the page is not necessarily
			yet read from disk! */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number */
{
	buf_block_t*	block;

	mutex_enter_fast(&(buf_pool->mutex));

	block = buf_page_hash_get(space, offset);

	mutex_exit(&(buf_pool->mutex));

	return(block);
}

/************************************************************************
Returns the current state of is_hashed of a page. FALSE if the page is
not in the pool. NOTE that this operation does not fix the page in the
pool if it is found there. 返回页面的is_hash的当前状态。
如果页不在池中，则为FALSE。注意，如果在池中找到页面，此操作不会将其固定在池中。*/
ibool
buf_page_peek_if_search_hashed(
/*===========================*/
			/* out: TRUE if page hash index is built in search
			system */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number */
{
	buf_block_t*	block;
	ibool		is_hashed;

	mutex_enter_fast(&(buf_pool->mutex));

	block = buf_page_hash_get(space, offset);

	if (!block) {
		is_hashed = FALSE;
	} else {
		is_hashed = block->is_hashed;
	}

	mutex_exit(&(buf_pool->mutex));

	return(is_hashed);
}

/************************************************************************
Returns TRUE if the page can be found in the buffer pool hash table. NOTE
that it is possible that the page is not yet read from disk, though. */
/*如果可以在缓冲池哈希表中找到该页，则返回TRUE。注意，可能还没有从磁盘读取页面。*/
ibool
buf_page_peek(
/*==========*/
			/* out: TRUE if found from page hash table,
			NOTE that the page is not necessarily yet read
			from disk! */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number */
{
	if (buf_page_peek_block(space, offset)) {

		return(TRUE);
	}

	return(FALSE);
}

/************************************************************************
Sets file_page_was_freed TRUE if the page is found in the buffer pool.
This function should be called when we free a file page and want the
debug version to check that it is not accessed any more unless
reallocated. 如果在缓冲池中找到页面，则设置file_page_was_freed为TRUE。
当释放文件页并希望调试版本检查该文件页是否不再被访问(除非重新分配)时，应该调用此函数。*/
buf_block_t*
buf_page_set_file_page_was_freed(
/*=============================*/
			/* out: control block if found from page hash table,
			otherwise NULL */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number */
{
	buf_block_t*	block;

	mutex_enter_fast(&(buf_pool->mutex));

	block = buf_page_hash_get(space, offset);

	if (block) {
		block->file_page_was_freed = TRUE;
	}

	mutex_exit(&(buf_pool->mutex));

	return(block);
}

/************************************************************************
Sets file_page_was_freed FALSE if the page is found in the buffer pool.
This function should be called when we free a file page and want the
debug version to check that it is not accessed any more unless
reallocated. 如果在缓冲池中找到页面，则将file_page_was_freed设置为FALSE。
当释放文件页并希望调试版本检查该文件页是否不再被访问(除非重新分配)时，应该调用此函数。*/

buf_block_t*
buf_page_reset_file_page_was_freed(
/*===============================*/
			/* out: control block if found from page hash table,
			otherwise NULL */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number */
{
	buf_block_t*	block;

	mutex_enter_fast(&(buf_pool->mutex));

	block = buf_page_hash_get(space, offset);

	if (block) {
		block->file_page_was_freed = FALSE;
	}

	mutex_exit(&(buf_pool->mutex));

	return(block);
}

/************************************************************************
This is the general function used to get access to a database page. */
/*这是用于访问数据库页的常规函数。*/
buf_frame_t*
buf_page_get_gen(
/*=============*/
				/* out: pointer to the frame or NULL */
	ulint		space,	/* in: space id */
	ulint		offset,	/* in: page number */
	ulint		rw_latch,/* in: RW_S_LATCH, RW_X_LATCH, RW_NO_LATCH */
	buf_frame_t*	guess,	/* in: guessed frame or NULL */
	ulint		mode,	/* in: BUF_GET, BUF_GET_IF_IN_POOL,
				BUF_GET_NO_LATCH, BUF_GET_NOWAIT */
	char*		file,	/* in: file name */
	ulint		line,	/* in: line where called */
	mtr_t*		mtr)	/* in: mini-transaction */
{
	buf_block_t*	block;
	ibool		accessed;
	ulint		fix_type;
	ibool		success;
	ibool		must_read;
	
	ut_ad(mtr);
	ut_ad((rw_latch == RW_S_LATCH)
	      || (rw_latch == RW_X_LATCH)
	      || (rw_latch == RW_NO_LATCH));
	ut_ad((mode != BUF_GET_NO_LATCH) || (rw_latch == RW_NO_LATCH));
	ut_ad((mode == BUF_GET) || (mode == BUF_GET_IF_IN_POOL)
	      || (mode == BUF_GET_NO_LATCH) || (mode == BUF_GET_NOWAIT));
#ifndef UNIV_LOG_DEBUG
	ut_ad(!ibuf_inside() || ibuf_page(space, offset));
#endif
	buf_pool->n_page_gets++;
loop:
	mutex_enter_fast(&(buf_pool->mutex));

	block = NULL;
	
	if (guess) {
		block = buf_block_align(guess);

		if ((offset != block->offset) || (space != block->space)
				|| (block->state != BUF_BLOCK_FILE_PAGE)) {

			block = NULL;
		}
	}

	if (block == NULL) {
		block = buf_page_hash_get(space, offset);
	}

	if (block == NULL) {
		/* Page not in buf_pool: needs to be read from file :Page not in buf_pool:需要从文件中读取*/

		mutex_exit(&(buf_pool->mutex));

		if (mode == BUF_GET_IF_IN_POOL) {

			return(NULL);
		}

		buf_read_page(space, offset);

		#ifdef UNIV_DEBUG
		buf_dbg_counter++;

		if (buf_dbg_counter % 37 == 0) {
			ut_ad(buf_validate());
		}
		#endif
		goto loop;
	}

	must_read = FALSE;
	
	if (block->io_fix == BUF_IO_READ) {

		must_read = TRUE;

		if (mode == BUF_GET_IF_IN_POOL) {

			/* The page is only being read to buffer 页面只被读取到缓冲区*/
			mutex_exit(&(buf_pool->mutex));

			return(NULL);
		}
	}		

#ifdef UNIV_SYNC_DEBUG
	buf_block_buf_fix_inc_debug(block, file, line);
#else
	buf_block_buf_fix_inc(block);
#endif
	buf_block_make_young(block);

	/* Check if this is the first access to the page 检查这是否是对页面的第一次访问*/

	accessed = block->accessed;

	block->accessed = TRUE;

#ifdef UNIV_DEBUG_FILE_ACCESSES
	ut_a(block->file_page_was_freed == FALSE);
#endif	
	mutex_exit(&(buf_pool->mutex));

#ifdef UNIV_DEBUG
	buf_dbg_counter++;

	if (buf_dbg_counter % 5771 == 0) {
		ut_ad(buf_validate());
	}
#endif
	ut_ad(block->buf_fix_count > 0);
	ut_ad(block->state == BUF_BLOCK_FILE_PAGE);

	if (mode == BUF_GET_NOWAIT) {
		if (rw_latch == RW_S_LATCH) {
			success = rw_lock_s_lock_func_nowait(&(block->lock),
								file, line);
			fix_type = MTR_MEMO_PAGE_S_FIX;
		} else {
			ut_ad(rw_latch == RW_X_LATCH);
			success = rw_lock_x_lock_func_nowait(&(block->lock),
					file, line);
			fix_type = MTR_MEMO_PAGE_X_FIX;
		}

		if (!success) {
			mutex_enter(&(buf_pool->mutex));

			block->buf_fix_count--;
#ifdef UNIV_SYNC_DEBUG
			rw_lock_s_unlock(&(block->debug_latch));
#endif			
			mutex_exit(&(buf_pool->mutex));

			return(NULL);
		}
	} else if (rw_latch == RW_NO_LATCH) {

		if (must_read) {
			rw_lock_x_lock(&(block->read_lock));
			rw_lock_x_unlock(&(block->read_lock));
		}

		fix_type = MTR_MEMO_BUF_FIX;
	} else if (rw_latch == RW_S_LATCH) {

		rw_lock_s_lock_func(&(block->lock), 0, file, line);

		fix_type = MTR_MEMO_PAGE_S_FIX;
	} else {
		rw_lock_x_lock_func(&(block->lock), 0, file, line);

		fix_type = MTR_MEMO_PAGE_X_FIX;
	}

	mtr_memo_push(mtr, block, fix_type);

	if (!accessed) {
		/* In the case of a first access, try to apply linear
		read-ahead 在第一次访问的情况下，尝试应用线性预读*/

		buf_read_ahead_linear(space, offset);
	}

#ifdef UNIV_IBUF_DEBUG
	ut_a(ibuf_count_get(block->space, block->offset) == 0);
#endif
	return(block->frame);		
}

/************************************************************************
This is the general function used to get optimistic access to a database
page. 这是用于优化访问数据库页面的通用函数。*/

ibool
buf_page_optimistic_get_func(
/*=========================*/
				/* out: TRUE if success */
	ulint		rw_latch,/* in: RW_S_LATCH, RW_X_LATCH */
	buf_frame_t*	guess,	/* in: guessed frame */
	dulint		modify_clock,/* in: modify clock value if mode is
				..._GUESS_ON_CLOCK */
	char*		file,	/* in: file name */
	ulint		line,	/* in: line where called */
	mtr_t*		mtr)	/* in: mini-transaction */
{
	buf_block_t*	block;
	ibool		accessed;
	ibool		success;
	ulint		fix_type;

	ut_ad(mtr && guess);
	ut_ad((rw_latch == RW_S_LATCH) || (rw_latch == RW_X_LATCH));

	buf_pool->n_page_gets++;

	block = buf_block_align(guess);
	
	mutex_enter(&(buf_pool->mutex));

	if (block->state != BUF_BLOCK_FILE_PAGE) {

		mutex_exit(&(buf_pool->mutex));

		return(FALSE);
	}

#ifdef UNIV_SYNC_DEBUG
	buf_block_buf_fix_inc_debug(block, file, line);
#else
	buf_block_buf_fix_inc(block);
#endif
	buf_block_make_young(block);

	/* Check if this is the first access to the page 检查这是否是对页面的第一次访问*/

	accessed = block->accessed;

	block->accessed = TRUE;

	mutex_exit(&(buf_pool->mutex));

	ut_ad(!ibuf_inside() || ibuf_page(block->space, block->offset));

	if (rw_latch == RW_S_LATCH) {
		success = rw_lock_s_lock_func_nowait(&(block->lock),
								file, line);
		fix_type = MTR_MEMO_PAGE_S_FIX;
	} else {
		success = rw_lock_x_lock_func_nowait(&(block->lock),
								file, line);
		fix_type = MTR_MEMO_PAGE_X_FIX;
	}

	if (!success) {
		mutex_enter(&(buf_pool->mutex));
		
		block->buf_fix_count--;
#ifdef UNIV_SYNC_DEBUG
		rw_lock_s_unlock(&(block->debug_latch));
#endif			
		mutex_exit(&(buf_pool->mutex));

		return(FALSE);
	}

	if (!UT_DULINT_EQ(modify_clock, block->modify_clock)) {

		buf_page_dbg_add_level(block->frame, SYNC_NO_ORDER_CHECK);
	
		if (rw_latch == RW_S_LATCH) {
			rw_lock_s_unlock(&(block->lock));
		} else {
			rw_lock_x_unlock(&(block->lock));
		}

		mutex_enter(&(buf_pool->mutex));
		
		block->buf_fix_count--;
#ifdef UNIV_SYNC_DEBUG
		rw_lock_s_unlock(&(block->debug_latch));
#endif			
		mutex_exit(&(buf_pool->mutex));
		
		return(FALSE);
	}

	mtr_memo_push(mtr, block, fix_type);

#ifdef UNIV_DEBUG
	buf_dbg_counter++;

	if (buf_dbg_counter % 5771 == 0) {
		ut_ad(buf_validate());
	}
#endif
	ut_ad(block->buf_fix_count > 0);
	ut_ad(block->state == BUF_BLOCK_FILE_PAGE);

#ifdef UNIV_DEBUG_FILE_ACCESSES
	ut_a(block->file_page_was_freed == FALSE);
#endif
	if (!accessed) {
		/* In the case of a first access, try to apply linear
		read-ahead */

		buf_read_ahead_linear(buf_frame_get_space_id(guess),
					buf_frame_get_page_no(guess));
	}

#ifdef UNIV_IBUF_DEBUG
	ut_a(ibuf_count_get(block->space, block->offset) == 0);
#endif
	return(TRUE);
}

/************************************************************************
This is used to get access to a known database page, when no waiting can be
done. */
/*在无法等待的情况下，这用于访问已知的数据库页面。*/
ibool
buf_page_get_known_nowait(
/*======================*/
				/* out: TRUE if success */
	ulint		rw_latch,/* in: RW_S_LATCH, RW_X_LATCH */
	buf_frame_t*	guess,	/* in: the known page frame */
	ulint		mode,	/* in: BUF_MAKE_YOUNG or BUF_KEEP_OLD */
	char*		file,	/* in: file name */
	ulint		line,	/* in: line where called */
	mtr_t*		mtr)	/* in: mini-transaction */
{
	buf_block_t*	block;
	ibool		success;
	ulint		fix_type;

	ut_ad(mtr);
	ut_ad((rw_latch == RW_S_LATCH) || (rw_latch == RW_X_LATCH));

	buf_pool->n_page_gets++;

	block = buf_block_align(guess);
	
	mutex_enter(&(buf_pool->mutex));

	if (block->state == BUF_BLOCK_REMOVE_HASH) {
	        /* Another thread is just freeing the block from the LRU list
	        of the buffer pool: do not try to access this page; this
		attempt to access the page can only come through the hash
		index because when the buffer block state is ..._REMOVE_HASH,
		we have already removed it from the page address hash table
		of the buffer pool. 另一个线程只是从缓冲池的LRU列表中释放块:不要尝试访问这个页面;
		这种访问页面的尝试只能通过哈希索引，因为当缓冲区块状态是…_REMOVE_HASH，我们已经从缓冲池的页地址哈希表中删除了它。*/
	        mutex_exit(&(buf_pool->mutex));

		return(FALSE);
	}

#ifdef UNIV_SYNC_DEBUG
	buf_block_buf_fix_inc_debug(block, file, line);
#else
	buf_block_buf_fix_inc(block);
#endif
	if (mode == BUF_MAKE_YOUNG) {
		buf_block_make_young(block);
	}

	mutex_exit(&(buf_pool->mutex));

	ut_ad(!ibuf_inside() || (mode == BUF_KEEP_OLD));

	if (rw_latch == RW_S_LATCH) {
		success = rw_lock_s_lock_func_nowait(&(block->lock),
								file, line);
		fix_type = MTR_MEMO_PAGE_S_FIX;
	} else {
		success = rw_lock_x_lock_func_nowait(&(block->lock),
								file, line);
		fix_type = MTR_MEMO_PAGE_X_FIX;
	}
	
	if (!success) {
		mutex_enter(&(buf_pool->mutex));
		
		block->buf_fix_count--;
#ifdef UNIV_SYNC_DEBUG
		rw_lock_s_unlock(&(block->debug_latch));
#endif		
		mutex_exit(&(buf_pool->mutex));

		return(FALSE);
	}

	mtr_memo_push(mtr, block, fix_type);

#ifdef UNIV_DEBUG
	buf_dbg_counter++;

	if (buf_dbg_counter % 5771 == 0) {
		ut_ad(buf_validate());
	}
#endif
	ut_ad(block->buf_fix_count > 0);
	ut_ad(block->state == BUF_BLOCK_FILE_PAGE);
#ifdef UNIV_DEBUG_FILE_ACCESSES
	ut_a(block->file_page_was_freed == FALSE);
#endif

#ifdef UNIV_IBUF_DEBUG
	ut_a((mode == BUF_KEEP_OLD)
		|| (ibuf_count_get(block->space, block->offset) == 0));
#endif
	return(TRUE);
}

/************************************************************************
Inits a page to the buffer buf_pool. 将一个页面初始化为缓冲区buf_pool。*/
static
void
buf_page_init(
/*==========*/
				/* out: pointer to the block */
	ulint		space,	/* in: space id */
	ulint		offset,	/* in: offset of the page within space
				in units of a page 页在空间内的偏移量，以页为单位*/
	buf_block_t*	block)	/* in: block to init */
{
	ut_ad(mutex_own(&(buf_pool->mutex)));
	ut_ad(block->state == BUF_BLOCK_READY_FOR_USE);

	/* Set the state of the block 设置块的状态*/
	block->state 		= BUF_BLOCK_FILE_PAGE;
	block->space 		= space;
	block->offset 		= offset;

	block->lock_hash_val	= lock_rec_hash(space, offset);
	block->lock_mutex	= NULL;
	
	/* Insert into the hash table of file pages 插入到文件页的哈希表中*/

	HASH_INSERT(buf_block_t, hash, buf_pool->page_hash,
				buf_page_address_fold(space, offset), block);

	block->freed_page_clock = 0;

	block->newest_modification = ut_dulint_zero;
	block->oldest_modification = ut_dulint_zero;
	
	block->accessed		= FALSE;
	block->buf_fix_count 	= 0;
	block->io_fix		= 0;

	block->n_hash_helps	= 0;
	block->is_hashed	= FALSE;
	block->n_fields         = 1;
	block->n_bytes          = 0;
	block->side             = BTR_SEARCH_LEFT_SIDE;

	block->file_page_was_freed = FALSE;
}

/************************************************************************
Function which inits a page for read to the buffer buf_pool. If the page is
already in buf_pool, does nothing. Sets the io_fix flag to BUF_IO_READ and
sets a non-recursive exclusive lock on the buffer frame. The io-handler must
take care that the flag is cleared and the lock released later. This is one
of the functions which perform the state transition NOT_USED => FILE_PAGE to
a block (the other is buf_page_create). */ 
/*初始化buffer buf_pool的读页面的函数。如果页面已经在buf_pool中，则不执行任何操作。
设置io_fix标志为BUF_IO_READ，并设置缓冲区帧上的非递归排他锁。io处理程序必须注意清除标志并稍后释放锁。
这是执行状态转换NOT_USED => FILE_PAGE到一个块的函数之一(另一个是buf_page_create)。*/
buf_block_t*
buf_page_init_for_read(
/*===================*/
			/* out: pointer to the block or NULL */
	ulint	mode,	/* in: BUF_READ_IBUF_PAGES_ONLY, ... */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number */
{
	buf_block_t*	block;
	mtr_t		mtr;
	
	ut_ad(buf_pool);

	if (mode == BUF_READ_IBUF_PAGES_ONLY) {
		/* It is a read-ahead within an ibuf routine 它是ibuf例程中的预读*/

		ut_ad(!ibuf_bitmap_page(offset));
		ut_ad(ibuf_inside());
	
		mtr_start(&mtr);
	
		if (!ibuf_page_low(space, offset, &mtr)) {

			mtr_commit(&mtr);

			return(NULL);
		}
	} else {
		ut_ad(mode == BUF_READ_ANY_PAGE);
	}
	
	block = buf_block_alloc();

	ut_ad(block);

	mutex_enter(&(buf_pool->mutex));
	
	if (NULL != buf_page_hash_get(space, offset)) {

		/* The page is already in buf_pool, return 页面已经在buf_pool中，返回*/

		mutex_exit(&(buf_pool->mutex));
		buf_block_free(block);

		if (mode == BUF_READ_IBUF_PAGES_ONLY) {

			mtr_commit(&mtr);
		}

		return(NULL);
	}

	ut_ad(block);
	
	buf_page_init(space, offset, block);

	/* The block must be put to the LRU list, to the old blocks 该块必须放到LRU列表中，放到旧的块中*/

	buf_LRU_add_block(block, TRUE); 	/* TRUE == to old blocks */
	
	block->io_fix = BUF_IO_READ;
	buf_pool->n_pend_reads++;
	
	/* We set a pass-type x-lock on the frame because then the same
	thread which called for the read operation (and is running now at
	this point of code) can wait for the read to complete by waiting
	for the x-lock on the frame; if the x-lock were recursive, the
	same thread would illegally get the x-lock before the page read
	is completed. The x-lock is cleared by the io-handler thread.
	 我们在帧上设置一个pass类型的x-lock，因为调用读操作的同一个线程(现在正在运行)可以通过等待帧上的x-lock来等待读操作完成;
	 如果x-lock是递归的，那么同一个线程会在页读完成之前非法获得x-lock。x-lock被io-handler线程清除。*/
	rw_lock_x_lock_gen(&(block->lock), BUF_IO_READ);

	rw_lock_x_lock_gen(&(block->read_lock), BUF_IO_READ);
	
 	mutex_exit(&(buf_pool->mutex));

	if (mode == BUF_READ_IBUF_PAGES_ONLY) {

		mtr_commit(&mtr);
	}

	return(block);
}	

/************************************************************************
Initializes a page to the buffer buf_pool. The page is usually not read
from a file even if it cannot be found in the buffer buf_pool. This is one
of the functions which perform to a block a state transition NOT_USED =>
FILE_PAGE (the other is buf_page_init_for_read above). */
/*将页初始化为缓冲区buf_pool。
通常不会从文件中读取页，即使在buffer buf_pool中找不到。
这是一个函数，它执行一个块的状态转换NOT_USED=>FILE_PAGE（另一个是buf_page_init_for_read above）。 */
buf_frame_t*
buf_page_create(
/*============*/
			/* out: pointer to the frame, page bufferfixed */
	ulint	space,	/* in: space id */
	ulint	offset,	/* in: offset of the page within space in units of
			a page */
	mtr_t*	mtr)	/* in: mini-transaction handle */
{
	buf_frame_t*	frame;
	buf_block_t*	block;
	buf_block_t*	free_block	= NULL;
	
	ut_ad(mtr);

	free_block = buf_LRU_get_free_block();

	/* Delete possible entries for the page from the insert buffer:
	such can exist if the page belonged to an index which was dropped
	从插入缓冲区中删除该页的可能条目:如果该页属于被删除的索引，则可能存在这样的条目 */
	ibuf_merge_or_delete_for_page(NULL, space, offset);	
	
	mutex_enter(&(buf_pool->mutex));

	block = buf_page_hash_get(space, offset);

	if (block != NULL) {
#ifdef UNIV_IBUF_DEBUG
		ut_a(ibuf_count_get(block->space, block->offset) == 0);
#endif
		block->file_page_was_freed = FALSE;

		/* Page can be found in buf_pool 页面可以在buf_pool中找到*/
		mutex_exit(&(buf_pool->mutex));

		buf_block_free(free_block);

		frame = buf_page_get_with_no_latch(space, offset, mtr);

		return(frame);
	}

	/* If we get here, the page was not in buf_pool: init it there 如果我们到达这里，页面不在buf_pool中:在那里初始化它*/

	if (buf_debug_prints) {
		printf("Creating space %lu page %lu to buffer\n", space,
								offset);
	}

	block = free_block;
	
	buf_page_init(space, offset, block);

	/* The block must be put to the LRU list 该块必须放到LRU列表中*/
	buf_LRU_add_block(block, FALSE);
		
#ifdef UNIV_SYNC_DEBUG
	buf_block_buf_fix_inc_debug(block, IB__FILE__, __LINE__);
#else
	buf_block_buf_fix_inc(block);
#endif
	mtr_memo_push(mtr, block, MTR_MEMO_BUF_FIX);

	block->accessed = TRUE;
	
	buf_pool->n_pages_created++;

	mutex_exit(&(buf_pool->mutex));

	/* Flush pages from the end of the LRU list if necessary 如果需要，从LRU列表的末尾刷新页面*/
	buf_flush_free_margin();

	frame = block->frame;
#ifdef UNIV_DEBUG
	buf_dbg_counter++;

	if (buf_dbg_counter % 357 == 0) {
		ut_ad(buf_validate());
	}
#endif
#ifdef UNIV_IBUF_DEBUG
	ut_a(ibuf_count_get(block->space, block->offset) == 0);
#endif
	return(frame);
}

/************************************************************************
Completes an asynchronous read or write request of a file page to or from
the buffer pool. 完成对缓冲池的文件页的异步读或写请求。*/

void
buf_page_io_complete(
/*=================*/
	buf_block_t*	block)	/* in: pointer to the block in question */
{
	dulint		id;
	dict_index_t*	index;
	ulint		io_type;

	ut_ad(block);

	io_type = block->io_fix;

	if (io_type == BUF_IO_READ) {
		/* From version 3.23.38 up we store the page checksum
		   to the 4 upper bytes of the page end lsn field
		从3.23.38版本开始，我们将页面校验和存储到页面结束lsn字段的上部4个字节*/
		if (buf_page_is_corrupted(block->frame)) {
		  	fprintf(stderr,
			  "InnoDB: Database page corruption or a failed\n"
			  "InnoDB: file read of page %lu.\n", block->offset);
			  
		  	fprintf(stderr,
			  "InnoDB: You may have to recover from a backup.\n");

			buf_page_print(block->frame);

		  	fprintf(stderr,
			  "InnoDB: Database page corruption or a failed\n"
			  "InnoDB: file read of page %lu.\n", block->offset);
		  	fprintf(stderr,
			  "InnoDB: You may have to recover from a backup.\n");
			fprintf(stderr,
			  "InnoDB: It is also possible that your operating\n"
			  "InnoDB: system has corrupted its own file cache\n"
			  "InnoDB: and rebooting your computer removes the\n"
			  "InnoDB: error.\n");
			  
			if (srv_force_recovery < SRV_FORCE_IGNORE_CORRUPT) { 
		  		exit(1);
		  	}
		}

		if (recv_recovery_is_on()) {
			recv_recover_page(TRUE, block->frame, block->space,
								block->offset);
		}

		if (!recv_no_ibuf_operations) {
			ibuf_merge_or_delete_for_page(block->frame,
						block->space, block->offset);
		}
	}
	
#ifdef UNIV_IBUF_DEBUG
	ut_a(ibuf_count_get(block->space, block->offset) == 0);
#endif
	mutex_enter(&(buf_pool->mutex));
	
	/* Because this thread which does the unlocking is not the same that
	did the locking, we use a pass value != 0 in unlock, which simply
	removes the newest lock debug record, without checking the thread
	id. 因为执行解锁的线程与执行锁定的线程不同，我们在unlock中使用了一个pass值!= 0，它只是删除了最新的锁调试记录，而不检查线程id。*/

	block->io_fix = 0;
	
	if (io_type == BUF_IO_READ) {
		/* NOTE that the call to ibuf may have moved the ownership of
		the x-latch to this OS thread: do not let this confuse you in
		debugging! 注意，对ibuf的调用可能已经将x-latch的所有权移到了这个操作系统线程:在调试时不要让它迷惑你!*/		
	
		ut_ad(buf_pool->n_pend_reads > 0);
		buf_pool->n_pend_reads--;
		buf_pool->n_pages_read++;


		rw_lock_x_unlock_gen(&(block->lock), BUF_IO_READ);
		rw_lock_x_unlock_gen(&(block->read_lock), BUF_IO_READ);

		if (buf_debug_prints) {
			printf("Has read ");
		}
	} else {
		ut_ad(io_type == BUF_IO_WRITE);

		/* Write means a flush operation: call the completion
		routine in the flush system 写表示刷新操作:在刷新系统中调用完成例程*/

		buf_flush_write_complete(block);

		rw_lock_s_unlock_gen(&(block->lock), BUF_IO_WRITE);

		buf_pool->n_pages_written++;

		if (buf_debug_prints) {
			printf("Has written ");
		}
	}
	
	mutex_exit(&(buf_pool->mutex));

	if (buf_debug_prints) {
		printf("page space %lu page no %lu", block->space,
								block->offset);
		id = btr_page_get_index_id(block->frame);

		index = NULL;
		/* The following can cause deadlocks if used: */
		/*
		index = dict_index_get_if_in_cache(id);

  		if (index) {
			printf(" index name %s table %s", index->name,
							index->table->name);
		}
		*/

		printf("\n");
	}
}

/*************************************************************************
Invalidates the file pages in the buffer pool when an archive recovery is
completed. All the file pages buffered must be in a replaceable state when
this function is called: not latched and not modified. 
当存档恢复完成时，使缓冲池中的文件页无效。当调用此函数时，所有缓冲的文件页必须处于可替换状态:未锁存和未修改。*/

void
buf_pool_invalidate(void)
/*=====================*/
{
	ibool	freed;

	ut_ad(buf_all_freed());
	
	freed = TRUE;

	while (freed) {
		freed = buf_LRU_search_and_free_block(0);
	}
	
	mutex_enter(&(buf_pool->mutex));

	ut_ad(UT_LIST_GET_LEN(buf_pool->LRU) == 0);

	mutex_exit(&(buf_pool->mutex));
}

/*************************************************************************
Validates the buffer buf_pool data structure.验证缓冲区buf_pool数据结构。 */

ibool
buf_validate(void)
/*==============*/
{
	buf_block_t*	block;
	ulint		i;
	ulint		n_single_flush	= 0;
	ulint		n_lru_flush	= 0;
	ulint		n_list_flush	= 0;
	ulint		n_lru		= 0;
	ulint		n_flush		= 0;
	ulint		n_free		= 0;
	ulint		n_page		= 0;
	
	ut_ad(buf_pool);

	mutex_enter(&(buf_pool->mutex));

	for (i = 0; i < buf_pool->curr_size; i++) {

		block = buf_pool_get_nth_block(buf_pool, i);

		if (block->state == BUF_BLOCK_FILE_PAGE) {

			ut_a(buf_page_hash_get(block->space,
						block->offset) == block);
			n_page++;

#ifdef UNIV_IBUF_DEBUG
			ut_a((block->io_fix == BUF_IO_READ)
			     || ibuf_count_get(block->space, block->offset)
								== 0);
#endif
			if (block->io_fix == BUF_IO_WRITE) {

				if (block->flush_type == BUF_FLUSH_LRU) {
					n_lru_flush++;
					ut_a(rw_lock_is_locked(&(block->lock),
							RW_LOCK_SHARED));
				} else if (block->flush_type ==
						BUF_FLUSH_LIST) {
					n_list_flush++;
				} else if (block->flush_type ==
						BUF_FLUSH_SINGLE_PAGE) {
					n_single_flush++;
				} else {
					ut_error;
				}

			} else if (block->io_fix == BUF_IO_READ) {

				ut_a(rw_lock_is_locked(&(block->lock),
							RW_LOCK_EX));
			}
			
			n_lru++;

			if (ut_dulint_cmp(block->oldest_modification,
						ut_dulint_zero) > 0) {
					n_flush++;
			}	
		
		} else if (block->state == BUF_BLOCK_NOT_USED) {
			n_free++;
		}
 	}

	if (n_lru + n_free > buf_pool->curr_size) {
		printf("n LRU %lu, n free %lu\n", n_lru, n_free);
		ut_error;
	}

	ut_a(UT_LIST_GET_LEN(buf_pool->LRU) == n_lru);
	if (UT_LIST_GET_LEN(buf_pool->free) != n_free) {
		printf("Free list len %lu, free blocks %lu\n",
		    UT_LIST_GET_LEN(buf_pool->free), n_free);
		ut_error;
	}
	ut_a(UT_LIST_GET_LEN(buf_pool->flush_list) == n_flush);

	ut_a(buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE] == n_single_flush);
	ut_a(buf_pool->n_flush[BUF_FLUSH_LIST] == n_list_flush);
	ut_a(buf_pool->n_flush[BUF_FLUSH_LRU] == n_lru_flush);
	
	mutex_exit(&(buf_pool->mutex));

	ut_a(buf_LRU_validate());
	ut_a(buf_flush_validate());

	return(TRUE);
}	

/*************************************************************************
Prints info of the buffer buf_pool data structure. 打印缓冲区buf_pool数据结构的信息。*/

void
buf_print(void)
/*===========*/
{
	dulint*		index_ids;
	ulint*		counts;
	ulint		size;
	ulint		i;
	ulint		j;
	dulint		id;
	ulint		n_found;
	buf_frame_t* 	frame;
	dict_index_t*	index;
	
	ut_ad(buf_pool);

	size = buf_pool_get_curr_size() / UNIV_PAGE_SIZE;

	index_ids = mem_alloc(sizeof(dulint) * size);
	counts = mem_alloc(sizeof(ulint) * size);

	mutex_enter(&(buf_pool->mutex));
	
	printf("LRU len %lu \n", UT_LIST_GET_LEN(buf_pool->LRU));
	printf("free len %lu \n", UT_LIST_GET_LEN(buf_pool->free));
	printf("flush len %lu \n", UT_LIST_GET_LEN(buf_pool->flush_list));
	printf("buf_pool size %lu \n", size);

	printf("n pending reads %lu \n", buf_pool->n_pend_reads);

	printf("n pending flush LRU %lu list %lu single page %lu\n",
		buf_pool->n_flush[BUF_FLUSH_LRU],
		buf_pool->n_flush[BUF_FLUSH_LIST],
		buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE]);

	printf("pages read %lu, created %lu, written %lu\n",
			buf_pool->n_pages_read, buf_pool->n_pages_created,
						buf_pool->n_pages_written);

	/* Count the number of blocks belonging to each index in the buffer 计算属于缓冲区中每个索引的块的数量*/
	
	n_found = 0;

	for (i = 0 ; i < size; i++) {
		counts[i] = 0;
	}

	for (i = 0; i < size; i++) {
		frame = buf_pool_get_nth_block(buf_pool, i)->frame;

		if (fil_page_get_type(frame) == FIL_PAGE_INDEX) {

			id = btr_page_get_index_id(frame);

			/* Look for the id in the index_ids array 在index_ids数组中查找id*/
			j = 0;

			while (j < n_found) {

				if (ut_dulint_cmp(index_ids[j], id) == 0) {
					(counts[j])++;

					break;
				}
				j++;
			}

			if (j == n_found) {
				n_found++;
				index_ids[j] = id;
				counts[j] = 1;
			}
		}
	}

	mutex_exit(&(buf_pool->mutex));

	for (i = 0; i < n_found; i++) {
		index = dict_index_get_if_in_cache(index_ids[i]);

		printf("Block count for index %lu in buffer is about %lu",
			ut_dulint_get_low(index_ids[i]), counts[i]);

		if (index) {
			printf(" index name %s table %s", index->name,
				index->table->name);
		}

		printf("\n");
	}
	
	mem_free(index_ids);
	mem_free(counts);

	ut_a(buf_validate());
}	

/*************************************************************************
Returns the number of pending buf pool ios. 返回挂起的buf池ios的数量。*/

ulint
buf_get_n_pending_ios(void)
/*=======================*/
{
	return(buf_pool->n_pend_reads
		+ buf_pool->n_flush[BUF_FLUSH_LRU]
		+ buf_pool->n_flush[BUF_FLUSH_LIST]
		+ buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE]);
}

/*************************************************************************
Prints info of the buffer i/o. 打印缓冲区i/o的信息。*/

void
buf_print_io(void)
/*==============*/
{
	time_t	current_time;
	double	time_elapsed;
	ulint	size;
	
	ut_ad(buf_pool);

	size = buf_pool_get_curr_size() / UNIV_PAGE_SIZE;

	mutex_enter(&(buf_pool->mutex));
	
	printf("Free list length  %lu \n", UT_LIST_GET_LEN(buf_pool->free));
	printf("LRU list length   %lu \n", UT_LIST_GET_LEN(buf_pool->LRU));
	printf("Flush list length %lu \n",
				UT_LIST_GET_LEN(buf_pool->flush_list));
	printf("Buffer pool size  %lu\n", size);

	printf("Pending reads %lu \n", buf_pool->n_pend_reads);

	printf("Pending writes: LRU %lu, flush list %lu, single page %lu\n",
		buf_pool->n_flush[BUF_FLUSH_LRU],
		buf_pool->n_flush[BUF_FLUSH_LIST],
		buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE]);

	current_time = time(NULL);
	time_elapsed = difftime(current_time, buf_pool->last_printout_time);

	buf_pool->last_printout_time = current_time;

	printf("Pages read %lu, created %lu, written %lu\n",
			buf_pool->n_pages_read, buf_pool->n_pages_created,
						buf_pool->n_pages_written);
	printf("%.2f reads/s, %.2f creates/s, %.2f writes/s\n",
		(buf_pool->n_pages_read - buf_pool->n_pages_read_old)
		/ time_elapsed,
		(buf_pool->n_pages_created - buf_pool->n_pages_created_old)
		/ time_elapsed,
		(buf_pool->n_pages_written - buf_pool->n_pages_written_old)
		/ time_elapsed);

	if (buf_pool->n_page_gets > buf_pool->n_page_gets_old) {
		printf("Buffer pool hit rate %lu / 1000\n",
		1000
		- ((1000 *
		    (buf_pool->n_pages_read - buf_pool->n_pages_read_old))
		/ (buf_pool->n_page_gets - buf_pool->n_page_gets_old)));
	} else {
		printf("No buffer pool activity since the last printout\n");
	}

	buf_pool->n_page_gets_old = buf_pool->n_page_gets;
	buf_pool->n_pages_read_old = buf_pool->n_pages_read;
	buf_pool->n_pages_created_old = buf_pool->n_pages_created;
	buf_pool->n_pages_written_old = buf_pool->n_pages_written;

	mutex_exit(&(buf_pool->mutex));
}

/*************************************************************************
Checks that all file pages in the buffer are in a replaceable state. 检查缓冲区中的所有文件页是否处于可替换状态。*/

ibool
buf_all_freed(void)
/*===============*/
{
	buf_block_t*	block;
	ulint		i;
	
	ut_ad(buf_pool);

	mutex_enter(&(buf_pool->mutex));

	for (i = 0; i < buf_pool->curr_size; i++) {

		block = buf_pool_get_nth_block(buf_pool, i);

		if (block->state == BUF_BLOCK_FILE_PAGE) {

			if (!buf_flush_ready_for_replace(block)) {

			    	/* printf("Page %lu %lu still fixed or dirty\n",
			    		block->space, block->offset); */
			    	ut_error;
			}
		}
 	}

	mutex_exit(&(buf_pool->mutex));

	return(TRUE);
}	

/*************************************************************************
Checks that there currently are no pending i/o-operations for the buffer
pool. 检查缓冲池当前没有挂起的i/o操作。*/

ibool
buf_pool_check_no_pending_io(void)
/*==============================*/
				/* out: TRUE if there is no pending i/o */
{
	ibool	ret;

	mutex_enter(&(buf_pool->mutex));

	if (buf_pool->n_pend_reads + buf_pool->n_flush[BUF_FLUSH_LRU]
				+ buf_pool->n_flush[BUF_FLUSH_LIST]
				+ buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE]) {
		ret = FALSE;
	} else {
		ret = TRUE;
	}

	mutex_exit(&(buf_pool->mutex));

	return(ret);
}

/*************************************************************************
Gets the current length of the free list of buffer blocks. 获取缓冲区块的空闲列表的当前长度。*/

ulint
buf_get_free_list_len(void)
/*=======================*/
{
	ulint	len;

	mutex_enter(&(buf_pool->mutex));

	len = UT_LIST_GET_LEN(buf_pool->free);

	mutex_exit(&(buf_pool->mutex));

	return(len);
}
