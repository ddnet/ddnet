#ifndef GAME_CLIENT_FILE_LOADER_H
#define GAME_CLIENT_FILE_LOADER_H

#include <cstdint>
#include <engine/storage.h>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

/* The purpose of this API is to allow loading of files en masse with minimal setup. A perk of presenting it in this way is
 * that it's much easier to parallelize, and an asynchronous implementation is provided in CFileLoaderAsync. You must provide,
 * at minimum, flags to define behavior of the search (can be none; see LOAD_FLAGS), a path or set of paths to search through,
 * an error callback (through which the course of action is defined when a particular error is encountered; see LOAD_ERROR)
 * and a file loaded callback (through which the file data is received). Optionally, a regular expression can be used to define
 * which files or directories to include, in order to avoid the loading of an unwanted file's data before it can be filtered
 * out in the file load callback. Files are not guaranteed to be loaded in any particular order.
 *
 * Usage:
 *      1. Construct with a valid IStorage pointer and necessary flags (see LOAD_FLAGS);
 *      2. Set file loaded and load failed callbacks (see LOAD_ERROR);
 *      3. Add paths;
 *      4. Add match regular expression (optional);
 *      5. Call implementation-specific load function
 *
 * 1.
 *      The constructor takes 2 parameters. The first is a pointer to an IStorage instance that must be valid at the time of loading.
 *      The second describes the flags that will be used in the file loading process.
 *
 * 2.
 *      The load failed callback takes 2 parameters and returns a bool. Its first parameter is a LOAD_ERROR which describes the nature
 *      of the error and what the value of the second parameter (user data) will be. The second parameter can be cast into the type
 *      specified in the LOAD_ERROR value to get error-specific data. The error callback's return value is used to determine whether
 *      or not to proceed in the file load process after a potentially non-fatal issue has been encountered.
 *
 *      You can set it by calling SetLoadFailedCallback(std::function<LoadFailedCallbackSignature> Function), where
 *      LoadFailedCallbackSignature is bool(LOAD_ERROR Error, const void *pUser).
 *
 *      The file loaded callback takes 3 parameters and has no return value. Its first parameter is the name of the file loaded, the
 *      second is a pointer to the file data, and the third is the size of the data in bytes. This data must be copied if it's meant
 *      to be retained; the pointer cannot be held after the callback is returned from. If the flags contain LOAD_FLAGS_DONT_READ_FILE,
 *      the second and third parameters will both be null.
 *
 *      You can set it by calling SetLoadFailedCallback(std::function<LoadFailedCallbackSignature> Function), where
 *      LoadFailedCallbackSignature is bool(LOAD_ERROR Error, const void *pUser).
 *
 * 3.
 *      SetPaths(...) takes any amount of paths and has no return value. Paths starting with ':' are treated
 *      as "basic" paths, a distinction made by IStorage that describes paths whose locations are not absolute and can exist in a place
 *      that only IStorage APIs know how to find. If you are not providing a basic path, it's expected that the path is absolute.
 *      For example:
 *              ":foo"; A directory called foo in the storage paths (usually the game's base directory and DDNet's application data folder).
 *              "/home/user/foo/bar"; Some other absolute path, e.g., not basic.
 *
 * 4.
 *      SetFileExtension() takes a single string that will be used to filter files to be included in the search based on whether their
 *      path ends with the provided extension. It must start with a dot (.) and have at least one character afterwards. It has no return
 *      value. If this method is not called or it's called with an invalid expression, the load process will match all files it finds and
 *      subsequently fire the file loaded callback with their data.
 *
 * 5.
 *      The implementation-specific load function has undefined parameters and return type. It will do these steps in this order.
 *              - It will check the validity of the paths provided; if any path is not valid, the load failed callback is called
 *                with LOAD_ERROR_INVALID_SEARCH_PATH.
 *              - It will check the validity of the file extension provided; if it is provided but invalid, the load failed callback
 *                is called with LOAD_ERROR_INVALID_EXTENSION.
 *              - It will begin the process of file loading, calling the file loaded callback (and subsequently triggering the file read
 *                from the disk, under normal circumstances) for every valid file.
 *
 *      In the reference implementation, it takes shape in Load(), which returns the number of files counted. This call will block
 *      its calling thread until the entire operation has completed.
 *      In the asynchronous implementation, it takes shape in LoadAsync(), which has no return value. This call will simply begin the process
 *      that fires the file loaded callback and will not block its calling thread. This has the benefit of allowing time-insensitive assets
 *      to eventually be loaded without waiting for them to load in their completion (such as skins or assets).
 *
 * Depending on whether or not the flags contain LOAD_FLAGS_ABSOLUTE_PATH, pathnames provided to the callbacks will differ. If on, you should
 * expect something like "/home/user/foo/bar.txt", without you should expect "bar.txt" for BOTH the aforementioned absolute path and any other
 * file of the same name in the search paths, such as "/home/user/foobar/foo/bar.txt". This flag is mainly intended to be used where this
 * distinction is important.
 *
 * You probably shouldn't pass this around or re-use it.
 */

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

