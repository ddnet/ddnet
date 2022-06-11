
#ifndef GAME_CLIENT_COMPONENTS_OUTLINES_H
#define GAME_CLIENT_COMPONENTS_OUTLINES_H
#include <game/client/component.h>

class COutlines : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif
