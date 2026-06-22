/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef BASE_IO_H
#define BASE_IO_H

#include "types.h"

#include <cstdint>

/**
 * File I/O related operations.
 *
 * @defgroup File-IO File I/O
 */

/**
 * @ingroup File-IO
 */
enum
{
	/**
	 * Open file for reading.
	 *
	 * @see io_open
	 */
	IOFLAG_READ = 1,
	/**
	 * Open file for writing.
	 *
	 * @see io_open
	 */
	IOFLAG_WRITE = 2,
	/**
	 * Open file for appending at the end.
	 *
	 * @see io_open
	 */
	IOFLAG_APPEND = 4,
};

/**
 * @ingroup File-IO
 */
enum ESeekOrigin
{
	/**
	 * Start seeking from the beginning of the file.
	 *
	 * @see io_seek
	 */
	IOSEEK_START = 0,
	/**
	 * Start seeking from the current position.
	 *
	 * @see io_seek
	 */
	IOSEEK_CUR = 1,
	/**
	 * Start seeking from the end of the file.
	 *
	 * @see io_seek
	 */
	IOSEEK_END = 2,
};

/**
 * Opens a file.
 *
 * @ingroup File-IO
 *
 * @param filename File to open.
 * @param flags A set of IOFLAG flags.
 *
 * @see IOFLAG_READ, IOFLAG_WRITE, IOFLAG_APPEND.
 *
 * @return A handle to the file on success, or `nullptr` on failure.
 */
IOHANDLE io_open(const char *filename, int flags);

/**
 * Reads data into a buffer from a file.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file to read data from.
 * @param buffer Pointer to the buffer that will receive the data.
 * @param size Number of bytes to read from the file.
 *
 * @return Number of bytes read.
 */
unsigned io_read(IOHANDLE io, void *buffer, unsigned size);

/**
 * Reads the rest of the file into a buffer.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file to read data from.
 * @param result Receives the file's remaining contents.
 * @param result_len Receives the file's remaining length.
 *
 * @return `true` on success, `false` on failure.
 *
 * @remark Does NOT guarantee that there are no internal null bytes.
 * @remark The result must be freed after it has been used.
 * @remark The function will fail if more than 1 GiB of memory would
 *         have to be allocated. Large files should not be loaded into memory.
 */
bool io_read_all(IOHANDLE io, void **result, unsigned *result_len);

/**
 * Reads the rest of the file into a null-terminated buffer with
 * no internal null bytes.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file to read data from.
 *
 * @return The file's remaining contents, or `nullptr` on failure.
 *
 * @remark Guarantees that there are no internal null bytes.
 * @remark Guarantees that result will contain null-termination.
 * @remark The result must be freed after it has been used.
 * @remark The function will fail if more than 1 GiB of memory would
 *         have to be allocated. Large files should not be loaded into memory.
 */
char *io_read_all_str(IOHANDLE io);

/**
 * Skips data in a file.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 * @param size Number of bytes to skip.
 *
 * @return `0` on success.
 */
int io_skip(IOHANDLE io, int64_t size);

/**
 * Seeks to a specified offset in the file.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 * @param offset Offset from position to search.
 * @param origin Position to start searching from.
 *
 * @return `0` on success.
 */
int io_seek(IOHANDLE io, int64_t offset, ESeekOrigin origin);

/**
 * Gets the current position in the file.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 *
 * @return The current position, or `-1` on failure.
 */
int64_t io_tell(IOHANDLE io);

/**
 * Gets the total length of the file. Resets cursor to the beginning.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 *
 * @return The total size, or `-1` on failure.
 */
int64_t io_length(IOHANDLE io);

/**
 * Writes data from a buffer to a file.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 * @param buffer Pointer to the data that should be written.
 * @param size Number of bytes to write.
 *
 * @return Number of bytes written.
 */
unsigned io_write(IOHANDLE io, const void *buffer, unsigned size);

/**
 * Writes a platform dependent newline to a file.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 *
 * @return `true` on success, `false` on failure.
 */
bool io_write_newline(IOHANDLE io);

/**
 * Closes a file.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 *
 * @return `0` on success.
 */
int io_close(IOHANDLE io);

/**
 * Empties all buffers and writes all pending data.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 *
 * @return `0` on success.
 */
int io_flush(IOHANDLE io);

/**
 * Synchronize file changes to disk.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 *
 * @return `0` on success.
 */
int io_sync(IOHANDLE io);

/**
 * Checks whether an error occurred during I/O with the file.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 *
 * @return `0` on success, or non-`0` on error.
 */
int io_error(IOHANDLE io);

/**
 * Returns a handle for the standard input.
 *
 * @ingroup File-IO
 *
 * @return An @link IOHANDLE @endlink for the standard input.
 *
 * @remark The handle must not be closed.
 */
IOHANDLE io_stdin();

/**
 * Returns a handle for the standard output.
 *
 * @ingroup File-IO
 *
 * @return An @link IOHANDLE @endlink for the standard output.
 *
 * @remark The handle must not be closed.
 */
IOHANDLE io_stdout();

/**
 * Returns a handle for the standard error.
 *
 * @ingroup File-IO
 *
 * @return An @link IOHANDLE @endlink for the standard error.
 *
 * @remark The handle must not be closed.
 */
IOHANDLE io_stderr();

/**
 * Returns a handle for the current executable.
 *
 * @ingroup File-IO
 *
 * @return An @link IOHANDLE @endlink for the current executable.
 */
IOHANDLE io_current_exe();

#endif
