/*
** Copyright (C) 1998-2000 Greg Stein. All Rights Reserved.
**
** By using this file, you agree to the terms and conditions set forth in
** the LICENSE.html file which can be found at the top level of the mod_dav
** distribution or at http://www.webdav.org/mod_dav/license-1.html.
**
** Contact information:
**   Greg Stein, PO Box 760, Palo Alto, CA, 94302
**   gstein@lyra.org, http://www.webdav.org/mod_dav/
*/

/*
** DAV filesystem lock implementation
**
** Written 06/99 by Keith Wannamaker, wannamak@us.ibm.com
**
** Modified 08/99 by John Vasta, vasta@rational.com. to extract
** repository-dependent code from dav_lock.c
*/

#include <sys/stat.h>

#include "httpd.h"
#include "http_log.h"

#include "mod_dav.h"
#include "dav_opaquelock.h"
#include "dav_fs_repos.h"


/*
** Provide some slack time before we actually mark a lock as expire. This
** is caused by a problem in Microsoft Office. They refresh locks *right*
** at the expiration time. If network latency or other delays cause us to
** not see the refresh in time, then we will expire the lock; Office will
** then completely get hosed because it cannot deal with a 412 (Failed
** Dependency) response code; the user is left with a document they cannot
** save back to the server.
**
** This slack time gives Office a bit of time to get that refresh back to us.
*/
#define DAV_TIMEOUT_SLACK       120     /* N seconds */


/* ---------------------------------------------------------------
**
** Lock database primitives
**
*/

/*
** LOCK DATABASES
** 
** Lockdiscovery information is stored in the single lock database specified
** by the DAVLockDB directive.  Information about this db is stored in the
** global server configuration.
**
** KEY
**
** The database is keyed by a key_type unsigned char (DAV_TYPE_INODE or
** DAV_TYPE_FNAME) followed by inode and device number if possible,
** otherwise full path (in the case of Win32 or lock-null resources).
**
** VALUE
**
** The value consists of a list of elements.
**    DIRECT LOCK:     [char  (DAV_LOCK_DIRECT),
**			char  (dav_lock_scope),
**			char  (dav_lock_type),
**			int    depth,
**			time_t expires,
**			dav_uuid_t locktoken,
**			char[] owner,
**                      char[] auth_user]
**
**    INDIRECT LOCK:   [char  (DAV_LOCK_INDIRECT),
**			dav_uuid_t locktoken,
**			time_t expires,
**			int    key_size,
**			char[] key]
**       The key is to the collection lock that resulted in this indirect lock
*/

#define DAV_TRUE		1
#define DAV_FALSE		0

#define DAV_CREATE_LIST		23
#define DAV_APPEND_LIST		24

/* Stored lock_discovery prefix */
#define DAV_LOCK_DIRECT		1
#define DAV_LOCK_INDIRECT	2

#define DAV_TYPE_INODE		10
#define DAV_TYPE_FNAME		11


/* ack. forward declare. */
static dav_error * dav_fs_remove_locknull_member(pool *p,
						 const char *filename,
						 dav_buffer *pbuf);

/*
** Use the opaquelock scheme for locktokens
*/
struct dav_locktoken {
    dav_uuid_t uuid;
};


/* #################################################################
** ### keep these structures (internal) or move fully to dav_lock?
*/

/*
** We need to reliably size the fixed-length portion of
** dav_lock_discovery; best to separate it into another 
** struct for a convenient sizeof, unless we pack lock_discovery.
*/
typedef struct dav_lock_discovery_fixed
{
    char scope;
    char type;
    int depth;
    time_t timeout;
} dav_lock_discovery_fixed;

typedef struct dav_lock_discovery
{
    struct dav_lock_discovery_fixed f;

    dav_locktoken *locktoken;
    const char *owner;		/* owner field from activelock */
    const char *auth_user;	/* authenticated user who created the lock */
    struct dav_lock_discovery *next;
} dav_lock_discovery;

/* Indirect locks represent locks inherited from containing collections.
 * They reference the lock token for the collection the lock is
 * inherited from. A lock provider may also define a key to the
 * inherited lock, for fast datbase lookup. The key is opaque outside
 * the lock provider.
 */
typedef struct dav_lock_indirect
{
    dav_locktoken *locktoken;
    dav_datum key;
    struct dav_lock_indirect *next;
    time_t timeout;
} dav_lock_indirect;

/* ################################################################# */


/*
** Stored direct lock info - full lock_discovery length:  
** prefix + Fixed length + lock token + 2 strings + 2 nulls (one for each string)
*/
#define dav_size_direct(a)	(1 + sizeof(dav_lock_discovery_fixed) \
				 + sizeof(uuid_t) \
				 + ((a)->owner ? strlen((a)->owner) : 0) \
				 + ((a)->auth_user ? strlen((a)->auth_user) : 0) \
				 + 2)

/* Stored indirect lock info - lock token and dav_datum */
#define dav_size_indirect(a)	(1 + sizeof(uuid_t) \
				 + sizeof(time_t) \
				 + sizeof(int) + (a)->key.dsize)

/*
** The lockdb structure.
**
** The <db> field may be NULL, meaning one of two things:
** 1) That we have not actually opened the underlying database (yet). The
**    <opened> field should be false.
** 2) We opened it readonly and it wasn't present.
**
** The delayed opening (determined by <opened>) makes creating a lockdb
** quick, while deferring the underlying I/O until it is actually required.
**
** We export the notion of a lockdb, but hide the details of it. Most
** implementations will use a database of some kind, but it is certainly
** possible that alternatives could be used.
*/
struct dav_lockdb_private
{
    request_rec *r;			/* for accessing the uuid state */
    pool *pool;				/* a pool to use */
    const char *lockdb_path;		/* where is the lock database? */

    int opened;				/* we opened the database */
    dav_db *db;				/* if non-NULL, the lock database */
};
typedef struct
{
    dav_lockdb pub;
    dav_lockdb_private priv;
} dav_lockdb_combined;

