/******************************************************
Index page routines

(c) 1994-1996 Innobase Oy

Created 2/2/1994 Heikki Tuuri
*******************************************************/
/*索引页的例程*/
#define THIS_MODULE
#include "page0page.h"
#ifdef UNIV_NONINL
#include "page0page.ic"
#endif
#undef THIS_MODULE

#include "page0cur.h"
#include "lock0lock.h"
#include "fut0lst.h"
#include "btr0sea.h"

/* A cached template page used in page_create */ /*page_create中使用的缓存模板页面*/
page_t*	page_template	= NULL;

/*			THE INDEX PAGE
			==============
		
The index page consists of a page header which contains the page's
id and other information. On top of it are the the index records
in a heap linked into a one way linear list according to alphabetic order.
索引页由页头组成，页头包含页的id和其他信息。它的顶部是堆中的索引记录，按照字母顺序链接到单向线性列表。
Just below page end is an array of pointers which we call page directory,
to about every sixth record in the list. The pointers are placed in
the directory in the alphabetical order of the records pointed to,
enabling us to make binary search using the array. Each slot n:o I
in the directory points to a record, where a 4-bit field contains a count
of those records which are in the linear list between pointer I and 
the pointer I - 1 in the directory, including the record
pointed to by pointer I and not including the record pointed to by I - 1.
We say that the record pointed to by slot I, or that slot I, owns
these records. The count is always kept in the range 4 to 8, with
the exception that it is 1 for the first slot, and 1--8 for the second slot.  
就在page end下面是一个指针数组，我们称之为页目录，指向列表中大约每6条记录。
指针按所指向记录的字母顺序放置在目录中，使我们能够使用数组进行二进制搜索。
每个槽n:啊,我在目录指向一个记录,4比特字段包含一个计数的那些之间的线性表的记录指针和指针我- 1的目录,
包括记录指针所指向我,不包括记录指出- 1。我们说槽I指向的记录，或者槽I，拥有这些记录。
计数总是保持在4到8的范围内，除了第一个位置的计数为1，第二个位置的计数为1- 8。		
An essentially binary search can be performed in the list of index
records, like we could do if we had pointer to every record in the
page directory. The data structure is, however, more efficient when
we are doing inserts, because most inserts are just pushed on a heap.
Only every 8th insert requires block move in the directory pointer
table, which itself is quite small. A record is deleted from the page
by just taking it off the linear list and updating the number of owned
records-field of the record which owns it, and updating the page directory,
if necessary. A special case is the one when the record owns itself.
Because the overhead of inserts is so small, we may also increase the
page size from the projected default of 8 kB to 64 kB without too
much loss of efficiency in inserts. Bigger page becomes actual
when the disk transfer rate compared to seek and latency time rises.
On the present system, the page size is set so that the page transfer
time (3 ms) is 20 % of the disk random access time (15 ms).
一个本质上的二分搜索可以在索引记录列表中执行，就像我们在页面目录中有指向每条记录的指针时所做的那样。
然而，当我们执行插入时，数据结构更有效率，因为大多数插入只是压入堆。
只有第8次插入才需要在目录指针表中移动块，这本身是非常小的。
从页面中删除记录的方法是，将其从线性列表中删除，并更新拥有记录的记录字段的数量，如果需要，还可以更新页面目录。
一个特殊情况是记录拥有自己。由于插入的开销非常小，我们还可以将页面大小从预计的默认的8 kB增加到64 kB，而不会在插入时损失太多效率。
当磁盘传输速率与查找和延迟时间相比上升时，实际页面会变大。在当前系统中，页面大小被设置为页面传输时间(3 ms)是磁盘随机访问时间(15 ms)的20%。
When the page is split, merged, or becomes full but contains deleted
records, we have to reorganize the page.
当页面被拆分、合并或变成完整但包含删除的记录时，我们必须重新组织页面。
Assuming a page size of 8 kB, a typical index page of a secondary
index contains 300 index entries, and the size of the page directory
is 50 x 4 bytes = 200 bytes. 
假设页面大小为8 kB，二级索引的典型索引页包含300个索引项，页面目录的大小为50 x 4字节= 200字节。
*/

/******************************************************************
Used to check the consistency of a directory slot. */ /*用于检查目录槽位的一致性。*/
static
ibool
page_dir_slot_check(
/*================*/
					/* out: TRUE if succeed */
	page_dir_slot_t*	slot)	/* in: slot */
{
	page_t*	page;
	ulint	n_slots;
	ulint	n_owned;

	ut_a(slot);

	page = buf_frame_align(slot);

	n_slots = page_header_get_field(page, PAGE_N_DIR_SLOTS);

	ut_a(slot <= page_dir_get_nth_slot(page, 0));
	ut_a(slot >= page_dir_get_nth_slot(page, n_slots - 1));

	ut_a(page_rec_check(page + mach_read_from_2(slot)));

	n_owned = rec_get_n_owned(page + mach_read_from_2(slot));

	if (slot == page_dir_get_nth_slot(page, 0)) {
		ut_a(n_owned == 1);
	} else if (slot == page_dir_get_nth_slot(page, n_slots - 1)) {
		ut_a(n_owned >= 1);
		ut_a(n_owned <= PAGE_DIR_SLOT_MAX_N_OWNED);
	} else {
		ut_a(n_owned >= PAGE_DIR_SLOT_MIN_N_OWNED);
		ut_a(n_owned <= PAGE_DIR_SLOT_MAX_N_OWNED);
	}

	return(TRUE);
}

/*****************************************************************
Sets the max trx id field value. */
/*设置最大trx id字段值。*/
void
page_set_max_trx_id(
/*================*/
	page_t*	page,	/* in: page */
	dulint	trx_id)	/* in: transaction id */
{
	buf_block_t*	block;

	ut_ad(page);

	block = buf_block_align(page);

	if (block->is_hashed) {
		rw_lock_x_lock(&btr_search_latch);
	}

	/* It is not necessary to write this change to the redo log, as
	during a database recovery we assume that the max trx id of every
	page is the maximum trx id assigned before the crash. */
	/*没有必要将此更改写入重做日志，因为在数据库恢复期间，
	我们假设每个页面的最大trx id是崩溃前分配的最大trx id。*/
	mach_write_to_8(page + PAGE_HEADER + PAGE_MAX_TRX_ID, trx_id);

	if (block->is_hashed) {
		rw_lock_x_unlock(&btr_search_latch);
	}
}

/****************************************************************
Allocates a block of memory from an index page. */
/*从索引页分配一块内存。*/
byte*
page_mem_alloc(
/*===========*/
			/* out: pointer to start of allocated 
			buffer, or NULL if allocation fails */
	page_t*	page,	/* in: index page */
	ulint	need,	/* in: number of bytes needed */
	ulint*	heap_no)/* out: this contains the heap number
			of the allocated record if allocation succeeds */
{
	rec_t*	rec;
	byte*	block;
	ulint	avl_space;
	ulint	garbage;
	
	ut_ad(page && heap_no);

	/* If there are records in the free list, look if the first is
	big enough */
    /* 如果空闲列表中有记录，请查看第一个记录是否足够大*/
	rec = page_header_get_ptr(page, PAGE_FREE);

	if (rec && (rec_get_size(rec) >= need)) {

		page_header_set_ptr(page, PAGE_FREE, page_rec_get_next(rec));

		garbage = page_header_get_field(page, PAGE_GARBAGE);
		ut_ad(garbage >= need);

		page_header_set_field(page, PAGE_GARBAGE, garbage - need);

		*heap_no = rec_get_heap_no(rec);

		return(rec_get_start(rec));
	}

	/* Could not find space from the free list, try top of heap */
	/*无法从空闲列表中找到空间，试试堆的顶部*/
	avl_space = page_get_max_insert_size(page, 1);
	
	if (avl_space >= need) {
		block = page_header_get_ptr(page, PAGE_HEAP_TOP);

		page_header_set_ptr(page, PAGE_HEAP_TOP, block + need);
		*heap_no = page_header_get_field(page, PAGE_N_HEAP);

		page_header_set_field(page, PAGE_N_HEAP, 1 + *heap_no);

		return(block);
	}

	return(NULL);
}

/**************************************************************
Writes a log record of page creation. */ /*写页面创建的日志记录。*/
UNIV_INLINE
void
page_create_write_log(
/*==================*/
	buf_frame_t*	frame,	/* in: a buffer frame where the page is
				created */ /*创建页面的缓冲区帧*/
	mtr_t*		mtr)	/* in: mini-transaction handle */
{
	mlog_write_initial_log_record(frame, MLOG_PAGE_CREATE, mtr);
}

/***************************************************************
Parses a redo log record of creating a page. */
/*解析创建页面的重做日志记录。*/
byte*
page_parse_create(
/*==============*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr)	/* in: mtr or NULL */
{
	ut_ad(ptr && end_ptr);

	/* The record is empty, except for the record initial part */
    /*记录是空的，除了记录的初始部分*/
	if (page) {
		page_create(page, mtr);
	}

	return(ptr);
}

/**************************************************************
The index page creation function. */
/*索引页创建功能。*/
page_t* 
page_create(
/*========*/
				/* out: pointer to the page */
	buf_frame_t*	frame,	/* in: a buffer frame where the page is
				created */  /*创建页面的缓冲区帧*/
	mtr_t*		mtr)	/* in: mini-transaction handle */
{
	page_dir_slot_t* slot;
	mem_heap_t*	heap;
	dtuple_t*	tuple;	
	dfield_t*	field;
	byte*		heap_top;
	rec_t*		infimum_rec;
	rec_t*		supremum_rec;
	page_t*		page;
	
	ut_ad(frame && mtr);
	ut_ad(PAGE_BTR_IBUF_FREE_LIST + FLST_BASE_NODE_SIZE
	      <= PAGE_DATA);
	ut_ad(PAGE_BTR_IBUF_FREE_LIST_NODE + FLST_NODE_SIZE
	      <= PAGE_DATA);

	/* 1. INCREMENT MODIFY CLOCK */ /*增量修改时钟*/
	buf_frame_modify_clock_inc(frame);

	/* 2. WRITE LOG INFORMATION */ /*写日志信息*/
	page_create_write_log(frame, mtr);
	
	page = frame;

	fil_page_set_type(page, FIL_PAGE_INDEX);

	/* If we have a page template, copy the page structure from there */
    /*如果我们有一个页面模板，从那里复制页面结构*/
	if (page_template) {
		ut_memcpy(page + PAGE_HEADER,
			  page_template + PAGE_HEADER, PAGE_HEADER_PRIV_END);
		ut_memcpy(page + PAGE_DATA,
			  page_template + PAGE_DATA,
			  PAGE_SUPREMUM_END - PAGE_DATA);
		ut_memcpy(page + UNIV_PAGE_SIZE - PAGE_EMPTY_DIR_START,
			page_template + UNIV_PAGE_SIZE - PAGE_EMPTY_DIR_START,
		  		PAGE_EMPTY_DIR_START - PAGE_DIR);
		return(frame);
	}
	
	heap = mem_heap_create(200);
		
	/* 3. CREATE THE INFIMUM AND SUPREMUM RECORDS */
    /* 创建下限值和上限值记录*/
	/* Create first a data tuple for infimum record */
	tuple = dtuple_create(heap, 1);
	field = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(field, "infimum", strlen("infimum") + 1);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 20, 0);
	
	/* Set the corresponding physical record to its place in the page
	record heap */
    /* 将相应的物理记录设置为其在页记录堆中的位置*/
	heap_top = page + PAGE_DATA;
	
	infimum_rec = rec_convert_dtuple_to_rec(heap_top, tuple);

	ut_ad(infimum_rec == page + PAGE_INFIMUM);
	
	rec_set_n_owned(infimum_rec, 1);
	rec_set_heap_no(infimum_rec, 0);
	
	heap_top = rec_get_end(infimum_rec);
		
	/* Create then a tuple for supremum */
    /* 然后为supremum创建一个元组*/
	tuple = dtuple_create(heap, 1);
	field = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(field, "supremum", strlen("supremum") + 1);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 20, 0);

	supremum_rec = rec_convert_dtuple_to_rec(heap_top, tuple);

	ut_ad(supremum_rec == page + PAGE_SUPREMUM);

	rec_set_n_owned(supremum_rec, 1);
	rec_set_heap_no(supremum_rec, 1);
	
	heap_top = rec_get_end(supremum_rec);

	ut_ad(heap_top == page + PAGE_SUPREMUM_END);

	mem_heap_free(heap);

	/* 4. INITIALIZE THE PAGE HEADER */
    /* 初始化页头*/
	page_header_set_field(page, PAGE_N_DIR_SLOTS, 2);
	page_header_set_ptr(page, PAGE_HEAP_TOP, heap_top);
	page_header_set_field(page, PAGE_N_HEAP, 2);
	page_header_set_ptr(page, PAGE_FREE, NULL);
	page_header_set_field(page, PAGE_GARBAGE, 0);
	page_header_set_ptr(page, PAGE_LAST_INSERT, NULL);
	page_header_set_field(page, PAGE_N_RECS, 0);
	page_set_max_trx_id(page, ut_dulint_zero);
	
	/* 5. SET POINTERS IN RECORDS AND DIR SLOTS */
    /* 在记录和目录插槽中设置指针*/
	/* Set the slots to point to infimum and supremum. */
    /*设置插槽指向下限值和上限值。*/
	slot = page_dir_get_nth_slot(page, 0);
	page_dir_slot_set_rec(slot, infimum_rec);

	slot = page_dir_get_nth_slot(page, 1);
	page_dir_slot_set_rec(slot, supremum_rec);

	/* Set next pointers in infimum and supremum */
	/* 将next指针设置为下限值和上限值*/
	rec_set_next_offs(infimum_rec, (ulint)(supremum_rec - page)); 
	rec_set_next_offs(supremum_rec, 0);

	if (page_template == NULL) {
		page_template = mem_alloc(UNIV_PAGE_SIZE);

		ut_memcpy(page_template, page, UNIV_PAGE_SIZE);
	}
	
	return(page);
}

