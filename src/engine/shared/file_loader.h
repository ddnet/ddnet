#ifndef ENGINE_SHARED_FILE_LOADER_H
#define ENGINE_SHARED_FILE_LOADER_H

#include "config.h"
#include "console.h"
#include "engine/engine.h"
#include "engine/shared/uuid_manager.h"
#include "engine/storage.h"
#include "jobs.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/* The purpose of this API is to allow loading of files en masse with minimal setup. A perk of presenting it in this
 * way is that it's much easier to parallelize, and asynchronous functionality is provided and enabled with
 * LOAD_FLAG_ASYNC in ELoadFlags. You must provide, at minimum, flags to define behavior of the search (can be none;
 * see ELoadFlags), a path or set of paths to search through, an error callback (through which the course of action is
 * defined when a particular error is encountered; see ELoadError), a file loaded callback (through which the file data
 * is received), and a load finished callback to inform you that the operation has finished if using LOAD_FLAG_ASYNC.
 * Optionally, a file extension can be provided to act as a filter for files or directories to include, in order to
 * avoid the loading of an unwanted file's data before it can be filtered out in the file load callback. Files are not
 * guaranteed to be loaded in any particular order.
 *
 * Usage:
 *    1. Construct with a valid IStorage pointer, IEngine pointer (for async) and necessary flags (see ELoadFlags);
 *    2. Set file loaded, load failed (see ELoadError), and load finished callbacks (for async);
 *    3. Add paths;
 *    4. Add file extension (optional);
 *    5. Call Load();
 *
 * 1.
 *    The constructor takes 3 parameters. The first is a pointer to an IEngine instance that is only needed if the
 *    flags contain LOAD_FLAG_ASYNC; the second is a pointer to an IStorage instance that must be valid at the time
 *    Load() is called; the third describes the flags that will be used in the file loading process.
 *
 * 2.
 *    The load failed callback takes 3 parameters and returns a boolean. Its first parameter is a ELoadError which
 *    describes the nature of the error and what the value of the second parameter (user data) will be. The second
 *    parameter can be cast into the type specified near the respective ELoadError constant to get error-specific data.
 *    The third parameter is user data. The error callback's return value is used to determine whether or not to
 *    proceed in the file load process after a potentially non-fatal issue has been encountered.
 *
 *    You can set it by calling SetLoadFailedCallback(std::function<LoadFailedCallbackSignature> Function)
 *
 *    The file loaded callback takes 4 parameters and has no return value. Its first parameter is the name of the file
 *    loaded, the second is a pointer to the file data, the third is the size of the data in bytes, and the fourth is
 *    user data. This data must be copied if it's meant to be retained; the pointer cannot be held after the callback
 *    is returned from. If the flags contain LOAD_FLAGS_DONT_READ_FILE, the second and third parameters will both be
 *    null.
 *
 *    You can set it by calling SetFileLoadedCallback(std::function<FileLoadedCallbackSignature> Function)
 *
 *    The load finished callback takes 2 parameters and has no return value. Its first parameter is the number of files
 *    loaded and the second is user data. This callback is only fired if the flags contain LOAD_FLAG_ASYNC. If using
 *    the file loader synchronously, you can instead use the return value of Load() to get the number of items loaded.
 *
 *    You can set it by calling SetLoadFinishedCallback(std::function<LoadFinishedCallbackSignature> Function)
 *
 * 3.
 *    SetPaths(...) takes any amount of paths and has no return value. Paths starting with ':' are treated as "basic"
 *    paths, a distinction made by IStorage that describes paths whose locations are not absolute and can exist in a
 *    place that only IStorage APIs know how to find. If you are not providing a basic path, it's expected that the
 *    path is absolute.
 *    For example:
 *        ":foo"; A directory called foo in the storage paths (the game's working directory and its app data folder).
 *        "/home/user/foo/bar"; Some other absolute path, e.g., not basic.
 *
 * 4.
 *    SetFileExtension() takes a single string that will be used to filter files to be included in the search based on
 *    whether their path ends with the provided extension. It must start with a dot (.) and have at least one character
 *    afterwards. It has no return value. If this method is not called or it's called with an invalid expression, the
 *    load process will match all files it finds and subsequently fire the file loaded callback with their data.
 *
 * 5.
 *    Load() has no parameters and a return type which varies depending on the use case. It will do these steps in this
 *    order.
 *        - It will check the validity of the paths provided; if any path is not valid, the load failed callback is
 *          called with LOAD_ERROR_INVALID_SEARCH_PATH.
 *        - It will check the validity of the file extension provided; if it is provided but invalid, the load failed
 *          callback is called with LOAD_ERROR_INVALID_EXTENSION.
 *        - It will begin the process of file loading, calling the file loaded callback (and subsequently triggering
 *          the file read from the disk, under normal circumstances) for every valid file.
 *
 *    If the flags contain LOAD_FLAG_ASYNC, there will be no return value. This call will begin the process of file
 *    loading and immediately return. The end of the process can be determined by the firing of the load finished
 *    callback. This has the benefit of allowing time-insensitive assets to eventually be loaded without waiting for
 *    them to load in their completion (such as skins or assets). If the flags do not contain LOAD_FLAG_ASYNC, it will
 *    return the number of files loaded. This function will block until the file loading process is complete.
 *
 * Depending on whether or not the flags contain LOAD_FLAGS_ABSOLUTE_PATH, pathnames provided to the callbacks will
 * differ. If the flag is provided, expect something like "/home/user/foo/bar.txt". If not provided, expect "bar.txt"
 * for BOTH the aforementioned absolute path and any other file of the same name in the search paths, such as
 * "/home/user/foobar/foo/bar.txt". This flag is mainly intended to be used where the distinction between files of the
 * same name is important.
 *
 * You probably should not pass class this around or re-use it.
 */

// With exceptions disabled, we can't make use of bad_function_call so we have to do something like this in order to
// error properly
template<typename FnArgs>
constexpr void CallbackAssert(const std::function<FnArgs> &Function)
{
	dbg_assert(bool(Function), "Mass file loader used without imlpementing callback.");
}

template<typename Fn, typename... FnArgs>
[[maybe_unused]] bool TryCallback(std::function<Fn> Function, FnArgs... Args)
{
	CallbackAssert(Function);
	return Function(Args...);
}

class CMassFileLoader;

class CFileLoadJob : public IJob
{
protected:
	void (*m_Function)(void *);
	void *m_pData;
	void Run() override
	{
		m_Status = FILE_LOAD_JOB_STATUS_RUNNING;
		m_Function(m_pData);
	}

public:
	CFileLoadJob(void (*Function)(void *), void *pData)
	{
		m_pData = pData;
		m_Function = Function;
	}
	std::atomic<int> m_Status = FILE_LOAD_JOB_STATUS_PENDING;

	virtual ~CFileLoadJob() = default;

	enum
	{
		FILE_LOAD_JOB_STATUS_PENDING = IJob::STATE_PENDING,
		FILE_LOAD_JOB_STATUS_RUNNING = IJob::STATE_RUNNING,
		FILE_LOAD_JOB_STATUS_DONE = IJob::STATE_DONE,
		FILE_LOAD_JOB_STATUS_YIELD = IJob::STATE_DONE + 1
	};
};

class CMassFileLoader
{
public:
	// ELoadError represents known errors that can be encountered during the file loading process. Because the file
	// data is ultimately being processed by the caller, as the callee we don't know when to or not to continue the
	// process once a potentially non-fatal error has been encountered.
	enum ELoadError : uint8_t
	{
		LOAD_ERROR_UNKNOWN,
		// Unknown error.
		// If continued, the error is ignored but is likely to happen again.
		// pUser = nullptr

		LOAD_ERROR_INVALID_SEARCH_PATH,
		// Path describes a file, a directory which does not exist, or is malformed.
		// If continued, the path is ignored.
		// pUser = Provided invalid path (const char *)

		LOAD_ERROR_FILE_UNREADABLE,
		// File within current directory is not readable by current user
		// If continued, the file is ignored.
		// pUser = Absolute file path (const char *)

		LOAD_ERROR_FILE_TOO_LARGE,
		// File is too big, either requiring too much memory to be allocated to it without something like
		// pagefile backing, or it is bigger than 2GiB on a system which DDNet cannot address as much memory as
		// required in order to load it (e.g., 32 bit)
		// If continued, the file is ignored.
		// pUser = Absolute file path (const char *)

		LOAD_ERROR_INVALID_EXTENSION,
		// File extension provided is invalid
		// If continued, the extension is disregarded and the file load callback will be called for every file
		// in the selected paths.
		// pUser = File extension (const char *)
	};

	// All flags are opt-in
	enum ELoadFlags : uint8_t
	{
		LOAD_FLAGS_NONE = 0b00000000, // For comparison
		LOAD_FLAGS_MASK = 0b00011111, // For comparison

		// clang-format off
 
 		// Enter directories of any of the provided directories when loading
 		LOAD_FLAGS_RECURSE_SUBDIRECTORIES =	0b00000001,
 
 		// Return an absolute file path instead of just the filename. This will influence the regex. This is
 		// useful for weeding out duplicates because there can be multiple storage paths the game searches
 		// through for one provided pathname (e.g. "skins")
 		LOAD_FLAGS_ABSOLUTE_PATH =		0b00000010,
 
 		// Do not read file contents. This is useful if you are doing a dry-run of a path or set of paths.
 		// pData and Size in the file loaded callback will be null.
 		LOAD_FLAGS_DONT_READ_FILE =		0b00000100,
 
 		// Whether or not to use skip the UTF-8 BOM
 		LOAD_FLAGS_SKIP_BOM =			0b00001000,
 		
 		// Load asynchronously instead; return value of Begin() is not used and a file load finished callback
 		// is fired instead
 		LOAD_FLAGS_ASYNC =			0b00010000,

		// clang-format on
	};

	void SetFileExtension(std::string_view Extension); // Optional; will load all files otherwise
	void SetUserData(void *pUser);

	// Files are not guaranteed to come in any particular order. Do not hold this pointer (pData). Immediately
	// dereference and copy
	using FileLoadedCallbackSignature = void(const std::string_view ItemName,
		const unsigned char *pData,
		const unsigned int Size,
		void *pUser);
	void SetFileLoadedCallback(std::function<FileLoadedCallbackSignature> Function);

	// Fatal error = true, nonfatal = false
	using LoadFailedCallbackSignature = bool(ELoadError Error, const void *pData, void *pUser);
	void SetLoadFailedCallback(std::function<LoadFailedCallbackSignature> Function);

	// If this is implemented, it is expected that the object is deleted by the caller.
	using LoadFinishedCallbackSignature = void(unsigned int Count, void *pUser);
	void SetLoadFinishedCallback(std::function<LoadFinishedCallbackSignature> Function);

	explicit CMassFileLoader(IEngine *pEngine, IStorage *pStorage, uint8_t Flags = LOAD_FLAGS_NONE);
	~CMassFileLoader();

	template<typename T, typename... Ts>
	void SetPaths(T Path, Ts... Paths)
	{
		m_RequestedPaths.push_back(std::string(Path));
		(SetPaths(std::forward<T>(Paths)), ...);
	}
	int GetJobStatus() { return m_FileLoadJob ? m_FileLoadJob->m_Status.load() : CFileLoadJob::FILE_LOAD_JOB_STATUS_PENDING; }
	void SetJobStatus(int Status)
	{
		if(m_FileLoadJob)
			m_FileLoadJob->m_Status.store(Status);
	}

	std::optional<unsigned int> Load();

protected:
	static int ListDirectoryCallback(const char *Name, int IsDir, int, void *User);
	static unsigned int Begin(CMassFileLoader *pUserData);
	inline static bool CompareExtension(const std::filesystem::path &Filename,
		std::string_view Extension);

	void *m_pUser = nullptr;
	uint8_t m_Flags = LOAD_FLAGS_NONE; // LOAD_FLAGS

private:
	bool m_Continue = true, m_Finished = false;
	std::function<FileLoadedCallbackSignature> m_fnFileLoadedCallback;
	std::function<LoadFailedCallbackSignature> m_fnLoadFailedCallback;

	std::vector<std::string> m_RequestedPaths;
	std::unordered_map<std::string, std::vector<std::string> *> m_PathCollection; // oh geez
	char *m_pExtension = nullptr;

	IEngine *m_pEngine = nullptr;
	IStorage *m_pStorage = nullptr;

	using FileIndex = std::vector<std::string>;
	struct SListDirectoryCallbackUserInfo
	{
		const char *m_pCurrentDirectory;
		CMassFileLoader *m_pThis;
		bool *m_pContinue;
	};

	// async specific
	std::shared_ptr<CFileLoadJob> m_FileLoadJob;
	std::function<LoadFinishedCallbackSignature> m_fnLoadFinishedCallback;
};

inline void CMassFileLoader::SetUserData(void *pUser) { m_pUser = pUser; }
inline void CMassFileLoader::SetFileLoadedCallback(std::function<FileLoadedCallbackSignature> Function) { m_fnFileLoadedCallback = std::move(Function); }
inline void CMassFileLoader::SetLoadFailedCallback(std::function<LoadFailedCallbackSignature> Function) { m_fnLoadFailedCallback = std::move(Function); }
inline void CMassFileLoader::SetLoadFinishedCallback(std::function<LoadFinishedCallbackSignature> Function) { m_fnLoadFinishedCallback = std::move(Function); }

#endif // ENGINE_SHARED_FILE_LOADER_H
