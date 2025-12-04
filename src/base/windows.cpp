/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "windows.h"

#if defined(CONF_FAMILY_WINDOWS)

#include "system.h"

#include <shlobj.h> // SHChangeNotify
#include <windows.h>

std::string windows_format_system_message(unsigned long error)
{
	WCHAR *wide_message;
	const DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_MAX_WIDTH_MASK;
	if(FormatMessageW(flags, nullptr, error, 0, (LPWSTR)&wide_message, 0, nullptr) == 0)
		return "unknown error";

	std::optional<std::string> message = windows_wide_to_utf8(wide_message);
	LocalFree(wide_message);
	return message.value_or("(invalid UTF-16 in error message)");
}

std::wstring windows_args_to_wide(const char **arguments, size_t num_arguments)
{
	std::wstring wide_arguments;

	for(size_t i = 0; i < num_arguments; ++i)
	{
		if(i > 0)
		{
			wide_arguments += L' ';
		}

		const std::wstring wide_arg = windows_utf8_to_wide(arguments[i]);
		wide_arguments += L'"';

		size_t backslashes = 0;
		for(const wchar_t c : wide_arg)
		{
			if(c == L'\\')
			{
				backslashes++;
			}
			else
			{
				if(c == L'"')
				{
					// Add n+1 backslashes to total 2n+1 before internal '"'
					for(size_t j = 0; j <= backslashes; ++j)
					{
						wide_arguments += L'\\';
					}
				}
				backslashes = 0;
			}
			wide_arguments += c;
		}

		// Add n backslashes to total 2n before ending '"'
		for(size_t j = 0; j < backslashes; ++j)
		{
			wide_arguments += L'\\';
		}
		wide_arguments += L'"';
	}

	return wide_arguments;
}

std::wstring windows_utf8_to_wide(const char *str)
{
	const int orig_length = str_length(str);
	if(orig_length == 0)
		return L"";
	const int size_needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, orig_length, nullptr, 0);
	dbg_assert(size_needed > 0, "Invalid UTF-8 passed to windows_utf8_to_wide");
	std::wstring wide_string(size_needed, L'\0');
	dbg_assert(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, orig_length, wide_string.data(), size_needed) == size_needed, "MultiByteToWideChar failure");
	return wide_string;
}

std::optional<std::string> windows_wide_to_utf8(const wchar_t *wide_str)
{
	const int orig_length = wcslen(wide_str);
	if(orig_length == 0)
		return "";
	const int size_needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_str, orig_length, nullptr, 0, nullptr, nullptr);
	if(size_needed == 0)
		return {};
	std::string string(size_needed, '\0');
	dbg_assert(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_str, orig_length, string.data(), size_needed, nullptr, nullptr) == size_needed, "WideCharToMultiByte failure");
	return string;
}

// See https://learn.microsoft.com/en-us/windows/win32/learnwin32/initializing-the-com-library
CWindowsComLifecycle::CWindowsComLifecycle(bool HasWindow)
{
	HRESULT result = CoInitializeEx(nullptr, (HasWindow ? COINIT_APARTMENTTHREADED : COINIT_MULTITHREADED) | COINIT_DISABLE_OLE1DDE);
	dbg_assert(result != S_FALSE, "COM library already initialized on this thread");
	dbg_assert(result == S_OK, "COM library initialization failed");
}
CWindowsComLifecycle::~CWindowsComLifecycle()
{
	CoUninitialize();
}

static void windows_print_error(const char *system, const char *prefix, HRESULT error)
{
	const std::string message = windows_format_system_message(error);
	dbg_msg(system, "%s: %s", prefix, message.c_str());
}

static std::wstring filename_from_path(const std::wstring &path)
{
	const size_t pos = path.find_last_of(L"/\\");
	return pos == std::wstring::npos ? path : path.substr(pos + 1);
}

