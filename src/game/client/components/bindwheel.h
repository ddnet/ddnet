
#ifndef GAME_CLIENT_COMPONENTS_BIND_WHEEL_H
#define GAME_CLIENT_COMPONENTS_BIND_WHEEL_H
#include <game/client/component.h>

class CBindWheel : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }
	
};

#endif
