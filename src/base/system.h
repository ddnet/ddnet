/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

/*
	Title: OS Abstraction
*/

#ifndef BASE_SYSTEM_H
#define BASE_SYSTEM_H

#include "detect.h"

#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <stdint.h>
#include <time.h>

#ifdef CONF_FAMILY_UNIX
#include <sys/un.h>
#endif

#ifdef CONF_PLATFORM_LINUX
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <chrono>
#include <functional>

extern "C" {

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
void dbg_assert_imp(const char *filename, int line, int test, const char *msg);

#ifdef __clang_analyzer__
#include <assert.h>
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
void mem_copy(void *dest, const void *source, unsigned size);

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
void mem_move(void *dest, const void *source, unsigned size);

/**
 * Sets a complete memory block to 0.
 *
 * @ingroup Memory
 *
 * @param block Pointer to the block to zero out.
 * @param size Size of the block.
 */
void mem_zero(void *block, unsigned size);

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
int mem_comp(const void *a, const void *b, int size);

/**
 * Checks whether a block of memory contains null bytes.
 *
 * @ingroup Memory
 *
 * @param block Pointer to the block to check for nulls.
 * @param size Size of the block.
 *
 * @return 1 - The block has a null byte.
 * @return 0 - The block does not have a null byte.
 */
int mem_has_null(const void *block, unsigned size);

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
	IOFLAG_READ = 1,
	IOFLAG_WRITE = 2,
	IOFLAG_APPEND = 4,
	IOFLAG_SKIP_BOM = 8,

	IOSEEK_START = 0,
	IOSEEK_CUR = 1,
	IOSEEK_END = 2,

	IO_MAX_PATH_LENGTH = 512,
};

typedef struct IOINTERNAL *IOHANDLE;

/**
 * Opens a file.
 *
 * @ingroup File-IO
 *
 * @param File to open.
 * @param flags A set of IOFLAG flags.
 *
 * @sa IOFLAG_READ, IOFLAG_WRITE, IOFLAG_APPEND, IOFLAG_SKIP_BOM.
 *
 * @return A handle to the file on success and 0 on failure.
 *
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
 *
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
 * @remark Does NOT guarantee that there are no internal null bytes.
 * @remark The result must be freed after it has been used.
 */
void io_read_all(IOHANDLE io, void **result, unsigned *result_len);

/**
 * Reads the rest of the file into a zero-terminated buffer with
 * no internal null bytes.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file to read data from.
 *
 * @return The file's remaining contents or null on failure.
 *
 * @remark Guarantees that there are no internal null bytes.
 * @remark Guarantees that result will contain zero-termination.
 * @remark The result must be freed after it has been used.
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
 * @return Number of bytes skipped.
 */
unsigned io_skip(IOHANDLE io, int size);

/**
 * Writes data from a buffer to file.
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
 * Writes newline to file.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 *
 * @return Number of bytes written.
 */
unsigned io_write_newline(IOHANDLE io);

/**
 * Seeks to a specified offset in the file.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 * @param offset Offset from pos to stop.
 * @param origin Position to start searching from.
 *
 * @return 0 on success.
 */
int io_seek(IOHANDLE io, int offset, int origin);

/**
 * Gets the current position in the file.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 *
 * @return The current position. @c -1L if an error occurred.
 */
long int io_tell(IOHANDLE io);

/**
 * Gets the total length of the file. Resetting cursor to the beginning
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 *
 * @return The total size. @c -1L if an error occurred.
 */
long int io_length(IOHANDLE io);

/**
 * Closes a file.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 *
 * @return 0 on success.
 */
int io_close(IOHANDLE io);

/**
 * Empties all buffers and writes all pending data.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 *
 * @return 0 on success.
 */
int io_flush(IOHANDLE io);

/**
 * Synchronize file changes to disk.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 *
 * @return 0 on success.
 */
int io_sync(IOHANDLE io);

/**
 * Checks whether an error occurred during I/O with the file.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 *
 * @return nonzero on error, 0 otherwise.
 */
int io_error(IOHANDLE io);

