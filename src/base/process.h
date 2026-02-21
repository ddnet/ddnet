#ifndef BASE_PROCESS_H
#define BASE_PROCESS_H

#include "detect.h"
#include "types.h"

/**
 * Process management.
 *
 * @defgroup Process Process
 */

/**
 * Returns the ID of the current process.
 *
 * @ingroup Process
 *
 * @return PID of the current process.
 */
int process_id();

#if !defined(CONF_PLATFORM_ANDROID)
/**
 * Determines the initial window state when using @link process_execute @endlink
 * to execute a process.
 *
 * @ingroup Process
 *
 * @remark Currently only supported on Windows.
 */
enum class EShellExecuteWindowState
{
	/**
	 * The process window is opened in the foreground and activated.
	 */
	FOREGROUND,

	/**
	 * The process window is opened in the background without focus.
	 */
	BACKGROUND,
};

/**
 * Executes a given file.
 *
 * @ingroup Process
 *
 * @param file The file to execute.
 * @param window_state The window state how the process window should be shown.
 * @param arguments Optional array of arguments to pass to the process.
 * @param num_arguments The number of arguments.
 *
 * @return Handle of the new process, or @link INVALID_PROCESS @endlink on error.
 */
PROCESS process_execute(const char *file, EShellExecuteWindowState window_state, const char **arguments = nullptr, size_t num_arguments = 0);

/**
 * Sends kill signal to a process.
 *
 * @ingroup Process
 *
 * @param process Handle of the process to kill.
 *
 * @return `1` on success, `0` on error.
 */
int process_kill(PROCESS process);

/**
 * Checks if a process is alive.
 *
 * @ingroup Process
 *
 * @param process Handle/PID of the process.
 *
 * @return `true` if the process is currently running,
 * @return `false` if the process is not running (dead).
 */
bool process_is_alive(PROCESS process);
#endif // !defined(CONF_PLATFORM_ANDROID)

#endif