bool windows_shell_register_protocol(const char *protocol_name, const char *executable, bool *updated)
{
	const std::wstring protocol_name_wide = windows_utf8_to_wide(protocol_name);
	const std::wstring executable_wide = windows_utf8_to_wide(executable);

	// Open registry key for protocol associations of the current user
	HKEY handle_subkey_classes;
	const LRESULT result_subkey_classes = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Classes", 0, KEY_ALL_ACCESS, &handle_subkey_classes);
	if(result_subkey_classes != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_protocol", "Error opening registry key", result_subkey_classes);
		return false;
	}

	// Create the protocol key
	HKEY handle_subkey_protocol;
	const LRESULT result_subkey_protocol = RegCreateKeyExW(handle_subkey_classes, protocol_name_wide.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &handle_subkey_protocol, nullptr);
	RegCloseKey(handle_subkey_classes);
	if(result_subkey_protocol != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_protocol", "Error creating registry key", result_subkey_protocol);
		return false;
	}

	// Set the default value for the key, which specifies the name of the display name of the protocol
	const std::wstring value_protocol = L"URL:" + protocol_name_wide + L" Protocol";
	const LRESULT result_value_protocol = RegSetValueExW(handle_subkey_protocol, L"", 0, REG_SZ, (BYTE *)value_protocol.c_str(), (value_protocol.length() + 1) * sizeof(wchar_t));
	if(result_value_protocol != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_protocol", "Error setting registry value", result_value_protocol);
		RegCloseKey(handle_subkey_protocol);
		return false;
	}

	// Set the "URL Protocol" value, to specify that this key describes a URL protocol
	const LRESULT result_value_empty = RegSetValueEx(handle_subkey_protocol, L"URL Protocol", 0, REG_SZ, (BYTE *)L"", sizeof(wchar_t));
	if(result_value_empty != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_protocol", "Error setting registry value", result_value_empty);
		RegCloseKey(handle_subkey_protocol);
		return false;
	}

	// Create the "DefaultIcon" subkey
	HKEY handle_subkey_icon;
	const LRESULT result_subkey_icon = RegCreateKeyExW(handle_subkey_protocol, L"DefaultIcon", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &handle_subkey_icon, nullptr);
	if(result_subkey_icon != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_protocol", "Error creating registry key", result_subkey_icon);
		RegCloseKey(handle_subkey_protocol);
		return false;
	}

	// Set the default value for the key, which specifies the icon associated with the protocol
	const std::wstring value_icon = L"\"" + executable_wide + L"\",0";
	const LRESULT result_value_icon = RegSetValueExW(handle_subkey_icon, L"", 0, REG_SZ, (BYTE *)value_icon.c_str(), (value_icon.length() + 1) * sizeof(wchar_t));
	RegCloseKey(handle_subkey_icon);
	if(result_value_icon != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_protocol", "Error setting registry value", result_value_icon);
		RegCloseKey(handle_subkey_protocol);
		return false;
	}

	// Create the "shell\open\command" subkeys
	HKEY handle_subkey_shell_open_command;
	const LRESULT result_subkey_shell_open_command = RegCreateKeyExW(handle_subkey_protocol, L"shell\\open\\command", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &handle_subkey_shell_open_command, nullptr);
	RegCloseKey(handle_subkey_protocol);
	if(result_subkey_shell_open_command != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_protocol", "Error creating registry key", result_subkey_shell_open_command);
		return false;
	}

	// Get the previous default value for the key, so we can determine if it changed
	wchar_t old_value_executable[MAX_PATH + 16];
	DWORD old_size_executable = sizeof(old_value_executable);
	const LRESULT result_old_value_executable = RegGetValueW(handle_subkey_shell_open_command, nullptr, L"", RRF_RT_REG_SZ, nullptr, (BYTE *)old_value_executable, &old_size_executable);
	const std::wstring value_executable = L"\"" + executable_wide + L"\" \"%1\"";
	if(result_old_value_executable != ERROR_SUCCESS || wcscmp(old_value_executable, value_executable.c_str()) != 0)
	{
		// Set the default value for the key, which specifies the executable command associated with the protocol
		const LRESULT result_value_executable = RegSetValueExW(handle_subkey_shell_open_command, L"", 0, REG_SZ, (BYTE *)value_executable.c_str(), (value_executable.length() + 1) * sizeof(wchar_t));
		RegCloseKey(handle_subkey_shell_open_command);
		if(result_value_executable != ERROR_SUCCESS)
		{
			windows_print_error("shell_register_protocol", "Error setting registry value", result_value_executable);
			return false;
		}

		*updated = true;
	}
	else
	{
		RegCloseKey(handle_subkey_shell_open_command);
	}

	return true;
}

