
#ifndef GAME_CLIENT_COMPONENTS_PLAYER_INDICATOR_H
#define GAME_CLIENT_COMPONENTS_PLAYER_INDICATOR_H
#include <game/client/component.h>

class CPlayerIndicator : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif
