/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#ifndef _SMB_CONN_H_
#define _SMB_CONN_H_
#ifndef _NETINET_IN_H_
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#endif
#include <sys/kauth.h>

#ifdef _KERNEL
#include <gssd/gssd_mach_types.h>
#else
#include <Kernel/gssd/gssd_mach_types.h>
#endif

#include <libkern/OSTypes.h>

/*
 * Two levels of connection hierarchy
 */
#define	SMBL_VCLIST	0x0000
#define SMBL_VC		0x0001
#define SMBL_SHARE	0x0002
#define SMBL_VALID	0x0003

/*
 * Device flags
 */

#define NSMBFL_OPEN		0x0001
#define NSMBFL_CANCEL	0x0002
#define NSMBFL_SHAREVC	0x0004

/*
 * Common object flags
 *
 * Remember that vc_flags and ss_flags are defined to be co_flags,
 * so those flags can never reuse this value. 
 */
#define SMBO_GONE		0x80000000

/*
 * access modes
 */
#define	SMBM_READ		0400	/* read conn attrs.(like list shares) */
#define	SMBM_WRITE		0200	/* modify conn attrs */
#define	SMBM_EXEC		0100	/* can send SMB requests */
#define	SMBM_READGRP		0040
#define	SMBM_WRITEGRP		0020
#define	SMBM_EXECGRP		0010
#define	SMBM_READOTH		0004
#define	SMBM_WRITEOTH		0002
#define	SMBM_EXECOTH		0001
#define	SMBM_MASK		0777
#define	SMBM_ALL		(SMBM_READ | SMBM_WRITE | SMBM_EXEC)
#define	SMBM_DEFAULT		(SMBM_READ | SMBM_WRITE | SMBM_EXEC)

#define SMBM_RECONNECT_WAIT_TIME	60 * 10	/* Wait 10 minutes for reconnect? */

/*
 * OS Strings - Currently we only check for Windows 2000, XP and Dawin because
 * we want to treat them special.
 */
#define WIN2K_XP_UTF8_NAME "Windows 5."
#define DARWIN_UTF8_NAME "Darwin"

/*
 * VC flags
 */
#define SMBV_USER_SECURITY          SMB_SM_USER		/* 0x01, server in the user security mode */
#define SMBV_ENCRYPT_PASSWORD       SMB_SM_ENCRYPT	/* 0x02 use challenge/response for password */
#define SMBV_SIGNING                SMB_SM_SIGS		/* 0x04 server does SMB Signing */
#define SMBV_SIGNING_REQUIRED       SMB_SM_SIGS_REQ	/* 0x08 server requires SMB Signing */
#define SMBV_SECURITY_MODE_MASK     0x000000ff		/* Lower byte reserved for the security modes */

#define	SMBV_NT4                    0x00000100		/* Tells us the server is a NT4 */
#define	SMBV_WIN2K_XP               0x00000200		/* Tells us the server is Windows 2000 or XP */
#define SMBV_DARWIN                 0x00000400		/* Mac OS X Server */
#define SMBV_SMB30                  0x00000800		/* Using SMB 3.0 */
#define SMBV_SMB2                   0x00001000		/* Using some version of SMB 2 or 3 */
#define SMBV_SMB2002                0x00002000		/* Using SMB 2.002 */
#define SMBV_SMB21                  0x00004000		/* Using SMB 2.1 */
#define SMBV_SMB302                 0x00008000		/* Using SMB 3.02 */
#define SMBV_SERVER_MODE_MASK       0x0000ff00		/* This nible is reserved for special server types */

