/* (c) DDNet developers. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.  */

#include <base/math.h>
#include <base/system.h>
#include <engine/graphics.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/mapitems.h>

#include <pnglite.h>
/*
	Usage: map_convert_07 <source map filepath> <dest map filepath>
*/

CDataFileReader g_DataReader;
CDataFileWriter g_DataWriter;

// global new image data (set by ReplaceImageItem)
int g_NewDataSize[64];
void *g_pNewData[64];

int g_Index = 0;
int g_NextDataItemID = -1;

int g_aImageIDs[64];

int LoadPNG(CImageInfo *pImg, const char *pFilename)
{
	unsigned char *pBuffer;
	png_t Png;

	int Error = png_open_file(&Png, pFilename);
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("map_convert_07", "failed to open image file. filename='%s', pnglite: %s", pFilename, png_error_string(Error));
		if(Error != PNG_FILE_ERROR)
			png_close_file(&Png);
		return 0;
	}

	if(Png.depth != 8 || (Png.color_type != PNG_TRUECOLOR && Png.color_type != PNG_TRUECOLOR_ALPHA) || Png.width > (2 << 12) || Png.height > (2 << 12))
	{
		dbg_msg("map_convert_07", "invalid image format. filename='%s'", pFilename);
		png_close_file(&Png);
		return 0;
	}

	pBuffer = (unsigned char *)malloc(Png.width * Png.height * Png.bpp);
	Error = png_get_data(&Png, pBuffer);
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("map_convert_07", "failed to read image. filename='%s', pnglite: %s", pFilename, png_error_string(Error));
		free(pBuffer);
		png_close_file(&Png);
		return 0;
	}
	png_close_file(&Png);

	pImg->m_Width = Png.width;
	pImg->m_Height = Png.height;
	if(Png.color_type == PNG_TRUECOLOR)
		pImg->m_Format = CImageInfo::FORMAT_RGB;
	else if(Png.color_type == PNG_TRUECOLOR_ALPHA)
		pImg->m_Format = CImageInfo::FORMAT_RGBA;
	pImg->m_pData = pBuffer;
	return 1;
}

inline void IntsToStr(const int *pInts, int Num, char *pStr)
{
	while(Num)
	{
		pStr[0] = (((*pInts) >> 24) & 0xff) - 128;
		pStr[1] = (((*pInts) >> 16) & 0xff) - 128;
		pStr[2] = (((*pInts) >> 8) & 0xff) - 128;
		pStr[3] = ((*pInts) & 0xff) - 128;
		pStr += 4;
		pInts++;
		Num--;
	}

	// null terminate
	pStr[-1] = 0;
}

bool CheckImageDimensions(void *pItem, int Type, const char *pFilename)
{
	if(Type != MAPITEMTYPE_LAYER)
		return true;

	CMapItemLayer *pImgLayer = (CMapItemLayer *)pItem;
	if(pImgLayer->m_Type != LAYERTYPE_TILES)
		return true;

	CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pImgLayer;
	if(pTMap->m_Image == -1)
		return true;

	int TypeImg = 0;
	int ID = 0;
	void *pItem2 = g_DataReader.GetItem(g_aImageIDs[pTMap->m_Image], &TypeImg, &ID);
	if(TypeImg != MAPITEMTYPE_IMAGE)
		return true;

	CMapItemImage *pImgItem = (CMapItemImage *)pItem2;

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
	ImgInfo.m_Format = CImageInfo::FORMAT_RGB;
	char aStr[64];
	str_format(aStr, sizeof(aStr), "data/mapres/%s.png", pName);
	if(!LoadPNG(&ImgInfo, aStr))
		return pItem; // keep as external if we don't have a mapres to replace

	*pNewImgItem = *pImgItem;

	pNewImgItem->m_Width = ImgInfo.m_Width;
	pNewImgItem->m_Height = ImgInfo.m_Height;
	pNewImgItem->m_External = false;
	int PixelSize = ImgInfo.m_Format == CImageInfo::FORMAT_RGB ? 3 : 4;
	pNewImgItem->m_ImageData = g_NextDataItemID++;

	g_pNewData[g_Index] = ImgInfo.m_pData;
	g_NewDataSize[g_Index] = ImgInfo.m_Width * ImgInfo.m_Height * PixelSize;
	g_Index++;

	return (void *)pNewImgItem;
}

int main(int argc, const char **argv)
{
	dbg_logger_stdout();

	IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_BASIC, argc, argv);

	if(argc < 2 || argc > 3)
	{
		dbg_msg("map_convert_07", "Invalid arguments");
		dbg_msg("map_convert_07", "Usage: map_convert_07 <source map filepath> [<dest map filepath>]");
		return -1;
	}

	if(!pStorage)
	{
		dbg_msg("map_convert_07", "error loading storage");
		return -1;
	}

	const char *pSourceFileName = argv[1];

	const char *pDestFileName;
	char aDestFileName[MAX_PATH_LENGTH];

	if(argc == 3)
	{
		pDestFileName = argv[2];
	}
	else
	{
		char aBuf[MAX_PATH_LENGTH];
		IStorage::StripPathAndExtension(pSourceFileName, aBuf, sizeof(aBuf));
		str_format(aDestFileName, sizeof(aDestFileName), "data/maps7/%s.map", aBuf);
		pDestFileName = aDestFileName;
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

	int ID = 0;
	int Type = 0;
	int Size = 0;
	void *pItem = 0;
	void *pData = 0;

	if(!g_DataReader.Open(pStorage, pSourceFileName, IStorage::TYPE_ABSOLUTE))
	{
		dbg_msg("map_convert_07", "failed to open source map. filename='%s'", pSourceFileName);
		return -1;
	}

	if(!g_DataWriter.Open(pStorage, pDestFileName, IStorage::TYPE_ABSOLUTE))
	{
		dbg_msg("map_convert_07", "failed to open destination map. filename='%s'", pDestFileName);
		return -1;
	}

	png_init(0, 0);

	g_NextDataItemID = g_DataReader.NumData();

	int i = 0;
	for(int Index = 0; Index < g_DataReader.NumItems(); Index++)
	{
		pItem = g_DataReader.GetItem(Index, &Type, &ID);
		if(Type == MAPITEMTYPE_IMAGE)
			g_aImageIDs[i++] = Index;
	}

	bool Success = true;

	if(i > 64)
		dbg_msg("map_convert_07", "%s: Uses more textures than the client maximum of 64.", pSourceFileName);

	// add all items
	for(int Index = 0; Index < g_DataReader.NumItems(); Index++)
	{
		CMapItemImage NewImageItem;
		pItem = g_DataReader.GetItem(Index, &Type, &ID);
		Size = g_DataReader.GetItemSize(Index);

		Success &= CheckImageDimensions(pItem, Type, pSourceFileName);

		pItem = ReplaceImageItem(pItem, Type, &NewImageItem);
		if(!pItem)
			return -1;
		g_DataWriter.AddItem(Type, ID, Size, pItem);
	}

	// add all data
	for(int Index = 0; Index < g_DataReader.NumData(); Index++)
	{
		pData = g_DataReader.GetData(Index);
		Size = g_DataReader.GetDataSize(Index);
		g_DataWriter.AddData(Size, pData);
	}

	for(int Index = 0; Index < g_Index; Index++)
	{
		pData = g_pNewData[Index];
		Size = g_NewDataSize[Index];
		g_DataWriter.AddData(Size, pData);
	}

	g_DataReader.Close();
	g_DataWriter.Finish();
	return Success ? 0 : -1;
}
