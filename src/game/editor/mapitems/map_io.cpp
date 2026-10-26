#include "document_export.h"
#include "image.h"
#include "sound.h"

#include <base/dbg.h>
#include <base/fs.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/engine.h>
#include <engine/gfx/image_manipulation.h>
#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/filecollection.h>
#include <engine/sound.h>
#include <engine/storage.h>

#include <game/editor/editor.h>
#include <game/gamecore.h>
#include <game/mapitems_ex.h>

// compatibility with old sound layers
class CSoundSourceDeprecated
{
public:
	CPoint m_Position;
	int m_Loop;
	int m_TimeDelay; // in s
	int m_FalloffDistance;
	int m_PosEnv;
	int m_PosEnvOffset;
	int m_SoundEnv;
	int m_SoundEnvOffset;
};

bool CEditorMap::Save(const char *pFilename, const FErrorHandler &ErrorHandler)
{
	return SaveWithKind(pFilename, editor_history::ESaveKind::MANUAL, ErrorHandler);
}

bool CEditorMap::SaveWithKind(const char *pFilename, editor_history::ESaveKind Kind, const FErrorHandler &ErrorHandler)
{
	if(Kind == editor_history::ESaveKind::AUTOMATIC && (m_DocumentHistory.Active() || m_DocumentHistory.Pending() || (Editor()->Map() == this && Editor()->DocumentNumberInputActive())))
		return false;
	if(Kind != editor_history::ESaveKind::AUTOMATIC)
	{
		if((Editor()->Map() == this && Editor()->DocumentNumberInputActive()) || m_DocumentHistory.Pending())
		{
			Editor()->DeferDocumentAction(this, [this, Filename = std::string(pFilename), Kind, ErrorHandler, CloseOnSave = m_CloseOnSave] {
				if(SaveWithKind(Filename.c_str(), Kind, ErrorHandler) && Kind == editor_history::ESaveKind::MANUAL)
					m_CloseOnSave = CloseOnSave;
			});
			return true;
		}
		if(Editor()->Map() == this)
			Editor()->FinishDocumentText(true);
		if(!m_DocumentHistory.CompleteActive(CEditorDocumentHistory::ECompletion::MANUAL_SAVE))
			return false;
		if(Editor()->Map() == this)
			Editor()->Reset();
	}

	if(Editor()->IsSaving(pFilename))
	{
		ErrorHandler("This map file is already being saved. Wait for the save to finish before saving it again.");
		return false;
	}
	char aSaveRoot[IO_MAX_PATH_LENGTH];
	Editor()->Storage()->GetCompletePath(IStorage::TYPE_SAVE, "", aSaveRoot, sizeof(aSaveRoot));
	if(str_length(aSaveRoot) + str_length(pFilename) + 64 >= IO_MAX_PATH_LENGTH)
	{
		ErrorHandler("The map path is too long to create a temporary save file.");
		return false;
	}
	if(!PerformPreSaveSanityChecks(ErrorHandler))
		return false;

	const auto *pHistory = m_DocumentHistory.History();
	if(!pHistory)
	{
		ErrorHandler("The document is not ready for saving.");
		return false;
	}
	auto Ticket = m_pSaveState->Capture(pHistory->PinCurrent(), pHistory->Revisions()[pHistory->Cursor()].m_PersistedKey, pFilename, Kind);
	if(!Ticket)
	{
		ErrorHandler("Could not capture the document for saving.");
		return false;
	}
	char aFilenameBase[IO_MAX_PATH_LENGTH], aFilenameTmp[IO_MAX_PATH_LENGTH];
	str_format(aFilenameBase, sizeof(aFilenameBase), "%s.%" PRIu64, pFilename, Editor()->m_NextSaveJob++);
	IStorage::FormatTmpPath(aFilenameTmp, sizeof(aFilenameTmp), aFilenameBase);
	log_info("editor/save", "Saving map to '%s'...", aFilenameTmp);

	std::string ExportError;
	{
		CDataFileWriter Writer;
		if(!Writer.Open(Editor()->Storage(), aFilenameTmp))
		{
			char aError[IO_MAX_PATH_LENGTH + 64];
			str_format(aError, sizeof(aError), "Error: Failed to open file '%s' for writing.", aFilenameTmp);
			ErrorHandler(aError);
			return false;
		}
		if(WriteEditorDocument(*Ticket->m_pRoot, Writer, ExportError))
		{
			// Native projection stays on the foreground. Only Finish and rename run
			// in the worker, and the snapshot pin is released before submission.
			auto pJob = std::make_shared<CDataFileWriterFinishJob>(Editor()->Storage(), aFilenameTmp, std::move(Writer), std::move(*Ticket));
			if(Kind == editor_history::ESaveKind::AUTOMATIC)
				pJob->SetAutosaveName(m_aAutosaveName, g_Config.m_EdAutosaveMax);
			Editor()->QueueWriterFinishJob(pJob);
			return true;
		}
	} // Close the unfinished writer before removing its temporary file.
	Editor()->Storage()->RemoveFile(aFilenameTmp, IStorage::TYPE_SAVE);
	ErrorHandler(ExportError.c_str());
	return false;
}