#define SMBV_NETWORK_SID            0x00010000		/* The user's sid has been set on the vc */
#define	SMBV_AUTH_DONE              0x00080000		/* Security compeleted successfully */
#define SMBV_PRIV_GUEST_ACCESS      0x00100000		/* Guest access is private */
#define SMBV_KERBEROS_ACCESS        0x00200000		/* This VC is using Kerberos */
#define SMBV_GUEST_ACCESS           0x00400000		/* user is using guess security */
#define SMBV_ANONYMOUS_ACCESS       0x00800000		/* user is using anonymous security */
#define SMBV_HOME_ACCESS_OK         0x01000000		/* <Currently unused> can touch the user home directory */
#define SMBV_RAW_NTLMSSP            0x02000000		/* server only supports RAW NTLM, no NTLMSSP */
#define SMBV_NO_NTLMV1              0x04000000		/* NTLMv1 not allowed in non Extended Security case */
#define SMBV_USER_LAND_MASK         0x07f00000		/* items that are changable by the user */
#define SMBV_SFS_ACCESS             0x08000000		/* Server is using simple file sharing. All access is forced to guest This is a kernel only flag */
#define	SMBV_GONE                   SMBO_GONE		/* 0x80000000 - Reserved see above for more details */

/*
 * vc_misc_flags - another flags field since vc_flags is almost full.
 */
#define	SMBV_NEG_SMB1_ONLY  0x00000001		/* Only allow SMB 1 */
#define	SMBV_NEG_SMB2_ONLY  0x00000002		/* Only allow SMB 2 */
#define	SMBV_64K_QUERY_DIR  0x00000004		/* Use 64Kb OutputBufLen in Query_Dir */
#define	SMBV_HAS_FILEIDS    0x00000010		/* Has File IDs that we can use for hash values and inode number */
#define	SMBV_NO_QUERYINFO   0x00000020		/* Server does not like Query Info for FileAllInformation */
#define	SMBV_OSX_SERVER     0x00000040		/* Server is OS X based */
#define	SMBV_OTHER_SERVER   0x00000080		/* Server is not OS X based */
#define SMBV_CLIENT_SIGNING_REQUIRED	0x00000100
#define SMBV_NON_COMPOUND_REPLIES       0x00000200    /* Server does not send compound replies */
#define SMBV_63K_IOCTL      0x00000400      /* Use 63K MaxOutputResponse */
#define SMBV_HAS_COPYCHUNK  0x00000800      /* Server supports FSCTL_SRV_COPY_CHUNK IOCTL */
#define	SMBV_NEG_SMB3_ONLY  0x00001000		/* Only allow SMB 3 */
#define	SMBV_NO_WRITE_THRU  0x00002000		/* Server does not like Write Through */

#define SMBV_HAS_GUEST_ACCESS(vcp)		(((vcp)->vc_flags & (SMBV_GUEST_ACCESS | SMBV_SFS_ACCESS)) != 0)
#define SMBV_HAS_ANONYMOUS_ACCESS(vcp)	(((vcp)->vc_flags & (SMBV_ANONYMOUS_ACCESS | SMBV_SFS_ACCESS)) != 0)

/*
 * True if dialect is SMB 2.1 or later (i.e., SMB 2.1, SMB 3.0, SMB 3.1, SMB 3.02, ...)
 * Important: Remember to update this when adding new dialects.
 */
#define SMBV_SMB21_OR_LATER(vcp) (((vcp)->vc_flags & (SMBV_SMB21 | SMBV_SMB30 | SMBV_SMB302)) != 0)

#define kSMB_64K 65536      /* For the QueryDir and QueryInfo limits */
#define kSMB_63K 65534      /* <14281932> Max Net App can handle in IOCTL */
#define kSMB_MAX_TX 1048576 /* 1 MB max transaction size to match Win Clients */

/*
 * smb_share flags
 */
#define SMBS_PERMANENT		0x0001
#define SMBS_RECONNECTING	0x0002
#define SMBS_CONNECTED		0x0004
#define SMBS_GOING_AWAY		0x0008
#define	SMBS_GONE			SMBO_GONE		/* 0x80000000 - Reserved see above for more details */

/*
 * Negotiated protocol parameters
 */
