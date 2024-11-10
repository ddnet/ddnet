/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_CONSOLE_H
#define ENGINE_SHARED_CONSOLE_H

#include "memheap.h"
#include <base/math.h>
#include <base/system.h>
#include <engine/console.h>
#include <engine/storage.h>

class CConsole : public IConsole
{
	class CCommand : public CCommandInfo
	{
	public:
		CCommand *m_pNext;
		int m_Flags;
		bool m_Temp;
		FCommandCallback m_pfnCallback;
		void *m_pUserData;

		const CCommandInfo *NextCommandInfo(int AccessLevel, int FlagMask) const override;

		void SetAccessLevel(int AccessLevel) { m_AccessLevel = clamp(AccessLevel, (int)(ACCESS_LEVEL_ADMIN), (int)(ACCESS_LEVEL_USER)); }
	};

	class CChain
	{
	public:
		FChainCommandCallback m_pfnChainCallback;
		FCommandCallback m_pfnCallback;
		void *m_pCallbackUserData;
		void *m_pUserData;
	};

	int m_FlagMask;
	bool m_StoreCommands;
	const char *m_apStrokeStr[2];
	CCommand *m_pFirstCommand;

	class CExecFile
	{
	public:
		const char *m_pFilename;
		CExecFile *m_pPrev;
	};

	CExecFile *m_pFirstExec;
	IStorage *m_pStorage;
	int m_AccessLevel;

	CCommand *m_pRecycleList;
	CHeap m_TempCommands;

	static void TraverseChain(FCommandCallback *ppfnCallback, void **ppUserData);

	static void Con_Chain(IResult *pResult, void *pUserData);
	static void Con_Echo(IResult *pResult, void *pUserData);
	static void Con_Exec(IResult *pResult, void *pUserData);
	static void ConCommandAccess(IResult *pResult, void *pUser);
	static void ConCommandStatus(IConsole::IResult *pResult, void *pUser);

	void ExecuteLineStroked(int Stroke, const char *pStr, int ClientId = -1, bool InterpretSemicolons = true) override;

	FTeeHistorianCommandCallback m_pfnTeeHistorianCommandCallback;
	void *m_pTeeHistorianCommandUserdata;

	FUnknownCommandCallback m_pfnUnknownCommandCallback = EmptyUnknownCommandCallback;
	void *m_pUnknownCommandUserdata = nullptr;

	enum
	{
		CONSOLE_MAX_STR_LENGTH = 8192,
		MAX_PARTS = (CONSOLE_MAX_STR_LENGTH + 1) / 2
	};

	class CResult : public IResult
	{
	public:
		char m_aStringStorage[CONSOLE_MAX_STR_LENGTH + 1];
		char *m_pArgsStart;

		const char *m_pCommand;
		const char *m_apArgs[MAX_PARTS];

		CResult(int ClientId) :
			IResult(ClientId)
		{
			mem_zero(m_aStringStorage, sizeof(m_aStringStorage));
			m_pArgsStart = 0;
			m_pCommand = 0;
			mem_zero(m_apArgs, sizeof(m_apArgs));
		}

		CResult(const CResult &Other) :
			IResult(Other)
		{
			mem_copy(m_aStringStorage, Other.m_aStringStorage, sizeof(m_aStringStorage));
			m_pArgsStart = m_aStringStorage + (Other.m_pArgsStart - Other.m_aStringStorage);
			m_pCommand = m_aStringStorage + (Other.m_pCommand - Other.m_aStringStorage);
			for(unsigned i = 0; i < Other.m_NumArgs; ++i)
				m_apArgs[i] = m_aStringStorage + (Other.m_apArgs[i] - Other.m_aStringStorage);
		}

		void AddArgument(const char *pArg)
		{
			m_apArgs[m_NumArgs++] = pArg;
		}

		const char *GetString(unsigned Index) const override;
		int GetInteger(unsigned Index) const override;
		float GetFloat(unsigned Index) const override;
		std::optional<ColorHSLA> GetColor(unsigned Index, float DarkestLighting) const override;

