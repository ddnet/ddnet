/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

/*
	Title: OS Abstraction
*/

#ifndef BASE_SYSTEM_H
#define BASE_SYSTEM_H

#include "detect.h"
#include "stddef.h"
#include <time.h>

#ifdef CONF_FAMILY_UNIX
#include <sys/un.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Group: Debug */
/*
	Function: dbg_assert
		Breaks into the debugger based on a test.

	Parameters:
		test - Result of the test.
		msg - Message that should be printed if the test fails.

	Remarks:
		Does nothing in release version

	See Also:
		<dbg_break>
*/
#ifdef CONF_DEBUG
#define dbg_assert(test,msg) dbg_assert_imp(__FILE__, __LINE__, test, msg)
#else
#define dbg_assert(test,msg)
#endif
void dbg_assert_imp(const char *filename, int line, int test, const char *msg);


#ifdef __clang_analyzer__
#include <assert.h>
#undef dbg_assert
#define dbg_assert(test,msg) assert(test)
#endif

#ifdef __GNUC__
#define GNUC_ATTRIBUTE(x) __attribute__(x)
#else
#define GNUC_ATTRIBUTE(x)
#endif

/*
	Function: dbg_break
		Breaks into the debugger.

	Remarks:
		Does nothing in release version

	See Also:
		<dbg_assert>
*/
#ifdef CONF_DEBUG
#define dbg_break() dbg_break_imp()
#else
#define dbg_break()
#endif
void dbg_break_imp();

/*
	Function: dbg_msg

	Prints a debug message.

	Parameters:
		sys - A string that describes what system the message belongs to
		fmt - A printf styled format string.

	Remarks:
		Also works in release version

	See Also:
		<dbg_assert>
*/
void dbg_msg(const char *sys, const char *fmt, ...)
GNUC_ATTRIBUTE((format(printf, 2, 3)));

/* Group: Memory */

/*
	Function: mem_alloc
		Allocates memory.

	Parameters:
		size - Size of the needed block.
		alignment - Alignment for the block.

	Returns:
		Returns a pointer to the newly allocated block. Returns a
		null pointer if the memory couldn't be allocated.

	Remarks:
		- Passing 0 to size will allocated the smallest amount possible
		and return a unique pointer.

	See Also:
		<mem_free>
*/
void *mem_alloc_debug(const char *filename, int line, unsigned size, unsigned alignment);
#define mem_alloc(s,a) mem_alloc_debug(__FILE__, __LINE__, (s), (a))

/*
	Function: mem_free
		Frees a block allocated through <mem_alloc>.

	Remarks:
		- Is safe on null pointers.

	See Also:
		<mem_alloc>
*/
void mem_free(void *block);

/*
	Function: mem_copy
		Copies a a memory block.

	Parameters:
		dest - Destination.
		source - Source to copy.
		size - Size of the block to copy.

	Remarks:
		- This functions DOES NOT handles cases where source and
		destination is overlapping.

	See Also:
		<mem_move>
*/
void mem_copy(void *dest, const void *source, unsigned size);

/*
	Function: mem_move
		Copies a a memory block

	Parameters:
		dest - Destination
		source - Source to copy
		size - Size of the block to copy

	Remarks:
		- This functions handles cases where source and destination
		is overlapping

	See Also:
		<mem_copy>
*/
void mem_move(void *dest, const void *source, unsigned size);

/*
	Function: mem_zero
		Sets a complete memory block to 0

	Parameters:
		block - Pointer to the block to zero out
		size - Size of the block
*/
void mem_zero(void *block, unsigned size);

/*
	Function: mem_comp
		Compares two blocks of memory

	Parameters:
		a - First block of data
		b - Second block of data
		size - Size of the data to compare

	Returns:
		<0 - Block a is lesser then block b
		0 - Block a is equal to block b
		>0 - Block a is greater than block b
*/
int mem_comp(const void *a, const void *b, int size);

