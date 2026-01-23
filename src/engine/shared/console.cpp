/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "console.h"

#include "config.h"
#include "linereader.h"

#include <base/color.h>
#include <base/log.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/client/checksum.h>
#include <engine/console.h>
#include <engine/shared/protocol.h>
#include <engine/storage.h>

#include <algorithm>
#include <iterator> // std::size
#include <new>

// todo: rework this

CConsole::CResult::CResult(int ClientId) :
	IResult(ClientId)
{
	mem_zero(m_aStringStorage, sizeof(m_aStringStorage));
	m_pArgsStart = nullptr;
	m_pCommand = nullptr;
	std::fill(std::begin(m_apArgs), std::end(m_apArgs), nullptr);
}

CConsole::CResult::CResult(const CResult &Other) :
	IResult(Other)
{
	mem_copy(m_aStringStorage, Other.m_aStringStorage, sizeof(m_aStringStorage));
	m_pArgsStart = m_aStringStorage + (Other.m_pArgsStart - Other.m_aStringStorage);
	m_pCommand = m_aStringStorage + (Other.m_pCommand - Other.m_aStringStorage);
	for(unsigned i = 0; i < Other.m_NumArgs; ++i)
		m_apArgs[i] = m_aStringStorage + (Other.m_apArgs[i] - Other.m_aStringStorage);
}

void CConsole::CResult::AddArgument(const char *pArg)
{
	m_apArgs[m_NumArgs++] = pArg;
}

void CConsole::CResult::RemoveArgument(unsigned Index)
{
	dbg_assert(Index < m_NumArgs, "invalid argument index");
	for(unsigned i = Index; i < m_NumArgs - 1; i++)
		m_apArgs[i] = m_apArgs[i + 1];

	m_apArgs[m_NumArgs--] = nullptr;
}

const char *CConsole::CResult::GetString(unsigned Index) const
{
	if(Index >= m_NumArgs)
		return "";
	return m_apArgs[Index];
}

int CConsole::CResult::GetInteger(unsigned Index) const
{
	if(Index >= m_NumArgs)
		return 0;
	int Out;
	return str_toint(m_apArgs[Index], &Out) ? Out : 0;
}

float CConsole::CResult::GetFloat(unsigned Index) const
{
	if(Index >= m_NumArgs)
		return 0.0f;
	float Out;
	return str_tofloat(m_apArgs[Index], &Out) ? Out : 0.0f;
}

ColorHSLA CConsole::CResult::GetColor(unsigned Index, float DarkestLighting) const
{
	if(Index >= m_NumArgs)
		return ColorHSLA(0, 0, 0);
	return ColorParse(m_apArgs[Index], DarkestLighting).value_or(ColorHSLA(0, 0, 0));
}

void CConsole::CCommand::SetAccessLevel(EAccessLevel AccessLevel)
{
	m_AccessLevel = AccessLevel;
}

const IConsole::ICommandInfo *CConsole::FirstCommandInfo(int ClientId, int FlagMask) const
{
	for(const CCommand *pCommand = m_pFirstCommand; pCommand; pCommand = pCommand->Next())
	{
		if(pCommand->m_Flags & FlagMask && CanUseCommand(ClientId, pCommand))
			return pCommand;
	}

	return nullptr;
}

const IConsole::ICommandInfo *CConsole::NextCommandInfo(const IConsole::ICommandInfo *pInfo, int ClientId, int FlagMask) const
{
	const CCommand *pNext = ((CCommand *)pInfo)->Next();
	while(pNext)
	{
		if(pNext->m_Flags & FlagMask && CanUseCommand(ClientId, pNext))
			break;
		pNext = pNext->Next();
	}
	return pNext;
}

std::optional<CConsole::EAccessLevel> CConsole::AccessLevelToEnum(const char *pAccessLevel)
{
	// alias for legacy integer access levels
	if(!str_comp(pAccessLevel, "0"))
		return EAccessLevel::ADMIN;
	if(!str_comp(pAccessLevel, "1"))
		return EAccessLevel::MODERATOR;
	if(!str_comp(pAccessLevel, "2"))
		return EAccessLevel::HELPER;
	if(!str_comp(pAccessLevel, "3"))
		return EAccessLevel::USER;

	// string access levels
	if(!str_comp(pAccessLevel, "admin"))
		return EAccessLevel::ADMIN;
	if(!str_comp(pAccessLevel, "moderator"))
		return EAccessLevel::MODERATOR;
	if(!str_comp(pAccessLevel, "helper"))
		return EAccessLevel::HELPER;
	if(!str_comp(pAccessLevel, "all"))
		return EAccessLevel::USER;
	return std::nullopt;
}

const char *CConsole::AccessLevelToString(EAccessLevel AccessLevel)
{
	switch(AccessLevel)
	{
	case EAccessLevel::ADMIN:
		return "admin";
	case EAccessLevel::MODERATOR:
		return "moderator";
	case EAccessLevel::HELPER:
		return "helper";
	case EAccessLevel::USER:
		return "all";
	}
	dbg_assert_failed("invalid access level: %d", (int)AccessLevel);
}

