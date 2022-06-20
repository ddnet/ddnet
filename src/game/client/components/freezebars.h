#ifndef GAME_CLIENT_COMPONENTS_FREEZEBARS_H
#define GAME_CLIENT_COMPONENTS_FREEZEBARS_H
#include <game/client/component.h>

class CFreezeBars : public CComponent
{
	void RenderFreezeBar(const int ClientID);
	void RenderFreezeBarPos(float x, const float y, const float width, const float height, float Progress, float Alpha = 1.0f);
	bool IsPlayerInfoAvailable(int ClientID) const;

public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif
