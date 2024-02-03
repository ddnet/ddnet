/* (c) DDNet developers. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.  */

#include <base/logger.h>
#include <base/system.h>
#include <engine/gfx/image_loader.h>
#include <engine/graphics.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/gamecore.h>
#include <game/mapitems.h>
/*
	Usage: map_convert_07 <source map filepath> <dest map filepath>
*/

CDataFileReader g_DataReader;
CDataFileWriter g_DataWriter;

// global new image data (set by ReplaceImageItem)
int g_aNewDataSize[MAX_MAPIMAGES];
void *g_apNewData[MAX_MAPIMAGES];

int g_Index = 0;
int g_NextDataItemID = -1;

int g_aImageIDs[MAX_MAPIMAGES];

int LoadPNG(CImageInfo *pImg, const char *pFilename)
{
	IOHANDLE File = io_open(pFilename, IOFLAG_READ);
	if(File)
	{
		io_seek(File, 0, IOSEEK_END);
		long int FileSize = io_tell(File);
		if(FileSize <= 0)
		{
			io_close(File);
			dbg_msg("map_convert_07", "failed to get file size (%ld). filename='%s'", FileSize, pFilename);
			return false;
		}
		io_seek(File, 0, IOSEEK_START);
		TImageByteBuffer ByteBuffer;
		SImageByteBuffer ImageByteBuffer(&ByteBuffer);

		ByteBuffer.resize(FileSize);
		io_read(File, &ByteBuffer.front(), FileSize);

		io_close(File);

		uint8_t *pImgBuffer = NULL;
		EImageFormat ImageFormat;
		int PngliteIncompatible;
		if(LoadPNG(ImageByteBuffer, pFilename, PngliteIncompatible, pImg->m_Width, pImg->m_Height, pImgBuffer, ImageFormat))
		{
			pImg->m_pData = pImgBuffer;

			if(ImageFormat == IMAGE_FORMAT_RGBA && pImg->m_Width <= (2 << 13) && pImg->m_Height <= (2 << 13))
			{
				pImg->m_Format = CImageInfo::FORMAT_RGBA;
			}
			else
			{
				dbg_msg("map_convert_07", "invalid image format. filename='%s'", pFilename);
				return 0;
			}
		}
		else
			return 0;
	}
	else
		return 0;
	return 1;
}

bool CheckImageDimensions(void *pLayerItem, int LayerType, const char *pFilename)
{
	if(LayerType != MAPITEMTYPE_LAYER)
		return true;

	CMapItemLayer *pImgLayer = (CMapItemLayer *)pLayerItem;
	if(pImgLayer->m_Type != LAYERTYPE_TILES)
		return true;

	CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pImgLayer;
	if(pTMap->m_Image == -1)
		return true;

	int Type;
	void *pItem = g_DataReader.GetItem(g_aImageIDs[pTMap->m_Image], &Type);
	if(Type != MAPITEMTYPE_IMAGE)
		return true;

	CMapItemImage *pImgItem = (CMapItemImage *)pItem;

	if(pImgItem->m_Width % 16 == 0 && pImgItem->m_Height % 16 == 0 && pImgItem->m_Width > 0 && pImgItem->m_Height > 0)
		return true;

	char aTileLayerName[12];
	IntsToStr(pTMap->m_aName, sizeof(pTMap->m_aName) / sizeof(int), aTileLayerName);

	const char *pName = g_DataReader.GetDataString(pImgItem->m_ImageName);
	dbg_msg("map_convert_07", "%s: Tile layer \"%s\" uses image \"%s\" with width %d, height %d, which is not divisible by 16. This is not supported in Teeworlds 0.7. Please scale the image and replace it manually.", pFilename, aTileLayerName, pName == nullptr ? "(error)" : pName, pImgItem->m_Width, pImgItem->m_Height);
	return false;
}