/*
** The private part of the lock structure.
*/
struct dav_lock_private
{
    dav_datum key;	/* key into the lock database */
};
typedef struct
{
    dav_lock pub;
    dav_lock_private priv;
    dav_locktoken token;
} dav_lock_combined;

/*
** This must be forward-declared so the open_lockdb function can use it.
*/
extern const dav_hooks_locks dav_hooks_locks_fs;


/* internal function for creating locks */
static dav_lock *dav_fs_alloc_lock(dav_lockdb *lockdb, dav_datum key,
				   const dav_locktoken *locktoken)
{
    dav_lock_combined *comb;

    comb = ap_pcalloc(lockdb->info->pool, sizeof(*comb));
    comb->pub.rectype = DAV_LOCKREC_DIRECT;
    comb->pub.info = &comb->priv;
    comb->priv.key = key;

    if (locktoken == NULL) {
	comb->pub.locktoken = &comb->token;
	dav_create_opaquelocktoken(dav_get_uuid_state(lockdb->info->r),
                                   &comb->token.uuid);
    }
    else {
	comb->pub.locktoken = locktoken;
    }

    return &comb->pub;
}

/*
** dav_fs_parse_locktoken
**
** Parse an opaquelocktoken URI into a locktoken.
*/
static dav_error * dav_fs_parse_locktoken(
    pool *p,
    const char *char_token,
    dav_locktoken **locktoken_p)
{
    dav_locktoken *locktoken;

    if (strstr(char_token, "opaquelocktoken:") != char_token) {
	return dav_new_error(p,
			     HTTP_BAD_REQUEST, DAV_ERR_LOCK_UNK_STATE_TOKEN,
			     "The lock token uses an unknown State-token "
			     "format and could not be parsed.");
    }
    char_token += 16;

    locktoken = ap_pcalloc(p, sizeof(*locktoken));
    if (dav_parse_opaquelocktoken(char_token, &locktoken->uuid)) {
	return dav_new_error(p, HTTP_BAD_REQUEST, DAV_ERR_LOCK_PARSE_TOKEN,
			     "The opaquelocktoken has an incorrect format "
			     "and could not be parsed.");
    }
    
    *locktoken_p = locktoken;
    return NULL;
}

/*
** dav_fs_format_locktoken
**
** Generate the URI for a locktoken
*/
static const char *dav_fs_format_locktoken(
    pool *p,
    const dav_locktoken *locktoken)
{
    const char *uuid_token = dav_format_opaquelocktoken(p, &locktoken->uuid);
    return ap_pstrcat(p, "opaquelocktoken:", uuid_token, NULL);
}

/*
** dav_fs_compare_locktoken
**
** Determine whether two locktokens are the same
*/
static int dav_fs_compare_locktoken(
    const dav_locktoken *lt1,
    const dav_locktoken *lt2)
{
    return dav_compare_opaquelocktoken(lt1->uuid, lt2->uuid);
}

/*
** dav_fs_really_open_lockdb:
**
** If the database hasn't been opened yet, then open the thing.
*/
static dav_error * dav_fs_really_open_lockdb(dav_lockdb *lockdb)
{
    dav_error *err;

    if (lockdb->info->opened)
	return NULL;

    err = dav_dbm_open_direct(lockdb->info->pool,
			      lockdb->info->lockdb_path,
			      lockdb->ro,
			      &lockdb->info->db);
    if (err != NULL) {
	return dav_push_error(lockdb->info->pool,
			      HTTP_INTERNAL_SERVER_ERROR,
			      DAV_ERR_LOCK_OPENDB,
			      "Could not open the lock database.",
			      err);
    }

    /* all right. it is opened now. */
    lockdb->info->opened = 1;

    return NULL;
}

/*
** dav_fs_open_lockdb:
**
** "open" the lock database, as specified in the global server configuration.
** If force is TRUE, then the database is opened now, rather than lazily.
**
** Note that only one can be open read/write.
*/
static dav_error * dav_fs_open_lockdb(request_rec *r, int ro, int force,
				      dav_lockdb **lockdb)
{
    dav_lockdb_combined *comb;

    comb = ap_pcalloc(r->pool, sizeof(*comb));
    comb->pub.hooks = &dav_hooks_locks_fs;
    comb->pub.ro = ro;
    comb->pub.info = &comb->priv;
    comb->priv.r = r;
    comb->priv.pool = r->pool;

    comb->priv.lockdb_path = dav_get_lockdb_path(r);
    if (comb->priv.lockdb_path == NULL) {
	return dav_new_error(r->pool, HTTP_INTERNAL_SERVER_ERROR,
			     DAV_ERR_LOCK_NO_DB,
			     "A lock database was not specified with the "
			     "DAVLockDB directive. One must be specified "
			     "to use the locking functionality.");
    }

    /* done initializing. return it. */
    *lockdb = &comb->pub;

    if (force) {
	/* ### add a higher-level comment? */
	return dav_fs_really_open_lockdb(*lockdb);
    }

    return NULL;
}

/*
** dav_fs_close_lockdb:
**
** Close it. Duh.
*/
static void dav_fs_close_lockdb(dav_lockdb *lockdb)
{
    if (lockdb->info->db != NULL)
	(*dav_hooks_db_dbm.close)(lockdb->info->db);
}

/*
** dav_fs_build_fname_key
**
** Given a pathname, build a DAV_TYPE_FNAME lock database key.
*/
static dav_datum dav_fs_build_fname_key(pool *p, const char *pathname)
{
    dav_datum key;

    /* ### does this allocation have a proper lifetime? need to check */
    /* ### can we use a buffer for this? */

    /* size is TYPE + pathname + null */
    key.dsize = strlen(pathname) + 2;
    key.dptr = ap_palloc(p, key.dsize);
    *key.dptr = DAV_TYPE_FNAME;
    memcpy(key.dptr + 1, pathname, key.dsize - 1);
    if (key.dptr[key.dsize - 2] == '/')
	key.dptr[--key.dsize - 1] = '\0';
    return key;
}

