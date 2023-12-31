/******************************************************
Purge old versions
清除旧版本
(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#include "trx0purge.h"

#ifdef UNIV_NONINL
#include "trx0purge.ic"
#endif

#include "fsp0fsp.h"
#include "mach0data.h"
#include "trx0rseg.h"
#include "trx0trx.h"
#include "trx0roll.h"
#include "read0read.h"
#include "fut0fut.h"
#include "que0que.h"
#include "row0purge.h"
#include "row0upd.h"
#include "trx0rec.h"
#include "srv0que.h"
#include "os0thread.h"

/* The global data structure coordinating a purge 全局数据结构协调清除*/
trx_purge_t*	purge_sys = NULL;

/* A dummy undo record used as a return value when we have a whole undo log
which needs no purge 当我们有一个不需要清除的完整的撤消日志时，一个虚拟的撤消记录用作返回值*/
trx_undo_rec_t	trx_purge_dummy_rec;

/*********************************************************************
Checks if trx_id is >= purge_view: then it is guaranteed that its update
undo log still exists in the system. 检查trx_id是否为>= purge_view:然后保证它的更新undo日志仍然存在于系统中。*/
ibool
trx_purge_update_undo_must_exist(
/*=============================*/
			/* out: TRUE if is sure that it is preserved, also
			if the function returns FALSE, it is possible that
			the undo log still exists in the system */
	dulint	trx_id)	/* in: transaction id */
{
	ut_ad(rw_lock_own(&(purge_sys->latch), RW_LOCK_SHARED));

	if (!read_view_sees_trx_id(purge_sys->view, trx_id)) {

		return(TRUE);
	}

	return(FALSE);
}

/*=================== PURGE RECORD ARRAY 清洗记录数组=============================*/

/***********************************************************************
Stores info of an undo log record during a purge. 在清除期间存储撤消日志记录的信息。*/
static
trx_undo_inf_t*
trx_purge_arr_store_info(
/*=====================*/
			/* out: pointer to the storage cell */
	dulint	trx_no,	/* in: transaction number */
	dulint	undo_no)/* in: undo number */
{
	trx_undo_inf_t*	cell;
	trx_undo_arr_t*	arr;
	ulint		i;

	arr = purge_sys->arr;
	
	for (i = 0;; i++) {
		cell = trx_undo_arr_get_nth_info(arr, i);

		if (!(cell->in_use)) {
			/* Not in use, we may store here 不用，我们可以放在这里*/
			cell->undo_no = undo_no;
			cell->trx_no = trx_no;
			cell->in_use = TRUE;
			
			arr->n_used++;

			return(cell);
		}
	}
}

/***********************************************************************
Removes info of an undo log record during a purge. 在清除过程中删除撤消日志记录的信息。*/
UNIV_INLINE
void
trx_purge_arr_remove_info(
/*======================*/
	trx_undo_inf_t*	cell)	/* in: pointer to the storage cell */
{
	trx_undo_arr_t*	arr;

	arr = purge_sys->arr;	

	cell->in_use = FALSE;
				
	ut_ad(arr->n_used > 0);

	arr->n_used--;
}

/***********************************************************************
Gets the biggest pair of a trx number and an undo number in a purge array.获取清除数组中trx号和撤消号的最大一对。 */
static
void
trx_purge_arr_get_biggest(
/*======================*/
	trx_undo_arr_t*	arr,	/* in: purge array */
	dulint*		trx_no,	/* out: transaction number: ut_dulint_zero
				if array is empty */
	dulint*		undo_no)/* out: undo number */
{
	trx_undo_inf_t*	cell;
	dulint		pair_trx_no;
	dulint		pair_undo_no;
	int		trx_cmp;
	ulint		n_used;
	ulint		i;
	ulint		n;
	
	n = 0;
	n_used = arr->n_used;
	pair_trx_no = ut_dulint_zero;
	pair_undo_no = ut_dulint_zero;
	
	for (i = 0;; i++) {
		cell = trx_undo_arr_get_nth_info(arr, i);

		if (cell->in_use) {
			n++;
 			trx_cmp = ut_dulint_cmp(cell->trx_no, pair_trx_no);

			if ((trx_cmp > 0)
			    || ((trx_cmp == 0)
			        && (ut_dulint_cmp(cell->undo_no,
						pair_undo_no) >= 0))) {
						
				pair_trx_no = cell->trx_no;
				pair_undo_no = cell->undo_no;
			}
		}

		if (n == n_used) {
			*trx_no = pair_trx_no;
			*undo_no = pair_undo_no;

			return;
		}
	}
}

