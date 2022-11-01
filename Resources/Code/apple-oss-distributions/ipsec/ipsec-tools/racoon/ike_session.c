/*
 * Copyright (c) 2008 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "debug.h"
#include "fsm.h"

#include "isakmp_var.h"
#include "isakmp.h"
#include "ike_session.h"
#include "handler.h"
#include "gcmalloc.h"
#include "nattraversal.h"
#include "schedule.h"
#include "pfkey.h"
#include "ipsec_doi.h"
#include "isakmp_inf.h"
#include "localconf.h"
#include "remoteconf.h"
#include "vpn_control.h"
#include "vpn_control_var.h"
#include "proposal.h"
#include "sainfo.h"
#include "power_mgmt.h"

#define GET_SAMPLE_PERIOD(s,m)	do {						\
									s = m / 20;				\
									if (s < 3) {			\
										s = 3;				\
										if (m < (s * 2)) {	\
											s = 1; /* bad */\
										}					\
									}						\
								} while(0);

const char *ike_session_stopped_by_vpn_disconnect = "Stopped by VPN disconnect";
const char *ike_session_stopped_by_controller_comm_lost = "Stopped by loss of controller communication";
const char *ike_session_stopped_by_flush          = "Stopped by Flush";
const char *ike_session_stopped_by_idle           = "Stopped by Idle";
const char *ike_session_stopped_by_xauth_timeout  = "Stopped by XAUTH timeout";
const char *ike_session_stopped_by_sleepwake      = "Stopped by Sleep-Wake";
const char *ike_session_stopped_by_assert         = "Stopped by Assert";
const char *ike_session_stopped_by_peer           = "Stopped by Peer";

LIST_HEAD(_ike_session_tree_, ike_session) ike_session_tree = { NULL };

static void ike_session_bindph12(phase1_handle_t *, phase2_handle_t *);
static void ike_session_rebindph12(phase1_handle_t *, phase2_handle_t *);
static void ike_session_unbind_all_ph2_from_ph1 (phase1_handle_t *);
static void ike_session_rebind_all_ph12_to_new_ph1 (phase1_handle_t *, phase1_handle_t *);

static ike_session_t *
new_ike_session (ike_session_id_t *id)
{
	ike_session_t *session;

	if (!id) {
		plog(ASL_LEVEL_ERR, "Invalid parameters in %s.\n", __FUNCTION__);
		return NULL;
	}
    
	session = racoon_calloc(1, sizeof(*session));
	if (session) {
		bzero(session, sizeof(*session));
		memcpy(&session->session_id, id, sizeof(*id));
		LIST_INIT(&session->ph1tree);
		LIST_INIT(&session->ph2tree);	
		LIST_INSERT_HEAD(&ike_session_tree, session, chain);
	}
	return session;
}

static void
free_ike_session (ike_session_t *session)
{
    int is_failure = TRUE;
	if (session) {
        SCHED_KILL(session->traffic_monitor.sc_mon);
        SCHED_KILL(session->traffic_monitor.sc_idle);
        SCHED_KILL(session->sc_xauth);
		if (session->start_timestamp.tv_sec || session->start_timestamp.tv_usec) {
			if (!(session->stop_timestamp.tv_sec || session->stop_timestamp.tv_usec)) {
				gettimeofday(&session->stop_timestamp, NULL);
			}
            if (session->term_reason != ike_session_stopped_by_vpn_disconnect ||
                session->term_reason != ike_session_stopped_by_controller_comm_lost ||
                session->term_reason != ike_session_stopped_by_flush ||
                session->term_reason != ike_session_stopped_by_idle) {
                is_failure = FALSE;
            }
		}
		// do MessageTracer cleanup here
		plog(ASL_LEVEL_NOTICE,
			 "Freeing IKE-Session to %s.\n",
			 saddr2str((struct sockaddr *)&session->session_id.remote));
		LIST_REMOVE(session, chain);
		racoon_free(session);
	}
}


void
ike_session_init (void)
{
	LIST_INIT(&ike_session_tree);
}

u_int
ike_session_get_rekey_lifetime (int local_spi_is_higher, u_int expiry_lifetime)
{
	u_int rekey_lifetime = expiry_lifetime / 10;

	if (rekey_lifetime) {
		if (local_spi_is_higher) {
			return (rekey_lifetime * 9);
		} else {
			return (rekey_lifetime * 8);
		}
	} else {
		if (local_spi_is_higher) {
			rekey_lifetime = expiry_lifetime - 1;
		} else {
			rekey_lifetime = expiry_lifetime - 2;
		}
	}
	if (rekey_lifetime < expiry_lifetime) {
		return rekey_lifetime;
	}
	return 0;
}

ike_session_t *
ike_session_create_session (ike_session_id_t *session_id)
{
    if (!session_id)
        return NULL;
    
    plog(ASL_LEVEL_NOTICE, "New IKE Session to %s.\n", saddr2str((struct sockaddr *)&session_id->remote));
    
    return new_ike_session(session_id);
}

void
ike_session_release_session (ike_session_t *session)
{
    while (!LIST_EMPTY(&session->ph2tree)) {
        phase2_handle_t *phase2 = LIST_FIRST(&session->ph2tree);
        ike_session_unlink_phase2(phase2);
    }
    
    while (!LIST_EMPTY(&session->ph1tree)) {
        phase1_handle_t *phase1 = LIST_FIRST(&session->ph1tree);
        ike_session_unlink_phase1(phase1);
    }
}

// %%%%%%%%% re-examine this - keep both floated and unfloated port when behind nat
ike_session_t *
ike_session_get_session (struct sockaddr_storage *local,
						 struct sockaddr_storage *remote,
						 int              alloc_if_absent,
						 isakmp_index *optionalIndex)
{
	ike_session_t    *p = NULL;
	ike_session_id_t  id;
	ike_session_id_t  id_default;
	ike_session_id_t  id_floated_default;
	ike_session_id_t  id_wop;
	ike_session_t    *best_match = NULL;
	u_int16_t         remote_port;
	int               is_isakmp_remote_port;

	if (!local || !remote) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return NULL;
	}

	remote_port = extract_port(remote);
	if (remote_port && remote_port != PORT_ISAKMP && remote_port != PORT_ISAKMP_NATT) {
		is_isakmp_remote_port = 0;
	} else {
		is_isakmp_remote_port = 1;
	}

	/* we will try a couple of matches first: if the exact id isn't found, then we'll try for an id that has zero'd ports */
	bzero(&id, sizeof(id));
	bzero(&id_default, sizeof(id_default));
	bzero(&id_floated_default, sizeof(id_floated_default));
	bzero(&id_wop, sizeof(id_wop));
	if (local->ss_family == AF_INET) {
		memcpy(&id.local, local, sizeof(struct sockaddr_in));
		memcpy(&id_default.local, local, sizeof(struct sockaddr_in));
		memcpy(&id_floated_default.local, local, sizeof(struct sockaddr_in));
		memcpy(&id_wop.local, local, sizeof(struct sockaddr_in));
	} else if (local->ss_family == AF_INET6) {
		memcpy(&id.local, local, sizeof(struct sockaddr_in6));
		memcpy(&id_default.local, local, sizeof(struct sockaddr_in6));
		memcpy(&id_floated_default.local, local, sizeof(struct sockaddr_in6));
		memcpy(&id_wop.local, local, sizeof(struct sockaddr_in6));
	}
	set_port(&id_default.local, PORT_ISAKMP);
	set_port(&id_floated_default.local, PORT_ISAKMP_NATT);
	set_port(&id_wop.local, 0);
	if (remote->ss_family == AF_INET) {
		memcpy(&id.remote, remote, sizeof(struct sockaddr_in));
		memcpy(&id_default.remote, remote, sizeof(struct sockaddr_in));
		memcpy(&id_floated_default.remote, remote, sizeof(struct sockaddr_in));
		memcpy(&id_wop.remote, remote, sizeof(struct sockaddr_in));
	} else if (remote->ss_family == AF_INET6) {
		memcpy(&id.remote, remote, sizeof(struct sockaddr_in6));
		memcpy(&id_default.remote, remote, sizeof(struct sockaddr_in6));
		memcpy(&id_floated_default.remote, remote, sizeof(struct sockaddr_in6));
		memcpy(&id_wop.remote, remote, sizeof(struct sockaddr_in6));
	}
	set_port(&id_default.remote, PORT_ISAKMP);
	set_port(&id_floated_default.remote, PORT_ISAKMP_NATT);
	set_port(&id_wop.remote, 0);

	plog(ASL_LEVEL_DEBUG,
		 "start search for IKE-Session. target %s.\n",
		 saddr2str((struct sockaddr *)remote));			

	LIST_FOREACH(p, &ike_session_tree, chain) {
		plog(ASL_LEVEL_DEBUG,
			 "still search for IKE-Session. this %s.\n",
			 saddr2str((struct sockaddr *)&p->session_id.remote));

		// for now: ignore any stopped sessions as they will go down
		if (p->is_dying || p->stopped_by_vpn_controller || p->stop_timestamp.tv_sec || p->stop_timestamp.tv_usec) {
			plog(ASL_LEVEL_DEBUG, "still searching. skipping... session to %s is already stopped, active ph1 %d ph2 %d.\n",
				 saddr2str((struct sockaddr *)&p->session_id.remote),
				 p->ikev1_state.active_ph1cnt, p->ikev1_state.active_ph2cnt);
			continue;
		}

		// Skip if the spi doesn't match
		if (optionalIndex != NULL && ike_session_getph1byindex(p, optionalIndex) == NULL) {
			continue;
		}

		if (memcmp(&p->session_id, &id, sizeof(id)) == 0) {
			plog(ASL_LEVEL_DEBUG,
				 "Pre-existing IKE-Session to %s. case 1.\n",
				 saddr2str((struct sockaddr *)remote));			
			return p;
		} else if (is_isakmp_remote_port && memcmp(&p->session_id, &id_default, sizeof(id_default)) == 0) {
			plog(ASL_LEVEL_DEBUG,
				 "Pre-existing IKE-Session to %s. case 2.\n",
				 saddr2str((struct sockaddr *)remote));	
			return p;
		} else if (is_isakmp_remote_port && p->ports_floated && memcmp(&p->session_id, &id_floated_default, sizeof(id_floated_default)) == 0) {
			plog(ASL_LEVEL_DEBUG,
				 "Pre-existing IKE-Session to %s. case 3.\n",
				 saddr2str((struct sockaddr *)remote));			
			return p;
		} else if (is_isakmp_remote_port && memcmp(&p->session_id, &id_wop, sizeof(id_wop)) == 0) {
			best_match = p;
		} else if (optionalIndex != NULL) {
			// If the SPI did match, this one counts as a best match
			best_match = p;
		}
	}
	if (best_match) {
		plog(ASL_LEVEL_DEBUG,
			 "Best-match IKE-Session to %s.\n",
			 saddr2str((struct sockaddr *)&best_match->session_id.remote));
		return best_match;
	}
	if (alloc_if_absent) {
		plog(ASL_LEVEL_DEBUG,
			 "New IKE-Session to %s.\n",
			 saddr2str((struct sockaddr *)&id.remote));			
		return new_ike_session(&id);
	} else {
		return NULL;
	}
}

