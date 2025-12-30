/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "time.h"

#include "dbg.h"
#include "system.h" // TODO: replace with str.h after moving str_format

#include <cmath>
#include <iomanip> // std::get_time
#include <sstream> // std::istringstream

static int new_tick = -1;

void set_new_tick()
{
	new_tick = 1;
}

static_assert(std::chrono::steady_clock::is_steady, "Compiler does not support steady clocks, it might be out of date.");
static_assert(std::chrono::steady_clock::period::den / std::chrono::steady_clock::period::num >= 1000000000, "Compiler has a bad timer precision and might be out of date.");
static const std::chrono::time_point<std::chrono::steady_clock> GLOBAL_START_TIME = std::chrono::steady_clock::now();

std::chrono::nanoseconds time_get_nanoseconds()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - GLOBAL_START_TIME);
}

int64_t time_get_impl()
{
	return time_get_nanoseconds().count();
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

int64_t time_timestamp()
{
	return time(nullptr);
}

static tm time_localtime_threadlocal(time_t *time_data)
{
#if defined(CONF_FAMILY_WINDOWS)
	// The result of localtime is thread-local on Windows
	// https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/localtime-localtime32-localtime64
	tm *time = localtime(time_data);
#else
	// Thread-local buffer for the result of localtime_r
	thread_local tm time_info_buf;
	tm *time = localtime_r(time_data, &time_info_buf);
#endif
	dbg_assert(time != nullptr, "Failed to get local time for time data %" PRId64, (int64_t)time_data);
	return *time;
}

int time_houroftheday()
{
	time_t time_data;
	time(&time_data);
	const tm time_info = time_localtime_threadlocal(&time_data);
	return time_info.tm_hour;
}

static bool time_iseasterday(time_t time_data, tm time_info)
{
	// compute Easter day (Sunday) using https://en.wikipedia.org/w/index.php?title=Computus&oldid=890710285#Anonymous_Gregorian_algorithm
	int Y = time_info.tm_year + 1900;
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
		const tm offset_time_info = time_localtime_threadlocal(&time_data);
		if(offset_time_info.tm_mon == month - 1 && offset_time_info.tm_mday == day)
			return true;
	}
	return false;
}

ETimeSeason time_season()
{
	time_t time_data;
	time(&time_data);
	const tm time_info = time_localtime_threadlocal(&time_data);

	if((time_info.tm_mon == 11 && time_info.tm_mday == 31) || (time_info.tm_mon == 0 && time_info.tm_mday == 1))
	{
		return SEASON_NEWYEAR;
	}
	else if(time_info.tm_mon == 11 && time_info.tm_mday >= 24 && time_info.tm_mday <= 26)
	{
		return SEASON_XMAS;
	}
	else if((time_info.tm_mon == 9 && time_info.tm_mday == 31) || (time_info.tm_mon == 10 && time_info.tm_mday == 1))
	{
		return SEASON_HALLOWEEN;
	}
	else if(time_iseasterday(time_data, time_info))
	{
		return SEASON_EASTER;
	}

	switch(time_info.tm_mon)
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
		dbg_assert_failed("Invalid month: %d", time_info.tm_mon);
	}
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
void str_timestamp(char *buffer, int buffer_size)
{
	str_timestamp_format(buffer, buffer_size, FORMAT_NOSPACE);
}

void str_timestamp_format(char *buffer, int buffer_size, const char *format)
{
	time_t time_data;
	time(&time_data);
	str_timestamp_ex(time_data, buffer, buffer_size, format);
}

void str_timestamp_ex(time_t time_data, char *buffer, int buffer_size, const char *format)
{
	const tm time_info = time_localtime_threadlocal(&time_data);
	strftime(buffer, buffer_size, format, &time_info);
	buffer[buffer_size - 1] = 0; /* assure null termination */
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
	return str_time(static_cast<int64_t>(std::roundf(secs * 1000) / 10), format, buffer, buffer_size);
}
