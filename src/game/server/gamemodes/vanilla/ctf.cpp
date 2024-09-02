#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entities/ddnet_pvp/vanilla_pickup.h>
#include <game/server/player.h>

#include "ctf.h"

CGameControllerCTF::CGameControllerCTF(class CGameContext *pGameServer) :
	CGameControllerBaseCTF(pGameServer)
{
	m_pGameType = "CTF*";
	m_GameFlags = GAMEFLAG_TEAMS | GAMEFLAG_FLAGS;
	m_AllowSkinChange = true;
	m_DefaultWeapon = WEAPON_GUN;
}

CGameControllerCTF::~CGameControllerCTF() = default;

void CGameControllerCTF::Tick()
{
	CGameControllerBaseCTF::Tick();
}

void CGameControllerCTF::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerBaseCTF::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(WEAPON_HAMMER, false, -1);
	pChr->GiveWeapon(WEAPON_GUN, false, 10);
}

int CGameControllerCTF::GameInfoExFlags(int SnappingClient)
{
	int Flags = CGameControllerPvp::GameInfoExFlags(SnappingClient);
	Flags &= ~(GAMEINFOFLAG_UNLIMITED_AMMO);
	return Flags;
}

bool CGameControllerCTF::OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character)
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

	if(Dmg > 2)
		GameServer()->CreateSound(Character.m_Pos, SOUND_PLAYER_PAIN_LONG);
	else
		GameServer()->CreateSound(Character.m_Pos, SOUND_PLAYER_PAIN_SHORT);

	return CGameControllerPvp::OnCharacterTakeDamage(Force, Dmg, From, Weapon, Character);
}

bool CGameControllerCTF::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
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
	else if(Index == ENTITY_POWERUP_NINJA)
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

	return CGameControllerBaseCTF::OnEntity(Index, x, y, Layer, Flags, Initial, Number);
}
