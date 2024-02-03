/* (c) DDNet developers. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.  */

#include <base/logger.h>
#include <base/system.h>
#include <engine/gfx/image_loader.h>
#include <engine/graphics.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/mapitems.h>
/*
	Usage: map_replace_image <source map filepath> <dest map filepath> <current image name> <new image filepath>
	Notes: map filepath must be relative to user default teeworlds folder
		new image filepath must be absolute or relative to the current position
*/

CDataFileReader g_DataReader;

// global new image data (set by ReplaceImageItem)
int g_NewNameID = -1;
char g_aNewName[128];
int g_NewDataID = -1;
int g_NewDataSize = 0;
void *g_pNewData = nullptr;

bool LoadPNG(CImageInfo *pImg, const char *pFilename)
{
	IOHANDLE File = io_open(pFilename, IOFLAG_READ);
	if(File)
	{
		io_seek(File, 0, IOSEEK_END);
		long int FileSize = io_tell(File);
		if(FileSize <= 0)
		{
			io_close(File);
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
			if((ImageFormat == IMAGE_FORMAT_RGBA || ImageFormat == IMAGE_FORMAT_RGB) && pImg->m_Width <= (2 << 13) && pImg->m_Height <= (2 << 13))
			{
				pImg->m_pData = pImgBuffer;

				if(ImageFormat == IMAGE_FORMAT_RGB)
					pImg->m_Format = CImageInfo::FORMAT_RGB;
				else if(ImageFormat == IMAGE_FORMAT_RGBA)
					pImg->m_Format = CImageInfo::FORMAT_RGBA;
				else
				{
					free(pImgBuffer);
					return false;
				}
			}
		}
		else
			return false;
	}
	else
		return false;
	return true;
}

void *ReplaceImageItem(int Index, CMapItemImage *pImgItem, const char *pImgName, const char *pImgFile, CMapItemImage *pNewImgItem)
{
	const char *pName = g_DataReader.GetDataString(pImgItem->m_ImageName);
	if(pName == nullptr || pName[0] == '\0')
	{
		dbg_msg("map_replace_image", "failed to load name of image %d", Index);
		return pImgItem;
	}

	if(str_comp(pImgName, pName) != 0)
		return pImgItem;

	dbg_msg("map_replace_image", "found image '%s'", pImgName);

	CImageInfo ImgInfo;
	if(!LoadPNG(&ImgInfo, pImgFile))
		return 0;

	if(ImgInfo.m_Format != CImageInfo::FORMAT_RGBA)
	{
		dbg_msg("map_replace_image", "image '%s' is not in RGBA format", pImgName);
		return 0;
	}

	*pNewImgItem = *pImgItem;

	pNewImgItem->m_Width = ImgInfo.m_Width;
	pNewImgItem->m_Height = ImgInfo.m_Height;

	g_NewNameID = pImgItem->m_ImageName;
	IStorage::StripPathAndExtension(pImgFile, g_aNewName, sizeof(g_aNewName));
	g_NewDataID = pImgItem->m_ImageData;
	g_pNewData = ImgInfo.m_pData;
	g_NewDataSize = (size_t)ImgInfo.m_Width * ImgInfo.m_Height * ImgInfo.PixelSize();

	return (void *)pNewImgItem;
}

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	if(argc != 5)
	{
		dbg_msg("map_replace_image", "Invalid arguments");
		dbg_msg("map_replace_image", "Usage: map_replace_image <source map filepath> <dest map filepath> <current image name> <new image filepath>");
		dbg_msg("map_replace_image", "Notes: map filepath must be relative to user default teeworlds folder");
		dbg_msg("map_replace_image", "       new image filepath must be absolute or relative to the current position");
		return -1;
	}

	IStorage *pStorage = CreateStorage(IStorage::STORAGETYPE_BASIC, argc, argv);
	if(!pStorage)
	{
		dbg_msg("map_replace_image", "error loading storage");
		return -1;
	}

	const char *pSourceFileName = argv[1];
	const char *pDestFileName = argv[2];
	const char *pImageName = argv[3];
	const char *pImageFile = argv[4];

	if(!g_DataReader.Open(pStorage, pSourceFileName, IStorage::TYPE_ALL))
	{
		dbg_msg("map_replace_image", "failed to open source map. filename='%s'", pSourceFileName);
		return -1;
	}

	CDataFileWriter Writer;
	if(!Writer.Open(pStorage, pDestFileName))
	{
		dbg_msg("map_replace_image", "failed to open destination map. filename='%s'", pDestFileName);
		return -1;
	}

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

		CMapItemImage NewImageItem;
		if(Type == MAPITEMTYPE_IMAGE)
		{
			pItem = ReplaceImageItem(Index, (CMapItemImage *)pItem, pImageName, pImageFile, &NewImageItem);
			if(!pItem)
				return -1;
			Size = sizeof(CMapItemImage);
			NewImageItem.m_Version = CMapItemImage::CURRENT_VERSION;
		}

		Writer.AddItem(Type, ID, Size, pItem, &Uuid);
	}

	if(g_NewDataID == -1)
	{
		dbg_msg("map_replace_image", "image '%s' not found on source map '%s'.", pImageName, pSourceFileName);
		return -1;
	}

	// add all data
	for(int Index = 0; Index < g_DataReader.NumData(); Index++)
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

		Writer.AddData(Size, pData);
	}

	g_DataReader.Close();
	Writer.Finish();

	dbg_msg("map_replace_image", "image '%s' replaced", pImageName);
	return 0;
}
