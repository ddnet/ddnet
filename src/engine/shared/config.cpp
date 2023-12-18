/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/log.h>

#include <engine/config.h>
#include <engine/shared/config.h>
#include <engine/shared/console.h>
#include <engine/shared/protocol.h>
#include <engine/storage.h>

CConfig g_Config;

CConfigManager::CConfigManager()
{
	m_pConsole = nullptr;
	m_pStorage = nullptr;
	m_ConfigFile = 0;
	m_Failed = false;
}

void CConfigManager::Init()
{
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	const auto &&AddVariable = [this](SConfigVariable *pVariable) {
		m_vpAllVariables.push_back(pVariable);
		if((pVariable->m_Flags & CFGFLAG_GAME) != 0)
			m_vpGameVariables.push_back(pVariable);
		pVariable->Register();
	};

#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) \
	{ \
		const char *pHelp = Min == Max ? Desc " (default: " #Def ")" : (Max == 0 ? Desc " (default: " #Def ", min: " #Min ")" : Desc " (default: " #Def ", min: " #Min ", max: " #Max ")"); \
		AddVariable(m_ConfigHeap.Allocate<SIntConfigVariable>(m_pConsole, #ScriptName, SConfigVariable::VAR_INT, Flags, pHelp, &g_Config.m_##Name, Def, Min, Max)); \
	}

#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) \
	{ \
		const size_t HelpSize = (size_t)str_length(Desc) + 32; \
		char *pHelp = static_cast<char *>(m_ConfigHeap.Allocate(HelpSize)); \
		const bool Alpha = ((Flags)&CFGFLAG_COLALPHA) != 0; \
		str_format(pHelp, HelpSize, "%s (default: $%0*X)", Desc, Alpha ? 8 : 6, color_cast<ColorRGBA>(ColorHSLA(Def, Alpha)).Pack(Alpha)); \
		AddVariable(m_ConfigHeap.Allocate<SColorConfigVariable>(m_pConsole, #ScriptName, SConfigVariable::VAR_COLOR, Flags, pHelp, &g_Config.m_##Name, Def)); \
	}

#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) \
	{ \
		const size_t HelpSize = (size_t)str_length(Desc) + str_length(Def) + 64; \
		char *pHelp = static_cast<char *>(m_ConfigHeap.Allocate(HelpSize)); \
		str_format(pHelp, HelpSize, "%s (default: \"%s\", max length: %d)", Desc, Def, Len - 1); \
		char *pOldValue = static_cast<char *>(m_ConfigHeap.Allocate(Len)); \
		AddVariable(m_ConfigHeap.Allocate<SStringConfigVariable>(m_pConsole, #ScriptName, SConfigVariable::VAR_STRING, Flags, pHelp, g_Config.m_##Name, Def, Len, pOldValue)); \
	}

#include "config_variables.h"

#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR

	m_pConsole->Register("reset", "s[config-name]", CFGFLAG_SERVER | CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Reset, this, "Reset a config to its default value");
	m_pConsole->Register("toggle", "s[config-option] i[value 1] i[value 2]", CFGFLAG_SERVER | CFGFLAG_CLIENT, Con_Toggle, this, "Toggle config value");
	m_pConsole->Register("+toggle", "s[config-option] i[value 1] i[value 2]", CFGFLAG_CLIENT, Con_ToggleStroke, this, "Toggle config value via keypress");
}

void CConfigManager::Reset(const char *pScriptName)
{
	for(SConfigVariable *pVariable : m_vpAllVariables)
	{
		if((pVariable->m_Flags & m_pConsole->FlagMask()) != 0 && str_comp(pScriptName, pVariable->m_pScriptName) == 0)
		{
			pVariable->ResetToDefault();
			return;
		}
	}

	char aBuf[IConsole::CMDLINE_LENGTH + 32];
	str_format(aBuf, sizeof(aBuf), "Invalid command: '%s'.", pScriptName);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
}

void CConfigManager::ResetGameSettings()
{
	for(SConfigVariable *pVariable : m_vpGameVariables)
	{
		pVariable->ResetToOld();
	}
}

void CConfigManager::SetReadOnly(const char *pScriptName, bool ReadOnly)
{
	for(SConfigVariable *pVariable : m_vpAllVariables)
	{
		if(str_comp(pScriptName, pVariable->m_pScriptName) == 0)
		{
			pVariable->m_ReadOnly = ReadOnly;
			return;
		}
	}
	char aBuf[IConsole::CMDLINE_LENGTH + 32];
	str_format(aBuf, sizeof(aBuf), "Invalid command for SetReadOnly: '%s'", pScriptName);
	dbg_assert(false, aBuf);
}

bool CConfigManager::Save()
{
	if(!m_pStorage || !g_Config.m_ClSaveSettings)
		return true;

	char aConfigFileTmp[IO_MAX_PATH_LENGTH];
	m_ConfigFile = m_pStorage->OpenFile(IStorage::FormatTmpPath(aConfigFileTmp, sizeof(aConfigFileTmp), CONFIG_FILE), IOFLAG_WRITE, IStorage::TYPE_SAVE);

	if(!m_ConfigFile)
	{
		log_error("config", "ERROR: opening %s failed", aConfigFileTmp);
		return false;
	}

	m_Failed = false;

	char aLineBuf[2048];
	for(const SConfigVariable *pVariable : m_vpAllVariables)
	{
		if((pVariable->m_Flags & CFGFLAG_SAVE) != 0 && !pVariable->IsDefault())
		{
			pVariable->Serialize(aLineBuf, sizeof(aLineBuf));
			WriteLine(aLineBuf);
		}
	}

	for(const auto &Callback : m_vCallbacks)
	{
		Callback.m_pfnFunc(this, Callback.m_pUserData);
	}

	for(const char *pCommand : m_vpUnknownCommands)
	{
		WriteLine(pCommand);
	}

	if(m_Failed)
	{
		log_error("config", "ERROR: writing to %s failed", aConfigFileTmp);
	}

	if(io_sync(m_ConfigFile) != 0)
	{
		m_Failed = true;
		log_error("config", "ERROR: synchronizing %s failed", aConfigFileTmp);
	}

	if(io_close(m_ConfigFile) != 0)
	{
		m_Failed = true;
		log_error("config", "ERROR: closing %s failed", aConfigFileTmp);
	}

	m_ConfigFile = 0;

	if(m_Failed)
	{
		return false;
	}

	if(!m_pStorage->RenameFile(aConfigFileTmp, CONFIG_FILE, IStorage::TYPE_SAVE))
	{
		log_error("config", "ERROR: renaming %s to " CONFIG_FILE " failed", aConfigFileTmp);
		return false;
	}

	return true;
}

void CConfigManager::RegisterCallback(SAVECALLBACKFUNC pfnFunc, void *pUserData)
{
	m_vCallbacks.emplace_back(pfnFunc, pUserData);
}

void CConfigManager::WriteLine(const char *pLine)
{
	if(!m_ConfigFile ||
		io_write(m_ConfigFile, pLine, str_length(pLine)) != static_cast<unsigned>(str_length(pLine)) ||
		!io_write_newline(m_ConfigFile))
	{
		m_Failed = true;
	}
}

void CConfigManager::StoreUnknownCommand(const char *pCommand)
{
	m_vpUnknownCommands.push_back(m_ConfigHeap.StoreString(pCommand));
}

void CConfigManager::PossibleConfigVariables(const char *pStr, int FlagMask, POSSIBLECFGFUNC pfnCallback, void *pUserData)
{
	for(const SConfigVariable *pVariable : m_vpAllVariables)
	{
		if(pVariable->m_Flags & FlagMask)
		{
			if(str_find_nocase(pVariable->m_pScriptName, pStr))
			{
				pfnCallback(pVariable, pUserData);
			}
		}
	}
}

void CConfigManager::Con_Reset(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CConfigManager *>(pUserData)->Reset(pResult->GetString(0));
}

void CConfigManager::Con_Toggle(IConsole::IResult *pResult, void *pUserData)
{
	CConfigManager *pConfigManager = static_cast<CConfigManager *>(pUserData);
	IConsole *pConsole = pConfigManager->m_pConsole;

	const char *pScriptName = pResult->GetString(0);
	for(SConfigVariable *pVariable : pConfigManager->m_vpAllVariables)
	{
		if((pVariable->m_Flags & pConsole->FlagMask()) == 0 ||
			str_comp(pScriptName, pVariable->m_pScriptName) != 0)
		{
			continue;
		}

		if(pVariable->m_Type == SConfigVariable::VAR_INT)
		{
			SIntConfigVariable *pIntVariable = static_cast<SIntConfigVariable *>(pVariable);
			pIntVariable->SetValue(*pIntVariable->m_pVariable == pResult->GetInteger(1) ? pResult->GetInteger(2) : pResult->GetInteger(1));
		}
		else if(pVariable->m_Type == SConfigVariable::VAR_COLOR)
		{
			SColorConfigVariable *pColorVariable = static_cast<SColorConfigVariable *>(pVariable);
			const float Darkest = pColorVariable->m_Light ? 0.5f : 0.0f;
			const ColorHSLA Value = *pColorVariable->m_pVariable == pResult->GetColor(1, pColorVariable->m_Light).Pack(Darkest, pColorVariable->m_Alpha) ? pResult->GetColor(2, pColorVariable->m_Light) : pResult->GetColor(1, pColorVariable->m_Light);
			pColorVariable->SetValue(Value.Pack(Darkest, pColorVariable->m_Alpha));
		}
		else if(pVariable->m_Type == SConfigVariable::VAR_STRING)
		{
			SStringConfigVariable *pStringVariable = static_cast<SStringConfigVariable *>(pVariable);
			pStringVariable->SetValue(str_comp(pStringVariable->m_pStr, pResult->GetString(1)) == 0 ? pResult->GetString(2) : pResult->GetString(1));
		}
		return;
	}

	char aBuf[IConsole::CMDLINE_LENGTH + 32];
	str_format(aBuf, sizeof(aBuf), "Invalid command: '%s'.", pScriptName);
	pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
}

void CConfigManager::Con_ToggleStroke(IConsole::IResult *pResult, void *pUserData)
{
	CConfigManager *pConfigManager = static_cast<CConfigManager *>(pUserData);
	IConsole *pConsole = pConfigManager->m_pConsole;

	const char *pScriptName = pResult->GetString(1);
	for(SConfigVariable *pVariable : pConfigManager->m_vpAllVariables)
	{
		if((pVariable->m_Flags & pConsole->FlagMask()) == 0 ||
			pVariable->m_Type != SConfigVariable::VAR_INT ||
			str_comp(pScriptName, pVariable->m_pScriptName) != 0)
		{
			continue;
		}

		SIntConfigVariable *pIntVariable = static_cast<SIntConfigVariable *>(pVariable);
		pIntVariable->SetValue(pResult->GetInteger(0) == 0 ? pResult->GetInteger(3) : pResult->GetInteger(2));
		return;
	}

	char aBuf[IConsole::CMDLINE_LENGTH + 32];
	str_format(aBuf, sizeof(aBuf), "Invalid command: '%s'.", pScriptName);
	pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
}

IConfigManager *CreateConfigManager() { return new CConfigManager; }
