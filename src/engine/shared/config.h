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
	CFGFLAG_COLLIGHT7 = 1 << 11,
	CFGFLAG_COLALPHA = 1 << 12,
	CFGFLAG_INSENSITIVE = 1 << 13,
	CMDFLAG_PRACTICE = 1 << 14,
};

struct SConfigVariable
{
	enum EVariableType
	{
		VAR_INT,
		VAR_COLOR,
		VAR_STRING,
	};
	IConsole *m_pConsole;
	const char *m_pScriptName;
	EVariableType m_Type;
	int m_Flags;
	const char *m_pHelp;
	// Note that this only applies to the console command and the SetValue function,
	// but the underlying config variable can still be modified programatically.
	bool m_ReadOnly = false;

	SConfigVariable(IConsole *pConsole, const char *pScriptName, EVariableType Type, int Flags, const char *pHelp) :
		m_pConsole(pConsole),
		m_pScriptName(pScriptName),
		m_Type(Type),
		m_Flags(Flags),
		m_pHelp(pHelp)
	{
	}

	virtual ~SConfigVariable() = default;

	virtual void Register() = 0;
	virtual bool IsDefault() const = 0;
	virtual void Serialize(char *pOut, size_t Size) const = 0;
	virtual void ResetToDefault() = 0;
	virtual void ResetToOld() = 0;

protected:
	void ExecuteLine(const char *pLine) const;
	bool CheckReadOnly() const;
};

struct SIntConfigVariable : public SConfigVariable
{
	int *m_pVariable;
	int m_Default;
	int m_Min;
	int m_Max;
	int m_OldValue;

	SIntConfigVariable(IConsole *pConsole, const char *pScriptName, EVariableType Type, int Flags, const char *pHelp, int *pVariable, int Default, int Min, int Max) :
		SConfigVariable(pConsole, pScriptName, Type, Flags, pHelp),
		m_pVariable(pVariable),
		m_Default(Default),
		m_Min(Min),
		m_Max(Max),
		m_OldValue(Default)
	{
		*m_pVariable = m_Default;
	}

	~SIntConfigVariable() override = default;

	static void CommandCallback(IConsole::IResult *pResult, void *pUserData);
	void Register() override;
	bool IsDefault() const override;
	void Serialize(char *pOut, size_t Size, int Value) const;
	void Serialize(char *pOut, size_t Size) const override;
	void SetValue(int Value);
	void ResetToDefault() override;
	void ResetToOld() override;
};

struct SColorConfigVariable : public SConfigVariable
{
	unsigned *m_pVariable;
	unsigned m_Default;
	float m_DarkestLighting;
	bool m_Alpha;
	unsigned m_OldValue;

	SColorConfigVariable(IConsole *pConsole, const char *pScriptName, EVariableType Type, int Flags, const char *pHelp, unsigned *pVariable, unsigned Default) :
		SConfigVariable(pConsole, pScriptName, Type, Flags, pHelp),
		m_pVariable(pVariable),
		m_Default(Default),
		m_Alpha(Flags & CFGFLAG_COLALPHA),
		m_OldValue(Default)
	{
		*m_pVariable = m_Default;
		if(Flags & CFGFLAG_COLLIGHT)
		{
			m_DarkestLighting = ColorHSLA::DARKEST_LGT;
		}
		else if(Flags & CFGFLAG_COLLIGHT7)
		{
			m_DarkestLighting = ColorHSLA::DARKEST_LGT7;
		}
		else
		{
			m_DarkestLighting = 0.0f;
		}
	}

	~SColorConfigVariable() override = default;

	static void CommandCallback(IConsole::IResult *pResult, void *pUserData);
	void Register() override;
	bool IsDefault() const override;
	void Serialize(char *pOut, size_t Size, unsigned Value) const;
	void Serialize(char *pOut, size_t Size) const override;
	void SetValue(unsigned Value);
	void ResetToDefault() override;
	void ResetToOld() override;
};

struct SStringConfigVariable : public SConfigVariable
{
	char *m_pStr;
	const char *m_pDefault;
	size_t m_MaxSize;
	char *m_pOldValue;

	SStringConfigVariable(IConsole *pConsole, const char *pScriptName, EVariableType Type, int Flags, const char *pHelp, char *pStr, const char *pDefault, size_t MaxSize, char *pOldValue);
	~SStringConfigVariable() override = default;

	static void CommandCallback(IConsole::IResult *pResult, void *pUserData);
	void Register() override;
	bool IsDefault() const override;
	void Serialize(char *pOut, size_t Size, const char *pValue) const;
	void Serialize(char *pOut, size_t Size) const override;
	void SetValue(const char *pValue);
	void ResetToDefault() override;
	void ResetToOld() override;
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

	std::vector<SConfigVariable *> m_vpAllVariables;
	std::vector<SConfigVariable *> m_vpGameVariables;
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

	void PossibleConfigVariables(const char *pStr, int FlagMask, POSSIBLECFGFUNC pfnCallback, void *pUserData) override;
};

#endif
