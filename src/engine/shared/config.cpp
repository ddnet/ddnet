/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/config.h>
#include <engine/storage.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

CConfiguration g_Config;

class CConfig : public IConfig
{
	IStorage *m_pStorage;
	IOHANDLE m_ConfigFile;
	bool m_Failed;

	struct CCallback
	{
		SAVECALLBACKFUNC m_pfnFunc;
		void *m_pUserData;
	};

	enum
	{
		MAX_CALLBACKS = 16
	};

	CCallback m_aCallbacks[MAX_CALLBACKS];
	int m_NumCallbacks;

	void EscapeParam(char *pDst, const char *pSrc, int Size)
	{
		str_escape(&pDst, pSrc, pDst + Size);
	}

public:

	CConfig()
	{
		m_ConfigFile = 0;
		m_NumCallbacks = 0;
		m_Failed = false;
	}

	virtual void Init()
	{
		m_pStorage = Kernel()->RequestInterface<IStorage>();
		Reset();
	}

	virtual void Reset()
	{
		#define MACRO_CONFIG_INT(Name,ScriptName,def,min,max,flags,desc) g_Config.m_##Name = def;
		#define MACRO_CONFIG_STR(Name,ScriptName,len,def,flags,desc) str_copy(g_Config.m_##Name, def, len);

		#include "config_variables.h"

		#undef MACRO_CONFIG_INT
		#undef MACRO_CONFIG_STR
	}

	virtual void Save()
	{
		if(!m_pStorage || !g_Config.m_ClSaveSettings)
			return;

		m_ConfigFile = m_pStorage->OpenFile(CONFIG_FILE_TMP, IOFLAG_WRITE, IStorage::TYPE_SAVE);

		if(!m_ConfigFile)
			return;

		m_Failed = false;

		char aLineBuf[1024*2];
		char aEscapeBuf[1024*2];

		#define MACRO_CONFIG_INT(Name,ScriptName,def,min,max,flags,desc) if((flags)&CFGFLAG_SAVE) { str_format(aLineBuf, sizeof(aLineBuf), "%s %i", #ScriptName, g_Config.m_##Name); WriteLine(aLineBuf); }
		#define MACRO_CONFIG_STR(Name,ScriptName,len,def,flags,desc) if((flags)&CFGFLAG_SAVE) { EscapeParam(aEscapeBuf, g_Config.m_##Name, sizeof(aEscapeBuf)); str_format(aLineBuf, sizeof(aLineBuf), "%s \"%s\"", #ScriptName, aEscapeBuf); WriteLine(aLineBuf); }

		#include "config_variables.h"

		#undef MACRO_CONFIG_INT
		#undef MACRO_CONFIG_STR

		for(int i = 0; i < m_NumCallbacks; i++)
			m_aCallbacks[i].m_pfnFunc(this, m_aCallbacks[i].m_pUserData);

		if(io_close(m_ConfigFile) != 0)
			m_Failed = true;

		m_ConfigFile = 0;

		if(m_Failed)
		{
			dbg_msg("config", "ERROR: writing to " CONFIG_FILE_TMP " failed");
			return;
		}

		if(!m_pStorage->RenameFile(CONFIG_FILE_TMP, CONFIG_FILE, IStorage::TYPE_SAVE))
			dbg_msg("config", "ERROR: renaming " CONFIG_FILE_TMP " to " CONFIG_FILE " failed");
	}

	virtual void RegisterCallback(SAVECALLBACKFUNC pfnFunc, void *pUserData)
	{
		dbg_assert(m_NumCallbacks < MAX_CALLBACKS, "too many config callbacks");
		m_aCallbacks[m_NumCallbacks].m_pfnFunc = pfnFunc;
		m_aCallbacks[m_NumCallbacks].m_pUserData = pUserData;
		m_NumCallbacks++;
	}

	virtual void WriteLine(const char *pLine)
	{
		if(!m_ConfigFile ||
			io_write(m_ConfigFile, pLine, str_length(pLine)) != static_cast<unsigned>(str_length(pLine)) ||
#if defined(CONF_FAMILY_WINDOWS)
			io_write_newline(m_ConfigFile) != 2)
#else
			io_write_newline(m_ConfigFile) != 1)
#endif
			m_Failed = true;
	}
};

IConfig *CreateConfig() { return new CConfig; }
