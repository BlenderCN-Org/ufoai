/**
 * @file net.c
 * @note This file should fully support ipv6 and any other protocol that is
 * compatible with the getaddrinfo interface, with the exception of
 * broadcast_datagram() which must be amended for each protocol (and
 * currently supports only ipv4)
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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


#include "qcommon.h"
#include "dbuffer.h"

#define MAX_STREAMS 56
#define MAX_DATAGRAM_SOCKETS 7

#ifdef _WIN32
# ifdef __MINGW32__
#  define _WIN32_WINNT 0x0501
# endif
# define FD_SETSIZE (MAX_STREAMS + 1)
# include <winsock2.h>
# include <ws2tcpip.h>
# define gai_strerrorA estr_n
#else
# define INVALID_SOCKET (-1)
typedef int SOCKET;
# include <sys/select.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <signal.h>
# include <netdb.h>
# include <arpa/inet.h>
# include <netinet/in.h>
# include <sys/time.h>
# include <unistd.h>
#endif /* WIN32 */
#include <errno.h>
#include <string.h>
#include <fcntl.h>

/**
 * AI_ADDRCONFIG, AI_ALL, and AI_V4MAPPED are available since glibc 2.3.3.
 * AI_NUMERICSERV is available since glibc 2.3.4.
 */
#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0
#endif
#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG 0
#endif

static cvar_t* net_ipv4;

struct net_stream {
	void *data;

	qboolean loopback;
	qboolean ready;
	qboolean closed;
	qboolean finished;
	int socket;
	int index;
	int family;
	int addrlen;

	struct dbuffer *inbound;
	struct dbuffer *outbound;

	stream_callback_func *func;
	struct net_stream *loopback_peer;
};

struct datagram {
	int len;
	char *msg;
	char *addr;
	struct datagram *next;
};

struct datagram_socket {
	int socket;
	int index;
	int family;
	int addrlen;
	struct datagram *queue;
	struct datagram **queue_tail;
	datagram_callback_func *func;
};

static fd_set read_fds;
static fd_set write_fds;
static int maxfd;
static struct net_stream *streams[MAX_STREAMS];
static struct datagram_socket *datagram_sockets[MAX_DATAGRAM_SOCKETS];

static qboolean loopback_ready = qfalse;
static qboolean server_running = qfalse;
static stream_callback_func *server_func = NULL;
static int server_socket = INVALID_SOCKET;
static int server_family, server_addrlen;

/**
 * @brief
 */
static int find_free_stream (void)
{
	static int start = 0;
	int i;

	for (i = 0; i < MAX_STREAMS; i++) {
		int pos = (i + start) % MAX_STREAMS;
		if (streams[pos] == NULL) {
			start = (pos + 1) % MAX_STREAMS;
			return pos;
		}
	}
	return -1;
}

static int find_free_datagram_socket (void)
{
	static int start = 0;
	int i;

	for (i = 0; i < MAX_DATAGRAM_SOCKETS; i++) {
		int pos = (i + start) % MAX_DATAGRAM_SOCKETS;
		if (datagram_sockets[pos] == NULL) {
			start = (pos + 1) % MAX_DATAGRAM_SOCKETS;
			return pos;
		}
	}
	return -1;
}

/**
 * @brief
 */
static struct net_stream *new_stream (int index)
{
	struct net_stream *s = malloc(sizeof(*s));
	s->data = NULL;
	s->loopback_peer = NULL;
	s->loopback = qfalse;
	s->closed = qfalse;
	s->finished = qfalse;
	s->ready = qfalse;
	s->socket = INVALID_SOCKET;
	s->inbound = NULL;
	s->outbound = NULL;
	s->index = index;
	s->family = 0;
	s->addrlen = 0;
	s->func = NULL;
	if (streams[index])
		free_stream(streams[index]);
	streams[index] = s;
	return s;
}

#ifdef _WIN32
/**
 * @brief
 */
