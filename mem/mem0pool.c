/************************************************************************
The lowest-level memory management

(c) 1997 Innobase Oy

Created 5/12/1997 Heikki Tuuri
*************************************************************************/
/* 最低级别的内存管理 */
#include "mem0pool.h"
#ifdef UNIV_NONINL
#include "mem0pool.ic"
#endif

#include "sync0sync.h"
#include "ut0mem.h"
#include "ut0lst.h"
#include "ut0byte.h"

/* We would like to use also the buffer frames to allocate memory. This
would be desirable, because then the memory consumption of the database
would be fixed, and we might even lock the buffer pool to the main memory.
The problem here is that the buffer management routines can themselves call
memory allocation, while the buffer pool mutex is reserved.

The main components of the memory consumption are:

1. buffer pool,
2. parsed and optimized SQL statements,
3. data dictionary cache,
4. log buffer,
5. locks for each transaction,
6. hash table for the adaptive index,
7. state and buffers for each SQL query currently being executed,
8. session for each user, and
9. stack for each OS thread.

Items 1-3 are managed by an LRU algorithm. Items 5 and 6 can potentially
consume very much memory. Items 7 and 8 should consume quite little memory,
and the OS should take care of item 9, which too should consume little memory.

A solution to the memory management:

1. the buffer pool size is set separately;
2. log buffer size is set separately;
3. the common pool size for all the other entries, except 8, is set separately.

Problems: we may waste memory if the common pool is set too big. Another
problem is the locks, which may take very much space in big transactions.
Then the shared pool size should be set very big. We can allow locks to take
space from the buffer pool, but the SQL optimizer is then unaware of the
usable size of the buffer pool. We could also combine the objects in the
common pool and the buffers in the buffer pool into a single LRU list and
manage it uniformly, but this approach does not take into account the parsing
and other costs unique to SQL statements.

So, let the SQL statements and the data dictionary entries form one single
LRU list, let us call it the dictionary LRU list. The locks for a transaction
can be seen as a part of the state of the transaction. Hence, they should be
stored in the common pool. We still have the problem of a very big update
transaction, for example, which will set very many x-locks on rows, and the
locks will consume a lot of memory, say, half of the buffer pool size.

Another problem is what to do if we are not able to malloc a requested
block of memory from the common pool. Then we can truncate the LRU list of
the dictionary cache. If it does not help, a system error results.

Because 5 and 6 may potentially consume very much memory, we let them grow
into the buffer pool. We may let the locks of a transaction take frames
from the buffer pool, when the corresponding memory heap block has grown to
the size of a buffer frame. Similarly for the hash node cells of the locks,
and for the adaptive index. Thus, for each individual transaction, its locks
can occupy at most about the size of the buffer frame of memory in the common
pool, and after that its locks will grow into the buffer pool. */
/*
我们还想使用缓冲帧来分配内存。
这个因为这样数据库的内存消耗我们甚至可以将缓冲池锁定到主内存。
这里的问题是缓冲区管理例程可以自己调用内存分配，而缓冲池互斥是保留的。

内存消耗的主要组成部分是：
1缓冲池，
2经过分析和优化的SQL语句，
3数据字典缓存，
4日志缓冲区，
5每个事务的锁，
6自适应索引的哈希表，
7当前正在执行的每个SQL查询的状态和缓冲区，
8每个用户的会话，以及
9每个操作系统线程的堆栈。

项目1-3由LRU算法管理。项目5和6可能消耗大量内存。
第7项和第8项应该占用很少的内存，操作系统应该处理第9项，这也应该消耗很少的内存。

内存管理解决方案：
1缓冲池大小单独设置；
2日志缓冲区大小单独设置；
3所有其他条目（8除外）的公共池大小都是单独设置的。

问题：如果公共池设置得太大，可能会浪费内存。
另一个问题是锁，在大型事务中可能会占用很大的空间。
那么共享池的大小应该设置得非常大。
我们可以允许锁被拿走但是SQL优化器不知道缓冲池的可用大小。
我们还可以将公共池和缓冲池中的缓冲区合并为一个LRU列表统一管理，
但这种方法不考虑解析以及SQL语句特有的其他开销。

因此，让SQL语句和数据字典条目形成一个单独的LRU列表，我们称之为字典LRU列表。
事务的锁可以看作是事务状态的一部分。
因此，他们应该存储在公共池中。
我们仍然有一个非常大的更新问题事务，它将在行上设置很多x锁，
而锁将消耗大量内存，例如缓冲池大小的一半。
另一个问题是，如果我们不能malloc一个请求，该怎么办公共池中的内存块。
然后我们可以截断字典缓存。如果没有帮助，将导致系统错误。
因为5和6可能会消耗很多内存，所以我们让它们增长进入缓冲池。
我们可以让事务的锁取帧当相应的内存堆块增长到缓冲帧的大小。
类似地，对于锁的哈希节点单元，对于自适应索引。
因此，对于每个事务，它的锁最多可以占用大约公共内存的缓冲帧大小
然后它的锁将增长到缓冲池中。
*/
/* Mask used to extract the free bit from area->size */
/* 用于从area->size中提取空闲位的掩码*/
#define MEM_AREA_FREE	1