void
ike_session_init_traffic_cop_params (phase1_handle_t *iph1)
{
    if (!iph1 ||
        !iph1->rmconf ||
        (!iph1->rmconf->idle_timeout && !iph1->rmconf->dpd_interval)) {
        return;
    }

    if (!iph1->parent_session->traffic_monitor.interv_idle) {
        iph1->parent_session->traffic_monitor.interv_idle = iph1->rmconf->idle_timeout;
    }

    if (!iph1->parent_session->traffic_monitor.dir_idle) {
        iph1->parent_session->traffic_monitor.dir_idle = iph1->rmconf->idle_timeout_dir;
    }

    if (!iph1->parent_session->traffic_monitor.interv_mon) {
        int min_period, max_period, sample_period = 0;

        /* calculate the sampling interval... half the smaller interval */
        if (iph1->rmconf->dpd_interval &&
            (iph1->rmconf->dpd_algo == DPD_ALGO_INBOUND_DETECT ||
             iph1->rmconf->dpd_algo == DPD_ALGO_BLACKHOLE_DETECT)) {
            // when certain types of dpd are enabled
            min_period = MIN(iph1->rmconf->dpd_interval, iph1->rmconf->idle_timeout);
            max_period = MAX(iph1->rmconf->dpd_interval, iph1->rmconf->idle_timeout);
        } else if (iph1->rmconf->idle_timeout) {
            min_period = max_period = iph1->rmconf->idle_timeout;
        } else {
            // DPD_ALGO_DEFAULT is configured and there's no idle timeout... we don't need to monitor traffic
            return;
        }
        if (min_period) {
            GET_SAMPLE_PERIOD(sample_period, min_period);
        } else {
            GET_SAMPLE_PERIOD(sample_period, max_period);
        }
        iph1->parent_session->traffic_monitor.interv_mon = sample_period;
    }
}

void
ike_session_update_mode (phase2_handle_t *iph2)
{
	if (!iph2 || !iph2->parent_session) {
		return;
	}
    if (iph2->phase2_type != PHASE2_TYPE_SA)
        return;
	
	// exit early if we already detected cisco-ipsec
	if (iph2->parent_session->is_cisco_ipsec) {
		return;
	}

	if (iph2->approval) {
		if (!ipsecdoi_any_transportmode(iph2->approval)) {
			// cisco & btmm ipsec are pure tunnel-mode (but cisco ipsec is detected by ph1)
			iph2->parent_session->is_cisco_ipsec = 0;
			iph2->parent_session->is_l2tpvpn_ipsec = 0;
			iph2->parent_session->is_btmm_ipsec = 1;
			return;
		} else if (ipsecdoi_transportmode(iph2->approval)) {
			iph2->parent_session->is_cisco_ipsec = 0;
			iph2->parent_session->is_l2tpvpn_ipsec = 1;
			iph2->parent_session->is_btmm_ipsec = 0;
			return;
		}
	} else if (iph2->proposal) {
		if (!ipsecdoi_any_transportmode(iph2->proposal)) {
			// cisco & btmm ipsec are pure tunnel-mode (but cisco ipsec is detected by ph1)
			iph2->parent_session->is_cisco_ipsec = 0;
			iph2->parent_session->is_l2tpvpn_ipsec = 0;
			iph2->parent_session->is_btmm_ipsec = 1;
			return;
		} else if (ipsecdoi_transportmode(iph2->proposal)) {
			iph2->parent_session->is_cisco_ipsec = 0;
			iph2->parent_session->is_l2tpvpn_ipsec = 1;
			iph2->parent_session->is_btmm_ipsec = 0;
			return;
		}
	}
}

static void
ike_session_cleanup_xauth_timeout (void *arg)
{
    ike_session_t *session = (ike_session_t *)arg;

    SCHED_KILL(session->sc_xauth);
    // if there are no more established ph2s, start a timer to teardown the session
    if (!ike_session_has_established_ph2(session)) {
        ike_session_cleanup(session, ike_session_stopped_by_xauth_timeout);
    } else {
        session->sc_xauth = sched_new(300 /* 5 mins */,
                                      ike_session_cleanup_xauth_timeout,
                                      session);
    }
}

int
ike_session_link_phase1 (ike_session_t *session, phase1_handle_t *iph1)
{
    
	if (!session || !iph1) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return -1;
	}
    
    gettimeofday(&session->start_timestamp, NULL);
	
	if (iph1->started_by_api) {
		session->is_cisco_ipsec = 1;
        session->is_l2tpvpn_ipsec = 0;
        session->is_btmm_ipsec = 0;
	}
	iph1->parent_session = session;
	LIST_INSERT_HEAD(&session->ph1tree, iph1, ph1ofsession_chain);
	session->ikev1_state.active_ph1cnt++;
    if ((!session->ikev1_state.ph1cnt &&
         iph1->side == INITIATOR) ||
        iph1->started_by_api) {
        // client initiates the first phase1 or, is started by controller api
        session->is_client = 1;
    }
	if (session->established &&
		session->ikev1_state.ph1cnt &&
        iph1->version == ISAKMP_VERSION_NUMBER_IKEV1) {
		iph1->is_rekey = 1;
	}
	session->ikev1_state.ph1cnt++;
    ike_session_init_traffic_cop_params(iph1);
    
	return 0;
}

int
ike_session_link_phase2 (ike_session_t *session, phase2_handle_t *iph2)
{
	if (!iph2) {
		plog(ASL_LEVEL_ERR, "Invalid parameters in %s.\n", __FUNCTION__);
		return -1;
	}
    if (iph2->parent_session) {
		plog(ASL_LEVEL_ERR, "Phase 2 already linked to session %s.\n", __FUNCTION__);
	}

	iph2->parent_session = session;
	LIST_INSERT_HEAD(&session->ph2tree, iph2, ph2ofsession_chain);
	session->ikev1_state.active_ph2cnt++;
    if (!session->ikev1_state.ph2cnt &&
        iph2->side == INITIATOR) {
        // client initiates the first phase2
        session->is_client = 1;
    }
	if (iph2->phase2_type == PHASE2_TYPE_SA &&
        session->established &&
		session->ikev1_state.ph2cnt &&
        iph2->version == ISAKMP_VERSION_NUMBER_IKEV1) {
		iph2->is_rekey = 1;
	}
	session->ikev1_state.ph2cnt++;
	ike_session_update_mode(iph2);

	return 0;
}

int
ike_session_link_ph2_to_ph1 (phase1_handle_t *iph1, phase2_handle_t *iph2)
{
    struct sockaddr_storage *local;
	struct sockaddr_storage *remote;
    int error = 0;
    
	if (!iph2) {
		plog(ASL_LEVEL_ERR, "Invalid parameters in %s.\n", __FUNCTION__);
		return -1;
	}
    if (iph2->ph1) {
		plog(ASL_LEVEL_ERR, "Phase 2 already linked %s.\n", __FUNCTION__);	
        if (iph2->ph1 == iph1)
            return 0;
        else 
            return -1;  // This shouldn't happen
	}

    local = iph2->src;
    remote = iph2->dst;
    
    if (iph2->parent_session == NULL)
        if ((error = ike_session_link_phase2(iph1->parent_session, iph2)))
            return error;
	
	ike_session_bindph12(iph1, iph2);
    return 0;
}

int
ike_session_unlink_phase1 (phase1_handle_t *iph1)
{
	ike_session_t *session;
	
	if (!iph1 || !iph1->parent_session) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return -1;
	}

    if (iph1->version == ISAKMP_VERSION_NUMBER_IKEV1) {
        if (LIST_FIRST(&iph1->bound_ph2tree)) {        
            // reparent any phase2 that may be hanging on to this phase1
            ike_session_update_ph1_ph2tree(iph1);
        } 
    }

    sched_scrub_param(iph1);
	session = iph1->parent_session;
	LIST_REMOVE(iph1, ph1ofsession_chain);
	iph1->parent_session = NULL;
	session->ikev1_state.active_ph1cnt--;
	if (session->ikev1_state.active_ph1cnt == 0 && session->ikev1_state.active_ph2cnt == 0) {
		session->is_dying = 1;
		free_ike_session(session);
	}
    ike_session_delph1(iph1);
	return 0;
}