static const char *estr_n (int code)
{
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

/**
 * @brief
 */
static const char *estr (void)
{
	return estr_n(WSAGetLastError());
}

static void close_socket (int socket)
{
	closesocket(socket);
}
#else
#define estr_n strerror
/**
 * @brief
 */
static const char *estr (void)
{
	return strerror(errno);
}

static void close_socket (int socket)
{
	close(socket);
}
#endif

/**
 * @brief
 */
void init_net (void)
{
	int i;

#ifdef _WIN32
	WSADATA winsockdata;
	if (WSAStartup(MAKEWORD(2, 0), &winsockdata) != 0)
		Com_Error(ERR_FATAL,"Winsock initialization failed.");
#endif

	maxfd = 0;
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);

	for (i = 0; i < MAX_STREAMS; i++)
		streams[i] = NULL;
	for (i = 0; i < MAX_DATAGRAM_SOCKETS; i++)
		datagram_sockets[i] = NULL;

#ifdef _WIN32

#else
	signal(SIGPIPE, SIG_IGN);
#endif

	net_ipv4 = Cvar_Get("net_ipv4", "1", CVAR_ARCHIVE, "Only use ipv4");
}

/**
 * @brief
 * @sa stream_finished
 */
static void close_stream (struct net_stream *s)
{
	if (!s || s->closed)
		return;

	if (s->socket != INVALID_SOCKET) {
		if (s->outbound && dbuffer_len(s->outbound))
			Com_Printf("The outbound buffer for this socket (%d) is not empty\n", s->socket);
		else if (s->inbound && dbuffer_len(s->inbound))
			Com_Printf("The inbound buffer for this socket (%d) is not empty\n", s->socket);

		FD_CLR(s->socket, &read_fds);
		FD_CLR(s->socket, &write_fds);
		close_socket(s->socket);
		s->socket = INVALID_SOCKET;
	}
	if (s->index >= 0)
		streams[s->index] = NULL;

	if (s->loopback_peer) {
		/* Detach the peer, so that it won't send us anything more */
		s->loopback_peer->outbound = NULL;
		s->loopback_peer->loopback_peer = NULL;
	}

	s->closed = qtrue;

	/* If we have a loopback peer, don't free the outbound buffer,
	 * because it's our peer's inbound buffer */
	if (!s->loopback_peer)
		free_dbuffer(s->outbound);

	s->outbound = NULL;
	s->socket = INVALID_SOCKET;

	/* Note that s is potentially invalid after the callback returns */
	if (s->finished) {
		free_dbuffer(s->inbound);
		free(s);
	} else if (s->func)
		s->func(s);
}

/**
 * @brief
 */
static void do_accept (int sock)
{
	int index = find_free_stream();
	struct net_stream *s;
	if (index == -1) {
		Com_Printf("Too many streams open, rejecting inbound connection\n");
		close_socket(sock);
		return;
	}

	s = new_stream(index);
	s->socket = sock;
	s->inbound = new_dbuffer();
	s->outbound = new_dbuffer();
	s->family = server_family;
	s->addrlen = server_addrlen;
	s->func = server_func;

	maxfd = max(sock + 1, maxfd);
	FD_SET(sock, &read_fds);

	server_func(s);
}

/**
 * @brief
 * @sa Qcommon_Frame
 */
