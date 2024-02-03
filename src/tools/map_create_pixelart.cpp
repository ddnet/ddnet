#include <base/logger.h>
#include <base/system.h>
#include <engine/gfx/image_loader.h>
#include <engine/graphics.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/mapitems.h>

bool CreatePixelArt(const char[3][64], const int[2], const int[2], int[2], const bool[2]);
void InsertCurrentQuads(CDataFileReader &, CMapItemLayerQuads *, CQuad *);
int InsertPixelArtQuads(CQuad *, int &, const CImageInfo &, const int[2], const int[2], const bool[2]);

bool LoadPNG(CImageInfo *, const char *);
bool OpenMaps(const char[2][64], CDataFileReader &, CDataFileWriter &);
void SaveOutputMap(CDataFileReader &, CDataFileWriter &, CMapItemLayerQuads *, int, CQuad *, int);

CMapItemLayerQuads *GetQuadLayer(CDataFileReader &, const int[2], int *);
CQuad CreateNewQuad(float, float, int, int, const uint8_t[4], const int[2]);

bool GetPixelClamped(const CImageInfo &, int, int, uint8_t[4]);
bool ComparePixel(const uint8_t[4], const uint8_t[4]);
bool IsPixelOptimizable(const CImageInfo &, int, int, const uint8_t[4], const bool[]);
void SetVisitedPixels(const CImageInfo &, int, int, int, int, bool[]);

int GetImagePixelSize(const CImageInfo &);
int FindSuperPixelSize(const CImageInfo &, const uint8_t[4], int, int, int, bool[]);
void GetOptimizedQuadSize(const CImageInfo &, int, const uint8_t[4], int, int, int &, int &, bool[]);

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	if(argc < 9 || argc > 12)
	{
		dbg_msg("map_create_pixelart", "Invalid arguments");
		dbg_msg("map_create_pixelart", "Usage: %s <image.png> <img_pixelsize> <input_map> <layergroup_id> <layer_id> <pos_x> <pos_y> <quad_pixelsize> <output_map> [optimize=0|1] [centralize=0|1]", argv[0]);
		dbg_msg("map_create_pixelart", "Note: use destination layer tiles as a reference for positions and pixels for sizes.");
		dbg_msg("map_create_pixelart", "Note: set img_pixelsize to 0 to consider the largest possible size.");
		dbg_msg("map_create_pixelart", "Note: set quad_pixelsize to 0 to consider the same value of img_pixelsize.");
		dbg_msg("map_create_pixelart", "Note: if image.png has not a perfect pixelart grid, result might be unexpected, manually fix img_pixelsize to get it better.");
		dbg_msg("map_create_pixelart", "Options: optimize tries to reduce the total number of quads (default: 1).");
		dbg_msg("map_create_pixelart", "Options: centralize places all pivots at the same position (default: 0).");

		return -1;
	}

	char aFilenames[3][64];
	str_copy(aFilenames[0], argv[3]); //input_map
	str_copy(aFilenames[1], argv[9]); //output_map
	str_copy(aFilenames[2], argv[1]); //image_file

	int aLayerID[2] = {str_toint(argv[4]), str_toint(argv[5])}; //layergroup_id, layer_id
	int aStartingPos[2] = {str_toint(argv[6]) * 32, str_toint(argv[7]) * 32}; //pos_x, pos_y
	int aPixelSizes[2] = {str_toint(argv[2]), str_toint(argv[8])}; //quad_pixelsize, img_pixelsize

	bool aArtOptions[3];
	aArtOptions[0] = argc >= 10 ? str_toint(argv[10]) : true; //optimize
	aArtOptions[1] = argc >= 11 ? str_toint(argv[11]) : false; //centralize

	dbg_msg("map_create_pixelart", "image_file='%s'; image_pixelsize='%dpx'; input_map='%s'; layergroup_id='#%d'; layer_id='#%d'; pos_x='#%dpx'; pos_y='%dpx'; quad_pixelsize='%dpx'; output_map='%s'; optimize='%d'; centralize='%d'",
		aFilenames[2], aPixelSizes[0], aFilenames[1], aLayerID[0], aLayerID[1], aStartingPos[0], aStartingPos[1], aPixelSizes[1], aFilenames[2], aArtOptions[0], aArtOptions[1]);

	return !CreatePixelArt(aFilenames, aLayerID, aStartingPos, aPixelSizes, aArtOptions);
}

