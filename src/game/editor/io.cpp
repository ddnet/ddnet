/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "editor.h"
#include <engine/client.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/serverbrowser.h>
#include <engine/shared/datafile.h>
#include <engine/sound.h>
#include <engine/storage.h>

#include <game/gamecore.h>
#include <game/mapitems_ex.h>

template<typename T>
static int MakeVersion(int i, const T &v)
{
	return (i << 16) + sizeof(T);
}

// compatibility with old sound layers
struct CSoundSource_DEPRECATED
{
	CPoint m_Position;
	int m_Loop;
	int m_TimeDelay; // in s
	int m_FalloffDistance;
	int m_PosEnv;
	int m_PosEnvOffset;
	int m_SoundEnv;
	int m_SoundEnvOffset;
};

int CEditor::Save(const char *pFilename)
{
	return m_Map.Save(Kernel()->RequestInterface<IStorage>(), pFilename);
}

int CEditorMap::Save(class IStorage *pStorage, const char *pFileName)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "saving to '%s'...", pFileName);
	m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "editor", aBuf);
	CDataFileWriter df;
	if(!df.Open(pStorage, pFileName))
	{
		str_format(aBuf, sizeof(aBuf), "failed to open file '%s'...", pFileName);
		m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "editor", aBuf);
		return 0;
	}

	// save version
	{
		CMapItemVersion Item;
		Item.m_Version = 1;
		df.AddItem(MAPITEMTYPE_VERSION, 0, sizeof(Item), &Item);
	}

	// save map info
	{
		CMapItemInfoSettings Item;
		Item.m_Version = 1;

		if(m_MapInfo.m_aAuthor[0])
			Item.m_Author = df.AddData(str_length(m_MapInfo.m_aAuthor) + 1, m_MapInfo.m_aAuthor);
		else
			Item.m_Author = -1;
		if(m_MapInfo.m_aVersion[0])
			Item.m_MapVersion = df.AddData(str_length(m_MapInfo.m_aVersion) + 1, m_MapInfo.m_aVersion);
		else
			Item.m_MapVersion = -1;
		if(m_MapInfo.m_aCredits[0])
			Item.m_Credits = df.AddData(str_length(m_MapInfo.m_aCredits) + 1, m_MapInfo.m_aCredits);
		else
			Item.m_Credits = -1;
		if(m_MapInfo.m_aLicense[0])
			Item.m_License = df.AddData(str_length(m_MapInfo.m_aLicense) + 1, m_MapInfo.m_aLicense);
		else
			Item.m_License = -1;

		Item.m_Settings = -1;
		if(!m_vSettings.empty())
		{
			int Size = 0;
			for(const auto &Setting : m_vSettings)
			{
				Size += str_length(Setting.m_aCommand) + 1;
			}

			char *pSettings = (char *)malloc(maximum(Size, 1));
			char *pNext = pSettings;
			for(const auto &Setting : m_vSettings)
			{
				int Length = str_length(Setting.m_aCommand) + 1;
				mem_copy(pNext, Setting.m_aCommand, Length);
				pNext += Length;
			}
			Item.m_Settings = df.AddData(Size, pSettings);
			free(pSettings);
		}

		df.AddItem(MAPITEMTYPE_INFO, 0, sizeof(Item), &Item);
	}

	// save images
	for(size_t i = 0; i < m_vpImages.size(); i++)
	{
		CEditorImage *pImg = m_vpImages[i];

		// analyse the image for when saving (should be done when we load the image)
		// TODO!
		pImg->AnalyseTileFlags();

		CMapItemImage Item;
		Item.m_Version = 1;

		Item.m_Width = pImg->m_Width;
		Item.m_Height = pImg->m_Height;
		Item.m_External = pImg->m_External;
		Item.m_ImageName = df.AddData(str_length(pImg->m_aName) + 1, pImg->m_aName);
		if(pImg->m_External)
			Item.m_ImageData = -1;
		else
		{
			if(pImg->m_Format == CImageInfo::FORMAT_RGB)
			{
				// Convert to RGBA
				unsigned char *pDataRGBA = (unsigned char *)malloc((size_t)Item.m_Width * Item.m_Height * 4);
				unsigned char *pDataRGB = (unsigned char *)pImg->m_pData;
				for(int j = 0; j < Item.m_Width * Item.m_Height; j++)
				{
					pDataRGBA[j * 4] = pDataRGB[j * 3];
					pDataRGBA[j * 4 + 1] = pDataRGB[j * 3 + 1];
					pDataRGBA[j * 4 + 2] = pDataRGB[j * 3 + 2];
					pDataRGBA[j * 4 + 3] = 255;
				}
				Item.m_ImageData = df.AddData(Item.m_Width * Item.m_Height * 4, pDataRGBA);
				free(pDataRGBA);
			}
			else
			{
				Item.m_ImageData = df.AddData(Item.m_Width * Item.m_Height * 4, pImg->m_pData);
			}
		}
		df.AddItem(MAPITEMTYPE_IMAGE, i, sizeof(Item), &Item);
	}

	// save sounds
	for(size_t i = 0; i < m_vpSounds.size(); i++)
	{
		CEditorSound *pSound = m_vpSounds[i];

		CMapItemSound Item;
		Item.m_Version = 1;

		Item.m_External = 0;
		Item.m_SoundName = df.AddData(str_length(pSound->m_aName) + 1, pSound->m_aName);
		Item.m_SoundData = df.AddData(pSound->m_DataSize, pSound->m_pData);
		Item.m_SoundDataSize = pSound->m_DataSize;

		df.AddItem(MAPITEMTYPE_SOUND, i, sizeof(Item), &Item);
	}

	// save layers
	int LayerCount = 0, GroupCount = 0;
	int AutomapperCount = 0;
	for(const auto &pGroup : m_vpGroups)
	{
		CMapItemGroup GItem;
		GItem.m_Version = CMapItemGroup::CURRENT_VERSION;

		GItem.m_ParallaxX = pGroup->m_ParallaxX;
		GItem.m_ParallaxY = pGroup->m_ParallaxY;
		GItem.m_OffsetX = pGroup->m_OffsetX;
		GItem.m_OffsetY = pGroup->m_OffsetY;
		GItem.m_UseClipping = pGroup->m_UseClipping;
		GItem.m_ClipX = pGroup->m_ClipX;
		GItem.m_ClipY = pGroup->m_ClipY;
		GItem.m_ClipW = pGroup->m_ClipW;
		GItem.m_ClipH = pGroup->m_ClipH;
		GItem.m_StartLayer = LayerCount;
		GItem.m_NumLayers = 0;

		// save group name
		StrToInts(GItem.m_aName, sizeof(GItem.m_aName) / sizeof(int), pGroup->m_aName);

		CMapItemGroupEx GItemEx;
		GItemEx.m_Version = CMapItemGroupEx::CURRENT_VERSION;
		GItemEx.m_ParallaxZoom = pGroup->m_ParallaxZoom;

		for(const auto &pLayer : pGroup->m_vpLayers)
		{
			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", "saving tiles layer");
				CLayerTiles *pLayerTiles = (CLayerTiles *)pLayer;
				pLayerTiles->PrepareForSave();

				CMapItemLayerTilemap Item;
				Item.m_Version = 3;

				Item.m_Layer.m_Version = 0; // was previously uninitialized, do not rely on it being 0
				Item.m_Layer.m_Flags = pLayerTiles->m_Flags;
				Item.m_Layer.m_Type = pLayerTiles->m_Type;

				Item.m_Color = pLayerTiles->m_Color;
				Item.m_ColorEnv = pLayerTiles->m_ColorEnv;
				Item.m_ColorEnvOffset = pLayerTiles->m_ColorEnvOffset;

				Item.m_Width = pLayerTiles->m_Width;
				Item.m_Height = pLayerTiles->m_Height;
				// Item.m_Flags = pLayerTiles->m_Game ? TILESLAYERFLAG_GAME : 0;

				if(pLayerTiles->m_Tele)
					Item.m_Flags = TILESLAYERFLAG_TELE;
				else if(pLayerTiles->m_Speedup)
					Item.m_Flags = TILESLAYERFLAG_SPEEDUP;
				else if(pLayerTiles->m_Front)
					Item.m_Flags = TILESLAYERFLAG_FRONT;
				else if(pLayerTiles->m_Switch)
					Item.m_Flags = TILESLAYERFLAG_SWITCH;
				else if(pLayerTiles->m_Tune)
					Item.m_Flags = TILESLAYERFLAG_TUNE;
				else
					Item.m_Flags = pLayerTiles->m_Game ? TILESLAYERFLAG_GAME : 0;

				Item.m_Image = pLayerTiles->m_Image;

				// the following values were previously uninitialized, do not rely on them being -1 when unused
				Item.m_Tele = -1;
				Item.m_Speedup = -1;
				Item.m_Front = -1;
				Item.m_Switch = -1;
				Item.m_Tune = -1;

				if(Item.m_Flags && !(pLayerTiles->m_Game))
				{
					CTile *pEmptyTiles = (CTile *)calloc((size_t)pLayerTiles->m_Width * pLayerTiles->m_Height, sizeof(CTile));
					mem_zero(pEmptyTiles, (size_t)pLayerTiles->m_Width * pLayerTiles->m_Height * sizeof(CTile));
					Item.m_Data = df.AddData((size_t)pLayerTiles->m_Width * pLayerTiles->m_Height * sizeof(CTile), pEmptyTiles);
					free(pEmptyTiles);

					if(pLayerTiles->m_Tele)
						Item.m_Tele = df.AddData((size_t)pLayerTiles->m_Width * pLayerTiles->m_Height * sizeof(CTeleTile), ((CLayerTele *)pLayerTiles)->m_pTeleTile);
					else if(pLayerTiles->m_Speedup)
						Item.m_Speedup = df.AddData((size_t)pLayerTiles->m_Width * pLayerTiles->m_Height * sizeof(CSpeedupTile), ((CLayerSpeedup *)pLayerTiles)->m_pSpeedupTile);
					else if(pLayerTiles->m_Front)
						Item.m_Front = df.AddData((size_t)pLayerTiles->m_Width * pLayerTiles->m_Height * sizeof(CTile), pLayerTiles->m_pTiles);
					else if(pLayerTiles->m_Switch)
						Item.m_Switch = df.AddData((size_t)pLayerTiles->m_Width * pLayerTiles->m_Height * sizeof(CSwitchTile), ((CLayerSwitch *)pLayerTiles)->m_pSwitchTile);
					else if(pLayerTiles->m_Tune)
						Item.m_Tune = df.AddData((size_t)pLayerTiles->m_Width * pLayerTiles->m_Height * sizeof(CTuneTile), ((CLayerTune *)pLayerTiles)->m_pTuneTile);
				}
				else
					Item.m_Data = df.AddData((size_t)pLayerTiles->m_Width * pLayerTiles->m_Height * sizeof(CTile), pLayerTiles->m_pTiles);

				// save layer name
				StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), pLayerTiles->m_aName);

				df.AddItem(MAPITEMTYPE_LAYER, LayerCount, sizeof(Item), &Item);

				// save auto mapper of each tile layer (not physics layer)
				if(!Item.m_Flags)
				{
					CMapItemAutoMapperConfig ItemAutomapper;
					ItemAutomapper.m_Version = CMapItemAutoMapperConfig::CURRENT_VERSION;
					ItemAutomapper.m_GroupId = GroupCount;
					ItemAutomapper.m_LayerId = GItem.m_NumLayers;
					ItemAutomapper.m_AutomapperConfig = pLayerTiles->m_AutoMapperConfig;
					ItemAutomapper.m_AutomapperSeed = pLayerTiles->m_Seed;
					ItemAutomapper.m_Flags = 0;
					if(pLayerTiles->m_AutoAutoMap)
						ItemAutomapper.m_Flags |= CMapItemAutoMapperConfig::FLAG_AUTOMATIC;

					df.AddItem(MAPITEMTYPE_AUTOMAPPER_CONFIG, AutomapperCount, sizeof(ItemAutomapper), &ItemAutomapper);
					AutomapperCount++;
				}

				GItem.m_NumLayers++;
				LayerCount++;
			}
			else if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", "saving quads layer");
				CLayerQuads *pLayerQuads = (CLayerQuads *)pLayer;
				if(!pLayerQuads->m_vQuads.empty())
				{
					CMapItemLayerQuads Item;
					Item.m_Version = 2;
					Item.m_Layer.m_Version = 0; // was previously uninitialized, do not rely on it being 0
					Item.m_Layer.m_Flags = pLayerQuads->m_Flags;
					Item.m_Layer.m_Type = pLayerQuads->m_Type;
					Item.m_Image = pLayerQuads->m_Image;

					// add the data
					Item.m_NumQuads = pLayerQuads->m_vQuads.size();
					Item.m_Data = df.AddDataSwapped(pLayerQuads->m_vQuads.size() * sizeof(CQuad), pLayerQuads->m_vQuads.data());

					// save layer name
					StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), pLayerQuads->m_aName);

					df.AddItem(MAPITEMTYPE_LAYER, LayerCount, sizeof(Item), &Item);

					// clean up
					//mem_free(quads);

					GItem.m_NumLayers++;
					LayerCount++;
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", "saving sounds layer");
				CLayerSounds *pLayerSounds = (CLayerSounds *)pLayer;
				if(!pLayerSounds->m_vSources.empty())
				{
					CMapItemLayerSounds Item;
					Item.m_Version = CMapItemLayerSounds::CURRENT_VERSION;
					Item.m_Layer.m_Version = 0; // was previously uninitialized, do not rely on it being 0
					Item.m_Layer.m_Flags = pLayerSounds->m_Flags;
					Item.m_Layer.m_Type = pLayerSounds->m_Type;
					Item.m_Sound = pLayerSounds->m_Sound;

					// add the data
					Item.m_NumSources = pLayerSounds->m_vSources.size();
					Item.m_Data = df.AddDataSwapped(pLayerSounds->m_vSources.size() * sizeof(CSoundSource), pLayerSounds->m_vSources.data());

					// save layer name
					StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), pLayerSounds->m_aName);

					df.AddItem(MAPITEMTYPE_LAYER, LayerCount, sizeof(Item), &Item);
					GItem.m_NumLayers++;
					LayerCount++;
				}
			}
		}

		df.AddItem(MAPITEMTYPE_GROUP, GroupCount, sizeof(GItem), &GItem);
		df.AddItem(MAPITEMTYPE_GROUP_EX, GroupCount, sizeof(GItemEx), &GItemEx);
		GroupCount++;
	}

	// save envelopes
	int PointCount = 0;
	for(size_t e = 0; e < m_vpEnvelopes.size(); e++)
	{
		CMapItemEnvelope Item;
		Item.m_Version = CMapItemEnvelope::CURRENT_VERSION;
		Item.m_Channels = m_vpEnvelopes[e]->m_Channels;
		Item.m_StartPoint = PointCount;
		Item.m_NumPoints = m_vpEnvelopes[e]->m_vPoints.size();
		Item.m_Synchronized = m_vpEnvelopes[e]->m_Synchronized;
		StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), m_vpEnvelopes[e]->m_aName);

		df.AddItem(MAPITEMTYPE_ENVELOPE, e, sizeof(Item), &Item);
		PointCount += Item.m_NumPoints;
	}

	// save points
	int TotalSize = sizeof(CEnvPoint) * PointCount;
	CEnvPoint *pPoints = (CEnvPoint *)calloc(maximum(PointCount, 1), sizeof(*pPoints));
	PointCount = 0;

	for(const auto &pEnvelope : m_vpEnvelopes)
	{
		int Count = pEnvelope->m_vPoints.size();
		mem_copy(&pPoints[PointCount], pEnvelope->m_vPoints.data(), sizeof(CEnvPoint) * Count);
		PointCount += Count;
	}

	df.AddItem(MAPITEMTYPE_ENVPOINTS, 0, TotalSize, pPoints);
	free(pPoints);

	// finish the data file
	df.Finish();
	m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", "saving done");

	// send rcon.. if we can
	if(m_pEditor->Client()->RconAuthed())
	{
		CServerInfo CurrentServerInfo;
		m_pEditor->Client()->GetServerInfo(&CurrentServerInfo);
		NETADDR ServerAddr = m_pEditor->Client()->ServerAddress();
		const unsigned char aIpv4Localhost[16] = {127, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		const unsigned char aIpv6Localhost[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

		// and if we're on localhost
		if(!mem_comp(ServerAddr.ip, aIpv4Localhost, sizeof(aIpv4Localhost)) || !mem_comp(ServerAddr.ip, aIpv6Localhost, sizeof(aIpv6Localhost)))
		{
			char aMapName[128];
			IStorage::StripPathAndExtension(pFileName, aMapName, sizeof(aMapName));
			if(!str_comp(aMapName, CurrentServerInfo.m_aMap))
				m_pEditor->Client()->Rcon("reload");
		}
	}

	return 1;
}

int CEditor::Load(const char *pFileName, int StorageType)
{
	Reset();
	int result = m_Map.Load(Kernel()->RequestInterface<IStorage>(), pFileName, StorageType);
	if(result)
	{
		str_copy(m_aFileName, pFileName, 512);
		SortImages();
		SelectGameLayer();
	}
	else
	{
		m_aFileName[0] = 0;
		Reset();
	}
	return result;
}

int CEditorMap::Load(class IStorage *pStorage, const char *pFileName, int StorageType)
{
	CDataFileReader DataFile;
	if(!DataFile.Open(pStorage, pFileName, StorageType))
		return 0;

	Clean();

	// check version
	CMapItemVersion *pItemVersion = (CMapItemVersion *)DataFile.FindItem(MAPITEMTYPE_VERSION, 0);
	if(!pItemVersion)
	{
		return 0;
	}
	else if(pItemVersion->m_Version == 1)
	{
		// load map info
		{
			int Start, Num;
			DataFile.GetType(MAPITEMTYPE_INFO, &Start, &Num);
			for(int i = Start; i < Start + Num; i++)
			{
				int ItemSize = DataFile.GetItemSize(Start);
				int ItemID;
				CMapItemInfoSettings *pItem = (CMapItemInfoSettings *)DataFile.GetItem(i, nullptr, &ItemID);
				if(!pItem || ItemID != 0)
					continue;

				if(pItem->m_Author > -1)
					str_copy(m_MapInfo.m_aAuthor, (char *)DataFile.GetData(pItem->m_Author), sizeof(m_MapInfo.m_aAuthor));
				if(pItem->m_MapVersion > -1)
					str_copy(m_MapInfo.m_aVersion, (char *)DataFile.GetData(pItem->m_MapVersion), sizeof(m_MapInfo.m_aVersion));
				if(pItem->m_Credits > -1)
					str_copy(m_MapInfo.m_aCredits, (char *)DataFile.GetData(pItem->m_Credits), sizeof(m_MapInfo.m_aCredits));
				if(pItem->m_License > -1)
					str_copy(m_MapInfo.m_aLicense, (char *)DataFile.GetData(pItem->m_License), sizeof(m_MapInfo.m_aLicense));

				if(pItem->m_Version != 1 || ItemSize < (int)sizeof(CMapItemInfoSettings))
					break;

				if(!(pItem->m_Settings > -1))
					break;

				const unsigned Size = DataFile.GetDataSize(pItem->m_Settings);
				char *pSettings = (char *)DataFile.GetData(pItem->m_Settings);
				char *pNext = pSettings;
				while(pNext < pSettings + Size)
				{
					int StrSize = str_length(pNext) + 1;
					CSetting Setting;
					str_copy(Setting.m_aCommand, pNext, sizeof(Setting.m_aCommand));
					m_vSettings.push_back(Setting);
					pNext += StrSize;
				}
			}
		}

		// load images
		{
			int Start, Num;
			DataFile.GetType(MAPITEMTYPE_IMAGE, &Start, &Num);
			for(int i = 0; i < Num; i++)
			{
				CMapItemImage *pItem = (CMapItemImage *)DataFile.GetItem(Start + i, nullptr, nullptr);
				char *pName = (char *)DataFile.GetData(pItem->m_ImageName);

				// copy base info
				CEditorImage *pImg = new CEditorImage(m_pEditor);
				pImg->m_External = pItem->m_External;

				if(pItem->m_External)
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "mapres/%s.png", pName);

					// load external
					CEditorImage ImgInfo(m_pEditor);
					if(m_pEditor->Graphics()->LoadPNG(&ImgInfo, aBuf, IStorage::TYPE_ALL))
					{
						*pImg = ImgInfo;
						int TextureLoadFlag = m_pEditor->Graphics()->HasTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE;
						if(ImgInfo.m_Width % 16 != 0 || ImgInfo.m_Height % 16 != 0)
							TextureLoadFlag = 0;
						pImg->m_Texture = m_pEditor->Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, CImageInfo::FORMAT_AUTO, TextureLoadFlag, aBuf);
						ImgInfo.m_pData = nullptr;
						pImg->m_External = 1;
					}
				}
				else
				{
					pImg->m_Width = pItem->m_Width;
					pImg->m_Height = pItem->m_Height;
					pImg->m_Format = CImageInfo::FORMAT_RGBA;

					// copy image data
					void *pData = DataFile.GetData(pItem->m_ImageData);
					pImg->m_pData = malloc((size_t)pImg->m_Width * pImg->m_Height * 4);
					mem_copy(pImg->m_pData, pData, pImg->m_Width * pImg->m_Height * 4);
					int TextureLoadFlag = m_pEditor->Graphics()->HasTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE;
					if(pImg->m_Width % 16 != 0 || pImg->m_Height % 16 != 0)
						TextureLoadFlag = 0;
					pImg->m_Texture = m_pEditor->Graphics()->LoadTextureRaw(pImg->m_Width, pImg->m_Height, pImg->m_Format, pImg->m_pData, CImageInfo::FORMAT_AUTO, TextureLoadFlag);
				}

				// copy image name
				if(pName)
					str_copy(pImg->m_aName, pName, 128);

				// load auto mapper file
				pImg->m_AutoMapper.Load(pImg->m_aName);

				m_vpImages.push_back(pImg);

				// unload image
				DataFile.UnloadData(pItem->m_ImageData);
				DataFile.UnloadData(pItem->m_ImageName);
			}
		}

		// load sounds
		{
			int Start, Num;
			DataFile.GetType(MAPITEMTYPE_SOUND, &Start, &Num);
			for(int i = 0; i < Num; i++)
			{
				CMapItemSound *pItem = (CMapItemSound *)DataFile.GetItem(Start + i, nullptr, nullptr);
				char *pName = (char *)DataFile.GetData(pItem->m_SoundName);

				// copy base info
				CEditorSound *pSound = new CEditorSound(m_pEditor);

				if(pItem->m_External)
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "mapres/%s.opus", pName);

					// load external
					if(pStorage->ReadFile(pName, IStorage::TYPE_ALL, &pSound->m_pData, &pSound->m_DataSize))
					{
						pSound->m_SoundID = m_pEditor->Sound()->LoadOpusFromMem(pSound->m_pData, pSound->m_DataSize, true);
					}
				}
				else
				{
					pSound->m_DataSize = pItem->m_SoundDataSize;

					// copy sample data
					void *pData = DataFile.GetData(pItem->m_SoundData);
					pSound->m_pData = malloc(pSound->m_DataSize);
					mem_copy(pSound->m_pData, pData, pSound->m_DataSize);
					pSound->m_SoundID = m_pEditor->Sound()->LoadOpusFromMem(pSound->m_pData, pSound->m_DataSize, true);
				}

				// copy image name
				if(pName)
					str_copy(pSound->m_aName, pName, sizeof(pSound->m_aName));

				m_vpSounds.push_back(pSound);

				// unload image
				DataFile.UnloadData(pItem->m_SoundData);
				DataFile.UnloadData(pItem->m_SoundName);
			}
		}

		// load groups
		{
			int LayersStart, LayersNum;
			DataFile.GetType(MAPITEMTYPE_LAYER, &LayersStart, &LayersNum);

			int Start, Num;
			DataFile.GetType(MAPITEMTYPE_GROUP, &Start, &Num);

			int StartEx, NumEx;
			DataFile.GetType(MAPITEMTYPE_GROUP_EX, &StartEx, &NumEx);
			for(int g = 0; g < Num; g++)
			{
				CMapItemGroup *pGItem = (CMapItemGroup *)DataFile.GetItem(Start + g, nullptr, nullptr);
				CMapItemGroupEx *pGItemEx = nullptr;
				if(NumEx)
					pGItemEx = (CMapItemGroupEx *)DataFile.GetItem(StartEx + g, nullptr, nullptr);

				if(pGItem->m_Version < 1 || pGItem->m_Version > CMapItemGroup::CURRENT_VERSION)
					continue;

				CLayerGroup *pGroup = NewGroup();
				pGroup->m_ParallaxX = pGItem->m_ParallaxX;
				pGroup->m_ParallaxY = pGItem->m_ParallaxY;
				pGroup->m_OffsetX = pGItem->m_OffsetX;
				pGroup->m_OffsetY = pGItem->m_OffsetY;

				if(pGItem->m_Version >= 2)
				{
					pGroup->m_UseClipping = pGItem->m_UseClipping;
					pGroup->m_ClipX = pGItem->m_ClipX;
					pGroup->m_ClipY = pGItem->m_ClipY;
					pGroup->m_ClipW = pGItem->m_ClipW;
					pGroup->m_ClipH = pGItem->m_ClipH;
				}

				// load group name
				if(pGItem->m_Version >= 3)
					IntsToStr(pGItem->m_aName, sizeof(pGroup->m_aName) / sizeof(int), pGroup->m_aName);

				pGroup->m_ParallaxZoom = GetParallaxZoom(pGItem, pGItemEx);
				pGroup->m_CustomParallaxZoom = pGroup->m_ParallaxZoom != GetParallaxZoomDefault(pGroup->m_ParallaxX, pGroup->m_ParallaxY);

				for(int l = 0; l < pGItem->m_NumLayers; l++)
				{
					CLayer *pLayer = nullptr;
					CMapItemLayer *pLayerItem = (CMapItemLayer *)DataFile.GetItem(LayersStart + pGItem->m_StartLayer + l, nullptr, nullptr);
					if(!pLayerItem)
						continue;

					if(pLayerItem->m_Type == LAYERTYPE_TILES)
					{
						CMapItemLayerTilemap *pTilemapItem = (CMapItemLayerTilemap *)pLayerItem;
						CLayerTiles *pTiles = nullptr;

						if(pTilemapItem->m_Flags & TILESLAYERFLAG_GAME)
						{
							pTiles = new CLayerGame(pTilemapItem->m_Width, pTilemapItem->m_Height);
							MakeGameLayer(pTiles);
							MakeGameGroup(pGroup);
						}
						else if(pTilemapItem->m_Flags & TILESLAYERFLAG_TELE)
						{
							if(pTilemapItem->m_Version <= 2)
								pTilemapItem->m_Tele = *((int *)(pTilemapItem) + 15);

							pTiles = new CLayerTele(pTilemapItem->m_Width, pTilemapItem->m_Height);
							MakeTeleLayer(pTiles);
						}
						else if(pTilemapItem->m_Flags & TILESLAYERFLAG_SPEEDUP)
						{
							if(pTilemapItem->m_Version <= 2)
								pTilemapItem->m_Speedup = *((int *)(pTilemapItem) + 16);

							pTiles = new CLayerSpeedup(pTilemapItem->m_Width, pTilemapItem->m_Height);
							MakeSpeedupLayer(pTiles);
						}
						else if(pTilemapItem->m_Flags & TILESLAYERFLAG_FRONT)
						{
							if(pTilemapItem->m_Version <= 2)
								pTilemapItem->m_Front = *((int *)(pTilemapItem) + 17);

							pTiles = new CLayerFront(pTilemapItem->m_Width, pTilemapItem->m_Height);
							MakeFrontLayer(pTiles);
						}
						else if(pTilemapItem->m_Flags & TILESLAYERFLAG_SWITCH)
						{
							if(pTilemapItem->m_Version <= 2)
								pTilemapItem->m_Switch = *((int *)(pTilemapItem) + 18);

							pTiles = new CLayerSwitch(pTilemapItem->m_Width, pTilemapItem->m_Height);
							MakeSwitchLayer(pTiles);
						}
						else if(pTilemapItem->m_Flags & TILESLAYERFLAG_TUNE)
						{
							if(pTilemapItem->m_Version <= 2)
								pTilemapItem->m_Tune = *((int *)(pTilemapItem) + 19);

							pTiles = new CLayerTune(pTilemapItem->m_Width, pTilemapItem->m_Height);
							MakeTuneLayer(pTiles);
						}
						else
						{
							pTiles = new CLayerTiles(pTilemapItem->m_Width, pTilemapItem->m_Height);
							pTiles->m_pEditor = m_pEditor;
							pTiles->m_Color = pTilemapItem->m_Color;
							pTiles->m_ColorEnv = pTilemapItem->m_ColorEnv;
							pTiles->m_ColorEnvOffset = pTilemapItem->m_ColorEnvOffset;
						}

						pLayer = pTiles;

						pGroup->AddLayer(pTiles);
						pTiles->m_Image = pTilemapItem->m_Image;
						pTiles->m_Game = pTilemapItem->m_Flags & TILESLAYERFLAG_GAME;

						// load layer name
						if(pTilemapItem->m_Version >= 3)
							IntsToStr(pTilemapItem->m_aName, sizeof(pTiles->m_aName) / sizeof(int), pTiles->m_aName);

						if(pTiles->m_Tele)
						{
							void *pTeleData = DataFile.GetData(pTilemapItem->m_Tele);
							unsigned int Size = DataFile.GetDataSize(pTilemapItem->m_Tele);
							if(Size >= (size_t)pTiles->m_Width * pTiles->m_Height * sizeof(CTeleTile))
							{
								static const int s_aTilesRep[] = {
									TILE_TELEIN,
									TILE_TELEINEVIL,
									TILE_TELEOUT,
									TILE_TELECHECK,
									TILE_TELECHECKIN,
									TILE_TELECHECKINEVIL,
									TILE_TELECHECKOUT,
									TILE_TELEINWEAPON,
									TILE_TELEINHOOK};
								CTeleTile *pLayerTeleTiles = ((CLayerTele *)pTiles)->m_pTeleTile;
								mem_copy(pLayerTeleTiles, pTeleData, (size_t)pTiles->m_Width * pTiles->m_Height * sizeof(CTeleTile));

								for(int i = 0; i < pTiles->m_Width * pTiles->m_Height; i++)
								{
									pTiles->m_pTiles[i].m_Index = 0;
									for(int TilesRep : s_aTilesRep)
									{
										if(pLayerTeleTiles[i].m_Type == TilesRep)
											pTiles->m_pTiles[i].m_Index = TilesRep;
									}
								}
							}
							DataFile.UnloadData(pTilemapItem->m_Tele);
						}
						else if(pTiles->m_Speedup)
						{
							void *pSpeedupData = DataFile.GetData(pTilemapItem->m_Speedup);
							unsigned int Size = DataFile.GetDataSize(pTilemapItem->m_Speedup);

							if(Size >= (size_t)pTiles->m_Width * pTiles->m_Height * sizeof(CSpeedupTile))
							{
								CSpeedupTile *pLayerSpeedupTiles = ((CLayerSpeedup *)pTiles)->m_pSpeedupTile;
								mem_copy(pLayerSpeedupTiles, pSpeedupData, (size_t)pTiles->m_Width * pTiles->m_Height * sizeof(CSpeedupTile));

								for(int i = 0; i < pTiles->m_Width * pTiles->m_Height; i++)
								{
									if(pLayerSpeedupTiles[i].m_Force > 0)
										pTiles->m_pTiles[i].m_Index = TILE_BOOST;
									else
										pTiles->m_pTiles[i].m_Index = 0;
								}
							}

							DataFile.UnloadData(pTilemapItem->m_Speedup);
						}
						else if(pTiles->m_Front)
						{
							void *pFrontData = DataFile.GetData(pTilemapItem->m_Front);
							unsigned int Size = DataFile.GetDataSize(pTilemapItem->m_Front);
							if(Size >= (size_t)pTiles->m_Width * pTiles->m_Height * sizeof(CTile))
								mem_copy(((CLayerFront *)pTiles)->m_pTiles, pFrontData, (size_t)pTiles->m_Width * pTiles->m_Height * sizeof(CTile));

							DataFile.UnloadData(pTilemapItem->m_Front);
						}
						else if(pTiles->m_Switch)
						{
							void *pSwitchData = DataFile.GetData(pTilemapItem->m_Switch);
							unsigned int Size = DataFile.GetDataSize(pTilemapItem->m_Switch);
							if(Size >= (size_t)pTiles->m_Width * pTiles->m_Height * sizeof(CSwitchTile))
							{
								const int s_aTilesComp[] = {
									TILE_SWITCHTIMEDOPEN,
									TILE_SWITCHTIMEDCLOSE,
									TILE_SWITCHOPEN,
									TILE_SWITCHCLOSE,
									TILE_FREEZE,
									TILE_DFREEZE,
									TILE_DUNFREEZE,
									TILE_LFREEZE,
									TILE_LUNFREEZE,
									TILE_HIT_ENABLE,
									TILE_HIT_DISABLE,
									TILE_JUMP,
									TILE_ADD_TIME,
									TILE_SUBTRACT_TIME,
									TILE_ALLOW_TELE_GUN,
									TILE_ALLOW_BLUE_TELE_GUN};
								CSwitchTile *pLayerSwitchTiles = ((CLayerSwitch *)pTiles)->m_pSwitchTile;
								mem_copy(pLayerSwitchTiles, pSwitchData, (size_t)pTiles->m_Width * pTiles->m_Height * sizeof(CSwitchTile));

								for(int i = 0; i < pTiles->m_Width * pTiles->m_Height; i++)
								{
									if(((pLayerSwitchTiles[i].m_Type > (ENTITY_CRAZY_SHOTGUN + ENTITY_OFFSET) && ((CLayerSwitch *)pTiles)->m_pSwitchTile[i].m_Type < (ENTITY_DRAGGER_WEAK + ENTITY_OFFSET)) || ((CLayerSwitch *)pTiles)->m_pSwitchTile[i].m_Type == (ENTITY_LASER_O_FAST + 1 + ENTITY_OFFSET)))
										continue;
									else if(pLayerSwitchTiles[i].m_Type >= (ENTITY_ARMOR_1 + ENTITY_OFFSET) && pLayerSwitchTiles[i].m_Type <= (ENTITY_DOOR + ENTITY_OFFSET))
									{
										pTiles->m_pTiles[i].m_Index = pLayerSwitchTiles[i].m_Type;
										pTiles->m_pTiles[i].m_Flags = pLayerSwitchTiles[i].m_Flags;
										continue;
									}

									for(int TilesComp : s_aTilesComp)
									{
										if(pLayerSwitchTiles[i].m_Type == TilesComp)
										{
											pTiles->m_pTiles[i].m_Index = TilesComp;
											pTiles->m_pTiles[i].m_Flags = pLayerSwitchTiles[i].m_Flags;
										}
									}
								}
							}
							DataFile.UnloadData(pTilemapItem->m_Switch);
						}
						else if(pTiles->m_Tune)
						{
							void *pTuneData = DataFile.GetData(pTilemapItem->m_Tune);
							unsigned int Size = DataFile.GetDataSize(pTilemapItem->m_Tune);
							if(Size >= (size_t)pTiles->m_Width * pTiles->m_Height * sizeof(CTuneTile))
							{
								CTuneTile *pLayerTuneTiles = ((CLayerTune *)pTiles)->m_pTuneTile;
								mem_copy(pLayerTuneTiles, pTuneData, (size_t)pTiles->m_Width * pTiles->m_Height * sizeof(CTuneTile));

								for(int i = 0; i < pTiles->m_Width * pTiles->m_Height; i++)
								{
									if(pLayerTuneTiles[i].m_Type == TILE_TUNE)
										pTiles->m_pTiles[i].m_Index = TILE_TUNE;
									else
										pTiles->m_pTiles[i].m_Index = 0;
								}
							}
							DataFile.UnloadData(pTilemapItem->m_Tune);
						}
						else // regular tile layer or game layer
						{
							void *pData = DataFile.GetData(pTilemapItem->m_Data);
							unsigned int Size = DataFile.GetDataSize(pTilemapItem->m_Data);
							if(Size >= (size_t)pTiles->m_Width * pTiles->m_Height * sizeof(CTile))
							{
								mem_copy(pTiles->m_pTiles, pData, (size_t)pTiles->m_Width * pTiles->m_Height * sizeof(CTile));

								if(pTiles->m_Game && pTilemapItem->m_Version == MakeVersion(1, *pTilemapItem))
								{
									for(int i = 0; i < pTiles->m_Width * pTiles->m_Height; i++)
									{
										if(pTiles->m_pTiles[i].m_Index)
											pTiles->m_pTiles[i].m_Index += ENTITY_OFFSET;
									}
								}
							}
							DataFile.UnloadData(pTilemapItem->m_Data);
						}
					}
					else if(pLayerItem->m_Type == LAYERTYPE_QUADS)
					{
						CMapItemLayerQuads *pQuadsItem = (CMapItemLayerQuads *)pLayerItem;
						CLayerQuads *pQuads = new CLayerQuads;
						pQuads->m_pEditor = m_pEditor;
						pLayer = pQuads;
						pQuads->m_Image = pQuadsItem->m_Image;
						if(pQuads->m_Image < -1 || pQuads->m_Image >= (int)m_vpImages.size())
							pQuads->m_Image = -1;

						// load layer name
						if(pQuadsItem->m_Version >= 2)
							IntsToStr(pQuadsItem->m_aName, sizeof(pQuads->m_aName) / sizeof(int), pQuads->m_aName);

						void *pData = DataFile.GetDataSwapped(pQuadsItem->m_Data);
						pGroup->AddLayer(pQuads);
						pQuads->m_vQuads.resize(pQuadsItem->m_NumQuads);
						mem_copy(pQuads->m_vQuads.data(), pData, sizeof(CQuad) * pQuadsItem->m_NumQuads);
						DataFile.UnloadData(pQuadsItem->m_Data);
					}
					else if(pLayerItem->m_Type == LAYERTYPE_SOUNDS)
					{
						CMapItemLayerSounds *pSoundsItem = (CMapItemLayerSounds *)pLayerItem;
						if(pSoundsItem->m_Version < 1 || pSoundsItem->m_Version > CMapItemLayerSounds::CURRENT_VERSION)
							continue;

						CLayerSounds *pSounds = new CLayerSounds;
						pSounds->m_pEditor = m_pEditor;
						pLayer = pSounds;
						pSounds->m_Sound = pSoundsItem->m_Sound;

						// validate m_Sound
						if(pSounds->m_Sound < -1 || pSounds->m_Sound >= (int)m_vpSounds.size())
							pSounds->m_Sound = -1;

						// load layer name
						if(pSoundsItem->m_Version >= 1)
							IntsToStr(pSoundsItem->m_aName, sizeof(pSounds->m_aName) / sizeof(int), pSounds->m_aName);

						// load data
						void *pData = DataFile.GetDataSwapped(pSoundsItem->m_Data);
						pGroup->AddLayer(pSounds);
						pSounds->m_vSources.resize(pSoundsItem->m_NumSources);
						mem_copy(pSounds->m_vSources.data(), pData, sizeof(CSoundSource) * pSoundsItem->m_NumSources);
						DataFile.UnloadData(pSoundsItem->m_Data);
					}
					else if(pLayerItem->m_Type == LAYERTYPE_SOUNDS_DEPRECATED)
					{
						// compatibility with old sound layers
						CMapItemLayerSounds *pSoundsItem = (CMapItemLayerSounds *)pLayerItem;
						if(pSoundsItem->m_Version < 1 || pSoundsItem->m_Version > CMapItemLayerSounds::CURRENT_VERSION)
							continue;

						CLayerSounds *pSounds = new CLayerSounds;
						pSounds->m_pEditor = m_pEditor;
						pLayer = pSounds;
						pSounds->m_Sound = pSoundsItem->m_Sound;

						// validate m_Sound
						if(pSounds->m_Sound < -1 || pSounds->m_Sound >= (int)m_vpSounds.size())
							pSounds->m_Sound = -1;

						// load layer name
						if(pSoundsItem->m_Version >= 1)
							IntsToStr(pSoundsItem->m_aName, sizeof(pSounds->m_aName) / sizeof(int), pSounds->m_aName);

						// load data
						CSoundSource_DEPRECATED *pData = (CSoundSource_DEPRECATED *)DataFile.GetDataSwapped(pSoundsItem->m_Data);
						pGroup->AddLayer(pSounds);
						pSounds->m_vSources.resize(pSoundsItem->m_NumSources);

						for(int i = 0; i < pSoundsItem->m_NumSources; i++)
						{
							CSoundSource_DEPRECATED *pOldSource = &pData[i];

							CSoundSource &Source = pSounds->m_vSources[i];
							Source.m_Position = pOldSource->m_Position;
							Source.m_Loop = pOldSource->m_Loop;
							Source.m_Pan = true;
							Source.m_TimeDelay = pOldSource->m_TimeDelay;
							Source.m_Falloff = 0;

							Source.m_PosEnv = pOldSource->m_PosEnv;
							Source.m_PosEnvOffset = pOldSource->m_PosEnvOffset;
							Source.m_SoundEnv = pOldSource->m_SoundEnv;
							Source.m_SoundEnvOffset = pOldSource->m_SoundEnvOffset;

							Source.m_Shape.m_Type = CSoundShape::SHAPE_CIRCLE;
							Source.m_Shape.m_Circle.m_Radius = pOldSource->m_FalloffDistance;
						}

						DataFile.UnloadData(pSoundsItem->m_Data);
					}

					if(pLayer)
						pLayer->m_Flags = pLayerItem->m_Flags;
				}
			}
		}

		// load envelopes
		{
			CEnvPoint *pPoints = nullptr;

			{
				int Start, Num;
				DataFile.GetType(MAPITEMTYPE_ENVPOINTS, &Start, &Num);
				if(Num)
					pPoints = (CEnvPoint *)DataFile.GetItem(Start, nullptr, nullptr);
			}

			int Start, Num;
			DataFile.GetType(MAPITEMTYPE_ENVELOPE, &Start, &Num);
			for(int e = 0; e < Num; e++)
			{
				CMapItemEnvelope *pItem = (CMapItemEnvelope *)DataFile.GetItem(Start + e, nullptr, nullptr);
				CEnvelope *pEnv = new CEnvelope(pItem->m_Channels);
				pEnv->m_vPoints.resize(pItem->m_NumPoints);
				mem_copy(pEnv->m_vPoints.data(), &pPoints[pItem->m_StartPoint], sizeof(CEnvPoint) * pItem->m_NumPoints);
				if(pItem->m_aName[0] != -1) // compatibility with old maps
					IntsToStr(pItem->m_aName, sizeof(pItem->m_aName) / sizeof(int), pEnv->m_aName);
				m_vpEnvelopes.push_back(pEnv);
				if(pItem->m_Version >= 2)
					pEnv->m_Synchronized = pItem->m_Synchronized;
			}
		}

		{
			int Start, Num;
			DataFile.GetType(MAPITEMTYPE_AUTOMAPPER_CONFIG, &Start, &Num);
			for(int i = 0; i < Num; i++)
			{
				CMapItemAutoMapperConfig *pItem = (CMapItemAutoMapperConfig *)DataFile.GetItem(Start + i, nullptr, nullptr);
				if(pItem->m_Version == CMapItemAutoMapperConfig::CURRENT_VERSION)
				{
					if(pItem->m_GroupId >= 0 && (size_t)pItem->m_GroupId < m_vpGroups.size() &&
						pItem->m_LayerId >= 0 && (size_t)pItem->m_LayerId < m_vpGroups[pItem->m_GroupId]->m_vpLayers.size())
					{
						CLayer *pLayer = m_vpGroups[pItem->m_GroupId]->m_vpLayers[pItem->m_LayerId];
						if(pLayer->m_Type == LAYERTYPE_TILES)
						{
							CLayerTiles *pTiles = (CLayerTiles *)m_vpGroups[pItem->m_GroupId]->m_vpLayers[pItem->m_LayerId];
							// only load auto mappers for tile layers (not physics layers)
							if(!(pTiles->m_Game || pTiles->m_Tele || pTiles->m_Speedup ||
								   pTiles->m_Front || pTiles->m_Switch || pTiles->m_Tune))
							{
								pTiles->m_AutoMapperConfig = pItem->m_AutomapperConfig;
								pTiles->m_Seed = pItem->m_AutomapperSeed;
								pTiles->m_AutoAutoMap = !!(pItem->m_Flags & CMapItemAutoMapperConfig::FLAG_AUTOMATIC);
							}
						}
					}
				}
			}
		}
	}
	else
		return 0;

	m_Modified = false;
	return 1;
}