struct smb_sopt {
    uint32_t    sv_maxtx;           /* maximum transmit buf size */
    uint16_t    sv_maxmux;          /* SMB 1 - max number of outstanding rq's */
    uint16_t    sv_maxvcs;          /* SMB 1 - max number of VCs */
    uint32_t    sv_skey;            /* session key */
    uint32_t    sv_caps;            /* SMB 1 - capabilities, preset for SMB 2/3 */
    uint32_t    sv_sessflags;       /* SMB 2/3 - final session setup reply flags */
    uint16_t    sv_dialect;         /* SMB 2 - dialect (non zero for SMB 2/3 */
    uint32_t    sv_capabilities;    /* SMB 2 - capabilities */
    uint32_t    sv_maxtransact;     /* SMB 2 - max transact size */
    uint32_t    sv_maxread;         /* SMB 2 - max read size */
    uint32_t    sv_maxwrite;        /* SMB 2 - max write size */
    uint8_t     sv_guid[16];        /* SMB 2 - GUID */
    uint16_t    sv_security_mode;   /* SMB 2 - security mode */
};

/*
 * network IO daemon states
 */
enum smbiod_state {
	SMBIOD_ST_NOTCONN,	/* no connect request was made */
	SMBIOD_ST_CONNECT,	/* a connect attempt is in progress */
	SMBIOD_ST_TRANACTIVE,	/* transport level is up */
	SMBIOD_ST_NEGOACTIVE,	/* completed negotiation */
	SMBIOD_ST_SSNSETUP,	/* started (a) session setup */
	SMBIOD_ST_VCACTIVE,	/* session established */
	SMBIOD_ST_DEAD,		/* connection broken, transport is down */
	SMBIOD_ST_RECONNECT	/* We need to attempt to reconnect again */
};

/*
 * File system types ss_fstype
 */
enum smb_fs_types { 
	SMB_FS_FAT = 0,				/* Fat file system */
	SMB_FS_CDFS = 1,			/* CD file system */
	SMB_FS_UDF = 2,				/* DVD file system */
	SMB_FS_NTFS_UNKNOWN = 3,	/* NTFS file system, sometimes faked by server no streams support */
	SMB_FS_NTFS = 4,			/* Real NTFS or fully pretending, NTFS share that also supports STREAMS. */
	SMB_FS_NTFS_UNIX = 5,		/* Pretending to be NTFS file system, no streams, but it is a UNIX system */
	SMB_FS_MAC_OS_X = 6			/* Mac OS X Server, SMB 2/3 or greater */
};

#ifdef _KERNEL

#include <sys/lock.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_fid.h>

struct smbioc_negotiate;
struct smbioc_setup;
struct smbioc_share;

TAILQ_HEAD(smb_rqhead, smb_rq);

#define SMB_NBTIMO	15
#define SMB_DEFRQTIMO	30	/* 30 for oplock revoke/writeback */
#define SMBWRTTIMO	60
#define SMBSSNSETUPTIMO	60
#define SMBNOREPLYWAIT (0)

struct smb_tran_desc;
struct smb_dev;

/*
 * Connection object
 */
struct smb_connobj;

typedef void smb_co_gone_t (struct smb_connobj *cp, vfs_context_t context);
typedef void smb_co_free_t (struct smb_connobj *cp);

struct smb_connobj {
	int			co_level;	/* SMBL_ */
	uint32_t	co_flags;
	lck_mtx_t   *co_lock;
	void		 *co_lockowner;
	int32_t		co_lockcount;
	uint32_t	co_lock_flags;
	lck_mtx_t	co_interlock;
	int			co_usecount;
	struct smb_connobj *	co_parent;
	SLIST_HEAD(,smb_connobj)co_children;
	SLIST_ENTRY(smb_connobj)co_next;
	smb_co_gone_t *		co_gone;
	smb_co_free_t *		co_free;
};

#define SMBFS_CO_LOCK_WAIT 1

#define SMBLIST_NEXT(var, field)    (typeof(var))(((struct smb_connobj*)var)->field.sle_next)
#define SMBLIST_FIRST(head, var)    (typeof (var))(SLIST_FIRST(head))

#define SMBLIST_FOREACH_SAFE(var, head, field, tvar)   \
for((var) = SMBLIST_FIRST(head, var);                  \
(var) &&  ((tvar) = SMBLIST_NEXT((var), field), 1);    \
(var) = (tvar))

#define SMBCO_FOREACH_SAFE(var, cp, tvar)    SMBLIST_FOREACH_SAFE((var), &(cp)->co_children, co_next, tvar)

/*
 * Data structure to access gssd for the use of SPNEGO/Kerberos
 */
struct smb_gss {
	mach_port_t	gss_mp;			/* Mach port to gssd */
	au_asid_t	gss_asid;		/* Audit session id to find gss_mp */
	gssd_nametype	gss_target_nt;		/* Service's principal's name type */
	uint32_t	gss_spn_len;		/* Service's principal's length */
	uint8_t *	gss_spn;		/* Service's principal name */
	gssd_nametype	gss_client_nt;		/* Client's principal's name type */
	uint32_t	gss_cpn_len;		/* Client's principal's length */
	uint8_t *	gss_cpn;		/* Client's principal name */
	char *		gss_cpn_display;	/* String representation of client principal */
	uint32_t	gss_tokenlen;		/* Gss token length */
	uint8_t *	gss_token;		/* Gss token */
	uint64_t	gss_ctx;		/* GSS opaque context handle */
	uint64_t	gss_cred;		/* GSS opaque cred handle */
	uint32_t	gss_rflags;		/* Flags returned from gssd */
	uint32_t	gss_major;		/* GSS major error code */
	uint32_t	gss_minor;		/* GSS minor (mech) error code */
	uint32_t	gss_smb_error;		/* Last error returned by smb SetUpAndX */
};

/*
 * Virtual Circuit (session) to a server.
 * This is the most (over)complicated part of SMB protocol.
 * For the user security level (usl), each session with different remote
 * user name has its own VC.
 * It is unclear however, should share security level (ssl) allow additional
 * VCs, because user name is not used and can be the same. On other hand,
 * multiple VCs allows us to create separate sessions to server on a per
 * user basis.
 */

/*
 * This lock protects vc_flags
 */
#define	SMBC_ST_LOCK(vcp)	lck_mtx_lock(&(vcp)->vc_stlock)
#define	SMBC_ST_UNLOCK(vcp)	lck_mtx_unlock(&(vcp)->vc_stlock)

/*
 * This lock protects vc_credits_ fields
 */
#define SMBC_CREDIT_LOCKPTR(vcp)  (&(vcp)->vc_credits_lock)
#define	SMBC_CREDIT_LOCK(vcp)	lck_mtx_lock(&(vcp)->vc_credits_lock)
#define	SMBC_CREDIT_UNLOCK(vcp)	lck_mtx_unlock(&(vcp)->vc_credits_lock)

/* SMB3 Signing/Encrypt Key Length */
#define SMB3_KEY_LEN 16

struct smb_vc {
	struct smb_connobj	obj;
	char				*vc_srvname;		/* The server name used for tree connect, also used for logging */
	char				*vc_localname;
	char				ipv4v6DotName[45+1];/* max IPv6 presentation len */
	struct sockaddr		*vc_saddr;			/* server addr */
	struct sockaddr		*vc_laddr;			/* local addr, if any, only used for port 139 */
	char				*vc_username;
	char				*vc_pass;			/* password for usl case */
	char				*vc_domain;			/* workgroup/primary domain */
	int32_t				vc_volume_cnt;
	unsigned			vc_timo;			/* default request timeout */
	int32_t				vc_number;			/* number of this VC from the client side */
	uid_t				vc_uid;				/* user id of connection */
	u_short				vc_smbuid;			/* unique vc id assigned by server */
	u_char				vc_hflags;			/* or'ed with flags in the smb header */
	u_short				vc_hflags2;			/* or'ed with flags in the smb header */
	void				*vc_tdata;			/* transport control block */
	struct smb_tran_desc *vc_tdesc;
	int					vc_chlen;			/* actual challenge length */
	u_char				vc_ch[SMB_MAXCHALLENGELEN];
	uint16_t			vc_mid;				/* multiplex id */
	uint16_t			vc_low_pid;			/* used for async requests only */
    uuid_t              vc_client_guid;     /* SMB 2/3 client Guid for Neg req */
	uint64_t            vc_message_id;		/* SMB 2/3 request message id */
	uint32_t            vc_credits_granted; /* SMB 2/3 credits granted */
	uint32_t            vc_credits_ss_granted; /* SMB 2/3 credits granted from session setup replies */
	uint32_t            vc_credits_max;     /* SMB 2/3 max amount of credits server has granted us */
	int32_t             vc_credits_wait;    /* SMB 2/3 credit wait */
    uint32_t            vc_req_pending;     /* SMB 2/3 set if there is a pending request */
    uint64_t            vc_oldest_message_id; /* SMB 2/3 oldest pending request message id */
	lck_mtx_t			vc_credits_lock;
	uint64_t            vc_session_id;      /* SMB 2/3 session id */
	uint64_t            vc_prev_session_id; /* SMB 2/3 prev sessID for reconnect */
	uint64_t            vc_misc_flags;      /* SMB 2/3 misc flags */
	struct smb_sopt		vc_sopt;			/* server options */
	uint32_t			vc_txmax;			/* max tx/rx packet size */
	uint32_t			vc_rxmax;			/* max readx data size */
	uint32_t			vc_wxmax;			/* max writex data size */
	struct smbiod		*vc_iod;
	lck_mtx_t			vc_stlock;
	uint32_t			vc_seqno;			/* my next sequence number */
	uint8_t				*vc_mackey;			/* MAC key */
	uint32_t			vc_mackeylen;		/* length of MAC key */
    