// This is not a 'real' interface, it doesn't interact with the kernel or extend IInterface.
// I just don't know which convention to use for an abstract class.
class IMassFileLoader
{
public:
	// LOAD_ERROR represents known errors that can be encountered during the file loading process.
	// Because the file data is ultimately being processed by the caller, as the callee we don't know when to
	// or not to continue the process once a potentially non-fatal error has been encountered.
	enum LOAD_ERROR : uint8_t
	{
		LOAD_ERROR_UNKNOWN,
		// Unknown error.
		// If continued, the error is ignored but is likely to happen again.
		// pUser = nullptr

		LOAD_ERROR_NOT_INIT,
		// Implementation-specific load function called with any combination of the following issues: no file load callback, no paths, invalid IStorage pointer, or invalid flags.
		// This is the only error where the return value is inconsequential.
		// pUser = nullptr

		LOAD_ERROR_INVALID_SEARCH_PATH,
		// Path is a file, does not exist, or is malformed.
		// If continued, the path is ignored.
		// pUser = Provided invalid path (const char *)

		LOAD_ERROR_UNWANTED_SYMLINK,
		// Path is a symlink and LOAD_FLAGS_FOLLOW_SYMBOLIC_LINKS has not been set.
		// If continued, the file or directory the symlink points to is ignored.
		// pUser = Absolute path to symlink (const char *)

		// LOAD_ERROR_DIRECTORY_UNREADABLE,
		// Current user does not have read access to directory.
		// If continued, the directory is ignored.
		// pUser = Absolute directory path (const char *)

		LOAD_ERROR_FILE_UNREADABLE,
		// File within current directory is not readable by current user
		// If continued, the file is ignored.
		// pUser = Absolute file path (const char *)

		LOAD_ERROR_FILE_TOO_LARGE,
		// File is too big to have memory allocated to it without pagefile backing or something
		// If continued, the file is ignored.
		// pUser = Absolute file path (const char *)

		LOAD_ERROR_INVALID_EXTENSION,
		// File extension provided is invalid
		// If continued, the extension is disregarded and the file load callback will be called for every file in the selected paths.
		// pUser = File extension (const char *)
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

	virtual void SetFileExtension(const std::string &Extension) = 0; // Optional; will load all files otherwise

	// Files are not guaranteed to come in any particular order. Do not hold this pointer (pData). Immediately dereference and copy
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
		m_RequestedPaths.push_back(std::string(Path));
		(SetPaths(std::forward<T>(Paths)), ...);
	}
	void SetFileExtension(const std::string &Extension) override;
	void SetFileLoadedCallback(std::function<FileLoadedCallbackSignature> Function) override { m_fnFileLoadedCallback = Function; }
	void SetLoadFailedCallback(std::function<LoadFailedCallbackSignature> Function) override { m_fnLoadFailedCallback = Function; }

	unsigned int Load();

private:
	bool m_Continue = true;
	std::function<FileLoadedCallbackSignature> m_fnFileLoadedCallback;
	std::function<LoadFailedCallbackSignature> m_fnLoadFailedCallback;

	std::vector<std::string> m_RequestedPaths;
	std::unordered_map<std::string, std::vector<std::string> *> m_PathCollection; // oh geez
	std::string m_Extension;

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
// class CMassFileLoaderAsync : IMassFileLoader {
//  public:
//    void Start();

//    std::function<void(unsigned int ItemsLoaded)> fnOperationCompletedCallback;
//};

#endif // GAME_CLIENT_FILE_LOADER_H