/**
 * @ingroup File-IO
 * @return An <IOHANDLE> to the standard input.
 */
IOHANDLE io_stdin();

/**
 * @ingroup File-IO
 * @return An <IOHANDLE> to the standard output.
 */
IOHANDLE io_stdout();

/**
 * @ingroup File-IO
 * @return An <IOHANDLE> to the standard error.
 */
IOHANDLE io_stderr();

/**
 * @ingroup File-IO
 * @return An <IOHANDLE> to the current executable.
 */
IOHANDLE io_current_exe();

typedef struct ASYNCIO ASYNCIO;

/**
 * Wraps a @link IOHANDLE @endlink for asynchronous writing.
 *
 * @ingroup File-IO
 *
 * @param io Handle to the file.
 *
 * @return The handle for asynchronous writing.
 *
 */
ASYNCIO *aio_new(IOHANDLE io);

/**
 * Locks the ASYNCIO structure so it can't be written into by
 * other threads.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 */
void aio_lock(ASYNCIO *aio);

/**
 * Unlocks the ASYNCIO structure after finishing the contiguous
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
 *
 */
void aio_write_newline(ASYNCIO *aio);

/**
 * Queues a chunk of data for writing. The ASYNCIO struct must be
 * locked using @link aio_lock @endlink first.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 * @param buffer Pointer to the data that should be written.
 * @param size Number of bytes to write.
 *
 */
void aio_write_unlocked(ASYNCIO *aio, const void *buffer, unsigned size);

/**
 * Queues a newline for writing. The ASYNCIO struct must be locked
 * using @link aio_lock @endlink first.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 *
 */
void aio_write_newline_unlocked(ASYNCIO *aio);

/**
 * Checks whether errors have occurred during the asynchronous
 * writing.
 *
 * Call this function regularly to see if there are errors. Call
 * this function after <aio_wait> to see if the process of writing
 * to the file succeeded.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 *
 * @eturn 0 if no error occurred, and nonzero on error.
 *
 */
int aio_error(ASYNCIO *aio);

/**
 * Queues file closing.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 *
 */
void aio_close(ASYNCIO *aio);

/**
 * Wait for the asynchronous operations to complete.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 *
 */
void aio_wait(ASYNCIO *aio);

/**
 * Frees the resources associated to the asynchronous file handle.
 *
 * @ingroup File-IO
 *
 * @param aio Handle to the file.
 *
 */
void aio_free(ASYNCIO *aio);

/**
 * @defgroup Threads
 * Threading related functions.
 *
 * @see Locks
 */

/**
 * Creates a new thread.
 *
 * @ingroup Threads
 *
 * @param threadfunc Entry point for the new thread.
 * @param user Pointer to pass to the thread.
 * @param name name describing the use of the thread
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
 * Yield the current threads execution slice.
 *
 * @ingroup Threads
 */
void thread_yield();

/**
 * Puts the thread in the detached thread, guaranteeing that
 * resources of the thread will be freed immediately when the
 * thread terminates.
 *
 * @ingroup Threads
 *
 * @param thread Thread to detach
 */
void thread_detach(void *thread);

/**
 * Creates a new thread and if it succeeded detaches it.
 *
 * @ingroup Threads
 *
 * @param threadfunc Entry point for the new thread.
 * @param user Pointer to pass to the thread.
 * @param name Name describing the use of the thread
 *
 * @return The thread if no error occured, 0 on error.
 */
void *thread_init_and_detach(void (*threadfunc)(void *), void *user, const char *name);

// Enable thread safety attributes only with clang.
// The attributes can be safely erased when compiling with other compilers.
#if defined(__clang__) && (!defined(SWIG))
#define THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define THREAD_ANNOTATION_ATTRIBUTE__(x) // no-op
#endif

#define CAPABILITY(x) \
	THREAD_ANNOTATION_ATTRIBUTE__(capability(x))

#define SCOPED_CAPABILITY \
	THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

#define GUARDED_BY(x) \
	THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

#define PT_GUARDED_BY(x) \
	THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

