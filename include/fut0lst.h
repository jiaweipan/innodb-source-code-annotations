/**********************************************************************
File-based list utilities

(c) 1995 Innobase Oy

Created 11/28/1995 Heikki Tuuri
***********************************************************************/
/*基于文件列表的公共代码*/
#ifndef fut0lst_h
#define fut0lst_h

#include "univ.i"

#include "fil0fil.h"
#include "mtr0mtr.h"


/* The C 'types' of base node and list node: these should be used to
write self-documenting code. Of course, the sizeof macro cannot be
applied to these types! */
/*基本节点和列表节点的C“类型”：这些应该用来编写自文档代码。当然，sizeof宏不能应用于这些类型！*/

typedef	byte	flst_base_node_t;
typedef	byte	flst_node_t;

/* The physical size of a list base node in bytes */
/* 列表基节点的物理大小（字节）*/
#define	FLST_BASE_NODE_SIZE	(4 + 2 * FIL_ADDR_SIZE)

/* The physical size of a list node in bytes */
/* 列表节点的物理大小（字节）*/
#define	FLST_NODE_SIZE		(2 * FIL_ADDR_SIZE)


/************************************************************************
Initializes a list base node. */
/*初始化列表基节点。*/
UNIV_INLINE
void
flst_init(
/*======*/
	flst_base_node_t*	base,	/* in: pointer to base node */
	mtr_t*			mtr);	/* in: mini-transaction handle */
/************************************************************************
Adds a node as the last node in a list. */
/*添加一个节点作为列表中的最后一个节点。*/
void
flst_add_last(
/*==========*/
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	flst_node_t*		node,	/* in: node to add */
	mtr_t*			mtr);	/* in: mini-transaction handle */
/************************************************************************
Adds a node as the first node in a list. */
/*添加一个节点作为列表中的第一个节点。 */
void
flst_add_first(
/*===========*/
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	flst_node_t*		node,	/* in: node to add */
	mtr_t*			mtr);	/* in: mini-transaction handle */
/************************************************************************
Inserts a node after another in a list. */
/*在列表中依次插入一个节点。*/
void
flst_insert_after(
/*==============*/
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	flst_node_t*		node1,	/* in: node to insert after */
	flst_node_t*		node2,	/* in: node to add */
	mtr_t*			mtr);	/* in: mini-transaction handle */
/************************************************************************
Inserts a node before another in a list. */
/*在列表中的另一个节点之前插入一个节点。*/
void
flst_insert_before(
/*===============*/
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	flst_node_t*		node2,	/* in: node to insert */
	flst_node_t*		node3,	/* in: node to insert before */
	mtr_t*			mtr);	/* in: mini-transaction handle */
/************************************************************************
Removes a node. */
/*删除一个结点*/
void
flst_remove(
/*========*/
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	flst_node_t*		node2,	/* in: node to remove */
	mtr_t*			mtr);	/* in: mini-transaction handle */
/************************************************************************
Cuts off the tail of the list, including the node given. The number of
nodes which will be removed must be provided by the caller, as this function
does not measure the length of the tail. */
/*切掉列表的尾部，包括给定的节点。将被移除的节点数必须由调用方提供，因为此函数不测量尾部的长度。*/
void
flst_cut_end(
/*=========*/
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	flst_node_t*		node2,	/* in: first node to remove */
	ulint			n_nodes,/* in: number of nodes to remove,
					must be >= 1 */
	mtr_t*			mtr);	/* in: mini-transaction handle */
/************************************************************************
Cuts off the tail of the list, not including the given node. The number of
nodes which will be removed must be provided by the caller, as this function
does not measure the length of the tail. */
/*切掉列表的尾部，不包括给定的节点。将被移除的节点数必须由调用方提供，因为此函数不测量尾部的长度。*/
void
flst_truncate_end(
/*==============*/
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	flst_node_t*		node2,	/* in: first node not to remove */
	ulint			n_nodes,/* in: number of nodes to remove */
	mtr_t*			mtr);	/* in: mini-transaction handle */
/************************************************************************
Gets list length. */
/*获取列表长度。 */
UNIV_INLINE
ulint
flst_get_len(
/*=========*/
					/* out: length */
	flst_base_node_t*	base,	/* in: pointer to base node */
	mtr_t*			mtr);	/* in: mini-transaction handle */
/************************************************************************
Gets list first node address. */
/*获取列表的第一个节点地址。*/
UNIV_INLINE
fil_addr_t
flst_get_first(
/*===========*/
					/* out: file address */
	flst_base_node_t*	base,	/* in: pointer to base node */
	mtr_t*			mtr);	/* in: mini-transaction handle */
/************************************************************************
Gets list last node address. */
/*获取列表的最后一个节点地址。*/
UNIV_INLINE
fil_addr_t
flst_get_last(
/*==========*/
					/* out: file address */
	flst_base_node_t*	base,	/* in: pointer to base node */
	mtr_t*			mtr);	/* in: mini-transaction handle */
/************************************************************************
Gets list next node address. */
/*获得列表下一个节点地址*/
UNIV_INLINE
fil_addr_t
flst_get_next_addr(
/*===============*/
				/* out: file address */
	flst_node_t*	node,	/* in: pointer to node */
	mtr_t*		mtr);	/* in: mini-transaction handle */
/************************************************************************
Gets list prev node address. */
/*获得列表前一个节点地址*/
UNIV_INLINE
fil_addr_t
flst_get_prev_addr(
/*===============*/
				/* out: file address */
	flst_node_t*	node,	/* in: pointer to node */
	mtr_t*		mtr);	/* in: mini-transaction handle */
/************************************************************************
Writes a file address. */
/*写入文件地址。*/
UNIV_INLINE
void
flst_write_addr(
/*============*/
	fil_faddr_t*	faddr,	/* in: pointer to file faddress */
	fil_addr_t	addr,	/* in: file address */
	mtr_t*		mtr);	/* in: mini-transaction handle */
/************************************************************************
Reads a file address. */
/*读取文件地址。*/
UNIV_INLINE
fil_addr_t
flst_read_addr(
/*===========*/
				/* out: file address */
	fil_faddr_t*	faddr,	/* in: pointer to file faddress */
	mtr_t*		mtr);	/* in: mini-transaction handle */
/************************************************************************
Validates a file-based list. */
/*验证基于文件的列表。*/
ibool
flst_validate(
/*==========*/
					/* out: TRUE if ok */
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	mtr_t*			mtr1);	/* in: mtr */
/************************************************************************
Prints info of a file-based list. */
/*打印基于文件的列表的信息。*/
void
flst_print(
/*=======*/
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	mtr_t*			mtr);	/* in: mtr */


#ifndef UNIV_NONINL
#include "fut0lst.ic"
#endif

#endif
