#include "accounts.h"
#include "fontconvert.h"
#include <base/system.h>
#include <cstring>
#include <engine/server/server.h>
#include <engine/shared/config.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/voting.h>
#include <generated/protocol.h>
#include <random>
#include <string>
#include <vector>

void CGameContext::FoxNetTick()
{
	m_VoteMenu.Tick();
	HandleEffects();

	if(g_Config.m_SvBanSyncing)
		BanSync();

	// Set moving tiles time for quads with pos envelopes
	m_Collision.SetTime(m_pController->GetTime());

	// Save all logged in accounts every 15 minutes
	if(Server()->Tick() % (Server()->TickSpeed() * 60 * 15) == 0)
	{
		m_AccountManager.SaveAllAccounts();
	}

	for(auto it = m_vFakeSnapPlayers.begin(); it != m_vFakeSnapPlayers.end();)
	{
		if(it->m_State == 2)
		{
			it = m_vFakeSnapPlayers.erase(it);
			continue;
		}
		if(it->m_State == 1)
		{
			const int Id = it->m_Id;
			CNetMsg_Sv_Chat Msg;
			Msg.m_Team = 0;
			Msg.m_ClientId = Id;
			Msg.m_pMessage = it->m_aMessage;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
			it->m_State = 2;
		}
		++it;
	}
}
void CGameContext::FoxNetInit()
{
	m_AccountManager.Init(this, ((CServer *)Server())->DbPool());
	m_VoteMenu.Init(this);
	m_Shop.Init(this);

	if(!m_Initialized)
	{
		if(g_Config.m_SvRandomMapVoteOnStart)
			RandomMapVote();
		m_Initialized = true;
	}
	if(Score())
		Score()->CacheMapInfo();
}

void CGameContext::HandleEffects()
{
	// Handle DamageInd effect
	for(auto it = m_DamageIndEffects.begin(); it != m_DamageIndEffects.end();)
	{
		if(it->Remaining > 0 && Server()->Tick() >= it->NextTick)
		{
			int Angles = it->Angles.size() - it->Remaining;
			if(Angles < 0)
				Angles = 0;
			int Positions = it->Pos.size() - it->Remaining;
			if(Positions < 0)
				Positions = 0;

			CreateDamageInd(it->Pos.at(Positions), it->Angles.at(Angles), 1, it->Mask);

			it->Remaining--;
			it->NextTick = Server()->Tick() + it->Delay;
		}
		if(it->Remaining <= 0)
			it = m_DamageIndEffects.erase(it);
		else
			++it;
	}

	// Sound Effect Handle
	for(auto it = m_LaserDeaths.begin(); it != m_LaserDeaths.end();)
	{
		const size_t count = std::min<size_t>((size_t)it->m_Remaining, it->m_StartTick.size());
		for(size_t at = 0; at < count; at++)
		{
			if(Server()->Tick() < it->m_EndTick)
			{
				if(it->m_StartTick[at] == Server()->Tick())
					CreateSound(it->m_Pos, it->m_Sound, it->m_Mask);
			}
		}
		++it;
	}
}

void CGameContext::BanSync()
{
	static int64_t ExecSaveDelay = Server()->Tick() + Server()->TickSpeed();
	if(m_BanSaveDelay < Server()->Tick())
	{
		static bool ExecBans = false;

		if(Storage()->FileExists("Bans.cfg", IStorage::TYPE_ALL))
		{
			if(!ExecBans)
			{
				Server()->SetQuietBan(true);
				Console()->ExecuteBansFile();
				ExecBans = true;
				ExecSaveDelay = Server()->Tick() + Server()->TickSpeed();
			}
		}
		else
		{
			// Info Message
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ban-sync", "Couldn't find \"Bans.cfg\", disabling component ");
			g_Config.m_SvBanSyncing = 0;
			if(g_Config.m_SvBanSyncing == 0)
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ban-sync", "fs_ban_syncing set to 0");
		}

		if(ExecSaveDelay < Server()->Tick() && ExecBans)
		{
			Console()->ExecuteLine("bans_save \"Bans.cfg\"");

			// Info Message
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ban-sync", "Saved Bans");

			ExecBans = false;
			m_BanSaveDelay = Server()->Tick() + Server()->TickSpeed() * (g_Config.m_SvBanSyncingDelay * 60);
		}
	}
	Server()->SetQuietBan(false);
}

