divert(-1)
#
# Copyright (c) 2000-2001 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Id: authinfo.m4,v 1.1.1.1 2002/03/12 17:59:46 zarzycki Exp $')
divert(-1)

define(`_AUTHINFO_TABLE_', `')

LOCAL_CONFIG
# authinfo list database: contains info for authentication as client
Kauthinfo ifelse(defn(`_ARG_'), `', DATABASE_MAP_TYPE MAIL_SETTINGS_DIR`authinfo',
		 defn(`_ARG_'), `LDAP', `ldap -1 -v sendmailMTAMapValue -k (&(objectClass=sendmailMTAMapObject)(|(sendmailMTACluster=${sendmailMTACluster})(sendmailMTAHost=$j))(sendmailMTAMapName=authinfo)(sendmailMTAKey=%0))',
		 `_ARG_')