static int gs_ModifyAddAmount = 0;
static void ModifyAdd(int *pIndex)
{
	if(*pIndex >= 0)
		*pIndex += gs_ModifyAddAmount;
}

int CEditor::Append(const char *pFileName, int StorageType)
{
	CEditorMap NewMap;
	NewMap.m_pEditor = this;

	int Err;
	Err = NewMap.Load(Kernel()->RequestInterface<IStorage>(), pFileName, StorageType);
	if(!Err)
		return Err;

	// modify indecies
	gs_ModifyAddAmount = m_Map.m_vpImages.size();
	NewMap.ModifyImageIndex(ModifyAdd);

	gs_ModifyAddAmount = m_Map.m_vpEnvelopes.size();
	NewMap.ModifyEnvelopeIndex(ModifyAdd);

	// transfer images
	for(const auto &pImage : NewMap.m_vpImages)
		m_Map.m_vpImages.push_back(pImage);
	NewMap.m_vpImages.clear();

	// transfer envelopes
	for(const auto &pEnvelope : NewMap.m_vpEnvelopes)
		m_Map.m_vpEnvelopes.push_back(pEnvelope);
	NewMap.m_vpEnvelopes.clear();

	// transfer groups

	for(const auto &pGroup : NewMap.m_vpGroups)
	{
		if(pGroup == NewMap.m_pGameGroup)
			delete pGroup;
		else
		{
			pGroup->m_pMap = &m_Map;
			m_Map.m_vpGroups.push_back(pGroup);
		}
	}
	NewMap.m_vpGroups.clear();

	// all done \o/
	return 0;
}
