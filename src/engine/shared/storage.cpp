/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "linereader.h"
#include <base/hash_ctxt.h>
#include <base/math.h>
#include <base/system.h>
#include <engine/client/updater.h>
#include <engine/engine.h>
#include <engine/storage.h>
#include <game/generated/data_hash.h>

#include <bitset>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

class CCheckJob : public IJob
{
public:
	struct SResult
	{
		std::vector<std::string> m_ExtraFiles;
		std::bitset<DATA_FILE_COUNT> m_DataFilePure;
		std::bitset<DATA_FILE_COUNT> m_DataFilePresent;
	};

private:
	char m_aDataDir[MAX_PATH_LENGTH];
	SResult *m_pResult;

	struct SCheckFileContext
	{
		char m_aSearchRoot[MAX_PATH_LENGTH];
		char m_aCurrentDir[MAX_PATH_LENGTH];
		CCheckJob *m_pSelf;
	};

	static int CheckFileCallback(const char *pName, int IsDir, int DirType, void *pUser)
	{
		SCheckFileContext *pContext = static_cast<SCheckFileContext *>(pUser);
		if(IsDir && (!str_comp(pName, ".") || !str_comp(pName, "..")))
			return 0;

		char aFullPath[MAX_PATH_LENGTH];
		str_format(aFullPath, sizeof(aFullPath), "%s/%s/%s", pContext->m_aSearchRoot, pContext->m_aCurrentDir, pName);

		char aRelPath[MAX_PATH_LENGTH];
		str_format(aRelPath, sizeof(aRelPath), "%s%s%s", pContext->m_aCurrentDir, pContext->m_aCurrentDir[0] ? "/" : "", pName);

		if(IsDir)
		{
			SCheckFileContext Context;
			str_copy(Context.m_aSearchRoot, pContext->m_aSearchRoot, sizeof(Context.m_aSearchRoot));
			str_copy(Context.m_aCurrentDir, aRelPath, sizeof(Context.m_aCurrentDir));
			Context.m_pSelf = pContext->m_pSelf;

			fs_listdir(aFullPath, CheckFileCallback, DirType, &Context);
			return 0;
		}

		int Index = GetCheckIndex(aRelPath);
		if(Index < 0)
		{
			pContext->m_pSelf->m_pResult->m_ExtraFiles.emplace_back(aFullPath);
			return 0;
		}

		IOHANDLE f = io_open(aFullPath, IOFLAG_READ);
		if(!f)
			return 0;

		pContext->m_pSelf->m_pResult->m_DataFilePresent[Index] = true;

		SHA256_CTX ctx;
		sha256_init(&ctx);

		char aBuf[4096];
		unsigned bytes = 0;
		while((bytes = io_read(f, aBuf, sizeof(aBuf))) > 0)
			sha256_update(&ctx, aBuf, bytes);

		io_close(f);
		SHA256_DIGEST Digest = sha256_finish(&ctx);
		if(g_aDataHashes[Index].m_Hash != Digest)
			return 0;

		pContext->m_pSelf->m_pResult->m_DataFilePure[Index] = true;
		return 0;
	}

	void Run()
	{
		SCheckFileContext Context;
		str_copy(Context.m_aSearchRoot, m_aDataDir, sizeof(Context.m_aSearchRoot));
		Context.m_aCurrentDir[0] = '\0';
		Context.m_pSelf = this;

		fs_listdir(m_aDataDir, CheckFileCallback, -1, &Context);
	}

public:
	CCheckJob(const char *pDataDir, SResult *pResult) :
		m_pResult(pResult)
	{
		str_copy(m_aDataDir, pDataDir, sizeof(m_aDataDir));
	}
};

class CStorage : public IStorage
{
public:
	char m_aaStoragePaths[MAX_PATHS][MAX_PATH_LENGTH];
	int m_NumPaths;
	char m_aDatadir[MAX_PATH_LENGTH];
	char m_aUserdir[MAX_PATH_LENGTH];
	char m_aCurrentdir[MAX_PATH_LENGTH];
	char m_aBinarydir[MAX_PATH_LENGTH];