#define ACQUIRED_BEFORE(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))

#define ACQUIRED_AFTER(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))

#define REQUIRES(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))

#define REQUIRES_SHARED(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))

#define ACQUIRE(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))

#define ACQUIRE_SHARED(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))

#define RELEASE(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))

#define RELEASE_SHARED(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))

#define RELEASE_GENERIC(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(release_generic_capability(__VA_ARGS__))

#define TRY_ACQUIRE(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(__VA_ARGS__))

#define TRY_ACQUIRE_SHARED(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(__VA_ARGS__))

#define EXCLUDES(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

#define ASSERT_CAPABILITY(x) \
	THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))

#define ASSERT_SHARED_CAPABILITY(x) \
	THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))

#define RETURN_CAPABILITY(x) \
	THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

#define NO_THREAD_SAFETY_ANALYSIS \
	THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

/**
 * @defgroup Locks
 *
 * Synchronization primitives.
 *
 * @see Threads
 */

typedef CAPABILITY("mutex") void *LOCK;

/**
 * @ingroup Locks
 */
LOCK lock_create();
/**
 * @ingroup Locks
 */
void lock_destroy(LOCK lock);

/**
 * @ingroup Locks
 */
int lock_trylock(LOCK lock) TRY_ACQUIRE(1, lock);
/**
 * @ingroup Locks
 */
void lock_wait(LOCK lock) ACQUIRE(lock);
/**
 * @ingroup Locks
 */
void lock_unlock(LOCK lock) RELEASE(lock);

/* Group: Semaphores */
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
 * @ingroup Locks
 */
void sphore_init(SEMAPHORE *sem);
/**
 * @ingroup Locks
 */
void sphore_wait(SEMAPHORE *sem);
/**
 * @ingroup Locks
 */
void sphore_signal(SEMAPHORE *sem);
/**
 * @ingroup Locks
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
int time_timestamp();

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
enum
{
	SEASON_SPRING = 0,
	SEASON_SUMMER,
	SEASON_AUTUMN,
	SEASON_WINTER,
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
int time_season();

/**
 * @defgroup Network-General
 */

/**
 * @ingroup Network-General
 */
typedef struct NETSOCKET_INTERNAL *NETSOCKET;

/**
 * @ingroup Network-General
 */
enum
{
	NETADDR_MAXSTRSIZE = 1 + (8 * 4 + 7) + 1 + 1 + 5 + 1, // [XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX]:XXXXX

	NETTYPE_LINK_BROADCAST = 4,

	NETTYPE_INVALID = 0,
	NETTYPE_IPV4 = 1,
	NETTYPE_IPV6 = 2,
	NETTYPE_WEBSOCKET_IPV4 = 8,

	NETTYPE_ALL = NETTYPE_IPV4 | NETTYPE_IPV6 | NETTYPE_WEBSOCKET_IPV4,
	NETTYPE_MASK = NETTYPE_ALL | NETTYPE_LINK_BROADCAST,
};

/**
 * @ingroup Network-General
 */
typedef struct NETADDR
{
	unsigned int type;
	unsigned char ip[16];
	unsigned short port;

	bool operator==(const NETADDR &other) const;
	bool operator!=(const NETADDR &other) const { return !(*this == other); }
} NETADDR;

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
 * @return 0 on success.
 *
 * @remark You must call this function before using any other network functions.
 */
int net_init();

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
 * @param add_port add port to string or not
 *
 * @remark The string will always be zero terminated
 */
void net_addr_str(const NETADDR *addr, char *string, int max_length, int add_port);

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
 * Copies a string to another.
 *
 * @ingroup Strings
 *
 * @param dst Pointer to a buffer that shall receive the string.
 * @param src String to be copied.
 * @param dst_size Size of the buffer dst.
 *
 * @remark The strings are treated as zero-terminated strings.
 * @remark Guarantees that dst string will contain zero-termination.
 */
