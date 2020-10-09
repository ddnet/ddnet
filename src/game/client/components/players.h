/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_PLAYERS_H
#define GAME_CLIENT_COMPONENTS_PLAYERS_H
#include <game/client/component.h>

class CPlayers : public CComponent
{
	friend class CGhost;

	CTeeRenderInfo m_RenderInfoSpec;
	CTeeRenderInfo m_aRenderInfo[MAX_CLIENTS];
	void RenderHand(class CTeeRenderInfo *pInfo, vec2 CenterPos, vec2 Dir, float AngleOffset, vec2 PostRotOffset, float Alpha = 1.0f);
	void RenderPlayer(
		const CNetObj_Character *pPrevChar,
		const CNetObj_Character *pPlayerChar,
		const CTeeRenderInfo *pRenderInfo,
		int ClientID,
		float Intra = 0.f);
	void RenderHook(
		const CNetObj_Character *pPrevChar,
		const CNetObj_Character *pPlayerChar,
		const CTeeRenderInfo *pRenderInfo,
		int ClientID,
		float Intra = 0.f);

	int m_WeaponEmoteQuadContainerIndex;
	int m_DirectionQuadContainerIndex;
	int m_WeaponSpriteMuzzleQuadContainerIndex[NUM_WEAPONS];

public:
	vec2 m_CurPredictedPos[MAX_CLIENTS];
	virtual void OnInit();
	virtual void OnRender();
};

#endif