/*
	Function: mem_check
		Validates the heap
		Will trigger a assert if memory has failed.
*/
int mem_check_imp();
#define mem_check() dbg_assert_imp(__FILE__, __LINE__, mem_check_imp(), "Memory check failed")

/* Group: File IO */
enum {
	IOFLAG_READ = 1,
	IOFLAG_WRITE = 2,
	IOFLAG_RANDOM = 4,
	IOFLAG_APPEND = 8,

	IOSEEK_START = 0,
	IOSEEK_CUR = 1,
	IOSEEK_END = 2
};

typedef struct IOINTERNAL *IOHANDLE;

/*
	Function: io_open
		Opens a file.

	Parameters:
		filename - File to open.
		flags - A set of flags. IOFLAG_READ, IOFLAG_WRITE, IOFLAG_RANDOM, IOFLAG_APPEND.

	Returns:
		Returns a handle to the file on success and 0 on failure.

*/
IOHANDLE io_open(const char *filename, int flags);

/*
	Function: io_read
		Reads data into a buffer from a file.

	Parameters:
		io - Handle to the file to read data from.
		buffer - Pointer to the buffer that will recive the data.
		size - Number of bytes to read from the file.

	Returns:
		Number of bytes read.

*/
unsigned io_read(IOHANDLE io, void *buffer, unsigned size);

/*
	Function: io_skip
		Skips data in a file.

	Parameters:
		io - Handle to the file.
		size - Number of bytes to skip.

	Returns:
		Number of bytes skipped.
*/
unsigned io_skip(IOHANDLE io, int size);

/*
	Function: io_write
		Writes data from a buffer to file.

	Parameters:
		io - Handle to the file.
		buffer - Pointer to the data that should be written.
		size - Number of bytes to write.

	Returns:
		Number of bytes written.
*/
unsigned io_write(IOHANDLE io, const void *buffer, unsigned size);

/*
	Function: io_write_newline
		Writes newline to file.

	Parameters:
		io - Handle to the file.

	Returns:
		Number of bytes written.
*/
unsigned io_write_newline(IOHANDLE io);

/*
	Function: io_seek
		Seeks to a specified offset in the file.

	Parameters:
		io - Handle to the file.
		offset - Offset from pos to stop.
		origin - Position to start searching from.

	Returns:
		Returns 0 on success.
*/
int io_seek(IOHANDLE io, int offset, int origin);

/*
	Function: io_tell
		Gets the current position in the file.

	Parameters:
		io - Handle to the file.

	Returns:
		Returns the current position. -1L if an error occurred.
*/
long int io_tell(IOHANDLE io);

/*
	Function: io_length
		Gets the total length of the file. Resetting cursor to the beginning

	Parameters:
		io - Handle to the file.

	Returns:
		Returns the total size. -1L if an error occurred.
*/
long int io_length(IOHANDLE io);

/*
	Function: io_close
		Closes a file.

	Parameters:
		io - Handle to the file.

	Returns:
		Returns 0 on success.
*/
int io_close(IOHANDLE io);

/*
	Function: io_flush
		Empties all buffers and writes all pending data.

	Parameters:
		io - Handle to the file.

	Returns:
		Returns 0 on success.
*/
int io_flush(IOHANDLE io);

/*
	Function: io_error
		Checks whether an error occurred during I/O with the file.

	Parameters:
		io - Handle to the file.

	Returns:
		Returns nonzero on error, 0 otherwise.
*/
int io_error(IOHANDLE io);


/*
	Function: io_stdin
		Returns an <IOHANDLE> to the standard input.
*/
IOHANDLE io_stdin();

/*
	Function: io_stdout
		Returns an <IOHANDLE> to the standard output.
*/
IOHANDLE io_stdout();

/*
	Function: io_stderr
		Returns an <IOHANDLE> to the standard error.
*/
IOHANDLE io_stderr();

typedef struct ASYNCIO ASYNCIO;

