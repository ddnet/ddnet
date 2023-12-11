/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_CONFIG_H
#define ENGINE_SHARED_CONFIG_H

#include <base/detect.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/shared/memheap.h>

#include <vector>

// include protocol for MAX_CLIENT used in config_variables
#include <engine/shared/protocol.h>

#define CONFIG_FILE "settings_ddnet.cfg"
#define AUTOEXEC_FILE "autoexec.cfg"
#define AUTOEXEC_CLIENT_FILE "autoexec_client.cfg"
#define AUTOEXEC_SERVER_FILE "autoexec_server.cfg"

class CConfig
{
public:
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) \
	static constexpr int ms_##Name = Def; \
	int m_##Name;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) \
	static constexpr unsigned ms_##Name = Def; \
	unsigned m_##Name;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) \
	static constexpr const char *ms_p##Name = Def; \
	char m_##Name[Len]; // Flawfinder: ignore
#include "config_variables.h"
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR
};

extern CConfig g_Config;

enum
{
	CFGFLAG_SAVE = 1 << 0,
	CFGFLAG_CLIENT = 1 << 1,
	CFGFLAG_SERVER = 1 << 2,
	CFGFLAG_STORE = 1 << 3,
	CFGFLAG_MASTER = 1 << 4,
	CFGFLAG_ECON = 1 << 5,
	// DDRace

	CMDFLAG_TEST = 1 << 6,
	CFGFLAG_CHAT = 1 << 7,
	CFGFLAG_GAME = 1 << 8,
	CFGFLAG_NONTEEHISTORIC = 1 << 9,
	CFGFLAG_COLLIGHT = 1 << 10,
	CFGFLAG_COLALPHA = 1 << 11,
	CFGFLAG_INSENSITIVE = 1 << 12,
};

class CConfigManager : public IConfigManager
{
	IConsole *m_pConsole;
	class IStorage *m_pStorage;

	IOHANDLE m_ConfigFile;
	bool m_Failed;

	struct SCallback
	{
		SAVECALLBACKFUNC m_pfnFunc;
		void *m_pUserData;

		SCallback(SAVECALLBACKFUNC pfnFunc, void *pUserData) :
			m_pfnFunc(pfnFunc),
			m_pUserData(pUserData)
		{
		}
	};
	std::vector<SCallback> m_vCallbacks;

	std::vector<struct SConfigVariable *> m_vpAllVariables;
	std::vector<struct SConfigVariable *> m_vpGameVariables;
	std::vector<const char *> m_vpUnknownCommands;
	CHeap m_ConfigHeap;

	static void Con_Reset(IConsole::IResult *pResult, void *pUserData);
	static void Con_Toggle(IConsole::IResult *pResult, void *pUserData);
	static void Con_ToggleStroke(IConsole::IResult *pResult, void *pUserData);

public:
	CConfigManager();

	void Init() override;
	void Reset(const char *pScriptName) override;
	void ResetGameSettings() override;
	void SetReadOnly(const char *pScriptName, bool ReadOnly) override;
	bool Save() override;
	CConfig *Values() override { return &g_Config; }

	void RegisterCallback(SAVECALLBACKFUNC pfnFunc, void *pUserData) override;

	void WriteLine(const char *pLine) override;

	void StoreUnknownCommand(const char *pCommand) override;
};

#endif