bool windows_shell_register_extension(const char *extension, const char *description, const char *executable_name, const char *executable, bool *updated)
{
	const std::wstring extension_wide = windows_utf8_to_wide(extension);
	const std::wstring executable_name_wide = windows_utf8_to_wide(executable_name);
	const std::wstring description_wide = executable_name_wide + L" " + windows_utf8_to_wide(description);
	const std::wstring program_id_wide = executable_name_wide + extension_wide;
	const std::wstring executable_wide = windows_utf8_to_wide(executable);

	// Open registry key for file associations of the current user
	HKEY handle_subkey_classes;
	const LRESULT result_subkey_classes = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Classes", 0, KEY_ALL_ACCESS, &handle_subkey_classes);
	if(result_subkey_classes != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_extension", "Error opening registry key", result_subkey_classes);
		return false;
	}

	// Create the program ID key
	HKEY handle_subkey_program_id;
	const LRESULT result_subkey_program_id = RegCreateKeyExW(handle_subkey_classes, program_id_wide.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &handle_subkey_program_id, nullptr);
	if(result_subkey_program_id != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_extension", "Error creating registry key", result_subkey_program_id);
		RegCloseKey(handle_subkey_classes);
		return false;
	}

	// Set the default value for the key, which specifies the file type description for legacy applications
	const LRESULT result_description_default = RegSetValueExW(handle_subkey_program_id, L"", 0, REG_SZ, (BYTE *)description_wide.c_str(), (description_wide.length() + 1) * sizeof(wchar_t));
	if(result_description_default != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_extension", "Error setting registry value", result_description_default);
		RegCloseKey(handle_subkey_program_id);
		RegCloseKey(handle_subkey_classes);
		return false;
	}

	// Set the "FriendlyTypeName" value, which specifies the file type description for modern applications
	const LRESULT result_description_friendly = RegSetValueExW(handle_subkey_program_id, L"FriendlyTypeName", 0, REG_SZ, (BYTE *)description_wide.c_str(), (description_wide.length() + 1) * sizeof(wchar_t));
	if(result_description_friendly != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_extension", "Error setting registry value", result_description_friendly);
		RegCloseKey(handle_subkey_program_id);
		RegCloseKey(handle_subkey_classes);
		return false;
	}

	// Create the "DefaultIcon" subkey
	HKEY handle_subkey_icon;
	const LRESULT result_subkey_icon = RegCreateKeyExW(handle_subkey_program_id, L"DefaultIcon", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &handle_subkey_icon, nullptr);
	if(result_subkey_icon != ERROR_SUCCESS)
	{
		windows_print_error("register_protocol", "Error creating registry key", result_subkey_icon);
		RegCloseKey(handle_subkey_program_id);
		RegCloseKey(handle_subkey_classes);
		return false;
	}

	// Set the default value for the key, which specifies the icon associated with the program ID
	const std::wstring value_icon = L"\"" + executable_wide + L"\",0";
	const LRESULT result_value_icon = RegSetValueExW(handle_subkey_icon, L"", 0, REG_SZ, (BYTE *)value_icon.c_str(), (value_icon.length() + 1) * sizeof(wchar_t));
	RegCloseKey(handle_subkey_icon);
	if(result_value_icon != ERROR_SUCCESS)
	{
		windows_print_error("register_protocol", "Error setting registry value", result_value_icon);
		RegCloseKey(handle_subkey_program_id);
		RegCloseKey(handle_subkey_classes);
		return false;
	}

	// Create the "shell\open\command" subkeys
	HKEY handle_subkey_shell_open_command;
	const LRESULT result_subkey_shell_open_command = RegCreateKeyExW(handle_subkey_program_id, L"shell\\open\\command", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &handle_subkey_shell_open_command, nullptr);
	RegCloseKey(handle_subkey_program_id);
	if(result_subkey_shell_open_command != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_extension", "Error creating registry key", result_subkey_shell_open_command);
		RegCloseKey(handle_subkey_classes);
		return false;
	}

	// Get the previous default value for the key, so we can determine if it changed
	wchar_t old_value_executable[MAX_PATH + 16];
	DWORD old_size_executable = sizeof(old_value_executable);
	const LRESULT result_old_value_executable = RegGetValueW(handle_subkey_shell_open_command, nullptr, L"", RRF_RT_REG_SZ, nullptr, (BYTE *)old_value_executable, &old_size_executable);
	const std::wstring value_executable = L"\"" + executable_wide + L"\" \"%1\"";
	if(result_old_value_executable != ERROR_SUCCESS || wcscmp(old_value_executable, value_executable.c_str()) != 0)
	{
		// Set the default value for the key, which specifies the executable command associated with the application
		const LRESULT result_value_executable = RegSetValueExW(handle_subkey_shell_open_command, L"", 0, REG_SZ, (BYTE *)value_executable.c_str(), (value_executable.length() + 1) * sizeof(wchar_t));
		RegCloseKey(handle_subkey_shell_open_command);
		if(result_value_executable != ERROR_SUCCESS)
		{
			windows_print_error("shell_register_extension", "Error setting registry value", result_value_executable);
			RegCloseKey(handle_subkey_classes);
			return false;
		}

		*updated = true;
	}

	// Create the file extension key
	HKEY handle_subkey_extension;
	const LRESULT result_subkey_extension = RegCreateKeyExW(handle_subkey_classes, extension_wide.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &handle_subkey_extension, nullptr);
	RegCloseKey(handle_subkey_classes);
	if(result_subkey_extension != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_extension", "Error creating registry key", result_subkey_extension);
		return false;
	}

	// Get the previous default value for the key, so we can determine if it changed
	wchar_t old_value_application[128];
	DWORD old_size_application = sizeof(old_value_application);
	const LRESULT result_old_value_application = RegGetValueW(handle_subkey_extension, nullptr, L"", RRF_RT_REG_SZ, nullptr, (BYTE *)old_value_application, &old_size_application);
	if(result_old_value_application != ERROR_SUCCESS || wcscmp(old_value_application, program_id_wide.c_str()) != 0)
	{
		// Set the default value for the key, which associates the file extension with the program ID
		const LRESULT result_value_application = RegSetValueExW(handle_subkey_extension, L"", 0, REG_SZ, (BYTE *)program_id_wide.c_str(), (program_id_wide.length() + 1) * sizeof(wchar_t));
		RegCloseKey(handle_subkey_extension);
		if(result_value_application != ERROR_SUCCESS)
		{
			windows_print_error("shell_register_extension", "Error setting registry value", result_value_application);
			return false;
		}

		*updated = true;
	}
	else
	{
		RegCloseKey(handle_subkey_extension);
	}

	return true;
}