// the maximum number of tokens occurs in a string of length CONSOLE_MAX_STR_LENGTH with tokens size 1 separated by single spaces

int CConsole::ParseStart(CResult *pResult, const char *pString, int Length)
{
	char *pStr;
	int Len = sizeof(pResult->m_aStringStorage);
	if(Length < Len)
		Len = Length;

	str_copy(pResult->m_aStringStorage, pString, Len);
	pStr = pResult->m_aStringStorage;

	// get command
	pStr = str_skip_whitespaces(pStr);
	pResult->m_pCommand = pStr;
	pStr = str_skip_to_whitespace(pStr);

	if(*pStr)
	{
		pStr[0] = 0;
		pStr++;
	}

	pResult->m_pArgsStart = pStr;
	return 0;
}

int CConsole::ParseArgs(CResult *pResult, const char *pFormat)
{
	char *pStr = pResult->m_pArgsStart;
	bool Optional = false;

	pResult->ResetVictim();

	for(char Command = *pFormat; Command != '\0'; Command = NextParam(pFormat))
	{
		if(Command == '?')
		{
			Optional = true;
			continue;
		}

		pStr = str_skip_whitespaces(pStr);

		if(*pStr == '\0') // error, non optional command needs value
		{
			if(!Optional)
			{
				return PARSEARGS_MISSING_VALUE;
			}

			while(Command)
			{
				if(Command == 'v')
				{
					pResult->SetVictim(CResult::VICTIM_ME);
					break;
				}
				Command = NextParam(pFormat);
			}
			return PARSEARGS_OK;
		}

		// add token
		if(*pStr == '"')
		{
			pStr++;
			pResult->AddArgument(pStr);

			char *pDst = pStr; // we might have to process escape data
			while(pStr[0] != '"')
			{
				if(pStr[0] == '\\')
				{
					if(pStr[1] == '\\')
						pStr++; // skip due to escape
					else if(pStr[1] == '"')
						pStr++; // skip due to escape
				}
				else if(pStr[0] == '\0')
				{
					return PARSEARGS_MISSING_VALUE; // return error
				}

				*pDst = *pStr;
				pDst++;
				pStr++;
			}
			*pDst = '\0';

			pStr++;
		}
		else
		{
			pResult->AddArgument(pStr);

			if(Command == 'r') // rest of the string
			{
				return PARSEARGS_OK;
			}

			pStr = str_skip_to_whitespace(pStr);
			if(pStr[0] != '\0') // check for end of string
			{
				pStr[0] = '\0';
				pStr++;
			}

			// validate arguments
			if(Command == 'v')
			{
				pResult->SetVictim(pResult->GetString(pResult->NumArguments() - 1));
			}
			else if(Command == 'i')
			{
				int Value;
				if(!str_toint(pResult->GetString(pResult->NumArguments() - 1), &Value) ||
					Value == std::numeric_limits<int>::max() ||
					Value == std::numeric_limits<int>::min())
				{
					return PARSEARGS_INVALID_INTEGER;
				}
			}
			else if(Command == 'c')
			{
				auto Color = ColorParse(pResult->GetString(pResult->NumArguments() - 1), 0.0f);
				if(!Color.has_value())
				{
					return PARSEARGS_INVALID_COLOR;
				}
			}
			else if(Command == 'f')
			{
				float Value;
				if(!str_tofloat(pResult->GetString(pResult->NumArguments() - 1), &Value) ||
					Value == std::numeric_limits<float>::max() ||
					Value == std::numeric_limits<float>::min())
				{
					return PARSEARGS_INVALID_FLOAT;
				}
			}
			// 's' and unknown commands are handled as strings
		}
	}

	return PARSEARGS_OK;
}

char CConsole::NextParam(const char *&pFormat)
{
	if(*pFormat)
	{
		pFormat++;

		if(*pFormat == '[')
		{
			// skip bracket contents
			for(; *pFormat != ']'; pFormat++)
			{
				if(!*pFormat)
					return *pFormat;
			}

			// skip ']'
			pFormat++;

			// skip space if there is one
			if(*pFormat == ' ')
				pFormat++;
		}
	}
	return *pFormat;
}

LEVEL IConsole::ToLogLevel(int Level)
{
	switch(Level)
	{
	case IConsole::OUTPUT_LEVEL_STANDARD:
		return LEVEL_INFO;
	case IConsole::OUTPUT_LEVEL_ADDINFO:
		return LEVEL_DEBUG;
	case IConsole::OUTPUT_LEVEL_DEBUG:
		return LEVEL_TRACE;
	}
	dbg_assert(0, "invalid log level");
	return LEVEL_INFO;
}

int IConsole::ToLogLevelFilter(int Level)
{
	if(!(-3 <= Level && Level <= 2))
	{
		dbg_assert(0, "invalid log level filter");
	}
	return Level + 2;
}

static LOG_COLOR ColorToLogColor(ColorRGBA Color)
{
	return LOG_COLOR{
		(uint8_t)(Color.r * 255.0),
		(uint8_t)(Color.g * 255.0),
		(uint8_t)(Color.b * 255.0)};
}

