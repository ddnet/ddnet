/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_CONFIG_H
#define ENGINE_SHARED_CONFIG_H

#define CONFIG_FILE "settings_ddnet.cfg"
#define AUTOEXEC_FILE "autoexec.cfg"
#define AUTOEXEC_CLIENT_FILE "autoexec_client.cfg"
#define AUTOEXEC_SERVER_FILE "autoexec_server.cfg"

struct CConfiguration
{
	#define MACRO_CONFIG_INT(Name,ScriptName,Def,Min,Max,Save,Desc) int m_##Name;
	#define MACRO_CONFIG_STR(Name,ScriptName,Len,Def,Save,Desc) char m_##Name[Len]; // Flawfinder: ignore
	#include "config_variables.h"
	#undef MACRO_CONFIG_INT
	#undef MACRO_CONFIG_STR
};

extern CConfiguration g_Config;

enum
{
	CFGFLAG_SAVE=1,
	CFGFLAG_CLIENT=2,
	CFGFLAG_SERVER=4,
	CFGFLAG_STORE=8,
	CFGFLAG_MASTER=16,
	CFGFLAG_ECON=32,
	// DDRace

	CMDFLAG_TEST=64,
	CFGFLAG_CHAT=128,
	CFGFLAG_GAME=256,
	CFGFLAG_NONTEEHISTORIC=512,
};

#endif
