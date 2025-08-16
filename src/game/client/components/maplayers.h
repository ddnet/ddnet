/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MAPLAYERS_H
#define GAME_CLIENT_COMPONENTS_MAPLAYERS_H

#include <game/client/component.h>
#include <game/map/render_layer.h>

#include <cstdint>
#include <memory>
#include <vector>

class CCamera;
class CLayers;
class CMapImages;
class ColorRGBA;

class CMapItemGroup;
class CMapItemLayer;
class CMapItemLayerTilemap;
class CMapItemLayerQuads;

class CMapLayers : public CComponent, public IEnvelopeEval
{
	friend class CBackground;
	friend class CMenuBackground;

	CLayers *m_pLayers;
	CMapImages *m_pImages;
	std::shared_ptr<CMapBasedEnvelopePointAccess> m_pEnvelopePoints;

	int m_Type;
	bool m_OnlineOnly;

public:
	static void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, IMap *pMap, CMapBasedEnvelopePointAccess *pEnvelopePoints, IClient *pClient, CGameClient *pGameClient, bool OnlineOnly);
	void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels) override;

	CMapLayers(int Type, bool OnlineOnly = true);
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnRender() override;
	void OnMapLoad() override;
	void Unload();

	virtual CCamera *GetCurCamera();

private:
	std::vector<std::unique_ptr<CRenderLayer>> m_vpRenderLayers;
	int GetLayerType(const CMapItemLayer *pLayer) const;
	CRenderLayerParams m_Params;
};

#endif
