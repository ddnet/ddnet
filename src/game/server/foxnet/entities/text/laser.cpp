#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>

#include <generated/protocol.h>

#include <base/math.h>
#include <base/str.h>
#include <base/vmath.h>

#include "text.h"

constexpr float CellSize = 16.0f;

CLaserText::CLaserText(CGameWorld *pGameWorld, vec2 Pos, int Owner, int AliveTicks, const char *pText) :
	CText(pGameWorld, Pos, Owner, AliveTicks, pText, CGameWorld::ENTTYPE_LASER)
{
	m_CurTicks = Server()->Tick();
	m_StartTick = Server()->Tick();
	m_AliveTicks = AliveTicks;
	m_Owner = Owner;

	str_copy(m_aText, pText);

	int strlen = str_length(m_aText);
	m_Pos = Pos;
	m_Pos -= vec2((strlen * (GlyphW + 1) - 1) * CellSize / 2.0f, GlyphH * CellSize / 2.0f); // center the text

	SetData(CellSize);
	GameWorld()->InsertEntity(this);
}

void CLaserText::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	for(auto *pData : m_pData)
	{
		CNetObj_DDNetLaser *pObj = Server()->SnapNewItem<CNetObj_DDNetLaser>(pData->m_Id);
		if(!pObj)
			return;

		pObj->m_ToX = pData->m_Pos.x;
		pObj->m_ToY = pData->m_Pos.y;
		pObj->m_FromX = pData->m_Pos.x;
		pObj->m_FromY = pData->m_Pos.y;
		pObj->m_StartTick = Server()->Tick();
		pObj->m_Owner = m_Owner;
		pObj->m_Type = LASERTYPE_RIFLE;
		pObj->m_Flags = LASERFLAG_NO_PREDICT;
	}
}
