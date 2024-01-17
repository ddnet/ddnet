/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_DEBUGHUD_H
#define GAME_CLIENT_COMPONENTS_DEBUGHUD_H
#include <engine/client/client.h>

#include <game/client/component.h>

class CDebugHud : public CComponent
{
	void RenderNetCorrections();
	void RenderTuning();
	void RenderHint();

	CGraph m_RampGraph;
	CGraph m_ZoomedInGraph;
	float m_SpeedTurningPoint;
	float MiddleOfZoomedInGraph;
	float m_OldVelrampStart;
	float m_OldVelrampRange;
	float m_OldVelrampCurvature;

public:
	CDebugHud();
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif
