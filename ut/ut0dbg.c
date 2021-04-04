/*********************************************************************
Debug utilities for Innobase.

(c) 1994, 1995 Innobase Oy

Created 1/30/1994 Heikki Tuuri
**********************************************************************/
/*debug 工具集*/
#include "univ.i"

/* This is used to eliminate compiler warnings */
/* 这是用来消除编译器警告*/
ulint	ut_dbg_zero	= 0;

/* If this is set to TRUE all threads will stop into the next assertion
and assert */ 
/* 如果设置为TRUE，所有线程将停止到下一个断言和断言 */
ibool	ut_dbg_stop_threads	= FALSE;

/* Null pointer used to generate memory trap */
/* 用于产生内存陷阱的空指针 */
ulint*	ut_dbg_null_ptr		= NULL;

/* Dummy function to prevent gcc from ignoring this file */
/* 防止gcc忽略这个文件的虚拟函数*/
void
ut_dummy(void)
{
  printf("Hello world\n");
}
