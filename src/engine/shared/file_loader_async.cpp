// #include "engine/shared/uuid_manager.h"
// #include "file_loader.h"
// #include <mutex>
// #include <thread>

// CMassFileLoaderAsync::CMassFileLoaderAsync(IStorage *pStorage, uint8_t Flags) :
//	CMassFileLoader(Flags)
//{
//	m_pStorage = pStorage;
// }

// CMassFileLoaderAsync::~CMassFileLoaderAsync()
//{
//	for(const auto &it : m_PathCollection)
//	{
//		delete it.second;
//	}
// }

// void CMassFileLoaderAsync::SetFileExtension(const std::string &Extension)
//{
//	m_Extension = Extension;
// }

// inline bool CompareExtension(const std::filesystem::path &Filename, const std::string &Extension)
//{
//	std::string FileExtension = Filename.extension().string();
//	std::transform(FileExtension.begin(), FileExtension.end(), FileExtension.begin(),
//		[](unsigned char c) { return std::tolower(c); });

//	return FileExtension == Extension; // Extension is already lowered
//}

//[[maybe_unused]] int CMassFileLoaderAsync::ListDirectoryCallback(const char *Name, int IsDir, int, void *User)
//{
//	auto *UserData = reinterpret_cast<SListDirectoryCallbackUserInfo *>(User);
//	if(*UserData->m_pContinue)
//	{
//		auto *pFileList = UserData->m_pThis->m_PathCollection.find(*UserData->m_pCurrentDirectory)->second;
//		std::string AbsolutePath = *UserData->m_pCurrentDirectory + "/" + Name;
//		std::string RelevantFilename = UserData->m_pThis->m_Flags & LOAD_FLAGS_ABSOLUTE_PATH ? AbsolutePath : Name;

//		if(!str_comp(Name, ".") || !str_comp(Name, ".."))
//			return 0;

//		if(!(UserData->m_pThis->m_Flags & LOAD_FLAGS_FOLLOW_SYMBOLIC_LINKS) && fs_is_symlink(AbsolutePath.c_str()))
//		{
//			*UserData->m_pContinue = TryCallback(UserData->m_pThis->m_fnLoadFailedCallback, LOAD_ERROR_UNWANTED_SYMLINK, AbsolutePath.c_str(), UserData->m_pThis->m_pUser);
//			return 0;
//		}
//		if(!IsDir)
//		{
//			if(UserData->m_pThis->m_Extension.empty() || CompareExtension(Name, UserData->m_pThis->m_Extension))
//				pFileList->push_back(Name);
//		}
//		else if(UserData->m_pThis->m_Flags & LOAD_FLAGS_RECURSE_SUBDIRECTORIES)
//		{
//			UserData->m_pThis->m_PathCollection.insert({AbsolutePath, new std::vector<std::string>});
//			// Note that adding data to a SORTED container that is currently being iterated on higher in scope would invalidate the iterator. This is not sorted
//			SListDirectoryCallbackUserInfo Data{&AbsolutePath, UserData->m_pThis, UserData->m_pContinue};
//			UserData->m_pThis->m_pStorage->ListDirectory(IStorage::TYPE_ALL, AbsolutePath.c_str(), ListDirectoryCallback, &Data); // Directory item is a directory, must be recursed
//		}
//	}

//	return 0;
//}

// void CMassFileLoaderAsync::Load(void *pUser)
//{
//	auto *pUserData = reinterpret_cast<CMassFileLoaderAsync *>(pUser);

//	dbg_assert(bool(pUserData), "Async mass file loader instance deleted before use.");

// #define MASS_FILE_LOADER_ERROR_PREFIX "Mass file loader used "
//	dbg_assert(!pUserData->m_RequestedPaths.empty(), MASS_FILE_LOADER_ERROR_PREFIX "without adding paths."); // Ensure paths have been added
//	dbg_assert(bool(pUserData->m_pStorage), MASS_FILE_LOADER_ERROR_PREFIX "without passing a valid IStorage instance."); // Ensure storage is valid
//	dbg_assert(bool(pUserData->m_fnFileLoadedCallback), MASS_FILE_LOADER_ERROR_PREFIX "without implementing file loaded callback."); // Ensure file loaded callback is implemented
//	dbg_assert(pUserData->m_Flags ^ LOAD_FLAGS_MASK, MASS_FILE_LOADER_ERROR_PREFIX "with invalid flags."); // Ensure flags are in bounds
//	dbg_assert(!pUserData->m_Finished, MASS_FILE_LOADER_ERROR_PREFIX "after having already been used."); // Ensure is not reused
// #undef MASS_FILE_LOADER_ERROR_PREFIX

//	char aPathBuffer[IO_MAX_PATH_LENGTH];
//	for(auto &It : pUserData->m_RequestedPaths)
//	{
//		if(pUserData->m_Continue)
//		{
//			int StorageType = It.find(':') == 0 ? IStorage::STORAGETYPE_BASIC : IStorage::STORAGETYPE_CLIENT;
//			if(StorageType == IStorage::STORAGETYPE_BASIC)
//				It.erase(0, 1);
//			pUserData->m_pStorage->GetCompletePath(StorageType, It.c_str(), aPathBuffer, sizeof(aPathBuffer));
//			if(fs_is_dir(aPathBuffer)) // Exists and is a directory
//				pUserData->m_PathCollection.insert({std::string(aPathBuffer), new std::vector<std::string>});
//			else
//				pUserData->m_Continue = TryCallback(pUserData->m_fnLoadFailedCallback, LOAD_ERROR_INVALID_SEARCH_PATH, It.c_str(), pUserData->m_pUser);
//		}
//	}

