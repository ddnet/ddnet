#include <game/generated/protocol.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <base/system.h>
#include <string>

#include "cosmetics/dot_trail.h"
#include "cosmetics/epic_circle.h"
#include "cosmetics/firework.h"
#include "cosmetics/flyingpoint.h"
#include "cosmetics/head_powerup.h"
#include "cosmetics/heart_hat.h"
#include "cosmetics/lovely.h"
#include "cosmetics/pickup_pet.h"
#include "cosmetics/rotating_ball.h"
#include "cosmetics/staff_ind.h"

#include "accounts.h"
#include "shop.h"

CAccountSession *CPlayer::Acc() { return &GameServer()->m_Account[m_ClientId]; }

void CPlayer::FoxNetTick()
{
	RainbowTick();

	if(Acc()->m_LoggedIn)
	{
		if(!IsAfk() && (Server()->Tick() - Acc()->m_LoginTick) % (Server()->TickSpeed() * 60) == 0 && Acc()->m_LoginTick != Server()->Tick())
		{
			GivePlaytime(1);
			int XP = 1;
			GiveXP(XP, "");
		}
	}
}

void CPlayer::FoxNetReset()
{
	m_IncludeServerInfo = -1;
	m_HideCosmetics = false;

	m_ExtraPing = false;
	m_Obfuscated = false;
	m_Vanish = false;
	m_IgnoreGamelayer = false;
	m_TelekinesisImmunity = false;
	m_SpiderHook = false;

	m_Cosmetics = CCosmetics();
}

void CPlayer::GivePlaytime(int Amount)
{
	if(!Acc()->m_LoggedIn)
		return;

	Acc()->m_Playtime++;
	if(Acc()->m_Playtime % 100 == 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "for reaching %d Minutes of Playtime!", Acc()->m_Playtime);
		GiveMoney(g_Config.m_SvPlaytimeMoney, aBuf);
	}
}

void CPlayer::GiveXP(int64_t Amount, const char *pMessage)
{
	if(!Acc()->m_LoggedIn)
		return;

	if(IsWeekend())
		Amount *= 2;

	Acc()->m_XP += Amount;

	char aBuf[256];

	if(pMessage[0])
	{
		str_format(aBuf, sizeof(aBuf), "+%lld XP %s%s", Amount, pMessage, "");
		GameServer()->SendChatTarget(m_ClientId, aBuf);
	}

	CheckLevelUp(Amount);
}

bool CPlayer::CheckLevelUp(int64_t Amount, bool Silent)
{
	bool LeveledUp = false;
	char aBuf[256];

	// ╔╦╦╦╦═════════════╗
	// ╚╩╩╩╩═════════════╝

	int ClampedLevel = std::clamp((int)Acc()->m_Level, 0, 4);
	int NeededXp = GameServer()->m_AccountManager.m_NeededXp[ClampedLevel];

	while(Acc()->m_XP >= NeededXp)
	{
		Acc()->m_Level++;
		Acc()->m_XP -= NeededXp;

		GiveMoney(g_Config.m_SvLevelUpMoney);
		LeveledUp = true;
	}
	if(LeveledUp && !Silent)
	{
		str_format(aBuf, sizeof(aBuf), "You are now level %d!", Acc()->m_Level);
		GameServer()->SendChatTarget(m_ClientId, aBuf);
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(i != m_ClientId && GameServer()->GetPlayerChar(i) && i < g_Config.m_SvMaxClients)
			{
				// send a level up message to everyone except for the player who leveled up
				str_format(aBuf, sizeof(aBuf), "%s is not level %d!", Server()->ClientName(m_ClientId), Acc()->m_Level);
				GameServer()->SendChatTarget(i, aBuf);
			}
		}
		GameServer()->m_AccountManager.SaveAccountsInfo(m_ClientId, *Acc());
		GameServer()->CreateBirthdayEffect(GetCharacter()->GetPos(), GetCharacter()->TeamMask());
	}

	return LeveledUp;
}