	std::shared_ptr<CCheckJob> m_pCheckJob;
	CCheckJob::SResult m_CheckResult;

	CStorage()
	{
		mem_zero(m_aaStoragePaths, sizeof(m_aaStoragePaths));
		m_NumPaths = 0;
		m_aDatadir[0] = 0;
		m_aUserdir[0] = 0;
		m_pCheckJob = nullptr;
	}

	int Init(const char *pApplicationName, int StorageType, int NumArgs, const char **ppArguments)
	{
		// get userdir
		fs_storage_path(pApplicationName, m_aUserdir, sizeof(m_aUserdir));

		// get datadir
		FindDatadir(ppArguments[0]);

		// get binarydir
		FindBinarydir(ppArguments[0]);

		// get currentdir
		if(!fs_getcwd(m_aCurrentdir, sizeof(m_aCurrentdir)))
			m_aCurrentdir[0] = 0;

		// load paths from storage.cfg
		LoadPaths(ppArguments[0]);

		if(!m_NumPaths)
		{
			dbg_msg("storage", "using standard paths");
			AddDefaultPaths();
		}

		// add save directories
		if(StorageType != STORAGETYPE_BASIC && m_NumPaths && (!m_aaStoragePaths[TYPE_SAVE][0] || !fs_makedir(m_aaStoragePaths[TYPE_SAVE])))
		{
			char aPath[MAX_PATH_LENGTH];
			if(StorageType == STORAGETYPE_CLIENT)
			{
				fs_makedir(GetPath(TYPE_SAVE, "screenshots", aPath, sizeof(aPath)));
				fs_makedir(GetPath(TYPE_SAVE, "screenshots/auto", aPath, sizeof(aPath)));
				fs_makedir(GetPath(TYPE_SAVE, "screenshots/auto/stats", aPath, sizeof(aPath)));
				fs_makedir(GetPath(TYPE_SAVE, "maps", aPath, sizeof(aPath)));
				fs_makedir(GetPath(TYPE_SAVE, "mapres", aPath, sizeof(aPath)));
				fs_makedir(GetPath(TYPE_SAVE, "downloadedmaps", aPath, sizeof(aPath)));
				fs_makedir(GetPath(TYPE_SAVE, "skins", aPath, sizeof(aPath)));
				fs_makedir(GetPath(TYPE_SAVE, "downloadedskins", aPath, sizeof(aPath)));
				fs_makedir(GetPath(TYPE_SAVE, "themes", aPath, sizeof(aPath)));
				fs_makedir(GetPath(TYPE_SAVE, "assets", aPath, sizeof(aPath)));
				fs_makedir(GetPath(TYPE_SAVE, "assets/emoticons", aPath, sizeof(aPath)));
				fs_makedir(GetPath(TYPE_SAVE, "assets/entities", aPath, sizeof(aPath)));
				fs_makedir(GetPath(TYPE_SAVE, "assets/game", aPath, sizeof(aPath)));
				fs_makedir(GetPath(TYPE_SAVE, "assets/particles", aPath, sizeof(aPath)));
#if defined(CONF_VIDEORECORDER)
				fs_makedir(GetPath(TYPE_SAVE, "videos", aPath, sizeof(aPath)));
#endif
			}
			fs_makedir(GetPath(TYPE_SAVE, "dumps", aPath, sizeof(aPath)));
			fs_makedir(GetPath(TYPE_SAVE, "demos", aPath, sizeof(aPath)));
			fs_makedir(GetPath(TYPE_SAVE, "demos/auto", aPath, sizeof(aPath)));
			fs_makedir(GetPath(TYPE_SAVE, "demos/auto/race", aPath, sizeof(aPath)));
			fs_makedir(GetPath(TYPE_SAVE, "demos/replays", aPath, sizeof(aPath)));
			fs_makedir(GetPath(TYPE_SAVE, "editor", aPath, sizeof(aPath)));
			fs_makedir(GetPath(TYPE_SAVE, "ghosts", aPath, sizeof(aPath)));
			fs_makedir(GetPath(TYPE_SAVE, "teehistorian", aPath, sizeof(aPath)));
		}

		return m_NumPaths ? 0 : 1;
	}