void CConsole::Print(int Level, const char *pFrom, const char *pStr, ColorRGBA PrintColor) const
{
	LEVEL LogLevel = IConsole::ToLogLevel(Level);
	// if console colors are not enabled or if the color is pure white, use default terminal color
	if(g_Config.m_ConsoleEnableColors && PrintColor != gs_ConsoleDefaultColor)
	{
		log_log_color(LogLevel, ColorToLogColor(PrintColor), pFrom, "%s", pStr);
	}
	else
	{
		log_log(LogLevel, pFrom, "%s", pStr);
	}
}

void CConsole::SetTeeHistorianCommandCallback(FTeeHistorianCommandCallback pfnCallback, void *pUser)
{
	m_pfnTeeHistorianCommandCallback = pfnCallback;
	m_pTeeHistorianCommandUserdata = pUser;
}

void CConsole::SetUnknownCommandCallback(FUnknownCommandCallback pfnCallback, void *pUser)
{
	m_pfnUnknownCommandCallback = pfnCallback;
	m_pUnknownCommandUserdata = pUser;
}

void CConsole::SetCanUseCommandCallback(FCanUseCommandCallback pfnCallback, void *pUser)
{
	m_pfnCanUseCommandCallback = pfnCallback;
	m_pCanUseCommandUserData = pUser;
}

void CConsole::InitChecksum(CChecksumData *pData) const
{
	pData->m_NumCommands = 0;
	for(CCommand *pCommand = m_pFirstCommand; pCommand; pCommand = pCommand->Next())
	{
		if(pData->m_NumCommands < (int)(std::size(pData->m_aCommandsChecksum)))
		{
			FCommandCallback pfnCallback = pCommand->m_pfnCallback;
			void *pUserData = pCommand->m_pUserData;
			TraverseChain(&pfnCallback, &pUserData);
			int CallbackBits = (uintptr_t)pfnCallback & 0xfff;
			int *pTarget = &pData->m_aCommandsChecksum[pData->m_NumCommands];
			*pTarget = ((uint8_t)pCommand->m_pName[0]) | ((uint8_t)pCommand->m_pName[1] << 8) | (CallbackBits << 16);
		}
		pData->m_NumCommands += 1;
	}
}

bool CConsole::LineIsValid(const char *pStr)
{
	if(!pStr || *pStr == 0)
		return false;

	do
	{
		CResult Result(IConsole::CLIENT_ID_UNSPECIFIED);
		const char *pEnd = pStr;
		const char *pNextPart = nullptr;
		int InString = 0;

		while(*pEnd)
		{
			if(*pEnd == '"')
				InString ^= 1;
			else if(*pEnd == '\\') // escape sequences
			{
				if(pEnd[1] == '"')
					pEnd++;
			}
			else if(!InString)
			{
				if(*pEnd == ';') // command separator
				{
					pNextPart = pEnd + 1;
					break;
				}
				else if(*pEnd == '#') // comment, no need to do anything more
					break;
			}

			pEnd++;
		}

		if(ParseStart(&Result, pStr, (pEnd - pStr) + 1) != 0)
			return false;

		CCommand *pCommand = FindCommand(Result.m_pCommand, m_FlagMask);
		if(!pCommand || ParseArgs(&Result, pCommand->m_pParams))
			return false;

		pStr = pNextPart;
	} while(pStr && *pStr);

	return true;
}

