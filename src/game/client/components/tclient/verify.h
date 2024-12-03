#ifndef GAME_CLIENT_COMPONENTS_VERIFY_H
#define GAME_CLIENT_COMPONENTS_VERIFY_H
#include <game/client/component.h>

class CVerify : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif
