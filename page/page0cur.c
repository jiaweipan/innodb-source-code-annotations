/************************************************************************
The page cursor

(c) 1994-1996 Innobase Oy

Created 10/4/1994 Heikki Tuuri
*************************************************************************/
/*页面的光标*/
#include "page0cur.h"
#ifdef UNIV_NONINL
#include "page0cur.ic"
#endif

#include "rem0cmp.h"
#include "mtr0log.h"

ulint	page_cur_short_succ	= 0;

ulint	page_rnd	= 976722341;

#ifdef PAGE_CUR_ADAPT

/********************************************************************
Tries a search shortcut based on the last insert. */ /*尝试基于最后一次插入的搜索快捷方式。*/
UNIV_INLINE
ibool
page_cur_try_search_shortcut(
/*=========================*/
	page_t*		page,	/* in: index page */
	dtuple_t*	tuple,	/* in: data tuple */
	ulint*		iup_matched_fields,
				/* in/out: already matched fields in upper
				limit record */ /*在上限记录中已经匹配的字段*/
	ulint*		iup_matched_bytes,
				/* in/out: already matched bytes in a field
				not yet completely matched */ /*字段中尚未完全匹配的已匹配字节*/
	ulint*		ilow_matched_fields,
				/* in/out: already matched fields in lower
				limit record */ /*在下限记录中已经匹配的字段*/
	ulint*		ilow_matched_bytes,
				/* in/out: already matched bytes in a field
				not yet completely matched */ /*字段中尚未完全匹配的已匹配字节*/
	page_cur_t*	cursor) /* out: page cursor */ 
{
	int	cmp;
	rec_t*	rec;
	rec_t*	next_rec;
	ulint	low_match;
	ulint	low_bytes;
	ulint	up_match;
	ulint	up_bytes;
#ifdef UNIV_SEARCH_DEBUG
	page_cur_t cursor2;
#endif
	ut_ad(dtuple_check_typed(tuple));

	rec = page_header_get_ptr(page, PAGE_LAST_INSERT);

	ut_ad(rec);
	ut_ad(page_rec_is_user_rec(rec));

	ut_pair_min(&low_match, &low_bytes,
			*ilow_matched_fields, *ilow_matched_bytes,
			*iup_matched_fields, *iup_matched_bytes);

	up_match = low_match;
	up_bytes = low_bytes;

	cmp = page_cmp_dtuple_rec_with_match(tuple, rec, &low_match,
								&low_bytes);
	if (cmp == -1) {

		return(FALSE);
	}

	next_rec = page_rec_get_next(rec);

	cmp = page_cmp_dtuple_rec_with_match(tuple, next_rec, &up_match,
								&up_bytes);
	if (cmp != -1) {

		return(FALSE);
	}

	cursor->rec = rec;

#ifdef UNIV_SEARCH_DEBUG
	page_cur_search_with_match(page, tuple, PAGE_CUR_DBG,
	    					iup_matched_fields,
	    					iup_matched_bytes,
	    					ilow_matched_fields,
	    					ilow_matched_bytes,
	    					&cursor2);
	ut_a(cursor2.rec == cursor->rec);

	if (next_rec != page_get_supremum_rec(page)) {

		ut_a(*iup_matched_fields == up_match);
		ut_a(*iup_matched_bytes == up_bytes);
	}

	ut_a(*ilow_matched_fields == low_match);
	ut_a(*ilow_matched_bytes == low_bytes);
#endif
	if (next_rec != page_get_supremum_rec(page)) {

		*iup_matched_fields = up_match;
		*iup_matched_bytes = up_bytes;
	}

	*ilow_matched_fields = low_match;
	*ilow_matched_bytes = low_bytes;

#ifdef UNIV_SEARCH_PERF_STAT
	page_cur_short_succ++;
#endif
	return(TRUE);
}

#endif