/********************************************************************
Builds a purge 'query' graph. The actual purge is performed by executing
this query graph.构建一个清除“查询”图。实际的清除是通过执行这个查询图来执行的。 */
static
que_t*
trx_purge_graph_build(void)
/*=======================*/
				/* out, own: the query graph */
{
	mem_heap_t*	heap;
	que_fork_t*	fork;
	que_thr_t*	thr;
/*	que_thr_t*	thr2; */
	
	heap = mem_heap_create(512);
	fork = que_fork_create(NULL, NULL, QUE_FORK_PURGE, heap);
	fork->trx = purge_sys->trx;
	
	thr = que_thr_create(fork, heap);

	thr->child = row_purge_node_create(thr, heap);  

/*	thr2 = que_thr_create(fork, fork, heap);

	thr2->child = row_purge_node_create(fork, thr2, heap);   */

	return(fork);
}

/************************************************************************
Creates the global purge system control structure and inits the history
mutex. 创建全局清除系统控制结构并初始化历史互斥。*/

void
trx_purge_sys_create(void)
/*======================*/
{
	com_endpoint_t*	com_endpoint;

	ut_ad(mutex_own(&kernel_mutex));

	purge_sys = mem_alloc(sizeof(trx_purge_t));

	purge_sys->state = TRX_STOP_PURGE;

	purge_sys->n_pages_handled = 0;

	purge_sys->purge_trx_no = ut_dulint_zero;
	purge_sys->purge_undo_no = ut_dulint_zero;
	purge_sys->next_stored = FALSE;
	
	rw_lock_create(&(purge_sys->purge_is_running));
	rw_lock_set_level(&(purge_sys->purge_is_running),
						SYNC_PURGE_IS_RUNNING);
	rw_lock_create(&(purge_sys->latch));
	rw_lock_set_level(&(purge_sys->latch), SYNC_PURGE_LATCH);

	mutex_create(&(purge_sys->mutex));
	mutex_set_level(&(purge_sys->mutex), SYNC_PURGE_SYS);

	purge_sys->heap = mem_heap_create(256);

	purge_sys->arr = trx_undo_arr_create();

	com_endpoint = (com_endpoint_t*)purge_sys; /* This is a dummy non-NULL
						   value */
	purge_sys->sess = sess_open(com_endpoint, (byte*)"purge_system", 13);

	purge_sys->trx = purge_sys->sess->trx;

	purge_sys->trx->type = TRX_PURGE;

	ut_a(trx_start_low(purge_sys->trx, ULINT_UNDEFINED));

	purge_sys->query = trx_purge_graph_build();
				
	purge_sys->view = read_view_oldest_copy_or_open_new(NULL,
							purge_sys->heap);
}

/*================ UNDO LOG HISTORY LIST =============================*/

/************************************************************************
Adds the update undo log as the first log in the history list. Removes the
update undo log segment from the rseg slot if it is too big for reuse. 
将更新撤销日志添加为历史列表中的第一个日志。如果更新undo日志段太大而不能重用，则从rseg槽位中删除该更新undo日志段。*/
void
trx_purge_add_update_undo_to_history(
/*=================================*/
	trx_t*	trx,		/* in: transaction */
	page_t*	undo_page,	/* in: update undo log header page,
				x-latched */
	mtr_t*	mtr)		/* in: mtr */
{
	trx_undo_t*	undo;
	trx_rseg_t*	rseg;
	trx_rsegf_t*	rseg_header;
	trx_usegf_t*	seg_header;
	trx_ulogf_t*	undo_header;
	trx_upagef_t*	page_header;
	ulint		hist_size;
	
	undo = trx->update_undo;
	
	ut_ad(undo);
	
	rseg = undo->rseg;
	ut_ad(mutex_own(&(rseg->mutex)));

	rseg_header = trx_rsegf_get(rseg->space, rseg->page_no, mtr);

	undo_header = undo_page + undo->hdr_offset;
	seg_header  = undo_page + TRX_UNDO_SEG_HDR;
	page_header = undo_page + TRX_UNDO_PAGE_HDR;
	
	if (undo->state != TRX_UNDO_CACHED) {
		/* The undo log segment will not be reused undo日志段不会被重用*/

		if (undo->id >= TRX_RSEG_N_SLOTS) {
			fprintf(stderr,
			"InnoDB: Error: undo->id is %lu\n", undo->id);
			ut_a(0);
		}

		trx_rsegf_set_nth_undo(rseg_header, undo->id, FIL_NULL, mtr);

		hist_size = mtr_read_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE,
							MLOG_4BYTES, mtr);
		ut_ad(undo->size ==
			flst_get_len(seg_header + TRX_UNDO_PAGE_LIST, mtr));

		mlog_write_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE,
				hist_size + undo->size, MLOG_4BYTES, mtr);	
	}

	/* Add the log as the first in the history list 将日志添加为历史列表中的第一个日志*/
	flst_add_first(rseg_header + TRX_RSEG_HISTORY,
				undo_header + TRX_UNDO_HISTORY_NODE, mtr);

	/* Write the trx number to the undo log header 将trx号写入撤消日志头*/
	mlog_write_dulint(undo_header + TRX_UNDO_TRX_NO, trx->no, MLOG_8BYTES,
									mtr);
	/* Write information about delete markings to the undo log header */
	
	if (!undo->del_marks) {
		mlog_write_ulint(undo_header + TRX_UNDO_DEL_MARKS, FALSE,
							MLOG_2BYTES, mtr);
	}
	
	if (rseg->last_page_no == FIL_NULL) {

		rseg->last_page_no = undo->hdr_page_no;
		rseg->last_offset = undo->hdr_offset;
		rseg->last_trx_no = trx->no;
		rseg->last_del_marks = undo->del_marks;
	}
}

/**************************************************************************
Frees an undo log segment which is in the history list. Cuts the end of the
history list at the youngest undo log in this segment.
释放历史列表中的undo日志段。在此段中最年轻的undo日志处削减历史列表的末尾。 */
static
void
trx_purge_free_segment(
/*===================*/
	trx_rseg_t*	rseg,		/* in: rollback segment */
	fil_addr_t	hdr_addr,	/* in: the file address of log_hdr */
	ulint		n_removed_logs)	/* in: count of how many undo logs we
					will cut off from the end of the
					history list */
{
	page_t*		undo_page;
	trx_rsegf_t*	rseg_hdr;
	trx_ulogf_t*	log_hdr;
	trx_usegf_t*	seg_hdr;
	ibool		freed;
	ulint		seg_size;
	ulint		hist_size;
	ibool		marked		= FALSE;
	mtr_t		mtr;
	
/*	printf("Freeing an update undo log segment\n"); */

	ut_ad(mutex_own(&(purge_sys->mutex)));
loop:	
	mtr_start(&mtr);
	mutex_enter(&(rseg->mutex));	
	
	rseg_hdr = trx_rsegf_get(rseg->space, rseg->page_no, &mtr);

	undo_page = trx_undo_page_get(rseg->space, hdr_addr.page, &mtr);
	seg_hdr = undo_page + TRX_UNDO_SEG_HDR;
	log_hdr = undo_page + hdr_addr.boffset;

	/* Mark the last undo log totally purged, so that if the system
	crashes, the tail of the undo log will not get accessed again. The
	list of pages in the undo log tail gets inconsistent during the
	freeing of the segment, and therefore purge should not try to access
	them again. 将最后一个撤消日志标记为完全清除，这样，如果系统崩溃，将不会再次访问撤消日志的尾部。
	在释放段期间，undo日志尾中的页面列表变得不一致，因此清除不应该尝试再次访问它们。*/
	if (!marked) {
		mlog_write_ulint(log_hdr + TRX_UNDO_DEL_MARKS, FALSE,
							MLOG_2BYTES, &mtr);
		marked = TRUE;
	}
	
	freed = fseg_free_step_not_header(seg_hdr + TRX_UNDO_FSEG_HEADER,
									&mtr);
	if (!freed) {
		mutex_exit(&(rseg->mutex));	
		mtr_commit(&mtr);

		goto loop;
	}

	/* The page list may now be inconsistent, but the length field
	stored in the list base node tells us how big it was before we
	started the freeing. 页面列表现在可能不一致，但是存储在列表基本节点中的length字段告诉我们在开始释放之前它有多大。*/
	
	seg_size = flst_get_len(seg_hdr + TRX_UNDO_PAGE_LIST, &mtr);

	/* We may free the undo log segment header page; it must be freed
	within the same mtr as the undo log header is removed from the
	history list: otherwise, in case of a database crash, the segment
	could become inaccessible garbage in the file space. 
	我们可以释放undo日志段报头页;当undo日志头从历史列表中删除时，必须在相同的MTR中释放它:否则，在数据库崩溃的情况下，段可能成为文件空间中不可访问的垃圾。*/
	flst_cut_end(rseg_hdr + TRX_RSEG_HISTORY,
			log_hdr + TRX_UNDO_HISTORY_NODE, n_removed_logs, &mtr);
	freed = FALSE;

	while (!freed) {
		/* Here we assume that a file segment with just the header
		page can be freed in a few steps, so that the buffer pool
		is not flooded with bufferfixed pages: see the note in
		fsp0fsp.c. 这里我们假设一个只有头页的文件段可以通过几个步骤释放，这样缓冲池就不会被缓冲固定页淹没:参见fsp0fsp.c中的说明。*/
		freed = fseg_free_step(seg_hdr + TRX_UNDO_FSEG_HEADER,
									&mtr);
	}

	hist_size = mtr_read_ulint(rseg_hdr + TRX_RSEG_HISTORY_SIZE,
							MLOG_4BYTES, &mtr);
	ut_ad(hist_size >= seg_size);

	mlog_write_ulint(rseg_hdr + TRX_RSEG_HISTORY_SIZE,
				hist_size - seg_size, MLOG_4BYTES, &mtr);

	ut_ad(rseg->curr_size >= seg_size);
			
	rseg->curr_size -= seg_size;

	mutex_exit(&(rseg->mutex));	

	mtr_commit(&mtr);
}

