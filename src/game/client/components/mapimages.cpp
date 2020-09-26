/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/serverbrowser.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <game/client/component.h>
#include <game/mapitems.h>

#include "mapimages.h"

CMapImages::CMapImages() :
	CMapImages(100)
{
}

CMapImages::CMapImages(int TextureSize)
{
	m_Count = 0;
	m_TextureScale = TextureSize;
	mem_zero(m_EntitiesIsLoaded, sizeof(m_EntitiesIsLoaded));
	m_SpeedupArrowIsLoaded = false;

	mem_zero(m_aTextureUsedByTileOrQuadLayerFlag, sizeof(m_aTextureUsedByTileOrQuadLayerFlag));

	str_copy(m_aEntitiesPath, "editor/entities_clear", sizeof(m_aEntitiesPath));

	static_assert(sizeof(gs_aModEntitiesNames) / sizeof(gs_aModEntitiesNames[0]) == MAP_IMAGE_MOD_TYPE_COUNT, "Mod name string count is not equal to mod type count");
}

void CMapImages::OnInit()
{
	InitOverlayTextures();

	if(str_comp(g_Config.m_ClAssetsEntites, "default") == 0)
		str_copy(m_aEntitiesPath, "editor/entities_clear", sizeof(m_aEntitiesPath));
	else
	{
		str_format(m_aEntitiesPath, sizeof(m_aEntitiesPath), "assets/entities/%s", g_Config.m_ClAssetsEntites);
	}
}

void CMapImages::OnMapLoadImpl(class CLayers *pLayers, IMap *pMap)
{
	// unload all textures
	for(int i = 0; i < m_Count; i++)
	{
		Graphics()->UnloadTexture(m_aTextures[i]);
		m_aTextures[i] = IGraphics::CTextureHandle();
		m_aTextureUsedByTileOrQuadLayerFlag[i] = 0;
	}
	m_Count = 0;

	int Start;
	pMap->GetType(MAPITEMTYPE_IMAGE, &Start, &m_Count);

	m_Count = clamp(m_Count, 0, 64);

	for(int g = 0; g < pLayers->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = pLayers->GetGroup(g);
		if(!pGroup)
		{
			continue;
		}

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = pLayers->GetLayer(pGroup->m_StartLayer + l);
			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CMapItemLayerTilemap *pTLayer = (CMapItemLayerTilemap *)pLayer;
				if(pTLayer->m_Image != -1 && pTLayer->m_Image < (int)(sizeof(m_aTextures) / sizeof(m_aTextures[0])))
				{
					m_aTextureUsedByTileOrQuadLayerFlag[pTLayer->m_Image] |= 1;
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				CMapItemLayerQuads *pQLayer = (CMapItemLayerQuads *)pLayer;
				if(pQLayer->m_Image != -1 && pQLayer->m_Image < (int)(sizeof(m_aTextures) / sizeof(m_aTextures[0])))
				{
					m_aTextureUsedByTileOrQuadLayerFlag[pQLayer->m_Image] |= 2;
				}
			}
		}
	}

	int TextureLoadFlag = Graphics()->HasTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE;

	// load new textures
	for(int i = 0; i < m_Count; i++)
	{
		int LoadFlag = (((m_aTextureUsedByTileOrQuadLayerFlag[i] & 1) != 0) ? TextureLoadFlag : 0) | (((m_aTextureUsedByTileOrQuadLayerFlag[i] & 2) != 0) ? 0 : (Graphics()->IsTileBufferingEnabled() ? IGraphics::TEXLOAD_NO_2D_TEXTURE : 0));
		CMapItemImage *pImg = (CMapItemImage *)pMap->GetItem(Start + i, 0, 0);
		if(pImg->m_External)
		{
			char Buf[256];
			char *pName = (char *)pMap->GetData(pImg->m_ImageName);
			str_format(Buf, sizeof(Buf), "mapres/%s.png", pName);
			m_aTextures[i] = Graphics()->LoadTexture(Buf, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, LoadFlag);
		}
		else
		{
			void *pData = pMap->GetData(pImg->m_ImageData);
			char *pName = (char *)pMap->GetData(pImg->m_ImageName);
			char aTexName[128];
			str_format(aTexName, sizeof(aTexName), "%s %s", "embedded:", pName);
			m_aTextures[i] = Graphics()->LoadTextureRaw(pImg->m_Width, pImg->m_Height, CImageInfo::FORMAT_RGBA, pData, CImageInfo::FORMAT_RGBA, LoadFlag, aTexName);
			pMap->UnloadData(pImg->m_ImageData);
		}
	}
}

