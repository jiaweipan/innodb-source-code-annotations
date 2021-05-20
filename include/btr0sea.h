/************************************************************************
The index tree adaptive search

(c) 1996 Innobase Oy

Created 2/17/1996 Heikki Tuuri
*************************************************************************/
/*索引树自适应搜索*/
#ifndef btr0sea_h
#define btr0sea_h

#include "univ.i"

#include "rem0rec.h"
#include "dict0dict.h"
#include "btr0types.h"
#include "mtr0mtr.h"
#include "ha0ha.h"

/*********************************************************************
Creates and initializes the adaptive search system at a database start. */
/*在数据库启动时创建并初始化自适应搜索系统。*/
void
btr_search_sys_create(
/*==================*/
	ulint	hash_size);	/* in: hash index hash table size */
/************************************************************************
Returns search info for an index. */ /*返回索引的搜索信息。*/
UNIV_INLINE
btr_search_t*
btr_search_get_info(
/*================*/
				/* out: search info; search mutex reserved */
	dict_index_t*	index);	/* in: index */
/*********************************************************************
Creates and initializes a search info struct. */
/*创建并初始化一个搜索信息结构。*/
btr_search_t*
btr_search_info_create(
/*===================*/
				/* out, own: search info struct */
	mem_heap_t*	heap);	/* in: heap where created */
/*************************************************************************
Updates the search info. */ /*更新搜索信息。*/
UNIV_INLINE
void
btr_search_info_update(
/*===================*/
	dict_index_t*	index,	/* in: index of the cursor */
	btr_cur_t*	cursor);/* in: cursor which was just positioned */
/**********************************************************************
Tries to guess the right search position based on the search pattern info
of the index. */
/*尝试根据索引的搜索模式信息猜测正确的搜索位置。*/
ibool
btr_search_guess_on_pattern(
/*========================*/
					/* out: TRUE if succeeded */	
	dict_index_t*	index,		/* in: index */
	btr_search_t*	info,		/* in: index search info */
	dtuple_t*	tuple,		/* in: logical record */
	ulint		mode,		/* in: PAGE_CUR_L, ... */
	ulint		latch_mode, 	/* in: BTR_SEARCH_LEAF, ... */
	btr_cur_t*	cursor, 	/* out: tree cursor */
	mtr_t*		mtr);		/* in: mtr */
/**********************************************************************
Tries to guess the right search position based on the hash search info
of the index. Note that if mode is PAGE_CUR_LE, which is used in inserts,
and the function returns TRUE, then cursor->up_match and cursor->low_match
both have sensible values. */
/*尝试根据索引的哈希搜索信息猜测正确的搜索位置。注意，如果mode是PAGE_CUR_LE(在插入中使用)，
并且函数返回TRUE，那么cursor->up_match和cursor->low_match都有合理的值。*/
ibool
btr_search_guess_on_hash(
/*=====================*/
					/* out: TRUE if succeeded */	
	dict_index_t*	index,		/* in: index */
	btr_search_t*	info,		/* in: index search info */
	dtuple_t*	tuple,		/* in: logical record */
	ulint		mode,		/* in: PAGE_CUR_L, ... */
	ulint		latch_mode, 	/* in: BTR_SEARCH_LEAF, ... */
	btr_cur_t*	cursor, 	/* out: tree cursor */
	ulint		has_search_latch,/* in: latch mode the caller
					currently has on btr_search_latch:
					RW_S_LATCH, RW_X_LATCH, or 0 */
	mtr_t*		mtr);		/* in: mtr */
/************************************************************************
Moves or deletes hash entries for moved records. If new_page is already hashed,
then the hash index for page, if any, is dropped. If new_page is not hashed,
and page is hashed, then a new hash index is built to new_page with the same
parameters as page (this often happens when a page is split). */
/*移动或删除移动记录的散列项。如果new_page已经散列，则删除page的散列索引(如果有的话)。
如果new_page没有进行散列，而page进行了散列，则会为new_page构建一个新的散列索引，其参数与page相同(这通常发生在分页时)。*/
void
btr_search_move_or_delete_hash_entries(
/*===================================*/
	page_t*	new_page,	/* in: records are copied to this page */
	page_t*	page);		/* in: index page */