/************************************************************************
Removes unnecessary history data from a rollback segment.从回滚段中删除不必要的历史数据。 */
static
void
trx_purge_truncate_rseg_history(
/*============================*/
	trx_rseg_t*	rseg,		/* in: rollback segment */
	dulint		limit_trx_no,	/* in: remove update undo logs whose
					trx number is < limit_trx_no */
	dulint		limit_undo_no)	/* in: if transaction number is equal
					to limit_trx_no, truncate undo records
					with undo number < limit_undo_no */
{
	fil_addr_t	hdr_addr;
	fil_addr_t	prev_hdr_addr;
	trx_rsegf_t*	rseg_hdr;
	page_t*		undo_page;
	trx_ulogf_t*	log_hdr;
	trx_usegf_t*	seg_hdr;
	int		cmp;
	ulint		n_removed_logs	= 0;
	mtr_t		mtr;

	ut_ad(mutex_own(&(purge_sys->mutex)));

	mtr_start(&mtr);
	mutex_enter(&(rseg->mutex));	
	
	rseg_hdr = trx_rsegf_get(rseg->space, rseg->page_no, &mtr);

	hdr_addr = trx_purge_get_log_from_hist(
			flst_get_last(rseg_hdr + TRX_RSEG_HISTORY, &mtr));
loop:
	if (hdr_addr.page == FIL_NULL) {

		mutex_exit(&(rseg->mutex));	

		mtr_commit(&mtr);

		return;
	}

	undo_page = trx_undo_page_get(rseg->space, hdr_addr.page, &mtr);

	log_hdr = undo_page + hdr_addr.boffset;

 	cmp = ut_dulint_cmp(mach_read_from_8(log_hdr + TRX_UNDO_TRX_NO),
 			  					limit_trx_no);
	if (cmp == 0) {
		trx_undo_truncate_start(rseg, rseg->space, hdr_addr.page,
					hdr_addr.boffset, limit_undo_no);
	}

	if (cmp >= 0) {
		flst_truncate_end(rseg_hdr + TRX_RSEG_HISTORY,
	    			log_hdr + TRX_UNDO_HISTORY_NODE,
				n_removed_logs, &mtr);

		mutex_exit(&(rseg->mutex));	
		mtr_commit(&mtr);

		return;
	}

	prev_hdr_addr = trx_purge_get_log_from_hist(
			  flst_get_prev_addr(log_hdr + TRX_UNDO_HISTORY_NODE,
									&mtr));
	n_removed_logs++;
	
	seg_hdr = undo_page + TRX_UNDO_SEG_HDR;

	if ((mach_read_from_2(seg_hdr + TRX_UNDO_STATE) == TRX_UNDO_TO_PURGE)
	     && (mach_read_from_2(log_hdr + TRX_UNDO_NEXT_LOG) == 0)) {

		/* We can free the whole log segment */

		mutex_exit(&(rseg->mutex));	
		mtr_commit(&mtr);
		
		trx_purge_free_segment(rseg, hdr_addr, n_removed_logs);

		n_removed_logs = 0;
	} else {
		mutex_exit(&(rseg->mutex));	
		mtr_commit(&mtr);
	}

	mtr_start(&mtr);
	mutex_enter(&(rseg->mutex));	

	rseg_hdr = trx_rsegf_get(rseg->space, rseg->page_no, &mtr);

	hdr_addr = prev_hdr_addr;
	
	goto loop;
}