/********************************************************************
Searches the right position for a page cursor. */
/*搜索页面光标的正确位置。*/
void
page_cur_search_with_match(
/*=======================*/
	page_t*		page,	/* in: index page */
	dtuple_t*	tuple,	/* in: data tuple */
	ulint		mode,	/* in: PAGE_CUR_L, PAGE_CUR_LE, PAGE_CUR_G,
				or PAGE_CUR_GE */
	ulint*		iup_matched_fields,
				/* in/out: already matched fields in upper
				limit record */
	ulint*		iup_matched_bytes,
				/* in/out: already matched bytes in a field
				not yet completely matched */
	ulint*		ilow_matched_fields,
				/* in/out: already matched fields in lower
				limit record */
	ulint*		ilow_matched_bytes,
				/* in/out: already matched bytes in a field
				not yet completely matched */
	page_cur_t*	cursor) /* out: page cursor */ 
{
	ulint	up;	
	ulint	low;	
	ulint	mid;
	page_dir_slot_t* slot;
	rec_t*	up_rec;
	rec_t*	low_rec;
	rec_t*	mid_rec;
	ulint	up_matched_fields;
	ulint	up_matched_bytes;
	ulint	low_matched_fields;
	ulint	low_matched_bytes;
	ulint	cur_matched_fields;
	ulint	cur_matched_bytes;
	int	cmp;
#ifdef UNIV_SEARCH_DEBUG
	int	dbg_cmp;
	ulint	dbg_matched_fields;
	ulint	dbg_matched_bytes;
#endif
	ut_ad(page && tuple && iup_matched_fields && iup_matched_bytes
	      && ilow_matched_fields && ilow_matched_bytes && cursor);
	ut_ad(dtuple_validate(tuple));
	ut_ad(dtuple_check_typed(tuple));
	ut_ad((mode == PAGE_CUR_L) || (mode == PAGE_CUR_LE)
	      || (mode == PAGE_CUR_G) || (mode == PAGE_CUR_GE)
	      || (mode == PAGE_CUR_DBG));
	      
#ifdef PAGE_CUR_ADAPT
	if ((page_header_get_field(page, PAGE_LEVEL) == 0)
	    && (mode == PAGE_CUR_LE)
	    && (page_header_get_field(page, PAGE_N_DIRECTION) > 3)
	    && (page_header_get_ptr(page, PAGE_LAST_INSERT))
	    && (page_header_get_field(page, PAGE_DIRECTION) == PAGE_RIGHT)) {

		if (page_cur_try_search_shortcut(page, tuple,
	    					iup_matched_fields,
	    					iup_matched_bytes,
	    					ilow_matched_fields,
	    					ilow_matched_bytes,
	    					cursor)) {
	    		return;
	    	}
	}
/*#ifdef UNIV_SEARCH_DEBUG */
	if (mode == PAGE_CUR_DBG) {
		mode = PAGE_CUR_LE;
	}
/*#endif */
#endif	
	/* If mode PAGE_CUR_G is specified, we are trying to position the
	cursor to answer a query of the form "tuple < X", where tuple is
	the input parameter, and X denotes an arbitrary physical record on
	the page. We want to position the cursor on the first X which
	satisfies the condition. */
    /*如果指定了PAGE_CUR_G模式，则试图定位游标以回答“tuple < X”形式的查询，其中tuple是输入参数，X表示页面上的任意物理记录。
	我们想把光标放在满足条件的第一个X上。*/
	up_matched_fields  = *iup_matched_fields;
	up_matched_bytes   = *iup_matched_bytes;
	low_matched_fields = *ilow_matched_fields;
	low_matched_bytes  = *ilow_matched_bytes;

	/* Perform binary search. First the search is done through the page
	directory, after that as a linear search in the list of records
	owned by the upper limit directory slot. */
    /*执行二进制搜索。首先通过页面目录进行搜索，然后在上限目录槽拥有的记录列表中进行线性搜索。*/
	low = 0;
	up = page_dir_get_n_slots(page) - 1;

	/* Perform binary search until the lower and upper limit directory
	slots come to the distance 1 of each other */
    /*执行二分查找，直到下限值和上限值目录槽彼此的距离为1*/
   	while (up - low > 1) {
		mid = (low + up) / 2;
		slot = page_dir_get_nth_slot(page, mid);
		mid_rec = page_dir_slot_get_rec(slot);

		ut_pair_min(&cur_matched_fields, &cur_matched_bytes,
				low_matched_fields, low_matched_bytes,
				up_matched_fields, up_matched_bytes);

		cmp = cmp_dtuple_rec_with_match(tuple, mid_rec,
						&cur_matched_fields,
						&cur_matched_bytes);
		if (cmp == 1) {
			low = mid;
			low_matched_fields = cur_matched_fields;
			low_matched_bytes = cur_matched_bytes;

		} else if (cmp == -1) {
			up = mid;
			up_matched_fields = cur_matched_fields;
			up_matched_bytes = cur_matched_bytes; 

		} else if ((mode == PAGE_CUR_G) || (mode == PAGE_CUR_LE)) {
			low = mid;
			low_matched_fields = cur_matched_fields;
			low_matched_bytes = cur_matched_bytes;
		} else {
			up = mid;
			up_matched_fields = cur_matched_fields;
			up_matched_bytes = cur_matched_bytes;
		}
   	}

	slot = page_dir_get_nth_slot(page, low);
	low_rec = page_dir_slot_get_rec(slot);
	slot = page_dir_get_nth_slot(page, up);
	up_rec = page_dir_slot_get_rec(slot);

	/* Perform linear search until the upper and lower records
	come to distance 1 of each other. */
    /*线性搜索，直到上下记录的距离为1。*/
   	while (page_rec_get_next(low_rec) != up_rec) {

		mid_rec = page_rec_get_next(low_rec);

		ut_pair_min(&cur_matched_fields, &cur_matched_bytes,
				low_matched_fields, low_matched_bytes,
				up_matched_fields, up_matched_bytes);

		cmp = cmp_dtuple_rec_with_match(tuple, mid_rec,
						&cur_matched_fields,
						&cur_matched_bytes);
		if (cmp == 1) {
			low_rec = mid_rec;
			low_matched_fields = cur_matched_fields;
			low_matched_bytes = cur_matched_bytes;

		} else if (cmp == -1) {
			up_rec = mid_rec;
			up_matched_fields = cur_matched_fields;
			up_matched_bytes = cur_matched_bytes; 

		} else if ((mode == PAGE_CUR_G) || (mode == PAGE_CUR_LE)) {
			low_rec = mid_rec;
			low_matched_fields = cur_matched_fields;
			low_matched_bytes = cur_matched_bytes;
		} else {
			up_rec = mid_rec;
			up_matched_fields = cur_matched_fields;
			up_matched_bytes = cur_matched_bytes;
		}
   	}

#ifdef UNIV_SEARCH_DEBUG

	/* Check that the lower and upper limit records have the
	right alphabetical order compared to tuple. */
	dbg_matched_fields = 0;
	dbg_matched_bytes = 0;

	dbg_cmp = page_cmp_dtuple_rec_with_match(tuple, low_rec,
						&dbg_matched_fields,
						&dbg_matched_bytes);
	if (mode == PAGE_CUR_G) {
		ut_a(dbg_cmp >= 0);
	} else if (mode == PAGE_CUR_GE) {
		ut_a(dbg_cmp == 1);
	} else if (mode == PAGE_CUR_L) {
		ut_a(dbg_cmp == 1);
	} else if (mode == PAGE_CUR_LE) {
		ut_a(dbg_cmp >= 0);
	}

	if (low_rec != page_get_infimum_rec(page)) {
	
		ut_a(low_matched_fields == dbg_matched_fields);
		ut_a(low_matched_bytes == dbg_matched_bytes);
	}

	dbg_matched_fields = 0;
	dbg_matched_bytes = 0;

	dbg_cmp = page_cmp_dtuple_rec_with_match(tuple, up_rec,
						&dbg_matched_fields,
						&dbg_matched_bytes);
	if (mode == PAGE_CUR_G) {
		ut_a(dbg_cmp == -1);
	} else if (mode == PAGE_CUR_GE) {
		ut_a(dbg_cmp <= 0);
	} else if (mode == PAGE_CUR_L) {
		ut_a(dbg_cmp <= 0);
	} else if (mode == PAGE_CUR_LE) {
		ut_a(dbg_cmp == -1);
	}

	if (up_rec != page_get_supremum_rec(page)) {

		ut_a(up_matched_fields == dbg_matched_fields);
		ut_a(up_matched_bytes == dbg_matched_bytes);
	}
#endif
	if (mode <= PAGE_CUR_GE) {
		cursor->rec = up_rec;
	} else {
		cursor->rec = low_rec;
	}

	*iup_matched_fields  = up_matched_fields;
	*iup_matched_bytes   = up_matched_bytes;
	*ilow_matched_fields = low_matched_fields;
	*ilow_matched_bytes  = low_matched_bytes;
}

