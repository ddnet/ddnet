#include "file_loader.h"
#include <base/system.h>

CMassFileLoader::CMassFileLoader(IStorage *pStorage, uint8_t Flags)
{
	m_pStorage = pStorage;
	m_Flags = Flags;
}

CMassFileLoader::~CMassFileLoader()
{
	if(m_pExtension)
		free(m_pExtension);
	for(const auto &it : m_PathCollection)
	{
		delete it.second;
	}
}

void CMassFileLoader::SetFileExtension(const std::string_view Extension)
{
	m_pExtension = static_cast<char *>(malloc((Extension.size() + 1) * sizeof(char)));
	str_copy(m_pExtension, Extension.data(), Extension.size());
}

inline bool CMassFileLoader::CompareExtension(const std::filesystem::path &Filename, const std::string_view Extension)
{
	// std::string is justified here because of std::transform, and because std::filesystem::path::c_str() will return const wchar_t *, but char width is handled automatically when using string
	std::string FileExtension = Filename.extension().string();
	std::transform(FileExtension.begin(), FileExtension.end(), FileExtension.begin(),
		[](unsigned char c) { return std::tolower(c); });
	return FileExtension == Extension; // Extension is already lowered
}

[[maybe_unused]] int CMassFileLoader::ListDirectoryCallback(const char *Name, int IsDir, int, void *User)
{
	auto *pUserData = reinterpret_cast<SListDirectoryCallbackUserInfo *>(User);
	if(*pUserData->m_pContinue)
	{
		auto *pFileList = pUserData->m_pThis->m_PathCollection.find(pUserData->m_pCurrentDirectory)->second;
		char AbsolutePath[IO_MAX_PATH_LENGTH];
		str_format(AbsolutePath, sizeof(AbsolutePath), "%s/%s", pUserData->m_pCurrentDirectory, Name);

		if(!str_comp(Name, ".") || !str_comp(Name, ".."))
			return 0;

		if(!(pUserData->m_pThis->m_Flags & LOAD_FLAGS_FOLLOW_SYMBOLIC_LINKS) && fs_is_symlink(AbsolutePath))
		{
			*pUserData->m_pContinue = TryCallback(pUserData->m_pThis->m_fnLoadFailedCallback, LOAD_ERROR_UNWANTED_SYMLINK, AbsolutePath, pUserData->m_pThis->m_pUser);
			return 0;
		}
		if(!IsDir)
		{
			if(str_comp(pUserData->m_pThis->m_pExtension, "") || CompareExtension(Name, pUserData->m_pThis->m_pExtension))
				pFileList->push_back(Name);
		}
		else if(pUserData->m_pThis->m_Flags & LOAD_FLAGS_RECURSE_SUBDIRECTORIES)
		{
			pUserData->m_pThis->m_PathCollection.insert({AbsolutePath, new std::vector<std::string>});
			// Note that adding data to a SORTED container that is currently being iterated on higher in scope would invalidate the iterator. This is not sorted
			SListDirectoryCallbackUserInfo Data{AbsolutePath, pUserData->m_pThis, pUserData->m_pContinue};
			pUserData->m_pThis->m_pStorage->ListDirectory(IStorage::TYPE_ALL, AbsolutePath, ListDirectoryCallback, &Data); // Directory item is a directory, must be recursed
		}
	}

	return 0;
}