int
ike_session_unlink_phase2 (phase2_handle_t *iph2)
{
	ike_session_t *session;
	
	if (!iph2 || !iph2->parent_session) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return -1;
	}
    sched_scrub_param(iph2);
    ike_session_unbindph12(iph2);
    
    LIST_REMOVE(iph2, ph2ofsession_chain);
    session = iph2->parent_session;
    iph2->parent_session = NULL;
    session->ikev1_state.active_ph2cnt--;
    if (session->ikev1_state.active_ph1cnt == 0 && session->ikev1_state.active_ph2cnt == 0) {
        session->is_dying = 1;
        free_ike_session(session);
    }
	ike_session_delph2(iph2);
	return 0;
}


phase1_handle_t *
ike_session_update_ph1_ph2tree (phase1_handle_t *iph1)
{
	phase1_handle_t *new_iph1 = NULL;

	if (!iph1) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return NULL;
	}

	if (iph1->parent_session) {
		new_iph1 = ike_session_get_established_ph1(iph1->parent_session);

		if (!new_iph1) {
			plog(ASL_LEVEL_NOTICE, "no ph1bind replacement found. NULL ph1.\n");
			ike_session_unbind_all_ph2_from_ph1(iph1);
		} else if (iph1 == new_iph1) {
			plog(ASL_LEVEL_NOTICE, "no ph1bind replacement found. same ph1.\n");
			ike_session_unbind_all_ph2_from_ph1(iph1);
		} else {
			ike_session_rebind_all_ph12_to_new_ph1(iph1, new_iph1);
		}
	} else {
		plog(ASL_LEVEL_NOTICE, "invalid parent session in %s.\n", __FUNCTION__);
	}
	return new_iph1;
}

phase1_handle_t *
ike_session_update_ph2_ph1bind (phase2_handle_t *iph2)
{
	phase1_handle_t *iph1;
	
	if (!iph2 || iph2->phase2_type != PHASE2_TYPE_SA) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return NULL;
	}
	
	iph1 = ike_session_get_established_ph1(iph2->parent_session);
	if (iph1 && iph2->ph1 && iph1 != iph2->ph1) {
		ike_session_rebindph12(iph1, iph2);
	} else if (iph1 && !iph2->ph1) {
		ike_session_bindph12(iph1, iph2);
	}
	
	return iph1;
}

phase1_handle_t *
ike_session_get_established_or_negoing_ph1 (ike_session_t *session)
{
	phase1_handle_t *p, *iph1 = NULL;
    
	if (!session) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return NULL;
	}
    
	// look for the most mature ph1 under the session
	LIST_FOREACH(p, &session->ph1tree, ph1ofsession_chain) {
		if (!p->is_dying && (FSM_STATE_IS_ESTABLISHED(p->status) || FSM_STATE_IS_NEGOTIATING(p->status))) {
			if (!iph1 || p->status > iph1->status) {
				iph1 = p;
			} else if (iph1 && p->status == iph1->status) {
				// TODO: pick better one based on farthest rekey/expiry remaining
			}
		}
	}
    
	return iph1;
}

phase1_handle_t *
ike_session_get_established_ph1 (ike_session_t *session)
{
	phase1_handle_t *p;
    
	if (!session) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return NULL;
	}
    
	LIST_FOREACH(p, &session->ph1tree, ph1ofsession_chain) {
		if (!p->is_dying && FSM_STATE_IS_ESTABLISHED(p->status)) {
            return p;
		}
	}
    
	return NULL;
}


int
ike_session_has_other_established_ph1 (ike_session_t *session, phase1_handle_t *iph1)
{
	phase1_handle_t *p;
    
	if (!session) {
		return 0;
	}
    
	LIST_FOREACH(p, &session->ph1tree, ph1ofsession_chain) {
		if (iph1 != p && !p->is_dying) {
			if (FSM_STATE_IS_ESTABLISHED(p->status) && p->sce_rekey) {
				return 1;
			}
		}
	}
    
	return 0;
}

int
ike_session_has_other_negoing_ph1 (ike_session_t *session, phase1_handle_t *iph1)
{
	phase1_handle_t *p;
	
	if (!session) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return 0;
	}
	
	LIST_FOREACH(p, &session->ph1tree, ph1ofsession_chain) {
		if (iph1 != p && !p->is_dying) {
			if (FSM_STATE_IS_NEGOTIATING(p->status)) {
				return 1;
			}
		}
	}
	
	return 0;
}

int
ike_session_has_other_established_ph2 (ike_session_t *session, phase2_handle_t *iph2)
{
	phase2_handle_t *p;
	
	if (!session) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return 0;
	}
	
	LIST_FOREACH(p, &session->ph2tree, ph2ofsession_chain) {
		if (p->phase2_type == PHASE2_TYPE_SA && iph2 != p && !p->is_dying && iph2->spid == p->spid) {
			if (FSM_STATE_IS_ESTABLISHED(p->status)) {
				return 1;
			}
		}
	}
	
	return 0;
}

int
ike_session_has_other_negoing_ph2 (ike_session_t *session, phase2_handle_t *iph2)
{
	phase2_handle_t *p;
	
	if (!session) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return 0;
	}
	
	LIST_FOREACH(p, &session->ph2tree, ph2ofsession_chain) {
		plog(ASL_LEVEL_DEBUG, "%s: ph2 sub spid %d, db spid %d\n", __FUNCTION__, iph2->spid, p->spid);
		if (p->phase2_type == PHASE2_TYPE_SA && iph2 != p && !p->is_dying && iph2->spid == p->spid) {
			if (FSM_STATE_IS_NEGOTIATING(p->status)) {
				return 1;
			}
		}
	}
	
	return 0;
}


void
ike_session_ikev1_float_ports (phase1_handle_t *iph1)
{
	struct sockaddr_storage  *local, *remote;
	phase2_handle_t *p;

	if (iph1->parent_session) {
		local  = &iph1->parent_session->session_id.local;
		remote = &iph1->parent_session->session_id.remote;

        set_port(local, extract_port(iph1->local));
        set_port(remote, extract_port(iph1->remote));
		iph1->parent_session->ports_floated = 1;

		LIST_FOREACH(p, &iph1->parent_session->ph2tree, ph2ofsession_chain) {

            local  = p->src;
            remote = p->dst;

            set_port(local, extract_port(iph1->local));
            set_port(remote, extract_port(iph1->remote));
		}
	} else {
		plog(ASL_LEVEL_NOTICE, "invalid parent session in %s.\n", __FUNCTION__);
	}
}

static void
ike_session_traffic_cop (void *arg)
{
    ike_session_t *session = (__typeof__(session))arg;
    
    if (session && 
		(session->established && !session->stopped_by_vpn_controller && !session->stop_timestamp.tv_sec && !session->stop_timestamp.tv_usec)) {
        SCHED_KILL(session->traffic_monitor.sc_mon);
        /* get traffic query from kernel */
        if (pk_sendget_inbound_sastats(session) < 0) {
            // log message
            plog(ASL_LEVEL_NOTICE, "pk_sendget_inbound_sastats failed in %s.\n", __FUNCTION__);
        }
        if (pk_sendget_outbound_sastats(session) < 0) {
            // log message
            plog(ASL_LEVEL_NOTICE, "pk_sendget_outbound_sastats failed in %s.\n", __FUNCTION__);
        }
        session->traffic_monitor.sc_mon = sched_new(session->traffic_monitor.interv_mon,
                                                    ike_session_traffic_cop,
                                                    session);
    } else {
        // log message
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
    }
}

static void
ike_session_cleanup_idle (void *arg)
{
    ike_session_cleanup((ike_session_t *)arg, ike_session_stopped_by_idle);
}

static void
ike_session_monitor_idle (ike_session_t *session)
{
	if (!session)
		return;

    if (session->traffic_monitor.dir_idle == IPSEC_DIR_INBOUND ||
        session->traffic_monitor.dir_idle == IPSEC_DIR_ANY) {
        if (session->peer_sent_data_sc_idle) {
			plog(ASL_LEVEL_NOTICE, "%s: restart idle-timeout because peer sent data. monitoring dir %d. idle timer %d s\n",
				 __FUNCTION__, session->traffic_monitor.dir_idle, session->traffic_monitor.interv_idle);
            SCHED_KILL(session->traffic_monitor.sc_idle);
			if (session->traffic_monitor.interv_idle) {
				session->traffic_monitor.sc_idle = sched_new(session->traffic_monitor.interv_idle,
															 ike_session_cleanup_idle,
															 session);
			}
            session->peer_sent_data_sc_idle = 0;
            session->i_sent_data_sc_idle = 0;
            return;
        }
    }
    if (session->traffic_monitor.dir_idle == IPSEC_DIR_OUTBOUND ||
        session->traffic_monitor.dir_idle == IPSEC_DIR_ANY) {
        if (session->i_sent_data_sc_idle) {
			plog(ASL_LEVEL_NOTICE, "%s: restart idle-timeout because i sent data. monitoring dir %d. idle times %d s\n",
				 __FUNCTION__, session->traffic_monitor.dir_idle, session->traffic_monitor.interv_idle);
            SCHED_KILL(session->traffic_monitor.sc_idle);
			if (session->traffic_monitor.interv_idle) {
				session->traffic_monitor.sc_idle = sched_new(session->traffic_monitor.interv_idle,
															 ike_session_cleanup_idle,
															 session);
			}
            session->peer_sent_data_sc_idle = 0;
            session->i_sent_data_sc_idle = 0;
            return;
        }
    }
}