void *ReplaceImageItem(int Index, CMapItemImage *pImgItem, CMapItemImage *pNewImgItem)
{
	if(!pImgItem->m_External)
		return pImgItem;

	const char *pName = g_DataReader.GetDataString(pImgItem->m_ImageName);
	if(pName == nullptr || pName[0] == '\0')
	{
		dbg_msg("map_convert_07", "failed to load name of image %d", Index);
		return pImgItem;
	}

	dbg_msg("map_convert_07", "embedding image '%s'", pName);

	CImageInfo ImgInfo;
	char aStr[IO_MAX_PATH_LENGTH];
	str_format(aStr, sizeof(aStr), "data/mapres/%s.png", pName);
	if(!LoadPNG(&ImgInfo, aStr))
		return pImgItem; // keep as external if we don't have a mapres to replace

	if(ImgInfo.m_Format != CImageInfo::FORMAT_RGBA)
	{
		dbg_msg("map_convert_07", "image '%s' is not in RGBA format", aStr);
		return pImgItem;
	}

	*pNewImgItem = *pImgItem;

	pNewImgItem->m_Width = ImgInfo.m_Width;
	pNewImgItem->m_Height = ImgInfo.m_Height;
	pNewImgItem->m_External = false;
	pNewImgItem->m_ImageData = g_NextDataItemID++;

	g_apNewData[g_Index] = ImgInfo.m_pData;
	g_aNewDataSize[g_Index] = (size_t)ImgInfo.m_Width * ImgInfo.m_Height * ImgInfo.PixelSize();
	g_Index++;

	return (void *)pNewImgItem;
}

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	if(argc < 2 || argc > 3)
	{
		dbg_msg("map_convert_07", "Invalid arguments");
		dbg_msg("map_convert_07", "Usage: map_convert_07 <source map filepath> [<dest map filepath>]");
		return -1;
	}

	IStorage *pStorage = CreateStorage(IStorage::STORAGETYPE_BASIC, argc, argv);
	if(!pStorage)
	{
		dbg_msg("map_convert_07", "error loading storage");
		return -1;
	}

	const char *pSourceFileName = argv[1];
	char aDestFileName[IO_MAX_PATH_LENGTH];

	if(argc == 3)
	{
		str_copy(aDestFileName, argv[2], sizeof(aDestFileName));
	}
	else
	{
		char aBuf[IO_MAX_PATH_LENGTH];
		IStorage::StripPathAndExtension(pSourceFileName, aBuf, sizeof(aBuf));
		str_format(aDestFileName, sizeof(aDestFileName), "data/maps7/%s.map", aBuf);
		if(fs_makedir("data") != 0)
		{
			dbg_msg("map_convert_07", "failed to create data directory");
			return -1;
		}

		if(fs_makedir("data/maps7") != 0)
		{
			dbg_msg("map_convert_07", "failed to create data/maps7 directory");
			return -1;
		}
	}

	if(!g_DataReader.Open(pStorage, pSourceFileName, IStorage::TYPE_ABSOLUTE))
	{
		dbg_msg("map_convert_07", "failed to open source map. filename='%s'", pSourceFileName);
		return -1;
	}

	if(!g_DataWriter.Open(pStorage, aDestFileName, IStorage::TYPE_ABSOLUTE))
	{
		dbg_msg("map_convert_07", "failed to open destination map. filename='%s'", aDestFileName);
		return -1;
	}

	g_NextDataItemID = g_DataReader.NumData();

	size_t i = 0;
	for(int Index = 0; Index < g_DataReader.NumItems(); Index++)
	{
		int Type;
		g_DataReader.GetItem(Index, &Type);
		if(Type == MAPITEMTYPE_IMAGE)
		{
			if(i >= MAX_MAPIMAGES)
			{
				dbg_msg("map_convert_07", "map uses more images than the client maximum of %" PRIzu ". filename='%s'", MAX_MAPIMAGES, pSourceFileName);
				break;
			}
			g_aImageIDs[i] = Index;
			i++;
		}
	}

	bool Success = true;

	// add all items
	for(int Index = 0; Index < g_DataReader.NumItems(); Index++)
	{
		int Type, ID;
		CUuid Uuid;
		void *pItem = g_DataReader.GetItem(Index, &Type, &ID, &Uuid);

		// Filter ITEMTYPE_EX items, they will be automatically added again.
		if(Type == ITEMTYPE_EX)
		{
			continue;
		}

		int Size = g_DataReader.GetItemSize(Index);
		Success &= CheckImageDimensions(pItem, Type, pSourceFileName);

		CMapItemImage NewImageItem;
		if(Type == MAPITEMTYPE_IMAGE)
		{
			pItem = ReplaceImageItem(Index, (CMapItemImage *)pItem, &NewImageItem);
			if(!pItem)
				return -1;
			Size = sizeof(CMapItemImage);
			NewImageItem.m_Version = CMapItemImage::CURRENT_VERSION;
		}
		g_DataWriter.AddItem(Type, ID, Size, pItem, &Uuid);
	}

	// add all data
	for(int Index = 0; Index < g_DataReader.NumData(); Index++)
	{
		void *pData = g_DataReader.GetData(Index);
		int Size = g_DataReader.GetDataSize(Index);
		g_DataWriter.AddData(Size, pData);
	}

	for(int Index = 0; Index < g_Index; Index++)
	{
		g_DataWriter.AddData(g_aNewDataSize[Index], g_apNewData[Index]);
	}

	g_DataReader.Close();
	g_DataWriter.Finish();
	return Success ? 0 : -1;
}
