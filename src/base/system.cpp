/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iomanip> // std::get_time
#include <iterator> // std::size
#include <sstream> // std::istringstream
#include <string_view>

#include "lock.h"
#include "logger.h"
#include "system.h"

#include <sys/types.h>

#if defined(CONF_WEBSOCKETS)
#include <engine/shared/websockets.h>
#endif

#if defined(CONF_FAMILY_UNIX)
#include <csignal>
#include <locale>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

/* unix net includes */
#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <dirent.h>

#if defined(CONF_PLATFORM_MACOS)
// some lock and pthread functions are already defined in headers
// included from Carbon.h
// this prevents having duplicate definitions of those
#define _lock_set_user_
#define _task_user_

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>
#include <mach/mach_time.h>

#if defined(__MAC_10_10) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_10
#include <pthread/qos.h>
#endif
#endif

#elif defined(CONF_FAMILY_WINDOWS)
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cerrno>
#include <cfenv>
#include <io.h>
#include <objbase.h>
#include <process.h>
#include <share.h>
#include <shellapi.h>
#include <shlobj.h> // SHChangeNotify
#include <shlwapi.h>
#include <wincrypt.h>
#else
#error NOT IMPLEMENTED
#endif

#if defined(CONF_PLATFORM_SOLARIS)
#include <sys/filio.h>
#endif

static NETSTATS network_stats = {0};

#define VLEN 128
#define PACKETSIZE 1400
typedef struct
{
#ifdef CONF_PLATFORM_LINUX
	int pos;
	int size;
	struct mmsghdr msgs[VLEN];
	struct iovec iovecs[VLEN];
	char bufs[VLEN][PACKETSIZE];
	char sockaddrs[VLEN][128];
#else
	char buf[PACKETSIZE];
#endif
} NETSOCKET_BUFFER;

void net_buffer_init(NETSOCKET_BUFFER *buffer);
void net_buffer_reinit(NETSOCKET_BUFFER *buffer);
void net_buffer_simple(NETSOCKET_BUFFER *buffer, char **buf, int *size);

struct NETSOCKET_INTERNAL
{
	int type;
	int ipv4sock;
	int ipv6sock;
	int web_ipv4sock;

	NETSOCKET_BUFFER buffer;
};
static NETSOCKET_INTERNAL invalid_socket = {NETTYPE_INVALID, -1, -1, -1};

#define AF_WEBSOCKET_INET (0xee)

std::atomic_bool dbg_assert_failing = false;
DBG_ASSERT_HANDLER dbg_assert_handler;

bool dbg_assert_has_failed()
{
	return dbg_assert_failing.load(std::memory_order_acquire);
}

void dbg_assert_imp(const char *filename, int line, bool test, const char *msg)
{
	if(!test)
	{
		const bool already_failing = dbg_assert_has_failed();
		dbg_assert_failing.store(true, std::memory_order_release);
		char error[512];
		str_format(error, sizeof(error), "%s(%d): %s", filename, line, msg);
		dbg_msg("assert", "%s", error);
		if(!already_failing)
		{
			DBG_ASSERT_HANDLER handler = dbg_assert_handler;
			if(handler)
				handler(error);
		}
		log_global_logger_finish();
		dbg_break();
	}
}

void dbg_break()
{
#ifdef __GNUC__
	__builtin_trap();
#else
	abort();
#endif
}

void dbg_assert_set_handler(DBG_ASSERT_HANDLER handler)
{
	dbg_assert_handler = std::move(handler);
}

void dbg_msg(const char *sys, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_log_v(LEVEL_INFO, sys, fmt, args);
	va_end(args);
}

/* */

void mem_copy(void *dest, const void *source, size_t size)
{
	memcpy(dest, source, size);
}

void mem_move(void *dest, const void *source, size_t size)
{
	memmove(dest, source, size);
}

int mem_comp(const void *a, const void *b, size_t size)
{
	return memcmp(a, b, size);
}

bool mem_has_null(const void *block, size_t size)
{
	const unsigned char *bytes = (const unsigned char *)block;
	for(size_t i = 0; i < size; i++)
	{
		if(bytes[i] == 0)
		{
			return true;
		}
	}
	return false;
}

IOHANDLE io_open(const char *filename, int flags)
{
	dbg_assert(flags == IOFLAG_READ || flags == IOFLAG_WRITE || flags == IOFLAG_APPEND, "flags must be read, write or append");
#if defined(CONF_FAMILY_WINDOWS)
	const std::wstring wide_filename = windows_utf8_to_wide(filename);
	DWORD desired_access;
	DWORD creation_disposition;
	const char *open_mode;
	if((flags & IOFLAG_READ) != 0)
	{
		desired_access = FILE_READ_DATA;
		creation_disposition = OPEN_EXISTING;
		open_mode = "rb";
	}
	else if(flags == IOFLAG_WRITE)
	{
		desired_access = FILE_WRITE_DATA;
		creation_disposition = CREATE_ALWAYS;
		open_mode = "wb";
	}
	else if(flags == IOFLAG_APPEND)
	{
		desired_access = FILE_APPEND_DATA;
		creation_disposition = OPEN_ALWAYS;
		open_mode = "ab";
	}
	else
	{
		dbg_assert(false, "logic error");
		return nullptr;
	}
	HANDLE handle = CreateFileW(wide_filename.c_str(), desired_access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, creation_disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
	if(handle == INVALID_HANDLE_VALUE)
		return nullptr;
	const int file_descriptor = _open_osfhandle((intptr_t)handle, 0);
	dbg_assert(file_descriptor != -1, "_open_osfhandle failure");
	FILE *file_stream = _fdopen(file_descriptor, open_mode);
	dbg_assert(file_stream != nullptr, "_fdopen failure");
	return file_stream;
#else
	const char *open_mode;
	if((flags & IOFLAG_READ) != 0)
	{
		open_mode = "rb";
	}
	else if(flags == IOFLAG_WRITE)
	{
		open_mode = "wb";
	}
	else if(flags == IOFLAG_APPEND)
	{
		open_mode = "ab";
	}
	else
	{
		dbg_assert(false, "logic error");
		return nullptr;
	}
	return fopen(filename, open_mode);
#endif
}

unsigned io_read(IOHANDLE io, void *buffer, unsigned size)
{
	return fread(buffer, 1, size, (FILE *)io);
}

void io_read_all(IOHANDLE io, void **result, unsigned *result_len)
{
	long signed_len = io_length(io);
	unsigned len = signed_len < 0 ? 1024 : (unsigned)signed_len; // use default initial size if we couldn't get the length
	char *buffer = (char *)malloc(len + 1);
	unsigned read = io_read(io, buffer, len + 1); // +1 to check if the file size is larger than expected
	if(read < len)
	{
		buffer = (char *)realloc(buffer, read + 1);
		len = read;
	}
	else if(read > len)
	{
		unsigned cap = 2 * read;
		len = read;
		buffer = (char *)realloc(buffer, cap);
		while((read = io_read(io, buffer + len, cap - len)) != 0)
		{
			len += read;
			if(len == cap)
			{
				cap *= 2;
				buffer = (char *)realloc(buffer, cap);
			}
		}
		buffer = (char *)realloc(buffer, len + 1);
	}
	buffer[len] = 0;
	*result = buffer;
	*result_len = len;
}

char *io_read_all_str(IOHANDLE io)
{
	void *buffer;
	unsigned len;
	io_read_all(io, &buffer, &len);
	if(mem_has_null(buffer, len))
	{
		free(buffer);
		return nullptr;
	}
	return (char *)buffer;
}

int io_skip(IOHANDLE io, int size)
{
	return io_seek(io, size, IOSEEK_CUR);
}

int io_seek(IOHANDLE io, int offset, int origin)
{
	int real_origin;
	switch(origin)
	{
	case IOSEEK_START:
		real_origin = SEEK_SET;
		break;
	case IOSEEK_CUR:
		real_origin = SEEK_CUR;
		break;
	case IOSEEK_END:
		real_origin = SEEK_END;
		break;
	default:
		dbg_assert(false, "origin invalid");
		return -1;
	}
	return fseek((FILE *)io, offset, real_origin);
}

long int io_tell(IOHANDLE io)
{
	return ftell((FILE *)io);
}

long int io_length(IOHANDLE io)
{
	long int length;
	io_seek(io, 0, IOSEEK_END);
	length = io_tell(io);
	io_seek(io, 0, IOSEEK_START);
	return length;
}

unsigned io_write(IOHANDLE io, const void *buffer, unsigned size)
{
	return fwrite(buffer, 1, size, (FILE *)io);
}

bool io_write_newline(IOHANDLE io)
{
#if defined(CONF_FAMILY_WINDOWS)
	return io_write(io, "\r\n", 2) == 2;
#else
	return io_write(io, "\n", 1) == 1;
#endif
}

int io_close(IOHANDLE io)
{
	return fclose((FILE *)io) != 0;
}

int io_flush(IOHANDLE io)
{
	return fflush((FILE *)io);
}

int io_sync(IOHANDLE io)
{
	if(io_flush(io))
	{
		return 1;
	}
#if defined(CONF_FAMILY_WINDOWS)
	return FlushFileBuffers((HANDLE)_get_osfhandle(_fileno((FILE *)io))) == FALSE;
#else
	return fsync(fileno((FILE *)io)) != 0;
#endif
}

int io_error(IOHANDLE io)
{
	return ferror((FILE *)io);
}

IOHANDLE io_stdin()
{
	return stdin;
}

IOHANDLE io_stdout()
{
	return stdout;
}

IOHANDLE io_stderr()
{
	return stderr;
}

IOHANDLE io_current_exe()
{
	// From https://stackoverflow.com/a/1024937.
#if defined(CONF_FAMILY_WINDOWS)
	wchar_t wide_path[IO_MAX_PATH_LENGTH];
	if(GetModuleFileNameW(NULL, wide_path, std::size(wide_path)) == 0 || GetLastError() != ERROR_SUCCESS)
	{
		return 0;
	}
	const std::optional<std::string> path = windows_wide_to_utf8(wide_path);
	return path.has_value() ? io_open(path.value().c_str(), IOFLAG_READ) : 0;
#elif defined(CONF_PLATFORM_MACOS)
	char path[IO_MAX_PATH_LENGTH];
	uint32_t path_size = sizeof(path);
	if(_NSGetExecutablePath(path, &path_size))
	{
		return 0;
	}
	return io_open(path, IOFLAG_READ);
#else
	static const char *NAMES[] = {
		"/proc/self/exe", // Linux, Android
		"/proc/curproc/exe", // NetBSD
		"/proc/curproc/file", // DragonFly
	};
	for(auto &name : NAMES)
	{
		IOHANDLE result = io_open(name, IOFLAG_READ);
		if(result)
		{
			return result;
		}
	}
	return 0;
#endif
}

#define ASYNC_BUFSIZE (8 * 1024)
#define ASYNC_LOCAL_BUFSIZE (64 * 1024)

struct ASYNCIO
{
	CLock lock;
	IOHANDLE io;
	SEMAPHORE sphore;
	void *thread;

	unsigned char *buffer;
	unsigned int buffer_size;
	unsigned int read_pos;
	unsigned int write_pos;

	int error;
	unsigned char finish;
	unsigned char refcount;
};

enum
{
	ASYNCIO_RUNNING,
	ASYNCIO_CLOSE,
	ASYNCIO_EXIT,
};

struct BUFFERS
{
	unsigned char *buf1;
	unsigned int len1;
	unsigned char *buf2;
	unsigned int len2;
};

static void buffer_ptrs(ASYNCIO *aio, struct BUFFERS *buffers)
{
	mem_zero(buffers, sizeof(*buffers));
	if(aio->read_pos < aio->write_pos)
	{
		buffers->buf1 = aio->buffer + aio->read_pos;
		buffers->len1 = aio->write_pos - aio->read_pos;
	}
	else if(aio->read_pos > aio->write_pos)
	{
		buffers->buf1 = aio->buffer + aio->read_pos;
		buffers->len1 = aio->buffer_size - aio->read_pos;
		buffers->buf2 = aio->buffer;
		buffers->len2 = aio->write_pos;
	}
}

static void aio_handle_free_and_unlock(ASYNCIO *aio) RELEASE(aio->lock)
{
	int do_free;
	aio->refcount--;

	do_free = aio->refcount == 0;
	aio->lock.unlock();
	if(do_free)
	{
		free(aio->buffer);
		sphore_destroy(&aio->sphore);
		delete aio;
	}
}

static void aio_thread(void *user)
{
	ASYNCIO *aio = (ASYNCIO *)user;

	aio->lock.lock();
	while(true)
	{
		struct BUFFERS buffers;
		int result_io_error;
		unsigned char local_buffer[ASYNC_LOCAL_BUFSIZE];
		unsigned int local_buffer_len = 0;

		if(aio->read_pos == aio->write_pos)
		{
			if(aio->finish != ASYNCIO_RUNNING)
			{
				if(aio->finish == ASYNCIO_CLOSE)
				{
					io_close(aio->io);
				}
				aio_handle_free_and_unlock(aio);
				break;
			}
			aio->lock.unlock();
			sphore_wait(&aio->sphore);
			aio->lock.lock();
			continue;
		}

		buffer_ptrs(aio, &buffers);
		if(buffers.buf1)
		{
			if(buffers.len1 > sizeof(local_buffer) - local_buffer_len)
			{
				buffers.len1 = sizeof(local_buffer) - local_buffer_len;
			}
			mem_copy(local_buffer + local_buffer_len, buffers.buf1, buffers.len1);
			local_buffer_len += buffers.len1;
			if(buffers.buf2)
			{
				if(buffers.len2 > sizeof(local_buffer) - local_buffer_len)
				{
					buffers.len2 = sizeof(local_buffer) - local_buffer_len;
				}
				mem_copy(local_buffer + local_buffer_len, buffers.buf2, buffers.len2);
				local_buffer_len += buffers.len2;
			}
		}
		aio->read_pos = (aio->read_pos + buffers.len1 + buffers.len2) % aio->buffer_size;
		aio->lock.unlock();

		io_write(aio->io, local_buffer, local_buffer_len);
		io_flush(aio->io);
		result_io_error = io_error(aio->io);

		aio->lock.lock();
		aio->error = result_io_error;
	}
}

ASYNCIO *aio_new(IOHANDLE io)
{
	ASYNCIO *aio = new ASYNCIO;
	if(!aio)
	{
		return 0;
	}
	aio->io = io;
	sphore_init(&aio->sphore);
	aio->thread = 0;

	aio->buffer = (unsigned char *)malloc(ASYNC_BUFSIZE);
	if(!aio->buffer)
	{
		sphore_destroy(&aio->sphore);
		delete aio;
		return 0;
	}
	aio->buffer_size = ASYNC_BUFSIZE;
	aio->read_pos = 0;
	aio->write_pos = 0;
	aio->error = 0;
	aio->finish = ASYNCIO_RUNNING;
	aio->refcount = 2;

	aio->thread = thread_init(aio_thread, aio, "aio");
	if(!aio->thread)
	{
		free(aio->buffer);
		sphore_destroy(&aio->sphore);
		delete aio;
		return 0;
	}
	return aio;
}

static unsigned int buffer_len(ASYNCIO *aio)
{
	if(aio->write_pos >= aio->read_pos)
	{
		return aio->write_pos - aio->read_pos;
	}
	else
	{
		return aio->buffer_size + aio->write_pos - aio->read_pos;
	}
}

static unsigned int next_buffer_size(unsigned int cur_size, unsigned int need_size)
{
	while(cur_size < need_size)
	{
		cur_size *= 2;
	}
	return cur_size;
}

void aio_lock(ASYNCIO *aio) ACQUIRE(aio->lock)
{
	aio->lock.lock();
}

void aio_unlock(ASYNCIO *aio) RELEASE(aio->lock)
{
	aio->lock.unlock();
	sphore_signal(&aio->sphore);
}

void aio_write_unlocked(ASYNCIO *aio, const void *buffer, unsigned size)
{
	unsigned int remaining;
	remaining = aio->buffer_size - buffer_len(aio);

	// Don't allow full queue to distinguish between empty and full queue.
	if(size < remaining)
	{
		unsigned int remaining_contiguous = aio->buffer_size - aio->write_pos;
		if(size > remaining_contiguous)
		{
			mem_copy(aio->buffer + aio->write_pos, buffer, remaining_contiguous);
			size -= remaining_contiguous;
			buffer = ((unsigned char *)buffer) + remaining_contiguous;
			aio->write_pos = 0;
		}
		mem_copy(aio->buffer + aio->write_pos, buffer, size);
		aio->write_pos = (aio->write_pos + size) % aio->buffer_size;
	}
	else
	{
		// Add 1 so the new buffer isn't completely filled.
		unsigned int new_written = buffer_len(aio) + size + 1;
		unsigned int next_size = next_buffer_size(aio->buffer_size, new_written);
		unsigned int next_len = 0;
		unsigned char *next_buffer = (unsigned char *)malloc(next_size);

		struct BUFFERS buffers;
		buffer_ptrs(aio, &buffers);
		if(buffers.buf1)
		{
			mem_copy(next_buffer + next_len, buffers.buf1, buffers.len1);
			next_len += buffers.len1;
			if(buffers.buf2)
			{
				mem_copy(next_buffer + next_len, buffers.buf2, buffers.len2);
				next_len += buffers.len2;
			}
		}
		mem_copy(next_buffer + next_len, buffer, size);
		next_len += size;

		free(aio->buffer);
		aio->buffer = next_buffer;
		aio->buffer_size = next_size;
		aio->read_pos = 0;
		aio->write_pos = next_len;
	}
}

void aio_write(ASYNCIO *aio, const void *buffer, unsigned size)
{
	aio_lock(aio);
	aio_write_unlocked(aio, buffer, size);
	aio_unlock(aio);
}

void aio_write_newline_unlocked(ASYNCIO *aio)
{
#if defined(CONF_FAMILY_WINDOWS)
	aio_write_unlocked(aio, "\r\n", 2);
#else
	aio_write_unlocked(aio, "\n", 1);
#endif
}

void aio_write_newline(ASYNCIO *aio)
{
	aio_lock(aio);
	aio_write_newline_unlocked(aio);
	aio_unlock(aio);
}

int aio_error(ASYNCIO *aio)
{
	CLockScope ls(aio->lock);
	return aio->error;
}

void aio_close(ASYNCIO *aio)
{
	{
		CLockScope ls(aio->lock);
		aio->finish = ASYNCIO_CLOSE;
	}
	sphore_signal(&aio->sphore);
}

void aio_wait(ASYNCIO *aio)
{
	void *thread;
	{
		CLockScope ls(aio->lock);
		thread = aio->thread;
		aio->thread = 0;
		if(aio->finish == ASYNCIO_RUNNING)
		{
			aio->finish = ASYNCIO_EXIT;
		}
	}
	sphore_signal(&aio->sphore);
	thread_wait(thread);
}

void aio_free(ASYNCIO *aio)
{
	aio->lock.lock();
	if(aio->thread)
	{
		thread_detach(aio->thread);
		aio->thread = 0;
	}
	aio_handle_free_and_unlock(aio);
}

struct THREAD_RUN
{
	void (*threadfunc)(void *);
	void *u;
};

#if defined(CONF_FAMILY_UNIX)
static void *thread_run(void *user)
#elif defined(CONF_FAMILY_WINDOWS)
static unsigned long __stdcall thread_run(void *user)
#else
#error not implemented
#endif
{
#if defined(CONF_FAMILY_WINDOWS)
	CWindowsComLifecycle WindowsComLifecycle(false);
#endif
	struct THREAD_RUN *data = (THREAD_RUN *)user;
	void (*threadfunc)(void *) = data->threadfunc;
	void *u = data->u;
	free(data);
	threadfunc(u);
	return 0;
}

void *thread_init(void (*threadfunc)(void *), void *u, const char *name)
{
	struct THREAD_RUN *data = (THREAD_RUN *)malloc(sizeof(*data));
	data->threadfunc = threadfunc;
	data->u = u;
#if defined(CONF_FAMILY_UNIX)
	{
		pthread_attr_t attr;
		dbg_assert(pthread_attr_init(&attr) == 0, "pthread_attr_init failure");
#if defined(CONF_PLATFORM_MACOS) && defined(__MAC_10_10) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_10
		dbg_assert(pthread_attr_set_qos_class_np(&attr, QOS_CLASS_USER_INTERACTIVE, 0) == 0, "pthread_attr_set_qos_class_np failure");
#endif
		pthread_t id;
		dbg_assert(pthread_create(&id, &attr, thread_run, data) == 0, "pthread_create failure");
		return (void *)id;
	}
#elif defined(CONF_FAMILY_WINDOWS)
	HANDLE thread = CreateThread(nullptr, 0, thread_run, data, 0, nullptr);
	dbg_assert(thread != nullptr, "CreateThread failure");
	// TODO: Set thread name using SetThreadDescription (would require minimum Windows 10 version 1607)
	return thread;
#else
#error not implemented
#endif
}

void thread_wait(void *thread)
{
#if defined(CONF_FAMILY_UNIX)
	dbg_assert(pthread_join((pthread_t)thread, nullptr) == 0, "pthread_join failure");
#elif defined(CONF_FAMILY_WINDOWS)
	dbg_assert(WaitForSingleObject((HANDLE)thread, INFINITE) == WAIT_OBJECT_0, "WaitForSingleObject failure");
	dbg_assert(CloseHandle(thread), "CloseHandle failure");
#else
#error not implemented
#endif
}

void thread_yield()
{
#if defined(CONF_FAMILY_UNIX)
	dbg_assert(sched_yield() == 0, "sched_yield failure");
#elif defined(CONF_FAMILY_WINDOWS)
	Sleep(0);
#else
#error not implemented
#endif
}

void thread_detach(void *thread)
{
#if defined(CONF_FAMILY_UNIX)
	dbg_assert(pthread_detach((pthread_t)thread) == 0, "pthread_detach failure");
#elif defined(CONF_FAMILY_WINDOWS)
	dbg_assert(CloseHandle(thread), "CloseHandle failure");
#else
#error not implemented
#endif
}

void thread_init_and_detach(void (*threadfunc)(void *), void *u, const char *name)
{
	void *thread = thread_init(threadfunc, u, name);
	thread_detach(thread);
}

#if defined(CONF_FAMILY_WINDOWS)
void sphore_init(SEMAPHORE *sem)
{
	*sem = CreateSemaphoreW(nullptr, 0, std::numeric_limits<LONG>::max(), nullptr);
	dbg_assert(*sem != nullptr, "CreateSemaphoreW failure");
}
void sphore_wait(SEMAPHORE *sem)
{
	dbg_assert(WaitForSingleObject((HANDLE)*sem, INFINITE) == WAIT_OBJECT_0, "WaitForSingleObject failure");
}
void sphore_signal(SEMAPHORE *sem)
{
	dbg_assert(ReleaseSemaphore((HANDLE)*sem, 1, nullptr), "ReleaseSemaphore failure");
}
void sphore_destroy(SEMAPHORE *sem)
{
	dbg_assert(CloseHandle((HANDLE)*sem), "CloseHandle failure");
}
#elif defined(CONF_PLATFORM_MACOS)
void sphore_init(SEMAPHORE *sem)
{
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "/%d.%p", pid(), (void *)sem);
	*sem = sem_open(aBuf, O_CREAT | O_EXCL, S_IRWXU | S_IRWXG, 0);
	if(*sem == SEM_FAILED)
	{
		char aError[128];
		str_format(aError, sizeof(aError), "sem_open failure, errno=%d, name='%s'", errno, aBuf);
		dbg_assert(false, aError);
	}
}
void sphore_wait(SEMAPHORE *sem)
{
	while(true)
	{
		if(sem_wait(*sem) == 0)
			break;
		dbg_assert(errno == EINTR, "sem_wait failure");
	}
}
void sphore_signal(SEMAPHORE *sem)
{
	dbg_assert(sem_post(*sem) == 0, "sem_post failure");
}
void sphore_destroy(SEMAPHORE *sem)
{
	dbg_assert(sem_close(*sem) == 0, "sem_close failure");
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "/%d.%p", pid(), (void *)sem);
	dbg_assert(sem_unlink(aBuf) == 0, "sem_unlink failure");
}
#elif defined(CONF_FAMILY_UNIX)
void sphore_init(SEMAPHORE *sem)
{
	dbg_assert(sem_init(sem, 0, 0) == 0, "sem_init failure");
}
void sphore_wait(SEMAPHORE *sem)
{
	while(true)
	{
		if(sem_wait(sem) == 0)
			break;
		dbg_assert(errno == EINTR, "sem_wait failure");
	}
}
void sphore_signal(SEMAPHORE *sem)
{
	dbg_assert(sem_post(sem) == 0, "sem_post failure");
}
void sphore_destroy(SEMAPHORE *sem)
{
	dbg_assert(sem_destroy(sem) == 0, "sem_destroy failure");
}
#endif