/*****************************************************************
Differs from page_copy_rec_list_end, because this function does not
touch the lock table and max trx id on page. */
/*不同于page_copy_rec_list_end，因为这个函数不接触锁表和页上的最大trx id。*/
void
page_copy_rec_list_end_no_locks(
/*============================*/
	page_t*	new_page,	/* in: index page to copy to */
	page_t*	page,		/* in: index page */
	rec_t*	rec,		/* in: record on page */
	mtr_t*	mtr)		/* in: mtr */
{
	page_cur_t	cur1;
	page_cur_t	cur2;
	rec_t*		sup;

	page_cur_position(rec, &cur1);

	if (page_cur_is_before_first(&cur1)) {

		page_cur_move_to_next(&cur1);
	}
	
	page_cur_set_before_first(new_page, &cur2);
	
	/* Copy records from the original page to the new page */	
    /* 将记录从原始页复制到新页*/
	sup = page_get_supremum_rec(page);
	
	while (sup != page_cur_get_rec(&cur1)) {
		ut_a(
		page_cur_rec_insert(&cur2, page_cur_get_rec(&cur1), mtr));

		page_cur_move_to_next(&cur1);
		page_cur_move_to_next(&cur2);
	}
}	

/*****************************************************************
Copies records from page to new_page, from a given record onward,
including that record. Infimum and supremum records are not copied.
The records are copied to the start of the record list on new_page. */
/*从给定记录(包括该记录)将记录从page复制到new_page。下位记录和上位记录不被复制。
记录被复制到new_page上的记录列表的开头。*/
void
page_copy_rec_list_end(
/*===================*/
	page_t*	new_page,	/* in: index page to copy to */
	page_t*	page,		/* in: index page */
	rec_t*	rec,		/* in: record on page */
	mtr_t*	mtr)		/* in: mtr */
{
	if (page_header_get_field(new_page, PAGE_N_HEAP) == 2) {
		page_copy_rec_list_end_to_created_page(new_page, page, rec,
									mtr);
	} else {
		page_copy_rec_list_end_no_locks(new_page, page, rec, mtr);
	}

	/* Update the lock table, MAX_TRX_ID, and possible hash index */
    /* 更新锁表、MAX_TRX_ID和可能的哈希索引*/
	lock_move_rec_list_end(new_page, page, rec);

	page_update_max_trx_id(new_page, page_get_max_trx_id(page));

	btr_search_move_or_delete_hash_entries(new_page, page);
}	

/*****************************************************************
Copies records from page to new_page, up to the given record,
NOT including that record. Infimum and supremum records are not copied.
The records are copied to the end of the record list on new_page. */
/*将记录从page复制到new_page，直到给定的记录，不包括该记录。
下位记录和上位记录不被复制。记录被复制到new_page上记录列表的末尾。*/
void
page_copy_rec_list_start(
/*=====================*/
	page_t*	new_page,	/* in: index page to copy to */
	page_t*	page,		/* in: index page */
	rec_t*	rec,		/* in: record on page */
	mtr_t*	mtr)		/* in: mtr */
{
	page_cur_t	cur1;
	page_cur_t	cur2;
	rec_t*		old_end;

	page_cur_set_before_first(page, &cur1);

	if (rec == page_cur_get_rec(&cur1)) {

		return;
	}

	page_cur_move_to_next(&cur1);
	
	page_cur_set_after_last(new_page, &cur2);
	page_cur_move_to_prev(&cur2);
	old_end = page_cur_get_rec(&cur2);
	
	/* Copy records from the original page to the new page */	
    /*将记录从原始页复制到新页*/
	while (page_cur_get_rec(&cur1) != rec) {
		ut_a(
		page_cur_rec_insert(&cur2, page_cur_get_rec(&cur1), mtr));

		page_cur_move_to_next(&cur1);
		page_cur_move_to_next(&cur2);
	}

	/* Update the lock table, MAX_TRX_ID, and possible hash index */
	/* 更新锁表、MAX_TRX_ID和可能的哈希索引*/
	lock_move_rec_list_start(new_page, page, rec, old_end);

	page_update_max_trx_id(new_page, page_get_max_trx_id(page));

	btr_search_move_or_delete_hash_entries(new_page, page);	
}	

/**************************************************************
Writes a log record of a record list end or start deletion. */
/*写记录列表结束或开始删除的日志记录。*/
UNIV_INLINE
void
page_delete_rec_list_write_log(
/*===========================*/
	page_t*	page,	/* in: index page */
	rec_t*	rec,	/* in: record on page */
	byte	type,	/* in: operation type: MLOG_LIST_END_DELETE, ... */
	mtr_t*	mtr)	/* in: mtr */
{
	ut_ad((type == MLOG_LIST_END_DELETE)
					|| (type == MLOG_LIST_START_DELETE));

	mlog_write_initial_log_record(page, type, mtr);

	/* Write the parameter as a 2-byte ulint */
	mlog_catenate_ulint(mtr, rec - page, MLOG_2BYTES);
}

