// libyaooi, Copyright (C) 2025-2026 topchetoeu, see LICENSE for full LGPL text

#pragma once

// Utility for poll-like (poll-ish) interfaces. This abstracts basically all operations around a poll-like API, except the poll function itself

#include <stdint.h>

#include <yaooi/io.h>
#include <yaooi/conf.h>
#include <yaooi/errno.h>
#include <yaooi/queue.h>

typedef enum {
	YOI_PL_READABLE = 1,
	YOI_PL_WRITABLE = 2,
} yoi_pl_evn_mask_t;

typedef enum {
	YOI_PL_READ,
	YOI_PL_WRITE,
	YOI_PL_PREAD,
	YOI_PL_PWRITE,
	YOI_PL_ACCEPT,
	// TODO: do connect here with async sockets
} yoi_pl_kind_t;

typedef struct {
	int notify_read, notify_write;
} yoi_pl_s, *yoi_pl_t;

typedef struct {
	yo_req_t next, *slot;
	yo_fd_t fd;
	#define yoi_list_req_ioq_next(node) (node)->running.ioq.next
	#define yoi_list_req_ioq_slot(node) (node)->running.ioq.slot

	yoi_pl_kind_t kind;
	union {
		struct {
			char *buff;
			size_t *pn, ptr;
		} rw;
		struct {
			yo_fd_t *pclient;
			yo_addr_t *paddr;
			uint16_t *pport;
		} accept;
	};
} yoi_req_ioq_t;

typedef struct {
	unsigned read_n, write_n;
} yoi_fd_ioq_t;

// yo_code_t yoa_socket_accept(yo_req_t req, yo_fd_t server, yo_fd_t client, yo_addr_t *paddr, uint16_t *pport);

// If mask is 0, removes fd from list. The other arguments are ignored
// If the file is not in the list, registers it with the given mask and attached udata
// If the file is in the list, modifies the mask and udata
static yo_code_t yoi_pl_impl_setmask(yo_queue_t queue, void *udata, int fd, yoi_pl_evn_mask_t mask);
// The function will set *pfd to the first fd that is ready for an op, and *pready to the type of operation to be performed
// Returns ETIMEDOUT if deadline is reached first
// If not deadline is set and no FDs are added sets pfd to -1
static yo_code_t yoi_pl_impl_poll(yo_queue_t queue, const yo_time_t *deadline, void **pudata, yoi_pl_evn_mask_t *pready);
// Forcefully unblocks yo_queue_poll (should be called when a request is added to the queue)
static yo_code_t yoi_queue_impl_notify(yo_queue_t queue);

static yo_code_t yoi_pl_init(yoi_pl_t pl, yo_queue_t queue);
static yo_code_t yoi_pl_free(yoi_pl_t pl);

static void (yoi_unix_onclose)(yo_fd_t fd);

// These are defined, so that the fallbacks can be ignored later on
#define yoa_read(...) yoa_read(__VA_ARGS__)
#define yoa_write(...) yoa_write(__VA_ARGS__)
#define yoa_file_read(...) yoa_file_read(__VA_ARGS__)
#define yoa_file_write(...) yoa_file_write(__VA_ARGS__)
#define yoa_socket_accept(...) yoa_socket_accept(__VA_ARGS__)
#define yo_queue_poll(...) yo_queue_poll(__VA_ARGS__)
#define yoi_unix_onclose(...) yoi_unix_onclose(__VA_ARGS__)