/*
	Function: aio_new
		Wraps a <IOHANDLE> for asynchronous writing.

	Parameters:
		io - Handle to the file.

	Returns:
		Returns the handle for asynchronous writing.

*/
ASYNCIO *aio_new(IOHANDLE io);

/*
	Function: aio_lock
		Locks the ASYNCIO structure so it can't be written into by
		other threads.

	Parameters:
		aio - Handle to the file.
*/
void aio_lock(ASYNCIO *aio);

/*
	Function: aio_unlock
		Unlocks the ASYNCIO structure after finishing the contiguous
		write.

	Parameters:
		aio - Handle to the file.
*/
void aio_unlock(ASYNCIO *aio);

/*
	Function: aio_write
		Queues a chunk of data for writing.

	Parameters:
		aio - Handle to the file.
		buffer - Pointer to the data that should be written.
		size - Number of bytes to write.

*/
void aio_write(ASYNCIO *aio, const void *buffer, unsigned size);

/*
	Function: aio_write_newline
		Queues a newline for writing.

	Parameters:
		aio - Handle to the file.

*/
void aio_write_newline(ASYNCIO *aio);

/*
	Function: aio_write_unlocked
		Queues a chunk of data for writing. The ASYNCIO struct must be
		locked using `aio_lock` first.

	Parameters:
		aio - Handle to the file.
		buffer - Pointer to the data that should be written.
		size - Number of bytes to write.

*/
void aio_write_unlocked(ASYNCIO *aio, const void *buffer, unsigned size);

/*
	Function: aio_write_newline_unlocked
		Queues a newline for writing. The ASYNCIO struct must be locked
		using `aio_lock` first.

	Parameters:
		aio - Handle to the file.

*/
void aio_write_newline_unlocked(ASYNCIO *aio);

/*
	Function: aio_error
		Checks whether errors have occurred during the asynchronous
		writing.

		Call this function regularly to see if there are errors. Call
		this function after <aio_wait> to see if the process of writing
		to the file succeeded.

	Parameters:
		aio - Handle to the file.

	Returns:
		Returns 0 if no error occurred, and nonzero on error.

*/
int aio_error(ASYNCIO *aio);

/*
	Function: aio_close
		Queues file closing.

	Parameters:
		aio - Handle to the file.

*/
void aio_close(ASYNCIO *aio);

/*
	Function: aio_wait
		Wait for the asynchronous operations to complete.

	Parameters:
		aio - Handle to the file.

*/
void aio_wait(ASYNCIO *aio);

/*
	Function: aio_free
		Frees the resources associated to the asynchronous file handle.

	Parameters:
		aio - Handle to the file.

*/
void aio_free(ASYNCIO *aio);

/* Group: Threads */

/*
	Function: thread_sleep
		Suspends the current thread for a given period.

	Parameters:
		milliseconds - Number of milliseconds to sleep.
*/
void thread_sleep(int milliseconds);

/*
	Function: thread_init
		Creates a new thread.

	Parameters:
		threadfunc - Entry point for the new thread.
		user - Pointer to pass to the thread.

*/
void *thread_init(void (*threadfunc)(void *), void *user);

/*
	Function: thread_wait
		Waits for a thread to be done or destroyed.

	Parameters:
		thread - Thread to wait for.
*/
void thread_wait(void *thread);

/*
	Function: thread_yield
		Yield the current threads execution slice.
*/
void thread_yield();

/*
	Function: thread_detach
		Puts the thread in the detached thread, guaranteeing that
		resources of the thread will be freed immediately when the
		thread terminates.

	Parameters:
		thread - Thread to detach
*/
void thread_detach(void *thread);

/* Group: Locks */
typedef void* LOCK;

LOCK lock_create();
void lock_destroy(LOCK lock);

int lock_trylock(LOCK lock);
void lock_wait(LOCK lock);
void lock_unlock(LOCK lock);


/* Group: Semaphores */
#if defined(CONF_FAMILY_WINDOWS)
	typedef void* SEMAPHORE;