void CConsole::ExecuteLineStroked(int Stroke, const char *pStr, int ClientId, bool InterpretSemicolons)
{
	const char *pWithoutPrefix = str_startswith(pStr, "mc;");
	if(pWithoutPrefix)
	{
		InterpretSemicolons = true;
		pStr = pWithoutPrefix;
	}
	while(pStr && *pStr)
	{
		CResult Result(ClientId);
		const char *pEnd = pStr;
		const char *pNextPart = nullptr;
		int InString = 0;

		while(*pEnd)
		{
			if(*pEnd == '"')
				InString ^= 1;
			else if(*pEnd == '\\') // escape sequences
			{
				if(pEnd[1] == '"')
					pEnd++;
			}
			else if(!InString && InterpretSemicolons)
			{
				if(*pEnd == ';') // command separator
				{
					pNextPart = pEnd + 1;
					break;
				}
				else if(*pEnd == '#') // comment, no need to do anything more
					break;
			}

			pEnd++;
		}

		if(ParseStart(&Result, pStr, (pEnd - pStr) + 1) != 0)
			return;

		if(!*Result.m_pCommand)
		{
			if(pNextPart)
			{
				pStr = pNextPart;
				continue;
			}
			return;
		}

		CCommand *pCommand;
		if(ClientId == IConsole::CLIENT_ID_GAME)
			pCommand = FindCommand(Result.m_pCommand, m_FlagMask | CFGFLAG_GAME);
		else
			pCommand = FindCommand(Result.m_pCommand, m_FlagMask);

		if(pCommand)
		{
			if(ClientId == IConsole::CLIENT_ID_GAME && !(pCommand->m_Flags & CFGFLAG_GAME))
			{
				if(Stroke)
				{
					char aBuf[CMDLINE_LENGTH + 64];
					str_format(aBuf, sizeof(aBuf), "Command '%s' cannot be executed from a map.", Result.m_pCommand);
					Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
				}
			}
			else if(ClientId == IConsole::CLIENT_ID_NO_GAME && pCommand->m_Flags & CFGFLAG_GAME)
			{
				if(Stroke)
				{
					char aBuf[CMDLINE_LENGTH + 64];
					str_format(aBuf, sizeof(aBuf), "Command '%s' cannot be executed from a non-map config file.", Result.m_pCommand);
					Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
					str_format(aBuf, sizeof(aBuf), "Hint: Put the command in '%s.cfg' instead of '%s.map.cfg' ", g_Config.m_SvMap, g_Config.m_SvMap);
					Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
				}
			}
			else if(CanUseCommand(Result.m_ClientId, pCommand))
			{
				int IsStrokeCommand = 0;
				if(Result.m_pCommand[0] == '+')
				{
					// insert the stroke direction token
					Result.AddArgument(m_apStrokeStr[Stroke]);
					IsStrokeCommand = 1;
				}

				if(Stroke || IsStrokeCommand)
				{
					if(int Error = ParseArgs(&Result, pCommand->m_pParams))
					{
						char aBuf[CMDLINE_LENGTH + 64];
						if(Error == PARSEARGS_INVALID_INTEGER)
							str_format(aBuf, sizeof(aBuf), "%s is not a valid integer.", Result.GetString(Result.NumArguments() - 1));
						else if(Error == PARSEARGS_INVALID_COLOR)
							str_format(aBuf, sizeof(aBuf), "%s is not a valid color.", Result.GetString(Result.NumArguments() - 1));
						else if(Error == PARSEARGS_INVALID_FLOAT)
							str_format(aBuf, sizeof(aBuf), "%s is not a valid decimal number.", Result.GetString(Result.NumArguments() - 1));
						else
							str_format(aBuf, sizeof(aBuf), "Invalid arguments. Usage: %s %s", pCommand->m_pName, pCommand->m_pParams);
						Print(OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
					}
					else if(m_StoreCommands && pCommand->m_Flags & CFGFLAG_STORE)
					{
						m_vExecutionQueue.emplace_back(pCommand, Result);
					}
					else
					{
						if(pCommand->m_Flags & CMDFLAG_TEST && !g_Config.m_SvTestingCommands)
						{
							Print(OUTPUT_LEVEL_STANDARD, "console", "Test commands aren't allowed, enable them with 'sv_test_cmds 1' in your initial config.");
							return;
						}

						if(m_pfnTeeHistorianCommandCallback && !(pCommand->m_Flags & CFGFLAG_NONTEEHISTORIC))
						{
							m_pfnTeeHistorianCommandCallback(ClientId, m_FlagMask, pCommand->m_pName, &Result, m_pTeeHistorianCommandUserdata);
						}

						if(Result.GetVictim() == CResult::VICTIM_ME)
							Result.SetVictim(ClientId);

						if(Result.HasVictim() && Result.GetVictim() == CResult::VICTIM_ALL)
						{
							for(int i = 0; i < MAX_CLIENTS; i++)
							{
								Result.SetVictim(i);
								pCommand->m_pfnCallback(&Result, pCommand->m_pUserData);
							}
						}
						else
						{
							pCommand->m_pfnCallback(&Result, pCommand->m_pUserData);
						}

						if(pCommand->m_Flags & CMDFLAG_TEST)
							m_Cheated = true;
					}
				}
			}
			else if(Stroke)
			{
				char aBuf[CMDLINE_LENGTH + 32];
				str_format(aBuf, sizeof(aBuf), "Access for command %s denied.", Result.m_pCommand);
				Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
			}
		}
		else if(Stroke)
		{
			// Pass the original string to the unknown command callback instead of the parsed command, as the latter
			// ends at the first whitespace, which breaks for unknown commands (filenames) containing spaces.
			if(!m_pfnUnknownCommandCallback(pStr, m_pUnknownCommandUserdata))
			{
				char aBuf[CMDLINE_LENGTH + 32];
				if(m_FlagMask & CFGFLAG_CHAT)
					str_format(aBuf, sizeof(aBuf), "No such command: %s. Use /cmdlist for a list of all commands.", Result.m_pCommand);
				else
					str_format(aBuf, sizeof(aBuf), "No such command: %s.", Result.m_pCommand);
				Print(OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
			}
		}

		pStr = pNextPart;
	}
}

bool CConsole::CanUseCommand(int ClientId, const IConsole::ICommandInfo *pCommand) const
{
	// the fallback is needed for the client and rust tests
	if(!m_pfnCanUseCommandCallback)
		return true;
	return m_pfnCanUseCommandCallback(ClientId, pCommand, m_pCanUseCommandUserData);
}

int CConsole::PossibleCommands(const char *pStr, int FlagMask, bool Temp, FPossibleCallback pfnCallback, void *pUser)
{
	int Index = 0;
	for(CCommand *pCommand = m_pFirstCommand; pCommand; pCommand = pCommand->Next())
	{
		if(pCommand->m_Flags & FlagMask && pCommand->m_Temp == Temp)
		{
			if(str_find_nocase(pCommand->m_pName, pStr))
			{
				pfnCallback(Index, pCommand->m_pName, pUser);
				Index++;
			}
		}
	}
	return Index;
}

CConsole::CCommand *CConsole::FindCommand(const char *pName, int FlagMask)
{
	for(CCommand *pCommand = m_pFirstCommand; pCommand; pCommand = pCommand->Next())
	{
		if(pCommand->m_Flags & FlagMask)
		{
			if(str_comp_nocase(pCommand->m_pName, pName) == 0)
				return pCommand;
		}
	}

	return nullptr;
}

void CConsole::ExecuteLine(const char *pStr, int ClientId, bool InterpretSemicolons)
{
	CConsole::ExecuteLineStroked(1, pStr, ClientId, InterpretSemicolons); // press it
	CConsole::ExecuteLineStroked(0, pStr, ClientId, InterpretSemicolons); // then release it
}

void CConsole::ExecuteLineFlag(const char *pStr, int FlagMask, int ClientId, bool InterpretSemicolons)
{
	int Temp = m_FlagMask;
	m_FlagMask = FlagMask;
	ExecuteLine(pStr, ClientId, InterpretSemicolons);
	m_FlagMask = Temp;
}

bool CConsole::ExecuteFile(const char *pFilename, int ClientId, bool LogFailure, int StorageType)
{
	int Count = 0;
	// make sure that this isn't being executed already and that recursion limit isn't met
	for(CExecFile *pCur = m_pFirstExec; pCur; pCur = pCur->m_pPrev)
	{
		Count++;

		if(str_comp(pFilename, pCur->m_pFilename) == 0 || Count > FILE_RECURSION_LIMIT)
			return false;
	}
	if(!m_pStorage)
		return false;

	// push this one to the stack
	CExecFile ThisFile;
	CExecFile *pPrev = m_pFirstExec;
	ThisFile.m_pFilename = pFilename;
	ThisFile.m_pPrev = m_pFirstExec;
	m_pFirstExec = &ThisFile;

	// exec the file
	CLineReader LineReader;
	bool Success = false;
	char aBuf[32 + IO_MAX_PATH_LENGTH];
	if(LineReader.OpenFile(m_pStorage->OpenFile(pFilename, IOFLAG_READ, StorageType)))
	{
		str_format(aBuf, sizeof(aBuf), "executing '%s'", pFilename);
		Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);

		while(const char *pLine = LineReader.Get())
		{
			ExecuteLine(pLine, ClientId);
		}

		Success = true;
	}
	else if(LogFailure)
	{
		str_format(aBuf, sizeof(aBuf), "failed to open '%s'", pFilename);
		Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}

	m_pFirstExec = pPrev;
	return Success;
}

void CConsole::Con_Echo(IResult *pResult, void *pUserData)
{
	((CConsole *)pUserData)->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", pResult->GetString(0));
}

void CConsole::Con_Exec(IResult *pResult, void *pUserData)
{
	((CConsole *)pUserData)->ExecuteFile(pResult->GetString(0), pResult->m_ClientId, true, IStorage::TYPE_ALL);
}

void CConsole::ConCommandAccess(IResult *pResult, void *pUser)
{
	CConsole *pConsole = static_cast<CConsole *>(pUser);
	char aBuf[CMDLINE_LENGTH + 64];
	CCommand *pCommand = pConsole->FindCommand(pResult->GetString(0), CFGFLAG_SERVER);
	if(pCommand)
	{
		if(pResult->NumArguments() == 2)
		{
			std::optional<EAccessLevel> AccessLevel = AccessLevelToEnum(pResult->GetString(1));
			if(!AccessLevel.has_value())
			{
				log_error("console", "Invalid access level '%s'. Allowed values are admin, moderator, helper and all.", pResult->GetString(1));
				return;
			}
			pCommand->SetAccessLevel(AccessLevel.value());
			str_format(aBuf, sizeof(aBuf), "moderator access for '%s' is now %s", pResult->GetString(0), pCommand->GetAccessLevel() >= EAccessLevel::MODERATOR ? "enabled" : "disabled");
			pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
			str_format(aBuf, sizeof(aBuf), "helper access for '%s' is now %s", pResult->GetString(0), pCommand->GetAccessLevel() >= EAccessLevel::HELPER ? "enabled" : "disabled");
			pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
			str_format(aBuf, sizeof(aBuf), "user access for '%s' is now %s", pResult->GetString(0), pCommand->GetAccessLevel() >= EAccessLevel::USER ? "enabled" : "disabled");
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "moderator access for '%s' is %s", pResult->GetString(0), pCommand->GetAccessLevel() >= EAccessLevel::MODERATOR ? "enabled" : "disabled");
			pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
			str_format(aBuf, sizeof(aBuf), "helper access for '%s' is %s", pResult->GetString(0), pCommand->GetAccessLevel() >= EAccessLevel::HELPER ? "enabled" : "disabled");
			pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
			str_format(aBuf, sizeof(aBuf), "user access for '%s' is %s", pResult->GetString(0), pCommand->GetAccessLevel() >= EAccessLevel::USER ? "enabled" : "disabled");
		}
	}
	else
		str_format(aBuf, sizeof(aBuf), "No such command: '%s'.", pResult->GetString(0));

	pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
}

