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
int g_aNewDataSize[64];
void *g_apNewData[64];

int g_Index = 0;
int g_NextDataItemID = -1;

int g_aImageIDs[64];

int LoadPNG(CImageInfo *pImg, const char *pFilename)
{
	IOHANDLE File = io_open(pFilename, IOFLAG_READ);
	if(File)
	{
		io_seek(File, 0, IOSEEK_END);
		unsigned int FileSize = io_tell(File);
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
	void *pItem = g_DataReader.GetItem(g_aImageIDs[pTMap->m_Image], &Type, nullptr);
	if(Type != MAPITEMTYPE_IMAGE)
		return true;

	CMapItemImage *pImgItem = (CMapItemImage *)pItem;

	if(pImgItem->m_Width % 16 == 0 && pImgItem->m_Height % 16 == 0 && pImgItem->m_Width > 0 && pImgItem->m_Height > 0)
		return true;

	char aTileLayerName[12];
	IntsToStr(pTMap->m_aName, sizeof(pTMap->m_aName) / sizeof(int), aTileLayerName);
	char *pName = (char *)g_DataReader.GetData(pImgItem->m_ImageName);
	dbg_msg("map_convert_07", "%s: Tile layer \"%s\" uses image \"%s\" with width %d, height %d, which is not divisible by 16. This is not supported in Teeworlds 0.7. Please scale the image and replace it manually.", pFilename, aTileLayerName, pName, pImgItem->m_Width, pImgItem->m_Height);
	return false;
}

void *ReplaceImageItem(void *pItem, int Type, CMapItemImage *pNewImgItem)
{
	if(Type != MAPITEMTYPE_IMAGE)
		return pItem;

	CMapItemImage *pImgItem = (CMapItemImage *)pItem;

	if(!pImgItem->m_External)
		return pItem;

	char *pName = (char *)g_DataReader.GetData(pImgItem->m_ImageName);
	dbg_msg("map_convert_07", "embedding image '%s'", pName);

	CImageInfo ImgInfo;
	char aStr[64];
	str_format(aStr, sizeof(aStr), "data/mapres/%s.png", pName);
	if(!LoadPNG(&ImgInfo, aStr))
		return pItem; // keep as external if we don't have a mapres to replace

	*pNewImgItem = *pImgItem;

	pNewImgItem->m_Width = ImgInfo.m_Width;
	pNewImgItem->m_Height = ImgInfo.m_Height;
	pNewImgItem->m_External = false;
	pNewImgItem->m_ImageData = g_NextDataItemID++;

	g_apNewData[g_Index] = ImgInfo.m_pData;
	g_aNewDataSize[g_Index] = ImgInfo.m_Width * ImgInfo.m_Height * 4;
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

	int i = 0;
	for(int Index = 0; Index < g_DataReader.NumItems(); Index++)
	{
		int Type;
		g_DataReader.GetItem(Index, &Type, nullptr);
		if(Type == MAPITEMTYPE_IMAGE)
			g_aImageIDs[i++] = Index;
	}

	bool Success = true;

	if(i > 64)
		dbg_msg("map_convert_07", "%s: Uses more textures than the client maximum of 64.", pSourceFileName);

	// add all items
	for(int Index = 0; Index < g_DataReader.NumItems(); Index++)
	{
		int Type, ID;
		void *pItem = g_DataReader.GetItem(Index, &Type, &ID);
		int Size = g_DataReader.GetItemSize(Index);

		// filter ITEMTYPE_EX items, they will be automatically added again
		if(Type == ITEMTYPE_EX)
		{
			continue;
		}

		Success &= CheckImageDimensions(pItem, Type, pSourceFileName);

		CMapItemImage NewImageItem;
		pItem = ReplaceImageItem(pItem, Type, &NewImageItem);
		if(!pItem)
			return -1;
		g_DataWriter.AddItem(Type, ID, Size, pItem);
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
