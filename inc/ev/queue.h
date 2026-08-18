#ifndef EV_QUEUE_H
#define EV_QUEUE_H

#include <stdbool.h>

#include <ev/time.h>
#include <ev/errno.h>

// Used to deploy a sync workload in an ev-managed thread
typedef int (*ev_worker_t)(void *pargs);

// A structure, keeping track of all pending operations and results
typedef struct ev_queue *ev_queue_t;
// A generic request in the queue
typedef struct ev_req *ev_req_t;

ev_queue_t ev_queue_new();
// Cancels all requests and frees all associated resources
void ev_queue_free(ev_queue_t queue);
// Gets the next completed request in the queue, or blocks until one is available
// - If `pdeadline` is not NULL, limits the block time until the monotonic time is reached
// - If `pdeadline` was reached before a result was pushed, NULL is stored in pres
// - If `pdeadline` is before the current moment, returns immediatly.
// A good way to non-blockingly poll is to pass ev_monotime() to `pdeadline`
ev_code_t ev_queue_poll(ev_queue_t queue, const ev_time_t *pdeadline, ev_req_t *preq, ev_code_t *pcode);

// Inserts a new request in the queue and returns it
ev_req_t ev_req_new(ev_queue_t queue);

// Executes the function in a thread pool returns its value via the request
// The function must return EV_EINTR, if it has been interrupted. This is interpreted as a cancellation
ev_code_t ev_req_exec(ev_req_t req, ev_worker_t worker, void *pargs);

// Cancels the request and removes it from the queue. If it is an IO operation, cancels it in the OS-appropriate way
void ev_req_cancel(ev_req_t req);
// Cancels the request and frees all associated resources
void ev_req_free(ev_req_t req);

#endif
