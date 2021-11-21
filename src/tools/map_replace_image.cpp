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
	Usage: map_replace_image <source map filepath> <dest map filepath> <current image name> <new image filepath>
	Notes: map filepath must be relative to user default teeworlds folder
		new image filepath must be absolute or relative to the current position
*/

CDataFileReader g_DataReader;
CDataFileWriter g_DataWriter;

// global new image data (set by ReplaceImageItem)
int g_NewNameID = -1;
char g_aNewName[128];
int g_NewDataID = -1;
int g_NewDataSize = 0;
void *g_pNewData = 0;

int LoadPNG(CImageInfo *pImg, const char *pFilename)
{
	png_t Png;

	IOHANDLE File = io_open(pFilename, IOFLAG_READ);
	if(!File)
	{
		dbg_msg("dilate", "failed to open file. filename='%s'", pFilename);
		return 0;
	}
	int Error = png_open_read(&Png, 0, File);
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("map_replace_image", "failed to open image file. filename='%s', pnglite: %s", pFilename, png_error_string(Error));
		io_close(File);
		return 0;
	}

	if(Png.depth != 8 || (Png.color_type != PNG_TRUECOLOR && Png.color_type != PNG_TRUECOLOR_ALPHA) || Png.width > (2 << 12) || Png.height > (2 << 12))
	{
		dbg_msg("map_replace_image", "invalid image format. filename='%s'", pFilename);
		io_close(File);
		return 0;
	}

	unsigned char *pBuffer = (unsigned char *)malloc((size_t)Png.width * Png.height * Png.bpp);
	Error = png_get_data(&Png, pBuffer);
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("map_convert_07", "failed to read image. filename='%s', pnglite: %s", pFilename, png_error_string(Error));
		free(pBuffer);
		io_close(File);
		return 0;
	}
	io_close(File);

	pImg->m_Width = Png.width;
	pImg->m_Height = Png.height;
	if(Png.color_type == PNG_TRUECOLOR)
		pImg->m_Format = CImageInfo::FORMAT_RGB;
	else if(Png.color_type == PNG_TRUECOLOR_ALPHA)
		pImg->m_Format = CImageInfo::FORMAT_RGBA;
	else
	{
		free(pBuffer);
		return 0;
	}
	pImg->m_pData = pBuffer;
	return 1;
}

void *ReplaceImageItem(void *pItem, int Type, const char *pImgName, const char *pImgFile, CMapItemImage *pNewImgItem)
{
	if(Type != MAPITEMTYPE_IMAGE)
		return pItem;

	CMapItemImage *pImgItem = (CMapItemImage *)pItem;
	char *pName = (char *)g_DataReader.GetData(pImgItem->m_ImageName);

	if(str_comp(pImgName, pName) != 0)
		return pItem;

	dbg_msg("map_replace_image", "found image '%s'", pImgName);

	CImageInfo ImgInfo;
	if(!LoadPNG(&ImgInfo, pImgFile))
		return 0;

	*pNewImgItem = *pImgItem;

	pNewImgItem->m_Width = ImgInfo.m_Width;
	pNewImgItem->m_Height = ImgInfo.m_Height;
	int PixelSize = ImgInfo.m_Format == CImageInfo::FORMAT_RGB ? 3 : 4;

	g_NewNameID = pImgItem->m_ImageName;
	IStorage::StripPathAndExtension(pImgFile, g_aNewName, sizeof(g_aNewName));
	g_NewDataID = pImgItem->m_ImageData;
	g_pNewData = ImgInfo.m_pData;
	g_NewDataSize = ImgInfo.m_Width * ImgInfo.m_Height * PixelSize;

	return (void *)pNewImgItem;
}

int main(int argc, char **argv)
{
	dbg_logger_stdout();
	cmdline_init(argc, argv);

	IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_BASIC);

	if(cmdline_arg_num() != 5)
	{
		dbg_msg("map_replace_image", "Invalid arguments");
		dbg_msg("map_replace_image", "Usage: map_replace_image <source map filepath> <dest map filepath> <current image name> <new image filepath>");
		dbg_msg("map_replace_image", "Notes: map filepath must be relative to user default teeworlds folder");
		dbg_msg("map_replace_image", "       new image filepath must be absolute or relative to the current position");
		return -1;
	}

	if(!pStorage)
	{
		dbg_msg("map_replace_image", "error loading storage");
		return -1;
	}

	char aSourceFileName[IO_MAX_PATH_LENGTH];
	cmdline_arg_get(1, aSourceFileName, sizeof(aSourceFileName));
	char aDestFileName[IO_MAX_PATH_LENGTH];
	cmdline_arg_get(2, aDestFileName, sizeof(aDestFileName));
	char aImageName[IO_MAX_PATH_LENGTH];
	cmdline_arg_get(3, aImageName, sizeof(aImageName));
	char aImageFile[IO_MAX_PATH_LENGTH];
	cmdline_arg_get(4, aImageFile, sizeof(aImageFile));

	if(!g_DataReader.Open(pStorage, aSourceFileName, IStorage::TYPE_ALL))
	{
		dbg_msg("map_replace_image", "failed to open source map. filename='%s'", aSourceFileName);
		return -1;
	}

	if(!g_DataWriter.Open(pStorage, aDestFileName))
	{
		dbg_msg("map_replace_image", "failed to open destination map. filename='%s'", aDestFileName);
		return -1;
	}

	png_init(0, 0);

	// add all items
	for(int Index = 0; Index < g_DataReader.NumItems(); Index++)
	{
		CMapItemImage NewImageItem;
		int Type, ID;
		void *pItem = g_DataReader.GetItem(Index, &Type, &ID);
		int Size = g_DataReader.GetItemSize(Index);

		// filter ITEMTYPE_EX items, they will be automatically added again
		if(Type == ITEMTYPE_EX)
		{
			continue;
		}

		pItem = ReplaceImageItem(pItem, Type, aImageName, aImageFile, &NewImageItem);
		if(!pItem)
			return -1;
		g_DataWriter.AddItem(Type, ID, Size, pItem);
	}

	if(g_NewDataID == -1)
	{
		dbg_msg("map_replace_image", "image '%s' not found on source map '%s'.", aImageName, aSourceFileName);
		return -1;
	}

	// add all data
	for(int Index = 0; Index < g_DataReader.NumItems(); Index++)
	{
		void *pData;
		int Size;
		if(Index == g_NewDataID)
		{
			pData = g_pNewData;
			Size = g_NewDataSize;
		}
		else if(Index == g_NewNameID)
		{
			pData = (void *)g_aNewName;
			Size = str_length(g_aNewName) + 1;
		}
		else
		{
			pData = g_DataReader.GetData(Index);
			Size = g_DataReader.GetDataSize(Index);
		}

		g_DataWriter.AddData(Size, pData);
	}

	g_DataReader.Close();
	g_DataWriter.Finish();

	dbg_msg("map_replace_image", "image '%s' replaced", aImageName);
	cmdline_free();
	return 0;
}
