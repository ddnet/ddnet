#include <base/system.h>
#include <engine/shared/protocol7.h>
#include <game/gamecore.h>
#include <game/generated/protocol7.h>
#include <game/localization.h>

#include <game/client/gameclient.h>

enum
{
	STR_TEAM_GAME,
	STR_TEAM_RED,
	STR_TEAM_BLUE,
	STR_TEAM_SPECTATORS,
};

static int GetStrTeam7(int Team, bool Teamplay)
{
	if(Teamplay)
	{
		if(Team == TEAM_RED)
			return STR_TEAM_RED;
		else if(Team == TEAM_BLUE)
			return STR_TEAM_BLUE;
	}
	else if(Team == 0)
		return STR_TEAM_GAME;

	return STR_TEAM_SPECTATORS;
}

enum
{
	DO_CHAT = 0,
	DO_BROADCAST,
	DO_SPECIAL,

	PARA_NONE = 0,
	PARA_I,
	PARA_II,
	PARA_III,
};

struct CGameMsg7
{
	int m_Action;
	int m_ParaType;
	const char *m_pText;
};

static CGameMsg7 gs_GameMsgList7[protocol7::NUM_GAMEMSGS] = {
	{/*GAMEMSG_TEAM_SWAP*/ DO_CHAT, PARA_NONE, "Teams were swapped"},
	{/*GAMEMSG_SPEC_INVALIDID*/ DO_CHAT, PARA_NONE, "Invalid spectator id used"}, //!
	{/*GAMEMSG_TEAM_SHUFFLE*/ DO_CHAT, PARA_NONE, "Teams were shuffled"},
	{/*GAMEMSG_TEAM_BALANCE*/ DO_CHAT, PARA_NONE, "Teams have been balanced"},
	{/*GAMEMSG_CTF_DROP*/ DO_SPECIAL, PARA_NONE, ""}, // special - play ctf drop sound
	{/*GAMEMSG_CTF_RETURN*/ DO_SPECIAL, PARA_NONE, ""}, // special - play ctf return sound

	{/*GAMEMSG_TEAM_ALL*/ DO_SPECIAL, PARA_I, ""}, // special - add team name
	{/*GAMEMSG_TEAM_BALANCE_VICTIM*/ DO_SPECIAL, PARA_I, ""}, // special - add team name
	{/*GAMEMSG_CTF_GRAB*/ DO_SPECIAL, PARA_I, ""}, // special - play ctf grab sound based on team

	{/*GAMEMSG_CTF_CAPTURE*/ DO_SPECIAL, PARA_III, ""}, // special - play ctf capture sound + capture chat message

	{/*GAMEMSG_GAME_PAUSED*/ DO_SPECIAL, PARA_I, ""}, // special - add player name
};

void CGameClient::DoTeamChangeMessage7(const char *pName, int ClientId, int Team, const char *pPrefix)
{
	char aBuf[128];
	switch(GetStrTeam7(Team, m_pClient->m_TranslationContext.m_GameFlags & protocol7::GAMEFLAG_TEAMS))
	{
	case STR_TEAM_GAME: str_format(aBuf, sizeof(aBuf), "'%s' %sjoined the game", pName, pPrefix); break;
	case STR_TEAM_RED: str_format(aBuf, sizeof(aBuf), "'%s' %sjoined the red team", pName, pPrefix); break;
	case STR_TEAM_BLUE: str_format(aBuf, sizeof(aBuf), "'%s' %sjoined the blue team", pName, pPrefix); break;
	case STR_TEAM_SPECTATORS: str_format(aBuf, sizeof(aBuf), "'%s' %sjoined the spectators", pName, pPrefix); break;
	}
	m_Chat.AddLine(-1, 0, aBuf);
}

