/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */ /* If you are missing that file, acquire a 
complete release at teeworlds.com.  */

#include <base/system.h>
#include <base/math.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <engine/external/pnglite/pnglite.h>
#include <engine/graphics.h>
#include <game/mapitems.h>

/*
    Usage: map_replace_image <source map filepath> <dest map filepath> <current image name> <new image filepath>
    Notes: map filepath must be relative to user default teeworlds folder
           new image filepath must be absolute or relative to the current position
*/

// global datafile reader and writer
CDataFileReader DataFile;
CDataFileWriter df;

// global new image data (set by ReplaceImageItem)
int NewNameID   = -1;
char aNewName[128];
int NewDataID   = -1;
int NewDataSize = 0;
void* pNewData   = 0;

int LoadPNG(CImageInfo* pImg, const char *pFilename)
{
        unsigned char *pBuffer;
        png_t Png;

        png_init(0,0);
        int Error = png_open_file(&Png, pFilename);
        if(Error != PNG_NO_ERROR)
        {
                dbg_msg("game/png", "failed to open file. filename='%s'", pFilename);
                if(Error != PNG_FILE_ERROR)
                        png_close_file(&Png);
                return 0;
        }

        if(Png.depth != 8 || (Png.color_type != PNG_TRUECOLOR && Png.color_type != PNG_TRUECOLOR_ALPHA) || Png.width > (2<<12) || Png.height > (2<<12)) 
        {
                dbg_msg("game/png", "invalid format. filename='%s'", pFilename);
                png_close_file(&Png);
                return 0;
        }

        pBuffer = (unsigned char *)mem_alloc(Png.width * Png.height * Png.bpp, 1);
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

void ExtractName(const char *pFileName, char *pName, int BufferSize)
{
    const char *pExtractedName = pFileName;
    const char *pEnd = 0;
    for(; *pFileName; ++pFileName)
    {
        if(*pFileName == '/' || *pFileName == '\\')
            pExtractedName = pFileName+1;
        else if(*pFileName == '.')
            pEnd = pFileName;
    }

    int Length = pEnd > pExtractedName ? min(BufferSize, (int)(pEnd-pExtractedName+1)) : BufferSize;
    str_copy(pName, pExtractedName, Length);
}

void* ReplaceImageItem(void *pItem, int Type, char *aImgName, char *aImgFile)
{
    if(Type != MAPITEMTYPE_IMAGE)
        return pItem;

    CMapItemImage *pImgItem = (CMapItemImage*) pItem;
    char *pName = (char *)DataFile.GetData(pImgItem->m_ImageName);

    if(str_comp(aImgName, pName))
        return pItem;

    CImageInfo* pImgInfo = new CImageInfo;
    if(!LoadPNG(pImgInfo, aImgFile))
        return pItem;

    CMapItemImage *pNewImgItem = new CMapItemImage;
    *pNewImgItem = *pImgItem;

    pNewImgItem->m_Width  = pImgInfo->m_Width;
    pNewImgItem->m_Height = pImgInfo->m_Height;
    int PixelSize = pImgInfo->m_Format == CImageInfo::FORMAT_RGB ? 3 : 4;
    
    NewNameID = pImgItem->m_ImageName;
    ExtractName(aImgFile, aNewName, sizeof(aNewName));
    NewDataID = pImgItem->m_ImageData;
    pNewData = pImgInfo->m_pData;
    NewDataSize = pImgInfo->m_Width*pImgInfo->m_Height*PixelSize;

    return (void *)pNewImgItem;
}

int main(int argc, const char **argv)
{
        IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_BASIC, argc, argv);
        int Index, ID = 0, Type = 0, Size;
        void *pPtr;
        char aFileName[1024], aImageName[1024], aImageFile[1024];

        if(!pStorage || argc != 5) {
                return -1;
        }

        str_format(aFileName, sizeof(aFileName), "%s", argv[2]);
        str_format(aImageName, sizeof(aImageName), "%s", argv[3]);
        str_format(aImageFile, sizeof(aImageFile), "%s", argv[4]);

        if(!DataFile.Open(pStorage, argv[1], IStorage::TYPE_ALL)) {
                return -1;
        }
        if(!df.Open(pStorage, aFileName)) {
                return -1;
        }
    
        // add all items
        for(Index = 0; Index < DataFile.NumItems(); Index++)
        {
                pPtr = DataFile.GetItem(Index, &Type, &ID);
                Size = DataFile.GetItemSize(Index);
                pPtr = ReplaceImageItem(pPtr, Type, aImageName, aImageFile);
                df.AddItem(Type, ID, Size, pPtr);
        }

        // add all data
        for(Index = 0; Index < DataFile.NumItems(); Index++)
        {
                if(Index == NewDataID) {
                    pPtr = pNewData;
                    Size = NewDataSize;
                }
                else if (Index == NewNameID) {
                    pPtr = (void *)aNewName;
                    Size = str_length(aNewName)+1;
                }
                else {
                    pPtr = DataFile.GetData(Index);
                    Size = DataFile.GetUncompressedDataSize(Index);
                }

                df.AddData(Size, pPtr);
        }

        DataFile.Close();
        df.Finish();
        return 0;
}
