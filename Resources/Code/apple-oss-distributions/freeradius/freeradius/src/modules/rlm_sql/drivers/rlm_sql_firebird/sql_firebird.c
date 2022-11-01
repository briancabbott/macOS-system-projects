/*
 * sql_firebird.c Part of Firebird rlm_sql driver
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Copyright 2006  The FreeRADIUS server project
 * Copyright 2006  Vitaly Bodzhgua <vitaly@eastera.net>
 */

#include <freeradius-devel/ident.h>
RCSID("$Id$")

#include "sql_fbapi.h"


/* Forward declarations */
static const char *sql_error(SQLSOCK *sqlsocket, SQL_CONFIG *config);
static int sql_free_result(SQLSOCK *sqlsocket, SQL_CONFIG *config);
static int sql_affected_rows(SQLSOCK *sqlsocket, SQL_CONFIG *config);
static int sql_num_fields(SQLSOCK *sqlsocket, SQL_CONFIG *config);
static int sql_finish_query(SQLSOCK *sqlsocket, SQL_CONFIG *config);

/*************************************************************************
 *
 *	Function: sql_init_socket
 *
 *	Purpose: Establish connection to the db
 *
 *************************************************************************/
static int sql_init_socket(SQLSOCK *sqlsocket, SQL_CONFIG *config) {
    rlm_sql_firebird_sock *firebird_sock;
    long res;


    if (!sqlsocket->conn) {
	sqlsocket->conn = (rlm_sql_firebird_sock *)rad_malloc(sizeof(rlm_sql_firebird_sock));
	if (!sqlsocket->conn) return -1;
    }

    firebird_sock = sqlsocket->conn;

    res=fb_init_socket(firebird_sock);
    if (res)  return -1;

    if (fb_connect(firebird_sock,config)) {
     radlog(L_ERR, "rlm_sql_firebird: Connection failed %s\n", firebird_sock->lasterror);
     return SQL_DOWN;
    }

    return 0;
}


/*************************************************************************
 *
 *      Function: sql_destroy_socket
 *
 *      Purpose: Free socket and private connection data
 *
 *************************************************************************/
static int sql_destroy_socket(SQLSOCK *sqlsocket, SQL_CONFIG *config)
{
    free(sqlsocket->conn);
    sqlsocket->conn = NULL;
    return 0;
}


/*************************************************************************
 *
 *	Function: sql_query
 *
 *	Purpose: Issue a non-SELECT query (ie: update/delete/insert) to
 *               the database.
 *
 *************************************************************************/

static int sql_query(SQLSOCK *sqlsocket, SQL_CONFIG *config, char *querystr) {
    rlm_sql_firebird_sock *firebird_sock = sqlsocket->conn;
    int deadlock=0;
 if (config->sqltrace)
        radlog(L_DBG, "sock_id %i: query:  %s", sqlsocket->id,querystr);

#ifdef _PTHREAD_H
 pthread_mutex_lock(&firebird_sock->mut);
#endif

TryAgain:
 if (fb_sql_query(firebird_sock,querystr)) {
//Try again query when deadlock, beacuse in any case it will be retried.
// but may be lost for short sessions
   if ((firebird_sock->sql_code==DEADLOCK_SQL_CODE) && !deadlock) {
      radlog(L_DBG,"sock_id %i: deadlock. Retry query %s\n",sqlsocket->id,querystr);
//For non READ_COMMITED transactions put rollback here
// fb_rollback(sock);
      deadlock=1;
      goto TryAgain;
   }
   radlog(L_ERR, "sock_id %i: rlm_sql_firebird,sql_query error:sql_code=%li, error='%s', query=%s\n",
     sqlsocket->id,
     firebird_sock->sql_code,
     firebird_sock->lasterror,
     querystr);

   if ((firebird_sock->sql_code==DOWN_SQL_CODE)) return SQL_DOWN;
//free problem query
   if (fb_rollback(firebird_sock)) {
    //assume the network is down if rollback had failed
    radlog(L_ERR,"Fail to rollback transaction after previous error. Error: %s\n",
       firebird_sock->lasterror);
    return SQL_DOWN;
   }
//   firebird_sock->in_use=0;
   return -1;
 }

 if (firebird_sock->statement_type!=isc_info_sql_stmt_select) {
    if (fb_commit(firebird_sock)) return -1;
 }

 return 0;
}


/*************************************************************************
 *
 *	Function: sql_select_query
 *
 *	Purpose: Issue a select query to the database
 *
 *************************************************************************/
static int sql_select_query(SQLSOCK *sqlsocket, SQL_CONFIG *config, char *querystr) {
//    rlm_sql_firebird_sock *firebird_sock = sqlsocket->conn;
    return (sql_query(sqlsocket, config, querystr));

}


/*************************************************************************
 *
 *	Function: sql_store_result
 *
 *	Purpose: database specific store_result function. Returns a result
 *               set for the query.
 *
 *************************************************************************/
static int sql_store_result(SQLSOCK *sqlsocket, SQL_CONFIG *config) {
  /*   Not used   */
    return 0;
}