template<typename T>
void CGameClient::ApplySkin7InfoFromGameMsg(const T *pMsg, int ClientId, int Conn)
{
	CClientData *pClient = &m_aClients[ClientId];
	char *apSkinPartsPtr[protocol7::NUM_SKINPARTS];
	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		str_utf8_copy_num(pClient->m_aSixup[Conn].m_aaSkinPartNames[Part], pMsg->m_apSkinPartNames[Part], sizeof(pClient->m_aSixup[Conn].m_aaSkinPartNames[Part]), protocol7::MAX_SKIN_LENGTH);
		apSkinPartsPtr[Part] = pClient->m_aSixup[Conn].m_aaSkinPartNames[Part];
		pClient->m_aSixup[Conn].m_aUseCustomColors[Part] = pMsg->m_aUseCustomColors[Part];
		pClient->m_aSixup[Conn].m_aSkinPartColors[Part] = pMsg->m_aSkinPartColors[Part];
	}
	m_Skins7.ValidateSkinParts(apSkinPartsPtr, pClient->m_aSixup[Conn].m_aUseCustomColors, pClient->m_aSixup[Conn].m_aSkinPartColors, m_pClient->m_TranslationContext.m_GameFlags);

	if(time_season() == SEASON_XMAS)
	{
		pClient->m_SkinInfo.m_aSixup[Conn].m_HatTexture = m_Skins7.XmasHatTexture();
		pClient->m_SkinInfo.m_aSixup[Conn].m_HatSpriteIndex = ClientId % CSkins7::HAT_NUM;
	}
	else
		pClient->m_SkinInfo.m_aSixup[Conn].m_HatTexture.Invalidate();

	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		const CSkins7::CSkinPart *pSkinPart = m_Skins7.FindSkinPart(Part, pClient->m_aSixup[Conn].m_aaSkinPartNames[Part], false);
		if(pClient->m_aSixup[Conn].m_aUseCustomColors[Part])
		{
			pClient->m_SkinInfo.m_aSixup[Conn].m_aTextures[Part] = pSkinPart->m_ColorTexture;
			pClient->m_SkinInfo.m_aSixup[Conn].m_aColors[Part] = m_Skins7.GetColor(pMsg->m_aSkinPartColors[Part], Part == protocol7::SKINPART_MARKING);
		}
		else
		{
			pClient->m_SkinInfo.m_aSixup[Conn].m_aTextures[Part] = pSkinPart->m_OrgTexture;
			pClient->m_SkinInfo.m_aSixup[Conn].m_aColors[Part] = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		}
		if(pClient->m_SkinInfo.m_aSixup[Conn].m_HatTexture.IsValid())
		{
			if(Part == protocol7::SKINPART_BODY && str_comp(pClient->m_aSixup[Conn].m_aaSkinPartNames[Part], "standard"))
				pClient->m_SkinInfo.m_aSixup[Conn].m_HatSpriteIndex = CSkins7::HAT_OFFSET_SIDE + (ClientId % CSkins7::HAT_NUM);
			if(Part == protocol7::SKINPART_DECORATION && !str_comp(pClient->m_aSixup[Conn].m_aaSkinPartNames[Part], "twinbopp"))
				pClient->m_SkinInfo.m_aSixup[Conn].m_HatSpriteIndex = CSkins7::HAT_OFFSET_SIDE + (ClientId % CSkins7::HAT_NUM);
		}
	}
}

void CGameClient::ApplySkin7InfoFromSnapObj(const protocol7::CNetObj_De_ClientInfo *pObj, int ClientId)
{
	char aSkinPartNames[protocol7::NUM_SKINPARTS][protocol7::MAX_SKIN_ARRAY_SIZE];
	protocol7::CNetMsg_Sv_SkinChange Msg;
	Msg.m_ClientId = ClientId;
	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		IntsToStr(pObj->m_aaSkinPartNames[Part], 6, aSkinPartNames[Part], std::size(aSkinPartNames[Part]));
		Msg.m_apSkinPartNames[Part] = aSkinPartNames[Part];
		Msg.m_aUseCustomColors[Part] = pObj->m_aUseCustomColors[Part];
		Msg.m_aSkinPartColors[Part] = pObj->m_aSkinPartColors[Part];
	}
	ApplySkin7InfoFromGameMsg(&Msg, ClientId, 0);
}