//	if(!pUserData->m_Extension.empty())
//	{
//		// must be .x at the shortest
//		if(pUserData->m_Extension.size() == 1 || pUserData->m_Extension.at(0) != '.')
//			pUserData->m_Continue = TryCallback(pUserData->m_fnLoadFailedCallback, LOAD_ERROR_INVALID_EXTENSION, pUserData->m_Extension.c_str(), pUserData->m_pUser);
//		std::transform(pUserData->m_Extension.begin(), pUserData->m_Extension.end(), pUserData->m_Extension.begin(),
//			[](unsigned char c) { return std::tolower(c); });
//	}

//	for(const auto &It : pUserData->m_PathCollection)
//	{
//		if(pUserData->m_Continue)
//		{
//			std::string Key = It.first;
//			SListDirectoryCallbackUserInfo Data{&Key, pUserData, &pUserData->m_Continue};
//			pUserData->m_pStorage->ListDirectory(IStorage::TYPE_ALL, Key.c_str(), ListDirectoryCallback, &Data);
//		}
//	}

//	// Index is now populated, load the files
//	unsigned char *pData = nullptr;
//	unsigned int Size, Count = 0;
//	IOHANDLE Handle;
//	for(const auto &Directory : pUserData->m_PathCollection)
//	{
//		for(const auto &File : *Directory.second)
//		{
//			if(pUserData->m_Continue)
//			{
//				std::string FilePath = Directory.first + "/" + File;
//				if(!(pUserData->m_Flags & LOAD_FLAGS_FOLLOW_SYMBOLIC_LINKS) && fs_is_symlink(FilePath.c_str()))
//				{
//					pUserData->m_Continue = TryCallback(pUserData->m_fnLoadFailedCallback, LOAD_ERROR_UNWANTED_SYMLINK, FilePath.c_str(), pUserData->m_pUser);
//					continue;
//				}

//				if(pUserData->m_Flags & LOAD_FLAGS_DONT_READ_FILE)
//				{
//					pUserData->m_fnFileLoadedCallback(pUserData->m_Flags & LOAD_FLAGS_ABSOLUTE_PATH ? FilePath : File, nullptr, 0, pUserData->m_pUser);
//					Count++;
//					continue;
//				}

//				Handle = io_open(FilePath.c_str(), LOAD_FLAGS_SKIP_BOM ? IOFLAG_READ | IOFLAG_SKIP_BOM : IOFLAG_READ);
//				if(!Handle)
//				{
//					pUserData->m_Continue = TryCallback(pUserData->m_fnLoadFailedCallback, LOAD_ERROR_FILE_UNREADABLE, FilePath.c_str(), pUserData->m_pUser);
//					continue;
//				}

//				// system.cpp APIs are only good up to 2 GiB at the moment (signed long cannot describe a size any larger)
//				io_seek(Handle, 0, IOSEEK_END);
//				long ExpectedSize = io_tell(Handle);
//				io_seek(Handle, 0, IOSEEK_START);

//				if(ExpectedSize <= 0) // File is either too large for io_tell/ftell to say or it's actually empty (MinGW returns 0; MSVC returns -1)
//				{
//					size_t RealSize = std::filesystem::file_size(FilePath);
//					if(static_cast<size_t>(ExpectedSize) != RealSize)
//					{
//						pUserData->m_Continue = TryCallback(pUserData->m_fnLoadFailedCallback, LOAD_ERROR_FILE_TOO_LARGE, FilePath.c_str(), pUserData->m_pUser);
//						continue;
//					}
//				}
//				io_read_all(Handle, reinterpret_cast<void **>(&pData), &Size);
//				if(static_cast<unsigned int>(ExpectedSize) != Size) // Possibly redundant, but accounts for memory allocation shortcomings and not just IO
//				{
//					pUserData->m_Continue = TryCallback(pUserData->m_fnLoadFailedCallback, LOAD_ERROR_FILE_TOO_LARGE, FilePath.c_str(), pUserData->m_pUser);
//					continue;
//				}

//				pUserData->m_fnFileLoadedCallback(pUserData->m_Flags & LOAD_FLAGS_ABSOLUTE_PATH ? FilePath : File, pData, Size, pUserData->m_pUser);
//				free(pData);
//				Count++;
//				io_close(Handle);
//				std::this_thread::sleep_for(std::chrono::milliseconds(20)); // i really, really dislike this. i would love to find a way to access the skin textures from the main thread in a lockfree way, so we do not have to wait and give the main thread a chance to access the lock.
//			}
//		}
//	}
//	if(pUserData->m_fnLoadFinishedCallback)
//		pUserData->m_fnLoadFinishedCallback(Count, pUserData->m_pUser);
//	pUserData->m_Finished = true;
//}
