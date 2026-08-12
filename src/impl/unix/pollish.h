#pragma once

// Utility for poll-like (poll-ish) interfaces. The gist is that with this interface (unlike uring),
// you will be notified that you can perform a certain IO operation without being blocked.

#include <stdint.h>

#include <ev/io.h>
#include <ev/conf.h>
#include <ev/errno.h>
#include <ev/queue.h>

typedef enum {
	EVI_PL_READABLE = 1,
	EVI_PL_WRITABLE = 2,
} evi_pl_evn_mask_t;

typedef void (*evi_pl_cb_t)(ev_req_t req);

typedef struct {
	int usermsg_read, usermsg_write;
} evi_pl_s, *evi_pl_t;

struct evi_impl_req {
	evi_pl_evn_mask_t mask;
	evi_pl_cb_t cb;
};

static bool evi_pl_cb(evi_pl_evn_mask_t mask, ev_req_t req);

ev_code_t evq_read(ev_req_t req, ev_fd_t fd, char *buff, size_t *n);
ev_code_t evq_write(ev_req_t req, ev_fd_t fd, char *buff, size_t *n);
ev_code_t evq_file_read(ev_req_t req, ev_fd_t fd, char *buff, size_t *n, size_t offset);
ev_code_t evq_file_write(ev_req_t req, ev_fd_t fd, char *buff, size_t *n, size_t offset);
ev_code_t evq_socket_accept(ev_req_t req, ev_fd_t server, ev_fd_t client, ev_addr_t *paddr, uint16_t *pport);

static ev_code_t evi_pl_impl_init(evi_pl_t ev);
static ev_code_t evi_pl_impl_free(evi_pl_t ev);
static ev_code_t evi_pl_impl_add(evi_pl_t ev, ev_req_t req, evi_pl_cb_t cb);
static ev_code_t evi_pl_impl_poll(ev_queue_t queue, const ev_time_t *timeout, ev_req_t *pres);

// typedef struct ev_poll_req {
// 	struct ev_poll_req **slot;
// 	struct ev_poll_req *next;
// 	size_t pollfd_i;
// 	void *ticket;
// 	ev_async_type_t type;
// 	int fd;
// 	union {
// 		struct { char *data; size_t *pn; size_t offset; } rw;
// 	};
// } *ev_poll_req_t;