    /* SMB 3 signing key (Session.SessionKey) */
    uint8_t             vc_smb3_signing_key[SMB3_KEY_LEN];
    uint32_t            vc_smb3_signing_key_len;
    
    /* SMB 3 encryption key (Session.EncryptionKey) */
    /* A 128-bit key used for encrypting messages sent by the client */
    uint8_t             vc_smb3_encrypt_key[SMB3_KEY_LEN];
    uint32_t            vc_smb3_encrypt_key_len;
    
    /* SMB 3 decryption key (Session.DecryptionKey) */
    /* A 128-bit key used for decrypting messages received from the server. */
    uint8_t             vc_smb3_decrypt_key[SMB3_KEY_LEN];
    uint32_t            vc_smb3_decrypt_key_len;
    
    /* SMB 3 Nonce used for encryption */
    uint64_t            vc_smb3_nonce_high;
    uint64_t            vc_smb3_nonce_low;
    
	uint32_t			reconnect_wait_time;	/* Amount of time to wait while reconnecting */
	uint32_t			*connect_flag;
	char				*NativeOS;
	char				*NativeLANManager;
	uint32_t			negotiate_tokenlen;	/* negotiate token length */
	uint8_t				*negotiate_token;	/* negotiate token */
	struct smb_gss		vc_gss;				/* Parameters for gssd */
	ntsid_t				vc_ntwrk_sid;
	void				*throttle_info;
	uint64_t            vc_server_caps;     /* SMB 2/3 server capabilities */
	uint64_t            vc_volume_caps;     /* SMB 2/3 volume capabilities*/
	char                *vc_model_info;     /* SMB 2/3 server model string */
    int32_t             vc_lease_key;       /* SMB 2/3 lease key incrementer to keep it unique */
    uint32_t            vc_resp_wait_timeout; /* max time to wait for any response to arrive */
};

#define vc_maxmux	vc_sopt.sv_maxmux
#define	vc_flags	obj.co_flags

#define SMB_UNICODE_STRINGS(vcp)	((vcp)->vc_hflags2 & SMB_FLAGS2_UNICODE)
#define VC_CAPS(a) ((a)->vc_sopt.sv_caps)
#define UNIX_SERVER(a) (VC_CAPS(a) & SMB_CAP_UNIX)

/*
 * smb_share structure describes connection to the given SMB share (tree).
 * Connection to share is always built on top of the VC.
 */

struct smb_share;

typedef int ss_going_away_t (struct smb_share *share);
typedef void ss_dead_t (struct smb_share *share);
typedef void ss_up_t (struct smb_share *share, int reconnect);
typedef int ss_down_t (struct smb_share *share, int timeToNotify);

