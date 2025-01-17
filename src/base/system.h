/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

/*
	Title: OS Abstraction
*/

#ifndef BASE_SYSTEM_H
#define BASE_SYSTEM_H

#include "detect.h"
#include "types.h"

#ifndef __USE_GNU
#define __USE_GNU
#endif

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

/**
 * @defgroup Debug
 *
 * Utilities for debugging.
 */

/**
 * @ingroup Debug
 *
 * Breaks into the debugger based on a test.
 *
 * @param test Result of the test.
 * @param msg Message that should be printed if the test fails.
 *
 * @remark Also works in release mode.
 *
 * @see dbg_break
 */
#define dbg_assert(test, msg) dbg_assert_imp(__FILE__, __LINE__, test, msg)
void dbg_assert_imp(const char *filename, int line, bool test, const char *msg);

#ifdef __clang_analyzer__
#include <cassert>
#undef dbg_assert
#define dbg_assert(test, msg) assert(test)
#endif

#ifdef __GNUC__
#define GNUC_ATTRIBUTE(x) __attribute__(x)
#else
#define GNUC_ATTRIBUTE(x)
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
 * @defgroup Memory
 * Memory management utilities.
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
 * @param a First block of data
 * @param b Second block of data
 * @param size Size of the data to compare
 *
 * @return < 0 - Block a is less than block b.
 * @return 0 - Block a is equal to block b.
 * @return > 0 - Block a is greater than block b.
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
 * @return true if the block has a null byte, false otherwise.
 */
bool mem_has_null(const void *block, size_t size);

/**
 * @defgroup File-IO
 *
 * I/O related operations.
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
 * @param File to open.
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
 * have to be allocated. Large files should not be loaded into memory.
 */
bool io_read_all(IOHANDLE io, void **result, unsigned *result_len);

/**
 * Reads the rest of the file into a zero-terminated buffer with
 * no internal null bytes.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file to read data from.
 *
 * @return The file's remaining contents, or `nullptr` on failure.
 *
 * @remark Guarantees that there are no internal null bytes.
 * @remark Guarantees that result will contain zero-termination.
 * @remark The result must be freed after it has been used.
 * @remark The function will fail if more than 1 GiB of memory would
 * have to be allocated. Large files should not be loaded into memory.
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
 * @return 0 on success.
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
 * @ingroup File-IO
 *
 * Returns a handle for the standard input.
 *
 * @return An @link IOHANDLE @endlink for the standard input.
 *
 * @remark The handle must not be closed.
 */
IOHANDLE io_stdin();

/**
 * @ingroup File-IO
 *
 * Returns a handle for the standard output.
 *
 * @return An @link IOHANDLE @endlink for the standard output.
 *
 * @remark The handle must not be closed.
 */
IOHANDLE io_stdout();

/**
 * @ingroup File-IO
 *
 * Returns a handle for the standard error.
 *
 * @return An @link IOHANDLE @endlink for the standard error.
 *
 * @remark The handle must not be closed.
 */
IOHANDLE io_stderr();

/**
 * @ingroup File-IO
 *
 * Returns a handle for the current executable.
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
 * Checks whether errors have occurred during the asynchronous
 * writing.
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
 * @defgroup Threads
 * Threading related functions.
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
 * @defgroup Time
 *
 * Time utilities.
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

/*
	Function: net_host_lookup
		Does a hostname lookup by name and fills out the passed
		NETADDR struct with the received details.

	Returns:
		0 on success.
*/
int net_host_lookup(const char *hostname, NETADDR *addr, int types);

/**
 * Compares two network addresses.
 *
 * @ingroup Network-General
 *
 * @param a Address to compare
 * @param b Address to compare to.
 *
 * @return `< 0` - Address a is less than address b
 * @return `0` - Address a is equal to address b
 * @return `> 0` - Address a is greater than address b
 */
int net_addr_comp(const NETADDR *a, const NETADDR *b);

/**
 * Compares two network addresses ignoring port.
 *
 * @ingroup Network-General
 *
 * @param a Address to compare
 * @param b Address to compare to.
 *
 * @return `< 0` - Address a is less than address b
 * @return `0` - Address a is equal to address b
 * @return `> 0` - Address a is greater than address b
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
 * @remark The string will always be zero terminated.
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
 * @param addr Address to fill in.
 * @param string String to parse.
 * @param host_buf Pointer to a buffer to write the host to
 *                 It will include the port if one is included in the url
 *                 It can also be set to NULL then it will be ignored
 * @param host_buf_size Size of the host buffer or 0 if no host_buf pointer is given
 *
 * @return 0 on success,
 *         positive if the input wasn't a valid DDNet URL,
 *         negative if the input is a valid DDNet URL but the host part was not a valid IPv4/IPv6 address
 */