void CConsole::PrintCommandList(EAccessLevel MinAccessLevel, int ExcludeFlagMask)
{
	char aBuf[240] = "";
	int Used = 0;

	for(CCommand *pCommand = m_pFirstCommand; pCommand; pCommand = pCommand->Next())
	{
		if((pCommand->m_Flags & m_FlagMask) &&
			!(pCommand->m_Flags & ExcludeFlagMask) &&
			pCommand->GetAccessLevel() >= MinAccessLevel)
		{
			int Length = str_length(pCommand->m_pName);
			if(Used + Length + 2 < (int)(sizeof(aBuf)))
			{
				if(Used > 0)
				{
					Used += 2;
					str_append(aBuf, ", ");
				}
				str_append(aBuf, pCommand->m_pName);
				Used += Length;
			}
			else
			{
				Print(OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
				str_copy(aBuf, pCommand->m_pName);
				Used = Length;
			}
		}
	}
	if(Used > 0)
		Print(OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
}

void CConsole::ConCommandStatus(IResult *pResult, void *pUser)
{
	CConsole *pConsole = static_cast<CConsole *>(pUser);
	std::optional<EAccessLevel> AccessLevel = AccessLevelToEnum(pResult->GetString(0));
	if(!AccessLevel.has_value())
	{
		log_error("console", "Invalid access level '%s'. Allowed values are admin, moderator, helper and all.", pResult->GetString(0));
		return;
	}
	pConsole->PrintCommandList(AccessLevel.value(), 0);
}

void CConsole::ConUserCommandStatus(IResult *pResult, void *pUser)
{
	CConsole *pConsole = static_cast<CConsole *>(pUser);
	pConsole->PrintCommandList(EAccessLevel::USER, CMDFLAG_PRACTICE);
}

void CConsole::TraverseChain(FCommandCallback *ppfnCallback, void **ppUserData)
{
	while(*ppfnCallback == Con_Chain)
	{
		CChain *pChainInfo = static_cast<CChain *>(*ppUserData);
		*ppfnCallback = pChainInfo->m_pfnCallback;
		*ppUserData = pChainInfo->m_pCallbackUserData;
	}
}

CConsole::CConsole(int FlagMask)
{
	m_FlagMask = FlagMask;
	m_pRecycleList = nullptr;
	m_TempCommands.Reset();
	m_StoreCommands = true;
	m_apStrokeStr[0] = "0";
	m_apStrokeStr[1] = "1";
	m_pFirstCommand = nullptr;
	m_pFirstExec = nullptr;
	m_pfnTeeHistorianCommandCallback = nullptr;
	m_pTeeHistorianCommandUserdata = nullptr;

	m_pStorage = nullptr;

	// register some basic commands
	Register("echo", "r[text]", CFGFLAG_SERVER, Con_Echo, this, "Echo the text");
	Register("exec", "r[file]", CFGFLAG_SERVER | CFGFLAG_CLIENT, Con_Exec, this, "Execute the specified file");

	Register("access_level", "s[command] ?s['admin'|'moderator'|'helper'|'all']", CFGFLAG_SERVER, ConCommandAccess, this, "Specify command accessibility for given access level");
	Register("access_status", "s['admin'|'moderator'|'helper'|'all']", CFGFLAG_SERVER, ConCommandStatus, this, "List all commands which are accessible for given access level");
	Register("cmdlist", "", CFGFLAG_SERVER | CFGFLAG_CHAT, ConUserCommandStatus, this, "List all commands which are accessible for users");

	// DDRace

	m_Cheated = false;
}

CConsole::~CConsole()
{
	CCommand *pCommand = m_pFirstCommand;
	while(pCommand)
	{
		CCommand *pNext = pCommand->Next();
		{
			FCommandCallback pfnCallback = pCommand->m_pfnCallback;
			void *pUserData = pCommand->m_pUserData;
			CChain *pChain = nullptr;
			while(pfnCallback == Con_Chain)
			{
				pChain = static_cast<CChain *>(pUserData);
				pfnCallback = pChain->m_pfnCallback;
				pUserData = pChain->m_pCallbackUserData;
				delete pChain;
			}
		}
		// Temp commands are on m_TempCommands heap, so don't delete them
		if(!pCommand->m_Temp)
			delete pCommand;
		pCommand = pNext;
	}
}

void CConsole::Init()
{
	m_pStorage = Kernel()->RequestInterface<IStorage>();
}

void CConsole::ParseArguments(int NumArgs, const char **ppArguments)
{
	for(int i = 0; i < NumArgs; i++)
	{
		// check for scripts to execute
		if(ppArguments[i][0] == '-' && ppArguments[i][1] == 'f' && ppArguments[i][2] == 0)
		{
			if(NumArgs - i > 1)
				ExecuteFile(ppArguments[i + 1], IConsole::CLIENT_ID_UNSPECIFIED, true, IStorage::TYPE_ABSOLUTE);
			i++;
		}
		else if(!str_comp("-s", ppArguments[i]) || !str_comp("--silent", ppArguments[i]))
		{
			// skip silent param
			continue;
		}
		else
		{
			// search arguments for overrides
			ExecuteLine(ppArguments[i]);
		}
	}
}

void CConsole::AddCommandSorted(CCommand *pCommand)
{
	if(!m_pFirstCommand || str_comp(pCommand->m_pName, m_pFirstCommand->m_pName) <= 0)
	{
		if(m_pFirstCommand && m_pFirstCommand->Next())
			pCommand->SetNext(m_pFirstCommand);
		else
			pCommand->SetNext(nullptr);
		m_pFirstCommand = pCommand;
	}
	else
	{
		for(CCommand *p = m_pFirstCommand; p; p = p->Next())
		{
			if(!p->Next() || str_comp(pCommand->m_pName, p->Next()->m_pName) <= 0)
			{
				pCommand->SetNext(p->Next());
				p->SetNext(pCommand);
				break;
			}
		}
	}
}

void CConsole::Register(const char *pName, const char *pParams,
	int Flags, FCommandCallback pfnFunc, void *pUser, const char *pHelp)
{
	CCommand *pCommand = FindCommand(pName, Flags);
	bool DoAdd = false;
	if(pCommand == nullptr)
	{
		pCommand = new CCommand();
		DoAdd = true;
	}
	pCommand->m_pfnCallback = pfnFunc;
	pCommand->m_pUserData = pUser;

	pCommand->m_pName = pName;
	pCommand->m_pHelp = pHelp;
	pCommand->m_pParams = pParams;

	pCommand->m_Flags = Flags;
	pCommand->m_Temp = false;

	if(DoAdd)
		AddCommandSorted(pCommand);

	if(pCommand->m_Flags & CFGFLAG_CHAT)
		pCommand->SetAccessLevel(EAccessLevel::USER);
}

void CConsole::RegisterTemp(const char *pName, const char *pParams, int Flags, const char *pHelp)
{
	CCommand *pCommand;
	if(m_pRecycleList)
	{
		pCommand = m_pRecycleList;
		str_copy(const_cast<char *>(pCommand->m_pName), pName, TEMPCMD_NAME_LENGTH);
		str_copy(const_cast<char *>(pCommand->m_pHelp), pHelp, TEMPCMD_HELP_LENGTH);
		str_copy(const_cast<char *>(pCommand->m_pParams), pParams, TEMPCMD_PARAMS_LENGTH);

		m_pRecycleList = m_pRecycleList->Next();
	}
	else
	{
		pCommand = new(m_TempCommands.Allocate(sizeof(CCommand))) CCommand;
		char *pMem = static_cast<char *>(m_TempCommands.Allocate(TEMPCMD_NAME_LENGTH));
		str_copy(pMem, pName, TEMPCMD_NAME_LENGTH);
		pCommand->m_pName = pMem;
		pMem = static_cast<char *>(m_TempCommands.Allocate(TEMPCMD_HELP_LENGTH));
		str_copy(pMem, pHelp, TEMPCMD_HELP_LENGTH);
		pCommand->m_pHelp = pMem;
		pMem = static_cast<char *>(m_TempCommands.Allocate(TEMPCMD_PARAMS_LENGTH));
		str_copy(pMem, pParams, TEMPCMD_PARAMS_LENGTH);
		pCommand->m_pParams = pMem;
	}

	pCommand->m_pfnCallback = nullptr;
	pCommand->m_pUserData = nullptr;
	pCommand->m_Flags = Flags;
	pCommand->m_Temp = true;

	AddCommandSorted(pCommand);
}

void CConsole::DeregisterTemp(const char *pName)
{
	if(!m_pFirstCommand)
		return;

	CCommand *pRemoved = nullptr;

	// remove temp entry from command list
	if(m_pFirstCommand->m_Temp && str_comp(m_pFirstCommand->m_pName, pName) == 0)
	{
		pRemoved = m_pFirstCommand;
		m_pFirstCommand = m_pFirstCommand->Next();
	}
	else
	{
		for(CCommand *pCommand = m_pFirstCommand; pCommand->Next(); pCommand = pCommand->Next())
			if(pCommand->Next()->m_Temp && str_comp(pCommand->Next()->m_pName, pName) == 0)
			{
				pRemoved = pCommand->Next();
				pCommand->SetNext(pCommand->Next()->Next());
				break;
			}
	}

	// add to recycle list
	if(pRemoved)
	{
		pRemoved->SetNext(m_pRecycleList);
		m_pRecycleList = pRemoved;
	}
}

void CConsole::DeregisterTempAll()
{
	// set non temp as first one
	for(; m_pFirstCommand && m_pFirstCommand->m_Temp; m_pFirstCommand = m_pFirstCommand->Next())
		;

	// remove temp entries from command list
	for(CCommand *pCommand = m_pFirstCommand; pCommand && pCommand->Next(); pCommand = pCommand->Next())
	{
		CCommand *pNext = pCommand->Next();
		if(pNext->m_Temp)
		{
			for(; pNext && pNext->m_Temp; pNext = pNext->Next())
				;
			pCommand->SetNext(pNext);
		}
	}

	m_TempCommands.Reset();
	m_pRecycleList = nullptr;
}

void CConsole::Con_Chain(IResult *pResult, void *pUserData)
{
	CChain *pInfo = (CChain *)pUserData;
	pInfo->m_pfnChainCallback(pResult, pInfo->m_pUserData, pInfo->m_pfnCallback, pInfo->m_pCallbackUserData);
}

void CConsole::Chain(const char *pName, FChainCommandCallback pfnChainFunc, void *pUser)
{
	CCommand *pCommand = FindCommand(pName, m_FlagMask);

	if(!pCommand)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "failed to chain '%s'", pName);
		Print(IConsole::OUTPUT_LEVEL_DEBUG, "console", aBuf);
		return;
	}

	CChain *pChainInfo = new CChain();

	// store info
	pChainInfo->m_pfnChainCallback = pfnChainFunc;
	pChainInfo->m_pUserData = pUser;
	pChainInfo->m_pfnCallback = pCommand->m_pfnCallback;
	pChainInfo->m_pCallbackUserData = pCommand->m_pUserData;

	// chain
	pCommand->m_pfnCallback = Con_Chain;
	pCommand->m_pUserData = pChainInfo;
}

