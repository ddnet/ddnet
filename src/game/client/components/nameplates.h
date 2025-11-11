#ifndef GAME_CLIENT_COMPONENTS_NAMEPLATES_H
#define GAME_CLIENT_COMPONENTS_NAMEPLATES_H

#include <base/color.h>
#include <base/vmath.h>

#include <game/client/component.h>

struct CNetObj_PlayerInfo;

class CNamePlates : public CComponent
{
private:
	class CNamePlatesData;
	CNamePlatesData *m_pData;

public:
	void RenderNamePlateGame(vec2 Position, const CNetObj_PlayerInfo *pPlayerInfo, float Alpha);
	void RenderNamePlatePreview(vec2 Position, int Dummy);
	void ResetNamePlates();
	int Sizeof() const override { return sizeof(*this); }
	void OnWindowResize() override;
	void OnRender() override;
	CNamePlates();
	~CNamePlates() override;
};

#endif
