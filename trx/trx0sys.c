/******************************************************
Transaction system 交易系统

(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#include "trx0sys.h"

#ifdef UNIV_NONINL
#include "trx0sys.ic"
#endif

#include "fsp0fsp.h"
#include "mtr0mtr.h"
#include "trx0trx.h"
#include "trx0rseg.h"
#include "trx0undo.h"
#include "srv0srv.h"
#include "trx0purge.h"
#include "log0log.h"

/* The transaction system 事务系统*/
trx_sys_t*		trx_sys 	= NULL;
trx_doublewrite_t*	trx_doublewrite = NULL;

/********************************************************************
Creates or initialializes the doublewrite buffer at a database start. 在数据库启动时创建或初始化doublewrite缓冲区。*/
static
void
trx_doublewrite_init(
/*=================*/
	byte*	doublewrite)	/* in: pointer to the doublewrite buf
				header on trx sys page In:指向TRX sys页上doublewrite buf头的指针*/
{
	trx_doublewrite = mem_alloc(sizeof(trx_doublewrite_t));

	mutex_create(&(trx_doublewrite->mutex));
	mutex_set_level(&(trx_doublewrite->mutex), SYNC_DOUBLEWRITE);

	trx_doublewrite->first_free = 0;

	trx_doublewrite->block1 = mach_read_from_4(
						doublewrite
						+ TRX_SYS_DOUBLEWRITE_BLOCK1);
	trx_doublewrite->block2 = mach_read_from_4(
						doublewrite
						+ TRX_SYS_DOUBLEWRITE_BLOCK2);
	trx_doublewrite->write_buf_unaligned = 
				ut_malloc(
				(1 + 2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE)
				* UNIV_PAGE_SIZE);
						
	trx_doublewrite->write_buf = ut_align(
					trx_doublewrite->write_buf_unaligned,
					UNIV_PAGE_SIZE);
	trx_doublewrite->buf_block_arr = mem_alloc(
					2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE
					* sizeof(void*));
}