/************************************************************************
Removes unnecessary history data from rollback segments. NOTE that when this
function is called, the caller must not have any latches on undo log pages! 
从回滚段中删除不必要的历史数据。注意，当这个函数被调用时，调用者不能在撤消日志页面上有任何锁存!*/
static
void
trx_purge_truncate_history(void)
/*============================*/
{
	trx_rseg_t*	rseg;
	dulint		limit_trx_no;
	dulint		limit_undo_no;

	ut_ad(mutex_own(&(purge_sys->mutex)));

	trx_purge_arr_get_biggest(purge_sys->arr, &limit_trx_no,
							&limit_undo_no);
	
	if (ut_dulint_cmp(limit_trx_no, ut_dulint_zero) == 0) {
		
		limit_trx_no = purge_sys->purge_trx_no;
		limit_undo_no = purge_sys->purge_undo_no;
	}

	/* We play safe and set the truncate limit at most to the purge view
	low_limit number, though this is not necessary 为了安全起见，我们将截断限制最多设置为清除视图low_limit数，尽管这不是必要的*/

	if (ut_dulint_cmp(limit_trx_no, purge_sys->view->low_limit_no) >= 0) {
		limit_trx_no = purge_sys->view->low_limit_no;
		limit_undo_no = ut_dulint_zero;
	}

	ut_ad((ut_dulint_cmp(limit_trx_no,
				purge_sys->view->low_limit_no) <= 0));

	rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);

	while (rseg) {
		trx_purge_truncate_rseg_history(rseg, limit_trx_no,
							limit_undo_no);
		rseg = UT_LIST_GET_NEXT(rseg_list, rseg);
	}
}

/************************************************************************
Does a truncate if the purge array is empty. NOTE that when this function is
called, the caller must not have any latches on undo log pages! 
如果清除数组为空，则进行截断。注意，当这个函数被调用时，调用者不能在撤消日志页面上有任何锁存!*/
UNIV_INLINE
ibool
trx_purge_truncate_if_arr_empty(void)
/*=================================*/
			/* out: TRUE if array empty */
{
	ut_ad(mutex_own(&(purge_sys->mutex)));

	if (purge_sys->arr->n_used == 0) {

		trx_purge_truncate_history();

		return(TRUE);
	}

	return(FALSE);
}

/***************************************************************************
Updates the last not yet purged history log info in rseg when we have purged
a whole undo log. Advances also purge_sys->purge_trx_no past the purged log. 
当我们已经清除了一个完整的撤消日志时，更新rseg中最后一个尚未清除的历史日志信息。purge_sys->purge_trx_no过去被清除的日志。*/
static 
void
trx_purge_rseg_get_next_history_log(
/*================================*/
	trx_rseg_t*	rseg)	/* in: rollback segment */
{
	page_t* 	undo_page;
	trx_ulogf_t*	log_hdr;
	trx_usegf_t*	seg_hdr;
	fil_addr_t	prev_log_addr;
	dulint		trx_no;
	ibool		del_marks;
	mtr_t		mtr;

	ut_ad(mutex_own(&(purge_sys->mutex)));

	mutex_enter(&(rseg->mutex));

	ut_ad(rseg->last_page_no != FIL_NULL);

	purge_sys->purge_trx_no = ut_dulint_add(rseg->last_trx_no, 1);
	purge_sys->purge_undo_no = ut_dulint_zero;
	purge_sys->next_stored = FALSE;
	
	mtr_start(&mtr);
	
	undo_page = trx_undo_page_get_s_latched(rseg->space,
						rseg->last_page_no, &mtr);
	log_hdr = undo_page + rseg->last_offset;
	seg_hdr = undo_page + TRX_UNDO_SEG_HDR;

	if ((mach_read_from_2(log_hdr + TRX_UNDO_NEXT_LOG) == 0)
	    && (mach_read_from_2(seg_hdr + TRX_UNDO_STATE)
		== TRX_UNDO_TO_PURGE)) {
	
		/* This is the last log header on this page and the log
		segment cannot be reused: we may increment the number of
		pages handled 这是该页面的最后一个日志头，日志段不能被重用:我们可以增加所处理的页的数量*/

		purge_sys->n_pages_handled++;
	}

	prev_log_addr = trx_purge_get_log_from_hist(
			 flst_get_prev_addr(log_hdr + TRX_UNDO_HISTORY_NODE,
									&mtr));
	if (prev_log_addr.page == FIL_NULL) {
		/* No logs left in the history list 历史列表中没有任何记录*/

		rseg->last_page_no = FIL_NULL;
	
		mutex_exit(&(rseg->mutex));
		mtr_commit(&mtr);

		return;
	}

	mutex_exit(&(rseg->mutex));
	mtr_commit(&mtr);

	/* Read the trx number and del marks from the previous log header 从以前的日志标头中读取trx号和del标记*/
	mtr_start(&mtr);

	log_hdr = trx_undo_page_get_s_latched(rseg->space,
						prev_log_addr.page, &mtr)
		  + prev_log_addr.boffset;

	trx_no = mach_read_from_8(log_hdr + TRX_UNDO_TRX_NO);
	
	del_marks = mach_read_from_2(log_hdr + TRX_UNDO_DEL_MARKS);

	mtr_commit(&mtr);

	mutex_enter(&(rseg->mutex));

	rseg->last_page_no = prev_log_addr.page;
	rseg->last_offset = prev_log_addr.boffset;
	rseg->last_trx_no = trx_no;
	rseg->last_del_marks = del_marks;

	mutex_exit(&(rseg->mutex));
}
	