#elif defined(CONF_PLATFORM_MACOSX)
	#include <semaphore.h>
	typedef sem_t* SEMAPHORE;
#elif defined(CONF_FAMILY_UNIX)
	#include <semaphore.h>
	typedef sem_t SEMAPHORE;
#else
	#error not implemented on this platform
#endif

void sphore_init(SEMAPHORE *sem);
void sphore_wait(SEMAPHORE *sem);
void sphore_signal(SEMAPHORE *sem);
void sphore_destroy(SEMAPHORE *sem);

/* Group: Timer */
#ifdef __GNUC__
/* if compiled with -pedantic-errors it will complain about long
	not being a C90 thing.
*/
__extension__ typedef long long int64;
__extension__ typedef unsigned long long uint64;
#else
typedef long long int64;
typedef unsigned long long uint64;
#endif

void set_new_tick();

/*
	Function: time_get
		Fetches a sample from a high resolution timer.

	Returns:
		Current value of the timer.

	Remarks:
		To know how fast the timer is ticking, see <time_freq>.
*/
int64 time_get();

/*
	Function: time_freq
		Returns the frequency of the high resolution timer.

	Returns:
		Returns the frequency of the high resolution timer.
*/
int64 time_freq();

/*
	Function: time_timestamp
		Retrives the current time as a UNIX timestamp

	Returns:
		The time as a UNIX timestamp
*/
int time_timestamp();

/* Group: Network General */
typedef struct
{
	int type;
	int ipv4sock;
	int ipv6sock;
	int web_ipv4sock;
} NETSOCKET;

enum
{
	NETADDR_MAXSTRSIZE = 1+(8*4+7)+1+1+5+1, // [XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX]:XXXXX

	NETTYPE_INVALID = 0,
	NETTYPE_IPV4 = 1,
	NETTYPE_IPV6 = 2,
	NETTYPE_LINK_BROADCAST = 4,
	NETTYPE_WEBSOCKET_IPV4 = 8,
	NETTYPE_ALL = NETTYPE_IPV4|NETTYPE_IPV6|NETTYPE_WEBSOCKET_IPV4
};

typedef struct
{
	unsigned int type;
	unsigned char ip[16];
	unsigned short port;
} NETADDR;

#ifdef CONF_FAMILY_UNIX
typedef int UNIXSOCKET;
typedef struct sockaddr_un UNIXSOCKETADDR;
#endif
/*
	Function: net_init
		Initiates network functionallity.

	Returns:
		Returns 0 on success,

	Remarks:
		You must call this function before using any other network
		functions.
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

/*
	Function: net_addr_comp
		Compares two network addresses.

	Parameters:
		a - Address to compare
		b - Address to compare to.

	Returns:
		<0 - Address a is lesser then address b
		0 - Address a is equal to address b
		>0 - Address a is greater then address b
*/
int net_addr_comp(const NETADDR *a, const NETADDR *b);

/*
	Function: net_addr_str
		Turns a network address into a representative string.

	Parameters:
		addr - Address to turn into a string.
		string - Buffer to fill with the string.
		max_length - Maximum size of the string.
		add_port - add port to string or not

	Remarks:
		- The string will always be zero terminated

*/
void net_addr_str(const NETADDR *addr, char *string, int max_length, int add_port);

/*
	Function: net_addr_from_str
		Turns string into a network address.

	Returns:
		0 on success

	Parameters:
		addr - Address to fill in.
		string - String to parse.
*/
int net_addr_from_str(NETADDR *addr, const char *string);

/* Group: Network UDP */

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

/*
	Function: net_udp_send
		Sends a packet over an UDP socket.

	Parameters:
		sock - Socket to use.
		addr - Where to send the packet.
		data - Pointer to the packet data to send.
		size - Size of the packet.

	Returns:
		On success it returns the number of bytes sent. Returns -1
		on error.
*/
int net_udp_send(NETSOCKET sock, const NETADDR *addr, const void *data, int size);

