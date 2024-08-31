#ifndef GAME_SERVER_GAMEMODES_BASE_PVP_CHARACTER_H
// hack for headerguard linter
#endif

#ifndef IN_CLASS_CHARACTER

#include <game/server/entity.h>
#include <game/server/save.h>

class CCharacter : public CEntity
{
#endif // IN_CLASS_CHARACTER

public:
	// ddnet-insta
	/*
		Reset instagib tee without resetting ddnet or teeworlds tee
		update grenade ammo state without selfkill
		useful for votes
	*/
	void ResetInstaSettings();
	bool m_IsGodmode;

	int Health() { return m_Health; };
	int Armor() { return m_Armor; };

	void SetHealth(int Amount) { m_Health = Amount; };
	// void SetArmor(int Amount) { m_Armor = Amount; }; // defined by ddnet

	void AddHealth(int Amount) { m_Health += Amount; };
	void AddArmor(int Amount) { m_Armor += Amount; };

#ifndef IN_CLASS_CHARACTER
};
#endif
