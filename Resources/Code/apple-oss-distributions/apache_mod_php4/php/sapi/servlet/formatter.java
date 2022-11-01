/*
   +----------------------------------------------------------------------+
   | PHP Version 4                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2006 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Sam Ruby (rubys@us.ibm.com)                                  |
   +----------------------------------------------------------------------+
*/

/* $Id: formatter.java,v 1.5.20.1 2006/01/01 13:47:02 sniper Exp $ */

package net.php;

import javax.servlet.*;
import javax.servlet.http.*;

public class formatter extends servlet {
  public void service(HttpServletRequest request, HttpServletResponse response) throws ServletException {
    display_source_mode = true;
    super.service(request, response);
  }
}
