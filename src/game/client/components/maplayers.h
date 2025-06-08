/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MAPLAYERS_H
#define GAME_CLIENT_COMPONENTS_MAPLAYERS_H
#include <game/client/component.h>
#include <game/client/render.h>

#include "render_layer.h"
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

	int m_Type;
	bool m_OnlineOnly;

public:
	enum
	{
		TYPE_BACKGROUND = 0,
		TYPE_BACKGROUND_FORCE,
		TYPE_FOREGROUND,
		TYPE_FULL_DESIGN,
		TYPE_ALL = -1,
	};

	static void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, IMap *pMap, CMapBasedEnvelopePointAccess *pEnvelopePoints, IClient *pClient, CGameClient *pGameClient, bool OnlineOnly);
	void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels);

	CMapLayers(int Type, bool OnlineOnly = true);
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;
	virtual void OnRender() override;
	virtual void OnMapLoad() override;

	virtual CCamera *GetCurCamera();

private:
	std::vector<std::unique_ptr<CRenderLayer>> m_vRenderLayers;
	int GetLayerType(const CMapItemLayer *pLayer) const;
	bool RenderGroup(const CRenderLayerParams &Params, int GroupId);
};

#endif