void wait_for_net (int timeout)
{
	struct timeval tv;
	int ready;
	int i;

	fd_set read_fds_out;
	fd_set write_fds_out;

	memcpy(&read_fds_out, &read_fds, sizeof(read_fds_out));
	memcpy(&write_fds_out, &write_fds, sizeof(write_fds_out));

	/* select() won't notice that loopback streams are ready, so we'll
	* eliminate the delay directly */
	if (loopback_ready)
		timeout = 0;

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = 1000 * (timeout % 1000);
#ifdef _WIN32
	if (read_fds_out.fd_count == 0) {
		Sleep(timeout);
		ready = 0;
	} else
#endif
		ready = select(maxfd, &read_fds_out, &write_fds_out, NULL, &tv);

	if (ready == -1)
		Sys_Error("select failed: %s\n", estr());

	if (ready == 0 && !loopback_ready)
		return;

	if (server_socket != INVALID_SOCKET && FD_ISSET(server_socket, &read_fds_out)) {
		int client_socket = accept(server_socket, NULL, 0);
		if (client_socket == INVALID_SOCKET) {
			if (errno != EAGAIN)
				Com_Printf("accept on socket %d failed: %s\n", server_socket, estr());
		} else
			do_accept(client_socket);
	}

	for (i = 0; i < MAX_STREAMS; i++) {
		struct net_stream *s = streams[i];

		if (!s)
			continue;

		if (s->loopback) {
			/* Note that s is potentially invalid after the callback returns - we'll close dead streams on the next pass */
			if (s->ready && s->func)
				s->func(s);
			/* If the peer is gone and the buffer is empty, close the stream */
			else if (!s->loopback_peer && stream_length(s) == 0)
				close_stream(s);

			continue;
		}

		if (s->socket == INVALID_SOCKET)
			continue;

		if (FD_ISSET(s->socket, &write_fds_out)) {
			char buf[4096];
			int len;

			if (dbuffer_len(s->outbound) == 0) {
				FD_CLR(s->socket, &write_fds);

				/* Finished streams are closed when their outbound queues empty */
				if (s->finished)
					close_stream(s);

				continue;
			}

			len = dbuffer_get(s->outbound, buf, sizeof(buf));
			len = send(s->socket, buf, len, 0);

			if (len < 0) {
				Com_Printf("write on socket %d failed: %s\n", s->socket, estr());
				close_stream(s);
				continue;
			}

			Com_Printf("wrote %d bytes to stream %d (%s)\n", len, i, stream_peer_name(s, buf, sizeof(buf), qfalse));

			dbuffer_remove(s->outbound, len);
		}

		if (FD_ISSET(s->socket, &read_fds_out)) {
			char buf[4096];
			int len = recv(s->socket, buf, sizeof(buf), 0);
			if (len <= 0) {
				if (len == -1)
					Com_Printf("read on socket %d failed: %s\n", s->socket, estr());
				close_stream(s);
				continue;
			} else {
				if (s->inbound) {
					dbuffer_add(s->inbound, buf, len);

					Com_Printf("read %d bytes from stream %d (%s)\n", len, i, stream_peer_name(s, buf, sizeof(buf), qfalse));

					/* Note that s is potentially invalid after the callback returns */
					if (s->func)
						s->func(s);

					continue;
				}
			}
		}
	}

	for (i = 0; i < MAX_DATAGRAM_SOCKETS; i++) {
		struct datagram_socket *s = datagram_sockets[i];

		if (!s)
			continue;

		if (FD_ISSET(s->socket, &write_fds_out)) {
			if (s->queue) {
				struct datagram *dgram = s->queue;
				int len = sendto(s->socket, dgram->msg, dgram->len, 0, (struct sockaddr *)dgram->addr, s->addrlen);
				if (len == -1)
					Com_Printf("sendto on socket %d failed: %s\n", s->socket, estr());
				/* Regardless of whether it worked, we don't retry datagrams */
				s->queue = dgram->next;
				free(dgram->msg);
				free(dgram->addr);
				free(dgram);
				if (!s->queue)
					s->queue_tail = &s->queue;
			} else {
				FD_CLR(s->socket, &write_fds);
			}
		}

		if (FD_ISSET(s->socket, &read_fds_out)) {
			char buf[256];
			char addrbuf[256];
			socklen_t addrlen = sizeof(addrbuf);
			int len = recvfrom(s->socket, buf, sizeof(buf), 0, (struct sockaddr *)addrbuf, &addrlen);
			if (len == -1)
				Com_Printf("recvfrom on socket %d failed: %s\n", s->socket, estr());
			else
				s->func(s, buf, len, (struct sockaddr *)addrbuf);
		}
	}

	loopback_ready = qfalse;
}

