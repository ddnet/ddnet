#include <base/fs.h>
#include <base/log.h>
#include <base/system.h>
#include <base/types.h>
#include <base/windows.h>

#include <cerrno>
#include <cstring>

#if defined(CONF_FAMILY_UNIX)
#include <sys/stat.h>
#include <unistd.h>

#elif defined(CONF_FAMILY_WINDOWS)
#include <io.h>
#include <windows.h>

#include <string>
#else
#error NOT IMPLEMENTED
#endif

int fs_makedir(const char *path)
{
#if defined(CONF_FAMILY_WINDOWS)
	const std::wstring wide_path = windows_utf8_to_wide(path);
	if(CreateDirectoryW(wide_path.c_str(), nullptr) != 0)
	{
		return 0;
	}
	const DWORD error = GetLastError();
	if(error == ERROR_ALREADY_EXISTS)
	{
		return 0;
	}
	log_error("filesystem", "Failed to create folder '%s' (%ld '%s')", path, error, windows_format_system_message(error).c_str());
	return -1;
#else
#if defined(CONF_PLATFORM_HAIKU)
	if(fs_is_dir(path))
	{
		return 0;
	}
#endif
	if(mkdir(path, 0755) == 0 || errno == EEXIST)
	{
		return 0;
	}
	log_error("filesystem", "Failed to create folder '%s' (%d '%s')", path, errno, strerror(errno));
	return -1;
#endif
}

int fs_removedir(const char *path)
{
#if defined(CONF_FAMILY_WINDOWS)
	const std::wstring wide_path = windows_utf8_to_wide(path);
	if(RemoveDirectoryW(wide_path.c_str()) != 0)
	{
		return 0;
	}
	const DWORD error = GetLastError();
	if(error == ERROR_FILE_NOT_FOUND)
	{
		return 0;
	}
	log_error("filesystem", "Failed to remove folder '%s' (%ld '%s')", path, error, windows_format_system_message(error).c_str());
	return -1;
#else
	if(rmdir(path) == 0 || errno == ENOENT)
	{
		return 0;
	}
	log_error("filesystem", "Failed to remove folder '%s' (%d '%s')", path, errno, strerror(errno));
	return -1;
#endif
}