/************************************************************************
Drops a page hash index. */
/*删除页散列索引。*/
void
btr_search_drop_page_hash_index(
/*============================*/
	page_t*	page);	/* in: index page, s- or x-latched */
/************************************************************************
Drops a page hash index when a page is freed from a fseg to the file system.
Drops possible hash index if the page happens to be in the buffer pool. */
/*当页从fseg释放到文件系统时删除页哈希索引。如果页面恰好在缓冲池中，则删除可能的哈希索引。*/
void
btr_search_drop_page_hash_when_freed(
/*=================================*/
	ulint	space,		/* in: space id */
	ulint	page_no);	/* in: page number */
/************************************************************************
Updates the page hash index when a single record is inserted on a page. */
/*在页面上插入单个记录时更新页面散列索引。*/
void
btr_search_update_hash_node_on_insert(
/*==================================*/
	btr_cur_t*	cursor);/* in: cursor which was positioned to the
				place to insert using btr_cur_search_...,
				and the new record has been inserted next
				to the cursor */
/************************************************************************
Updates the page hash index when a single record is inserted on a page. */
/*在页面上插入单个记录时更新页面散列索引。*/
void
btr_search_update_hash_on_insert(
/*=============================*/
	btr_cur_t*	cursor);/* in: cursor which was positioned to the
				place to insert using btr_cur_search_...,
				and the new record has been inserted next
				to the cursor */
/************************************************************************
Updates the page hash index when a single record is deleted from a page. */
/*当从页面中删除单个记录时，更新页面散列索引。*/
void
btr_search_update_hash_on_delete(
/*=============================*/
	btr_cur_t*	cursor);/* in: cursor which was positioned on the
				record to delete using btr_cur_search_...,
				the record is not yet deleted */
/************************************************************************
Prints info of the search system. */
/*打印搜索系统的信息。*/
void
btr_search_print_info(void);
/*=======================*/
/************************************************************************
Prints info of searches on an index. */
/*打印索引上的搜索信息。*/
void
btr_search_index_print_info(
/*========================*/
	dict_index_t*	index);	/* in: index */
/************************************************************************
Prints info of searches on a table. */
/*打印表上的搜索信息。*/
void
btr_search_table_print_info(
/*========================*/
	char*	name);	/* in: table name */
/************************************************************************
Validates the search system. */
/*验证搜索系统。*/
ibool
btr_search_validate(void);
/*=====================*/


/* Search info directions */ /*搜索信息的方向*/
#define BTR_SEA_NO_DIRECTION	1
#define BTR_SEA_LEFT		2
#define BTR_SEA_RIGHT		3
#define BTR_SEA_SAME_REC	4

