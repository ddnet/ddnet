#include "file_loader.h"
#include "base/system.h"

CMassFileLoader::CMassFileLoader(IStorage *pStorage, uint8_t Flags) :
	IMassFileLoader(Flags)
{
	m_pStorage = pStorage;
}

CMassFileLoader::~CMassFileLoader()
{
	for(auto it : m_PathCollection)
	{
		delete it.second;
	}
}

void CMassFileLoader::SetMatchExpression(const std::string& Match)
{
	m_Regex.emplace(std::regex(Match));
	// With exceptions disabled, we can't make use of regex_error so we can't fire LOAD_ERROR_INVALID_REGEX in this implementation. Good luck
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

		if(!fs_is_readable(AbsolutePath.c_str()))
		{
			*UserData->m_pContinue = TryCallback<bool>(UserData->m_pThis->m_fnLoadFailedCallback, LOAD_ERROR_DIRECTORY_UNREADABLE, AbsolutePath.c_str());
			return 0;
		}

		if(!(UserData->m_pThis->m_Flags & LOAD_FLAGS_FOLLOW_SYMBOLIC_LINKS) && fs_is_symlink(AbsolutePath.c_str()))
		{
			*UserData->m_pContinue = TryCallback<bool>(UserData->m_pThis->m_fnLoadFailedCallback, LOAD_ERROR_UNWANTED_SYMLINK, AbsolutePath.c_str());
			return 0;
		}

		if(!IsDir)
		{
			if(!UserData->m_pThis->m_Regex.has_value() || std::regex_search(RelevantFilename, UserData->m_pThis->m_Regex.value()))
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
	if(m_PathCollection.empty() /* No valid paths added */
		|| !m_pStorage /* Invalid storage */
		|| !m_fnFileLoadedCallback /* File loaded callback unimplemented */
		|| m_Flags & ~LOAD_FLAGS_MASK /* Out of bounds flag(s) */)
	{
		m_Continue = TryCallback<bool>(m_fnLoadFailedCallback, LOAD_ERROR_NOT_INIT, nullptr);
		return 0;
	}

	for(auto it : m_PathCollection)
	{
		if(m_Continue)
		{
			std::string Key = it.first;
			SListDirectoryCallbackUserInfo Data{&Key, this, &m_Continue};
			m_pStorage->ListDirectory(IStorage::TYPE_ALL, Key.c_str(), ListDirectoryCallback, &Data);
		}
	}

	// Index is now populated, load the files
	unsigned char *pData = nullptr;
	unsigned int Size, Count = 0;
	IOHANDLE Handle;
	for(auto directory : m_PathCollection)
	{
		for(auto file : *directory.second)
		{
			if(m_Continue)
			{
				std::string FilePath = directory.first + "/" + file;
				if(!(m_Flags & LOAD_FLAGS_FOLLOW_SYMBOLIC_LINKS) && fs_is_symlink(FilePath.c_str()))
				{
					m_Continue = TryCallback<bool>(m_fnLoadFailedCallback, LOAD_ERROR_UNWANTED_SYMLINK, FilePath.c_str());
					continue;
				}

				if(m_Flags & LOAD_FLAGS_DONT_READ_FILE)
				{
					m_fnFileLoadedCallback(m_Flags & LOAD_FLAGS_ABSOLUTE_PATH ? FilePath : file, nullptr, 0);
					Count++;
					continue;
				}

				if(!fs_is_readable(FilePath.c_str()))
				{
					m_Continue = TryCallback<bool>(m_fnLoadFailedCallback, LOAD_ERROR_FILE_UNREADABLE, FilePath.c_str());
					continue;
				}

				Handle = io_open(FilePath.c_str(), IOFLAG_READ | IOFLAG_SKIP_BOM);
				if(!Handle)
				{
					dbg_msg("Mass file loader", "File open failed for unknown reason: '%s'", FilePath.c_str());
					m_Continue = TryCallback<bool>(m_fnLoadFailedCallback, LOAD_ERROR_UNKNOWN, nullptr);
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

				m_fnFileLoadedCallback(m_Flags & LOAD_FLAGS_ABSOLUTE_PATH ? FilePath : file, pData, Size);
				free(pData);
				Count++;
				io_close(Handle);
			}
		}
	}

	return Count;
}

/* TODO:
 * [ ] test error callback return value, make sure if false is returned the callback is never called again
 * [ ] test every combination of flags
 * [+, ] test symlink and readable detection on windows and unix
 * ^ (waiting on working readable/symlink detection)
 * [x] see if you can error check regex

 * [+] test multiple directories
 * [ ] async implementation
 */