/***************************************************************
Positions a page cursor on a randomly chosen user record on a page. If there
are no user records, sets the cursor on the infimum record. */
/*将页面光标定位在页面上随机选择的用户记录上。如果没有用户记录，将光标设置在下一条记录上。*/
void
page_cur_open_on_rnd_user_rec(
/*==========================*/
	page_t*		page,	/* in: page */
	page_cur_t*	cursor)	/* in/out: page cursor */
{
	ulint	rnd;
	rec_t*	rec;

	if (page_get_n_recs(page) == 0) {
		page_cur_position(page_get_infimum_rec(page), cursor);

		return;
	}	

	page_rnd += 87584577;	

	rnd = page_rnd % page_get_n_recs(page);

	rec = page_get_infimum_rec(page);

	rec = page_rec_get_next(rec);

	while (rnd > 0) {
		rec = page_rec_get_next(rec);

		rnd--;
	}

	page_cur_position(rec, cursor);
}	
	
/***************************************************************
Writes the log record of a record insert on a page. */ /*将插入的记录的日志记录写入页上。*/
static
void
page_cur_insert_rec_write_log(
/*==========================*/
	rec_t*	insert_rec,	/* in: inserted physical record */
	ulint	rec_size,	/* in: insert_rec size */
	rec_t*	cursor_rec,	/* in: record the cursor is pointing to */
	mtr_t*	mtr)		/* in: mini-transaction handle */
{
	ulint	cur_rec_size;
	ulint	extra_size;
	ulint	cur_extra_size;
	ulint	min_rec_size;
	byte*	ins_ptr;
	byte*	cur_ptr;
	ulint	extra_info_yes;
	byte*	log_ptr;
	ulint	i;

	log_ptr = mlog_open(mtr, 30 + MLOG_BUF_MARGIN);

	if (log_ptr == NULL) {

		return;
	}

	extra_size = rec_get_extra_size(insert_rec);

	cur_extra_size = rec_get_extra_size(cursor_rec);
	cur_rec_size = rec_get_size(cursor_rec);

	ins_ptr = insert_rec - extra_size;

	i = 0;

	if (cur_extra_size == extra_size) {
		min_rec_size = ut_min(cur_rec_size, rec_size);

		cur_ptr = cursor_rec - cur_extra_size;

		/* Find out the first byte in insert_rec which differs from
		cursor_rec; skip the bytes in the record info */
		/*找出insert_rec中与cursor_rec不同的第一个字节;跳过记录信息中的字节*/
		for (;;) {
			if (i >= min_rec_size) {

				break;
			} else if (*ins_ptr == *cur_ptr) {
				i++;
				ins_ptr++;
				cur_ptr++;
			} else if ((i < extra_size)
				   && (i >= extra_size - REC_N_EXTRA_BYTES)) {
				i = extra_size;
				ins_ptr = insert_rec;
				cur_ptr = cursor_rec;
			} else {
				break;
			}
		}
	}

	if (mtr_get_log_mode(mtr) != MTR_LOG_SHORT_INSERTS) {
	
		log_ptr = mlog_write_initial_log_record_fast(insert_rec,
					MLOG_REC_INSERT, log_ptr, mtr);
		/* Write the cursor rec offset as a 2-byte ulint *//*将光标的rec偏移量写入一个2字节的ulint*/
		mach_write_to_2(log_ptr, cursor_rec
					- buf_frame_align(cursor_rec));
		log_ptr += 2;
	}

	if ((rec_get_info_bits(insert_rec) != rec_get_info_bits(cursor_rec))
	    || (extra_size != cur_extra_size)
	    || (rec_size != cur_rec_size)) {

		extra_info_yes = 1;
	} else {
		extra_info_yes = 0;
	}
	
	/* Write the record end segment length and the extra info storage
	flag */ /*写入记录结束段长度和额外的信息存储标志*/
	log_ptr += mach_write_compressed(log_ptr, 2 * (rec_size - i)
							+ extra_info_yes);
	if (extra_info_yes) {
		/* Write the info bits */ /*写入信息位*/
		mach_write_to_1(log_ptr, rec_get_info_bits(insert_rec));
		log_ptr++;

		/* Write the record origin offset */ /*写入记录的起始偏移量*/
		log_ptr += mach_write_compressed(log_ptr, extra_size);

		/* Write the mismatch index */ /*写入不匹配索引*/
		log_ptr += mach_write_compressed(log_ptr, i);
	}
	
	/* Write to the log the inserted index record end segment which
	differs from the cursor record */
    /*将插入的索引记录与游标记录不同的结束段写入日志*/
	if (rec_size - i < MLOG_BUF_MARGIN) {
		ut_memcpy(log_ptr, ins_ptr, rec_size - i);
		log_ptr += rec_size - i;
	}

	mlog_close(mtr, log_ptr);

	if (rec_size - i >= MLOG_BUF_MARGIN) {
		mlog_catenate_string(mtr, ins_ptr, rec_size - i);
	}
}	

