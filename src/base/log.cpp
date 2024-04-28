#include "logger.h"

#include "color.h"
#include "system.h"

#include <atomic>
#include <cstdio>
#include <memory>

#if defined(CONF_FAMILY_WINDOWS)
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#if defined(CONF_PLATFORM_ANDROID)
#include <android/log.h>
#endif

std::atomic<ILogger *> global_logger = nullptr;
thread_local ILogger *scope_logger = nullptr;
thread_local bool in_logger = false;

void log_set_global_logger(ILogger *logger)
{
	ILogger *null = nullptr;
	if(!global_logger.compare_exchange_strong(null, logger, std::memory_order_acq_rel))
	{
		dbg_assert(false, "global logger has already been set and can only be set once");
	}
	atexit(log_global_logger_finish);
}

void log_global_logger_finish()
{
	ILogger *logger = global_logger.load(std::memory_order_acquire);
	if(logger)
		logger->GlobalFinish();
}

void log_set_global_logger_default()
{
	std::unique_ptr<ILogger> logger;
#if defined(CONF_PLATFORM_ANDROID)
	logger = log_logger_android();
#else
	logger = log_logger_stdout();
#endif
	if(logger)
	{
		log_set_global_logger(logger.release());
	}
}

ILogger *log_get_scope_logger()
{
	if(!scope_logger)
	{
		scope_logger = global_logger.load(std::memory_order_acquire);
	}
	return scope_logger;
}

void log_set_scope_logger(ILogger *logger)
{
	scope_logger = logger;
	if(!scope_logger)
	{
		scope_logger = global_logger.load(std::memory_order_acquire);
	}
}

// Separate declaration, as attributes are not allowed on function definitions
void log_log_impl(LEVEL level, bool have_color, LOG_COLOR color, const char *sys, const char *fmt, va_list args)
	GNUC_ATTRIBUTE((format(printf, 5, 0)));

void log_log_impl(LEVEL level, bool have_color, LOG_COLOR color, const char *sys, const char *fmt, va_list args)
{
	// Make sure we're not logging recursively.
	if(in_logger)
	{
		return;
	}
	in_logger = true;
	if(!scope_logger)
	{
		scope_logger = global_logger.load(std::memory_order_acquire);
	}
	if(!scope_logger)
	{
		in_logger = false;
		return;
	}

	CLogMessage Msg;
	Msg.m_Level = level;
	Msg.m_HaveColor = have_color;
	Msg.m_Color = color;
	str_timestamp_format(Msg.m_aTimestamp, sizeof(Msg.m_aTimestamp), FORMAT_SPACE);
	Msg.m_TimestampLength = str_length(Msg.m_aTimestamp);
	str_copy(Msg.m_aSystem, sys);
	Msg.m_SystemLength = str_length(Msg.m_aSystem);

	// TODO: Add level?
	str_format(Msg.m_aLine, sizeof(Msg.m_aLine), "%s %c %s: ", Msg.m_aTimestamp, "EWIDT"[level], Msg.m_aSystem);
	Msg.m_LineMessageOffset = str_length(Msg.m_aLine);

	char *pMessage = Msg.m_aLine + Msg.m_LineMessageOffset;
	int MessageSize = sizeof(Msg.m_aLine) - Msg.m_LineMessageOffset;
	str_format_v(pMessage, MessageSize, fmt, args);
	Msg.m_LineLength = str_length(Msg.m_aLine);
	scope_logger->Log(&Msg);
	in_logger = false;
}

void log_log_v(LEVEL level, const char *sys, const char *fmt, va_list args)
{
	log_log_impl(level, false, LOG_COLOR{0, 0, 0}, sys, fmt, args);
}

void log_log(LEVEL level, const char *sys, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_log_impl(level, false, LOG_COLOR{0, 0, 0}, sys, fmt, args);
	va_end(args);
}

void log_log_color_v(LEVEL level, LOG_COLOR color, const char *sys, const char *fmt, va_list args)
{
	log_log_impl(level, true, color, sys, fmt, args);
}

void log_log_color(LEVEL level, LOG_COLOR color, const char *sys, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_log_impl(level, true, color, sys, fmt, args);
	va_end(args);
}

bool CLogFilter::Filters(const CLogMessage *pMessage)
{
	return pMessage->m_Level > m_MaxLevel.load(std::memory_order_relaxed);
}

