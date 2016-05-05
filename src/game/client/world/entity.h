/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_WORLD_ENTITY_H
#define GAME_CLIENT_WORLD_ENTITY_H

#include <new>
#include <base/vmath.h>
#include "gameworld.h"

#define MACRO_ALLOC_HEAP() \
	public: \
	void *operator new(size_t Size) \
	{ \
		void *p = mem_alloc(Size, 1); \
		/*dbg_msg("", "++ %p %d", p, size);*/ \
		mem_zero(p, Size); \
		return p; \
	} \
	void operator delete(void *pPtr) \
	{ \
		/*dbg_msg("", "-- %p", p);*/ \
		mem_free(pPtr); \
	} \
	private:

class CEntity
{
	MACRO_ALLOC_HEAP()
	friend class CGameWorld;	// entity list handling
	CEntity *m_pPrevTypeEntity;
	CEntity *m_pNextTypeEntity;
protected:
	class CGameWorld *m_pGameWorld;
	bool m_MarkedForDestroy;
	int m_ID;
	int m_ObjType;
public:
	CEntity(CGameWorld *pGameWorld, int Objtype);
	virtual ~CEntity();

	class CGameWorld *GameWorld() { return m_pGameWorld; }
	CTuningParams *Tuning() { return GameWorld()->Tuning(); }
	class CCollision *Collision() { return GameWorld()->Collision(); }
	CEntity *TypeNext() { return m_pNextTypeEntity; }
	CEntity *TypePrev() { return m_pPrevTypeEntity; }
	virtual void Destroy() { delete this; }
	virtual void Reset() {}
	virtual void Tick() {}
	virtual void TickDefered() {}
	virtual void TickPaused() {}
	bool GameLayerClipped(vec2 CheckPos);
	float m_ProximityRadius;
	vec2 m_Pos;
	int m_Number;
	int m_Layer;
};

#endif