/**************************************************************
Parses a log record of a record list end or start deletion. */
/*解析记录列表结束或开始删除的日志记录。*/
byte*
page_parse_delete_rec_list(
/*=======================*/
			/* out: end of log record or NULL */
	byte	type,	/* in: MLOG_LIST_END_DELETE or MLOG_LIST_START_DELETE */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */	
	mtr_t*	mtr)	/* in: mtr or NULL */
{
	ulint	offset;
	
	ut_ad((type == MLOG_LIST_END_DELETE)
	      || (type == MLOG_LIST_START_DELETE)); 
	      
	/* Read the record offset as a 2-byte ulint */
    /* 读取记录偏移量作为一个2字节的ulint*/
	if (end_ptr < ptr + 2) {

		return(NULL);
	}
	
	offset = mach_read_from_2(ptr);
	ptr += 2;

	if (!page) {

		return(ptr);
	}

	if (type == MLOG_LIST_END_DELETE) {
		page_delete_rec_list_end(page, page + offset, ULINT_UNDEFINED,
							ULINT_UNDEFINED, mtr);
	} else {
		page_delete_rec_list_start(page, page + offset, mtr);
	}

	return(ptr);
}

/*****************************************************************
Deletes records from a page from a given record onward, including that record.
The infimum and supremum records are not deleted. */
/*从一个给定记录开始从一个页面删除记录，包括该记录。下限值和上限值不被删除。*/
void
page_delete_rec_list_end(
/*=====================*/
	page_t*	page,	/* in: index page */
	rec_t*	rec,	/* in: record on page */
	ulint	n_recs,	/* in: number of records to delete, or ULINT_UNDEFINED
			if not known */
	ulint	size,	/* in: the sum of the sizes of the records in the end
			of the chain to delete, or ULINT_UNDEFINED if not
			known */
	mtr_t*	mtr)	/* in: mtr */
{
	page_dir_slot_t* slot;
	ulint	slot_index;
	rec_t*	last_rec;
	rec_t*	prev_rec;
	rec_t*	free;
	rec_t*	rec2;
	ulint	count;
	ulint	n_owned;
	rec_t*	sup;

	/* Reset the last insert info in the page header and increment
	the modify clock for the frame */
    /*重置页眉中的最后一个插入信息，并增加帧的修改时钟*/
	page_header_set_ptr(page, PAGE_LAST_INSERT, NULL);

	/* The page gets invalid for optimistic searches: increment the
	frame modify clock */
    /*页面对乐观搜索无效:增加帧修改时钟*/
	buf_frame_modify_clock_inc(page);
	
	sup = page_get_supremum_rec(page);
	
	if (rec == page_get_infimum_rec(page)) {
		rec = page_rec_get_next(rec);
	}

	page_delete_rec_list_write_log(page, rec, MLOG_LIST_END_DELETE, mtr);

	if (rec == sup) {

		return;
	}
	
	prev_rec = page_rec_get_prev(rec);

	last_rec = page_rec_get_prev(sup);

	if ((size == ULINT_UNDEFINED) || (n_recs == ULINT_UNDEFINED)) {
		/* Calculate the sum of sizes and the number of records *//*计算大小和记录数量的总和*/
		size = 0;
		n_recs = 0;
		rec2 = rec;

		while (rec2 != sup) {
			size += rec_get_size(rec2);
			n_recs++;

			rec2 = page_rec_get_next(rec2);
		}
	}

	/* Update the page directory; there is no need to balance the number
	of the records owned by the supremum record, as it is allowed to be
	less than PAGE_DIR_SLOT_MIN_N_OWNED */
	/*更新页面目录;不需要平衡上一条记录拥有的记录数，因为允许它小于PAGE_DIR_SLOT_MIN_N_OWNED*/
	rec2 = rec;
	count = 0;
	
	while (rec_get_n_owned(rec2) == 0) {
		count++;

		rec2 = page_rec_get_next(rec2);
	}

	ut_ad(rec_get_n_owned(rec2) - count > 0);

	n_owned = rec_get_n_owned(rec2) - count;
	
	slot_index = page_dir_find_owner_slot(rec2);
	slot = page_dir_get_nth_slot(page, slot_index);
	
	page_dir_slot_set_rec(slot, sup);
	page_dir_slot_set_n_owned(slot, n_owned);

	page_header_set_field(page, PAGE_N_DIR_SLOTS, slot_index + 1);
	
	/* Remove the record chain segment from the record chain */ /*从记录链中删除记录链段*/
	page_rec_set_next(prev_rec, page_get_supremum_rec(page));

	/* Catenate the deleted chain segment to the page free list */ /*将已删除的链段连接到页面空闲列表*/

	free = page_header_get_ptr(page, PAGE_FREE);

	page_rec_set_next(last_rec, free);
	page_header_set_ptr(page, PAGE_FREE, rec);

	page_header_set_field(page, PAGE_GARBAGE,
			size + page_header_get_field(page, PAGE_GARBAGE));

	page_header_set_field(page, PAGE_N_RECS,
				(ulint)(page_get_n_recs(page) - n_recs));
}	

/*****************************************************************
Deletes records from page, up to the given record, NOT including
that record. Infimum and supremum records are not deleted. */
/*从页面删除记录，直到给定的记录，不包括该记录。下位记录和上位记录不会被删除。*/
void
page_delete_rec_list_start(
/*=======================*/
	page_t*	page,	/* in: index page */
	rec_t*	rec,	/* in: record on page */
	mtr_t*	mtr)	/* in: mtr */
{
	page_cur_t	cur1;
	ulint		log_mode;

	page_delete_rec_list_write_log(page, rec, MLOG_LIST_START_DELETE, mtr);

	page_cur_set_before_first(page, &cur1);

	if (rec == page_cur_get_rec(&cur1)) {

		return;
	}

	page_cur_move_to_next(&cur1);
	
	/* Individual deletes are not logged */
    /* 单个删除不会被记录*/
	log_mode = mtr_set_log_mode(mtr, MTR_LOG_NONE);

	while (page_cur_get_rec(&cur1) != rec) {

		page_cur_delete_rec(&cur1, mtr);
	}

	/* Restore log mode */
    /*恢复日志模式*/
	mtr_set_log_mode(mtr, log_mode);
}	