/* The smallest memory area total size */
/* 最小内存区总大小 */
#define MEM_AREA_MIN_SIZE	(2 * MEM_AREA_EXTRA_SIZE)


/* Data structure for a memory pool. The space is allocated using the buddy
algorithm, where free list i contains areas of size 2 to power i. */
/* 内存池的数据结构。空间是使用伙伴分配的算法，其中free list i包含大小为2到幂i的区域。*/
struct mem_pool_struct{
	byte*		buf;		/* memory pool */ /*内存池*/
	ulint		size;		/* memory common pool size */ /*内存公用池大小*/
	ulint		reserved;	/* amount of currently allocated
					memory */ /*当前分配的内存量 */
	mutex_t		mutex;		/* mutex protecting this struct */ /*保护此结构的互斥体*/
	UT_LIST_BASE_NODE_T(mem_area_t)
			free_list[64];	/* lists of free memory areas: an
					area is put to the list whose number
					is the 2-logarithm of the area size */ /*可用内存区域列表：将一个区域放入列表中，其编号为区域大小的2对数*/
};

/* The common memory pool */
/* 公共内存池 */
mem_pool_t*	mem_comm_pool	= NULL;

ulint		mem_out_of_mem_err_msg_count	= 0;

/************************************************************************
Reserves the mem pool mutex. */
/*保留内存池互斥。*/
void
mem_pool_mutex_enter(void)
/*======================*/
{
	mutex_enter(&(mem_comm_pool->mutex));
}

/************************************************************************
Releases the mem pool mutex. */
/*释放内存池互斥。*/
void
mem_pool_mutex_exit(void)
/*=====================*/
{
	mutex_exit(&(mem_comm_pool->mutex));
}

/************************************************************************
Returns memory area size. */
/*返回内存区域大小。*/
UNIV_INLINE
ulint
mem_area_get_size(
/*==============*/
				/* out: size */
	mem_area_t*	area)	/* in: area */
{
	return(area->size_and_free & ~MEM_AREA_FREE);
}

/************************************************************************
Sets memory area size. */
/*设置内存区域大小。*/
UNIV_INLINE
void
mem_area_set_size(
/*==============*/
	mem_area_t*	area,	/* in: area */
	ulint		size)	/* in: size */
{
	area->size_and_free = (area->size_and_free & MEM_AREA_FREE)
				| size;
}

/************************************************************************
Returns memory area free bit. */
/*返回内存区域比特位。*/
UNIV_INLINE
ibool
mem_area_get_free(
/*==============*/
				/* out: TRUE if free */
	mem_area_t*	area)	/* in: area */
{
	ut_ad(TRUE == MEM_AREA_FREE);

	return(area->size_and_free & MEM_AREA_FREE);
}

/************************************************************************
Sets memory area free bit. */
/*设置内存区域空闲比特位。*/
UNIV_INLINE
void
mem_area_set_free(
/*==============*/
	mem_area_t*	area,	/* in: area */
	ibool		free)	/* in: free bit value */
{
	ut_ad(TRUE == MEM_AREA_FREE);
	
	area->size_and_free = (area->size_and_free & ~MEM_AREA_FREE)
				| free;
}

