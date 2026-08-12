#ifndef EV_FILELIST_H
#define EV_FILELIST_H

// A structure, keeping track of all files, directories, processes, etc.
typedef struct ev_filelist *ev_filelist_t;
ev_filelist_t ev_filelist_new();
// Closes all contained files and frees all associated resources
void ev_filelist_free(ev_filelist_t fl);

#endif
