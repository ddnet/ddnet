#ifndef GAME_CLIENT_COMPONENTS_FREEZEBARS_H
#define GAME_CLIENT_COMPONENTS_FREEZEBARS_H
#include <game/client/component.h>

class CFreezeBars : public CComponent
{
	void RenderFreezeBar(const int ClientId);
	void RenderFreezeBarPos(float x, const float y, const float Width, const float Height, float Progress, float Alpha = 1.0f);
	bool IsPlayerInfoAvailable(int ClientId) const;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
};

#endif
