/*
 * pam.c	Functions to access the PAM library. This was taken
 *		from the hacks that miguel a.l. paraz <map@iphil.net>
 *		did on radiusd-cistron-1.5.3 and migrated to a
 *		separate file.
 *
 *		That, in fact, was again based on the original stuff
 *		from Jeph Blaize <jblaize@kiva.net> done in May 1997.
 *
 * Version:	$Id$
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
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Copyright 2000,2006  The FreeRADIUS server project
 * Copyright 1997  Jeph Blaize <jblaize@kiva.net>
 * Copyright 1999  miguel a.l. paraz <map@iphil.net>
 */

#include	<freeradius-devel/ident.h>
RCSID("$Id$")

#include	<freeradius-devel/radiusd.h>
#include	<freeradius-devel/modules.h>

#include	"config.h"

#ifdef HAVE_SECURITY_PAM_APPL_H
#include	<security/pam_appl.h>
#endif

#ifdef HAVE_PAM_PAM_APPL_H
#include	<pam/pam_appl.h>
#endif


#ifdef HAVE_SYSLOG_H
#include	<syslog.h>
#endif

typedef struct rlm_pam_t {
	const char *pam_auth_name;
} rlm_pam_t;

static const CONF_PARSER module_config[] = {
	{ "pam_auth",    PW_TYPE_STRING_PTR, offsetof(rlm_pam_t,pam_auth_name),
	  NULL, "radiusd" },
	{ NULL, -1, 0, NULL, NULL }
};

/*
 *	(Re-)read radiusd.conf into memory.
 */
static int pam_instantiate(CONF_SECTION *conf, void **instance)
{
	rlm_pam_t *data;

	data = rad_malloc(sizeof(*data));
	if (!data) {
		return -1;
	}
	memset(data, 0, sizeof(*data));

	if (cf_section_parse(conf, data, module_config) < 0) {
		free(data);
		return -1;
	}

	*instance = data;
	return 0;
}

/*
 *	Clean up.
 */
static int pam_detach(void *instance)
{
	rlm_pam_t *data = (rlm_pam_t *) instance;

        free((char *) data);
	return 0;
}

/*************************************************************************
 *
 *	Function: PAM_conv
 *
 *	Purpose: Dialogue between RADIUS and PAM modules.
 *
 * jab - stolen from pop3d
 *
 * Alan DeKok: modified to use PAM's appdata_ptr, so that we're
 *             multi-threaded safe, and don't have any nasty static
 *             variables hanging around.
 *
 *************************************************************************/

typedef struct my_PAM {
  const char *username;
  const char *password;
  int         error;
} my_PAM;

static int PAM_conv (int num_msg,
                     const struct pam_message **msg,
                     struct pam_response **resp,
                     void *appdata_ptr) {
  int count;
  struct pam_response *reply;
  my_PAM *pam_config = (my_PAM *) appdata_ptr;

/* strdup(NULL) doesn't work on some platforms */
#define COPY_STRING(s) ((s) ? strdup(s) : NULL)

  reply = rad_malloc(num_msg * sizeof(struct pam_response));
  memset(reply, 0, num_msg * sizeof(struct pam_response));
  for (count = 0; count < num_msg; count++) {
    switch (msg[count]->msg_style) {
    case PAM_PROMPT_ECHO_ON:
      reply[count].resp_retcode = PAM_SUCCESS;
      reply[count].resp = COPY_STRING(pam_config->username);
      break;
    case PAM_PROMPT_ECHO_OFF:
      reply[count].resp_retcode = PAM_SUCCESS;
      reply[count].resp = COPY_STRING(pam_config->password);
      break;
    case PAM_TEXT_INFO:
      /* ignore it... */
      break;
    case PAM_ERROR_MSG:
    default:
      /* Must be an error of some sort... */
      for (count = 0; count < num_msg; count++) {
        if (reply[count].resp) {
          /* could be a password, let's be sanitary */
          memset(reply[count].resp, 0, strlen(reply[count].resp));
          free(reply[count].resp);
        }
      }
      free(reply);
      pam_config->error = 1;
      return PAM_CONV_ERR;
    }
  }
  *resp = reply;
  /* PAM frees reply (including reply[].resp) */

  return PAM_SUCCESS;
}

/*************************************************************************
 *
 *	Function: pam_pass
 *
 *	Purpose: Check the users password against the standard UNIX
 *		 password table + PAM.
 *
 * jab start 19970529
 *************************************************************************/