void CPlayer::GiveMoney(int64_t Amount, const char *pMessage)
{
	if(!Acc()->m_LoggedIn)
		return;

	if(IsWeekend())
		Amount *= 2.0f;

	Acc()->m_Money += Amount;

	char aBuf[256];

	if(pMessage[0])
	{
		str_format(aBuf, sizeof(aBuf), "+%lld %s %s", Amount, g_Config.m_SvCurrencyName, pMessage);
		GameServer()->SendChatTarget(m_ClientId, aBuf);
	}

	GameServer()->m_AccountManager.SaveAccountsInfo(m_ClientId, *Acc());
}

bool CPlayer::OwnsItem(const char *pItemName)
{
	if(!Acc()->m_LoggedIn)
		return false;

	const char *pInventory = Acc()->m_Inventory;

	char pItem[64];
	str_copy(pItem, pItemName);
	if(str_comp(GameServer()->m_Shop.NameToShortcut(pItem), "") != 0)
		str_copy(pItem, GameServer()->m_Shop.NameToShortcut(pItem));

	bool ItemExists = false;
	for(const char *pShortcut : ItemShortcuts)
	{
		if(!str_comp_nocase(pItem, pShortcut))
		{
			ItemExists = true;
			break;
		}
	}

	if(!ItemExists)
		return false;

	char FindItem[256];
	// Items are stored with a space at the end to avoid partial matches
	str_format(FindItem, sizeof(FindItem), "%s ", pItem);

	if(str_find_nocase(pInventory, FindItem))
		return true;

	return false;
}

int CPlayer::GetItemToggle(const char *pItemName) const
{
	int Value = -1;

	char pItem[64];
	str_copy(pItem, pItemName);
	if(str_comp(GameServer()->m_Shop.NameToShortcut(pItem), "") != 0)
		str_copy(pItem, GameServer()->m_Shop.NameToShortcut(pItem));

	if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_SPARKLE]))
		Value = (int)!m_Cosmetics.m_Sparkle;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_HEARTHAT]))
		Value = (int)!m_Cosmetics.m_HeartHat;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_INVERSEAIM]))
		Value = (int)!m_Cosmetics.m_InverseAim;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_LOVELY]))
		Value = (int)!m_Cosmetics.m_Lovely;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_ROTATINGBALL]))
		Value = (int)!m_Cosmetics.m_RotatingBall;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_EPICCIRCLE]))
		Value = (int)!m_Cosmetics.m_EpicCircle;
	// else if(!str_comp_nocase(pItem, ItemShortcuts[OTHER_BLOODY]))
	//	Value = (int)!m_Cosmetics.m_Bloody;

	else if(!str_comp_nocase(pItem, ItemShortcuts[C_RAINBOW_FEET]))
		Value = (int)!m_Cosmetics.m_RainbowFeet;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_RAINBOW_BODY]))
		Value = (int)!m_Cosmetics.m_RainbowBody;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_RAINBOW_HOOK]))
		Value = (int)m_Cosmetics.m_HookPower == HOOK_RAINBOW ? HOOK_NORMAL : HOOK_RAINBOW;

	else if(!str_comp_nocase(pItem, ItemShortcuts[C_GUN_EMOTICON]))
		Value = (int)m_Cosmetics.m_EmoticonGun;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_GUN_CONFETTI]))
		Value = (int)!m_Cosmetics.m_ConfettiGun;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_GUN_LASER]))
		Value = (int)!m_Cosmetics.m_PhaseGun;

	else if(!str_comp_nocase(pItem, ItemShortcuts[C_TRAIL_STAR]))
		Value = (int)m_Cosmetics.m_Trail == TRAIL_STAR ? TRAIL_NONE : TRAIL_STAR;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_TRAIL_DOT]))
		Value = (int)m_Cosmetics.m_Trail == TRAIL_DOT ? TRAIL_NONE : TRAIL_DOT;

	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_CLOCKWISE]))
		Value = (int)m_Cosmetics.m_DamageIndType == IND_CLOCKWISE ? IND_NONE : IND_CLOCKWISE;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_COUNTERCLOCKWISE]))
		Value = (int)m_Cosmetics.m_DamageIndType == IND_COUNTERWISE ? IND_NONE : IND_COUNTERWISE;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_INWARD_TURNING]))
		Value = (int)m_Cosmetics.m_DamageIndType == IND_INWARD ? IND_NONE : IND_INWARD;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_OUTWARD_TURNING]))
		Value = (int)m_Cosmetics.m_DamageIndType == IND_OUTWARD ? IND_NONE : IND_OUTWARD;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_LINE]))
		Value = (int)m_Cosmetics.m_DamageIndType == IND_LINE ? IND_NONE : IND_LINE;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_CRISSCROSS]))
		Value = (int)m_Cosmetics.m_DamageIndType == IND_CRISSCROSS ? IND_NONE : IND_CRISSCROSS;

	else if(!str_comp_nocase(pItem, ItemShortcuts[C_DEATH_EXPLOSIVE]))
		Value = (int)m_Cosmetics.m_DeathEffect == DEATH_EXPLOSION ? DEATH_NONE : DEATH_EXPLOSION;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_DEATH_HAMMERHIT]))
		Value = (int)m_Cosmetics.m_DeathEffect == DEATH_HAMMERHIT ? DEATH_NONE : DEATH_HAMMERHIT;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_DEATH_INDICATOR]))
		Value = (int)m_Cosmetics.m_DeathEffect == DEATH_DAMAGEIND ? DEATH_NONE : DEATH_DAMAGEIND;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_DEATH_LASER]))
		Value = (int)m_Cosmetics.m_DeathEffect == DEATH_LASER ? DEATH_NONE : DEATH_LASER;

	return Value;
}

