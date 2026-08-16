#pragma once

#include <assert.h>
#include <stddef.h>
#include <unistd.h>

#include <ev/errno.h>
#include <ev/queue.h>
#include <ev/filelist.h>
#include <ev/io.h>
#include <ev/ioq.h>

#include "./pollish.h" // IWYU pragma: export

#include "../queue.c"
#include "./impl.c"

static evi_pl_evn_mask_t _evi_pl_tomask(evi_pl_kind_t kind) {
	switch (kind) {
		case EVI_PL_READ:
		case EVI_PL_PREAD:
		case EVI_PL_ACCEPT:
			return EVI_PL_READABLE;
		case EVI_PL_WRITE:
		case EVI_PL_PWRITE:
			return EVI_PL_WRITABLE;
		default: return 0;
	}
}

static ev_code_t _evi_pl_req_stop(ev_req_t req);

static void _evi_pl_req_cancel(ev_req_t req) {
	_evi_pl_req_stop(req);
	evi_req_end(req, EV_EINTR);
}
static ev_req_t _evi_pl_get_req(ev_fd_t fd, evi_pl_evn_mask_t ready) {
	for (ev_req_t i = fd->head; i; i = i->running.ioq.next) {
		if (_evi_pl_tomask(i->running.ioq.kind) & ready) return i;
	}

	return NULL;
}
static ev_code_t _evi_pl_req_do(ev_req_t req) {
	switch (req->running.ioq.kind) {
		case EVI_PL_READ: return ev_read(req->running.ioq.fd, req->running.ioq.rw.buff, req->running.ioq.rw.pn);
		case EVI_PL_WRITE: return ev_write(req->running.ioq.fd, req->running.ioq.rw.buff, req->running.ioq.rw.pn);
		case EVI_PL_PREAD: return ev_file_read(req->running.ioq.fd, req->running.ioq.rw.buff, req->running.ioq.rw.pn, req->running.ioq.rw.ptr);
		case EVI_PL_PWRITE: return ev_file_write(req->running.ioq.fd, req->running.ioq.rw.buff, req->running.ioq.rw.pn, req->running.ioq.rw.ptr);
		case EVI_PL_ACCEPT: return ev_socket_accept(
			req->running.ioq.accept.fl,
			req->running.ioq.fd,
			req->running.ioq.accept.pclient,
			req->running.ioq.accept.paddr,
			req->running.ioq.accept.pport
		);
		default: return EV_ENOTSUP;
	}
}
static ev_code_t _evi_pl_req_start(ev_req_t req) {
	ev_fd_t fd = req->running.ioq.fd;

	switch (_evi_pl_tomask(req->running.ioq.kind)) {
		case EVI_PL_READABLE: fd->impl.ioq.read_n++; break;
		case EVI_PL_WRITABLE: fd->impl.ioq.write_n++; break;
	}

	evi_pl_evn_mask_t mask = 0;
	if (fd->impl.ioq.read_n) mask |= EVI_PL_READABLE;
	if (fd->impl.ioq.write_n) mask |= EVI_PL_WRITABLE;

	ev_code_t code = evi_pl_impl_setmask(req->queue, fd, fd->impl.fd, mask);
	evi_req_begin(req, _evi_pl_req_cancel);

	// The handle doesn't support epoll, it must go thru the sync route
	if (code == EV_EPERM) {
		evi_req_end(req, _evi_pl_req_do(req));
		return EV_OK;
	}

	return code;
}
static ev_code_t _evi_pl_req_stop(ev_req_t req) {
	ev_fd_t fd = req->running.ioq.fd;

	switch (_evi_pl_tomask(req->running.ioq.kind)) {
		case EVI_PL_READABLE: fd->impl.ioq.read_n--; break;
		case EVI_PL_WRITABLE: fd->impl.ioq.write_n--; break;
	}

	evi_pl_evn_mask_t mask = 0;
	if (fd->impl.ioq.read_n) mask |= EVI_PL_READABLE;
	if (fd->impl.ioq.write_n) mask |= EVI_PL_WRITABLE;

	return evi_pl_impl_setmask(req->queue, fd, fd->impl.fd, mask);
}