/********************************************************************
Creates the doublewrite buffer at a database start. The header of the
doublewrite buffer is placed on the trx system header page. 
在数据库启动时创建doublewrite缓冲区。doublewrite缓冲区的头被放置在trx系统头页上。*/
void
trx_sys_create_doublewrite_buf(void)
/*================================*/
{
	page_t*	page;
	page_t*	page2;
	page_t*	new_page;
	byte*	doublewrite;
	byte*	fseg_header;
	ulint	page_no;
	ulint	prev_page_no;
	ulint	i;
	mtr_t	mtr;

	if (trx_doublewrite) {
		/* Already inited */

		return;
	}

start_again:	
	mtr_start(&mtr);

	page = buf_page_get(TRX_SYS_SPACE, TRX_SYS_PAGE_NO, RW_X_LATCH, &mtr);
	buf_page_dbg_add_level(page, SYNC_NO_ORDER_CHECK);

	doublewrite = page + TRX_SYS_DOUBLEWRITE;
	
	if (mach_read_from_4(doublewrite + TRX_SYS_DOUBLEWRITE_MAGIC)
					== TRX_SYS_DOUBLEWRITE_MAGIC_N) {

		/* The doublewrite buffer has already been created:
		just read in some numbers doublewrite缓冲区已经创建:只读取一些数字*/

		trx_doublewrite_init(doublewrite);
		
		mtr_commit(&mtr);
	} else {
		fprintf(stderr,
		"InnoDB: Doublewrite buffer not found: creating new\n");

		if (buf_pool_get_curr_size() <
					(2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE
						+ FSP_EXTENT_SIZE / 2 + 100)
					* UNIV_PAGE_SIZE) {
			fprintf(stderr,
			"InnoDB: Cannot create doublewrite buffer: you must\n"
			"InnoDB: increase your buffer pool size.\n"
			"InnoDB: Cannot continue operation.\n");

			exit(1);
		}
	
		page2 = fseg_create(TRX_SYS_SPACE, TRX_SYS_PAGE_NO,
			TRX_SYS_DOUBLEWRITE + TRX_SYS_DOUBLEWRITE_FSEG, &mtr);

		/* fseg_create acquires a second latch on the page,
		therefore we must declare it: Fseg_create在页面上获得第二个闩锁，因此必须声明它:*/

		buf_page_dbg_add_level(page2, SYNC_NO_ORDER_CHECK);

		if (page2 == NULL) {
			fprintf(stderr,
			"InnoDB: Cannot create doublewrite buffer: you must\n"
			"InnoDB: increase your tablespace size.\n"
			"InnoDB: Cannot continue operation.\n");

			/* We exit without committing the mtr to prevent
			its modifications to the database getting to disk 我们退出时没有提交mtr，以防止它对数据库的修改到达磁盘*/
			
			exit(1);
		}

		fseg_header = page + TRX_SYS_DOUBLEWRITE
						+ TRX_SYS_DOUBLEWRITE_FSEG;
		prev_page_no = 0;

		for (i = 0; i < 2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE
						+ FSP_EXTENT_SIZE / 2; i++) {
			page_no = fseg_alloc_free_page(fseg_header,
			 				prev_page_no + 1,
							FSP_UP, &mtr);
			if (page_no == FIL_NULL) {
				fprintf(stderr,
			"InnoDB: Cannot create doublewrite buffer: you must\n"
			"InnoDB: increase your tablespace size.\n"
			"InnoDB: Cannot continue operation.\n");

				exit(1);
			}

			/* We read the allocated pages to the buffer pool;
			when they are written to disk in a flush, the space
			id and page number fields are also written to the
			pages. When we at database startup read pages
			from the doublewrite buffer, we know that if the
			space id and page number in them are the same as
			the page position in the tablespace, then the page
			has not been written to in doublewrite. 
			我们将已分配的页读入缓冲池;当它们被刷新到磁盘时，空间id和页号字段也被写入到页中。
			当我们在数据库启动时从doublewrite缓冲区读取页面时，我们知道，如果其中的空间id和页码与表空间中的页位置相同，那么该页没有被写入doublewrite。*/
			
			new_page = buf_page_get(TRX_SYS_SPACE, page_no,
							RW_X_LATCH, &mtr);
			buf_page_dbg_add_level(new_page, SYNC_NO_ORDER_CHECK);

			/* Make a dummy change to the page to ensure it will
			be written to disk in a flush 对该页做一个虚拟更改，以确保它将在刷新时写入磁盘*/

			mlog_write_ulint(new_page + FIL_PAGE_DATA,
					TRX_SYS_DOUBLEWRITE_MAGIC_N,
					MLOG_4BYTES, &mtr);

			if (i == FSP_EXTENT_SIZE / 2) {
				mlog_write_ulint(doublewrite
						+ TRX_SYS_DOUBLEWRITE_BLOCK1,
						page_no, MLOG_4BYTES, &mtr);
				mlog_write_ulint(doublewrite
						+ TRX_SYS_DOUBLEWRITE_REPEAT
						+ TRX_SYS_DOUBLEWRITE_BLOCK1,
						page_no, MLOG_4BYTES, &mtr);
			} else if (i == FSP_EXTENT_SIZE / 2
					+ TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
				mlog_write_ulint(doublewrite
						+ TRX_SYS_DOUBLEWRITE_BLOCK2,
						page_no, MLOG_4BYTES, &mtr);
				mlog_write_ulint(doublewrite
						+ TRX_SYS_DOUBLEWRITE_REPEAT
						+ TRX_SYS_DOUBLEWRITE_BLOCK2,
						page_no, MLOG_4BYTES, &mtr);
			} else if (i > FSP_EXTENT_SIZE / 2) {
				ut_a(page_no == prev_page_no + 1);
			}

			prev_page_no = page_no;
		}

		mlog_write_ulint(doublewrite + TRX_SYS_DOUBLEWRITE_MAGIC,
				TRX_SYS_DOUBLEWRITE_MAGIC_N, MLOG_4BYTES, &mtr);
		mlog_write_ulint(doublewrite + TRX_SYS_DOUBLEWRITE_MAGIC
						+ TRX_SYS_DOUBLEWRITE_REPEAT,
				TRX_SYS_DOUBLEWRITE_MAGIC_N, MLOG_4BYTES, &mtr);
		mtr_commit(&mtr);
		
		/* Flush the modified pages to disk and make a checkpoint 将修改后的页刷新到磁盘并创建检查点*/
		log_make_checkpoint_at(ut_dulint_max, TRUE);

		fprintf(stderr, "InnoDB: Doublewrite buffer created\n");

		goto start_again;
	}
}