bool CPlayer::ItemEnabled(const char *pItemName) const
{
	int Value = false;

	char pItem[64];
	str_copy(pItem, pItemName);
	if(str_comp(GameServer()->m_Shop.NameToShortcut(pItem), "") != 0)
		str_copy(pItem, GameServer()->m_Shop.NameToShortcut(pItem));

	if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_SPARKLE]))
		Value = m_Cosmetics.m_Sparkle;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_HEARTHAT]))
		Value = m_Cosmetics.m_HeartHat;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_INVERSEAIM]))
		Value = m_Cosmetics.m_InverseAim;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_LOVELY]))
		Value = m_Cosmetics.m_Lovely;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_ROTATINGBALL]))
		Value = m_Cosmetics.m_RotatingBall;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_EPICCIRCLE]))
		Value = m_Cosmetics.m_EpicCircle;
	// else if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_BLOODY]))
	//	Value = m_Cosmetics.m_Bloody;

	else if(!str_comp_nocase(pItem, ItemShortcuts[C_RAINBOW_FEET]))
		Value = m_Cosmetics.m_RainbowFeet;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_RAINBOW_BODY]))
		Value = m_Cosmetics.m_RainbowBody;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_RAINBOW_HOOK]))
		Value = m_Cosmetics.m_HookPower == HOOK_RAINBOW ? true : false;

	else if(!str_comp_nocase(pItem, ItemShortcuts[C_GUN_EMOTICON]))
		Value = m_Cosmetics.m_EmoticonGun;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_GUN_CONFETTI]))
		Value = m_Cosmetics.m_ConfettiGun;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_GUN_LASER]))
		Value = m_Cosmetics.m_PhaseGun;

	else if(!str_comp_nocase(pItem, ItemShortcuts[C_TRAIL_STAR]))
		Value = m_Cosmetics.m_Trail == TRAIL_STAR ? true : false;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_TRAIL_DOT]))
		Value = m_Cosmetics.m_Trail == TRAIL_DOT ? true : false;

	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_CLOCKWISE]))
		Value = m_Cosmetics.m_DamageIndType == IND_CLOCKWISE ? true : false;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_COUNTERCLOCKWISE]))
		Value = m_Cosmetics.m_DamageIndType == IND_COUNTERWISE ? true : false;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_INWARD_TURNING]))
		Value = m_Cosmetics.m_DamageIndType == IND_INWARD ? true : false;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_OUTWARD_TURNING]))
		Value = m_Cosmetics.m_DamageIndType == IND_OUTWARD ? true : false;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_LINE]))
		Value = m_Cosmetics.m_DamageIndType == IND_LINE ? true : false;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_CRISSCROSS]))
		Value = m_Cosmetics.m_DamageIndType == IND_CRISSCROSS ? true : false;

	else if(!str_comp_nocase(pItem, ItemShortcuts[C_DEATH_EXPLOSIVE]))
		Value = m_Cosmetics.m_DeathEffect == DEATH_EXPLOSION ? true : false;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_DEATH_HAMMERHIT]))
		Value = m_Cosmetics.m_DeathEffect == DEATH_HAMMERHIT ? true : false;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_DEATH_INDICATOR]))
		Value = m_Cosmetics.m_DeathEffect == DEATH_DAMAGEIND ? true : false;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_DEATH_LASER]))
		Value = m_Cosmetics.m_DeathEffect == DEATH_LASER ? true : false;

	return Value;
}