static void
ike_session_start_traffic_mon (ike_session_t *session)
{
	if (session->traffic_monitor.interv_mon) {
		session->traffic_monitor.sc_mon = sched_new(session->traffic_monitor.interv_mon,
															ike_session_traffic_cop,
															session);
	}
	if (session->traffic_monitor.interv_idle) {
		session->traffic_monitor.sc_idle = sched_new(session->traffic_monitor.interv_idle,
															ike_session_cleanup_idle,
															session);
	}
}

void
ike_session_ph2_established (phase2_handle_t *iph2)
{
	if (!iph2->parent_session || iph2->phase2_type != PHASE2_TYPE_SA) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return;
	}
	SCHED_KILL(iph2->parent_session->sc_xauth);
	if (!iph2->parent_session->established) {
		gettimeofday(&iph2->parent_session->estab_timestamp, NULL);
		iph2->parent_session->established = 1;
		ike_session_start_traffic_mon(iph2->parent_session);
	} else if (iph2->parent_session->is_asserted) {
		ike_session_start_traffic_mon(iph2->parent_session);
	}
	iph2->parent_session->is_asserted = 0;
    // nothing happening to this session
    iph2->parent_session->term_reason = NULL;

	ike_session_update_mode(iph2);
    if (iph2->version == ISAKMP_VERSION_NUMBER_IKEV1)
        ike_session_unbindph12(iph2);

#ifdef ENABLE_VPNCONTROL_PORT
	vpncontrol_notify_peer_resp_ph2(1, iph2);
#endif /* ENABLE_VPNCONTROL_PORT */
	plog(ASL_LEVEL_NOTICE, "%s: ph2 established, spid %d\n", __FUNCTION__, iph2->spid);
}

void
ike_session_cleanup_ph1 (phase1_handle_t *iph1)
{
    if (FSM_STATE_IS_EXPIRED(iph1->status)) {
		// since this got here via ike_session_cleanup_other_established_ph1s, assumes LIST_FIRST(&iph1->ph2tree) == NULL
		iph1->sce = sched_new(1, isakmp_ph1delete_stub, iph1);
		return;
    }
    
	/* send delete information */
	if (FSM_STATE_IS_ESTABLISHED(iph1->status)) {
		isakmp_info_send_d1(iph1);
    }
    
    isakmp_ph1expire(iph1);		
}

void
ike_session_cleanup_ph1_stub (void *p)
{
    
	ike_session_cleanup_ph1((phase1_handle_t *)p);
}

void
ike_session_replace_other_ph1 (phase1_handle_t *new_iph1,
                               phase1_handle_t *old_iph1)
{
	char *local, *remote, *index;
    ike_session_t *session = NULL;
    
    if (new_iph1)
        session = new_iph1->parent_session;
    
	if (!session || !new_iph1 || !old_iph1 || session != old_iph1->parent_session || new_iph1 == old_iph1) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return;
	}
    
	/*
	 * if we are responder, then we should wait until the server sends a delete notification.
	 */
	if (session->is_client &&
		new_iph1->side == RESPONDER) {
		return;
	}
    
    SCHED_KILL(old_iph1->sce);
    SCHED_KILL(old_iph1->sce_rekey);
    old_iph1->is_dying = 1;
    
    //log deletion
    local  = racoon_strdup(saddr2str((struct sockaddr *)old_iph1->local));
    remote = racoon_strdup(saddr2str((struct sockaddr *)old_iph1->remote));
    index = racoon_strdup(isakmp_pindex(&old_iph1->index, 0));
    STRDUP_FATAL(local);
    STRDUP_FATAL(remote);
    STRDUP_FATAL(index);
    plog(ASL_LEVEL_NOTICE, "ISAKMP-SA %s-%s (spi:%s) needs to be deleted, replaced by (spi:%s)\n", local, remote, index, isakmp_pindex(&new_iph1->index, 0));
    racoon_free(local);
    racoon_free(remote);
    racoon_free(index);
    
    // first rebind the children ph2s of this dying ph1 to the new ph1.
    ike_session_rebind_all_ph12_to_new_ph1 (old_iph1, new_iph1);
    
    if (old_iph1->side == INITIATOR) {
        /* everyone deletes old outbound SA */
        old_iph1->sce = sched_new(5, ike_session_cleanup_ph1_stub, old_iph1);
    } else {
        /* responder sets up timer to delete old inbound SAs... say 7 secs later and flags them as rekeyed */
        old_iph1->sce = sched_new(7, ike_session_cleanup_ph1_stub, old_iph1);
    }
}

void
ike_session_cleanup_other_established_ph1s (ike_session_t    *session,
											phase1_handle_t *new_iph1)
{
	phase1_handle_t *p, *next;
	char             *local, *remote;

	if (!session || !new_iph1 || session != new_iph1->parent_session) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return;
	}

	/*
	 * if we are responder, then we should wait until the server sends a delete notification.
	 */
	if (session->is_client &&
		new_iph1->side == RESPONDER) {
		return;
	}

	LIST_FOREACH_SAFE(p, &session->ph1tree, ph1ofsession_chain, next) {
		/*
		 * TODO: currently, most recently established SA wins. Need to revisit to see if 
		 * alternative selections is better (e.g. largest p->index stays).
		 */
		if (p != new_iph1 && !p->is_dying) {
			SCHED_KILL(p->sce);
			SCHED_KILL(p->sce_rekey);
			p->is_dying = 1;

			//log deletion
			local  = racoon_strdup(saddr2str((struct sockaddr *)p->local));
			remote = racoon_strdup(saddr2str((struct sockaddr *)p->remote));
			STRDUP_FATAL(local);
			STRDUP_FATAL(remote);
			plog(ASL_LEVEL_NOTICE,
				 "ISAKMP-SA needs to be deleted %s-%s spi:%s\n",
				 local, remote, isakmp_pindex(&p->index, 0));
			racoon_free(local);
			racoon_free(remote);

            // first rebind the children ph2s of this dying ph1 to the new ph1.
            ike_session_rebind_all_ph12_to_new_ph1 (p, new_iph1);

			if (p->side == INITIATOR) {
				/* everyone deletes old outbound SA */
				p->sce = sched_new(5, ike_session_cleanup_ph1_stub, p);
			} else {
				/* responder sets up timer to delete old inbound SAs... say 7 secs later and flags them as rekeyed */
				p->sce = sched_new(7, ike_session_cleanup_ph1_stub, p);
			}
		}
	}
}

void
ike_session_cleanup_ph2 (phase2_handle_t *iph2)
{
    if (iph2->phase2_type != PHASE2_TYPE_SA)
        return;
	if (FSM_STATE_IS_EXPIRED(iph2->status)) {
		return;
	}

	SCHED_KILL(iph2->sce);

	plog(ASL_LEVEL_ERR,
		 "about to cleanup ph2: status %d, seq %d dying %d\n",
		 iph2->status, iph2->seq, iph2->is_dying);

	/* send delete information */
	if (FSM_STATE_IS_ESTABLISHED(iph2->status)) {
		isakmp_info_send_d2(iph2);
    
		// delete outgoing SAs
		if (iph2->approval) {
			struct saproto *pr;

			for (pr = iph2->approval->head; pr != NULL; pr = pr->next) {
				if (pr->ok) {
					pfkey_send_delete(lcconf->sock_pfkey,
                                  ipsecdoi2pfkey_proto(pr->proto_id),
                                  IPSEC_MODE_ANY,
                                  iph2->src, iph2->dst, pr->spi_p /* pr->reqid_out */);
				}
			}
		}
	}
    
	delete_spd(iph2);
	ike_session_unlink_phase2(iph2);
}

void
ike_session_cleanup_ph2_stub (void *p)
{
    
	ike_session_cleanup_ph2((phase2_handle_t *)p);
}

void
ike_session_cleanup_other_established_ph2s (ike_session_t    *session,
											phase2_handle_t *new_iph2)
{
	phase2_handle_t *p, *next;

	if (!session || !new_iph2 || session != new_iph2->parent_session || new_iph2->phase2_type != PHASE2_TYPE_SA) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return;
	}

	/*
	 * if we are responder, then we should wait until the server sends a delete notification.
	 */
	if (session->is_client && new_iph2->side == RESPONDER) {
		return;
	}

	LIST_FOREACH_SAFE(p, &session->ph2tree, ph2ofsession_chain, next) {
		/*
		 * TODO: currently, most recently established SA wins. Need to revisit to see if 
		 * alternative selections is better.
		 */
		if (p != new_iph2 && p->spid == new_iph2->spid && !p->is_dying) {
			SCHED_KILL(p->sce);
			p->is_dying = 1;
			
			//log deletion
			plog(ASL_LEVEL_NOTICE,
				 "IPsec-SA needs to be deleted: %s\n",
				 sadbsecas2str(p->src, p->dst,
							   p->satype, p->spid, 0));

			if (p->side == INITIATOR) {
				/* responder sets up timer to delete old inbound SAs... say 5 secs later and flags them as rekeyed */
				p->sce = sched_new(3, ike_session_cleanup_ph2_stub, p);
			} else {
				/* responder sets up timer to delete old inbound SAs... say 5 secs later and flags them as rekeyed */
				p->sce = sched_new(5, ike_session_cleanup_ph2_stub, p);
			}
		}
	}
}

void
ike_session_stopped_by_controller (ike_session_t *session,
								   const char    *reason)
{	
	if (!session) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return;
	}
	if (session->stop_timestamp.tv_sec ||
		session->stop_timestamp.tv_usec) {
		plog(ASL_LEVEL_NOTICE, "already stopped %s.\n", __FUNCTION__);
		return;
	}
	session->stopped_by_vpn_controller = 1;
	gettimeofday(&session->stop_timestamp, NULL);
	if (!session->term_reason) {
		session->term_reason = (__typeof__(session->term_reason))reason;
	}
}

