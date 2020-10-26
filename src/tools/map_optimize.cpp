#include <algorithm>
#include <base/math.h>
#include <base/system.h>
#include <engine/shared/datafile.h>
#include <engine/shared/image_manipulation.h>
#include <engine/storage.h>
#include <game/mapitems.h>
#include <stdint.h>
#include <utility>
#include <vector>

void ClearTransparentPixels(uint8_t *pImg, int Width, int Height)
{
	for(int y = 0; y < Height; ++y)
	{
		for(int x = 0; x < Width; ++x)
		{
			int Index = y * Width * 4 + x * 4;
			if(pImg[Index + 3] == 0)
			{
				pImg[Index + 0] = 0;
				pImg[Index + 1] = 0;
				pImg[Index + 2] = 0;
			}
		}
	}
}

void CopyOpaquePixels(uint8_t *pDestImg, uint8_t *pSrcImg, int Width, int Height)
{
	for(int y = 0; y < Height; ++y)
	{
		for(int x = 0; x < Width; ++x)
		{
			int Index = y * Width * 4 + x * 4;
			if(pSrcImg[Index + 3] > TW_DILATE_ALPHA_THRESHOLD)
				mem_copy(&pDestImg[Index], &pSrcImg[Index], sizeof(uint8_t) * 4);
			else
				mem_zero(&pDestImg[Index], sizeof(uint8_t) * 4);
		}
	}
}

void ClearPixelsTile(uint8_t *pImg, int Width, int Height, int TileIndex)
{
	int WTile = Width / 16;
	int HTile = Height / 16;
	int xi = (TileIndex % 16) * WTile;
	int yi = (TileIndex / 16) * HTile;

	for(int y = yi; y < yi + HTile; ++y)
	{
		for(int x = xi; x < xi + WTile; ++x)
		{
			int Index = y * Width * 4 + x * 4;
			pImg[Index + 0] = 0;
			pImg[Index + 1] = 0;
			pImg[Index + 2] = 0;
			pImg[Index + 3] = 0;
		}
	}
}

void GetImageSHA256(uint8_t *pImgBuff, int ImgSize, int Width, int Height, char *pSHA256Str)
{
	uint8_t *pNewImgBuff = (uint8_t *)malloc(ImgSize);

	// Get all image pixels, that have a alpha threshold over the default threshold,
	// all other pixels are cleared to zero.
	// This is required since dilate modifies pixels under the alpha threshold, which would alter the SHA.
	CopyOpaquePixels(pNewImgBuff, pImgBuff, Width, Height);
	SHA256_DIGEST SHAStr = sha256(pNewImgBuff, (size_t)ImgSize);

	sha256_str(SHAStr, pSHA256Str, SHA256_MAXSTRSIZE * sizeof(char));

	free(pNewImgBuff);
}

