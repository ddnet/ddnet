#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>

#include <generated/protocol.h>

#include <base/math.h>
#include <base/str.h>
#include <base/vmath.h>

#include "text.h"

constexpr float CellSize = 8.0f;

CProjectileText::CProjectileText(CGameWorld *pGameWorld, vec2 Pos, int Owner, int AliveTicks, const char *pText, int Type) :
	CText(pGameWorld, Pos, Owner, AliveTicks, pText, CGameWorld::ENTTYPE_PROJECTILE)
{
	m_CurTicks = Server()->Tick();
	m_StartTick = Server()->Tick();
	m_AliveTicks = AliveTicks;
	m_Owner = Owner;
	m_Type = Type;

	str_copy(m_aText, pText);

	int strlen = str_length(m_aText);
	m_Pos = Pos;
	m_Pos -= vec2((strlen * (GlyphW + 1) - 1) * CellSize / 2.0f, GlyphH * CellSize / 2.0f); // center the text

	SetData(CellSize);
	GameWorld()->InsertEntity(this);
}

void CProjectileText::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	for(auto *pData : m_pData)
	{
		CNetObj_DDNetProjectile *pProj = Server()->SnapNewItem<CNetObj_DDNetProjectile>(pData->m_Id);
		if(!pProj)
			return;

		vec2 Pos = pData->m_Pos;

		pProj->m_X = round_to_int(Pos.x * 100.0f);
		pProj->m_Y = round_to_int(Pos.y * 100.0f);
		pProj->m_Type = m_Type;
		pProj->m_Owner = m_Owner;
		pProj->m_StartTick = 0;
		pProj->m_VelX = 0;
		pProj->m_VelY = 0;
	}
}