static ev_code_t evi_pl_init(evi_pl_t pl, ev_queue_t queue) {
	ev_code_t code;

	int msg_pipe[2];
	if (pipe(msg_pipe) < 0) { code = evi_unix_conv_errno(errno); goto fail_pipe; }
	if (fcntl(msg_pipe[0], F_SETFD, O_NONBLOCK) < 0) { code = evi_unix_conv_errno(errno); goto fail_fcntl; }
	if (fcntl(msg_pipe[1], F_SETFD, O_NONBLOCK) < 0) { code = evi_unix_conv_errno(errno); goto fail_fcntl; }

	pl->notify_read = msg_pipe[0];
	pl->notify_write = msg_pipe[1];

	if ((code = evi_pl_impl_setmask(queue, NULL, pl->notify_read, EVI_PL_READABLE)) != EV_OK) goto fail_setmask;

	return EV_OK;
fail_setmask:
fail_fcntl:
	close(msg_pipe[0]);
	close(msg_pipe[1]);
fail_pipe:
	return evi_unix_conv_errno(errno);
}
static ev_code_t evi_pl_free(evi_pl_t pl) {
	close(pl->notify_read);
	close(pl->notify_write);
	return EV_OK;
}
static ev_code_t evi_queue_impl_notify(ev_queue_t queue) {
	if (write(queue->impl.pl.notify_write, &(uint8_t) { 0 }, sizeof(uint8_t)) < 0) {
		if (errno != EWOULDBLOCK) return evi_unix_conv_errno(errno);
	}

	return EV_OK;
}

ev_code_t (ev_queue_poll)(ev_queue_t queue, const ev_time_t *deadline, ev_req_t *preq, ev_code_t *pcode) {
	while (true) {
		ev_req_t queue_req = evi_queue_pop(queue, pcode);
		if (queue_req) {
			*preq = queue_req;
			return EV_OK;
		}

		void *udata;
		evi_pl_evn_mask_t ready;

		ev_code_t err = evi_pl_impl_poll(queue, deadline, &udata, &ready);
		if (err != EV_OK) return err;

		ev_fd_t fd = (ev_fd_t)udata;
		if (!fd) continue;

		ev_req_t req = _evi_pl_get_req(fd, ready);
		if (!req) continue;

		_evi_pl_req_stop(req);
		evi_req_kill(req);

		*pcode = _evi_pl_req_do(req);
		*preq = req;
		return EV_OK;
	}
}

ev_code_t (evq_read)(ev_req_t req, ev_fd_t fd, char *buff, size_t *pn) {
	if (!evi_unix_isfd(fd)) return EV_EBADF;

	req->running.ioq.kind = EVI_PL_READ;
	req->running.ioq.fd = fd;

	req->running.ioq.rw.buff = buff;
	req->running.ioq.rw.pn = pn;

	return _evi_pl_req_start(req);
}
ev_code_t (evq_write)(ev_req_t req, ev_fd_t fd, char *buff, size_t *pn) {
	if (!evi_unix_isfd(fd)) return EV_EBADF;

	req->running.ioq.kind = EVI_PL_WRITE;
	req->running.ioq.fd = fd;

	req->running.ioq.rw.buff = buff;
	req->running.ioq.rw.pn = pn;

	return _evi_pl_req_start(req);
}
ev_code_t (evq_file_read)(ev_req_t req, ev_fd_t fd, char *buff, size_t *pn, size_t offset) {
	if (!evi_unix_isfd(fd)) return EV_EBADF;

	req->running.ioq.kind = EVI_PL_PREAD;
	req->running.ioq.fd = fd;

	req->running.ioq.rw.buff = buff;
	req->running.ioq.rw.pn = pn;
	req->running.ioq.rw.ptr = offset;

	return _evi_pl_req_start(req);
}
ev_code_t (evq_file_write)(ev_req_t req, ev_fd_t fd, char *buff, size_t *pn, size_t offset) {
	if (!evi_unix_isfd(fd)) return EV_EBADF;

	req->running.ioq.kind = EVI_PL_PWRITE;
	req->running.ioq.fd = fd;

	req->running.ioq.rw.buff = buff;
	req->running.ioq.rw.pn = pn;
	req->running.ioq.rw.ptr = offset;

	return _evi_pl_req_start(req);
}
ev_code_t (evq_socket_accept)(ev_req_t req, ev_filelist_t fl, ev_fd_t server, ev_fd_t *pclient, ev_addr_t *paddr, uint16_t *pport) {
	if (!evi_unix_isfd(server)) return EV_EBADF;

	req->running.ioq.kind = EVI_PL_ACCEPT;
	req->running.ioq.fd = server;

	req->running.ioq.accept.fl = fl;
	req->running.ioq.accept.pclient = pclient;
	req->running.ioq.accept.paddr = paddr;
	req->running.ioq.accept.pport = pport;

	return _evi_pl_req_start(req);
}