/*
** dav_fs_build_key:  Given a resource, return a dav_datum key
**    to look up lock information for this file.
**
**    (Win32 or file is lock-null):
**       dav_datum->dvalue = full path
**
**    (non-Win32 and file exists ):
**       dav_datum->dvalue = inode, dev_major, dev_minor
*/
static dav_datum dav_fs_build_key(pool *p, const dav_resource *resource)
{
    const char *file = dav_fs_pathname(resource);
#ifndef WIN32
    dav_datum key;
    struct stat finfo;

    /* ### use lstat() ?? */
    if (stat(file, &finfo) == 0) {

	/* ### can we use a buffer for this? */
	key.dsize = 1 + sizeof(finfo.st_ino) + sizeof(finfo.st_dev);
	key.dptr = ap_palloc(p, key.dsize);
	*key.dptr = DAV_TYPE_INODE;
	memcpy(key.dptr + 1, &finfo.st_ino, sizeof(finfo.st_ino));
	memcpy(key.dptr + 1 + sizeof(finfo.st_ino), &finfo.st_dev,
	       sizeof(finfo.st_dev));

	return key;
    }
#endif

    return dav_fs_build_fname_key(p, file);
}

/*
** dav_fs_lock_expired:  return 1 (true) if the given timeout is in the past
**    or present (the lock has expired), or 0 (false) if in the future
**    (the lock has not yet expired).
*/
static int dav_fs_lock_expired(time_t expires)
{
    return (expires != DAV_TIMEOUT_INFINITE
            && time(NULL) >= expires + DAV_TIMEOUT_SLACK);
}

/*
** dav_fs_save_lock_record:  Saves the lock information specified in the
**    direct and indirect lock lists about path into the lock database.
**    If direct and indirect == NULL, the key is removed.
*/
static dav_error * dav_fs_save_lock_record(dav_lockdb *lockdb, dav_datum key,
					   dav_lock_discovery *direct,
					   dav_lock_indirect *indirect)
{
    dav_error *err;
    dav_datum val = { 0 };
    char *ptr;
    dav_lock_discovery *dp = direct;
    dav_lock_indirect *ip = indirect;

#if DAV_DEBUG
    if (lockdb->ro) {
	return dav_new_error(lockdb->info->pool,
			     HTTP_INTERNAL_SERVER_ERROR, 0,
			     "INTERNAL DESIGN ERROR: the lockdb was opened "
			     "readonly, but an attempt to save locks was "
			     "performed.");
    }
#endif

    if ((err = dav_fs_really_open_lockdb(lockdb)) != NULL) {
	/* ### add a higher-level error? */
	return err;
    }

    /* If nothing to save, delete key */
    if (dp == NULL && ip == NULL) {
        /* don't fail if the key is not present */
        /* ### but what about other errors? */
	(void) (*dav_hooks_db_dbm.remove)(lockdb->info->db, key);
        return NULL;
    }
		
    while(dp) {
	val.dsize += dav_size_direct(dp);
	dp = dp->next;
    }
    while(ip) {
	val.dsize += dav_size_indirect(ip);
	ip = ip->next;
    }

    /* ### can this be ap_palloc() ? */
    /* ### hmmm.... investigate the use of a buffer here */
    ptr = val.dptr = ap_pcalloc(lockdb->info->pool, val.dsize);
    dp  = direct;
    ip  = indirect;

    while(dp) {
	*ptr++ = DAV_LOCK_DIRECT;	/* Direct lock - lock_discovery struct follows */
	memcpy(ptr, dp, sizeof(dp->f));	/* Fixed portion of struct */
	ptr += sizeof(dp->f);
        memcpy(ptr, dp->locktoken, sizeof(*dp->locktoken));
        ptr += sizeof(*dp->locktoken);
	if (dp->owner == NULL) {
	    *ptr++ = '\0';
	}
	else {
	    memcpy(ptr, dp->owner, strlen(dp->owner) + 1);	
	    ptr += strlen(dp->owner) + 1;
	}
	if (dp->auth_user == NULL) {
            *ptr++ = '\0';
	}
	else {
	    memcpy(ptr, dp->auth_user, strlen(dp->auth_user) + 1);
	    ptr += strlen(dp->auth_user) + 1;
	}

	dp = dp->next;
    }

    while(ip) {
	*ptr++ = DAV_LOCK_INDIRECT;	/* Indirect lock prefix */
	memcpy(ptr, ip->locktoken, sizeof(*ip->locktoken));	/* Locktoken */
	ptr += sizeof(*ip->locktoken);
	memcpy(ptr, &ip->timeout, sizeof(ip->timeout));		/* Expire time */
	ptr += sizeof(ip->timeout);
	memcpy(ptr, &ip->key.dsize, sizeof(ip->key.dsize));	/* Size of key */
	ptr += sizeof(ip->key.dsize);
	memcpy(ptr, ip->key.dptr, ip->key.dsize);	/* Key data */
	ptr += ip->key.dsize;
	ip = ip->next;
    }

    if ((err = (*dav_hooks_db_dbm.store)(lockdb->info->db,
						key, val)) != NULL) {
	/* ### more details? add an error_id? */
	return dav_push_error(lockdb->info->pool,
			      HTTP_INTERNAL_SERVER_ERROR,
			      DAV_ERR_LOCK_SAVE_LOCK,
			      "Could not save lock information.",
			      err);
    }

    return NULL;
}