void CGameContext::FoxNetSnap(int ClientId, bool GlobalSnap)
{
	SnapLaserEffect(ClientId);
	SnapDebuggedQuad(ClientId);

	// Snap the Fake Player
	for(auto it = m_vFakeSnapPlayers.begin(); it != m_vFakeSnapPlayers.end();)
	{
		const int Id = it->m_Id;

		if(auto *pInfo = Server()->SnapNewItem<CNetObj_ClientInfo>(Id))
		{
			StrToInts(pInfo->m_aName, std::size(pInfo->m_aName), it->m_aName);
			StrToInts(pInfo->m_aClan, std::size(pInfo->m_aClan), it->m_aClan);
			pInfo->m_Country = it->m_Country;
			StrToInts(pInfo->m_aSkin, std::size(pInfo->m_aSkin), it->m_aSkinName);
			pInfo->m_UseCustomColor = it->m_CustomColors;
			pInfo->m_ColorBody = it->m_ColorBody;
			pInfo->m_ColorFeet = it->m_ColorFeet;
		}

		if(auto *pPI = Server()->SnapNewItem<CNetObj_PlayerInfo>(Id))
		{
			pPI->m_Latency = 0;
			pPI->m_Score = 0;
			pPI->m_Team = TEAM_SPECTATORS;
			pPI->m_Local = 0;
			pPI->m_ClientId = Id;
		}
		if(!it->m_State)
			it->m_State = 1;
		++it;
	}
}

void CGameContext::ClearVotes(int ClientId)
{
	if(ClientId == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] && !Server()->ClientSlotEmpty(i))
				ClearVotes(i);
		}
		return;
	}

	CNetMsg_Sv_VoteClearOptions ClearMsg;
	Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientId);
	m_VoteMenu.AddHeader(ClientId);
}

bool CGameContext::ChatDetection(int ClientId, const char *pMsg)
{
	// Thx to Pointer31 for the blueprint
	if(ClientId < 0)
		return false;

	if(m_ChatDetection.empty())
		return false;

	float count = 0; // amount of flagged strings (some strings may count more than others)
	int BanDuration = 0;
	char Reason[64] = "Chat Detection Auto Ban";
	bool IsBan = false;

	const char *ClientName = Server()->ClientName(ClientId);
	const char *pText = FontConvert(pMsg);
	// const char *pActualText = pMsg;

	// make a check for the latest message and the new message coming in right now
	// if the message is the same as the latest one but in a fancy font, then they use some bot client
	// aka ban them

	std::vector<std::string> FoundStrings;
	std::vector<int> Times;
	FoundStrings.clear();
	Times.clear();

	for(const auto &Entry : m_ChatDetection)
	{
		if(Entry.String()[0] == '\0')
			continue;

		if(str_find_nocase(pText, Entry.String()))
		{
			FoundStrings.push_back(Entry.String());
			Times.push_back(Entry.Time());

			count += Entry.Addition();
			BanDuration = Entry.Time();

			if(!IsBan) // if one of the strings is a ban string, then we set IsBan to true
				IsBan = Entry.IsBan();

			if(str_comp(Entry.Reason(), "") != 0)
				str_copy(Reason, Entry.Reason());
		}
	}
	if(!Times.empty())
		BanDuration = *std::max_element(Times.begin(), Times.end());
	else
		BanDuration = 0;

	char InfoMsg[256] = "";
	if(FoundStrings.size() > 0)
	{
		for(const auto &str : FoundStrings)
		{
			str_append(InfoMsg, str.c_str());
			if(&str != &FoundStrings.back())
				str_append(InfoMsg, ", ");
		}

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Name: %s | Strings Found: %s", ClientName, InfoMsg);
		dbg_msg("chat-detection", aBuf);
	}

	if(g_Config.m_SvAntiAdBot)
	{
		// anti whisper ad bot
		if((str_find_nocase(pText, "/whisper") || str_find_nocase(pText, "/w")) && str_find_nocase(pText, "bro, check out this client"))
		{
			str_copy(Reason, "Bot Client Message");
			IsBan = true;
			count += 2;
			BanDuration = 1000;
		}

		// anti mass ping ad bot
		if((str_find_nocase(pText, "stop being a noob") && str_find_nocase(pText, "get good with")) || (str_find_nocase(pText, "Think you could do better") && str_find_nocase(pText, "Not without"))) // mass ping advertising
		{
			if(str_length(pText) > 70) // Usually it pings alot of people
			{
				// try to not remove their message if they are just trying to be funny
				if(!str_find_nocase(pText, "github.com") && !str_find_nocase(pText, "tater") && !str_find_nocase(pText, "tclient") && !str_find_nocase(pText, "t-client") && !str_find_nocase(pText, "tclient.app") // TClient
					&& !str_find_nocase(pText, "aiodob") && !str_find_nocase(pText, "aidob") && !str_find_nocase(pText, "a-client") && !str_find(pText, "A Client") && !str_find(pText, "A client") // AClient
					&& !str_find_nocase(pText, "eclient") && !str_find_nocase(pText, "e client") && !str_find_nocase(pText, "entity client") && !str_find_nocase(pText, "e-client") // Other
					&& !str_find_nocase(pText, "chillerbot") && !str_find_nocase(pText, "cactus")) // Other
				{
					IsBan = true;
					count += 2;
					BanDuration = 1200;
				}
				if(str_find(pText, " ")) // This is the little white space it uses between some letters
				{
					IsBan = true;
					count += 2;
					BanDuration = 1200;
				}
				str_copy(Reason, "Bot Client Message");
			}
		}
	}

	if(count >= 2 && BanDuration > 0)
	{
		if(IsBan)
			Server()->Ban(ClientId, BanDuration * 60, Reason, "");
		else
			MuteWithMessage(Server()->ClientAddr(ClientId), BanDuration * 60, Reason, ClientName);

		return true; // Don't send their chat message
	}
	return false;
}