/*
	Function: net_udp_recv
		Recives a packet over an UDP socket.

	Parameters:
		sock - Socket to use.
		addr - Pointer to an NETADDR that will recive the address.
		data - Pointer to a buffer that will recive the data.
		maxsize - Maximum size to recive.

	Returns:
		On success it returns the number of bytes received. Returns -1
		on error.
*/
int net_udp_recv(NETSOCKET sock, NETADDR *addr, void *data, int maxsize);

/*
	Function: net_udp_close
		Closes an UDP socket.

	Parameters:
		sock - Socket to close.

	Returns:
		Returns 0 on success. -1 on error.
*/
int net_udp_close(NETSOCKET sock);


/* Group: Network TCP */

/*
	Function: net_tcp_create
		Creates a TCP socket.

	Parameters:
		bindaddr - Address to bind the socket to.

	Returns:
		On success it returns an handle to the socket. On failure it returns NETSOCKET_INVALID.
*/
NETSOCKET net_tcp_create(NETADDR bindaddr);

/*
	Function: net_tcp_listen
		Makes the socket start listening for new connections.

	Parameters:
		sock - Socket to start listen to.
		backlog - Size of the queue of incoming connections to keep.

	Returns:
		Returns 0 on success.
*/
int net_tcp_listen(NETSOCKET sock, int backlog);

/*
	Function: net_tcp_accept
		Polls a listning socket for a new connection.

	Parameters:
		sock - Listning socket to poll.
		new_sock - Pointer to a socket to fill in with the new socket.
		addr - Pointer to an address that will be filled in the remote address (optional, can be NULL).

	Returns:
		Returns a non-negative integer on success. Negative integer on failure.
*/
int net_tcp_accept(NETSOCKET sock, NETSOCKET *new_sock, NETADDR *addr);

/*
	Function: net_tcp_connect
		Connects one socket to another.

	Parameters:
		sock - Socket to connect.
		addr - Address to connect to.

	Returns:
		Returns 0 on success.

*/
int net_tcp_connect(NETSOCKET sock, const NETADDR *addr);

/*
	Function: net_tcp_send
		Sends data to a TCP stream.

	Parameters:
		sock - Socket to send data to.
		data - Pointer to the data to send.
		size - Size of the data to send.

	Returns:
		Number of bytes sent. Negative value on failure.
*/
int net_tcp_send(NETSOCKET sock, const void *data, int size);

/*
	Function: net_tcp_recv
		Recvives data from a TCP stream.

	Parameters:
		sock - Socket to recvive data from.
		data - Pointer to a buffer to write the data to
		max_size - Maximum of data to write to the buffer.

	Returns:
		Number of bytes recvived. Negative value on failure. When in
		non-blocking mode, it returns 0 when there is no more data to
		be fetched.
*/
int net_tcp_recv(NETSOCKET sock, void *data, int maxsize);

/*
	Function: net_tcp_close
		Closes a TCP socket.

	Parameters:
		sock - Socket to close.

	Returns:
		Returns 0 on success. Negative value on failure.
*/
int net_tcp_close(NETSOCKET sock);

#if defined(CONF_FAMILY_UNIX)
/* Group: Network Unix Sockets */

/*
	Function: net_unix_create_unnamed
		Creates an unnamed unix datagram socket.

	Returns:
		On success it returns a handle to the socket. On failure it returns -1.
*/
UNIXSOCKET net_unix_create_unnamed();

/*
	Function: net_unix_send
		Sends data to a Unix socket.

	Parameters:
		sock - Socket to use.
		addr - Where to send the packet.
		data - Pointer to the packet data to send.
		size - Size of the packet.

	Returns:
		Number of bytes sent. Negative value on failure.
*/
int net_unix_send(UNIXSOCKET sock, UNIXSOCKETADDR *addr, void *data, int size);

