/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "io.h"

#include "dbg.h"
#include "detect.h"
#include "fs.h"
#include "mem.h"
#include "windows.h"

#include <cstdio>
#include <cstdlib>

#if defined(CONF_FAMILY_WINDOWS)
#include <io.h> // _get_osfhandle
#include <windows.h> // FlushFileBuffers
#else
#include <unistd.h> // fsync
#endif

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
		dbg_assert_failed("logic error");
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
		dbg_assert_failed("Invalid flags: %d", flags);
	}
	return fopen(filename, open_mode);
#endif
}

unsigned io_read(IOHANDLE io, void *buffer, unsigned size)
{
	return fread(buffer, 1, size, (FILE *)io);
}

bool io_read_all(IOHANDLE io, void **result, unsigned *result_len)
{
	// Loading files larger than 1 GiB into memory is not supported.
	constexpr int64_t MAX_FILE_SIZE = (int64_t)1024 * 1024 * 1024;

	int64_t real_len = io_length(io);
	if(real_len > MAX_FILE_SIZE)
	{
		*result = nullptr;
		*result_len = 0;
		return false;
	}

	int64_t len = real_len < 0 ? 1024 : real_len; // use default initial size if we couldn't get the length
	char *buffer = (char *)malloc(len + 1);
	int64_t read = io_read(io, buffer, len + 1); // +1 to check if the file size is larger than expected
	if(read < len)
	{
		buffer = (char *)realloc(buffer, read + 1);
		len = read;
	}
	else if(read > len)
	{
		int64_t cap = 2 * read;
		if(cap > MAX_FILE_SIZE)
		{
			free(buffer);
			*result = nullptr;
			*result_len = 0;
			return false;
		}
		len = read;
		buffer = (char *)realloc(buffer, cap);
		while((read = io_read(io, buffer + len, cap - len)) != 0)
		{
			len += read;
			if(len == cap)
			{
				cap *= 2;
				if(cap > MAX_FILE_SIZE)
				{
					free(buffer);
					*result = nullptr;
					*result_len = 0;
					return false;
				}
				buffer = (char *)realloc(buffer, cap);
			}
		}
		buffer = (char *)realloc(buffer, len + 1);
	}
	buffer[len] = 0;
	*result = buffer;
	*result_len = len;
	return true;
}

char *io_read_all_str(IOHANDLE io)
{
	void *buffer;
	unsigned len;
	if(!io_read_all(io, &buffer, &len))
	{
		return nullptr;
	}
	if(mem_has_null(buffer, len))
	{
		free(buffer);
		return nullptr;
	}
	return (char *)buffer;
}

int io_skip(IOHANDLE io, int64_t size)
{
	return io_seek(io, size, IOSEEK_CUR);
}

int io_seek(IOHANDLE io, int64_t offset, ESeekOrigin origin)
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
		dbg_assert_failed("Invalid origin: %d", origin);
	}
#if defined(CONF_FAMILY_WINDOWS)
	return _fseeki64((FILE *)io, offset, real_origin);
#else
	return fseeko((FILE *)io, offset, real_origin);
#endif
}

int64_t io_tell(IOHANDLE io)
{
#if defined(CONF_FAMILY_WINDOWS)
	return _ftelli64((FILE *)io);
#else
	return ftello((FILE *)io);
#endif
}

int64_t io_length(IOHANDLE io)
{
	if(io_seek(io, 0, IOSEEK_END) != 0)
	{
		return -1;
	}
	const int64_t length = io_tell(io);
	if(io_seek(io, 0, IOSEEK_START) != 0)
	{
		return -1;
	}
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
	char path[IO_MAX_PATH_LENGTH];
	if(fs_executable_path(path, sizeof(path)) != 0)
	{
		return nullptr;
	}
	return io_open(path, IOFLAG_READ);
}