void str_copy(char *dst, const char *src, int dst_size);

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
 * @remark Garantees that dst string will contain zero-termination.
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
 * @param ... Parameters for the formatting.
 *
 * @return Length of written string, even if it has been truncated
 *
 * @remark See the C manual for syntax for the printf formatting string.
 * @remark The strings are treated as zero-terminated strings.
 * @remark Guarantees that dst string will contain zero-termination.
 */
int str_format(char *buffer, int buffer_size, const char *format, ...)
	GNUC_ATTRIBUTE((format(printf, 3, 4)));

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
 * Finds the last occurance of a character
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

/*
	Function: str_hex
		Takes a datablock and generates a hex string of it, with spaces
		between bytes.

	Parameters:
		dst - Buffer to fill with hex data
		dst_size - size of the buffer
		data - Data to turn into hex
		data - Size of the data

	Remarks:
		- The destination buffer will be zero-terminated
*/
void str_hex(char *dst, int dst_size, const void *data, int data_size);

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

/* Group: Filesystem */

/*
	Function: fs_listdir
		Lists the files in a directory

	Parameters:
		dir - Directory to list
		cb - Callback function to call for each entry
		type - Type of the directory
		user - Pointer to give to the callback
*/
typedef int (*FS_LISTDIR_CALLBACK)(const char *name, int is_dir, int dir_type, void *user);
void fs_listdir(const char *dir, FS_LISTDIR_CALLBACK cb, int type, void *user);

typedef struct
{
	const char *m_pName;
	time_t m_TimeCreated; // seconds since UNIX Epoch
	time_t m_TimeModified; // seconds since UNIX Epoch
} CFsFileInfo;

/*
	Function: fs_listdir_fileinfo
		Lists the files in a directory and gets additional file information

	Parameters:
		dir - Directory to list
		cb - Callback function to call for each entry
		type - Type of the directory
		user - Pointer to give to the callback
*/
typedef int (*FS_LISTDIR_CALLBACK_FILEINFO)(const CFsFileInfo *info, int is_dir, int dir_type, void *user);
void fs_listdir_fileinfo(const char *dir, FS_LISTDIR_CALLBACK_FILEINFO cb, int type, void *user);

/*
	Function: fs_makedir
		Creates a directory

	Parameters:
		path - Directory to create

	Returns:
		Returns 0 on success. Negative value on failure.

	Remarks:
		Does not create several directories if needed. "a/b/c" will result
		in a failure if b or a does not exist.
*/
int fs_makedir(const char *path);

/*
	Function: fs_removedir
		Removes a directory

	Parameters:
		path - Directory to remove

	Returns:
		Returns 0 on success. Negative value on failure.

	Remarks:
		Cannot remove a non-empty directory.
*/
int fs_removedir(const char *path);

/*
	Function: fs_makedir_rec_for
		Recursively create directories for a file

	Parameters:
		path - File for which to create directories

	Returns:
		Returns 0 on success. Negative value on failure.
*/
int fs_makedir_rec_for(const char *path);

/*
	Function: fs_storage_path
		Fetches per user configuration directory.

	Returns:
		Returns 0 on success. Negative value on failure.

	Remarks:
		- Returns ~/.appname on UNIX based systems
		- Returns ~/Library/Applications Support/appname on macOS
		- Returns %APPDATA%/Appname on Windows based systems
*/
int fs_storage_path(const char *appname, char *path, int max);

/*
	Function: fs_is_dir
		Checks if directory exists

	Returns:
		Returns 1 on success, 0 on failure.
*/
int fs_is_dir(const char *path);

/*
    Function: fs_is_relative_path
        Checks whether a given path is relative or absolute.

    Returns:
        Returns 1 if relative, 0 if absolute.
*/
int fs_is_relative_path(const char *path);

/*
	Function: fs_chdir
		Changes current working directory

	Returns:
		Returns 0 on success, 1 on failure.
*/
int fs_chdir(const char *path);

/*
	Function: fs_getcwd
		Gets the current working directory.

	Returns:
		Returns a pointer to the buffer on success, 0 on failure.
*/
char *fs_getcwd(char *buffer, int buffer_size);