/*
	Function: net_unix_set_addr
		Sets the unixsocketaddress for a path to a socket file.

	Parameters:
		addr - Pointer to the addressstruct to fill.
		path - Path to the (named) unix socket.
*/
void net_unix_set_addr(UNIXSOCKETADDR *addr, const char *path);

/*
	Function: net_unix_close
		Closes a Unix socket.

	Parameters:
		sock - Socket to close.
*/
void net_unix_close(UNIXSOCKET sock);

#endif

/* Group: Strings */

/*
	Function: str_append
		Appends a string to another.

	Parameters:
		dst - Pointer to a buffer that contains a string.
		src - String to append.
		dst_size - Size of the buffer of the dst string.

	Remarks:
		- The strings are treated as zero-termineted strings.
		- Guarantees that dst string will contain zero-termination.
*/
void str_append(char *dst, const char *src, int dst_size);

/*
	Function: str_copy
		Copies a string to another.

	Parameters:
		dst - Pointer to a buffer that shall recive the string.
		src - String to be copied.
		dst_size - Size of the buffer dst.

	Remarks:
		- The strings are treated as zero-termineted strings.
		- Guarantees that dst string will contain zero-termination.
*/
void str_copy(char *dst, const char *src, int dst_size);

/*
	Function: str_length
		Returns the length of a zero terminated string.

	Parameters:
		str - Pointer to the string.

	Returns:
		Length of string in bytes excluding the zero termination.
*/
int str_length(const char *str);

/*
	Function: str_format
		Performs printf formating into a buffer.

	Parameters:
		buffer - Pointer to the buffer to recive the formatted string.
		buffer_size - Size of the buffer.
		format - printf formating string.
		... - Parameters for the formating.

	Returns:
		Length of written string

	Remarks:
		- See the C manual for syntax for the printf formating string.
		- The strings are treated as zero-termineted strings.
		- Guarantees that dst string will contain zero-termination.
*/
int str_format(char *buffer, int buffer_size, const char *format, ...)
GNUC_ATTRIBUTE((format(printf, 3, 4)));

/*
	Function: str_trim_words
		Trims specific number of words at the start of a string.

	Parameters:
		str - String to trim the words from.
		words - Count of words to trim.

	Returns:
		Trimmed string

	Remarks:
		- The strings are treated as zero-termineted strings.
*/
char *str_trim_words(char *str, int words);

/*
	Function: str_sanitize_strong
		Replaces all characters below 32 and above 127 with whitespace.

	Parameters:
		str - String to sanitize.

	Remarks:
		- The strings are treated as zero-termineted strings.
*/
void str_sanitize_strong(char *str);

/*
	Function: str_sanitize_cc
		Replaces all characters below 32 with whitespace.

	Parameters:
		str - String to sanitize.

	Remarks:
		- The strings are treated as zero-termineted strings.
*/
void str_sanitize_cc(char *str);

/*
	Function: str_sanitize
		Replaces all characters below 32 with whitespace with
		exception to \t, \n and \r.

	Parameters:
		str - String to sanitize.

	Remarks:
		- The strings are treated as zero-termineted strings.
*/
void str_sanitize(char *str);

/*
	Function: str_sanitize_filename
		Replaces all invalid filename characters with whitespace.

	Parameters:
		str - String to sanitize.

	Remarks:
		- The strings are treated as zero-termineted strings.
*/
void str_sanitize_filename(char *str);

/*
	Function: str_skip_to_whitespace
		Skips leading non-whitespace characters(all but ' ', '\t', '\n', '\r').

	Parameters:
		str - Pointer to the string.

	Returns:
		Pointer to the first whitespace character found
		within the string.

	Remarks:
		- The strings are treated as zero-termineted strings.
*/
char *str_skip_to_whitespace(char *str);

/*
	Function: str_skip_whitespaces
		Skips leading whitespace characters(' ', '\t', '\n', '\r').

	Parameters:
		str - Pointer to the string.

	Returns:
		Pointer to the first non-whitespace character found
		within the string.

	Remarks:
		- The strings are treated as zero-termineted strings.
*/
char *str_skip_whitespaces(char *str);

