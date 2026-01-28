/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CONSOLE_H
#define ENGINE_CONSOLE_H

#include "kernel.h"

#include <base/color.h>

#include <engine/storage.h>

#include <memory>

static constexpr ColorRGBA CONSOLE_DEFAULT_COLOR = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);

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
	};

	enum
	{
		TEMPCMD_NAME_LENGTH = 64,
		TEMPCMD_HELP_LENGTH = 192,
		TEMPCMD_PARAMS_LENGTH = 96,

		CMDLINE_LENGTH = 512,

		// The id `-1` has full admin access on the server.
		// It is used in the following cases:
		// - Loading the config file on server start
		// - Econ and fifo connections
		// - Commands run on passed votes
		//
		// Client side it has less importance
		// because there is no difference in access levels.
		CLIENT_ID_UNSPECIFIED = -1,
		CLIENT_ID_GAME = -2,
		CLIENT_ID_NO_GAME = -3,

		FILE_RECURSION_LIMIT = 16,
	};

	enum class EAccessLevel
	{
		ADMIN,
		MODERATOR,
		HELPER,
		USER,
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
		virtual ~IResult() = default;

		virtual int GetInteger(unsigned Index) const = 0;
		virtual float GetFloat(unsigned Index) const = 0;
		virtual const char *GetString(unsigned Index) const = 0;
		virtual ColorHSLA GetColor(unsigned Index, float DarkestLighting) const = 0;

		virtual void RemoveArgument(unsigned Index) = 0;

		int NumArguments() const { return m_NumArgs; }
		int m_ClientId;

		// DDRace

		virtual int GetVictim() const = 0;
	};

	class ICommandInfo
	{
	public:
		virtual ~ICommandInfo() = default;
		virtual const char *Name() const = 0;
		virtual const char *Help() const = 0;
		virtual const char *Params() const = 0;
		virtual int Flags() const = 0;
		virtual EAccessLevel GetAccessLevel() const = 0;
	};

	typedef void (*FTeeHistorianCommandCallback)(int ClientId, int FlagMask, const char *pCmd, IResult *pResult, void *pUser);
	typedef void (*FPossibleCallback)(int Index, const char *pCmd, void *pUser);
	typedef void (*FCommandCallback)(IResult *pResult, void *pUserData);
	typedef void (*FChainCommandCallback)(IResult *pResult, void *pUserData, FCommandCallback pfnCallback, void *pCallbackUserData);
	typedef bool (*FUnknownCommandCallback)(const char *pCommand, void *pUser); // returns true if the callback has handled the argument
	typedef bool (*FCanUseCommandCallback)(int ClientId, const ICommandInfo *pCommand, void *pUser);

	static void EmptyPossibleCommandCallback(int Index, const char *pCmd, void *pUser) {}
	static bool EmptyUnknownCommandCallback(const char *pCommand, void *pUser) { return false; }

	virtual void Init() = 0;
	virtual const ICommandInfo *FirstCommandInfo(int ClientId, int FlagMask) const = 0;
	virtual const ICommandInfo *NextCommandInfo(const IConsole::ICommandInfo *pInfo, int ClientId, int FlagMask) const = 0;
	virtual const ICommandInfo *GetCommandInfo(const char *pName, int FlagMask, bool Temp) = 0;
	virtual int PossibleCommands(const char *pStr, int FlagMask, bool Temp, FPossibleCallback pfnCallback = EmptyPossibleCommandCallback, void *pUser = nullptr) = 0;
	virtual void ParseArguments(int NumArgs, const char **ppArguments) = 0;

	virtual void Register(const char *pName, const char *pParams, int Flags, FCommandCallback pfnFunc, void *pUser, const char *pHelp) = 0;
	virtual void RegisterTemp(const char *pName, const char *pParams, int Flags, const char *pHelp) = 0;
	virtual void DeregisterTemp(const char *pName) = 0;
	virtual void DeregisterTempAll() = 0;
	virtual void Chain(const char *pName, FChainCommandCallback pfnChainFunc, void *pUser) = 0;
	virtual void StoreCommands(bool Store) = 0;

	virtual bool LineIsValid(const char *pStr) = 0;
	virtual void ExecuteLine(const char *pStr, int ClientId, bool InterpretSemicolons = true) = 0;
	virtual void ExecuteLineFlag(const char *pStr, int FlasgMask, int ClientId, bool InterpretSemicolons = true) = 0;
	virtual void ExecuteLineStroked(int Stroke, const char *pStr, int ClientId, bool InterpretSemicolons = true) = 0;
	virtual bool ExecuteFile(const char *pFilename, int ClientId, bool LogFailure = false, int StorageType = IStorage::TYPE_ALL) = 0;

	/**
	 * @deprecated Prefer using the `log_*` functions from base/log.h instead of this function for the following reasons:
	 * - They support `printf`-formatting without a separate buffer.
	 * - They support all five log levels.
	 * - They do not require a pointer to `IConsole` to be used.
	 */
	virtual void Print(int Level, const char *pFrom, const char *pStr, ColorRGBA PrintColor = CONSOLE_DEFAULT_COLOR) const = 0;
	virtual void SetTeeHistorianCommandCallback(FTeeHistorianCommandCallback pfnCallback, void *pUser) = 0;
	virtual void SetUnknownCommandCallback(FUnknownCommandCallback pfnCallback, void *pUser) = 0;
	virtual void SetCanUseCommandCallback(FCanUseCommandCallback pfnCallback, void *pUser) = 0;
	virtual void InitChecksum(CChecksumData *pData) const = 0;

	static LEVEL ToLogLevel(int ConsoleLevel);
	static int ToLogLevelFilter(int ConsoleLevel);

	// DDRace

	virtual bool Cheated() const = 0;

	virtual int FlagMask() const = 0;
	virtual void SetFlagMask(int FlagMask) = 0;
};

std::unique_ptr<IConsole> CreateConsole(int FlagMask);

#endif // FILE_ENGINE_CONSOLE_H
