#ifndef EV_IOQ_H
#define EV_IOQ_H

#include <ev/io.h>
#include <ev/filelist.h>
#include <ev/queue.h>
#include <ev/addr.h>
#include <ev/errno.h>
#include <ev/signo.h>

// Here, queue versions of some functions are presented
// Those that don't have queue equivalents should be used only synchronously

// On linux, some of these are 'technically' blocking-only. However, this design is retarded,
// as network-based FS-es will gladly block for minutes when the underlying connection is lost.
// If that happens, our app would block for minutes as well, rendering the queue system rather useless.
// Underneath, blocking-only ops are run on a thread pool

// Poignant language here used because i am a victim of this "great" design of linux. Thanks, linus!

ev_code_t evq_read(ev_req_t req, ev_fd_t fd, char *buff, size_t *pn);
ev_code_t evq_write(ev_req_t req, ev_fd_t fd, char *buff, size_t *pn);
ev_code_t evq_sync(ev_req_t req, ev_fd_t fd);
ev_code_t evq_stat(ev_req_t req, ev_fd_t fd, ev_stat_t *buff);

ev_code_t evq_file_remove(ev_req_t req, const char *EV_NONULL path);
ev_code_t evq_file_symlink(ev_req_t req, const char *EV_NONULL src, const char *EV_NONULL dst);
ev_code_t evq_file_hardlink(ev_req_t req, const char *EV_NONULL src, const char *EV_NONULL dst);
ev_code_t evq_file_readlink(ev_req_t req, const char *EV_NONULL path, char **pres);

ev_code_t evq_file_open(ev_req_t req, ev_filelist_t fl, ev_fd_t *pres, const char *path, ev_open_flags_t flags, int mode);
ev_code_t evq_file_read(ev_req_t req, ev_fd_t fd, char *buff, size_t *pn, size_t offset);
ev_code_t evq_file_write(ev_req_t req, ev_fd_t fd, char *buff, size_t *pn, size_t offset);

ev_code_t evq_dir_new(ev_req_t req, const char *EV_NONULL path, int mode);
ev_code_t evq_dir_open(ev_req_t req, ev_filelist_t fl, ev_dir_t *pres, const char *EV_NONULL path);
ev_code_t evq_dir_next(ev_req_t req, ev_dir_t dir, char **EV_NONULL pname);

ev_code_t evq_socket_connect(ev_req_t req, ev_filelist_t fl, ev_fd_t *pclient, ev_addr_t addr, uint16_t port);
ev_code_t evq_socket_accept(ev_req_t req, ev_filelist_t fl, ev_fd_t server, ev_fd_t *pclient, ev_addr_t *EV_NONULL paddr, uint16_t *EV_NONULL pport);

ev_code_t evq_dns_getaddrinfo(ev_req_t req, ev_addrinfo_t *EV_NONULL pres, const char *EV_NONULL name, ev_addrinfo_flags_t flags);

ev_code_t evq_proc_wait(ev_req_t req, ev_proc_t proc, int *EV_NONULL psig, int *EV_NONULL pcode);

ev_code_t evq_sig_wait(ev_req_t req, ev_signo_t *EV_NONULL pres);

void evq_sleep(ev_req_t req, ev_time_t deadline);

#endif