void CMapImages::OnMapLoad()
{
	IMap *pMap = Kernel()->RequestInterface<IMap>();
	CLayers *pLayers = m_pClient->Layers();
	OnMapLoadImpl(pLayers, pMap);
}

void CMapImages::LoadBackground(class CLayers *pLayers, class IMap *pMap)
{
	OnMapLoadImpl(pLayers, pMap);
}

bool CMapImages::HasFrontLayer(EMapImageModType ModType)
{
	return ModType == MAP_IMAGE_MOD_TYPE_DDNET || ModType == MAP_IMAGE_MOD_TYPE_DDRACE;
}

bool CMapImages::HasSpeedupLayer(EMapImageModType ModType)
{
	return ModType == MAP_IMAGE_MOD_TYPE_DDNET || ModType == MAP_IMAGE_MOD_TYPE_DDRACE;
}

bool CMapImages::HasSwitchLayer(EMapImageModType ModType)
{
	return ModType == MAP_IMAGE_MOD_TYPE_DDNET || ModType == MAP_IMAGE_MOD_TYPE_DDRACE;
}

bool CMapImages::HasTeleLayer(EMapImageModType ModType)
{
	return ModType == MAP_IMAGE_MOD_TYPE_DDNET || ModType == MAP_IMAGE_MOD_TYPE_DDRACE;
}

bool CMapImages::HasTuneLayer(EMapImageModType ModType)
{
	return ModType == MAP_IMAGE_MOD_TYPE_DDNET || ModType == MAP_IMAGE_MOD_TYPE_DDRACE;
}