	void LoadPaths(const char *pArgv0)
	{
		// check current directory
		IOHANDLE File = io_open("storage.cfg", IOFLAG_READ);
		if(!File)
		{
			// check usable path in argv[0]
			unsigned int Pos = ~0U;
			for(unsigned i = 0; pArgv0[i]; i++)
				if(pArgv0[i] == '/' || pArgv0[i] == '\\')
					Pos = i;
			if(Pos < MAX_PATH_LENGTH)
			{
				char aBuffer[MAX_PATH_LENGTH];
				str_copy(aBuffer, pArgv0, Pos + 1);
				str_append(aBuffer, "/storage.cfg", sizeof(aBuffer));
				File = io_open(aBuffer, IOFLAG_READ);
			}

			if(Pos >= MAX_PATH_LENGTH || !File)
			{
				dbg_msg("storage", "couldn't open storage.cfg");
				return;
			}
		}

		char *pLine;
		CLineReader LineReader;
		LineReader.Init(File);

		while((pLine = LineReader.Get()))
		{
			const char *pLineWithoutPrefix = str_startswith(pLine, "add_path ");
			if(pLineWithoutPrefix)
			{
				AddPath(pLineWithoutPrefix);
			}
		}

		io_close(File);

		if(!m_NumPaths)
			dbg_msg("storage", "no paths found in storage.cfg");
	}

	void AddDefaultPaths()
	{
		AddPath("$USERDIR");
		AddPath("$DATADIR");
		AddPath("$CURRENTDIR");
	}

	void AddPath(const char *pPath)
	{
		if(m_NumPaths >= MAX_PATHS || !pPath[0])
			return;

		if(!str_comp(pPath, "$USERDIR"))
		{
			if(m_aUserdir[0])
			{
				str_copy(m_aaStoragePaths[m_NumPaths++], m_aUserdir, MAX_PATH_LENGTH);
				dbg_msg("storage", "added path '$USERDIR' ('%s')", m_aUserdir);
			}
		}
		else if(!str_comp(pPath, "$DATADIR"))
		{
			if(m_aDatadir[0])
			{
				str_copy(m_aaStoragePaths[m_NumPaths++], m_aDatadir, MAX_PATH_LENGTH);
				dbg_msg("storage", "added path '$DATADIR' ('%s')", m_aDatadir);
			}
		}
		else if(!str_comp(pPath, "$CURRENTDIR"))
		{
			m_aaStoragePaths[m_NumPaths++][0] = 0;
			dbg_msg("storage", "added path '$CURRENTDIR' ('%s')", m_aCurrentdir);
		}
		else
		{
			if(fs_is_dir(pPath))
			{
				str_copy(m_aaStoragePaths[m_NumPaths++], pPath, MAX_PATH_LENGTH);
				dbg_msg("storage", "added path '%s'", pPath);
			}
		}
	}