static int new_tick = -1;

void set_new_tick()
{
	new_tick = 1;
}

/* -----  time ----- */
static_assert(std::chrono::steady_clock::is_steady, "Compiler does not support steady clocks, it might be out of date.");
static_assert(std::chrono::steady_clock::period::den / std::chrono::steady_clock::period::num >= 1000000000, "Compiler has a bad timer precision and might be out of date.");
static const std::chrono::time_point<std::chrono::steady_clock> tw_start_time = std::chrono::steady_clock::now();

int64_t time_get_impl()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - tw_start_time).count();
}

int64_t time_get()
{
	static int64_t last = 0;
	if(new_tick == 0)
		return last;
	if(new_tick != -1)
		new_tick = 0;

	last = time_get_impl();
	return last;
}

int64_t time_freq()
{
	using namespace std::chrono_literals;
	return std::chrono::nanoseconds(1s).count();
}

/* -----  network ----- */

const NETADDR NETADDR_ZEROED = {NETTYPE_INVALID, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 0};

static void netaddr_to_sockaddr_in(const NETADDR *src, struct sockaddr_in *dest)
{
	mem_zero(dest, sizeof(struct sockaddr_in));
	if(!(src->type & NETTYPE_IPV4) && !(src->type & NETTYPE_WEBSOCKET_IPV4))
	{
		dbg_msg("system", "couldn't convert NETADDR of type %d to ipv4", src->type);
		return;
	}

	dest->sin_family = AF_INET;
	dest->sin_port = htons(src->port);
	mem_copy(&dest->sin_addr.s_addr, src->ip, 4);
}

static void netaddr_to_sockaddr_in6(const NETADDR *src, struct sockaddr_in6 *dest)
{
	mem_zero(dest, sizeof(struct sockaddr_in6));
	if(!(src->type & NETTYPE_IPV6))
	{
		dbg_msg("system", "couldn't convert NETADDR of type %d to ipv6", src->type);
		return;
	}

	dest->sin6_family = AF_INET6;
	dest->sin6_port = htons(src->port);
	mem_copy(&dest->sin6_addr.s6_addr, src->ip, 16);
}

static void sockaddr_to_netaddr(const struct sockaddr *src, NETADDR *dst)
{
	if(src->sa_family == AF_INET)
	{
		mem_zero(dst, sizeof(NETADDR));
		dst->type = NETTYPE_IPV4;
		dst->port = htons(((struct sockaddr_in *)src)->sin_port);
		mem_copy(dst->ip, &((struct sockaddr_in *)src)->sin_addr.s_addr, 4);
	}
	else if(src->sa_family == AF_WEBSOCKET_INET)
	{
		mem_zero(dst, sizeof(NETADDR));
		dst->type = NETTYPE_WEBSOCKET_IPV4;
		dst->port = htons(((struct sockaddr_in *)src)->sin_port);
		mem_copy(dst->ip, &((struct sockaddr_in *)src)->sin_addr.s_addr, 4);
	}
	else if(src->sa_family == AF_INET6)
	{
		mem_zero(dst, sizeof(NETADDR));
		dst->type = NETTYPE_IPV6;
		dst->port = htons(((struct sockaddr_in6 *)src)->sin6_port);
		mem_copy(dst->ip, &((struct sockaddr_in6 *)src)->sin6_addr.s6_addr, 16);
	}
	else
	{
		mem_zero(dst, sizeof(struct sockaddr));
		dbg_msg("system", "couldn't convert sockaddr of family %d", src->sa_family);
	}
}

int net_addr_comp(const NETADDR *a, const NETADDR *b)
{
	return mem_comp(a, b, sizeof(NETADDR));
}

bool NETADDR::operator==(const NETADDR &other) const
{
	return net_addr_comp(this, &other) == 0;
}

int net_addr_comp_noport(const NETADDR *a, const NETADDR *b)
{
	NETADDR ta = *a, tb = *b;
	ta.port = tb.port = 0;

	return net_addr_comp(&ta, &tb);
}

void net_addr_str_v6(const unsigned short ip[8], int port, char *buffer, int buffer_size)
{
	int longest_seq_len = 0;
	int longest_seq_start = -1;
	int w = 0;
	int i;
	{
		int seq_len = 0;
		int seq_start = -1;
		// Determine longest sequence of zeros.
		for(i = 0; i < 8 + 1; i++)
		{
			if(seq_start != -1)
			{
				if(i == 8 || ip[i] != 0)
				{
					if(longest_seq_len < seq_len)
					{
						longest_seq_len = seq_len;
						longest_seq_start = seq_start;
					}
					seq_len = 0;
					seq_start = -1;
				}
				else
				{
					seq_len += 1;
				}
			}
			else
			{
				if(i != 8 && ip[i] == 0)
				{
					seq_start = i;
					seq_len = 1;
				}
			}
		}
	}
	if(longest_seq_len <= 1)
	{
		longest_seq_len = 0;
		longest_seq_start = -1;
	}
	w += str_copy(buffer + w, "[", buffer_size - w);
	for(i = 0; i < 8; i++)
	{
		if(longest_seq_start <= i && i < longest_seq_start + longest_seq_len)
		{
			if(i == longest_seq_start)
			{
				w += str_copy(buffer + w, "::", buffer_size - w);
			}
		}
		else
		{
			const char *colon = (i == 0 || i == longest_seq_start + longest_seq_len) ? "" : ":";
			w += str_format(buffer + w, buffer_size - w, "%s%x", colon, ip[i]);
		}
	}
	w += str_copy(buffer + w, "]", buffer_size - w);
	if(port >= 0)
	{
		str_format(buffer + w, buffer_size - w, ":%d", port);
	}
}

bool net_addr_str(const NETADDR *addr, char *string, int max_length, int add_port)
{
	if(addr->type & NETTYPE_IPV4 || addr->type & NETTYPE_WEBSOCKET_IPV4)
	{
		if(add_port != 0)
			str_format(string, max_length, "%d.%d.%d.%d:%d", addr->ip[0], addr->ip[1], addr->ip[2], addr->ip[3], addr->port);
		else
			str_format(string, max_length, "%d.%d.%d.%d", addr->ip[0], addr->ip[1], addr->ip[2], addr->ip[3]);
	}
	else if(addr->type & NETTYPE_IPV6)
	{
		int port = -1;
		unsigned short ip[8];
		int i;
		if(add_port)
		{
			port = addr->port;
		}
		for(i = 0; i < 8; i++)
		{
			ip[i] = (addr->ip[i * 2] << 8) | (addr->ip[i * 2 + 1]);
		}
		net_addr_str_v6(ip, port, string, max_length);
	}
	else
	{
		str_format(string, max_length, "unknown type %d", addr->type);
		return false;
	}
	return true;
}

static int priv_net_extract(const char *hostname, char *host, int max_host, int *port)
{
	int i;

	*port = 0;
	host[0] = 0;

	if(hostname[0] == '[')
	{
		// ipv6 mode
		for(i = 1; i < max_host && hostname[i] && hostname[i] != ']'; i++)
			host[i - 1] = hostname[i];
		host[i - 1] = 0;
		if(hostname[i] != ']') // malformatted
			return -1;

		i++;
		if(hostname[i] == ':')
			*port = str_toint(hostname + i + 1);
	}
	else
	{
		// generic mode (ipv4, hostname etc)
		for(i = 0; i < max_host - 1 && hostname[i] && hostname[i] != ':'; i++)
			host[i] = hostname[i];
		host[i] = 0;

		if(hostname[i] == ':')
			*port = str_toint(hostname + i + 1);
	}

	return 0;
}

int net_host_lookup_impl(const char *hostname, NETADDR *addr, int types)
{
	struct addrinfo hints;
	struct addrinfo *result = NULL;
	int e;
	char host[256];
	int port = 0;

	if(priv_net_extract(hostname, host, sizeof(host), &port))
		return -1;

	dbg_msg("host_lookup", "host='%s' port=%d %d", host, port, types);

	mem_zero(&hints, sizeof(hints));

	hints.ai_family = AF_UNSPEC;

	if(types == NETTYPE_IPV4)
		hints.ai_family = AF_INET;
	else if(types == NETTYPE_IPV6)
		hints.ai_family = AF_INET6;

	e = getaddrinfo(host, NULL, &hints, &result);

	if(!result)
		return -1;

	if(e != 0)
	{
		freeaddrinfo(result);
		return -1;
	}

	sockaddr_to_netaddr(result->ai_addr, addr);
	addr->port = port;
	freeaddrinfo(result);
	return 0;
}

int net_host_lookup(const char *hostname, NETADDR *addr, int types)
{
	const char *ws_hostname = str_startswith(hostname, "ws://");
	if(ws_hostname)
	{
		if((types & NETTYPE_WEBSOCKET_IPV4) == 0)
		{
			return -1;
		}
		int result = net_host_lookup_impl(ws_hostname, addr, NETTYPE_IPV4);
		if(result == 0 && addr->type == NETTYPE_IPV4)
		{
			addr->type = NETTYPE_WEBSOCKET_IPV4;
		}
		return result;
	}
	return net_host_lookup_impl(hostname, addr, types & ~NETTYPE_WEBSOCKET_IPV4);
}

static int parse_int(int *out, const char **str)
{
	int i = 0;
	*out = 0;
	if(!str_isnum(**str))
		return -1;

	i = **str - '0';
	(*str)++;

	while(true)
	{
		if(!str_isnum(**str))
		{
			*out = i;
			return 0;
		}

		i = (i * 10) + (**str - '0');
		(*str)++;
	}

	return 0;
}

static int parse_char(char c, const char **str)
{
	if(**str != c)
		return -1;
	(*str)++;
	return 0;
}

static int parse_uint8(unsigned char *out, const char **str)
{
	int i;
	if(parse_int(&i, str) != 0)
		return -1;
	if(i < 0 || i > 0xff)
		return -1;
	*out = i;
	return 0;
}

static int parse_uint16(unsigned short *out, const char **str)
{
	int i;
	if(parse_int(&i, str) != 0)
		return -1;
	if(i < 0 || i > 0xffff)
		return -1;
	*out = i;
	return 0;
}

int net_addr_from_url(NETADDR *addr, const char *string, char *host_buf, size_t host_buf_size)
{
	bool sixup = false;
	mem_zero(addr, sizeof(*addr));
	const char *str = str_startswith(string, "tw-0.6+udp://");
	if(!str && (str = str_startswith(string, "tw-0.7+udp://")))
	{
		addr->type |= NETTYPE_TW7;
		sixup = true;
	}
	if(!str)
		return 1;

	int length = str_length(str);
	int start = 0;
	int end = length;
	for(int i = 0; i < length; i++)
	{
		if(str[i] == '@')
		{
			if(start != 0)
			{
				// Two at signs.
				return true;
			}
			start = i + 1;
		}
		else if(str[i] == '/' || str[i] == '?' || str[i] == '#')
		{
			end = i;
			break;
		}
	}

	char host[128];
	str_truncate(host, sizeof(host), str + start, end - start);
	if(host_buf)
		str_copy(host_buf, host, host_buf_size);

	int failure = net_addr_from_str(addr, host);
	if(failure)
		return failure;

	if(sixup)
		addr->type |= NETTYPE_TW7;

	return failure;
}

int net_addr_from_str(NETADDR *addr, const char *string)
{
	const char *str = string;
	mem_zero(addr, sizeof(NETADDR));

	if(str[0] == '[')
	{
		/* ipv6 */
		struct sockaddr_in6 sa6;
		char buf[128];
		int i;
		str++;
		for(i = 0; i < 127 && str[i] && str[i] != ']'; i++)
			buf[i] = str[i];
		buf[i] = 0;
		str += i;
#if defined(CONF_FAMILY_WINDOWS)
		{
			int size;
			sa6.sin6_family = AF_INET6;
			size = (int)sizeof(sa6);
			if(WSAStringToAddressA(buf, AF_INET6, NULL, (struct sockaddr *)&sa6, &size) != 0)
				return -1;
		}
#else
		sa6.sin6_family = AF_INET6;

		if(inet_pton(AF_INET6, buf, &sa6.sin6_addr) != 1)
			return -1;
#endif
		sockaddr_to_netaddr((struct sockaddr *)&sa6, addr);

		if(*str == ']')
		{
			str++;
			if(*str == ':')
			{
				str++;
				if(parse_uint16(&addr->port, &str))
					return -1;
			}
			else
			{
				addr->port = 0;
			}
		}
		else
			return -1;

		return 0;
	}
	else
	{
		/* ipv4 */
		if(parse_uint8(&addr->ip[0], &str))
			return -1;
		if(parse_char('.', &str))
			return -1;
		if(parse_uint8(&addr->ip[1], &str))
			return -1;
		if(parse_char('.', &str))
			return -1;
		if(parse_uint8(&addr->ip[2], &str))
			return -1;
		if(parse_char('.', &str))
			return -1;
		if(parse_uint8(&addr->ip[3], &str))
			return -1;
		if(*str == ':')
		{
			str++;
			if(parse_uint16(&addr->port, &str))
				return -1;
		}
		if(*str != '\0')
			return -1;

		addr->type = NETTYPE_IPV4;
	}

	return 0;
}

