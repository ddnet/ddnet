/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_CONFIG_H
#define ENGINE_SHARED_CONFIG_H

#include <base/detect.h>
#include <base/system.h>

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

static void EscapeParam(char *pDst, const char *pSrc, int Size)
{
	str_escape(&pDst, pSrc, pDst + Size);
}

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
	void ExecuteLine(const char *pLine) const
	{
		m_pConsole->ExecuteLine(pLine, (m_Flags & CFGFLAG_GAME) != 0 ? IConsole::CLIENT_ID_GAME : -1);
	}

	bool CheckReadOnly() const
	{
		if(!m_ReadOnly)
			return false;
		char aBuf[IConsole::CMDLINE_LENGTH + 64];
		str_format(aBuf, sizeof(aBuf), "The config variable '%s' cannot be changed right now.", m_pScriptName);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
		return true;
	}
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

	static void CommandCallback(IConsole::IResult *pResult, void *pUserData)
	{
		SIntConfigVariable *pData = static_cast<SIntConfigVariable *>(pUserData);

		if(pResult->NumArguments())
		{
			if(pData->CheckReadOnly())
				return;

			int Value = pResult->GetInteger(0);

			// do clamping
			if(pData->m_Min != pData->m_Max)
			{
				if(Value < pData->m_Min)
					Value = pData->m_Min;
				if(pData->m_Max != 0 && Value > pData->m_Max)
					Value = pData->m_Max;
			}

			*pData->m_pVariable = Value;
			if(pResult->m_ClientID != IConsole::CLIENT_ID_GAME)
				pData->m_OldValue = Value;
		}
		else
		{
			char aBuf[32];
			str_format(aBuf, sizeof(aBuf), "Value: %d", *pData->m_pVariable);
			pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
		}
	}

	void Register() override
	{
		m_pConsole->Register(m_pScriptName, "?i", m_Flags, CommandCallback, this, m_pHelp);
	}

	bool IsDefault() const override
	{
		return *m_pVariable == m_Default;
	}

	void Serialize(char *pOut, size_t Size, int Value) const
	{
		str_format(pOut, Size, "%s %i", m_pScriptName, Value);
	}

	void Serialize(char *pOut, size_t Size) const override
	{
		Serialize(pOut, Size, *m_pVariable);
	}

	void SetValue(int Value)
	{
		if(CheckReadOnly())
			return;
		char aBuf[IConsole::CMDLINE_LENGTH];
		Serialize(aBuf, sizeof(aBuf), Value);
		ExecuteLine(aBuf);
	}

	void ResetToDefault() override
	{
		SetValue(m_Default);
	}

	void ResetToOld() override
	{
		*m_pVariable = m_OldValue;
	}
};

struct SColorConfigVariable : public SConfigVariable
{
	unsigned *m_pVariable;
	unsigned m_Default;
	bool m_Light;
	bool m_Alpha;
	unsigned m_OldValue;

	SColorConfigVariable(IConsole *pConsole, const char *pScriptName, EVariableType Type, int Flags, const char *pHelp, unsigned *pVariable, unsigned Default) :
		SConfigVariable(pConsole, pScriptName, Type, Flags, pHelp),
		m_pVariable(pVariable),
		m_Default(Default),
		m_Light(Flags & CFGFLAG_COLLIGHT),
		m_Alpha(Flags & CFGFLAG_COLALPHA),
		m_OldValue(Default)
	{
		*m_pVariable = m_Default;
	}

	~SColorConfigVariable() override = default;

	static void CommandCallback(IConsole::IResult *pResult, void *pUserData)
	{
		SColorConfigVariable *pData = static_cast<SColorConfigVariable *>(pUserData);

		if(pResult->NumArguments())
		{
			if(pData->CheckReadOnly())
				return;

			const ColorHSLA Color = pResult->GetColor(0, pData->m_Light);
			const unsigned Value = Color.Pack(pData->m_Light ? 0.5f : 0.0f, pData->m_Alpha);

			*pData->m_pVariable = Value;
			if(pResult->m_ClientID != IConsole::CLIENT_ID_GAME)
				pData->m_OldValue = Value;
		}
		else
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "Value: %u", *pData->m_pVariable);
			pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);

			ColorHSLA Hsla = ColorHSLA(*pData->m_pVariable, true);
			if(pData->m_Light)
				Hsla = Hsla.UnclampLighting();
			str_format(aBuf, sizeof(aBuf), "H: %d°, S: %d%%, L: %d%%", round_truncate(Hsla.h * 360), round_truncate(Hsla.s * 100), round_truncate(Hsla.l * 100));
			pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);

			const ColorRGBA Rgba = color_cast<ColorRGBA>(Hsla);
			str_format(aBuf, sizeof(aBuf), "R: %d, G: %d, B: %d, #%06X", round_truncate(Rgba.r * 255), round_truncate(Rgba.g * 255), round_truncate(Rgba.b * 255), Rgba.Pack(false));
			pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);

			if(pData->m_Alpha)
			{
				str_format(aBuf, sizeof(aBuf), "A: %d%%", round_truncate(Hsla.a * 100));
				pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
			}
		}
	}

	void Register() override
	{
		m_pConsole->Register(m_pScriptName, "?i", m_Flags, CommandCallback, this, m_pHelp);
	}

	bool IsDefault() const override
	{
		return *m_pVariable == m_Default;
	}

	void Serialize(char *pOut, size_t Size, unsigned Value) const
	{
		str_format(pOut, Size, "%s %u", m_pScriptName, Value);
	}

	void Serialize(char *pOut, size_t Size) const override
	{
		Serialize(pOut, Size, *m_pVariable);
	}

	void SetValue(unsigned Value)
	{
		if(CheckReadOnly())
			return;
		char aBuf[IConsole::CMDLINE_LENGTH];
		Serialize(aBuf, sizeof(aBuf), Value);
		ExecuteLine(aBuf);
	}

	void ResetToDefault() override
	{
		SetValue(m_Default);
	}

	void ResetToOld() override
	{
		*m_pVariable = m_OldValue;
	}
};

struct SStringConfigVariable : public SConfigVariable
{
	char *m_pStr;
	const char *m_pDefault;
	size_t m_MaxSize;
	char *m_pOldValue;

	SStringConfigVariable(IConsole *pConsole, const char *pScriptName, EVariableType Type, int Flags, const char *pHelp, char *pStr, const char *pDefault, size_t MaxSize, char *pOldValue) :
		SConfigVariable(pConsole, pScriptName, Type, Flags, pHelp),
		m_pStr(pStr),
		m_pDefault(pDefault),
		m_MaxSize(MaxSize),
		m_pOldValue(pOldValue)
	{
		str_copy(m_pStr, m_pDefault, m_MaxSize);
		str_copy(m_pOldValue, m_pDefault, m_MaxSize);
	}

	~SStringConfigVariable() override = default;

	static void CommandCallback(IConsole::IResult *pResult, void *pUserData)
	{
		SStringConfigVariable *pData = static_cast<SStringConfigVariable *>(pUserData);

		if(pResult->NumArguments())
		{
			if(pData->CheckReadOnly())
				return;

			const char *pString = pResult->GetString(0);
			if(!str_utf8_check(pString))
			{
				char aTemp[4];
				size_t Length = 0;
				while(*pString)
				{
					size_t Size = str_utf8_encode(aTemp, static_cast<unsigned char>(*pString++));
					if(Length + Size < pData->m_MaxSize)
					{
						mem_copy(pData->m_pStr + Length, aTemp, Size);
						Length += Size;
					}
					else
						break;
				}
				pData->m_pStr[Length] = '\0';
			}
			else
				str_copy(pData->m_pStr, pString, pData->m_MaxSize);

			if(pResult->m_ClientID != IConsole::CLIENT_ID_GAME)
				str_copy(pData->m_pOldValue, pData->m_pStr, pData->m_MaxSize);
		}
		else
		{
			char aBuf[1024];
			str_format(aBuf, sizeof(aBuf), "Value: %s", pData->m_pStr);
			pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "config", aBuf);
		}
	}

	void Register() override
	{
		m_pConsole->Register(m_pScriptName, "?r", m_Flags, CommandCallback, this, m_pHelp);
	}

	bool IsDefault() const override
	{
		return str_comp(m_pStr, m_pDefault) == 0;
	}

	void Serialize(char *pOut, size_t Size, const char *pValue) const
	{
		str_copy(pOut, m_pScriptName, Size);
		str_append(pOut, " \"", Size);
		const int OutLen = str_length(pOut);
		EscapeParam(pOut + OutLen, pValue, Size - OutLen - 1); // -1 to ensure space for final quote
		str_append(pOut, "\"", Size);
	}

	void Serialize(char *pOut, size_t Size) const override
	{
		Serialize(pOut, Size, m_pStr);
	}

	void SetValue(const char *pValue)
	{
		if(CheckReadOnly())
			return;
		char aBuf[2048];
		Serialize(aBuf, sizeof(aBuf), pValue);
		ExecuteLine(aBuf);
	}

	void ResetToDefault() override
	{
		SetValue(m_pDefault);
	}

	void ResetToOld() override
	{
		str_copy(m_pStr, m_pOldValue, m_MaxSize);
	}
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
