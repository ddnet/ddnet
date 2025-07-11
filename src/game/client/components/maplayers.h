/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MAPLAYERS_H
#define GAME_CLIENT_COMPONENTS_MAPLAYERS_H
#include <game/client/component.h>
#include <game/client/render.h>

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

class CMapLayers : public CComponent
{
	friend class CBackground;
	friend class CMenuBackground;
	friend class CRenderLayer;
	friend class CRenderLayerTile;
	friend class CRenderLayerQuads;
	friend class CRenderLayerEntityGame;
	friend class CRenderLayerEntityFront;
	friend class CRenderLayerEntityTele;
	friend class CRenderLayerEntitySpeedup;
	friend class CRenderLayerEntitySwitch;
	friend class CRenderLayerEntityTune;

	CLayers *m_pLayers;
	CMapImages *m_pImages;
	std::shared_ptr<CMapBasedEnvelopePointAccess> m_pEnvelopePoints;
	std::shared_ptr<IMapData> m_pMapData;

	int m_Type;
	bool m_OnlineOnly;

public:
	static void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, IMap *pMap, CMapBasedEnvelopePointAccess *pEnvelopePoints, IClient *pClient, CGameClient *pGameClient, bool OnlineOnly);
	void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels);

	CMapLayers(int Type, bool OnlineOnly = true);
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnRender() override;
	void OnMapLoad() override;

	virtual CCamera *GetCurCamera();

private:
	std::vector<std::unique_ptr<CRenderLayer>> m_vRenderLayers;
	int GetLayerType(const CMapItemLayer *pLayer) const;
};

#endif
