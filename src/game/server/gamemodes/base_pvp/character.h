#ifndef GAME_SERVER_GAMEMODES_BASE_PVP_CHARACTER_H
// hack for headerguard linter
#endif

#ifndef IN_CLASS_CHARACTER

#include <game/server/entity.h>
#include <game/server/save.h>

class CCharacter : public CEntity
{
#endif // IN_CLASS_CHARACTER

	friend class CGameControllerVanilla;
	friend class CGameControllerPvp;
	friend class CGameControllerCTF;
	friend class CGameControllerBaseFng;

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
	int GetAimDir() { return m_Input.m_TargetX < 0 ? -1 : 1; };

	bool OnFngFireWeapon(CCharacter &Character, int &Weapon, vec2 &Direction, vec2 &MouseTarget, vec2 &ProjStartPos);
	void TakeHammerHit(CCharacter *pFrom);

#ifndef IN_CLASS_CHARACTER
};
#endif
