/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_HUD_H
#define GAME_CLIENT_COMPONENTS_HUD_H
#include <game/client/component.h>

class CHud : public CComponent
{
	float m_Width, m_Height;
	float m_AverageFPS;

	void RenderCursor();

	void RenderFps();
	void RenderConnectionWarning();
	void RenderTeambalanceWarning();
	void RenderVoting();
	void RenderHealthAndAmmo(const CNetObj_Character_DDNet *pCharacter);
	void RenderGameTimer();
	void RenderPauseNotification();
	void RenderSuddenDeath();
	void RenderScoreHud();
	void RenderSpectatorHud();
	void RenderWarmupTimer();

	void MapscreenToGroup(float CenterX, float CenterY, struct CMapItemGroup *PGroup);
public:
	CHud();

	virtual void OnReset();
	virtual void OnRender();

	// DDRace

	virtual void OnMessage(int MsgType, void *pRawMsg);

private:

	void RenderRecord();
	void RenderDDRaceEffects();
	float m_CheckpointDiff;
	float m_ServerRecord;
	float m_PlayerRecord;
	int m_DDRaceTime;
	int m_LastReceivedTimeTick;
	int m_CheckpointTick;
	int m_DDRaceTick;
	bool m_FinishTime;
	bool m_DDRaceTimeReceived;
};

#endif