int net_addr_from_url(NETADDR *addr, const char *string, char *host_buf, size_t host_buf_size);

/**
 * Turns string into a network address.
 *
 * @param addr Address to fill in.
 * @param string String to parse.
 *
 * @return 0 on success
 */
int net_addr_from_str(NETADDR *addr, const char *string);

/**
 * @defgroup Network-UDP
 * @ingroup Network-General
 */

/*
	Function: net_socket_type
		Determine a socket's type.

	Parameters:
		sock - Socket whose type should be determined.

	Returns:
		The socket type, a bitset of `NETTYPE_IPV4`, `NETTYPE_IPV6` and
		`NETTYPE_WEBSOCKET_IPV4`.
*/
int net_socket_type(NETSOCKET sock);

/*
	Function: net_udp_create
		Creates a UDP socket and binds it to a port.

	Parameters:
		bindaddr - Address to bind the socket to.

	Returns:
		On success it returns an handle to the socket. On failure it
		returns NETSOCKET_INVALID.
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
 * @return On success it returns the number of bytes sent. Returns -1
 * on error.
 */
int net_udp_send(NETSOCKET sock, const NETADDR *addr, const void *data, int size);

/*
	Function: net_udp_recv
		Receives a packet over an UDP socket.

	Parameters:
		sock - Socket to use.
		addr - Pointer to an NETADDR that will receive the address.
		data - Received data. Will be invalidated when this function is
		called again.

	Returns:
		On success it returns the number of bytes received. Returns -1
		on error.
*/
int net_udp_recv(NETSOCKET sock, NETADDR *addr, unsigned char **data);

/**
 * Closes an UDP socket.
 *
 * @ingroup Network-UDP
 *
 * @param sock Socket to close.
 *
 * @return 0 on success. -1 on error.
 */
int net_udp_close(NETSOCKET sock);

/**
 * @defgroup Network-TCP
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
 * @return 0 on success.
 */
int net_tcp_listen(NETSOCKET sock, int backlog);

/**
 * Polls a listning socket for a new connection.
 *
 * @ingroup Network-TCP
 *
 * @param sock - Listning socket to poll.
 * @param new_sock - Pointer to a socket to fill in with the new socket.
 * @param addr - Pointer to an address that will be filled in the remote address (optional, can be NULL).
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
 * @return 0 on success.
 *
 */
int net_tcp_connect(NETSOCKET sock, const NETADDR *addr);

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
 * @param data Pointer to a buffer to write the data to
 * @param max_size Maximum of data to write to the buffer.
 *
 * @return Number of bytes recvived. Negative value on failure. When in
 * non-blocking mode, it returns 0 when there is no more data to
 * be fetched.
 */
int net_tcp_recv(NETSOCKET sock, void *data, int maxsize);

/**
 * Closes a TCP socket.
 *
 * @ingroup Network-TCP
 *
 * @param sock Socket to close.
 *
 * @return 0 on success. Negative value on failure.
 */
int net_tcp_close(NETSOCKET sock);

#if defined(CONF_FAMILY_UNIX)
/**
 * @defgroup Network-Unix-Sockets
 * @ingroup Network-General
 */

/**
 * Creates an unnamed unix datagram socket.
 *
 * @ingroup Network-Unix-Sockets
 *
 * @return On success it returns a handle to the socket. On failure it returns -1.
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
 * @param addr Pointer to the addressstruct to fill.
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
 * @defgroup Strings
 *
 * String related functions.
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
 * @remark The strings are treated as zero-terminated strings.
 * @remark Guarantees that dst string will contain zero-termination.
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
 * @remark The strings are treated as zero-terminated strings.
 * @remark Guarantees that dst string will contain zero-termination.
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
 * @remark The strings are treated as zero-terminated strings.
 * @remark Guarantees that dst string will contain zero-termination.
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
 * @remark The strings are treated as zero-terminated strings.
 * @remark Guarantees that dst string will contain zero-termination.
 */
template<int N>
void str_copy(char (&dst)[N], const char *src)
{
	str_copy(dst, src, N);
}

/**
 * Truncates a utf8 encoded string to a given length.
 *
 * @ingroup Strings
 *
 * @param dst Pointer to a buffer that shall receive the string.
 * @param dst_size Size of the buffer dst.
 * @param str String to be truncated.
 * @param truncation_len Maximum codepoints in the returned string.
 *
 * @remark The strings are treated as utf8-encoded zero-terminated strings.
 * @remark Guarantees that dst string will contain zero-termination.
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
 * counting the zero termination).
 *
 * @remark The strings are treated as zero-terminated strings.
 * @remark Guarantees that dst string will contain zero-termination.
 */
