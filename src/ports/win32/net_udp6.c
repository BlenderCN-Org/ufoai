/**
 * @file net_udp6.c
 * @brief Windows IPv6 network code
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/win32/net_wins.c
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "../../qcommon/qcommon.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <tpipv6.h>
#include <wsipx.h>

#define	MAX_LOOPBACK	4
#define UFOAIMCAST "ff12::666"

typedef struct
{
	byte data[MAX_MSGLEN];
	int datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t msgs[MAX_LOOPBACK];
	int get, send;
} loopback_t;


cvar_t *net_shownet;
static cvar_t *noudp;
static cvar_t *noipx;

loopback_t loopbacks[2];
int ip_sockets[2];
int ip6_sockets[2];
int ipx_sockets[2];

char *multicast_interface;

char *NET_ErrorString (void);

/* extern in net_chan.c */
int net_inittime;

uint64_t net_total_in;
uint64_t net_total_out;
uint64_t net_packets_in;
uint64_t net_packets_out;

/**
 * @brief Print the local IP addresses
 */
void Sys_ShowIP(void)
{
}

/**
 * @brief Convert a network address to a socket one
 * @param[in] a Pointer to network address which is to be converted
 * @param[out] s Pointer to where the socket address will be written to
 * @sa SockadrToNetadr
 */
void NetadrToSockadr (netadr_t *a, struct sockaddr_storage *s)
{
	struct sockaddr_in6 *s6;
	struct addrinfo hints;
	struct addrinfo *res;
	int error;

	memset(s, 0, sizeof(*s));
	switch (a->type) {
	case NA_BROADCAST:
		((struct sockaddr_in*)s)->sin_family = AF_INET;
		((struct sockaddr_in*)s)->sin_port = a->port;
		((struct sockaddr_in*)s)->sin_addr.s_addr = INADDR_BROADCAST;
		break;

	case NA_IP:
		((struct sockaddr_in*)s)->sin_family = AF_INET;
		*(int *)&((struct sockaddr_in*)s)->sin_addr = *(int *)&a->ip;
		((struct sockaddr_in*)s)->sin_port = a->port;
		break;

	case NA_MULTICAST6:
		s6 = (struct sockaddr_in6 *)s;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET6;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = AI_NUMERICHOST;

		error = getaddrinfo(UFOAIMCAST, NULL, &hints, &res);
		if (error) {
			Com_Printf("NET_NetadrToSockadr: inet_pton: %s", gai_strerror(error));
			return;
		}

		/* sockaddr_in6 should now have a valid scope_id. */
		memcpy(s6, res->ai_addr, res->ai_addrlen);
		s6->sin6_port = a->port;
		/* scope_id is important for link-local
		* destination. @todo: check in code for possible
		* uninitialized netadr_t.scope_id instances */
		s6->sin6_scope_id = a->scope_id;

		break;

	case NA_IPV6:
		if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *) a->ip)) {
#ifdef HAVE_SIN6_LEN
			s->ss_len = sizeof(struct sockaddr_in);
#endif
			s->ss_family = AF_INET;
			memcpy(&((struct sockaddr_in *)s)->sin_addr,
					&((struct in6_addr *) a->ip)->s6_addr[12],
					sizeof(struct in_addr));
			((struct sockaddr_in*)s)->sin_port = a->port;

		} else {
			s6 = (struct sockaddr_in6 *)s;

			s6->sin6_family = AF_INET6;
			memcpy(&s6->sin6_addr, a->ip, sizeof(s6->sin6_addr));
			s6->sin6_port = a->port;
#ifdef HAVE_SIN6_LEN
			s6->sin6_len = sizeof(struct sockaddr_in6);
#endif
			/* scope_id is important for link-local
			* destination. @todo: check in code for possible
			* uninitialized netadr_t.scope_id instances */
			s6->sin6_scope_id = a->scope_id;
		}
		break;

	case NA_LOOPBACK:
	case NA_IPX:
	case NA_BROADCAST_IPX:
		/* no handling of NA_LOOPBACK, NA_IPX, NA_BROADCAST_IPX */
		break;
	}
}

/**
 * @brief Convert a socket address to a network one
 * @param[in] s Pointer to the socket address which is to be converted
 * @param[out] a Pointer to where the network address will be written to
 * @sa NetadrToSockadr
 */
void SockadrToNetadr (struct sockaddr_storage *s, netadr_t *a)
{
	struct sockaddr_in6 *s6;

	if (s->ss_family == AF_INET) {
		*(int *)&a->ip = *(int *)&((struct sockaddr_in*)s)->sin_addr;
		a->port = ((struct sockaddr_in*)s)->sin_port;
		a->type = NA_IP;
	} else if (s->ss_family == AF_INET6) {
		s6 = (struct sockaddr_in6 *)s;

		if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)&s6->sin6_addr)) {
			memcpy(a->ip, (struct in_addr *)&s6->sin6_addr.s6_addr[12], sizeof(struct in_addr));
			a->port = ((struct sockaddr_in*)s)->sin_port;
			a->type = NA_IP;
		} else {
			memcpy(a->ip, &s6->sin6_addr, sizeof(a->ip));
			a->port = s6->sin6_port;
			a->type = NA_IPV6;
			a->scope_id = s6->sin6_scope_id;
		}
	} else
		s = NULL;
}