/*
** dav_load_lock_record:  Reads lock information about key from lock db;
**    creates linked lists of the direct and indirect locks.
**
**    If add_method = DAV_APPEND_LIST, the result will be appended to the
**    head of the direct and indirect lists supplied.
**
**    Passive lock removal:  If lock has timed out, it will not be returned.
**    ### How much "logging" does RFC 2518 require?
*/
static dav_error * dav_fs_load_lock_record(dav_lockdb *lockdb, dav_datum key,
					   int add_method,
					   dav_lock_discovery **direct,
					   dav_lock_indirect **indirect)
{
    dav_error *err;
    size_t offset = 0;
    int need_save = DAV_FALSE;
    dav_datum val = { 0 };
    dav_lock_discovery *dp;
    dav_lock_indirect *ip;
    dav_buffer buf = { 0 };

    if (add_method != DAV_APPEND_LIST) {
	*direct = NULL;
	*indirect = NULL;
    }

    if ((err = dav_fs_really_open_lockdb(lockdb)) != NULL) {
	/* ### add a higher-level error? */
	return err;
    }

    /*
    ** If we opened readonly and the db wasn't there, then there are no
    ** locks for this resource. Just exit.
    */
    if (lockdb->info->db == NULL)
	return NULL;

    if ((err = (*dav_hooks_db_dbm.fetch)(lockdb->info->db, key, &val)) != NULL)
        return err;
	
    if (!val.dsize)
	return NULL;

    while (offset < val.dsize) {
	switch (*(val.dptr + offset++)) {
	case DAV_LOCK_DIRECT:
	    /* Create and fill a dav_lock_discovery structure */

	    dp = ap_pcalloc(lockdb->info->pool, sizeof(*dp));
	    memcpy(dp, val.dptr + offset, sizeof(dp->f));
	    offset += sizeof(dp->f);
            dp->locktoken = ap_palloc(lockdb->info->pool, sizeof(*dp->locktoken));
            memcpy(dp->locktoken, val.dptr + offset, sizeof(*dp->locktoken));
            offset += sizeof(*dp->locktoken);
	    if (*(val.dptr + offset) == '\0') {
		++offset;
	    }
	    else {
		dp->owner = ap_pstrdup(lockdb->info->pool, val.dptr + offset);
		offset += strlen(dp->owner) + 1;
	    }

            if (*(val.dptr + offset) == '\0') {
                ++offset;
            } 
            else {
                dp->auth_user = ap_pstrdup(lockdb->info->pool, val.dptr + offset);
                offset += strlen(dp->auth_user) + 1;
            }

	    if (!dav_fs_lock_expired(dp->f.timeout)) {
		dp->next = *direct;
		*direct = dp;
	    }
	    else {
		need_save = DAV_TRUE;

		/* Remove timed-out locknull fm .locknull list */
		if (*key.dptr == DAV_TYPE_FNAME) {
		    const char *fname = key.dptr + 1;
		    struct stat finfo;

		    /* if we don't see the file, then it's a locknull */
		    if (lstat(fname, &finfo) != 0) {
			if ((err = dav_fs_remove_locknull_member(lockdb->info->pool, fname, &buf)) != NULL) {
                            /* ### push a higher-level description? */
                            return err;
                        }
		    }
		}
	    }
	    break;

	case DAV_LOCK_INDIRECT:
	    /* Create and fill a dav_lock_indirect structure */

	    ip = ap_pcalloc(lockdb->info->pool, sizeof(*ip));
            ip->locktoken = ap_palloc(lockdb->info->pool, sizeof(*ip->locktoken));
	    memcpy(ip->locktoken, val.dptr + offset, sizeof(*ip->locktoken));
	    offset += sizeof(*ip->locktoken);
	    memcpy(&ip->timeout, val.dptr + offset, sizeof(ip->timeout));
	    offset += sizeof(ip->timeout);
	    memcpy(&ip->key.dsize, val.dptr + offset, sizeof(int)); /* length of datum */
	    offset += sizeof(ip->key.dsize);
	    ip->key.dptr = ap_palloc(lockdb->info->pool, ip->key.dsize); 
	    memcpy(ip->key.dptr, val.dptr + offset, ip->key.dsize);
	    offset += ip->key.dsize;

	    if (!dav_fs_lock_expired(ip->timeout)) {
		ip->next = *indirect;
		*indirect = ip;
	    }
	    else {
		need_save = DAV_TRUE;
		/* A locknull resource will never be locked indirectly */
	    }

	    break;

	default:
	    (*dav_hooks_db_dbm.freedatum)(lockdb->info->db, val);

	    /* ### should use a computed_desc and insert corrupt token data */
	    --offset;
	    return dav_new_error(lockdb->info->pool,
				 HTTP_INTERNAL_SERVER_ERROR,
				 DAV_ERR_LOCK_CORRUPT_DB,
				 ap_psprintf(lockdb->info->pool,
					     "The lock database was found to "
					     "be corrupt. offset %i, c=%02x",
					     offset, val.dptr[offset]));
	}
    }

    (*dav_hooks_db_dbm.freedatum)(lockdb->info->db, val);

    /* Clean up this record if we found expired locks */
    /*
    ** ### shouldn't do this if we've been opened READONLY. elide the
    ** ### timed-out locks from the response, but don't save that info back
    */
    if (need_save == DAV_TRUE) {
	return dav_fs_save_lock_record(lockdb, key, *direct, *indirect);
    }

    return NULL;
}

/* resolve <indirect>, returning <*direct> */
static dav_error * dav_fs_resolve(dav_lockdb *lockdb,
				  dav_lock_indirect *indirect,
				  dav_lock_discovery **direct,
				  dav_lock_discovery **ref_dp,
				  dav_lock_indirect **ref_ip)
{
    dav_error *err;
    dav_lock_discovery *dir;
    dav_lock_indirect *ind;
	
    if ((err = dav_fs_load_lock_record(lockdb, indirect->key,
				       DAV_CREATE_LIST,
				       &dir, &ind)) != NULL) {
	/* ### insert a higher-level description? */
	return err;
    }
    if (ref_dp != NULL) {
	*ref_dp = dir;
	*ref_ip = ind;
    }
		
    for (; dir != NULL; dir = dir->next) {
	if (!dav_compare_opaquelocktoken(indirect->locktoken->uuid, 
					 dir->locktoken->uuid)) {
	    *direct = dir;
	    return NULL;
	}
    }

    /* No match found (but we should have found one!) */

    /* ### use a different description and/or error ID? */
    return dav_new_error(lockdb->info->pool,
			 HTTP_INTERNAL_SERVER_ERROR,
			 DAV_ERR_LOCK_CORRUPT_DB,
			 "The lock database was found to be corrupt. "
			 "An indirect lock's direct lock could not "
			 "be found.");
}