static void priv_net_close_socket(int sock)
{
#if defined(CONF_FAMILY_WINDOWS)
	closesocket(sock);
#else
	if(close(sock) != 0)
		dbg_msg("socket", "close failed: %d", errno);
#endif
}

static int priv_net_close_all_sockets(NETSOCKET sock)
{
	/* close down ipv4 */
	if(sock->ipv4sock >= 0)
	{
		priv_net_close_socket(sock->ipv4sock);
		sock->ipv4sock = -1;
		sock->type &= ~NETTYPE_IPV4;
	}

#if defined(CONF_WEBSOCKETS)
	/* close down websocket_ipv4 */
	if(sock->web_ipv4sock >= 0)
	{
		websocket_destroy(sock->web_ipv4sock);
		sock->web_ipv4sock = -1;
		sock->type &= ~NETTYPE_WEBSOCKET_IPV4;
	}
#endif

	/* close down ipv6 */
	if(sock->ipv6sock >= 0)
	{
		priv_net_close_socket(sock->ipv6sock);
		sock->ipv6sock = -1;
		sock->type &= ~NETTYPE_IPV6;
	}

	free(sock);
	return 0;
}

#if defined(CONF_FAMILY_WINDOWS)
std::string windows_format_system_message(unsigned long error)
{
	WCHAR *wide_message;
	const DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_MAX_WIDTH_MASK;
	if(FormatMessageW(flags, NULL, error, 0, (LPWSTR)&wide_message, 0, NULL) == 0)
		return "unknown error";

	std::optional<std::string> message = windows_wide_to_utf8(wide_message);
	LocalFree(wide_message);
	return message.value_or("(invalid UTF-16 in error message)");
}
#endif

static int priv_net_create_socket(int domain, int type, struct sockaddr *addr, int sockaddrlen)
{
	int sock, e;

	/* create socket */
	sock = socket(domain, type, 0);
	if(sock < 0)
	{
#if defined(CONF_FAMILY_WINDOWS)
		int error = WSAGetLastError();
		const std::string message = windows_format_system_message(error);
		dbg_msg("net", "failed to create socket with domain %d and type %d (%d '%s')", domain, type, error, message.c_str());
#else
		dbg_msg("net", "failed to create socket with domain %d and type %d (%d '%s')", domain, type, errno, strerror(errno));
#endif
		return -1;
	}

#if defined(CONF_FAMILY_UNIX)
	/* on tcp sockets set SO_REUSEADDR
		to fix port rebind on restart */
	if(domain == AF_INET && type == SOCK_STREAM)
	{
		int option = 1;
		if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) != 0)
			dbg_msg("socket", "Setting SO_REUSEADDR failed: %d", errno);
	}
#endif

	/* set to IPv6 only if that's what we are creating */
#if defined(IPV6_V6ONLY) /* windows sdk 6.1 and higher */
	if(domain == AF_INET6)
	{
		int ipv6only = 1;
		if(setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&ipv6only, sizeof(ipv6only)) != 0)
			dbg_msg("socket", "Setting V6ONLY failed: %d", errno);
	}
#endif

	/* bind the socket */
	e = bind(sock, addr, sockaddrlen);
	if(e != 0)
	{
#if defined(CONF_FAMILY_WINDOWS)
		int error = WSAGetLastError();
		const std::string message = windows_format_system_message(error);
		dbg_msg("net", "failed to bind socket with domain %d and type %d (%d '%s')", domain, type, error, message.c_str());
#else
		dbg_msg("net", "failed to bind socket with domain %d and type %d (%d '%s')", domain, type, errno, strerror(errno));
#endif
		priv_net_close_socket(sock);
		return -1;
	}

	/* return the newly created socket */
	return sock;
}

int net_socket_type(NETSOCKET sock)
{
	return sock->type;
}

NETSOCKET net_udp_create(NETADDR bindaddr)
{
	NETSOCKET sock = (NETSOCKET_INTERNAL *)malloc(sizeof(*sock));
	*sock = invalid_socket;

	if(bindaddr.type & NETTYPE_IPV4)
	{
		struct sockaddr_in addr;
		NETADDR tmpbindaddr = bindaddr;
		tmpbindaddr.type = NETTYPE_IPV4;
		netaddr_to_sockaddr_in(&tmpbindaddr, &addr);
		int socket = priv_net_create_socket(AF_INET, SOCK_DGRAM, (struct sockaddr *)&addr, sizeof(addr));
		if(socket >= 0)
		{
			sock->type |= NETTYPE_IPV4;
			sock->ipv4sock = socket;

			/* set broadcast */
			int broadcast = 1;
			if(setsockopt(socket, SOL_SOCKET, SO_BROADCAST, (const char *)&broadcast, sizeof(broadcast)) != 0)
			{
				dbg_msg("socket", "Setting BROADCAST on ipv4 failed: %d", net_errno());
			}

			{
				/* set DSCP/TOS */
				int iptos = 0x10 /* IPTOS_LOWDELAY */;
				if(setsockopt(socket, IPPROTO_IP, IP_TOS, (char *)&iptos, sizeof(iptos)) != 0)
				{
					dbg_msg("socket", "Setting TOS on ipv4 failed: %d", net_errno());
				}
			}
		}
	}

#if defined(CONF_WEBSOCKETS)
	if(bindaddr.type & NETTYPE_WEBSOCKET_IPV4)
	{
		char addr_str[NETADDR_MAXSTRSIZE];
		NETADDR tmpbindaddr = bindaddr;
		tmpbindaddr.type = NETTYPE_WEBSOCKET_IPV4;
		net_addr_str(&tmpbindaddr, addr_str, sizeof(addr_str), 0);
		int socket = websocket_create(addr_str, tmpbindaddr.port);
		if(socket >= 0)
		{
			sock->type |= NETTYPE_WEBSOCKET_IPV4;
			sock->web_ipv4sock = socket;
		}
	}
#endif

	if(bindaddr.type & NETTYPE_IPV6)
	{
		struct sockaddr_in6 addr;
		NETADDR tmpbindaddr = bindaddr;
		tmpbindaddr.type = NETTYPE_IPV6;
		netaddr_to_sockaddr_in6(&tmpbindaddr, &addr);
		int socket = priv_net_create_socket(AF_INET6, SOCK_DGRAM, (struct sockaddr *)&addr, sizeof(addr));
		if(socket >= 0)
		{
			sock->type |= NETTYPE_IPV6;
			sock->ipv6sock = socket;

			/* set broadcast */
			int broadcast = 1;
			if(setsockopt(socket, SOL_SOCKET, SO_BROADCAST, (const char *)&broadcast, sizeof(broadcast)) != 0)
			{
				dbg_msg("socket", "Setting BROADCAST on ipv6 failed: %d", net_errno());
			}

			// TODO: setting IP_TOS on ipv6 with setsockopt is not supported on Windows, see https://github.com/ddnet/ddnet/issues/7605
#if !defined(CONF_FAMILY_WINDOWS)
			{
				/* set DSCP/TOS */
				int iptos = 0x10 /* IPTOS_LOWDELAY */;
				if(setsockopt(socket, IPPROTO_IP, IP_TOS, (char *)&iptos, sizeof(iptos)) != 0)
				{
					dbg_msg("socket", "Setting TOS on ipv6 failed: %d", net_errno());
				}
			}
#endif
		}
	}

	if(sock->type == NETTYPE_INVALID)
	{
		free(sock);
		sock = nullptr;
	}
	else
	{
		net_set_non_blocking(sock);
		net_buffer_init(&sock->buffer);
	}

	return sock;
}

int net_udp_send(NETSOCKET sock, const NETADDR *addr, const void *data, int size)
{
	int d = -1;

	if(addr->type & NETTYPE_IPV4)
	{
		if(sock->ipv4sock >= 0)
		{
			struct sockaddr_in sa;
			if(addr->type & NETTYPE_LINK_BROADCAST)
			{
				mem_zero(&sa, sizeof(sa));
				sa.sin_port = htons(addr->port);
				sa.sin_family = AF_INET;
				sa.sin_addr.s_addr = INADDR_BROADCAST;
			}
			else
				netaddr_to_sockaddr_in(addr, &sa);

			d = sendto((int)sock->ipv4sock, (const char *)data, size, 0, (struct sockaddr *)&sa, sizeof(sa));
		}
		else
			dbg_msg("net", "can't send ipv4 traffic to this socket");
	}

#if defined(CONF_WEBSOCKETS)
	if(addr->type & NETTYPE_WEBSOCKET_IPV4)
	{
		if(sock->web_ipv4sock >= 0)
		{
			char addr_str[NETADDR_MAXSTRSIZE];
			str_format(addr_str, sizeof(addr_str), "%d.%d.%d.%d", addr->ip[0], addr->ip[1], addr->ip[2], addr->ip[3]);
			d = websocket_send(sock->web_ipv4sock, (const unsigned char *)data, size, addr_str, addr->port);
		}

		else
			dbg_msg("net", "can't send websocket_ipv4 traffic to this socket");
	}
#endif

	if(addr->type & NETTYPE_IPV6)
	{
		if(sock->ipv6sock >= 0)
		{
			struct sockaddr_in6 sa;
			if(addr->type & NETTYPE_LINK_BROADCAST)
			{
				mem_zero(&sa, sizeof(sa));
				sa.sin6_port = htons(addr->port);
				sa.sin6_family = AF_INET6;
				sa.sin6_addr.s6_addr[0] = 0xff; /* multicast */
				sa.sin6_addr.s6_addr[1] = 0x02; /* link local scope */
				sa.sin6_addr.s6_addr[15] = 1; /* all nodes */
			}
			else
				netaddr_to_sockaddr_in6(addr, &sa);

			d = sendto((int)sock->ipv6sock, (const char *)data, size, 0, (struct sockaddr *)&sa, sizeof(sa));
		}
		else
			dbg_msg("net", "can't send ipv6 traffic to this socket");
	}
	/*
	else
		dbg_msg("net", "can't send to network of type %d", addr->type);
		*/

	/*if(d < 0)
	{
		char addrstr[256];
		net_addr_str(addr, addrstr, sizeof(addrstr));

		dbg_msg("net", "sendto error (%d '%s')", errno, strerror(errno));
		dbg_msg("net", "\tsock = %d %x", sock, sock);
		dbg_msg("net", "\tsize = %d %x", size, size);
		dbg_msg("net", "\taddr = %s", addrstr);

	}*/
	network_stats.sent_bytes += size;
	network_stats.sent_packets++;
	return d;
}

void net_buffer_init(NETSOCKET_BUFFER *buffer)
{
#if defined(CONF_PLATFORM_LINUX)
	int i;
	buffer->pos = 0;
	buffer->size = 0;
	mem_zero(buffer->msgs, sizeof(buffer->msgs));
	mem_zero(buffer->iovecs, sizeof(buffer->iovecs));
	mem_zero(buffer->sockaddrs, sizeof(buffer->sockaddrs));
	for(i = 0; i < VLEN; ++i)
	{
		buffer->iovecs[i].iov_base = buffer->bufs[i];
		buffer->iovecs[i].iov_len = PACKETSIZE;
		buffer->msgs[i].msg_hdr.msg_iov = &(buffer->iovecs[i]);
		buffer->msgs[i].msg_hdr.msg_iovlen = 1;
		buffer->msgs[i].msg_hdr.msg_name = &(buffer->sockaddrs[i]);
		buffer->msgs[i].msg_hdr.msg_namelen = sizeof(buffer->sockaddrs[i]);
	}
#endif
}

void net_buffer_reinit(NETSOCKET_BUFFER *buffer)
{
#if defined(CONF_PLATFORM_LINUX)
	for(int i = 0; i < VLEN; i++)
	{
		buffer->msgs[i].msg_hdr.msg_namelen = sizeof(buffer->sockaddrs[i]);
	}
#endif
}

void net_buffer_simple(NETSOCKET_BUFFER *buffer, char **buf, int *size)
{
#if defined(CONF_PLATFORM_LINUX)
	*buf = buffer->bufs[0];
	*size = sizeof(buffer->bufs[0]);
#else
	*buf = buffer->buf;
	*size = sizeof(buffer->buf);
#endif
}

int net_udp_recv(NETSOCKET sock, NETADDR *addr, unsigned char **data)
{
	char sockaddrbuf[128];
	int bytes = 0;

#if defined(CONF_PLATFORM_LINUX)
	if(sock->ipv4sock >= 0)
	{
		if(sock->buffer.pos >= sock->buffer.size)
		{
			net_buffer_reinit(&sock->buffer);
			sock->buffer.size = recvmmsg(sock->ipv4sock, sock->buffer.msgs, VLEN, 0, NULL);
			sock->buffer.pos = 0;
		}
	}

	if(sock->ipv6sock >= 0)
	{
		if(sock->buffer.pos >= sock->buffer.size)
		{
			net_buffer_reinit(&sock->buffer);
			sock->buffer.size = recvmmsg(sock->ipv6sock, sock->buffer.msgs, VLEN, 0, NULL);
			sock->buffer.pos = 0;
		}
	}

	if(sock->buffer.pos < sock->buffer.size)
	{
		sockaddr_to_netaddr((struct sockaddr *)&(sock->buffer.sockaddrs[sock->buffer.pos]), addr);
		bytes = sock->buffer.msgs[sock->buffer.pos].msg_len;
		*data = (unsigned char *)sock->buffer.bufs[sock->buffer.pos];
		sock->buffer.pos++;
		network_stats.recv_bytes += bytes;
		network_stats.recv_packets++;
		return bytes;
	}
#else
	if(sock->ipv4sock >= 0)
	{
		socklen_t fromlen = sizeof(struct sockaddr_in);
		bytes = recvfrom(sock->ipv4sock, sock->buffer.buf, sizeof(sock->buffer.buf), 0, (struct sockaddr *)&sockaddrbuf, &fromlen);
		*data = (unsigned char *)sock->buffer.buf;
	}

	if(bytes <= 0 && sock->ipv6sock >= 0)
	{
		socklen_t fromlen = sizeof(struct sockaddr_in6);
		bytes = recvfrom(sock->ipv6sock, sock->buffer.buf, sizeof(sock->buffer.buf), 0, (struct sockaddr *)&sockaddrbuf, &fromlen);
		*data = (unsigned char *)sock->buffer.buf;
	}
#endif

#if defined(CONF_WEBSOCKETS)
	if(bytes <= 0 && sock->web_ipv4sock >= 0)
	{
		char *buf;
		int size;
		net_buffer_simple(&sock->buffer, &buf, &size);
		socklen_t fromlen = sizeof(struct sockaddr);
		struct sockaddr_in *sockaddrbuf_in = (struct sockaddr_in *)&sockaddrbuf;
		bytes = websocket_recv(sock->web_ipv4sock, (unsigned char *)buf, size, sockaddrbuf_in, fromlen);
		*data = (unsigned char *)buf;
		sockaddrbuf_in->sin_family = AF_WEBSOCKET_INET;
	}
#endif

	if(bytes > 0)
	{
		sockaddr_to_netaddr((struct sockaddr *)&sockaddrbuf, addr);
		network_stats.recv_bytes += bytes;
		network_stats.recv_packets++;
		return bytes;
	}
	else if(bytes == 0)
		return 0;
	return -1; /* error */
}

int net_udp_close(NETSOCKET sock)
{
	return priv_net_close_all_sockets(sock);
}

NETSOCKET net_tcp_create(NETADDR bindaddr)
{
	NETSOCKET sock = (NETSOCKET_INTERNAL *)malloc(sizeof(*sock));
	*sock = invalid_socket;

	if(bindaddr.type & NETTYPE_IPV4)
	{
		struct sockaddr_in addr;
		NETADDR tmpbindaddr = bindaddr;
		tmpbindaddr.type = NETTYPE_IPV4;
		netaddr_to_sockaddr_in(&tmpbindaddr, &addr);
		int socket4 = priv_net_create_socket(AF_INET, SOCK_STREAM, (struct sockaddr *)&addr, sizeof(addr));
		if(socket4 >= 0)
		{
			sock->type |= NETTYPE_IPV4;
			sock->ipv4sock = socket4;
		}
	}

	if(bindaddr.type & NETTYPE_IPV6)
	{
		struct sockaddr_in6 addr;
		NETADDR tmpbindaddr = bindaddr;
		tmpbindaddr.type = NETTYPE_IPV6;
		netaddr_to_sockaddr_in6(&tmpbindaddr, &addr);
		int socket6 = priv_net_create_socket(AF_INET6, SOCK_STREAM, (struct sockaddr *)&addr, sizeof(addr));
		if(socket6 >= 0)
		{
			sock->type |= NETTYPE_IPV6;
			sock->ipv6sock = socket6;
		}
	}

	if(sock->type == NETTYPE_INVALID)
	{
		free(sock);
		sock = nullptr;
	}

	return sock;
}

static int net_set_blocking_impl(NETSOCKET sock, bool blocking)
{
	unsigned long mode = blocking ? 0 : 1;
	const char *mode_str = blocking ? "blocking" : "non-blocking";
	int sockets[] = {sock->ipv4sock, sock->ipv6sock};
	const char *socket_str[] = {"ipv4", "ipv6"};

	for(size_t i = 0; i < std::size(sockets); ++i)
	{
		if(sockets[i] >= 0)
		{
#if defined(CONF_FAMILY_WINDOWS)
			int result = ioctlsocket(sockets[i], FIONBIO, (unsigned long *)&mode);
			if(result != NO_ERROR)
				dbg_msg("socket", "setting %s %s failed: %d", socket_str[i], mode_str, result);
#else
			if(ioctl(sockets[i], FIONBIO, (unsigned long *)&mode) == -1)
				dbg_msg("socket", "setting %s %s failed: %d", socket_str[i], mode_str, errno);
#endif
		}
	}

	return 0;
}

int net_set_non_blocking(NETSOCKET sock)
{
	return net_set_blocking_impl(sock, false);
}

