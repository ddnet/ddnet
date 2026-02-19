#ifndef BASE_AIO_H
#define BASE_AIO_H

#include "types.h"

/**
 * Wraps a @link IOHANDLE @endlink for asynchronous writing.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 *
 * @return The handle for asynchronous writing.
 */
ASYNCIO *aio_new(IOHANDLE io);

/**
 * Locks the `ASYNCIO` structure so it can't be written into by
 * other threads.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 */
void aio_lock(ASYNCIO *aio);

/**
 * Unlocks the `ASYNCIO` structure after finishing the contiguous
 * write.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 */
void aio_unlock(ASYNCIO *aio);

/**
 * Queues a chunk of data for writing.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 * @param buffer Pointer to the data that should be written.
 * @param size Number of bytes to write.
 */
void aio_write(ASYNCIO *aio, const void *buffer, unsigned size);

/**
 * Queues a newline for writing.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 */
void aio_write_newline(ASYNCIO *aio);

/**
 * Queues a chunk of data for writing. The `ASYNCIO` struct must be
 * locked using @link aio_lock @endlink first.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 * @param buffer Pointer to the data that should be written.
 * @param size Number of bytes to write.
 */
void aio_write_unlocked(ASYNCIO *aio, const void *buffer, unsigned size);

/**
 * Queues a newline for writing. The `ASYNCIO` struct must be locked
 * using @link aio_lock @endlink first.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 */
void aio_write_newline_unlocked(ASYNCIO *aio);

/**
 * Checks whether errors have occurred during the asynchronous writing.
 *
 * Call this function regularly to see if there are errors. Call this
 * function after @link aio_wait @endlink to see if the process of writing
 * to the file succeeded.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 *
 * @return `0` on success, or non-`0` on error.
 */
int aio_error(ASYNCIO *aio);

/**
 * Queues file closing.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 */
void aio_close(ASYNCIO *aio);

/**
 * Wait for the asynchronous operations to complete.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 */
void aio_wait(ASYNCIO *aio);

/**
 * Frees the resources associated with the asynchronous file handle.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 */
void aio_free(ASYNCIO *aio);

#endif