void CConsole::StoreCommands(bool Store)
{
	if(!Store)
	{
		for(CExecutionQueueEntry &Entry : m_vExecutionQueue)
		{
			Entry.m_pCommand->m_pfnCallback(&Entry.m_Result, Entry.m_pCommand->m_pUserData);
		}
		m_vExecutionQueue.clear();
	}
	m_StoreCommands = Store;
}

const IConsole::ICommandInfo *CConsole::GetCommandInfo(const char *pName, int FlagMask, bool Temp)
{
	for(CCommand *pCommand = m_pFirstCommand; pCommand; pCommand = pCommand->Next())
	{
		if(pCommand->m_Flags & FlagMask && pCommand->m_Temp == Temp)
		{
			if(str_comp_nocase(pCommand->Name(), pName) == 0)
				return pCommand;
		}
	}

	return nullptr;
}

std::unique_ptr<IConsole> CreateConsole(int FlagMask) { return std::make_unique<CConsole>(FlagMask); }

int CConsole::CResult::GetVictim() const
{
	return m_Victim;
}

void CConsole::CResult::ResetVictim()
{
	m_Victim = VICTIM_NONE;
}

bool CConsole::CResult::HasVictim() const
{
	return m_Victim != VICTIM_NONE;
}

void CConsole::CResult::SetVictim(int Victim)
{
	m_Victim = std::clamp<int>(Victim, VICTIM_NONE, MAX_CLIENTS - 1);
}

