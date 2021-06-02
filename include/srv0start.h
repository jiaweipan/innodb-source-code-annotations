/******************************************************
Starts the Innobase database server 启动Innobase数据库服务器

(c) 1995-2000 Innobase Oy

Created 10/10/1995 Heikki Tuuri
*******************************************************/


#ifndef srv0start_h
#define srv0start_h

#include "univ.i"

/********************************************************************
Starts Innobase and creates a new database if database files
are not found and the user wants. Server parameters are
read from a file of name "srv_init" in the ib_home directory. 
启动Innobase并创建一个新的数据库，如果没有找到数据库文件，而用户想要。从ib_home目录中名为“srv_init”的文件中读取服务器参数。*/
int
innobase_start_or_create_for_mysql(void);
/*====================================*/
				/* out: DB_SUCCESS or error code */
/********************************************************************
Shuts down the Innobase database. 关闭Innobase数据库。*/

int
innobase_shutdown_for_mysql(void);
/*=============================*/
				/* out: DB_SUCCESS or error code */

extern	ibool	srv_startup_is_before_trx_rollback_phase;
extern	ibool	srv_is_being_shut_down;

/* At a shutdown the value first climbs from 0 to SRV_SHUTDOWN_CLEANUP
and then to SRV_SHUTDOWN_LAST_PHASE 在关闭时，该值首先从0上升到SRV_SHUTDOWN_CLEANUP，然后上升到SRV_SHUTDOWN_LAST_PHASE*/

extern 	ulint	srv_shutdown_state;

#define SRV_SHUTDOWN_CLEANUP	1
#define SRV_SHUTDOWN_LAST_PHASE	2

#endif
