/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef BASE_THREAD_H
#define BASE_THREAD_H

#include <cstddef>
#include <functional>

/**
 * Threading related functions.
 *
 * @defgroup Threads Threading
 *
 * @see Locks
 * @see Semaphore
 */

/**
 * Creates a new thread.
 *
 * @ingroup Threads
 *
 * @param threadfunc Entry point for the new thread.
 * @param user Pointer to pass to the thread.
 * @param name Name describing the use of the thread.
 *
 * @return Handle for the new thread.
 */
void *thread_init(void (*threadfunc)(void *), void *user, const char *name);

/**
 * Waits for a thread to be done or destroyed.
 *
 * @ingroup Threads
 *
 * @param thread Thread to wait for.
 */
void thread_wait(void *thread);

/**
 * Yield the current thread's execution slice.
 *
 * @ingroup Threads
 */
void thread_yield();

/**
 * Puts the thread in the detached state, guaranteeing that
 * resources of the thread will be freed immediately when the
 * thread terminates.
 *
 * @ingroup Threads
 *
 * @param thread Thread to detach.
 */
void thread_detach(void *thread);

/**
 * Creates a new thread and detaches it.
 *
 * @ingroup Threads
 *
 * @param threadfunc Entry point for the new thread.
 * @param user Pointer to pass to the thread.
 * @param name Name describing the use of the thread.
 */
void thread_init_and_detach(void (*threadfunc)(void *), void *user, const char *name);

/**
 * Calls `Function(Index)` once for every index in `[0, Count)`, distributing the
 * indices over worker threads, and returns when all of them have completed.
 *
 * Indices are handed out dynamically, so this also balances work when the
 * individual items take very different amounts of time.
 *
 * @ingroup Threads
 *
 * @param Count Number of items to process.
 * @param Function Work to perform for one item. Must be safe to call concurrently.
 *
 * @remark Must be called on a thread that may block, as it waits for completion.
 * The calling thread participates in the work.
 */
void thread_parallel_for(size_t Count, const std::function<void(size_t Index)> &Function);

#endif