void str_truncate(char *dst, int dst_size, const char *src, int truncation_len);

/**
 * Returns the length of a zero terminated string.
 *
 * @ingroup Strings
 *
 * @param str Pointer to the string.
 *
 * @return Length of string in bytes excluding the zero termination.
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
 * @remark The strings are treated as zero-terminated strings.
 * @remark Guarantees that buffer string will contain zero-termination.
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
 * @remark The strings are treated as zero-terminated strings.
 * @remark Guarantees that buffer string will contain zero-termination.
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
 * @remark The strings are treated as zero-terminated strings.
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
 * @remark The strings are treated as zero-terminated strings.
 */
bool str_has_cc(const char *str);

/**
 * Replaces all characters below 32 with whitespace.
 *
 * @ingroup Strings
 *
 * @param str String to sanitize.
 *
 * @remark The strings are treated as zero-terminated strings.
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
 * @remark The strings are treated as zero-terminated strings.
 */
void str_sanitize(char *str);

/**
 * Replaces all invalid filename characters with whitespace.
 *
 * @param str String to sanitize.
 *
 * @remark The strings are treated as zero-terminated strings.
 */
void str_sanitize_filename(char *str);

/**
 * Removes leading and trailing spaces and limits the use of multiple spaces.
 *
 * @ingroup Strings
 *
 * @param str String to clean up
 *
 * @remark The strings are treated as zero-terminated strings.
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
 * @remark The strings are treated as zero-terminated strings.
 * @remark Whitespace is defined according to str_isspace.
 */
char *str_skip_to_whitespace(char *str);

/**
 * @ingroup Strings
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
 * Pointer to the first non-whitespace character found
 * within the string.
 *
 * @remark The strings are treated as zero-terminated strings.
 * @remark Whitespace is defined according to str_isspace.
 */
char *str_skip_whitespaces(char *str);

/**
 * @ingroup Strings
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
 * @return `< 0` - String a is less than string b
 * @return `0` - String a is equal to string b
 * @return `> 0` - String a is greater than string b
 *
 * @remark Only guaranteed to work with a-z/A-Z.
 * @remark The strings are treated as zero-terminated strings.
 */
int str_comp_nocase(const char *a, const char *b);

/**
 * Compares up to num characters of two strings case insensitively.
 *
 * @ingroup Strings
 *
 * @param a String to compare.
 * @param b String to compare.
 * @param num Maximum characters to compare
 *
 * @return `< 0` - String a is less than string b
 * @return `0` - String a is equal to string b
 * @return `> 0` - String a is greater than string b
 *
 * @remark Only guaranteed to work with a-z/A-Z.
 * (use str_utf8_comp_nocase_num for unicode support)
 * @remark The strings are treated as zero-terminated strings.
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
 * @return `< 0` - String a is less than string b
 * @return `0` - String a is equal to string b
 * @return `> 0` - String a is greater than string b
 *
 * @remark The strings are treated as zero-terminated strings.
 */
int str_comp(const char *a, const char *b);

/**
 * Compares up to num characters of two strings case sensitive.
 *
 * @ingroup Strings
 *
 * @param a String to compare.
 * @param b String to compare.
 * @param num Maximum characters to compare
 *
 * @return `< 0` - String a is less than string b
 * @return `0` - String a is equal to string b
 * @return `> 0` - String a is greater than string b
 *
 * @remark The strings are treated as zero-terminated strings.
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
 * @remark The strings are treated as zero-terminated strings.
 */
int str_comp_filenames(const char *a, const char *b);

/*
       Function: str_startswith_nocase
               Checks case insensitive whether the string begins with a certain prefix.

       Parameter:
               str - String to check.
               prefix - Prefix to look for.

       Returns:
               A pointer to the string str after the string prefix, or 0 if
               the string prefix isn't a prefix of the string str.

       Remarks:
               - The strings are treated as zero-terminated strings.
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
 * @return A pointer to the string str after the string prefix, or 0 if
 *		   the string prefix isn't a prefix of the string str.
 *
 * @remark The strings are treated as zero-terminated strings.
 */
const char *str_startswith(const char *str, const char *prefix);

/*
       Function: str_endswith_nocase
               Checks case insensitive whether the string ends with a certain suffix.

       Parameter:
               str - String to check.
               suffix - Suffix to look for.

       Returns:
               A pointer to the beginning of the suffix in the string str, or
               0 if the string suffix isn't a suffix of the string str.

       Remarks:
               - The strings are treated as zero-terminated strings.
*/
const char *str_endswith_nocase(const char *str, const char *suffix);

