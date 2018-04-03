/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_PLAYERS_H
#define GAME_CLIENT_COMPONENTS_PLAYERS_H
#include <game/client/component.h>

class CPlayers : public CComponent
{
	friend class CGhost;

	CTeeRenderInfo m_aRenderInfo[MAX_CLIENTS];
	void RenderHand(class CTeeRenderInfo *pInfo, vec2 CenterPos, vec2 Dir, float AngleOffset, vec2 PostRotOffset);
	void RenderPlayer(
		const CNetObj_Character *pPrevChar,
		const CNetObj_Character *pPlayerChar,
		const CTeeRenderInfo *pRenderInfo,
		int ClientID,
		const vec2 &Position,
		float Intra = 0.f
/*		vec2 &PrevPredPos,
		vec2 &SmoothPos,
		int &MoveCnt
*/
	);
	void RenderHook(
		const CNetObj_Character *pPrevChar,
		const CNetObj_Character *pPlayerChar,
		const CTeeRenderInfo *pRenderInfo,
		int ClientID,
		const vec2 &Position,
		const vec2 &PositionTo,
		float Intra = 0.f
	);

	void Predict(
		const CNetObj_Character *pPrevChar,
		const CNetObj_Character *pPlayerChar,
		const CNetObj_PlayerInfo *pPrevInfo,
		const CNetObj_PlayerInfo *pPlayerInfo,
		vec2 &PrevPredPos,
		vec2 &SmoothPos,
		int &MoveCnt,
		vec2 &Position
	);

	int m_WeaponEmoteQuadContainerIndex;
	int m_DirectionQuadContainerIndex;
	int m_WeaponSpriteMuzzleQuadContainerIndex[NUM_WEAPONS];
public:
	vec2 m_CurPredictedPos[MAX_CLIENTS];
	virtual void OnInit();
	virtual void OnRender();
};

#endif
