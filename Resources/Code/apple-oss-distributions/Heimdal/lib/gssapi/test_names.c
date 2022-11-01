/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <roken.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <gssapi.h>
#include <gssapi_krb5.h>
#include <gssapi_spnego.h>
#include <gssapi_ntlm.h>
#include <gssapi_spi.h>
#include <err.h>
#include <getarg.h>

static void
gss_err(int exitval, int status, const char *fmt, ...)
    __attribute__((format (printf, 3, 4)));


static void
gss_print_errors (int min_stat)
{
    OM_uint32 new_stat;
    OM_uint32 msg_ctx = 0;
    gss_buffer_desc status_string;
    OM_uint32 ret;

    do {
	ret = gss_display_status (&new_stat,
				  min_stat,
				  GSS_C_MECH_CODE,
				  GSS_C_NO_OID,
				  &msg_ctx,
				  &status_string);
	if (!GSS_ERROR(ret)) {
	    fprintf (stderr, "%.*s\n", (int)status_string.length,
					(char *)status_string.value);
	    gss_release_buffer (&new_stat, &status_string);
	}
    } while (!GSS_ERROR(ret) && msg_ctx != 0);
}

static void
gss_err(int exitval, int status, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vwarnx (fmt, args);
    gss_print_errors (status);
    va_end(args);
    exit (exitval);
}

static int verbose_flag = 0;
static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"verbose",	0,	arg_counter,	&verbose_flag, "verbose output", NULL },
    {"version",	0,	arg_flag,	&version_flag, "print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,  NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args, sizeof(args)/sizeof(*args),
		    NULL, "service@host");
    exit (ret);
}

static void
test_import_export(void)
{
    gss_buffer_desc name_buffer;
    OM_uint32 maj_stat, min_stat;
    gss_name_t name, MNname, MNname2;
    int len, equal;
    char *str;

    /*
     * test import/export
     */

    len = asprintf(&str, "ftp@freeze-arrow.mit.edu");
    if (len == -1)
	errx(1, "asprintf");

    name_buffer.value = str;
    name_buffer.length = len;

    maj_stat = gss_import_name(&min_stat, &name_buffer,
			       GSS_C_NT_HOSTBASED_SERVICE,
			       &name);
    if (maj_stat != GSS_S_COMPLETE)
	gss_err(1, min_stat, "import name error");
    free(str);

    maj_stat = gss_canonicalize_name (&min_stat,
				      name,
				      GSS_KRB5_MECHANISM,
				      &MNname);
    if (maj_stat != GSS_S_COMPLETE)
	gss_err(1, min_stat, "canonicalize name error");

    maj_stat = gss_export_name(&min_stat,
			       MNname,
			       &name_buffer);
    if (maj_stat != GSS_S_COMPLETE)
	gss_err(1, min_stat, "export name error (KRB5)");


    /*
     * test import/export
     */

    str = NULL;
    len = asprintf(&str, "ftp@freeze-arrow.mit.edu");
    if (len < 0 || str == NULL)
	errx(1, "asprintf");

    name_buffer.value = str;
    name_buffer.length = len;

    maj_stat = gss_import_name(&min_stat, &name_buffer,
			       GSS_C_NT_HOSTBASED_SERVICE,
			       &name);
    if (maj_stat != GSS_S_COMPLETE)
	gss_err(1, min_stat, "import name error");
    free(str);

    maj_stat = gss_canonicalize_name (&min_stat,
				      name,
				      GSS_KRB5_MECHANISM,
				      &MNname);
    if (maj_stat != GSS_S_COMPLETE)
	gss_err(1, min_stat, "canonicalize name error");

    maj_stat = gss_export_name(&min_stat,
			       MNname,
			       &name_buffer);
    if (maj_stat != GSS_S_COMPLETE)
	gss_err(1, min_stat, "export name error (KRB5)");

    /*
     * Import the exported name and compare
     */

    maj_stat = gss_import_name(&min_stat, &name_buffer,
			       GSS_C_NT_EXPORT_NAME,
			       &MNname2);
    if (maj_stat != GSS_S_COMPLETE)
	gss_err(1, min_stat, "import name error (exported KRB5 name)");


    maj_stat = gss_compare_name(&min_stat, MNname, MNname2, &equal);
    if (maj_stat != GSS_S_COMPLETE)
	errx(1, "gss_compare_name");
    if (!equal)
	errx(1, "names not equal");

    gss_release_name(&min_stat, &MNname2);
    gss_release_buffer(&min_stat, &name_buffer);
    gss_release_name(&min_stat, &MNname);
    gss_release_name(&min_stat, &name);

    /*
     * Import oid less name and compare to mech name.
     * Dovecot SASL lib does this.
     */

    str = NULL;
    len = asprintf(&str, "lha");
    if (len < 0 || str == NULL)
	errx(1, "asprintf");

    name_buffer.value = str;
    name_buffer.length = len;

    maj_stat = gss_import_name(&min_stat, &name_buffer,
			       GSS_C_NO_OID,
			       &name);
    if (maj_stat != GSS_S_COMPLETE)
	gss_err(1, min_stat, "import (no oid) name error");

    maj_stat = gss_import_name(&min_stat, &name_buffer,
			       GSS_KRB5_NT_USER_NAME,
			       &MNname);
    if (maj_stat != GSS_S_COMPLETE)
	gss_err(1, min_stat, "import (krb5 mn) name error");

    free(str);

    maj_stat = gss_compare_name(&min_stat, name, MNname, &equal);
    if (maj_stat != GSS_S_COMPLETE)
	errx(1, "gss_compare_name");
    if (!equal)
	errx(1, "names not equal");

    gss_release_name(&min_stat, &MNname);
    gss_release_name(&min_stat, &name);

#if 0
    maj_stat = gss_canonicalize_name (&min_stat,
				      name,
				      GSS_SPNEGO_MECHANISM,
				      &MNname);
    if (maj_stat != GSS_S_COMPLETE)
	gss_err(1, min_stat, "canonicalize name error");


    maj_stat = gss_export_name(&maj_stat,
			       MNname,
			       &name_buffer);
    if (maj_stat != GSS_S_COMPLETE)
	gss_err(1, min_stat, "export name error (SPNEGO)");

    gss_release_name(&min_stat, &MNname);
    gss_release_buffer(&min_stat, &name_buffer);
#endif
}