/***************************************************************
Parses a log record of a record insert on a page. */
/*解析页上插入的记录的日志记录。*/
byte*
page_cur_parse_insert_rec(
/*======================*/
			/* out: end of log record or NULL */
	ibool	is_short,/* in: TRUE if short inserts */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr)	/* in: mtr or NULL */
{
	ulint	extra_info_yes;
	ulint	offset;
	ulint	origin_offset;
	ulint	end_seg_len;
	ulint	mismatch_index;
	rec_t*	cursor_rec;
	byte	buf1[1024];
	byte*	buf;
	ulint	info_bits;
	page_cur_t cursor;

	if (!is_short) {
		/* Read the cursor rec offset as a 2-byte ulint */
        /* 读取光标的rec偏移量作为一个2字节的ulint*/
		if (end_ptr < ptr + 2) {

			return(NULL);
		}

		offset = mach_read_from_2(ptr);
		
		ptr += 2;
	}

	ptr = mach_parse_compressed(ptr, end_ptr, &end_seg_len);

	if (ptr == NULL) {

		return(NULL);
	}

	extra_info_yes = end_seg_len & 0x1;
	end_seg_len = end_seg_len / 2;
	
	if (extra_info_yes) {
		/* Read the info bits */

		if (end_ptr < ptr + 1) {

			return(NULL);
		}
		
		info_bits = mach_read_from_1(ptr);
		ptr++;

		ptr = mach_parse_compressed(ptr, end_ptr, &origin_offset);

		if (ptr == NULL) {

			return(NULL);
		}

		ptr = mach_parse_compressed(ptr, end_ptr, &mismatch_index);

		if (ptr == NULL) {

			return(NULL);
		}
	}

	if (end_ptr < ptr + end_seg_len) {

		return(NULL);
	}
	
	if (page == NULL) {

		return(ptr + end_seg_len);
	}

	/* Read from the log the inserted index record end segment which
	differs from the cursor record */
    /*从日志中读取插入的索引记录与游标记录不同的结束段*/
	if (is_short) {
		cursor_rec = page_rec_get_prev(page_get_supremum_rec(page));
	} else {
		cursor_rec = page + offset;
	}

	if (extra_info_yes == 0) {
		info_bits = rec_get_info_bits(cursor_rec);
		origin_offset = rec_get_extra_size(cursor_rec);
		mismatch_index = rec_get_size(cursor_rec) - end_seg_len;
	} 
	
	if (mismatch_index + end_seg_len < 1024) {
		buf = buf1;
	} else {
		buf = mem_alloc(mismatch_index + end_seg_len);
	}

	/* Build the inserted record to buf */
	/* 将插入的记录构建为buf*/
	ut_memcpy(buf, rec_get_start(cursor_rec), mismatch_index);
	ut_memcpy(buf + mismatch_index, ptr, end_seg_len);

	rec_set_info_bits(buf + origin_offset, info_bits);

	page_cur_position(cursor_rec, &cursor);

	page_cur_rec_insert(&cursor, buf + origin_offset, mtr);

	if (mismatch_index + end_seg_len >= 1024) {

		mem_free(buf);
	}

	return(ptr + end_seg_len);
}	