/**
 * @brief Compare two network addresses
 * @param[in] a The first address
 * @param[in] b The second address
 * @returns qtrue if their types are the same and
 * they are either loopback addresses,
 * or if both addresses' IPs and ports are the same.
 * Otherwise it returns false.
 */
qboolean NET_CompareAdr (netadr_t a, netadr_t b)
{
	if (a.type != b.type)
		return qfalse;

	if (a.type == NA_LOOPBACK)
		return qtrue;

	if (a.type == NA_IP) {
		if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3] && a.port == b.port)
			return qtrue;
		return qfalse;
	}

	if (a.type == NA_IPV6) {
		if ((memcmp(a.ip, b.ip, 16) == 0) && a.port == b.port)
			return qtrue;
	}

	if (a.type == NA_IPX) {
		if ((memcmp(a.ipx, b.ipx, 10) == 0) && a.port == b.port)
			return qtrue;
		return qfalse;
	}

	return qfalse;

}

/**
 * @brief Compares two network addresses ignoring their ports
 * @param[in] a The first address
 * @param[in] b The second address
 * @returns qtrue if their types are the same and
 * they are either loopback addresses,
 * or if both addresses' IPs are the same.
 * Otherwise it returns false.
 */
qboolean NET_CompareBaseAdr (netadr_t a, netadr_t b)
{
	if (a.type != b.type)
		return qfalse;

	if (a.type == NA_LOOPBACK)
		return qtrue;

	if (a.type == NA_IP) {
		if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3])
			return qtrue;
		return qfalse;
	}

	if (a.type == NA_IPV6) {
		if ((memcmp(a.ip, b.ip, 16) == 0))
			return qtrue;
		return qfalse;
	}

	if (a.type == NA_IPX) {
		if ((memcmp(a.ipx, b.ipx, 10) == 0))
			return qtrue;
		return qfalse;
	}

	return qfalse;
}

/**
 * @brief convert a network address to a string - port included
 * @param[in] a The network address which is to be converted
 * @returns a pointer to the string representation of the address
 * @post the returned pointer is not null
 * @sa *NET_BaseAdrToString
 */
char *NET_BaseAdrToString (netadr_t a)
{
	static	char	s[64], tmp[64];
	struct sockaddr_storage ss;
	struct sockaddr_in6 *s6;
	int flags;

	switch (a.type) {
	case NA_IP:
	case NA_LOOPBACK:
		Com_sprintf(s, sizeof(s), "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3]);
		break;
	case NA_BROADCAST:
		Com_sprintf(s, sizeof(s), "255.255.255.255");
		break;
	case NA_IPV6:
	case NA_MULTICAST6:
		memset(&ss, 0, sizeof(ss));
		s6 = (struct sockaddr_in6 *)&ss;

		if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *) a.ip)) {
#ifdef HAVE_SIN6_LEN
			ss.ss_len = sizeof(struct sockaddr_in);
#endif
			ss.ss_family = AF_INET;
			memcpy(&((struct sockaddr_in *) &ss)->sin_addr,
					&((struct in6_addr *) a.ip)->s6_addr[12],
					sizeof(struct in_addr));
		} else {
#ifdef HAVE_SIN6_LEN
			s6->sin6_len = sizeof(struct sockaddr_in6);
#endif
			/* Possible bug: a.scope_id may not have been
			 * initialized. See comments below on better
			 * way to handle this */
			s6->sin6_scope_id = a.scope_id;
			s6->sin6_family = AF_INET6;
			memcpy(&s6->sin6_addr, a.ip, sizeof(struct in6_addr));
		}

		flags = NI_NUMERICHOST;

#ifdef HAVE_SIN6_LEN
		if (getnameinfo((struct sockaddr *) &ss, ss.ss_len, s, sizeof(s),
				NULL, 0, NI_NUMERICHOST))
#else
		if (getnameinfo((struct sockaddr *) &ss, sizeof(struct sockaddr_in6),
				s, sizeof(s), NULL, 0, NI_NUMERICHOST))
#endif
			Com_sprintf(s, sizeof(s), "<invalid>");
		else {
			if (a.type == NA_MULTICAST6 ||
				IN6_IS_ADDR_LINKLOCAL(&((struct sockaddr_in6 *)&ss)->sin6_addr)) {
				/**
				 * If the address is multicast (link) or a
				 * link-local, need to carry the scope. The string
				 * format of the IPv6 address is used by the
				 * client to extablish the connect to the
				 * server.
				 * A better way to handle this is to always use
				 * sockaddr_storage to represent an IP (v4, v6)
				 * address
				 * Check first if address is already
				 * scoped. getnameinfo under Windows
				 * and Linux (?) already return scoped
				 * IPv6 address
				 */
				if (strchr(s, '%') == NULL) {
					Com_sprintf(tmp, sizeof(tmp), "%s%%%d", s, s6->sin6_scope_id);
					memcpy(s, tmp, sizeof(s));
				}
			}
		}

		break;
	case NA_IPX:
	case NA_BROADCAST_IPX:
		Com_sprintf(s, sizeof(s), "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x:%i",
			a.ipx[0], a.ipx[1], a.ipx[2], a.ipx[3], a.ipx[4],
			a.ipx[5], a.ipx[6], a.ipx[7], a.ipx[8], a.ipx[9], ntohs(a.port));
		break;

	default:
		Com_sprintf(s, sizeof(s), "invalid IP address family type");
		break;
	}

	return s;
}