/* ---------------------------------------------------------------
**
** Property-related lock functions
**
*/

/*
** dav_fs_get_supportedlock:  Returns a static string for all supportedlock
**    properties. I think we save more returning a static string than
**    constructing it every time, though it might look cleaner.
*/
static const char *dav_fs_get_supportedlock(void)
{
    static const char supported[] = DEBUG_CR
	"<D:lockentry>" DEBUG_CR
	"<D:lockscope><D:exclusive/></D:lockscope>" DEBUG_CR
	"<D:locktype><D:write/></D:locktype>" DEBUG_CR
	"</D:lockentry>" DEBUG_CR
	"<D:lockentry>" DEBUG_CR
	"<D:lockscope><D:shared/></D:lockscope>" DEBUG_CR
	"<D:locktype><D:write/></D:locktype>" DEBUG_CR
	"</D:lockentry>" DEBUG_CR;

    return supported;
}

/* ---------------------------------------------------------------
**
** General lock functions
**
*/

/* ---------------------------------------------------------------
**
** Functions dealing with lock-null resources
**
*/

/*
** dav_fs_load_locknull_list:  Returns a dav_buffer dump of the locknull file
**    for the given directory.
*/
static dav_error * dav_fs_load_locknull_list(pool *p, const char *dirpath,
					     dav_buffer *pbuf) 
{
    struct stat finfo;
    int fd;
    dav_error *err = NULL;

    dav_buffer_init(p, pbuf, dirpath);

    if (pbuf->buf[pbuf->cur_len - 1] == '/')
	pbuf->buf[--pbuf->cur_len] = '\0';

    dav_buffer_place(p, pbuf, "/" DAV_FS_STATE_DIR "/" DAV_FS_LOCK_NULL_FILE);

    /* reset this in case we leave w/o reading into the buffer */
    pbuf->cur_len = 0;

    if ((fd = open(pbuf->buf, O_RDONLY | O_BINARY)) == -1) {
	return NULL;
    }

    if (fstat(fd, &finfo) == -1) {
	err = dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0,
			    ap_psprintf(p,
					"Opened but could not stat file %s",
					pbuf->buf));
	goto loaderror;
    }

    dav_set_bufsize(p, pbuf, finfo.st_size);
    if (read(fd, pbuf->buf, finfo.st_size) != finfo.st_size) {
	err = dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0,
			    ap_psprintf(p,
					"Failure reading locknull file "
					"for %s", dirpath));

	/* just in case the caller disregards the returned error */
	pbuf->cur_len = 0;
	goto loaderror;
    }

  loaderror:
    close(fd);
    return err;
}

/*
** dav_fs_save_locknull_list:  Saves contents of pbuf into the
**    locknull file for dirpath.
*/
static dav_error * dav_fs_save_locknull_list(pool *p, const char *dirpath,
					     dav_buffer *pbuf)
{
    const char *pathname;
    int fd;
    dav_error *err = NULL;

    if (pbuf->buf == NULL)
	return NULL;

    dav_fs_ensure_state_dir(p, dirpath);
    pathname = ap_pstrcat(p,
			  dirpath,
			  dirpath[strlen(dirpath) - 1] == '/' ? "" : "/",
			  DAV_FS_STATE_DIR "/" DAV_FS_LOCK_NULL_FILE,
			  NULL);

    if (pbuf->cur_len == 0) {
	/* delete the file if cur_len == 0 */
	if (remove(pathname) != 0) {
	    return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0,
				 ap_psprintf(p,
					     "Error removing %s", pathname));
	}
	return NULL;
    }

    if ((fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY,
		   DAV_FS_MODE_FILE)) == -1) {
	return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0,
			     ap_psprintf(p,
					 "Error opening %s for writing",
					 pathname));
    }

    if (write(fd, pbuf->buf, pbuf->cur_len) != pbuf->cur_len) {
	err = dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0,
			    ap_psprintf(p,
					"Error writing %i bytes to %s",
					pbuf->cur_len, pathname));
    }

    close(fd);
    return err;
}

/*
** dav_fs_remove_locknull_member:  Removes filename from the locknull list
**    for directory path.
*/
static dav_error * dav_fs_remove_locknull_member(pool *p, const char *filename,
						 dav_buffer *pbuf)
{
    dav_error *err;
    size_t len;
    size_t scanlen;
    char *scan;
    const char *scanend;
    char *dirpath = ap_pstrdup(p, filename);
    char *fname = strrchr(dirpath, '/');
    int dirty = 0;

    if (fname != NULL)
	*fname++ = '\0';
    else
	fname = dirpath;
    len = strlen(fname) + 1;

    if ((err = dav_fs_load_locknull_list(p, dirpath, pbuf)) != NULL) {
	/* ### add a higher level description? */
	return err;
    }

    for (scan = pbuf->buf, scanend = scan + pbuf->cur_len;
	 scan < scanend;
	 scan += scanlen) {
	scanlen = strlen(scan) + 1;
	if (len == scanlen && memcmp(fname, scan, scanlen) == 0) {
	    pbuf->cur_len -= scanlen;
	    memmove(scan, scan + scanlen, scanend - (scan + scanlen));
	    dirty = 1;
	    break;
	}
    }

    if (dirty) {
	if ((err = dav_fs_save_locknull_list(p, dirpath, pbuf)) != NULL) {
	    /* ### add a higher level description? */
	    return err;
	}
    }

    return NULL;
}

/* Note: used by dav_fs_repos.c */
dav_error * dav_fs_get_locknull_members(
    const dav_resource *resource,
    dav_buffer *pbuf)
{
    const char *dirpath;

    dav_fs_dir_file_name(resource, &dirpath, NULL);
    return dav_fs_load_locknull_list(dav_fs_pool(resource), dirpath, pbuf);
}