/*
	Function: str_endswith
		Checks case sensitive whether the string ends with a certain suffix.

	Parameter:
		str - String to check.
		suffix - Suffix to look for.

	Returns:
		A pointer to the beginning of the suffix in the string str, or
		0 if the string suffix isn't a suffix of the string str.

	Remarks:
		- The strings are treated as zero-terminated strings.
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
 * @remark The strings are treated as zero-terminated strings.
 */
int str_utf8_dist(const char *a, const char *b);

/**
 * Computes the edit distance between two strings, allows buffers
 * to be passed in.
 *
 * @ingroup Strings
 *
 * @param a - First string for the edit distance.
 * @param b - Second string for the edit distance.
 * @param buf - Buffer for the function.
 * @param buf_len Length of the buffer, must be at least as long as
 *				  twice the length of both strings combined plus two.
 *
 * @return The edit distance between the both strings.
 *
 * @remark The strings are treated as zero-terminated strings.
 */
int str_utf8_dist_buffer(const char *a, const char *b, int *buf, int buf_len);

/*
	Function: str_utf32_dist_buffer
		Computes the edit distance between two strings, allows buffers
		to be passed in.

	Parameters:
		a - First string for the edit distance.
		a_len - Length of the first string.
		b - Second string for the edit distance.
		b_len - Length of the second string.
		buf - Buffer for the function.
		buf_len - Length of the buffer, must be at least as long as
		          the length of both strings combined plus two.

	Returns:
		The edit distance between the both strings.

	Remarks:
		- The strings are treated as zero-terminated strings.
*/
int str_utf32_dist_buffer(const int *a, int a_len, const int *b, int b_len, int *buf, int buf_len);

/*
	Function: str_find_nocase
		Finds a string inside another string case insensitively.

	Parameters:
		haystack - String to search in
		needle - String to search for

	Returns:
		A pointer into haystack where the needle was found.
		Returns NULL if needle could not be found.

	Remarks:
		- Only guaranteed to work with a-z/A-Z.
		  (use str_utf8_find_nocase for unicode support)
		- The strings are treated as zero-terminated strings.
*/
const char *str_find_nocase(const char *haystack, const char *needle);

/*
	Function: str_find
		Finds a string inside another string case sensitive.

	Parameters:
		haystack - String to search in
		needle - String to search for

	Returns:
		A pointer into haystack where the needle was found.
		Returns NULL if needle could not be found.

	Remarks:
		- The strings are treated as zero-terminated strings.
*/
const char *str_find(const char *haystack, const char *needle);

/**
 * @ingroup Strings
 *
 * @param haystack String to search in
 * @param delim String to search for
 * @param offset Number of characters into the haystack
 * @param start Will be set to the first delimiter on the left side of the offset (or haystack start)
 * @param end Will be set to the first delimiter on the right side of the offset (or haystack end)
 *
 * @return `true` if both delimiters were found
 * @return 'false' if a delimiter is missing (it uses haystack start and end as fallback)
 *
 * @remark The strings are treated as zero-terminated strings.
 */
bool str_delimiters_around_offset(const char *haystay, const char *delim, int offset, int *start, int *end);

/**
 * Finds the last occurrence of a character
 *
 * @ingroup Strings
 *
 * @param haystack String to search in
 * @param needle Character to search for

 * @return A pointer into haystack where the needle was found.
 * Returns NULL if needle could not be found.
 *
 * @remark The strings are treated as zero-terminated strings.
 * @remark The zero-terminator character can also be found with this function.
 */
const char *str_rchr(const char *haystack, char needle);

/**
 * Counts the number of occurrences of a character in a string.
 *
 * @ingroup Strings
 *
 * @param haystack String to count in
 * @param needle Character to count

 * @return The number of characters in the haystack string matching
 * the needle character.
 *
 * @remark The strings are treated as zero-terminated strings.
 * @remark The number of zero-terminator characters cannot be counted.
 */
int str_countchr(const char *haystack, char needle);

/**
 * Takes a datablock and generates a hex string of it, with spaces between bytes.
 *
 * @param dst Buffer to fill with hex data.
 * @param dst_size Size of the buffer (at least 3 * data_size + 1 to contain all data).
 * @param data Data to turn into hex.
 * @param data_size Size of the data.
 *
 * @remark The destination buffer will be zero-terminated.
 */
void str_hex(char *dst, int dst_size, const void *data, int data_size);