/**
 * @brief
 */
static qboolean set_non_blocking (int socket)
{
#ifdef _WIN32
	unsigned long t = 1;
	if (ioctlsocket(socket, FIONBIO, &t) == -1) {
		Com_Printf("ioctl FIONBIO failed: %s\n", strerror(errno));
		return qfalse;
	}
#else
	if (fcntl(socket, F_SETFL, O_NONBLOCK) == -1) {
		Com_Printf("fcntl F_SETFL failed: %s\n", strerror(errno));
		return qfalse;
	}
#endif
	return qtrue;
}

/**
 * @brief
 */
static struct net_stream *do_connect (const char *node, const char *service, const struct addrinfo *addr, int i)
{
	struct net_stream *s;
	SOCKET sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if (sock == INVALID_SOCKET) {
		Com_Printf("Failed to create socket: %s\n", estr());
		return NULL;
	}

	if (!set_non_blocking(sock)) {
		close_socket(sock);
		return NULL;
	}

	if (connect(sock, addr->ai_addr, addr->ai_addrlen) != 0) {
#ifdef _WIN32
		int err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK) {
			Com_Printf("Failed to start connection to %s:%s: %s\n", node, service, estr_n(err));
			closesocket(sock);
			return NULL;
		}
#else
		int err = errno;
		if (err != EINPROGRESS) {
			Com_Printf("Failed to start connection to %s:%s: %s\n", node, service, estr_n(err));
			close(sock);
			return NULL;
		}
#endif
	}

	s = new_stream(i);
	s->socket = sock;
	s->inbound = new_dbuffer();
	s->outbound = new_dbuffer();
	s->family = addr->ai_family;
	s->addrlen = addr->ai_addrlen;

	maxfd = max(sock + 1, maxfd);
	FD_SET(sock, &read_fds);

	return s;
}

/**
 * @brief
 */
struct net_stream *connect_to_host (const char *node, const char *service)
{
	struct addrinfo *res;
	struct addrinfo hints;
	int rc;
	struct net_stream *s = NULL;
	int index;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST | AI_ADDRCONFIG | AI_NUMERICSERV;
	hints.ai_socktype = SOCK_STREAM;

	rc = getaddrinfo(node, service, &hints, &res);

	if (rc != 0) {
		Com_Printf("Failed to resolve host %s:%s: %s\n", node, service, gai_strerror(rc));
		return NULL;
	}

	index = find_free_stream();
	if (index == -1) {
		Com_Printf("Failed to connect to host %s:%s, too many streams open\n", node, service);
		return NULL;
	}

	s = do_connect(node, service, res, index);

	freeaddrinfo(res);
	return s;
}

/**
 * @brief
 */
struct net_stream *connect_to_loopback (void)
{
	struct net_stream *client, *server;
	int server_index, client_index;

	if (!server_running)
		return NULL;

	server_index = find_free_stream();
	client_index = find_free_stream();

	if (server_index == -1 || client_index == -1 || server_index == client_index) {
		Com_Printf("Failed to connect to loopback server, too many streams open\n");
		return NULL;
	}

	client = new_stream(client_index);
	client->loopback = qtrue;
	client->inbound = new_dbuffer();
	client->outbound = new_dbuffer();

	server = new_stream(server_index);
	server->loopback = qtrue;
	server->inbound = client->outbound;
	server->outbound = client->inbound;
	server->func = server_func;

	client->loopback_peer = server;
	server->loopback_peer = client;

	server_func(server);

	return client;
}

/**
 * @brief Enqueue a network message into a stream
 * @sa stream_dequeue
 * @sa dbuffer_add
 */