bool CreatePixelArt(const char aFilenames[3][64], const int aLayerID[2], const int aStartingPos[2], int aPixelSizes[2], const bool aArtOptions[2])
{
	CImageInfo Img;
	if(!LoadPNG(&Img, aFilenames[2]))
		return false;

	aPixelSizes[0] = aPixelSizes[0] ? aPixelSizes[0] : GetImagePixelSize(Img);
	aPixelSizes[1] = aPixelSizes[1] ? aPixelSizes[1] : aPixelSizes[0];

	CDataFileReader InputMap;
	CDataFileWriter OutputMap;
	if(!OpenMaps(aFilenames, InputMap, OutputMap))
		return false;

	int ItemNumber = 0;
	CMapItemLayerQuads *pQuadLayer = GetQuadLayer(InputMap, aLayerID, &ItemNumber);
	if(!pQuadLayer)
		return false;

	int MaxNewQuads = std::ceil((Img.m_Width * Img.m_Height) / aPixelSizes[0]);
	CQuad *pQuads = new CQuad[pQuadLayer->m_NumQuads + MaxNewQuads];

	InsertCurrentQuads(InputMap, pQuadLayer, pQuads);
	int QuadsCounter = InsertPixelArtQuads(pQuads, pQuadLayer->m_NumQuads, Img, aStartingPos, aPixelSizes, aArtOptions);
	SaveOutputMap(InputMap, OutputMap, pQuadLayer, ItemNumber, pQuads, ((int)sizeof(CQuad)) * (pQuadLayer->m_NumQuads + 1));
	delete[] pQuads;

	dbg_msg("map_create_pixelart", "INFO: successfully added %d new pixelart quads.", QuadsCounter);
	return true;
}

void InsertCurrentQuads(CDataFileReader &InputMap, CMapItemLayerQuads *pQuadLayer, CQuad *pNewQuads)
{
	CQuad *pCurrentQuads = (CQuad *)InputMap.GetDataSwapped(pQuadLayer->m_Data);
	for(int i = 0; i < pQuadLayer->m_NumQuads; i++)
		pNewQuads[i] = pCurrentQuads[i];
}