bool CGameContext::NameDetection(int ClientId, const char *pName, bool PreventNameChange)
{
	if(ClientId < 0)
		return false;

	if(m_NameDetection.empty())
		return false;

	const char *ClientName = pName;

	int BanDuration = 0;
	char Reason[64] = "Name Detection Auto Ban";

	std::vector<std::string> FoundStrings;
	std::vector<int> Times;
	FoundStrings.clear();
	Times.clear();

	for(const auto &Entry : m_NameDetection)
	{
		if(Entry.String()[0] == '\0')
			continue;

		bool FoundEntry = false;

		if(Entry.ExactMatch())
		{
			if(!str_comp(ClientName, Entry.String()))
				FoundEntry = true;
		}
		else if(str_find_nocase(ClientName, Entry.String()))
			FoundEntry = true;

		if(FoundEntry)
		{
			FoundStrings.push_back(Entry.String());
			Times.push_back(Entry.Time());

			BanDuration = Entry.Time();

			if(str_comp(Entry.Reason(), "") != 0)
				str_copy(Reason, Entry.Reason());
		}
	}

	if(!Times.empty())
		BanDuration = *std::max_element(Times.begin(), Times.end());
	else
		BanDuration = 0;

	if(FoundStrings.size() > 0)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Name: %s | Banned String Found: ", ClientName);
		for(const auto &str : FoundStrings)
		{
			str_append(aBuf, str.c_str());
			str_append(aBuf, ", ");
		}
		dbg_msg("name-detection", aBuf);

		if(!PreventNameChange && BanDuration > 0)
			Server()->Ban(ClientId, BanDuration * 60, Reason, "");
		return true;
	}

	return false;
}

void CGameContext::OnLogin(int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	CPlayer *pPl = m_apPlayers[ClientId];
	if(!pPl)
		return;
	const int Flags = pPl->Acc()->m_Flags;
	if(Flags & ACC_FLAG_HIDE_COSMETICS)
		pPl->m_HideCosmetics = true;
	char aItemsCopy[1028];
	str_copy(aItemsCopy, pPl->Acc()->m_LastActiveItems, sizeof(aItemsCopy));

	for(char *pToken = strtok(aItemsCopy, " "); pToken; pToken = strtok(nullptr, " "))
	{
		char *pEqual = strchr(pToken, '=');
		if(pEqual)
		{
			*pEqual = '\0';
			const char *pShortcut = pToken;
			int value = str_toint(pEqual + 1);

			if(!str_comp(pShortcut, "G_E"))
			{
				pPl->ToggleItem(pShortcut, value);
				continue;
			}
			// Add more shortcut checks if any other new cosmetic needs it
		}
		else
		{
			pPl->ToggleItem(pToken, true);
		}
	}
}
void CGameContext::OnLogout(int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	CPlayer *pPl = m_apPlayers[ClientId];
	if(!pPl)
		return;
	pPl->DisableAllCosmetics();
}

