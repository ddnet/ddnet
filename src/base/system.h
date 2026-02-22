/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

/*
	Title: OS Abstraction
*/

#ifndef BASE_SYSTEM_H
#define BASE_SYSTEM_H

#include "dbg.h"
#include "detect.h"
#include "fs.h"
#include "io.h"
#include "mem.h"
#include "secure.h"
#include "str.h"
#include "time.h"
#include "types.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

#ifdef CONF_FAMILY_UNIX
#include <sys/un.h>
#endif

#ifdef CONF_PLATFORM_LINUX
#include <netinet/in.h>
#include <sys/socket.h>
#endif

/**
 * @defgroup Network Networking
 */

/**
 * @defgroup Network-General General networking
 *
 * @ingroup Network
 */

/**
 * @ingroup Network-General
 */
extern const NETADDR NETADDR_ZEROED;

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
 * @defgroup Network-UDP UDP Networking
 *
 * @ingroup Network
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
 * @return On success it returns an handle to the socket. On failure it returns `nullptr`.
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
 * @defgroup Network-TCP TCP Networking
 *
 * @ingroup Network
 */

/**
 * Creates a TCP socket.
 *
 * @ingroup Network-TCP
 *
 * @param bindaddr Address to bind the socket to.
 *
 * @return On success it returns an handle to the socket. On failure it returns `nullptr`.
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
 * Receives data from a TCP stream.
 *
 * @ingroup Network-TCP
 *
 * @param sock Socket to recvive data from.
 * @param data Pointer to a buffer to write the data to.
 * @param maxsize Maximum of data to write to the buffer.
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
 * @defgroup Network-Unix-Sockets UNIX Socket Networking
 *
 * @ingroup Network
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

#endif

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
 * Shell and OS specific functionality.
 *
 * @defgroup Shell Shell
 */

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

#if !defined(CONF_PLATFORM_ANDROID)
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
	CCmdlineFix(const CCmdlineFix &) = delete;
};

#endif