/*
	Function: fs_parent_dir
		Get the parent directory of a directory

	Parameters:
		path - The directory string

	Returns:
		Returns 0 on success, 1 on failure.

	Remarks:
		- The string is treated as zero-terminated string.
*/
int fs_parent_dir(char *path);

/*
	Function: fs_remove
		Deletes the file with the specified name.

	Parameters:
		filename - The file to delete

	Returns:
		Returns 0 on success, 1 on failure.

	Remarks:
		- The strings are treated as zero-terminated strings.
		- Returns an error if the path specifies a directory name.
*/
int fs_remove(const char *filename);

/*
	Function: fs_rename
		Renames the file or directory. If the paths differ the file will be moved.

	Parameters:
		oldname - The current name
		newname - The new name

	Returns:
		Returns 0 on success, 1 on failure.

	Remarks:
		- The strings are treated as zero-terminated strings.
*/
int fs_rename(const char *oldname, const char *newname);

/*
	Function: fs_file_time
		Gets the creation and the last modification date of a file.

	Parameters:
		name - The filename.
		created - Pointer to time_t
		modified - Pointer to time_t

	Returns:
		0 on success, non-zero on failure

	Remarks:
		- Returned time is in seconds since UNIX Epoch
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

/*
	Function: open_link
		Opens a link in the browser.

	Parameters:
		link - The link to open in a browser.

	Returns:
		Returns 1 on success, 0 on failure.

	Remarks:
		This may not be called with untrusted input or it'll result in arbitrary code execution, especially on Windows.
*/
int open_link(const char *link);

/*
	Function: open_file
		Opens a file or directory with default program.

	Parameters:
		path - The path to open.

	Returns:
		Returns 1 on success, 0 on failure.

	Remarks:
		This may not be called with untrusted input or it'll result in arbitrary code execution, especially on Windows.
*/
int open_file(const char *path);

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
int str_toint_base(const char *str, int base);
unsigned long str_toulong_base(const char *str, int base);
float str_tofloat(const char *str);

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
int str_isallnum(const char *str);
unsigned str_quickhash(const char *str);

struct SKELETON;
void str_utf8_skeleton_begin(struct SKELETON *skel, const char *str);
int str_utf8_skeleton_next(struct SKELETON *skel);
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

	Returns:
		A pointer into haystack where the needle was found.
		Returns NULL if needle could not be found.

	Remarks:
		- The strings are treated as zero-terminated strings.
*/
const char *str_utf8_find_nocase(const char *haystack, const char *needle);

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
void str_utf8_stats(const char *str, int max_size, int max_count, int *size, int *count);

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

/*
	Function: bytes_be_to_int
		Packs 4 big endian bytes into an int

	Returns:
		The packed int

	Remarks:
		- Assumes the passed array is 4 bytes
		- Assumes int is 4 bytes
*/
int bytes_be_to_int(const unsigned char *bytes);

/*
	Function: int_to_bytes_be
		Packs an int into 4 big endian bytes

	Remarks:
		- Assumes the passed array is 4 bytes
		- Assumes int is 4 bytes
*/
void int_to_bytes_be(unsigned char *bytes, int value);

/*
	Function: bytes_be_to_uint
		Packs 4 big endian bytes into an unsigned

	Returns:
		The packed unsigned

	Remarks:
		- Assumes the passed array is 4 bytes
		- Assumes unsigned is 4 bytes
*/
unsigned bytes_be_to_uint(const unsigned char *bytes);

/*
	Function: uint_to_bytes_be
		Packs an unsigned into 4 big endian bytes

	Remarks:
		- Assumes the passed array is 4 bytes
		- Assumes unsigned is 4 bytes
*/
void uint_to_bytes_be(unsigned char *bytes, unsigned value);

/*
	Function: pid
		Returns the pid of the current process.

	Returns:
		pid of the current process
*/
int pid();

/*
	Function: cmdline_fix
		Fixes the command line arguments to be encoded in UTF-8 on all
		systems.

	Parameters:
		argc - A pointer to the argc parameter that was passed to the main function.
		argv - A pointer to the argv parameter that was passed to the main function.

	Remarks:
		- You need to call cmdline_free once you're no longer using the
		results.
*/
void cmdline_fix(int *argc, const char ***argv);

