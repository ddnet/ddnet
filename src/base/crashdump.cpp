#include "detect.h"

#if defined(CONF_CRASHDUMP)
#if !defined(CONF_FAMILY_WINDOWS)
#error crash dumping not implemented
#else

#include "log.h"
#include "system.h"

#include <windows.h>

static const char *CRASHDUMP_LIB = "exchndl.dll";
static const char *CRASHDUMP_FN = "ExcHndlSetLogFileNameW";
void crashdump_init_if_available(const char *log_file_path)
{
	HMODULE pCrashdumpLib = LoadLibraryA(CRASHDUMP_LIB);
	if(pCrashdumpLib == nullptr)
	{
		const DWORD LastError = GetLastError();
		const std::string ErrorMsg = windows_format_system_message(LastError);
		log_error("crashdump", "failed to load crashdump library '%s' (error %ld %s)", CRASHDUMP_LIB, LastError, ErrorMsg.c_str());
		return;
	}
	const std::wstring wide_log_file_path = windows_utf8_to_wide(log_file_path);
	// Intentional
#ifdef __MINGW32__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
	auto exception_log_file_path_func = (BOOL(APIENTRY *)(const WCHAR *))(GetProcAddress(pCrashdumpLib, CRASHDUMP_FN));
#ifdef __MINGW32__
#pragma GCC diagnostic pop
#endif
	if(exception_log_file_path_func == nullptr)
	{
		const DWORD LastError = GetLastError();
		const std::string ErrorMsg = windows_format_system_message(LastError);
		log_error("exception_handling", "could not find function '%s' in exception handling library (error %ld %s)", CRASHDUMP_FN, LastError, ErrorMsg.c_str());
		return;
	}

	exception_log_file_path_func(wide_log_file_path.c_str());
}
#endif
#else
void crashdump_init_if_available(const char *log_file_path)
{
	(void)log_file_path;
}
#endif
