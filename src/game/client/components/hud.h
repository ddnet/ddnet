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

	int m_TextRankContainerIndex = 0;
	int m_TextScoreContainerIndex = 0;
	float m_ScoreTextWidth = 0;
	char m_aScoreText[16] = {0};
	char m_aRankText[16] = {0};
	char m_aPlayerNameText[MAX_NAME_LENGTH] = {0};
	int m_RoundRectQuadContainerIndex = 0;
	int m_OptionalNameTextContainerIndex = 0;

	bool m_Initialized = false;
};

class CHud : public CComponent
{
	float m_Width = 0, m_Height = 0;
	float m_FrameTimeAvg = 0;

	int m_HudQuadContainerIndex = 0;
	SScoreInfo m_aScoreInfo[2];
	int m_FPSTextContainerIndex = 0;

	void RenderCursor();

	void RenderTextInfo();
	void RenderConnectionWarning();
	void RenderTeambalanceWarning();
	void RenderVoting();

	void PrepareAmmoHealthAndArmorQuads();
	void RenderAmmoHealthAndArmor(const CNetObj_Character *pCharacter);

	void PreparePlayerStateQuads();
	void RenderPlayerState(const int ClientID);
	void RenderDummyActions();
	void RenderMovementInformation(const int ClientID);

	void RenderGameTimer();
	void RenderPauseNotification();
	void RenderSuddenDeath();
	void RenderScoreHud();
	void RenderSpectatorHud();
	void RenderWarmupTimer();
	void RenderLocalTime(float x);

	static constexpr float MOVEMENT_INFORMATION_LINE_HEIGHT = 8.0f;

public:
	CHud();
	virtual int Sizeof() const override { return sizeof(*this); }

	void ResetHudContainers();
	virtual void OnWindowResize() override;
	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual void OnInit() override;

	// DDRace

	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	void RenderNinjaBarPos(float x, const float y, const float width, const float height, float Progress, float Alpha = 1.0f);

private:
	void RenderRecord();
	void RenderDDRaceEffects();
	float m_TimeCpDiff = 0;
	float m_ServerRecord = 0;
	float m_aPlayerRecord[NUM_DUMMIES] = {0};
	float m_FinishTimeDiff = 0;
	int m_DDRaceTime = 0;
	int m_FinishTimeLastReceivedTick = 0;
	int m_TimeCpLastReceivedTick = 0;
	bool m_ShowFinishTime = false;

	inline float GetMovementInformationBoxHeight();
	inline int GetDigitsIndex(int Value, int Max);

	// Quad Offsets
	int m_aAmmoOffset[NUM_WEAPONS] = {0};
	int m_HealthOffset = 0;
	int m_EmptyHealthOffset = 0;
	int m_ArmorOffset = 0;
	int m_EmptyArmorOffset = 0;
	int m_aCursorOffset[NUM_WEAPONS] = {0};
	int m_FlagOffset = 0;
	int m_AirjumpOffset = 0;
	int m_AirjumpEmptyOffset = 0;
	int m_WeaponHammerOffset = 0;
	int m_WeaponGunOffset = 0;
	int m_WeaponShotgunOffset = 0;
	int m_WeaponGrenadeOffset = 0;
	int m_WeaponLaserOffset = 0;
	int m_WeaponNinjaOffset = 0;
	int m_EndlessJumpOffset = 0;
	int m_EndlessHookOffset = 0;
	int m_JetpackOffset = 0;
	int m_TeleportGrenadeOffset = 0;
	int m_TeleportGunOffset = 0;
	int m_TeleportLaserOffset = 0;
	int m_SoloOffset = 0;
	int m_CollisionDisabledOffset = 0;
	int m_HookHitDisabledOffset = 0;
	int m_HammerHitDisabledOffset = 0;
	int m_GunHitDisabledOffset = 0;
	int m_ShotgunHitDisabledOffset = 0;
	int m_GrenadeHitDisabledOffset = 0;
	int m_LaserHitDisabledOffset = 0;
	int m_DeepFrozenOffset = 0;
	int m_LiveFrozenOffset = 0;
	int m_DummyHammerOffset = 0;
	int m_DummyCopyOffset = 0;
	int m_PracticeModeOffset = 0;
};

#endif