/********************************************************************
At a database startup uses a possible doublewrite buffer to restore
half-written pages in the data files. 
在数据库启动时，使用一个可能的doublewrite缓冲区来恢复数据文件中半写的页。*/
void
trx_sys_doublewrite_restore_corrupt_pages(void)
/*===========================================*/
{
	byte*	buf;
	byte*	read_buf;
	byte*	unaligned_read_buf;
	ulint	block1;
	ulint	block2;
	byte*	page;
	byte*	doublewrite;
	ulint	space_id;
	ulint	page_no;
	ulint	i;
	
	/* We do the file i/o past the buffer pool 我们通过缓冲池执行文件i/o*/

	unaligned_read_buf = ut_malloc(2 * UNIV_PAGE_SIZE);
	read_buf = ut_align(unaligned_read_buf, UNIV_PAGE_SIZE);	

	/* Read the trx sys header to check if we are using the
	doublewrite buffer 读取trx sys头文件以检查是否使用了doublewrite缓冲区*/

	fil_io(OS_FILE_READ, TRUE, TRX_SYS_SPACE, TRX_SYS_PAGE_NO, 0,
					UNIV_PAGE_SIZE, read_buf, NULL);

	doublewrite = read_buf + TRX_SYS_DOUBLEWRITE;

	if (mach_read_from_4(doublewrite + TRX_SYS_DOUBLEWRITE_MAGIC)
					== TRX_SYS_DOUBLEWRITE_MAGIC_N) {
		/* The doublewrite buffer has been created 
		doublewrite缓冲区已经创建*/
		trx_doublewrite_init(doublewrite);

		block1 = trx_doublewrite->block1;
		block2 = trx_doublewrite->block2;

		buf = trx_doublewrite->write_buf;
	} else {
		goto leave_func;
	}

	/* Read the pages from the doublewrite buffer to memory 将页从doublewrite缓冲区读入内存*/

	fil_io(OS_FILE_READ, TRUE, TRX_SYS_SPACE, block1, 0,
			TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * UNIV_PAGE_SIZE,
			buf, NULL);
	fil_io(OS_FILE_READ, TRUE, TRX_SYS_SPACE, block2, 0,
			TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * UNIV_PAGE_SIZE,
			buf + TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * UNIV_PAGE_SIZE,
			NULL);
	/* Check if any of these pages is half-written in data files, in the
	intended position 检查这些页面中是否有半写在数据文件的指定位置*/

	page = buf;
	
	for (i = 0; i < TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * 2; i++) {
		
		space_id = mach_read_from_4(page + FIL_PAGE_SPACE);
		page_no = mach_read_from_4(page + FIL_PAGE_OFFSET);

		if (!fil_check_adress_in_tablespace(space_id, page_no)) {
		  	fprintf(stderr,
	"InnoDB: Warning: an inconsistent page in the doublewrite buffer\n"
	"InnoDB: space id %lu page number %lu, %lu'th page in dblwr buf.\n",
				space_id, page_no, i);
		
		} else if (space_id == TRX_SYS_SPACE
		    && (  (page_no >= block1
			   && page_no
				< block1 + TRX_SYS_DOUBLEWRITE_BLOCK_SIZE)
		        || (page_no >= block2
			   && page_no
				< block2 + TRX_SYS_DOUBLEWRITE_BLOCK_SIZE))) {

			/* It is an unwritten doublewrite buffer page:
			do nothing 它是一个未写的doublewrite缓冲页:什么也不做*/

		} else {
			/* Read in the actual page from the data files 从数据文件中读取实际页面*/
			
			fil_io(OS_FILE_READ, TRUE, space_id, page_no, 0,
					UNIV_PAGE_SIZE, read_buf, NULL);
			/* Check if the page is corrupt 检查页面是否损坏*/

			if (buf_page_is_corrupted(read_buf)) {

		  		fprintf(stderr,
		"InnoDB: Warning: database page corruption or a failed\n"
		"InnoDB: file read of page %lu.\n", page_no);
		  		fprintf(stderr,
		"InnoDB: Trying to recover it from the doublewrite buffer.\n");
				
				if (buf_page_is_corrupted(page)) {
		  			fprintf(stderr,
		"InnoDB: Also the page in the doublewrite buffer is corrupt.\n"
		"InnoDB: Cannot continue operation.\n");
					exit(1);
				}

				/* Write the good page from the
				doublewrite buffer to the intended
				position 将好的页从doublewrite缓冲区写到预期的位置*/

				fil_io(OS_FILE_WRITE, TRUE, space_id,
					page_no, 0,
					UNIV_PAGE_SIZE, page, NULL);
		  		fprintf(stderr,
		"InnoDB: Recovered the page from the doublewrite buffer.\n");
			}
		}

		page += UNIV_PAGE_SIZE;
	}

	fil_flush_file_spaces(FIL_TABLESPACE);
	
leave_func:
	ut_free(unaligned_read_buf);
}

/********************************************************************
Checks that trx is in the trx list. */

ibool
trx_in_trx_list(
/*============*/
			/* out: TRUE if is in */
	trx_t*	in_trx)	/* in: trx */
{
	trx_t*	trx;

	ut_ad(mutex_own(&(kernel_mutex)));

	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (trx != NULL) {

		if (trx == in_trx) {

			return(TRUE);
		}

		trx = UT_LIST_GET_NEXT(trx_list, trx);
	}

	return(FALSE);
}

/*********************************************************************
Writes the value of max_trx_id to the file based trx system header. 将max_trx_id的值写入基于文件的trx系统头文件。*/

void
trx_sys_flush_max_trx_id(void)
/*==========================*/
{
	trx_sysf_t*	sys_header;
	mtr_t		mtr;

	ut_ad(mutex_own(&kernel_mutex));

	mtr_start(&mtr);

	sys_header = trx_sysf_get(&mtr);

	mlog_write_dulint(sys_header + TRX_SYS_TRX_ID_STORE,
				trx_sys->max_trx_id, MLOG_8BYTES, &mtr);
	mtr_commit(&mtr);
}

/*********************************************************************
Updates the offset information about the end of the MySQL binlog entry
which corresponds to the transaction just being committed. */

void
trx_sys_update_mysql_binlog_offset(
/*===============================*/
	trx_t*	trx,	/* in: transaction being committed */
	mtr_t*	mtr)	/* in: mtr */
{
	trx_sysf_t*	sys_header;
	char		namebuf[TRX_SYS_MYSQL_LOG_NAME_LEN];
	
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(trx->mysql_log_file_name);

	memset(namebuf, ' ', TRX_SYS_MYSQL_LOG_NAME_LEN - 1);
	namebuf[TRX_SYS_MYSQL_LOG_NAME_LEN - 1] = '\0';

	/* Copy the whole MySQL log file name to the buffer, or only the
	last characters, if it does not fit */

	if (ut_strlen(trx->mysql_log_file_name)
			> TRX_SYS_MYSQL_LOG_NAME_LEN - 1) {
		ut_memcpy(namebuf, trx->mysql_log_file_name
			+ ut_strlen(trx->mysql_log_file_name)
			- (TRX_SYS_MYSQL_LOG_NAME_LEN - 1),
			TRX_SYS_MYSQL_LOG_NAME_LEN - 1);
	} else {
		ut_memcpy(namebuf, trx->mysql_log_file_name,
				1 + ut_strlen(trx->mysql_log_file_name));
	}

	namebuf[TRX_SYS_MYSQL_LOG_NAME_LEN - 1] = '\0';

	sys_header = trx_sysf_get(mtr);

	if (mach_read_from_4(sys_header + TRX_SYS_MYSQL_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_MAGIC_N_FLD)
	   != TRX_SYS_MYSQL_LOG_MAGIC_N) {

	   	mlog_write_ulint(sys_header + TRX_SYS_MYSQL_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_MAGIC_N_FLD,
				TRX_SYS_MYSQL_LOG_MAGIC_N,
				MLOG_4BYTES, mtr);
	}

	if (0 != ut_memcmp(sys_header + TRX_SYS_MYSQL_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_NAME,
			 namebuf, TRX_SYS_MYSQL_LOG_NAME_LEN)) {

		mlog_write_string(sys_header + TRX_SYS_MYSQL_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_NAME,
				namebuf, TRX_SYS_MYSQL_LOG_NAME_LEN, mtr);
	}

	if (mach_read_from_4(sys_header + TRX_SYS_MYSQL_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_OFFSET_HIGH) > 0
	   || (trx->mysql_log_offset >> 32) > 0) {
				
		mlog_write_ulint(sys_header + TRX_SYS_MYSQL_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_OFFSET_HIGH,
				(ulint)(trx->mysql_log_offset >> 32),
				MLOG_4BYTES, mtr);
	}

	mlog_write_ulint(sys_header + TRX_SYS_MYSQL_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_OFFSET_LOW,
				(ulint)(trx->mysql_log_offset & 0xFFFFFFFF),
				MLOG_4BYTES, mtr);				

	trx->mysql_log_file_name = NULL;
}