/**
 * @brief convert a network address to a string - port included
 * @param[in] a The network address which is to be converted
 * @returns a pointer to the string representation of the address
 * @post the returned pointer is not null
 * @sa *NET_BaseAdrToString
 */
char *NET_AdrToString (netadr_t a)
{
	static char s[64];
	const char *base;

	base = NET_BaseAdrToString(a);
	Com_sprintf(s, sizeof(s), "[%s]:%d", base, ntohs(a.port));

	return s;
}

#if 0
#define DO(src,dest)	\
	copy[0] = s[src];	\
	copy[1] = s[src + 1];	\
	sscanf(copy, "%x", &val);	\
	((struct sockaddr_ipx *)sadr)->dest = val
#endif

/**
 * @brief Convert a string to a socket address
 * @param[out] sadr Pointer to where the resulting socket address will be written
 * @param[in] s String which is to be converted to a socket address
 * @returns qtrue if the supplied string was a valid socket address
 * and conversion succeeded. Otherwise returns qfalse.
 * @note valid string formats include: localhost
 * idnewt
 * idnewt:28000
 * 192.246.40.70
 * 192.246.40.70:28000
 */
qboolean NET_StringToSockaddr (char *s, struct sockaddr_storage *sadr)
{
	char	copy[128];
	char	*addrs, *space;
	char	*ports = NULL;
	int	err;
	struct addrinfo hints;
	struct addrinfo *resultp;

	memset(sadr, 0, sizeof(*sadr));
	memset(&hints,0, sizeof(hints));
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_family = PF_UNSPEC;

	strcpy(copy, s);
	addrs = space = copy;
	if (*addrs == '[') {
		addrs++;
		for (; *space && *space != ']'; space++);
		if (!*space) {
				Com_Printf("NET_StringToSockaddr: invalid IPv6 address %s\n", s);
				return qfalse;
		}
		*space++ = '\0';
	}

	for (; *space; space++) {
		if (*space == ':') {
			*space = '\0';
			ports = space + 1;
		}
	}

	if ((err = getaddrinfo(addrs, ports, &hints, &resultp))) {
		/* Error */
		Com_Printf("NET_StringToSockaddr: string %s:\n%s\n", s, gai_strerror(err));
		return qfalse;
	}

	switch (resultp->ai_family) {
	case AF_INET:
		/* convert to ipv4 addr */
		memset(sadr, 0, sizeof(struct sockaddr_storage));
		memcpy(sadr, resultp->ai_addr, resultp->ai_addrlen);
		break;
	case AF_INET6:
		/* convert to ipv6 addr */
		memset(sadr, 0, sizeof(struct sockaddr_storage));
		memcpy(sadr, resultp->ai_addr, resultp->ai_addrlen);
		break;
	default:
		Com_Printf("NET_StringToSockaddr: string %s:\nprotocol family %d not supported\n",
			s, resultp->ai_family);
		return qfalse;
	}

	/* Detection of IPX address clashes with IPv6 addresses. Resolve this later. */
#if 0
	/* check for an IPX address */
	if ((strlen(s) >= 23) && (s[8] == ':') && (s[21] == ':')) {
		((struct sockaddr_ipx *)sadr)->sa_family = AF_IPX;
		copy[2] = 0;
		DO(0, sa_netnum[0]);
		DO(2, sa_netnum[1]);
		DO(4, sa_netnum[2]);
		DO(6, sa_netnum[3]);
		DO(9, sa_nodenum[0]);
		DO(11, sa_nodenum[1]);
		DO(13, sa_nodenum[2]);
		DO(15, sa_nodenum[3]);
		DO(17, sa_nodenum[4]);
		DO(19, sa_nodenum[5]);
		sscanf(&s[22], "%u", &val);
		((struct sockaddr_ipx *)sadr)->sa_socket = htons((unsigned short)val);
	}
#endif

	return qtrue;
}

/**
 * @brief Convert a string to a network address
 * @param[out] a Pointer to where the resulting network address will be written
 * @param[in] s String which is to be converted to a network address
 * @returns qtrue if the supplied string was a valid network address
 * and conversion succeeded. Otherwise returns qfalse.
 * @note valid string formats include: localhost
 * idnewt
 * idnewt:28000
 * 192.246.40.70
 * 192.246.40.70:28000
 */
