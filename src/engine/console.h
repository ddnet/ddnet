/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CONSOLE_H
#define ENGINE_CONSOLE_H

#include "kernel.h"
#include <base/color.h>
#include <engine/storage.h>

#include <memory>

static const ColorRGBA gs_ConsoleDefaultColor(1, 1, 1, 1);

enum LEVEL : char;
struct CChecksumData;

class IConsole : public IInterface
{
	MACRO_INTERFACE("console")
public:
	//	TODO: rework/cleanup
	enum
	{
		OUTPUT_LEVEL_STANDARD = 0,
		OUTPUT_LEVEL_ADDINFO,
		OUTPUT_LEVEL_DEBUG,

		ACCESS_LEVEL_ADMIN = 0,
		ACCESS_LEVEL_MOD,
		ACCESS_LEVEL_HELPER,
		ACCESS_LEVEL_USER,

		TEMPCMD_NAME_LENGTH = 64,
		TEMPCMD_HELP_LENGTH = 192,
		TEMPCMD_PARAMS_LENGTH = 96,

		CMDLINE_LENGTH = 512,

		CLIENT_ID_GAME = -2,
		CLIENT_ID_NO_GAME = -3,
	};

	// TODO: rework this interface to reduce the amount of virtual calls
	class IResult
	{
	protected:
		unsigned m_NumArgs;

	public:
		IResult(int ClientId) :
			m_NumArgs(0),
			m_ClientId(ClientId) {}
		IResult(const IResult &Other) :
			m_NumArgs(Other.m_NumArgs),
			m_ClientId(Other.m_ClientId) {}
		virtual ~IResult() {}

		virtual int GetInteger(unsigned Index) const = 0;
		virtual float GetFloat(unsigned Index) const = 0;
		virtual const char *GetString(unsigned Index) const = 0;
		virtual std::optional<ColorHSLA> GetColor(unsigned Index, float DarkestLighting) const = 0;

		virtual void RemoveArgument(unsigned Index) = 0;

		int NumArguments() const { return m_NumArgs; }
		int m_ClientId;

		// DDRace

		virtual int GetVictim() const = 0;
	};

	class CCommandInfo
	{
	protected:
		int m_AccessLevel;

	public:
		CCommandInfo() { m_AccessLevel = ACCESS_LEVEL_ADMIN; }
		virtual ~CCommandInfo() {}
		const char *m_pName;
		const char *m_pHelp;
		const char *m_pParams;

		virtual const CCommandInfo *NextCommandInfo(int AccessLevel, int FlagMask) const = 0;

		int GetAccessLevel() const { return m_AccessLevel; }
	};

	typedef void (*FTeeHistorianCommandCallback)(int ClientId, int FlagMask, const char *pCmd, IResult *pResult, void *pUser);
	typedef void (*FPossibleCallback)(int Index, const char *pCmd, void *pUser);
	typedef void (*FCommandCallback)(IResult *pResult, void *pUserData);
	typedef void (*FChainCommandCallback)(IResult *pResult, void *pUserData, FCommandCallback pfnCallback, void *pCallbackUserData);
	typedef bool (*FUnknownCommandCallback)(const char *pCommand, void *pUser); // returns true if the callback has handled the argument

	static void EmptyPossibleCommandCallback(int Index, const char *pCmd, void *pUser) {}
	static bool EmptyUnknownCommandCallback(const char *pCommand, void *pUser) { return false; }

	virtual void Init() = 0;
	virtual const CCommandInfo *FirstCommandInfo(int AccessLevel, int Flagmask) const = 0;
	virtual const CCommandInfo *GetCommandInfo(const char *pName, int FlagMask, bool Temp) = 0;
	virtual int PossibleCommands(const char *pStr, int FlagMask, bool Temp, FPossibleCallback pfnCallback = EmptyPossibleCommandCallback, void *pUser = nullptr) = 0;
	virtual void ParseArguments(int NumArgs, const char **ppArguments) = 0;

	virtual void Register(const char *pName, const char *pParams, int Flags, FCommandCallback pfnFunc, void *pUser, const char *pHelp) = 0;
	virtual void RegisterTemp(const char *pName, const char *pParams, int Flags, const char *pHelp) = 0;
	virtual void DeregisterTemp(const char *pName) = 0;
	virtual void DeregisterTempAll() = 0;
	virtual void Chain(const char *pName, FChainCommandCallback pfnChainFunc, void *pUser) = 0;
	virtual void StoreCommands(bool Store) = 0;

	virtual bool LineIsValid(const char *pStr) = 0;
	virtual void ExecuteLine(const char *pStr, int ClientId = -1, bool InterpretSemicolons = true) = 0;
	virtual void ExecuteLineFlag(const char *pStr, int FlasgMask, int ClientId = -1, bool InterpretSemicolons = true) = 0;
	virtual void ExecuteLineStroked(int Stroke, const char *pStr, int ClientId = -1, bool InterpretSemicolons = true) = 0;
	virtual bool ExecuteFile(const char *pFilename, int ClientId = -1, bool LogFailure = false, int StorageType = IStorage::TYPE_ALL) = 0;

	/**
	 * @deprecated Prefer using the `log_*` functions from base/log.h instead of this function for the following reasons:
	 * - They support `printf`-formatting without a separate buffer.
	 * - They support all five log levels.
	 * - They do not require a pointer to `IConsole` to be used.
	 */
	virtual void Print(int Level, const char *pFrom, const char *pStr, ColorRGBA PrintColor = gs_ConsoleDefaultColor) const = 0;
	virtual void SetTeeHistorianCommandCallback(FTeeHistorianCommandCallback pfnCallback, void *pUser) = 0;
	virtual void SetUnknownCommandCallback(FUnknownCommandCallback pfnCallback, void *pUser) = 0;
	virtual void InitChecksum(CChecksumData *pData) const = 0;

	virtual void SetAccessLevel(int AccessLevel) = 0;

	static LEVEL ToLogLevel(int ConsoleLevel);
	static int ToLogLevelFilter(int ConsoleLevel);

	// DDRace

	virtual bool Cheated() const = 0;

	virtual int FlagMask() const = 0;
	virtual void SetFlagMask(int FlagMask) = 0;
};

std::unique_ptr<IConsole> CreateConsole(int FlagMask);

#endif // FILE_ENGINE_CONSOLE_H
