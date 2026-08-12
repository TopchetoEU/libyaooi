#pragma once

#include <assert.h>
#include <unistd.h>

#include <ev/errno.h>
#include <ev/queue.h>
#include <ev/io.h>
#include <ev/ioq.h>

#include "./pollish.h"

#include "../../core/queue.h"
#include "./impl.h"
#include "./async.h"

#include "../../core/queue.c"
#include "./impl.c"
#include "./async.c"

static bool evi_pl_cb(evi_pl_evn_t evn, ev_req_t *preq) {
	if (evn.fd == ev->impl.async.pl.usermsg_read) {
		uint8_t dummy;
		read(ev->impl.async.pl.usermsg_read, &dummy, sizeof dummy);
		return false;
	}

	assert(evn.req != NULL);

	ev_hnd_t fd = evn.req->hnd;

	*preq = evn.req;

	switch (evn.req->args.type) {
		case EVI_READ:
			evn.req->res = evs_read(fd, evn->rw.data, evn->rw.pn);
			break;
	}

	switch (evn.kind) {
		case EVI_POLL_PREAD:
			*perr = evs_file_read(fd, evn->rw.data, evn->rw.pn, evn->rw.offset);
			break;
		case EVI_POLL_READ:

		case EVI_POLL_PWRITE:
			*perr = evs_file_write(fd, evn->rw.data, evn->rw.pn, evn->rw.offset);
			break;
		case EVI_POLL_WRITE:
			*perr = evs_write(fd, evn->rw.data, evn->rw.pn);
			break;
		case EVI_POLL_ACCEPT:
			*perr = evs_server_accept(evn->accept.pres, evn->accept.paddr, evn->accept.pport, (ev_server_t)(size_t)evn->fd);
			break;
	}

	evi_unix_freefd(fd);
	return true;
}

ev_code_t evi_pl_push(ev_t ev, void *udata, ev_code_t err) {
	ev_code_t code = evi_queue_push(ev, udata, err);
	if (code != EV_OK) return code;

	if (write(ev->async->pl->usermsg_write, &(uint8_t) { 0 }, sizeof(uint8_t)) < 0) {
		if (errno == EWOULDBLOCK) return EV_OK;
		return evi_unix_conv_errno(errno);
	}

	return EV_OK;
}

ev_code_t ev_push(ev_t ev, ev_req_t req, ev_code_t err) {
	ev_code_t code = evi_queue_push(ev, req, err);
	if (code != EV_OK) return code;

	if (write(ev->impl.async.pl.usermsg_write, &(uint8_t) { 0 }, sizeof(uint8_t)) < 0) {
		if (errno == EWOULDBLOCK) return EV_OK;
		return evi_unix_conv_errno(errno);
	}
	return EV_OK;
}
ev_code_t ev_queue_poll(ev_t ev, const ev_time_t *ptimeout, ev_req_t *pres) {
	while (true) {
		ev_req_t res = evi_queue_pop(ev);
		if (res) {
			ev_end(ev);
			*pres = res;
			return EV_OK;
		}

		switch (evi_pl_impl_poll(ev, ptimeout, pticket, perr)) {
			case EV_POLL_OK:
				ev_end(ev);
				return true;
			case EV_POLL_TIMEOUT:
				return false;
			case EV_POLL_EMPTY:
				break;
		}
	}
}

ev_code_t evq_read(ev_req_t req, ev_t ev, ev_hnd_t stream, char *buff, size_t *pn) {
	if (!evi_unix_isfd(stream)) return EV_EBADF;

	ev_begin(ev);

	ev_code_t code = evi_pl_impl_add(ev, (evi_pl_event_t) {
		.ticket = udata,
		.type = EVI_POLL_READ,
		.fd = evi_unix_fd(stream),
		.rw = { .data = buff, .pn = pn },
	});

	// The handle doesn't support epoll, it must go thru the sync route
	if (code == EV_EPERM) return ev_push(ev, udata, evs_read(stream, buff, pn));
	return code;
}
ev_code_t evq_write(ev_req_t req, ev_t ev, ev_hnd_t stream, char *buff, size_t *pn) {
	if (!evi_unix_isfd(stream)) return EV_EBADF;

	ev_begin(ev);

	ev_code_t code = evi_pl_impl_add(ev, (evi_pl_event_t) {
		.ticket = udata,
		.type = EVI_POLL_WRITE,
		.fd = evi_unix_fd(stream),
		.rw = { .data = buff, .pn = pn },
	});

	if (code == EV_EPERM) return ev_push(ev, udata, evs_write(stream, buff, pn));
	return code;
}
ev_code_t evq_file_read(ev_req_t req, ev_t ev, ev_hnd_t stream, char *buff, size_t *pn, size_t offset) {
	if (!evi_unix_isfd(stream)) return EV_EBADF;

	ev_begin(ev);

	ev_code_t code = evi_pl_impl_add(ev, (evi_pl_event_t) {
		.ticket = udata,
		.type = EVI_POLL_PREAD,
		.fd = evi_unix_fd(stream),
		.rw = { .data = buff, .pn = pn, .offset = offset },
	});

	if (code == EV_EPERM) return ev_push(ev, udata, evs_file_read(stream, buff, pn, offset));
	return code;
}
ev_code_t evq_file_write(ev_req_t req, ev_t ev, ev_hnd_t stream, char *buff, size_t *pn, size_t offset) {
	if (!evi_unix_isfd(stream)) return EV_EBADF;

	ev_begin(ev);

	ev_code_t code = evi_pl_impl_add(ev, (evi_pl_event_t) {
		.ticket = udata,
		.type = EVI_POLL_PWRITE,
		.fd = evi_unix_fd(stream),
		.rw = { .data = buff, .pn = pn, .offset = offset },
	});

	if (code == EV_EPERM) return ev_push(ev, udata, evs_file_write(stream, buff, pn, offset));
	return code;
}
ev_code_t evq_socket_accept(ev_req_t req, ev_t ev, ev_hnd_t server, ev_hnd_t client, ev_addr_t *paddr, uint16_t *pport) {
	ev_begin(ev);

	return evi_pl_impl_add(ev, (evi_pl_event_t) {
		.ticket = udata,
		.type = EVI_POLL_ACCEPT,
		.fd = (int)(size_t)server,
		.accept = { .pres = pres, .paddr = paddr, .pport = pport },
	});
}

static ev_code_t evi_async_init(ev_t ev) {
	ev_code_t code = evi_pl_impl_init(ev);
	if (code != EV_OK) return code;

	int msg_pipe[2];
	if (pipe(msg_pipe) < 0) goto fail;
	if (fcntl(msg_pipe[0], F_SETFD, O_NONBLOCK) < 0) goto fail_pipe;
	if (fcntl(msg_pipe[1], F_SETFD, O_NONBLOCK) < 0) goto fail_pipe;

	ev->async->pl->usermsg_read = msg_pipe[0];
	ev->async->pl->usermsg_write = msg_pipe[1];

	evi_pl_impl_add(ev, (evi_pl_event_t) {
		.type = EVI_POLL_READ,
		.fd = ev->async->pl->usermsg_read,
	});

	return EV_OK;

fail_pipe:
	close(msg_pipe[0]);
	close(msg_pipe[1]);
fail:
	return evi_unix_conv_errno(errno);
}
static ev_code_t evi_async_free(ev_t ev) {
	ev_code_t code = evi_pl_impl_free(ev);
	if (code != EV_OK) return code;

	close(ev->async->pl->usermsg_read);
	close(ev->async->pl->usermsg_write);
	return EV_OK;
}

#define EVI_ASYNC_READ
#define EVI_ASYNC_WRITE
#define EVI_ASYNC_FILE_READ
#define EVI_ASYNC_FILE_WRITE
#define EVI_ASYNC_SERVER_ACCEPT
