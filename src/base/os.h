#ifndef BASE_OS_H
#define BASE_OS_H

#include "detect.h"

#include <cstddef>

/**
 * OS specific functionality.
 *
 * @defgroup OS OS
 */

/**
 * Fixes the command line arguments to be encoded in UTF-8 on all systems.
 *
 * @ingroup OS
 *
 * @param argc A pointer to the argc parameter that was passed to the main function.
 * @param argv A pointer to the argv parameter that was passed to the main function.
 *
 * @remark You need to call @link cmdline_free @endlink once you're no longer using the results.
 */
void cmdline_fix(int *argc, const char ***argv);

/**
 * Frees memory that was allocated by @link cmdline_fix @endlink.
 *
 * @ingroup OS
 *
 * @param argc The argc obtained from `cmdline_fix`.
 * @param argv The argv obtained from `cmdline_fix`.
 */
void cmdline_free(int argc, const char **argv);

/**
 * Fixes the command line arguments to be encoded in UTF-8 on all systems.
 * This is a RAII wrapper for @link cmdline_fix @endlink and @link cmdline_free @endlink.
 *
 * @ingroup OS
 */
class CCmdlineFix
{
	int m_Argc;
	const char **m_ppArgv;

public:
	CCmdlineFix(int *pArgc, const char ***pppArgv)
	{
		cmdline_fix(pArgc, pppArgv);
		m_Argc = *pArgc;
		m_ppArgv = *pppArgv;
	}
	~CCmdlineFix()
	{
		cmdline_free(m_Argc, m_ppArgv);
	}
	CCmdlineFix(const CCmdlineFix &) = delete;
};

#if !defined(CONF_PLATFORM_ANDROID)
/**
 * Opens a link in the browser.
 *
 * @ingroup OS
 *
 * @param link The link to open in a browser.
 *
 * @return `1` on success, `0` on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark This may not be called with untrusted input or it'll result in arbitrary code execution, especially on Windows.
 */
int os_open_link(const char *link);

/**
 * Opens a file or directory with the default program.
 *
 * @ingroup OS
 *
 * @param path The file or folder to open with the default program.
 *
 * @return `1` on success, `0` on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark This may not be called with untrusted input or it'll result in arbitrary code execution, especially on Windows.
 */
int os_open_file(const char *path);
#endif // !defined(CONF_PLATFORM_ANDROID)

/**
 * Returns a human-readable version string of the operating system.
 *
 * @ingroup OS
 *
 * @param version Buffer to use for the output.
 * @param length Length of the output buffer.
 *
 * @return `true` on success, `false` on failure.
 */
bool os_version_str(char *version, size_t length);

/**
 * Returns a string of the preferred locale of the user / operating system.
 * The string conforms to [RFC 3066](https://www.ietf.org/rfc/rfc3066.txt)
 * and only contains the characters `a`-`z`, `A`-`Z`, `0`-`9` and `-`.
 * If the preferred locale could not be determined this function
 * falls back to the locale `"en-US"`.
 *
 * @ingroup OS
 *
 * @param locale Buffer to use for the output.
 * @param length Length of the output buffer.
 *
 * @remark The strings are treated as null-terminated strings.
 */
void os_locale_str(char *locale, size_t length);

#endif