#if defined(CONF_PLATFORM_ANDROID)
class CLoggerAndroid : public ILogger
{
public:
	void Log(const CLogMessage *pMessage) override
	{
		if(m_Filter.Filters(pMessage))
		{
			return;
		}
		int AndroidLevel;
		switch(pMessage->m_Level)
		{
		case LEVEL_TRACE: AndroidLevel = ANDROID_LOG_VERBOSE; break;
		case LEVEL_DEBUG: AndroidLevel = ANDROID_LOG_DEBUG; break;
		case LEVEL_INFO: AndroidLevel = ANDROID_LOG_INFO; break;
		case LEVEL_WARN: AndroidLevel = ANDROID_LOG_WARN; break;
		case LEVEL_ERROR: AndroidLevel = ANDROID_LOG_ERROR; break;
		}
		__android_log_write(AndroidLevel, pMessage->m_aSystem, pMessage->Message());
	}
};
std::unique_ptr<ILogger> log_logger_android()
{
	return std::make_unique<CLoggerAndroid>();
}
#else
std::unique_ptr<ILogger> log_logger_android()
{
	dbg_assert(0, "Android logger on non-Android");
	return nullptr;
}
#endif

class CLoggerCollection : public ILogger
{
	std::vector<std::shared_ptr<ILogger>> m_vpLoggers;

public:
	CLoggerCollection(std::vector<std::shared_ptr<ILogger>> &&vpLoggers) :
		m_vpLoggers(std::move(vpLoggers))
	{
		m_Filter.m_MaxLevel.store(LEVEL_TRACE, std::memory_order_relaxed);
	}
	void Log(const CLogMessage *pMessage) override
	{
		if(m_Filter.Filters(pMessage))
		{
			return;
		}
		for(auto &pLogger : m_vpLoggers)
		{
			pLogger->Log(pMessage);
		}
	}
	void GlobalFinish() override
	{
		for(auto &pLogger : m_vpLoggers)
		{
			pLogger->GlobalFinish();
		}
	}
};

std::unique_ptr<ILogger> log_logger_collection(std::vector<std::shared_ptr<ILogger>> &&vpLoggers)
{
	return std::make_unique<CLoggerCollection>(std::move(vpLoggers));
}

class CLoggerAsync : public ILogger
{
	ASYNCIO *m_pAio;
	bool m_AnsiTruecolor;
	bool m_Close;

public:
	CLoggerAsync(IOHANDLE File, bool AnsiTruecolor, bool Close) :
		m_pAio(aio_new(File)),
		m_AnsiTruecolor(AnsiTruecolor),
		m_Close(Close)
	{
	}
	void Log(const CLogMessage *pMessage) override
	{
		if(m_Filter.Filters(pMessage))
		{
			return;
		}
		aio_lock(m_pAio);
		if(m_AnsiTruecolor && pMessage->m_HaveColor)
		{
			// https://en.wikipedia.org/w/index.php?title=ANSI_escape_code&oldid=1077146479#24-bit
			char aAnsi[32];
			str_format(aAnsi, sizeof(aAnsi),
				"\x1b[38;2;%d;%d;%dm",
				pMessage->m_Color.r,
				pMessage->m_Color.g,
				pMessage->m_Color.b);
			aio_write_unlocked(m_pAio, aAnsi, str_length(aAnsi));
		}
		aio_write_unlocked(m_pAio, pMessage->m_aLine, pMessage->m_LineLength);
		if(m_AnsiTruecolor && pMessage->m_HaveColor)
		{
			const char aResetColor[] = "\x1b[0m";
			aio_write_unlocked(m_pAio, aResetColor, str_length(aResetColor)); // reset
		}
		aio_write_newline_unlocked(m_pAio);
		aio_unlock(m_pAio);
	}
	~CLoggerAsync() override
	{
		if(m_Close)
		{
			aio_close(m_pAio);
		}
		aio_wait(m_pAio);
		aio_free(m_pAio);
	}
	void GlobalFinish() override
	{
		if(m_Close)
		{
			aio_close(m_pAio);
		}
		aio_wait(m_pAio);
	}
};

std::unique_ptr<ILogger> log_logger_file(IOHANDLE logfile)
{
	return std::make_unique<CLoggerAsync>(logfile, false, true);
}

#if defined(CONF_FAMILY_WINDOWS)
static int color_hsv_to_windows_console_color(const ColorHSVA &Hsv)
{
	int h = Hsv.h * 255.0f;
	int s = Hsv.s * 255.0f;
	int v = Hsv.v * 255.0f;
	if(s >= 0 && s <= 10)
	{
		if(v <= 150)
			return FOREGROUND_INTENSITY;
		return FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY;
	}
	if(h < 0)
		return FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY;
	else if(h < 15)
		return FOREGROUND_RED | FOREGROUND_INTENSITY;
	else if(h < 30)
		return FOREGROUND_GREEN | FOREGROUND_RED;
	else if(h < 60)
		return FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY;
	else if(h < 110)
		return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
	else if(h < 140)
		return FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
	else if(h < 170)
		return FOREGROUND_BLUE | FOREGROUND_INTENSITY;
	else if(h < 195)
		return FOREGROUND_BLUE | FOREGROUND_RED;
	else if(h < 240)
		return FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY;
	else
		return FOREGROUND_RED | FOREGROUND_INTENSITY;
}