bool windows_shell_register_application(const char *name, const char *executable, bool *updated)
{
	const std::wstring name_wide = windows_utf8_to_wide(name);
	const std::wstring executable_filename = filename_from_path(windows_utf8_to_wide(executable));

	// Open registry key for application registrations
	HKEY handle_subkey_applications;
	const LRESULT result_subkey_applications = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Classes\\Applications", 0, KEY_ALL_ACCESS, &handle_subkey_applications);
	if(result_subkey_applications != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_application", "Error opening registry key", result_subkey_applications);
		return false;
	}

	// Create the program key
	HKEY handle_subkey_program;
	const LRESULT result_subkey_program = RegCreateKeyExW(handle_subkey_applications, executable_filename.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &handle_subkey_program, nullptr);
	RegCloseKey(handle_subkey_applications);
	if(result_subkey_program != ERROR_SUCCESS)
	{
		windows_print_error("shell_register_application", "Error creating registry key", result_subkey_program);
		return false;
	}

	// Get the previous default value for the key, so we can determine if it changed
	wchar_t old_value_executable[MAX_PATH];
	DWORD old_size_executable = sizeof(old_value_executable);
	const LRESULT result_old_value_executable = RegGetValueW(handle_subkey_program, nullptr, L"FriendlyAppName", RRF_RT_REG_SZ, nullptr, (BYTE *)old_value_executable, &old_size_executable);
	if(result_old_value_executable != ERROR_SUCCESS || wcscmp(old_value_executable, name_wide.c_str()) != 0)
	{
		// Set the "FriendlyAppName" value, which specifies the displayed name of the application
		const LRESULT result_program_name = RegSetValueExW(handle_subkey_program, L"FriendlyAppName", 0, REG_SZ, (BYTE *)name_wide.c_str(), (name_wide.length() + 1) * sizeof(wchar_t));
		RegCloseKey(handle_subkey_program);
		if(result_program_name != ERROR_SUCCESS)
		{
			windows_print_error("shell_register_application", "Error setting registry value", result_program_name);
			return false;
		}

		*updated = true;
	}
	else
	{
		RegCloseKey(handle_subkey_program);
	}

	return true;
}