		void RemoveArgument(unsigned Index) override
		{
			dbg_assert(Index < m_NumArgs, "invalid argument index");
			for(unsigned i = Index; i < m_NumArgs - 1; i++)
				m_apArgs[i] = m_apArgs[i + 1];

			m_apArgs[m_NumArgs--] = 0;
		}

		// DDRace

		enum
		{
			VICTIM_NONE = -3,
			VICTIM_ME = -2,
			VICTIM_ALL = -1,
		};

		int m_Victim;
		void ResetVictim();
		bool HasVictim() const;
		void SetVictim(int Victim);
		void SetVictim(const char *pVictim);
		int GetVictim() const override;
	};

	int ParseStart(CResult *pResult, const char *pString, int Length);

	enum
	{
		PARSEARGS_OK = 0,
		PARSEARGS_MISSING_VALUE,
		PARSEARGS_INVALID_INTEGER,
		PARSEARGS_INVALID_FLOAT,
	};

	int ParseArgs(CResult *pResult, const char *pFormat, bool IsColor = false);

	/*
	this function will set pFormat to the next parameter (i,s,r,v,?) it contains and
	return the parameter; descriptions in brackets like [file] will be skipped;
	returns '\0' if there is no next parameter; expects pFormat to point at a
	parameter
	*/
	char NextParam(const char *&pFormat);

	class CExecutionQueueEntry
	{
	public:
		CCommand *m_pCommand;
		CResult m_Result;
		CExecutionQueueEntry(CCommand *pCommand, CResult Result) :
			m_pCommand(pCommand),
			m_Result(Result) {}
	};
	std::vector<CExecutionQueueEntry> m_vExecutionQueue;

	void AddCommandSorted(CCommand *pCommand);
	CCommand *FindCommand(const char *pName, int FlagMask);

	bool m_Cheated;

public:
	CConsole(int FlagMask);
	~CConsole();

	void Init() override;
	const CCommandInfo *FirstCommandInfo(int AccessLevel, int FlagMask) const override;
	const CCommandInfo *GetCommandInfo(const char *pName, int FlagMask, bool Temp) override;
	int PossibleCommands(const char *pStr, int FlagMask, bool Temp, FPossibleCallback pfnCallback, void *pUser) override;

	void ParseArguments(int NumArgs, const char **ppArguments) override;
	void Register(const char *pName, const char *pParams, int Flags, FCommandCallback pfnFunc, void *pUser, const char *pHelp) override;
	void RegisterTemp(const char *pName, const char *pParams, int Flags, const char *pHelp) override;
	void DeregisterTemp(const char *pName) override;
	void DeregisterTempAll() override;
	void Chain(const char *pName, FChainCommandCallback pfnChainFunc, void *pUser) override;
	void StoreCommands(bool Store) override;

	bool LineIsValid(const char *pStr) override;
	void ExecuteLine(const char *pStr, int ClientId = -1, bool InterpretSemicolons = true) override;
	void ExecuteLineFlag(const char *pStr, int FlagMask, int ClientId = -1, bool InterpretSemicolons = true) override;
	bool ExecuteFile(const char *pFilename, int ClientId = -1, bool LogFailure = false, int StorageType = IStorage::TYPE_ALL) override;

	void Print(int Level, const char *pFrom, const char *pStr, ColorRGBA PrintColor = gs_ConsoleDefaultColor) const override;
	void SetTeeHistorianCommandCallback(FTeeHistorianCommandCallback pfnCallback, void *pUser) override;
	void SetUnknownCommandCallback(FUnknownCommandCallback pfnCallback, void *pUser) override;
	void InitChecksum(CChecksumData *pData) const override;

	void SetAccessLevel(int AccessLevel) override { m_AccessLevel = clamp(AccessLevel, (int)(ACCESS_LEVEL_ADMIN), (int)(ACCESS_LEVEL_USER)); }

	// DDRace

	static void ConUserCommandStatus(IConsole::IResult *pResult, void *pUser);

	bool Cheated() const override { return m_Cheated; }

	int FlagMask() const override { return m_FlagMask; }
	void SetFlagMask(int FlagMask) override { m_FlagMask = FlagMask; }
};

#endif