/**
 * Takes a datablock and generates a hex string of it, in the C style array format,
 * i.e. with bytes formatted in 0x00-0xFF notation and commas with spaces between the bytes.
 * The output can be split over multiple lines by specifying the maximum number of bytes
 * that should be printed per line.
 *
 * @param dst Buffer to fill with hex data.
 * @param dst_size Size of the buffer (at least 6 * data_size + 1 to contain all data).
 * @param data Data to turn into hex.
 * @param data_size Size of the data.
 * @param bytes_per_line After this many printed bytes a newline will be printed.
 *
 * @remark The destination buffer will be zero-terminated.
 */
void str_hex_cstyle(char *dst, int dst_size, const void *data, int data_size, int bytes_per_line = 12);

/*
	Function: str_hex_decode
		Takes a hex string *without spaces between bytes* and returns a
		byte array.

	Parameters:
		dst - Buffer for the byte array
		dst_size - size of the buffer
		data - String to decode

	Returns:
		2 - String doesn't exactly fit the buffer
		1 - Invalid character in string
		0 - Success

	Remarks:
		- The contents of the buffer is only valid on success
*/
int str_hex_decode(void *dst, int dst_size, const char *src);

/*
	Function: str_base64
		Takes a datablock and generates the base64 encoding of it.

	Parameters:
		dst - Buffer to fill with base64 data
		dst_size - Size of the buffer
		data - Data to turn into base64
		data - Size of the data

	Remarks:
		- The destination buffer will be zero-terminated
*/
void str_base64(char *dst, int dst_size, const void *data, int data_size);

/*
	Function: str_base64_decode
		Takes a base64 string without any whitespace and correct
		padding and returns a byte array.

	Parameters:
		dst - Buffer for the byte array
		dst_size - Size of the buffer
		data - String to decode

	Returns:
		<0 - Error
		>= 0 - Success, length of the resulting byte buffer

	Remarks:
		- The contents of the buffer is only valid on success
*/
int str_base64_decode(void *dst, int dst_size, const char *data);

/*
	Function: str_timestamp
		Copies a time stamp in the format year-month-day_hour-minute-second to the string.

	Parameters:
		buffer - Pointer to a buffer that shall receive the time stamp string.
		buffer_size - Size of the buffer.

	Remarks:
		- Guarantees that buffer string will contain zero-termination.
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
 * @param string Pointer to the string to parse
 * @param format The time format to use (for example FORMAT_NOSPACE below)
 * @param timestamp Pointer to the timestamp result
 *
 * @return true on success, false if the string could not be parsed with the specified format
 *
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

/*
	Function: str_times
		Formats a time string.

	Parameters:
		centisecs - Time in centiseconds, minimum value clamped to 0
		format - Format of the time string, see enum above, for example TIME_DAYS
		buffer - Pointer to a buffer that shall receive the time stamp string.
		buffer_size - Size of the buffer.

	Returns:
		Number of bytes written, -1 on invalid format or buffer_size <= 0

	Remarks:
		- Guarantees that buffer string will contain zero-termination, assuming
		  buffer_size > 0.
*/
int str_time(int64_t centisecs, int format, char *buffer, int buffer_size);
int str_time_float(float secs, int format, char *buffer, int buffer_size);

/*
	Function: str_escape
		Escapes \ and " characters in a string.

	Parameters:
		dst - Destination array pointer, gets increased, will point to
		      the terminating null.
		src - Source array
		end - End of destination array
*/
void str_escape(char **dst, const char *src, const char *end);

/**
 * @defgroup Filesystem
 *
 * Utilities for accessing the file system.
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
 * @remark The strings are treated as zero-terminated strings.
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
 * @remark The strings are treated as zero-terminated strings.
 */
void fs_listdir_fileinfo(const char *dir, FS_LISTDIR_CALLBACK_FILEINFO cb, int type, void *user);

/**
 * Creates a directory.
 *
 * @ingroup Filesystem
 *
 * @param path Directory to create.
 *
 * @return 0 on success. Negative value on failure.
 *
 * @remark Does not create several directories if needed. "a/b/c" will
 * result in a failure if b or a does not exist.
 *
 * @remark The strings are treated as zero-terminated strings.
 */
int fs_makedir(const char *path);

/**
 * Removes a directory.
 *
 * @ingroup Filesystem
 *
 * @param path Directory to remove.
 *
 * @return 0 on success. Negative value on failure.
 *
 * @remark Cannot remove a non-empty directory.
 *
 * @remark The strings are treated as zero-terminated strings.
 */
int fs_removedir(const char *path);

/**
 * Recursively creates parent directories for a file or directory.
 *
 * @ingroup Filesystem
 *
 * @param path File or directory for which to create parent directories.
 *
 * @return 0 on success. Negative value on failure.
 *
 * @remark The strings are treated as zero-terminated strings.
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
 * @return 0 on success. Negative value on failure.
 *
 * @remark Returns ~/.appname on UNIX based systems.
 * @remark Returns ~/Library/Applications Support/appname on macOS.
 * @remark Returns %APPDATA%/Appname on Windows based systems.
 *
 * @remark The strings are treated as zero-terminated strings.
 */