struct smb_share {
	struct smb_connobj obj;
	lck_mtx_t		ss_stlock;	/* Used to lock the flags field only */
	char			*ss_name;
	struct smbmount	*ss_mount;	/* used for smb up/down */
	ss_going_away_t	*ss_going_away;
	ss_dead_t		*ss_dead;
	ss_down_t		*ss_down;
	ss_up_t			*ss_up;
	lck_mtx_t		ss_shlock;	/* used to protect ss_mount */ 
	uint32_t		ss_dead_timer;	/* Time to wait before this share should be marked dead, zero means never */
	uint32_t		ss_soft_timer;	/* Time to wait before this share should return time out errors, zero means never */
	u_short			ss_tid;         /* Tree ID for SMB 1 */
	uint32_t		ss_tree_id;		/* Tree ID for SMB 2/3 */
	uint32_t		ss_share_type;	/* Tree share type for SMB 2/3 */
	uint32_t		ss_share_flags;	/* Tree share flags for SMB 2/3 */
	uint32_t		ss_share_caps;	/* Tree share capabilities for SMB 2/3 */
	uint64_t		ss_unix_caps;	/* Unix capabilites are per share not VC */
	enum smb_fs_types ss_fstype;	/* File system type of the share */
	uint32_t		ss_attributes;	/* File System Attributes */
	uint32_t		ss_maxfilenamelen;
	uint16_t		optionalSupport;
	uint32_t		maxAccessRights;    /* SMB 1 and SMB 2/3 */
	uint32_t		maxGuestAccessRights;
	
	/* SMB 2/3 FID mapping support */
	lck_mtx_t		ss_fid_lock;
	uint64_t		ss_fid_collisions;
	uint64_t		ss_fid_inserted;
	uint64_t		ss_fid_max_iter;
	FID_HASH_TABLE_SLOT	ss_fid_table[SMB_FID_TABLE_SIZE];
};

#define	ss_flags	obj.co_flags

#define VCTOCP(vcp)		(&(vcp)->obj)
#define	SSTOVC(ssp)		((struct smb_vc*)((ssp)->obj.co_parent))
#define SSTOCP(ssp)		(&(ssp)->obj)
#define UNIX_CAPS(ssp)	(ssp)->ss_unix_caps

/*
 * Session level functions
 */
int smb_sm_init(void);
int smb_sm_done(void);
int smb_sm_negotiate(struct smbioc_negotiate *vcspec, vfs_context_t context, 
					 struct smb_vc **vcpp, struct smb_dev *sdp, int searchOnly);
int smb_sm_ssnsetup(struct smb_vc *vcp, struct smbioc_setup * sspec, 
					vfs_context_t context);
int smb_sm_tcon(struct smb_vc *vcp, struct smbioc_share *sspec, 
				struct smb_share **shpp, vfs_context_t context);
uint32_t smb_vc_caps(struct smb_vc *vcp);
void parse_server_os_lanman_strings(struct smb_vc *vcp, void *refptr, 
									uint16_t bc);

/*
 * session level functions
 */
int  smb_vc_negotiate(struct smb_vc *vcp, vfs_context_t context);
int  smb_vc_ssnsetup(struct smb_vc *vcp);
int  smb_vc_access(struct smb_vc *vcp, vfs_context_t context);
void smb_vc_ref(struct smb_vc *vcp);
void smb_vc_rele(struct smb_vc *vcp, vfs_context_t context);
int  smb_vc_lock(struct smb_vc *vcp);
void smb_vc_unlock(struct smb_vc *vcp);
int smb_vc_reconnect_ref(struct smb_vc *vcp, vfs_context_t context);
void smb_vc_reconnect_rel(struct smb_vc *vcp);
const char * smb_vc_getpass(struct smb_vc *vcp);

/*
 * share level functions
 */
void smb_share_ref(struct smb_share *share);
void smb_share_rele(struct smb_share *share, vfs_context_t context);
const char * smb_share_getpass(struct smb_share *share);

/*
 * SMB protocol level functions
 */
int  smb1_smb_negotiate(struct smb_vc *vcp, vfs_context_t user_context,
                        int inReconnect, int onlySMB1, vfs_context_t context);