/*****************************************************************
Moves record list end to another page. Moved records include
split_rec. */
/*移动记录列表到另一个页面。移动的记录包括split_rec。*/
void
page_move_rec_list_end(
/*===================*/
	page_t*	new_page,	/* in: index page where to move */
	page_t*	page,		/* in: index page */
	rec_t*	split_rec,	/* in: first record to move */
	mtr_t*	mtr)		/* in: mtr */
{
	ulint	old_data_size;
	ulint	new_data_size;
	ulint	old_n_recs;
	ulint	new_n_recs;

	old_data_size = page_get_data_size(new_page);
	old_n_recs = page_get_n_recs(new_page);
	
	page_copy_rec_list_end(new_page, page, split_rec, mtr);

	new_data_size = page_get_data_size(new_page);
	new_n_recs = page_get_n_recs(new_page);

	ut_ad(new_data_size >= old_data_size);

	page_delete_rec_list_end(page, split_rec, new_n_recs - old_n_recs,
					new_data_size - old_data_size, mtr);
}

/*****************************************************************
Moves record list start to another page. Moved records do not include
split_rec. */
/*移动记录列表开始到另一个页面。移动的记录不包括split_rec。*/
void
page_move_rec_list_start(
/*=====================*/
	page_t*	new_page,	/* in: index page where to move */
	page_t*	page,		/* in: index page */
	rec_t*	split_rec,	/* in: first record not to move */
	mtr_t*	mtr)		/* in: mtr */
{
	page_copy_rec_list_start(new_page, page, split_rec, mtr);

	page_delete_rec_list_start(page, split_rec, mtr);
}

/***************************************************************************
This is a low-level operation which is used in a database index creation
to update the page number of a created B-tree to a data dictionary record. */
/*这是一种低级操作，用于创建数据库索引，将已创建的b -树的页码更新为数据字典记录。*/
void
page_rec_write_index_page_no(
/*=========================*/
	rec_t*	rec,	/* in: record to update */
	ulint	i,	/* in: index of the field to update */
	ulint	page_no,/* in: value to write */
	mtr_t*	mtr)	/* in: mtr */
{
	byte*	data;
	ulint	len;
	
	data = rec_get_nth_field(rec, i, &len);

	ut_ad(len == 4);

	mlog_write_ulint(data, page_no, MLOG_4BYTES, mtr);
}

/******************************************************************
Used to delete n slots from the directory. This function updates
also n_owned fields in the records, so that the first slot after
the deleted ones inherits the records of the deleted slots. */
/*从目录中删除n个槽位。这个函数还更新记录中n_owned的字段，以便在被删除的槽之后的第一个槽继承被删除槽的记录。*/
UNIV_INLINE
void
page_dir_delete_slots(
/*==================*/
	page_t*	page,	/* in: the index page */
	ulint	start,	/* in: first slot to be deleted */
	ulint	n)	/* in: number of slots to delete (currently 
			only n == 1 allowed) */
{
	page_dir_slot_t*	slot;
	ulint			i;
	ulint			sum_owned = 0;
	ulint			n_slots;
	rec_t*			rec;

	ut_ad(n == 1);	
	ut_ad(start > 0);
	ut_ad(start + n < page_dir_get_n_slots(page));

	n_slots = page_dir_get_n_slots(page);

	/* 1. Reset the n_owned fields of the slots to be
	deleted */ /*1.重置待删除槽位的n_owned字段*/
	for (i = start; i < start + n; i++) {
		slot = page_dir_get_nth_slot(page, i);
		sum_owned += page_dir_slot_get_n_owned(slot);
		page_dir_slot_set_n_owned(slot, 0);
	}

	/* 2. Update the n_owned value of the first non-deleted slot */
	/*2. 更新第一个未删除槽位的n_owned值*/
	slot = page_dir_get_nth_slot(page, start + n);
	page_dir_slot_set_n_owned(slot,
				sum_owned + page_dir_slot_get_n_owned(slot));

	/* 3. Destroy start and other slots by copying slots */ 
	/*3.通过复制插槽来破坏start和其他插槽*/
	for (i = start + n; i < n_slots; i++) {
		slot = page_dir_get_nth_slot(page, i);
		rec = page_dir_slot_get_rec(slot);

		slot = page_dir_get_nth_slot(page, i - n);
		page_dir_slot_set_rec(slot, rec);
	}

	/* 4. Update the page header */
	/*4. 更新页眉*/
	page_header_set_field(page, PAGE_N_DIR_SLOTS, n_slots - n);
}

/******************************************************************
Used to add n slots to the directory. Does not set the record pointers
in the added slots or update n_owned values: this is the responsibility
of the caller. */
/*用于向目录中添加n个槽位。在添加的槽中不设置记录指针或更新n_owned值:这是调用者的责任。*/
UNIV_INLINE
void
page_dir_add_slots(
/*===============*/
	page_t*	page,	/* in: the index page */
	ulint	start,	/* in: the slot above which the new slots are added */
	ulint	n)	/* in: number of slots to add (currently only n == 1 
			allowed) */
{
	page_dir_slot_t*	slot;
	ulint			n_slots;
	ulint			i;
	rec_t*			rec;

	ut_ad(n == 1);
	
	n_slots = page_dir_get_n_slots(page);

	ut_ad(start < n_slots - 1);

	/* Update the page header */ /*更新页眉*/
	page_header_set_field(page, PAGE_N_DIR_SLOTS, n_slots + n);

	/* Move slots up */ /*移动槽了*/

	for (i = n_slots - 1; i > start; i--) {

		slot = page_dir_get_nth_slot(page, i);
		rec = page_dir_slot_get_rec(slot);

		slot = page_dir_get_nth_slot(page, i + n);
		page_dir_slot_set_rec(slot, rec);
	}
}

