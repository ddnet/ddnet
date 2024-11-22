
#ifndef GAME_CLIENT_COMPONENTS_RAINBOW_H
#define GAME_CLIENT_COMPONENTS_RAINBOW_H
#include "game/client/render.h"
#include <game/client/component.h>

class CRainbow : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;

	void TransformColor(unsigned char Mode, int Tick, CTeeRenderInfo *pInfo);
	enum COLORMODE
	{
		COLORMODE_RAINBOW = 1,
		COLORMODE_PULSE,
		COLORMODE_DARKNESS,
	};
};

#endif
