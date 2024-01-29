#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_CHARACTER_H
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

#ifndef IN_CLASS_CHARACTER
};
#endif