/********************************************************************
Splits a directory slot which owns too many records. */
/*分割拥有过多记录的目录槽。*/
void
page_dir_split_slot(
/*================*/
	page_t*	page,		/* in: the index page in question */
	ulint	slot_no)	/* in: the directory slot */
{		
	rec_t*			rec;
	page_dir_slot_t*	new_slot;
	page_dir_slot_t*	prev_slot;
	page_dir_slot_t*	slot;
	ulint			i;
	ulint			n_owned;

	ut_ad(page);
	ut_ad(slot_no > 0);

	slot = page_dir_get_nth_slot(page, slot_no);
	
	n_owned = page_dir_slot_get_n_owned(slot);
	ut_ad(n_owned == PAGE_DIR_SLOT_MAX_N_OWNED + 1);

	/* 1. We loop to find a record approximately in the middle of the 
	records owned by the slot. */
	/*1. 我们循环查找一个记录，它大约位于槽拥有的记录的中间。*/
	prev_slot = page_dir_get_nth_slot(page, slot_no - 1);
	rec = page_dir_slot_get_rec(prev_slot);

	for (i = 0; i < n_owned / 2; i++) {
		rec = page_rec_get_next(rec);
	}

	ut_ad(n_owned / 2 >= PAGE_DIR_SLOT_MIN_N_OWNED);

	/* 2. We add one directory slot immediately below the slot to be
	split. */
    /*2. 我们在要拆分的槽的正下方添加一个目录槽。*/
	page_dir_add_slots(page, slot_no - 1, 1);

	/* The added slot is now number slot_no, and the old slot is
	now number slot_no + 1 */
    /*添加的槽位现在是slot_no号，旧的槽位现在是slot_no + 1号*/
	new_slot = page_dir_get_nth_slot(page, slot_no);
	slot = page_dir_get_nth_slot(page, slot_no + 1);

	/* 3. We store the appropriate values to the new slot. */
	/*3.我们将适当的值存储到新槽中。*/
	page_dir_slot_set_rec(new_slot, rec);
	page_dir_slot_set_n_owned(new_slot, n_owned / 2);
	
	/* 4. Finally, we update the number of records field of the 
	original slot */
    /*4. 最后，我们更新原始槽的记录数字段*/
	page_dir_slot_set_n_owned(slot, n_owned - (n_owned / 2));
}

/*****************************************************************
Tries to balance the given directory slot with too few records with the upper
neighbor, so that there are at least the minimum number of records owned by
the slot; this may result in the merging of two slots. */
/*尝试与上层邻居平衡记录过少的给定目录槽，以便至少有最小数量的记录属于该槽;这可能导致两个槽的合并。*/
void
page_dir_balance_slot(
/*==================*/
	page_t*	page,		/* in: index page */
	ulint	slot_no) 	/* in: the directory slot */
{
	page_dir_slot_t*	slot;
	page_dir_slot_t*	up_slot;
	ulint			n_owned;
	ulint			up_n_owned;
	rec_t*			old_rec;
	rec_t*			new_rec;

	ut_ad(page);
	ut_ad(slot_no > 0);

	slot = page_dir_get_nth_slot(page, slot_no);
	
	/* The last directory slot cannot be balanced with the upper
	neighbor, as there is none. */
    /*最后一个目录槽位不能与上面的邻居进行均衡，因为上面没有目录槽位。*/
	if (slot_no == page_dir_get_n_slots(page) - 1) {

		return;
	}
	
	up_slot = page_dir_get_nth_slot(page, slot_no + 1);
		
	n_owned = page_dir_slot_get_n_owned(slot);
	up_n_owned = page_dir_slot_get_n_owned(up_slot);
	
	ut_ad(n_owned == PAGE_DIR_SLOT_MIN_N_OWNED - 1);

	/* If the upper slot has the minimum value of n_owned, we will merge
	the two slots, therefore we assert: */ /*如果上面的槽有最小的n_owned值，我们将合并这两个槽，因此我们断言:*/
	ut_ad(2 * PAGE_DIR_SLOT_MIN_N_OWNED - 1 <= PAGE_DIR_SLOT_MAX_N_OWNED);
	
	if (up_n_owned > PAGE_DIR_SLOT_MIN_N_OWNED) {

		/* In this case we can just transfer one record owned
		by the upper slot to the property of the lower slot */
		/*在这种情况下，我们只需要将上面槽拥有的一条记录转移到下面槽的属性中*/
		old_rec = page_dir_slot_get_rec(slot);
		new_rec = page_rec_get_next(old_rec);
		
		rec_set_n_owned(old_rec, 0);
		rec_set_n_owned(new_rec, n_owned + 1);
		
		page_dir_slot_set_rec(slot, new_rec);
		
		page_dir_slot_set_n_owned(up_slot, up_n_owned -1);
	} else {
		/* In this case we may merge the two slots */
		/*在这种情况下，我们可以合并两个槽*/
		page_dir_delete_slots(page, slot_no, 1);
	}		
}

/****************************************************************
Returns the middle record of the record list. If there are an even number
of records in the list, returns the first record of the upper half-list. */
/*返回记录列表的中间记录。如果列表中有偶数条记录，则返回上半列表的第一条记录。*/
rec_t*
page_get_middle_rec(
/*================*/
			/* out: middle record */
	page_t*	page)	/* in: page */
{
	page_dir_slot_t*	slot;
	ulint			middle;
	ulint			i;
	ulint			n_owned;
	ulint			count;
	rec_t*			rec;

	/* This many records we must leave behind */ /*我们必须留下这么多记录*/
	middle = (page_get_n_recs(page) + 2) / 2;

	count = 0;

	for (i = 0;; i++) {

		slot = page_dir_get_nth_slot(page, i);
		n_owned = page_dir_slot_get_n_owned(slot);

		if (count + n_owned > middle) {
			break;
		} else {
			count += n_owned;
		}
	}

	ut_ad(i > 0);
	slot = page_dir_get_nth_slot(page, i - 1);
	rec = page_dir_slot_get_rec(slot);
	rec = page_rec_get_next(rec);

	/* There are now count records behind rec */
    /* 现在有计数记录落后于rec*/
	for (i = 0; i < middle - count; i++) {
		rec = page_rec_get_next(rec);
	}

	return(rec);
}
	
/*******************************************************************
Returns the number of records before the given record in chain.
The number includes infimum and supremum records. */
/*返回链中给定记录之前的记录数。该数字包括下限值和上限值记录。*/
ulint
page_rec_get_n_recs_before(
/*=======================*/
			/* out: number of records */
	rec_t*	rec)	/* in: the physical record */
{
	page_dir_slot_t*	slot;
	rec_t*			slot_rec;
	page_t*			page;
	ulint			i;
	lint			n	= 0;

	ut_ad(page_rec_check(rec));

	page = buf_frame_align(rec);
	
	while (rec_get_n_owned(rec) == 0) {

		rec = page_rec_get_next(rec);
		n--;
	}
	
	for (i = 0; ; i++) {
		slot = page_dir_get_nth_slot(page, i);
		slot_rec = page_dir_slot_get_rec(slot);

		n += rec_get_n_owned(slot_rec);

		if (rec == slot_rec) {

			break;
		}
	}

	n--;

	ut_ad(n >= 0);

	return((ulint) n);
}

