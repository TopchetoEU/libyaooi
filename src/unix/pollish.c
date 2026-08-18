#pragma once

#include <assert.h>
#include <stddef.h>
#include <unistd.h>

#include <yaioi/errno.h>
#include <yaioi/queue.h>
#include <yaioi/io.h>
#include <yaioi/ioq.h>

#include "./pollish.h" // IWYU pragma: export

#include "../queue.c"
#include "./impl.c"

static yoi_pl_evn_mask_t _yoi_pl_tomask(yoi_pl_kind_t kind) {
	switch (kind) {
		case YOI_PL_READ:
		case YOI_PL_PREAD:
		case YOI_PL_ACCEPT:
			return YOI_PL_READABLE;
		case YOI_PL_WRITE:
		case YOI_PL_PWRITE:
			return YOI_PL_WRITABLE;
		default: return 0;
	}
}

static yo_code_t _yoi_pl_req_stop(yo_req_t req);

static void _yoi_pl_req_cancel(yo_req_t req) {
	_yoi_pl_req_stop(req);
	yoi_req_end(req, YO_EINTR);
}
static yo_req_t _yoi_pl_get_req(yo_fd_t fd, yoi_pl_evn_mask_t ready) {
	for (yo_req_t i = fd->head; i; i = i->running.ioq.next) {
		if (_yoi_pl_tomask(i->running.ioq.kind) & ready) return i;
	}

	return NULL;
}
static yo_code_t _yoi_pl_req_do(yo_req_t req) {
	switch (req->running.ioq.kind) {
		case YOI_PL_READ: return yo_read(req->running.ioq.fd, req->running.ioq.rw.buff, req->running.ioq.rw.pn);
		case YOI_PL_WRITE: return yo_write(req->running.ioq.fd, req->running.ioq.rw.buff, req->running.ioq.rw.pn);
		case YOI_PL_PREAD: return yo_file_read(req->running.ioq.fd, req->running.ioq.rw.buff, req->running.ioq.rw.pn, req->running.ioq.rw.ptr);
		case YOI_PL_PWRITE: return yo_file_write(req->running.ioq.fd, req->running.ioq.rw.buff, req->running.ioq.rw.pn, req->running.ioq.rw.ptr);
		case YOI_PL_ACCEPT: return yo_socket_accept(
			req->running.ioq.fd,
			req->running.ioq.accept.pclient,
			req->running.ioq.accept.paddr,
			req->running.ioq.accept.pport
		);
		default: return YO_ENOTSUP;
	}
}
static yo_code_t _yoi_pl_req_start(yo_req_t req) {
	yo_fd_t fd = req->running.ioq.fd;

	switch (_yoi_pl_tomask(req->running.ioq.kind)) {
		case YOI_PL_READABLE: fd->ioq.read_n++; break;
		case YOI_PL_WRITABLE: fd->ioq.write_n++; break;
	}

	yoi_pl_evn_mask_t mask = 0;
	if (fd->ioq.read_n) mask |= YOI_PL_READABLE;
	if (fd->ioq.write_n) mask |= YOI_PL_WRITABLE;

	yo_code_t code = yoi_pl_impl_setmask(req->queue, fd, fd->fd, mask);
	yoi_req_begin(req, _yoi_pl_req_cancel);

	yoi_dlist_add(req_ioq, fd->head, req);

	// The handle doesn't support epoll, it must go thru the sync route
	if (code == YO_EPERM) {
		yoi_req_end(req, _yoi_pl_req_do(req));
		return YO_OK;
	}

	return code;
}
static yo_code_t _yoi_pl_req_stop(yo_req_t req) {
	yo_fd_t fd = req->running.ioq.fd;

	switch (_yoi_pl_tomask(req->running.ioq.kind)) {
		case YOI_PL_READABLE: fd->ioq.read_n--; break;
		case YOI_PL_WRITABLE: fd->ioq.write_n--; break;
	}

	yoi_pl_evn_mask_t mask = 0;
	if (fd->ioq.read_n) mask |= YOI_PL_READABLE;
	if (fd->ioq.write_n) mask |= YOI_PL_WRITABLE;

	yoi_dlist_del(req_ioq, req);

	return yoi_pl_impl_setmask(req->queue, fd, fd->fd, mask);
}