void CGameContext::SendEmote(int ClientId, int Type)
{
	int EmoteType = Type;
	switch(Type)
	{
	case EMOTICON_EXCLAMATION:
	case EMOTICON_GHOST:
	case EMOTICON_QUESTION:
	case EMOTICON_WTF:
		EmoteType = EMOTE_SURPRISE;
		break;
	case EMOTICON_DOTDOT:
	case EMOTICON_DROP:
	case EMOTICON_ZZZ:
		EmoteType = EMOTE_BLINK;
		break;
	case EMOTICON_EYES:
	case EMOTICON_HEARTS:
	case EMOTICON_MUSIC:
		EmoteType = EMOTE_HAPPY;
		break;
	case EMOTICON_OOP:
	case EMOTICON_SORRY:
	case EMOTICON_SUSHI:
		EmoteType = EMOTE_PAIN;
		break;
	case EMOTICON_DEVILTEE:
	case EMOTICON_SPLATTEE:
	case EMOTICON_ZOMG:
		EmoteType = EMOTE_ANGRY;
		break;
	default:
		break;
	}

	GetPlayerChar(ClientId)->SetEmote(EmoteType, Server()->Tick() + 2 * Server()->TickSpeed());
	SendEmoticon(ClientId, Type, -1);
}

void CGameContext::CreateIndEffect(int Type, vec2 Pos, vec2 Direction, CClientMask Mask)
{
	float AngleOffset = 0;
	float StarDistance = 0.18f;
	float Angle = -std::atan2(Direction.x, Direction.y);

	DamageIndEffects effect;
	if(Type >= IND_CLOCKWISE && Type <= IND_COUNTERWISE)
	{
		AngleOffset = 0.80f;
		effect.Remaining = 10;
		for(int Remaining = 0; Remaining < effect.Remaining; Remaining++)
		{
			if(Type == IND_CLOCKWISE)
				effect.Angles.push_back(Angle - AngleOffset + (Remaining * StarDistance));
			else
				effect.Angles.push_back(Angle + AngleOffset - (Remaining * StarDistance));
		}
		effect.Pos.push_back(Pos);
		effect.Delay = 1;
		effect.NextTick = Server()->Tick();
		effect.Mask = Mask;
		m_DamageIndEffects.push_back(effect);

		effect.Pos.clear();
		effect.Angles.clear();
	}
	else if(Type == IND_INWARD)
	{
		AngleOffset = -0.90f;

		for(int i = 0; i < 2; i++)
		{
			effect.Remaining = 5;
			for(int Remaining = 0; Remaining < effect.Remaining; Remaining++)
			{
				if(i == 0)
					effect.Angles.push_back(Angle + AngleOffset + (Remaining * StarDistance));
				else
					effect.Angles.push_back(Angle - AngleOffset - (Remaining * StarDistance));
			}
			effect.Pos.push_back(Pos);
			effect.Delay = 2;
			effect.NextTick = Server()->Tick();
			effect.Mask = Mask;

			m_DamageIndEffects.push_back(effect);

			effect.Pos.clear();
			effect.Angles.clear();
		}
	}
	else if(Type == IND_OUTWARD)
	{
		AngleOffset = 0.20f;

		for(int i = 0; i < 2; i++)
		{
			effect.Remaining = 5;
			for(int Remaining = 0; Remaining < effect.Remaining; Remaining++)
			{
				if(i == 0)
					effect.Angles.push_back(Angle - AngleOffset - (Remaining * StarDistance));
				else
					effect.Angles.push_back(Angle + AngleOffset + (Remaining * StarDistance));
			}
			effect.Pos.push_back(Pos);
			effect.Delay = 2;
			effect.NextTick = Server()->Tick();
			effect.Mask = Mask;

			m_DamageIndEffects.push_back(effect);

			effect.Pos.clear();
			effect.Angles.clear();
		}
	}
	else if(Type == IND_LINE)
	{
		effect.Remaining = 6;
		for(int Remaining = 0; Remaining < effect.Remaining; Remaining++)
		{
			float Offset = Remaining * 15.0f;
			vec2 CalcPos = Pos - Direction * 25.0f + Direction * Offset;
			effect.Pos.push_back(CalcPos);
		}
		effect.Angles.push_back(Angle - AngleOffset);

		effect.Delay = 1;
		effect.NextTick = Server()->Tick();
		effect.Mask = Mask;
		m_DamageIndEffects.push_back(effect);
		effect.Pos.clear();
		effect.Angles.clear();
	}
	else if(Type == IND_CRISSCROSS)
	{
		effect.Remaining = 3;
		for(int Remaining = 0; Remaining < effect.Remaining; Remaining++)
		{
			vec2 CalcPos;
			float perpAngle = 0.0f;

			float GetAngle = angle(Direction);
			if(GetAngle < 0.0f)
				GetAngle += 2.0f * pi;

			if(Remaining == 0)
			{
				perpAngle = GetAngle - AngleOffset + pi / 2;
				effect.Angles.push_back(Angle - AngleOffset - 0.85f);
				CalcPos = Pos + vec2(cosf(perpAngle), sinf(perpAngle)) * 25.0f;
			}
			else if(Remaining == 1)
			{
				CalcPos = Pos - Direction * 15.0f;
				effect.Angles.push_back(Angle - AngleOffset);
			}
			else
			{
				perpAngle = GetAngle - AngleOffset - pi / 2;
				effect.Angles.push_back(Angle - AngleOffset + 0.85f);
				CalcPos = Pos + vec2(cosf(perpAngle), sinf(perpAngle)) * 25.0f;
			}

			effect.Pos.push_back(CalcPos);
		}

		effect.Delay = 1;
		effect.NextTick = Server()->Tick();
		effect.Mask = Mask;
		m_DamageIndEffects.push_back(effect);
		effect.Pos.clear();
		effect.Angles.clear();
	}
	else
	{
		CreateDamageInd(Pos, Angle, 10, Mask);
		return;
	}
}

