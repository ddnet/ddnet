/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/color.h>
#include <base/log.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/client/checksum.h>
#include <engine/shared/protocol.h>
#include <engine/storage.h>

#include "config.h"
#include "console.h"
#include "linereader.h"

#include <iterator> // std::size
#include <new>

// todo: rework this

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
	return str_toint(m_apArgs[Index]);
}

float CConsole::CResult::GetFloat(unsigned Index) const
{
	if(Index >= m_NumArgs)
		return 0.0f;
	return str_tofloat(m_apArgs[Index]);
}

std::optional<ColorHSLA> CConsole::CResult::GetColor(unsigned Index, float DarkestLighting) const
{
	if(Index >= m_NumArgs)
		return std::nullopt;

	const char *pStr = m_apArgs[Index];
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
		return ColorHSLA(0.0f / 6.0f, 1, .5f);
	else if(!str_comp_nocase(pStr, "yellow"))
		return ColorHSLA(1.0f / 6.0f, 1, .5f);
	else if(!str_comp_nocase(pStr, "green"))
		return ColorHSLA(2.0f / 6.0f, 1, .5f);
	else if(!str_comp_nocase(pStr, "cyan"))
		return ColorHSLA(3.0f / 6.0f, 1, .5f);
	else if(!str_comp_nocase(pStr, "blue"))
		return ColorHSLA(4.0f / 6.0f, 1, .5f);
	else if(!str_comp_nocase(pStr, "magenta"))
		return ColorHSLA(5.0f / 6.0f, 1, .5f);
	else if(!str_comp_nocase(pStr, "white"))
		return ColorHSLA(0, 0, 1);
	else if(!str_comp_nocase(pStr, "gray"))
		return ColorHSLA(0, 0, .5f);
	else if(!str_comp_nocase(pStr, "black"))
		return ColorHSLA(0, 0, 0);

	return std::nullopt;
}

const IConsole::CCommandInfo *CConsole::CCommand::NextCommandInfo(int AccessLevel, int FlagMask) const
{
	const CCommand *pInfo = m_pNext;
	while(pInfo)
	{
		if(pInfo->m_Flags & FlagMask && pInfo->m_AccessLevel >= AccessLevel)
			break;
		pInfo = pInfo->m_pNext;
	}
	return pInfo;
}