qboolean NET_StringToAdr (char *s, netadr_t *a)
{
	struct sockaddr_storage sadr;

	if (!strcmp(s, "localhost")) {
		memset(a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;
		return qtrue;
	}

	if (!NET_StringToSockaddr(s, &sadr))
		return qfalse;

	SockadrToNetadr(&sadr, a);

	return qtrue;
}

/**
 * @brief Converts a socket address to a string representation
 * @param[in] s_ptr Pointer to the socket address
 * @returns A pointer to the string representation of this socket address
 * @post the returned pointer is not null
 * @sa SockadrToNetadr
 */
char *NET_SocketToString (void *s_ptr)
{
	netadr_t addr;
	struct sockaddr_storage *s = (struct sockaddr_storage*)s_ptr;

	if (!s)
		return "";
	memset(&addr, 0, sizeof(netadr_t));
	SockadrToNetadr(s, &addr);
	return NET_AdrToString(addr);
}

/**
 * @brief Decide whether the address is local
 * @param[in] adr The address that is in question
 * @returns qtrue if the supplied address is local. Otherwise returns qfalse.
 */
qboolean NET_IsLocalAddress (netadr_t adr)
{
	return adr.type == NA_LOOPBACK;
}

/*
=============================================================================
LOOPBACK BUFFERS FOR LOCAL PLAYER
=============================================================================
*/

/**
 * @brief
 */
qboolean NET_GetLoopPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if (loop->get >= loop->send)
		return qfalse;

	i = loop->get & (MAX_LOOPBACK-1);
	loop->get++;

	memcpy(net_message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	net_message->cursize = loop->msgs[i].datalen;
	memset(net_from, 0, sizeof(*net_from));
	net_from->type = NA_LOOPBACK;
	return qtrue;
}


/**
 * @brief
 */
void NET_SendLoopPacket (netsrc_t sock, int length, void *data, netadr_t to)
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock^1];

	i = loop->send & (MAX_LOOPBACK-1);
	loop->send++;

	memcpy(loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}

/**
 * @brief
 */
int NET_GetPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int 	ret;
	struct sockaddr_storage	from;
	socklen_t	fromlen;
	int		net_socket;
	int		protocol;
	int		err;

	if (NET_GetLoopPacket(sock, net_from, net_message))
		return 1;

	for (protocol = 0; protocol < 3; protocol++) {
		if (protocol == 0)
			net_socket = ip_sockets[sock];
		else if (protocol == 1)
			net_socket = ip6_sockets[sock];
		else
			net_socket = ipx_sockets[sock];

		if (!net_socket)
			continue;

		fromlen = sizeof(from);
		ret = recvfrom(net_socket, net_message->data, net_message->maxsize,
			0, (struct sockaddr *)&from, &fromlen);

		net_packets_in++;
		net_total_in += ret;

		SockadrToNetadr(&from, net_from);

		if (ret == -1) {
			err = WSAGetLastError();

			if (err == WSAEWOULDBLOCK)
				continue;
				if (err == WSAEMSGSIZE) {
					Com_Printf("Warning:  Oversize packet from %s\n", NET_AdrToString(*net_from));
				continue;
			}

			if (dedicated->value)	/* let dedicated servers continue after errors */
				Com_Printf("NET_GetPacket: %s from %s\n", NET_ErrorString(),
					NET_AdrToString(*net_from));
			else
				Com_Printf("NET_GetPacket: %s from %s",
					NET_ErrorString(), NET_AdrToString(*net_from));

			continue;
		}

		if (ret == net_message->maxsize) {
			Com_Printf("Oversize packet from %s\n", NET_AdrToString(*net_from));
			continue;
		}

		net_message->cursize = ret;
		return 1;
	}

	return 0;
}

/**
 * @brief
 */
void NET_SendPacket (netsrc_t sock, int length, void *data, netadr_t to)
{
	int		ret;
	struct sockaddr_storage	addr;
	int		net_socket;
	socklen_t		addr_size = sizeof(struct sockaddr_in);

	switch (to.type) {
#ifndef DEDICATED_ONLY
	case NA_LOOPBACK:
		NET_SendLoopPacket(sock, length, data, to);
		return;
#endif
	case NA_BROADCAST:
	case NA_IP:
		net_socket = ip_sockets[sock];
		break;
	case NA_IPV6:
	case NA_MULTICAST6:
		net_socket = ip6_sockets[sock];
		addr_size = sizeof(struct sockaddr_in6);
		break;
	case NA_IPX:
	case NA_BROADCAST_IPX:
		net_socket = ipx_sockets[sock];
		break;
	default:
		Com_Error(ERR_FATAL, "NET_SendPacket: bad address type");
		return;
	}

	if (!net_socket)
		return;

	NetadrToSockadr(&to, &addr);

	/**
	 * Re-check the address family. If to.type is NA_IPV6 but
	 * contains an IPv4 mapped address, NetadrToSockadr will
	 * return an AF_INET struct.  If so, switch back to AF_INET
	 * socket.
	 */
	if (to.type == NA_IPV6 && addr.ss_family == AF_INET) {
		net_socket = ip_sockets[sock];
		addr_size = sizeof(struct sockaddr_in);
		if (!net_socket)
			return;
	}

	if (addr.ss_family == AF_INET6) {
		struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)&addr;

		/* If multicast socket, must specify scope. So multicast_interface must be specified */
		if (IN6_IS_ADDR_MULTICAST(&s6->sin6_addr)) {
			struct addrinfo hints;
			struct addrinfo *res;
			char tmp[128], mcast_addr[128], mcast_port[10];
			int error;

			/**
			 * Do a getnameinfo/getaddrinfo cycle
			 * to calculate the scope_id of the
			 * multicast address. getaddrinfo is
			 * passed a multicast address of the
			 * form
			 * ff0x::xxx%multicast_interface
			 */
#ifdef HAVE_SIN6_LEN
			error = getnameinfo((struct sockaddr *) s6, s6->sin6_len, tmp, sizeof(tmp),
					NULL, 0, NI_NUMERICHOST);
#else
			error = getnameinfo((struct sockaddr *) s6, sizeof(struct sockaddr_in6), tmp, sizeof(tmp),
					NULL, 0, NI_NUMERICHOST);
#endif
			if (error) {
				Com_Printf("NET_SendPacket: getnameinfo: %s", gai_strerror(error));
				return;
			}

			if (multicast_interface != NULL) {
				/*
					On Unix, specify the multicast scope
					on the address (ex:
					ff12::666%fxp0). getaddrinfo will map
					the proper scope_id for the address.

					On Win32, we don't. More tests required.
				*/
				Com_sprintf(mcast_addr, sizeof(mcast_addr), "%s", tmp);
				Com_sprintf(mcast_port, sizeof(mcast_port), "%d", ntohs(s6->sin6_port));
				memset(&hints, 0, sizeof(hints));
				hints.ai_family = AF_INET6;
				hints.ai_socktype = SOCK_DGRAM;
				hints.ai_flags =  AI_NUMERICHOST;

				error = getaddrinfo(mcast_addr, mcast_port, &hints, &res);
				if (error) {
					Com_Printf("NET_SendPacket: getaddrinfo: %s", gai_strerror(error));
					return;
				}

				/* sockaddr_in6 should now have a valid scope_id. */
				memcpy(s6, res->ai_addr, res->ai_addrlen);

			} else {
				Com_Printf("NET_SendPacket: IPv6 multicast destination but +set multicast not specified: %s", tmp);
				return;
			}

		}
	}

	ret = sendto(net_socket, data, length, 0, (struct sockaddr *)&addr, addr_size );
	if (ret == -1) {
		int err = WSAGetLastError();

		/* wouldblock is silent */
		if (err == WSAEWOULDBLOCK)
			return;

		/* some PPP links dont allow broadcasts */
		if ((err == WSAEADDRNOTAVAIL) && ((to.type == NA_BROADCAST) || (to.type == NA_BROADCAST_IPX)))
			return;

		if (dedicated->value) {
			/* let dedicated servers continue after errors */
			Com_Printf("NET_SendPacket Warning: %s to %s\n", NET_ErrorString(),
				NET_AdrToString(to));
		} else {
			if (err == WSAEADDRNOTAVAIL) {
				Com_DPrintf("NET_SendPacket Warning: %s : %s\n",
					NET_ErrorString(), NET_AdrToString(to));
			} else {
				Com_Printf("NET_SendPacket ERROR: %s to %s\n",
					NET_ErrorString(), NET_AdrToString(to));
			}
		}
	} else {
		net_packets_out++;
		net_total_out += ret;
	}
}