bool CGameContext::IsValidHookPower(int HookPower)
{
	return HookPower == HOOK_NORMAL || HookPower == HOOK_RAINBOW || HookPower == HOOK_BLOODY;
}

const char *CGameContext::HookTypeName(int HookType)
{
	switch(HookType)
	{
	case HOOK_NORMAL:
		return "Normal Hook";
	case HOOK_RAINBOW:
		return "Rainbow Hook";
	case HOOK_BLOODY:
		return "Bloody Hook";
	}
	return "Unknown";
}

void CGameContext::UnsetTelekinesis(int ClientId)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter *pChr = GetPlayerChar(i);
		if(pChr && pChr->m_TelekinesisId == ClientId)
		{
			pChr->m_TelekinesisId = -1;
			break; // can break here, every entity can only be picked by one player using telekinesis at the time
		}
	}
}

bool CGameContext::SendFakeTuningParams(int ClientId, const CTuningParams &FakeTuning, bool RealTune)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !GetPlayerChar(ClientId))
		return false;

	CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
	for(unsigned i = 0; i < sizeof(FakeTuning) / sizeof(int); i++)
	{
		Msg.AddInt(((int *)&FakeTuning)[i]);
	}

	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientId);

	if(RealTune)
		GetPlayerChar(ClientId)->SetFakeTuned(true, FakeTuning);
	else
		GetPlayerChar(ClientId)->SetFakeTuned(true);

	return true;
}

bool CGameContext::ResetFakeTunes(int ClientId, int Zone)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !GetPlayerChar(ClientId))
		return false;

	GetPlayerChar(ClientId)->SetFakeTuned(false);
	SendTuningParams(ClientId, Zone);
	return true;
}

