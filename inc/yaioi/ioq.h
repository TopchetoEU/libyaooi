#ifndef YO_IOQ_H
#define YO_IOQ_H

#include <yaioi/io.h>
#include <yaioi/queue.h>
#include <yaioi/addr.h>
#include <yaioi/errno.h>
#include <yaioi/signo.h>

// Here, queue versions of some functions are presented
// Those that don't have queue equivalents should be used only synchronously

// On linux, some of these are 'technically' blocking-only. However, this design is retarded,
// as network-based FS-es will gladly block for minutes when the underlying connection is lost.
// If that happens, our app would block for minutes as well, rendering the queue system rather useless.
// Underneath, blocking-only ops are run on a thread pool

// Poignant language here used because i am a victim of this "great" design of linux. Thanks, linus!

yo_code_t yoa_read(yo_req_t req, yo_fd_t fd, char *buff, size_t *pn);
yo_code_t yoa_write(yo_req_t req, yo_fd_t fd, char *buff, size_t *pn);
yo_code_t yoa_sync(yo_req_t req, yo_fd_t fd);
yo_code_t yoa_stat(yo_req_t req, yo_fd_t fd, yo_stat_t *buff);

yo_code_t yoa_file_remove(yo_req_t req, const char *path);
yo_code_t yoa_file_symlink(yo_req_t req, const char *src, const char *dst);
yo_code_t yoa_file_hardlink(yo_req_t req, const char *src, const char *dst);
yo_code_t yoa_file_readlink(yo_req_t req, const char *path, char **pres);

// yo_code_t yoa_file_open(yo_req_t req, yo_filelist_t fl, yo_fd_t *pres, const char *path, yo_open_flags_t flags, int mode);
yo_code_t yoa_file_read(yo_req_t req, yo_fd_t fd, char *buff, size_t *pn, size_t offset);
yo_code_t yoa_file_write(yo_req_t req, yo_fd_t fd, char *buff, size_t *pn, size_t offset);

yo_code_t yoa_dir_new(yo_req_t req, const char *path, int mode);
yo_code_t yoa_dir_open(yo_req_t req, yo_dir_t *pres, const char *path);
yo_code_t yoa_dir_next(yo_req_t req, yo_dir_t dir, char **pname);

yo_code_t yoa_socket_connect(yo_req_t req, yo_fd_t *pclient, yo_proto_t proto, yo_addr_t addr, uint16_t port);
yo_code_t yoa_socket_accept(yo_req_t req, yo_fd_t server, yo_fd_t *pclient, yo_addr_t *paddr, uint16_t *pport);

yo_code_t yoa_dns_getaddrinfo(yo_req_t req, yo_addrinfo_t *pres, const char *name, yo_addrinfo_flags_t flags);

yo_code_t yoa_proc_wait(yo_req_t req, yo_proc_t proc, int *psig, int *pcode);

yo_code_t yoa_sig_wait(yo_req_t req, yo_signo_t *pres);

#endif
