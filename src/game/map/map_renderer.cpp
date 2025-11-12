#include "map_renderer.h"

#include <base/log.h>

#include <game/map/envelope_manager.h>

const int LAYER_DEFAULT_TILESET = -1;

void CMapRenderer::Clear()
{
	for(auto &pLayer : m_vpRenderLayers)
		pLayer->Unload();
	m_vpRenderLayers.clear();
}

void CMapRenderer::Load(ERenderType Type, CLayers *pLayers, IMapImages *pMapImages, IEnvelopeEval *pEnvelopeEval, std::optional<FRenderUploadCallback> RenderCallbackOptional)
{
	Clear();

	std::shared_ptr<CEnvelopeManager> pEnvelopeManager = std::make_shared<CEnvelopeManager>(pEnvelopeEval, pLayers->Map());
	bool PassedGameLayer = false;

	for(int GroupId = 0; GroupId < pLayers->NumGroups(); GroupId++)
	{
		CMapItemGroup *pGroup = pLayers->GetGroup(GroupId);
		std::unique_ptr<CRenderLayer> pRenderLayerGroup = std::make_unique<CRenderLayerGroup>(GroupId, pGroup);
		pRenderLayerGroup->OnInit(Graphics(), TextRender(), RenderMap(), pEnvelopeManager, pLayers->Map(), pMapImages, RenderCallbackOptional);
		if(!pRenderLayerGroup->IsValid())
		{
			log_error("map_renderer", "error group was null, group number = %d, total groups = %d", GroupId, pLayers->NumGroups());
			log_error("map_renderer", "this is here to prevent a crash but the source of this is unknown, please report this for it to get fixed");
			log_error("map_renderer", "we need mapname and crc and the map that caused this if possible, and anymore info you think is relevant");
			continue;
		}

		for(int LayerId = 0; LayerId < pGroup->m_NumLayers; LayerId++)
		{
			CMapItemLayer *pLayer = pLayers->GetLayer(pGroup->m_StartLayer + LayerId);
			int LayerType = GetLayerType(pLayer, pLayers);
			PassedGameLayer |= LayerType == LAYER_GAME;

			if(Type == ERenderType::RENDERTYPE_BACKGROUND_FORCE || Type == ERenderType::RENDERTYPE_BACKGROUND)
			{
				if(PassedGameLayer)
					return;
			}
			else if(Type == ERenderType::RENDERTYPE_FOREGROUND)
			{
				if(!PassedGameLayer)
					continue;
			}

			if(pRenderLayerGroup)
				m_vpRenderLayers.push_back(std::move(pRenderLayerGroup));

			std::unique_ptr<CRenderLayer> pRenderLayer;

			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CMapItemLayerTilemap *pTileLayer = (CMapItemLayerTilemap *)pLayer;

				switch(LayerType)
				{
				case LAYER_DEFAULT_TILESET:
					pRenderLayer = std::make_unique<CRenderLayerTile>(
						GroupId,
						LayerId,
						pLayer->m_Flags,
						pTileLayer);
					break;
				case LAYER_GAME:
					pRenderLayer = std::make_unique<CRenderLayerEntityGame>(
						GroupId,
						LayerId,
						pLayer->m_Flags,
						pTileLayer);
					break;
				case LAYER_FRONT:
					pRenderLayer = std::make_unique<CRenderLayerEntityFront>(
						GroupId,
						LayerId,
						pLayer->m_Flags,
						pTileLayer);
					break;
				case LAYER_TELE:
					pRenderLayer = std::make_unique<CRenderLayerEntityTele>(
						GroupId,
						LayerId,
						pLayer->m_Flags,
						pTileLayer);
					break;
				case LAYER_SPEEDUP:
					pRenderLayer = std::make_unique<CRenderLayerEntitySpeedup>(
						GroupId,
						LayerId,
						pLayer->m_Flags,
						pTileLayer);
					break;
				case LAYER_SWITCH:
					pRenderLayer = std::make_unique<CRenderLayerEntitySwitch>(
						GroupId,
						LayerId,
						pLayer->m_Flags,
						pTileLayer);
					break;
				case LAYER_TUNE:
					pRenderLayer = std::make_unique<CRenderLayerEntityTune>(
						GroupId,
						LayerId,
						pLayer->m_Flags,
						pTileLayer);
					break;
				default:
					dbg_assert_failed("Unknown LayerType %d", LayerType);
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				CMapItemLayerQuads *pQLayer = (CMapItemLayerQuads *)pLayer;

				pRenderLayer = std::make_unique<CRenderLayerQuads>(
					GroupId,
					LayerId,
					pLayer->m_Flags,
					pQLayer);
			}

			// just ignore invalid layers from rendering
			if(pRenderLayer)
			{
				pRenderLayer->OnInit(Graphics(), TextRender(), RenderMap(), pEnvelopeManager, pLayers->Map(), pMapImages, RenderCallbackOptional);
				if(pRenderLayer->IsValid())
				{
					pRenderLayer->Init();
					m_vpRenderLayers.push_back(std::move(pRenderLayer));
				}
			}
		}
	}
}

void CMapRenderer::Render(const CRenderLayerParams &Params)
{
	float ScreenXLeft, ScreenYTop, ScreenXRight, ScreenYBottom;
	Graphics()->GetScreen(&ScreenXLeft, &ScreenYTop, &ScreenXRight, &ScreenYBottom);

	bool DoRenderGroup = true;
	for(auto &pRenderLayer : m_vpRenderLayers)
	{
		if(pRenderLayer->IsGroup())
			DoRenderGroup = pRenderLayer->DoRender(Params);

		if(!DoRenderGroup)
			continue;

		if(pRenderLayer->DoRender(Params))
			pRenderLayer->Render(Params);
	}

	// Reset clip from last group
	Graphics()->ClipDisable();

	// don't reset screen on background
	if(Params.m_RenderType != ERenderType::RENDERTYPE_BACKGROUND && Params.m_RenderType != ERenderType::RENDERTYPE_BACKGROUND_FORCE)
	{
		// reset the screen like it was before
		Graphics()->MapScreen(ScreenXLeft, ScreenYTop, ScreenXRight, ScreenYBottom);
	}
	else
	{
		// reset the screen to the default interface
		Graphics()->MapScreenToInterface(Params.m_Center.x, Params.m_Center.y, Params.m_Zoom);
	}
}

int CMapRenderer::GetLayerType(const CMapItemLayer *pLayer, const CLayers *pLayers) const
{
	if(pLayer == (CMapItemLayer *)pLayers->GameLayer())
		return LAYER_GAME;
	else if(pLayer == (CMapItemLayer *)pLayers->FrontLayer())
		return LAYER_FRONT;
	else if(pLayer == (CMapItemLayer *)pLayers->SwitchLayer())
		return LAYER_SWITCH;
	else if(pLayer == (CMapItemLayer *)pLayers->TeleLayer())
		return LAYER_TELE;
	else if(pLayer == (CMapItemLayer *)pLayers->SpeedupLayer())
		return LAYER_SPEEDUP;
	else if(pLayer == (CMapItemLayer *)pLayers->TuneLayer())
		return LAYER_TUNE;
	return LAYER_DEFAULT_TILESET;
}
