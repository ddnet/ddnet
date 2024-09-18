#include <base/math.h>
#include <base/system.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entities/ddnet_pvp/vanilla_pickup.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "base_vanilla.h"

CGameControllerVanilla::CGameControllerVanilla(class CGameContext *pGameServer) :
	CGameControllerPvp(pGameServer)
{
	m_GameFlags = 0;
	m_AllowSkinChange = true;
	m_DefaultWeapon = WEAPON_GUN;
}

CGameControllerVanilla::~CGameControllerVanilla() = default;

int CGameControllerVanilla::GameInfoExFlags(int SnappingClient)
{
	int Flags = CGameControllerPvp::GameInfoExFlags(SnappingClient);
	Flags &= ~(GAMEINFOFLAG_UNLIMITED_AMMO);
	Flags &= ~(GAMEINFOFLAG_PREDICT_DDRACE);
	return Flags;
}

bool CGameControllerVanilla::OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character)
{
	if(Weapon == WEAPON_GUN || Weapon == WEAPON_SHOTGUN)
		Dmg = 1;
	if(Weapon == WEAPON_LASER)
		Dmg = 5;
	if(Weapon == WEAPON_GRENADE)
		Dmg = 6;

	// m_pPlayer only inflicts half damage on self
	if(From == Character.GetPlayer()->GetCid())
		Dmg = maximum(1, Dmg / 2);

	Character.m_DamageTaken++;

	// create healthmod indicator
	if(Server()->Tick() < Character.m_DamageTakenTick + 25)
	{
		// make sure that the damage indicators doesn't group together
		GameServer()->CreateDamageInd(Character.m_Pos, Character.m_DamageTaken * 0.25f, Dmg);
	}
	else
	{
		Character.m_DamageTaken = 0;
		GameServer()->CreateDamageInd(Character.m_Pos, 0, Dmg);
	}

	if(Dmg)
	{
		if(Character.m_Armor)
		{
			if(Dmg > 1)
			{
				Character.m_Health--;
				Dmg--;
			}

			if(Dmg > Character.m_Armor)
			{
				Dmg -= Character.m_Armor;
				Character.m_Armor = 0;
			}
			else
			{
				Character.m_Armor -= Dmg;
				Dmg = 0;
			}
		}
	}

	Character.m_DamageTakenTick = Server()->Tick();

	if(From >= 0 && From < MAX_CLIENTS && From != Character.GetPlayer()->GetCid() && GameServer()->m_apPlayers[From])
	{
		// do damage Hit sound
		CClientMask Mask = CClientMask().set(From);
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS && GameServer()->m_apPlayers[i]->m_SpectatorId == From)
				Mask.set(i);
		}
		GameServer()->CreateSound(GameServer()->m_apPlayers[From]->m_ViewPos, SOUND_HIT, Mask);
	}

	if(Dmg > 2)
		GameServer()->CreateSound(Character.m_Pos, SOUND_PLAYER_PAIN_LONG);
	else
		GameServer()->CreateSound(Character.m_Pos, SOUND_PLAYER_PAIN_SHORT);

	return CGameControllerPvp::OnCharacterTakeDamage(Force, Dmg, From, Weapon, Character);
}

bool CGameControllerVanilla::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
{
	const vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);

	int Type = -1;
	int SubType = 0;

	if(Index == ENTITY_ARMOR_1)
		Type = POWERUP_ARMOR;
	else if(Index == ENTITY_ARMOR_SHOTGUN)
		Type = POWERUP_ARMOR_SHOTGUN;
	else if(Index == ENTITY_ARMOR_GRENADE)
		Type = POWERUP_ARMOR_GRENADE;
	else if(Index == ENTITY_ARMOR_NINJA)
		Type = POWERUP_ARMOR_NINJA;
	else if(Index == ENTITY_ARMOR_LASER)
		Type = POWERUP_ARMOR_LASER;
	else if(Index == ENTITY_HEALTH_1)
		Type = POWERUP_HEALTH;
	else if(Index == ENTITY_WEAPON_SHOTGUN)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_SHOTGUN;
	}
	else if(Index == ENTITY_WEAPON_GRENADE)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_GRENADE;
	}
	else if(Index == ENTITY_WEAPON_LASER)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_LASER;
	}
	else if(Index == ENTITY_POWERUP_NINJA && g_Config.m_SvPowerups)
	{
		Type = POWERUP_NINJA;
		SubType = WEAPON_NINJA;
	}

	if(Type != -1) // NOLINT(clang-analyzer-unix.Malloc)
	{
		CVanillaPickup *pPickup = new CVanillaPickup(&GameServer()->m_World, Type, SubType, Layer, Number);
		pPickup->m_Pos = Pos;
		return true; // NOLINT(clang-analyzer-unix.Malloc)
	}

	return CGameControllerPvp::OnEntity(Index, x, y, Layer, Flags, Initial, Number);
}