bool CPlayer::ToggleItem(const char *pItemName, int Set, bool IgnoreAccount)
{
	if(!Acc()->m_LoggedIn && !IgnoreAccount)
		return false;
	char pItem[64];
	str_copy(pItem, pItemName);
	if(str_comp(GameServer()->m_Shop.NameToShortcut(pItem), "") != 0)
		str_copy(pItem, GameServer()->m_Shop.NameToShortcut(pItem));

	if(!OwnsItem(pItem) && !IgnoreAccount)
		return false;

	const int pValue = GetItemToggle(pItem);
	if(pValue == -1 && Set == -1)
		return false;

	if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_SPARKLE]))
		m_Cosmetics.m_Sparkle = pValue;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_HEARTHAT]))
		SetHeartHat(pValue);
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_INVERSEAIM]))
		m_Cosmetics.m_InverseAim = pValue;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_LOVELY]))
		SetLovely(pValue);
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_ROTATINGBALL]))
		SetRotatingBall(pValue);
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_OTHER_EPICCIRCLE]))
		SetEpicCircle(pValue);
	// else if(!str_comp_nocase(pItem, ItemShortcuts[OTHER_BLOODY]))
	//	m_Cosmetics.m_Bloody = pValue;

	else if(!str_comp_nocase(pItem, ItemShortcuts[C_RAINBOW_FEET]))
		m_Cosmetics.m_RainbowFeet = pValue;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_RAINBOW_BODY]))
		m_Cosmetics.m_RainbowBody = pValue;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_RAINBOW_HOOK]))
		m_Cosmetics.m_HookPower = pValue;

	else if(!str_comp_nocase(pItem, ItemShortcuts[C_GUN_EMOTICON]))
		m_Cosmetics.m_EmoticonGun = Set; // Special

	else if(!str_comp_nocase(pItem, ItemShortcuts[C_GUN_CONFETTI]))
		m_Cosmetics.m_ConfettiGun = pValue;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_GUN_LASER]))
		m_Cosmetics.m_PhaseGun = pValue;

	else if(!str_comp_nocase(pItem, ItemShortcuts[C_TRAIL_STAR]))
		SetTrail(pValue);
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_TRAIL_DOT]))
		SetTrail(pValue);

	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_CLOCKWISE]))
		m_Cosmetics.m_DamageIndType = pValue;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_COUNTERCLOCKWISE]))
		m_Cosmetics.m_DamageIndType = pValue;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_INWARD_TURNING]))
		m_Cosmetics.m_DamageIndType = pValue;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_OUTWARD_TURNING]))
		m_Cosmetics.m_DamageIndType = pValue;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_LINE]))
		m_Cosmetics.m_DamageIndType = pValue;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_INDICATOR_CRISSCROSS]))
		m_Cosmetics.m_DamageIndType = pValue;

	else if(!str_comp_nocase(pItem, ItemShortcuts[C_DEATH_EXPLOSIVE]))
		m_Cosmetics.m_DeathEffect = pValue;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_DEATH_HAMMERHIT]))
		m_Cosmetics.m_DeathEffect = pValue;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_DEATH_INDICATOR]))
		m_Cosmetics.m_DeathEffect = pValue;
	else if(!str_comp_nocase(pItem, ItemShortcuts[C_DEATH_LASER]))
		m_Cosmetics.m_DeathEffect = pValue;

	UpdateActiveItems();

	return true;
}