unsigned int CMassFileLoader::Begin(CMassFileLoader *pUserData)
{
	char aPathBuffer[IO_MAX_PATH_LENGTH];
	for(auto &It : pUserData->m_RequestedPaths)
	{
		if(pUserData->m_Continue)
		{
			int StorageType = It.find(':') == 0 ? IStorage::STORAGETYPE_BASIC : IStorage::STORAGETYPE_CLIENT;
			if(StorageType == IStorage::STORAGETYPE_BASIC)
				It.erase(0, 1);
			pUserData->m_pStorage->GetCompletePath(StorageType, It.c_str(), aPathBuffer, sizeof(aPathBuffer));
			if(fs_is_dir(aPathBuffer)) // Exists and is a directory
				pUserData->m_PathCollection.insert({std::string(aPathBuffer), new std::vector<std::string>});
			else
				pUserData->m_Continue = TryCallback(pUserData->m_fnLoadFailedCallback, LOAD_ERROR_INVALID_SEARCH_PATH, It.c_str(), pUserData->m_pUser);
		}
	}

	if(!str_comp(pUserData->m_pExtension, ""))
	{
		// must be .x at the shortest
		if(str_length(pUserData->m_pExtension) == 1 || pUserData->m_pExtension[0] != '.')
			pUserData->m_Continue = TryCallback(pUserData->m_fnLoadFailedCallback, LOAD_ERROR_INVALID_EXTENSION, pUserData->m_pExtension, pUserData->m_pUser);
		for(int i = 0; i < str_length(pUserData->m_pExtension); i++)
			pUserData->m_pExtension[i] = std::tolower(pUserData->m_pExtension[i]);
	}

	for(const auto &It : pUserData->m_PathCollection)
	{
		if(pUserData->m_Continue)
		{
			const char *Key = It.first.c_str();
			SListDirectoryCallbackUserInfo Data{Key, pUserData, &pUserData->m_Continue};
			pUserData->m_pStorage->ListDirectory(IStorage::TYPE_ALL, Key, ListDirectoryCallback, &Data);
		}
	}

	// Index is now populated, load the files
	unsigned char *pData = nullptr;
	unsigned int Size, Count = 0;
	IOHANDLE Handle;
	for(const auto &Directory : pUserData->m_PathCollection)
	{
		for(const auto &File : *Directory.second)
		{
			if(pUserData->m_Continue)
			{
				char FilePath[IO_MAX_PATH_LENGTH];
				str_format(FilePath, sizeof(FilePath), "%s/%s", Directory.first.c_str(), File.c_str());
				if(!(pUserData->m_Flags & LOAD_FLAGS_FOLLOW_SYMBOLIC_LINKS) && fs_is_symlink(FilePath))
				{
					pUserData->m_Continue = TryCallback(pUserData->m_fnLoadFailedCallback, LOAD_ERROR_UNWANTED_SYMLINK, FilePath, pUserData->m_pUser);
					continue;
				}

				if(pUserData->m_Flags & LOAD_FLAGS_DONT_READ_FILE)
				{
					pUserData->m_fnFileLoadedCallback(pUserData->m_Flags & LOAD_FLAGS_ABSOLUTE_PATH ? FilePath : File, nullptr, 0, pUserData->m_pUser);
					Count++;
					continue;
				}

				Handle = io_open(FilePath, pUserData->m_Flags & IOFLAG_READ | (pUserData->m_Flags & LOAD_FLAGS_SKIP_BOM ? IOFLAG_SKIP_BOM : 0));
				if(!Handle)
				{
					pUserData->m_Continue = TryCallback(pUserData->m_fnLoadFailedCallback, LOAD_ERROR_FILE_UNREADABLE, FilePath, pUserData->m_pUser);
					continue;
				}

				// system.cpp APIs are only good up to 2 GiB at the moment (signed long cannot describe a size any larger)
				io_seek(Handle, 0, IOSEEK_END);
				long ExpectedSize = io_tell(Handle);
				io_seek(Handle, 0, IOSEEK_START);

				if(ExpectedSize <= 0) // File is either too large for io_tell/ftell to say or it's actually empty (MinGW returns 0; MSVC returns -1)
				{
					size_t RealSize = std::filesystem::file_size(FilePath);
					if(static_cast<size_t>(ExpectedSize) != RealSize)
					{
						pUserData->m_Continue = TryCallback(pUserData->m_fnLoadFailedCallback, LOAD_ERROR_FILE_TOO_LARGE, FilePath, pUserData->m_pUser);
						continue;
					}
				}
				io_read_all(Handle, reinterpret_cast<void **>(&pData), &Size);
				if(static_cast<unsigned int>(ExpectedSize) != Size) // Possibly redundant, but accounts for memory allocation shortcomings and not just IO
				{
					pUserData->m_Continue = TryCallback(pUserData->m_fnLoadFailedCallback, LOAD_ERROR_FILE_TOO_LARGE, FilePath, pUserData->m_pUser);
					continue;
				}

				pUserData->m_fnFileLoadedCallback(pUserData->m_Flags & LOAD_FLAGS_ABSOLUTE_PATH ? FilePath : File, pData, Size, pUserData->m_pUser);
				free(pData);
				Count++;
				io_close(Handle);
				if(pUserData->m_Flags & LOAD_FLAGS_ASYNC)
					std::this_thread::sleep_for(std::chrono::milliseconds(20)); // i really, really dislike this. i would love to find a way to access the skin textures from the main thread in a lockfree way, so we do not have to wait and give the main thread a chance to access the lock.
			}
		}
	}

	if(pUserData->m_Flags & LOAD_FLAGS_ASYNC)
	{
		if(pUserData->m_fnLoadFinishedCallback)
			pUserData->m_fnLoadFinishedCallback(Count, pUserData->m_pUser);
		pUserData->m_Finished = true;
		return 0;
	}
	else
		return Count;
}

