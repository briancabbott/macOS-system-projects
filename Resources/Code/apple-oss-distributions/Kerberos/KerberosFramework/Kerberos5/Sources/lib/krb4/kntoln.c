/*
 * kntoln.c
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#include "mit-copyright.h"
#include "krb.h"
#include <string.h>

/*
 * krb_kntoln converts an auth name into a local name by looking up
 * the auth name in the /etc/aname file.  The format of the aname
 * file is:
 *
 * +-----+-----+-----+-----+------+----------+-------+-------+
 * | anl | inl | rll | lnl | name | instance | realm | lname |
 * +-----+-----+-----+-----+------+----------+-------+-------+
 * | 1by | 1by | 1by | 1by | name | instance | realm | lname |
 * +-----+-----+-----+-----+------+----------+-------+-------+
 *
 * If the /etc/aname file can not be opened it will set the
 * local name to the auth name.  Thus, in this case it performs as
 * the identity function.
 *
 * The name instance and realm are passed to krb_kntoln through
 * the AUTH_DAT structure (ad).
 *
 * Now here's what it *really* does:
 *
 * Given a Kerberos name in an AUTH_DAT structure, check that the
 * instance is null, and that the realm is the same as the local
 * realm, and return the principal's name in "lname".  Return
 * KSUCCESS if all goes well, otherwise KFAILURE.
 */

/* The definition of MAX_USERNAME here MUST agree with kuserok.c, or bad
 * things will happen. */
#define MAX_USERNAME 10

int
krb_kntoln(ad,lname)
    AUTH_DAT *ad;
    char *lname;
{
    static char lrealm[REALM_SZ];

    if (!(*lrealm) && (krb_get_lrealm(lrealm,1) == KFAILURE))
        return(KFAILURE);

    if (strcmp(ad->pinst,""))
        return(KFAILURE);
    if (strcmp(ad->prealm,lrealm))
        return(KFAILURE);
    (void) strncpy(lname,ad->pname,MAX_USERNAME-1);
    lname[MAX_USERNAME - 1] = '\0';
    return(KSUCCESS);
}
