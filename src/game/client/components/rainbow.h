
#ifndef GAME_CLIENT_COMPONENTS_RAINBOW_H
#define GAME_CLIENT_COMPONENTS_RAINBOW_H
#include <game/client/component.h>

class CRainbow : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;

	void TransformColor(unsigned char mode, int tick, CTeeRenderInfo *pinfo);
	enum COLORMODE
	{
		COLORMODE_OFF = 1,
		COLORMODE_RAINBOW,
		COLORMODE_PULSE,
		COLORMODE_DARKNESS,
	};
};

#endif