std::optional<unsigned int> CMassFileLoader::Load()
{
#define MASS_FILE_LOADER_ERROR_PREFIX "Mass file loader used "
	dbg_assert(!m_RequestedPaths.empty(), MASS_FILE_LOADER_ERROR_PREFIX "without adding paths."); // Ensure paths have been added
	dbg_assert(bool(m_pStorage), MASS_FILE_LOADER_ERROR_PREFIX "without passing a valid IStorage instance."); // Ensure storage is valid
	dbg_assert(bool(m_fnFileLoadedCallback), MASS_FILE_LOADER_ERROR_PREFIX "without implementing file loaded callback."); // Ensure file loaded callback is implemented
	dbg_assert(m_Flags ^ LOAD_FLAGS_MASK, MASS_FILE_LOADER_ERROR_PREFIX "with invalid flags."); // Ensure flags are in bounds
	dbg_assert(!m_Finished, MASS_FILE_LOADER_ERROR_PREFIX "after having already been used."); // Ensure is not reused
#undef MASS_FILE_LOADER_ERROR_PREFIX
	if(m_Flags & LOAD_FLAGS_ASYNC)
	{
		static constexpr const char aThreadIdPrefix[] = "fileloadjob-";
		ThreadId = RandomUuid();

		char aAux[UUID_MAXSTRSIZE];
		FormatUuid(ThreadId, aAux, sizeof(aAux));

		char aId[sizeof(aThreadIdPrefix) + sizeof(aAux)];
		str_format(aId, sizeof(aId), "%s%s", aThreadIdPrefix, aAux);

		char aAuxBuf[512];
		str_format(aAuxBuf, sizeof(aAuxBuf), "Unable to create file loader thread with id \"%s\"", aId);
		auto f0 = reinterpret_cast<void (*)(void *)>(&CMassFileLoader::Begin);
		dbg_assert(thread_init_and_detach(f0, this, aId) != 0, aAuxBuf);
		return std::nullopt;
	}
	else
	{
		return Begin(this);
	}
}

/* TODO:
 * [+] test error callback return value, make sure if false is returned the callback is never called again
 * [ ] test every combination of flags
 * [+,+] test symlink and readable detection on windows and unix
 * ^ (waiting on working readable/symlink detection)
 * [x] see if you can error check regex
 *
 * [ ] fix unreadable
 *
 * [+] test multiple directories
 * [ ] async implementation
 */