/************************************************************************
Creates a memory pool. */
/* 创建内存池。 */
mem_pool_t*
mem_pool_create(
/*============*/
			/* out: memory pool */
	ulint	size)	/* in: pool size in bytes */
{
	mem_pool_t*	pool;
	mem_area_t*	area;
	ulint		i;
	ulint		used;

	ut_a(size > 10000);
	
	pool = ut_malloc(sizeof(mem_pool_t));

	/* We do not set the memory to zero (FALSE) in the pool,
	but only when allocated at a higher level in mem0mem.c.
	This is to avoid masking useful Purify warnings. */
	/*我们不会将池中的内存设置为零（FALSE），
	但只有在mem0mem.c中分配到更高级别时。
	这是为了避免掩盖有用的Purify警告。 */

	pool->buf = ut_malloc_low(size, FALSE);
	pool->size = size;

	mutex_create(&(pool->mutex));
	mutex_set_level(&(pool->mutex), SYNC_MEM_POOL);

	/* Initialize the free lists */

	for (i = 0; i < 64; i++) {

		UT_LIST_INIT(pool->free_list[i]);
	}

	used = 0;

	while (size - used >= MEM_AREA_MIN_SIZE) {

		i = ut_2_log(size - used);

		if (ut_2_exp(i) > size - used) {

			/* ut_2_log rounds upward */
		
			i--;
		}

		area = (mem_area_t*)(pool->buf + used);

		mem_area_set_size(area, ut_2_exp(i));
		mem_area_set_free(area, TRUE);

		UT_LIST_ADD_FIRST(free_list, pool->free_list[i], area);

		used = used + ut_2_exp(i);
	}

	ut_ad(size >= used);

	pool->reserved = 0;
	
	return(pool);
}

/************************************************************************
Fills the specified free list. */
/*填充指定的空闲列表。*/
static
ibool
mem_pool_fill_free_list(
/*====================*/
				/* out: TRUE if we were able to insert a
				block to the free list */
	ulint		i,	/* in: free list index */
	mem_pool_t*	pool)	/* in: memory pool */
{
	mem_area_t*	area;
	mem_area_t*	area2;
	ibool		ret;

	ut_ad(mutex_own(&(pool->mutex)));

	if (i >= 63) {
		/* We come here when we have run out of space in the
		memory pool: */

		if (mem_out_of_mem_err_msg_count % 1000000000 == 0) {
			/* We do not print the message every time: */

	    		ut_print_timestamp(stderr);
			
			fprintf(stderr,
	"  InnoDB: Out of memory in additional memory pool.\n"
	"InnoDB: InnoDB will start allocating memory from the OS.\n"
	"InnoDB: You may get better performance if you configure a bigger\n"
        "InnoDB: value in the MySQL my.cnf file for\n"
	"InnoDB: innodb_additional_mem_pool_size.\n");
     		}

		mem_out_of_mem_err_msg_count++;
     
		return(FALSE);
	}

	area = UT_LIST_GET_FIRST(pool->free_list[i + 1]);

	if (area == NULL) {
		ret = mem_pool_fill_free_list(i + 1, pool);

		if (ret == FALSE) {
			return(FALSE);
		}

		area = UT_LIST_GET_FIRST(pool->free_list[i + 1]);
	}
	
	UT_LIST_REMOVE(free_list, pool->free_list[i + 1], area);

	area2 = (mem_area_t*)(((byte*)area) + ut_2_exp(i));

	mem_area_set_size(area2, ut_2_exp(i));
	mem_area_set_free(area2, TRUE);

	UT_LIST_ADD_FIRST(free_list, pool->free_list[i], area2);
	
	mem_area_set_size(area, ut_2_exp(i));

	UT_LIST_ADD_FIRST(free_list, pool->free_list[i], area);

	return(TRUE);
}
	