/*************************************************************************
 *
 *	Function: sql_num_fields
 *
 *	Purpose: database specific num_fields function. Returns number
 *               of columns from query
 *
 *************************************************************************/
static int sql_num_fields(SQLSOCK *sqlsocket, SQL_CONFIG *config) {
    return  ((rlm_sql_firebird_sock *) sqlsocket->conn)->sqlda_out->sqld;
}


/*************************************************************************
 *
 *	Function: sql_num_rows
 *
 *	Purpose: database specific num_rows. Returns number of rows in
 *               query
 *
 *************************************************************************/
static int sql_num_rows(SQLSOCK *sqlsocket, SQL_CONFIG *config) {
    int res=sql_affected_rows(sqlsocket, config);
    radlog(L_DBG,"sock_id %i: sql_num_rows: %i\n",sqlsocket->id,res);
    return res;
}


/*************************************************************************
 *
 *	Function: sql_fetch_row
 *
 *	Purpose: database specific fetch_row. Returns a SQL_ROW struct
 *               with all the data for the query in 'sqlsocket->row'. Returns
 *		 0 on success, -1 on failure, SQL_DOWN if 'database is down'.
 *
 *************************************************************************/
static int sql_fetch_row(SQLSOCK *sqlsocket, SQL_CONFIG *config) {
    rlm_sql_firebird_sock *firebird_sock = sqlsocket->conn;
    int res;

    sqlsocket->row = NULL;
    if (firebird_sock->statement_type!=isc_info_sql_stmt_exec_procedure) {
     res=fb_fetch(firebird_sock);
     if (res==100) return 0;
     if (res) {
       radlog(L_ERR, "rlm_sql_firebird. Fetch problem:'%s'\n", firebird_sock->lasterror);
       return -1;
     }
    } else firebird_sock->statement_type=0;
    fb_store_row(firebird_sock);

    sqlsocket->row = firebird_sock->row;
    return 0;
}


/*************************************************************************
 *
 *	Function: sql_finish_select_query
 *
 *	Purpose: End the select query, such as freeing memory or result
 *
 *************************************************************************/
static int sql_finish_select_query(SQLSOCK * sqlsocket, SQL_CONFIG *config) {
    rlm_sql_firebird_sock *sock=(rlm_sql_firebird_sock *) sqlsocket->conn;
    fb_commit(sock);
    fb_close_cursor(sock);
    return 0;
}

/*************************************************************************
 *
 *	Function: sql_finish_query
 *
 *	Purpose: End the query, such as freeing memory
 *
 *************************************************************************/
static int sql_finish_query(SQLSOCK *sqlsocket, SQL_CONFIG *config) {
//    sql_free_result(sqlsocket,config);
    return 0;
}

/*************************************************************************
 *
 *	Function: sql_free_result
 *
 *	Purpose: database specific free_result. Frees memory allocated
 *               for a result set
 *
 *************************************************************************/
static int sql_free_result(SQLSOCK *sqlsocket, SQL_CONFIG *config) {
    return 0;
}

/*************************************************************************
 *
 *	Function: sql_close
 *
 *	Purpose: database specific close. Closes an open database
 *               connection and cleans up any open handles.
 *
 *************************************************************************/
static int sql_close(SQLSOCK *sqlsocket, SQL_CONFIG *config) {
//    radlog(L_INFO,"Closing sql\n");
    fb_destroy_socket((rlm_sql_firebird_sock *) sqlsocket->conn);
    return 0;
}

/*************************************************************************
 *
 *	Function: sql_error
 *
 *	Purpose: database specific error. Returns error associated with
 *               connection
 *
 *************************************************************************/
static const char *sql_error(SQLSOCK *sqlsocket, SQL_CONFIG *config) {
    rlm_sql_firebird_sock *firebird_sock = sqlsocket->conn;
    return firebird_sock->lasterror;
}
/*************************************************************************
 *
 *	Function: sql_affected_rows
 *
 *	Purpose: Return the number of rows affected by the query (update,
 *               or insert)
 *
 *************************************************************************/
static int sql_affected_rows(SQLSOCK *sqlsocket, SQL_CONFIG *config) {
 int affected_rows=fb_affected_rows(sqlsocket->conn);
 if (affected_rows<0)
   radlog(L_ERR, "sql_affected_rows, rlm_sql_firebird. error:%s\n", sql_error(sqlsocket,config));
 radlog(L_DBG,"sock_id %i: affected_rows: %i\n",sqlsocket->id,affected_rows);
 return affected_rows;
}

/* Exported to rlm_sql */
rlm_sql_module_t rlm_sql_firebird = {
	"rlm_sql_firebird",
	sql_init_socket,
	sql_destroy_socket,
	sql_query,
	sql_select_query,
	sql_store_result,
	sql_num_fields,
	sql_num_rows,
	sql_fetch_row,
	sql_free_result,
	sql_error,
	sql_close,
	sql_finish_query,
	sql_finish_select_query,
	sql_affected_rows
};
