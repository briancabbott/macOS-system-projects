/*
 * vmps.c	Handle VMPS traffic.
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
 * Copyright 2007  The FreeRADIUS server project
 * Copyright 2007  Alan DeKok <aland@deployingradius.com>
 */

#include <freeradius-devel/ident.h>
RCSID("$Id$")

#include <freeradius-devel/radiusd.h>
#include <freeradius-devel/modules.h>
#include <freeradius-devel/vqp.h>
#include <freeradius-devel/vmps.h>
#include <freeradius-devel/rad_assert.h>

#ifdef WITH_VMPS
/*
 *	Check if an incoming request is "ok"
 *
 *	It takes packets, not requests.  It sees if the packet looks
 *	OK.  If so, it does a number of sanity checks on it.
 */
int vqp_socket_recv(rad_listen_t *listener,
		    RAD_REQUEST_FUNP *pfun, REQUEST **prequest)
{
	RADIUS_PACKET	*packet;
	RAD_REQUEST_FUNP fun = NULL;
	RADCLIENT	*client;

	packet = vqp_recv(listener->fd);
	if (!packet) {
		radlog(L_ERR, "%s", fr_strerror());
		return 0;
	}

	if ((client = client_listener_find(listener,
					   &packet->src_ipaddr,
					   packet->src_port)) == NULL) {
		rad_free(&packet);
		return 0;
	}

	/*
	 *	Do new stuff.
	 */
	fun = vmps_process;

	if (!received_request(listener, packet, prequest, client)) {
		rad_free(&packet);
		return 0;
	}

	*pfun = fun;

	return 1;
}


/*
 *	Send an authentication response packet
 */
int vqp_socket_send(rad_listen_t *listener, REQUEST *request)
{
	rad_assert(request->listener == listener);
	rad_assert(listener->send == vqp_socket_send);

	if (vqp_encode(request->reply, request->packet) < 0) {
		DEBUG2("Failed encoding packet: %s\n", fr_strerror());
		return -1;
	}

	return vqp_send(request->reply);
}


int vqp_socket_encode(UNUSED rad_listen_t *listener, REQUEST *request)
{
	return vqp_encode(request->reply, request->packet);
}


int vqp_socket_decode(UNUSED rad_listen_t *listener, REQUEST *request)
{
	return vqp_decode(request->packet);
}


int vmps_process(REQUEST *request)
{
	DEBUG2("Doing VMPS");
	module_post_auth(0, request);
	DEBUG2("Done VMPS");

	request->reply->code = PW_AUTHENTICATION_ACK;

	return 0;
}
#endif
