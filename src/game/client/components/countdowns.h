#ifndef GAME_CLIENT_COMPONENTS_COUNTDOWNS_H
#define GAME_CLIENT_COMPONENTS_COUNTDOWNS_H
#include <base/vmath.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

class CCountdowns : public CComponent
{
	void RenderFreezeBar(int ClientID);
	void RenderFreezeBarPos(float x, float y, float width, float height, float Progress, float Alpha = 1.0f);
	bool IsPlayerInfoAvailable(int ClientID) const;
	void GenerateFreezeStars();
	void RenderFreezeBars();

	int m_LastGenerateTick[MAX_CLIENTS];

public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;
	virtual void OnRender() override;
};

#endif