int fs_storage_path(const char *appname, char *path, int max);

/**
 * Checks if a file exists.
 *
 * @ingroup Filesystem
 *
 * @param path the path to check.
 *
 * @return 1 if a file with the given path exists,
 * 0 on failure or if the file does not exist.
 *
 * @remark The strings are treated as zero-terminated strings.
 */
int fs_is_file(const char *path);

/**
 * Checks if a folder exists.
 *
 * @ingroup Filesystem
 *
 * @param path the path to check.
 *
 * @return 1 if a folder with the given path exists,
 * 0 on failure or if the folder does not exist.
 *
 * @remark The strings are treated as zero-terminated strings.
 */
int fs_is_dir(const char *path);

/**
 * Checks whether a given path is relative or absolute.
 *
 * @ingroup Filesystem
 *
 * @param path Path to check.
 *
 * @return 1 if relative, 0 if absolute.
 *
 * @remark The strings are treated as zero-terminated strings.
 */
int fs_is_relative_path(const char *path);

/**
 * Changes the current working directory.
 *
 * @ingroup Filesystem
 *
 * @param path New working directory path.
 *
 * @return 0 on success, 1 on failure.
 *
 * @remark The strings are treated as zero-terminated strings.
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
 * @return Pointer to the buffer on success, nullptr on failure.
 *
 * @remark The strings are treated as zero-terminated strings.
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
 * @remark The strings are treated as zero-terminated strings.
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
 * @remark The strings are treated as zero-terminated strings.
 */
void fs_split_file_extension(const char *filename, char *name, size_t name_size, char *extension = nullptr, size_t extension_size = 0);

/**
 * Get the parent directory of a directory.
 *
 * @ingroup Filesystem
 *
 * @param path Path of the directory. The parent will be store in this buffer as well.
 *
 * @return 0 on success, 1 on failure.
 *
 * @remark The strings are treated as zero-terminated strings.
 */
int fs_parent_dir(char *path);

/**
 * Deletes a file.
 *
 * @ingroup Filesystem
 *
 * @param filename Path of the file to delete.
 *
 * @return 0 on success, 1 on failure.
 *
 * @remark The strings are treated as zero-terminated strings.
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
 * @return 0 on success, 1 on failure.
 *
 * @remark The strings are treated as zero-terminated strings.
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
 * @return 0 on success, non-zero on failure.
 *
 * @remark The strings are treated as zero-terminated strings.
 * @remark Returned time is in seconds since UNIX Epoch.
 */
int fs_file_time(const char *name, time_t *created, time_t *modified);

/*
	Group: Undocumented
*/

/*
	Function: net_tcp_connect_non_blocking

	DOCTODO: serp
*/
int net_tcp_connect_non_blocking(NETSOCKET sock, NETADDR bindaddr);

/*
	Function: net_set_non_blocking

	DOCTODO: serp
*/
int net_set_non_blocking(NETSOCKET sock);

/*
	Function: net_set_non_blocking

	DOCTODO: serp
*/
int net_set_blocking(NETSOCKET sock);

/*
	Function: net_errno

	DOCTODO: serp
*/
int net_errno();

/*
	Function: net_would_block

	DOCTODO: serp
*/
int net_would_block();

int net_socket_read_wait(NETSOCKET sock, int time);

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

typedef struct
{
	uint64_t sent_packets;
	uint64_t sent_bytes;
	uint64_t recv_packets;
	uint64_t recv_bytes;
} NETSTATS;

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
 * @param c the character to check
 *
 * @return 1 if the character is whitespace, 0 otherwise.
 *
 * @remark The following characters are considered whitespace: ' ', '\n', '\r', '\t'
 */
int str_isspace(char c);

char str_uppercase(char c);

bool str_isnum(char c);

int str_isallnum(const char *str);

int str_isallnum_hex(const char *str);

unsigned str_quickhash(const char *str);

int str_utf8_to_skeleton(const char *str, int *buf, int buf_len);

/*
	Function: str_utf8_comp_confusable
		Compares two strings for visual appearance.

	Parameters:
		str1 - String to compare.
		str2 - String to compare.

	Returns:
		0 if the strings are confusable.
		!=0 otherwise.
*/
int str_utf8_comp_confusable(const char *str1, const char *str2);

/*
	Function: str_utf8_tolower
		Converts the given Unicode codepoint to lowercase (locale insensitive).

	Parameters:
		code - Unicode codepoint to convert.

	Returns:
		Lowercase codepoint
*/
int str_utf8_tolower(int code);

/*
	Function: str_utf8_comp_nocase
		Compares two utf8 strings case insensitively.

	Parameters:
		a - String to compare.
		b - String to compare.

	Returns:
		<0 - String a is less than string b
		0 - String a is equal to string b
		>0 - String a is greater than string b
*/
int str_utf8_comp_nocase(const char *a, const char *b);

/*
	Function: str_utf8_comp_nocase_num
		Compares up to num bytes of two utf8 strings case insensitively.

	Parameters:
		a - String to compare.
		b - String to compare.
		num - Maximum bytes to compare

	Returns:
		<0 - String a is less than string b
		0 - String a is equal to string b
		>0 - String a is greater than string b
*/
int str_utf8_comp_nocase_num(const char *a, const char *b, int num);

/*
	Function: str_utf8_find_nocase
		Finds a utf8 string inside another utf8 string case insensitively.

	Parameters:
		haystack - String to search in
		needle - String to search for
        end - A pointer that will be set to a pointer into haystack directly behind the
            last character where the needle was found. Will be set to nullptr if needle
            could not be found. Optional parameter.

	Returns:
		A pointer into haystack where the needle was found.
		Returns NULL if needle could not be found.

	Remarks:
		- The strings are treated as zero-terminated strings.
*/
const char *str_utf8_find_nocase(const char *haystack, const char *needle, const char **end = nullptr);

/*
	Function: str_utf8_isspace
		Checks whether the given Unicode codepoint renders as space.

	Parameters:
		code - Unicode codepoint to check.

	Returns:
		0 if the codepoint does not render as space, != 0 if it does.
*/
int str_utf8_isspace(int code);

int str_utf8_isstart(char c);

/*
	Function: str_utf8_skip_whitespaces
		Skips leading characters that render as spaces.

	Parameters:
		str - Pointer to the string.

	Returns:
		Pointer to the first non-whitespace character found
		within the string.

	Remarks:
		- The strings are treated as zero-terminated strings.
*/
const char *str_utf8_skip_whitespaces(const char *str);

/*
	Function: str_utf8_trim_right
		Removes trailing characters that render as spaces by modifying
		the string in-place.

	Parameters:
		param - Pointer to the string.

	Remarks:
		- The strings are treated as zero-terminated strings.
		- The string is modified in-place.
*/
void str_utf8_trim_right(char *param);

/*
	Function: str_utf8_rewind
		Moves a cursor backwards in an utf8 string

	Parameters:
		str - utf8 string
		cursor - position in the string

	Returns:
		New cursor position.

	Remarks:
		- Won't move the cursor less then 0
*/
int str_utf8_rewind(const char *str, int cursor);

/*
	Function: str_utf8_fix_truncation
		Fixes truncation of a Unicode character at the end of a UTF-8
		string.

	Returns:
		The new string length.

	Parameters:
		str - utf8 string
*/
int str_utf8_fix_truncation(char *str);

/*
	Function: str_utf8_forward
		Moves a cursor forwards in an utf8 string

	Parameters:
		str - utf8 string
		cursor - position in the string

	Returns:
		New cursor position.

	Remarks:
		- Won't move the cursor beyond the zero termination marker
*/
int str_utf8_forward(const char *str, int cursor);

/*
	Function: str_utf8_decode
		Decodes a utf8 codepoint

	Parameters:
		ptr - Pointer to a utf8 string. This pointer will be moved forward.

	Returns:
		The Unicode codepoint. -1 for invalid input and 0 for end of string.

	Remarks:
		- This function will also move the pointer forward.
		- You may call this function again after an error occurred.
*/
int str_utf8_decode(const char **ptr);

/*
	Function: str_utf8_encode
		Encode an utf8 character

	Parameters:
		ptr - Pointer to a buffer that should receive the data. Should be able to hold at least 4 bytes.

	Returns:
		Number of bytes put into the buffer.

	Remarks:
		- Does not do zero termination of the string.
*/
int str_utf8_encode(char *ptr, int chr);

/*
	Function: str_utf8_check
		Checks if a strings contains just valid utf8 characters.

	Parameters:
		str - Pointer to a possible utf8 string.

	Returns:
		0 - invalid characters found.
		1 - only valid characters found.

	Remarks:
		- The string is treated as zero-terminated utf8 string.
*/
int str_utf8_check(const char *str);