int net_set_blocking(NETSOCKET sock)
{
	return net_set_blocking_impl(sock, true);
}

int net_tcp_listen(NETSOCKET sock, int backlog)
{
	int err = -1;
	if(sock->ipv4sock >= 0)
		err = listen(sock->ipv4sock, backlog);
	if(sock->ipv6sock >= 0)
		err = listen(sock->ipv6sock, backlog);
	return err;
}

int net_tcp_accept(NETSOCKET sock, NETSOCKET *new_sock, NETADDR *a)
{
	int s;
	socklen_t sockaddr_len;

	*new_sock = nullptr;

	if(sock->ipv4sock >= 0)
	{
		struct sockaddr_in addr;
		sockaddr_len = sizeof(addr);

		s = accept(sock->ipv4sock, (struct sockaddr *)&addr, &sockaddr_len);

		if(s != -1)
		{
			sockaddr_to_netaddr((const struct sockaddr *)&addr, a);

			*new_sock = (NETSOCKET_INTERNAL *)malloc(sizeof(**new_sock));
			**new_sock = invalid_socket;
			(*new_sock)->type = NETTYPE_IPV4;
			(*new_sock)->ipv4sock = s;
			return s;
		}
	}

	if(sock->ipv6sock >= 0)
	{
		struct sockaddr_in6 addr;
		sockaddr_len = sizeof(addr);

		s = accept(sock->ipv6sock, (struct sockaddr *)&addr, &sockaddr_len);

		if(s != -1)
		{
			*new_sock = (NETSOCKET_INTERNAL *)malloc(sizeof(**new_sock));
			**new_sock = invalid_socket;
			sockaddr_to_netaddr((const struct sockaddr *)&addr, a);
			(*new_sock)->type = NETTYPE_IPV6;
			(*new_sock)->ipv6sock = s;
			return s;
		}
	}

	return -1;
}

int net_tcp_connect(NETSOCKET sock, const NETADDR *a)
{
	if(a->type & NETTYPE_IPV4)
	{
		struct sockaddr_in addr;
		netaddr_to_sockaddr_in(a, &addr);
		if(sock->ipv4sock < 0)
			return -2;
		return connect(sock->ipv4sock, (struct sockaddr *)&addr, sizeof(addr));
	}

	if(a->type & NETTYPE_IPV6)
	{
		struct sockaddr_in6 addr;
		netaddr_to_sockaddr_in6(a, &addr);
		if(sock->ipv6sock < 0)
			return -2;
		return connect(sock->ipv6sock, (struct sockaddr *)&addr, sizeof(addr));
	}

	return -1;
}

int net_tcp_connect_non_blocking(NETSOCKET sock, NETADDR bindaddr)
{
	int res = 0;

	net_set_non_blocking(sock);
	res = net_tcp_connect(sock, &bindaddr);
	net_set_blocking(sock);

	return res;
}

int net_tcp_send(NETSOCKET sock, const void *data, int size)
{
	int bytes = -1;

	if(sock->ipv4sock >= 0)
		bytes = send((int)sock->ipv4sock, (const char *)data, size, 0);
	if(sock->ipv6sock >= 0)
		bytes = send((int)sock->ipv6sock, (const char *)data, size, 0);

	return bytes;
}

int net_tcp_recv(NETSOCKET sock, void *data, int maxsize)
{
	int bytes = -1;

	if(sock->ipv4sock >= 0)
		bytes = recv((int)sock->ipv4sock, (char *)data, maxsize, 0);
	if(sock->ipv6sock >= 0)
		bytes = recv((int)sock->ipv6sock, (char *)data, maxsize, 0);

	return bytes;
}

int net_tcp_close(NETSOCKET sock)
{
	return priv_net_close_all_sockets(sock);
}

int net_errno()
{
#if defined(CONF_FAMILY_WINDOWS)
	return WSAGetLastError();
#else
	return errno;
#endif
}

int net_would_block()
{
#if defined(CONF_FAMILY_WINDOWS)
	return net_errno() == WSAEWOULDBLOCK;
#else
	return net_errno() == EWOULDBLOCK;
#endif
}

void net_init()
{
#if defined(CONF_FAMILY_WINDOWS)
	WSADATA wsa_data;
	dbg_assert(WSAStartup(MAKEWORD(1, 1), &wsa_data) == 0, "network initialization failed.");
#endif
}

#if defined(CONF_FAMILY_UNIX)
UNIXSOCKET net_unix_create_unnamed()
{
	return socket(AF_UNIX, SOCK_DGRAM, 0);
}

int net_unix_send(UNIXSOCKET sock, UNIXSOCKETADDR *addr, void *data, int size)
{
	return sendto(sock, data, size, 0, (struct sockaddr *)addr, sizeof(struct sockaddr_un));
}

void net_unix_set_addr(UNIXSOCKETADDR *addr, const char *path)
{
	mem_zero(addr, sizeof(*addr));
	addr->sun_family = AF_UNIX;
	str_copy(addr->sun_path, path);
}

void net_unix_close(UNIXSOCKET sock)
{
	close(sock);
}
#endif

#if defined(CONF_FAMILY_WINDOWS)
static inline time_t filetime_to_unixtime(LPFILETIME filetime)
{
	time_t t;
	ULARGE_INTEGER li;
	li.LowPart = filetime->dwLowDateTime;
	li.HighPart = filetime->dwHighDateTime;

	li.QuadPart /= 10000000; // 100ns to 1s
	li.QuadPart -= 11644473600LL; // Windows epoch is in the past

	t = li.QuadPart;
	return t == (time_t)li.QuadPart ? t : (time_t)-1;
}
#endif