/***************************************************************
Inserts a record next to page cursor. Returns pointer to inserted record if
succeed, i.e., enough space available, NULL otherwise. The record to be
inserted can be in a data tuple or as a physical record. The other parameter
must then be NULL. The cursor stays at the same position. */
/*在页面光标旁边插入一条记录。如果成功，返回指向插入记录的指针，即有足够的可用空间，否则为NULL。
要插入的记录可以在一个数据元组中，也可以作为一个物理记录。另一个参数必须为NULL。光标保持在相同的位置。*/
rec_t*
page_cur_insert_rec_low(
/*====================*/
				/* out: pointer to record if succeed, NULL
				otherwise */
	page_cur_t*	cursor,	/* in: a page cursor */
	dtuple_t*      	tuple,  /* in: pointer to a data tuple or NULL */
	ulint		data_size,/* in: data size of tuple */
	rec_t*      	rec,  	/* in: pointer to a physical record or NULL */
	mtr_t*		mtr)	/* in: mini-transaction handle */
{
	byte*	insert_buf	= NULL;
	ulint	rec_size;
	byte*	page;		/* the relevant page */ /*相关的页面*/
	rec_t*	last_insert;	/* cursor position at previous insert */ /*光标位于前一个插入位置*/
	rec_t*	insert_rec;	/* inserted record */
	ulint	heap_no;	/* heap number of the inserted record */
	rec_t*	current_rec;	/* current record after which the
				new record is inserted */ /*插入新记录之后的当前记录*/
	rec_t*	next_rec;	/* next record after current before
				the insertion */ /*插入前当前记录后的下一个记录*/
	ulint	owner_slot;	/* the slot which owns the inserted record */  /*拥有插入的记录的槽*/
	rec_t*	owner_rec;	
	ulint	n_owned;
	
	ut_ad(cursor && mtr);
	ut_ad(tuple || rec);
	ut_ad(!(tuple && rec));
	ut_ad(rec || dtuple_check_typed(tuple));
	ut_ad(rec || (dtuple_get_data_size(tuple) == data_size));

	page = page_cur_get_page(cursor);

	ut_ad(cursor->rec != page_get_supremum_rec(page));	

	/* 1. Get the size of the physical record in the page */ /*1. 获取页中物理记录的大小*/
	if (tuple != NULL) {
		rec_size = data_size + rec_get_converted_extra_size(
						data_size,
						dtuple_get_n_fields(tuple));
	} else {
		rec_size = rec_get_size(rec);
	}

	/* 2. Try to find suitable space from page memory management */ /*2. 尝试从页面内存管理中找到合适的空间*/
	insert_buf = page_mem_alloc(page, rec_size, &heap_no);

	if (insert_buf == NULL) {

		return(NULL);
	}

	/* 3. Create the record */ /*3.创建的记录*/
	if (tuple != NULL) {
		insert_rec = rec_convert_dtuple_to_rec_low(insert_buf, tuple,
								data_size);
	} else {
		insert_rec = rec_copy(insert_buf, rec);
	}

	ut_ad(insert_rec);
	ut_ad(rec_size == rec_get_size(insert_rec));
	
	/* 4. Insert the record in the linked list of records */
	/*4. 将记录插入记录链表中*/
	current_rec = cursor->rec;

	next_rec = page_rec_get_next(current_rec);
	page_rec_set_next(insert_rec, next_rec);
	page_rec_set_next(current_rec, insert_rec);

	page_header_set_field(page, PAGE_N_RECS, 1 + page_get_n_recs(page));

	/* 5. Set the n_owned field in the inserted record to zero,
	and set the heap_no field */	
	/*5. 将插入记录中的n_owned字段设置为零，并设置heap_no字段*/
	rec_set_n_owned(insert_rec, 0);
	rec_set_heap_no(insert_rec, heap_no);

	/* 6. Update the last insertion info in page header */	
    /*6. 更新页眉中的最后一个插入信息*/
	last_insert = page_header_get_ptr(page, PAGE_LAST_INSERT);

	if (last_insert == NULL) {
	    	page_header_set_field(page, PAGE_DIRECTION, PAGE_NO_DIRECTION);
	    	page_header_set_field(page, PAGE_N_DIRECTION, 0);

	} else if ((last_insert == current_rec)
	    && (page_header_get_field(page, PAGE_DIRECTION) != PAGE_LEFT)) {

	    	page_header_set_field(page, PAGE_DIRECTION, PAGE_RIGHT);
	    	page_header_set_field(page, PAGE_N_DIRECTION,
			page_header_get_field(page, PAGE_N_DIRECTION) + 1);

	} else if ((page_rec_get_next(insert_rec) == last_insert)
	    && (page_header_get_field(page, PAGE_DIRECTION) != PAGE_RIGHT)) {

	    	page_header_set_field(page, PAGE_DIRECTION, PAGE_LEFT);
	    	page_header_set_field(page, PAGE_N_DIRECTION,
			page_header_get_field(page, PAGE_N_DIRECTION) + 1);
	} else {
	    	page_header_set_field(page, PAGE_DIRECTION, PAGE_NO_DIRECTION);
	    	page_header_set_field(page, PAGE_N_DIRECTION, 0);
	}
	
	page_header_set_ptr(page, PAGE_LAST_INSERT, insert_rec);

	/* 7. It remains to update the owner record. */		
	/* 7. 它仍然需要更新所有者记录。*/
	owner_rec = page_rec_find_owner_rec(insert_rec);
	n_owned = rec_get_n_owned(owner_rec);
	rec_set_n_owned(owner_rec, n_owned + 1);

	/* 8. Now we have incremented the n_owned field of the owner
	record. If the number exceeds PAGE_DIR_SLOT_MAX_N_OWNED,
	we have to split the corresponding directory slot in two. */
    /*8. 现在我们增加了所有者记录的n_owned字段。如果数量超过PAGE_DIR_SLOT_MAX_N_OWNED，我们必须将对应的目录槽位拆分为两个。*/
	if (n_owned == PAGE_DIR_SLOT_MAX_N_OWNED) {
		owner_slot = page_dir_find_owner_slot(owner_rec);
		page_dir_split_slot(page, owner_slot);
	}

	/* 9. Write log record of the insert */ /*9. 写入插入的日志记录*/
 	page_cur_insert_rec_write_log(insert_rec, rec_size, current_rec, mtr);
	
	return(insert_rec);
}

