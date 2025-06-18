/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

/*
	Title: OS Abstraction
*/

#ifndef BASE_SYSTEM_H
#define BASE_SYSTEM_H

#include "detect.h"
#include "types.h"

#include <chrono>
#include <cinttypes>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

#ifdef __MINGW32__
#undef PRId64
#undef PRIu64
#undef PRIX64
#define PRId64 "I64d"
#define PRIu64 "I64u"
#define PRIX64 "I64X"
#define PRIzu "Iu"
#else
#define PRIzu "zu"
#endif

#ifdef CONF_FAMILY_UNIX
#include <sys/un.h>
#endif

#ifdef CONF_PLATFORM_LINUX
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#if __cplusplus >= 201703L
#define MAYBE_UNUSED [[maybe_unused]]
#elif defined(__GNUC__)
#define MAYBE_UNUSED __attribute__((unused))
#else
#define MAYBE_UNUSED
#endif

#ifdef __GNUC__
#define GNUC_ATTRIBUTE(x) __attribute__(x)
#else
#define GNUC_ATTRIBUTE(x)
#endif

/**
 * Utilities for debugging.
 *
 * @defgroup Debug
 */

/**
 * Breaks into the debugger based on a test.
 *
 * @ingroup Debug
 *
 * @param test Result of the test.
 * @param msg Message that should be printed if the test fails.
 *
 * @remark Also works in release mode.
 *
 * @see dbg_break
 */
#define dbg_assert(test, fmt, ...) \
	do \
	{ \
		if(!(test)) \
		{ \
			dbg_assert_imp(__FILE__, __LINE__, fmt, ##__VA_ARGS__); \
		} \
	} while(false)

/**
 * Use dbg_assert instead!
 *
 * @ingroup Debug
 */
#if defined(__cplusplus)
[[noreturn]]
#endif
void
dbg_assert_imp(const char *filename, int line, const char *fmt, ...)
	GNUC_ATTRIBUTE((format(printf, 3, 4)));

#ifdef __clang_analyzer__
#include <cassert>
#undef dbg_assert
#define dbg_assert(test, fmt, ...) assert(test)
#endif

/**
 * Checks whether the program is currently shutting down due to a failed
 * assert.
 *
 * @ingroup Debug
 *
 * @return indication whether the program is currently shutting down due to a
 * failed assert.
 */
bool dbg_assert_has_failed();

/**
 * Breaks into the debugger.
 *
 * @ingroup Debug
 *
 * @remark Also works in release mode.
 *
 * @see dbg_assert
 */
#if defined(__cplusplus)
[[noreturn]]
#endif
void
dbg_break();

typedef std::function<void(const char *message)> DBG_ASSERT_HANDLER;
void dbg_assert_set_handler(DBG_ASSERT_HANDLER handler);

/**
 * Prints a debug message.
 *
 * @ingroup Debug
 *
 * @param sys A string that describes what system the message belongs to.
 * @param fmt A printf styled format string.
 *
 * @remark Also works in release mode.
 *
 * @see dbg_assert
 */
void dbg_msg(const char *sys, const char *fmt, ...)
	GNUC_ATTRIBUTE((format(printf, 2, 3)));

/**
 * Memory management utilities.
 *
 * @defgroup Memory
 */

/**
 * Copies a a memory block.
 *
 * @ingroup Memory
 *
 * @param dest Destination.
 * @param source Source to copy.
 * @param size Size of the block to copy.
 *
 * @remark This functions DOES NOT handle cases where the source and destination is overlapping.
 *
 * @see mem_move
 */
void mem_copy(void *dest, const void *source, size_t size);

/**
 * Copies a a memory block.
 *
 * @ingroup Memory
 *
 * @param dest Destination.
 * @param source Source to copy.
 * @param size Size of the block to copy.
 *
 * @remark This functions handles the cases where the source and destination is overlapping.
 *
 * @see mem_copy
 */
void mem_move(void *dest, const void *source, size_t size);

/**
 * Sets a complete memory block to 0.
 *
 * @ingroup Memory
 *
 * @param block Pointer to the block to zero out.
 * @param size Size of the block.
 */
template<typename T>
inline void mem_zero(T *block, size_t size)
{
	static_assert((std::is_trivially_constructible<T>::value && std::is_trivially_destructible<T>::value) || std::is_fundamental<T>::value);
	memset(block, 0, size);
}

/**
 * Compares two blocks of memory
 *
 * @ingroup Memory
 *
 * @param a First block of data.
 * @param b Second block of data.
 * @param size Size of the data to compare.
 *
 * @return `< 0` if block a is less than block b.
 * @return `0` if block a is equal to block b.
 * @return `> 0` if block a is greater than block b.
 */
int mem_comp(const void *a, const void *b, size_t size);

/**
 * Checks whether a block of memory contains null bytes.
 *
 * @ingroup Memory
 *
 * @param block Pointer to the block to check for nulls.
 * @param size Size of the block.
 *
 * @return `true` if the block has a null byte, `false` otherwise.
 */
bool mem_has_null(const void *block, size_t size);

/**
 * I/O related operations.
 *
 * @defgroup File-IO
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

/**
 * Wrapper for asynchronously writing to an @link IOHANDLE @endlink.
 *
 * @ingroup File-IO
 */
typedef struct ASYNCIO ASYNCIO;

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

/**
 * Threading related functions.
 *
 * @defgroup Threads
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
 * @defgroup Semaphore
 *
 * @see Threads
 */

#if defined(CONF_FAMILY_WINDOWS)
typedef void *SEMAPHORE;
#elif defined(CONF_PLATFORM_MACOS)
#include <semaphore.h>
typedef sem_t *SEMAPHORE;
#elif defined(CONF_FAMILY_UNIX)
#include <semaphore.h>
typedef sem_t SEMAPHORE;
#else
#error not implemented on this platform
#endif

/**
 * @ingroup Semaphore
 */
void sphore_init(SEMAPHORE *sem);
/**
 * @ingroup Semaphore
 */
void sphore_wait(SEMAPHORE *sem);
/**
 * @ingroup Semaphore
 */
void sphore_signal(SEMAPHORE *sem);
/**
 * @ingroup Semaphore
 */
void sphore_destroy(SEMAPHORE *sem);

/**
 * Time utilities.
 *
 * @defgroup Time
 */

/**
 * @ingroup Time
 */
void set_new_tick();

/**
 * Fetches a sample from a high resolution timer.
 *
 * @ingroup Time
 *
 * @return Current value of the timer.
 *
 * @remark To know how fast the timer is ticking, see @link time_freq @endlink.
 *
 * @see time_freq
 */
int64_t time_get_impl();

/**
 * Fetches a sample from a high resolution timer.
 *
 * @ingroup Time
 *
 * @return Current value of the timer.
 *
 * @remark To know how fast the timer is ticking, see @link time_freq @endlink.
 * @remark Uses @link time_get_impl @endlink to fetch the sample.
 *
 * @see time_freq time_get_impl
 */
int64_t time_get();

/**
 * @ingroup Time
 *
 * @return The frequency of the high resolution timer.
 */
int64_t time_freq();

/**
 * Retrieves the current time as a UNIX timestamp
 *
 * @ingroup Time
 *
 * @return The time as a UNIX timestamp
 */
int64_t time_timestamp();

/**
 * Retrieves the hours since midnight (0..23)
 *
 * @ingroup Time
 *
 * @return The current hour of the day
 */
int time_houroftheday();

/**
 * @ingroup Time
 */
enum ETimeSeason
{
	SEASON_SPRING = 0,
	SEASON_SUMMER,
	SEASON_AUTUMN,
	SEASON_WINTER,
	SEASON_EASTER,
	SEASON_HALLOWEEN,
	SEASON_XMAS,
	SEASON_NEWYEAR
};

/**
 * Retrieves the current season of the year.
 *
 * @ingroup Time
 *
 * @return One of the SEASON_* enum literals
 *
 * @see SEASON_SPRING
 */
ETimeSeason time_season();

/**
 * @defgroup Network-General
 */

extern const NETADDR NETADDR_ZEROED;

/**
 * @ingroup Network-General
 */

#ifdef CONF_FAMILY_UNIX
/**
 * @ingroup Network-General
 */
typedef int UNIXSOCKET;
/**
 * @ingroup Network-General
 */
typedef struct sockaddr_un UNIXSOCKETADDR;
#endif

/**
 * Initiates network functionality.
 *
 * @ingroup Network-General
 *
 * @remark You must call this function before using any other network functions.
 */
void net_init();

/**
 * Looks up the ip of a hostname.
 *
 * @ingroup Network-General
 *
 * @param hostname Host name to look up.
 * @param addr The output address to write to.
 * @param types The type of IP that should be returned.
 *
 * @return `0` on success.
 */
int net_host_lookup(const char *hostname, NETADDR *addr, int types);

/**
 * Compares two network addresses.
 *
 * @ingroup Network-General
 *
 * @param a Address to compare.
 * @param b Address to compare to.
 *
 * @return `< 0` if address a is less than address b.
 * @return `0` if address a is equal to address b.
 * @return `> 0` if address a is greater than address b.
 */
int net_addr_comp(const NETADDR *a, const NETADDR *b);

/**
 * Compares two network addresses ignoring port.
 *
 * @ingroup Network-General
 *
 * @param a Address to compare.
 * @param b Address to compare to.
 *
 * @return `< 0` if address a is less than address b.
 * @return `0` if address a is equal to address b.
 * @return `> 0` if address a is greater than address b.
 */
int net_addr_comp_noport(const NETADDR *a, const NETADDR *b);

/**
 * Turns a network address into a representative string.
 *
 * @ingroup Network-General
 *
 * @param addr Address to turn into a string.
 * @param string Buffer to fill with the string.
 * @param max_length Maximum size of the string.
 * @param add_port Whether to add the port to the string.
 *
 * @remark The string will always be null-terminated.
 */
void net_addr_str(const NETADDR *addr, char *string, int max_length, bool add_port);

/**
 * Turns url string into a network address struct.
 * The url format is tw-0.6+udp://{ipaddr}[:{port}]
 * ipaddr: can be ipv4 or ipv6
 * port: is a optional internet protocol port
 *
 * This format is used for parsing the master server, be careful before changing it.
 *
 * Examples:
 *   tw-0.6+udp://127.0.0.1
 *   tw-0.6+udp://127.0.0.1:8303
 *
 * @ingroup Network-General
 *
 * @param addr Address to fill in.
 * @param string String to parse.
 * @param host_buf Pointer to a buffer to write the host to
 *                 It will include the port if one is included in the url
 *                 It can also be set to `nullptr` then it will be ignored.
 * @param host_buf_size Size of the host buffer or 0 if no host_buf pointer is given.
 *
 * @return `0` on success.
 * @return `> 0` if the input wasn't a valid DDNet URL,
 * @return `< 0` if the input is a valid DDNet URL but the host part was not a valid IPv4/IPv6 address
 */
int net_addr_from_url(NETADDR *addr, const char *string, char *host_buf, size_t host_buf_size);

/**
 * Checks if an address is local.
 *
 * @ingroup Network-General
 *
 * @param addr Address to check.
 *
 * @return `true` if the address is local, `false` otherwise.
 */
bool net_addr_is_local(const NETADDR *addr);

/**
 * Turns string into a network address.
 *
 * @ingroup Network-General
 *
 * @param addr Address to fill in.
 * @param string String to parse.
 *
 * @return `0` on success.
 */
int net_addr_from_str(NETADDR *addr, const char *string);

/**
 * Make a socket not block on operations
 *
 * @ingroup Network-General
 *
 * @param sock The socket to set the mode on.
 *
 * @returns `0` on success.
 */
int net_set_non_blocking(NETSOCKET sock);

/**
 * Make a socket block on operations.
 *
 * @param sock The socket to set the mode on.
 *
 * @returns `0` on success.
 */
int net_set_blocking(NETSOCKET sock);

/**
 * If a network operation failed, the error code.
 *
 * @ingroup Network-General
 *
 * @returns The error code.
 */
int net_errno();

/**
 * If a network operation failed, the platform-specific error code and string.
 *
 * @ingroup Network-General
 *
 * @returns The error code and string combined into one string.
 */
std::string net_error_message();

/**
 * Determines whether a network operation would block.
 *
 * @ingroup Network-General
 *
 * @returns `0` if wouldn't block, `1` if would block.
 */
int net_would_block();

/**
 * Waits for a socket to have data available to receive up the specified timeout duration.
 *
 * @ingroup Network-General
 *
 * @param sock Socket to wait on.
 * @param nanoseconds Timeout duration to wait.
 *
 * @return `1` if data was received within the timeout duration, `0` otherwise.
 */
int net_socket_read_wait(NETSOCKET sock, std::chrono::nanoseconds nanoseconds);

/**
 * @defgroup Network-UDP
 *
 * @ingroup Network-General
 */

/**
 * Determine a socket's type.
 *
 * @ingroup Network-General
 *
 * @param sock Socket whose type should be determined.
 *
 * @return The socket type, a bitset of `NETTYPE_IPV4`, `NETTYPE_IPV6`, `NETTYPE_WEBSOCKET_IPV4`
 *         and `NETTYPE_WEBSOCKET_IPV6`, or `NETTYPE_INVALID` if the socket is invalid.
 */
int net_socket_type(NETSOCKET sock);

/**
 * Creates a UDP socket and binds it to a port.
 *
 * @ingroup Network-UDP
 *
 * @param bindaddr Address to bind the socket to.
 *
 * @return On success it returns an handle to the socket. On failure it returns `NETSOCKET_INVALID`
 */
NETSOCKET net_udp_create(NETADDR bindaddr);

/**
 * Sends a packet over an UDP socket.
 *
 * @ingroup Network-UDP
 *
 * @param sock Socket to use.
 * @param addr Where to send the packet.
 * @param data Pointer to the packet data to send.
 * @param size Size of the packet.
 *
 * @return On success it returns the number of bytes sent. Returns `-1` on error.
 */
int net_udp_send(NETSOCKET sock, const NETADDR *addr, const void *data, int size);

/**
 * Receives a packet over an UDP socket.
 *
 * @ingroup Network-UDP
 *
 * @param sock Socket to use.
 * @param addr Pointer to an NETADDR that will receive the address.
 * @param data Received data. Will be invalidated when this function is called again.
 *
 * @return On success it returns the number of bytes received. Returns `-1` on error.
 */
int net_udp_recv(NETSOCKET sock, NETADDR *addr, unsigned char **data);

/**
 * Closes an UDP socket.
 *
 * @ingroup Network-UDP
 *
 * @param sock Socket to close.
 */
void net_udp_close(NETSOCKET sock);

/**
 * @defgroup Network-TCP
 *
 * @ingroup Network-General
 */

/**
 * Creates a TCP socket.
 *
 * @ingroup Network-TCP
 *
 * @param bindaddr Address to bind the socket to.
 *
 * @return On success it returns an handle to the socket. On failure it returns NETSOCKET_INVALID.
 */
NETSOCKET net_tcp_create(NETADDR bindaddr);

/**
 * Makes the socket start listening for new connections.
 *
 * @ingroup Network-TCP
 *
 * @param sock Socket to start listen to.
 * @param backlog Size of the queue of incoming connections to keep.
 *
 * @return `0` on success.
 */
int net_tcp_listen(NETSOCKET sock, int backlog);

/**
 * Polls a listening socket for a new connection.
 *
 * @ingroup Network-TCP
 *
 * @param sock Listening socket to poll.
 * @param new_sock Pointer to a socket to fill in with the new socket.
 * @param addr Pointer to an address that will be filled in the remote address, can be `nullptr`.
 *
 * @return A non-negative integer on success. Negative integer on failure.
 */
int net_tcp_accept(NETSOCKET sock, NETSOCKET *new_sock, NETADDR *addr);

/**
 * Connects one socket to another.
 *
 * @ingroup Network-TCP
 *
 * @param sock Socket to connect.
 * @param addr Address to connect to.
 *
 * @return `0` on success.
 *
 */
int net_tcp_connect(NETSOCKET sock, const NETADDR *addr);

/**
 * Connect a socket to a TCP address without blocking.
 *
 * @ingroup Network-TCP
 *
 * @param sock The socket to connect with.
 * @param bindaddr The address to connect to.
 *
 * @returns `0` on success.
 */
int net_tcp_connect_non_blocking(NETSOCKET sock, NETADDR bindaddr);

/**
 * Sends data to a TCP stream.
 *
 * @ingroup Network-TCP
 *
 * @param sock Socket to send data to.
 * @param data Pointer to the data to send.
 * @param size Size of the data to send.
 *
 * @return Number of bytes sent. Negative value on failure.
 */
int net_tcp_send(NETSOCKET sock, const void *data, int size);

/**
 * Recvives data from a TCP stream.
 *
 * @ingroup Network-TCP
 *
 * @param sock Socket to recvive data from.
 * @param data Pointer to a buffer to write the data to.
 * @param max_size Maximum of data to write to the buffer.
 *
 * @return Number of bytes recvived. Negative value on failure. When in
 * non-blocking mode, it returns 0 when there is no more data to be fetched.
 */
int net_tcp_recv(NETSOCKET sock, void *data, int maxsize);

/**
 * Closes a TCP socket.
 *
 * @ingroup Network-TCP
 *
 * @param sock Socket to close.
 */
void net_tcp_close(NETSOCKET sock);

#if defined(CONF_FAMILY_UNIX)
/**
 * @defgroup Network-Unix-Sockets
 *
 * @ingroup Network-General
 */

/**
 * Creates an unnamed unix datagram socket.
 *
 * @ingroup Network-Unix-Sockets
 *
 * @return On success it returns a handle to the socket. On failure it returns `-1`.
 */
UNIXSOCKET net_unix_create_unnamed();

/**
 * Sends data to a Unix socket.
 *
 * @ingroup Network-Unix-Sockets
 *
 * @param sock Socket to use.
 * @param addr Where to send the packet.
 * @param data Pointer to the packet data to send.
 * @param size Size of the packet.
 *
 * @return Number of bytes sent. Negative value on failure.
 */
int net_unix_send(UNIXSOCKET sock, UNIXSOCKETADDR *addr, void *data, int size);

/**
 * Sets the unixsocketaddress for a path to a socket file.
 *
 * @ingroup Network-Unix-Sockets
 *
 * @param addr Pointer to the `UNIXSOCKETADDR` to fill.
 * @param path Path to the (named) unix socket.
 */
void net_unix_set_addr(UNIXSOCKETADDR *addr, const char *path);

/**
 * Closes a Unix socket.
 *
 * @ingroup Network-Unix-Sockets
 *
 * @param sock Socket to close.
 */
void net_unix_close(UNIXSOCKET sock);

#elif defined(CONF_FAMILY_WINDOWS)

/**
 * Formats a Windows error code as a human-readable string.
 *
 * @param error The Windows error code.
 *
 * @return A new std::string representing the error code.
 */
std::string windows_format_system_message(unsigned long error);

#endif

/**
 * String related functions.
 *
 * @defgroup Strings
 */

/**
 * Appends a string to another.
 *
 * @ingroup Strings
 *
 * @param dst Pointer to a buffer that contains a string.
 * @param src String to append.
 * @param dst_size Size of the buffer of the dst string.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Guarantees that dst string will contain null-termination.
 */
void str_append(char *dst, const char *src, int dst_size);

/**
 * Appends a string to a fixed-size array of chars.
 *
 * @ingroup Strings
 *
 * @param dst Array that shall receive the string.
 * @param src String to append.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Guarantees that dst string will contain null-termination.
 */
template<int N>
void str_append(char (&dst)[N], const char *src)
{
	str_append(dst, src, N);
}

/**
 * Copies a string to another.
 *
 * @ingroup Strings
 *
 * @param dst Pointer to a buffer that shall receive the string.
 * @param src String to be copied.
 * @param dst_size Size of the buffer dst.
 *
 * @return Length of written string, even if it has been truncated
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Guarantees that dst string will contain null-termination.
 */
int str_copy(char *dst, const char *src, int dst_size);

/**
 * Copies a string to a fixed-size array of chars.
 *
 * @ingroup Strings
 *
 * @param dst Array that shall receive the string.
 * @param src String to be copied.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Guarantees that dst string will contain null-termination.
 */
template<int N>
void str_copy(char (&dst)[N], const char *src)
{
	str_copy(dst, src, N);
}

/**
 * Truncates a UTF-8 encoded string to a given length.
 *
 * @ingroup Strings
 *
 * @param dst Pointer to a buffer that shall receive the string.
 * @param dst_size Size of the buffer dst.
 * @param str String to be truncated.
 * @param truncation_len Maximum codepoints in the returned string.
 *
 * @remark The strings are treated as utf8-encoded null-terminated strings.
 * @remark Guarantees that dst string will contain null-termination.
 */
void str_utf8_truncate(char *dst, int dst_size, const char *src, int truncation_len);

/**
 * Truncates a string to a given length.
 *
 * @ingroup Strings
 *
 * @param dst Pointer to a buffer that shall receive the string.
 * @param dst_size Size of the buffer dst.
 * @param src String to be truncated.
 * @param truncation_len Maximum length of the returned string (not
 *                       counting the null-termination).
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Guarantees that dst string will contain null-termination.
 */
void str_truncate(char *dst, int dst_size, const char *src, int truncation_len);

/**
 * Returns the length of a null-terminated string.
 *
 * @ingroup Strings
 *
 * @param str Pointer to the string.
 *
 * @return Length of string in bytes excluding the null-termination.
 */
int str_length(const char *str);

/**
 * Performs printf formatting into a buffer.
 *
 * @ingroup Strings
 *
 * @param buffer Pointer to the buffer to receive the formatted string.
 * @param buffer_size Size of the buffer.
 * @param format printf formatting string.
 * @param args The variable argument list.
 *
 * @return Length of written string, even if it has been truncated.
 *
 * @remark See the C manual for syntax for the printf formatting string.
 * @remark The strings are treated as null-terminated strings.
 * @remark Guarantees that buffer string will contain null-termination.
 */
int str_format_v(char *buffer, int buffer_size, const char *format, va_list args)
	GNUC_ATTRIBUTE((format(printf, 3, 0)));

/**
 * Performs printf formatting into a buffer.
 *
 * @ingroup Strings
 *
 * @param buffer Pointer to the buffer to receive the formatted string.
 * @param buffer_size Size of the buffer.
 * @param format printf formatting string.
 * @param ... Parameters for the formatting.
 *
 * @return Length of written string, even if it has been truncated.
 *
 * @remark See the C manual for syntax for the printf formatting string.
 * @remark The strings are treated as null-terminated strings.
 * @remark Guarantees that buffer string will contain null-termination.
 */
int str_format(char *buffer, int buffer_size, const char *format, ...)
	GNUC_ATTRIBUTE((format(printf, 3, 4)));

#if !defined(CONF_DEBUG)
int str_format_int(char *buffer, size_t buffer_size, int value);

template<typename... Args>
int str_format_opt(char *buffer, int buffer_size, const char *format, Args... args)
{
	static_assert(sizeof...(args) > 0, "Use str_copy instead of str_format without format arguments");
	return str_format(buffer, buffer_size, format, args...);
}

template<>
inline int str_format_opt(char *buffer, int buffer_size, const char *format, int val)
{
	if(strcmp(format, "%d") == 0)
	{
		return str_format_int(buffer, buffer_size, val);
	}
	else
	{
		return str_format(buffer, buffer_size, format, val);
	}
}

#define str_format str_format_opt
#endif

/**
 * Trims specific number of words at the start of a string.
 *
 * @ingroup Strings
 *
 * @param str String to trim the words from.
 * @param words Count of words to trim.
 *
 * @return Trimmed string
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Leading whitespace is always trimmed.
 */
const char *str_trim_words(const char *str, int words);

/**
 * Check whether string has ASCII control characters.
 *
 * @ingroup Strings
 *
 * @param str String to check.
 *
 * @return Whether the string has ASCII control characters.
 *
 * @remark The strings are treated as null-terminated strings.
 */
bool str_has_cc(const char *str);

/**
 * Replaces all characters below 32 with whitespace.
 *
 * @ingroup Strings
 *
 * @param str String to sanitize.
 *
 * @remark The strings are treated as null-terminated strings.
 */
void str_sanitize_cc(char *str);

/**
 * Replaces all characters below 32 with whitespace with
 * exception to \t, \n and \r.
 *
 * @ingroup Strings
 *
 * @param str String to sanitize.
 *
 * @remark The strings are treated as null-terminated strings.
 */
void str_sanitize(char *str);

/**
 * Replaces all invalid filename characters with whitespace.
 *
 * @param str String to sanitize.
 * @remark The strings are treated as null-terminated strings.
 */
void str_sanitize_filename(char *str);

/**
 * Checks if a string is a valid filename on all supported platforms.
 *
 * @param str Filename to check.
 *
 * @return `true` if the string is a valid filename, `false` otherwise.
 *
 * @remark The strings are treated as null-terminated strings.
 */
bool str_valid_filename(const char *str);

/**
 * Removes leading and trailing spaces and limits the use of multiple spaces.
 *
 * @ingroup Strings
 *
 * @param str String to clean up.
 *
 * @remark The strings are treated as null-terminated strings.
 */
void str_clean_whitespaces(char *str);

/**
 * Skips leading non-whitespace characters.
 *
 * @ingroup Strings
 *
 * @param str Pointer to the string.
 *
 * @return Pointer to the first whitespace character found
 *		   within the string.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Whitespace is defined according to str_isspace.
 */
char *str_skip_to_whitespace(char *str);

/**
 * @ingroup Strings
 *
 * @see str_skip_to_whitespace
 */
const char *str_skip_to_whitespace_const(const char *str);

/**
 * Skips leading whitespace characters.
 *
 * @ingroup Strings
 *
 * @param str Pointer to the string.
 *
 * @return Pointer to the first non-whitespace character found
 *         within the string.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Whitespace is defined according to str_isspace.
 */
char *str_skip_whitespaces(char *str);

/**
 * @ingroup Strings
 *
 * @see str_skip_whitespaces
 */
const char *str_skip_whitespaces_const(const char *str);

/**
 * Compares to strings case insensitively.
 *
 * @ingroup Strings
 *
 * @param a String to compare.
 * @param b String to compare.
 *
 * @return `< 0` if string a is less than string b.
 * @return `0` if string a is equal to string b.
 * @return `> 0` if string a is greater than string b.
 *
 * @remark Only guaranteed to work with a-z/A-Z.
 * @remark The strings are treated as null-terminated strings.
 */
int str_comp_nocase(const char *a, const char *b);

/**
 * Compares up to `num` characters of two strings case insensitively.
 *
 * @ingroup Strings
 *
 * @param a String to compare.
 * @param b String to compare.
 * @param num Maximum characters to compare.
 *
 * @return `< 0` if string a is less than string b.
 * @return `0` if string a is equal to string b.
 * @return `> 0` if string a is greater than string b.
 *
 * @remark Only guaranteed to work with a-z/A-Z.
 * @remark Use `str_utf8_comp_nocase_num` for unicode support.
 * @remark The strings are treated as null-terminated strings.
 */
int str_comp_nocase_num(const char *a, const char *b, int num);

/**
 * Compares two strings case sensitive.
 *
 * @ingroup Strings
 *
 * @param a String to compare.
 * @param b String to compare.
 *
 * @return `< 0` if string a is less than string b.
 * @return `0` if string a is equal to string b.
 * @return `> 0` if string a is greater than string b.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int str_comp(const char *a, const char *b);

/**
 * Compares up to `num` characters of two strings case sensitive.
 *
 * @ingroup Strings
 *
 * @param a String to compare.
 * @param b String to compare.
 * @param num Maximum characters to compare.
 *
 * @return `< 0` if string a is less than string b.
 * @return `0` if string a is equal to string b.
 * @return `> 0` if string a is greater than string b.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int str_comp_num(const char *a, const char *b, int num);

/**
 * Compares two strings case insensitive, digit chars will be compared as numbers.
 *
 * @ingroup Strings
 *
 * @param a String to compare.
 * @param b String to compare.
 *
 * @return `< 0` - String a is less than string b
 * @return `0` - String a is equal to string b
 * @return `> 0` - String a is greater than string b
 *
 * @remark The strings are treated as null-terminated strings.
 */
int str_comp_filenames(const char *a, const char *b);

/**
 * Checks case insensitive whether the string begins with a certain prefix.
 *
 * @ingroup Strings
 *
 * @param str String to check.
 * @param prefix Prefix to look for.
 *
 * @return A pointer to the string `str` after the string prefix, or `nullptr` if
 *         the string prefix isn't a prefix of the string `str`.
 *
 * @remark The strings are treated as null-terminated strings.
 */
const char *str_startswith_nocase(const char *str, const char *prefix);

/**
 * Checks case sensitive whether the string begins with a certain prefix.
 *
 * @ingroup Strings
 *
 * @param str String to check.
 * @param prefix Prefix to look for.
 *
 * @return A pointer to the string `str` after the string prefix, or `nullptr` if
 *         the string prefix isn't a prefix of the string `str`.
 *
 * @remark The strings are treated as null-terminated strings.
 */
const char *str_startswith(const char *str, const char *prefix);

/**
 * Checks case insensitive whether the string ends with a certain suffix.
 *
 * @ingroup Strings
 *
 * @param str String to check.
 * @param suffix Suffix to look.
 *
 * @return A pointer to the beginning of the suffix in the string `str`.
 * @return `nullptr` if the string suffix isn't a suffix of the string `str`.
 *
 * @remark The strings are treated as null-terminated strings.
 */
const char *str_endswith_nocase(const char *str, const char *suffix);

/**
 * Checks case sensitive whether the string ends with a certain suffix.
 *
 * @param str String to check.
 * @param suffix Suffix to look for.
 *
 * @return A pointer to the beginning of the suffix in the string `str`.
 * @return `nullptr` if the string suffix isn't a suffix of the string `str`.
 *
 * @remark The strings are treated as null-terminated strings.
 */
const char *str_endswith(const char *str, const char *suffix);

/**
 * Computes the edit distance between two strings.
 *
 * @param a First string for the edit distance.
 * @param b Second string for the edit distance.
 *
 * @return The edit distance between the both strings.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int str_utf8_dist(const char *a, const char *b);

/**
 * Computes the edit distance between two strings, allows buffers
 * to be passed in.
 *
 * @ingroup Strings
 *
 * @param a First string for the edit distance.
 * @param b Second string for the edit distance.
 * @param buf Buffer for the function.
 * @param buf_len Length of the buffer, must be at least as long as
 *                twice the length of both strings combined plus two.
 *
 * @return The edit distance between the both strings.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int str_utf8_dist_buffer(const char *a, const char *b, int *buf, int buf_len);

/**
 * Computes the edit distance between two strings, allows buffers
 * to be passed in.
 *
 * @ingroup Strings
 *
 * @param a First string for the edit distance.
 * @param a_len Length of the first string.
 * @param b Second string for the edit distance.
 * @param b_len Length of the second string.
 * @param buf Buffer for the function.
 * @param buf_len Length of the buffer, must be at least as long as
 *                the length of both strings combined plus two.
 *
 * @return The edit distance between the both strings.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int str_utf32_dist_buffer(const int *a, int a_len, const int *b, int b_len, int *buf, int buf_len);

/**
 * Finds a string inside another string case insensitively.
 *
 * @ingroup Strings
 *
 * @param haystack String to search in.
 * @param needle String to search for.
 *
 * @return A pointer into `haystack` where the needle was found.
 * @return Returns `nullptr` if `needle` could not be found.
 *
 * @remark Only guaranteed to work with a-z/A-Z.
 * @remark Use str_utf8_find_nocase for unicode support.
 * @remark The strings are treated as null-terminated strings.
 */
const char *str_find_nocase(const char *haystack, const char *needle);

/**
 * Finds a string inside another string case sensitive.
 *
 * @ingroup Strings
 *
 * @param haystack String to search in.
 * @param needle String to search for.
 *
 * @return A pointer into `haystack` where the needle was found.
 * @return Returns `nullptr` if `needle` could not be found.
 *
 * @remark The strings are treated as null-terminated strings.
 */
const char *str_find(const char *haystack, const char *needle);

/**
 * @ingroup Strings
 *
 * @param haystack String to search in.
 * @param delim String to search for.
 * @param offset Number of characters into `haystack`.
 * @param start Will be set to the first delimiter on the left side of the offset (or `haystack` start).
 * @param end Will be set to the first delimiter on the right side of the offset (or `haystack` end).
 *
 * @return `true` if both delimiters were found.
 * @return `false` if a delimiter is missing (it uses `haystack` start and end as fallback).
 *
 * @remark The strings are treated as null-terminated strings.
 */
bool str_delimiters_around_offset(const char *haystay, const char *delim, int offset, int *start, int *end);

/**
 * Finds the last occurrence of a character
 *
 * @ingroup Strings
 *
 * @param haystack String to search in.
 * @param needle Character to search for.

 * @return A pointer into haystack where the needle was found.
 * @return Returns `nullptr` if needle could not be found.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark The zero-terminator character can also be found with this function.
 */
const char *str_rchr(const char *haystack, char needle);

/**
 * Counts the number of occurrences of a character in a string.
 *
 * @ingroup Strings
 *
 * @param haystack String to count in.
 * @param needle Character to count.

 * @return The number of characters in the haystack string matching
 *         the needle character.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark The number of zero-terminator characters cannot be counted.
 */
int str_countchr(const char *haystack, char needle);

/**
 * Takes a datablock and generates a hex string of it, with spaces between bytes.
 *
 * @ingroup Strings
 *
 * @param dst Buffer to fill with hex data.
 * @param dst_size Size of the buffer (at least 3 * data_size + 1 to contain all data).
 * @param data Data to turn into hex.
 * @param data_size Size of the data.
 *
 * @remark The destination buffer will be null-terminated.
 */
void str_hex(char *dst, int dst_size, const void *data, int data_size);

/**
 * Takes a datablock and generates a hex string of it, in the C style array format,
 * i.e. with bytes formatted in 0x00-0xFF notation and commas with spaces between the bytes.
 * The output can be split over multiple lines by specifying the maximum number of bytes
 * that should be printed per line.
 *
 * @ingroup Strings
 *
 * @param dst Buffer to fill with hex data.
 * @param dst_size Size of the buffer (at least `6 * data_size + 1` to contain all data).
 * @param data Data to turn into hex.
 * @param data_size Size of the data.
 * @param bytes_per_line After this many printed bytes a newline will be printed.
 *
 * @remark The destination buffer will be null-terminated.
 */
void str_hex_cstyle(char *dst, int dst_size, const void *data, int data_size, int bytes_per_line = 12);

/**
 * Takes a hex string *without spaces between bytes* and returns a byte array.
 *
 * @ingroup Strings
 *
 * @param dst Buffer for the byte array.
 * @param dst_size size of the buffer.
 * @param data String to decode.
 *
 * @return `2` if string doesn't exactly fit the buffer.
 * @return `1` if invalid character in string.
 * @return `0` if success.
 *
 * @remark The contents of the buffer is only valid on success.
 */
int str_hex_decode(void *dst, int dst_size, const char *src);

/**
 * Takes a datablock and generates the base64 encoding of it.
 *
 * @ingroup Strings
 *
 * @param dst Buffer to fill with base64 data.
 * @param dst_size Size of the buffer.
 * @param data Data to turn into base64.
 * @param data Size of the data.
 *
 * @remark The destination buffer will be null-terminated
 */
void str_base64(char *dst, int dst_size, const void *data, int data_size);

/**
 * Takes a base64 string without any whitespace and correct
 * padding and returns a byte array.
 *
 * @ingroup Strings
 *
 * @param dst Buffer for the byte array.
 * @param dst_size Size of the buffer.
 * @param data String to decode.
 *
 * @return `< 0` - Error.
 * @return `<= 0` - Success, length of the resulting byte buffer.
 *
 * @remark The contents of the buffer is only valid on success.
 */
int str_base64_decode(void *dst, int dst_size, const char *data);

/**
 * Copies a timestamp in the format year-month-day_hour-minute-second to the string.
 *
 * @ingroup Strings
 *
 * @param buffer Pointer to a buffer that shall receive the timestamp string.
 * @param buffer_size Size of the buffer.
 *
 * @remark Guarantees that buffer string will contain null-termination.
 */
void str_timestamp(char *buffer, int buffer_size);
void str_timestamp_format(char *buffer, int buffer_size, const char *format)
	GNUC_ATTRIBUTE((format(strftime, 3, 0)));
void str_timestamp_ex(time_t time, char *buffer, int buffer_size, const char *format)
	GNUC_ATTRIBUTE((format(strftime, 4, 0)));

/**
 * Parses a string into a timestamp following a specified format.
 *
 * @ingroup Timestamp
 *
 * @param string Pointer to the string to parse.
 * @param format The time format to use (for example `FORMAT_NOSPACE` below).
 * @param timestamp Pointer to the timestamp result.
 *
 * @return true on success, false if the string could not be parsed with the specified format
 */
bool timestamp_from_str(const char *string, const char *format, time_t *timestamp)
	GNUC_ATTRIBUTE((format(strftime, 2, 0)));

#define FORMAT_TIME "%H:%M:%S"
#define FORMAT_SPACE "%Y-%m-%d %H:%M:%S"
#define FORMAT_NOSPACE "%Y-%m-%d_%H-%M-%S"

enum
{
	TIME_DAYS,
	TIME_HOURS,
	TIME_MINS,
	TIME_HOURS_CENTISECS,
	TIME_MINS_CENTISECS,
	TIME_SECS_CENTISECS,
};

/**
 * Formats a time string.
 *
 * @ingroup Timestamp
 *
 * @param centisecs Time in centiseconds.
 * @param format Format of the time string, see enum above, for example `TIME_DAYS`.
 * @param buffer Pointer to a buffer that shall receive the timestamp string.
 * @param buffer_size Size of the buffer.
 *
 * @return Number of bytes written, `-1` on invalid format or `buffer_size <= 0`.
 */
int str_time(int64_t centisecs, int format, char *buffer, int buffer_size);

/**
 * Formats a time string.
 *
 * @ingroup Timestamp
 *
 * @param secs Time in seconds.
 * @param format Format of the time string, see enum above, for example `TIME_DAYS`.
 * @param buffer Pointer to a buffer that shall receive the timestamp string.
 * @param buffer_size Size of the buffer.
 *
 * @remark The time is rounded to the nearest centisecond.
 *
 * @return Number of bytes written, `-1` on invalid format or `buffer_size <= 0`.
 */
int str_time_float(float secs, int format, char *buffer, int buffer_size);

/**
 * Escapes \ and " characters in a string.
 *
 * @param dst Destination array pointer, gets increased, will point to the terminating null.
 * @param src Source array.
 * @param end End of destination array.
 */
void str_escape(char **dst, const char *src, const char *end);

/**
 * Utilities for accessing the file system.
 *
 * @defgroup Filesystem
 */

/**
 * Lists the files and folders in a directory.
 *
 * @ingroup Filesystem
 *
 * @param dir Directory to list.
 * @param cb Callback function to call for each entry.
 * @param type Type of the directory.
 * @param user Pointer to give to the callback.
 *
 * @remark The strings are treated as null-terminated strings.
 */
void fs_listdir(const char *dir, FS_LISTDIR_CALLBACK cb, int type, void *user);

/**
 * Lists the files and folders in a directory and gets additional file information.
 *
 * @ingroup Filesystem
 *
 * @param dir Directory to list.
 * @param cb Callback function to call for each entry.
 * @param type Type of the directory.
 * @param user Pointer to give to the callback.
 *
 * @remark The strings are treated as null-terminated strings.
 */
void fs_listdir_fileinfo(const char *dir, FS_LISTDIR_CALLBACK_FILEINFO cb, int type, void *user);

/**
 * Creates a directory.
 *
 * @ingroup Filesystem
 *
 * @param path Directory to create.
 *
 * @return `0` on success. Negative value on failure.
 *
 * @remark Does not create several directories if needed. "a/b/c" will
 *         result in a failure if b or a does not exist.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_makedir(const char *path);

/**
 * Removes a directory.
 *
 * @ingroup Filesystem
 *
 * @param path Directory to remove.
 *
 * @return `0` on success. Negative value on failure.
 *
 * @remark Cannot remove a non-empty directory.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_removedir(const char *path);

/**
 * Recursively creates parent directories for a file or directory.
 *
 * @ingroup Filesystem
 *
 * @param path File or directory for which to create parent directories.
 *
 * @return `0` on success. Negative value on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_makedir_rec_for(const char *path);

/**
 * Fetches per user configuration directory.
 *
 * @ingroup Filesystem
 *
 * @param appname Name of the application.
 * @param path Buffer that will receive the storage path.
 * @param max Size of the buffer.
 *
 * @return `0` on success. Negative value on failure.
 *
 * @remark Returns ~/.appname on UNIX based systems.
 * @remark Returns ~/Library/Applications Support/appname on macOS.
 * @remark Returns %APPDATA%/Appname on Windows based systems.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_storage_path(const char *appname, char *path, int max);

/**
 * Checks if a file exists.
 *
 * @ingroup Filesystem
 *
 * @param path The path to check.
 *
 * @return `1` if a file with the given path exists.
 * @return `0` on failure or if the file does not exist.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_is_file(const char *path);

/**
 * Checks if a folder exists.
 *
 * @ingroup Filesystem
 *
 * @param path The path to check.
 *
 * @return `1` if a folder with the given path exists.
 * @return `0` on failure or if the folder does not exist.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_is_dir(const char *path);

/**
 * Checks whether a given path is relative or absolute.
 *
 * @ingroup Filesystem
 *
 * @param path Path to check.
 *
 * @return `1` if relative, `0` if absolute.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_is_relative_path(const char *path);

/**
 * Changes the current working directory.
 *
 * @ingroup Filesystem
 *
 * @param path New working directory path.
 *
 * @return `0` on success. `1` on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_chdir(const char *path);

/**
 * Gets the current working directory.
 *
 * @ingroup Filesystem
 *
 * @param buffer Buffer that will receive the current working directory.
 * @param buffer_size Size of the buffer.
 *
 * @return Pointer to the buffer on success, `nullptr` on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 */
char *fs_getcwd(char *buffer, int buffer_size);

/**
 * Gets the name of a file or folder specified by a path,
 * i.e. the last segment of the path.
 *
 * @ingroup Filesystem
 *
 * @param path Path from which to retrieve the filename.
 *
 * @return Filename of the path.
 *
 * @remark Supports forward and backward slashes as path segment separator.
 * @remark No distinction between files and folders is being made.
 * @remark The strings are treated as null-terminated strings.
 */
const char *fs_filename(const char *path);

/**
 * Splits a filename into name (without extension) and file extension.
 *
 * @ingroup Filesystem
 *
 * @param filename The filename to split.
 * @param name Buffer that will receive the name without extension, may be nullptr.
 * @param name_size Size of the name buffer (ignored if name is nullptr).
 * @param extension Buffer that will receive the extension, may be nullptr.
 * @param extension_size Size of the extension buffer (ignored if extension is nullptr).
 *
 * @remark Does NOT handle forward and backward slashes.
 * @remark No distinction between files and folders is being made.
 * @remark The strings are treated as null-terminated strings.
 */
void fs_split_file_extension(const char *filename, char *name, size_t name_size, char *extension = nullptr, size_t extension_size = 0);

/**
 * Get the parent directory of a directory.
 *
 * @ingroup Filesystem
 *
 * @param path Path of the directory. The parent will be store in this buffer as well.
 *
 * @return `0` on success. `1` on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_parent_dir(char *path);

/**
 * Deletes a file.
 *
 * @ingroup Filesystem
 *
 * @param filename Path of the file to delete.
 *
 * @return `0` on success. `1` on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Returns an error if the path specifies a directory name.
 */
int fs_remove(const char *filename);

/**
 * Renames the file or directory. If the paths differ the file will be moved.
 *
 * @ingroup Filesystem
 *
 * @param oldname The current path of a file or directory.
 * @param newname The new path for the file or directory.
 *
 * @return `0` on success. `1` on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_rename(const char *oldname, const char *newname);

/**
 * Gets the creation and the last modification date of a file or directory.
 *
 * @ingroup Filesystem
 *
 * @param name Path of a file or directory.
 * @param created Pointer where the creation time will be stored.
 * @param modified Pointer where the modification time will be stored.
 *
 * @return `0` on success. non-zero on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Returned time is in seconds since UNIX Epoch.
 */
int fs_file_time(const char *name, time_t *created, time_t *modified);

/**
 * Swaps the endianness of data. Each element is swapped individually by reversing its bytes.
 *
 * @param data Pointer to data to be swapped.
 * @param elem_size Size in bytes of each element.
 * @param num Number of elements.
 *
 * @remark The caller must ensure that the data is at least `elem_size * num` bytes in size.
 */
void swap_endian(void *data, unsigned elem_size, unsigned num);

void net_stats(NETSTATS *stats);

int str_toint(const char *str);
bool str_toint(const char *str, int *out);
int str_toint_base(const char *str, int base);
unsigned long str_toulong_base(const char *str, int base);
int64_t str_toint64_base(const char *str, int base = 10);
float str_tofloat(const char *str);
bool str_tofloat(const char *str, float *out);

/**
 * Determines whether a character is whitespace.
 *
 * @ingroup Strings
 *
 * @param c the character to check.
 *
 * @return `1` if the character is whitespace, `0` otherwise.
 *
 * @remark The following characters are considered whitespace: ' ', '\n', '\r', '\t'.
 */
int str_isspace(char c);

char str_uppercase(char c);

bool str_isnum(char c);

int str_isallnum(const char *str);

int str_isallnum_hex(const char *str);

unsigned str_quickhash(const char *str);

int str_utf8_to_skeleton(const char *str, int *buf, int buf_len);

/**
 * Checks if two strings only differ by confusable characters.
 *
 * @ingroup Strings
 *
 * @param str1 String to compare.
 * @param str2 String to compare.
 *
 * @return `0` if the strings are confusables.
 */
int str_utf8_comp_confusable(const char *str1, const char *str2);

/**
 * Converts the given Unicode codepoint to lowercase (locale insensitive).
 *
 * @ingroup Strings
 *
 * @param code Unicode codepoint to convert.
 *
 * @return Lowercase codepoint, or the original codepoint if there is no lowercase version.
 */
int str_utf8_tolower_codepoint(int code);

/**
 * Converts the given UTF-8 string to lowercase (locale insensitive).
 *
 * @ingroup Strings
 *
 * @param str String to convert to lowercase.
 * @param output Buffer that will receive the lowercase string.
 * @param size Size of the output buffer.
 *
 * @remark The strings are treated as zero-terminated strings.
 * @remark This function does not work in-place as converting a UTF-8 string to
 *         lowercase may increase its length.
 */
void str_utf8_tolower(const char *input, char *output, size_t size);

/**
 * Compares two UTF-8 strings case insensitively.
 *
 * @ingroup Strings
 *
 * @param a String to compare.
 * @param b String to compare.
 *
 * @return `< 0` if string a is less than string b.
 * @return `0` if string a is equal to string b.
 * @return `> 0` if string a is greater than string b.
 */
int str_utf8_comp_nocase(const char *a, const char *b);

/**
 * Compares up to `num` bytes of two UTF-8 strings case insensitively.
 *
 * @ingroup Strings
 *
 * @param a String to compare.
 * @param b String to compare.
 * @param num Maximum bytes to compare.
 *
 * @return `< 0` if string a is less than string b.
 * @return `0` if string a is equal to string b.
 * @return `> 0` if string a is greater than string b.
 */
int str_utf8_comp_nocase_num(const char *a, const char *b, int num);

/**
 * Finds a UTF-8 string inside another UTF-8 string case insensitively.
 *
 * @ingroup Strings
 *
 * @param haystack String to search in.
 * @param needle String to search for.
 * @param end A pointer that will be set to a pointer into haystack directly behind the
 *            last character where the needle was found. Will be set to `nullptr `if needle
 *            could not be found. Optional parameter.
 *
 * @return A pointer into haystack where the needle was found.
 * @return Returns `nullptr` if needle could not be found.
 *
 * @remark The strings are treated as null-terminated strings.
 */
const char *str_utf8_find_nocase(const char *haystack, const char *needle, const char **end = nullptr);

/**
 * Checks whether the given Unicode codepoint renders as space.
 *
 * @ingroup Strings
 *
 * @param code Unicode codepoint to check.
 *
 * @return Whether the codepoint is a space.
 */
int str_utf8_isspace(int code);

/**
 * Checks whether a given byte is the start of a UTF-8 character.
 *
 * @ingroup Strings
 *
 * @param c Byte to check.
 *
 * @return Whether the char starts a UTF-8 character.
 */
int str_utf8_isstart(char c);

/**
 * Skips leading characters that render as spaces.
 *
 * @ingroup Strings
 *
 * @param str Input string.
 *
 * @return Pointer to the first non-whitespace character found within the string.
 * @remark The strings are treated as null-terminated strings.
 */
const char *str_utf8_skip_whitespaces(const char *str);

/**
 * Removes trailing characters that render as spaces by modifying the string in-place.
 *
 * @ingroup Strings
 *
 * @param param Input string.
 *
 * @remark The string is modified in-place.
 * @remark The strings are treated as null-terminated.
 */
void str_utf8_trim_right(char *param);

/**
 * Moves a cursor backwards in an UTF-8 string,
 *
 * @ingroup Strings
 *
 * @param str UTF-8 string.
 * @param cursor Position in the string.
 *
 * @return New cursor position.
 *
 * @remark Won't move the cursor less then 0.
 * @remark The strings are treated as null-terminated.
 */
int str_utf8_rewind(const char *str, int cursor);

/**
 * Fixes truncation of a Unicode character at the end of a UTF-8 string.
 *
 * @ingroup Strings
 *
 * @param str UTF-8 string.
 *
 * @return The new string length.
 *
 * @remark The strings are treated as null-terminated.
 */
int str_utf8_fix_truncation(char *str);

/**
 * Moves a cursor forwards in an UTF-8 string.
 *
 * @ingroup Strings
 *
 * @param str UTF-8 string.
 * @param cursor Position in the string.
 *
 * @return New cursor position.
 *
 * @remark Won't move the cursor beyond the null-termination marker.
 * @remark The strings are treated as null-terminated.
 */
int str_utf8_forward(const char *str, int cursor);

/**
 * Decodes a UTF-8 codepoint.
 *
 * @ingroup Strings
 *
 * @param ptr Pointer to a UTF-8 string. This pointer will be moved forward.
 *
 * @return The Unicode codepoint. `-1` for invalid input and 0 for end of string.
 *
 * @remark This function will also move the pointer forward.
 * @remark You may call this function again after an error occurred.
 * @remark The strings are treated as null-terminated.
 */
int str_utf8_decode(const char **ptr);

/**
 * Encode a UTF-8 character.
 *
 * @ingroup Strings
 *
 * @param ptr Pointer to a buffer that should receive the data. Should be able to hold at least 4 bytes.
 *
 * @return Number of bytes put into the buffer.
 *
 * @remark Does not do null-termination of the string.
 */
int str_utf8_encode(char *ptr, int chr);

/**
 * Checks if a strings contains just valid UTF-8 characters.
 *
 * @ingroup Strings
 *
 * @param str Pointer to a possible UTF-8 string.
 *
 * @return `0` if invalid characters were found, `1` if only valid characters were found.
 *
 * @remark The string is treated as null-terminated UTF-8 string.
 */
int str_utf8_check(const char *str);

/**
 * Copies a number of UTF-8 characters from one string to another.
 *
 * @ingroup Strings
 *
 * @param dst Pointer to a buffer that shall receive the string.
 * @param src String to be copied.
 * @param dst_size Size of the buffer dst.
 * @param num Maximum number of UTF-8 characters to be copied.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Garantees that dst string will contain null-termination.
 */
void str_utf8_copy_num(char *dst, const char *src, int dst_size, int num);

/**
 * Determines the byte size and UTF-8 character count of a UTF-8 string.
 *
 * @ingroup Strings
 *
 * @param str Pointer to the string.
 * @param max_size Maximum number of bytes to count.
 * @param max_count Maximum number of UTF-8 characters to count.
 * @param size Pointer to store size (number of non. Zero bytes) of the string.
 * @param count Pointer to store count of UTF-8 characters of the string.
 *
 * @remark The string is treated as null-terminated UTF-8 string.
 * @remark It's the user's responsibility to make sure the bounds are aligned.
 */
void str_utf8_stats(const char *str, size_t max_size, size_t max_count, size_t *size, size_t *count);

/**
 * Converts a byte offset of a UTF-8 string to the UTF-8 character offset.
 *
 * @ingroup Strings
 *
 * @param text Pointer to the string.
 * @param byte_offset Offset in bytes.
 *
 * @return Offset in UTF-8 characters. Clamped to the maximum length of the string in UTF-8 characters.
 *
 * @remark The string is treated as a null-terminated UTF-8 string.
 * @remark It's the user's responsibility to make sure the bounds are aligned.
 */
size_t str_utf8_offset_bytes_to_chars(const char *str, size_t byte_offset);

/**
 * Converts a UTF-8 character offset of a UTF-8 string to the byte offset.
 *
 * @ingroup Strings
 *
 * @param text Pointer to the string.
 * @param char_offset Offset in UTF-8 characters.
 *
 * @return Offset in bytes. Clamped to the maximum length of the string in bytes.
 *
 * @remark The string is treated as a null-terminated UTF-8 string.
 * @remark It's the user's responsibility to make sure the bounds are aligned.
 */
size_t str_utf8_offset_chars_to_bytes(const char *str, size_t char_offset);

/**
 * Writes the next token after str into buf, returns the rest of the string.
 *
 * @ingroup Strings
 *
 * @param str Pointer to string.
 * @param delim Delimiter for tokenization.
 * @param buffer Buffer to store token in.
 * @param buffer_size Size of the buffer.
 *
 * @return Pointer to rest of the string.
 *
 * @remark The token is always null-terminated.
 */
const char *str_next_token(const char *str, const char *delim, char *buffer, int buffer_size);

/**
 * Checks if needle is in list delimited by delim.
 *
 * @param list List.
 * @param delim List delimiter.
 * @param needle Item that is being looked for.
 *
 * @return `1` - Item is in list.
 * @return `0` - Item isn't in list.
 *
* @remark The strings are treated as null-terminated strings.
 */
int str_in_list(const char *list, const char *delim, const char *needle);

/**
 * Packs 4 big endian bytes into an unsigned.
 *
 * @param bytes Pointer to an array of bytes that will be packed.
 *
 * @return The packed unsigned.
 *
 * @remark Assumes the passed array is least 4 bytes in size.
 * @remark Assumes unsigned is 4 bytes in size.
 *
 * @see uint_to_bytes_be
 */
unsigned bytes_be_to_uint(const unsigned char *bytes);

/**
 * Packs an unsigned into 4 big endian bytes.
 *
 * @param bytes Pointer to an array where the bytes will be stored.
 * @param value The values that will be packed into the array.
 *
 * @remark Assumes the passed array is least 4 bytes in size.
 * @remark Assumes unsigned is 4 bytes in size.
 *
 * @see bytes_be_to_uint
 */
void uint_to_bytes_be(unsigned char *bytes, unsigned value);

/**
 * Shell, process management, OS specific functionality.
 *
 * @defgroup Shell
 */

/**
 * Returns the ID of the current process.
 *
 * @ingroup Shell
 *
 * @return PID of the current process.
 */
int pid();

/**
 * Fixes the command line arguments to be encoded in UTF-8 on all systems.
 *
 * @ingroup Shell
 *
 * @param argc A pointer to the argc parameter that was passed to the main function.
 * @param argv A pointer to the argv parameter that was passed to the main function.
 *
 * @remark You need to call @link cmdline_free @endlink once you're no longer using the results.
 */
void cmdline_fix(int *argc, const char ***argv);

/**
 * Frees memory that was allocated by @link cmdline_fix @endlink.
 *
 * @ingroup Shell
 *
 * @param argc The argc obtained from `cmdline_fix`.
 * @param argv The argv obtained from `cmdline_fix`.
 */
void cmdline_free(int argc, const char **argv);

#if defined(CONF_FAMILY_WINDOWS)
/**
 * A handle for a process.
 *
 * @ingroup Shell
 */
typedef void *PROCESS;
/**
 * A handle that denotes an invalid process.
 *
 * @ingroup Shell
 */
constexpr PROCESS INVALID_PROCESS = nullptr;
#else
/**
 * A handle for a process.
 *
 * @ingroup Shell
 */
typedef pid_t PROCESS;
/**
 * A handle that denotes an invalid process.
 *
 * @ingroup Shell
 */
constexpr PROCESS INVALID_PROCESS = 0;
#endif
#if !defined(CONF_PLATFORM_ANDROID)
/**
 * Determines the initial window state when using @link shell_execute @endlink
 * to execute a process.
 *
 * @ingroup Shell
 *
 * @remark Currently only supported on Windows.
 */
enum class EShellExecuteWindowState
{
	/**
	 * The process window is opened in the foreground and activated.
	 */
	FOREGROUND,

	/**
	 * The process window is opened in the background without focus.
	 */
	BACKGROUND,
};

#if defined(CONF_FAMILY_WINDOWS)
/**
 * Converts an array of arguments into a wide string with proper escaping for Windows command-line usage.
 *
 * @ingroup Shell
 *
 * @param arguments Array of arguments.
 * @param num_arguments The number of arguments.
 *
 * @return Wide string of arguments with escaped quotes.
 */
std::wstring windows_args_to_wide(const char **arguments, const size_t num_arguments);
#endif

/**
 * Executes a given file.
 *
 * @ingroup Shell
 *
 * @param file The file to execute.
 * @param window_state The window state how the process window should be shown.
 * @param arguments Optional array of arguments to pass to the process.
 * @param num_arguments The number of arguments.
 *
 * @return Handle of the new process, or @link INVALID_PROCESS @endlink on error.
 */
PROCESS shell_execute(const char *file, EShellExecuteWindowState window_state, const char **arguments = nullptr, const size_t num_arguments = 0);

/**
 * Sends kill signal to a process.
 *
 * @ingroup Shell
 *
 * @param process Handle of the process to kill.
 *
 * @return `1` on success, `0` on error.
 */
int kill_process(PROCESS process);

/**
 * Checks if a process is alive.
 *
 * @ingroup Shell
 *
 * @param process Handle/PID of the process.
 *
 * @return `true` if the process is currently running,
 * @return `false` if the process is not running (dead).
 */
bool is_process_alive(PROCESS process);

/**
 * Opens a link in the browser.
 *
 * @ingroup Shell
 *
 * @param link The link to open in a browser.
 *
 * @return `1` on success, `0` on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark This may not be called with untrusted input or it'll result in arbitrary code execution, especially on Windows.
 */
int open_link(const char *link);

/**
 * Opens a file or directory with the default program.
 *
 * @ingroup Shell
 *
 * @param path The file or folder to open with the default program.
 *
 * @return `1` on success, `0` on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark This may not be called with untrusted input or it'll result in arbitrary code execution, especially on Windows.
 */
int open_file(const char *path);
#endif // !defined(CONF_PLATFORM_ANDROID)

/**
 * Secure random number generation.
 *
 * @defgroup Secure-Random
 */

/**
 * Generates a null-terminated password of length `2 * random_length`.
 *
 * @ingroup Secure-Random
 *
 * @param buffer Pointer to the start of the output buffer.
 * @param length Length of the buffer.
 * @param random Pointer to a randomly-initialized array of shorts.
 * @param random_length Length of the short array.
 */
void generate_password(char *buffer, unsigned length, const unsigned short *random, unsigned random_length);

/**
 * Initializes the secure random module.
 * You *MUST* check the return value of this function.
 *
 * @ingroup Secure-Random
 *
 * @return `0` on success.
 */
[[nodiscard]] int secure_random_init();

/**
 * Uninitializes the secure random module.
 *
 * @ingroup Secure-Random
 *
 * @return `0` on success.
 */
int secure_random_uninit();

/**
 * Fills the buffer with the specified amount of random password characters.
 *
 * @ingroup Secure-Random
 *
 * @param buffer Pointer to the start of the buffer.
 * @param length Length of the buffer.
 * @param pw_length Length of the desired password.
 *
 * @remark The desired password length must be greater or equal to 6,
 *         even and smaller or equal to 128.
 */
void secure_random_password(char *buffer, unsigned length, unsigned pw_length);

/**
 * Fills the buffer with the specified amount of random bytes.
 *
 * @ingroup Secure-Random
 *
 * @param buffer Pointer to the start of the buffer.
 * @param length Length of the buffer.
 */
void secure_random_fill(void *bytes, unsigned length);

/**
 * Returns random `int`.
 *
 * @ingroup Secure-Random
 *
 * @return Random int.
 *
 * @remark Can be used as a replacement for the `rand` function.
 */
int secure_rand();

/**
 * Returns a random nonnegative integer below the given number,
 * with a uniform distribution.
 *
 * @ingroup Secure-Random
 *
 * @param below Upper limit (exclusive) of integers to return.
 *
 * @return Random nonnegative below the given number.
 */
int secure_rand_below(int below);

/**
 * Returns a human-readable version string of the operating system.
 *
 * @ingroup Shell
 *
 * @param version Buffer to use for the output.
 * @param length Length of the output buffer.
 *
 * @return `true` on success, `false` on failure.
 */
bool os_version_str(char *version, size_t length);

/**
 * Returns a string of the preferred locale of the user / operating system.
 * The string conforms to [RFC 3066](https://www.ietf.org/rfc/rfc3066.txt)
 * and only contains the characters `a`-`z`, `A`-`Z`, `0`-`9` and `-`.
 * If the preferred locale could not be determined this function
 * falls back to the locale `"en-US"`.
 *
 * @ingroup Shell
 *
 * @param locale Buffer to use for the output.
 * @param length Length of the output buffer.
 *
 * @remark The strings are treated as null-terminated strings.
 */
void os_locale_str(char *locale, size_t length);

/**
 * @defgroup Crash-dumping
 */

/**
 * Initializes the crash dumper and sets the filename to write the crash dump
 * to, if support for crash logging was compiled in. Otherwise does nothing.
 *
 * @ingroup Crash-dumping
 *
 * @param log_file_path Absolute path to which crash log file should be written.
 */
void crashdump_init_if_available(const char *log_file_path);

/**
 * Fetches a sample from a high resolution timer and converts it in nanoseconds.
 *
 * @ingroup Time
 *
 * @return Current value of the timer in nanoseconds.
 */
std::chrono::nanoseconds time_get_nanoseconds();

/**
 * Fixes the command line arguments to be encoded in UTF-8 on all systems.
 * This is a RAII wrapper for @link cmdline_fix @endlink and @link cmdline_free @endlink.
 *
 * @ingroup Shell
 */
class CCmdlineFix
{
	int m_Argc;
	const char **m_ppArgv;

public:
	CCmdlineFix(int *pArgc, const char ***pppArgv)
	{
		cmdline_fix(pArgc, pppArgv);
		m_Argc = *pArgc;
		m_ppArgv = *pppArgv;
	}
	~CCmdlineFix()
	{
		cmdline_free(m_Argc, m_ppArgv);
	}
};

#if defined(CONF_FAMILY_WINDOWS)
/**
 * Converts a UTF-8 encoded string to a wide character string
 * for use with the Windows API.
 *
 * @ingroup Shell
 *
 * @param str The UTF-8 encoded string to convert.
 *
 * @return The argument as a wide character string.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Fails with assertion error if passed UTF-8 is invalid.
 */
std::wstring windows_utf8_to_wide(const char *str);

/**
 * Converts a wide character string obtained from the Windows API
 * to a UTF-8 encoded string.
 *
 * @ingroup Shell
 *
 * @param wide_str The wide character string to convert.
 *
 * @return The argument as a UTF-8 encoded string, wrapped in an optional.
 * The optional is empty, if the wide string contains invalid codepoints.
 *
 * @remark The strings are treated as null-terminated strings.
 */
std::optional<std::string> windows_wide_to_utf8(const wchar_t *wide_str);

/**
 * This is a RAII wrapper to initialize/uninitialize the Windows COM library,
 * which may be necessary for using the @link open_file @endlink and
 * @link open_link @endlink functions.
 * Must be used on every thread. It's automatically used on threads created
 * with @link thread_init @endlink. Pass `true` to the constructor on threads
 * that own a window (i.e. pump a message queue).
 *
 * @ingroup Shell
 */
class CWindowsComLifecycle
{
public:
	CWindowsComLifecycle(bool HasWindow);
	~CWindowsComLifecycle();
};

/**
 * Registers a protocol handler.
 *
 * @ingroup Shell
 *
 * @param protocol_name The name of the protocol.
 * @param executable The absolute path of the executable that will be associated with the protocol.
 * @param updated Pointer to a variable that will be set to `true`, iff the shell needs to be updated.
 *
 * @return `true` on success, `false` on failure.
 *
 * @remark The caller must later call @link shell_update @endlink, iff the shell needs to be updated.
 */
bool shell_register_protocol(const char *protocol_name, const char *executable, bool *updated);

/**
 * Registers a file extension.
 *
 * @ingroup Shell
 *
 * @param extension The file extension, including the leading dot.
 * @param description A readable description for the file extension.
 * @param executable_name A unique name that will used to describe the application.
 * @param executable The absolute path of the executable that will be associated with the file extension.
 * @param updated Pointer to a variable that will be set to `true`, iff the shell needs to be updated.
 *
 * @return `true` on success, `false` on failure.
 *
 * @remark The caller must later call @link shell_update @endlink, iff the shell needs to be updated.
 */
bool shell_register_extension(const char *extension, const char *description, const char *executable_name, const char *executable, bool *updated);

/**
 * Registers an application.
 *
 * @ingroup Shell
 *
 * @param name Readable name of the application.
 * @param executable The absolute path of the executable being registered.
 * @param updated Pointer to a variable that will be set to `true`, iff the shell needs to be updated.
 *
 * @return `true` on success, `false` on failure.
 *
 * @remark The caller must later call @link shell_update @endlink, iff the shell needs to be updated.
 */
bool shell_register_application(const char *name, const char *executable, bool *updated);

/**
 * Unregisters a protocol or file extension handler.
 *
 * @ingroup Shell
 *
 * @param shell_class The shell class to delete.
 * For protocols this is the name of the protocol.
 * For file extensions this is the program ID associated with the file extension.
 * @param updated Pointer to a variable that will be set to `true`, iff the shell needs to be updated.
 *
 * @return `true` on success, `false` on failure.
 *
 * @remark The caller must later call @link shell_update @endlink, iff the shell needs to be updated.
 */
bool shell_unregister_class(const char *shell_class, bool *updated);

/**
 * Unregisters an application.
 *
 * @ingroup Shell
 *
 * @param executable The absolute path of the executable being unregistered.
 * @param updated Pointer to a variable that will be set to `true`, iff the shell needs to be updated.
 *
 * @return `true` on success, `false` on failure.
 *
 * @remark The caller must later call @link shell_update @endlink, iff the shell needs to be updated.
 */
bool shell_unregister_application(const char *executable, bool *updated);

/**
 * Notifies the system that a protocol or file extension has been changed and the shell needs to be updated.
 *
 * @ingroup Shell
 *
 * @remark This is a potentially expensive operation, so it should only be called when necessary.
 */
void shell_update();
#endif

#endif
