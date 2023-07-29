#include "file_loader_async.h"
#include "engine/shared/uuid_manager.h"
#include <mutex>
#include <thread>

CMassFileLoaderAsync::CMassFileLoaderAsync(IStorage *pStorage, uint8_t Flags) :
	IMassFileLoader(Flags)
{
	m_pStorage = pStorage;
}

CMassFileLoaderAsync::~CMassFileLoaderAsync()
{
	for(const auto &it : m_PathCollection)
	{
		delete it.second;
	}
}

void CMassFileLoaderAsync::SetFileExtension(const std::string &Extension)
{
	m_Extension = Extension;
}

inline bool CompareExtension(const std::filesystem::path &Filename, const std::string &Extension)
{
	std::string FileExtension = Filename.extension().string();
	std::transform(FileExtension.begin(), FileExtension.end(), FileExtension.begin(),
		[](unsigned char c) { return std::tolower(c); });

	return FileExtension == Extension; // Extension is already lowered
}

[[maybe_unused]] int CMassFileLoaderAsync::ListDirectoryCallback(const char *Name, int IsDir, int, void *User)
{
	auto *UserData = reinterpret_cast<SListDirectoryCallbackUserInfo *>(User);
	if(*UserData->m_pContinue)
	{
		auto *pFileList = UserData->m_pThis->m_PathCollection.find(*UserData->m_pCurrentDirectory)->second;
		std::string AbsolutePath = *UserData->m_pCurrentDirectory + "/" + Name;
		std::string RelevantFilename = UserData->m_pThis->m_Flags & LOAD_FLAGS_ABSOLUTE_PATH ? AbsolutePath : Name;

		if(!str_comp(Name, ".") || !str_comp(Name, ".."))
			return 0;

		if(!(UserData->m_pThis->m_Flags & LOAD_FLAGS_FOLLOW_SYMBOLIC_LINKS) && fs_is_symlink(AbsolutePath.c_str()))
		{
			*UserData->m_pContinue = TryCallback(UserData->m_pThis->m_fnLoadFailedCallback, LOAD_ERROR_UNWANTED_SYMLINK, AbsolutePath.c_str(), UserData->m_pThis->m_pUser);
			return 0;
		}
		if(!IsDir)
		{
			if(UserData->m_pThis->m_Extension.empty() || CompareExtension(Name, UserData->m_pThis->m_Extension))
				pFileList->push_back(Name);
		}
		else if(UserData->m_pThis->m_Flags & LOAD_FLAGS_RECURSE_SUBDIRECTORIES)
		{
			UserData->m_pThis->m_PathCollection.insert({AbsolutePath, new std::vector<std::string>});
			// Note that adding data to a SORTED container that is currently being iterated on higher in scope would invalidate the iterator. This is not sorted
			SListDirectoryCallbackUserInfo Data{&AbsolutePath, UserData->m_pThis, UserData->m_pContinue};
			UserData->m_pThis->m_pStorage->ListDirectory(IStorage::TYPE_ALL, AbsolutePath.c_str(), ListDirectoryCallback, &Data); // Directory item is a directory, must be recursed
		}
	}

	return 0;
}