/***************************************************************************
Chooses the next undo log to purge and updates the info in purge_sys. This
function is used to initialize purge_sys when the next record to purge is
not known, and also to update the purge system info on the next record when
purge has handled the whole undo log for a transaction. 
选择下一个撤消日志来清除和更新purge_sys中的信息。当不知道要清除的下一个记录时，该函数用于初始化purge_sys，
当清除处理了一个事务的整个撤销日志时，该函数还用于更新下一个记录上的清除系统信息。*/
static 
void
trx_purge_choose_next_log(void)
/*===========================*/
{
	trx_undo_rec_t*	rec;
	trx_rseg_t*	rseg;
	trx_rseg_t*	min_rseg;
	dulint		min_trx_no;
	ulint		space;
	ulint		page_no;
	ulint		offset;
	mtr_t		mtr;
	
	ut_ad(mutex_own(&(purge_sys->mutex)));
	ut_ad(purge_sys->next_stored == FALSE);

	rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);

	min_trx_no = ut_dulint_max;

	min_rseg = NULL;
	
	while (rseg) {
		mutex_enter(&(rseg->mutex));
		
		if (rseg->last_page_no != FIL_NULL) {

			if ((min_rseg == NULL)
			    || (ut_dulint_cmp(min_trx_no, rseg->last_trx_no)
			    	> 0)) {

				min_rseg = rseg;
				min_trx_no = rseg->last_trx_no;
				space = rseg->space;
				ut_a(space == 0); /* We assume in purge of
						externally stored fields
						that space id == 0 */
				page_no = rseg->last_page_no;
				offset = rseg->last_offset;
			}
		}

		mutex_exit(&(rseg->mutex));

		rseg = UT_LIST_GET_NEXT(rseg_list, rseg);
	}
	
	if (min_rseg == NULL) {

		return;
	}

	mtr_start(&mtr);

	if (!min_rseg->last_del_marks) {
		/* No need to purge this log */

		rec = &trx_purge_dummy_rec;
	} else {
		rec = trx_undo_get_first_rec(space, page_no, offset,
							RW_S_LATCH, &mtr);
		if (rec == NULL) {
			/* Undo log empty */

			rec = &trx_purge_dummy_rec;
		}
	}
	
	purge_sys->next_stored = TRUE;
	purge_sys->rseg = min_rseg;

	purge_sys->hdr_page_no = page_no;
	purge_sys->hdr_offset = offset;

	purge_sys->purge_trx_no = min_trx_no;

	if (rec == &trx_purge_dummy_rec) {

		purge_sys->purge_undo_no = ut_dulint_zero;
		purge_sys->page_no = page_no;
		purge_sys->offset = 0;
	} else {
		purge_sys->purge_undo_no = trx_undo_rec_get_undo_no(rec);

		purge_sys->page_no = buf_frame_get_page_no(rec);
		purge_sys->offset = rec - buf_frame_align(rec);
	}

	mtr_commit(&mtr);
}