	void FindDatadir(const char *pArgv0)
	{
		// 1) use data-dir in PWD if present
		if(fs_is_dir("data/mapres"))
		{
			str_copy(m_aDatadir, "data", sizeof(m_aDatadir));
			return;
		}

#if defined(DATA_DIR)
		// 2) use compiled-in data-dir if present
		if(fs_is_dir(DATA_DIR "/mapres"))
		{
			str_copy(m_aDatadir, DATA_DIR, sizeof(m_aDatadir));
			return;
		}
#endif

		// 3) check for usable path in argv[0]
		{
			unsigned int Pos = ~0U;
			for(unsigned i = 0; pArgv0[i]; i++)
				if(pArgv0[i] == '/' || pArgv0[i] == '\\')
					Pos = i;

			if(Pos < MAX_PATH_LENGTH)
			{
				char aBuf[MAX_PATH_LENGTH];
				char aDir[MAX_PATH_LENGTH];
				str_copy(aDir, pArgv0, Pos + 1);
				str_format(aBuf, sizeof(aBuf), "%s/data/mapres", aDir);
				if(fs_is_dir(aBuf))
				{
					str_format(m_aDatadir, sizeof(m_aDatadir), "%s/data", aDir);
					return;
				}
			}
		}

#if defined(CONF_FAMILY_UNIX)
		// 4) check for all default locations
		{
			const char *apDirs[] = {
				"/usr/share/ddnet",
				"/usr/share/games/ddnet",
				"/usr/local/share/ddnet",
				"/usr/local/share/games/ddnet",
				"/usr/pkg/share/ddnet",
				"/usr/pkg/share/games/ddnet",
				"/opt/ddnet"};
			const int DirsCount = sizeof(apDirs) / sizeof(apDirs[0]);

			int i;
			for(i = 0; i < DirsCount; i++)
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "%s/data/mapres", apDirs[i]);
				if(fs_is_dir(aBuf))
				{
					str_format(m_aDatadir, sizeof(m_aDatadir), "%s/data", apDirs[i]);
					return;
				}
			}
		}