/************************************************************************
Allocates memory from a pool. NOTE: This low-level function should only be
used in mem0mem.*! */
/*从池中分配内存。注意：此低级函数只应用于mem0mem.*！*/
void*
mem_area_alloc(
/*===========*/
				/* out, own: allocated memory buffer */
	ulint		size,	/* in: allocated size in bytes; for optimum
				space usage, the size should be a power of 2
				minus MEM_AREA_EXTRA_SIZE */
	mem_pool_t*	pool)	/* in: memory pool */
{
	mem_area_t*	area;
	ulint		n;
	ibool		ret;

	n = ut_2_log(ut_max(size + MEM_AREA_EXTRA_SIZE, MEM_AREA_MIN_SIZE));

	mutex_enter(&(pool->mutex));

	area = UT_LIST_GET_FIRST(pool->free_list[n]);

	if (area == NULL) {
		ret = mem_pool_fill_free_list(n, pool);

		if (ret == FALSE) {
			/* Out of memory in memory pool: we try to allocate
			from the operating system with the regular malloc: */

			mutex_exit(&(pool->mutex));

			return(ut_malloc(size));
		}

		area = UT_LIST_GET_FIRST(pool->free_list[n]);
	}

	ut_a(mem_area_get_free(area));
	ut_ad(mem_area_get_size(area) == ut_2_exp(n));	

	mem_area_set_free(area, FALSE);
	
	UT_LIST_REMOVE(free_list, pool->free_list[n], area);

	pool->reserved += mem_area_get_size(area);
	
	mutex_exit(&(pool->mutex));

	ut_ad(mem_pool_validate(pool));
	
	return((void*)(MEM_AREA_EXTRA_SIZE + ((byte*)area))); 
}

/************************************************************************
Gets the buddy of an area, if it exists in pool. */
/*获取某个区域的伙伴，如果该区域存在于池中。*/
UNIV_INLINE
mem_area_t*
mem_area_get_buddy(
/*===============*/
				/* out: the buddy, NULL if no buddy in pool */
	mem_area_t*	area,	/* in: memory area */
			ulint		size,	/* in: memory area size */
			mem_pool_t*	pool)	/* in: memory pool */
{
	mem_area_t*	buddy;

	ut_ad(size != 0);

	if (((((byte*)area) - pool->buf) % (2 * size)) == 0) {
	
		/* The buddy is in a higher address */
		/* 伙伴在更高的地址 */
		buddy = (mem_area_t*)(((byte*)area) + size);

		if ((((byte*)buddy) - pool->buf) + size > pool->size) {

			/* The buddy is not wholly contained in the pool:
			there is no buddy */
			/* 伙伴不完全包含在池中：没有兄弟*/
			buddy = NULL;
		}
	} else {
		/* The buddy is in a lower address; NOTE that area cannot
		be at the pool lower end, because then we would end up to
		the upper branch in this if-clause: the remainder would be
		0 */
		/*伙伴的地址较低；请注意，该区域不能在游泳池的低端，因为那样我们就可以if子句中的上分支：余数为0 */
		buddy = (mem_area_t*)(((byte*)area) - size);
	}

	return(buddy);
}

