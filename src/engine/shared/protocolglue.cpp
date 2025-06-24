#include <game/generated/protocol.h>
#include <game/generated/protocol7.h>
#include <game/generated/protocolglue.h>

#include "protocolglue.h"

int GameFlags_ClampToSix(int Flags)
{
	int Six = 0;
	if(Flags & GAMEFLAG_TEAMS)
		Six |= GAMEFLAG_TEAMS;
	if(Flags & GAMEFLAG_FLAGS)
		Six |= GAMEFLAG_FLAGS;
	return Six;
}

int PlayerFlags_SevenToSix(int Flags)
{
	int Six = 0;
	if(Flags & protocol7::PLAYERFLAG_CHATTING)
		Six |= PLAYERFLAG_CHATTING;
	if(Flags & protocol7::PLAYERFLAG_SCOREBOARD)
		Six |= PLAYERFLAG_SCOREBOARD;
	if(Flags & protocol7::PLAYERFLAG_AIM)
		Six |= PLAYERFLAG_AIM;
	return Six;
}

int PlayerFlags_SixToSeven(int Flags)
{
	int Seven = 0;
	if(Flags & PLAYERFLAG_CHATTING)
		Seven |= protocol7::PLAYERFLAG_CHATTING;
	if(Flags & PLAYERFLAG_SCOREBOARD)
		Seven |= protocol7::PLAYERFLAG_SCOREBOARD;
	if(Flags & PLAYERFLAG_AIM)
		Seven |= protocol7::PLAYERFLAG_AIM;
	return Seven;
}

void PickupType_SevenToSix(int Type7, int &Type6, int &SubType6)
{
	SubType6 = 0;
	Type6 = POWERUP_WEAPON;
	switch(Type7)
	{
	case protocol7::PICKUP_HEALTH:
	case protocol7::PICKUP_ARMOR:
		Type6 = Type7;
		break;
	case protocol7::PICKUP_GRENADE:
		SubType6 = WEAPON_GRENADE;
		break;
	case protocol7::PICKUP_SHOTGUN:
		SubType6 = WEAPON_SHOTGUN;
		break;
	case protocol7::PICKUP_LASER:
		SubType6 = WEAPON_LASER;
		break;
	case protocol7::PICKUP_GUN:
		SubType6 = WEAPON_GUN;
		break;
	case protocol7::PICKUP_HAMMER:
		SubType6 = WEAPON_HAMMER;
		break;
	case protocol7::PICKUP_NINJA:
		SubType6 = WEAPON_NINJA;
		Type6 = POWERUP_NINJA;
		break;
	default:
		// dbg_msg("sixup", "ERROR: failed to translate weapon=%d to 0.6", Type7);
		break;
	}
}

int PickupType_SixToSeven(int Type6, int SubType6)
{
	if(Type6 == POWERUP_WEAPON)
		return SubType6 == WEAPON_SHOTGUN ? protocol7::PICKUP_SHOTGUN : SubType6 == WEAPON_GRENADE ? protocol7::PICKUP_GRENADE : protocol7::PICKUP_LASER;
	else if(Type6 == POWERUP_NINJA)
		return protocol7::PICKUP_NINJA;
	else if(Type6 == POWERUP_ARMOR)
		return protocol7::PICKUP_ARMOR;
	return 0;
}