void stream_enqueue (struct net_stream *s, const char *data, int len)
{
	if (len <= 0 || !s || s->closed || s->finished)
		return;

	if (s->outbound)
		dbuffer_add(s->outbound, data, len);

	if (s->socket >= 0)
		FD_SET(s->socket, &write_fds);

	if (s->loopback_peer) {
		loopback_ready = qtrue;
		s->loopback_peer->ready = qtrue;
	}
}

/**
 * @brief
 */
qboolean stream_closed (struct net_stream *s)
{
	return s ? (s->closed || s->finished) : qtrue;
}

/**
 * @brief
 */
int stream_length (struct net_stream *s)
{
	return (!s || !s->inbound) ? 0 : dbuffer_len(s->inbound);
}

/**
 * @brief Returns the length of the waiting inbound buffer
 * @sa dbuffer_get
 */
int stream_peek (struct net_stream *s, char *data, int len)
{
	if (len <= 0 || !s)
		return 0;

	if ((s->closed || s->finished) && (!s->inbound || dbuffer_len(s->inbound) == 0))
		return 0;

	return dbuffer_get(s->inbound, data, len);
}

/**
 * @brief
 * @sa stream_enqueue
 * @sa dbuffer_extract
 */
int stream_dequeue (struct net_stream *s, char *data, int len)
{
	if (len <= 0 || !s || s->finished)
		return 0;

	return dbuffer_extract(s->inbound, data, len);
}

/**
 * @brief
 */
void *stream_data (struct net_stream *s)
{
	return s ? s->data : NULL;
}

/**
 * @brief
 */
void set_stream_data (struct net_stream *s, void *data)
{
	if (!s)
		return;
	s->data = data;
}

/**
 * @brief
 * @sa close_stream
 * @sa stream_finished
 */
void free_stream (struct net_stream *s)
{
	if (!s)
		return;
	s->finished = qtrue;
	close_stream(s);
}

/**
 * @brief
 */
void stream_finished (struct net_stream *s)
{
	if (!s)
		return;

	s->finished = qtrue;

	if (s->socket >= 0)
		FD_CLR(s->socket, &read_fds);

	/* Stop the loopback peer from queueing stuff up in here */
	if (s->loopback_peer)
		s->loopback_peer->outbound = NULL;

	free_dbuffer(s->inbound);
	s->inbound = NULL;

	/* If there's nothing in the outbound buffer, any finished stream is
		ready to be closed */
	if (!s->outbound || dbuffer_len(s->outbound) == 0)
		close_stream(s);
}

/* Any code which calls this function with ip_hack set to true is
 * considered broken - it should not make assumptions about the format
 * of the result, and this function is only really intended for
 * displaying data to the user
 */

/**
 * @brief
 */
const char * stream_peer_name (struct net_stream *s, char *dst, int len, qboolean ip_hack)
{
	if (!s)
		return "(null)";
	else if (stream_is_loopback(s))
		return "loopback connection";
	else {
		char buf[128];
		char node[64];
		char service[64];
		int rc;
		socklen_t addrlen = s->addrlen;
		if (getpeername(s->socket, (struct sockaddr *)buf, &addrlen) != 0)
			return "(error)";

		rc = getnameinfo((struct sockaddr *)buf, addrlen, node, sizeof(node), service, sizeof(service),
				NI_NUMERICHOST | NI_NUMERICSERV);
		if (rc != 0) {
			Com_Printf("Failed to convert sockaddr to string: %s\n", gai_strerror(rc));
			return "(error)";
		}
		if (ip_hack)
			Com_sprintf(dst, len, "%s", node);
		else {
			node[sizeof(node) - 1] = '\0';
			service[sizeof(service) - 1] = '\0';
			Com_sprintf(dst, len, "[%s]:%s", node, service);
		}
		return dst;
	}
}

/**
 * @brief
 */
void stream_callback (struct net_stream *s, stream_callback_func *func)
{
	if (!s)
		return;
	s->func = func;
}

/**
 * @brief
 */
qboolean stream_is_loopback (struct net_stream *s)
{
	return s && s->loopback;
}

/**
 * @brief
 */