/* ### fold into append_lock? */
/* ### take an optional buf parameter? */
static dav_error * dav_fs_add_locknull_state(
    dav_lockdb *lockdb,
    const dav_resource *resource)
{
    dav_buffer buf = { 0 };
    pool *p = lockdb->info->pool;
    const char *dirpath;
    const char *fname;
    dav_error *err;

    dav_fs_dir_file_name(resource, &dirpath, &fname);

    if ((err = dav_fs_load_locknull_list(p, dirpath, &buf)) != NULL) {
        return dav_push_error(p, HTTP_INTERNAL_SERVER_ERROR, 0,
			      "Could not load .locknull file.", err);
    }

    dav_buffer_append(p, &buf, fname);
    buf.cur_len++;	/* we want the null-term here */

    if ((err = dav_fs_save_locknull_list(p, dirpath, &buf)) != NULL) {
        return dav_push_error(p, HTTP_INTERNAL_SERVER_ERROR, 0,
			      "Could not save .locknull file.", err);
    }

    return NULL;
}

/*
** dav_fs_remove_locknull_state:  Given a request, check to see if r->filename
**    is/was a lock-null resource.  If so, return it to an existant state.
**
**    ### this function is broken... it doesn't check!
**
**    In this implementation, this involves two things:
**    (a) remove it from the list in the appropriate .DAV/locknull file
**    (b) on *nix, convert the key from a filename to an inode.
*/
static dav_error * dav_fs_remove_locknull_state(
    dav_lockdb *lockdb,
    const dav_resource *resource)
{
    dav_buffer buf = { 0 };
    dav_error *err;
    pool *p = lockdb->info->pool;
    const char *pathname = dav_fs_pathname(resource);

    if ((err = dav_fs_remove_locknull_member(p, pathname, &buf)) != NULL) {
	/* ### add a higher-level description? */
	return err;
    }

#ifndef WIN32
    {
	dav_lock_discovery *ld;
	dav_lock_indirect  *id;
	dav_datum key;

	/*
	** Fetch the lock(s) that made the resource lock-null. Remove
	** them under the filename key. Obtain the new inode key, and
	** save the same lock information under it.
	*/
	key = dav_fs_build_fname_key(p, pathname);
	if ((err = dav_fs_load_lock_record(lockdb, key, DAV_CREATE_LIST,
					   &ld, &id)) != NULL) {
	    /* ### insert a higher-level error description */
	    return err;
	}

	if ((err = dav_fs_save_lock_record(lockdb, key, NULL, NULL)) != NULL) {
	    /* ### insert a higher-level error description */
	    return err;
        }

	key = dav_fs_build_key(p, resource);
	if ((err = dav_fs_save_lock_record(lockdb, key, ld, id)) != NULL) {
	    /* ### insert a higher-level error description */
	    return err;
        }
    }
#endif

    return NULL;
}

static dav_error * dav_fs_create_lock(dav_lockdb *lockdb,
				      const dav_resource *resource,
				      dav_lock **lock)
{
    dav_datum key;

    key = dav_fs_build_key(lockdb->info->pool, resource);

    *lock = dav_fs_alloc_lock(lockdb,
			      key,
			      NULL);

    (*lock)->is_locknull = !resource->exists;

    return NULL;
}

static dav_error * dav_fs_get_locks(dav_lockdb *lockdb,
				    const dav_resource *resource,
				    int calltype,
				    dav_lock **locks)
{
    pool *p = lockdb->info->pool;
    dav_datum key;
    dav_error *err;
    dav_lock *lock = NULL;
    dav_lock *newlock;
    dav_lock_discovery *dp;
    dav_lock_indirect *ip;

#if DAV_DEBUG
    if (calltype == DAV_GETLOCKS_COMPLETE) {
	return dav_new_error(lockdb->info->pool,
			     HTTP_INTERNAL_SERVER_ERROR, 0,
			     "INTERNAL DESIGN ERROR: DAV_GETLOCKS_COMPLETE "
			     "is not yet supported");
    }
#endif

    key = dav_fs_build_key(p, resource);
    if ((err = dav_fs_load_lock_record(lockdb, key, DAV_CREATE_LIST,
				       &dp, &ip)) != NULL) {
	/* ### push a higher-level desc? */
	return err;
    }

    /* copy all direct locks to the result list */
    for (; dp != NULL; dp = dp->next) {
	newlock = dav_fs_alloc_lock(lockdb, key, dp->locktoken);
	newlock->is_locknull = !resource->exists;
	newlock->scope = dp->f.scope;
	newlock->type = dp->f.type;
	newlock->depth = dp->f.depth;
	newlock->timeout = dp->f.timeout;
	newlock->owner = dp->owner;
        newlock->auth_user = dp->auth_user;

	/* hook into the result list */
	newlock->next = lock;
	lock = newlock;
    }

    /* copy all the indirect locks to the result list. resolve as needed. */
    for (; ip != NULL; ip = ip->next) {
	newlock = dav_fs_alloc_lock(lockdb, ip->key, ip->locktoken);
	newlock->is_locknull = !resource->exists;

	if (calltype == DAV_GETLOCKS_RESOLVED) {
	    if ((err = dav_fs_resolve(lockdb, ip, &dp, NULL, NULL)) != NULL) {
		/* ### push a higher-level desc? */
		return err;
	    }

	    newlock->scope = dp->f.scope;
	    newlock->type = dp->f.type;
	    newlock->depth = dp->f.depth;
	    newlock->timeout = dp->f.timeout;
	    newlock->owner = dp->owner;
            newlock->auth_user = dp->auth_user;
	}
	else {
	    /* DAV_GETLOCKS_PARTIAL */
	    newlock->rectype = DAV_LOCKREC_INDIRECT_PARTIAL;
	}

	/* hook into the result list */
	newlock->next = lock;
	lock = newlock;
    }

    *locks = lock;
    return NULL;
}

