/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "dbg.h"

#include "logger.h"
#include "system.h" // TODO: replace with str.h after moving str_format

#include <atomic>
#include <cstdarg>
#include <cstdlib>

std::atomic_bool dbg_assert_failing = false;
DBG_ASSERT_HANDLER dbg_assert_handler;

bool dbg_assert_has_failed()
{
	return dbg_assert_failing.load(std::memory_order_acquire);
}

void dbg_assert_imp(const char *filename, int line, const char *fmt, ...)
{
	const bool already_failing = dbg_assert_has_failed();
	dbg_assert_failing.store(true, std::memory_order_release);
	char msg[512];
	va_list args;
	va_start(args, fmt);
	str_format_v(msg, sizeof(msg), fmt, args);
	char error[1024];
	str_format(error, sizeof(error), "%s(%d): %s", filename, line, msg);
	va_end(args);
	log_error("assert", "%s", error);
	if(!already_failing)
	{
		DBG_ASSERT_HANDLER handler = dbg_assert_handler;
		if(handler)
			handler(error);
	}
	log_global_logger_finish();
	dbg_break();
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