static int do_start_server (const struct addrinfo *addr)
{
	SOCKET sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	int t = 1;

	if (sock == INVALID_SOCKET) {
		Com_Printf("Failed to create socket: %s\n", estr());
		return INVALID_SOCKET;
	}

	if (!set_non_blocking(sock)) {
		close_socket(sock);
		return INVALID_SOCKET;
	}

#ifdef _WIN32
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &t, sizeof(t)) != 0) {
#else
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)) != 0) {
#endif
		Com_Printf("Failed to set SO_REUSEADDR on socket: %s\n", estr());
		close_socket(sock);
		return INVALID_SOCKET;
	}

	if (bind(sock, addr->ai_addr, addr->ai_addrlen) != 0) {
		Com_Printf("Failed to bind socket: %s\n", estr());
		close_socket(sock);
		return INVALID_SOCKET;
	}

	if (listen(sock, SOMAXCONN) != 0) {
		Com_Printf("Failed to listen on socket: %s\n", estr());
		close_socket(sock);
		return INVALID_SOCKET;
	}

	maxfd = max(sock + 1, maxfd);
	FD_SET(sock, &read_fds);
	server_family = addr->ai_family;
	server_addrlen = addr->ai_addrlen;

	return sock;
}

/**
 * @brief
 */
qboolean SV_Start (const char *node, const char *service, stream_callback_func *func)
{
	if (!func)
		return qfalse;

	if (server_running) {
		Com_Printf("SV_Start: Server is still running - call SV_Stop before\n");
		return qfalse;
	}

	if (service) {
		struct addrinfo *res;
		struct addrinfo hints;
		int rc;

		memset(&hints, 0, sizeof(hints));
		hints.ai_flags = AI_NUMERICHOST | AI_ADDRCONFIG | AI_NUMERICSERV | AI_PASSIVE;
		hints.ai_socktype = SOCK_STREAM;
		/* force ipv4 */
		if (net_ipv4->integer)
			hints.ai_family = AF_INET;

		rc = getaddrinfo(node, service, &hints, &res);

		if (rc != 0) {
			Com_Printf("Failed to resolve host %s:%s: %s\n", node ? node : "*", service, gai_strerror(rc));
			return qfalse;
		}

		server_socket = do_start_server(res);
		if (server_socket == INVALID_SOCKET) {
			Com_Printf("Failed to start server on %s:%s\n", node ? node : "*", service);
		} else {
			server_running = qtrue;
			server_func = func;
		}

		freeaddrinfo(res);
	} else {
		/* Loopback server only */
		server_running = qtrue;
		server_func = func;
	}

	return server_running;
}

/**
 * @brief
 */
void SV_Stop (void)
{
	server_running = qfalse;
	server_func = NULL;
	if (server_socket != INVALID_SOCKET) {
		FD_CLR(server_socket, &read_fds);
		close_socket(server_socket);
	}
	server_socket = INVALID_SOCKET;
}

/**
 * @brief
 * @sa new_datagram_socket
 */
static struct datagram_socket *do_new_datagram_socket (const struct addrinfo *addr)
{
	struct datagram_socket *s;
	SOCKET sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	int t = 1;
	int index = find_free_datagram_socket();

	if (index == -1) {
		Com_Printf("Too many datagram sockets open\n");
		return NULL;
	}

	if (sock == INVALID_SOCKET) {
		Com_Printf("Failed to create socket: %s\n", estr());
		return NULL;
	}

