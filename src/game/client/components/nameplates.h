/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_NAMEPLATES_H
#define GAME_CLIENT_COMPONENTS_NAMEPLATES_H
#include <base/vmath.h>

#include <engine/shared/protocol.h>

#include <game/client/component.h>

struct CNetObj_Character;
struct CNetObj_PlayerInfo;

struct SPlayerNamePlate
{
	SPlayerNamePlate()
	{
		Reset();
	}

	void Reset()
	{
		m_NameTextContainerIndex = m_ClanNameTextContainerIndex = -1;
		m_aName[0] = 0;
		m_aClanName[0] = 0;
		m_NameTextWidth = m_ClanNameTextWidth = 0.f;
		m_NameTextFontSize = m_ClanNameTextFontSize = 0;
	}

	char m_aName[MAX_NAME_LENGTH] = {0};
	float m_NameTextWidth = 0;
	int m_NameTextContainerIndex = 0;
	float m_NameTextFontSize = 0;

	char m_aClanName[MAX_CLAN_LENGTH] = {0};
	float m_ClanNameTextWidth = 0;
	int m_ClanNameTextContainerIndex = 0;
	float m_ClanNameTextFontSize = 0;
};

class CNamePlates : public CComponent
{
	void RenderNameplate(
		const CNetObj_Character *pPrevChar,
		const CNetObj_Character *pPlayerChar,
		const CNetObj_PlayerInfo *pPlayerInfo);
	void RenderNameplatePos(vec2 Position, const CNetObj_PlayerInfo *pPlayerInfo, float Alpha, bool ForceAlpha = false);

	SPlayerNamePlate m_aNamePlates[MAX_CLIENTS];
	class CPlayers *m_pPlayers = nullptr;

	void ResetNamePlates();

	int m_DirectionQuadContainerIndex = 0;

public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnWindowResize() override;
	virtual void OnInit() override;
	virtual void OnRender() override;

	void SetPlayers(class CPlayers *pPlayers);
};

#endif
