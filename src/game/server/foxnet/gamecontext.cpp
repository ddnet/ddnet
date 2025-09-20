#include <game/collision.h>
#include <game/gamecore.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/voting.h>

#include <engine/server/server.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <base/log.h>
#include <base/system.h>
#include <base/vmath.h>

#include <generated/protocol.h>

#include <cstring>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "accounts.h"
#include "entities/pickupdrop.h"
#include "entities/powerup.h"
#include "fontconvert.h"

void CGameContext::FoxNetTick()
{
	m_VoteMenu.Tick();
	HandleEffects();
	PowerUpSpawner();

	if(Server()->Tick() % (Server()->TickSpeed() * 60 * 60 * 12) == 0) // every 12 hours
		m_IsWeekend = IsWeekend();

	if(g_Config.m_SvBanSyncing)
		BanSync();

	// Handle Moving Tiles
	for(const auto *pQuadLayer : Collision()->QuadLayers())
	{
		if(!pQuadLayer)
			continue;

		const int Type = Collision()->GetQuadType(pQuadLayer);
		for(int QuadIndex = 0; QuadIndex < pQuadLayer->m_NumQuads; ++QuadIndex)
		{
			HandleQuads(pQuadLayer, QuadIndex, Type);
		}
	}

	// Set moving tiles time for quads with pos envelopes
	m_Collision.SetTime(m_pController->GetTime());

	// Save all logged in accounts every 15 minutes
	if(Server()->Tick() % (Server()->TickSpeed() * 60 * 15) == 0)
	{
		m_AccountManager.SaveAllAccounts();
	}
}

void CGameContext::FoxNetInit()
{
	m_AccountManager.Init(this, ((CServer *)Server())->DbPool());
	m_VoteMenu.Init(this);
	m_Shop.Init(this);
	m_vPowerups.clear();
	m_PowerUpDelay = Server()->Tick() + Server()->TickSpeed() * 5;

	m_BanSaveDelay = Server()->Tick() + Server()->TickSpeed() * (g_Config.m_SvBanSyncingDelay * 60);

	m_IsWeekend = IsWeekend();

	if(Score())
		Score()->CacheMapInfo();

	if(!m_InitRandomMap)
	{
		if(g_Config.m_SvRandomMapVoteOnStart)
		{
			if(RandomMapVote())
				m_InitRandomMap = true;
		}
		else
			m_InitRandomMap = true;
	}
}

