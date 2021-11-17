/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_CONFIG_H
#define ENGINE_SHARED_CONFIG_H

#include <base/detect.h>
#include <engine/config.h>

#define CONFIG_FILE "settings_ddnet.cfg"
#define AUTOEXEC_FILE "autoexec.cfg"
#define AUTOEXEC_CLIENT_FILE "autoexec_client.cfg"
#define AUTOEXEC_SERVER_FILE "autoexec_server.cfg"

class CConfig
{
public:
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) int m_##Name;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) unsigned m_##Name;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) char m_##Name[Len]; // Flawfinder: ignore
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
};

class CConfigManager : public IConfigManager
{
	enum
	{
		MAX_CALLBACKS = 16
	};

	struct CCallback
	{
		SAVECALLBACKFUNC m_pfnFunc;
		void *m_pUserData;
	};

	class IStorage *m_pStorage;
	IOHANDLE m_ConfigFile;
	bool m_Failed;
	CCallback m_aCallbacks[MAX_CALLBACKS];
	int m_NumCallbacks;

public:
	CConfigManager();

	virtual void Init();
	virtual void Reset();
	virtual bool Save();
	virtual CConfig *Values() { return &g_Config; }

	virtual void RegisterCallback(SAVECALLBACKFUNC pfnFunc, void *pUserData);

	virtual void WriteLine(const char *pLine);
};

#endif