void CConsole::CResult::SetVictim(const char *pVictim)
{
	if(!str_comp(pVictim, "me"))
		m_Victim = VICTIM_ME;
	else if(!str_comp(pVictim, "all"))
		m_Victim = VICTIM_ALL;
	else
		m_Victim = std::clamp<int>(str_toint(pVictim), 0, MAX_CLIENTS - 1);
}

std::optional<ColorHSLA> CConsole::ColorParse(const char *pStr, float DarkestLighting)
{
	if(str_isallnum(pStr) || ((pStr[0] == '-' || pStr[0] == '+') && str_isallnum(pStr + 1))) // Teeworlds Color (Packed HSL)
	{
		unsigned long Value = str_toulong_base(pStr, 10);
		if(Value == std::numeric_limits<unsigned long>::max())
			return std::nullopt;
		return ColorHSLA(Value, true).UnclampLighting(DarkestLighting);
	}
	else if(*pStr == '$') // Hex RGB/RGBA
	{
		auto ParsedColor = color_parse<ColorRGBA>(pStr + 1);
		if(ParsedColor)
			return color_cast<ColorHSLA>(ParsedColor.value());
		else
			return std::nullopt;
	}
	else if(!str_comp_nocase(pStr, "red"))
		return ColorHSLA(0.0f / 6.0f, 1.0f, 0.5f);
	else if(!str_comp_nocase(pStr, "yellow"))
		return ColorHSLA(1.0f / 6.0f, 1.0f, 0.5f);
	else if(!str_comp_nocase(pStr, "green"))
		return ColorHSLA(2.0f / 6.0f, 1.0f, 0.5f);
	else if(!str_comp_nocase(pStr, "cyan"))
		return ColorHSLA(3.0f / 6.0f, 1.0f, 0.5f);
	else if(!str_comp_nocase(pStr, "blue"))
		return ColorHSLA(4.0f / 6.0f, 1.0f, 0.5f);
	else if(!str_comp_nocase(pStr, "magenta"))
		return ColorHSLA(5.0f / 6.0f, 1.0f, 0.5f);
	else if(!str_comp_nocase(pStr, "white"))
		return ColorHSLA(0.0f, 0.0f, 1.0f);
	else if(!str_comp_nocase(pStr, "gray"))
		return ColorHSLA(0.0f, 0.0f, 0.5f);
	else if(!str_comp_nocase(pStr, "black"))
		return ColorHSLA(0.0f, 0.0f, 0.0f);

	return std::nullopt;
}