/**
 * @brief
 */
int NET_IPSocket (char *net_interface, int port, netsrc_t type, int family)
{
	char Buf[BUFSIZ], *Host, *Service;
	int newsocket, Error;
	struct sockaddr_storage ss;
	struct addrinfo hints, *res, *ai;
	qboolean _true = qtrue;
	int one = 1;
	int	i = 1;
#ifdef IPV6_V6ONLY
	int dummy;
#endif
	struct ipv6_mreq mreq;
	cvar_t *mcast;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_PASSIVE;

	if (!net_interface || !net_interface[0] || !stricmp(net_interface, "localhost"))
		Host = (family == AF_INET6) ? "::" : "0.0.0.0";
	else
		Host = net_interface;

	if (port == PORT_ANY)
		Service = NULL;
	else {
		sprintf(Buf, "%5d", port);
		Service = Buf;
	}

	if ((Error = getaddrinfo(Host, Service, &hints, &res))) {
		return 0;
	}

	for (ai = res; ai != NULL; ai = ai->ai_next) {
		if ((newsocket = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) == -1) {
			Com_Printf("NET_IPSocket: socket: %s\n", strerror(errno));
			continue;
		}
		/* make it non-blocking */
		if (ioctlsocket(newsocket, FIONBIO, &_true) == -1) {
				Com_Printf("NET_IPSocket: ioctl FIONBIO: %s\n", strerror(errno));
				continue;
		}
		if (setsockopt(newsocket, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one))) {
				printf("NET_IPSocket: setsockopt(SO_REUSEADDR) failed: %u\n",
					WSAGetLastError());
				continue;
		}
#ifdef IPV6_V6ONLY
		if (family == AF_INET6)
			if (ioctlsocket(newsocket, IPPROTO_IPV6, IPV6_V6ONLY, &dummy, sizeof(dummy)) < 0) {
				Com_Printf("NET_IPSocket: setsockopt IPV6_V6ONLY: %s\n", strerror(errno));
			}
#endif
			if (family == AF_INET) {
				/* make it broadcast capable */
				if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&one, sizeof(one))) {
						Com_Printf("ERROR: NET_IPSocket: setsockopt SO_BROADCAST:%s\n", strerror(errno));
						return 0;
				}
			}
			if (bind(newsocket, ai->ai_addr, ai->ai_addrlen) < 0) {
				Com_Printf("NET_IPSocket: bind: %s\n", strerror(errno));
				closesocket(newsocket);
			} else {
				memcpy(&ss, ai->ai_addr, ai->ai_addrlen);
				break;
			}

	}

	if (res != NULL)
		freeaddrinfo(res);

	if (ai == NULL)
		return 0;

	switch (ss.ss_family) {
	case AF_INET:
		break;

	case AF_INET6:
		/**
		 * Multicast outgoing interface is specified for
		 * client and server (+set multicast <ifname>)
		 */
		mcast = Cvar_Get("multicast", "NULL", CVAR_NOSET, NULL);
		multicast_interface = (strcmp(mcast->string, "NULL") ? mcast->string : NULL);

		if (multicast_interface != NULL) {
			/* multicast_interface is a global variable. Also used in NET_SendPacket() */

			mreq.ipv6mr_interface = mcast->integer;
			if (setsockopt(newsocket, IPPROTO_IPV6, IPV6_MULTICAST_IF, (char*)&mreq.ipv6mr_interface,
					sizeof(mreq.ipv6mr_interface)) < 0) {
				Com_Printf("NET_IPSocket: IPV6_MULTICAST_IF: %s\n", strerror(errno));
			}
			/* Join multicast group ONLY if server */
			if (type == NS_SERVER) {
				memset(&hints, 0, sizeof(hints));
				hints.ai_family = AF_INET6;
				hints.ai_socktype = SOCK_DGRAM;
				hints.ai_protocol = IPPROTO_UDP;
				hints.ai_flags = AI_PASSIVE;

				if ((Error = getaddrinfo(UFOAIMCAST, NULL, &hints, &res))) {
					Com_Printf("NET_IPSocket: getaddrinfo: %s\n", gai_strerror(Error));
					break;
				}

				memcpy(&mreq.ipv6mr_multiaddr.s6_addr,
						&((struct sockaddr_in6 *)(res->ai_addr))->sin6_addr,
						sizeof(mreq.ipv6mr_multiaddr.s6_addr));
				freeaddrinfo(res);

				/**
				 * Haven't figured out how to make this
				 * work in Win32. Always
				 * fails. Consequence is that the
				 * server started on a Win32 plateform
				 * will not be listening on the IPv6
				 * multicast group.
				 */
				Error = setsockopt(newsocket, IPPROTO_IPV6, IPV6_JOIN_GROUP,
					(char *)&mreq, sizeof(mreq));

				if (Error) {
					Com_Printf("NET_IPSocket: IPV6_JOIN_GROUP: %s\n",
						strerror(errno));
					break;
				}
			}
		}
		break;
	}

	return newsocket;
}