/***************************************************************************
Gets the next record to purge and updates the info in the purge system. 获取要清除的下一个记录并更新清除系统中的信息。*/
static
trx_undo_rec_t*
trx_purge_get_next_rec(
/*===================*/
				/* out: copy of an undo log record or
				pointer to the dummy undo log record */
	mem_heap_t*	heap)	/* in: memory heap where copied */
{
	trx_undo_rec_t*	rec;
	trx_undo_rec_t*	rec_copy;
	trx_undo_rec_t*	rec2;
	trx_undo_rec_t*	next_rec;
	page_t* 	undo_page;
	page_t* 	page;
	ulint		offset;
	ulint		page_no;
	ulint		space;
	ulint		type;
	ulint		cmpl_info;
	mtr_t		mtr;

	ut_ad(mutex_own(&(purge_sys->mutex)));
	ut_ad(purge_sys->next_stored);

	space = purge_sys->rseg->space;
	page_no = purge_sys->page_no;
	offset = purge_sys->offset;

	if (offset == 0) {
		/* It is the dummy undo log record, which means that there is
		no need to purge this undo log 它是虚拟的撤销日志记录，这意味着不需要清除该撤销日志*/

		trx_purge_rseg_get_next_history_log(purge_sys->rseg);
	
		/* Look for the next undo log and record to purge 寻找下一个要清除的撤销日志和记录*/

		trx_purge_choose_next_log();

		return(&trx_purge_dummy_rec);
	}
			
	mtr_start(&mtr);

	undo_page = trx_undo_page_get_s_latched(space, page_no, &mtr);
	rec = undo_page + offset;

	rec2 = rec;

	for (;;) {
		/* Try first to find the next record which requires a purge
		operation from the same page of the same undo log 首先尝试找到需要从同一撤销日志的同一页进行清除操作的下一个记录*/
	
		next_rec = trx_undo_page_get_next_rec(rec2,
						purge_sys->hdr_page_no,
						purge_sys->hdr_offset);
		if (next_rec == NULL) {
			rec2 = trx_undo_get_next_rec(rec2,
						purge_sys->hdr_page_no,
						purge_sys->hdr_offset, &mtr);
			break;
		}

		rec2 = next_rec;
		
		type = trx_undo_rec_get_type(rec2);

		if (type == TRX_UNDO_DEL_MARK_REC) {

			break;
		}	    		

		cmpl_info = trx_undo_rec_get_cmpl_info(rec2);

		if (trx_undo_rec_get_extern_storage(rec2)) {
			break;
		}
		
		if ((type == TRX_UNDO_UPD_EXIST_REC)
				&& !(cmpl_info & UPD_NODE_NO_ORD_CHANGE)) {
	    	    	break;
	   	}
	}

	if (rec2 == NULL) {
		mtr_commit(&mtr);
		
		trx_purge_rseg_get_next_history_log(purge_sys->rseg);
	
		/* Look for the next undo log and record to purge 寻找下一个要清除的撤销日志和记录*/

		trx_purge_choose_next_log();		

		mtr_start(&mtr);

		undo_page = trx_undo_page_get_s_latched(space, page_no, &mtr);

		rec = undo_page + offset;
	} else {
		page = buf_frame_align(rec2);
		
		purge_sys->purge_undo_no = trx_undo_rec_get_undo_no(rec2);
		purge_sys->page_no = buf_frame_get_page_no(page);
		purge_sys->offset = rec2 - page;

		if (undo_page != page) {
			/* We advance to a new page of the undo log: 我们进入撤销日志的新页面:*/
			purge_sys->n_pages_handled++;
		}
	}
	
	rec_copy = trx_undo_rec_copy(rec, heap);

	mtr_commit(&mtr);

	return(rec_copy);
}

/************************************************************************
Fetches the next undo log record from the history list to purge. It must be
released with the corresponding release function. 从要清除的历史列表中获取下一个撤消日志记录。必须有相应的释放功能。*/

trx_undo_rec_t*
trx_purge_fetch_next_rec(
/*=====================*/
				/* out: copy of an undo log record or
				pointer to the dummy undo log record
				&trx_purge_dummy_rec, if the whole undo log
				can skipped in purge; NULL if none left */
	dulint*		roll_ptr,/* out: roll pointer to undo record */
	trx_undo_inf_t** cell,	/* out: storage cell for the record in the
				purge array */
	mem_heap_t*	heap)	/* in: memory heap where copied */
{
	trx_undo_rec_t*	undo_rec;
	
	mutex_enter(&(purge_sys->mutex));

	if (purge_sys->state == TRX_STOP_PURGE) {
		trx_purge_truncate_if_arr_empty();

		mutex_exit(&(purge_sys->mutex));

		return(NULL);
	}

	if (!purge_sys->next_stored) {
		trx_purge_choose_next_log();

		if (!purge_sys->next_stored) {
			purge_sys->state = TRX_STOP_PURGE;
	
			trx_purge_truncate_if_arr_empty();

			if (srv_print_thread_releases) {
				printf(
	"Purge: No logs left in the history list; pages handled %lu\n",
					purge_sys->n_pages_handled);
			}

			mutex_exit(&(purge_sys->mutex));

			return(NULL);
		}			
	}	

	if (purge_sys->n_pages_handled >= purge_sys->handle_limit) {

		purge_sys->state = TRX_STOP_PURGE;
	
		trx_purge_truncate_if_arr_empty();

		mutex_exit(&(purge_sys->mutex));

		return(NULL);
	}		

	if (ut_dulint_cmp(purge_sys->purge_trx_no,
				purge_sys->view->low_limit_no) >= 0) {
		purge_sys->state = TRX_STOP_PURGE;
	
		trx_purge_truncate_if_arr_empty();

		mutex_exit(&(purge_sys->mutex));

		return(NULL);
	}
		
/*	printf("Thread %lu purging trx %lu undo record %lu\n",
		os_thread_get_curr_id(),
		ut_dulint_get_low(purge_sys->purge_trx_no),
		ut_dulint_get_low(purge_sys->purge_undo_no)); */

	*roll_ptr = trx_undo_build_roll_ptr(FALSE, (purge_sys->rseg)->id,
							purge_sys->page_no,
							purge_sys->offset);

	*cell = trx_purge_arr_store_info(purge_sys->purge_trx_no,
					 purge_sys->purge_undo_no);

	ut_ad(ut_dulint_cmp(purge_sys->purge_trx_no,
			    (purge_sys->view)->low_limit_no) < 0);
	
	/* The following call will advance the stored values of purge_trx_no
	and purge_undo_no, therefore we had to store them first 
	下面的调用将提前存储purge_trx_no和purge_undo_no的值，因此我们必须先存储它们*/
	undo_rec = trx_purge_get_next_rec(heap);

	mutex_exit(&(purge_sys->mutex));

	return(undo_rec);
}

