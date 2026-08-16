#ifndef BASE_TYPES_H
#define BASE_TYPES_H

#include <base/detect.h>

#include <cstdint>
#include <ctime>
#include <functional>

#if defined(CONF_FAMILY_UNIX)
#include <sys/types.h> // pid_t
#endif

enum class TRISTATE
{
	NONE,
	SOME,
	ALL,
};

/**
 * The maximum supported length of a file/folder path including null-termination.
 *
 * @ingroup File-IO
 */
inline constexpr auto IO_MAX_PATH_LENGTH = 512;

/**
 * Handle for input/output files/streams.
 *
 * @ingroup File-IO
 */
typedef void *IOHANDLE;

/**
 * Wrapper for asynchronously writing to an @link IOHANDLE @endlink.
 *
 * @ingroup File-IO
 */
typedef struct ASYNCIO ASYNCIO;

/**
 * Callback function type for @link fs_listdir @endlink.
 *
 * @ingroup Filesystem
 *
 * @param name The name of the file/folder entry.
 * @param is_dir Whether the entry is a file (`0`) or folder (`1`).
 * @param dir_type The value of the `type` parameter passed to `fs_listdir`.
 * @param user The value of the `user` parameter passed to `fs_listdir`.
 *
 * @return `0` to continue enumerating file/folder entries, or `1` to stop enumerating.
 *
 * @see fs_listdir
 */
typedef int (*FS_LISTDIR_CALLBACK)(const char *name, int is_dir, int dir_type, void *user);

/**
 * Represents a file/folder entry for the @link FS_LISTDIR_CALLBACK_FILEINFO @endlink
 * when using @link fs_listdir_fileinfo @endlink.
 *
 * @ingroup Filesystem
 */
class CFsFileInfo
{
public:
	/**
	 * The name of the file/folder entry.
	 */
	const char *m_pName;

	/**
	 * The creation time of the file/folder.
	 */
	time_t m_TimeCreated;

	/**
	 * The modification time of the file/folder.
	 */
	time_t m_TimeModified;
};

/**
 * Callback function type for @link fs_listdir_fileinfo @endlink.
 *
 * @ingroup Filesystem
 *
 * @param info Information of the file/folder entry.
 * @param is_dir Whether the entry is a file (`0`) or folder (`1`).
 * @param dir_type The value of the `type` parameter passed to `fs_listdir_fileinfo`.
 * @param user The value of the `user` parameter passed to `fs_listdir_fileinfo`.
 *
 * @return `0` to continue enumerating file/folder entries, or `1` to stop enumerating.
 *
 * @see fs_listdir_fileinfo
 */
typedef int (*FS_LISTDIR_CALLBACK_FILEINFO)(const CFsFileInfo *info, int is_dir, int dir_type, void *user);

/**
 * The maximum bytes necessary to encode one Unicode codepoint with UTF-8.
 *
 * @ingroup Strings
 */
inline constexpr auto UTF8_BYTE_LENGTH = 4;

/**
 * @ingroup Network-General
 */
typedef struct NETSOCKET_INTERNAL *NETSOCKET;

/**
 * @ingroup Network-General
 */
inline constexpr auto NETTYPE_INVALID = 0;

/**
 * @ingroup Network-General
 */
inline constexpr auto NETTYPE_IPV4 = 1 << 0;

/**
 * @ingroup Network-General
 */
inline constexpr auto NETTYPE_IPV6 = 1 << 1;

/**
 * @ingroup Network-General
 */
inline constexpr auto NETTYPE_WEBSOCKET_IPV4 = 1 << 2;

/**
 * @ingroup Network-General
 */
inline constexpr auto NETTYPE_WEBSOCKET_IPV6 = 1 << 3;

/**
 * @ingroup Network-General
 */
inline constexpr auto NETTYPE_LINK_BROADCAST = 1 << 4;

/**
 * 0.7 address. This is a flag in NETADDR to avoid introducing a parameter to every networking function
 * to differentiate between 0.6 and 0.7 connections.
 *
 * @ingroup Network-General
 */
inline constexpr auto NETTYPE_TW7 = 1 << 5;

/**
 * @ingroup Network-General
 */
inline constexpr auto NETTYPE_ALL = NETTYPE_IPV4 | NETTYPE_IPV6 | NETTYPE_WEBSOCKET_IPV4 | NETTYPE_WEBSOCKET_IPV6;

/**
 * @ingroup Network-General
 */
inline constexpr auto NETTYPE_MASK = NETTYPE_ALL | NETTYPE_LINK_BROADCAST | NETTYPE_TW7;

/**
 * @ingroup Network-Address
 */
inline constexpr auto NETADDR_MAXSTRSIZE = 1 + (8 * 4 + 7) + 1 + 1 + 5 + 1; // [XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX]:XXXXX

/**
 * @ingroup Network-Address
 */
typedef struct NETADDR
{
	unsigned int type;
	unsigned char ip[16];
	unsigned short port;

	bool operator==(const NETADDR &other) const;
	bool operator!=(const NETADDR &other) const;
	bool operator<(const NETADDR &other) const;
} NETADDR;

/**
 * @ingroup Network-Address
 */
template<>
struct std::hash<NETADDR>
{
	size_t operator()(const NETADDR &Addr) const noexcept;
};

/**
 * @ingroup Network-General
 */
typedef struct NETSTATS
{
	uint64_t sent_packets;
	uint64_t sent_bytes;
	uint64_t recv_packets;
	uint64_t recv_bytes;
} NETSTATS;

#if defined(CONF_FAMILY_WINDOWS)
/**
 * A handle for a process.
 *
 * @ingroup Process
 */
typedef void *PROCESS;
/**
 * A handle that denotes an invalid process.
 *
 * @ingroup Process
 */
constexpr PROCESS INVALID_PROCESS = nullptr; // NOLINT(misc-misplaced-const)
#else
/**
 * A handle for a process.
 *
 * @ingroup Process
 */
typedef pid_t PROCESS;
/**
 * A handle that denotes an invalid process.
 *
 * @ingroup Process
 */
constexpr PROCESS INVALID_PROCESS = 0;
#endif

#endif // BASE_TYPES_H
