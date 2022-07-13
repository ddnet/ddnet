/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_PREDICTION_ENTITY_H
#define GAME_CLIENT_PREDICTION_ENTITY_H

#include "gameworld.h"
#include <base/vmath.h>

#define MACRO_ALLOC_HEAP() \
public: \
	void *operator new(size_t Size) \
	{ \
		void *p = malloc(Size); \
		mem_zero(p, Size); \
		return p; \
	} \
	void operator delete(void *pPtr) \
	{ \
		free(pPtr); \
	} \
\
private:

class CEntity
{
	MACRO_ALLOC_HEAP()
	friend class CGameWorld; // entity list handling
	CEntity *m_pPrevTypeEntity = nullptr;
	CEntity *m_pNextTypeEntity = nullptr;

protected:
	class CGameWorld *m_pGameWorld = nullptr;
	bool m_MarkedForDestroy = false;
	int m_ID = 0;
	int m_ObjType = 0;

public:
	int GetID() const { return m_ID; }

	CEntity(CGameWorld *pGameWorld, int Objtype, vec2 Pos = vec2(0, 0), int ProximityRadius = 0);
	virtual ~CEntity();

	std::vector<SSwitchers> &Switchers() { return m_pGameWorld->Switchers(); }
	class CGameWorld *GameWorld() { return m_pGameWorld; }
	CTuningParams *Tuning() { return GameWorld()->Tuning(); }
	CTuningParams *TuningList() { return GameWorld()->TuningList(); }
	CTuningParams *GetTuning(int i) { return GameWorld()->GetTuning(i); }
	class CCollision *Collision() { return GameWorld()->Collision(); }
	CEntity *TypeNext() { return m_pNextTypeEntity; }
	CEntity *TypePrev() { return m_pPrevTypeEntity; }
	const vec2 &GetPos() const { return m_Pos; }
	float GetProximityRadius() const { return m_ProximityRadius; }

	void Destroy() { delete this; }
	virtual void Tick() {}
	virtual void TickDefered() {}

	bool GameLayerClipped(vec2 CheckPos);
	float m_ProximityRadius = 0;
	vec2 m_Pos;
	int m_Number = 0;
	int m_Layer = 0;

	int m_SnapTicks = 0;
	int m_DestroyTick = 0;
	int m_LastRenderTick = 0;
	CEntity *m_pParent = nullptr;
	CEntity *m_pChild = nullptr;
	CEntity *NextEntity() { return m_pNextTypeEntity; }
	void Keep()
	{
		m_SnapTicks = 0;
		m_MarkedForDestroy = false;
	}

	CEntity()
	{
		m_ID = -1;
		m_pGameWorld = 0;
	}
};

#endif