void
ike_sessions_stopped_by_controller (struct sockaddr_storage *remote,
                                    int              withport,
								    const char      *reason)
{
	ike_session_t *p = NULL;
	ike_session_t *next_session = NULL;

	if (!remote) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return;
	}

	LIST_FOREACH_SAFE(p, &ike_session_tree, chain, next_session) {
        if ((withport && cmpsaddrstrict(&p->session_id.remote, remote) == 0) ||
            (!withport && cmpsaddrwop(&p->session_id.remote, remote) == 0)) {
                ike_session_stopped_by_controller(p, reason);
		}
	}
}

void
ike_session_purge_ph1s_by_session (ike_session_t *session)
{
	phase1_handle_t *iph1;
	phase1_handle_t *next_iph1 = NULL;

	LIST_FOREACH_SAFE(iph1, &session->ph1tree, ph1ofsession_chain, next_iph1) {
		plog(ASL_LEVEL_NOTICE, "deleteallph1 of given session: got a ph1 handler...\n");

		vpncontrol_notify_ike_failed(VPNCTL_NTYPE_NO_PROPOSAL_CHOSEN, FROM_REMOTE,
					    iph1_get_remote_v4_address(iph1), 0, NULL);

		ike_session_unlink_phase1(iph1);
	}
}

void
ike_session_purge_ph2s_by_ph1 (phase1_handle_t *iph1)
{
	phase2_handle_t *p, *next;

	if (!iph1 || !iph1->parent_session) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return;
	}

	LIST_FOREACH_SAFE(p, &iph1->parent_session->ph2tree, ph2ofsession_chain, next) {
		if (p->is_dying) {
			continue;
		}
        SCHED_KILL(p->sce);
        p->is_dying = 1;
			
        //log deletion
        plog(ASL_LEVEL_NOTICE,
             "IPsec-SA needs to be purged: %s\n",
             sadbsecas2str(p->src, p->dst,
                           p->satype, p->spid, 0));

        ike_session_cleanup_ph2(p);
	}
}

void
ike_session_update_ph2_ports (phase2_handle_t *iph2)
{
    struct sockaddr_storage *local;
    struct sockaddr_storage *remote;
    
	if (iph2->parent_session) {
		local  = &iph2->parent_session->session_id.local;
		remote = &iph2->parent_session->session_id.remote;
        
        set_port(iph2->src, extract_port(local));
        set_port(iph2->dst, extract_port(remote));
	} else {
		plog(ASL_LEVEL_NOTICE, "invalid parent session in %s.\n", __FUNCTION__);
	}
}

u_int32_t
ike_session_get_sas_for_stats (ike_session_t *session,
                               u_int8_t       dir,
                               u_int32_t     *seq,
                               struct sastat *stats,
                               u_int32_t      max_stats)
{
    int               found = 0;
	phase2_handle_t *iph2;

    if (!session || !seq || !stats || !max_stats || (dir != IPSEC_DIR_INBOUND && dir != IPSEC_DIR_OUTBOUND)) {
		plog(ASL_LEVEL_ERR, "invalid args in %s.\n", __FUNCTION__);
        return found;
    }

    *seq = 0;
    LIST_FOREACH(iph2, &session->ph2tree, ph2ofsession_chain) {
        if (iph2->approval) {
            struct saproto *pr;

            for (pr = iph2->approval->head; pr != NULL; pr = pr->next) {
                if (pr->ok && pr->proto_id == IPSECDOI_PROTO_IPSEC_ESP) {
                    if (!*seq) {
                        *seq = iph2->seq;
                    }
                    if (dir == IPSEC_DIR_INBOUND) {
                        stats[found].spi = pr->spi;
                    } else {
                        stats[found].spi = pr->spi_p;
                    }
                    if (++found == max_stats) {
                        return found;
                    }
                }
            }
        }
    }
    return found;
}

void
ike_session_update_traffic_idle_status (ike_session_t *session,
                                        u_int32_t      dir,
                                        struct sastat *new_stats,
                                        u_int32_t      max_stats)
{
    int i, j, found = 0, idle = 1;

    if (!session || !new_stats || (dir != IPSEC_DIR_INBOUND && dir != IPSEC_DIR_OUTBOUND)) {
		plog(ASL_LEVEL_ERR, "invalid args in %s.\n", __FUNCTION__);
        return;
    }

    if (!session->established || session->stopped_by_vpn_controller || session->stop_timestamp.tv_sec || session->stop_timestamp.tv_usec) {
        plog(ASL_LEVEL_NOTICE, "dropping update on invalid session in %s.\n", __FUNCTION__);
        return;
    }

    for (i = 0; i < max_stats; i++) {
        if (dir == IPSEC_DIR_INBOUND) {
            for (j = 0; j < session->traffic_monitor.num_in_last_poll; j++) {
                if (new_stats[i].spi != session->traffic_monitor.in_last_poll[j].spi) {
                    continue;
                }
                found = 1;
                if (new_stats[i].lft_c.sadb_lifetime_bytes != session->traffic_monitor.in_last_poll[j].lft_c.sadb_lifetime_bytes) {
                    idle = 0;
                }
            }
        } else {
            for (j = 0; j < session->traffic_monitor.num_out_last_poll; j++) {
                if (new_stats[i].spi != session->traffic_monitor.out_last_poll[j].spi) {
                    continue;
                }
                found = 1;
                if (new_stats[i].lft_c.sadb_lifetime_bytes != session->traffic_monitor.out_last_poll[j].lft_c.sadb_lifetime_bytes) {
                    idle = 0;
                }
            }
        }
        // new SA.... check for any activity
        if (!found) {
            if (new_stats[i].lft_c.sadb_lifetime_bytes) {
                plog(ASL_LEVEL_NOTICE, "new SA: dir %d....\n", dir);
                idle = 0;
            }
        }
    }
    if (dir == IPSEC_DIR_INBOUND) {
        // overwrite old stats
        bzero(session->traffic_monitor.in_last_poll, sizeof(session->traffic_monitor.in_last_poll));
        bcopy(new_stats, session->traffic_monitor.in_last_poll, (max_stats * sizeof(*new_stats)));
        session->traffic_monitor.num_in_last_poll = max_stats;
        if (!idle) {
            //plog(ASL_LEVEL_DEBUG, "peer sent data....\n");
            session->peer_sent_data_sc_dpd = 1;
            session->peer_sent_data_sc_idle = 1;
        }
    } else {
        // overwrite old stats
        bzero(session->traffic_monitor.out_last_poll, sizeof(session->traffic_monitor.out_last_poll));
        bcopy(new_stats, session->traffic_monitor.out_last_poll, (max_stats * sizeof(*new_stats)));
        session->traffic_monitor.num_out_last_poll = max_stats;
        if (!idle) {
            //plog(ASL_LEVEL_DEBUG, "i sent data....\n");
            session->i_sent_data_sc_dpd = 1;
            session->i_sent_data_sc_idle = 1;
        }
    }
	if (!idle)
		session->last_time_data_sc_detected = time(NULL);                                                                               

	ike_session_monitor_idle(session);
}

void
ike_session_cleanup (ike_session_t *session,
                     const char    *reason)
{
    phase2_handle_t *iph2 = NULL;
    phase2_handle_t *next_iph2 = NULL;
    phase1_handle_t *iph1 = NULL;
    phase1_handle_t *next_iph1 = NULL;
    nw_nat64_prefix_t nat64_prefix;

    if (!session)
        return;

    memset(&nat64_prefix, 0, sizeof(nat64_prefix));
    session->is_dying = 1;
	ike_session_stopped_by_controller(session, reason);

	SCHED_KILL(session->traffic_monitor.sc_idle);
    // do ph2's first... we need the ph1s for notifications
    LIST_FOREACH_SAFE(iph2, &session->ph2tree, ph2ofsession_chain, next_iph2) {
        if (FSM_STATE_IS_ESTABLISHED(iph2->status)) {
            isakmp_info_send_d2(iph2);
        }
        isakmp_ph2expire(iph2); // iph2 will go down 1 second later.
    }

    // do the ph1s last.
    LIST_FOREACH_SAFE(iph1, &session->ph1tree, ph1ofsession_chain, next_iph1) {

        if (iph1->nat64_prefix.length > 0) {
            memcpy(&nat64_prefix, &iph1->nat64_prefix, sizeof(nat64_prefix));
        }

        if (FSM_STATE_IS_ESTABLISHED(iph1->status)) {
            isakmp_info_send_d1(iph1);
        }
        isakmp_ph1expire(iph1);
    }

    // send ipsecManager a notification
    if (session->is_cisco_ipsec && reason && reason != ike_session_stopped_by_vpn_disconnect
            && reason != ike_session_stopped_by_controller_comm_lost) {
        u_int32_t address = 0;
        if ((&session->session_id.remote)->ss_family == AF_INET) {
            address = ((struct sockaddr_in *)&session->session_id.remote)->sin_addr.s_addr;
        } else {
            if (nat64_prefix.length > 0) {
                struct in_addr inaddr;
                nw_nat64_extract_v4(&nat64_prefix,
                                    &((struct sockaddr_in6 *)&session->session_id.remote)->sin6_addr,
                                    &inaddr);
                address = inaddr.s_addr;
            }
        }
        // TODO: log
        if (reason == ike_session_stopped_by_idle) {
            (void)vpncontrol_notify_ike_failed(VPNCTL_NTYPE_IDLE_TIMEOUT, FROM_LOCAL, address, 0, NULL);
        } else {
            (void)vpncontrol_notify_ike_failed(VPNCTL_NTYPE_INTERNAL_ERROR, FROM_LOCAL, address, 0, NULL);
        }
    }
}

