// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <chiaki/stoppipe.h>
#include <chiaki/sock.h>

#include <fcntl.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#elif defined(__PSVITA__)
#include <psp2/net/net.h>
#include <debugnet.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#endif

CHIAKI_EXPORT ChiakiErrorCode chiaki_stop_pipe_init(ChiakiStopPipe *stop_pipe)
{
#ifdef _WIN32
	stop_pipe->event = WSACreateEvent();
	if(stop_pipe->event == WSA_INVALID_EVENT)
		return CHIAKI_ERR_UNKNOWN;
#elif defined(__SWITCH__)
	// currently pipe or socketpare are not available on switch
	// use a custom udp socket as pipe

	// struct sockaddr_in addr;
	int addr_size = sizeof(stop_pipe->addr);

	stop_pipe->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(stop_pipe->fd < 0)
		return CHIAKI_ERR_UNKNOWN;
	stop_pipe->addr.sin_family = AF_INET;
	// bind to localhost
	stop_pipe->addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	// use a random port (dedicate one socket per object)
	stop_pipe->addr.sin_port = htons(0);
	// bind on localhostcreate UDP socket
	bind(stop_pipe->fd, (struct sockaddr *) &stop_pipe->addr, addr_size);
	// listen
	getsockname(stop_pipe->fd, (struct sockaddr *) &stop_pipe->addr, &addr_size);
	int r = fcntl(stop_pipe->fd, F_SETFL, O_NONBLOCK);
	if(r == -1)
	{
		close(stop_pipe->fd);
		return CHIAKI_ERR_UNKNOWN;
	}
#elif defined(__PSVITA__)
	int addr_size = sizeof(stop_pipe->addr);
	stop_pipe->fd = sceNetSocket("", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, SCE_NET_IPPROTO_UDP);
	if (stop_pipe->fd < 0)
		return CHIAKI_ERR_UNKNOWN;
	stop_pipe->addr.sin_family = AF_INET;
	stop_pipe->addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	stop_pipe->addr.sin_port = htons(0);
	int r = sceNetBind(stop_pipe->fd, (SceNetSockaddr *) &stop_pipe->addr, (unsigned int) addr_size);
	if (r < 0) {
		debugNetPrintf(DEBUG, "stoppipe: Failed to bind stop pipe fd %d (error was 0x%x).\n", stop_pipe->fd, r);
		return CHIAKI_ERR_UNKNOWN;
	}
	sceNetGetsockname(stop_pipe->fd, (SceNetSockaddr *) &stop_pipe->addr, (unsigned int*) &addr_size);
	int val = 1;
	r = sceNetSetsockopt(stop_pipe->fd, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &val, sizeof(val));
	if(r < 0) {
		debugNetPrintf(DEBUG, "stoppipe: Failed to make stop pipe socket %d nonblocking (error was 0x%x).\n", stop_pipe->fd, r);
		sceNetSocketClose(stop_pipe->fd);
		return CHIAKI_ERR_UNKNOWN;
	}

	stop_pipe->epoll_fd = sceNetEpollCreate("chiaki-epoll", 0);
	if (stop_pipe->epoll_fd < 0) {
		debugNetPrintf(DEBUG, "stoppipe: Failed to create epoll fd (error was 0x%x).\n", stop_pipe->epoll_fd);
		return CHIAKI_ERR_UNKNOWN;
	}
#else
	int r = pipe(stop_pipe->fds);
	if(r < 0)
		return CHIAKI_ERR_UNKNOWN;
	r = fcntl(stop_pipe->fds[0], F_SETFL, O_NONBLOCK);
	if(r == -1)
	{
		close(stop_pipe->fds[0]);
		close(stop_pipe->fds[1]);
		return CHIAKI_ERR_UNKNOWN;
	}
#endif
	return CHIAKI_ERR_SUCCESS;
}

CHIAKI_EXPORT void chiaki_stop_pipe_fini(ChiakiStopPipe *stop_pipe)
{
#ifdef _WIN32
	WSACloseEvent(stop_pipe->event);
#elif defined(__SWITCH__)
	close(stop_pipe->fd);
#elif defined(__PSVITA__)
	sceNetSocketClose(stop_pipe->fd);
	sceNetEpollDestroy(stop_pipe->epoll_fd);
#else
	close(stop_pipe->fds[0]);
	close(stop_pipe->fds[1]);
#endif
}

