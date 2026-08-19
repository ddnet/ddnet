#include <game/gamecore.h>

#include <gtest/gtest.h>

// Reset() has to leave the core in a fully initialized state. Both the server and
// the client prediction index m_aWeapons with m_ActiveWeapon (CCharacter::Unfreeze(),
// CCharacter::FireWeapon(), ...), so an uninitialized m_ActiveWeapon is an out of
// bounds access waiting to happen.
TEST(GameCore, ResetInitializesWeapons)
{
	CCharacterCore Core;
	// values as found in uninitialized memory
	Core.m_ActiveWeapon = 0x5105503e;
	for(auto &Weapon : Core.m_aWeapons)
	{
		Weapon.m_AmmoRegenStart = 0x5105503e;
		Weapon.m_Ammo = 0x5105503e;
		Weapon.m_Ammocost = 0x5105503e;
		Weapon.m_Got = true;
	}

	Core.Reset();

	EXPECT_GE(Core.m_ActiveWeapon, 0);
	EXPECT_LT(Core.m_ActiveWeapon, NUM_WEAPONS);
	for(const auto &Weapon : Core.m_aWeapons)
	{
		EXPECT_FALSE(Weapon.m_Got);
	}
}