int
ike_session_has_negoing_ph1 (ike_session_t *session)
{
	phase1_handle_t *p;
    
	if (!session) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return 0;
	}
    
	LIST_FOREACH(p, &session->ph1tree, ph1ofsession_chain) {
		if (!p->is_dying && FSM_STATE_IS_NEGOTIATING(p->status)) {
			return 1;
		}
	}
    
	return 0;
}

int
ike_session_has_established_ph1 (ike_session_t *session)
{
	phase1_handle_t *p;
    
	if (!session) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return 0;
	}
    
	LIST_FOREACH(p, &session->ph1tree, ph1ofsession_chain) {
		if (!p->is_dying && FSM_STATE_IS_ESTABLISHED(p->status)) {
			return 1;
		}
	}
    
	return 0;
}

int
ike_session_has_negoing_ph2 (ike_session_t *session)
{
	phase2_handle_t *p;

	if (!session) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return 0;
	}

	LIST_FOREACH(p, &session->ph2tree, ph2ofsession_chain) {
		if (!p->is_dying && FSM_STATE_IS_NEGOTIATING(p->status)) {
            return 1;
		}
	}

	return 0;
}

int
ike_session_has_established_ph2 (ike_session_t *session)
{
	phase2_handle_t *p;
    
	if (!session) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return 0;
	}
    
	LIST_FOREACH(p, &session->ph2tree, ph2ofsession_chain) {
		if (!p->is_dying && FSM_STATE_IS_ESTABLISHED(p->status)) {
            return 1;
		}
	}
    
	return 0;
}

void
ike_session_cleanup_ph1s_by_ph2 (phase2_handle_t *iph2)
{
	phase1_handle_t *iph1 = NULL;
	phase1_handle_t *next_iph1 = NULL;
	
	if (!iph2 || !iph2->parent_session) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return;
	}

	// phase1 is no longer useful
	LIST_FOREACH_SAFE(iph1, &iph2->parent_session->ph1tree, ph1ofsession_chain, next_iph1) {
		if (FSM_STATE_IS_ESTABLISHED(iph1->status)) {
			isakmp_info_send_d1(iph1);
		}
		isakmp_ph1expire(iph1);
	}
}

int
ike_session_is_client_ph2_rekey (phase2_handle_t *iph2)
{
    if (iph2->parent_session &&
        iph2->parent_session->is_client &&
        iph2->is_rekey &&
        iph2->parent_session->is_cisco_ipsec) {
        return 1;
    }
    return 0;
}

int
ike_session_is_client_ph1_rekey (phase1_handle_t *iph1)
{
    if (iph1->parent_session &&
        iph1->parent_session->is_client &&
        iph1->is_rekey &&
        iph1->parent_session->is_cisco_ipsec) {
        return 1;
    }
    return 0;
}

int
ike_session_is_client_ph1 (phase1_handle_t *iph1)
{
	if (iph1->parent_session &&
		iph1->parent_session->is_client) {
		return 1;
	}
	return 0;
}

int
ike_session_is_client_ph2 (phase2_handle_t *iph2)
{
	if (iph2->parent_session &&
		iph2->parent_session->is_client) {
		return 1;
	}
	return 0;	
}

void
ike_session_start_xauth_timer (phase1_handle_t *iph1)
{
    // if there are no more established ph2s, start a timer to teardown the session
    if (iph1->parent_session &&
        iph1->parent_session->is_client &&
        iph1->parent_session->is_cisco_ipsec &&
        !iph1->parent_session->sc_xauth) {
        iph1->parent_session->sc_xauth = sched_new(300 /* 5 mins */,
                                                   ike_session_cleanup_xauth_timeout,
                                                   iph1->parent_session);
    }
}

void
ike_session_stop_xauth_timer (phase1_handle_t *iph1)
{
    if (iph1->parent_session) {
        SCHED_KILL(iph1->parent_session->sc_xauth);
    }
}

static int
ike_session_is_id_ipany (vchar_t *ext_id)
{
	struct id {
		u_int8_t type;		/* ID Type */
		u_int8_t proto_id;	/* Protocol ID */
		u_int16_t port;		/* Port */
		u_int32_t addr;		/* IPv4 address */
		u_int32_t mask;
	} *id_ptr;
	
	/* ignore protocol and port */
	id_ptr = ALIGNED_CAST(struct id *)ext_id->v;
	if (id_ptr->type == IPSECDOI_ID_IPV4_ADDR &&
	    id_ptr->addr == 0) {
		return 1;
	} else if (id_ptr->type == IPSECDOI_ID_IPV4_ADDR_SUBNET &&
			   id_ptr->mask == 0 &&
			   id_ptr->addr == 0) {
		return 1;
	}
	plog(ASL_LEVEL_DEBUG, "not ipany_ids in %s: type %d, addr %x, mask %x.\n",
		 __FUNCTION__, id_ptr->type, id_ptr->addr, id_ptr->mask);
	return 0;
}

static int
ike_session_is_id_portany (vchar_t *ext_id)
{
	struct id {
		u_int8_t type;		/* ID Type */
		u_int8_t proto_id;	/* Protocol ID */
		u_int16_t port;		/* Port */
		u_int32_t addr;		/* IPv4 address */
		u_int32_t mask;
	} *id_ptr;
	
	/* ignore addr */
	id_ptr = ALIGNED_CAST(struct id *)ext_id->v;
	if (id_ptr->type == IPSECDOI_ID_IPV4_ADDR &&
	    id_ptr->port == 0) {
		return 1;
	}
	plog(ASL_LEVEL_DEBUG, "not portany_ids in %s: type %d, port %x.\n",
		 __FUNCTION__, id_ptr->type, id_ptr->port);
	return 0;
}

static void
ike_session_set_id_portany (vchar_t *ext_id)
{
	struct id {
		u_int8_t type;		/* ID Type */
		u_int8_t proto_id;	/* Protocol ID */
		u_int16_t port;		/* Port */
		u_int32_t addr;		/* IPv4 address */
		u_int32_t mask;
	} *id_ptr;
	
	/* ignore addr */
	id_ptr = ALIGNED_CAST(struct id *)ext_id->v;
	if (id_ptr->type == IPSECDOI_ID_IPV4_ADDR) {
	    id_ptr->port = 0;
		return;
	}
}

static int
ike_session_cmp_ph2_ids_ipany (vchar_t *ext_id,
							   vchar_t *ext_id_p)
{
	if (ike_session_is_id_ipany(ext_id) &&
	    ike_session_is_id_ipany(ext_id_p)) {
		return 1;
	}
	return 0;
}

/*
 * ipsec rekeys for l2tp-over-ipsec fail particularly when client is behind nat because the client's configs and policies don't 
 * match the server's view of the client's address and port.
 * servers behave differently when using this address-port info to generate ids during phase2 rekeys, so try to match the incoming id to 
 * a variety of info saved in the older phase2.
 */
int
ike_session_cmp_ph2_ids (phase2_handle_t *iph2,
						 phase2_handle_t *older_ph2)
{
	vchar_t *portany_id = NULL;
	vchar_t *portany_id_p = NULL;

	if (iph2->id && older_ph2->id &&
	    iph2->id->l == older_ph2->id->l &&
	    memcmp(iph2->id->v, older_ph2->id->v, iph2->id->l) == 0 &&
	    iph2->id_p && older_ph2->id_p &&
	    iph2->id_p->l == older_ph2->id_p->l &&
	    memcmp(iph2->id_p->v, older_ph2->id_p->v, iph2->id_p->l) == 0) {
		return 0;
	}
	if (iph2->ext_nat_id && older_ph2->ext_nat_id &&
	    iph2->ext_nat_id->l == older_ph2->ext_nat_id->l &&
	    memcmp(iph2->ext_nat_id->v, older_ph2->ext_nat_id->v, iph2->ext_nat_id->l) == 0 &&
	    iph2->ext_nat_id_p && older_ph2->ext_nat_id_p &&
	    iph2->ext_nat_id_p->l == older_ph2->ext_nat_id_p->l &&
	    memcmp(iph2->ext_nat_id_p->v, older_ph2->ext_nat_id_p->v, iph2->ext_nat_id_p->l) == 0) {
		return 0;
	}
	if (iph2->id && older_ph2->ext_nat_id &&
	    iph2->id->l == older_ph2->ext_nat_id->l &&
	    memcmp(iph2->id->v, older_ph2->ext_nat_id->v, iph2->id->l) == 0 &&
	    iph2->id_p && older_ph2->ext_nat_id_p &&
	    iph2->id_p->l == older_ph2->ext_nat_id_p->l &&
	    memcmp(iph2->id_p->v, older_ph2->ext_nat_id_p->v, iph2->id_p->l) == 0) {
		return 0;
	}
	if (iph2->id && older_ph2->ext_nat_id &&
	    iph2->id->l == older_ph2->ext_nat_id->l &&
	    memcmp(iph2->id->v, older_ph2->ext_nat_id->v, iph2->id->l) == 0 &&
	    iph2->id_p && older_ph2->id_p &&
	    iph2->id_p->l == older_ph2->id_p->l &&
	    memcmp(iph2->id_p->v, older_ph2->id_p->v, iph2->id_p->l) == 0) {
		return 0;
	}
	if (iph2->id && older_ph2->id &&
	    iph2->id->l == older_ph2->id->l &&
	    memcmp(iph2->id->v, older_ph2->id->v, iph2->id->l) == 0 &&
	    iph2->id_p && older_ph2->ext_nat_id_p &&
	    iph2->id_p->l == older_ph2->ext_nat_id_p->l &&
	    memcmp(iph2->id_p->v, older_ph2->ext_nat_id_p->v, iph2->id_p->l) == 0) {
		return 0;
	}

	/* check if the external id has a wildcard port and compare ids accordingly */
	if ((older_ph2->ext_nat_id && ike_session_is_id_portany(older_ph2->ext_nat_id)) ||
		(older_ph2->ext_nat_id_p && ike_session_is_id_portany(older_ph2->ext_nat_id_p))) {
		// try ignoring ports in iph2->id and iph2->id
		if (iph2->id && (portany_id = vdup(iph2->id))) {			
			ike_session_set_id_portany(portany_id);
		}
		if (iph2->id_p && (portany_id_p = vdup(iph2->id_p))) {
			ike_session_set_id_portany(portany_id_p);
		}
		if (portany_id && older_ph2->ext_nat_id &&
			portany_id->l == older_ph2->ext_nat_id->l &&
			memcmp(portany_id->v, older_ph2->ext_nat_id->v, portany_id->l) == 0 &&
			portany_id_p && older_ph2->ext_nat_id_p &&
			portany_id_p->l == older_ph2->ext_nat_id_p->l &&
			memcmp(portany_id_p->v, older_ph2->ext_nat_id_p->v, portany_id_p->l) == 0) {
			if (portany_id) {
				vfree(portany_id);
			}
			if (portany_id_p) {
				vfree(portany_id_p);
			}
			return 0;
		}
		if (portany_id && iph2->id && older_ph2->ext_nat_id &&
			iph2->id->l == older_ph2->ext_nat_id->l &&
			memcmp(portany_id->v, older_ph2->ext_nat_id->v, portany_id->l) == 0 &&
			iph2->id_p && older_ph2->id_p &&
			iph2->id_p->l == older_ph2->id_p->l &&
			memcmp(iph2->id_p->v, older_ph2->id_p->v, iph2->id_p->l) == 0) {
			if (portany_id) {
				vfree(portany_id);
			}
			if (portany_id_p) {
				vfree(portany_id_p);
			}
			return 0;
		}
		if (portany_id_p && iph2->id && older_ph2->id &&
			iph2->id->l == older_ph2->id->l &&
			memcmp(iph2->id->v, older_ph2->id->v, iph2->id->l) == 0 &&
			iph2->id_p && older_ph2->ext_nat_id_p &&
			iph2->id_p->l == older_ph2->ext_nat_id_p->l &&
			memcmp(portany_id_p->v, older_ph2->ext_nat_id_p->v, portany_id_p->l) == 0) {
			if (portany_id) {
				vfree(portany_id);
			}
			if (portany_id_p) {
				vfree(portany_id_p);
			}
			return 0;
		}
		if (portany_id) {
			vfree(portany_id);
		}
		if (portany_id_p) {
			vfree(portany_id_p);
		}
	}
	return -1;
}