void CPlayer::UpdateActiveItems()
{
	char pActiveItems[1024] = "";

	int Index = 0;
	char intBuf[12];
	for(const char *pShortcut : ItemShortcuts)
	{
		if(ItemEnabled(pShortcut))
		{
			if(pActiveItems[0] != '\0')
				str_append(pActiveItems, " ");
			str_append(pActiveItems, pShortcut);
			if(Index == C_GUN_EMOTICON)
			{
				str_format(intBuf, sizeof(intBuf), "=%d", m_Cosmetics.m_EmoticonGun);
				str_append(pActiveItems, intBuf);
			}
		}
		Index++;
	}
	str_copy(Acc()->m_LastActiveItems, pActiveItems, sizeof(Acc()->m_LastActiveItems));
}

void CPlayer::RainbowSnap(int SnappingClient, CNetObj_ClientInfo *pClientInfo)
{
	if(!GetCharacter() || (!m_Cosmetics.m_RainbowBody && !m_Cosmetics.m_RainbowFeet && GetCharacter()->GetPowerHooked() != HOOK_RAINBOW))
		return;

	if(GetCharacter()->GetPowerHooked() == HOOK_RAINBOW)
		GetCharacter()->m_IsRainbowHooked = true;

	int BaseColor = m_RainbowColor * 0x010000;
	int Color = 0xff32;

	// only send rainbow updates to people close to you, to reduce network traffic
	if(GameServer()->m_apPlayers[SnappingClient] && !GetCharacter()->NetworkClipped(SnappingClient))
	{
		if(!GameServer()->m_apPlayers[SnappingClient]->m_HideCosmetics)
		{
			pClientInfo->m_UseCustomColor = 1;
			if(m_Cosmetics.m_RainbowBody || GetCharacter()->m_IsRainbowHooked)
				pClientInfo->m_ColorBody = BaseColor + Color;
			if(m_Cosmetics.m_RainbowFeet || GetCharacter()->m_IsRainbowHooked)
				pClientInfo->m_ColorFeet = BaseColor + Color;
		}
	}
}

void CPlayer::RainbowTick()
{
	if(!GetCharacter() || (!m_Cosmetics.m_RainbowBody && !m_Cosmetics.m_RainbowFeet && GetCharacter()->GetPowerHooked() != HOOK_RAINBOW))
		return;

	if(m_Cosmetics.m_RainbowSpeed < 1)
		m_Cosmetics.m_RainbowSpeed = 1;

	if(Server()->Tick() % 2 == 1)
		m_RainbowColor = (m_RainbowColor + m_Cosmetics.m_RainbowSpeed) % 256;
}

