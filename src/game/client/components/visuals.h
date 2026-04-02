#ifndef GAME_CLIENT_COMPONENTS_VISUALS_H
#define GAME_CLIENT_COMPONENTS_VISUALS_H

#include <game/client/component.h>

class CVisuals : public CComponent
{
	void RenderLine(const void *pData);
	void RenderCircle(const void *pData);
	void RenderQuad(const void *pData);
	void RenderTile(const void *pData);

	void SetColorFromPacked(int Color);

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
};

#endif