/***********************************************************************
Releases a reserved purge undo record. 释放一个保留的清除撤销记录。*/

void
trx_purge_rec_release(
/*==================*/
	trx_undo_inf_t*	cell)	/* in: storage cell */
{
	trx_undo_arr_t*	arr;
	
	mutex_enter(&(purge_sys->mutex));

	arr = purge_sys->arr;

	trx_purge_arr_remove_info(cell);

	mutex_exit(&(purge_sys->mutex));
}

/***********************************************************************
This function runs a purge batch. 此函数运行一个清除批处理。*/

ulint
trx_purge(void)
/*===========*/
				/* out: number of undo log pages handled in
				the batch */
{
	que_thr_t*	thr;
/*	que_thr_t*	thr2; */
	ulint		old_pages_handled;

	mutex_enter(&(purge_sys->mutex));

	if (purge_sys->trx->n_active_thrs > 0) {
	
		mutex_exit(&(purge_sys->mutex));

		/* Should not happen */

		ut_a(0);
		
		return(0);
	}		

	rw_lock_x_lock(&(purge_sys->latch));

	mutex_enter(&kernel_mutex);

	/* Close and free the old purge view 关闭并释放旧的清除视图*/	

	read_view_close(purge_sys->view);
	purge_sys->view = NULL;
	mem_heap_empty(purge_sys->heap);

	purge_sys->view = read_view_oldest_copy_or_open_new(NULL,
							purge_sys->heap);
	mutex_exit(&kernel_mutex);	

	rw_lock_x_unlock(&(purge_sys->latch));

	purge_sys->state = TRX_PURGE_ON;	
	
	/* Handle at most 20 undo log pages in one purge batch */

	purge_sys->handle_limit = purge_sys->n_pages_handled + 20;

	old_pages_handled = purge_sys->n_pages_handled;

	mutex_exit(&(purge_sys->mutex));

	mutex_enter(&kernel_mutex);

	thr = que_fork_start_command(purge_sys->query, SESS_COMM_EXECUTE, 0);

	ut_ad(thr);
	
/*	thr2 = que_fork_start_command(purge_sys->query, SESS_COMM_EXECUTE, 0);
	
	ut_ad(thr2); */
	

	mutex_exit(&kernel_mutex);

/*	srv_que_task_enqueue(thr2); */

	if (srv_print_thread_releases) {
	
		printf("Starting purge\n");
	}

	que_run_threads(thr);

	if (srv_print_thread_releases) {

		printf(
		"Purge ends; pages handled %lu\n", purge_sys->n_pages_handled);
	}

	return(purge_sys->n_pages_handled - old_pages_handled);
}

/**********************************************************************
Prints information of the purge system to stderr. 将清除系统的信息打印到标准错误。*/

void
trx_purge_sys_print(void)
/*=====================*/
{
	fprintf(stderr, "InnoDB: Purge system view:\n");
	read_view_print(purge_sys->view);

	fprintf(stderr, "InnoDB: Purge trx n:o %lu %lu, undo n_o %lu %lu\n",
			ut_dulint_get_high(purge_sys->purge_trx_no),
			ut_dulint_get_low(purge_sys->purge_trx_no),
			ut_dulint_get_high(purge_sys->purge_undo_no),
			ut_dulint_get_low(purge_sys->purge_undo_no));
	fprintf(stderr,
	"InnoDB: Purge next stored %lu, page_no %lu, offset %lu,\n"
	"InnoDB: Purge hdr_page_no %lu, hdr_offset %lu\n",
		purge_sys->next_stored,
		purge_sys->page_no,
		purge_sys->offset,
		purge_sys->hdr_page_no,
		purge_sys->hdr_offset);
}
