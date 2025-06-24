#ifndef BASE_LOG_H
#define BASE_LOG_H

#include <cstdarg>
#include <cstdint>

#ifdef __GNUC__
#define GNUC_ATTRIBUTE(x) __attribute__(x)
#else
#define GNUC_ATTRIBUTE(x)
#endif

enum LEVEL : char
{
	LEVEL_ERROR,
	LEVEL_WARN,
	LEVEL_INFO,
	LEVEL_DEBUG,
	LEVEL_TRACE,
};

struct LOG_COLOR
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

#define log_error(sys, ...) log_log(LEVEL_ERROR, sys, __VA_ARGS__)
#define log_warn(sys, ...) log_log(LEVEL_WARN, sys, __VA_ARGS__)
#define log_info(sys, ...) log_log(LEVEL_INFO, sys, __VA_ARGS__)
#define log_debug(sys, ...) log_log(LEVEL_DEBUG, sys, __VA_ARGS__)
#define log_trace(sys, ...) log_log(LEVEL_TRACE, sys, __VA_ARGS__)

#define log_error_color(color, sys, ...) log_log_color(LEVEL_ERROR, color, sys, __VA_ARGS__)
#define log_warn_color(color, sys, ...) log_log_color(LEVEL_WARN, color, sys, __VA_ARGS__)
#define log_info_color(color, sys, ...) log_log_color(LEVEL_INFO, color, sys, __VA_ARGS__)
#define log_debug_color(color, sys, ...) log_log_color(LEVEL_DEBUG, color, sys, __VA_ARGS__)
#define log_trace_color(color, sys, ...) log_log_color(LEVEL_TRACE, color, sys, __VA_ARGS__)

/**
 * @defgroup Log
 *
 * Methods for outputting log messages and way of handling them.
 */

/**
 * @ingroup Log
 *
 * Prints a log message.
 *
 * @param level Severity of the log message.
 * @param sys A string that describes what system the message belongs to.
 * @param fmt A printf styled format string.
 */
void log_log(LEVEL level, const char *sys, const char *fmt, ...)
	GNUC_ATTRIBUTE((format(printf, 3, 4)));

/**
 * @ingroup Log
 *
 * Prints a log message with a given color.
 *
 * @param level Severity of the log message.
 * @param color Requested color for the log message output.
 * @param sys A string that describes what system the message belongs to.
 * @param fmt A printf styled format string.
 */
void log_log_color(LEVEL level, LOG_COLOR color, const char *sys, const char *fmt, ...)
	GNUC_ATTRIBUTE((format(printf, 4, 5)));

/**
 * @ingroup Log
 *
 * Same as `log_log`, but takes a `va_list` instead.
 *
 * @param level Severity of the log message.
 * @param sys A string that describes what system the message belongs to.
 * @param fmt A printf styled format string.
 * @param args The variable argument list.
 */
void log_log_v(LEVEL level, const char *sys, const char *fmt, va_list args)
	GNUC_ATTRIBUTE((format(printf, 3, 0)));

/**
 * @ingroup Log
 *
 * Same as `log_log_color`, but takes a `va_list` instead.
 *
 * @param level Severity of the log message.
 * @param color Requested color for the log message output.
 * @param sys A string that describes what system the message belongs to.
 * @param fmt A printf styled format string.
 * @param args The variable argument list.
 */
void log_log_color_v(LEVEL level, LOG_COLOR color, const char *sys, const char *fmt, va_list args)
	GNUC_ATTRIBUTE((format(printf, 4, 0)));
#endif // BASE_LOG_H