static yo_code_t yoi_pl_init(yoi_pl_t pl, yo_queue_t queue) {
	yo_code_t code;

	int msg_pipe[2];
	if (pipe(msg_pipe) < 0) { code = yoi_unix_conv_errno(errno); goto fail_pipe; }
	if (fcntl(msg_pipe[0], F_SETFD, O_NONBLOCK) < 0) { code = yoi_unix_conv_errno(errno); goto fail_fcntl; }
	if (fcntl(msg_pipe[1], F_SETFD, O_NONBLOCK) < 0) { code = yoi_unix_conv_errno(errno); goto fail_fcntl; }

	pl->notify_read = msg_pipe[0];
	pl->notify_write = msg_pipe[1];

	if ((code = yoi_pl_impl_setmask(queue, NULL, pl->notify_read, YOI_PL_READABLE)) != YO_OK) goto fail_setmask;

	return YO_OK;
fail_setmask:
fail_fcntl:
	close(msg_pipe[0]);
	close(msg_pipe[1]);
fail_pipe:
	return yoi_unix_conv_errno(errno);
}
static yo_code_t yoi_pl_free(yoi_pl_t pl) {
	close(pl->notify_read);
	close(pl->notify_write);
	return YO_OK;
}
static yo_code_t yoi_queue_impl_notify(yo_queue_t queue) {
	if (write(queue->impl.pl.notify_write, &(uint8_t) { 0 }, sizeof(uint8_t)) < 0) {
		if (errno != EWOULDBLOCK) return yoi_unix_conv_errno(errno);
	}

	return YO_OK;
}

static void (yoi_unix_onclose)(yo_fd_t fd) {
	for (yo_req_t i = fd->head; i; i = i->running.ioq.next) {
		// We must iterate, as requests from multiple queues may be on the same fd
		yoi_pl_impl_setmask(i->queue, NULL, fd->fd, 0);
		yoi_req_end(i, YO_ECANCELED);
	}
}

yo_code_t (yo_queue_poll)(yo_queue_t queue, const yo_time_t *deadline, yo_req_t *preq, yo_code_t *pcode) {
	while (true) {
		yo_req_t queue_req = yoi_queue_pop(queue, pcode);
		if (queue_req) {
			*preq = queue_req;
			return YO_OK;
		}

		void *udata;
		yoi_pl_evn_mask_t ready;

		yo_code_t err = yoi_pl_impl_poll(queue, deadline, &udata, &ready);
		if (err != YO_OK) return err;

		yo_fd_t fd = (yo_fd_t)udata;
		if (!fd) continue;

		yo_req_t req = _yoi_pl_get_req(fd, ready);
		if (!req) continue;

		_yoi_pl_req_stop(req);

		*pcode = _yoi_pl_req_do(req);
		*preq = req;
		return YO_OK;
	}
}

yo_code_t (yoa_read)(yo_req_t req, yo_fd_t fd, char *buff, size_t *pn) {
	if (!yoi_unix_isfd(fd)) return YO_EBADF;

	req->running.ioq.kind = YOI_PL_READ;
	req->running.ioq.fd = fd;

	req->running.ioq.rw.buff = buff;
	req->running.ioq.rw.pn = pn;

	return _yoi_pl_req_start(req);
}
yo_code_t (yoa_write)(yo_req_t req, yo_fd_t fd, char *buff, size_t *pn) {
	if (!yoi_unix_isfd(fd)) return YO_EBADF;

	req->running.ioq.kind = YOI_PL_WRITE;
	req->running.ioq.fd = fd;

	req->running.ioq.rw.buff = buff;
	req->running.ioq.rw.pn = pn;

	return _yoi_pl_req_start(req);
}
yo_code_t (yoa_file_read)(yo_req_t req, yo_fd_t fd, char *buff, size_t *pn, size_t offset) {
	if (!yoi_unix_isfd(fd)) return YO_EBADF;

	req->running.ioq.kind = YOI_PL_PREAD;
	req->running.ioq.fd = fd;

	req->running.ioq.rw.buff = buff;
	req->running.ioq.rw.pn = pn;
	req->running.ioq.rw.ptr = offset;

	return _yoi_pl_req_start(req);
}
yo_code_t (yoa_file_write)(yo_req_t req, yo_fd_t fd, char *buff, size_t *pn, size_t offset) {
	if (!yoi_unix_isfd(fd)) return YO_EBADF;

	req->running.ioq.kind = YOI_PL_PWRITE;
	req->running.ioq.fd = fd;

	req->running.ioq.rw.buff = buff;
	req->running.ioq.rw.pn = pn;
	req->running.ioq.rw.ptr = offset;

	return _yoi_pl_req_start(req);
}
yo_code_t (yoa_socket_accept)(yo_req_t req, yo_fd_t server, yo_fd_t *pclient, yo_addr_t *paddr, uint16_t *pport) {
	if (!yoi_unix_isfd(server)) return YO_EBADF;

	req->running.ioq.kind = YOI_PL_ACCEPT;
	req->running.ioq.fd = server;

	req->running.ioq.accept.pclient = pclient;
	req->running.ioq.accept.paddr = paddr;
	req->running.ioq.accept.pport = pport;

	return _yoi_pl_req_start(req);
}