CHIAKI_EXPORT void chiaki_stop_pipe_stop(ChiakiStopPipe *stop_pipe)
{
#ifdef _WIN32
	WSASetEvent(stop_pipe->event);
#elif defined(__SWITCH__)
	// send to local socket (FIXME MSG_CONFIRM)
	sendto(stop_pipe->fd, "\x00", 1, 0,
		(struct sockaddr*)&stop_pipe->addr, sizeof(struct sockaddr_in));
#elif defined(__PSVITA__)
	debugNetPrintf(DEBUG, "Sending stop signal to stop pipe.\n");
	int r = sceNetSendto(stop_pipe->fd, "\x00", 1, 0, (SceNetSockaddr*)&stop_pipe->addr, sizeof(stop_pipe->addr));
	if (r < 0) {
		debugNetPrintf(DEBUG, "stoppipe: Failed to send stop signal (error was 0x%x).\n", r);
	}
#else
	write(stop_pipe->fds[1], "\x00", 1);
#endif
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_stop_pipe_select_single(ChiakiStopPipe *stop_pipe, chiaki_socket_t fd, bool write, uint64_t timeout_ms)
{
#ifdef _WIN32
	WSAEVENT events[2];
	DWORD events_count = 1;
	events[0] = stop_pipe->event;

	if(!CHIAKI_SOCKET_IS_INVALID(fd))
	{
		events_count = 2;
		events[1] = WSACreateEvent();
		if(events[1] == WSA_INVALID_EVENT)
			return CHIAKI_ERR_UNKNOWN;
		WSAEventSelect(fd, events[1], write ? FD_WRITE : FD_READ);
	}

	DWORD r = WSAWaitForMultipleEvents(events_count, events, FALSE, timeout_ms == UINT64_MAX ? WSA_INFINITE : (DWORD)timeout_ms, FALSE);

	if(events_count == 2)
		WSACloseEvent(events[1]);

	switch(r)
	{
		case WSA_WAIT_EVENT_0:
			return CHIAKI_ERR_CANCELED;
		case WSA_WAIT_EVENT_0+1:
			return CHIAKI_ERR_SUCCESS;
		case WSA_WAIT_TIMEOUT:
			return CHIAKI_ERR_TIMEOUT;
		default:
			return CHIAKI_ERR_UNKNOWN;
	}
#elif defined(__PSVITA__)
	SceNetEpollEvent ev = {0};
	ev.events = SCE_NET_EPOLLIN | SCE_NET_EPOLLHUP;
	ev.data.fd = stop_pipe->fd;
	debugNetPrintf(DEBUG, "stoppipe: Adding stop pipe socket %d to epoll control (epoll_fd=%d).\n", stop_pipe->fd, stop_pipe->epoll_fd);
	int r = sceNetEpollControl(stop_pipe->epoll_fd, SCE_NET_EPOLL_CTL_ADD, stop_pipe->fd, &ev);
	if (r < 0) {
		debugNetPrintf(DEBUG, "stoppipe: Oooops during epoll add for stop fd! (error was: 0x%x)\n", r);
		return CHIAKI_ERR_UNKNOWN;
	}

	if (!CHIAKI_SOCKET_IS_INVALID(fd)) {
		ev.events = (write ? SCE_NET_EPOLLOUT : SCE_NET_EPOLLIN) | SCE_NET_EPOLLHUP;
		ev.data.fd = fd;
		debugNetPrintf(DEBUG, "stoppipe: Adding socket %d to epoll control (epoll_fd=%d).\n", fd, stop_pipe->epoll_fd);
		r = sceNetEpollControl(stop_pipe->epoll_fd, SCE_NET_EPOLL_CTL_ADD, fd, &ev);
		if (r < 0) {
			debugNetPrintf(DEBUG, "stoppipe: Oooops during epoll add for regular fd! (error was: 0x%x)\n", r);
			return CHIAKI_ERR_UNKNOWN;
		}
	}

	if (r == 0) {
		debugNetPrintf(DEBUG, "stoppipe: Checking for stop signal for %llums.\n", timeout_ms);
		r = sceNetEpollWait(stop_pipe->epoll_fd, &ev, 1, timeout_ms * 1000);
		debugNetPrintf(DEBUG, "stoppipe: Done checking, got an event for, evt=0x%x, fd=0x%x\n", ev.events, ev.data.fd);
	}

	sceNetEpollControl(stop_pipe->epoll_fd, SCE_NET_EPOLL_CTL_DEL, stop_pipe->fd, NULL);
	if (!CHIAKI_SOCKET_IS_INVALID(fd)) {
		sceNetEpollControl(stop_pipe->epoll_fd, SCE_NET_EPOLL_CTL_DEL, fd, NULL);
	}

	if(r < 0) {
		debugNetPrintf(DEBUG, "stoppipe: Oooops during epoll wait! (error was: 0x%x)\n", r);
		return CHIAKI_ERR_UNKNOWN;
	}

	if (ev.data.fd == stop_pipe->fd) {
		debugNetPrintf(DEBUG, "stoppipe: Looks like we were canceled!\n");
		return CHIAKI_ERR_CANCELED;
	} else if (ev.data.fd == fd) {
		debugNetPrintf(DEBUG, "stoppipe: Success!\n");
		return CHIAKI_ERR_SUCCESS;
	}

	debugNetPrintf(DEBUG, "stoppipe: Timeout (rc was 0x%x, event.data.fd was 0x%x, was waiting for 0x%x or 0x%x)\n", r, ev.data.fd, stop_pipe->fd, fd);
	return CHIAKI_ERR_TIMEOUT;
#else
	fd_set rfds;
	FD_ZERO(&rfds);
#if defined(__SWITCH__)
	// push udp local socket as fd
	int stop_fd = stop_pipe->fd;
#else
	int stop_fd = stop_pipe->fds[0];
#endif
	FD_SET(stop_fd, &rfds);
	int nfds = stop_fd;


	fd_set wfds;
	FD_ZERO(&wfds);
	if(!CHIAKI_SOCKET_IS_INVALID(fd))
	{
		FD_SET(fd, write ? &wfds : &rfds);
		if(fd > nfds)
			nfds = fd;
	}
	nfds++;

	struct timeval timeout_s;
	struct timeval *timeout = NULL;
	if(timeout_ms != UINT64_MAX)
	{
		timeout_s.tv_sec = timeout_ms / 1000;
		timeout_s.tv_usec = (timeout_ms % 1000) * 1000;
		timeout = &timeout_s;
	}
	int r = select(nfds, &rfds, write ? &wfds : NULL, NULL, timeout);

	if(r < 0)
		return CHIAKI_ERR_UNKNOWN;

	if(FD_ISSET(stop_fd, &rfds))
		return CHIAKI_ERR_CANCELED;

	if(!CHIAKI_SOCKET_IS_INVALID(fd) && FD_ISSET(fd, write ? &wfds : &rfds))
		return CHIAKI_ERR_SUCCESS;

	return CHIAKI_ERR_TIMEOUT;
#endif
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_stop_pipe_connect(ChiakiStopPipe *stop_pipe, chiaki_socket_t fd, struct sockaddr *addr, size_t addrlen)
{
	#ifdef __PSVITA__
	int r = sceNetConnect(fd, (SceNetSockaddr*) addr, addrlen);
	int errno = r;
	#else
	int r = connect(fd, addr, (socklen_t)addrlen);
	#endif
	if(r >= 0)
		return CHIAKI_ERR_SUCCESS;

	if(CHIAKI_SOCKET_EINPROGRESS)
	{
		ChiakiErrorCode err = chiaki_stop_pipe_select_single(stop_pipe, fd, true, UINT64_MAX);
		if(err != CHIAKI_ERR_SUCCESS)
			return err;
	}
	else
	{
#ifdef _WIN32
		int err = WSAGetLastError();
		if(err == WSAECONNREFUSED)
			return CHIAKI_ERR_CONNECTION_REFUSED;
		else
			return CHIAKI_ERR_NETWORK;
#elif defined(__PSVITA__)
	if (r == SCE_NET_ERROR_ECONNREFUSED)
			return CHIAKI_ERR_CONNECTION_REFUSED;
	else
		return CHIAKI_ERR_NETWORK;
#else
		if(errno == ECONNREFUSED)
			return CHIAKI_ERR_CONNECTION_REFUSED;
		else
			return CHIAKI_ERR_NETWORK;
#endif
	}

	struct sockaddr peer;
	socklen_t peerlen = sizeof(peer);
	if(getpeername(fd, &peer, &peerlen) == 0)
		return CHIAKI_ERR_SUCCESS;

	#ifdef __PSVITA__
	if (r == SCE_NET_ERROR_ENOTCONN)
	#else
	if(errno != ENOTCONN)
	#endif
		return CHIAKI_ERR_UNKNOWN;

#ifdef _WIN32
	DWORD sockerr;
#else
	int sockerr;
#endif
#ifdef __PSVITA__
	socklen_t sockerr_sz = sizeof(sockerr);
	if(sceNetGetsockopt(fd, SCE_NET_SOL_SOCKET, SCE_NET_SO_ERROR, &sockerr, &sockerr_sz) < 0)
		return CHIAKI_ERR_UNKNOWN;
#else
	socklen_t sockerr_sz = sizeof(sockerr);
	if(getsockopt(fd, SOL_SOCKET, SO_ERROR, &sockerr, &sockerr_sz) < 0)
		return CHIAKI_ERR_UNKNOWN;
#endif

#ifdef _WIN32
	switch(sockerr)
	{
		case WSAETIMEDOUT:
			return CHIAKI_ERR_TIMEOUT;
		case WSAECONNREFUSED:
			return CHIAKI_ERR_CONNECTION_REFUSED;
		case WSAEHOSTDOWN:
			return CHIAKI_ERR_HOST_DOWN;
		case WSAEHOSTUNREACH:
			return CHIAKI_ERR_HOST_UNREACH;
		default:
			return CHIAKI_ERR_UNKNOWN;
	}
#elif defined(__PSVITA__)
	switch (sockerr) {
		case SCE_NET_ERROR_ETIMEDOUT:
			return CHIAKI_ERR_TIMEOUT;
		case SCE_NET_ERROR_ECONNREFUSED:
			return CHIAKI_ERR_CONNECTION_REFUSED;
		case SCE_NET_ERROR_EHOSTDOWN:
			return CHIAKI_ERR_HOST_DOWN;
		case SCE_NET_ERROR_EHOSTUNREACH:
			return CHIAKI_ERR_HOST_UNREACH;
		default:
			return CHIAKI_ERR_UNKNOWN;
	}
#else
	switch(sockerr)
	{
		case ETIMEDOUT:
			return CHIAKI_ERR_TIMEOUT;
		case ECONNREFUSED:
			return CHIAKI_ERR_CONNECTION_REFUSED;
		case EHOSTDOWN:
			return CHIAKI_ERR_HOST_DOWN;
		case EHOSTUNREACH:
			return CHIAKI_ERR_HOST_UNREACH;
		default:
			return CHIAKI_ERR_UNKNOWN;
	}
#endif
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_stop_pipe_reset(ChiakiStopPipe *stop_pipe)
{
#ifdef _WIN32
	BOOL r = WSAResetEvent(stop_pipe->event);
	return r ? CHIAKI_ERR_SUCCESS : CHIAKI_ERR_UNKNOWN;
#elif defined(__SWITCH__)
	//FIXME
	uint8_t v;
	int r;
	while((r = read(stop_pipe->fd, &v, sizeof(v))) > 0);
	return r < 0 ? CHIAKI_ERR_UNKNOWN : CHIAKI_ERR_SUCCESS;
#elif defined(__PSVITA__)
	int r;
	uint8_t v;
	while((r = sceNetRecv(stop_pipe->fd, &v, sizeof(v), 0)) > 0);
	return r < 0 ? CHIAKI_ERR_UNKNOWN : CHIAKI_ERR_SUCCESS;
#else
	uint8_t v;
	int r;
	while((r = read(stop_pipe->fds[0], &v, sizeof(v))) > 0);
	return r < 0 ? CHIAKI_ERR_UNKNOWN : CHIAKI_ERR_SUCCESS;
#endif
}
