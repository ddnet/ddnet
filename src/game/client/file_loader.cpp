#include "file_loader.h"
#include <base/system.h>

CMassFileLoader::CMassFileLoader(IStorage *pStorage, uint8_t Flags) :
	IMassFileLoader(Flags)
{
	m_pStorage = pStorage;
}

CMassFileLoader::~CMassFileLoader()
{
	for(const auto &it : m_PathCollection)
	{
		delete it.second;
	}
}

void CMassFileLoader::SetFileExtension(const std::string &Extension)
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

[[maybe_unused]] int CMassFileLoader::ListDirectoryCallback(const char *Name, int IsDir, int, void *User)
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
			*UserData->m_pContinue = TryCallback<bool>(UserData->m_pThis->m_fnLoadFailedCallback, LOAD_ERROR_UNWANTED_SYMLINK, AbsolutePath.c_str());
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

unsigned int CMassFileLoader::Load()
{
	m_Continue = true;
	if(m_RequestedPaths.empty() /* No valid paths added */
		|| !m_pStorage /* Invalid storage */
		|| !m_fnFileLoadedCallback /* File loaded callback unimplemented */
		|| m_Flags & ~LOAD_FLAGS_MASK /* Out of bounds flag(s) */)
	{
		TryCallback<bool>(m_fnLoadFailedCallback, LOAD_ERROR_NOT_INIT, nullptr);
		return 0;
	}

	char aPathBuffer[IO_MAX_PATH_LENGTH];
	for(auto &It : m_RequestedPaths)
	{
		if(m_Continue)
		{
			int StorageType = It.find(':') == 0 ? IStorage::STORAGETYPE_BASIC : IStorage::STORAGETYPE_CLIENT;
			if(StorageType == IStorage::STORAGETYPE_BASIC)
				It.erase(0, 1);
			m_pStorage->GetCompletePath(StorageType, It.c_str(), aPathBuffer, sizeof(aPathBuffer));
			if(fs_is_dir(aPathBuffer)) // Exists and is a directory
			{
				// if(fs_is_readable(aPathBuffer))
				m_PathCollection.insert({std::string(aPathBuffer), new std::vector<std::string>});
				// else
				//	m_Continue = TryCallback<bool>(m_fnLoadFailedCallback, LOAD_ERROR_DIRECTORY_UNREADABLE, it.c_str());
			}
			else
				m_Continue = TryCallback<bool>(m_fnLoadFailedCallback, LOAD_ERROR_INVALID_SEARCH_PATH, It.c_str());
		}
	}

	if(!m_Extension.empty())
	{
		// must be .x at the shortest
		if(m_Extension.size() == 1 || m_Extension.at(0) != '.')
			m_Continue = TryCallback<bool>(m_fnLoadFailedCallback, LOAD_ERROR_INVALID_EXTENSION, m_Extension.c_str());
		std::transform(m_Extension.begin(), m_Extension.end(), m_Extension.begin(),
			[](unsigned char c) { return std::tolower(c); });
	}

	for(const auto &It : m_PathCollection)
	{
		if(m_Continue)
		{
			std::string Key = It.first;
			SListDirectoryCallbackUserInfo Data{&Key, this, &m_Continue};
			m_pStorage->ListDirectory(IStorage::TYPE_ALL, Key.c_str(), ListDirectoryCallback, &Data);
		}
	}

	// Index is now populated, load the files
	unsigned char *pData = nullptr;
	unsigned int Size, Count = 0;
	IOHANDLE Handle;
	for(const auto &Directory : m_PathCollection)
	{
		for(const auto &File : *Directory.second)
		{
			if(m_Continue)
			{
				std::string FilePath = Directory.first + "/" + File;
				if(!(m_Flags & LOAD_FLAGS_FOLLOW_SYMBOLIC_LINKS) && fs_is_symlink(FilePath.c_str()))
				{
					m_Continue = TryCallback<bool>(m_fnLoadFailedCallback, LOAD_ERROR_UNWANTED_SYMLINK, FilePath.c_str());
					continue;
				}

				if(m_Flags & LOAD_FLAGS_DONT_READ_FILE)
				{
					m_fnFileLoadedCallback(m_Flags & LOAD_FLAGS_ABSOLUTE_PATH ? FilePath : File, nullptr, 0);
					Count++;
					continue;
				}

				// if(!fs_is_readable(FilePath.c_str()))
				// {
				// 	m_Continue = TryCallback<bool>(m_fnLoadFailedCallback, LOAD_ERROR_FILE_UNREADABLE, FilePath.c_str());
				// 	continue;
				// }

				Handle = io_open(FilePath.c_str(), IOFLAG_READ | IOFLAG_SKIP_BOM);
				if(!Handle)
				{
					// There could be other issues than this, but I have no way to distinguish now that fs_is_readable is gone.
					m_Continue = TryCallback<bool>(m_fnLoadFailedCallback, LOAD_ERROR_FILE_UNREADABLE, FilePath.c_str());
					continue;
				}

				// system.cpp APIs are only good up to 2 GiB at the moment (signed long cannot describe a size any larger)
				io_seek(Handle, 0, IOSEEK_END);
				long ExpectedSize = io_tell(Handle);
				io_seek(Handle, 0, IOSEEK_START);

				if(ExpectedSize == 0) // File is either too large for io_tell/ftell to say or it's actually empty
				{
					size_t RealSize = std::filesystem::file_size(FilePath);
					if(static_cast<size_t>(ExpectedSize) != RealSize)
					{
						m_Continue = TryCallback<bool>(m_fnLoadFailedCallback, LOAD_ERROR_FILE_TOO_LARGE, FilePath.c_str());
						continue;
					}
				}

				io_read_all(Handle, reinterpret_cast<void **>(&pData), &Size);
				if(static_cast<unsigned int>(ExpectedSize) != Size) // Possibly redundant, but accounts for memory allocation shortcomings and not just IO
				{
					m_Continue = TryCallback<bool>(m_fnLoadFailedCallback, LOAD_ERROR_FILE_TOO_LARGE, FilePath.c_str());
					continue;
				}

				m_fnFileLoadedCallback(m_Flags & LOAD_FLAGS_ABSOLUTE_PATH ? FilePath : File, pData, Size);
				free(pData);
				Count++;
				io_close(Handle);
			}
		}
	}

	return Count;
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