IGraphics::CTextureHandle CMapImages::GetEntities(EMapImageEntityLayerType EntityLayerType)
{
	EMapImageModType EntitiesModType = MAP_IMAGE_MOD_TYPE_DDNET;
	bool EntitesAreMasked = !GameClient()->m_GameInfo.m_DontMaskEntities;

	if(GameClient()->m_GameInfo.m_EntitiesDDNet)
		EntitiesModType = MAP_IMAGE_MOD_TYPE_DDNET;
	else if(GameClient()->m_GameInfo.m_EntitiesDDRace)
		EntitiesModType = MAP_IMAGE_MOD_TYPE_DDRACE;
	else if(GameClient()->m_GameInfo.m_EntitiesRace)
		EntitiesModType = MAP_IMAGE_MOD_TYPE_RACE;
	else if(GameClient()->m_GameInfo.m_EntitiesBW)
		EntitiesModType = MAP_IMAGE_MOD_TYPE_BLOCKWORLDS;
	else if(GameClient()->m_GameInfo.m_EntitiesFNG)
		EntitiesModType = MAP_IMAGE_MOD_TYPE_FNG;
	else if(GameClient()->m_GameInfo.m_EntitiesVanilla)
		EntitiesModType = MAP_IMAGE_MOD_TYPE_VANILLA;

	if(!m_EntitiesIsLoaded[(EntitiesModType * 2) + (int)EntitesAreMasked])
	{
		m_EntitiesIsLoaded[(EntitiesModType * 2) + (int)EntitesAreMasked] = true;

		// any mod that does not mask, will get all layers unmasked
		bool WasUnknwon = !EntitesAreMasked;

		char aPath[64];
		str_format(aPath, sizeof(aPath), "%s/%s.png", m_aEntitiesPath, gs_aModEntitiesNames[EntitiesModType]);

		bool GameTypeHasFrontLayer = HasFrontLayer(EntitiesModType) || WasUnknwon;
		bool GameTypeHasSpeedupLayer = HasSpeedupLayer(EntitiesModType) || WasUnknwon;
		bool GameTypeHasSwitchLayer = HasSwitchLayer(EntitiesModType) || WasUnknwon;
		bool GameTypeHasTeleLayer = HasTeleLayer(EntitiesModType) || WasUnknwon;
		bool GameTypeHasTuneLayer = HasTuneLayer(EntitiesModType) || WasUnknwon;

		int TextureLoadFlag = Graphics()->HasTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE;

		CImageInfo ImgInfo;
		bool ImagePNGLoaded = false;
		if(Graphics()->LoadPNG(&ImgInfo, aPath, IStorage::TYPE_ALL))
			ImagePNGLoaded = true;
		else
		{
			bool TryDefault = true;
			// try as single ddnet replacement
			if(EntitiesModType == MAP_IMAGE_MOD_TYPE_DDNET)
			{
				str_format(aPath, sizeof(aPath), "%s.png", m_aEntitiesPath);
				if(Graphics()->LoadPNG(&ImgInfo, aPath, IStorage::TYPE_ALL))
				{
					ImagePNGLoaded = true;
					TryDefault = false;
				}
			}

			if(!ImagePNGLoaded && TryDefault)
			{
				// try default
				str_format(aPath, sizeof(aPath), "editor/entities_clear/%s.png", gs_aModEntitiesNames[EntitiesModType]);
				if(Graphics()->LoadPNG(&ImgInfo, aPath, IStorage::TYPE_ALL))
				{
					ImagePNGLoaded = true;
				}
			}
		}

		if(ImagePNGLoaded && ImgInfo.m_Width > 0 && ImgInfo.m_Height > 0)
		{
			int ColorChannelCount = 0;
			if(ImgInfo.m_Format == CImageInfo::FORMAT_ALPHA)
				ColorChannelCount = 1;
			else if(ImgInfo.m_Format == CImageInfo::FORMAT_RGB)
				ColorChannelCount = 3;
			else if(ImgInfo.m_Format == CImageInfo::FORMAT_RGBA)
				ColorChannelCount = 4;

			int BuildImageSize = ColorChannelCount * ImgInfo.m_Width * ImgInfo.m_Height;

			uint8_t *pTmpImgData = (uint8_t *)ImgInfo.m_pData;
			uint8_t *pBuildImgData = (uint8_t *)malloc(BuildImageSize);

			// build game layer
			for(int n = 0; n < MAP_IMAGE_ENTITY_LAYER_TYPE_COUNT; ++n)
			{
				bool BuildThisLayer = true;
				if(n == MAP_IMAGE_ENTITY_LAYER_TYPE_FRONT && !GameTypeHasFrontLayer)
					BuildThisLayer = false;
				else if(n == MAP_IMAGE_ENTITY_LAYER_TYPE_SPEEDUP && !GameTypeHasSpeedupLayer)
					BuildThisLayer = false;
				else if(n == MAP_IMAGE_ENTITY_LAYER_TYPE_SWITCH && !GameTypeHasSwitchLayer)
					BuildThisLayer = false;
				else if(n == MAP_IMAGE_ENTITY_LAYER_TYPE_TELE && !GameTypeHasTeleLayer)
					BuildThisLayer = false;
				else if(n == MAP_IMAGE_ENTITY_LAYER_TYPE_TUNE && !GameTypeHasTuneLayer)
					BuildThisLayer = false;

				dbg_assert(m_EntitiesTextures[(EntitiesModType * 2) + (int)EntitesAreMasked][n] == -1, "entities texture already loaded when it should not be");

				if(BuildThisLayer)
				{
					// set everything transparent
					mem_zero(pBuildImgData, BuildImageSize);

					for(int i = 0; i < 256; ++i)
					{
						bool ValidTile = i != 0;
						int TileIndex = i;
						if(EntitesAreMasked)
						{
							if(EntitiesModType == MAP_IMAGE_MOD_TYPE_DDNET || EntitiesModType == MAP_IMAGE_MOD_TYPE_DDRACE)
							{
								if(EntitiesModType == MAP_IMAGE_MOD_TYPE_DDNET || TileIndex != TILE_BOOST)
								{
									if(n == MAP_IMAGE_ENTITY_LAYER_TYPE_GAME && !IsValidGameTile((int)TileIndex))
										ValidTile = false;
									else if(n == MAP_IMAGE_ENTITY_LAYER_TYPE_FRONT && !IsValidFrontTile((int)TileIndex))
										ValidTile = false;
									else if(n == MAP_IMAGE_ENTITY_LAYER_TYPE_SPEEDUP && !IsValidSpeedupTile((int)TileIndex))
										ValidTile = false;
									else if(n == MAP_IMAGE_ENTITY_LAYER_TYPE_SWITCH)
									{
										if(!IsValidSwitchTile((int)TileIndex))
											ValidTile = false;
									}
									else if(n == MAP_IMAGE_ENTITY_LAYER_TYPE_TELE && !IsValidTeleTile((int)TileIndex))
										ValidTile = false;
									else if(n == MAP_IMAGE_ENTITY_LAYER_TYPE_TUNE && !IsValidTuneTile((int)TileIndex))
										ValidTile = false;
								}
							}
							else if((EntitiesModType == MAP_IMAGE_MOD_TYPE_RACE) && IsCreditsTile((int)TileIndex))
							{
								ValidTile = false;
							}
							else if((EntitiesModType == MAP_IMAGE_MOD_TYPE_FNG) && IsCreditsTile((int)TileIndex))
							{
								ValidTile = false;
							}
							else if((EntitiesModType == MAP_IMAGE_MOD_TYPE_VANILLA) && IsCreditsTile((int)TileIndex))
							{
								ValidTile = false;
							}
							/*else if((EntitiesModType == MAP_IMAGE_MOD_TYPE_RACE_BLOCKWORLD) && ...)
							{
								ValidTile = false;
							}*/
						}

						if(EntitiesModType == MAP_IMAGE_MOD_TYPE_DDNET || EntitiesModType == MAP_IMAGE_MOD_TYPE_DDRACE)
						{
							if(n == MAP_IMAGE_ENTITY_LAYER_TYPE_SWITCH && TileIndex == TILE_SWITCHTIMEDOPEN)
								TileIndex = 8;
						}

						int X = TileIndex % 16;
						int Y = TileIndex / 16;

						int CopyWidth = ImgInfo.m_Width / 16;
						int CopyHeight = ImgInfo.m_Height / 16;
						if(ValidTile)
						{
							Graphics()->CopyTextureBufferSub(pBuildImgData, pTmpImgData, ImgInfo.m_Width, ImgInfo.m_Height, ColorChannelCount, X * CopyWidth, Y * CopyHeight, CopyWidth, CopyHeight);
						}
					}

					m_EntitiesTextures[(EntitiesModType * 2) + (int)EntitesAreMasked][n] = Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, pBuildImgData, ImgInfo.m_Format, TextureLoadFlag, aPath);
				}
				else
				{
					if(m_TransparentTexture == -1)
					{
						// set everything transparent
						mem_zero(pBuildImgData, BuildImageSize);

						m_TransparentTexture = Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, pBuildImgData, ImgInfo.m_Format, TextureLoadFlag, aPath);
					}
					m_EntitiesTextures[(EntitiesModType * 2) + (int)EntitesAreMasked][n] = m_TransparentTexture;
				}
			}

			free(pBuildImgData);
		}
	}

	return m_EntitiesTextures[(EntitiesModType * 2) + (int)EntitesAreMasked][EntityLayerType];
}