void CMassFileLoaderAsync::Load(void *pUser)
{
	auto *UserData = reinterpret_cast<CMassFileLoaderAsync *>(pUser);

	dbg_assert(bool(UserData), "Async mass file loader instance deleted before use.");

#define MASS_FILE_LOADER_ERROR_PREFIX "Mass file loader used "
	dbg_assert(!UserData->m_RequestedPaths.empty(), MASS_FILE_LOADER_ERROR_PREFIX "without adding paths."); // Ensure paths have been added
	dbg_assert(bool(UserData->m_pStorage), MASS_FILE_LOADER_ERROR_PREFIX "without passing a valid IStorage instance."); // Ensure storage is valid
	dbg_assert(bool(UserData->m_fnFileLoadedCallback), MASS_FILE_LOADER_ERROR_PREFIX "without implementing file loaded callback."); // Ensure file loaded callback is implemented
	dbg_assert(UserData->m_Flags ^ LOAD_FLAGS_MASK, MASS_FILE_LOADER_ERROR_PREFIX "with invalid flags."); // Ensure flags are in bounds
	dbg_assert(!UserData->m_Finished, MASS_FILE_LOADER_ERROR_PREFIX "after having already been used."); // Ensure is not reused
#undef MASS_FILE_LOADER_ERROR_PREFIX

	char aPathBuffer[IO_MAX_PATH_LENGTH];
	for(auto &It : UserData->m_RequestedPaths)
	{
		if(UserData->m_Continue)
		{
			int StorageType = It.find(':') == 0 ? IStorage::STORAGETYPE_BASIC : IStorage::STORAGETYPE_CLIENT;
			if(StorageType == IStorage::STORAGETYPE_BASIC)
				It.erase(0, 1);
			UserData->m_pStorage->GetCompletePath(StorageType, It.c_str(), aPathBuffer, sizeof(aPathBuffer));
			if(fs_is_dir(aPathBuffer)) // Exists and is a directory
				UserData->m_PathCollection.insert({std::string(aPathBuffer), new std::vector<std::string>});
			else
				UserData->m_Continue = TryCallback(UserData->m_fnLoadFailedCallback, LOAD_ERROR_INVALID_SEARCH_PATH, It.c_str(), UserData->m_pUser);
		}
	}

	if(!UserData->m_Extension.empty())
	{
		// must be .x at the shortest
		if(UserData->m_Extension.size() == 1 || UserData->m_Extension.at(0) != '.')
			UserData->m_Continue = TryCallback(UserData->m_fnLoadFailedCallback, LOAD_ERROR_INVALID_EXTENSION, UserData->m_Extension.c_str(), UserData->m_pUser);
		std::transform(UserData->m_Extension.begin(), UserData->m_Extension.end(), UserData->m_Extension.begin(),
			[](unsigned char c) { return std::tolower(c); });
	}

	for(const auto &It : UserData->m_PathCollection)
	{
		if(UserData->m_Continue)
		{
			std::string Key = It.first;
			SListDirectoryCallbackUserInfo Data{&Key, UserData, &UserData->m_Continue};
			UserData->m_pStorage->ListDirectory(IStorage::TYPE_ALL, Key.c_str(), ListDirectoryCallback, &Data);
		}
	}

	// Index is now populated, load the files
	unsigned char *pData = nullptr;
	unsigned int Size, Count = 0;
	IOHANDLE Handle;
	for(const auto &Directory : UserData->m_PathCollection)
	{
		for(const auto &File : *Directory.second)
		{
			if(UserData->m_Continue)
			{
				std::string FilePath = Directory.first + "/" + File;
				if(!(UserData->m_Flags & LOAD_FLAGS_FOLLOW_SYMBOLIC_LINKS) && fs_is_symlink(FilePath.c_str()))
				{
					UserData->m_Continue = TryCallback(UserData->m_fnLoadFailedCallback, LOAD_ERROR_UNWANTED_SYMLINK, FilePath.c_str(), UserData->m_pUser);
					continue;
				}

				if(UserData->m_Flags & LOAD_FLAGS_DONT_READ_FILE)
				{
					UserData->m_fnFileLoadedCallback(UserData->m_Flags & LOAD_FLAGS_ABSOLUTE_PATH ? FilePath : File, nullptr, 0, UserData->m_pUser);
					Count++;
					continue;
				}

				Handle = io_open(FilePath.c_str(), IOFLAG_READ | IOFLAG_SKIP_BOM);
				if(!Handle)
				{
					UserData->m_Continue = TryCallback(UserData->m_fnLoadFailedCallback, LOAD_ERROR_FILE_UNREADABLE, FilePath.c_str(), UserData->m_pUser);
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
						UserData->m_Continue = TryCallback(UserData->m_fnLoadFailedCallback, LOAD_ERROR_FILE_TOO_LARGE, FilePath.c_str(), UserData->m_pUser);
						continue;
					}
				}
				io_read_all(Handle, reinterpret_cast<void **>(&pData), &Size);
				if(static_cast<unsigned int>(ExpectedSize) != Size) // Possibly redundant, but accounts for memory allocation shortcomings and not just IO
				{
					UserData->m_Continue = TryCallback(UserData->m_fnLoadFailedCallback, LOAD_ERROR_FILE_TOO_LARGE, FilePath.c_str(), UserData->m_pUser);
					continue;
				}

				UserData->m_fnFileLoadedCallback(UserData->m_Flags & LOAD_FLAGS_ABSOLUTE_PATH ? FilePath : File, pData, Size, UserData->m_pUser);
				free(pData);
				Count++;
				io_close(Handle);
				std::this_thread::sleep_for(std::chrono::milliseconds(20)); // i really, really dislike this. i would love to find a way to access the skin textures from the main thread in a lockfree way, so we do not have to wait and give the main thread a chance to access the lock.
			}
		}
	}
	if(UserData->m_fnLoadFinishedCallback)
		UserData->m_fnLoadFinishedCallback(Count, UserData->m_pUser);
	UserData->m_Finished = true;
}

void CMassFileLoaderAsync::BeginLoad()
{
	static constexpr const char aThreadIdPrefix[] = "fileloadjob-";
	ThreadId = RandomUuid();

	char aAux[UUID_MAXSTRSIZE];
	FormatUuid(ThreadId, aAux, sizeof(aAux));

	char aId[sizeof(aThreadIdPrefix) + sizeof(aAux)];
	str_format(aId, sizeof(aId), "%s%s", aThreadIdPrefix, aAux);

	char aAuxBuf[512];
	str_format(aAuxBuf, sizeof(aAuxBuf), "Unable to create file loader thread with id \"%s\"", aId);
	dbg_assert(thread_init_and_detach(CMassFileLoaderAsync::Load, this, aId) != 0, aAuxBuf);
}
