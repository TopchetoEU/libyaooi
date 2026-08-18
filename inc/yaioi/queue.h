#ifndef YO_QUEUE_H
#define YO_QUEUE_H

#include <stdbool.h>

#include <yaioi/time.h>
#include <yaioi/errno.h>

// Used to deploy a sync workload in an ev-managed thread
typedef int (*yo_worker_t)(void *pargs);

// A structure, keeping track of all pending operations and results
typedef struct yo_queue *yo_queue_t;
// A generic request in the queue
typedef struct yo_req *yo_req_t;

yo_queue_t yo_queue_new();
// Cancels all requests and frees all associated resources
void yo_queue_free(yo_queue_t queue);
// Gets the next completed request in the queue, or blocks until one is available
// - If `pdeadline` is not NULL, limits the block time until the monotonic time is reached
// - If `pdeadline` was reached before a result was pushed, NULL is stored in pres
// - If `pdeadline` is before the current moment, returns immediatly.
// A good way to non-blockingly poll is to pass yo_monotime() to `pdeadline`
yo_code_t yo_queue_poll(yo_queue_t queue, const yo_time_t *pdeadline, yo_req_t *preq, yo_code_t *pcode);

// Inserts a new request in the queue and returns it
yo_req_t yo_req_new(yo_queue_t queue);

// Executes the function in a thread pool returns its value via the request
// The function must return YO_EINTR, if it has been interrupted. This is interpreted as a cancellation
yo_code_t yo_req_exec(yo_req_t req, yo_worker_t worker, void *pargs);

// Cancels the request and removes it from the queue. If it is an IO operation, cancels it in the OS-appropriate way
void yo_req_cancel(yo_req_t req);
// Cancels the request and frees all associated resources
void yo_req_free(yo_req_t req);

#endif