/************************************************************************
Frees memory to a pool. */
/*将内存释放到池中。*/
void
mem_area_free(
/*==========*/
	void*		ptr,	/* in, own: pointer to allocated memory
				buffer */
	mem_pool_t*	pool)	/* in: memory pool */
{
	mem_area_t*	area;
	mem_area_t*	buddy;
	void*		new_ptr;
	ulint		size;
	ulint		n;
	
	if (mem_out_of_mem_err_msg_count > 0) {
		/* It may be that the area was really allocated from the
		OS with regular malloc: check if ptr points within
		our memory pool */
		/*这可能是因为这个区域确实是从有常规malloc的OS分配的具：检查ptr是否在我们的内存池*/
		if ((byte*)ptr < pool->buf
				|| (byte*)ptr >= pool->buf + pool->size) {
			ut_free(ptr);

			return;
		}
	}

	area = (mem_area_t*) (((byte*)ptr) - MEM_AREA_EXTRA_SIZE);

	size = mem_area_get_size(area);
	
	ut_ad(size != 0);
	ut_a(!mem_area_get_free(area));

#ifdef UNIV_LIGHT_MEM_DEBUG	
	if (((byte*)area) + size < pool->buf + pool->size) {

		ulint	next_size;

		next_size = mem_area_get_size(
					(mem_area_t*)(((byte*)area) + size));
		ut_a(ut_2_power_up(next_size) == next_size);
	}
#endif
	buddy = mem_area_get_buddy(area, size, pool);
	
	n = ut_2_log(size);
	
	mutex_enter(&(pool->mutex));

	if (buddy && mem_area_get_free(buddy)
				&& (size == mem_area_get_size(buddy))) {

		/* The buddy is in a free list */

		if ((byte*)buddy < (byte*)area) {
			new_ptr = ((byte*)buddy) + MEM_AREA_EXTRA_SIZE;

			mem_area_set_size(buddy, 2 * size);
			mem_area_set_free(buddy, FALSE);
		} else {
			new_ptr = ptr;

			mem_area_set_size(area, 2 * size);
		}

		/* Remove the buddy from its free list and merge it to area */
		/* 从空闲列表中删除伙伴并将其合并到区域  */
		UT_LIST_REMOVE(free_list, pool->free_list[n], buddy);

		pool->reserved += ut_2_exp(n);

		mutex_exit(&(pool->mutex));

		mem_area_free(new_ptr, pool);

		return;
	} else {
		UT_LIST_ADD_FIRST(free_list, pool->free_list[n], area);

		mem_area_set_free(area, TRUE);

		ut_ad(pool->reserved >= size);

		pool->reserved -= size;
	}
	
	mutex_exit(&(pool->mutex));

	ut_ad(mem_pool_validate(pool));
}

/************************************************************************
Validates a memory pool. */
/* 验证内存池。*/
ibool
mem_pool_validate(
/*==============*/
				/* out: TRUE if ok */
	mem_pool_t*	pool)	/* in: memory pool */
{
	mem_area_t*	area;
	mem_area_t*	buddy;
	ulint		free;
	ulint		i;

	mutex_enter(&(pool->mutex));

	free = 0;
	
	for (i = 0; i < 64; i++) {
	
		UT_LIST_VALIDATE(free_list, mem_area_t, pool->free_list[i]);

		area = UT_LIST_GET_FIRST(pool->free_list[i]);

		while (area != NULL) {
			ut_a(mem_area_get_free(area));
			ut_a(mem_area_get_size(area) == ut_2_exp(i));

			buddy = mem_area_get_buddy(area, ut_2_exp(i), pool);

			ut_a(!buddy || !mem_area_get_free(buddy)
	    		     || (ut_2_exp(i) != mem_area_get_size(buddy)));

			area = UT_LIST_GET_NEXT(free_list, area);

			free += ut_2_exp(i);
		}
	}

	ut_a(free + pool->reserved == pool->size
					- (pool->size % MEM_AREA_MIN_SIZE));
	mutex_exit(&(pool->mutex));

	return(TRUE);
}

/************************************************************************
Prints info of a memory pool. */
/*打印内存池的信息。*/
void
mem_pool_print_info(
/*================*/
	FILE*	        outfile,/* in: output file to write to */
	mem_pool_t*	pool)	/* in: memory pool */
{
	ulint		i;

	mem_pool_validate(pool);

	fprintf(outfile, "INFO OF A MEMORY POOL\n");

	mutex_enter(&(pool->mutex));

	for (i = 0; i < 64; i++) {
		if (UT_LIST_GET_LEN(pool->free_list[i]) > 0) {

			fprintf(outfile,
			  "Free list length %lu for blocks of size %lu\n",
			  UT_LIST_GET_LEN(pool->free_list[i]),
			  ut_2_exp(i));
		}	
	}

	fprintf(outfile, "Pool size %lu, reserved %lu.\n", pool->size,
							pool->reserved);
	mutex_exit(&(pool->mutex));
}

/************************************************************************
Returns the amount of reserved memory. */
/*返回保留内存量。*/
ulint
mem_pool_get_reserved(
/*==================*/
				/* out: reserved memory in bytes */
	mem_pool_t*	pool)	/* in: memory pool */
{
	ulint	reserved;

	mutex_enter(&(pool->mutex));

	reserved = pool->reserved;
	
	mutex_exit(&(pool->mutex));

	return(reserved);
}