/*
	Function: str_comp_nocase
		Compares to strings case insensitive.

	Parameters:
		a - String to compare.
		b - String to compare.

	Returns:
		<0 - String a is lesser then string b
		0 - String a is equal to string b
		>0 - String a is greater then string b

	Remarks:
		- Only garanted to work with a-z/A-Z.
		- The strings are treated as zero-termineted strings.
*/
int str_comp_nocase(const char *a, const char *b);

/*
	Function: str_comp_nocase_num
		Compares up to num characters of two strings case insensitive.

	Parameters:
		a - String to compare.
		b - String to compare.
		num - Maximum characters to compare

	Returns:
		<0 - String a is lesser than string b
		0 - String a is equal to string b
		>0 - String a is greater than string b

	Remarks:
		- Only garanted to work with a-z/A-Z.
		- The strings are treated as zero-termineted strings.
*/
int str_comp_nocase_num(const char *a, const char *b, const int num);

/*
	Function: str_comp
		Compares two strings case sensitive.

	Parameters:
		a - String to compare.
		b - String to compare.

	Returns:
		<0 - String a is lesser then string b
		0 - String a is equal to string b
		>0 - String a is greater then string b

	Remarks:
		- The strings are treated as zero-termineted strings.
*/
int str_comp(const char *a, const char *b);

/*
	Function: str_comp_num
		Compares up to num characters of two strings case sensitive.

	Parameters:
		a - String to compare.
		b - String to compare.
		num - Maximum characters to compare

	Returns:
		<0 - String a is lesser then string b
		0 - String a is equal to string b
		>0 - String a is greater then string b

	Remarks:
		- The strings are treated as zero-termineted strings.
*/
int str_comp_num(const char *a, const char *b, const int num);

/*
	Function: str_comp_filenames
		Compares two strings case sensitive, digit chars will be compared as numbers.

	Parameters:
		a - String to compare.
		b - String to compare.

	Returns:
		<0 - String a is lesser then string b
		0 - String a is equal to string b
		>0 - String a is greater then string b

	Remarks:
		- The strings are treated as zero-termineted strings.
*/
int str_comp_filenames(const char *a, const char *b);

/*
	Function: str_utf8_dist
		Computes the edit distance between two strings.

	Parameters:
		a - First string for the edit distance.
		b - Second string for the edit distance.

	Returns:
		The edit distance between the both strings.

	Remarks:
		- The strings are treated as zero-terminated strings.
*/
int str_utf8_dist(const char *a, const char *b);

/*
	Function: str_utf8_dist_buffer
		Computes the edit distance between two strings, allows buffers
		to be passed in.

	Parameters:
		a - First string for the edit distance.
		b - Second string for the edit distance.
		buf - Buffer for the function.
		buf_len - Length of the buffer, must be at least as long as
		          twice the length of both strings combined plus two.

	Returns:
		The edit distance between the both strings.

	Remarks:
		- The strings are treated as zero-terminated strings.
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
		Finds a string inside another string case insensitive.

	Parameters:
		haystack - String to search in
		needle - String to search for

	Returns:
		A pointer into haystack where the needle was found.
		Returns NULL of needle could not be found.

	Remarks:
		- Only garanted to work with a-z/A-Z.
		- The strings are treated as zero-termineted strings.
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
		Returns NULL of needle could not be found.

	Remarks:
		- The strings are treated as zero-termineted strings.
*/
const char *str_find(const char *haystack, const char *needle);

/*
	Function: str_hex
		Takes a datablock and generates a hexstring of it.

	Parameters:
		dst - Buffer to fill with hex data
		dst_size - size of the buffer
		data - Data to turn into hex
		data - Size of the data

	Remarks:
		- The desination buffer will be zero-terminated
*/
void str_hex(char *dst, int dst_size, const void *data, int data_size);