#endif

		dbg_msg("storage", "warning: no data directory found");
	}

	void FindBinarydir(const char *pArgv0)
	{
#if defined(BINARY_DIR)
		str_copy(m_aBinarydir, BINARY_DIR, sizeof(m_aBinarydir));
		return;
#endif

		// check for usable path in argv[0]
		{
			unsigned int Pos = ~0U;
			for(unsigned i = 0; pArgv0[i]; i++)
				if(pArgv0[i] == '/' || pArgv0[i] == '\\')
					Pos = i;

			if(Pos < MAX_PATH_LENGTH)
			{
				char aBuf[MAX_PATH_LENGTH];
				str_copy(m_aBinarydir, pArgv0, Pos + 1);
				str_format(aBuf, sizeof(aBuf), "%s/" PLAT_SERVER_EXEC, m_aBinarydir);
				IOHANDLE File = io_open(aBuf, IOFLAG_READ);
				if(File)
				{
					io_close(File);
					return;
				}
				else
				{
#if defined(CONF_PLATFORM_MACOS)
					str_append(m_aBinarydir, "/../../../DDNet-Server.app/Contents/MacOS", sizeof(m_aBinarydir));
					str_format(aBuf, sizeof(aBuf), "%s/" PLAT_SERVER_EXEC, m_aBinarydir);
					IOHANDLE File = io_open(aBuf, IOFLAG_READ);
					if(File)
					{
						io_close(File);
						return;
					}
					else
						m_aBinarydir[0] = 0;
#else
					m_aBinarydir[0] = 0;
#endif
				}
			}
		}

		// no binary directory found, use $PATH on Posix, $PWD on Windows
	}

	virtual bool DIStartCheck(IEngine *pEngine)
	{
		if(m_aDatadir[0])
			m_pCheckJob = std::make_shared<CCheckJob>(m_aDatadir, &m_CheckResult);
		else
			return false;

		dbg_msg("integrity", "starting check");
		pEngine->AddJob(m_pCheckJob);

		return true;
	}

	virtual int DIStatus() const
	{
		if(!m_pCheckJob)
			return INTEGRITY_DISABLED;

		if(m_pCheckJob->Status() != IJob::STATE_DONE)
			return INTEGRITY_PENDING;

		if(!m_CheckResult.m_ExtraFiles.size() && m_CheckResult.m_DataFilePure.all())
			return INTEGRITY_PURE;

		return INTEGRITY_DIRTY;
	}

	virtual const std::vector<std::string> &DIExtraFiles() const
	{
		return m_CheckResult.m_ExtraFiles;
	}

	virtual std::vector<std::string> DIMissingFiles()
	{
		std::vector<std::string> Result;
		for(size_t i = 0; i < m_CheckResult.m_DataFilePresent.size(); i++)
		{
			if(!m_CheckResult.m_DataFilePresent[i])
				Result.emplace_back(g_aDataHashes[i].m_aPath);
		}

		return Result;
	}

	virtual std::vector<std::string> DIModifiedFiles()
	{
		std::vector<std::string> Result;
		for(size_t i = 0; i < m_CheckResult.m_DataFilePure.size(); i++)
		{
			if(m_CheckResult.m_DataFilePresent[i] && !m_CheckResult.m_DataFilePure[i])
				Result.emplace_back(g_aDataHashes[i].m_aPath);
		}

		return Result;
	}

	virtual int Store(const char *pFile)
	{
		dbg_msg("storage", "storing file %s", pFile);

		return 0;
	}

	virtual void ListDirectoryInfo(int Type, const char *pPath, FS_LISTDIR_INFO_CALLBACK pfnCallback, void *pUser)
	{
		char aBuffer[MAX_PATH_LENGTH];
		if(Type == TYPE_ALL)
		{
			// list all available directories
			for(int i = 0; i < m_NumPaths; ++i)
				fs_listdir_info(GetPath(i, pPath, aBuffer, sizeof(aBuffer)), pfnCallback, i, pUser);
		}
		else if(Type >= 0 && Type < m_NumPaths)
		{
			// list wanted directory
			fs_listdir_info(GetPath(Type, pPath, aBuffer, sizeof(aBuffer)), pfnCallback, Type, pUser);
		}
	}

	virtual void ListDirectory(int Type, const char *pPath, FS_LISTDIR_CALLBACK pfnCallback, void *pUser)
	{
		char aBuffer[MAX_PATH_LENGTH];
		if(Type == TYPE_ALL)
		{
			// list all available directories
			for(int i = 0; i < m_NumPaths; ++i)
				fs_listdir(GetPath(i, pPath, aBuffer, sizeof(aBuffer)), pfnCallback, i, pUser);
		}
		else if(Type >= 0 && Type < m_NumPaths)
		{
			// list wanted directory
			fs_listdir(GetPath(Type, pPath, aBuffer, sizeof(aBuffer)), pfnCallback, Type, pUser);
		}
	}

	virtual const char *GetPath(int Type, const char *pDir, char *pBuffer, unsigned BufferSize)
	{
		if(Type == TYPE_ABSOLUTE)
		{
			str_copy(pBuffer, pDir, BufferSize);
		}
		else
		{
			str_format(pBuffer, BufferSize, "%s%s%s", m_aaStoragePaths[Type], !m_aaStoragePaths[Type][0] ? "" : "/", pDir);
		}
		return pBuffer;
	}

	virtual IOHANDLE OpenFile(const char *pFilename, int Flags, int Type, char *pBuffer = 0, int BufferSize = 0)
	{
		char aBuffer[MAX_PATH_LENGTH];
		if(!pBuffer)
		{
			pBuffer = aBuffer;
			BufferSize = sizeof(aBuffer);
		}

		if(Type == TYPE_ABSOLUTE)
		{
			return io_open(pFilename, Flags);
		}
		if(str_startswith(pFilename, "mapres/../skins/"))
		{
			pFilename = pFilename + 10; // just start from skins/
		}
		if(pFilename[0] == '/' || pFilename[0] == '\\' || str_find(pFilename, "../") != NULL || str_find(pFilename, "..\\") != NULL
#ifdef CONF_FAMILY_WINDOWS
			|| (pFilename[0] && pFilename[1] == ':')
#endif
		)
		{
			// don't escape base directory
		}
		else if(Flags & IOFLAG_WRITE)
		{
			return io_open(GetPath(TYPE_SAVE, pFilename, pBuffer, BufferSize), Flags);
		}
		else
		{
			IOHANDLE Handle = 0;

			if(Type <= TYPE_ALL)
			{
				// check all available directories
				for(int i = 0; i < m_NumPaths; ++i)
				{
					Handle = io_open(GetPath(i, pFilename, pBuffer, BufferSize), Flags);
					if(Handle)
						return Handle;
				}
			}
			else if(Type >= 0 && Type < m_NumPaths)
			{
				// check wanted directory
				Handle = io_open(GetPath(Type, pFilename, pBuffer, BufferSize), Flags);
				if(Handle)
					return Handle;
			}
		}

		pBuffer[0] = 0;
		return 0;
	}

	struct CFindCBData
	{
		CStorage *m_pStorage;
		const char *m_pFilename;
		const char *m_pPath;
		char *m_pBuffer;
		int m_BufferSize;
	};

	static int FindFileCallback(const char *pName, int IsDir, int Type, void *pUser)
	{
		CFindCBData Data = *static_cast<CFindCBData *>(pUser);
		if(IsDir)
		{
			if(pName[0] == '.')
				return 0;

			// search within the folder
			char aBuf[MAX_PATH_LENGTH];
			char aPath[MAX_PATH_LENGTH];
			str_format(aPath, sizeof(aPath), "%s/%s", Data.m_pPath, pName);
			Data.m_pPath = aPath;
			fs_listdir(Data.m_pStorage->GetPath(Type, aPath, aBuf, sizeof(aBuf)), FindFileCallback, Type, &Data);
			if(Data.m_pBuffer[0])
				return 1;
		}
		else if(!str_comp(pName, Data.m_pFilename))
		{
			// found the file = end
			str_format(Data.m_pBuffer, Data.m_BufferSize, "%s/%s", Data.m_pPath, Data.m_pFilename);
			return 1;
		}

		return 0;
	}

	virtual bool FindFile(const char *pFilename, const char *pPath, int Type, char *pBuffer, int BufferSize)
	{
		if(BufferSize < 1)
			return false;

		pBuffer[0] = 0;
		char aBuf[MAX_PATH_LENGTH];
		CFindCBData Data;
		Data.m_pStorage = this;
		Data.m_pFilename = pFilename;
		Data.m_pPath = pPath;
		Data.m_pBuffer = pBuffer;
		Data.m_BufferSize = BufferSize;

		if(Type == TYPE_ALL)
		{
			// search within all available directories
			for(int i = 0; i < m_NumPaths; ++i)
			{
				fs_listdir(GetPath(i, pPath, aBuf, sizeof(aBuf)), FindFileCallback, i, &Data);
				if(pBuffer[0])
					return true;
			}
		}
		else if(Type >= 0 && Type < m_NumPaths)
		{
			// search within wanted directory
			fs_listdir(GetPath(Type, pPath, aBuf, sizeof(aBuf)), FindFileCallback, Type, &Data);
		}

		return pBuffer[0] != 0;
	}

	virtual bool RemoveFile(const char *pFilename, int Type)
	{
		if(Type < TYPE_ABSOLUTE || Type == TYPE_ALL || Type >= m_NumPaths)
			return false;

		char aBuffer[MAX_PATH_LENGTH];
		GetPath(Type, pFilename, aBuffer, sizeof(aBuffer));

		bool Success = !fs_remove(aBuffer);
		if(!Success)
			dbg_msg("storage", "failed to remove: %s", aBuffer);
		return Success;
	}

	virtual bool RemoveBinaryFile(const char *pFilename)
	{
		char aBuffer[MAX_PATH_LENGTH];
		GetBinaryPath(pFilename, aBuffer, sizeof(aBuffer));

		bool Success = !fs_remove(aBuffer);
		if(!Success)
			dbg_msg("storage", "failed to remove binary: %s", aBuffer);
		return Success;
	}

	virtual bool RenameFile(const char *pOldFilename, const char *pNewFilename, int Type)
	{
		if(Type < 0 || Type >= m_NumPaths)
			return false;

		char aOldBuffer[MAX_PATH_LENGTH];
		char aNewBuffer[MAX_PATH_LENGTH];
		GetPath(Type, pOldFilename, aOldBuffer, sizeof(aOldBuffer));
		GetPath(Type, pNewFilename, aNewBuffer, sizeof(aNewBuffer));

		bool Success = !fs_rename(aOldBuffer, aNewBuffer);
		if(!Success)
			dbg_msg("storage", "failed to rename: %s -> %s", aOldBuffer, aNewBuffer);
		return Success;
	}

	virtual bool RenameBinaryFile(const char *pOldFilename, const char *pNewFilename)
	{
		char aOldBuffer[MAX_PATH_LENGTH];
		char aNewBuffer[MAX_PATH_LENGTH];
		GetBinaryPath(pOldFilename, aOldBuffer, sizeof(aOldBuffer));
		GetBinaryPath(pNewFilename, aNewBuffer, sizeof(aNewBuffer));

		if(fs_makedir_rec_for(aNewBuffer) < 0)
			dbg_msg("storage", "cannot create folder for: %s", aNewBuffer);

		bool Success = !fs_rename(aOldBuffer, aNewBuffer);
		if(!Success)
			dbg_msg("storage", "failed to rename: %s -> %s", aOldBuffer, aNewBuffer);
		return Success;
	}

	virtual bool CreateFolder(const char *pFoldername, int Type)
	{
		if(Type < 0 || Type >= m_NumPaths)
			return false;

		char aBuffer[MAX_PATH_LENGTH];
		GetPath(Type, pFoldername, aBuffer, sizeof(aBuffer));

		bool Success = !fs_makedir(aBuffer);
		if(!Success)
			dbg_msg("storage", "failed to create folder: %s", aBuffer);
		return Success;
	}

	virtual void GetCompletePath(int Type, const char *pDir, char *pBuffer, unsigned BufferSize)
	{
		if(Type < 0 || Type >= m_NumPaths)
		{
			if(BufferSize > 0)
				pBuffer[0] = 0;
			return;
		}

		GetPath(Type, pDir, pBuffer, BufferSize);
	}

	virtual const char *GetBinaryPath(const char *pFilename, char *pBuffer, unsigned BufferSize)
	{
		str_format(pBuffer, BufferSize, "%s%s%s", m_aBinarydir, !m_aBinarydir[0] ? "" : "/", pFilename);
		return pBuffer;
	}

	static IStorage *Create(const char *pApplicationName, int StorageType, int NumArgs, const char **ppArguments)
	{
		CStorage *p = new CStorage();
		if(p && p->Init(pApplicationName, StorageType, NumArgs, ppArguments))
		{
			dbg_msg("storage", "initialisation failed");
			delete p;
			p = 0;
		}
		return p;
	}
};