/**
 * @brief Open the server and client IPv6 (and IPv4) sockets
 * @sa NET_OpenIPX
 * @todo: should return a value to indicate success/failure
 */
void NET_OpenIP (void)
{
	cvar_t	*ip;
	int		port;
	int		dedicated;

	/* get our ip address from cvar "ip"
	 * or use "localhost" if it is not set */
	ip = Cvar_Get("ip", "localhost", CVAR_NOSET, NULL);

	dedicated = Cvar_VariableInteger("dedicated");

	/* create server sockets if there are none yet */
	if (!ip_sockets[NS_SERVER]) {
		/* lookup the server port from cvar "ip_hostport",
		 * if it isn't set, use 0 */
		port = Cvar_Get("ip_hostport", "0", CVAR_NOSET, NULL)->integer;
		/* for our purposes port 0 is invalid */
		if (!port) {
			/* if we didn't get a valid port, try cvar "hostport" */
			port = Cvar_Get("hostport", "0", CVAR_NOSET, NULL)->integer;
			if (!port) {
				/* finaly try cvar "port"
				 * and if it isn't set use the value of PORT_SERVER */
				port = Cvar_Get("port", va("%i", PORT_SERVER), CVAR_NOSET, NULL)->integer;
			}
		}
		/* at this point we might or might not have a valid port */
		/* attempt to open the server IPv6 and IPv4 sockets with the best settings found */
		ip6_sockets[NS_SERVER] = NET_IPSocket(ip->string, port, NS_SERVER, AF_INET6);
		ip_sockets[NS_SERVER] = NET_IPSocket(ip->string, port, NS_SERVER, AF_INET);
		if (!ip_sockets[NS_SERVER] && !ip6_sockets[NS_SERVER] && dedicated)
			Com_Error(ERR_FATAL, "Couldn't allocate server IP port (%i)  - use cvar ip_hostport", port);
	}

	/* dedicated servers don't need client ports */
	if (dedicated)
		return;

	/* at this point we need client sockets as well */
	/* create them if there are none yet */
	if (!ip_sockets[NS_CLIENT]) {
		/* lookup the client port from cvar "ip_clientport",
		 * if it isn't set, use 0 */
		port = Cvar_Get("ip_clientport", "0", CVAR_NOSET, NULL)->value;
		if (!port) {
			/* if that wasn't valid, try cvar "clientport" or PORT_CLIENT */
			port = Cvar_Get("clientport", va("%i", PORT_CLIENT), CVAR_NOSET, NULL)->value;
			/* if we can't find a valid port in the settings, use any available */
			if (!port)
				port = PORT_ANY;
		}
		/* open the client IPv6 and IPv4 sockets with the best settings found */
		ip6_sockets[NS_CLIENT] = NET_IPSocket(ip->string, port, NS_CLIENT, AF_INET6);
		ip_sockets[NS_CLIENT] = NET_IPSocket(ip->string, port, NS_CLIENT, AF_INET);
		/* if that didn't work, retry by opening sockets with any available port */
		if (!ip_sockets[NS_CLIENT] && !ip6_sockets[NS_CLIENT]) {
			ip6_sockets[NS_CLIENT] = NET_IPSocket(ip->string, PORT_ANY, NS_CLIENT, AF_INET6);
			ip_sockets[NS_CLIENT] = NET_IPSocket(ip->string, PORT_ANY, NS_CLIENT, AF_INET);
		}
	}

	/* reset connection counters */
	net_total_in = net_packets_in = net_total_out = net_packets_out = 0;
	net_inittime = (unsigned int)time(NULL);
}


