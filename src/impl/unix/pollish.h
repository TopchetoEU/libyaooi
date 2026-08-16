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

typedef enum {
	EVI_PL_READ,
	EVI_PL_WRITE,
	EVI_PL_PREAD,
	EVI_PL_PWRITE,
	EVI_PL_ACCEPT,
	// TODO: do connect here with async sockets
} evi_pl_kind_t;

typedef struct {
	int notify_read, notify_write;
} evi_pl_s, *evi_pl_t;

typedef struct {
	ev_req_t next;
	ev_fd_t fd;
	#define evi_list_req_ioq_next(node) (node)->running.ioq.next

	evi_pl_kind_t kind;
	union {
		struct {
			char *buff;
			size_t *pn, ptr;
		} rw;
		struct {
			ev_filelist_t fl;
			ev_fd_t *pclient;
			ev_addr_t *paddr;
			uint16_t *pport;
		} accept;
	};
} evi_req_ioq_t;

typedef struct {
	unsigned read_n, write_n;
} evi_fd_ioq_t;

// ev_code_t evq_socket_accept(ev_req_t req, ev_fd_t server, ev_fd_t client, ev_addr_t *paddr, uint16_t *pport);

// If mask is 0, removes fd from list. The other arguments are ignored
// If the file is not in the list, registers it with the given mask and attached udata
// If the file is in the list, modifies the mask and udata
static ev_code_t evi_pl_impl_setmask(ev_queue_t queue, void *udata, int fd, evi_pl_evn_mask_t mask);
// The function will set *pfd to the first fd that is ready for an op, and *pready to the type of operation to be performed
// Returns ETIMEDOUT if deadline is reached first
// If not deadline is set and no FDs are added sets pfd to -1
static ev_code_t evi_pl_impl_poll(ev_queue_t queue, const ev_time_t *deadline, void **pudata, evi_pl_evn_mask_t *pready);
// Forcefully unblocks ev_queue_poll (should be called when a request is added to the queue)
static ev_code_t evi_queue_impl_notify(ev_queue_t queue);

static ev_code_t evi_pl_init(evi_pl_t pl, ev_queue_t queue);
static ev_code_t evi_pl_free(evi_pl_t pl);

// These are defined, so that the fallbacks can be ignored later on
#define evq_read(...) evq_read(__VA_ARGS__)
#define evq_write(...) evq_write(__VA_ARGS__)
#define evq_file_read(...) evq_file_read(__VA_ARGS__)
#define evq_file_write(...) evq_file_write(__VA_ARGS__)
#define evq_socket_accept(...) evq_socket_accept(__VA_ARGS__)
#define ev_queue_poll(...) ev_queue_poll(__VA_ARGS__)