int main(int argc, const char **argv)
{
	dbg_logger_stdout();

	IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_BASIC, argc, argv);
	int Index, ID = 0, Type = 0, Size;
	void *pPtr;
	char aFileName[1024];
	CDataFileReader DataFile;
	CDataFileWriter df;

	if(!pStorage || argc <= 1 || argc > 3)
	{
		dbg_msg("map_optimize", "Invalid parameters or other unknown error.");
		dbg_msg("map_optimize", "Usage: map_optimize <source map filepath> [<dest map filepath>]");
		return -1;
	}

	if(argc == 3)
	{
		str_format(aFileName, sizeof(aFileName), "out/%s", argv[2]);

		fs_makedir_rec_for(aFileName);
	}
	else
	{
		fs_makedir("out");
		char aBuff[MAX_PATH_LENGTH];
		pStorage->StripPathAndExtension(argv[1], aBuff, sizeof(aBuff));
		str_format(aFileName, sizeof(aFileName), "out/%s.map", aBuff);
	}

	if(!DataFile.Open(pStorage, argv[1], IStorage::TYPE_ABSOLUTE))
	{
		dbg_msg("map_optimize", "Failed to open source file.");
		return -1;
	}

	if(!df.Open(pStorage, aFileName, IStorage::TYPE_ABSOLUTE))
	{
		dbg_msg("map_optimize", "Failed to open target file.");
		return -1;
	}

	int aImageFlags[64] = {
		0,
	};

	bool aImageTiles[64][256]{
		{
			false,
		},
	};

	struct SMapOptimizeItem
	{
		CMapItemImage *m_pImage;
		int m_Index;
		int m_Data;
		int m_Text;
	};

	std::vector<SMapOptimizeItem> DataFindHelper;

	// add all items
	int i = 0;
	for(int Index = 0; Index < DataFile.NumItems(); Index++)
	{
		pPtr = DataFile.GetItem(Index, &Type, &ID);
		Size = DataFile.GetItemSize(Index);
		// for all layers, check if it uses a image and set the corresponding flag
		if(Type == MAPITEMTYPE_LAYER)
		{
			CMapItemLayer *pLayer = (CMapItemLayer *)pPtr;
			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CMapItemLayerTilemap *pTLayer = (CMapItemLayerTilemap *)pLayer;
				if(pTLayer->m_Image >= 0 && pTLayer->m_Image < 64 && pTLayer->m_Flags == 0)
				{
					aImageFlags[pTLayer->m_Image] |= 1;
					// check tiles that are used in this image
					int DataIndex = pTLayer->m_Data;
					unsigned int Size = DataFile.GetDataSize(DataIndex);
					void *pTiles = DataFile.GetData(DataIndex);
					unsigned int TileSize = sizeof(CTile);

					if(Size >= pTLayer->m_Width * pTLayer->m_Height * TileSize)
					{
						int x = 0;
						int y = 0;
						for(y = 0; y < pTLayer->m_Height; ++y)
						{
							for(x = 0; x < pTLayer->m_Width; ++x)
							{
								int TileIndex = ((CTile *)pTiles)[y * pTLayer->m_Width + x].m_Index;
								if(TileIndex > 0)
								{
									aImageTiles[pTLayer->m_Image][TileIndex] = true;
								}
							}
						}
					}
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				CMapItemLayerQuads *pQLayer = (CMapItemLayerQuads *)pLayer;
				if(pQLayer->m_Image >= 0 && pQLayer->m_Image < 64)
				{
					aImageFlags[pQLayer->m_Image] |= 2;
				}
			}
		}
		else if(Type == MAPITEMTYPE_IMAGE)
		{
			CMapItemImage *pImg = (CMapItemImage *)pPtr;
			if(!pImg->m_External)
			{
				SMapOptimizeItem Item;
				Item.m_pImage = pImg;
				Item.m_Index = i;
				Item.m_Data = pImg->m_ImageData;
				Item.m_Text = pImg->m_ImageName;
				DataFindHelper.push_back(Item);
			}

			// found an image
			++i;
		}

		df.AddItem(Type, ID, Size, pPtr);
	}

	// add all data
	for(Index = 0; Index < DataFile.NumData(); Index++)
	{
		bool DeletePtr = false;
		pPtr = DataFile.GetData(Index);
		Size = DataFile.GetDataSize(Index);
		std::vector<SMapOptimizeItem>::iterator it = std::find_if(DataFindHelper.begin(), DataFindHelper.end(), [Index](const SMapOptimizeItem &Other) -> bool { return Other.m_Data == Index || Other.m_Text == Index; });
		if(it != DataFindHelper.end())
		{
			int Width = it->m_pImage->m_Width;
			int Height = it->m_pImage->m_Height;

			int ImageIndex = it->m_Index;
			if(it->m_Data == Index)
			{
				DeletePtr = true;
				// optimize embedded images
				// use a new pointer, to be safe, when using the original image data
				void *pNewPtr = malloc(Size);
				mem_copy(pNewPtr, pPtr, Size);
				pPtr = pNewPtr;
				uint8_t *pImgBuff = (uint8_t *)pPtr;

				bool DoClearTransparentPixels = false;
				bool DilateAs2DArray = false;
				bool DoDilate = false;

				// all tiles that aren't used are cleared(if image was only used by tilemap)
				if(aImageFlags[ImageIndex] == 1)
				{
					for(int i = 0; i < 256; ++i)
					{
						if(!aImageTiles[ImageIndex][i])
						{
							ClearPixelsTile(pImgBuff, Width, Height, i);
						}
					}

					DoClearTransparentPixels = true;
					DilateAs2DArray = true;
					DoDilate = true;
				}
				else if(aImageFlags[ImageIndex] == 0)
				{
					mem_zero(pImgBuff, Width * Height * 4);
				}
				else
				{
					DoClearTransparentPixels = true;
					DoDilate = true;
				}

				if(DoClearTransparentPixels)
				{
					// clear unused pixels and make a clean dilate for the compressor
					ClearTransparentPixels(pImgBuff, Width, Height);
				}

				if(DoDilate)
				{
					if(DilateAs2DArray)
					{
						for(int i = 0; i < 256; ++i)
						{
							int ImgTileW = Width / 16;
							int ImgTileH = Height / 16;
							int x = (i % 16) * ImgTileW;
							int y = (i / 16) * ImgTileH;
							DilateImageSub(pImgBuff, Width, Height, 4, x, y, ImgTileW, ImgTileH);
						}
					}
					else
					{
						DilateImage(pImgBuff, Width, Height, 4);
					}
				}
			}
			else if(it->m_Text == Index)
			{
				char *pImgName = (char *)pPtr;
				uint8_t *pImgBuff = (uint8_t *)DataFile.GetData(it->m_Data);
				int ImgSize = DataFile.GetDataSize(it->m_Data);

				char aSHA256Str[SHA256_MAXSTRSIZE];
				// This is the important function, that calculates the SHA256 in a special way
				// Please read the comments inside the functions to understand it
				GetImageSHA256(pImgBuff, ImgSize, Width, Height, aSHA256Str);

				char aNewName[MAX_PATH_LENGTH];
				int StrLen = str_format(aNewName, sizeof(aNewName) / sizeof(aNewName[0]), "%s_cut_%s", pImgName, aSHA256Str);

				DeletePtr = true;
				// make the new name ready
				char *pNewPtr = (char *)malloc(StrLen + 1);
				str_copy(pNewPtr, aNewName, StrLen + 1);
				pPtr = pNewPtr;
				Size = StrLen + 1;
			}
		}

		df.AddData(Size, pPtr, Z_BEST_COMPRESSION);

		if(DeletePtr)
			free(pPtr);
	}

	DataFile.Close();
	df.Finish();

	return 0;
}