void CPlayer::OverrideName(int SnappingClient, CNetObj_ClientInfo *pClientInfo)
{
	if(m_Obfuscated)
	{
		constexpr int maxBytes = sizeof(pClientInfo->m_aName);
		std::string obfStr = RandomUnicode(maxBytes / 3);
		if(obfStr.size() >= maxBytes)
			obfStr.resize(maxBytes - 1);
		const char *pObf = obfStr.c_str();

		StrToInts(pClientInfo->m_aName, std::size(pClientInfo->m_aName), pObf);
		StrToInts(pClientInfo->m_aClan, std::size(pClientInfo->m_aClan), " ");
	}

	if(!GetCharacter())
		return;

	if(GetCharacter()->m_InSnake)
	{
		StrToInts(pClientInfo->m_aName, std::size(pClientInfo->m_aName), " ");
		StrToInts(pClientInfo->m_aClan, std::size(pClientInfo->m_aClan), " ");
	}
}
void CPlayer::SetRainbowBody(bool Active)
{
	m_Cosmetics.m_RainbowBody = Active;
}

void CPlayer::SetRainbowFeet(bool Active)
{
	m_Cosmetics.m_RainbowFeet = Active;
}

void CPlayer::SetSparkle(bool Active)
{
	m_Cosmetics.m_Sparkle = Active;
}

void CPlayer::SetInverseAim(bool Active)
{
	m_Cosmetics.m_InverseAim = Active;
}

void CPlayer::SetBloody(bool Active)
{
	m_Cosmetics.m_Bloody = Active;
	m_Cosmetics.m_StrongBloody = false;
}

void CPlayer::SetRotatingBall(bool Active)
{
	if(m_Cosmetics.m_RotatingBall == Active)
		return;

	m_Cosmetics.m_RotatingBall = Active;
	if(GetCharacter() && m_Cosmetics.m_RotatingBall)
		new CRotatingBall(&GameServer()->m_World, GetCid(), vec2(0, 0));
}

void CPlayer::SetEpicCircle(bool Active)
{
	if(m_Cosmetics.m_EpicCircle == Active)
		return;

	m_Cosmetics.m_EpicCircle = Active;

	if(GetCharacter() && Active)
		new CEpicCircle(&GameServer()->m_World, GetCid(), vec2(0, 0));
}

void CPlayer::SetLovely(bool Active)
{
	if(m_Cosmetics.m_Lovely == Active)
		return;

	m_Cosmetics.m_Lovely = Active;
	if(Active)
		new CLovely(&GameServer()->m_World, GetCid(), vec2(0, 0));
}

void CPlayer::SetTrail(int Type)
{
	if(m_Cosmetics.m_Trail == Type)
		return;

	m_Cosmetics.m_Trail = Type;
	if(Type == TRAIL_DOT)
		new CDotTrail(&GameServer()->m_World, GetCid(), vec2(0, 0));
}

void CPlayer::SetPickupPet(bool Active)
{
	if(m_Cosmetics.m_PickupPet == Active)
		return;
	m_Cosmetics.m_PickupPet = Active;
	if(Active)
		m_pPickupPet = new CPickupPet(&GameServer()->m_World, GetCid(), vec2(0, 0));
}

void CPlayer::SetStaffInd(bool Active)
{
	if(m_Cosmetics.m_StaffInd == Active)
		return;
	m_Cosmetics.m_StaffInd = Active;
	if(Active)
		new CStaffInd(&GameServer()->m_World, GetCid(), vec2(0, 0));
}

void CPlayer::SetHeartHat(bool Active)
{
	if(m_Cosmetics.m_HeartHat == Active)
		return;
	m_Cosmetics.m_HeartHat = Active;
	if(Active)
		new CHeartHat(&GameServer()->m_World, GetCid());
}

void CPlayer::SetDamageIndType(int Type)
{
	m_Cosmetics.m_DamageIndType = Type;
}

void CPlayer::SetDeathEffect(int Type)
{
	m_Cosmetics.m_DeathEffect = Type;
}