/****************************************************************
Prints record contents including the data relevant only in
the index page context. */
/*打印记录内容，包括仅在索引页上下文中相关的数据。*/
void
page_rec_print(
/*===========*/
	rec_t*	rec)
{
	rec_print(rec);
	printf(
     		"            n_owned: %lu; heap_no: %lu; next rec: %lu\n",
		rec_get_n_owned(rec),
		rec_get_heap_no(rec),
		rec_get_next_offs(rec));

	page_rec_check(rec);
	rec_validate(rec);
}

/*******************************************************************
This is used to print the contents of the directory for
debugging purposes. */
/*它用于打印目录的内容，以供调试之用。*/
void
page_dir_print(
/*===========*/
	page_t*	page,	/* in: index page */
	ulint	pr_n)	/* in: print n first and n last entries */
{
	ulint			n;
	ulint			i;
	page_dir_slot_t*	slot;

	n = page_dir_get_n_slots(page);
	
	printf("--------------------------------\n");
	printf("PAGE DIRECTORY\n");
	printf("Page address %lx\n", (ulint)page);
	printf("Directory stack top at offs: %lu; number of slots: %lu\n", 
		(ulint)(page_dir_get_nth_slot(page, n - 1) - page), n);
	for (i = 0; i < n; i++) {
		slot = page_dir_get_nth_slot(page, i);
		if ((i == pr_n) && (i < n - pr_n)) {
			printf("    ...   \n");
		}
	    	if ((i < pr_n) || (i >= n - pr_n)) {
	   		printf(
	   	   "Contents of slot: %lu: n_owned: %lu, rec offs: %lu\n",
			i, page_dir_slot_get_n_owned(slot),
			(ulint)(page_dir_slot_get_rec(slot) - page));
	    	}
	}
	printf("Total of %lu records\n", 2 + page_get_n_recs(page));	
	printf("--------------------------------\n");
}	
	
/*******************************************************************
This is used to print the contents of the page record list for
debugging purposes. */
/*它用于打印页面记录列表的内容，以供调试之用。*/
void
page_print_list(
/*============*/
	page_t*	page,	/* in: index page */
	ulint	pr_n)	/* in: print n first and n last entries */
{
	page_cur_t	cur;
	rec_t*		rec;
	ulint		count;
	ulint		n_recs;

	printf("--------------------------------\n");
	printf("PAGE RECORD LIST\n");
	printf("Page address %lu\n", (ulint)page);

	n_recs = page_get_n_recs(page);

	page_cur_set_before_first(page, &cur);
	count = 0;
	for (;;) {
		rec = (&cur)->rec;
		page_rec_print(rec);

		if (count == pr_n) {
			break;
		}	
		if (page_cur_is_after_last(&cur)) {
			break;
		}	
		page_cur_move_to_next(&cur);
		count++;	
	}
	
	if (n_recs > 2 * pr_n) {
		printf(" ... \n");
	}
	
	for (;;) {
		if (page_cur_is_after_last(&cur)) {
			break;
		}	
		page_cur_move_to_next(&cur);

		if (count + pr_n >= n_recs) {	
			rec = (&cur)->rec;
			page_rec_print(rec);
		}
		count++;	
	}

	printf("Total of %lu records \n", count + 1);	
	printf("--------------------------------\n");
}	

/*******************************************************************
Prints the info in a page header. */
/*打印页眉中的信息。*/
void
page_header_print(
/*==============*/
	page_t*	page)
{
	printf("--------------------------------\n");
	printf("PAGE HEADER INFO\n");
	printf("Page address %lx, n records %lu\n", (ulint)page,
		page_header_get_field(page, PAGE_N_RECS));

	printf("n dir slots %lu, heap top %lu\n",
		page_header_get_field(page, PAGE_N_DIR_SLOTS),
		page_header_get_field(page, PAGE_HEAP_TOP));

	printf("Page n heap %lu, free %lu, garbage %lu\n",
		page_header_get_field(page, PAGE_N_HEAP),
		page_header_get_field(page, PAGE_FREE),
		page_header_get_field(page, PAGE_GARBAGE));

	printf("Page last insert %lu, direction %lu, n direction %lu\n",
		page_header_get_field(page, PAGE_LAST_INSERT),
		page_header_get_field(page, PAGE_DIRECTION),
		page_header_get_field(page, PAGE_N_DIRECTION));
}

/*******************************************************************
This is used to print the contents of the page for
debugging purposes. */
/*它用于打印页面的内容以供调试之用。*/
void
page_print(
/*======*/
	page_t*	page,	/* in: index page */
	ulint	dn,	/* in: print dn first and last entries in directory */
	ulint	rn)	/* in: print rn first and last records on page */
{
	page_header_print(page);
	page_dir_print(page, dn);
	page_print_list(page, rn);
}	

/*******************************************************************
The following is used to validate a record on a page. This function
differs from rec_validate as it can also check the n_owned field and
the heap_no field. */
/*下面的代码用于验证页面上的记录。这个函数与rec_validate不同，因为它也可以检查n_owned字段和heap_no字段。*/
ibool
page_rec_validate(
/*==============*/
			/* out: TRUE if ok */
	rec_t* 	rec)	/* in: record on the page */
{
	ulint	n_owned;
	ulint	heap_no;
	page_t* page;

	page = buf_frame_align(rec);

	page_rec_check(rec);
	rec_validate(rec);

	n_owned = rec_get_n_owned(rec);
	heap_no = rec_get_heap_no(rec);

	if (!(n_owned <= PAGE_DIR_SLOT_MAX_N_OWNED)) {
		fprintf(stderr, "Dir slot n owned too big %lu\n", n_owned);
		return(FALSE);
	}

	if (!(heap_no < page_header_get_field(page, PAGE_N_HEAP))) {
		fprintf(stderr, "Heap no too big %lu %lu\n", heap_no,
				page_header_get_field(page, PAGE_N_HEAP));
		return(FALSE);
	}
	
	return(TRUE);
}
	