	if (!set_non_blocking(sock)) {
		close_socket(sock);
		return NULL;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &t, sizeof(t)) != 0) {
		Com_Printf("Failed to set SO_REUSEADDR on socket: %s\n", estr());
		close_socket(sock);
		return NULL;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *) &t, sizeof(t)) != 0) {
		Com_Printf("Failed to set SO_BROADCAST on socket: %s\n", estr());
		close_socket(sock);
		return NULL;
	}

	if (bind(sock, addr->ai_addr, addr->ai_addrlen) != 0) {
		Com_Printf("Failed to bind socket: %s\n", estr());
		close_socket(sock);
		return NULL;
	}

	maxfd = max(sock + 1, maxfd);
	FD_SET(sock, &read_fds);

	s = malloc(sizeof(*s));
	s->family = addr->ai_family;
	s->addrlen = addr->ai_addrlen;
	s->socket = sock;
	s->index = index;
	s->queue = NULL;
	s->queue_tail = &s->queue;
	s->func = NULL;
	datagram_sockets[index] = s;

	return s;
}

/**
 * @brief Opens a datagram socket (UDP)
 * @sa do_new_datagram_socket
 */
struct datagram_socket *new_datagram_socket (const char *node, const char *service, datagram_callback_func *func)
{
	struct datagram_socket *s;
	struct addrinfo *res;
	struct addrinfo hints;
	int rc;

	if (!service || !func)
		return NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST | AI_ADDRCONFIG | AI_NUMERICSERV | AI_PASSIVE;
	hints.ai_socktype = SOCK_DGRAM;

	rc = getaddrinfo(node, service, &hints, &res);

	if (rc != 0) {
		Com_Printf("Failed to resolve host %s:%s: %s\n", node ? node : "*", service, gai_strerror(rc));
		return qfalse;
	}

	s = do_new_datagram_socket(res);
	if (s)
		s->func = func;

	freeaddrinfo(res);
	return s;
}

/**
 * @brief
 * @sa new_datagram_socket
 */
void send_datagram (struct datagram_socket *s, const char *buf, int len, struct sockaddr *to)
{
	struct datagram *dgram;
	if (!s || len <= 0 || !buf || !to)
		return;

	dgram = malloc(sizeof(*dgram));
	dgram->msg = malloc(len);
	dgram->addr = malloc(s->addrlen);
	memcpy(dgram->msg, buf, len);
	memcpy(dgram->addr, to, len);
	dgram->len = len;
	dgram->next = NULL;

	*s->queue_tail = dgram;
	s->queue_tail = &dgram->next;

	FD_SET(s->socket, &write_fds);
}

/**
 * @brief
 * @sa send_datagram
 * @sa new_datagram_socket
 */
void broadcast_datagram (struct datagram_socket *s, const char *buf, int len, int port)
{
	if (s->family == AF_INET) {
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = INADDR_BROADCAST;
		send_datagram(s, buf, len, (struct sockaddr *)&addr);
	} else if (s->family == AF_INET6) {
		/* @todo: I'm not sure whether this is the correct approach - but it works */
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = INADDR_BROADCAST;
		send_datagram(s, buf, len, (struct sockaddr *)&addr);
	} else {
		Sys_Error("Broadcast unsupported on address family %d\n", s->family);
	}
}

/**
 * @brief
 * @sa new_datagram_socket
 * @sa do_new_datagram_socket
 */
void close_datagram_socket (struct datagram_socket *s)
{
	if (!s)
		return;

	FD_CLR(s->socket, &read_fds);
	FD_CLR(s->socket, &write_fds);
	close_socket(s->socket);

	while (s->queue) {
		struct datagram *dgram = s->queue;
		s->queue = dgram->next;
		free(dgram->msg);
		free(dgram->addr);
		free(dgram);
	}

	datagram_sockets[s->index] = NULL;
	free(s);
}

void sockaddr_to_strings (struct datagram_socket *s, struct sockaddr *addr, char *node, size_t nodelen, char *service, size_t servicelen)
{
	int rc = getnameinfo(addr, s->addrlen, node, nodelen, service, servicelen,
			NI_NUMERICHOST | NI_NUMERICSERV | NI_DGRAM);
	if (rc != 0) {
		Com_Printf("Failed to convert sockaddr to string: %s\n", gai_strerror(rc));
		Q_strncpyz(node, "(error)", nodelen);
		Q_strncpyz(service, "(error)", servicelen);
	}
	return;
}
