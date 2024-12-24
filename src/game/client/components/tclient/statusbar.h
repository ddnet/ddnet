#ifndef GAME_CLIENT_COMPONENTS_STATUSBAR_H
#define GAME_CLIENT_COMPONENTS_STATUSBAR_H
#include <game/client/component.h>

class CStatusBar : public CComponent
{

public:
	CStatusBar();
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;

};

#endif