bool CEditorMap::PerformPreSaveSanityChecks(const FErrorHandler &ErrorHandler)
{
	bool Success = true;
	char aErrorMessage[256];

	for(const std::shared_ptr<CEditorImage> &pImage : m_vpImages)
	{
		if(!pImage->m_External && pImage->Data() == nullptr)
		{
			str_format(aErrorMessage, sizeof(aErrorMessage), "Error: Saving is not possible because the image '%s' could not be loaded. Remove or replace this image.", pImage->m_aName);
			ErrorHandler(aErrorMessage);
			Success = false;
		}
	}

	for(const std::shared_ptr<CEditorSound> &pSound : m_vpSounds)
	{
		if(pSound->Data() == nullptr)
		{
			str_format(aErrorMessage, sizeof(aErrorMessage), "Error: Saving is not possible because the sound '%s' could not be loaded. Remove or replace this sound.", pSound->m_aName);
			ErrorHandler(aErrorMessage);
			Success = false;
		}
	}

	return Success;
}

bool CEditorMap::Load(const char *pFilename, int StorageType, const FErrorHandler &ErrorHandler)
{
	std::unique_ptr<IMap> pMap = CreateMap();
	if(!pMap->Load(Editor()->Storage(), pFilename, StorageType))
	{
		ErrorHandler("Error: Failed to open map file. See local console for details.");
		return false;
	}

	// load map info
	{
		int Start, Num;
		pMap->GetType(MAPITEMTYPE_INFO, &Start, &Num);
		for(int i = Start; i < Start + Num; i++)
		{
			int ItemSize = pMap->GetItemSize(i);
			int ItemId;
			CMapItemInfoSettings *pItem = (CMapItemInfoSettings *)pMap->GetItem(i, nullptr, &ItemId);
			if(!pItem || ItemId != 0)
				continue;

			const auto &&ReadStringInfo = [&](int Index, char *pBuffer, size_t BufferSize, const char *pErrorContext) {
				const char *pStr = pMap->GetDataString(Index);
				if(pStr == nullptr)
				{
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), "Error: Failed to read %s from map info.", pErrorContext);
					ErrorHandler(aBuf);
					pBuffer[0] = '\0';
				}
				else
				{
					str_copy(pBuffer, pStr, BufferSize);
				}
			};

			ReadStringInfo(pItem->m_Author, m_MapInfo.m_Author.Buffer(), sizeof(m_MapInfo.m_Author.Buffer()), "author");
			ReadStringInfo(pItem->m_MapVersion, m_MapInfo.m_Version.Buffer(), sizeof(m_MapInfo.m_Version.Buffer()), "version");
			ReadStringInfo(pItem->m_Credits, m_MapInfo.m_Credits.Buffer(), sizeof(m_MapInfo.m_Credits.Buffer()), "credits");
			ReadStringInfo(pItem->m_License, m_MapInfo.m_License.Buffer(), sizeof(m_MapInfo.m_License.Buffer()), "license");

			if(pItem->m_Version != 1 || ItemSize < (int)sizeof(CMapItemInfoSettings))
				break;

			if(!(pItem->m_Settings > -1))
				break;

			const unsigned Size = pMap->GetDataSize(pItem->m_Settings);
			char *pSettings = (char *)pMap->GetData(pItem->m_Settings);
			char *pNext = pSettings;
			while(pNext < pSettings + Size)
			{
				int StrSize = str_length(pNext) + 1;
				m_vSettings.emplace_back(pNext);
				pNext += StrSize;
			}
		}
	}

	// load images
	{
		int Start, Num;
		pMap->GetType(MAPITEMTYPE_IMAGE, &Start, &Num);
		for(int i = 0; i < Num; i++)
		{
			CMapItemImage_v2 *pItem = (CMapItemImage_v2 *)pMap->GetItem(Start + i);

			// copy base info
			std::shared_ptr<CEditorImage> pImg = std::make_shared<CEditorImage>(this);
			pImg->m_External = pItem->m_External;

			const char *pName = pMap->GetDataString(pItem->m_ImageName);
			if(pName == nullptr || pName[0] == '\0')
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Error: Failed to read name of image %d.", i);
				ErrorHandler(aBuf);
			}
			else
				str_copy(pImg->m_aName, pName);

			if(pItem->m_Version > 1 && pItem->m_MustBe1 != 1)
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Error: Unsupported image type of image %d '%s'.", i, pImg->m_aName);
				ErrorHandler(aBuf);
			}

			if(pImg->m_External || (pItem->m_Version > 1 && pItem->m_MustBe1 != 1))
			{
				char aBuf[IO_MAX_PATH_LENGTH];
				str_format(aBuf, sizeof(aBuf), "mapres/%s.png", pImg->m_aName);

				// load external
				CImageInfo Image;
				if(m_pEditor->Graphics()->LoadPng(Image, aBuf, IStorage::TYPE_ALL))
				{
					ConvertToRgba(Image);
					*pImg = std::move(Image);

					int TextureLoadFlag = m_pEditor->Graphics()->TextureLoadFlags();
					if(pImg->m_Width % 16 != 0 || pImg->m_Height % 16 != 0)
						TextureLoadFlag = 0;
					pImg->m_External = 1;
					pImg->m_Texture = m_pEditor->Graphics()->LoadTextureRaw(pImg->ImageCopy(), TextureLoadFlag, aBuf);
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), "Error: Failed to load external image '%s'.", pImg->m_aName);
					ErrorHandler(aBuf);
				}
			}
			else
			{
				pImg->m_Width = pItem->m_Width;
				pImg->m_Height = pItem->m_Height;
				pImg->m_Format = CImageInfo::FORMAT_RGBA;

				const void *pData = pMap->GetData(pItem->m_ImageData);
				if(pItem->m_Width <= 0 || pItem->m_Height <= 0 || pData == nullptr || (size_t)pMap->GetDataSize(pItem->m_ImageData) < pImg->DataSize())
				{
					pImg->m_Width = 0;
					pImg->m_Height = 0;
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), "Error: Failed to load data of image %d '%s'.", i, pImg->m_aName);
					ErrorHandler(aBuf);
				}
				else
				{
					// copy image data
					pImg->m_Content.emplace(std::span<const std::uint8_t>(static_cast<const std::uint8_t *>(pData), pImg->DataSize()));
					int TextureLoadFlag = m_pEditor->Graphics()->TextureLoadFlags();
					if(pImg->m_Width % 16 != 0 || pImg->m_Height % 16 != 0)
						TextureLoadFlag = 0;
					pImg->m_Texture = m_pEditor->Graphics()->LoadTextureRaw(pImg->ImageCopy(), TextureLoadFlag, pImg->m_aName);
				}
			}

			// load auto mapper file
			pImg->m_Automapper.Load(pImg->m_aName);

			m_vpImages.push_back(pImg);

			// unload image
			pMap->UnloadData(pItem->m_ImageData);
			pMap->UnloadData(pItem->m_ImageName);
		}
	}

	// load sounds
	{
		int Start, Num;
		pMap->GetType(MAPITEMTYPE_SOUND, &Start, &Num);
		for(int i = 0; i < Num; i++)
		{
			CMapItemSound *pItem = (CMapItemSound *)pMap->GetItem(Start + i);

			// copy base info
			std::shared_ptr<CEditorSound> pSound = std::make_shared<CEditorSound>(this);

			const char *pName = pMap->GetDataString(pItem->m_SoundName);
			if(pName == nullptr || pName[0] == '\0')
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Error: Failed to read name of sound %d.", i);
				ErrorHandler(aBuf);
			}
			else
				str_copy(pSound->m_aName, pName);

			if(pItem->m_External)
			{
				char aBuf[IO_MAX_PATH_LENGTH];
				str_format(aBuf, sizeof(aBuf), "mapres/%s.opus", pSound->m_aName);

				// load external
				void *pData;
				unsigned DataSize;
				if(m_pEditor->Storage()->ReadFile(aBuf, IStorage::TYPE_ALL, &pData, &DataSize))
				{
					std::unique_ptr<void, decltype(&free)> pInput(pData, free);
					pSound->m_Content.emplace(std::span<const std::uint8_t>(static_cast<const std::uint8_t *>(pData), DataSize));
					pSound->m_SoundId = m_pEditor->Sound()->LoadOpusFromMem(pSound->Data(), pSound->DataSize(), true, pSound->m_aName);
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), "Error: Failed to load external sound '%s'.", pSound->m_aName);
					ErrorHandler(aBuf);
				}
			}
			else
			{
				const auto *pData = static_cast<const std::uint8_t *>(pMap->GetData(pItem->m_SoundData));
				const int DataSize = pMap->GetDataSize(pItem->m_SoundData);
				if(pData != nullptr && DataSize >= 0)
				{
					pSound->m_Content.emplace(std::span<const std::uint8_t>(pData, DataSize));
					pSound->m_SoundId = m_pEditor->Sound()->LoadOpusFromMem(pSound->Data(), pSound->DataSize(), true, pSound->m_aName);
				}
			}

			m_vpSounds.push_back(pSound);

			// unload sound
			pMap->UnloadData(pItem->m_SoundData);
			pMap->UnloadData(pItem->m_SoundName);
		}
	}

	// load envelopes
	{
		const CMapBasedEnvelopePointAccess EnvelopePoints(pMap.get());

		int EnvelopeStart, EnvelopeNum;
		pMap->GetType(MAPITEMTYPE_ENVELOPE, &EnvelopeStart, &EnvelopeNum);
		for(int EnvelopeIndex = 0; EnvelopeIndex < EnvelopeNum; EnvelopeIndex++)
		{
			CMapItemEnvelope *pItem = (CMapItemEnvelope *)pMap->GetItem(EnvelopeStart + EnvelopeIndex);
			int Channels = pItem->m_Channels;
			if(Channels <= 0 || Channels == 2 || Channels > CEnvPoint::MAX_CHANNELS)
			{
				// Fall back to showing all channels if the number of channels is unsupported
				Channels = CEnvPoint::MAX_CHANNELS;
			}
			if(Channels != pItem->m_Channels)
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Error: Envelope %d had an invalid number of channels, %d, which was changed to %d.", EnvelopeIndex, pItem->m_Channels, Channels);
				ErrorHandler(aBuf);
			}

			std::shared_ptr<CEnvelope> pEnvelope = std::make_shared<CEnvelope>(this, Channels);
			pEnvelope->m_vPoints.resize(pItem->m_NumPoints);
			for(int PointIndex = 0; PointIndex < pItem->m_NumPoints; PointIndex++)
			{
				const CEnvPoint *pPoint = EnvelopePoints.GetPoint(pItem->m_StartPoint + PointIndex);
				if(pPoint != nullptr)
					static_cast<CEnvPoint &>(pEnvelope->m_vPoints[PointIndex]) = *pPoint;
				const CEnvPointBezier *pPointBezier = EnvelopePoints.GetBezier(pItem->m_StartPoint + PointIndex);
				if(pPointBezier != nullptr)
					pEnvelope->m_vPoints[PointIndex].m_Bezier = *pPointBezier;
				pEnvelope->m_vPoints[PointIndex].m_Id = AllocateObjectId();
			}
			if(pItem->m_aName[0] != -1) // compatibility with old maps
				IntsToStr(pItem->m_aName, std::size(pItem->m_aName), pEnvelope->m_aName, std::size(pEnvelope->m_aName));
			m_vpEnvelopes.push_back(pEnvelope);
			if(pItem->m_Version >= 2)
				pEnvelope->m_Synchronized = pItem->m_Synchronized;
		}
	}

	// load groups
	{
		int LayersStart, LayersNum;
		pMap->GetType(MAPITEMTYPE_LAYER, &LayersStart, &LayersNum);

		int Start, Num;
		pMap->GetType(MAPITEMTYPE_GROUP, &Start, &Num);

		for(int g = 0; g < Num; g++)
		{
			CMapItemGroup *pGItem = (CMapItemGroup *)pMap->GetItem(Start + g);

			if(pGItem->m_Version < 1 || pGItem->m_Version > 3)
				continue;

			std::shared_ptr<CLayerGroup> pGroup = NewGroup();
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
				IntsToStr(pGItem->m_aName, std::size(pGItem->m_aName), pGroup->m_aName, std::size(pGroup->m_aName));

			for(int l = 0; l < pGItem->m_NumLayers; l++)
			{
				CMapItemLayer *pLayerItem = (CMapItemLayer *)pMap->GetItem(LayersStart + pGItem->m_StartLayer + l);
				if(!pLayerItem)
					continue;

				if(pLayerItem->m_Type == LAYERTYPE_TILES)
				{
					CMapItemLayerTilemap *pTilemapItem = (CMapItemLayerTilemap *)pLayerItem;

					std::shared_ptr<CLayerTiles> pTiles;
					if(pTilemapItem->m_Flags & TILESLAYERFLAG_GAME)
					{
						pTiles = std::make_shared<CLayerGame>(this, pTilemapItem->m_Width, pTilemapItem->m_Height);
						MakeGameLayer(pTiles);
						MakeGameGroup(pGroup);
					}
					else if(pTilemapItem->m_Flags & TILESLAYERFLAG_TELE)
					{
						pTiles = std::make_shared<CLayerTele>(this, pTilemapItem->m_Width, pTilemapItem->m_Height);
						MakeTeleLayer(pTiles);
					}
					else if(pTilemapItem->m_Flags & TILESLAYERFLAG_SPEEDUP)
					{
						pTiles = std::make_shared<CLayerSpeedup>(this, pTilemapItem->m_Width, pTilemapItem->m_Height);
						MakeSpeedupLayer(pTiles);
					}
					else if(pTilemapItem->m_Flags & TILESLAYERFLAG_FRONT)
					{
						pTiles = std::make_shared<CLayerFront>(this, pTilemapItem->m_Width, pTilemapItem->m_Height);
						MakeFrontLayer(pTiles);
					}
					else if(pTilemapItem->m_Flags & TILESLAYERFLAG_SWITCH)
					{
						pTiles = std::make_shared<CLayerSwitch>(this, pTilemapItem->m_Width, pTilemapItem->m_Height);
						MakeSwitchLayer(pTiles);
					}
					else if(pTilemapItem->m_Flags & TILESLAYERFLAG_TUNE)
					{
						pTiles = std::make_shared<CLayerTune>(this, pTilemapItem->m_Width, pTilemapItem->m_Height);
						MakeTuneLayer(pTiles);
					}
					else
					{
						pTiles = std::make_shared<CLayerTiles>(this, pTilemapItem->m_Width, pTilemapItem->m_Height);
						pTiles->m_Color = pTilemapItem->m_Color;
						pTiles->m_ColorEnv = EnvelopeReference(pTilemapItem->m_ColorEnv);
						pTiles->m_ColorEnvOffset = pTilemapItem->m_ColorEnvOffset;
					}

					pTiles->m_Flags = pLayerItem->m_Flags;

					pGroup->AddLayer(pTiles);
					pTiles->m_Image = ImageReference(pTilemapItem->m_Image);

					// load layer name
					IntsToStr(pTilemapItem->m_aName, std::size(pTilemapItem->m_aName), pTiles->m_aName, std::size(pTiles->m_aName));

					if(pTiles->m_HasTele)
					{
						const void *pData = pMap->GetData(pTilemapItem->m_Tele);
						if(pData != nullptr)
						{
							auto &LayerTeleTiles = std::static_pointer_cast<CLayerTele>(pTiles)->m_TeleTiles;
							LayerTeleTiles.Assign(std::span(static_cast<const CTeleTile *>(pData), pTiles->m_Tiles.Size()));
							pTiles->m_Tiles.AssignSparse(LayerTeleTiles, [](const CTeleTileValues &Cell) {
								CTile Tile{};
								if(IsValidTeleTile(Cell.m_Type))
									Tile.m_Index = Cell.m_Type;
								return CTileValues(Tile);
							});
						}
						pMap->UnloadData(pTilemapItem->m_Tele);
					}
					else if(pTiles->m_HasSpeedup)
					{
						const void *pData = pMap->GetData(pTilemapItem->m_Speedup);
						if(pData != nullptr)
						{
							auto &LayerSpeedupTiles = std::static_pointer_cast<CLayerSpeedup>(pTiles)->m_SpeedupTiles;
							LayerSpeedupTiles.Assign(std::span(static_cast<const CSpeedupTile *>(pData), pTiles->m_Tiles.Size()));
							pTiles->m_Tiles.AssignSparse(LayerSpeedupTiles, [](const CSpeedupTileValues &Cell) {
								CTile Tile{};
								if(IsValidSpeedupTile(Cell.m_Type) && Cell.m_Force > 0)
									Tile.m_Index = Cell.m_Type;
								return CTileValues(Tile);
							});
						}
						pMap->UnloadData(pTilemapItem->m_Speedup);
					}
					else if(pTiles->m_HasFront)
					{
						const void *pData = pMap->GetData(pTilemapItem->m_Front);
						if(pData != nullptr)
						{
							pTiles->m_Tiles.Assign(std::span(static_cast<const CTile *>(pData), pTiles->m_Tiles.Size()));
						}
						pMap->UnloadData(pTilemapItem->m_Front);
					}
					else if(pTiles->m_HasSwitch)
					{
						const void *pData = pMap->GetData(pTilemapItem->m_Switch);
						if(pData != nullptr)
						{
							auto &LayerSwitchTiles = std::static_pointer_cast<CLayerSwitch>(pTiles)->m_SwitchTiles;
							LayerSwitchTiles.Assign(std::span(static_cast<const CSwitchTile *>(pData), pTiles->m_Tiles.Size()));
							pTiles->m_Tiles.AssignSparse(LayerSwitchTiles, [](const CSwitchTileValues &Cell) {
								CTile Tile{};
								if((Cell.m_Type > ENTITY_CRAZY_SHOTGUN + ENTITY_OFFSET && Cell.m_Type < ENTITY_DRAGGER_WEAK + ENTITY_OFFSET) || Cell.m_Type == ENTITY_LASER_O_FAST + 1 + ENTITY_OFFSET)
									return CTileValues(Tile);
								if((Cell.m_Type >= ENTITY_ARMOR_1 + ENTITY_OFFSET && Cell.m_Type <= ENTITY_DOOR + ENTITY_OFFSET) || IsValidSwitchTile(Cell.m_Type))
								{
									Tile.m_Index = Cell.m_Type;
									Tile.m_Flags = Cell.m_Flags;
								}
								return CTileValues(Tile);
							});
						}
						pMap->UnloadData(pTilemapItem->m_Switch);
					}
					else if(pTiles->m_HasTune)
					{
						const void *pData = pMap->GetData(pTilemapItem->m_Tune);
						if(pData != nullptr)
						{
							auto &LayerTuneTiles = std::static_pointer_cast<CLayerTune>(pTiles)->m_TuneTiles;
							LayerTuneTiles.Assign(std::span(static_cast<const CTuneTile *>(pData), pTiles->m_Tiles.Size()));
							pTiles->m_Tiles.AssignSparse(LayerTuneTiles, [](const CTuneTileValues &Cell) {
								CTile Tile{};
								if(IsValidTuneTile(Cell.m_Type))
									Tile.m_Index = Cell.m_Type;
								return CTileValues(Tile);
							});
						}
						pMap->UnloadData(pTilemapItem->m_Tune);
					}
					else // regular tile layer or game layer
					{
						const void *pData = pMap->GetData(pTilemapItem->m_Data);
						if(pData != nullptr)
						{
							pTiles->m_Tiles.Assign(std::span(static_cast<const CTile *>(pData), pTiles->m_Tiles.Size()));
						}
						pMap->UnloadData(pTilemapItem->m_Data);
					}
				}
				else if(pLayerItem->m_Type == LAYERTYPE_QUADS)
				{
					const CMapItemLayerQuads *pQuadsItem = (CMapItemLayerQuads *)pLayerItem;

					std::shared_ptr<CLayerQuads> pQuads = std::make_shared<CLayerQuads>(this);
					pQuads->m_Flags = pLayerItem->m_Flags;
					pQuads->m_Image = ImageReference(pQuadsItem->m_Image);

					// load layer name
					if(pQuadsItem->m_Version >= 2)
						IntsToStr(pQuadsItem->m_aName, std::size(pQuadsItem->m_aName), pQuads->m_aName, std::size(pQuads->m_aName));

					if(pQuadsItem->m_NumQuads > 0)
					{
						const void *pData = pMap->GetDataSwapped(pQuadsItem->m_Data);
						if(pData != nullptr && (size_t)pMap->GetDataSize(pQuadsItem->m_Data) >= sizeof(CQuad) * (size_t)pQuadsItem->m_NumQuads)
						{
							pQuads->m_vQuads.resize(pQuadsItem->m_NumQuads);
							for(int Index = 0; Index < pQuadsItem->m_NumQuads; ++Index)
							{
								pQuads->m_vQuads[Index] = CQuadValues::Import(static_cast<const CQuad *>(pData)[Index], [this](int EnvelopeIndex) { return EnvelopeReference(EnvelopeIndex); });
								pQuads->m_vQuads[Index].m_Id = AllocateObjectId();
							}
						}
						else
						{
							char aBuf[128];
							str_format(aBuf, sizeof(aBuf), "Error: Failed to read quads of layer %d.", l);
							ErrorHandler(aBuf);
						}
						pMap->UnloadData(pQuadsItem->m_Data);
					}

					pGroup->AddLayer(pQuads);
				}
				else if(pLayerItem->m_Type == LAYERTYPE_SOUNDS)
				{
					const CMapItemLayerSounds *pSoundsItem = (CMapItemLayerSounds *)pLayerItem;
					if(pSoundsItem->m_Version < 1 || pSoundsItem->m_Version > 2)
						continue;

					std::shared_ptr<CLayerSounds> pSounds = std::make_shared<CLayerSounds>(this);
					pSounds->m_Flags = pLayerItem->m_Flags;
					pSounds->m_Sound = SoundReference(pSoundsItem->m_Sound);

					// load layer name
					IntsToStr(pSoundsItem->m_aName, std::size(pSoundsItem->m_aName), pSounds->m_aName, std::size(pSounds->m_aName));

					// load data
					if(pSoundsItem->m_NumSources > 0)
					{
						const void *pData = pMap->GetDataSwapped(pSoundsItem->m_Data);
						if(pData != nullptr && (size_t)pMap->GetDataSize(pSoundsItem->m_Data) >= sizeof(CSoundSource) * (size_t)pSoundsItem->m_NumSources)
						{
							pSounds->m_vSources.resize(pSoundsItem->m_NumSources);
							for(int Index = 0; Index < pSoundsItem->m_NumSources; ++Index)
							{
								pSounds->m_vSources[Index] = CSoundSourceValues::Import(static_cast<const CSoundSource *>(pData)[Index], [this](int EnvelopeIndex) { return EnvelopeReference(EnvelopeIndex); });
								pSounds->m_vSources[Index].m_Id = AllocateObjectId();
							}
						}
						else
						{
							char aBuf[128];
							str_format(aBuf, sizeof(aBuf), "Error: Failed to read sound sources of layer %d.", l);
							ErrorHandler(aBuf);
						}
						pMap->UnloadData(pSoundsItem->m_Data);
					}

					pGroup->AddLayer(pSounds);
				}
				else if(pLayerItem->m_Type == LAYERTYPE_SOUNDS_DEPRECATED)
				{
					// compatibility with old sound layers
					const CMapItemLayerSounds *pSoundsItem = (CMapItemLayerSounds *)pLayerItem;
					if(pSoundsItem->m_Version < 1 || pSoundsItem->m_Version > 2)
						continue;

					std::shared_ptr<CLayerSounds> pSounds = std::make_shared<CLayerSounds>(this);
					pSounds->m_Flags = pLayerItem->m_Flags;
					pSounds->m_Sound = SoundReference(pSoundsItem->m_Sound);

					// load layer name
					IntsToStr(pSoundsItem->m_aName, std::size(pSoundsItem->m_aName), pSounds->m_aName, std::size(pSounds->m_aName));

					pGroup->AddLayer(pSounds);

					// load data
					if(pSoundsItem->m_NumSources > 0)
					{
						const CSoundSourceDeprecated *pData = (const CSoundSourceDeprecated *)pMap->GetDataSwapped(pSoundsItem->m_Data);
						if(pData == nullptr || (size_t)pMap->GetDataSize(pSoundsItem->m_Data) < sizeof(CSoundSourceDeprecated) * (size_t)pSoundsItem->m_NumSources)
						{
							char aBuf[128];
							str_format(aBuf, sizeof(aBuf), "Error: Failed to read sound sources of layer %d.", l);
							ErrorHandler(aBuf);
						}
						else
						{
							pSounds->m_vSources.resize(pSoundsItem->m_NumSources);

							for(int i = 0; i < pSoundsItem->m_NumSources; i++)
							{
								const CSoundSourceDeprecated *pOldSource = &pData[i];

								auto &Source = pSounds->m_vSources[i];
								Source.m_Id = AllocateObjectId();
								Source.m_Position = pOldSource->m_Position;
								Source.m_Loop = pOldSource->m_Loop;
								Source.m_Pan = true;
								Source.m_TimeDelay = pOldSource->m_TimeDelay;
								Source.m_Falloff = 0;

								Source.m_PosEnv = EnvelopeReference(pOldSource->m_PosEnv);
								Source.m_PosEnvOffset = pOldSource->m_PosEnvOffset;
								Source.m_SoundEnv = EnvelopeReference(pOldSource->m_SoundEnv);
								Source.m_SoundEnvOffset = pOldSource->m_SoundEnvOffset;

								Source.m_Shape.m_Type = CSoundShape::SHAPE_CIRCLE;
								Source.m_Shape.m_Circle.m_Radius = pOldSource->m_FalloffDistance;
							}
						}

						pMap->UnloadData(pSoundsItem->m_Data);
					}
				}
			}
		}
	}

	// load automapper configurations
	{
		int AutomapperConfigStart, AutomapperConfigNum;
		pMap->GetType(MAPITEMTYPE_AUTOMAPPER_CONFIG, &AutomapperConfigStart, &AutomapperConfigNum);
		for(int i = 0; i < AutomapperConfigNum; i++)
		{
			CMapItemAutomapperConfig *pItem = (CMapItemAutomapperConfig *)pMap->GetItem(AutomapperConfigStart + i);
			if(pItem->m_Version == 1)
			{
				if(pItem->m_GroupId >= 0 && (size_t)pItem->m_GroupId < m_vpGroups.size() &&
					pItem->m_LayerId >= 0 && (size_t)pItem->m_LayerId < m_vpGroups[pItem->m_GroupId]->m_vpLayers.size())
				{
					std::shared_ptr<CLayer> pLayer = m_vpGroups[pItem->m_GroupId]->m_vpLayers[pItem->m_LayerId];
					if(pLayer->m_Type == LAYERTYPE_TILES)
					{
						std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(m_vpGroups[pItem->m_GroupId]->m_vpLayers[pItem->m_LayerId]);
						// only load auto mappers for tile layers (not physics layers)
						if(!(pTiles->m_HasGame || pTiles->m_HasTele || pTiles->m_HasSpeedup ||
							   pTiles->m_HasFront || pTiles->m_HasSwitch || pTiles->m_HasTune))
						{
							pTiles->m_AutomapperConfig = pItem->m_AutomapperConfig;
							pTiles->m_Seed = pItem->m_AutomapperSeed;
							pTiles->m_AutoAutomapper = !!(pItem->m_Flags & CMapItemAutomapperConfig::FLAG_AUTOMATIC);
						}
					}
				}
			}
		}
	}

	str_copy(m_aFilename, pFilename);

	CheckIntegrity();
	PerformSanityChecks(ErrorHandler);

	SortImages();
	SelectGameLayer();

	ResetModifiedState();
	if(!InitializeLoadedSaveState())
	{
		ErrorHandler("Could not retain the loaded document's saved-content marker.");
		return false;
	}
	return true;
}

