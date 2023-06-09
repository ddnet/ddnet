#ifndef FILE_LOADER_H
#define FILE_LOADER_H

#include <cstdint>
#include <functional>
#include <optional>
#include <regex>
#include <string>
#include <variant>
#include <vector>
#include <filesystem>
#include "engine/storage.h"

// With exceptions disabled, we can't make use of bad_function_call so we have to do something like this in order to error properly

template<typename FnArgs>
constexpr void CallbackAssert(const std::function<FnArgs> &Function)
{
	dbg_assert(bool(Function), "Mass file loader used without imlpementing callback.");
}

template<typename T, typename Fn, typename... FnArgs>
T TryCallback(std::function<Fn> Function, FnArgs... Args)
{
	CallbackAssert(Function);
	return Function(Args...);
}
template<typename Fn, typename... FnArgs>
void TryCallback(std::function<Fn> Function, FnArgs... Args)
{
	CallbackAssert(Function);
	Function(Args...);
}

/* Usage:
 * 1. Construct with a pointer to a valid IStorage instance and necessary flags (see LOAD_FLAGS);
 * 2. Set file loaded and load failed callbacks (see LOAD_ERROR);
 * 3. Add paths (prepend ":" for basic paths, e.g. ":skins");
 * 4. Add match regular expression (optional);
 * 5. Call implementation-specific load function
 *
 * You probably shouldn't pass this around or re-use it.
 */

// This is not a 'real' interface, it doesn't interact with the kernel or extend IInterface.
// I just don't know which convention to use for an abstract class.
class IMassFileLoader
{
public:
	// LOAD_ERROR represents known errors that can be encountered during the file search and load process.
	// Because the file data is ultimately being processed by the caller, as the callee we don't know when to
	// or not to continue the current 'step' once a potentially non-fatal error has been encountered.
	// Note that because SetPaths() and the implementation-specific load function are different 'steps',
	// so if you return false when fired from SetPaths() the implementation-specific load function will still start.
	enum LOAD_ERROR : uint8_t
	{
		LOAD_ERROR_UNKNOWN, // Unknown error.
				    // If continued, the error is ignored but is likely to happen again.
				    // pUser = nullptr

		LOAD_ERROR_NOT_INIT, // Implementation-specific load function called with any combination of the following issues: no file load callback, no paths, invalid IStorage pointer, or invalid flags.
				     // This is the only error where the return value is inconsequential.
				     // pUser = nullptr

		LOAD_ERROR_INVALID_SEARCH_PATH, // Path is a file, does not exist, or is malformed.
						// If continued, the path is ignored.
						// pUser = Provided invalid path (const char *)

		LOAD_ERROR_UNWANTED_SYMLINK, // Path is a symlink and LOAD_FLAGS_FOLLOW_SYMBOLIC_LINKS has not been set.
					     // If continued, the file or directory the symlink points to is ignored.
					     // pUser = Absolute path to symlink (const char *)

		LOAD_ERROR_DIRECTORY_UNREADABLE, // Current user does not have read access to directory.
						 // If continued, the directory is ignored.
						 // pUser = Absolute directory path (const char *)

		LOAD_ERROR_FILE_UNREADABLE, // File within current directory is not readable by current user
					    // If continued, the file is ignored.
					    // pUser = Absolute file path (const char *)

		LOAD_ERROR_FILE_TOO_LARGE, // File is too big to have memory allocated to it without pagefile backing or something
					   // If continued, the file is ignored.
					   // pUser = Absolute file path (const char *)

		LOAD_ERROR_INVALID_REGEX, // Regular expression provided is malformed
					  // If continued, the regex is disregarded and the file load callback will be called for every file in the selected paths.
					  // pUser = Regex match expression (const char *)
		// This error is currently unused in the reference implementation because exceptions are off and std::regex's only error checking mechanism is through the std::regex_error exception.
	};

	// All flags are opt-in
	enum LOAD_FLAGS : uint8_t
	{
		LOAD_FLAGS_NONE = 0b00000000, // For comparison
		LOAD_FLAGS_MASK = 0b00001111, // For comparison

