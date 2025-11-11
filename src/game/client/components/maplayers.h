/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MAPLAYERS_H
#define GAME_CLIENT_COMPONENTS_MAPLAYERS_H

#include <game/client/component.h>
#include <game/map/map_renderer.h>

#include <cstdint>
#include <memory>

class CCamera;
class CLayers;
class CMapImages;
class ColorRGBA;

class CMapLayers : public CComponent, public IEnvelopeEval
{
	friend class CBackground;
	friend class CMenuBackground;

	CLayers *m_pLayers;
	CMapImages *m_pImages;
	std::shared_ptr<CMapBasedEnvelopePointAccess> m_pEnvelopePoints;

	ERenderType m_Type;
	bool m_OnlineOnly;

public:
	static void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, IMap *pMap, CMapBasedEnvelopePointAccess *pEnvelopePoints, IClient *pClient, CGameClient *pGameClient, bool OnlineOnly);
	void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels) override;

	CMapLayers(ERenderType Type, bool OnlineOnly = true);
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnRender() override;
	void OnMapLoad() override;
	void Unload();

	virtual CCamera *GetCurCamera();

private:
	CRenderLayerParams m_Params;
	CMapRenderer m_MapRenderer;
};

#endif