int InsertPixelArtQuads(CQuad *pQuads, int &NumQuads, const CImageInfo &Img, const int aStartingPos[2], const int aPixelSizes[2], const bool aArtOptions[2])
{
	int ImgPixelSize = aPixelSizes[0], QuadPixelSize = aPixelSizes[1], OriginalNumQuads = NumQuads;
	int aForcedPivot[2] = {std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
	bool *aVisitedPixels = new bool[Img.m_Height * Img.m_Width];
	mem_zero(aVisitedPixels, sizeof(bool) * Img.m_Height * Img.m_Width);

	for(int y = 0; y < Img.m_Height; y += ImgPixelSize)
		for(int x = 0; x < Img.m_Width; x += ImgPixelSize)
		{
			uint8_t aPixel[4];
			if(aVisitedPixels[x + y * Img.m_Width] || !GetPixelClamped(Img, x, y, aPixel))
				continue;

			int Width = 1, Height = 1;
			if(aArtOptions[0])
				GetOptimizedQuadSize(Img, ImgPixelSize, aPixel, x, y, Width, Height, aVisitedPixels);

			float Posx = aStartingPos[0] + ((x / (float)ImgPixelSize) + (Width / 2.f)) * QuadPixelSize;
			float Posy = aStartingPos[1] + ((y / (float)ImgPixelSize) + (Height / 2.f)) * QuadPixelSize;
			if(aArtOptions[1] && aForcedPivot[0] == std::numeric_limits<int>::max())
			{
				aForcedPivot[0] = Posx;
				aForcedPivot[1] = Posy;
			}

			pQuads[NumQuads] = CreateNewQuad(Posx, Posy, QuadPixelSize * Width, QuadPixelSize * Height, aPixel, aArtOptions[1] ? aForcedPivot : 0x0);
			NumQuads++;
		}
	delete[] aVisitedPixels;

	return NumQuads - OriginalNumQuads;
}

void GetOptimizedQuadSize(const CImageInfo &Img, const int ImgPixelSize, const uint8_t aPixel[4], const int PosX, const int PosY, int &Width, int &Height, bool aVisitedPixels[])
{
	int w = 0, h = 0, OptimizedWidth = 0, OptimizedHeight = 0;

	while(IsPixelOptimizable(Img, PosX + w, PosY + h, aPixel, aVisitedPixels))
	{
		while(IsPixelOptimizable(Img, PosX + w, PosY + h, aPixel, aVisitedPixels) && (!OptimizedHeight || h < OptimizedHeight))
			h += ImgPixelSize;

		if(!OptimizedHeight || h < OptimizedHeight)
			OptimizedHeight = h;

		h = 0;
		w += ImgPixelSize;
		OptimizedWidth = w;
	}

	SetVisitedPixels(Img, PosX, PosY, OptimizedWidth, OptimizedHeight, aVisitedPixels);
	Width = OptimizedWidth / ImgPixelSize;
	Height = OptimizedHeight / ImgPixelSize;
}

int GetImagePixelSize(const CImageInfo &Img)
{
	int ImgPixelSize = std::numeric_limits<int>::max();
	bool *aVisitedPixels = new bool[Img.m_Height * Img.m_Width];
	mem_zero(aVisitedPixels, sizeof(bool) * Img.m_Height * Img.m_Width);

	for(int y = 0; y < Img.m_Height && ImgPixelSize > 1; y++)
		for(int x = 0; x < Img.m_Width && ImgPixelSize > 1; x++)
		{
			uint8_t aPixel[4];
			if(aVisitedPixels[x + y * Img.m_Width])
				continue;

			GetPixelClamped(Img, x, y, aPixel);
			int SuperPixelSize = FindSuperPixelSize(Img, aPixel, x, y, 1, aVisitedPixels);
			if(SuperPixelSize < ImgPixelSize)
				ImgPixelSize = SuperPixelSize;
		}
	delete[] aVisitedPixels;

	dbg_msg("map_create_pixelart", "INFO: automatically detected img_pixelsize of %dpx", ImgPixelSize);
	return ImgPixelSize;
}

int FindSuperPixelSize(const CImageInfo &Img, const uint8_t aPixel[4], const int PosX, const int PosY, const int CurrentSize, bool aVisitedPixels[])
{
	if(PosX + CurrentSize >= Img.m_Width || PosY + CurrentSize >= Img.m_Height)
	{
		SetVisitedPixels(Img, PosX, PosY, CurrentSize, CurrentSize, aVisitedPixels);
		return CurrentSize;
	}

	for(int i = 0; i < 2; i++)
	{
		for(int j = 0; j < CurrentSize + 1; j++)
		{
			uint8_t aCheckPixel[4];
			int x = PosX, y = PosY;
			x += i == 0 ? j : CurrentSize;
			y += i == 0 ? CurrentSize : j;

			GetPixelClamped(Img, x, y, aCheckPixel);
			if(x >= Img.m_Width || y >= Img.m_Height || !ComparePixel(aPixel, aCheckPixel))
			{
				SetVisitedPixels(Img, PosX, PosY, CurrentSize, CurrentSize, aVisitedPixels);
				return CurrentSize;
			}
		}
	}

	return FindSuperPixelSize(Img, aPixel, PosX, PosY, CurrentSize + 1, aVisitedPixels);
}

bool GetPixelClamped(const CImageInfo &Img, int x, int y, uint8_t aPixel[4])
{
	x = clamp<int>(x, 0, (int)Img.m_Width - 1);
	y = clamp<int>(y, 0, (int)Img.m_Height - 1);
	aPixel[0] = 255;
	aPixel[1] = 255;
	aPixel[2] = 255;
	aPixel[3] = 255;

	const size_t PixelSize = Img.PixelSize();
	for(size_t i = 0; i < PixelSize; i++)
		aPixel[i] = ((uint8_t *)Img.m_pData)[x * PixelSize + (Img.m_Width * PixelSize * y) + i];

	return aPixel[3] > 0;
}

bool ComparePixel(const uint8_t aPixel1[4], const uint8_t aPixel2[4])
{
	for(int i = 0; i < 4; i++)
		if(aPixel1[i] != aPixel2[i])
			return false;
	return true;
}

bool IsPixelOptimizable(const CImageInfo &Img, const int PosX, const int PosY, const uint8_t aPixel[4], const bool aVisitedPixels[])
{
	uint8_t aCheckPixel[4];
	return PosX < Img.m_Width && PosY < Img.m_Height && !aVisitedPixels[PosX + PosY * Img.m_Width] && GetPixelClamped(Img, PosX, PosY, aCheckPixel) && ComparePixel(aPixel, aCheckPixel);
}

void SetVisitedPixels(const CImageInfo &Img, int PosX, int PosY, int Width, int Height, bool aVisitedPixels[])
{
	for(int y = PosY; y < PosY + Height; y++)
		for(int x = PosX; x < PosX + Width; x++)
			aVisitedPixels[x + y * Img.m_Width] = true;
}

CMapItemLayerQuads *GetQuadLayer(CDataFileReader &InputMap, const int aLayerID[2], int *pItemNumber)
{
	int Start, Num;
	InputMap.GetType(MAPITEMTYPE_GROUP, &Start, &Num);

	CMapItemGroup *pGroupItem = aLayerID[0] >= Num ? 0x0 : (CMapItemGroup *)InputMap.GetItem(Start + aLayerID[0]);

	if(!pGroupItem)
	{
		dbg_msg("map_create_pixelart", "ERROR: unable to find layergroup '#%d'", aLayerID[0]);
		return 0x0;
	}

	InputMap.GetType(MAPITEMTYPE_LAYER, &Start, &Num);
	*pItemNumber = Start + pGroupItem->m_StartLayer + aLayerID[1];

	CMapItemLayer *pLayerItem = aLayerID[1] >= pGroupItem->m_NumLayers ? 0x0 : (CMapItemLayer *)InputMap.GetItem(*pItemNumber);
	if(!pLayerItem)
	{
		dbg_msg("map_create_pixelart", "ERROR: unable to find layer '#%d' in group '#%d'", aLayerID[1], aLayerID[0]);
		return 0x0;
	}

	if(pLayerItem->m_Type != LAYERTYPE_QUADS)
	{
		dbg_msg("map_create_pixelart", "ERROR: layer '#%d' in group '#%d' is not a quad layer", aLayerID[1], aLayerID[0]);
		return 0x0;
	}

	return (CMapItemLayerQuads *)pLayerItem;
}

CQuad CreateNewQuad(const float PosX, const float PosY, const int Width, const int Height, const uint8_t aColor[4], const int aForcedPivot[2] = 0x0)
{
	CQuad Quad;
	Quad.m_PosEnv = Quad.m_ColorEnv = -1;
	Quad.m_PosEnvOffset = Quad.m_ColorEnvOffset = 0;
	float x = f2fx(PosX), y = f2fx(PosY), w = f2fx(Width / 2.f), h = f2fx(Height / 2.f);

	for(int i = 0; i < 2; i++)
	{
		Quad.m_aPoints[i].y = y - h;
		Quad.m_aPoints[i + 2].y = y + h;
		Quad.m_aPoints[i * 2].x = x - w;
		Quad.m_aPoints[i * 2 + 1].x = x + w;
	}

	for(auto &QuadColor : Quad.m_aColors)
	{
		QuadColor.r = aColor[0];
		QuadColor.g = aColor[1];
		QuadColor.b = aColor[2];
		QuadColor.a = aColor[3];
	}

	Quad.m_aPoints[4].x = aForcedPivot ? f2fx(aForcedPivot[0]) : x;
	Quad.m_aPoints[4].y = aForcedPivot ? f2fx(aForcedPivot[1]) : y;

	return Quad;
}

bool LoadPNG(CImageInfo *pImg, const char *pFilename)
{
	IOHANDLE File = io_open(pFilename, IOFLAG_READ);
	if(!File)
	{
		dbg_msg("map_create_pixelart", "ERROR: Unable to open file %s", pFilename);
		return false;
	}

	io_seek(File, 0, IOSEEK_END);
	long int FileSize = io_tell(File);
	if(FileSize <= 0)
	{
		io_close(File);
		dbg_msg("map_create_pixelart", "ERROR: Failed to get file size (%ld). filename='%s'", FileSize, pFilename);
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

	if(!LoadPNG(ImageByteBuffer, pFilename, PngliteIncompatible, pImg->m_Width, pImg->m_Height, pImgBuffer, ImageFormat))
	{
		dbg_msg("map_create_pixelart", "ERROR: Unable to load a valid PNG from file %s", pFilename);
		return false;
	}

	if(ImageFormat != IMAGE_FORMAT_RGBA && ImageFormat != IMAGE_FORMAT_RGB)
	{
		dbg_msg("map_create_pixelart", "ERROR: only RGB and RGBA PNG images are supported");
		free(pImgBuffer);
		return false;
	}

	pImg->m_pData = pImgBuffer;
	pImg->m_Format = ImageFormat == IMAGE_FORMAT_RGB ? CImageInfo::FORMAT_RGB : CImageInfo::FORMAT_RGBA;

	return true;
}

bool OpenMaps(const char pMapNames[2][64], CDataFileReader &InputMap, CDataFileWriter &OutputMap)
{
	IStorage *pStorage = CreateLocalStorage();

	if(!InputMap.Open(pStorage, pMapNames[0], IStorage::TYPE_ABSOLUTE))
	{
		dbg_msg("map_create_pixelart", "ERROR: unable to open map '%s'", pMapNames[0]);
		return false;
	}

	if(!OutputMap.Open(pStorage, pMapNames[1], IStorage::TYPE_ABSOLUTE))
	{
		dbg_msg("map_create_pixelart", "ERROR: unable to open map '%s'", pMapNames[1]);
		return false;
	}

	return true;
}

void SaveOutputMap(CDataFileReader &InputMap, CDataFileWriter &OutputMap, CMapItemLayerQuads *pNewItem, const int NewItemNumber, CQuad *pNewData, const int NewDataSize)
{
	for(int i = 0; i < InputMap.NumItems(); i++)
	{
		int ID, Type;
		CUuid Uuid;
		void *pItem = InputMap.GetItem(i, &Type, &ID, &Uuid);

		// Filter ITEMTYPE_EX items, they will be automatically added again.
		if(Type == ITEMTYPE_EX)
		{
			continue;
		}

		if(i == NewItemNumber)
			pItem = pNewItem;

		int Size = InputMap.GetItemSize(i);
		OutputMap.AddItem(Type, ID, Size, pItem, &Uuid);
	}

	for(int i = 0; i < InputMap.NumData(); i++)
	{
		if(i == pNewItem->m_Data)
			OutputMap.AddData(NewDataSize, pNewData);
		else
			OutputMap.AddData(InputMap.GetDataSize(i), InputMap.GetData(i));
	}

	OutputMap.Finish();
}
