/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_PHYSICS_WORLD_CONFIG_H
#define GAME_PHYSICS_WORLD_CONFIG_H

// Switches for which parts of the game world get simulated. The server runs
// the full simulation and keeps the defaults. The client prediction world
// fills them from the gameinfo of the server it is connected to and from the
// antiping settings, so shared physics code skips what the client cannot or
// should not predict.
class CWorldConfig
{
public:
	bool m_IsDDRace = true;
	bool m_IsVanilla = false;
	bool m_IsFNG = false;
	bool m_InfiniteAmmo = false;
	bool m_PredictTiles = true;
	int m_PredictFreeze = 1;
	bool m_PredictWeapons = true;
	bool m_PredictDDRace = true;
	bool m_IsSolo = false;
	bool m_UseTuneZones = true;
	bool m_BugDDRaceInput = true;
	// The server reads g_Config directly where the following apply
	bool m_NoWeakHookAndBounce = false;
	bool m_PredictEvents = true;
	bool m_OldLaser = false;
};

#endif // GAME_PHYSICS_WORLD_CONFIG_H