bool CEditorMap::Append(const char *pFilename, int StorageType, const FErrorHandler &ErrorHandler)
{
	auto pPrepared = std::make_unique<CEditorMap>(Editor());
	if(!pPrepared->Load(pFilename, StorageType, ErrorHandler))
		return false;
	auto &NewMap = *pPrepared;

	std::unordered_map<std::uint64_t, CDocumentReference> ImageReferences;
	std::unordered_map<std::uint64_t, CDocumentReference> SoundReferences;
	std::unordered_map<std::uint64_t, CDocumentReference> EnvelopeReferences;

	const auto &&Rename = [&](const std::shared_ptr<CEditorImage> &pImage) {
		char aRenamed[IO_MAX_PATH_LENGTH];
		int DuplicateCount = 1;
		str_copy(aRenamed, pImage->m_aName);
		while(std::find_if(m_vpImages.begin(), m_vpImages.end(), [aRenamed](const std::shared_ptr<CEditorImage> &OtherImage) { return str_comp(OtherImage->m_aName, aRenamed) == 0; }) != m_vpImages.end())
			str_format(aRenamed, sizeof(aRenamed), "%s (%d)", pImage->m_aName, DuplicateCount++); // Rename to "image_name (%d)"
		str_copy(pImage->m_aName, aRenamed);
	};

	return m_DocumentHistory.Edit(this, "Append map", editor_history::ECategory::MAP, [&] {
		// Transfer non-duplicate images
		for(const auto &pNewImage : NewMap.m_vpImages)
		{
			auto NameIsTaken = [pNewImage](const std::shared_ptr<CEditorImage> &OtherImage) { return str_comp(pNewImage->m_aName, OtherImage->m_aName) == 0; };
			auto MatchInCurrentMap = std::find_if(m_vpImages.begin(), m_vpImages.end(), NameIsTaken);

			const bool IsDuplicate = MatchInCurrentMap != m_vpImages.end();
			const auto OriginalId = pNewImage->m_Id;

			if(IsDuplicate)
			{
				// Check for image data
				const bool ImageDataEquals = (*MatchInCurrentMap)->DataEquals(*pNewImage);

				if(ImageDataEquals)
				{
					const int IndexToReplaceWith = MatchInCurrentMap - m_vpImages.begin();

					dbg_msg("editor", "map already contains image %s with the same data, removing duplicate", pNewImage->m_aName);

					// In the new map, replace the index of the duplicate image to the index of the same in the current map.
					ImageReferences.emplace(OriginalId, ImageReference(IndexToReplaceWith));
				}
				else
				{
					// Rename image and add it
					Rename(pNewImage);

					dbg_msg("editor", "map already contains image %s but contents of appended image is different. Renaming to %s", (*MatchInCurrentMap)->m_aName, pNewImage->m_aName);

					pNewImage->OnAttach(this);
					ImageReferences.emplace(OriginalId, CDocumentReference{pNewImage->m_Id});
					m_vpImages.push_back(pNewImage);
				}
			}
			else
			{
				pNewImage->OnAttach(this);
				ImageReferences.emplace(OriginalId, CDocumentReference{pNewImage->m_Id});
				m_vpImages.push_back(pNewImage);
			}
		}
		NewMap.m_vpImages.clear();

		// transfer sounds
		for(const auto &pSound : NewMap.m_vpSounds)
		{
			const auto OriginalId = pSound->m_Id;
			pSound->OnAttach(this);
			SoundReferences.emplace(OriginalId, CDocumentReference{pSound->m_Id});
			m_vpSounds.push_back(pSound);
		}
		NewMap.m_vpSounds.clear();

		// transfer envelopes
		for(const auto &pEnvelope : NewMap.m_vpEnvelopes)
		{
			const auto OriginalId = pEnvelope->m_Id;
			pEnvelope->OnAttach(this);
			EnvelopeReferences.emplace(OriginalId, CDocumentReference{pEnvelope->m_Id});
			m_vpEnvelopes.push_back(pEnvelope);
		}
		NewMap.m_vpEnvelopes.clear();

		NewMap.VisitImageReferences([&](CDocumentReference &Reference) { Reference = ImageReferences[Reference.m_Id]; });
		NewMap.VisitSoundReferences([&](CDocumentReference &Reference) { Reference = SoundReferences[Reference.m_Id]; });
		NewMap.VisitAllEnvelopeReferences([&](CDocumentReference &Reference) { Reference = EnvelopeReferences[Reference.m_Id]; });

		// transfer groups
		for(const auto &pGroup : NewMap.m_vpGroups)
		{
			if(pGroup != NewMap.m_pGameGroup)
			{
				pGroup->OnAttach(this);
				m_vpGroups.push_back(pGroup);
			}
		}
		NewMap.m_vpGroups.clear();

		// transfer server settings
		for(const auto &pSetting : NewMap.m_vSettings)
		{
			// Check if setting already exists
			bool AlreadyExists = false;
			for(const auto &pExistingSetting : m_vSettings)
			{
				if(!str_comp(pExistingSetting.m_Command.Buffer(), pSetting.m_Command.Buffer()))
					AlreadyExists = true;
			}
			if(!AlreadyExists)
				m_vSettings.push_back(pSetting);
		}
		NewMap.m_vSettings.clear();

		SortImages();
		CheckIntegrity();
		OnModify();
	});
}