const IConsole::CCommandInfo *CConsole::FirstCommandInfo(int AccessLevel, int FlagMask) const
{
	for(const CCommand *pCommand = m_pFirstCommand; pCommand; pCommand = pCommand->m_pNext)
	{
		if(pCommand->m_Flags & FlagMask && pCommand->GetAccessLevel() >= AccessLevel)
			return pCommand;
	}

	return 0;
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

int CConsole::ParseArgs(CResult *pResult, const char *pFormat, bool IsColor)
{
	char Command = *pFormat;
	char *pStr;
	int Optional = 0;
	int Error = PARSEARGS_OK;

	pResult->ResetVictim();

	pStr = pResult->m_pArgsStart;

	while(true)
	{
		if(!Command)
			break;

		if(Command == '?')
			Optional = 1;
		else
		{
			pStr = str_skip_whitespaces(pStr);

			if(!(*pStr)) // error, non optional command needs value
			{
				if(!Optional)
				{
					Error = PARSEARGS_MISSING_VALUE;
					break;
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
				break;
			}

			// add token
			if(*pStr == '"')
			{
				char *pDst;
				pStr++;
				pResult->AddArgument(pStr);

				pDst = pStr; // we might have to process escape data
				while(true)
				{
					if(pStr[0] == '"')
						break;
					else if(pStr[0] == '\\')
					{
						if(pStr[1] == '\\')
							pStr++; // skip due to escape
						else if(pStr[1] == '"')
							pStr++; // skip due to escape
					}
					else if(pStr[0] == 0)
						return PARSEARGS_MISSING_VALUE; // return error

					*pDst = *pStr;
					pDst++;
					pStr++;
				}

				// write null termination
				*pDst = 0;

				pStr++;
			}
			else
			{
				char *pVictim = 0;

				pResult->AddArgument(pStr);
				if(Command == 'v')
				{
					pVictim = pStr;
				}

				if(Command == 'r') // rest of the string
					break;
				else if(Command == 'v' || Command == 'i' || Command == 'f' || Command == 's')
					pStr = str_skip_to_whitespace(pStr);

				if(pStr[0] != 0) // check for end of string
				{
					pStr[0] = 0;
					pStr++;
				}

				// validate args
				if(Command == 'i')
				{
					// don't validate colors here
					if(!IsColor)
					{
						int Value;
						if(!str_toint(pResult->GetString(pResult->NumArguments() - 1), &Value) ||
							Value == std::numeric_limits<int>::max() || Value == std::numeric_limits<int>::min())
						{
							Error = PARSEARGS_INVALID_INTEGER;
							break;
						}
					}
				}
				else if(Command == 'f')
				{
					float Value;
					if(!str_tofloat(pResult->GetString(pResult->NumArguments() - 1), &Value) ||
						Value == std::numeric_limits<float>::max() || Value == std::numeric_limits<float>::min())
					{
						Error = PARSEARGS_INVALID_FLOAT;
						break;
					}
				}

				if(pVictim)
				{
					pResult->SetVictim(pVictim);
				}
			}
		}
		// fetch next command
		Command = NextParam(pFormat);
	}

	return Error;
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

LOG_COLOR ColorToLogColor(ColorRGBA Color)
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

void CConsole::InitChecksum(CChecksumData *pData) const
{
	pData->m_NumCommands = 0;
	for(CCommand *pCommand = m_pFirstCommand; pCommand; pCommand = pCommand->m_pNext)
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
		CResult Result(-1);
		const char *pEnd = pStr;
		const char *pNextPart = 0;
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
		const char *pNextPart = 0;
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
			return;

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
			else if(pCommand->GetAccessLevel() >= m_AccessLevel)
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
					bool IsColor = false;
					{
						FCommandCallback pfnCallback = pCommand->m_pfnCallback;
						void *pUserData = pCommand->m_pUserData;
						TraverseChain(&pfnCallback, &pUserData);
						IsColor = pfnCallback == &SColorConfigVariable::CommandCallback;
					}

					if(int Error = ParseArgs(&Result, pCommand->m_pParams, IsColor))
					{
						char aBuf[CMDLINE_LENGTH + 64];
						if(Error == PARSEARGS_INVALID_INTEGER)
							str_format(aBuf, sizeof(aBuf), "%s is not a valid integer.", Result.GetString(Result.NumArguments() - 1));
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

int CConsole::PossibleCommands(const char *pStr, int FlagMask, bool Temp, FPossibleCallback pfnCallback, void *pUser)
{
	int Index = 0;
	for(CCommand *pCommand = m_pFirstCommand; pCommand; pCommand = pCommand->m_pNext)
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
	for(CCommand *pCommand = m_pFirstCommand; pCommand; pCommand = pCommand->m_pNext)
	{
		if(pCommand->m_Flags & FlagMask)
		{
			if(str_comp_nocase(pCommand->m_pName, pName) == 0)
				return pCommand;
		}
	}

	return 0x0;
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
	// make sure that this isn't being executed already
	for(CExecFile *pCur = m_pFirstExec; pCur; pCur = pCur->m_pPrev)
		if(str_comp(pFilename, pCur->m_pFilename) == 0)
			return false;

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
	((CConsole *)pUserData)->ExecuteFile(pResult->GetString(0), -1, true, IStorage::TYPE_ALL);
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
			pCommand->SetAccessLevel(pResult->GetInteger(1));
			str_format(aBuf, sizeof(aBuf), "moderator access for '%s' is now %s", pResult->GetString(0), pCommand->GetAccessLevel() ? "enabled" : "disabled");
			pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
			str_format(aBuf, sizeof(aBuf), "helper access for '%s' is now %s", pResult->GetString(0), pCommand->GetAccessLevel() >= ACCESS_LEVEL_HELPER ? "enabled" : "disabled");
			pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
			str_format(aBuf, sizeof(aBuf), "user access for '%s' is now %s", pResult->GetString(0), pCommand->GetAccessLevel() >= ACCESS_LEVEL_USER ? "enabled" : "disabled");
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "moderator access for '%s' is %s", pResult->GetString(0), pCommand->GetAccessLevel() ? "enabled" : "disabled");
			pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
			str_format(aBuf, sizeof(aBuf), "helper access for '%s' is %s", pResult->GetString(0), pCommand->GetAccessLevel() >= ACCESS_LEVEL_HELPER ? "enabled" : "disabled");
			pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
			str_format(aBuf, sizeof(aBuf), "user access for '%s' is %s", pResult->GetString(0), pCommand->GetAccessLevel() >= ACCESS_LEVEL_USER ? "enabled" : "disabled");
		}
	}
	else
		str_format(aBuf, sizeof(aBuf), "No such command: '%s'.", pResult->GetString(0));

	pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
}

void CConsole::ConCommandStatus(IResult *pResult, void *pUser)
{
	CConsole *pConsole = static_cast<CConsole *>(pUser);
	char aBuf[240];
	mem_zero(aBuf, sizeof(aBuf));
	int Used = 0;

	for(CCommand *pCommand = pConsole->m_pFirstCommand; pCommand; pCommand = pCommand->m_pNext)
	{
		if(pCommand->m_Flags & pConsole->m_FlagMask && pCommand->GetAccessLevel() >= clamp(pResult->GetInteger(0), (int)ACCESS_LEVEL_ADMIN, (int)ACCESS_LEVEL_USER))
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
				pConsole->Print(OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
				mem_zero(aBuf, sizeof(aBuf));
				str_copy(aBuf, pCommand->m_pName);
				Used = Length;
			}
		}
	}
	if(Used > 0)
		pConsole->Print(OUTPUT_LEVEL_STANDARD, "chatresp", aBuf);
}

void CConsole::ConUserCommandStatus(IResult *pResult, void *pUser)
{
	CConsole *pConsole = static_cast<CConsole *>(pUser);
	CResult Result(pResult->m_ClientId);
	Result.m_pCommand = "access_status";
	char aBuf[4];
	str_format(aBuf, sizeof(aBuf), "%d", (int)IConsole::ACCESS_LEVEL_USER);
	Result.AddArgument(aBuf);

	CConsole::ConCommandStatus(&Result, pConsole);
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
	m_AccessLevel = ACCESS_LEVEL_ADMIN;
	m_pRecycleList = 0;
	m_TempCommands.Reset();
	m_StoreCommands = true;
	m_apStrokeStr[0] = "0";
	m_apStrokeStr[1] = "1";
	m_pFirstCommand = 0;
	m_pFirstExec = 0;
	m_pfnTeeHistorianCommandCallback = 0;
	m_pTeeHistorianCommandUserdata = 0;

	m_pStorage = 0;

	// register some basic commands
	Register("echo", "r[text]", CFGFLAG_SERVER, Con_Echo, this, "Echo the text");
	Register("exec", "r[file]", CFGFLAG_SERVER | CFGFLAG_CLIENT, Con_Exec, this, "Execute the specified file");

	Register("access_level", "s[command] ?i[accesslevel]", CFGFLAG_SERVER, ConCommandAccess, this, "Specify command accessibility (admin = 0, moderator = 1, helper = 2, all = 3)");
	Register("access_status", "i[accesslevel]", CFGFLAG_SERVER, ConCommandStatus, this, "List all commands which are accessible for admin = 0, moderator = 1, helper = 2, all = 3");
	Register("cmdlist", "", CFGFLAG_SERVER | CFGFLAG_CHAT, ConUserCommandStatus, this, "List all commands which are accessible for users");

	// DDRace

	m_Cheated = false;
}

CConsole::~CConsole()
{
	CCommand *pCommand = m_pFirstCommand;
	while(pCommand)
	{
		CCommand *pNext = pCommand->m_pNext;
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
				ExecuteFile(ppArguments[i + 1], -1, true, IStorage::TYPE_ABSOLUTE);
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
		if(m_pFirstCommand && m_pFirstCommand->m_pNext)
			pCommand->m_pNext = m_pFirstCommand;
		else
			pCommand->m_pNext = 0;
		m_pFirstCommand = pCommand;
	}
	else
	{
		for(CCommand *p = m_pFirstCommand; p; p = p->m_pNext)
		{
			if(!p->m_pNext || str_comp(pCommand->m_pName, p->m_pNext->m_pName) <= 0)
			{
				pCommand->m_pNext = p->m_pNext;
				p->m_pNext = pCommand;
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
	if(pCommand == 0)
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
		pCommand->SetAccessLevel(ACCESS_LEVEL_USER);
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

		m_pRecycleList = m_pRecycleList->m_pNext;
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

	pCommand->m_pfnCallback = 0;
	pCommand->m_pUserData = 0;
	pCommand->m_Flags = Flags;
	pCommand->m_Temp = true;

	AddCommandSorted(pCommand);
}

void CConsole::DeregisterTemp(const char *pName)
{
	if(!m_pFirstCommand)
		return;

	CCommand *pRemoved = 0;

	// remove temp entry from command list
	if(m_pFirstCommand->m_Temp && str_comp(m_pFirstCommand->m_pName, pName) == 0)
	{
		pRemoved = m_pFirstCommand;
		m_pFirstCommand = m_pFirstCommand->m_pNext;
	}
	else
	{
		for(CCommand *pCommand = m_pFirstCommand; pCommand->m_pNext; pCommand = pCommand->m_pNext)
			if(pCommand->m_pNext->m_Temp && str_comp(pCommand->m_pNext->m_pName, pName) == 0)
			{
				pRemoved = pCommand->m_pNext;
				pCommand->m_pNext = pCommand->m_pNext->m_pNext;
				break;
			}
	}

	// add to recycle list
	if(pRemoved)
	{
		pRemoved->m_pNext = m_pRecycleList;
		m_pRecycleList = pRemoved;
	}
}

void CConsole::DeregisterTempAll()
{
	// set non temp as first one
	for(; m_pFirstCommand && m_pFirstCommand->m_Temp; m_pFirstCommand = m_pFirstCommand->m_pNext)
		;

	// remove temp entries from command list
	for(CCommand *pCommand = m_pFirstCommand; pCommand && pCommand->m_pNext; pCommand = pCommand->m_pNext)
	{
		CCommand *pNext = pCommand->m_pNext;
		if(pNext->m_Temp)
		{
			for(; pNext && pNext->m_Temp; pNext = pNext->m_pNext)
				;
			pCommand->m_pNext = pNext;
		}
	}

	m_TempCommands.Reset();
	m_pRecycleList = 0;
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

const IConsole::CCommandInfo *CConsole::GetCommandInfo(const char *pName, int FlagMask, bool Temp)
{
	for(CCommand *pCommand = m_pFirstCommand; pCommand; pCommand = pCommand->m_pNext)
	{
		if(pCommand->m_Flags & FlagMask && pCommand->m_Temp == Temp)
		{
			if(str_comp_nocase(pCommand->m_pName, pName) == 0)
				return pCommand;
		}
	}

	return 0;
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
	m_Victim = clamp<int>(Victim, VICTIM_NONE, MAX_CLIENTS - 1);
}

void CConsole::CResult::SetVictim(const char *pVictim)
{
	if(!str_comp(pVictim, "me"))
		m_Victim = VICTIM_ME;
	else if(!str_comp(pVictim, "all"))
		m_Victim = VICTIM_ALL;
	else
		m_Victim = clamp<int>(str_toint(pVictim), 0, MAX_CLIENTS - 1);
}