void fs_listdir(const char *dir, FS_LISTDIR_CALLBACK cb, int type, void *user)
{
#if defined(CONF_FAMILY_WINDOWS)
	char buffer[IO_MAX_PATH_LENGTH];
	str_format(buffer, sizeof(buffer), "%s/*", dir);
	const std::wstring wide_buffer = windows_utf8_to_wide(buffer);

	WIN32_FIND_DATAW finddata;
	HANDLE handle = FindFirstFileW(wide_buffer.c_str(), &finddata);
	if(handle == INVALID_HANDLE_VALUE)
		return;

	do
	{
		const std::optional<std::string> current_entry = windows_wide_to_utf8(finddata.cFileName);
		if(!current_entry.has_value())
		{
			log_error("filesystem", "ERROR: file/folder name containing invalid UTF-16 found in folder '%s'", dir);
			continue;
		}
		if(cb(current_entry.value().c_str(), (finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0, type, user))
			break;
	} while(FindNextFileW(handle, &finddata));

	FindClose(handle);
#else
	DIR *dir_handle = opendir(dir);
	if(dir_handle == nullptr)
		return;

	char buffer[IO_MAX_PATH_LENGTH];
	str_format(buffer, sizeof(buffer), "%s/", dir);
	size_t length = str_length(buffer);
	while(true)
	{
		struct dirent *entry = readdir(dir_handle);
		if(entry == nullptr)
			break;
		if(!str_utf8_check(entry->d_name))
		{
			log_error("filesystem", "ERROR: file/folder name containing invalid UTF-8 found in folder '%s'", dir);
			continue;
		}
		str_copy(buffer + length, entry->d_name, sizeof(buffer) - length);
		if(cb(entry->d_name, fs_is_dir(buffer), type, user))
			break;
	}

	closedir(dir_handle);
#endif
}

void fs_listdir_fileinfo(const char *dir, FS_LISTDIR_CALLBACK_FILEINFO cb, int type, void *user)
{
#if defined(CONF_FAMILY_WINDOWS)
	char buffer[IO_MAX_PATH_LENGTH];
	str_format(buffer, sizeof(buffer), "%s/*", dir);
	const std::wstring wide_buffer = windows_utf8_to_wide(buffer);

	WIN32_FIND_DATAW finddata;
	HANDLE handle = FindFirstFileW(wide_buffer.c_str(), &finddata);
	if(handle == INVALID_HANDLE_VALUE)
		return;

	do
	{
		const std::optional<std::string> current_entry = windows_wide_to_utf8(finddata.cFileName);
		if(!current_entry.has_value())
		{
			log_error("filesystem", "ERROR: file/folder name containing invalid UTF-16 found in folder '%s'", dir);
			continue;
		}

		CFsFileInfo info;
		info.m_pName = current_entry.value().c_str();
		info.m_TimeCreated = filetime_to_unixtime(&finddata.ftCreationTime);
		info.m_TimeModified = filetime_to_unixtime(&finddata.ftLastWriteTime);

		if(cb(&info, (finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0, type, user))
			break;
	} while(FindNextFileW(handle, &finddata));

	FindClose(handle);
#else
	DIR *dir_handle = opendir(dir);
	if(dir_handle == nullptr)
		return;

	char buffer[IO_MAX_PATH_LENGTH];
	str_format(buffer, sizeof(buffer), "%s/", dir);
	size_t length = str_length(buffer);

	while(true)
	{
		struct dirent *entry = readdir(dir_handle);
		if(entry == nullptr)
			break;
		if(!str_utf8_check(entry->d_name))
		{
			log_error("filesystem", "ERROR: file/folder name containing invalid UTF-8 found in folder '%s'", dir);
			continue;
		}
		str_copy(buffer + length, entry->d_name, sizeof(buffer) - length);
		time_t created = -1, modified = -1;
		fs_file_time(buffer, &created, &modified);

		CFsFileInfo info;
		info.m_pName = entry->d_name;
		info.m_TimeCreated = created;
		info.m_TimeModified = modified;

		if(cb(&info, fs_is_dir(buffer), type, user))
			break;
	}

	closedir(dir_handle);
#endif
}

int fs_storage_path(const char *appname, char *path, int max)
{
#if defined(CONF_FAMILY_WINDOWS)
	WCHAR *wide_home = _wgetenv(L"APPDATA");
	if(!wide_home)
	{
		path[0] = '\0';
		return -1;
	}
	const std::optional<std::string> home = windows_wide_to_utf8(wide_home);
	if(!home.has_value())
	{
		log_error("filesystem", "ERROR: the APPDATA environment variable contains invalid UTF-16");
		path[0] = '\0';
		return -1;
	}
	str_format(path, max, "%s/%s", home.value().c_str(), appname);
	return 0;
#else
	char *home = getenv("HOME");
	if(!home)
	{
		path[0] = '\0';
		return -1;
	}

	if(!str_utf8_check(home))
	{
		log_error("filesystem", "ERROR: the HOME environment variable contains invalid UTF-8");
		path[0] = '\0';
		return -1;
	}

#if defined(CONF_PLATFORM_HAIKU)
	str_format(path, max, "%s/config/settings/%s", home, appname);
#elif defined(CONF_PLATFORM_MACOS)
	str_format(path, max, "%s/Library/Application Support/%s", home, appname);
#else
	if(str_comp(appname, "Teeworlds") == 0)
	{
		// fallback for old directory for Teeworlds compatibility
		str_format(path, max, "%s/.%s", home, appname);
	}
	else
	{
		char *data_home = getenv("XDG_DATA_HOME");
		if(data_home)
		{
			if(!str_utf8_check(data_home))
			{
				log_error("filesystem", "ERROR: the XDG_DATA_HOME environment variable contains invalid UTF-8");
				path[0] = '\0';
				return -1;
			}
			str_format(path, max, "%s/%s", data_home, appname);
		}
		else
			str_format(path, max, "%s/.local/share/%s", home, appname);
	}
	for(int i = str_length(path) - str_length(appname); path[i]; i++)
		path[i] = tolower((unsigned char)path[i]);
#endif

	return 0;
#endif
}

int fs_makedir_rec_for(const char *path)
{
	char buffer[IO_MAX_PATH_LENGTH];
	str_copy(buffer, path);
	for(int index = 1; buffer[index] != '\0'; ++index)
	{
		// Do not try to create folder for drive letters on Windows,
		// as this is not necessary and may fail for system drives.
		if((buffer[index] == '/' || buffer[index] == '\\') && buffer[index + 1] != '\0' && buffer[index - 1] != ':')
		{
			buffer[index] = '\0';
			if(fs_makedir(buffer) < 0)
			{
				return -1;
			}
			buffer[index] = '/';
		}
	}
	return 0;
}

int fs_makedir(const char *path)
{
#if defined(CONF_FAMILY_WINDOWS)
	const std::wstring wide_path = windows_utf8_to_wide(path);
	if(CreateDirectoryW(wide_path.c_str(), NULL) != 0)
		return 0;
	if(GetLastError() == ERROR_ALREADY_EXISTS)
		return 0;
	return -1;
#else
#ifdef CONF_PLATFORM_HAIKU
	struct stat st;
	if(stat(path, &st) == 0)
		return 0;
#endif
	if(mkdir(path, 0755) == 0)
		return 0;
	if(errno == EEXIST)
		return 0;
	return -1;
#endif
}

int fs_removedir(const char *path)
{
#if defined(CONF_FAMILY_WINDOWS)
	const std::wstring wide_path = windows_utf8_to_wide(path);
	if(RemoveDirectoryW(wide_path.c_str()) != 0)
		return 0;
	return -1;
#else
	if(rmdir(path) == 0)
		return 0;
	return -1;
#endif
}

int fs_is_file(const char *path)
{
#if defined(CONF_FAMILY_WINDOWS)
	const std::wstring wide_path = windows_utf8_to_wide(path);
	DWORD attributes = GetFileAttributesW(wide_path.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
#else
	struct stat sb;
	if(stat(path, &sb) == -1)
		return 0;
	return S_ISREG(sb.st_mode) ? 1 : 0;
#endif
}

int fs_is_dir(const char *path)
{
#if defined(CONF_FAMILY_WINDOWS)
	const std::wstring wide_path = windows_utf8_to_wide(path);
	DWORD attributes = GetFileAttributesW(wide_path.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
#else
	struct stat sb;
	if(stat(path, &sb) == -1)
		return 0;
	return S_ISDIR(sb.st_mode) ? 1 : 0;
#endif
}

int fs_is_relative_path(const char *path)
{
#if defined(CONF_FAMILY_WINDOWS)
	const std::wstring wide_path = windows_utf8_to_wide(path);
	return PathIsRelativeW(wide_path.c_str()) ? 1 : 0;
#else
	return path[0] == '/' ? 0 : 1; // yes, it's that simple
#endif
}

int fs_chdir(const char *path)
{
#if defined(CONF_FAMILY_WINDOWS)
	const std::wstring wide_path = windows_utf8_to_wide(path);
	return SetCurrentDirectoryW(wide_path.c_str()) != 0 ? 0 : 1;
#else
	return chdir(path) ? 1 : 0;
#endif
}

char *fs_getcwd(char *buffer, int buffer_size)
{
#if defined(CONF_FAMILY_WINDOWS)
	const DWORD size_needed = GetCurrentDirectoryW(0, nullptr);
	std::wstring wide_current_dir(size_needed, L'0');
	dbg_assert(GetCurrentDirectoryW(size_needed, wide_current_dir.data()) == size_needed - 1, "GetCurrentDirectoryW failure");
	const std::optional<std::string> current_dir = windows_wide_to_utf8(wide_current_dir.c_str());
	if(!current_dir.has_value())
	{
		buffer[0] = '\0';
		return nullptr;
	}
	str_copy(buffer, current_dir.value().c_str(), buffer_size);
	return buffer;
#else
	char *result = getcwd(buffer, buffer_size);
	if(result == nullptr || !str_utf8_check(result))
	{
		buffer[0] = '\0';
		return nullptr;
	}
	return result;
#endif
}

const char *fs_filename(const char *path)
{
	for(const char *filename = path + str_length(path); filename >= path; --filename)
	{
		if(filename[0] == '/' || filename[0] == '\\')
			return filename + 1;
	}
	return path;
}

void fs_split_file_extension(const char *filename, char *name, size_t name_size, char *extension, size_t extension_size)
{
	dbg_assert(name != nullptr || extension != nullptr, "name or extension parameter required");
	dbg_assert(name == nullptr || name_size > 0, "name_size invalid");
	dbg_assert(extension == nullptr || extension_size > 0, "extension_size invalid");

	const char *last_dot = str_rchr(filename, '.');
	if(last_dot == nullptr || last_dot == filename)
	{
		if(extension != nullptr)
			extension[0] = '\0';
		if(name != nullptr)
			str_copy(name, filename, name_size);
	}
	else
	{
		if(extension != nullptr)
			str_copy(extension, last_dot + 1, extension_size);
		if(name != nullptr)
			str_truncate(name, name_size, filename, last_dot - filename);
	}
}

int fs_parent_dir(char *path)
{
	char *parent = 0;
	for(; *path; ++path)
	{
		if(*path == '/' || *path == '\\')
			parent = path;
	}

	if(parent)
	{
		*parent = 0;
		return 0;
	}
	return 1;
}

int fs_remove(const char *filename)
{
#if defined(CONF_FAMILY_WINDOWS)
	const std::wstring wide_filename = windows_utf8_to_wide(filename);
	return DeleteFileW(wide_filename.c_str()) == 0;
#else
	return unlink(filename) != 0;
#endif
}

int fs_rename(const char *oldname, const char *newname)
{
#if defined(CONF_FAMILY_WINDOWS)
	const std::wstring wide_oldname = windows_utf8_to_wide(oldname);
	const std::wstring wide_newname = windows_utf8_to_wide(newname);
	if(MoveFileExW(wide_oldname.c_str(), wide_newname.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH) == 0)
		return 1;
#else
	if(rename(oldname, newname) != 0)
		return 1;
#endif
	return 0;
}

int fs_file_time(const char *name, time_t *created, time_t *modified)
{
#if defined(CONF_FAMILY_WINDOWS)
	WIN32_FIND_DATAW finddata;
	const std::wstring wide_name = windows_utf8_to_wide(name);
	HANDLE handle = FindFirstFileW(wide_name.c_str(), &finddata);
	if(handle == INVALID_HANDLE_VALUE)
		return 1;

	*created = filetime_to_unixtime(&finddata.ftCreationTime);
	*modified = filetime_to_unixtime(&finddata.ftLastWriteTime);
	FindClose(handle);
#elif defined(CONF_FAMILY_UNIX)
	struct stat sb;
	if(stat(name, &sb))
		return 1;

	*created = sb.st_ctime;
	*modified = sb.st_mtime;
#else
#error not implemented
#endif

	return 0;
}

void swap_endian(void *data, unsigned elem_size, unsigned num)
{
	char *src = (char *)data;
	char *dst = src + (elem_size - 1);

	while(num)
	{
		unsigned n = elem_size >> 1;
		char tmp;
		while(n)
		{
			tmp = *src;
			*src = *dst;
			*dst = tmp;

			src++;
			dst--;
			n--;
		}

		src = src + (elem_size >> 1);
		dst = src + (elem_size - 1);
		num--;
	}
}

int net_socket_read_wait(NETSOCKET sock, int time)
{
	struct timeval tv;
	fd_set readfds;
	int sockid;

	tv.tv_sec = time / 1000000;
	tv.tv_usec = time % 1000000;
	sockid = 0;

	FD_ZERO(&readfds);
	if(sock->ipv4sock >= 0)
	{
		FD_SET(sock->ipv4sock, &readfds);
		sockid = sock->ipv4sock;
	}
	if(sock->ipv6sock >= 0)
	{
		FD_SET(sock->ipv6sock, &readfds);
		if(sock->ipv6sock > sockid)
			sockid = sock->ipv6sock;
	}
#if defined(CONF_WEBSOCKETS)
	if(sock->web_ipv4sock >= 0)
	{
		int maxfd = websocket_fd_set(sock->web_ipv4sock, &readfds);
		if(maxfd > sockid)
		{
			sockid = maxfd;
			FD_SET(sockid, &readfds);
		}
	}
#endif

	/* don't care about writefds and exceptfds */
	if(time < 0)
		select(sockid + 1, &readfds, NULL, NULL, NULL);
	else
		select(sockid + 1, &readfds, NULL, NULL, &tv);

	if(sock->ipv4sock >= 0 && FD_ISSET(sock->ipv4sock, &readfds))
		return 1;
#if defined(CONF_WEBSOCKETS)
	if(sock->web_ipv4sock >= 0 && FD_ISSET(sockid, &readfds))
		return 1;
#endif
	if(sock->ipv6sock >= 0 && FD_ISSET(sock->ipv6sock, &readfds))
		return 1;

	return 0;
}

int64_t time_timestamp()
{
	return time(0);
}

static struct tm *time_localtime_threadlocal(time_t *time_data)
{
#if defined(CONF_FAMILY_WINDOWS)
	// The result of localtime is thread-local on Windows
	// https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/localtime-localtime32-localtime64
	return localtime(time_data);
#else
	// Thread-local buffer for the result of localtime_r
	thread_local struct tm time_info_buf;
	return localtime_r(time_data, &time_info_buf);
#endif
}

int time_houroftheday()
{
	time_t time_data;
	time(&time_data);
	struct tm *time_info = time_localtime_threadlocal(&time_data);
	return time_info->tm_hour;
}

static bool time_iseasterday(time_t time_data, struct tm *time_info)
{
	// compute Easter day (Sunday) using https://en.wikipedia.org/w/index.php?title=Computus&oldid=890710285#Anonymous_Gregorian_algorithm
	int Y = time_info->tm_year + 1900;
	int a = Y % 19;
	int b = Y / 100;
	int c = Y % 100;
	int d = b / 4;
	int e = b % 4;
	int f = (b + 8) / 25;
	int g = (b - f + 1) / 3;
	int h = (19 * a + b - d - g + 15) % 30;
	int i = c / 4;
	int k = c % 4;
	int L = (32 + 2 * e + 2 * i - h - k) % 7;
	int m = (a + 11 * h + 22 * L) / 451;
	int month = (h + L - 7 * m + 114) / 31;
	int day = ((h + L - 7 * m + 114) % 31) + 1;

	// (now-1d ≤ easter ≤ now+2d) <=> (easter-2d ≤ now ≤ easter+1d) <=> (Good Friday ≤ now ≤ Easter Monday)
	for(int day_offset = -1; day_offset <= 2; day_offset++)
	{
		time_data = time_data + day_offset * 60 * 60 * 24;
		time_info = time_localtime_threadlocal(&time_data);
		if(time_info->tm_mon == month - 1 && time_info->tm_mday == day)
			return true;
	}
	return false;
}

ETimeSeason time_season()
{
	time_t time_data;
	time(&time_data);
	struct tm *time_info = time_localtime_threadlocal(&time_data);

	if((time_info->tm_mon == 11 && time_info->tm_mday == 31) || (time_info->tm_mon == 0 && time_info->tm_mday == 1))
	{
		return SEASON_NEWYEAR;
	}
	else if(time_info->tm_mon == 11 && time_info->tm_mday >= 24 && time_info->tm_mday <= 26)
	{
		return SEASON_XMAS;
	}
	else if((time_info->tm_mon == 9 && time_info->tm_mday == 31) || (time_info->tm_mon == 10 && time_info->tm_mday == 1))
	{
		return SEASON_HALLOWEEN;
	}
	else if(time_iseasterday(time_data, time_info))
	{
		return SEASON_EASTER;
	}

	switch(time_info->tm_mon)
	{
	case 11:
	case 0:
	case 1:
		return SEASON_WINTER;
	case 2:
	case 3:
	case 4:
		return SEASON_SPRING;
	case 5:
	case 6:
	case 7:
		return SEASON_SUMMER;
	case 8:
	case 9:
	case 10:
		return SEASON_AUTUMN;
	default:
		dbg_assert(false, "Invalid month");
		dbg_break();
	}
}

void str_append(char *dst, const char *src, int dst_size)
{
	int s = str_length(dst);
	int i = 0;
	while(s < dst_size)
	{
		dst[s] = src[i];
		if(!src[i]) /* check for null termination */
			break;
		s++;
		i++;
	}

	dst[dst_size - 1] = 0; /* assure null termination */
	str_utf8_fix_truncation(dst);
}

int str_copy(char *dst, const char *src, int dst_size)
{
	dst[0] = '\0';
	strncat(dst, src, dst_size - 1);
	return str_utf8_fix_truncation(dst);
}

void str_utf8_truncate(char *dst, int dst_size, const char *src, int truncation_len)
{
	int size = -1;
	const char *cursor = src;
	int pos = 0;
	while(pos <= truncation_len && cursor - src < dst_size && size != cursor - src)
	{
		size = cursor - src;
		if(str_utf8_decode(&cursor) == 0)
		{
			break;
		}
		pos++;
	}
	str_copy(dst, src, size + 1);
}

void str_truncate(char *dst, int dst_size, const char *src, int truncation_len)
{
	int size = dst_size;
	if(truncation_len < size)
	{
		size = truncation_len + 1;
	}
	str_copy(dst, src, size);
}

int str_length(const char *str)
{
	return (int)strlen(str);
}

int str_format_v(char *buffer, int buffer_size, const char *format, va_list args)
{
#if defined(CONF_FAMILY_WINDOWS)
	_vsprintf_p(buffer, buffer_size, format, args);
	buffer[buffer_size - 1] = 0; /* assure null termination */
#else
	vsnprintf(buffer, buffer_size, format, args);
	/* null termination is assured by definition of vsnprintf */
#endif
	return str_utf8_fix_truncation(buffer);
}

int str_format_int(char *buffer, size_t buffer_size, int value)
{
	buffer[0] = '\0'; // Fix false positive clang-analyzer-core.UndefinedBinaryOperatorResult when using result
	auto result = std::to_chars(buffer, buffer + buffer_size - 1, value);
	result.ptr[0] = '\0';
	return result.ptr - buffer;
}

#undef str_format
int str_format(char *buffer, int buffer_size, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	int length = str_format_v(buffer, buffer_size, format, args);
	va_end(args);
	return length;
}
#if !defined(CONF_DEBUG)
#define str_format str_format_opt
#endif

const char *str_trim_words(const char *str, int words)
{
	while(*str && str_isspace(*str))
		str++;
	while(words && *str)
	{
		if(str_isspace(*str) && !str_isspace(*(str + 1)))
			words--;
		str++;
	}
	return str;
}

bool str_has_cc(const char *str)
{
	unsigned char *s = (unsigned char *)str;
	while(*s)
	{
		if(*s < 32)
		{
			return true;
		}
		s++;
	}
	return false;
}

/* makes sure that the string only contains the characters between 32 and 255 */
void str_sanitize_cc(char *str_in)
{
	unsigned char *str = (unsigned char *)str_in;
	while(*str)
	{
		if(*str < 32)
			*str = ' ';
		str++;
	}
}

/* makes sure that the string only contains the characters between 32 and 255 + \r\n\t */
void str_sanitize(char *str_in)
{
	unsigned char *str = (unsigned char *)str_in;
	while(*str)
	{
		if(*str < 32 && !(*str == '\r') && !(*str == '\n') && !(*str == '\t'))
			*str = ' ';
		str++;
	}
}

void str_sanitize_filename(char *str_in)
{
	unsigned char *str = (unsigned char *)str_in;
	while(*str)
	{
		if(*str < 32 || *str == '\\' || *str == '/' || *str == '|' || *str == ':' || *str == '*' || *str == '?' || *str == '<' || *str == '>' || *str == '"')
			*str = ' ';
		str++;
	}
}

/* removes leading and trailing spaces and limits the use of multiple spaces */
void str_clean_whitespaces(char *str_in)
{
	char *read = str_in;
	char *write = str_in;

	/* skip initial whitespace */
	while(*read == ' ')
		read++;

	/* end of read string is detected in the loop */
	while(true)
	{
		/* skip whitespace */
		int found_whitespace = 0;
		for(; *read == ' '; read++)
			found_whitespace = 1;
		/* if not at the end of the string, put a found whitespace here */
		if(*read)
		{
			if(found_whitespace)
				*write++ = ' ';
			*write++ = *read++;
		}
		else
		{
			*write = 0;
			break;
		}
	}
}

char *str_skip_to_whitespace(char *str)
{
	while(*str && !str_isspace(*str))
		str++;
	return str;
}

const char *str_skip_to_whitespace_const(const char *str)
{
	while(*str && !str_isspace(*str))
		str++;
	return str;
}

char *str_skip_whitespaces(char *str)
{
	while(*str && str_isspace(*str))
		str++;
	return str;
}

const char *str_skip_whitespaces_const(const char *str)
{
	while(*str && str_isspace(*str))
		str++;
	return str;
}

/* case */
int str_comp_nocase(const char *a, const char *b)
{
#if defined(CONF_FAMILY_WINDOWS)
	return _stricmp(a, b);
#else
	return strcasecmp(a, b);
#endif
}

int str_comp_nocase_num(const char *a, const char *b, int num)
{
#if defined(CONF_FAMILY_WINDOWS)
	return _strnicmp(a, b, num);
#else
	return strncasecmp(a, b, num);
#endif
}

int str_comp(const char *a, const char *b)
{
	return strcmp(a, b);
}

int str_comp_num(const char *a, const char *b, int num)
{
	return strncmp(a, b, num);
}

int str_comp_filenames(const char *a, const char *b)
{
	int result;

	for(; *a && *b; ++a, ++b)
	{
		if(str_isnum(*a) && str_isnum(*b))
		{
			result = 0;
			do
			{
				if(!result)
					result = *a - *b;
				++a;
				++b;
			} while(str_isnum(*a) && str_isnum(*b));

			if(str_isnum(*a))
				return 1;
			else if(str_isnum(*b))
				return -1;
			else if(result || *a == '\0' || *b == '\0')
				return result;
		}

		result = tolower(*a) - tolower(*b);
		if(result)
			return result;
	}
	return *a - *b;
}

const char *str_startswith_nocase(const char *str, const char *prefix)
{
	int prefixl = str_length(prefix);
	if(str_comp_nocase_num(str, prefix, prefixl) == 0)
	{
		return str + prefixl;
	}
	else
	{
		return 0;
	}
}

const char *str_startswith(const char *str, const char *prefix)
{
	int prefixl = str_length(prefix);
	if(str_comp_num(str, prefix, prefixl) == 0)
	{
		return str + prefixl;
	}
	else
	{
		return 0;
	}
}

const char *str_endswith_nocase(const char *str, const char *suffix)
{
	int strl = str_length(str);
	int suffixl = str_length(suffix);
	const char *strsuffix;
	if(strl < suffixl)
	{
		return 0;
	}
	strsuffix = str + strl - suffixl;
	if(str_comp_nocase(strsuffix, suffix) == 0)
	{
		return strsuffix;
	}
	else
	{
		return 0;
	}
}

const char *str_endswith(const char *str, const char *suffix)
{
	int strl = str_length(str);
	int suffixl = str_length(suffix);
	const char *strsuffix;
	if(strl < suffixl)
	{
		return 0;
	}
	strsuffix = str + strl - suffixl;
	if(str_comp(strsuffix, suffix) == 0)
	{
		return strsuffix;
	}
	else
	{
		return 0;
	}
}

static int min3(int a, int b, int c)
{
	int min = a;
	if(b < min)
		min = b;
	if(c < min)
		min = c;
	return min;
}

int str_utf8_dist(const char *a, const char *b)
{
	int buf_len = 2 * (str_length(a) + 1 + str_length(b) + 1);
	int *buf = (int *)calloc(buf_len, sizeof(*buf));
	int result = str_utf8_dist_buffer(a, b, buf, buf_len);
	free(buf);
	return result;
}

static int str_to_utf32_unchecked(const char *str, int **out)
{
	int out_len = 0;
	while((**out = str_utf8_decode(&str)))
	{
		(*out)++;
		out_len++;
	}
	return out_len;
}

int str_utf32_dist_buffer(const int *a, int a_len, const int *b, int b_len, int *buf, int buf_len)
{
	int i, j;
	dbg_assert(buf_len >= (a_len + 1) + (b_len + 1), "buffer too small");
	if(a_len > b_len)
	{
		int tmp1 = a_len;
		const int *tmp2 = a;

		a_len = b_len;
		a = b;

		b_len = tmp1;
		b = tmp2;
	}
#define B(i, j) buf[((j)&1) * (a_len + 1) + (i)]
	for(i = 0; i <= a_len; i++)
	{
		B(i, 0) = i;
	}
	for(j = 1; j <= b_len; j++)
	{
		B(0, j) = j;
		for(i = 1; i <= a_len; i++)
		{
			int subst = (a[i - 1] != b[j - 1]);
			B(i, j) = min3(
				B(i - 1, j) + 1,
				B(i, j - 1) + 1,
				B(i - 1, j - 1) + subst);
		}
	}
	return B(a_len, b_len);
#undef B
}

int str_utf8_dist_buffer(const char *a_utf8, const char *b_utf8, int *buf, int buf_len)
{
	int a_utf8_len = str_length(a_utf8);
	int b_utf8_len = str_length(b_utf8);
	int *a, *b; // UTF-32
	int a_len, b_len; // UTF-32 length
	dbg_assert(buf_len >= 2 * (a_utf8_len + 1 + b_utf8_len + 1), "buffer too small");
	if(a_utf8_len > b_utf8_len)
	{
		const char *tmp2 = a_utf8;
		a_utf8 = b_utf8;
		b_utf8 = tmp2;
	}
	a = buf;
	a_len = str_to_utf32_unchecked(a_utf8, &buf);
	b = buf;
	b_len = str_to_utf32_unchecked(b_utf8, &buf);
	return str_utf32_dist_buffer(a, a_len, b, b_len, buf, buf_len - b_len - a_len);
}

const char *str_find_nocase(const char *haystack, const char *needle)
{
	while(*haystack) /* native implementation */
	{
		const char *a = haystack;
		const char *b = needle;
		while(*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b))
		{
			a++;
			b++;
		}
		if(!(*b))
			return haystack;
		haystack++;
	}

	return 0;
}

const char *str_find(const char *haystack, const char *needle)
{
	while(*haystack) /* native implementation */
	{
		const char *a = haystack;
		const char *b = needle;
		while(*a && *b && *a == *b)
		{
			a++;
			b++;
		}
		if(!(*b))
			return haystack;
		haystack++;
	}

	return 0;
}

bool str_delimiters_around_offset(const char *haystack, const char *delim, int offset, int *start, int *end)
{
	bool found = true;
	const char *search = haystack;
	const int delim_len = str_length(delim);
	*start = 0;
	while(str_find(search, delim))
	{
		const char *test = str_find(search, delim) + delim_len;
		int distance = test - haystack;
		if(distance > offset)
			break;

		*start = distance;
		search = test;
	}
	if(search == haystack)
		found = false;

	if(str_find(search, delim))
	{
		*end = str_find(search, delim) - haystack;
	}
	else
	{
		*end = str_length(haystack);
		found = false;
	}

	return found;
}

const char *str_rchr(const char *haystack, char needle)
{
	return strrchr(haystack, needle);
}

int str_countchr(const char *haystack, char needle)
{
	int count = 0;
	while(*haystack)
	{
		if(*haystack == needle)
			count++;
		haystack++;
	}
	return count;
}

void str_hex(char *dst, int dst_size, const void *data, int data_size)
{
	static const char hex[] = "0123456789ABCDEF";
	int data_index;
	int dst_index;
	for(data_index = 0, dst_index = 0; data_index < data_size && dst_index < dst_size - 3; data_index++)
	{
		dst[data_index * 3] = hex[((const unsigned char *)data)[data_index] >> 4];
		dst[data_index * 3 + 1] = hex[((const unsigned char *)data)[data_index] & 0xf];
		dst[data_index * 3 + 2] = ' ';
		dst_index += 3;
	}
	dst[dst_index] = '\0';
}

void str_hex_cstyle(char *dst, int dst_size, const void *data, int data_size, int bytes_per_line)
{
	static const char hex[] = "0123456789ABCDEF";
	int data_index;
	int dst_index;
	int remaining_bytes_per_line = bytes_per_line;
	for(data_index = 0, dst_index = 0; data_index < data_size && dst_index < dst_size - 6; data_index++)
	{
		--remaining_bytes_per_line;
		dst[data_index * 6] = '0';
		dst[data_index * 6 + 1] = 'x';
		dst[data_index * 6 + 2] = hex[((const unsigned char *)data)[data_index] >> 4];
		dst[data_index * 6 + 3] = hex[((const unsigned char *)data)[data_index] & 0xf];
		dst[data_index * 6 + 4] = ',';
		if(remaining_bytes_per_line == 0)
		{
			dst[data_index * 6 + 5] = '\n';
			remaining_bytes_per_line = bytes_per_line;
		}
		else
		{
			dst[data_index * 6 + 5] = ' ';
		}
		dst_index += 6;
	}
	dst[dst_index] = '\0';
	// Remove trailing comma and space/newline
	if(dst_index >= 1)
		dst[dst_index - 1] = '\0';
	if(dst_index >= 2)
		dst[dst_index - 2] = '\0';
}

static int hexval(char x)
{
	switch(x)
	{
	case '0': return 0;
	case '1': return 1;
	case '2': return 2;
	case '3': return 3;
	case '4': return 4;
	case '5': return 5;
	case '6': return 6;
	case '7': return 7;
	case '8': return 8;
	case '9': return 9;
	case 'a':
	case 'A': return 10;
	case 'b':
	case 'B': return 11;
	case 'c':
	case 'C': return 12;
	case 'd':
	case 'D': return 13;
	case 'e':
	case 'E': return 14;
	case 'f':
	case 'F': return 15;
	default: return -1;
	}
}

static int byteval(const char *hex, unsigned char *dst)
{
	int v1 = hexval(hex[0]);
	int v2 = hexval(hex[1]);

	if(v1 < 0 || v2 < 0)
		return 1;

	*dst = v1 * 16 + v2;
	return 0;
}

int str_hex_decode(void *dst, int dst_size, const char *src)
{
	unsigned char *cdst = (unsigned char *)dst;
	int slen = str_length(src);
	int len = slen / 2;
	int i;
	if(slen != dst_size * 2)
		return 2;

	for(i = 0; i < len && dst_size; i++, dst_size--)
	{
		if(byteval(src + i * 2, cdst++))
			return 1;
	}
	return 0;
}

void str_base64(char *dst, int dst_size, const void *data_raw, int data_size)
{
	static const char DIGITS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	const unsigned char *data = (const unsigned char *)data_raw;
	unsigned value = 0;
	int num_bits = 0;
	int i = 0;
	int o = 0;

	dst_size -= 1;
	dst[dst_size] = 0;
	while(true)
	{
		if(num_bits < 6 && i < data_size)
		{
			value = (value << 8) | data[i];
			num_bits += 8;
			i += 1;
		}
		if(o == dst_size)
		{
			return;
		}
		if(num_bits > 0)
		{
			unsigned padded;
			if(num_bits >= 6)
			{
				padded = (value >> (num_bits - 6)) & 0x3f;
			}
			else
			{
				padded = (value << (6 - num_bits)) & 0x3f;
			}
			dst[o] = DIGITS[padded];
			num_bits -= 6;
			o += 1;
		}
		else if(o % 4 != 0)
		{
			dst[o] = '=';
			o += 1;
		}
		else
		{
			dst[o] = 0;
			return;
		}
	}
}

static int base64_digit_value(char digit)
{
	if('A' <= digit && digit <= 'Z')
	{
		return digit - 'A';
	}
	else if('a' <= digit && digit <= 'z')
	{
		return digit - 'a' + 26;
	}
	else if('0' <= digit && digit <= '9')
	{
		return digit - '0' + 52;
	}
	else if(digit == '+')
	{
		return 62;
	}
	else if(digit == '/')
	{
		return 63;
	}
	return -1;
}

int str_base64_decode(void *dst_raw, int dst_size, const char *data)
{
	unsigned char *dst = (unsigned char *)dst_raw;
	int data_len = str_length(data);

	int i;
	int o = 0;

	if(data_len % 4 != 0)
	{
		return -3;
	}
	if(data_len / 4 * 3 > dst_size)
	{
		// Output buffer too small.
		return -2;
	}
	for(i = 0; i < data_len; i += 4)
	{
		int num_output_bytes = 3;
		char copy[4];
		int d[4];
		int value;
		int b;
		mem_copy(copy, data + i, sizeof(copy));
		if(i == data_len - 4)
		{
			if(copy[3] == '=')
			{
				copy[3] = 'A';
				num_output_bytes = 2;
				if(copy[2] == '=')
				{
					copy[2] = 'A';
					num_output_bytes = 1;
				}
			}
		}
		d[0] = base64_digit_value(copy[0]);
		d[1] = base64_digit_value(copy[1]);
		d[2] = base64_digit_value(copy[2]);
		d[3] = base64_digit_value(copy[3]);
		if(d[0] == -1 || d[1] == -1 || d[2] == -1 || d[3] == -1)
		{
			// Invalid digit.
			return -1;
		}
		value = (d[0] << 18) | (d[1] << 12) | (d[2] << 6) | d[3];
		for(b = 0; b < 3; b++)
		{
			unsigned char byte_value = (value >> (16 - 8 * b)) & 0xff;
			if(b < num_output_bytes)
			{
				dst[o] = byte_value;
				o += 1;
			}
			else
			{
				if(byte_value != 0)
				{
					// Padding not zeroed.
					return -2;
				}
			}
		}
	}
	return o;
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
void str_timestamp_ex(time_t time_data, char *buffer, int buffer_size, const char *format)
{
	struct tm *time_info = time_localtime_threadlocal(&time_data);
	strftime(buffer, buffer_size, format, time_info);
	buffer[buffer_size - 1] = 0; /* assure null termination */
}

void str_timestamp_format(char *buffer, int buffer_size, const char *format)
{
	time_t time_data;
	time(&time_data);
	str_timestamp_ex(time_data, buffer, buffer_size, format);
}

void str_timestamp(char *buffer, int buffer_size)
{
	str_timestamp_format(buffer, buffer_size, FORMAT_NOSPACE);
}

bool timestamp_from_str(const char *string, const char *format, time_t *timestamp)
{
	std::tm tm{};
	std::istringstream ss(string);
	ss >> std::get_time(&tm, format);
	if(ss.fail() || !ss.eof())
		return false;

	time_t result = mktime(&tm);
	if(result < 0)
		return false;

	*timestamp = result;
	return true;
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

int str_time(int64_t centisecs, int format, char *buffer, int buffer_size)
{
	const int sec = 100;
	const int min = 60 * sec;
	const int hour = 60 * min;
	const int day = 24 * hour;

	if(buffer_size <= 0)
		return -1;

	if(centisecs < 0)
		centisecs = 0;

	buffer[0] = 0;

	switch(format)
	{
	case TIME_DAYS:
		if(centisecs >= day)
			return str_format(buffer, buffer_size, "%" PRId64 "d %02" PRId64 ":%02" PRId64 ":%02" PRId64, centisecs / day,
				(centisecs % day) / hour, (centisecs % hour) / min, (centisecs % min) / sec);
		[[fallthrough]];
	case TIME_HOURS:
		if(centisecs >= hour)
			return str_format(buffer, buffer_size, "%02" PRId64 ":%02" PRId64 ":%02" PRId64, centisecs / hour,
				(centisecs % hour) / min, (centisecs % min) / sec);
		[[fallthrough]];
	case TIME_MINS:
		return str_format(buffer, buffer_size, "%02" PRId64 ":%02" PRId64, centisecs / min,
			(centisecs % min) / sec);
	case TIME_HOURS_CENTISECS:
		if(centisecs >= hour)
			return str_format(buffer, buffer_size, "%02" PRId64 ":%02" PRId64 ":%02" PRId64 ".%02" PRId64, centisecs / hour,
				(centisecs % hour) / min, (centisecs % min) / sec, centisecs % sec);
		[[fallthrough]];
	case TIME_MINS_CENTISECS:
		if(centisecs >= min)
			return str_format(buffer, buffer_size, "%02" PRId64 ":%02" PRId64 ".%02" PRId64, centisecs / min,
				(centisecs % min) / sec, centisecs % sec);
		[[fallthrough]];
	case TIME_SECS_CENTISECS:
		return str_format(buffer, buffer_size, "%02" PRId64 ".%02" PRId64, (centisecs % min) / sec, centisecs % sec);
	}

	return -1;
}

int str_time_float(float secs, int format, char *buffer, int buffer_size)
{
	return str_time(llroundf(secs * 1000) / 10, format, buffer, buffer_size);
}

void str_escape(char **dst, const char *src, const char *end)
{
	while(*src && *dst + 1 < end)
	{
		if(*src == '"' || *src == '\\') // escape \ and "
		{
			if(*dst + 2 < end)
				*(*dst)++ = '\\';
			else
				break;
		}
		*(*dst)++ = *src++;
	}
	**dst = 0;
}

void net_stats(NETSTATS *stats_inout)
{
	*stats_inout = network_stats;
}

int str_isspace(char c)
{
	return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

char str_uppercase(char c)
{
	if(c >= 'a' && c <= 'z')
		return 'A' + (c - 'a');
	return c;
}

bool str_isnum(char c)
{
	return c >= '0' && c <= '9';
}

int str_isallnum(const char *str)
{
	while(*str)
	{
		if(!str_isnum(*str))
			return 0;
		str++;
	}
	return 1;
}

int str_isallnum_hex(const char *str)
{
	while(*str)
	{
		if(!str_isnum(*str) && !(*str >= 'a' && *str <= 'f') && !(*str >= 'A' && *str <= 'F'))
			return 0;
		str++;
	}
	return 1;
}

int str_toint(const char *str)
{
	return str_toint_base(str, 10);
}

bool str_toint(const char *str, int *out)
{
	// returns true if conversion was successful
	char *end;
	int value = strtol(str, &end, 10);
	if(*end != '\0')
		return false;
	if(out != nullptr)
		*out = value;
	return true;
}

int str_toint_base(const char *str, int base)
{
	return strtol(str, nullptr, base);
}

unsigned long str_toulong_base(const char *str, int base)
{
	return strtoul(str, nullptr, base);
}

int64_t str_toint64_base(const char *str, int base)
{
	return strtoll(str, nullptr, base);
}

float str_tofloat(const char *str)
{
	return strtod(str, nullptr);
}

bool str_tofloat(const char *str, float *out)
{
	// returns true if conversion was successful
	char *end;
	float value = strtod(str, &end);
	if(*end != '\0')
		return false;
	if(out != nullptr)
		*out = value;
	return true;
}

int str_utf8_comp_nocase(const char *a, const char *b)
{
	int code_a;
	int code_b;

	while(*a && *b)
	{
		code_a = str_utf8_tolower(str_utf8_decode(&a));
		code_b = str_utf8_tolower(str_utf8_decode(&b));

		if(code_a != code_b)
			return code_a - code_b;
	}
	return (unsigned char)*a - (unsigned char)*b;
}

int str_utf8_comp_nocase_num(const char *a, const char *b, int num)
{
	int code_a;
	int code_b;
	const char *old_a = a;

	if(num <= 0)
		return 0;

	while(*a && *b)
	{
		code_a = str_utf8_tolower(str_utf8_decode(&a));
		code_b = str_utf8_tolower(str_utf8_decode(&b));

		if(code_a != code_b)
			return code_a - code_b;

		if(a - old_a >= num)
			return 0;
	}

	return (unsigned char)*a - (unsigned char)*b;
}

const char *str_utf8_find_nocase(const char *haystack, const char *needle, const char **end)
{
	while(*haystack) /* native implementation */
	{
		const char *a = haystack;
		const char *b = needle;
		const char *a_next = a;
		const char *b_next = b;
		while(*a && *b && str_utf8_tolower(str_utf8_decode(&a_next)) == str_utf8_tolower(str_utf8_decode(&b_next)))
		{
			a = a_next;
			b = b_next;
		}
		if(!(*b))
		{
			if(end != nullptr)
				*end = a_next;
			return haystack;
		}
		str_utf8_decode(&haystack);
	}

	if(end != nullptr)
		*end = nullptr;
	return nullptr;
}

int str_utf8_isspace(int code)
{
	return code <= 0x0020 || code == 0x0085 || code == 0x00A0 || code == 0x034F ||
	       code == 0x115F || code == 0x1160 || code == 0x1680 || code == 0x180E ||
	       (code >= 0x2000 && code <= 0x200F) || (code >= 0x2028 && code <= 0x202F) ||
	       (code >= 0x205F && code <= 0x2064) || (code >= 0x206A && code <= 0x206F) ||
	       code == 0x2800 || code == 0x3000 || code == 0x3164 ||
	       (code >= 0xFE00 && code <= 0xFE0F) || code == 0xFEFF || code == 0xFFA0 ||
	       (code >= 0xFFF9 && code <= 0xFFFC);
}

const char *str_utf8_skip_whitespaces(const char *str)
{
	const char *str_old;
	int code;

	while(*str)
	{
		str_old = str;
		code = str_utf8_decode(&str);

		// check if unicode is not empty
		if(!str_utf8_isspace(code))
		{
			return str_old;
		}
	}

	return str;
}

void str_utf8_trim_right(char *param)
{
	const char *str = param;
	char *end = 0;
	while(*str)
	{
		char *str_old = (char *)str;
		int code = str_utf8_decode(&str);

		// check if unicode is not empty
		if(!str_utf8_isspace(code))
		{
			end = 0;
		}
		else if(!end)
		{
			end = str_old;
		}
	}
	if(end)
	{
		*end = 0;
	}
}

int str_utf8_isstart(char c)
{
	if((c & 0xC0) == 0x80) /* 10xxxxxx */
		return 0;
	return 1;
}

int str_utf8_rewind(const char *str, int cursor)
{
	while(cursor)
	{
		cursor--;
		if(str_utf8_isstart(*(str + cursor)))
			break;
	}
	return cursor;
}

int str_utf8_fix_truncation(char *str)
{
	int len = str_length(str);
	if(len > 0)
	{
		int last_char_index = str_utf8_rewind(str, len);
		const char *last_char = str + last_char_index;
		// Fix truncated UTF-8.
		if(str_utf8_decode(&last_char) == -1)
		{
			str[last_char_index] = 0;
			return last_char_index;
		}
	}
	return len;
}

int str_utf8_forward(const char *str, int cursor)
{
	const char *ptr = str + cursor;
	if(str_utf8_decode(&ptr) == 0)
	{
		return cursor;
	}
	return ptr - str;
}

int str_utf8_encode(char *ptr, int chr)
{
	/* encode */
	if(chr <= 0x7F)
	{
		ptr[0] = (char)chr;
		return 1;
	}
	else if(chr <= 0x7FF)
	{
		ptr[0] = 0xC0 | ((chr >> 6) & 0x1F);
		ptr[1] = 0x80 | (chr & 0x3F);
		return 2;
	}
	else if(chr <= 0xFFFF)
	{
		ptr[0] = 0xE0 | ((chr >> 12) & 0x0F);
		ptr[1] = 0x80 | ((chr >> 6) & 0x3F);
		ptr[2] = 0x80 | (chr & 0x3F);
		return 3;
	}
	else if(chr <= 0x10FFFF)
	{
		ptr[0] = 0xF0 | ((chr >> 18) & 0x07);
		ptr[1] = 0x80 | ((chr >> 12) & 0x3F);
		ptr[2] = 0x80 | ((chr >> 6) & 0x3F);
		ptr[3] = 0x80 | (chr & 0x3F);
		return 4;
	}

	return 0;
}

static unsigned char str_byte_next(const char **ptr)
{
	unsigned char byte_value = **ptr;
	(*ptr)++;
	return byte_value;
}

static void str_byte_rewind(const char **ptr)
{
	(*ptr)--;
}

int str_utf8_decode(const char **ptr)
{
	// As per https://encoding.spec.whatwg.org/#utf-8-decoder.
	unsigned char utf8_lower_boundary = 0x80;
	unsigned char utf8_upper_boundary = 0xBF;
	int utf8_code_point = 0;
	int utf8_bytes_seen = 0;
	int utf8_bytes_needed = 0;
	while(true)
	{
		unsigned char byte_value = str_byte_next(ptr);
		if(utf8_bytes_needed == 0)
		{
			if(byte_value <= 0x7F)
			{
				return byte_value;
			}
			else if(0xC2 <= byte_value && byte_value <= 0xDF)
			{
				utf8_bytes_needed = 1;
				utf8_code_point = byte_value - 0xC0;
			}
			else if(0xE0 <= byte_value && byte_value <= 0xEF)
			{
				if(byte_value == 0xE0)
					utf8_lower_boundary = 0xA0;
				if(byte_value == 0xED)
					utf8_upper_boundary = 0x9F;
				utf8_bytes_needed = 2;
				utf8_code_point = byte_value - 0xE0;
			}
			else if(0xF0 <= byte_value && byte_value <= 0xF4)
			{
				if(byte_value == 0xF0)
					utf8_lower_boundary = 0x90;
				if(byte_value == 0xF4)
					utf8_upper_boundary = 0x8F;
				utf8_bytes_needed = 3;
				utf8_code_point = byte_value - 0xF0;
			}
			else
			{
				return -1; // Error.
			}
			utf8_code_point = utf8_code_point << (6 * utf8_bytes_needed);
			continue;
		}
		if(!(utf8_lower_boundary <= byte_value && byte_value <= utf8_upper_boundary))
		{
			// Resetting variables not necessary, will be done when
			// the function is called again.
			str_byte_rewind(ptr);
			return -1;
		}
		utf8_lower_boundary = 0x80;
		utf8_upper_boundary = 0xBF;
		utf8_bytes_seen += 1;
		utf8_code_point = utf8_code_point + ((byte_value - 0x80) << (6 * (utf8_bytes_needed - utf8_bytes_seen)));
		if(utf8_bytes_seen != utf8_bytes_needed)
		{
			continue;
		}
		// Resetting variables not necessary, see above.
		return utf8_code_point;
	}
}

int str_utf8_check(const char *str)
{
	int codepoint;
	while((codepoint = str_utf8_decode(&str)))
	{
		if(codepoint == -1)
		{
			return 0;
		}
	}
	return 1;
}

void str_utf8_copy_num(char *dst, const char *src, int dst_size, int num)
{
	int new_cursor;
	int cursor = 0;

	while(src[cursor] && num > 0)
	{
		new_cursor = str_utf8_forward(src, cursor);
		if(new_cursor >= dst_size) // reserve 1 byte for the null termination
			break;
		else
			cursor = new_cursor;
		--num;
	}

	str_copy(dst, src, cursor < dst_size ? cursor + 1 : dst_size);
}

void str_utf8_stats(const char *str, size_t max_size, size_t max_count, size_t *size, size_t *count)
{
	const char *cursor = str;
	*size = 0;
	*count = 0;
	while(*size < max_size && *count < max_count)
	{
		if(str_utf8_decode(&cursor) == 0)
		{
			break;
		}
		if((size_t)(cursor - str) >= max_size)
		{
			break;
		}
		*size = cursor - str;
		++(*count);
	}
}

size_t str_utf8_offset_bytes_to_chars(const char *str, size_t byte_offset)
{
	size_t char_offset = 0;
	size_t current_offset = 0;
	while(current_offset < byte_offset)
	{
		const size_t prev_byte_offset = current_offset;
		current_offset = str_utf8_forward(str, current_offset);
		if(current_offset == prev_byte_offset)
			break;
		char_offset++;
	}
	return char_offset;
}

size_t str_utf8_offset_chars_to_bytes(const char *str, size_t char_offset)
{
	size_t byte_offset = 0;
	for(size_t i = 0; i < char_offset; i++)
	{
		const size_t prev_byte_offset = byte_offset;
		byte_offset = str_utf8_forward(str, byte_offset);
		if(byte_offset == prev_byte_offset)
			break;
	}
	return byte_offset;
}

unsigned str_quickhash(const char *str)
{
	unsigned hash = 5381;
	for(; *str; str++)
		hash = ((hash << 5) + hash) + (*str); /* hash * 33 + c */
	return hash;
}

static const char *str_token_get(const char *str, const char *delim, int *length)
{
	size_t len = strspn(str, delim);
	if(len > 1)
		str++;
	else
		str += len;
	if(!*str)
		return NULL;

	*length = strcspn(str, delim);
	return str;
}

int str_in_list(const char *list, const char *delim, const char *needle)
{
	const char *tok = list;
	int len = 0, notfound = 1, needlelen = str_length(needle);

	while(notfound && (tok = str_token_get(tok, delim, &len)))
	{
		notfound = needlelen != len || str_comp_num(tok, needle, len);
		tok = tok + len;
	}

	return !notfound;
}

const char *str_next_token(const char *str, const char *delim, char *buffer, int buffer_size)
{
	int len = 0;
	const char *tok = str_token_get(str, delim, &len);
	if(len < 0 || tok == NULL)
	{
		buffer[0] = '\0';
		return NULL;
	}

	len = buffer_size > len ? len : buffer_size - 1;
	mem_copy(buffer, tok, len);
	buffer[len] = '\0';

	return tok + len;
}

static_assert(sizeof(unsigned) == 4, "unsigned must be 4 bytes in size");
static_assert(sizeof(unsigned) == sizeof(int), "unsigned and int must have the same size");

unsigned bytes_be_to_uint(const unsigned char *bytes)
{
	return ((bytes[0] & 0xffu) << 24u) | ((bytes[1] & 0xffu) << 16u) | ((bytes[2] & 0xffu) << 8u) | (bytes[3] & 0xffu);
}

void uint_to_bytes_be(unsigned char *bytes, unsigned value)
{
	bytes[0] = (value >> 24u) & 0xffu;
	bytes[1] = (value >> 16u) & 0xffu;
	bytes[2] = (value >> 8u) & 0xffu;
	bytes[3] = value & 0xffu;
}

int pid()
{
#if defined(CONF_FAMILY_WINDOWS)
	return _getpid();
#else
	return getpid();
#endif
}

void cmdline_fix(int *argc, const char ***argv)
{
#if defined(CONF_FAMILY_WINDOWS)
	int wide_argc = 0;
	WCHAR **wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
	dbg_assert(wide_argv != NULL, "CommandLineToArgvW failure");
	dbg_assert(wide_argc > 0, "Invalid argc value");

	int total_size = 0;

	for(int i = 0; i < wide_argc; i++)
	{
		int size = WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, NULL, 0, NULL, NULL);
		dbg_assert(size != 0, "WideCharToMultiByte failure");
		total_size += size;
	}

	char **new_argv = (char **)malloc((wide_argc + 1) * sizeof(*new_argv));
	new_argv[0] = (char *)malloc(total_size);
	mem_zero(new_argv[0], total_size);

	int remaining_size = total_size;
	for(int i = 0; i < wide_argc; i++)
	{
		int size = WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, new_argv[i], remaining_size, NULL, NULL);
		dbg_assert(size != 0, "WideCharToMultiByte failure");

		remaining_size -= size;
		new_argv[i + 1] = new_argv[i] + size;
	}

	LocalFree(wide_argv);
	new_argv[wide_argc] = 0;
	*argc = wide_argc;
	*argv = (const char **)new_argv;
#endif
}

void cmdline_free(int argc, const char **argv)
{
#if defined(CONF_FAMILY_WINDOWS)
	free((void *)*argv);
	free((char **)argv);
#endif
}

#if !defined(CONF_PLATFORM_ANDROID)
PROCESS shell_execute(const char *file, EShellExecuteWindowState window_state)
{
#if defined(CONF_FAMILY_WINDOWS)
	const std::wstring wide_file = windows_utf8_to_wide(file);

	SHELLEXECUTEINFOW info;
	mem_zero(&info, sizeof(SHELLEXECUTEINFOW));
	info.cbSize = sizeof(SHELLEXECUTEINFOW);
	info.lpVerb = L"open";
	info.lpFile = wide_file.c_str();
	switch(window_state)
	{
	case EShellExecuteWindowState::FOREGROUND:
		info.nShow = SW_SHOW;
		break;
	case EShellExecuteWindowState::BACKGROUND:
		info.nShow = SW_SHOWMINNOACTIVE;
		break;
	default:
		dbg_assert(false, "window_state invalid");
		dbg_break();
	}
	info.fMask = SEE_MASK_NOCLOSEPROCESS;
	// Save and restore the FPU control word because ShellExecute might change it
	fenv_t floating_point_environment;
	int fegetenv_result = fegetenv(&floating_point_environment);
	ShellExecuteExW(&info);
	if(fegetenv_result == 0)
		fesetenv(&floating_point_environment);
	return info.hProcess;
#elif defined(CONF_FAMILY_UNIX)
	char *argv[2];
	pid_t pid;
	argv[0] = (char *)file;
	argv[1] = NULL;
	pid = fork();
	if(pid == -1)
	{
		return 0;
	}
	if(pid == 0)
	{
		execvp(file, argv);
		_exit(1);
	}
	return pid;
#endif
}

int kill_process(PROCESS process)
{
#if defined(CONF_FAMILY_WINDOWS)
	BOOL success = TerminateProcess(process, 0);
	BOOL is_alive = is_process_alive(process);
	if(success || !is_alive)
	{
		CloseHandle(process);
		return true;
	}
	return false;
#elif defined(CONF_FAMILY_UNIX)
	if(!is_process_alive(process))
		return true;
	int status;
	kill(process, SIGTERM);
	return waitpid(process, &status, 0) != -1;
#endif
}

bool is_process_alive(PROCESS process)
{
	if(process == INVALID_PROCESS)
		return false;
#if defined(CONF_FAMILY_WINDOWS)
	DWORD exit_code;
	GetExitCodeProcess(process, &exit_code);
	return exit_code == STILL_ACTIVE;
#else
	return waitpid(process, nullptr, WNOHANG) == 0;
#endif
}

int open_link(const char *link)
{
#if defined(CONF_FAMILY_WINDOWS)
	const std::wstring wide_link = windows_utf8_to_wide(link);

	SHELLEXECUTEINFOW info;
	mem_zero(&info, sizeof(SHELLEXECUTEINFOW));
	info.cbSize = sizeof(SHELLEXECUTEINFOW);
	info.lpVerb = NULL; // NULL to use the default verb, as "open" may not be available
	info.lpFile = wide_link.c_str();
	info.nShow = SW_SHOWNORMAL;
	// The SEE_MASK_NOASYNC flag ensures that the ShellExecuteEx function
	// finishes its DDE conversation before it returns, so it's not necessary
	// to pump messages in the calling thread.
	// The SEE_MASK_FLAG_NO_UI flag suppresses error messages that would pop up
	// when the link cannot be opened, e.g. when a folder does not exist.
	// The SEE_MASK_ASYNCOK flag is not used. It would allow the call to
	// ShellExecuteEx to return earlier, but it also prevents us from doing
	// our own error handling, as the function would always return TRUE.
	info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
	// Save and restore the FPU control word because ShellExecute might change it
	fenv_t floating_point_environment;
	int fegetenv_result = fegetenv(&floating_point_environment);
	BOOL success = ShellExecuteExW(&info);
	if(fegetenv_result == 0)
		fesetenv(&floating_point_environment);
	return success;
#elif defined(CONF_PLATFORM_LINUX)
	const int pid = fork();
	if(pid == 0)
		execlp("xdg-open", "xdg-open", link, nullptr);
	return pid > 0;
#elif defined(CONF_FAMILY_UNIX)
	const int pid = fork();
	if(pid == 0)
		execlp("open", "open", link, nullptr);
	return pid > 0;
#endif
}

int open_file(const char *path)
{
#if defined(CONF_PLATFORM_MACOS)
	return open_link(path);
#else
	// Create a file link so the path can contain forward and
	// backward slashes. But the file link must be absolute.
	char buf[512];
	char workingDir[IO_MAX_PATH_LENGTH];
	if(fs_is_relative_path(path))
	{
		if(!fs_getcwd(workingDir, sizeof(workingDir)))
			return 0;
		str_append(workingDir, "/");
	}
	else
		workingDir[0] = '\0';
	str_format(buf, sizeof(buf), "file://%s%s", workingDir, path);
	return open_link(buf);
#endif
}
#endif // !defined(CONF_PLATFORM_ANDROID)

struct SECURE_RANDOM_DATA
{
	int initialized;
#if defined(CONF_FAMILY_WINDOWS)
	HCRYPTPROV provider;
#else
	IOHANDLE urandom;
#endif
};

static struct SECURE_RANDOM_DATA secure_random_data = {0};

int secure_random_init()
{
	if(secure_random_data.initialized)
	{
		return 0;
	}
#if defined(CONF_FAMILY_WINDOWS)
	if(CryptAcquireContext(&secure_random_data.provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
	{
		secure_random_data.initialized = 1;
		return 0;
	}
	else
	{
		return 1;
	}
#else
	secure_random_data.urandom = io_open("/dev/urandom", IOFLAG_READ);
	if(secure_random_data.urandom)
	{
		secure_random_data.initialized = 1;
		return 0;
	}
	else
	{
		return 1;
	}
#endif
}

int secure_random_uninit()
{
	if(!secure_random_data.initialized)
	{
		return 0;
	}
#if defined(CONF_FAMILY_WINDOWS)
	if(CryptReleaseContext(secure_random_data.provider, 0))
	{
		secure_random_data.initialized = 0;
		return 0;
	}
	else
	{
		return 1;
	}
#else
	if(!io_close(secure_random_data.urandom))
	{
		secure_random_data.initialized = 0;
		return 0;
	}
	else
	{
		return 1;
	}
#endif
}

void generate_password(char *buffer, unsigned length, const unsigned short *random, unsigned random_length)
{
	static const char VALUES[] = "ABCDEFGHKLMNPRSTUVWXYZabcdefghjkmnopqt23456789";
	static const size_t NUM_VALUES = sizeof(VALUES) - 1; // Disregard the '\0'.
	unsigned i;
	dbg_assert(length >= random_length * 2 + 1, "too small buffer");
	dbg_assert(NUM_VALUES * NUM_VALUES >= 2048, "need at least 2048 possibilities for 2-character sequences");

	buffer[random_length * 2] = 0;

	for(i = 0; i < random_length; i++)
	{
		unsigned short random_number = random[i] % 2048;
		buffer[2 * i + 0] = VALUES[random_number / NUM_VALUES];
		buffer[2 * i + 1] = VALUES[random_number % NUM_VALUES];
	}
}

#define MAX_PASSWORD_LENGTH 128

void secure_random_password(char *buffer, unsigned length, unsigned pw_length)
{
	unsigned short random[MAX_PASSWORD_LENGTH / 2];
	// With 6 characters, we get a password entropy of log(2048) * 6/2 = 33bit.
	dbg_assert(length >= pw_length + 1, "too small buffer");
	dbg_assert(pw_length >= 6, "too small password length");
	dbg_assert(pw_length % 2 == 0, "need an even password length");
	dbg_assert(pw_length <= MAX_PASSWORD_LENGTH, "too large password length");

	secure_random_fill(random, pw_length);

	generate_password(buffer, length, random, pw_length / 2);
}

#undef MAX_PASSWORD_LENGTH

void secure_random_fill(void *bytes, unsigned length)
{
	if(!secure_random_data.initialized)
	{
		dbg_msg("secure", "called secure_random_fill before secure_random_init");
		dbg_break();
	}
#if defined(CONF_FAMILY_WINDOWS)
	if(!CryptGenRandom(secure_random_data.provider, length, (unsigned char *)bytes))
	{
		const DWORD LastError = GetLastError();
		const std::string ErrorMsg = windows_format_system_message(LastError);
		dbg_msg("secure", "CryptGenRandom failed: %ld %s", LastError, ErrorMsg.c_str());
		dbg_break();
	}
#else
	if(length != io_read(secure_random_data.urandom, bytes, length))
	{
		dbg_msg("secure", "io_read returned with a short read");
		dbg_break();
	}
#endif
}

int secure_rand()
{
	unsigned int i;
	secure_random_fill(&i, sizeof(i));
	return (int)(i % RAND_MAX);
}

// From https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2.
static unsigned int find_next_power_of_two_minus_one(unsigned int n)
{
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 4;
	n |= n >> 16;
	return n;
}

int secure_rand_below(int below)
{
	unsigned int mask = find_next_power_of_two_minus_one(below);
	dbg_assert(below > 0, "below must be positive");
	while(true)
	{
		unsigned int n;
		secure_random_fill(&n, sizeof(n));
		n &= mask;
		if((int)n < below)
		{
			return n;
		}
	}
}

bool os_version_str(char *version, size_t length)
{
#if defined(CONF_FAMILY_WINDOWS)
	const WCHAR *module_path = L"kernel32.dll";
	DWORD handle;
	DWORD size = GetFileVersionInfoSizeW(module_path, &handle);
	if(!size)
	{
		return false;
	}
	void *data = malloc(size);
	if(!GetFileVersionInfoW(module_path, handle, size, data))
	{
		free(data);
		return false;
	}
	VS_FIXEDFILEINFO *fileinfo;
	UINT unused;
	if(!VerQueryValueW(data, L"\\", (void **)&fileinfo, &unused))
	{
		free(data);
		return false;
	}
	str_format(version, length, "Windows %hu.%hu.%hu.%hu",
		HIWORD(fileinfo->dwProductVersionMS),
		LOWORD(fileinfo->dwProductVersionMS),
		HIWORD(fileinfo->dwProductVersionLS),
		LOWORD(fileinfo->dwProductVersionLS));
	free(data);
	return true;
#else
	struct utsname u;
	if(uname(&u))
	{
		return false;
	}
	char extra[128];
	extra[0] = 0;

	do
	{
		IOHANDLE os_release = io_open("/etc/os-release", IOFLAG_READ);
		char buf[4096];
		int read;
		int offset;
		char *newline;
		if(!os_release)
		{
			break;
		}
		read = io_read(os_release, buf, sizeof(buf) - 1);
		io_close(os_release);
		buf[read] = 0;
		if(str_startswith(buf, "PRETTY_NAME="))
		{
			offset = 0;
		}
		else
		{
			const char *found = str_find(buf, "\nPRETTY_NAME=");
			if(!found)
			{
				break;
			}
			offset = found - buf + 1;
		}
		newline = (char *)str_find(buf + offset, "\n");
		if(newline)
		{
			*newline = 0;
		}
		str_format(extra, sizeof(extra), "; %s", buf + offset + 12);
	} while(false);

	str_format(version, length, "%s %s (%s, %s)%s", u.sysname, u.release, u.machine, u.version, extra);
	return true;
#endif
}

void os_locale_str(char *locale, size_t length)
{
#if defined(CONF_FAMILY_WINDOWS)
	wchar_t wide_buffer[LOCALE_NAME_MAX_LENGTH];
	dbg_assert(GetUserDefaultLocaleName(wide_buffer, std::size(wide_buffer)) > 0, "GetUserDefaultLocaleName failure");

	const std::optional<std::string> buffer = windows_wide_to_utf8(wide_buffer);
	dbg_assert(buffer.has_value(), "GetUserDefaultLocaleName returned invalid UTF-16");
	str_copy(locale, buffer.value().c_str(), length);
#elif defined(CONF_PLATFORM_MACOS)
	CFLocaleRef locale_ref = CFLocaleCopyCurrent();
	CFStringRef locale_identifier_ref = static_cast<CFStringRef>(CFLocaleGetValue(locale_ref, kCFLocaleIdentifier));

	// Count number of UTF16 codepoints, +1 for zero-termination.
	// Assume maximum possible length for encoding as UTF-8.
	CFIndex locale_identifier_size = (UTF8_BYTE_LENGTH * CFStringGetLength(locale_identifier_ref) + 1) * sizeof(char);
	char *locale_identifier = (char *)malloc(locale_identifier_size);
	dbg_assert(CFStringGetCString(locale_identifier_ref, locale_identifier, locale_identifier_size, kCFStringEncodingUTF8), "CFStringGetCString failure");

	str_copy(locale, locale_identifier, length);

	free(locale_identifier);
	CFRelease(locale_ref);
#else
	static const char *ENV_VARIABLES[] = {
		"LC_ALL",
		"LC_MESSAGES",
		"LANG",
	};

	locale[0] = '\0';
	for(const char *env_variable : ENV_VARIABLES)
	{
		const char *env_value = getenv(env_variable);
		if(env_value)
		{
			str_copy(locale, env_value, length);
			break;
		}
	}
#endif

	// Ensure RFC 3066 format:
	// - use hyphens instead of underscores
	// - truncate locale string after first non-standard letter
	for(int i = 0; i < str_length(locale); ++i)
	{
		if(locale[i] == '_')
		{
			locale[i] = '-';
		}
		else if(locale[i] != '-' && !(locale[i] >= 'a' && locale[i] <= 'z') && !(locale[i] >= 'A' && locale[i] <= 'Z') && !(str_isnum(locale[i])))
		{
			locale[i] = '\0';
			break;
		}
	}

	// Use default if we could not determine the locale,
	// i.e. if only the C or POSIX locale is available.
	if(locale[0] == '\0' || str_comp(locale, "C") == 0 || str_comp(locale, "POSIX") == 0)
		str_copy(locale, "en-US", length);
}

#if defined(CONF_EXCEPTION_HANDLING)
#if defined(CONF_FAMILY_WINDOWS)
static HMODULE exception_handling_module = nullptr;
#endif

void init_exception_handler()
{
#if defined(CONF_FAMILY_WINDOWS)
	const char *module_name = "exchndl.dll";
	exception_handling_module = LoadLibraryA(module_name);
	if(exception_handling_module == nullptr)
	{
		const DWORD LastError = GetLastError();
		const std::string ErrorMsg = windows_format_system_message(LastError);
		dbg_msg("exception_handling", "failed to load exception handling library '%s' (error %ld %s)", module_name, LastError, ErrorMsg.c_str());
	}
#else
#error exception handling not implemented
#endif
}

void set_exception_handler_log_file(const char *log_file_path)
{
#if defined(CONF_FAMILY_WINDOWS)
	if(exception_handling_module != nullptr)
	{
		const std::wstring wide_log_file_path = windows_utf8_to_wide(log_file_path);
		// Intentional
#ifdef __MINGW32__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
		const char *function_name = "ExcHndlSetLogFileNameW";
		auto exception_log_file_path_func = (BOOL(APIENTRY *)(const WCHAR *))(GetProcAddress(exception_handling_module, function_name));
#ifdef __MINGW32__
#pragma GCC diagnostic pop
#endif
		if(exception_log_file_path_func == nullptr)
		{
			const DWORD LastError = GetLastError();
			const std::string ErrorMsg = windows_format_system_message(LastError);
			dbg_msg("exception_handling", "could not find function '%s' in exception handling library (error %ld %s)", function_name, LastError, ErrorMsg.c_str());
		}
		else
			exception_log_file_path_func(wide_log_file_path.c_str());
	}
#else
#error exception handling not implemented
#endif
}
#endif

std::chrono::nanoseconds time_get_nanoseconds()
{
	return std::chrono::nanoseconds(time_get_impl());
}

int net_socket_read_wait(NETSOCKET sock, std::chrono::nanoseconds nanoseconds)
{
	using namespace std::chrono_literals;
	return ::net_socket_read_wait(sock, (nanoseconds / std::chrono::nanoseconds(1us).count()).count());
}

#if defined(CONF_FAMILY_WINDOWS)
std::wstring windows_utf8_to_wide(const char *str)
{
	const int orig_length = str_length(str);
	if(orig_length == 0)
		return L"";
	const int size_needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, orig_length, nullptr, 0);
	dbg_assert(size_needed > 0, "Invalid UTF-8 passed to windows_utf8_to_wide");
	std::wstring wide_string(size_needed, L'\0');
	dbg_assert(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, orig_length, wide_string.data(), size_needed) == size_needed, "MultiByteToWideChar failure");
	return wide_string;
}

std::optional<std::string> windows_wide_to_utf8(const wchar_t *wide_str)
{
	const int orig_length = wcslen(wide_str);
	if(orig_length == 0)
		return "";
	const int size_needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_str, orig_length, nullptr, 0, nullptr, nullptr);
	if(size_needed == 0)
		return {};
	std::string string(size_needed, '\0');
	dbg_assert(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_str, orig_length, string.data(), size_needed, nullptr, nullptr) == size_needed, "WideCharToMultiByte failure");
	return string;
}

// See https://learn.microsoft.com/en-us/windows/win32/learnwin32/initializing-the-com-library
CWindowsComLifecycle::CWindowsComLifecycle(bool HasWindow)
{
	HRESULT result = CoInitializeEx(NULL, (HasWindow ? COINIT_APARTMENTTHREADED : COINIT_MULTITHREADED) | COINIT_DISABLE_OLE1DDE);
	dbg_assert(result != S_FALSE, "COM library already initialized on this thread");
	dbg_assert(result == S_OK, "COM library initialization failed");
}
CWindowsComLifecycle::~CWindowsComLifecycle()
{
	CoUninitialize();
}

static void windows_print_error(const char *system, const char *prefix, HRESULT error)
{
	const std::string message = windows_format_system_message(error);
	dbg_msg(system, "%s: %s", prefix, message.c_str());
}

static std::wstring filename_from_path(const std::wstring &path)
{
	const size_t pos = path.find_last_of(L"/\\");
	return pos == std::wstring::npos ? path : path.substr(pos + 1);
}

bool shell_register_protocol(const char *protocol_name, const char *executable, bool *updated)
{
	const std::wstring protocol_name_wide = windows_utf8_to_wide(protocol_name);
	const std::wstring executable_wide = windows_utf8_to_wide(executable);

	// Open registry key for protocol associations of the current user
	HKEY handle_subkey_classes;
	const LRESULT result_subkey_classes = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Classes", 0, KEY_ALL_ACCESS, &handle_subkey_classes);
	if(result_subkey_classes != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_protocol", "Error opening registry key", result_subkey_classes);
		return false;
	}

	// Create the protocol key
	HKEY handle_subkey_protocol;
	const LRESULT result_subkey_protocol = RegCreateKeyExW(handle_subkey_classes, protocol_name_wide.c_str(), 0, NULL, 0, KEY_ALL_ACCESS, NULL, &handle_subkey_protocol, NULL);
	RegCloseKey(handle_subkey_classes);
	if(result_subkey_protocol != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_protocol", "Error creating registry key", result_subkey_protocol);
		return false;
	}

	// Set the default value for the key, which specifies the name of the display name of the protocol
	const std::wstring value_protocol = L"URL:" + protocol_name_wide + L" Protocol";
	const LRESULT result_value_protocol = RegSetValueExW(handle_subkey_protocol, L"", 0, REG_SZ, (BYTE *)value_protocol.c_str(), (value_protocol.length() + 1) * sizeof(wchar_t));
	if(result_value_protocol != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_protocol", "Error setting registry value", result_value_protocol);
		RegCloseKey(handle_subkey_protocol);
		return false;
	}

	// Set the "URL Protocol" value, to specify that this key describes a URL protocol
	const LRESULT result_value_empty = RegSetValueEx(handle_subkey_protocol, L"URL Protocol", 0, REG_SZ, (BYTE *)L"", sizeof(wchar_t));
	if(result_value_empty != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_protocol", "Error setting registry value", result_value_empty);
		RegCloseKey(handle_subkey_protocol);
		return false;
	}

	// Create the "DefaultIcon" subkey
	HKEY handle_subkey_icon;
	const LRESULT result_subkey_icon = RegCreateKeyExW(handle_subkey_protocol, L"DefaultIcon", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &handle_subkey_icon, NULL);
	if(result_subkey_icon != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_protocol", "Error creating registry key", result_subkey_icon);
		RegCloseKey(handle_subkey_protocol);
		return false;
	}

	// Set the default value for the key, which specifies the icon associated with the protocol
	const std::wstring value_icon = L"\"" + executable_wide + L"\",0";
	const LRESULT result_value_icon = RegSetValueExW(handle_subkey_icon, L"", 0, REG_SZ, (BYTE *)value_icon.c_str(), (value_icon.length() + 1) * sizeof(wchar_t));
	RegCloseKey(handle_subkey_icon);
	if(result_value_icon != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_protocol", "Error setting registry value", result_value_icon);
		RegCloseKey(handle_subkey_protocol);
		return false;
	}

	// Create the "shell\open\command" subkeys
	HKEY handle_subkey_shell_open_command;
	const LRESULT result_subkey_shell_open_command = RegCreateKeyExW(handle_subkey_protocol, L"shell\\open\\command", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &handle_subkey_shell_open_command, NULL);
	RegCloseKey(handle_subkey_protocol);
	if(result_subkey_shell_open_command != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_protocol", "Error creating registry key", result_subkey_shell_open_command);
		return false;
	}

	// Get the previous default value for the key, so we can determine if it changed
	wchar_t old_value_executable[MAX_PATH + 16];
	DWORD old_size_executable = sizeof(old_value_executable);
	const LRESULT result_old_value_executable = RegGetValueW(handle_subkey_shell_open_command, NULL, L"", RRF_RT_REG_SZ, NULL, (BYTE *)old_value_executable, &old_size_executable);
	const std::wstring value_executable = L"\"" + executable_wide + L"\" \"%1\"";
	if(result_old_value_executable != ERROR_SUCCESS || wcscmp(old_value_executable, value_executable.c_str()) != 0)
	{
		// Set the default value for the key, which specifies the executable command associated with the protocol
		const LRESULT result_value_executable = RegSetValueExW(handle_subkey_shell_open_command, L"", 0, REG_SZ, (BYTE *)value_executable.c_str(), (value_executable.length() + 1) * sizeof(wchar_t));
		RegCloseKey(handle_subkey_shell_open_command);
		if(result_value_executable != ERROR_SUCCESS)
		{
			windows_print_error("shell_register_protocol", "Error setting registry value", result_value_executable);
			return false;
		}

		*updated = true;
	}
	else
	{
		RegCloseKey(handle_subkey_shell_open_command);
	}

	return true;
}

bool shell_register_extension(const char *extension, const char *description, const char *executable_name, const char *executable, bool *updated)
{
	const std::wstring extension_wide = windows_utf8_to_wide(extension);
	const std::wstring executable_name_wide = windows_utf8_to_wide(executable_name);
	const std::wstring description_wide = executable_name_wide + L" " + windows_utf8_to_wide(description);
	const std::wstring program_id_wide = executable_name_wide + extension_wide;
	const std::wstring executable_wide = windows_utf8_to_wide(executable);

	// Open registry key for file associations of the current user
	HKEY handle_subkey_classes;
	const LRESULT result_subkey_classes = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Classes", 0, KEY_ALL_ACCESS, &handle_subkey_classes);
	if(result_subkey_classes != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_extension", "Error opening registry key", result_subkey_classes);
		return false;
	}

	// Create the program ID key
	HKEY handle_subkey_program_id;
	const LRESULT result_subkey_program_id = RegCreateKeyExW(handle_subkey_classes, program_id_wide.c_str(), 0, NULL, 0, KEY_ALL_ACCESS, NULL, &handle_subkey_program_id, NULL);
	if(result_subkey_program_id != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_extension", "Error creating registry key", result_subkey_program_id);
		RegCloseKey(handle_subkey_classes);
		return false;
	}

	// Set the default value for the key, which specifies the file type description for legacy applications
	const LRESULT result_description_default = RegSetValueExW(handle_subkey_program_id, L"", 0, REG_SZ, (BYTE *)description_wide.c_str(), (description_wide.length() + 1) * sizeof(wchar_t));
	if(result_description_default != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_extension", "Error setting registry value", result_description_default);
		RegCloseKey(handle_subkey_program_id);
		RegCloseKey(handle_subkey_classes);
		return false;
	}

	// Set the "FriendlyTypeName" value, which specifies the file type description for modern applications
	const LRESULT result_description_friendly = RegSetValueExW(handle_subkey_program_id, L"FriendlyTypeName", 0, REG_SZ, (BYTE *)description_wide.c_str(), (description_wide.length() + 1) * sizeof(wchar_t));
	if(result_description_friendly != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_extension", "Error setting registry value", result_description_friendly);
		RegCloseKey(handle_subkey_program_id);
		RegCloseKey(handle_subkey_classes);
		return false;
	}

	// Create the "DefaultIcon" subkey
	HKEY handle_subkey_icon;
	const LRESULT result_subkey_icon = RegCreateKeyExW(handle_subkey_program_id, L"DefaultIcon", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &handle_subkey_icon, NULL);
	if(result_subkey_icon != ERROR_SUCCESS)
	{
		windows_print_error("register_protocol", "Error creating registry key", result_subkey_icon);
		RegCloseKey(handle_subkey_program_id);
		RegCloseKey(handle_subkey_classes);
		return false;
	}

	// Set the default value for the key, which specifies the icon associated with the program ID
	const std::wstring value_icon = L"\"" + executable_wide + L"\",0";
	const LRESULT result_value_icon = RegSetValueExW(handle_subkey_icon, L"", 0, REG_SZ, (BYTE *)value_icon.c_str(), (value_icon.length() + 1) * sizeof(wchar_t));
	RegCloseKey(handle_subkey_icon);
	if(result_value_icon != ERROR_SUCCESS)
	{
		windows_print_error("register_protocol", "Error setting registry value", result_value_icon);
		RegCloseKey(handle_subkey_program_id);
		RegCloseKey(handle_subkey_classes);
		return false;
	}

	// Create the "shell\open\command" subkeys
	HKEY handle_subkey_shell_open_command;
	const LRESULT result_subkey_shell_open_command = RegCreateKeyExW(handle_subkey_program_id, L"shell\\open\\command", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &handle_subkey_shell_open_command, NULL);
	RegCloseKey(handle_subkey_program_id);
	if(result_subkey_shell_open_command != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_extension", "Error creating registry key", result_subkey_shell_open_command);
		RegCloseKey(handle_subkey_classes);
		return false;
	}

	// Get the previous default value for the key, so we can determine if it changed
	wchar_t old_value_executable[MAX_PATH + 16];
	DWORD old_size_executable = sizeof(old_value_executable);
	const LRESULT result_old_value_executable = RegGetValueW(handle_subkey_shell_open_command, NULL, L"", RRF_RT_REG_SZ, NULL, (BYTE *)old_value_executable, &old_size_executable);
	const std::wstring value_executable = L"\"" + executable_wide + L"\" \"%1\"";
	if(result_old_value_executable != ERROR_SUCCESS || wcscmp(old_value_executable, value_executable.c_str()) != 0)
	{
		// Set the default value for the key, which specifies the executable command associated with the application
		const LRESULT result_value_executable = RegSetValueExW(handle_subkey_shell_open_command, L"", 0, REG_SZ, (BYTE *)value_executable.c_str(), (value_executable.length() + 1) * sizeof(wchar_t));
		RegCloseKey(handle_subkey_shell_open_command);
		if(result_value_executable != ERROR_SUCCESS)
		{
			windows_print_error("shell_register_extension", "Error setting registry value", result_value_executable);
			RegCloseKey(handle_subkey_classes);
			return false;
		}

		*updated = true;
	}

	// Create the file extension key
	HKEY handle_subkey_extension;
	const LRESULT result_subkey_extension = RegCreateKeyExW(handle_subkey_classes, extension_wide.c_str(), 0, NULL, 0, KEY_ALL_ACCESS, NULL, &handle_subkey_extension, NULL);
	RegCloseKey(handle_subkey_classes);
	if(result_subkey_extension != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_extension", "Error creating registry key", result_subkey_extension);
		return false;
	}

	// Get the previous default value for the key, so we can determine if it changed
	wchar_t old_value_application[128];
	DWORD old_size_application = sizeof(old_value_application);
	const LRESULT result_old_value_application = RegGetValueW(handle_subkey_extension, NULL, L"", RRF_RT_REG_SZ, NULL, (BYTE *)old_value_application, &old_size_application);
	if(result_old_value_application != ERROR_SUCCESS || wcscmp(old_value_application, program_id_wide.c_str()) != 0)
	{
		// Set the default value for the key, which associates the file extension with the program ID
		const LRESULT result_value_application = RegSetValueExW(handle_subkey_extension, L"", 0, REG_SZ, (BYTE *)program_id_wide.c_str(), (program_id_wide.length() + 1) * sizeof(wchar_t));
		RegCloseKey(handle_subkey_extension);
		if(result_value_application != ERROR_SUCCESS)
		{
			windows_print_error("shell_register_extension", "Error setting registry value", result_value_application);
			return false;
		}

		*updated = true;
	}
	else
	{
		RegCloseKey(handle_subkey_extension);
	}

	return true;
}

bool shell_register_application(const char *name, const char *executable, bool *updated)
{
	const std::wstring name_wide = windows_utf8_to_wide(name);
	const std::wstring executable_filename = filename_from_path(windows_utf8_to_wide(executable));

	// Open registry key for application registrations
	HKEY handle_subkey_applications;
	const LRESULT result_subkey_applications = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Classes\\Applications", 0, KEY_ALL_ACCESS, &handle_subkey_applications);
	if(result_subkey_applications != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_application", "Error opening registry key", result_subkey_applications);
		return false;
	}

	// Create the program key
	HKEY handle_subkey_program;
	const LRESULT result_subkey_program = RegCreateKeyExW(handle_subkey_applications, executable_filename.c_str(), 0, NULL, 0, KEY_ALL_ACCESS, NULL, &handle_subkey_program, NULL);
	RegCloseKey(handle_subkey_applications);
	if(result_subkey_program != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_application", "Error creating registry key", result_subkey_program);
		return false;
	}

	// Get the previous default value for the key, so we can determine if it changed
	wchar_t old_value_executable[MAX_PATH];
	DWORD old_size_executable = sizeof(old_value_executable);
	const LRESULT result_old_value_executable = RegGetValueW(handle_subkey_program, NULL, L"FriendlyAppName", RRF_RT_REG_SZ, NULL, (BYTE *)old_value_executable, &old_size_executable);
	if(result_old_value_executable != ERROR_SUCCESS || wcscmp(old_value_executable, name_wide.c_str()) != 0)
	{
		// Set the "FriendlyAppName" value, which specifies the displayed name of the application
		const LRESULT result_program_name = RegSetValueExW(handle_subkey_program, L"FriendlyAppName", 0, REG_SZ, (BYTE *)name_wide.c_str(), (name_wide.length() + 1) * sizeof(wchar_t));
		RegCloseKey(handle_subkey_program);
		if(result_program_name != ERROR_SUCCESS)
		{
			windows_print_error("shell_register_application", "Error setting registry value", result_program_name);
			return false;
		}

		*updated = true;
	}
	else
	{
		RegCloseKey(handle_subkey_program);
	}

	return true;
}

bool shell_unregister_class(const char *shell_class, bool *updated)
{
	const std::wstring class_wide = windows_utf8_to_wide(shell_class);

	// Open registry key for protocol and file associations of the current user
	HKEY handle_subkey_classes;
	const LRESULT result_subkey_classes = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Classes", 0, KEY_ALL_ACCESS, &handle_subkey_classes);
	if(result_subkey_classes != ERROR_SUCCESS)
	{
		windows_print_error("shell_unregister_class", "Error opening registry key", result_subkey_classes);
		return false;
	}

	// Delete the registry keys for the shell class (protocol or program ID)
	LRESULT result_delete = RegDeleteTreeW(handle_subkey_classes, class_wide.c_str());
	RegCloseKey(handle_subkey_classes);
	if(result_delete == ERROR_SUCCESS)
	{
		*updated = true;
	}
	else if(result_delete != ERROR_FILE_NOT_FOUND)
	{
		windows_print_error("shell_unregister_class", "Error deleting registry key", result_delete);
		return false;
	}

	return true;
}

bool shell_unregister_application(const char *executable, bool *updated)
{
	const std::wstring executable_filename = filename_from_path(windows_utf8_to_wide(executable));

	// Open registry key for application registrations
	HKEY handle_subkey_applications;
	const LRESULT result_subkey_applications = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Classes\\Applications", 0, KEY_ALL_ACCESS, &handle_subkey_applications);
	if(result_subkey_applications != ERROR_SUCCESS)
	{
		windows_print_error("shell_unregister_application", "Error opening registry key", result_subkey_applications);
		return false;
	}

	// Delete the registry keys for the application description
	LRESULT result_delete = RegDeleteTreeW(handle_subkey_applications, executable_filename.c_str());
	RegCloseKey(handle_subkey_applications);
	if(result_delete == ERROR_SUCCESS)
	{
		*updated = true;
	}
	else if(result_delete != ERROR_FILE_NOT_FOUND)
	{
		windows_print_error("shell_unregister_application", "Error deleting registry key", result_delete);
		return false;
	}

	return true;
}

void shell_update()
{
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
}
#endif

size_t std::hash<NETADDR>::operator()(const NETADDR &Addr) const noexcept
{
	return std::hash<std::string_view>{}(std::string_view((const char *)&Addr, sizeof(Addr)));
}