int
ike_session_get_sainfo_r (phase2_handle_t *iph2)
{
    if (iph2->parent_session &&
        iph2->parent_session->is_client &&
        iph2->id && iph2->id_p) {
        phase2_handle_t *p;
        int ipany_ids = ike_session_cmp_ph2_ids_ipany(iph2->id, iph2->id_p);
        plog(ASL_LEVEL_DEBUG, "ipany_ids %d in %s.\n", ipany_ids, __FUNCTION__);
        
        LIST_FOREACH(p, &iph2->parent_session->ph2tree, ph2ofsession_chain) {
            if (iph2 != p && !p->is_dying && FSM_STATE_IS_ESTABLISHED_OR_EXPIRED(p->status) && p->sainfo) {
                plog(ASL_LEVEL_DEBUG, "candidate ph2 found in %s.\n", __FUNCTION__);
                if (ipany_ids ||
                    ike_session_cmp_ph2_ids(iph2, p) == 0) {
                    plog(ASL_LEVEL_DEBUG, "candidate ph2 matched in %s.\n", __FUNCTION__);
                    iph2->sainfo = p->sainfo;
                    if (iph2->sainfo)
                        retain_sainfo(iph2->sainfo);
                    if (!iph2->spid) {
                        iph2->spid = p->spid;
                    } else {
                        plog(ASL_LEVEL_DEBUG, "%s: pre-assigned spid %d.\n", __FUNCTION__, iph2->spid);
                    }
                    if (p->ext_nat_id) {
                        if (iph2->ext_nat_id) {
                            vfree(iph2->ext_nat_id);
                        }
                        iph2->ext_nat_id = vdup(p->ext_nat_id);
                    }
                    if (p->ext_nat_id_p) {
                        if (iph2->ext_nat_id_p) {
                            vfree(iph2->ext_nat_id_p);
                        }
                        iph2->ext_nat_id_p = vdup(p->ext_nat_id_p);
                    }
                    return 0;
                }
            }
        }
    }
    return -1;
}

int
ike_session_get_proposal_r (phase2_handle_t *iph2)
{
	if (iph2->parent_session &&
	    iph2->parent_session->is_client &&
	    iph2->id && iph2->id_p) {
		phase2_handle_t *p;
		int ipany_ids = ike_session_cmp_ph2_ids_ipany(iph2->id, iph2->id_p);
		plog(ASL_LEVEL_DEBUG, "ipany_ids %d in %s.\n", ipany_ids, __FUNCTION__);

		LIST_FOREACH(p, &iph2->parent_session->ph2tree, ph2ofsession_chain) {
			if (iph2 != p && !p->is_dying && FSM_STATE_IS_ESTABLISHED_OR_EXPIRED(p->status) &&
			    p->approval) {
				plog(ASL_LEVEL_DEBUG, "candidate ph2 found in %s.\n", __FUNCTION__);
				if (ipany_ids ||
				    ike_session_cmp_ph2_ids(iph2, p) == 0) {
					plog(ASL_LEVEL_DEBUG, "candidate ph2 matched in %s.\n", __FUNCTION__);
					iph2->proposal = dupsaprop(p->approval, 1);
					if (!iph2->spid) {
						iph2->spid = p->spid;
					} else {
						plog(ASL_LEVEL_DEBUG, "%s: pre-assigned spid %d.\n", __FUNCTION__, iph2->spid);
					}
					return 0;
				}
			}
		}
	}
	return -1;
}

void
ike_session_update_natt_version (phase1_handle_t *iph1)
{
	if (iph1->parent_session) {
		if (iph1->natt_options) {
			iph1->parent_session->natt_version = iph1->natt_options->version;
		} else {
			iph1->parent_session->natt_version = 0;
		}
	}
}

int
ike_session_get_natt_version (phase1_handle_t *iph1)
{
	if (iph1->parent_session) {
		return(iph1->parent_session->natt_version);
	}
	return 0;
}

int
ike_session_drop_rekey (ike_session_t *session, ike_session_rekey_type_t rekey_type)
{
	if (session) {
		if (session->is_btmm_ipsec &&
			session->last_time_data_sc_detected &&
			session->traffic_monitor.interv_mon &&
			session->traffic_monitor.interv_idle) {
			// for btmm: drop ph1/ph2 rekey if session is idle
			time_t now = time(NULL);

			if ((now - session->last_time_data_sc_detected) > (session->traffic_monitor.interv_mon << 1)) {
				plog(ASL_LEVEL_NOTICE, "btmm session is idle: drop ph%drekey.\n",
					 rekey_type);
				return 1;
			}
		} else if (!session->is_btmm_ipsec) {
			if (rekey_type == IKE_SESSION_REKEY_TYPE_PH1 &&
				!ike_session_has_negoing_ph2(session) && !ike_session_has_established_ph2(session)) {
				// for vpn: only drop ph1 if there are no more ph2s.
				plog(ASL_LEVEL_NOTICE, "vpn session is idle: drop ph1 rekey.\n");
				return 1;
			}
		}
	}
	return 0;
}

/*
 * this is called after racooon receives a 'kIOMessageSystemHasPoweredOn'
 * a lot is done to make sure that we don't sweep a session that's already been asserted.
 * however, it'll be too bad if the assertion comes after the session has already been swept.
 */
void
ike_session_sweep_sleepwake (void)
{
	ike_session_t *p = NULL;
	ike_session_t *next_session = NULL;

	// flag session as dying if all ph1/ph2 are dead/dying
	LIST_FOREACH_SAFE(p, &ike_session_tree, chain, next_session) {
		if (p->is_dying) {
			plog(ASL_LEVEL_NOTICE, "skipping sweep of dying session.\n");
			continue;
		}
		SCHED_KILL(p->sc_xauth);
		if (p->is_asserted) {
			// for asserted session, traffic monitors will be restared after phase2 becomes established.
			SCHED_KILL(p->traffic_monitor.sc_mon);
			SCHED_KILL(p->traffic_monitor.sc_idle);
			plog(ASL_LEVEL_NOTICE, "skipping sweep of asserted session.\n");
			continue;
		}

		// cleanup any stopped sessions as they will go down                                                                                                                               
                if (p->stopped_by_vpn_controller || p->stop_timestamp.tv_sec || p->stop_timestamp.tv_usec) {
			plog(ASL_LEVEL_NOTICE, "sweeping stopped session.\n");
			ike_session_cleanup(p, ike_session_stopped_by_sleepwake);
			continue;
                }

		if (!ike_session_has_established_ph1(p) && !ike_session_has_established_ph2(p)) {
			plog(ASL_LEVEL_NOTICE, "session died while sleeping.\n");
			ike_session_cleanup(p, ike_session_stopped_by_sleepwake);
			continue;
		}
		if (p->traffic_monitor.sc_mon) {
            time_t xtime;
            if (sched_get_time(p->traffic_monitor.sc_mon, &xtime)) {
                if (xtime <= swept_at) {
                    SCHED_KILL(p->traffic_monitor.sc_mon);
                    if (!p->is_dying && p->traffic_monitor.interv_mon) {
                        p->traffic_monitor.sc_mon = sched_new(p->traffic_monitor.interv_mon,
														  ike_session_traffic_cop,
														  p);
                    }
                }
			}			
		}
		if (p->traffic_monitor.sc_idle) {
            time_t xtime;
            if (sched_get_time(p->traffic_monitor.sc_idle, &xtime)) {
                if (xtime <= swept_at) {
                    SCHED_KILL(p->traffic_monitor.sc_idle);
                    if (!p->is_dying && p->traffic_monitor.interv_idle) {
                        p->traffic_monitor.sc_idle = sched_new(p->traffic_monitor.interv_idle,
														   ike_session_cleanup_idle,
														   p);
                    }
                }
			}
		}
	}
}