int  smb_smb_ssnsetup(struct smb_vc *vcp, int inReconnect, vfs_context_t context);
int  smb_smb_ssnclose(struct smb_vc *vcp, vfs_context_t context);
int  smb_smb_treeconnect(struct smb_share *share, vfs_context_t context);
int  smb_smb_treedisconnect(struct smb_share *share, vfs_context_t context);
int  smb1_read(struct smb_share *share, SMBFID fid, uio_t uio, 
              vfs_context_t context);
int  smb1_write(struct smb_share *share, SMBFID fid, uio_t uio, int ioflag, 
               vfs_context_t context);
int  smb1_echo(struct smb_vc *vcp, int timo, uint32_t EchoCount, 
              vfs_context_t context);
int  smb_checkdir(struct smb_share *share, struct smbnode *dnp, 
                  const char *name, size_t nmlen, vfs_context_t context);

#define SMBIOD_INTR_TIMO		2       
#define SMBIOD_SLEEP_TIMO       2       
#define SMB_SEND_WAIT_TIMO		60 * 2	/* How long should we wait for the server to response to a request. */
#define SMB_RESP_WAIT_TIMO		30		/* How long should we wait for the server to response to any request. */

/*
 * After this many seconds we want an unresponded-to request to trigger 
 * some sort of UE (dialogue).  If the connection hasn't responded at all
 * in this many seconds then the dialogue is of the "connection isn't
 * responding would you like to force unmount" variety.  If the connection
 * has been responding (to other requests that is) then we need a dialogue
 * of the "operation is still pending do you want to cancel it" variety.
 * At present this latter dialogue does not exist so we have no UE and
 * just keep waiting for the slow operation.
 *
 * Raised from 8 to 12, to improve PPP DSL connections. Set to 10s to better
 * match SMB_RESP_WAIT_TIMO second timeout of 30s.
 */
#define SMBUETIMEOUT			10		/* Seconds until we send an Echo request */
#define SMB_MAX_SLEEP_CNT		5		/* Max seconds we wait between connections while doing a reconnect. */
#define NOTIFY_USER_TIMEOUT		5		/* Seconds before we will notify user there is a problem. */
#define SOFTMOUNT_TIMEOUT		12		/* Seconds to wait before soft mount calls time out */
#define DEAD_TIMEOUT			60		/* Seconds until we force unmount a soft mount (ie its dead) */
#define HARD_DEAD_TIMER			10 * 60	/* Seconds until we force unmount a hard mount (ie its dead) */
#define TRIGGER_DEAD_TIMEOUT	30		/* Seconds until we force unmount a trigger mount (ie its dead) */

#define SMB_IOD_EVLOCKPTR(iod)  (&((iod)->iod_evlock))
#define SMB_IOD_EVLOCK(iod)     lck_mtx_lock(&((iod)->iod_evlock))
#define SMB_IOD_EVUNLOCK(iod)   lck_mtx_unlock(&((iod)->iod_evlock))

#define SMB_IOD_RQLOCKPTR(iod)  (&((iod)->iod_rqlock))
#define SMB_IOD_RQLOCK(iod)     lck_mtx_lock(&((iod)->iod_rqlock))
#define SMB_IOD_RQUNLOCK(iod)   lck_mtx_unlock(&((iod)->iod_rqlock))

#define SMB_IOD_FLAGSLOCKPTR(iod)       (&((iod)->iod_flagslock))
#define SMB_IOD_FLAGSLOCK(iod)          lck_mtx_lock(&((iod)->iod_flagslock))
#define SMB_IOD_FLAGSUNLOCK(iod)        lck_mtx_unlock(&((iod)->iod_flagslock))

#define smb_iod_wakeup(iod)     wakeup(&(iod)->iod_flags)

/*
 * smbiod thread
 */

/* 
 * Event type (ev_type) must be less than 0xff 
 * Upper bits are used to setting Sync and Processing
 */
#define	SMBIOD_EV_NEWRQ		0x0001
#define	SMBIOD_EV_SHUTDOWN	0x0002
#define SMBIOD_EV_FORCE_RECONNECT 0x0003
#define	SMBIOD_EV_DISCONNECT 0x0004
/* 0x0005 available for use */
#define	SMBIOD_EV_NEGOTIATE	0x0006
#define	SMBIOD_EV_SSNSETUP	0x0007