void IStorage::StripPathAndExtension(const char *pFilename, char *pBuffer, int BufferSize)
{
	const char *pFilenameEnd = pFilename + str_length(pFilename);
	const char *pExtractedName = pFilename;
	const char *pEnd = pFilenameEnd;
	for(const char *pIter = pFilename; *pIter; pIter++)
	{
		if(*pIter == '/' || *pIter == '\\')
		{
			pExtractedName = pIter + 1;
			pEnd = pFilenameEnd;
		}
		else if(*pIter == '.')
		{
			pEnd = pIter;
		}
	}

	int Length = minimum(BufferSize, (int)(pEnd - pExtractedName + 1));
	str_copy(pBuffer, pExtractedName, Length);
}

IStorage *CreateStorage(const char *pApplicationName, int StorageType, int NumArgs, const char **ppArguments) { return CStorage::Create(pApplicationName, StorageType, NumArgs, ppArguments); }

IStorage *CreateLocalStorage()
{
	CStorage *pStorage = new CStorage();
	if(pStorage)
	{
		if(!fs_getcwd(pStorage->m_aCurrentdir, sizeof(pStorage->m_aCurrentdir)))
		{
			delete pStorage;
			return NULL;
		}
		pStorage->AddPath("$CURRENTDIR");
	}
	return pStorage;
}
