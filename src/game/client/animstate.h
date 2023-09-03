/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_ANIMSTATE_H
#define GAME_CLIENT_ANIMSTATE_H

#include <game/generated/client_data.h>

class CAnimState
{
	CAnimKeyframe m_Body;
	CAnimKeyframe m_BackFoot;
	CAnimKeyframe m_FrontFoot;
	CAnimKeyframe m_Attach;

	void AnimAdd(CAnimState *pState, const CAnimState *pAdded, float Amount);

public:
	const CAnimKeyframe *GetBody() const { return &m_Body; }
	const CAnimKeyframe *GetBackFoot() const { return &m_BackFoot; }
	const CAnimKeyframe *GetFrontFoot() const { return &m_FrontFoot; }
	const CAnimKeyframe *GetAttach() const { return &m_Attach; }
	void Set(CAnimation *pAnim, float Time);
	void Add(CAnimation *pAnim, float Time, float Amount);

	const static CAnimState *GetIdle();
};

#endif
