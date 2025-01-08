#ifndef GAME_CLIENT_COMPONENTS_RAINBOW_H
#define GAME_CLIENT_COMPONENTS_RAINBOW_H
#include <game/client/component.h>

class CRainbow : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;

	enum COLORMODE
	{
		COLORMODE_RAINBOW = 1,
		COLORMODE_PULSE,
		COLORMODE_DARKNESS,
		COLORMODE_RANDOM
	};

	ColorRGBA m_RainbowColor = ColorRGBA(1, 1, 1, 1);
};

#endif