void CEditorMap::PerformSanityChecks(const FErrorHandler &ErrorHandler)
{
	// Check if there are any images with a width or height that is not divisible by 16 which are
	// used in tile layers. Reset the image for these layers, to prevent crashes with some drivers.
	size_t ImageIndex = 0;
	for(const std::shared_ptr<CEditorImage> &pImage : m_vpImages)
	{
		if(pImage->m_Width % 16 != 0 || pImage->m_Height % 16 != 0)
		{
			size_t GroupIndex = 0;
			for(const std::shared_ptr<CLayerGroup> &pGroup : m_vpGroups)
			{
				size_t LayerIndex = 0;
				for(const std::shared_ptr<CLayer> &pLayer : pGroup->m_vpLayers)
				{
					if(pLayer->m_Type == LAYERTYPE_TILES)
					{
						std::shared_ptr<CLayerTiles> pLayerTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
						if(this->ImageIndex(pLayerTiles->m_Image) >= 0 && (size_t)this->ImageIndex(pLayerTiles->m_Image) == ImageIndex)
						{
							pLayerTiles->m_Image = {};
							char aBuf[IO_MAX_PATH_LENGTH + 128];
							str_format(aBuf, sizeof(aBuf), "Error: The image '%s' (size %" PRIzu "x%" PRIzu ") has a width or height that is not divisible by 16 and therefore cannot be used for tile layers. The image of layer #%" PRIzu " '%s' in group #%" PRIzu " '%s' has been unset.", pImage->m_aName, pImage->m_Width, pImage->m_Height, LayerIndex, pLayer->m_aName, GroupIndex, pGroup->m_aName);
							ErrorHandler(aBuf);
						}
					}
					++LayerIndex;
				}
				++GroupIndex;
			}
		}
		++ImageIndex;
	}
}

bool CEditorMap::PerformAutosave(const std::function<void(const char *pErrorMessage)> &ErrorHandler)
{
	if(m_DocumentHistory.Active() || m_DocumentHistory.Pending() || (Editor()->Map() == this && Editor()->DocumentNumberInputActive()))
		return true; // Deferred without advancing the autosave timer.
	char aDate[20];
	char aAutosavePath[IO_MAX_PATH_LENGTH];
	str_timestamp(aDate, sizeof(aDate));
	str_format(aAutosavePath, sizeof(aAutosavePath), "maps/auto/%s_%s.map", m_aAutosaveName, aDate);

	m_LastSaveTime = Editor()->Client()->GlobalTime();
	if(SaveWithKind(aAutosavePath, editor_history::ESaveKind::AUTOMATIC, ErrorHandler))
	{
		return true;
	}
	else
	{
		char aErrorMessage[IO_MAX_PATH_LENGTH + 128];
		str_format(aErrorMessage, sizeof(aErrorMessage), "Failed to automatically save map to file '%s'.", aAutosavePath);
		ErrorHandler(aErrorMessage);
		return false;
	}
}