struct {
    gss_OID type;
    const char *name;
    OM_uint32 e;
} names[] = {
    { GSS_C_NT_USER_NAME, "DOMAIN\\user", GSS_S_COMPLETE },
    { GSS_C_NT_USER_NAME, "user", GSS_S_COMPLETE },
    { GSS_C_NT_USER_NAME, "DOMAIN@user", GSS_S_COMPLETE },
    { GSS_C_NT_NTLM, "user", GSS_S_FAILURE },
    { GSS_C_NT_NTLM, "DOMAIN\\user", GSS_S_COMPLETE },
    { GSS_C_NT_HOSTBASED_SERVICE, "service@host.domain", GSS_S_COMPLETE },
    { GSS_C_NT_HOSTBASED_SERVICE, "host.domain", GSS_S_BAD_NAME }
};


static void
test_names(void)
{
    gss_buffer_desc buffer;
    OM_uint32 maj_stat, min_stat;
    gss_name_t name;
    unsigned i;

    for (i = 0; i < sizeof(names)/sizeof(names[0]); i++) {
	buffer.value = rk_UNCONST(names[i].name);
	buffer.length = strlen(names[i].name);

	if (verbose_flag)
	    printf("running name test: %s\n", names[i].name);

	maj_stat = gss_import_name(&min_stat, &buffer, names[i].type, &name);
	if (maj_stat != names[i].e)
	    errx(1, "gss_import_name unexpected: %u:%s", i, names[i].name);

	gss_release_name(&min_stat, &name);
    }
}


int
main(int argc, char **argv)
{
    int optidx = 0;

    setprogname(argv[0]);
    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    gsskrb5_set_default_realm("MIT.EDU");

    test_names();

    test_import_export();

    return 0;
}