IGraphics::CTextureHandle CMapImages::GetSpeedupArrow()
{
	if(!m_SpeedupArrowIsLoaded)
	{
		int TextureLoadFlag = (Graphics()->HasTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE_SINGLE_LAYER : IGraphics::TEXLOAD_TO_3D_TEXTURE_SINGLE_LAYER) | IGraphics::TEXLOAD_NO_2D_TEXTURE;
		m_SpeedupArrowTexture = Graphics()->LoadTexture(g_pData->m_aImages[IMAGE_SPEEDUP_ARROW].m_pFilename, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, TextureLoadFlag);

		m_SpeedupArrowIsLoaded = true;
	}

	return m_SpeedupArrowTexture;
}

IGraphics::CTextureHandle CMapImages::GetOverlayBottom()
{
	return m_OverlayBottomTexture;
}

IGraphics::CTextureHandle CMapImages::GetOverlayTop()
{
	return m_OverlayTopTexture;
}

IGraphics::CTextureHandle CMapImages::GetOverlayCenter()
{
	return m_OverlayCenterTexture;
}

void CMapImages::ChangeEntitiesPath(const char *pPath)
{
	if(str_comp(pPath, "default") == 0)
		str_copy(m_aEntitiesPath, "editor/entities_clear", sizeof(m_aEntitiesPath));
	else
	{
		str_format(m_aEntitiesPath, sizeof(m_aEntitiesPath), "assets/entities/%s", pPath);
	}

	for(int i = 0; i < MAP_IMAGE_MOD_TYPE_COUNT * 2; ++i)
	{
		if(m_EntitiesIsLoaded[i])
		{
			for(int n = 0; n < MAP_IMAGE_ENTITY_LAYER_TYPE_COUNT; ++n)
			{
				if(m_EntitiesTextures[i][n] != -1)
					Graphics()->UnloadTexture(m_EntitiesTextures[i][n]);
				m_EntitiesTextures[i][n] = IGraphics::CTextureHandle();
			}

			m_EntitiesIsLoaded[i] = false;
		}
	}
}