/*********************************************************************
Prints to stderr the MySQL binlog offset info in the trx system header if
the magic number shows it valid. */

void
trx_sys_print_mysql_binlog_offset(void)
/*===================================*/
{
	trx_sysf_t*	sys_header;
	mtr_t		mtr;
	
	mtr_start(&mtr);

	sys_header = trx_sysf_get(&mtr);

	if (mach_read_from_4(sys_header + TRX_SYS_MYSQL_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_MAGIC_N_FLD)
	   != TRX_SYS_MYSQL_LOG_MAGIC_N) {

		mtr_commit(&mtr);

		return;
	}

	fprintf(stderr,
	"InnoDB: Last MySQL binlog file offset %lu %lu, file name %s\n",
		mach_read_from_4(sys_header + TRX_SYS_MYSQL_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_OFFSET_HIGH),
		mach_read_from_4(sys_header + TRX_SYS_MYSQL_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_OFFSET_LOW),
		sys_header + TRX_SYS_MYSQL_LOG_INFO + TRX_SYS_MYSQL_LOG_NAME);

	mtr_commit(&mtr);
}

/********************************************************************
Looks for a free slot for a rollback segment in the trx system file copy. */

ulint
trx_sysf_rseg_find_free(
/*====================*/
			/* out: slot index or ULINT_UNDEFINED if not found */
	mtr_t*	mtr)	/* in: mtr */
{
	trx_sysf_t*	sys_header;
	ulint		page_no;
	ulint		i;
	
	ut_ad(mutex_own(&(kernel_mutex)));

	sys_header = trx_sysf_get(mtr);

	for (i = 0; i < TRX_SYS_N_RSEGS; i++) {

		page_no = trx_sysf_rseg_get_page_no(sys_header, i, mtr);

		if (page_no == FIL_NULL) {

			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}
	
/*********************************************************************
Creates the file page for the transaction system. This function is called only
at the database creation, before trx_sys_init. */
static
void
trx_sysf_create(
/*============*/
	mtr_t*	mtr)	/* in: mtr */
{
	trx_sysf_t*	sys_header;
	ulint		slot_no;
	page_t*		page;
	ulint		page_no;
	ulint		i;
	
	ut_ad(mtr);

	/* Note that below we first reserve the file space x-latch, and
	then enter the kernel: we must do it in this order to conform
	to the latching order rules. */

	mtr_x_lock(fil_space_get_latch(TRX_SYS_SPACE), mtr);
	mutex_enter(&kernel_mutex);

	/* Create the trx sys file block in a new allocated file segment */
	page = fseg_create(TRX_SYS_SPACE, 0, TRX_SYS + TRX_SYS_FSEG_HEADER,
					    				mtr);
	ut_a(buf_frame_get_page_no(page) == TRX_SYS_PAGE_NO);

	buf_page_dbg_add_level(page, SYNC_TRX_SYS_HEADER);

	sys_header = trx_sysf_get(mtr);

	/* Start counting transaction ids from number 1 up */
	mlog_write_dulint(sys_header + TRX_SYS_TRX_ID_STORE,
				ut_dulint_create(0, 1), MLOG_8BYTES, mtr);

	/* Reset the rollback segment slots */
	for (i = 0; i < TRX_SYS_N_RSEGS; i++) {

		trx_sysf_rseg_set_page_no(sys_header, i, FIL_NULL, mtr);
	}

	/* Create the first rollback segment in the SYSTEM tablespace */
	page_no = trx_rseg_header_create(TRX_SYS_SPACE, ULINT_MAX, &slot_no,
									mtr);
	ut_a(slot_no == TRX_SYS_SYSTEM_RSEG_ID);
	ut_a(page_no != FIL_NULL);

	mutex_exit(&kernel_mutex);
}

/*********************************************************************
Creates and initializes the central memory structures for the transaction
system. This is called when the database is started. */

void
trx_sys_init_at_db_start(void)
/*==========================*/
{
	trx_sysf_t*	sys_header;
	mtr_t		mtr;

	mtr_start(&mtr);
	
	ut_ad(trx_sys == NULL);

	mutex_enter(&kernel_mutex);

	trx_sys = mem_alloc(sizeof(trx_sys_t));
	
	sys_header = trx_sysf_get(&mtr);

	trx_rseg_list_and_array_init(sys_header, &mtr);
	
	trx_sys->latest_rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);

	/* VERY important: after the database is started, max_trx_id value is
	divisible by TRX_SYS_TRX_ID_WRITE_MARGIN, and the 'if' in
	trx_sys_get_new_trx_id will evaluate to TRUE when the function
	is first time called, and the value for trx id will be written
	to the disk-based header! Thus trx id values will not overlap when
	the database is repeatedly started! */

	trx_sys->max_trx_id = ut_dulint_add(
			      	ut_dulint_align_up(
					mtr_read_dulint(sys_header
						+ TRX_SYS_TRX_ID_STORE,
						MLOG_8BYTES, &mtr),
					TRX_SYS_TRX_ID_WRITE_MARGIN),
				2 * TRX_SYS_TRX_ID_WRITE_MARGIN);

	UT_LIST_INIT(trx_sys->mysql_trx_list);				
	trx_lists_init_at_db_start();

	if (UT_LIST_GET_LEN(trx_sys->trx_list) > 0) {
		fprintf(stderr,
	"InnoDB: %lu uncommitted transaction(s) which must be rolled back\n",
				UT_LIST_GET_LEN(trx_sys->trx_list));

		fprintf(stderr, "InnoDB: Trx id counter is %lu %lu\n", 
			ut_dulint_get_high(trx_sys->max_trx_id),
			ut_dulint_get_low(trx_sys->max_trx_id));
	}

	UT_LIST_INIT(trx_sys->view_list);

	trx_purge_sys_create();

	mutex_exit(&kernel_mutex);

	mtr_commit(&mtr);
}

/*********************************************************************
Creates and initializes the transaction system at the database creation. */

void
trx_sys_create(void)
/*================*/
{
	mtr_t	mtr;

	mtr_start(&mtr);

	trx_sysf_create(&mtr);

	mtr_commit(&mtr);

	trx_sys_init_at_db_start();
}