/*
 * this is called after racooon receives an assert command from the controller/pppd.
 * this is intended to make racoon prepare to rekey both SAs because a network event occurred.
 * in the event of a sleepwake, the assert could happen before or after 'ike_session_sweep_sleepwake'.
 */
int
ike_session_assert_session (ike_session_t *session)
{
	phase2_handle_t *iph2 = NULL;
	phase2_handle_t *iph2_next = NULL;
	phase1_handle_t *iph1 = NULL;
	phase1_handle_t *iph1_next = NULL;

	if (!session || session->is_dying) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return -1;
	}

	// the goal is to prepare the session for fresh rekeys by silently deleting the currently active phase2s
	LIST_FOREACH_SAFE(iph2, &session->ph2tree, ph2ofsession_chain, iph2_next) {
		if (!iph2->is_dying && !FSM_STATE_IS_EXPIRED(iph2->status)) {
			SCHED_KILL(iph2->sce);
			iph2->is_dying = 1;
			
			// delete SAs (in the kernel)
			if (FSM_STATE_IS_ESTABLISHED(iph2->status) && iph2->approval) {
				struct saproto *pr;
				
				for (pr = iph2->approval->head; pr != NULL; pr = pr->next) {
					if (pr->ok) {
						//log deletion
						plog(ASL_LEVEL_NOTICE,
							 "Assert: Phase 2 %s deleted\n",
							 sadbsecas2str(iph2->src, iph2->dst, iph2->satype, iph2->spid, ipsecdoi2pfkey_mode(pr->encmode)));
						
						pfkey_send_delete(lcconf->sock_pfkey,
										  ipsecdoi2pfkey_proto(pr->proto_id),
										  ipsecdoi2pfkey_mode(pr->encmode),
										  iph2->src, iph2->dst, pr->spi_p);
					}
				}
			}
			
			fsm_set_state(&iph2->status, IKEV1_STATE_PHASE2_EXPIRED); 	// we want to delete SAs without telling the PEER
			iph2->sce = sched_new(3, ike_session_cleanup_ph2_stub, iph2);
		}
	}

	// the goal is to prepare the session for fresh rekeys by silently deleting the currently active phase1s
	LIST_FOREACH_SAFE(iph1, &session->ph1tree, ph1ofsession_chain, iph1_next) {
		if (!iph1->is_dying && !FSM_STATE_IS_EXPIRED(iph1->status)) {
			SCHED_KILL(iph1->sce);
			SCHED_KILL(iph1->sce_rekey);
			iph1->is_dying = 1;

			//log deletion
			plog(ASL_LEVEL_NOTICE,
				 "Assert: Phase 1 %s deleted\n",
				 isakmp_pindex(&iph1->index, 0));
			
			ike_session_unbind_all_ph2_from_ph1(iph1);
			
			fsm_set_state(&iph1->status, IKEV1_STATE_PHASE1_EXPIRED); 	// we want to delete SAs without telling the PEER
			/* responder sets up timer to delete old inbound SAs... say 7 secs later and flags them as rekeyed */
			iph1->sce = sched_new(5, ike_session_cleanup_ph1_stub, iph1);
		}
	}
	session->is_asserted = 1;

	return 0;
}

int
ike_session_assert (struct sockaddr_storage *local, 
					struct sockaddr_storage *remote)
{
	ike_session_t *sess;

	if (!local || !remote) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return -1;
	}

	if ((sess = ike_session_get_session(local, remote, FALSE, NULL))) {
		return(ike_session_assert_session(sess));
	}
	return -1;
}

void
ike_session_ph2_retransmits (phase2_handle_t *iph2)
{
	int num_retransmits;

	if (!iph2->is_dying &&
		iph2->is_rekey &&
		iph2->ph1 &&
		iph2->ph1->sce_rekey && !sched_is_dead(iph2->ph1->sce_rekey) &&
		iph2->side == INITIATOR &&
		iph2->parent_session &&
		!iph2->parent_session->is_cisco_ipsec && /* not for Cisco */
		iph2->parent_session->is_client) {
		num_retransmits = iph2->ph1->rmconf->retry_counter - iph2->retry_counter;
		if (num_retransmits == 3) {
			/*
			 * phase2 negotiation is stalling on retransmits, inspite of a valid ph1.
			 * one of the following is possible:
			 * - (0) severe packet loss.
			 * - (1) the peer is dead.
			 * - (2) the peer is out of sync hence dropping this phase2 rekey (and perhaps responding with insecure 
			 *       invalid-cookie notifications... but those are untrusted and so we can't rekey phase1 off that)
			 *		(2.1) the peer rebooted (or process restarted) and is now alive.
			 *     (2.2) the peer has deleted phase1 without notifying us (or the notification got dropped somehow).
			 *     (2.3) the peer has a policy/bug stopping this phase2 rekey
			 *
			 * in all these cases, one sure way to know is to trigger a phase1 rekey early.
			 */
			plog(ASL_LEVEL_NOTICE, "Many Phase 2 retransmits: try Phase 1 rekey and this Phase 2 to quit earlier.\n");
			isakmp_ph1rekeyexpire(iph2->ph1, TRUE);
			iph2->retry_counter = 0;
		}
	}
}

void
ike_session_ph1_retransmits (phase1_handle_t *iph1)
{
	int num_retransmits;
	
	if (!iph1->is_dying &&
		iph1->is_rekey &&
		!iph1->sce_rekey &&
		FSM_STATE_IS_NEGOTIATING(iph1->status) &&
		iph1->side == INITIATOR &&
		iph1->parent_session &&
		iph1->parent_session->is_client &&
		!ike_session_has_other_negoing_ph1(iph1->parent_session, iph1)) {
		num_retransmits = iph1->rmconf->retry_counter - iph1->retry_counter;
		if (num_retransmits == 3) {
			plog(ASL_LEVEL_NOTICE, "Many Phase 1 retransmits: try quit earlier.\n");
			iph1->retry_counter = 0;
		}
	}
}

static void
ike_session_bindph12(phase1_handle_t *iph1, phase2_handle_t *iph2)
{
	if (iph2->ph1) {
		plog(ASL_LEVEL_ERR, "Phase 2 already bound %s.\n", __FUNCTION__);
	}
	iph2->ph1 = iph1;
    LIST_INSERT_HEAD(&iph1->bound_ph2tree, iph2, ph1bind_chain);
}

void
ike_session_unbindph12(phase2_handle_t *iph2)
{
	if (iph2->ph1 != NULL) {
		iph2->ph1 = NULL;
        LIST_REMOVE(iph2, ph1bind_chain);
	}
}

static void
ike_session_rebindph12(phase1_handle_t *new_ph1, phase2_handle_t *iph2)
{
	if (!new_ph1) {
		return;
	}
    
	// reconcile the ph1-to-ph2 binding
	ike_session_unbindph12(iph2);
	ike_session_bindph12(new_ph1, iph2);
	// recalculate ivm since ph1 binding has changed
	if (iph2->ivm != NULL) {
		oakley_delivm(iph2->ivm);
		if (FSM_STATE_IS_ESTABLISHED(new_ph1->status)) {
			iph2->ivm = oakley_newiv2(new_ph1, iph2->msgid);
			plog(ASL_LEVEL_NOTICE, "Phase 1-2 binding changed... recalculated ivm.\n");
		} else {
			iph2->ivm = NULL;
		}
	}
}

static void
ike_session_unbind_all_ph2_from_ph1 (phase1_handle_t *iph1)
{
	phase2_handle_t *p = NULL;
	phase2_handle_t *next = NULL;
	
	LIST_FOREACH_SAFE(p, &iph1->bound_ph2tree, ph1bind_chain, next) {
		ike_session_unbindph12(p);
	}
}

static void
ike_session_rebind_all_ph12_to_new_ph1 (phase1_handle_t *old_iph1,
                                        phase1_handle_t *new_iph1)
{
	phase2_handle_t *p = NULL;
	phase2_handle_t *next = NULL;
	
	if (old_iph1 == new_iph1 || !old_iph1 || !new_iph1) {
		plog(ASL_LEVEL_ERR, "invalid parameters in %s.\n", __FUNCTION__);
		return;
	}
	
	if (old_iph1->parent_session != new_iph1->parent_session) {
		plog(ASL_LEVEL_ERR, "Invalid parent sessions in %s.\n", __FUNCTION__);
		return;
	}
	
	LIST_FOREACH_SAFE(p, &old_iph1->bound_ph2tree, ph1bind_chain, next) {
		if (p->parent_session != new_iph1->parent_session) {
			plog(ASL_LEVEL_ERR, "Mismatched parent session in ph1bind replacement.\n");
		}
		if (p->ph1 == new_iph1) {
			plog(ASL_LEVEL_ERR, "Same Phase 2 in ph1bind replacement in %s.\n",__FUNCTION__);
		}
        ike_session_rebindph12(new_iph1, p);
	}
}

