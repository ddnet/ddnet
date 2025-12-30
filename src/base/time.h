/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef BASE_TIME_H
#define BASE_TIME_H

#include <chrono>
#include <cstdint>
#include <ctime>

/**
 * Time utilities.
 *
 * @defgroup Time Time
 *
 * @ref Timestamp
 */

/**
 * Timestamp related functions.
 *
 * @defgroup Timestamp Timestamps
 *
 * @ref Time
 */

/**
 * Clears the cached sample of the high resolution timer.
 *
 * @ingroup Time
 *
 * @see time_get
 */
void set_new_tick();

/**
 * Fetches a sample from a high resolution timer and converts it to nanoseconds.
 *
 * @ingroup Time
 *
 * @return Current value of the timer in nanoseconds.
 */
std::chrono::nanoseconds time_get_nanoseconds();

/**
 * Fetches a sample from a high resolution timer.
 *
 * @ingroup Time
 *
 * @return Current value of the timer.
 *
 * @remark To know how fast the timer is ticking, see @link time_freq @endlink.
 *
 * @see time_freq
 */
int64_t time_get_impl();

/**
 * Fetches a cached sample from a high resolution timer.
 *
 * @ingroup Time
 *
 * @return Current value of the timer.
 *
 * @remark To know how fast the timer is ticking, see @link time_freq @endlink.
 * @remark The value is cached for each tick, see @link set_new_tick @endlink.
 *         Uses @link time_get_impl @endlink to fetch the uncached sample.
 *
 * @see time_freq time_get_impl
 */
int64_t time_get();

/**
 * @ingroup Time
 *
 * @return The frequency of the high resolution timer.
 */
int64_t time_freq();

/**
 * Retrieves the current time as a UNIX timestamp.
 *
 * @ingroup Timestamp
 *
 * @return The time as a UNIX timestamp.
 */
int64_t time_timestamp();

/**
 * Retrieves the hours since midnight (0..23).
 *
 * @ingroup Time
 *
 * @return The current hour of the day.
 */
int time_houroftheday();

/**
 * A season of the year or seasonal event.
 *
 * @ingroup Time
 */
enum ETimeSeason
{
	SEASON_SPRING = 0,
	SEASON_SUMMER,
	SEASON_AUTUMN,
	SEASON_WINTER,
	SEASON_EASTER,
	SEASON_HALLOWEEN,
	SEASON_XMAS,
	SEASON_NEWYEAR
};

/**
 * Retrieves the current season of the year.
 *
 * @ingroup Time
 *
 * @return One of the SEASON_* enum literals.
 */
ETimeSeason time_season();

/**
 * Copies a timestamp of the current time in the format `year-month-day_hour-minute-second` to the string.
 *
 * @ingroup Timestamp
 *
 * @param buffer Pointer to a buffer that shall receive the timestamp string.
 * @param buffer_size Size of the buffer.
 *
 * @remark Guarantees that buffer string will contain null-termination.
 */
void str_timestamp(char *buffer, int buffer_size);

/**
 * Copies a timestamp of the current time in the given format to the string.
 *
 * @ingroup Timestamp
 *
 * @param buffer Pointer to a buffer that shall receive the timestamp string.
 * @param buffer_size Size of the buffer.
 * @param format Time formatting string. See https://cppreference.com/w/c/chrono/strftime.html for format description.
 *
 * @remark Guarantees that buffer string will contain null-termination.
 */
[[gnu::format(strftime, 3, 0)]] void str_timestamp_format(char *buffer, int buffer_size, const char *format);

/**
 * Copies a timestamp of the given time in the given format to the string.
 *
 * @ingroup Timestamp
 *
 * @param time The time value to represent as a string.
 * @param buffer Pointer to a buffer that shall receive the timestamp string.
 * @param buffer_size Size of the buffer.
 * @param format Time formatting string. See https://cppreference.com/w/c/chrono/strftime.html for format description.
 *
 * @remark Guarantees that buffer string will contain null-termination.
 */
[[gnu::format(strftime, 4, 0)]] void str_timestamp_ex(time_t time, char *buffer, int buffer_size, const char *format);

/**
 * Parses a string into a timestamp following a specified format.
 *
 * @ingroup Timestamp
 *
 * @param string Pointer to the string to parse.
 * @param format The time format to use (for example `FORMAT_NOSPACE` below).
 * @param timestamp Pointer to the timestamp result.
 *
 * @return `true` on success, `false` if the string could not be parsed with the specified format.
 */
[[gnu::format(strftime, 2, 0)]] bool timestamp_from_str(const char *string, const char *format, time_t *timestamp);

#define FORMAT_TIME "%H:%M:%S"
#define FORMAT_SPACE "%Y-%m-%d %H:%M:%S"
#define FORMAT_NOSPACE "%Y-%m-%d_%H-%M-%S"

enum
{
	TIME_DAYS,
	TIME_HOURS,
	TIME_MINS,
	TIME_HOURS_CENTISECS,
	TIME_MINS_CENTISECS,
	TIME_SECS_CENTISECS,
};

/**
 * Formats a time string.
 *
 * @ingroup Timestamp
 *
 * @param centisecs Time in centiseconds.
 * @param format Format of the time string, see enum above, for example `TIME_DAYS`.
 * @param buffer Pointer to a buffer that shall receive the timestamp string.
 * @param buffer_size Size of the buffer.
 *
 * @return Number of bytes written, `-1` on invalid format or `buffer_size <= 0`.
 */
int str_time(int64_t centisecs, int format, char *buffer, int buffer_size);

/**
 * Formats a time string.
 *
 * @ingroup Timestamp
 *
 * @param secs Time in seconds.
 * @param format Format of the time string, see enum above, for example `TIME_DAYS`.
 * @param buffer Pointer to a buffer that shall receive the timestamp string.
 * @param buffer_size Size of the buffer.
 *
 * @remark The time is rounded to the nearest centisecond.
 *
 * @return Number of bytes written, `-1` on invalid format or `buffer_size <= 0`.
 */
int str_time_float(float secs, int format, char *buffer, int buffer_size);

#endif