void CMapImages::SetTextureScale(int Scale)
{
	if(m_TextureScale == Scale)
		return;

	m_TextureScale = Scale;

	if(Graphics() && m_OverlayCenterTexture != -1) // check if component was initialized
	{
		// reinitialize component
		Graphics()->UnloadTexture(m_OverlayBottomTexture);
		Graphics()->UnloadTexture(m_OverlayTopTexture);
		Graphics()->UnloadTexture(m_OverlayCenterTexture);

		m_OverlayBottomTexture = IGraphics::CTextureHandle();
		m_OverlayTopTexture = IGraphics::CTextureHandle();
		m_OverlayCenterTexture = IGraphics::CTextureHandle();

		InitOverlayTextures();
	}
}

int CMapImages::GetTextureScale()
{
	return m_TextureScale;
}

IGraphics::CTextureHandle CMapImages::UploadEntityLayerText(int TextureSize, int MaxWidth, int YOffset)
{
	void *pMem = calloc(1024 * 1024 * 4, 1);

	UpdateEntityLayerText(pMem, 4, 1024, 1024, TextureSize, MaxWidth, YOffset, 0);
	UpdateEntityLayerText(pMem, 4, 1024, 1024, TextureSize, MaxWidth, YOffset, 1);
	UpdateEntityLayerText(pMem, 4, 1024, 1024, TextureSize, MaxWidth, YOffset, 2, 255);

	int TextureLoadFlag = (Graphics()->HasTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE) | IGraphics::TEXLOAD_NO_2D_TEXTURE;
	IGraphics::CTextureHandle Texture = Graphics()->LoadTextureRaw(1024, 1024, CImageInfo::FORMAT_RGBA, pMem, CImageInfo::FORMAT_RGBA, TextureLoadFlag);
	free(pMem);

	return Texture;
}

void CMapImages::UpdateEntityLayerText(void *pTexBuffer, int ImageColorChannelCount, int TexWidth, int TexHeight, int TextureSize, int MaxWidth, int YOffset, int NumbersPower, int MaxNumber)
{
	char aBuf[4];
	int DigitsCount = NumbersPower + 1;

	int CurrentNumber = pow(10, NumbersPower);

	if(MaxNumber == -1)
		MaxNumber = CurrentNumber * 10 - 1;

	str_format(aBuf, 4, "%d", CurrentNumber);

	int CurrentNumberSuitableFontSize = TextRender()->AdjustFontSize(aBuf, DigitsCount, TextureSize, MaxWidth);
	int UniversalSuitableFontSize = CurrentNumberSuitableFontSize * 0.92f; // should be smoothed enough to fit any digits combination

	YOffset += ((TextureSize - UniversalSuitableFontSize) / 2);

	for(; CurrentNumber <= MaxNumber; ++CurrentNumber)
	{
		str_format(aBuf, 4, "%d", CurrentNumber);

		float x = (CurrentNumber % 16) * 64;
		float y = (CurrentNumber / 16) * 64;

		int ApproximateTextWidth = TextRender()->CalculateTextWidth(aBuf, DigitsCount, 0, UniversalSuitableFontSize);
		int XOffSet = (MaxWidth - clamp(ApproximateTextWidth, 0, MaxWidth)) / 2;

		TextRender()->UploadEntityLayerText(pTexBuffer, ImageColorChannelCount, TexWidth, TexHeight, (TexWidth / 16) - XOffSet, (TexHeight / 16) - YOffset, aBuf, DigitsCount, x + XOffSet, y + YOffset, UniversalSuitableFontSize);
	}
}

void CMapImages::InitOverlayTextures()
{
	int TextureSize = 64 * m_TextureScale / 100;
	int TextureToVerticalCenterOffset = (64 - TextureSize) / 2; // should be used to move texture to the center of 64 pixels area

	if(m_OverlayBottomTexture == -1)
	{
		m_OverlayBottomTexture = UploadEntityLayerText(TextureSize / 2, 64, 32 + TextureToVerticalCenterOffset / 2);
	}

	if(m_OverlayTopTexture == -1)
	{
		m_OverlayTopTexture = UploadEntityLayerText(TextureSize / 2, 64, TextureToVerticalCenterOffset / 2);
	}

	if(m_OverlayCenterTexture == -1)
	{
		m_OverlayCenterTexture = UploadEntityLayerText(TextureSize, 64, TextureToVerticalCenterOffset);
	}
}