bool windows_shell_unregister_class(const char *shell_class, bool *updated)
{
	const std::wstring class_wide = windows_utf8_to_wide(shell_class);

	// Open registry key for protocol and file associations of the current user
	HKEY handle_subkey_classes;
	const LRESULT result_subkey_classes = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Classes", 0, KEY_ALL_ACCESS, &handle_subkey_classes);
	if(result_subkey_classes != ERROR_SUCCESS)
	{
		windows_print_error("shell_unregister_class", "Error opening registry key", result_subkey_classes);
		return false;
	}

	// Delete the registry keys for the shell class (protocol or program ID)
	LRESULT result_delete = RegDeleteTreeW(handle_subkey_classes, class_wide.c_str());
	RegCloseKey(handle_subkey_classes);
	if(result_delete == ERROR_SUCCESS)
	{
		*updated = true;
	}
	else if(result_delete != ERROR_FILE_NOT_FOUND)
	{
		windows_print_error("shell_unregister_class", "Error deleting registry key", result_delete);
		return false;
	}

	return true;
}

bool windows_shell_unregister_application(const char *executable, bool *updated)
{
	const std::wstring executable_filename = filename_from_path(windows_utf8_to_wide(executable));

	// Open registry key for application registrations
	HKEY handle_subkey_applications;
	const LRESULT result_subkey_applications = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Classes\\Applications", 0, KEY_ALL_ACCESS, &handle_subkey_applications);
	if(result_subkey_applications != ERROR_SUCCESS)
	{
		windows_print_error("shell_unregister_application", "Error opening registry key", result_subkey_applications);
		return false;
	}

	// Delete the registry keys for the application description
	LRESULT result_delete = RegDeleteTreeW(handle_subkey_applications, executable_filename.c_str());
	RegCloseKey(handle_subkey_applications);
	if(result_delete == ERROR_SUCCESS)
	{
		*updated = true;
	}
	else if(result_delete != ERROR_FILE_NOT_FOUND)
	{
		windows_print_error("shell_unregister_application", "Error deleting registry key", result_delete);
		return false;
	}

	return true;
}

void windows_shell_update()
{
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

#endif