/*******************************************************************
This function checks the consistency of an index page. */
/*这个函数检查索引页的一致性。*/
ibool
page_validate(
/*==========*/
				/* out: TRUE if ok */
	page_t*		page,	/* in: index page */
	dict_index_t*	index)	/* in: data dictionary index containing
				the page record type definition */ /*包含页记录类型定义的数据字典索引*/
{
	page_dir_slot_t* slot;
	mem_heap_t*	heap;
	page_cur_t 	cur;
	byte*		buf;
	ulint		count;
	ulint		own_count;
	ulint		slot_no;
	ulint		data_size;
	rec_t*		rec;
	rec_t*		old_rec	= NULL;
	ulint		offs;
	ulint		n_slots;
	ibool		ret	= FALSE;
	ulint		i;
	char           	err_buf[1000];
	
	heap = mem_heap_create(UNIV_PAGE_SIZE);
	
	/* The following buffer is used to check that the
	records in the page record heap do not overlap */

	buf = mem_heap_alloc(heap, UNIV_PAGE_SIZE);
	for (i = 0; i < UNIV_PAGE_SIZE; i++) {
		buf[i] = 0;
	}

	/* Check first that the record heap and the directory do not
	overlap. */

	n_slots = page_dir_get_n_slots(page);

	if (!(page_header_get_ptr(page, PAGE_HEAP_TOP) <=
			page_dir_get_nth_slot(page, n_slots - 1))) {
		fprintf(stderr,
       	"Record heap and dir overlap on a page in index %s, %lu, %lu\n",
       		index->name, (ulint)page_header_get_ptr(page, PAGE_HEAP_TOP),
       		(ulint)page_dir_get_nth_slot(page, n_slots - 1));

       		goto func_exit;
       	}

	/* Validate the record list in a loop checking also that
	it is consistent with the directory. */
	count = 0;
	data_size = 0;
	own_count = 1;
	slot_no = 0;
	slot = page_dir_get_nth_slot(page, slot_no);

	page_cur_set_before_first(page, &cur);

	for (;;) {
		rec = (&cur)->rec;

		if (!page_rec_validate(rec)) {
			goto func_exit;
		}
		
		/* Check that the records are in the ascending order */
		if ((count >= 2) && (!page_cur_is_after_last(&cur))) {
			if (!(1 == cmp_rec_rec(rec, old_rec, index))) {
				fprintf(stderr,
					"Records in wrong order in index %s\n",
					index->name);
	 		 	rec_sprintf(err_buf, 900, old_rec);
	  			fprintf(stderr, "InnoDB: record %s\n", err_buf);
				
	 		 	rec_sprintf(err_buf, 900, rec);
	  			fprintf(stderr, "InnoDB: record %s\n", err_buf);
				
				goto func_exit;
			}
		}

		if ((rec != page_get_supremum_rec(page))
		    && (rec != page_get_infimum_rec(page))) {

			data_size += rec_get_size(rec);
		}
		
		offs = rec_get_start(rec) - page;
		
		for (i = 0; i < rec_get_size(rec); i++) {
			if (!buf[offs + i] == 0) {
				/* No other record may overlap this */

				fprintf(stderr,
				"Record overlaps another in index %s \n",
				index->name);

				goto func_exit;
			}
				
			buf[offs + i] = 1;
		}
		
		if (rec_get_n_owned(rec) != 0) {
			/* This is a record pointed to by a dir slot */
			if (rec_get_n_owned(rec) != own_count) {
				fprintf(stderr,
				"Wrong owned count %lu, %lu, in index %s\n",
				rec_get_n_owned(rec), own_count,
				index->name);

				goto func_exit;
			}

			if (page_dir_slot_get_rec(slot) != rec) {
				fprintf(stderr,
				"Dir slot does not point to right rec in %s\n",
				index->name);

				goto func_exit;
			}
			
			page_dir_slot_check(slot);
			
			own_count = 0;
			if (!page_cur_is_after_last(&cur)) {
				slot_no++;
				slot = page_dir_get_nth_slot(page, slot_no);
			}
		}

		if (page_cur_is_after_last(&cur)) {
			break;
		}

		if (rec_get_next_offs(rec) < FIL_PAGE_DATA
				|| rec_get_next_offs(rec) >= UNIV_PAGE_SIZE) {
			fprintf(stderr,
			  "Next record offset wrong %lu in index %s\n",
			  rec_get_next_offs(rec), index->name);

			goto func_exit;
		}

		count++;		
		page_cur_move_to_next(&cur);
		own_count++;
		old_rec = rec;
	}
	
	if (rec_get_n_owned(rec) == 0) {
		fprintf(stderr, "n owned is zero in index %s\n", index->name);

		goto func_exit;
	}
		
	if (slot_no != n_slots - 1) {
		fprintf(stderr, "n slots wrong %lu %lu in index %s\n",
			slot_no, n_slots - 1, index->name);
		goto func_exit;
	}		

	if (page_header_get_field(page, PAGE_N_RECS) + 2 != count + 1) {
		fprintf(stderr, "n recs wrong %lu %lu in index %s\n",
		page_header_get_field(page, PAGE_N_RECS) + 2,  count + 1,
		index->name);

		goto func_exit;
	}

	if (data_size != page_get_data_size(page)) {
		fprintf(stderr, "Summed data size %lu, returned by func %lu\n",
			data_size, page_get_data_size(page));
		goto func_exit;
	}

	/* Check then the free list */
	rec = page_header_get_ptr(page, PAGE_FREE);

	while (rec != NULL) {
		if (!page_rec_validate(rec)) {

			goto func_exit;
		}
		
		count++;	
		offs = rec_get_start(rec) - page;
		
		for (i = 0; i < rec_get_size(rec); i++) {

			if (buf[offs + i] != 0) {
				fprintf(stderr,
	                "Record overlaps another in free list, index %s \n",
				index->name);

				goto func_exit;
			}
				
			buf[offs + i] = 1;
		}
		
		rec = page_rec_get_next(rec);
	}
	
	if (page_header_get_field(page, PAGE_N_HEAP) != count + 1) {

		fprintf(stderr, "N heap is wrong %lu %lu in index %s\n",
		page_header_get_field(page, PAGE_N_HEAP), count + 1,
		index->name);
	}

	ret = TRUE;	

func_exit:
	mem_heap_free(heap);

	return(ret);			  
}

/*******************************************************************
Looks in the page record list for a record with the given heap number. */
/*在页记录列表中查找具有给定堆号的记录。*/
rec_t*
page_find_rec_with_heap_no(
/*=======================*/
			/* out: record, NULL if not found */
	page_t*	page,	/* in: index page */
	ulint	heap_no)/* in: heap number */
{
	page_cur_t	cur;
	rec_t*		rec;

	page_cur_set_before_first(page, &cur);

	for (;;) {
		rec = (&cur)->rec;

		if (rec_get_heap_no(rec) == heap_no) {

			return(rec);
		}

		if (page_cur_is_after_last(&cur)) {

			return(NULL);
		}	

		page_cur_move_to_next(&cur);
	}
}
