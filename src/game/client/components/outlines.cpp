#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/animstate.h>
#include <game/client/render.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>
#include <game/mapitems.h>


#include "outlines.h"

void COutlines::OnRender()
{
	if(GameClient()->m_MapLayersBackGround.m_OnlineOnly && Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!g_Config.m_ClOverlayEntities && g_Config.m_ClOutlineEntities)
		return;
	for(int g = 0; g < GameClient()->Layers()->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = GameClient()->Layers()->GetGroup(g);

		if(!pGroup)

			continue;

		bool PassedGameLayer = false;

		CTile *pGameTiles = NULL;

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = GameClient()->Layers()->GetLayer(pGroup->m_StartLayer + l);
			if(!pLayer)
				return;
			bool Render = false;
			bool IsGameLayer = false;
			bool IsFrontLayer = false;
			bool IsSwitchLayer = false;
			bool IsTeleLayer = false;
			bool IsSpeedupLayer = false;
			bool IsTuneLayer = false;
			bool IsEntityLayer = false;

			if(pLayer == (CMapItemLayer *)GameClient()->Layers()->GameLayer())
			{
				IsEntityLayer = IsGameLayer = true;
				PassedGameLayer = true;
			}
			if(pLayer == (CMapItemLayer *)GameClient()->Layers()->TeleLayer())
				IsEntityLayer = IsTeleLayer = true;

			if(g_Config.m_ClOutline && IsGameLayer)
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				if(!pTMap)
					return;
				CTile *pTiles = (CTile *)GameClient()->Layers()->Map()->GetData(pTMap->m_Data);
				if(!pTiles)
					return;
				unsigned int Size = GameClient()->Layers()->Map()->GetDataSize(pTMap->m_Data);
				if(IsGameLayer)
					pGameTiles = pTiles;
				if((g_Config.m_ClOutlineFreeze || g_Config.m_ClOutlineSolid || g_Config.m_ClOutlineUnFreeze) && IsGameLayer && Size >= (size_t)pTMap->m_Width * pTMap->m_Height * sizeof(CTile))
				{
					if(g_Config.m_ClOutlineUnFreeze)
						RenderTools()->RenderGameTileOutlines(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, TILE_UNFREEZE, (float)g_Config.m_ClOutlineAlpha / 100.0f);
					if(g_Config.m_ClOutlineFreeze)
						RenderTools()->RenderGameTileOutlines(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, TILE_FREEZE, (float)g_Config.m_ClOutlineAlpha / 100.0f);
					if(g_Config.m_ClOutlineSolid)
						RenderTools()->RenderGameTileOutlines(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, TILE_SOLID, (float)g_Config.m_ClOutlineAlphaSolid / 100.0f);
				}
			}
			if(g_Config.m_ClOutline && IsTeleLayer)
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				if(!pTMap)
					return;
				CTile *pTiles = (CTile *)GameClient()->Layers()->Map()->GetData(pTMap->m_Data);
				if(!pTiles)
					return;
				unsigned int Size = GameClient()->Layers()->Map()->GetDataSize(pTMap->m_Data);
				if(IsGameLayer)
					pGameTiles = pTiles;
				if(g_Config.m_ClOutlineTele && IsTeleLayer)
				{
					CTeleTile *pTeleTiles = (CTeleTile *)GameClient()->Layers()->Map()->GetData(pTMap->m_Tele);
					unsigned int TeleSize = GameClient()->Layers()->Map()->GetDataSize(pTMap->m_Tele);
					if(TeleSize >= (size_t)pTMap->m_Width * pTMap->m_Height * sizeof(CTeleTile))
					{
						if(pGameTiles != NULL)
							RenderTools()->RenderTeleOutlines(pGameTiles, pTeleTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, (float)g_Config.m_ClOutlineAlpha / 100.0f);
					}
				}
			}
		}
	}
}