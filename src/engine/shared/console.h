/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_CONSOLE_H
#define ENGINE_SHARED_CONSOLE_H

#include "memheap.h"

#include <engine/console.h>
#include <engine/storage.h>

#include <optional>
#include <vector>

class CConsole : public IConsole
{
	class CCommand : public ICommandInfo
	{
		EAccessLevel m_AccessLevel;
		CCommand *m_pNext;

	public:
		const char *m_pName;
		const char *m_pHelp;
		const char *m_pParams;

		const CCommand *Next() const { return m_pNext; }
		CCommand *Next() { return m_pNext; }
		void SetNext(CCommand *pNext) { m_pNext = pNext; }
		int m_Flags;
		bool m_Temp;
		FCommandCallback m_pfnCallback;
		void *m_pUserData;

		const char *Name() const override { return m_pName; }
		const char *Help() const override { return m_pHelp; }
		const char *Params() const override { return m_pParams; }
		int Flags() const override { return m_Flags; }
		EAccessLevel GetAccessLevel() const override { return m_AccessLevel; }
		void SetAccessLevel(EAccessLevel AccessLevel);
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

	CCommand *m_pRecycleList;
	CHeap m_TempCommands;

	static void TraverseChain(FCommandCallback *ppfnCallback, void **ppUserData);

	static void Con_Chain(IResult *pResult, void *pUserData);
	static void Con_Echo(IResult *pResult, void *pUserData);
	static void Con_Exec(IResult *pResult, void *pUserData);
	static void ConCommandAccess(IResult *pResult, void *pUser);
	static void ConCommandStatus(IConsole::IResult *pResult, void *pUser);
	void PrintCommandList(EAccessLevel MinAccessLevel, int ExcludeFlagMask);

	void ExecuteLineStroked(int Stroke, const char *pStr, int ClientId = IConsole::CLIENT_ID_UNSPECIFIED, bool InterpretSemicolons = true) override;

	FTeeHistorianCommandCallback m_pfnTeeHistorianCommandCallback;
	void *m_pTeeHistorianCommandUserdata;

	FUnknownCommandCallback m_pfnUnknownCommandCallback = EmptyUnknownCommandCallback;
	void *m_pUnknownCommandUserdata = nullptr;

	FCanUseCommandCallback m_pfnCanUseCommandCallback = nullptr;
	void *m_pCanUseCommandUserData;

	bool CanUseCommand(int ClientId, const IConsole::ICommandInfo *pCommand) const;

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

		CResult(int ClientId);
		CResult(const CResult &Other);

		void AddArgument(const char *pArg);
		void RemoveArgument(unsigned Index) override;

		const char *GetString(unsigned Index) const override;
		int GetInteger(unsigned Index) const override;
		float GetFloat(unsigned Index) const override;
		ColorHSLA GetColor(unsigned Index, float DarkestLighting) const override;

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
		PARSEARGS_INVALID_COLOR,
		PARSEARGS_INVALID_FLOAT,
	};

	int ParseArgs(CResult *pResult, const char *pFormat);

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
		CExecutionQueueEntry(CCommand *pCommand, const CResult &Result) :
			m_pCommand(pCommand),
			m_Result(Result) {}
	};
	std::vector<CExecutionQueueEntry> m_vExecutionQueue;

	void AddCommandSorted(CCommand *pCommand);
	CCommand *FindCommand(const char *pName, int FlagMask);

	bool m_Cheated;

public:
	CConsole(int FlagMask);
	~CConsole() override;

	void Init() override;
	const ICommandInfo *FirstCommandInfo(int ClientId, int FlagMask) const override;
	const ICommandInfo *NextCommandInfo(const IConsole::ICommandInfo *pInfo, int ClientId, int FlagMask) const override;
	const ICommandInfo *GetCommandInfo(const char *pName, int FlagMask, bool Temp) override;
	int PossibleCommands(const char *pStr, int FlagMask, bool Temp, FPossibleCallback pfnCallback, void *pUser) override;

	void ParseArguments(int NumArgs, const char **ppArguments) override;
	void Register(const char *pName, const char *pParams, int Flags, FCommandCallback pfnFunc, void *pUser, const char *pHelp) override;
	void RegisterTemp(const char *pName, const char *pParams, int Flags, const char *pHelp) override;
	void DeregisterTemp(const char *pName) override;
	void DeregisterTempAll() override;
	void Chain(const char *pName, FChainCommandCallback pfnChainFunc, void *pUser) override;
	void StoreCommands(bool Store) override;

	bool LineIsValid(const char *pStr) override;
	void ExecuteLine(const char *pStr, int ClientId = IConsole::CLIENT_ID_UNSPECIFIED, bool InterpretSemicolons = true) override;
	void ExecuteLineFlag(const char *pStr, int FlagMask, int ClientId = IConsole::CLIENT_ID_UNSPECIFIED, bool InterpretSemicolons = true) override;
	bool ExecuteFile(const char *pFilename, int ClientId = IConsole::CLIENT_ID_UNSPECIFIED, bool LogFailure = false, int StorageType = IStorage::TYPE_ALL) override;

	void Print(int Level, const char *pFrom, const char *pStr, ColorRGBA PrintColor = gs_ConsoleDefaultColor) const override;
	void SetTeeHistorianCommandCallback(FTeeHistorianCommandCallback pfnCallback, void *pUser) override;
	void SetUnknownCommandCallback(FUnknownCommandCallback pfnCallback, void *pUser) override;
	void SetCanUseCommandCallback(FCanUseCommandCallback pfnCallback, void *pUser) override;
	void InitChecksum(CChecksumData *pData) const override;

	/**
	 * Converts access level string to access level enum.
	 *
	 * @param pAccessLevel should be either "admin", "mod", "moderator", "helper" or "user".
	 * @return `std::nullopt` on error otherwise one of the auth enums such as `EAccessLevel::ADMIN`.
	 */
	static std::optional<EAccessLevel> AccessLevelToEnum(const char *pAccessLevel);

	/**
	 * Converts access level enum to access level string.
	 *
	 * @param AccessLevel should be one of these: `EAccessLevel::ADMIN`, `EAccessLevel::MODERATOR`, `EAccessLevel::HELPER` or `EAccessLevel::USER`.
	 * @return `nullptr` on error or access level string like "admin".
	 */
	static const char *AccessLevelToString(EAccessLevel AccessLevel);

	static std::optional<ColorHSLA> ColorParse(const char *pStr, float DarkestLighting);

	// DDRace

	static void ConUserCommandStatus(IConsole::IResult *pResult, void *pUser);

	bool Cheated() const override { return m_Cheated; }

	int FlagMask() const override { return m_FlagMask; }
	void SetFlagMask(int FlagMask) override { m_FlagMask = FlagMask; }
};

#endif