/*
	Function: str_hex_decode
		Takes a hex string and returns a byte array.

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
int str_hex_decode(unsigned char *dst, int dst_size, const char *src);
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
void str_timestamp_format(char *buffer, int buffer_size, const char *format);
void str_timestamp_ex(time_t time, char *buffer, int buffer_size, const char *format)
GNUC_ATTRIBUTE((format(strftime, 4, 0)));

#define FORMAT_TIME "%H:%M:%S"
#define FORMAT_SPACE "%Y-%m-%d %H:%M:%S"
#define FORMAT_NOSPACE "%Y-%m-%d_%H-%M-%S"

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

	Returns:
		Always returns 0.
*/
typedef int (*FS_LISTDIR_CALLBACK)(const char *name, int is_dir, int dir_type, void *user);
typedef int (*FS_LISTDIR_INFO_CALLBACK)(const char *name, time_t date, int is_dir, int dir_type, void *user);
int fs_listdir(const char *dir, FS_LISTDIR_CALLBACK cb, int type, void *user);
int fs_listdir_info(const char *dir, FS_LISTDIR_INFO_CALLBACK cb, int type, void *user);

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
		- Returns ~/Library/Applications Support/appname on Mac OS X
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
	Function: fs_getmtime
		Gets the modification time of a file
*/
time_t fs_getmtime(const char *path);

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
		- The string is treated as zero-termineted string.
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
*/
int fs_remove(const char *filename);

/*
	Function: fs_rename
		Renames the file or directory. If the paths differ the file will be moved.

	Parameters:
		oldname - The actual name
		newname - The new name

	Returns:
		Returns 0 on success, 1 on failure.

	Remarks:
		- The strings are treated as zero-terminated strings.
*/
int fs_rename(const char *oldname, const char *newname);

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

void mem_debug_dump(IOHANDLE file);

void swap_endian(void *data, unsigned elem_size, unsigned num);


typedef void (*DBG_LOGGER)(const char *line, void *user);
typedef void (*DBG_LOGGER_FINISH)(void *user);
void dbg_logger(DBG_LOGGER logger, DBG_LOGGER_FINISH finish, void *user);

void dbg_logger_stdout();
void dbg_logger_debugger();
void dbg_logger_file(const char *filename);

typedef struct
{
	int allocated;
	int active_allocations;
	int total_allocations;
} MEMSTATS;

const MEMSTATS *mem_stats();

typedef struct
{
	int sent_packets;
	int sent_bytes;
	int recv_packets;
	int recv_bytes;
} NETSTATS;


void net_stats(NETSTATS *stats);

int str_toint(const char *str);
int str_toint_base(const char *str, int base);
float str_tofloat(const char *str);
int str_isspace(char c);
char str_uppercase(char c);
unsigned str_quickhash(const char *str);

struct SKELETON;
void str_utf8_skeleton_begin(struct SKELETON *skel, const char *str);
int str_utf8_skeleton_next(struct SKELETON *skel);
int str_utf8_to_skeleton(const char *str, int *buf, int buf_len);

/*
	Function: str_utf8_comp_confusable
		Compares two strings for visual appearance.

	Parameters:
		a - String to compare.
		b - String to compare.

	Returns:
		0 if the strings are confusable.
		!=0 otherwise.
*/
int str_utf8_comp_confusable(const char *a, const char *b);

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
		str - Pointer to the string.

	Remarks:
		- The strings are treated as zero-terminated strings.
		- The string is modified in-place.
*/
void str_utf8_trim_right(char *str);

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

int pid();

/*
	Function: shell_execute
		Executes a given file.
*/
void shell_execute(const char *file);

/*
	Function: os_is_winxp_or_lower
		Checks whether the program runs on Windows XP or lower.

	Returns:
		1 - Windows XP or lower.
		0 - Higher Windows version, Linux, macOS, etc.
*/
int os_is_winxp_or_lower();

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

#ifdef __cplusplus
}
#endif

#endif