/*
	Function: cmdline_free
		Frees memory that was allocated by cmdline_fix.

	Parameters:
		argc - The argc obtained from cmdline_fix.
		argv - The argv obtained from cmdline_fix.

*/
void cmdline_free(int argc, const char **argv);

#if defined(CONF_FAMILY_WINDOWS)
typedef void *PROCESS;
#else
typedef pid_t PROCESS;
#endif

/*
	Function: shell_execute
		Executes a given file.

	Returns:
		handle/pid of the new process
*/
PROCESS shell_execute(const char *file);

/*
	Function: kill_process
		Sends kill signal to a process.

	Parameters:
		process - handle/pid of the process

	Returns:
		0 - Error
		1 - Success
*/
int kill_process(PROCESS process);

/*
	Function: generate_password
		Generates a null-terminated password of length `2 *
		random_length`.

	Parameters:
		buffer - Pointer to the start of the output buffer.
		length - Length of the buffer.
		random - Pointer to a randomly-initialized array of shorts.
		random_length - Length of the short array.
*/
void generate_password(char *buffer, unsigned length, unsigned short *random, unsigned random_length);

/*
	Function: secure_random_init
		Initializes the secure random module.
		You *MUST* check the return value of this function.

	Returns:
		0 - Initialization succeeded.
		1 - Initialization failed.
*/
int secure_random_init();

/*
	Function: secure_random_uninit
		Uninitializes the secure random module.

	Returns:
		0 - Uninitialization succeeded.
		1 - Uninitialization failed.
*/
int secure_random_uninit();

/*
	Function: secure_random_password
		Fills the buffer with the specified amount of random password
		characters.

		The desired password length must be greater or equal to 6, even
		and smaller or equal to 128.

	Parameters:
		buffer - Pointer to the start of the buffer.
		length - Length of the buffer.
		pw_length - Length of the desired password.
*/
void secure_random_password(char *buffer, unsigned length, unsigned pw_length);

/*
	Function: secure_random_fill
		Fills the buffer with the specified amount of random bytes.

	Parameters:
		buffer - Pointer to the start of the buffer.
		length - Length of the buffer.
*/
void secure_random_fill(void *bytes, unsigned length);

/*
	Function: secure_rand
		Returns random int (replacement for rand()).
*/
int secure_rand();

/*
	Function: secure_rand_below
		Returns a random nonnegative integer below the given number,
		with a uniform distribution.

	Parameters:
		below - Upper limit (exclusive) of integers to return.
*/
int secure_rand_below(int below);

/*
	Function: set_console_msg_color
		Sets the console color.

	Parameters:
		rgb - If NULL it will reset the console color to default, else it will transform the rgb color to a console color
*/
void set_console_msg_color(const void *rgbvoid);

/*
	Function: os_version_str
		Returns a human-readable version string of the operating system

	Parameters:
		version - Buffer to use for the output.
		length - Length of the output buffer.

	Returns:
		0 - Success in getting the version.
		1 - Failure in getting the version.
*/
int os_version_str(char *version, int length);

#if defined(CONF_EXCEPTION_HANDLING)
void init_exception_handler();
void set_exception_handler_log_file(const char *log_file_path);
#endif
}

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
 * This is a RAII wrapper for cmdline_fix and cmdline_free.
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
 * This is a RAII wrapper to initialize/uninitialize the Windows COM library,
 * which may be necessary for using the open_file and open_link functions.
 * Must be used on every thread. It's automatically used on threads created
 * with thread_init. Pass true to the constructor on threads that own a
 * window (i.e. pump a message queue).
 */
class CWindowsComLifecycle
{
public:
	CWindowsComLifecycle(bool HasWindow);
	~CWindowsComLifecycle();
};
#endif

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

template<>
struct std::hash<NETADDR>
{
	size_t operator()(const NETADDR &Addr) const noexcept;
};

#endif