		// clang-format off
		LOAD_FLAGS_RECURSE_SUBDIRECTORIES =	0b00000001,	// Enter directories of any of the provided directories when loading
		LOAD_FLAGS_FOLLOW_SYMBOLIC_LINKS =	0b00000010,	// Follow symlinks (may hang)
		LOAD_FLAGS_ABSOLUTE_PATH =		0b00000100,	// Return an absolute file path instead of just the filename. This will influence the regex.
									// This is useful for weeding out duplicates because there can be multiple storage paths the game searches through for one provided pathname (e.g. "skins")
		LOAD_FLAGS_DONT_READ_FILE =		0b00001000,	// Do not read file contents.
									// This is useful if you are doing a dry-run of a path or set of paths. pData and Size in the file loaded callback will be null.
		// clang-format on
	};

	IMassFileLoader(uint8_t Flags) { m_Flags = Flags; };

	// Set the paths the loader will search through.
	//    virtual void SetPaths(const std::initializer_list<std::string> &vPaths /* aPaths? */) = 0;
	//    virtual void SetPaths(const Ts ...vPaths /* aPaths? */) = 0;
	virtual void SetMatchExpression(const std::string &Match) = 0; // Optional; will load all files otherwise

	// Do not hold this pointer (pData). Immediately dereference and copy
	using FileLoadedCallbackSignature = void(const std::string &ItemName, const unsigned char *pData, const unsigned int Size);
	virtual void SetFileLoadedCallback(std::function<FileLoadedCallbackSignature> Function) = 0;

	// Fatal error = true, nonfatal = false
	using LoadFailedCallbackSignature = bool(LOAD_ERROR Error, const void *pUser);
	virtual void SetLoadFailedCallback(std::function<LoadFailedCallbackSignature> Function) = 0;

protected:
	uint8_t m_Flags = LOAD_FLAGS_NONE; // LOAD_FLAGS
};

// Reference implementation, single-threaded
class CMassFileLoader : IMassFileLoader
{
public:
	CMassFileLoader(IStorage *pStorage, uint8_t Flags = LOAD_FLAGS_NONE);
	~CMassFileLoader();

	template<typename T, typename... Ts>
	void SetPaths(T Path, Ts... Paths)
	{
		if(m_Continue)
		{
			std::string PathStr(Path);
			static char aPathBuffer[IO_MAX_PATH_LENGTH];
			int StorageType = PathStr.find(':') == 0 ? IStorage::STORAGETYPE_BASIC : IStorage::STORAGETYPE_CLIENT;
			if(StorageType == IStorage::STORAGETYPE_BASIC)
				PathStr.erase(0, 1);
			m_pStorage->GetCompletePath(StorageType, PathStr.c_str(), aPathBuffer, sizeof(aPathBuffer));
			if(fs_is_dir(aPathBuffer)) // Exists and is a directory
			{
				if(fs_is_readable(aPathBuffer))
					m_PathCollection.insert({std::string(aPathBuffer), new std::vector<std::string>});
				else
					m_Continue = TryCallback<bool>(m_fnLoadFailedCallback, LOAD_ERROR_DIRECTORY_UNREADABLE, PathStr.c_str());
			}
			else
				m_Continue = TryCallback<bool>(m_fnLoadFailedCallback, LOAD_ERROR_INVALID_SEARCH_PATH, PathStr.c_str());
		}
		(SetPaths(std::forward<T>(Paths)), ...);
	}
	void SetMatchExpression(const std::string &Match) override;
	void SetFileLoadedCallback(std::function<FileLoadedCallbackSignature> Function) override { m_fnFileLoadedCallback = Function; }
	void SetLoadFailedCallback(std::function<LoadFailedCallbackSignature> Function) override { m_fnLoadFailedCallback = Function; }

	unsigned int Load();

private:
	bool m_Continue = true;
	std::function<FileLoadedCallbackSignature> m_fnFileLoadedCallback;
	std::function<LoadFailedCallbackSignature> m_fnLoadFailedCallback;

	std::unordered_map<std::string, std::vector<std::string> *> m_PathCollection; // oh geez
	std::optional<std::regex> m_Regex = std::nullopt;

	IStorage *m_pStorage = nullptr;

	static int ListDirectoryCallback(const char *Name, int IsDir, int, void *User);
	using FileIndex = std::vector<std::string>;
	struct SListDirectoryCallbackUserInfo
	{
		std::string *m_pCurrentDirectory;
		CMassFileLoader *m_pThis;
		bool *m_pContinue;
	};
};

// Multi-threaded implementation (WIP)
//class CMassFileLoaderAsync : IMassFileLoader {
//  public:
//    void Start();

//    std::function<void(unsigned int ItemsLoaded)> fnOperationCompletedCallback;
//};

#endif // FILE_LOADER_H
