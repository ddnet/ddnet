/* (c) DDNet developers. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.  */

#include <base/system.h>
#include <base/math.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <engine/graphics.h>
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

int LoadPNG(CImageInfo *pImg, const char *pFilename)
{
	unsigned char *pBuffer;
	png_t Png;

	int Error = png_open_file(&Png, pFilename);
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("map_convert_07", "failed to open image file. filename='%s'", pFilename);
		if(Error != PNG_FILE_ERROR)
			png_close_file(&Png);
		return 0;
	}

	if(Png.depth != 8 || (Png.color_type != PNG_TRUECOLOR && Png.color_type != PNG_TRUECOLOR_ALPHA) || Png.width > (2<<12) || Png.height > (2<<12))
	{
		dbg_msg("map_convert_07", "invalid image format. filename='%s'", pFilename);
		png_close_file(&Png);
		return 0;
	}

	pBuffer = (unsigned char *)malloc(Png.width * Png.height * Png.bpp);
	png_get_data(&Png, pBuffer);
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

	pNewImgItem->m_Width  = ImgInfo.m_Width;
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

	if(argc != 3)
	{
		dbg_msg("map_convert_07", "Invalid arguments");
		dbg_msg("map_convert_07", "Usage: map_convert_07 <source map filepath> <dest map filepath>");
		return -1;
	}

	if (!pStorage)
	{
		dbg_msg("map_convert_07", "error loading storage");
		return -1;
	}

	const char *pSourceFileName = argv[1];
	const char *pDestFileName = argv[2];

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

	png_init(0,0);

	g_NextDataItemID = g_DataReader.NumItems();

	// add all items
	for(int Index = 0; Index < g_DataReader.NumItems(); Index++)
	{
		CMapItemImage NewImageItem;
		pItem = g_DataReader.GetItem(Index, &Type, &ID);
		Size = g_DataReader.GetItemSize(Index);
		pItem = ReplaceImageItem(pItem, Type, &NewImageItem);
		if(!pItem)
			return -1;
		g_DataWriter.AddItem(Type, ID, Size, pItem);
	}

	// add all data
	for(int Index = 0; Index < g_DataReader.NumItems(); Index++)
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
	return 0;
}
