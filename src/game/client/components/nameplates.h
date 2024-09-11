/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_NAMEPLATES_H
#define GAME_CLIENT_COMPONENTS_NAMEPLATES_H
#include <base/vmath.h>

#include <engine/shared/protocol.h>
#include <engine/textrender.h>

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
		m_NameTextContainerIndex.Reset();
		m_ClanTextContainerIndex.Reset();
		m_aName[0] = '\0';
		m_aClan[0] = '\0';
		m_NameTextFontSize = m_ClanTextFontSize = 0.0f;
	}

	char m_aName[MAX_NAME_LENGTH];
	STextContainerIndex m_NameTextContainerIndex;
	float m_NameTextFontSize;

	char m_aClan[MAX_CLAN_LENGTH];
	STextContainerIndex m_ClanTextContainerIndex;
	float m_ClanTextFontSize;
};

class CNamePlates : public CComponent
{
	void RenderNameplate(vec2 Position, const CNetObj_PlayerInfo *pPlayerInfo, float Alpha, bool ForceAlpha);

	SPlayerNamePlate m_aNamePlates[MAX_CLIENTS];

	void ResetNamePlates();

	int m_DirectionQuadContainerIndex;

public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnWindowResize() override;
	virtual void OnInit() override;
	virtual void OnRender() override;
};

#endif