/**************************************************************
Writes a log record of copying a record list end to a new created page. */
/*将复制记录列表结束的日志记录写入新创建的页。*/
UNIV_INLINE
byte*
page_copy_rec_list_to_created_page_write_log(
/*=========================================*/
			/* out: 4-byte field where to write the log data
			length */
	page_t*	page,	/* in: index page */
	mtr_t*	mtr)	/* in: mtr */
{
	byte*	log_ptr;
	
	mlog_write_initial_log_record(page, MLOG_LIST_END_COPY_CREATED, mtr);

	log_ptr = mlog_open(mtr, 4);

	mlog_close(mtr, log_ptr + 4);

	return(log_ptr);
}

/**************************************************************
Parses a log record of copying a record list end to a new created page. */
/*解析将记录列表复制到新创建页面的日志记录。*/
byte*
page_parse_copy_rec_list_to_created_page(
/*=====================================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr)	/* in: mtr or NULL */
{
	byte*	rec_end;
	ulint	log_data_len;
	
	if (ptr + 4 > end_ptr) {

		return(NULL);
	}

	log_data_len = mach_read_from_4(ptr);
	ptr += 4;

	rec_end = ptr + log_data_len;

	if (rec_end > end_ptr) {

		return(NULL);
	}

	if (!page) {

		return(rec_end);
	}

	while (ptr < rec_end) {
		ptr = page_cur_parse_insert_rec(TRUE, ptr, end_ptr, page, mtr);
	}

	ut_a(ptr == rec_end);
	
	page_header_set_ptr(page, PAGE_LAST_INSERT, NULL);
	page_header_set_field(page, PAGE_DIRECTION, PAGE_NO_DIRECTION);
	page_header_set_field(page, PAGE_N_DIRECTION, 0);

	return(rec_end);	
}

/*****************************************************************
Copies records from page to a newly created page, from a given record onward,
including that record. Infimum and supremum records are not copied. */
/*从一个给定记录(包括该记录)开始，将记录从一个页复制到一个新创建的页。下位记录和上位记录不被复制。*/
void
page_copy_rec_list_end_to_created_page(
/*===================================*/
	page_t*	new_page,	/* in: index page to copy to */
	page_t*	page,		/* in: index page */
	rec_t*	rec,		/* in: first record to copy */
	mtr_t*	mtr)		/* in: mtr */
{
	page_dir_slot_t* slot;
	byte*	heap_top;
	rec_t*	insert_rec;
	rec_t*	prev_rec;
	ulint	count;
	ulint	n_recs;
	ulint	slot_index;
	ulint	rec_size;
	ulint	log_mode;
	byte*	log_ptr;
	ulint	log_data_len;
	
	ut_ad(page_header_get_field(new_page, PAGE_N_HEAP) == 2);
	ut_ad(page != new_page);
	
	if (rec == page_get_infimum_rec(page)) {

		rec = page_rec_get_next(rec);
	}

	if (rec == page_get_supremum_rec(page)) {

		return;
	}
	
#ifdef UNIV_DEBUG
	/* To pass the debug tests we have to set these dummy values
	in the debug version */
	page_header_set_field(new_page, PAGE_N_DIR_SLOTS, UNIV_PAGE_SIZE / 2);
	page_header_set_ptr(new_page, PAGE_HEAP_TOP,
					new_page + UNIV_PAGE_SIZE - 1);
#endif

	log_ptr = page_copy_rec_list_to_created_page_write_log(new_page, mtr);

	log_data_len = dyn_array_get_data_size(&(mtr->log));

	/* Individual inserts are logged in a shorter form */
    /* 每个插入都以较短的形式记录下来*/
	log_mode = mtr_set_log_mode(mtr, MTR_LOG_SHORT_INSERTS);
	
	prev_rec = page_get_infimum_rec(new_page);
	heap_top = new_page + PAGE_SUPREMUM_END;
	count = 0;
	slot_index = 0;
	n_recs = 0;

	while (rec != page_get_supremum_rec(page)) {
		
		insert_rec = rec_copy(heap_top, rec);

		rec_set_next_offs(prev_rec, insert_rec - new_page);

		rec_set_n_owned(insert_rec, 0);
		rec_set_heap_no(insert_rec, 2 + n_recs);

		rec_size = rec_get_size(insert_rec);

		heap_top = heap_top + rec_size;
		
		ut_ad(heap_top < new_page + UNIV_PAGE_SIZE);

		count++;
		n_recs++;

		if (count == (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2) {

			slot_index++;

			slot = page_dir_get_nth_slot(new_page, slot_index);

			page_dir_slot_set_rec(slot, insert_rec);
			page_dir_slot_set_n_owned(slot, count);

			count = 0;
		}
		
 		page_cur_insert_rec_write_log(insert_rec, rec_size, prev_rec,
									mtr);
		prev_rec = insert_rec;
		rec = page_rec_get_next(rec);
	}

	if ((slot_index > 0) && (count + 1
				+ (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2
			     	 <= PAGE_DIR_SLOT_MAX_N_OWNED)) {
		/* We can merge the two last dir slots. This operation is
		here to make this function imitate exactly the equivalent
		task made using page_cur_insert_rec, which we use in database
		recovery to reproduce the task performed by this function.
		To be able to check the correctness of recovery, it is good
		that it imitates exactly. */
        /*我们可以合并最后两个dir槽。这里的操作是让这个函数完全模仿使用page_cur_insert_rec完成的等价任务，
		我们在数据库恢复中使用page_cur_insert_rec来重现这个函数执行的任务。为了能够检查恢复的正确性，最好是精确地模拟。*/
		count += (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2;
		
		page_dir_slot_set_n_owned(slot, 0);

		slot_index--;
	}

	log_data_len = dyn_array_get_data_size(&(mtr->log)) - log_data_len;

	mach_write_to_4(log_ptr, log_data_len);
	
	rec_set_next_offs(insert_rec, PAGE_SUPREMUM);

	slot = page_dir_get_nth_slot(new_page, 1 + slot_index);

	page_dir_slot_set_rec(slot, page_get_supremum_rec(new_page));
	page_dir_slot_set_n_owned(slot, count + 1);

	page_header_set_field(new_page, PAGE_N_DIR_SLOTS, 2 + slot_index);
	page_header_set_ptr(new_page, PAGE_HEAP_TOP, heap_top);
	page_header_set_field(new_page, PAGE_N_HEAP, 2 + n_recs);
	page_header_set_field(new_page, PAGE_N_RECS, n_recs);
	
	page_header_set_ptr(new_page, PAGE_LAST_INSERT, NULL);
	page_header_set_field(new_page, PAGE_DIRECTION, PAGE_NO_DIRECTION);
	page_header_set_field(new_page, PAGE_N_DIRECTION, 0);

	/* Restore the log mode */
    /*恢复日志模式*/
	mtr_set_log_mode(mtr, log_mode);
}

/***************************************************************
Writes log record of a record delete on a page. */ /*将已删除记录的日志记录写入页面。*/
UNIV_INLINE
void
page_cur_delete_rec_write_log(
/*==========================*/
	rec_t*	cursor_rec,	/* in: record to be deleted */
	mtr_t*	mtr)		/* in: mini-transaction handle */
{
	mlog_write_initial_log_record(cursor_rec, MLOG_REC_DELETE, mtr);

	/* Write the cursor rec offset as a 2-byte ulint *//*将已删除记录的日志记录写入页面。*/
	mlog_catenate_ulint(mtr, cursor_rec - buf_frame_align(cursor_rec),
								MLOG_2BYTES);
}	

/***************************************************************
Parses log record of a record delete on a page. */
/*在页面上解析删除记录的日志记录。*/
byte*
page_cur_parse_delete_rec(
/*======================*/
			/* out: pointer to record end or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr)	/* in: mtr or NULL */
{
	ulint		offset;
	page_cur_t	cursor;
	
	if (end_ptr < ptr + 2) {

		return(NULL);
	}

	/* Read the cursor rec offset as a 2-byte ulint */
	offset = mach_read_from_2(ptr);
	ptr += 2;

	if (page) {
		page_cur_position(page + offset, &cursor);

		page_cur_delete_rec(&cursor, mtr);
	}

	return(ptr);
}	

/***************************************************************
Deletes a record at the page cursor. The cursor is moved to the next
record after the deleted one. */
/*删除页光标处的记录。光标移动到删除记录后的下一条记录。*/
void
page_cur_delete_rec(
/*================*/
	page_cur_t*  	cursor,	/* in: a page cursor */
	mtr_t*		mtr)	/* in: mini-transaction handle */
{
	page_dir_slot_t* cur_dir_slot;
	page_dir_slot_t* prev_slot;
	page_t*		page;
	rec_t*		current_rec;
	rec_t*		prev_rec	= NULL;
	rec_t*		next_rec;
	ulint		cur_slot_no;
	ulint		cur_n_owned;
	rec_t*		rec;
	
	ut_ad(cursor && mtr);
	
	page = page_cur_get_page(cursor);
	current_rec = cursor->rec;

	/* The record must not be the supremum or infimum record. */ /*记录不能是上记录或下记录。*/
	ut_ad(current_rec != page_get_supremum_rec(page));	
	ut_ad(current_rec != page_get_infimum_rec(page));	
	
	/* Save to local variables some data associated with current_rec *//*将与current_rec相关的一些数据保存到本地变量*/
	cur_slot_no = page_dir_find_owner_slot(current_rec);
	cur_dir_slot = page_dir_get_nth_slot(page, cur_slot_no);
	cur_n_owned = page_dir_slot_get_n_owned(cur_dir_slot);

	/* 0. Write the log record */ /*0. 写入日志记录*/
	page_cur_delete_rec_write_log(current_rec, mtr);

	/* 1. Reset the last insert info in the page header and increment
	the modify clock for the frame */ 
     /*1. 重置页眉中的最后一个插入信息，并增加帧的修改时钟*/
	page_header_set_ptr(page, PAGE_LAST_INSERT, NULL);

	/* The page gets invalid for optimistic searches: increment the
	frame modify clock */
    /*页面对乐观搜索无效:增加帧修改时钟*/
	buf_frame_modify_clock_inc(page);
	
	/* 2. Find the next and the previous record. Note that the cursor is
	left at the next record. */
    /*2. 查找下一个和上一个记录。注意，游标留在下一个记录处。*/
	ut_ad(cur_slot_no > 0);
	prev_slot = page_dir_get_nth_slot(page, cur_slot_no - 1);
	
	rec = page_dir_slot_get_rec(prev_slot);
	
	/* rec now points to the record of the previous directory slot. Look
	for the immediate predecessor of current_rec in a loop. */
    /*Rec现在指向前一个目录槽的记录。在循环中查找current_rec的直接前身。*/
	while(current_rec != rec) {
		prev_rec = rec;
		rec = page_rec_get_next(rec);
	}

	page_cur_move_to_next(cursor);
	next_rec = cursor->rec;
	
	/* 3. Remove the record from the linked list of records */
    /*3.从记录链表中删除该记录*/
	page_rec_set_next(prev_rec, next_rec);
	page_header_set_field(page, PAGE_N_RECS,
				(ulint)(page_get_n_recs(page) - 1));

	/* 4. If the deleted record is pointed to by a dir slot, update the
	record pointer in slot. In the following if-clause we assume that
	prev_rec is owned by the same slot, i.e., PAGE_DIR_SLOT_MIN_N_OWNED
	>= 2. */
	/*4. 如果被删除的记录是由dir槽指向的，更新槽中的记录指针。
	在下面的if-子句中，我们假设prev_rec属于同一个槽位，即PAGE_DIR_SLOT_MIN_N_OWNED>= 2。*/
	ut_ad(PAGE_DIR_SLOT_MIN_N_OWNED >= 2);
	ut_ad(cur_n_owned > 1);

	if (current_rec == page_dir_slot_get_rec(cur_dir_slot)) {
		page_dir_slot_set_rec(cur_dir_slot, prev_rec);
	}
	
	/* 5. Update the number of owned records of the slot */
    /*5. 更新槽位拥有的记录数*/
	page_dir_slot_set_n_owned(cur_dir_slot, cur_n_owned - 1);

	/* 6. Free the memory occupied by the record */ /*6. 释放记录占用的内存*/
	page_mem_free(page, current_rec);

	/* 7. Now we have decremented the number of owned records of the slot.
	If the number drops below PAGE_DIR_SLOT_MIN_N_OWNED, we balance the
	slots. */
	/*7. 现在我们已经减少了槽拥有的记录的数量。如果数字低于PAGE_DIR_SLOT_MIN_N_OWNED，我们就平衡插槽。*/
	if (cur_n_owned <= PAGE_DIR_SLOT_MIN_N_OWNED) {
		page_dir_balance_slot(page, cur_slot_no);
	}
}
