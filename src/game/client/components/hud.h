/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_HUD_H
#define GAME_CLIENT_COMPONENTS_HUD_H
#include <game/client/component.h>

struct SScoreInfo
{
	SScoreInfo()
	{
		Reset();
	}
	
	void Reset()
	{
		m_TextRankContainerIndex = m_TextScoreContainerIndex = m_RoundRectQuadContainerIndex = m_OptionalNameTextContainerIndex = -1;
		m_aScoreText[0] = 0;
		m_aRankText[0] = 0;
		m_aPlayerNameText[0] = 0;
		m_ScoreTextWidth = 0.f;
		m_Initialized = false;
	}

	int m_TextRankContainerIndex;
	int m_TextScoreContainerIndex;
	float m_ScoreTextWidth;
	char m_aScoreText[16];
	char m_aRankText[16];
	char m_aPlayerNameText[MAX_NAME_LENGTH];
	int m_RoundRectQuadContainerIndex;
	int m_OptionalNameTextContainerIndex;

	bool m_Initialized;
};

class CHud : public CComponent
{
	float m_Width, m_Height;
	float m_FrameTimeAvg;

	int m_HudQuadContainerIndex;
	SScoreInfo m_aScoreInfo[2];

	void RenderCursor();

	void RenderTextInfo();
	void RenderConnectionWarning();
	void RenderTeambalanceWarning();
	void RenderVoting();

	void PrepareHealthAmoQuads();
	void RenderHealthAndAmmo(const CNetObj_Character *pCharacter);

	void RenderGameTimer();
	void RenderPauseNotification();
	void RenderSuddenDeath();
	void RenderScoreHud();
	void RenderSpectatorHud();
	void RenderWarmupTimer();
	void RenderLocalTime(float x);

	void MapscreenToGroup(float CenterX, float CenterY, struct CMapItemGroup *PGroup);
public:
	CHud();

	virtual void OnReset();
	virtual void OnRender();
	virtual void OnInit();

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