void CGameContext::CreateLaserDeath(int Type, int pOwner, vec2 pPos, CClientMask pMask)
{
	LaserDeath effect;

	std::random_device rd;
	std::uniform_int_distribution<long> dist(5.0, 50.0);

	effect.m_Pos = pPos;
	effect.m_Mask = pMask;
	effect.m_Owner = pOwner;

	effect.m_Remaining = 15;
	effect.m_EndTick = Server()->Tick() + (Server()->TickSpeed() / 4.5f * effect.m_Remaining);
	effect.m_Sound = SOUND_HOOK_LOOP;
	for(int Num = 0; Num < effect.m_Remaining; Num++)
	{
		long Random = dist(rd) + Num;

		vec2 Pos = pPos + random_direction() * Random;

		effect.m_aIds.push_back(Server()->SnapNewId());

		effect.m_From.push_back(Pos);
		effect.m_To.push_back(Pos);
		effect.m_StartTick.push_back(Server()->Tick() + Server()->TickSpeed() / 5 * Num);
	}

	m_LaserDeaths.push_back(effect);
}

void CGameContext::SnapLaserEffect(int ClientId)
{
	if(m_LaserDeaths.empty())
		return;

	CPlayer *pPlayer = m_apPlayers[ClientId];

	if(!pPlayer || pPlayer->m_HideCosmetics)
		return;

	for(auto it = m_LaserDeaths.begin(); it != m_LaserDeaths.end();)
	{
		const size_t count = std::min({(size_t)it->m_Remaining,
			it->m_aIds.size(), it->m_From.size(), it->m_To.size(), it->m_StartTick.size()});
		for(size_t at = 0; at < count; at++)
		{
			if(Server()->Tick() > it->m_StartTick[at] && Server()->Tick() < it->m_EndTick)
			{
				CNetObj_DDNetLaser *pObj = Server()->SnapNewItem<CNetObj_DDNetLaser>(it->m_aIds[at]);
				if(pObj)
				{
					const vec2 &To = it->m_To[at];
					const vec2 &From = it->m_From[at];
					pObj->m_ToX = (int)To.x;
					pObj->m_ToY = (int)To.y;
					pObj->m_FromX = (int)From.x;
					pObj->m_FromY = (int)From.y;
					pObj->m_StartTick = it->m_EndTick;
					pObj->m_Owner = it->m_Owner;
					pObj->m_Flags = LASERFLAG_NO_PREDICT;
				}
			}
		}
		if(Server()->Tick() > it->m_EndTick)
		{
			for(const auto aIds : it->m_aIds)
				Server()->SnapFreeId(aIds);
			it = m_LaserDeaths.erase(it);
		}
		else
			++it;
	}
}