/*
	Function: str_utf8_copy_num
		Copies a number of utf8 characters from one string to another.

	Parameters:
		dst - Pointer to a buffer that shall receive the string.
		src - String to be copied.
		dst_size - Size of the buffer dst.
		num - maximum number of utf8 characters to be copied.

	Remarks:
		- The strings are treated as zero-terminated strings.
		- Garantees that dst string will contain zero-termination.
*/
void str_utf8_copy_num(char *dst, const char *src, int dst_size, int num);

/*
	Function: str_utf8_stats
		Determines the byte size and utf8 character count of a utf8 string.

	Parameters:
		str - Pointer to the string.
		max_size - Maximum number of bytes to count.
		max_count - Maximum number of utf8 characters to count.
		size - Pointer to store size (number of non-zero bytes) of the string.
		count - Pointer to store count of utf8 characters of the string.

	Remarks:
		- The string is treated as zero-terminated utf8 string.
		- It's the user's responsibility to make sure the bounds are aligned.
*/
void str_utf8_stats(const char *str, size_t max_size, size_t max_count, size_t *size, size_t *count);

/**
 * Converts a byte offset of a utf8 string to the utf8 character offset.
 *
 * @param text Pointer to the string.
 * @param byte_offset Offset in bytes.
 *
 * @return Offset in utf8 characters. Clamped to the maximum length of the string in utf8 characters.
 *
 * @remark The string is treated as a zero-terminated utf8 string.
 * @remark It's the user's responsibility to make sure the bounds are aligned.
 */
size_t str_utf8_offset_bytes_to_chars(const char *str, size_t byte_offset);

/**
 * Converts a utf8 character offset of a utf8 string to the byte offset.
 *
 * @param text Pointer to the string.
 * @param char_offset Offset in utf8 characters.
 *
 * @return Offset in bytes. Clamped to the maximum length of the string in bytes.
 *
 * @remark The string is treated as a zero-terminated utf8 string.
 * @remark It's the user's responsibility to make sure the bounds are aligned.
 */
size_t str_utf8_offset_chars_to_bytes(const char *str, size_t char_offset);

/*
	Function: str_next_token
		Writes the next token after str into buf, returns the rest of the string.

	Parameters:
		str - Pointer to string.
		delim - Delimiter for tokenization.
		buffer - Buffer to store token in.
		buffer_size - Size of the buffer.

	Returns:
		Pointer to rest of the string.

	Remarks:
		- The token is always null-terminated.
*/
const char *str_next_token(const char *str, const char *delim, char *buffer, int buffer_size);

/*
	Function: str_in_list
		Checks if needle is in list delimited by delim

	Parameters:
		list - List
		delim - List delimiter.
		needle - Item that is being looked for.

	Returns:
		1 - Item is in list.
		0 - Item isn't in list.
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
 * @defgroup Shell
 * Shell, process management, OS specific functionality.
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

/**
 * Executes a given file.
 *
 * @ingroup Shell
 *
 * @param file The file to execute.
 * @param window_state The window state how the process window should be shown.
 *
 * @return Handle of the new process, or @link INVALID_PROCESS @endlink on error.
 */
PROCESS shell_execute(const char *file, EShellExecuteWindowState window_state);

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
 * `false` if the process is not running (dead).
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
 * @remark The strings are treated as zero-terminated strings.
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
 * @remark The strings are treated as zero-terminated strings.
 * @remark This may not be called with untrusted input or it'll result in arbitrary code execution, especially on Windows.
 */
int open_file(const char *path);
#endif // !defined(CONF_PLATFORM_ANDROID)

/**
 * @defgroup Secure-Random
 * Secure random number generation.
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
 * even and smaller or equal to 128.
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
 * @remark The destination buffer will be zero-terminated.
 */
void os_locale_str(char *locale, size_t length);

#if defined(CONF_EXCEPTION_HANDLING)
/**
 * @defgroup Exception-Handling
 * Exception handling (crash logging).
 */

/**
 * Initializes the exception handling module.
 *
 * @ingroup Exception-Handling
 */
void init_exception_handler();

/**
 * Sets the filename for writing the crash log.
 *
 * @ingroup Exception-Handling
 *
 * @param log_file_path Absolute path to which crash log file should be written.
 */
void set_exception_handler_log_file(const char *log_file_path);
#endif

/**
 * Fetches a sample from a high resolution timer and converts it in nanoseconds.
 *
 * @ingroup Time
 *
 * @return Current value of the timer in nanoseconds.
 */
std::chrono::nanoseconds time_get_nanoseconds();

int net_socket_read_wait(NETSOCKET sock, std::chrono::nanoseconds nanoseconds);

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
 * @remark The argument string must be zero-terminated.
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
 * @remark The argument string must be zero-terminated.
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

template<>
struct std::hash<NETADDR>
{
	size_t operator()(const NETADDR &Addr) const noexcept;
};

#endif