static dav_error * dav_fs_find_lock(dav_lockdb *lockdb,
				    const dav_resource *resource,
				    const dav_locktoken *locktoken,
				    int partial_ok,
				    dav_lock **lock)
{
    dav_error *err;
    dav_datum key;
    dav_lock_discovery *dp;
    dav_lock_indirect *ip;

    *lock = NULL;

    key = dav_fs_build_key(lockdb->info->pool, resource);
    if ((err = dav_fs_load_lock_record(lockdb, key, DAV_CREATE_LIST,
				       &dp, &ip)) != NULL) {
	/* ### push a higher-level desc? */
	return err;
    }

    for (; dp != NULL; dp = dp->next) {
	if (!dav_compare_opaquelocktoken(locktoken->uuid,
					 dp->locktoken->uuid)) {
	    *lock = dav_fs_alloc_lock(lockdb, key, locktoken);
	    (*lock)->is_locknull = !resource->exists;
	    (*lock)->scope = dp->f.scope;
	    (*lock)->type = dp->f.type;
	    (*lock)->depth = dp->f.depth;
	    (*lock)->timeout = dp->f.timeout;
	    (*lock)->owner = dp->owner;
            (*lock)->auth_user = dp->auth_user;
	    return NULL;
	}
    }

    for (; ip != NULL; ip = ip->next) {
	if (!dav_compare_opaquelocktoken(locktoken->uuid,
					 ip->locktoken->uuid)) {
	    *lock = dav_fs_alloc_lock(lockdb, ip->key, locktoken);
	    (*lock)->is_locknull = !resource->exists;

	    /* ### nobody uses the resolving right now! */
	    if (partial_ok) {
		(*lock)->rectype = DAV_LOCKREC_INDIRECT_PARTIAL;
	    }
	    else {
		(*lock)->rectype = DAV_LOCKREC_INDIRECT;
		if ((err = dav_fs_resolve(lockdb, ip, &dp,
					  NULL, NULL)) != NULL) {
		    /* ### push a higher-level desc? */
		    return err;
		}
		(*lock)->scope = dp->f.scope;
		(*lock)->type = dp->f.type;
		(*lock)->depth = dp->f.depth;
		(*lock)->timeout = dp->f.timeout;
		(*lock)->owner = dp->owner;
                (*lock)->auth_user = dp->auth_user;
	    }
	    return NULL;
	}
    }

    return NULL;
}

static dav_error * dav_fs_has_locks(dav_lockdb *lockdb,
				    const dav_resource *resource,
				    int *locks_present)
{
    dav_error *err;
    dav_datum key;

    *locks_present = 0;

    if ((err = dav_fs_really_open_lockdb(lockdb)) != NULL) {
	/* ### insert a higher-level error description */
	return err;
    }

    /*
    ** If we opened readonly and the db wasn't there, then there are no
    ** locks for this resource. Just exit.
    */
    if (lockdb->info->db == NULL)
	return NULL;

    key = dav_fs_build_key(lockdb->info->pool, resource);

    *locks_present = (*dav_hooks_db_dbm.exists)(lockdb->info->db, key);

    return NULL;
}

static dav_error * dav_fs_append_locks(dav_lockdb *lockdb,
				       const dav_resource *resource,
				       int make_indirect,
				       const dav_lock *lock)
{
    pool *p = lockdb->info->pool;
    dav_error *err;
    dav_lock_indirect *ip;
    dav_lock_discovery *dp;
    dav_datum key;

    key = dav_fs_build_key(lockdb->info->pool, resource);
    if ((err = dav_fs_load_lock_record(lockdb, key, 0, &dp, &ip)) != NULL) {
	/* ### maybe add in a higher-level description */
	return err;
    }

    /*
    ** ### when we store the lock more directly, we need to update
    ** ### lock->rectype and lock->is_locknull
    */

    if (make_indirect) {
	for (; lock != NULL; lock = lock->next) {

	    /* ### this works for any <lock> rectype */
	    dav_lock_indirect *newi = ap_pcalloc(p, sizeof(*newi));

	    /* ### shut off the const warning for now */
	    newi->locktoken = (dav_locktoken *)lock->locktoken;
	    newi->timeout   = lock->timeout;
	    newi->key       = lock->info->key;
	    newi->next      = ip;
	    ip              = newi;
	}
    }
    else {
	for (; lock != NULL; lock = lock->next) {
	    /* create and link in the right kind of lock */

	    if (lock->rectype == DAV_LOCKREC_DIRECT) {
		dav_lock_discovery *newd = ap_pcalloc(p, sizeof(*newd));

		newd->f.scope = lock->scope;
		newd->f.type = lock->type;
		newd->f.depth = lock->depth;
		newd->f.timeout = lock->timeout;
		/* ### shut off the const warning for now */
		newd->locktoken = (dav_locktoken *)lock->locktoken;
		newd->owner = lock->owner;
                newd->auth_user = lock->auth_user;
		newd->next = dp;
		dp = newd;
	    }
	    else {
		/* DAV_LOCKREC_INDIRECT(_PARTIAL) */

		dav_lock_indirect *newi = ap_pcalloc(p, sizeof(*newi));

		/* ### shut off the const warning for now */
		newi->locktoken = (dav_locktoken *)lock->locktoken;
		newi->key       = lock->info->key;
		newi->next      = ip;
		ip              = newi;
	    }
	}
    }

    if ((err = dav_fs_save_lock_record(lockdb, key, dp, ip)) != NULL) {
	/* ### maybe add a higher-level description */
	return err;
    }

    /* we have a special list for recording locknull resources */
    /* ### ack! this can add two copies to the locknull list */
    if (!resource->exists
	&& (err = dav_fs_add_locknull_state(lockdb, resource)) != NULL) {
	/* ### maybe add a higher-level description */
	return err;
    }

    return NULL;
}