void CGameContext::Explosion(vec2 Pos, CClientMask Mask)
{
	CNetEvent_Explosion *pEvent = m_Events.Create<CNetEvent_Explosion>(Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

int CGameContext::GetWeaponType(int Weapon)
{
	switch(Weapon)
	{
	case WEAPON_HAMMER:
		return WEAPON_HAMMER;
	case WEAPON_GUN:
		return WEAPON_GUN;
	case WEAPON_SHOTGUN:
		return WEAPON_SHOTGUN;
	case WEAPON_GRENADE:
		return WEAPON_GRENADE;
	case WEAPON_LASER:
		return WEAPON_LASER;
	case WEAPON_NINJA:
		return WEAPON_NINJA;
	case WEAPON_TELEKINESIS:
		return WEAPON_GUN;
	case WEAPON_HEARTGUN:
		return WEAPON_GUN;
	case WEAPON_LIGHTSABER:
		return WEAPON_GUN;
	}
	return Weapon;
}

void CGameContext::SnapDebuggedQuad(int ClientId)
{
	CPlayer *pPlayer = m_apPlayers[ClientId];

	if(!pPlayer || !g_Config.m_SvDebugQuadPos)
		return;

	if(!m_QuadDebugIds.empty())
	{
		int GetId = 0;
		for(const auto *pQuadLayer : Collision()->QuadLayers())
		{
			if(!pQuadLayer)
				continue;

			for(int i = 0; i < pQuadLayer->m_NumQuads; i++)
			{
				vec2 TopLeft;
				Collision()->GetQuadCorners(i, pQuadLayer, 0.0f, &TopLeft);

				CNetObj_DDNetLaser *pObj = Server()->SnapNewItem<CNetObj_DDNetLaser>(m_QuadDebugIds.at(GetId));
				if(pObj)
				{
					pObj->m_ToX = (int)TopLeft.x;
					pObj->m_ToY = (int)TopLeft.y;
					pObj->m_FromX = (int)TopLeft.x;
					pObj->m_FromY = (int)TopLeft.y;
					pObj->m_StartTick = Server()->Tick();
					pObj->m_Owner = -1;
					pObj->m_Flags = LASERFLAG_NO_PREDICT;
				}
				GetId++;
			}
		}
	}
}

void CGameContext::QuadDebugIds(bool Clear)
{
	if(Clear)
	{
		for(const auto *pQuadLayer : Collision()->QuadLayers())
		{
			if(!pQuadLayer)
				continue;

			for(int i = 0; i < pQuadLayer->m_NumQuads; i++)
			{
				m_QuadDebugIds.push_back(Server()->SnapNewId());
			}
		}
	}
	else if(!Clear && !m_QuadDebugIds.empty())
	{
		for(int i = 0; i < (int)m_QuadDebugIds.size(); i++)
		{
			Server()->SnapFreeId(m_QuadDebugIds[i]);
		}
		m_QuadDebugIds.clear();
	}
}

bool CGameContext::IncludedInServerInfo(int ClientId)
{
	bool Included = true;

	if(Server()->DebugDummy(ClientId))
		Included = false;

	CPlayer *pPl = m_apPlayers[ClientId];
	if(pPl)
	{
		if(pPl->m_IncludeServerInfo != -1)
			Included = pPl->m_IncludeServerInfo;
		if(pPl->m_Vanish)
			Included = false;
	}

	return Included;
}
bool CGameContext::AddFakeMessage(const char *pName, const char *pMessage, const char *pSkinName, bool CustomColor, int ColorBody, int ColorFeet)
{
	if(!pName[0] || !pMessage[0])
		return false;

	static int LastUsedId = -1;

	int FreeId = -1;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_apPlayers[i] && Server()->ClientSlotEmpty(i) && LastUsedId != i)
		{
			FreeId = i;
			break;
		}
	}
	if(FreeId == -1)
		return false; // no free visual slot
	LastUsedId = FreeId;
	CFakeSnapPlayer FakeSnap;
	FakeSnap.m_Id = FreeId;
	str_copy(FakeSnap.m_aName, pName);
	FakeSnap.m_aClan[0] = '\0';
	FakeSnap.m_Country = -1;
	str_copy(FakeSnap.m_aSkinName, pSkinName ? pSkinName : "default");
	FakeSnap.m_CustomColors = CustomColor;
	FakeSnap.m_ColorBody = ColorBody;
	FakeSnap.m_ColorFeet = ColorFeet;
	str_copy(FakeSnap.m_aMessage, pMessage);

	m_vFakeSnapPlayers.push_back(FakeSnap);
	return true;
}

const char *GetMapName(const char *pCmd)
{
	const char *pChangeMap = str_find(pCmd, "change_map ");
	if(pChangeMap)
	{
		pChangeMap += str_length("change_map ");
		// Copy until space, semicolon, or end
		static char aMapName[64] = {0};
		int i = 0;
		while(pChangeMap[i] && pChangeMap[i] != ' ' && pChangeMap[i] != ';' && i < (int)sizeof(aMapName) - 1)
		{
			aMapName[i] = pChangeMap[i];
			i++;
		}
		aMapName[i] = 0;
		return aMapName;
	}
	return "";
}

void CGameContext::RandomMapVote()
{
	int Count = 0;
	std::vector<const char *> MapVotes;
	MapVotes.clear();

	for(CVoteOptionServer *pOption = m_pVoteOptionFirst; pOption; pOption = pOption->m_pNext, Count++)
	{
		if(!str_find(pOption->m_aCommand, "change_map "))
			continue;

		if(!str_comp(GetMapName(pOption->m_aCommand), Server()->GetMapName()))
			continue;

		MapVotes.push_back(pOption->m_aCommand);
	}

	std::random_device rd;
	std::uniform_int_distribution<int> dist(0, (int)MapVotes.size() - 1);
	int Random = dist(rd);

	Console()->ExecuteLine(MapVotes.at(Random));
}