// Made by qxdFox
#ifndef GAME_SERVER_FOXNET_ENTITIES_PICKUPDROP_H
#define GAME_SERVER_FOXNET_ENTITIES_PICKUPDROP_H

#include <base/vmath.h>
#include <engine/shared/protocol.h>
#include <game/mapitems.h>
#include <game/server/entity.h>
#include <game/server/gameworld.h>

class CPickupDrop : public CEntity
{
	int m_LastOwner;
	int m_Lifetime; // In ticks
	int m_PickupDelay; // In ticks
	int m_Type;

	vec2 m_PrevPos;
	vec2 m_Vel;

	int m_aIds[2]; // Extra Ids

	int m_Team;

	vec2 m_GroundElasticity;

	static bool IsSwitchActiveCb(int Number, void *pUser);
	bool IsGrounded();
	void HandleTiles(int Index);
	void HandleQuads(const CMapItemLayerQuads *pQuadLayer, int QuadIndex);
	void HandleQuadStopa(const CMapItemLayerQuads *pQuadLayer, int QuadIndex);

	bool m_InsideFreeze;

	int m_TeleCheckpoint;
	int m_TileIndex;
	int m_TileFIndex;
	int m_TuneZone;
	int m_MoveRestrictions;

	bool CollectItem();
	bool CheckArmor();

public:
	CPickupDrop(CGameWorld *pGameWorld, int LastOwner, vec2 Pos, int Team, int TeleCheckpoint, vec2 Dir, int Lifetime /*Seconds*/, int Type);

	void Reset(bool PickdUp);
	virtual void Reset() override { Reset(false); }
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_FOXNET_ENTITIES_PICKUPDROP_H