class CWindowsConsoleLogger : public ILogger
{
	HANDLE m_pConsole;
	bool m_EnableColor;
	int m_BackgroundColor;
	int m_ForegroundColor;
	CLock m_OutputLock;
	bool m_Finished = false;

public:
	CWindowsConsoleLogger(HANDLE pConsole, bool EnableColor) :
		m_pConsole(pConsole),
		m_EnableColor(EnableColor)
	{
		CONSOLE_SCREEN_BUFFER_INFO ConsoleInfo;
		if(GetConsoleScreenBufferInfo(pConsole, &ConsoleInfo))
		{
			m_BackgroundColor = ConsoleInfo.wAttributes & (BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY);
			m_ForegroundColor = ConsoleInfo.wAttributes & (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
		}
		else
		{
			m_BackgroundColor = 0;
			m_ForegroundColor = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY;
		}
	}
	void Log(const CLogMessage *pMessage) override REQUIRES(!m_OutputLock)
	{
		if(m_Filter.Filters(pMessage))
		{
			return;
		}
		const std::wstring WideMessage = windows_utf8_to_wide(pMessage->m_aLine);

		int Color = m_BackgroundColor;
		if(m_EnableColor && pMessage->m_HaveColor)
		{
			const ColorRGBA Rgba(pMessage->m_Color.r / 255.0f, pMessage->m_Color.g / 255.0f, pMessage->m_Color.b / 255.0f);
			Color |= color_hsv_to_windows_console_color(color_cast<ColorHSVA>(Rgba));
		}
		else
			Color |= FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY;

		const CLockScope LockScope(m_OutputLock);
		if(!m_Finished)
		{
			SetConsoleTextAttribute(m_pConsole, Color);
			WriteConsoleW(m_pConsole, WideMessage.c_str(), WideMessage.length(), nullptr, nullptr);
			WriteConsoleW(m_pConsole, L"\r\n", 2, nullptr, nullptr);
		}
	}
	void GlobalFinish() override REQUIRES(!m_OutputLock)
	{
		// Restore original color
		const CLockScope LockScope(m_OutputLock);
		SetConsoleTextAttribute(m_pConsole, m_BackgroundColor | m_ForegroundColor);
		m_Finished = true;
	}
};

static IOHANDLE ConvertWindowsHandle(HANDLE pHandle, int OpenFlags)
{
	int FileDescriptor = _open_osfhandle(reinterpret_cast<intptr_t>(pHandle), OpenFlags);
	dbg_assert(FileDescriptor != -1, "_open_osfhandle failure");
	IOHANDLE FileStream = _wfdopen(FileDescriptor, L"w");
	dbg_assert(FileStream != nullptr, "_wfdopen failure");
	return FileStream;
}
#endif

std::unique_ptr<ILogger> log_logger_stdout()
{
#if !defined(CONF_FAMILY_WINDOWS)
	// TODO: Only enable true color when COLORTERM contains "truecolor".
	// https://github.com/termstandard/colors/tree/65bf0cd1ece7c15fa33a17c17528b02c99f1ae0b#checking-for-colorterm
	const bool Colors = getenv("NO_COLOR") == nullptr && isatty(STDOUT_FILENO);
	return std::make_unique<CLoggerAsync>(io_stdout(), Colors, false);
#else
	// If we currently have no stdout (console, file, pipe),
	// try to attach to the console of the parent process.
	if(GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_UNKNOWN)
	{
		AttachConsole(ATTACH_PARENT_PROCESS);
	}

	HANDLE pOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	if(pOutput == nullptr)
	{
		// There is no console, file or pipe that we can output to.
		return nullptr;
	}
	dbg_assert(pOutput != INVALID_HANDLE_VALUE, "GetStdHandle failure");

	const DWORD OutputType = GetFileType(pOutput);
	if(OutputType == FILE_TYPE_CHAR)
	{
		DWORD OldConsoleMode = 0;
		if(!GetConsoleMode(pOutput, &OldConsoleMode))
		{
			// GetConsoleMode can fail with ERROR_INVALID_HANDLE when redirecting output to "nul",
			// which is considered a character file but cannot be used as a console.
			dbg_assert(GetLastError() == ERROR_INVALID_HANDLE, "GetConsoleMode failure");
			return nullptr;
		}

		const bool Colors = _wgetenv(L"NO_COLOR") == nullptr;

		// Try to enable virtual terminal processing in the Windows console.
		// See https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
		if(!SetConsoleMode(pOutput, OldConsoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN))
		{
			// Try to downgrade mode gracefully when failing to set both.
			if(!SetConsoleMode(pOutput, OldConsoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
			{
				// Fallback to old, slower Windows logging API, when failing to enable virtual terminal processing.
				return std::make_unique<CWindowsConsoleLogger>(pOutput, Colors);
			}
		}

		// Virtual terminal processing was enabled successfully. We can
		// use the async logger with ANSI escape codes for colors now.
		// We need to set the output encoding to UTF-8 manually and
		// convert the HANDLE to an IOHANDLE to use the async logger.
		// We assume UTF-8 is available when virtual terminal processing is.
		dbg_assert(SetConsoleOutputCP(CP_UTF8) != 0, "SetConsoleOutputCP failure");
		return std::make_unique<CLoggerAsync>(ConvertWindowsHandle(pOutput, _O_TEXT), Colors, false);
	}
	else if(OutputType == FILE_TYPE_DISK || OutputType == FILE_TYPE_PIPE)
	{
		// Writing to a pipe works the same as writing to a file.
		// We can use the async logger to write to files and pipes
		// by converting the HANDLE to an IOHANDLE.
		// For pipes there does not seem to be any way to determine
		// whether the console supports ANSI escape codes.
		return std::make_unique<CLoggerAsync>(ConvertWindowsHandle(pOutput, _O_APPEND), false, false);
	}
	else
	{
		dbg_assert(false, "GetFileType failure");
		dbg_break();
	}
#endif
}

#if defined(CONF_FAMILY_WINDOWS)
class CLoggerWindowsDebugger : public ILogger
{
public:
	void Log(const CLogMessage *pMessage) override
	{
		if(m_Filter.Filters(pMessage))
		{
			return;
		}
		const std::wstring WideMessage = windows_utf8_to_wide(pMessage->m_aLine);
		OutputDebugStringW(WideMessage.c_str());
	}
};
std::unique_ptr<ILogger> log_logger_windows_debugger()
{
	return std::make_unique<CLoggerWindowsDebugger>();
}
#else
std::unique_ptr<ILogger> log_logger_windows_debugger()
{
	dbg_assert(0, "Windows Debug logger on non-Windows");
	return nullptr;
}
#endif

class CLoggerNoOp : public ILogger
{
public:
	void Log(const CLogMessage *pMessage) override
	{
		// no-op
	}
};
std::unique_ptr<ILogger> log_logger_noop()
{
	return std::make_unique<CLoggerNoOp>();
}

void CFutureLogger::Set(std::shared_ptr<ILogger> pLogger)
{
	const CLockScope LockScope(m_PendingLock);
	std::shared_ptr<ILogger> pNullLogger;
	if(!std::atomic_compare_exchange_strong_explicit(&m_pLogger, &pNullLogger, pLogger, std::memory_order_acq_rel, std::memory_order_acq_rel))
	{
		dbg_assert(false, "future logger has already been set and can only be set once");
	}
	m_pLogger = std::move(pLogger);

	for(const auto &Pending : m_vPending)
	{
		m_pLogger->Log(&Pending);
	}
	m_vPending.clear();
	m_vPending.shrink_to_fit();
}

void CFutureLogger::Log(const CLogMessage *pMessage)
{
	auto pLogger = std::atomic_load_explicit(&m_pLogger, std::memory_order_acquire);
	if(pLogger)
	{
		pLogger->Log(pMessage);
		return;
	}
	const CLockScope LockScope(m_PendingLock);
	pLogger = std::atomic_load_explicit(&m_pLogger, std::memory_order_relaxed);
	if(pLogger)
	{
		pLogger->Log(pMessage);
		return;
	}
	m_vPending.push_back(*pMessage);
}

void CFutureLogger::GlobalFinish()
{
	auto pLogger = std::atomic_load_explicit(&m_pLogger, std::memory_order_acquire);
	if(pLogger)
	{
		pLogger->GlobalFinish();
	}
}

void CFutureLogger::OnFilterChange()
{
	auto pLogger = std::atomic_load_explicit(&m_pLogger, std::memory_order_acquire);
	if(pLogger)
	{
		pLogger->SetFilter(m_Filter);
	}
}