/* The search info struct in an index */
/* 索引中的搜索信息结构*/
struct btr_search_struct{
	/* The following 4 fields are currently not used: */
	rec_t*	last_search;	/* pointer to the lower limit record of the
				previous search; NULL if not known */
	ulint	n_direction;	/* number of consecutive searches in the
				same direction */
	ulint	direction;	/* BTR_SEA_NO_DIRECTION, BTR_SEA_LEFT,
				BTR_SEA_RIGHT, BTR_SEA_SAME_REC,
				or BTR_SEA_SAME_PAGE */
	dulint	modify_clock;	/* value of modify clock at the time
				last_search was stored */
	/*----------------------*/
	/* The following 4 fields are not protected by any latch: */
	page_t*	root_guess;	/* the root page frame when it was last time
				fetched, or NULL */ /*根页面帧的最后一次获取，或NULL*/
	ulint	hash_analysis;	/* when this exceeds a certain value, the
				hash analysis starts; this is reset if no
				success noticed */ /*当这个值超过某个值时，开始散列分析;如果没有注意到成功，这将被重置*/
	ibool	last_hash_succ;	/* TRUE if the last search would have
				succeeded, or did succeed, using the hash
				index; NOTE that the value here is not exact:
				it is not calculated for every search, and the
				calculation itself is not always accurate! */ /*如果使用哈希索引，最后一次搜索已经成功或确实成功，则为TRUE;
				注意，这里的值是不精确的:它不是为每一个搜索计算，计算本身并不总是准确的!*/
	ulint	n_hash_potential;/* number of consecutive searches which would
				have succeeded, or did succeed, using the hash
				index */ /*使用散列索引可能成功或确实成功的连续搜索的数量*/
	/*----------------------*/			
	ulint	n_fields;	/* recommended prefix length for hash search:
				number of full fields */ /*哈希搜索推荐的前缀长度:完整字段的数量*/
	ulint	n_bytes;	/* recommended prefix: number of bytes in
				an incomplete field */ /*推荐前缀:不完整字段的字节数*/
	ulint	side;		/* BTR_SEARCH_LEFT_SIDE or
				BTR_SEARCH_RIGHT_SIDE, depending on whether
				the leftmost record of several records with
				the same prefix should be indexed in the
				hash index */ /*BTR_SEARCH_LEFT_SIDE或BTR_SEARCH_RIGHT_SIDE，取决于是否应该在哈希索引中索引几个具有相同前缀的记录的最左边的记录*/
	/*----------------------*/
	ulint	n_hash_succ;	/* number of successful hash searches thus
				far */
	ulint	n_hash_fail;	/* number of failed hash searches */
	ulint	n_patt_succ;	/* number of successful pattern searches thus
				far */
	ulint	n_searches;	/* number of searches */
};

/* The hash index system */
/*哈希索引系统*/
typedef struct btr_search_sys_struct	btr_search_sys_t;

struct btr_search_sys_struct{
	hash_table_t*	hash_index;
};

extern btr_search_sys_t*	btr_search_sys;

/* The latch protecting the adaptive search system: this latch protects the
(1) positions of records on those pages where a hash index has been built.
NOTE: It does not protect values of non-ordering fields within a record from
being updated in-place! We can use fact (1) to perform unique searches to
indexes. */
/*保护自适应搜索系统的锁存器:这个锁存器保护(1)那些已经建立了哈希索引的页面上的记录位置。
注意:它不保护记录中非排序字段的值不被就地更新!我们可以使用事实(1)对索引执行惟一搜索。*/
extern rw_lock_t*	btr_search_latch_temp;

#define btr_search_latch	(*btr_search_latch_temp)

extern ulint	btr_search_n_succ;
extern ulint	btr_search_n_hash_fail;

/* After change in n_fields or n_bytes in info, this many rounds are waited
before starting the hash analysis again: this is to save CPU time when there
is no hope in building a hash index. */
/*在改变了info中的n_fields或n_bytes后，在再次开始哈希分析之前会等待很多轮:这是为了在没有希望建立哈希索引时节省CPU时间。*/
#define BTR_SEARCH_HASH_ANALYSIS	17

#define BTR_SEARCH_LEFT_SIDE	1
#define BTR_SEARCH_RIGHT_SIDE	2

/* Limit of consecutive searches for trying a search shortcut on the search
pattern */
/*在搜索模式上尝试搜索快捷方式的连续搜索限制*/
#define BTR_SEARCH_ON_PATTERN_LIMIT	3

/* Limit of consecutive searches for trying a search shortcut using the hash
index */
/*使用散列索引尝试搜索快捷方式的连续搜索限制*/
#define BTR_SEARCH_ON_HASH_LIMIT	3

/* We do this many searches before trying to keep the search latch over calls
from MySQL. If we notice someone waiting for the latch, we again set this
much timeout. This is to reduce contention. */
/*在尝试将搜索锁存到MySQL调用之前，我们做了这么多的搜索。如果我们注意到有人在等待门闩，我们再次设置这个超时时间。这是为了减少争用。*/
#define BTR_SEA_TIMEOUT			10000

#ifndef UNIV_NONINL
#include "btr0sea.ic"
#endif

#endif 
