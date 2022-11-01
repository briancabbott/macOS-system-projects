/* 
   +----------------------------------------------------------------------+
   | PHP Version 4                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2008 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Andrei Zmievski <andrei@php.net>                             |
   +----------------------------------------------------------------------+
*/

/* $Id: aggregation.h,v 1.2.6.1.8.3 2007/12/31 07:22:51 sebastian Exp $ */

#ifndef AGGREGATION_H
#define AGGREGATION_H

#define AGGREGATE_ALL			0

#define AGGREGATE_METHODS		1
#define AGGREGATE_PROPERTIES	2

#define AGGREGATE_BY_LIST   	1
#define AGGREGATE_BY_REGEXP 	2

/*
 * Data structure that is stored in aggregation_table hashtable for each object.
 */
typedef struct {
	zend_class_entry *new_ce;
	zval *aggr_members;
} aggregation_info;

PHP_FUNCTION(aggregate);
PHP_FUNCTION(aggregate_methods);
PHP_FUNCTION(aggregate_methods_by_list);
PHP_FUNCTION(aggregate_methods_by_regexp);
PHP_FUNCTION(aggregate_properties);
PHP_FUNCTION(aggregate_properties_by_list);
PHP_FUNCTION(aggregate_properties_by_regexp);
PHP_FUNCTION(aggregate);
PHP_FUNCTION(deaggregate);
PHP_FUNCTION(aggregation_info);

#endif /* AGGREGATION_H */
