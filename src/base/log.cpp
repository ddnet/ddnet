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