void CGameContext::OnExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, CClientMask Mask)
{
	// deal damage
	CEntity *apDrops[(int)MAX_CLIENTS * (int)NUM_MAX_DROPS];
	float Radius = 135.0f;
	float InnerRadius = 48.0f;
	int NumDrops = m_World.FindEntities(Pos, Radius, apDrops, std::size(apDrops), CGameWorld::ENTTYPE_PICKUPDROP);
	CClientMask TeamMask = Mask;
	for(int i = 0; i < NumDrops; i++)
	{
		auto *pPickup = static_cast<CPickupDrop *>(apDrops[i]);
		if(!pPickup)
			continue;

		vec2 Diff = pPickup->m_Pos - Pos;
		vec2 ForceDir(0, 1);
		float l = length(Diff);
		if(l)
			ForceDir = normalize(Diff);
		l = 1 - std::clamp((l - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
		float Strength;
		if(Owner == -1 || !m_apPlayers[Owner] || !m_apPlayers[Owner]->m_TuneZone)
			Strength = Tuning()->m_ExplosionStrength;
		else
			Strength = TuningList()[m_apPlayers[Owner]->m_TuneZone].m_ExplosionStrength;

		float Dmg = Strength * l;
		if(!(int)Dmg)
			continue;

		if((GetPlayerChar(Owner) ? !GetPlayerChar(Owner)->GrenadeHitDisabled() : g_Config.m_SvHit) || NoDamage)
		{

			if(Owner == -1 && ActivatedTeam != -1 && pPickup->Team() != ActivatedTeam)
				continue;
			// Explode at most once per team
			int PickupTeam = pPickup->Team();

			if((GetPlayerChar(Owner) ? GetPlayerChar(Owner)->GrenadeHitDisabled() : !g_Config.m_SvHit) || NoDamage)
			{
				if(PickupTeam == TEAM_SUPER)
					continue;
				if(!TeamMask.test(PickupTeam))
					continue;
				TeamMask.reset(PickupTeam);
			}

			pPickup->TakeDamage(ForceDir * Dmg * 2);
		}
	}
}

int RandGeometricXP(std::mt19937 &rng, int minXP, int maxXP, double p)
{
	std::geometric_distribution<int> geo(p);
	int range = std::max(0, maxXP - minXP);
	int k = geo(rng);
	if(k > range)
		k = range;
	return minXP + k;
}
void CGameContext::PowerUpSpawner()
{
	if(!g_Config.m_SvSpawnPowerUps)
		return;
	if(m_vPowerups.size() >= 5)
		return;
	if(m_PowerUpDelay > Server()->Tick())
		return;

	const auto RandomPos = GetRandomAccessablePos();
	if(!RandomPos)
	{
		m_PowerUpDelay = Server()->Tick() + Server()->TickSpeed() * 5;
		return;
	}

	static std::mt19937 rng{std::random_device{}()};
	int Xp = RandGeometricXP(rng, 5, 35, 0.35);
	int Lifetime = 120 + Xp * 15; // Lifetime in seconds
	CPowerUp *NewPowerUp = new CPowerUp(&m_World, *RandomPos, Lifetime, Xp);

	m_vPowerups.push_back(NewPowerUp);
	m_PowerUpDelay = Server()->Tick() + Server()->TickSpeed() * 25;
}

void CGameContext::HandleEffects()
{
	// Handle DamageInd effect
	for(auto it = m_vDamageIndEffects.begin(); it != m_vDamageIndEffects.end();)
	{
		if(it->m_Remaining > 0 && Server()->Tick() >= it->m_NextTick)
		{
			int Angles = it->m_vAngles.size() - it->m_Remaining;
			if(Angles < 0)
				Angles = 0;
			int Positions = it->m_vPos.size() - it->m_Remaining;
			if(Positions < 0)
				Positions = 0;

			CreateDamageInd(it->m_vPos.at(Positions), it->m_vAngles.at(Angles), 1, it->m_Mask);

			it->m_Remaining--;
			it->m_NextTick = Server()->Tick() + it->m_Delay;
		}
		if(it->m_Remaining <= 0)
			it = m_vDamageIndEffects.erase(it);
		else
			++it;
	}

	// Sound Effect Handle
	for(auto it = m_vLaserDeaths.begin(); it != m_vLaserDeaths.end();)
	{
		const size_t count = std::min<size_t>((size_t)it->m_Remaining, it->m_vStartTick.size());
		for(size_t at = 0; at < count; at++)
		{
			if(Server()->Tick() < it->m_EndTick)
			{
				if(it->m_vStartTick[at] == Server()->Tick())
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
	for(auto pFakePlayer = m_vFakeSnapPlayers.begin(); pFakePlayer < m_vFakeSnapPlayers.end();)
	{
		if(auto *pClientInfo = Server()->SnapNewItem<CNetObj_ClientInfo>(pFakePlayer->m_ClientId))
		{
			StrToInts(pClientInfo->m_aName, std::size(pClientInfo->m_aName), pFakePlayer->m_aName);
			StrToInts(pClientInfo->m_aClan, std::size(pClientInfo->m_aClan), pFakePlayer->m_aClan);
			pClientInfo->m_Country = pFakePlayer->m_Country;
			StrToInts(pClientInfo->m_aSkin, std::size(pClientInfo->m_aSkin), pFakePlayer->m_aSkinName);
			pClientInfo->m_UseCustomColor = pFakePlayer->m_CustomColors;
			pClientInfo->m_ColorBody = pFakePlayer->m_ColorBody;
			pClientInfo->m_ColorFeet = pFakePlayer->m_ColorFeet;
		}

		if(auto *pPlayerInfo = Server()->SnapNewItem<CNetObj_PlayerInfo>(pFakePlayer->m_ClientId))
		{
			pPlayerInfo->m_Latency = 0;
			pPlayerInfo->m_Score = 0;
			pPlayerInfo->m_Team = TEAM_SPECTATORS;
			pPlayerInfo->m_Local = 0;
			pPlayerInfo->m_ClientId = pFakePlayer->m_ClientId;
		}
		if(pFakePlayer->m_ClientId == -1)
			pFakePlayer = m_vFakeSnapPlayers.erase(pFakePlayer);
		else
			pFakePlayer++;
	}
}

void CGameContext::FoxNetPostGlobalSnap()
{
	for(auto pFakePlayer = m_vFakeSnapPlayers.begin(); pFakePlayer < m_vFakeSnapPlayers.end(); pFakePlayer++)
	{
		const int ClientId = pFakePlayer->m_ClientId;
		if(ClientId < 0 || ClientId >= MAX_CLIENTS)
			continue;

		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 0;
		Msg.m_ClientId = ClientId;
		Msg.m_pMessage = pFakePlayer->m_aMessage;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

		log_info(pFakePlayer->m_Context, "%d:%d:%s: %s", ClientId, Msg.m_Team, pFakePlayer->m_aName, pFakePlayer->m_aMessage);

		pFakePlayer->m_ClientId = -1;
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

	if(m_vChatDetection.empty())
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

	for(const auto &Entry : m_vChatDetection)
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

	if(m_vNameDetection.empty())
		return false;

	const char *ClientName = pName;

	int BanDuration = 0;
	char Reason[64] = "Name Detection Auto Ban";

	std::vector<std::string> FoundStrings;
	std::vector<int> Times;
	FoundStrings.clear();
	Times.clear();

	for(const auto &Entry : m_vNameDetection)
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
	if(Flags & ACC_FLAG_HIDE_POWERUPS)
		pPl->m_HidePowerUps = true;

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
	effect.m_Mask = Mask;
	if(Type >= IND_CLOCKWISE && Type <= IND_COUNTERWISE)
	{
		AngleOffset = 0.80f;
		effect.m_Remaining = 10;
		for(int Remaining = 0; Remaining < effect.m_Remaining; Remaining++)
		{
			if(Type == IND_CLOCKWISE)
				effect.m_vAngles.push_back(Angle - AngleOffset + (Remaining * StarDistance));
			else
				effect.m_vAngles.push_back(Angle + AngleOffset - (Remaining * StarDistance));
		}
		effect.m_vPos.push_back(Pos);
		effect.m_Delay = 1;
		effect.m_NextTick = Server()->Tick();
		m_vDamageIndEffects.push_back(effect);
	}
	else if(Type == IND_INWARD)
	{
		AngleOffset = -0.90f;

		for(int i = 0; i < 2; i++)
		{
			effect.m_Remaining = 5;
			for(int Remaining = 0; Remaining < effect.m_Remaining; Remaining++)
			{
				if(i == 0)
					effect.m_vAngles.push_back(Angle + AngleOffset + (Remaining * StarDistance));
				else
					effect.m_vAngles.push_back(Angle - AngleOffset - (Remaining * StarDistance));
			}
			effect.m_vPos.push_back(Pos);
			effect.m_Delay = 2;
			effect.m_NextTick = Server()->Tick();
			m_vDamageIndEffects.push_back(effect);
		}
	}
	else if(Type == IND_OUTWARD)
	{
		AngleOffset = 0.20f;

		for(int i = 0; i < 2; i++)
		{
			effect.m_Remaining = 5;
			for(int Remaining = 0; Remaining < effect.m_Remaining; Remaining++)
			{
				if(i == 0)
					effect.m_vAngles.push_back(Angle - AngleOffset - (Remaining * StarDistance));
				else
					effect.m_vAngles.push_back(Angle + AngleOffset + (Remaining * StarDistance));
			}
			effect.m_vPos.push_back(Pos);
			effect.m_Delay = 2;
			effect.m_NextTick = Server()->Tick();
			m_vDamageIndEffects.push_back(effect);
		}
	}
	else if(Type == IND_LINE)
	{
		effect.m_Remaining = 6;
		for(int Remaining = 0; Remaining < effect.m_Remaining; Remaining++)
		{
			float Offset = Remaining * 15.0f;
			vec2 CalcPos = Pos - Direction * 25.0f + Direction * Offset;
			effect.m_vPos.push_back(CalcPos);
		}
		effect.m_vAngles.push_back(Angle - AngleOffset);

		effect.m_Delay = 1;
		effect.m_NextTick = Server()->Tick();
		m_vDamageIndEffects.push_back(effect);
	}
	else if(Type == IND_CRISSCROSS)
	{
		effect.m_Remaining = 3;
		for(int Remaining = 0; Remaining < effect.m_Remaining; Remaining++)
		{
			vec2 CalcPos;
			float perpAngle = 0.0f;

			float GetAngle = angle(Direction);
			if(GetAngle < 0.0f)
				GetAngle += 2.0f * pi;

			if(Remaining == 0)
			{
				perpAngle = GetAngle - AngleOffset + pi / 2;
				effect.m_vAngles.push_back(Angle - AngleOffset - 0.85f);
				CalcPos = Pos + vec2(cosf(perpAngle), sinf(perpAngle)) * 25.0f;
			}
			else if(Remaining == 1)
			{
				CalcPos = Pos - Direction * 15.0f;
				effect.m_vAngles.push_back(Angle - AngleOffset);
			}
			else
			{
				perpAngle = GetAngle - AngleOffset - pi / 2;
				effect.m_vAngles.push_back(Angle - AngleOffset + 0.85f);
				CalcPos = Pos + vec2(cosf(perpAngle), sinf(perpAngle)) * 25.0f;
			}

			effect.m_vPos.push_back(CalcPos);
		}

		effect.m_Delay = 1;
		effect.m_NextTick = Server()->Tick();
		m_vDamageIndEffects.push_back(effect);
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

		effect.m_vIds.push_back(Server()->SnapNewId());

		effect.m_vFrom.push_back(Pos);
		effect.m_vTo.push_back(Pos);
		effect.m_vStartTick.push_back(Server()->Tick() + Server()->TickSpeed() / 5 * Num);
	}

	m_vLaserDeaths.push_back(effect);
}

void CGameContext::SnapLaserEffect(int ClientId)
{
	if(m_vLaserDeaths.empty())
		return;

	CPlayer *pPlayer = m_apPlayers[ClientId];

	if(!pPlayer || pPlayer->m_HideCosmetics)
		return;

	for(auto it = m_vLaserDeaths.begin(); it != m_vLaserDeaths.end();)
	{
		const size_t count = std::min({(size_t)it->m_Remaining,
			it->m_vIds.size(), it->m_vFrom.size(), it->m_vTo.size(), it->m_vStartTick.size()});
		for(size_t at = 0; at < count; at++)
		{
			if(Server()->Tick() > it->m_vStartTick[at] && Server()->Tick() < it->m_EndTick)
			{
				CNetObj_DDNetLaser *pObj = Server()->SnapNewItem<CNetObj_DDNetLaser>(it->m_vIds[at]);
				if(pObj)
				{
					const vec2 &To = it->m_vTo[at];
					const vec2 &From = it->m_vFrom[at];
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
			for(const auto aIds : it->m_vIds)
				Server()->SnapFreeId(aIds);
			it = m_vLaserDeaths.erase(it);
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
	case WEAPON_PORTALGUN:
		return WEAPON_LASER;
	}
	return Weapon;
}

void CGameContext::SnapDebuggedQuad(int ClientId)
{
	CPlayer *pPlayer = m_apPlayers[ClientId];

	if(!pPlayer || !g_Config.m_SvDebugQuadPos)
		return;

	if(!m_vQuadDebugIds.empty())
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

				CNetObj_DDNetLaser *pObj = Server()->SnapNewItem<CNetObj_DDNetLaser>(m_vQuadDebugIds.at(GetId));
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
				m_vQuadDebugIds.push_back(Server()->SnapNewId());
			}
		}
	}
	else if(!Clear && !m_vQuadDebugIds.empty())
	{
		for(int i = 0; i < (int)m_vQuadDebugIds.size(); i++)
		{
			Server()->SnapFreeId(m_vQuadDebugIds[i]);
		}
		m_vQuadDebugIds.clear();
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
	FakeSnap.m_ClientId = FreeId;
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

bool CGameContext::RandomMapVote()
{
	int Count = 0;
	std::vector<const char *> MapVotes;

	for(CVoteOptionServer *pOption = m_pVoteOptionFirst; pOption; pOption = pOption->m_pNext, Count++)
	{
		if(!str_find(pOption->m_aCommand, "change_map "))
			continue;

		if(!str_comp(GetMapName(pOption->m_aCommand), Server()->GetMapName()))
			continue;

		MapVotes.push_back(pOption->m_aCommand);
	}

	if(MapVotes.empty())
		return false;

	std::random_device rd;
	std::uniform_int_distribution<int> dist(0, (int)MapVotes.size() - 1);
	int Random = dist(rd);

	Console()->ExecuteLine(MapVotes[Random]);
	return true;
}

void CGameContext::OnPreShutdown()
{
	m_AccountManager.LogoutAllAccountsPort(Server()->Port()); // Save all info before CPlayer is destroyed
}

void CGameContext::HandleQuads(const CMapItemLayerQuads *pQuadLayer, int QuadIndex, int Type)
{
	if(!pQuadLayer)
		return;

	const bool IsFreeze = Type == CCollision::QUADTYPE_FREEZE;
	const bool IsUnFreeze = Type == CCollision::QUADTYPE_UNFREEZE;
	const bool IsDeath = Type == CCollision::QUADTYPE_DEATH;
	const bool IsStopa = Type == CCollision::QUADTYPE_STOPA;
	const bool IsCfrm = Type == CCollision::QUADTYPE_CFRM;

	vec2 TL, TR, BL, BR;
	const int Found = Collision()->GetQuadCorners(QuadIndex, pQuadLayer, 0.0f, &TL, &TR, &BL, &BR);
	if(Found < 0 || Found >= pQuadLayer->m_NumQuads)
		return;

	const vec2 Center = (TL + TR + BL + BR) * 0.25f;
	const float R =
		maximum(maximum(distance(Center, TL), distance(Center, TR)),
			maximum(distance(Center, BL), distance(Center, BR)));

	float TestRadius = 0.0f;
	if(IsDeath)
		TestRadius = 8.0f;
	else if(IsStopa)
		TestRadius = CCharacterCore::PhysicalSize() * 0.5f;

	{
		CEntity *apEnts[MAX_CLIENTS];
		const int Num = m_World.FindEntities(Center, R + CCharacterCore::PhysicalSize(), apEnts, std::size(apEnts), CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; ++i)
		{
			auto *pChar = static_cast<CCharacter *>(apEnts[i]);
			if(!pChar || !pChar->IsAlive())
				continue;
			pChar->m_InQuadFreeze = false;

			if(!Collision()->InsideQuad(pChar->m_Pos, TestRadius, TL, TR, BL, BR))
				continue;

			if(IsFreeze)
			{
				pChar->Freeze();
				pChar->m_InQuadFreeze = true;
			}
			else if(IsUnFreeze && !pChar->Core()->m_IsInFreeze)
			{
				pChar->UnFreeze();
				pChar->m_InQuadFreeze = false;
			}
			else if(IsDeath)
			{
				pChar->Die(pChar->GetPlayer()->GetCid(), WEAPON_WORLD);
			}
			else if(IsStopa)
			{
				HandleQuadStopa(pChar, pQuadLayer, QuadIndex);
			}
			else if(IsCfrm)
			{
				// Teleport to last tele checkpoint out, or spawn if none
				if(pChar->Core()->m_Super || pChar->Core()->m_Invincible)
					continue;

				// First try TeleCheckOuts from current to older checkpoints
				bool Teleported = false;
				for(int k = pChar->m_TeleCheckpoint - 1; k >= 0; --k)
				{
					const auto &outs = Collision()->TeleCheckOuts(k);
					if(!outs.empty())
					{
						const int idx = m_World.m_Core.RandomOr0(outs.size());
						pChar->SetPosition(outs[idx]);
						pChar->ResetVelocity();

						if(!g_Config.m_SvTeleportHoldHook)
						{
							pChar->ResetHook();
							m_World.ReleaseHooked(pChar->GetPlayer()->GetCid());
						}
						Teleported = true;
						break;
					}
				}

				// If none found, teleport to spawn
				if(!Teleported)
				{
					vec2 SpawnPos;
					if(m_pController->CanSpawn(pChar->GetPlayer()->GetTeam(), &SpawnPos, GetDDRaceTeam(pChar->GetPlayer()->GetCid())))
					{
						pChar->SetPosition(SpawnPos);
						pChar->ResetVelocity();

						if(!g_Config.m_SvTeleportHoldHook)
						{
							pChar->ResetHook();
							m_World.ReleaseHooked(pChar->GetPlayer()->GetCid());
						}
					}
				}
			}
		}
	}

	if(!IsUnFreeze)
	{
		CEntity *apDrops[(int)MAX_CLIENTS * (int)NUM_MAX_DROPS];
		const int NumDrops = m_World.FindEntities(Center, R + CCharacterCore::PhysicalSize(), apDrops, std::size(apDrops), CGameWorld::ENTTYPE_PICKUPDROP);
		for(int i = 0; i < NumDrops; ++i)
		{
			CPickupDrop *pPickup = static_cast<CPickupDrop *>(apDrops[i]);
			if(!pPickup)
				continue;
			if(!Collision()->InsideQuad(pPickup->m_Pos, TestRadius, TL, TR, BL, BR))
				continue;

			if(IsFreeze)
			{
				if(g_Config.m_SvDropsInFreezeFloat)
					pPickup->m_InsideFreeze = true;
			}
			else if(IsDeath)
			{
				pPickup->Reset();
			}
			else if(IsStopa)
			{
				HandleQuadStopa(pPickup, pQuadLayer, QuadIndex);
			}
			else if(IsCfrm)
			{
				for(int k = pPickup->TeleCheckpoint() - 1; k >= 0; k--)
				{
					if(!Collision()->TeleCheckOuts(k).empty())
					{
						int TeleOut = m_World.m_Core.RandomOr0(Collision()->TeleCheckOuts(k).size());
						pPickup->ForceSetPos(Collision()->TeleCheckOuts(k)[TeleOut]);
						pPickup->ResetVelocity();

						return;
					}
				}
				vec2 SpawnPos;
				if(m_pController->CanSpawn(0, &SpawnPos, pPickup->Team()))
				{
					pPickup->ForceSetPos(SpawnPos);
					pPickup->ResetVelocity();
				}
			}
		}
	}
}

void CGameContext::HandleQuadStopa(CEntity *pEntity, const CMapItemLayerQuads *pQuadLayer, int QuadIndex)
{
	if(!pQuadLayer)
		return;

	vec2 TL, TR, BL, BR;
	int Found = Collision()->GetQuadCorners(QuadIndex, pQuadLayer, 0.0f, &TL, &TR, &BL, &BR);
	if(Found < 0 || Found >= pQuadLayer->m_NumQuads)
		return;

	const float R = pEntity->GetProximityRadius() * 0.5f;
	const vec2 P = pEntity->m_Pos;

	const vec2 aA[4] = {TL, TR, BR, BL};
	const vec2 aB[4] = {TR, BR, BL, TL};

	float MinPenetration = std::numeric_limits<float>::infinity();
	vec2 BestInwardNormal = vec2(0.0f, 0.0f);
	int BestEdgeIdx = -1;
	vec2 BestEdgeVec = vec2(0.0f, 0.0f);

	for(int i = 0; i < 4; ++i)
	{
		vec2 E = aB[i] - aA[i];
		float Elen2 = dot(E, E);
		if(Elen2 <= 1e-6f)
			continue;

		vec2 N_in = normalize(vec2(-E.y, E.x));
		float d = dot(P - aA[i], N_in);
		float penetration = d + R;

		if(penetration < MinPenetration)
		{
			MinPenetration = penetration;
			BestInwardNormal = N_in;
			BestEdgeIdx = i;
			BestEdgeVec = E;
		}
	}

	if(MinPenetration == std::numeric_limits<float>::infinity())
		return;

	if(MinPenetration > 0.0f)
	{
		const float Epsilon = 0.0f;
		vec2 MTV = -BestInwardNormal * (MinPenetration + Epsilon);

		auto CanPlace = [&](const vec2 &Pos) {
			return !Collision()->TestBox(Pos, vec2(pEntity->GetProximityRadius(), pEntity->GetProximityRadius()));
		};

		auto MoveAxis = [&](vec2 &Pos, const vec2 &Delta) {
			if(Delta.x == 0.0f && Delta.y == 0.0f)
				return vec2(0.f, 0.f);

			vec2 Target = Pos + Delta;
			if(CanPlace(Target))
			{
				Pos = Target;
				return Delta;
			}

			float lo = 0.0f;
			float hi = 1.0f;
			for(int i = 0; i < 10; ++i)
			{
				float mid = (lo + hi) * 0.5f;
				vec2 MidPos = Pos + Delta * mid;
				if(CanPlace(MidPos))
					lo = mid;
				else
					hi = mid;
			}
			if(lo > 0.0f)
			{
				vec2 Applied = Delta * lo;
				Pos += Applied;
				return Applied;
			}
			return vec2(0.0f, 0.0f);
		};

		vec2 NewPos = pEntity->m_Pos;

		vec2 AppliedX = MoveAxis(NewPos, vec2(MTV.x, 0.0f));
		vec2 AppliedY = MoveAxis(NewPos, vec2(0.0f, MTV.y));

		vec2 vel = pEntity->GetVelocity();
		pEntity->ForceSetPos(NewPos);

		float vIn = dot(vel, BestInwardNormal);
		if(vIn > 0.0f)
			pEntity->SetRawVelocity(vel - BestInwardNormal * vIn);

		if(AppliedX.x == 0.0f && MTV.x != 0.0f)
			pEntity->SetRawVelocity(vec2(0.0f, vel.y));
		if(AppliedY.y == 0.0f && MTV.y != 0.0f)
			pEntity->SetRawVelocity(vec2(vel.x, 0.0f));

		if(pEntity->GetObjType() == CGameWorld::ENTTYPE_CHARACTER)
		{
			if(g_Config.m_SvQStopaGivesDj && BestEdgeIdx >= 0)
			{
				const float NormalThresh = 0.35f;
				const float SlopeThresh = 0.60f;

				float edgeLen = length(BestEdgeVec);
				float edgeSlope = edgeLen > 1e-6f ? absolute(BestEdgeVec.y) / edgeLen : 1.0f;
				bool IsFloorNormal = (BestInwardNormal.y >= NormalThresh);
				bool IsFlatEnough = (edgeSlope <= SlopeThresh);
				bool PushedUp = (AppliedY.y < 0.0f);
				bool WasFallingOrRest = (vel.y >= 0.0f);

				if(IsFloorNormal && IsFlatEnough && PushedUp && WasFallingOrRest)
				{
					static_cast<CCharacter *>(pEntity)->ResetJumps();
				}
			}
		}
	}
}

std::optional<vec2> CGameContext::GetRandomAccessablePos()
{
	const auto Dist2 = [](const vec2 &a, const vec2 &b) {
		const float dx = a.x - b.x;
		const float dy = a.y - b.y;
		return dx * dx + dy * dy;
	};

	constexpr float MinPlayerDist = 640.0f; // 20 tiles

	for(int Tries = 0; Tries < 16; ++Tries)
	{
		vec2 Pos;
		if(!Collision()->TryPickCachedCandidate(Pos))
			return std::nullopt;

		CEntity *apEnts[64] = {0};
		const int num = m_World.FindEntities(Pos, MinPlayerDist, apEnts, std::size(apEnts), CGameWorld::ENTTYPE_CHARACTER);
		bool NearPlayer = false;
		for(int i = 0; i < num; ++i)
		{
			auto *pChr = static_cast<CCharacter *>(apEnts[i]);
			if(pChr && pChr->IsAlive())
			{
				NearPlayer = true;
				break;
			}
		}
		if(NearPlayer)
			continue;

		return Pos;
	}

	float BestScore = -1.0f;
	vec2 BestPos;
	for(int k = 0; k < 32; ++k)
	{
		vec2 Pos;
		if(!Collision()->TryPickCachedCandidate(Pos))
			break;

		float MinDist2 = std::numeric_limits<float>::infinity();
		CEntity *apEnts[128] = {0};
		const int Num = m_World.FindEntities(Pos, 1024.0f, apEnts, std::size(apEnts), CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; ++i)
		{
			auto *pChr = static_cast<CCharacter *>(apEnts[i]);
			if(!pChr || !pChr->IsAlive())
				continue;
			MinDist2 = std::min(MinDist2, Dist2(pChr->m_Pos, Pos));
			if(MinDist2 == 0.0f)
				break;
		}
		if(MinDist2 > BestScore)
		{
			BestScore = MinDist2;
			BestPos = Pos;
		}
	}
	if(BestScore >= 0.0f)
		return BestPos;

	return std::nullopt;
}

void CGameContext::CollectedPowerup(int ClientId, int XP) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	CPlayer *pPlayer = m_apPlayers[ClientId];
	if(!pPlayer)
		return;

	if(pPlayer->m_HidePowerUps)
	{
		pPlayer->GiveXP(XP); // GiveXP has a logged in check
		return;
	}

	if(!pPlayer->Acc()->m_LoggedIn)
	{
		SendChatTarget(ClientId, "You need to be logged in to collect Powerups");
		SendChatTarget(ClientId, "/register <name> <pw> <pw>");
		return;
	}

	pPlayer->GiveXP(XP, "for collecting a PowerUp!");
}