void CPlayer::SetStrongBloody(bool Active)
{
	m_Cosmetics.m_StrongBloody = Active;
	m_Cosmetics.m_Bloody = false;
}

void CPlayer::HookPower(int Extra)
{
	if(m_Cosmetics.m_HookPower == HOOK_NORMAL && Extra == HOOK_NORMAL)
		return;
	m_Cosmetics.m_HookPower = Extra;
}

void CPlayer::SetEmoticonGun(int EmoteType)
{
	m_Cosmetics.m_EmoticonGun = EmoteType;
}

void CPlayer::SetPhaseGun(bool Active)
{
	m_Cosmetics.m_PhaseGun = Active;
}

void CPlayer::SetConfettiGun(bool Set)
{
	m_Cosmetics.m_ConfettiGun = Set;
}

void CPlayer::SetInvisible(bool Active)
{
	m_Invisible = Active;
}

void CPlayer::SetExtraPing(int Ping)
{
	m_ExtraPing = Ping;
}

void CPlayer::SetIgnoreGameLayer(bool Set)
{
	m_IgnoreGamelayer = Set;
}

void CPlayer::SetObfuscated(bool Set)
{
	m_Obfuscated = Set;
}

void CPlayer::SetTelekinesisImmunity(bool Active)
{
	m_TelekinesisImmunity = Active;
}

void CPlayer::SetHideCosmetics(bool Set)
{
	m_HideCosmetics = Set;
	if(!Acc()->m_LoggedIn)
		return;
	if(Set)
	{
		Acc()->m_Flags |= ACC_FLAG_HIDE_COSMETICS;
		GameServer()->SendChatTarget(m_ClientId, "Cosmetics will be hidden");
		DisableAllCosmetics();
	}
	else
	{
		Acc()->m_Flags &= ~ACC_FLAG_HIDE_COSMETICS;
		GameServer()->SendChatTarget(m_ClientId, "Cosmetics will show");
	}
}

static const char *GetAbilityName(int Type)
{
	switch(Type)
	{
	case ABILITY_HEART:
		return "Heart";
	case ABILITY_SHIELD:
		return "Shield";
	case ABILITY_FIREWORK:
		return "Firework";
	case ABILITY_TELEKINESIS:
		return "Telekinesis";
	}
	return "Unknown";
}

void CPlayer::SetAbility(int Type)
{
	if(m_Cosmetics.m_Ability == Type)
		return;

	m_Cosmetics.m_Ability = Type;

	int ClientId = GetCid();

	if(m_Cosmetics.m_Ability <= ABILITY_NONE)
		return;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Ability set to %s", GetAbilityName(m_Cosmetics.m_Ability));
	GameServer()->SendChatTarget(ClientId, aBuf);

	if(ABILITY_HEART <= Type && Type <= ABILITY_SHIELD)
	{
		GameServer()->SendChatTarget(ClientId, "Use f3 (Vote Yes) to use ability 1");
		GameServer()->SendChatTarget(ClientId, "If there is one, Use f4 (Vote No) to use ability 2");
	}
	if(Type == ABILITY_FIREWORK)
		GameServer()->SendChatTarget(ClientId, "If there is one, Use f4 (Vote No) to use your Ability");
	if(Type == ABILITY_TELEKINESIS)
		GameServer()->SendChatTarget(ClientId, "Use f3 (Vote Yes) to use your Ability");
}

void CPlayer::DisableAllCosmetics()
{
	SetInverseAim(false);
	SetLovely(false);
	SetBloody(false);
	HookPower(false);
	SetSparkle(false);
	SetEpicCircle(false);

	SetRotatingBall(false);
	SetHeartHat(false);

	SetConfettiGun(false);
	SetEmoticonGun(0);
	SetPhaseGun(false);

	SetRainbowBody(false);
	SetRainbowFeet(false);

	SetTrail(false);

	// Not Available Normally
	SetStaffInd(false);
	SetPickupPet(false);
}