static dav_error * dav_fs_remove_lock(dav_lockdb *lockdb,
				      const dav_resource *resource,
				      const dav_locktoken *locktoken)
{
    dav_error *err;
    dav_buffer buf = { 0 };
    dav_lock_discovery *dh = NULL;
    dav_lock_indirect *ih = NULL;
    dav_datum key;

    key = dav_fs_build_key(lockdb->info->pool, resource);

    if (locktoken != NULL) {
	dav_lock_discovery *dp;
	dav_lock_discovery *dprev = NULL;
	dav_lock_indirect *ip;
	dav_lock_indirect *iprev = NULL;

	if ((err = dav_fs_load_lock_record(lockdb, key, DAV_CREATE_LIST,
					   &dh, &ih)) != NULL) {
	    /* ### maybe add a higher-level description */
	    return err;
	}

	for (dp = dh; dp != NULL; dp = dp->next) {
	    if (dav_compare_opaquelocktoken(locktoken->uuid,
					    dp->locktoken->uuid) == 0) {
		if (dprev)
		    dprev->next = dp->next;
		else
		    dh = dh->next;
	    }
	    dprev = dp;
	}

	for (ip = ih; ip != NULL; ip = ip->next) {
	    if (dav_compare_opaquelocktoken(locktoken->uuid,
					    ip->locktoken->uuid) == 0) {
		if (iprev)
		    iprev->next = ip->next;
		else
		    ih = ih->next;
	    }
	    iprev = ip;
	}

    }

    /* save the modified locks, or remove all locks (dh=ih=NULL). */
    if ((err = dav_fs_save_lock_record(lockdb, key, dh, ih)) != NULL) {
        /* ### maybe add a higher-level description */
        return err;
    }

    /*
    ** If this resource is a locknull resource AND no more locks exist,
    ** then remove the locknull member.
    **
    ** Note: remove_locknull_state() attempts to convert a locknull member
    **       to a real member. In this case, all locks are gone, so the
    **       locknull resource returns to the null state (ie. doesn't exist),
    **       so there is no need to update the lockdb (and it won't find
    **       any because a precondition is that none exist).
    */
    if (!resource->exists && dh == NULL && ih == NULL
	&& (err = dav_fs_remove_locknull_member(lockdb->info->pool,
						dav_fs_pathname(resource),
						&buf)) != NULL) {
	/* ### maybe add a higher-level description */
	return err;
    }

    return NULL;
}

static int dav_fs_do_refresh(dav_lock_discovery *dp,
			     const dav_locktoken_list *ltl,
			     time_t new_time)
{
    int dirty = 0;

    for (; ltl != NULL; ltl = ltl->next) {
	if (dav_compare_opaquelocktoken(dp->locktoken->uuid,
					ltl->locktoken->uuid) == 0)
	{
	    dp->f.timeout = new_time;
	    dirty = 1;
	}
    }

    return dirty;
}

static dav_error * dav_fs_refresh_locks(dav_lockdb *lockdb,
					const dav_resource *resource,
					const dav_locktoken_list *ltl,
					time_t new_time,
					dav_lock **locks)
{
    dav_error *err;
    dav_datum key;
    dav_lock_discovery *dp;
    dav_lock_discovery *dp_scan;
    dav_lock_indirect *ip;
    int dirty = 0;
    dav_lock *newlock;

    *locks = NULL;

    key = dav_fs_build_key(lockdb->info->pool, resource);
    if ((err = dav_fs_load_lock_record(lockdb, key, DAV_CREATE_LIST,
				       &dp, &ip)) != NULL) {
	/* ### maybe add in a higher-level description */
	return err;
    }

    /* ### we should be refreshing direct AND (resolved) indirect locks! */

    /* refresh all of the direct locks on this resource */
    for (dp_scan = dp; dp_scan != NULL; dp_scan = dp_scan->next) {
	if (dav_fs_do_refresh(dp_scan, ltl, new_time)) {
	    /* the lock was refreshed. return the lock. */
	    newlock = dav_fs_alloc_lock(lockdb, key, dp_scan->locktoken);
	    newlock->is_locknull = !resource->exists;
	    newlock->scope = dp_scan->f.scope;
	    newlock->type = dp_scan->f.type;
	    newlock->depth = dp_scan->f.depth;
	    newlock->timeout = dp_scan->f.timeout;
	    newlock->owner = dp_scan->owner;
            newlock->auth_user = dp_scan->auth_user;

	    newlock->next = *locks;
	    *locks = newlock;

	    dirty = 1;
	}
    }

    /* if we refreshed any locks, then save them back. */
    if (dirty
	&& (err = dav_fs_save_lock_record(lockdb, key, dp, ip)) != NULL) {
	/* ### maybe add in a higher-level description */
	return err;
    }

    /* for each indirect lock, find its direct lock and refresh it. */
    for (; ip != NULL; ip = ip->next) {
	dav_lock_discovery *ref_dp;
	dav_lock_indirect *ref_ip;

	if ((err = dav_fs_resolve(lockdb, ip, &dp_scan,
				  &ref_dp, &ref_ip)) != NULL) {
	    /* ### push a higher-level desc? */
	    return err;
	}
	if (dav_fs_do_refresh(dp_scan, ltl, new_time)) {
	    /* the lock was refreshed. return the lock. */
	    newlock = dav_fs_alloc_lock(lockdb, ip->key, dp->locktoken);
	    newlock->is_locknull = !resource->exists;
	    newlock->scope = dp->f.scope;
	    newlock->type = dp->f.type;
	    newlock->depth = dp->f.depth;
	    newlock->timeout = dp->f.timeout;
	    newlock->owner = dp->owner;
            newlock->auth_user = dp_scan->auth_user;

	    newlock->next = *locks;
	    *locks = newlock;

	    /* save the (resolved) direct lock back */
	    if ((err = dav_fs_save_lock_record(lockdb, ip->key, ref_dp,
					       ref_ip)) != NULL) {
		/* ### push a higher-level desc? */
		return err;
	    }
	}
    }

    return NULL;
}


const dav_hooks_locks dav_hooks_locks_fs =
{
    dav_fs_get_supportedlock,
    dav_fs_parse_locktoken,
    dav_fs_format_locktoken,
    dav_fs_compare_locktoken,
    dav_fs_open_lockdb,
    dav_fs_close_lockdb,
    dav_fs_remove_locknull_state,
    dav_fs_create_lock,
    dav_fs_get_locks,
    dav_fs_find_lock,
    dav_fs_has_locks,
    dav_fs_append_locks,
    dav_fs_remove_lock,
    dav_fs_refresh_locks,
    NULL, /* get_resource */
};
