/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef BASE_WINDOWS_H
#define BASE_WINDOWS_H

#include "detect.h"

#if defined(CONF_FAMILY_WINDOWS)

#include <cstdint>
#include <optional>
#include <string>

/**
 * Windows-specific functionality.
 *
 * @defgroup Windows Windows
 */

/**
 * Formats a Windows error code as a human-readable string.
 *
 * @ingroup Windows
 *
 * @param error The Windows error code.
 *
 * @return A new std::string representing the error code.
 */
std::string windows_format_system_message(unsigned long error);

/**
 * Converts an array of arguments into a wide string with proper escaping for Windows command-line usage.
 *
 * @ingroup Windows
 *
 * @param arguments Array of arguments.
 * @param num_arguments The number of arguments.
 *
 * @return Wide string of arguments with escaped quotes.
 */
std::wstring windows_args_to_wide(const char **arguments, size_t num_arguments);

/**
 * Converts a UTF-8 encoded string to a wide character string
 * for use with the Windows API.
 *
 * @ingroup Windows
 *
 * @param str The UTF-8 encoded string to convert.
 *
 * @return The argument as a wide character string.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Fails with assertion error if passed UTF-8 is invalid.
 */
std::wstring windows_utf8_to_wide(const char *str);

/**
 * Converts a wide character string obtained from the Windows API
 * to a UTF-8 encoded string.
 *
 * @ingroup Windows
 *
 * @param wide_str The wide character string to convert.
 *
 * @return The argument as a UTF-8 encoded string, wrapped in an optional.
 * The optional is empty, if the wide string contains invalid codepoints.
 *
 * @remark The strings are treated as null-terminated strings.
 */
std::optional<std::string> windows_wide_to_utf8(const wchar_t *wide_str);

/**
 * This is a RAII wrapper to initialize/uninitialize the Windows COM library,
 * which may be necessary for using the @link open_file @endlink and
 * @link open_link @endlink functions.
 * Must be used on every thread. It's automatically used on threads created
 * with @link thread_init @endlink. Pass `true` to the constructor on threads
 * that own a window (i.e. pump a message queue).
 *
 * @ingroup Windows
 */
class CWindowsComLifecycle
{
public:
	CWindowsComLifecycle(bool HasWindow);
	~CWindowsComLifecycle();
	CWindowsComLifecycle(const CWindowsComLifecycle &) = delete;
};

/**
 * Registers a protocol handler.
 *
 * @ingroup Windows
 *
 * @param protocol_name The name of the protocol.
 * @param executable The absolute path of the executable that will be associated with the protocol.
 * @param updated Pointer to a variable that will be set to `true`, iff the shell needs to be updated.
 *
 * @return `true` on success, `false` on failure.
 *
 * @remark The caller must later call @link shell_update @endlink, iff the shell needs to be updated.
 */
bool windows_shell_register_protocol(const char *protocol_name, const char *executable, bool *updated);

/**
 * Registers a file extension.
 *
 * @ingroup Windows
 *
 * @param extension The file extension, including the leading dot.
 * @param description A readable description for the file extension.
 * @param executable_name A unique name that will used to describe the application.
 * @param executable The absolute path of the executable that will be associated with the file extension.
 * @param updated Pointer to a variable that will be set to `true`, iff the shell needs to be updated.
 *
 * @return `true` on success, `false` on failure.
 *
 * @remark The caller must later call @link shell_update @endlink, iff the shell needs to be updated.
 */
bool windows_shell_register_extension(const char *extension, const char *description, const char *executable_name, const char *executable, bool *updated);

/**
 * Registers an application.
 *
 * @ingroup Windows
 *
 * @param name Readable name of the application.
 * @param executable The absolute path of the executable being registered.
 * @param updated Pointer to a variable that will be set to `true`, iff the shell needs to be updated.
 *
 * @return `true` on success, `false` on failure.
 *
 * @remark The caller must later call @link shell_update @endlink, iff the shell needs to be updated.
 */
bool windows_shell_register_application(const char *name, const char *executable, bool *updated);

/**
 * Unregisters a protocol or file extension handler.
 *
 * @ingroup Windows
 *
 * @param shell_class The shell class to delete.
 * For protocols this is the name of the protocol.
 * For file extensions this is the program ID associated with the file extension.
 * @param updated Pointer to a variable that will be set to `true`, iff the shell needs to be updated.
 *
 * @return `true` on success, `false` on failure.
 *
 * @remark The caller must later call @link shell_update @endlink, iff the shell needs to be updated.
 */
bool windows_shell_unregister_class(const char *shell_class, bool *updated);

/**
 * Unregisters an application.
 *
 * @ingroup Windows
 *
 * @param executable The absolute path of the executable being unregistered.
 * @param updated Pointer to a variable that will be set to `true`, iff the shell needs to be updated.
 *
 * @return `true` on success, `false` on failure.
 *
 * @remark The caller must later call @link shell_update @endlink, iff the shell needs to be updated.
 */
bool windows_shell_unregister_application(const char *executable, bool *updated);

/**
 * Notifies the system that a protocol or file extension has been changed and the shell needs to be updated.
 *
 * @ingroup Windows
 *
 * @remark This is a potentially expensive operation, so it should only be called when necessary.
 */
void windows_shell_update();

#endif

#endif