#define	SMBIOD_EV_MASK		0x00ff
#define	SMBIOD_EV_SYNC		0x0100
#define	SMBIOD_EV_PROCESSING	0x0200

struct smbiod_event {
	int	ev_type;
	int	ev_error;
	void *	ev_ident;
	STAILQ_ENTRY(smbiod_event)	ev_link;
};

#define	SMBIOD_SHUTDOWN			0x0001
#define	SMBIOD_RUNNING			0x0002
#define	SMBIOD_RECONNECT		0x0004
#define	SMBIOD_START_RECONNECT	0x0008
#define	SMBIOD_VC_NOTRESP		0x0010

struct smbiod {
    int                 iod_id;
    int                 iod_flags;
    enum smbiod_state   iod_state;
    lck_mtx_t           iod_flagslock;  /* iod_flags */
    /* number of active outstanding requests (keep it signed!) */
    int64_t             iod_muxcnt;
    /* number of active outstanding async requests (keep it signed!) */
    int32_t             iod_asynccnt;	
    struct timespec     iod_sleeptimespec;
    struct smb_vc *     iod_vc;
    lck_mtx_t           iod_rqlock;     /* iod_rqlist, iod_muxwant */
    struct smb_rqhead   iod_rqlist;     /* list of outstanding requests */
    int                 iod_muxwant;
    vfs_context_t       iod_context;
    lck_mtx_t           iod_evlock;     /* iod_evlist */
    STAILQ_HEAD(,smbiod_event) iod_evlist;
    struct timespec     iod_lastrqsent;
    struct timespec     iod_lastrecv;
    int                 iod_workflag;   /* should be protected with lock */
    struct timespec     reconnectStartTime; /* Time when the reconnect was started */
};

int  smb_iod_nb_intr(struct smb_vc *vcp);
int  smb_iod_init(void);
int  smb_iod_done(void);
int smb_vc_force_reconnect(struct smb_vc *vcp);
void smb_vc_reset(struct smb_vc *vcp);
int  smb_iod_create(struct smb_vc *vcp);
int  smb_iod_destroy(struct smbiod *iod);
int  smb_iod_request(struct smbiod *iod, int event, void *ident);
int  smb_iod_rq_enqueue(struct smb_rq *rqp);
int  smb_iod_waitrq(struct smb_rq *rqp);
int  smb_iod_removerq(struct smb_rq *rqp);
void smb_iod_errorout_share_request(struct smb_share *share, int error);

extern lck_grp_attr_t *co_grp_attr;
extern lck_grp_t *co_lck_group;
extern lck_attr_t *co_lck_attr;

extern lck_grp_attr_t *vc_credits_grp_attr;
extern lck_grp_t *vc_credits_lck_group;
extern lck_attr_t *vc_credits_lck_attr;

extern lck_grp_attr_t *vcst_grp_attr;
extern lck_grp_t *vcst_lck_group;
extern lck_attr_t *vcst_lck_attr;

extern lck_grp_attr_t *ssst_grp_attr;
extern lck_grp_t *ssst_lck_group;
extern lck_attr_t *ssst_lck_attr;

extern lck_grp_attr_t *fid_lck_grp_attr;
extern lck_grp_t *fid_lck_grp;
extern lck_attr_t *fid_lck_attr;

extern lck_grp_attr_t *iodflags_grp_attr;
extern lck_grp_t *iodflags_lck_group;
extern lck_attr_t *iodflags_lck_attr;

extern lck_grp_attr_t *iodrq_grp_attr;
extern lck_grp_t *iodrq_lck_group;
extern lck_attr_t *iodrq_lck_attr;

extern lck_grp_attr_t *iodev_grp_attr;
extern lck_grp_t *iodev_lck_group;
extern lck_attr_t *iodev_lck_attr;

extern lck_grp_attr_t *srs_grp_attr;
extern lck_grp_t *srs_lck_group;
extern lck_attr_t *srs_lck_attr;

#endif /* _KERNEL */
#endif // _SMB_CONN_H_
