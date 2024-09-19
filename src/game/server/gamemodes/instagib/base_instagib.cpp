#include <base/system.h>
#include <engine/shared/protocol.h>
#include <game/generated/protocol.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "base_instagib.h"

CGameControllerInstagib::CGameControllerInstagib(class CGameContext *pGameServer) :
	CGameControllerPvp(pGameServer)
{
	m_GameFlags = GAMEFLAG_TEAMS | GAMEFLAG_FLAGS;
	m_AllowSkinChange = true;
}

CGameControllerInstagib::~CGameControllerInstagib() = default;

bool CGameControllerInstagib::OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character)
{
	if(From == Character.GetPlayer()->GetCid())
	{
		// Give back ammo on grenade self push
		// Only if not infinite ammo and activated
		if(Weapon == WEAPON_GRENADE && g_Config.m_SvGrenadeAmmoRegen && g_Config.m_SvGrenadeAmmoRegenSpeedNade)
		{
			Character.SetWeaponAmmo(WEAPON_GRENADE, minimum(Character.GetCore().m_aWeapons[WEAPON_GRENADE].m_Ammo + 1, g_Config.m_SvGrenadeAmmoRegenNum));
		}

		// no pain eyes when hitting our self with laser
		if(Weapon == WEAPON_LASER)
			Dmg = 0;

		// no self damage
		//
		// self damage counts as boosting
		// so the hit/misses rate should not be affected
		//
		// yes this means that grenade boost kills
		// can get you a accuracy over 100%
		Character.GetPlayer()->m_Stats.m_ShotsFired--;
		return false;
	}
	if(Dmg < g_Config.m_SvDamageNeededForKill && Weapon == WEAPON_GRENADE)
		return false;
	Dmg = 20;

	dbg_assert(Character.IsAlive(), "tried to apply damage to dead character");

	if(CGameControllerPvp::OnCharacterTakeDamage(Force, Dmg, From, Weapon, Character))
		return true;

	CPlayer *pFrom = nullptr;
	if(From >= 0 && From < MAX_CLIENTS)
		pFrom = GameServer()->m_apPlayers[From];

	if(!Character.IsAlive() && From != Character.GetPlayer()->GetCid() && pFrom)
	{
		// do damage Hit sound
		CClientMask Mask = CClientMask().set(From);
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS && GameServer()->m_apPlayers[i]->m_SpectatorId == From)
				Mask.set(i);
		}
		GameServer()->CreateSound(pFrom->m_ViewPos, SOUND_HIT, Mask);

		CCharacter *pChr = pFrom->GetCharacter();
		if(!pChr)
			return false;

		// refill nades
		int RefillNades = 0;
		if(g_Config.m_SvGrenadeAmmoRegenOnKill == 1)
			RefillNades = 1;
		else if(g_Config.m_SvGrenadeAmmoRegenOnKill == 2)
			RefillNades = g_Config.m_SvGrenadeAmmoRegenNum;
		if(RefillNades && g_Config.m_SvGrenadeAmmoRegen && Weapon == WEAPON_GRENADE)
		{
			pChr->SetWeaponAmmo(WEAPON_GRENADE, minimum(pChr->GetCore().m_aWeapons[WEAPON_GRENADE].m_Ammo + RefillNades, g_Config.m_SvGrenadeAmmoRegenNum));
		}
		return false;
	}
	return false;
}

bool CGameControllerInstagib::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
{
	// ddnet-insta
	// do not spawn pickups
	if(Index == ENTITY_ARMOR_1 || Index == ENTITY_HEALTH_1 || Index == ENTITY_WEAPON_SHOTGUN || Index == ENTITY_WEAPON_GRENADE || Index == ENTITY_WEAPON_LASER || Index == ENTITY_POWERUP_NINJA)
		return false;

	return CGameControllerPvp::OnEntity(Index, x, y, Layer, Flags, Initial, Number);
}
