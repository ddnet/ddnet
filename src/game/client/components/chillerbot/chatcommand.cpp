// ChillerDragon 2023 - chillerbot ux

#include <game/client/components/chillerbot/chillerbotux.h>
#include <game/client/gameclient.h>

#include <cinttypes>
#include <engine/shared/protocol.h>

#include <game/client/components/chat.h>

#include <base/system.h>

#include "chatcommand.h"

void CChatCommand::OnServerMsg(const char *pMsg)
{
}

bool CChatCommand::OnChatMsg(int ClientId, int Team, const char *pMsg)
{
	if(!pMsg[1])
		return false;
	if(pMsg[0] == '.' || pMsg[0] == ':' || pMsg[0] == '!' || pMsg[0] == '#' || pMsg[0] == '$')
		if(ParseChatCmd(pMsg[0], ClientId, Team, pMsg + 1))
			return true;

	OnNoChatCommandMatches(ClientId, Team, pMsg);
	return false;
}

void CChatCommand::OnNoChatCommandMatches(int ClientId, int Team, const char *pMsg)
{
	// ux components

	// m_pClient->m_WarList.OnNoChatCommandMatches(ClientId, Team, pMsg);

	// zx components
}

bool CChatCommand::OnChatCmd(char Prefix, int ClientId, int Team, const char *pCmd, int NumArgs, const char **ppArgs, const char *pRawArgLine)
{
	bool match = false;
	// ux components

	if(m_pClient->m_WarList.OnChatCmd(Prefix, ClientId, Team, pCmd, NumArgs, ppArgs, pRawArgLine))
		match = true;

	// zx components

	return match;
}

bool CChatCommand::ParseChatCmd(char Prefix, int ClientId, int Team, const char *pCmdWithArgs)
{
	char aRawArgLine[512];
	str_copy(aRawArgLine, pCmdWithArgs);
	const int MaxArgLen = 256;
	char aCmd[MaxArgLen];
	int i;
	for(i = 0; pCmdWithArgs[i] && i < MaxArgLen; i++)
	{
		if(pCmdWithArgs[i] == ' ')
			break;
		aCmd[i] = pCmdWithArgs[i];
	}

	int Skip = i;
	while(pCmdWithArgs[Skip] && str_isspace(pCmdWithArgs[Skip]))
		Skip++;
	str_copy(aRawArgLine, pCmdWithArgs + Skip);

	aCmd[i] = '\0';
	int ROffset = m_pClient->m_ChatHelper.ChatCommandGetROffset(aCmd);

	// max 8 args of 128 len each
	const int MaxArgs = 16;
	char **ppArgs = new char *[MaxArgs];
	for(int x = 0; x < MaxArgs; ++x)
	{
		ppArgs[x] = new char[MaxArgLen];
		ppArgs[x][0] = '\0';
	}
	int NumArgs = 0;
	int k = 0;
	while(pCmdWithArgs[i])
	{
		if(k + 1 >= MaxArgLen)
		{
			dbg_msg("chillerbot-ux", "ERROR: chat command has too long arg");
			break;
		}
		if(NumArgs + 1 >= MaxArgs)
		{
			dbg_msg("chillerbot-ux", "ERROR: chat command has too many args");
			break;
		}
		if(pCmdWithArgs[i] == ' ')
		{
			// do not delimit on space
			// if we reached the r parameter
			if(NumArgs == ROffset)
			{
				// strip spaces from the beggining
				// add spaces in the middle and end
				if(ppArgs[NumArgs][0])
				{
					ppArgs[NumArgs][k] = pCmdWithArgs[i];
					k++;
					i++;
					continue;
				}
			}
			else if(ppArgs[NumArgs][0])
			{
				ppArgs[NumArgs][k] = '\0';
				k = 0;
				NumArgs++;
			}
			i++;
			continue;
		}
		ppArgs[NumArgs][k] = pCmdWithArgs[i];
		k++;
		i++;
	}
	if(ppArgs[NumArgs][0])
	{
		ppArgs[NumArgs][k] = '\0';
		NumArgs++;
	}

	// char aArgsStr[128];
	// aArgsStr[0] = '\0';
	// for(i = 0; i < NumArgs; i++)
	// {
	// 	if(aArgsStr[0] != '\0')
	// 		str_append(aArgsStr, ", ", sizeof(aArgsStr));
	// 	str_append(aArgsStr, ppArgs[i], sizeof(aArgsStr));
	// }

	// char aBuf[512];
	// str_format(aBuf, sizeof(aBuf), "got cmd '%s' with %d args: %s", aCmd, NumArgs, aArgsStr);
	// Say(aBuf);
	bool match = OnChatCmd(Prefix, ClientId, Team, aCmd, NumArgs, (const char **)ppArgs, aRawArgLine);
	for(int x = 0; x < 8; ++x)
		delete[] ppArgs[x];
	delete[] ppArgs;
	return match;
}

void CChatCommand::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		int ClientId = pMsg->m_ClientId;
		if(ClientId == -1 && pMsg->m_Team < 2)
			{
			OnServerMsg(pMsg->m_pMessage);
			}
		else if (ClientId != m_pClient->m_aLocalIds[0] && (!m_pClient->Client()->DummyConnected() || ClientId != m_pClient->m_aLocalIds[1]))
		{
			OnChatMsg(ClientId, pMsg->m_Team, pMsg->m_pMessage);
		}
	}
}