/**
 * @brief Create an IPX socket
 * param[in] port Port for this socket
 * returns The created socket's descriptor upon success. Otherwise returns 0.
 * @sa NET_IPSocket
 */
int NET_IPXSocket (int port)
{
	int					newsocket;
	struct sockaddr_ipx	address;
	int					_true = 1;
	int					err;

	if ((newsocket = socket(PF_IPX, SOCK_DGRAM, NSPROTO_IPX)) == -1) {
		err = WSAGetLastError();
		if (err != WSAEAFNOSUPPORT)
			Com_Printf("WARNING: IPX_Socket: socket: %s\n", NET_ErrorString());
		return 0;
	}

	/* make it non-blocking */
	if (ioctlsocket(newsocket, FIONBIO, &_true) == -1) {
		Com_Printf("WARNING: IPX_Socket: ioctl FIONBIO: %s\n", NET_ErrorString());
		return 0;
	}

	/* make it broadcast capable */
	if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&_true, sizeof(_true)) == -1) {
		Com_Printf("WARNING: IPX_Socket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString());
		return 0;
	}

	address.sa_family = AF_IPX;
	memset(address.sa_netnum, 0, 4);
	memset(address.sa_nodenum, 0, 6);
	if (port == PORT_ANY)
		address.sa_socket = 0;
	else
		address.sa_socket = htons((short)port);

	if (bind(newsocket, (void *)&address, sizeof(address)) == -1) {
		Com_Printf("WARNING: IPX_Socket: bind: %s\n", NET_ErrorString());
		closesocket(newsocket);
		return 0;
	}

	return newsocket;
}


/**
 * @brief Open the server and client IPX sockets
 * @sa NET_OpenIP
 * @todo: should return a value to indicate success/failure
 */
void NET_OpenIPX (void)
{
	int		port;
	int		dedicated;

	dedicated = Cvar_VariableInteger("dedicated");

	if (!ipx_sockets[NS_SERVER]) {
		/* lookup the server port from cvar "ipx_hostport",
		 * if it isn't set, use 0 */
		port = Cvar_Get("ipx_hostport", "0", CVAR_NOSET, NULL)->integer;
		if (!port) {
			/* if that was invalid try cvar "hostport" */
			port = Cvar_Get("hostport", "0", CVAR_NOSET, NULL)->integer;
			if (!port) {
				/* finally try cvar "port" followed by PORT_SERVER */
				port = Cvar_Get("port", va("%i", PORT_SERVER), CVAR_NOSET, NULL)->integer;
			}
		}
		/* attempt to open the server socket with the best settings found */
		ipx_sockets[NS_SERVER] = NET_IPXSocket(port);
	}

	/* dedicated servers don't need client ports */
	if (dedicated)
		return;

	/* at this point we need a client socket as well */
	/* create it if there is none yet */
	if (!ipx_sockets[NS_CLIENT]) {
		/* lookup the client port from cvar "ipx_clientport",
		 * if it isn't set, use 0 */
		port = Cvar_Get("ipx_clientport", "0", CVAR_NOSET, NULL)->value;
		if (!port) {
			/* if that was invalid try cvar "clientport", followed by PORT_CLIENT */
			port = Cvar_Get("clientport", va("%i", PORT_CLIENT), CVAR_NOSET, NULL)->value;
			/* if we can't find a valid port in the settings, use any available */
			if (!port)
				port = PORT_ANY;
		}
		/* attempt to open the client socket with the best settings found */
		ipx_sockets[NS_CLIENT] = NET_IPXSocket(port);
		if (!ipx_sockets[NS_CLIENT])
			ipx_sockets[NS_CLIENT] = NET_IPXSocket(PORT_ANY);
	}

	/* reset connection counters */
	net_total_in = net_packets_in = net_total_out = net_packets_out = 0;
	net_inittime = (unsigned int)time(NULL);
}


/**
 * @brief Configure the network connections
 * @param multiplayer Specify whether this is a single or a multi player game
 * @note A single player game will only use the loopback code
 * @todo: should return a value to indicate success/failure
 */
void NET_Config (qboolean multiplayer)
{
	int		i;
	static	qboolean	old_config;

	if (old_config == multiplayer)
		return;

	old_config = multiplayer;

	if (!multiplayer) {
		/* shut down any existing sockets */
		for (i = 0; i < 2; i++) {
			if (ip_sockets[i]) {
				closesocket(ip_sockets[i]);
				ip_sockets[i] = 0;
			}
			if (ip6_sockets[i]) {
				closesocket(ip6_sockets[i]);
				ip6_sockets[i] = 0;
			}

			if (ipx_sockets[i]) {
				closesocket(ipx_sockets[i]);
				ipx_sockets[i] = 0;
			}
		}
	} else {
		/* open sockets */
		if (! noudp->value)
			NET_OpenIP();
		if (! noipx->value)
			NET_OpenIPX();
	}
}

/**
 * @brief sleeps msec or until net socket is ready
 */