void *CGameClient::TranslateGameMsg(int *pMsgId, CUnpacker *pUnpacker, int Conn)
{
	if(!m_pClient->IsSixup())
	{
		return m_NetObjHandler.SecureUnpackMsg(*pMsgId, pUnpacker);
	}

	void *pRawMsg = m_NetObjHandler7.SecureUnpackMsg(*pMsgId, pUnpacker);
	if(!pRawMsg)
	{
		if(*pMsgId > __NETMSGTYPE_UUID_HELPER && *pMsgId < OFFSET_MAPITEMTYPE_UUID)
		{
			void *pDDNetExMsg = m_NetObjHandler.SecureUnpackMsg(*pMsgId, pUnpacker);
			if(pDDNetExMsg)
				return pDDNetExMsg;
		}

		dbg_msg("sixup", "dropped weird message '%s' (%d), failed on '%s'", m_NetObjHandler7.GetMsgName(*pMsgId), *pMsgId, m_NetObjHandler7.FailedMsgOn());
		return nullptr;
	}
	static char s_aRawMsg[1024];

	if(*pMsgId == protocol7::NETMSGTYPE_SV_MOTD)
	{
		protocol7::CNetMsg_Sv_Motd *pMsg7 = (protocol7::CNetMsg_Sv_Motd *)pRawMsg;
		::CNetMsg_Sv_Motd *pMsg = (::CNetMsg_Sv_Motd *)s_aRawMsg;

		pMsg->m_pMessage = pMsg7->m_pMessage;

		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_BROADCAST)
	{
		protocol7::CNetMsg_Sv_Broadcast *pMsg7 = (protocol7::CNetMsg_Sv_Broadcast *)pRawMsg;
		::CNetMsg_Sv_Broadcast *pMsg = (::CNetMsg_Sv_Broadcast *)s_aRawMsg;

		pMsg->m_pMessage = pMsg7->m_pMessage;

		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_CL_SETTEAM)
	{
		protocol7::CNetMsg_Cl_SetTeam *pMsg7 = (protocol7::CNetMsg_Cl_SetTeam *)pRawMsg;
		::CNetMsg_Cl_SetTeam *pMsg = (::CNetMsg_Cl_SetTeam *)s_aRawMsg;

		pMsg->m_Team = pMsg7->m_Team;

		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_TEAM)
	{
		protocol7::CNetMsg_Sv_Team *pMsg7 = (protocol7::CNetMsg_Sv_Team *)pRawMsg;

		if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
		{
			m_aClients[pMsg7->m_ClientId].m_Team = pMsg7->m_Team;
			m_pClient->m_TranslationContext.m_aClients[pMsg7->m_ClientId].m_Team = pMsg7->m_Team;
			m_aClients[pMsg7->m_ClientId].UpdateRenderInfo(IsTeamPlay());

			// if(pMsg7->m_ClientId == m_LocalClientId)
			// {
			// 	m_TeamCooldownTick = pMsg7->m_CooldownTick;
			// 	m_TeamChangeTime = Client()->LocalTime();
			// }
		}

		if(Conn != g_Config.m_ClDummy)
			return nullptr;

		if(pMsg7->m_Silent == 0)
		{
			DoTeamChangeMessage7(m_aClients[pMsg7->m_ClientId].m_aName, pMsg7->m_ClientId, pMsg7->m_Team);
		}

		// we drop the message and add the new team
		// info to the playerinfo snap item
		// using translation context
		return nullptr;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_WEAPONPICKUP)
	{
		protocol7::CNetMsg_Sv_WeaponPickup *pMsg7 = (protocol7::CNetMsg_Sv_WeaponPickup *)pRawMsg;
		::CNetMsg_Sv_WeaponPickup *pMsg = (::CNetMsg_Sv_WeaponPickup *)s_aRawMsg;

		pMsg->m_Weapon = pMsg7->m_Weapon;

		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_SERVERSETTINGS)
	{
		// 0.7 only message for ui enrichment like locked teams
		protocol7::CNetMsg_Sv_ServerSettings *pMsg = (protocol7::CNetMsg_Sv_ServerSettings *)pRawMsg;

		if(!m_pClient->m_TranslationContext.m_ServerSettings.m_TeamLock && pMsg->m_TeamLock)
			m_Chat.AddLine(-1, 0, "Teams were locked");
		else if(m_pClient->m_TranslationContext.m_ServerSettings.m_TeamLock && !pMsg->m_TeamLock)
			m_Chat.AddLine(-1, 0, "Teams were unlocked");

		m_pClient->m_TranslationContext.m_ServerSettings.m_KickVote = pMsg->m_KickVote;
		m_pClient->m_TranslationContext.m_ServerSettings.m_KickMin = pMsg->m_KickMin;
		m_pClient->m_TranslationContext.m_ServerSettings.m_SpecVote = pMsg->m_SpecVote;
		m_pClient->m_TranslationContext.m_ServerSettings.m_TeamLock = pMsg->m_TeamLock;
		m_pClient->m_TranslationContext.m_ServerSettings.m_TeamBalance = pMsg->m_TeamBalance;
		m_pClient->m_TranslationContext.m_ServerSettings.m_PlayerSlots = pMsg->m_PlayerSlots;
		return nullptr; // There is no 0.6 equivalent
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_RACEFINISH)
	{
		*pMsgId = NETMSGTYPE_SV_RACEFINISH;
		protocol7::CNetMsg_Sv_RaceFinish *pMsg7 = (protocol7::CNetMsg_Sv_RaceFinish *)pRawMsg;
		::CNetMsg_Sv_RaceFinish *pMsg = (::CNetMsg_Sv_RaceFinish *)s_aRawMsg;

		pMsg->m_ClientId = pMsg7->m_ClientId;
		pMsg->m_Time = pMsg7->m_Time;
		pMsg->m_Diff = pMsg7->m_Diff;
		pMsg->m_RecordPersonal = pMsg7->m_RecordPersonal;
		pMsg->m_RecordServer = pMsg7->m_RecordServer;

		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_COMMANDINFOREMOVE)
	{
		*pMsgId = NETMSGTYPE_SV_COMMANDINFOREMOVE;
		protocol7::CNetMsg_Sv_CommandInfoRemove *pMsg7 = (protocol7::CNetMsg_Sv_CommandInfoRemove *)pRawMsg;
		::CNetMsg_Sv_CommandInfoRemove *pMsg = (::CNetMsg_Sv_CommandInfoRemove *)s_aRawMsg;

		pMsg->m_pName = pMsg7->m_pName;

		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_COMMANDINFO)
	{
		*pMsgId = NETMSGTYPE_SV_COMMANDINFO;
		protocol7::CNetMsg_Sv_CommandInfo *pMsg7 = (protocol7::CNetMsg_Sv_CommandInfo *)pRawMsg;
		::CNetMsg_Sv_CommandInfo *pMsg = (::CNetMsg_Sv_CommandInfo *)s_aRawMsg;

		pMsg->m_pName = pMsg7->m_pName;
		pMsg->m_pArgsFormat = pMsg7->m_pArgsFormat;
		pMsg->m_pHelpText = pMsg7->m_pHelpText;

		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_SKINCHANGE)
	{
		protocol7::CNetMsg_Sv_SkinChange *pMsg7 = (protocol7::CNetMsg_Sv_SkinChange *)pRawMsg;

		if(pMsg7->m_ClientId < 0 || pMsg7->m_ClientId >= MAX_CLIENTS)
		{
			dbg_msg("sixup", "Sv_SkinChange got invalid ClientId: %d", pMsg7->m_ClientId);
			return nullptr;
		}

		CTranslationContext::CClientData &Client = m_pClient->m_TranslationContext.m_aClients[pMsg7->m_ClientId];
		Client.m_Active = true;
		ApplySkin7InfoFromGameMsg(pMsg7, pMsg7->m_ClientId, Conn);
		// skin will be moved to the 0.6 snap by the translation context
		// and we drop the game message
		return nullptr;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_VOTECLEAROPTIONS)
	{
		*pMsgId = NETMSGTYPE_SV_VOTECLEAROPTIONS;
		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_VOTEOPTIONADD)
	{
		*pMsgId = NETMSGTYPE_SV_VOTEOPTIONADD;
		protocol7::CNetMsg_Sv_VoteOptionAdd *pMsg7 = (protocol7::CNetMsg_Sv_VoteOptionAdd *)pRawMsg;
		::CNetMsg_Sv_VoteOptionAdd *pMsg = (::CNetMsg_Sv_VoteOptionAdd *)s_aRawMsg;
		pMsg->m_pDescription = pMsg7->m_pDescription;
		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_VOTEOPTIONREMOVE)
	{
		*pMsgId = NETMSGTYPE_SV_VOTEOPTIONREMOVE;
		protocol7::CNetMsg_Sv_VoteOptionRemove *pMsg7 = (protocol7::CNetMsg_Sv_VoteOptionRemove *)pRawMsg;
		::CNetMsg_Sv_VoteOptionRemove *pMsg = (::CNetMsg_Sv_VoteOptionRemove *)s_aRawMsg;
		pMsg->m_pDescription = pMsg7->m_pDescription;
		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_VOTEOPTIONLISTADD)
	{
		::CNetMsg_Sv_VoteOptionListAdd *pMsg = (::CNetMsg_Sv_VoteOptionListAdd *)s_aRawMsg;
		int NumOptions = pUnpacker->GetInt();
		if(NumOptions > 14)
		{
			for(int i = 0; i < NumOptions; i++)
			{
				const char *pDescription = pUnpacker->GetString(CUnpacker::SANITIZE_CC);
				if(pUnpacker->Error())
					continue;

				m_Voting.AddOption(pDescription);
			}
			// 0.7 can send more vote options than
			// the 0.6 protocol fit
			// in that case we do not translate it but just
			// reimplement what 0.6 would do
			return nullptr;
		}
		pMsg->m_NumOptions = 0;
		for(int i = 0; i < NumOptions; i++)
		{
			const char *pDescription = pUnpacker->GetString(CUnpacker::SANITIZE_CC);
			if(pUnpacker->Error())
				continue;

			pMsg->m_NumOptions++;
			switch(i)
			{
			case 0: (pMsg->m_pDescription0 = pDescription); break;
			case 1: (pMsg->m_pDescription1 = pDescription); break;
			case 2: (pMsg->m_pDescription2 = pDescription); break;
			case 3: (pMsg->m_pDescription3 = pDescription); break;
			case 4: (pMsg->m_pDescription4 = pDescription); break;
			case 5: (pMsg->m_pDescription5 = pDescription); break;
			case 6: (pMsg->m_pDescription6 = pDescription); break;
			case 7: (pMsg->m_pDescription7 = pDescription); break;
			case 8: (pMsg->m_pDescription8 = pDescription); break;
			case 9: (pMsg->m_pDescription9 = pDescription); break;
			case 10: (pMsg->m_pDescription10 = pDescription); break;
			case 11: (pMsg->m_pDescription11 = pDescription); break;
			case 12: (pMsg->m_pDescription12 = pDescription); break;
			case 13: (pMsg->m_pDescription13 = pDescription); break;
			case 14: (pMsg->m_pDescription14 = pDescription);
			}
		}
		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_VOTESET)
	{
		*pMsgId = NETMSGTYPE_SV_VOTESET;
		protocol7::CNetMsg_Sv_VoteSet *pMsg7 = (protocol7::CNetMsg_Sv_VoteSet *)pRawMsg;
		::CNetMsg_Sv_VoteSet *pMsg = (::CNetMsg_Sv_VoteSet *)s_aRawMsg;

		pMsg->m_Timeout = pMsg7->m_Timeout;
		pMsg->m_pDescription = pMsg7->m_pDescription;
		pMsg->m_pReason = pMsg7->m_pReason;

		char aBuf[128];
		if(pMsg7->m_Timeout)
		{
			if(pMsg7->m_ClientId != -1)
			{
				const char *pName = m_aClients[pMsg7->m_ClientId].m_aName;
				switch(pMsg7->m_Type)
				{
				case protocol7::VOTE_START_OP:
					str_format(aBuf, sizeof(aBuf), "'%s' called vote to change server option '%s' (%s)", pName, pMsg7->m_pDescription, pMsg7->m_pReason);
					m_Chat.AddLine(-1, 0, aBuf);
					break;
				case protocol7::VOTE_START_KICK:
				{
					str_format(aBuf, sizeof(aBuf), "'%s' called for vote to kick '%s' (%s)", pName, pMsg7->m_pDescription, pMsg7->m_pReason);
					m_Chat.AddLine(-1, 0, aBuf);
					break;
				}
				case protocol7::VOTE_START_SPEC:
				{
					str_format(aBuf, sizeof(aBuf), "'%s' called for vote to move '%s' to spectators (%s)", pName, pMsg7->m_pDescription, pMsg7->m_pReason);
					m_Chat.AddLine(-1, 0, aBuf);
				}
				}
			}
		}
		else
		{
			switch(pMsg7->m_Type)
			{
			case protocol7::VOTE_START_OP:
				str_format(aBuf, sizeof(aBuf), "Admin forced server option '%s' (%s)", pMsg7->m_pDescription, pMsg7->m_pReason);
				m_Chat.AddLine(-1, 0, aBuf);
				break;
			case protocol7::VOTE_START_SPEC:
				str_format(aBuf, sizeof(aBuf), "Admin moved '%s' to spectator (%s)", pMsg7->m_pDescription, pMsg7->m_pReason);
				m_Chat.AddLine(-1, 0, aBuf);
				break;
			case protocol7::VOTE_END_ABORT:
				m_Voting.OnReset();
				m_Chat.AddLine(-1, 0, "Vote aborted");
				break;
			case protocol7::VOTE_END_PASS:
				m_Voting.OnReset();
				m_Chat.AddLine(-1, 0, pMsg7->m_ClientId == -1 ? "Admin forced vote yes" : "Vote passed");
				break;
			case protocol7::VOTE_END_FAIL:
				m_Voting.OnReset();
				m_Chat.AddLine(-1, 0, pMsg7->m_ClientId == -1 ? "Admin forced vote no" : "Vote failed");
			}
		}

		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_VOTESTATUS)
	{
		*pMsgId = NETMSGTYPE_SV_VOTESTATUS;
		protocol7::CNetMsg_Sv_VoteStatus *pMsg7 = (protocol7::CNetMsg_Sv_VoteStatus *)pRawMsg;
		::CNetMsg_Sv_VoteStatus *pMsg = (::CNetMsg_Sv_VoteStatus *)s_aRawMsg;

		pMsg->m_Yes = pMsg7->m_Yes;
		pMsg->m_No = pMsg7->m_No;
		pMsg->m_Pass = pMsg7->m_Pass;
		pMsg->m_Total = pMsg7->m_Total;

		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_READYTOENTER)
	{
		*pMsgId = NETMSGTYPE_SV_READYTOENTER;
		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_CLIENTDROP)
	{
		protocol7::CNetMsg_Sv_ClientDrop *pMsg7 = (protocol7::CNetMsg_Sv_ClientDrop *)pRawMsg;
		if(pMsg7->m_ClientId < 0 || pMsg7->m_ClientId >= MAX_CLIENTS)
		{
			dbg_msg("sixup", "Sv_ClientDrop got invalid ClientId: %d", pMsg7->m_ClientId);
			return nullptr;
		}
		CTranslationContext::CClientData &Client = m_pClient->m_TranslationContext.m_aClients[pMsg7->m_ClientId];
		Client.Reset();

		if(pMsg7->m_Silent)
			return nullptr;

		if(Conn != g_Config.m_ClDummy)
			return nullptr;

		static char s_aBuf[128];
		if(pMsg7->m_pReason[0])
			str_format(s_aBuf, sizeof(s_aBuf), "'%s' has left the game (%s)", m_aClients[pMsg7->m_ClientId].m_aName, pMsg7->m_pReason);
		else
			str_format(s_aBuf, sizeof(s_aBuf), "'%s' has left the game", m_aClients[pMsg7->m_ClientId].m_aName);
		m_Chat.AddLine(-1, 0, s_aBuf);

		return nullptr;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_CLIENTINFO)
	{
		protocol7::CNetMsg_Sv_ClientInfo *pMsg7 = (protocol7::CNetMsg_Sv_ClientInfo *)pRawMsg;
		if(pMsg7->m_ClientId < 0 || pMsg7->m_ClientId >= MAX_CLIENTS)
		{
			dbg_msg("sixup", "Sv_ClientInfo got invalid ClientId: %d", pMsg7->m_ClientId);
			return nullptr;
		}

		if(pMsg7->m_Local)
		{
			m_pClient->m_TranslationContext.m_aLocalClientId[Conn] = pMsg7->m_ClientId;
		}
		CTranslationContext::CClientData &Client = m_pClient->m_TranslationContext.m_aClients[pMsg7->m_ClientId];
		Client.m_Active = true;
		Client.m_Team = pMsg7->m_Team;
		str_copy(Client.m_aName, pMsg7->m_pName);
		str_copy(Client.m_aClan, pMsg7->m_pClan);
		Client.m_Country = pMsg7->m_Country;
		ApplySkin7InfoFromGameMsg(pMsg7, pMsg7->m_ClientId, Conn);
		if(m_pClient->m_TranslationContext.m_aLocalClientId[Conn] == -1)
			return nullptr;
		if(pMsg7->m_Silent || pMsg7->m_Local)
			return nullptr;

		if(Conn != g_Config.m_ClDummy)
			return nullptr;

		DoTeamChangeMessage7(
			pMsg7->m_pName,
			pMsg7->m_ClientId,
			pMsg7->m_Team,
			"entered and ");

		return nullptr; // we only do side effects and add stuff to the snap here
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_GAMEINFO)
	{
		protocol7::CNetMsg_Sv_GameInfo *pMsg7 = (protocol7::CNetMsg_Sv_GameInfo *)pRawMsg;
		m_pClient->m_TranslationContext.m_GameFlags = pMsg7->m_GameFlags;
		m_pClient->m_TranslationContext.m_ScoreLimit = pMsg7->m_ScoreLimit;
		m_pClient->m_TranslationContext.m_TimeLimit = pMsg7->m_TimeLimit;
		m_pClient->m_TranslationContext.m_MatchNum = pMsg7->m_MatchNum;
		m_pClient->m_TranslationContext.m_MatchCurrent = pMsg7->m_MatchCurrent;
		m_pClient->m_TranslationContext.m_ShouldSendGameInfo = true;
		return nullptr; // Added to snap by translation context
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_EMOTICON)
	{
		*pMsgId = NETMSGTYPE_SV_EMOTICON;
		protocol7::CNetMsg_Sv_Emoticon *pMsg7 = (protocol7::CNetMsg_Sv_Emoticon *)pRawMsg;
		::CNetMsg_Sv_Emoticon *pMsg = (::CNetMsg_Sv_Emoticon *)s_aRawMsg;

		pMsg->m_ClientId = pMsg7->m_ClientId;
		pMsg->m_Emoticon = pMsg7->m_Emoticon;

		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_KILLMSG)
	{
		*pMsgId = NETMSGTYPE_SV_KILLMSG;

		protocol7::CNetMsg_Sv_KillMsg *pMsg7 = (protocol7::CNetMsg_Sv_KillMsg *)pRawMsg;
		::CNetMsg_Sv_KillMsg *pMsg = (::CNetMsg_Sv_KillMsg *)s_aRawMsg;

		pMsg->m_Killer = pMsg7->m_Killer;
		pMsg->m_Victim = pMsg7->m_Victim;
		pMsg->m_Weapon = pMsg7->m_Weapon;
		pMsg->m_ModeSpecial = pMsg7->m_ModeSpecial;

		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_CHAT)
	{
		*pMsgId = NETMSGTYPE_SV_CHAT;

		protocol7::CNetMsg_Sv_Chat *pMsg7 = (protocol7::CNetMsg_Sv_Chat *)pRawMsg;
		::CNetMsg_Sv_Chat *pMsg = (::CNetMsg_Sv_Chat *)s_aRawMsg;

		if(pMsg7->m_Mode == protocol7::CHAT_WHISPER)
		{
			bool Receive = pMsg7->m_TargetId == m_pClient->m_TranslationContext.m_aLocalClientId[Conn];

			pMsg->m_Team = Receive ? 3 : 2;
			pMsg->m_ClientId = Receive ? pMsg7->m_ClientId : pMsg7->m_TargetId;
		}
		else
		{
			pMsg->m_Team = pMsg7->m_Mode == protocol7::CHAT_TEAM ? 1 : 0;
			pMsg->m_ClientId = pMsg7->m_ClientId;
		}

		pMsg->m_pMessage = pMsg7->m_pMessage;

		return s_aRawMsg;
	}
	else if(*pMsgId == protocol7::NETMSGTYPE_SV_GAMEMSG)
	{
		int GameMsgId = pUnpacker->GetInt();

		/**
		 * Prints chat message only once
		 * even if it is being sent to main tee and dummy
		 */
		auto SendChat = [Conn, GameMsgId, this](const char *pText) -> void {
			if(GameMsgId != protocol7::GAMEMSG_TEAM_BALANCE_VICTIM && GameMsgId != protocol7::GAMEMSG_SPEC_INVALIDID)
			{
				if(Conn != g_Config.m_ClDummy)
					return;
			}
			m_Chat.AddLine(-1, 0, pText);
		};

		// check for valid gamemsgid
		if(GameMsgId < 0 || GameMsgId >= protocol7::NUM_GAMEMSGS)
			return nullptr;

		int aParaI[3] = {0};
		int NumParaI = 0;

		// get paras
		switch(gs_GameMsgList7[GameMsgId].m_ParaType)
		{
		case PARA_I: NumParaI = 1; break;
		case PARA_II: NumParaI = 2; break;
		case PARA_III: NumParaI = 3; break;
		}
		for(int i = 0; i < NumParaI; i++)
		{
			aParaI[i] = pUnpacker->GetInt();
		}

		// check for unpacking errors
		if(pUnpacker->Error())
			return nullptr;

		// handle special messages
		char aBuf[256];
		bool TeamPlay = m_pClient->m_TranslationContext.m_GameFlags & protocol7::GAMEFLAG_TEAMS;
		if(gs_GameMsgList7[GameMsgId].m_Action == DO_SPECIAL)
		{
			switch(GameMsgId)
			{
			case protocol7::GAMEMSG_CTF_DROP:
				if(Conn == g_Config.m_ClDummy)
					m_Sounds.Enqueue(CSounds::CHN_GLOBAL, SOUND_CTF_DROP);
				break;
			case protocol7::GAMEMSG_CTF_RETURN:
				if(Conn == g_Config.m_ClDummy)
					m_Sounds.Enqueue(CSounds::CHN_GLOBAL, SOUND_CTF_RETURN);
				break;
			case protocol7::GAMEMSG_TEAM_ALL:
			{
				const char *pMsg = "";
				switch(GetStrTeam7(aParaI[0], TeamPlay))
				{
				case STR_TEAM_GAME: pMsg = "All players were moved to the game"; break;
				case STR_TEAM_RED: pMsg = "All players were moved to the red team"; break;
				case STR_TEAM_BLUE: pMsg = "All players were moved to the blue team"; break;
				case STR_TEAM_SPECTATORS: pMsg = "All players were moved to the spectators"; break;
				}
				m_Broadcast.DoBroadcast(pMsg); // client side broadcast
			}
			break;
			case protocol7::GAMEMSG_TEAM_BALANCE_VICTIM:
			{
				const char *pMsg = "";
				switch(GetStrTeam7(aParaI[0], TeamPlay))
				{
				case STR_TEAM_RED: pMsg = "You were moved to the red team due to team balancing"; break;
				case STR_TEAM_BLUE: pMsg = "You were moved to the blue team due to team balancing"; break;
				}
				m_Broadcast.DoBroadcast(pMsg); // client side broadcast
			}
			break;
			case protocol7::GAMEMSG_CTF_GRAB:
				if(Conn == g_Config.m_ClDummy)
					m_Sounds.Enqueue(CSounds::CHN_GLOBAL, SOUND_CTF_GRAB_EN);
				break;
			case protocol7::GAMEMSG_GAME_PAUSED:
			{
				int ClientId = clamp(aParaI[0], 0, MAX_CLIENTS - 1);
				str_format(aBuf, sizeof(aBuf), "'%s' initiated a pause", m_aClients[ClientId].m_aName);
				SendChat(aBuf);
			}
			break;
			case protocol7::GAMEMSG_CTF_CAPTURE:
				if(Conn == g_Config.m_ClDummy)
					m_Sounds.Enqueue(CSounds::CHN_GLOBAL, SOUND_CTF_CAPTURE);
				int ClientId = clamp(aParaI[1], 0, MAX_CLIENTS - 1);
				m_aStats[ClientId].m_FlagCaptures++;

				float Time = aParaI[2] / (float)Client()->GameTickSpeed();
				if(Time <= 60)
				{
					if(aParaI[0])
					{
						str_format(aBuf, sizeof(aBuf), "The blue flag was captured by '%s' (%.2f seconds)", m_aClients[ClientId].m_aName, Time);
					}
					else
					{
						str_format(aBuf, sizeof(aBuf), "The red flag was captured by '%s' (%.2f seconds)", m_aClients[ClientId].m_aName, Time);
					}
				}
				else
				{
					if(aParaI[0])
					{
						str_format(aBuf, sizeof(aBuf), "The blue flag was captured by '%s'", m_aClients[ClientId].m_aName);
					}
					else
					{
						str_format(aBuf, sizeof(aBuf), "The red flag was captured by '%s'", m_aClients[ClientId].m_aName);
					}
				}
				SendChat(aBuf);
			}
			return nullptr;
		}

		// build message
		const char *pText = "";
		if(NumParaI == 0)
		{
			pText = gs_GameMsgList7[GameMsgId].m_pText;
		}

		// handle message
		switch(gs_GameMsgList7[GameMsgId].m_Action)
		{
		case DO_CHAT:
			SendChat(pText);
			break;
		case DO_BROADCAST:
			m_Broadcast.DoBroadcast(pText); // client side broadcast
			break;
		}

		// no need to handle it in 0.6 since we printed
		// the message already
		return nullptr;
	}

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "dropped weird message '%s' (%d), failed on '%s'", m_NetObjHandler7.GetMsgName(*pMsgId), *pMsgId, m_NetObjHandler7.FailedMsgOn());
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);

	return nullptr;
}