/* cjd 19980706
 *
 * for most flexibility, passing a pamauth type to this function
 * allows you to have multiple authentication types (i.e. multiple
 * files associated with radius in /etc/pam.d)
 */
static int pam_pass(const char *name, const char *passwd, const char *pamauth)
{
    pam_handle_t *pamh=NULL;
    int retval;
    my_PAM pam_config;
    struct pam_conv conv;

    /*
     *  Initialize the structures.
     */
    conv.conv = PAM_conv;
    conv.appdata_ptr = &pam_config;
    pam_config.username = name;
    pam_config.password = passwd;
    pam_config.error = 0;

    DEBUG("pam_pass: using pamauth string <%s> for pam.conf lookup", pamauth);
    retval = pam_start(pamauth, name, &conv, &pamh);
    if (retval != PAM_SUCCESS) {
      DEBUG("pam_pass: function pam_start FAILED for <%s>. Reason: %s",
	    name, pam_strerror(pamh, retval));
      return -1;
    }

    retval = pam_authenticate(pamh, 0);
    if (retval != PAM_SUCCESS) {
      DEBUG("pam_pass: function pam_authenticate FAILED for <%s>. Reason: %s",
	    name, pam_strerror(pamh, retval));
      pam_end(pamh, retval);
      return -1;
    }

    /*
     * FreeBSD 3.x doesn't have account and session management
     * functions in PAM, while 4.0 does.
     */
#if !defined(__FreeBSD_version) || (__FreeBSD_version >= 400000)
    retval = pam_acct_mgmt(pamh, 0);
    if (retval != PAM_SUCCESS) {
      DEBUG("pam_pass: function pam_acct_mgmt FAILED for <%s>. Reason: %s",
	    name, pam_strerror(pamh, retval));
      pam_end(pamh, retval);
      return -1;
    }
#endif

    DEBUG("pam_pass: authentication succeeded for <%s>", name);
    pam_end(pamh, retval);
    return 0;
}

/* translate between function declarations */
static int pam_auth(void *instance, REQUEST *request)
{
	int	r;
	VALUE_PAIR *pair;
	rlm_pam_t *data = (rlm_pam_t *) instance;

	const char *pam_auth_string = data->pam_auth_name;

	/*
	 *	We can only authenticate user requests which HAVE
	 *	a User-Name attribute.
	 */
	if (!request->username) {
		radlog(L_AUTH, "rlm_pam: Attribute \"User-Name\" is required for authentication.");
		return RLM_MODULE_INVALID;
	}

	/*
	 *	We can only authenticate user requests which HAVE
	 *	a User-Password attribute.
	 */
	if (!request->password) {
		radlog(L_AUTH, "rlm_pam: Attribute \"User-Password\" is required for authentication.");
		return RLM_MODULE_INVALID;
	}

	/*
	 *  Ensure that we're being passed a plain-text password,
	 *  and not anything else.
	 */
	if (request->password->attribute != PW_USER_PASSWORD) {
		radlog(L_AUTH, "rlm_pam: Attribute \"User-Password\" is required for authentication.  Cannot use \"%s\".", request->password->name);
		return RLM_MODULE_INVALID;
	}

	/*
	 *	Let the 'users' file over-ride the PAM auth name string,
	 *	for backwards compatibility.
	 */
	pair = pairfind(request->config_items, PAM_AUTH_ATTR);
	if (pair) pam_auth_string = (char *)pair->vp_strvalue;

	r = pam_pass((char *)request->username->vp_strvalue,
		     (char *)request->password->vp_strvalue,
		     pam_auth_string);

	if (r == 0) {
		return RLM_MODULE_OK;
	}
	return RLM_MODULE_REJECT;
}

module_t rlm_pam = {
	RLM_MODULE_INIT,
	"pam",
	RLM_TYPE_THREAD_UNSAFE,	/* The PAM libraries are not thread-safe */
	pam_instantiate,		/* instantiation */
	pam_detach,			/* detach */
	{
		pam_auth,		/* authenticate */
		NULL,			/* authorize */
		NULL,			/* pre-accounting */
		NULL,			/* accounting */
		NULL,			/* checksimul */
		NULL,			/* pre-proxy */
		NULL,			/* post-proxy */
		NULL			/* post-auth */
	},
};