void NET_Sleep(int msec)
{
    struct timeval timeout;
	fd_set	fdset;
	extern cvar_t *dedicated;
	int i;

	if (!dedicated || !dedicated->value)
		return; /* we're not a server, just run full speed */

	FD_ZERO(&fdset);
	i = 0;
	if (ip6_sockets[NS_SERVER]) {
		FD_SET(ip6_sockets[NS_SERVER], &fdset); /* network socket */
		i = ip6_sockets[NS_SERVER];
	}
	if (ip_sockets[NS_SERVER]) {
		FD_SET(ip_sockets[NS_SERVER], &fdset); /* network socket */
		i = ip_sockets[NS_SERVER];
	}
	if (ipx_sockets[NS_SERVER]) {
		FD_SET(ipx_sockets[NS_SERVER], &fdset); /* network socket */
		if (ipx_sockets[NS_SERVER] > i)
			i = ipx_sockets[NS_SERVER];
	}
	timeout.tv_sec = msec/1000;
	timeout.tv_usec = (msec%1000)*1000;
	i = max(ip_sockets[NS_SERVER],ip6_sockets[NS_SERVER]);
	i = max(i,ipx_sockets[NS_SERVER]);
	select(i+1, &fdset, NULL, NULL, &timeout);
}

//===================================================================


static WSADATA		winsockdata;

/**
 * @brief
 */
void NET_Init (void)
{
	WORD	wVersionRequested;
	int		r;

	wVersionRequested = MAKEWORD(1, 1);

	r = WSAStartup(MAKEWORD(1, 1), &winsockdata);

	if (r)
		Com_Error(ERR_FATAL,"Winsock initialization failed.");

	Com_Printf("Winsock Initialized\n");

	noudp = Cvar_Get("noudp", "0", CVAR_NOSET, NULL);
	noipx = Cvar_Get("noipx", "0", CVAR_NOSET, NULL);

	net_shownet = Cvar_Get("net_shownet", "0", 0, NULL);
}


/**
 * @brief
 */
void NET_Shutdown (void)
{
	/* close sockets */
	NET_Config(qfalse);

	WSACleanup();
}


/**
 * @brief
 */
char *NET_ErrorString (void)
{
	int		code;

	code = WSAGetLastError();
	switch (code) {
	case WSAEINTR: return "WSAEINTR";
	case WSAEBADF: return "WSAEBADF";
	case WSAEACCES: return "WSAEACCES";
	case WSAEDISCON: return "WSAEDISCON";
	case WSAEFAULT: return "WSAEFAULT";
	case WSAEINVAL: return "WSAEINVAL";
	case WSAEMFILE: return "WSAEMFILE";
	case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK";
	case WSAEINPROGRESS: return "WSAEINPROGRESS";
	case WSAEALREADY: return "WSAEALREADY";
	case WSAENOTSOCK: return "WSAENOTSOCK";
	case WSAEDESTADDRREQ: return "WSAEDESTADDRREQ";
	case WSAEMSGSIZE: return "WSAEMSGSIZE";
	case WSAEPROTOTYPE: return "WSAEPROTOTYPE";
	case WSAENOPROTOOPT: return "WSAENOPROTOOPT";
	case WSAEPROTONOSUPPORT: return "WSAEPROTONOSUPPORT";
	case WSAESOCKTNOSUPPORT: return "WSAESOCKTNOSUPPORT";
	case WSAEOPNOTSUPP: return "WSAEOPNOTSUPP";
	case WSAEPFNOSUPPORT: return "WSAEPFNOSUPPORT";
	case WSAEAFNOSUPPORT: return "WSAEAFNOSUPPORT";
	case WSAEADDRINUSE: return "WSAEADDRINUSE";
	case WSAEADDRNOTAVAIL: return "WSAEADDRNOTAVAIL";
	case WSAENETDOWN: return "WSAENETDOWN";
	case WSAENETUNREACH: return "WSAENETUNREACH";
	case WSAENETRESET: return "WSAENETRESET";
	case WSAEHOSTDOWN: return "WSAEHOSTDOWN";
	case WSAEHOSTUNREACH: return "WSAEHOSTUNREACH";
	case WSAECONNABORTED: return "WSWSAECONNABORTEDAEINTR";
	case WSAECONNRESET: return "WSAECONNRESET";
	case WSAENOBUFS: return "WSAENOBUFS";
	case WSAEISCONN: return "WSAEISCONN";
	case WSAENOTCONN: return "WSAENOTCONN";
	case WSAESHUTDOWN: return "WSAESHUTDOWN";
	case WSAETOOMANYREFS: return "WSAETOOMANYREFS";
	case WSAETIMEDOUT: return "WSAETIMEDOUT";
	case WSAECONNREFUSED: return "WSAECONNREFUSED";
	case WSAELOOP: return "WSAELOOP";
	case WSAENAMETOOLONG: return "WSAENAMETOOLONG";
	case WSAEHOSTDOWN: return "WSAEHOSTDOWN";
	case WSASYSNOTREADY: return "WSASYSNOTREADY";
	case WSAVERNOTSUPPORTED: return "WSAVERNOTSUPPORTED";
	case WSANOTINITIALISED: return "WSANOTINITIALISED";
	case WSAHOST_NOT_FOUND: return "WSAHOST_NOT_FOUND";
	case WSATRY_AGAIN: return "WSATRY_AGAIN";
	case WSANO_RECOVERY: return "WSANO_RECOVERY";
	case WSANO_DATA: return "WSANO_DATA";
	default: return "NO ERROR";
	}
}
