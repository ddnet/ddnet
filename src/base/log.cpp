#include "logger.h"

#include "color.h"
#include "system.h"

#include <atomic>
#include <cstdio>

#if defined(CONF_FAMILY_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501 /* required for mingw to get getaddrinfo to work */
#include <windows.h>
#else
#include <unistd.h>
#endif

#if defined(CONF_PLATFORM_ANDROID)
#include <android/log.h>
#endif

extern "C" {

std::atomic<LEVEL> loglevel = LEVEL_INFO;
std::atomic<ILogger *> global_logger = nullptr;
thread_local ILogger *scope_logger = nullptr;
thread_local bool in_logger = false;

void log_set_loglevel(LEVEL level)
{
	loglevel.store(level, std::memory_order_release);
}

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
	global_logger.load(std::memory_order_acquire)->GlobalFinish();
}

void log_set_global_logger_default()
{
	std::unique_ptr<ILogger> logger;
#if defined(CONF_PLATFORM_ANDROID)
	logger = log_logger_android();
#else
	logger = log_logger_stdout();
#endif
	log_set_global_logger(logger.release());
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

void log_log_impl(LEVEL level, bool have_color, LOG_COLOR color, const char *sys, const char *fmt, va_list args)
{
	if(level > loglevel.load(std::memory_order_acquire))
		return;

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
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#if defined(CONF_FAMILY_WINDOWS)
	_vsnprintf(pMessage, MessageSize, fmt, args);
#else
	vsnprintf(pMessage, MessageSize, fmt, args);
#endif
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
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
}

#if defined(CONF_PLATFORM_ANDROID)
class CLoggerAndroid : public ILogger
{
public:
	void Log(const CLogMessage *pMessage) override
	{
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
	}
	void Log(const CLogMessage *pMessage) override
	{
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
		aio_lock(m_pAio);
		if(m_AnsiTruecolor)
		{
			// https://en.wikipedia.org/w/index.php?title=ANSI_escape_code&oldid=1077146479#24-bit
			char aAnsi[32];
			if(pMessage->m_HaveColor)
			{
				str_format(aAnsi, sizeof(aAnsi),
					"\x1b[38;2;%d;%d;%dm",
					pMessage->m_Color.r,
					pMessage->m_Color.g,
					pMessage->m_Color.b);
				aio_write_unlocked(m_pAio, aAnsi, str_length(aAnsi));
			}
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
	~CLoggerAsync()
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
	int m_BackgroundColor;
	int m_ForegroundColor;
	std::mutex m_OutputLock;
	bool m_Finished = false;

public:
	CWindowsConsoleLogger(HANDLE pConsole) :
		m_pConsole(pConsole)
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
	void Log(const CLogMessage *pMessage) override
	{
		int WLen = MultiByteToWideChar(CP_UTF8, 0, pMessage->m_aLine, pMessage->m_LineLength, NULL, 0);
		if(!WLen)
		{
			WCHAR aError[] = L"Failed to obtain length of log message\r\n";
			WriteConsoleW(m_pConsole, aError, std::size(aError) - 1, NULL, NULL);
			return;
		}
		WCHAR *pWide = (WCHAR *)malloc((WLen + 2) * sizeof(*pWide));
		WLen = MultiByteToWideChar(CP_UTF8, 0, pMessage->m_aLine, pMessage->m_LineLength, pWide, WLen);
		if(!WLen)
		{
			WCHAR aError[] = L"Failed to convert log message encoding\r\n";
			WriteConsoleW(m_pConsole, aError, std::size(aError) - 1, NULL, NULL);
			free(pWide);
			return;
		}
		pWide[WLen++] = '\r';
		pWide[WLen++] = '\n';

		int Color = m_BackgroundColor;
		if(pMessage->m_HaveColor)
		{
			ColorRGBA Rgba(1.0, 1.0, 1.0, 1.0);
			Rgba.r = pMessage->m_Color.r / 255.0;
			Rgba.g = pMessage->m_Color.g / 255.0;
			Rgba.b = pMessage->m_Color.b / 255.0;
			Color |= color_hsv_to_windows_console_color(color_cast<ColorHSVA>(Rgba));
		}
		else
			Color |= FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY;

		m_OutputLock.lock();
		if(!m_Finished)
		{
			SetConsoleTextAttribute(m_pConsole, Color);
			WriteConsoleW(m_pConsole, pWide, WLen, NULL, NULL);
		}
		m_OutputLock.unlock();
		free(pWide);
	}
	void GlobalFinish() override
	{
		// Restore original color
		m_OutputLock.lock();
		SetConsoleTextAttribute(m_pConsole, m_BackgroundColor | m_ForegroundColor);
		m_Finished = true;
		m_OutputLock.unlock();
	}
};
class CWindowsFileLogger : public ILogger
{
	HANDLE m_pFile;
	std::mutex m_OutputLock;

public:
	CWindowsFileLogger(HANDLE pFile) :
		m_pFile(pFile)
	{
	}
	void Log(const CLogMessage *pMessage) override
	{
		m_OutputLock.lock();
		DWORD Written; // we don't care about the value, but Windows 7 crashes if we pass NULL
		WriteFile(m_pFile, pMessage->m_aLine, pMessage->m_LineLength, &Written, NULL);
		WriteFile(m_pFile, "\r\n", 2, &Written, NULL);
		m_OutputLock.unlock();
	}
};
#endif

std::unique_ptr<ILogger> log_logger_stdout()
{
#if !defined(CONF_FAMILY_WINDOWS)
	// TODO: Only enable true color when COLORTERM contains "truecolor".
	// https://github.com/termstandard/colors/tree/65bf0cd1ece7c15fa33a17c17528b02c99f1ae0b#checking-for-colorterm
	const bool colors = getenv("NO_COLOR") == nullptr && isatty(STDOUT_FILENO);
	return std::make_unique<CLoggerAsync>(io_stdout(), colors, false);
#else
	if(GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_UNKNOWN)
		AttachConsole(ATTACH_PARENT_PROCESS);
	HANDLE pOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	switch(GetFileType(pOutput))
	{
	case FILE_TYPE_CHAR: return std::make_unique<CWindowsConsoleLogger>(pOutput);
	case FILE_TYPE_PIPE: [[fallthrough]]; // writing to pipe works the same as writing to a file
	case FILE_TYPE_DISK: return std::make_unique<CWindowsFileLogger>(pOutput);
	default: return std::make_unique<CLoggerAsync>(io_stdout(), false, false);
	}
#endif
}

#if defined(CONF_FAMILY_WINDOWS)
class CLoggerWindowsDebugger : public ILogger
{
public:
	void Log(const CLogMessage *pMessage) override
	{
		WCHAR aWBuffer[4096];
		MultiByteToWideChar(CP_UTF8, 0, pMessage->m_aLine, -1, aWBuffer, sizeof(aWBuffer) / sizeof(WCHAR));
		OutputDebugStringW(aWBuffer);
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

void CFutureLogger::Set(std::unique_ptr<ILogger> &&pLogger)
{
	ILogger *null = nullptr;
	m_PendingLock.lock();
	ILogger *pLoggerRaw = pLogger.release();
	if(!m_pLogger.compare_exchange_strong(null, pLoggerRaw, std::memory_order_acq_rel))
	{
		dbg_assert(false, "future logger has already been set and can only be set once");
	}
	for(const auto &Pending : m_vPending)
	{
		pLoggerRaw->Log(&Pending);
	}
	m_vPending.clear();
	m_vPending.shrink_to_fit();
	m_PendingLock.unlock();
}

void CFutureLogger::Log(const CLogMessage *pMessage)
{
	ILogger *pLogger = m_pLogger.load(std::memory_order_acquire);
	if(pLogger)
	{
		pLogger->Log(pMessage);
		return;
	}
	m_PendingLock.lock();
	m_vPending.push_back(*pMessage);
	m_PendingLock.unlock();
}

void CFutureLogger::GlobalFinish()
{
	ILogger *pLogger = m_pLogger.load(std::memory_order_acquire);
	if(pLogger)
	{
		pLogger->GlobalFinish();
	}
}
