/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CONFIG_H
#define ENGINE_CONFIG_H

#include "kernel.h"

class IConfigManager : public IInterface
{
	MACRO_INTERFACE("config")
public:
	typedef void (*SAVECALLBACKFUNC)(IConfigManager *pConfig, void *pUserData);
	typedef void (*POSSIBLECFGFUNC)(const struct SConfigVariable *, void *pUserData);

	enum class EInitializationType
	{
		// Set all config variables to their default values.
		STANDALONE,
		// For an integrated server sharing the process-global config with a
		// running client: set only the variables exclusive to the server to
		// their default values, so every server start begins from a clean
		// server configuration without touching the client's configuration.
		INTEGRATED_SERVER,
	};

	virtual void Init(EInitializationType InitializationType = EInitializationType::STANDALONE) = 0;
	virtual void Reset(const char *pScriptName) = 0;
	virtual void ResetGameSettings() = 0;
	virtual void SetReadOnly(const char *pScriptName, bool ReadOnly) = 0;
	virtual void SetGameSettingsReadOnly(bool ReadOnly) = 0;
	virtual bool Save() = 0;
	virtual class CConfig *Values() = 0;

	virtual void RegisterCallback(SAVECALLBACKFUNC pfnFunc, void *pUserData) = 0;

	virtual void WriteLine(const char *pLine) = 0;

	virtual void StoreUnknownCommand(const char *pCommand) = 0;

	virtual void PossibleConfigVariables(const char *pStr, int FlagMask, POSSIBLECFGFUNC pfnCallback, void *pUserData) = 0;
};

extern IConfigManager *CreateConfigManager();

#endif
