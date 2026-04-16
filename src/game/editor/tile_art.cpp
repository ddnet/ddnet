#include "editor.h"
#include "editor_actions.h"

#include <base/fs.h>
#include <base/str.h>

#include <game/editor/mapitems/image.h>

#include <array>

static bool operator<(const ColorRGBA &Left, const ColorRGBA &Right) // NOLINT(unused-function)
{
	if(Left.r != Right.r)
		return Left.r < Right.r;
	else if(Left.g != Right.g)
		return Left.g < Right.g;
	else if(Left.b != Right.b)
		return Left.b < Right.b;
	else
		return Left.a < Right.a;
}

static std::vector<ColorRGBA> GetUniqueColors(const CImageInfo &Image)
{
	std::set<ColorRGBA> ColorSet;
	std::vector<ColorRGBA> vUniqueColors;
	for(size_t x = 0; x < Image.m_Width; x++)
	{
		for(size_t y = 0; y < Image.m_Height; y++)
		{
			ColorRGBA Color = Image.PixelColor(x, y);
			if(Color.a > 0 && ColorSet.insert(Color).second)
				vUniqueColors.push_back(Color);
		}
	}
	std::sort(vUniqueColors.begin(), vUniqueColors.end());

	return vUniqueColors;
}

constexpr int NumTilesRow = 16;
constexpr int NumTilesColumn = 16;
constexpr int NumTiles = NumTilesRow * NumTilesColumn;
constexpr int TileSize = 64;

static int GetColorIndex(const std::array<ColorRGBA, NumTiles> &ColorGroup, ColorRGBA Color)
{
	std::array<ColorRGBA, NumTiles>::const_iterator Iterator = std::find(ColorGroup.begin(), ColorGroup.end(), Color);
	if(Iterator == ColorGroup.end())
		return 0;
	return Iterator - ColorGroup.begin();
}

static std::vector<std::array<ColorRGBA, NumTiles>> GroupColors(const std::vector<ColorRGBA> &vColors)
{
	std::vector<std::array<ColorRGBA, NumTiles>> vaColorGroups;

	for(size_t i = 0; i < vColors.size(); i += NumTiles - 1)
	{
		auto &Group = vaColorGroups.emplace_back();
		std::copy_n(vColors.begin() + i, std::min<size_t>(NumTiles - 1, vColors.size() - i), Group.begin() + 1);
	}

	return vaColorGroups;
}

static void SetColorTile(CImageInfo &Image, int x, int y, ColorRGBA Color)
{
	for(int i = 0; i < TileSize; i++)
	{
		for(int j = 0; j < TileSize; j++)
			Image.SetPixelColor(x * TileSize + i, y * TileSize + j, Color);
	}
}

static CImageInfo ColorGroupToImage(const std::array<ColorRGBA, NumTiles> &aColorGroup)
{
	CImageInfo Image;
	Image.m_Width = NumTilesRow * TileSize;
	Image.m_Height = NumTilesColumn * TileSize;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.Allocate();

	for(int y = 0; y < NumTilesColumn; y++)
	{
		for(int x = 0; x < NumTilesRow; x++)
		{
			int ColorIndex = x + NumTilesRow * y;
			SetColorTile(Image, x, y, aColorGroup[ColorIndex]);
		}
	}

	return Image;
}

static std::vector<CImageInfo> ColorGroupsToImages(const std::vector<std::array<ColorRGBA, NumTiles>> &vaColorGroups)
{
	std::vector<CImageInfo> vImages;
	vImages.reserve(vaColorGroups.size());
	for(const auto &ColorGroup : vaColorGroups)
		vImages.push_back(ColorGroupToImage(ColorGroup));

	return vImages;
}

static std::shared_ptr<CEditorImage> ImageInfoToEditorImage(CEditorMap *pMap, CImageInfo &Image, const char *pName)
{
	std::shared_ptr<CEditorImage> pEditorImage = std::make_shared<CEditorImage>(pMap);
	*pEditorImage = std::move(Image);

	pEditorImage->m_Texture = pMap->Editor()->Graphics()->LoadTextureRaw(*pEditorImage, pMap->Editor()->Graphics()->TextureLoadFlags(), pName);
	pEditorImage->m_External = 0;
	str_copy(pEditorImage->m_aName, pName);

	return pEditorImage;
}

static std::shared_ptr<CLayerTiles> AddLayerWithImage(CEditorMap *pMap, const std::shared_ptr<CLayerGroup> &pGroup, CImageInfo &Image, const char *pName)
{
	std::shared_ptr<CEditorImage> pEditorImage = ImageInfoToEditorImage(pMap, Image, pName);
	pMap->m_vpImages.push_back(pEditorImage);

	std::shared_ptr<CLayerTiles> pLayer = std::make_shared<CLayerTiles>(pMap, pEditorImage->m_Width, pEditorImage->m_Height);
	str_copy(pLayer->m_aName, pName);
	pLayer->m_Image = pMap->m_vpImages.size() - 1;
	pGroup->AddLayer(pLayer);

	return pLayer;
}

static void SetTilelayerIndices(const std::shared_ptr<CLayerTiles> &pLayer, const std::array<ColorRGBA, NumTiles> &aColorGroup, const CImageInfo &Image)
{
	for(int x = 0; x < pLayer->m_Width; x++)
	{
		for(int y = 0; y < pLayer->m_Height; y++)
			pLayer->m_pTiles[x + y * pLayer->m_Width].m_Index = GetColorIndex(aColorGroup, Image.PixelColor(x, y));
	}
}

void CEditorMap::AddTileArt(CImageInfo &&Image, const char *pFilename, bool IgnoreHistory)
{
	char aTileArtFilename[IO_MAX_PATH_LENGTH];
	fs_split_file_extension(fs_filename(pFilename), aTileArtFilename, sizeof(aTileArtFilename));

	std::shared_ptr<CLayerGroup> pGroup = NewGroup();
	str_copy(pGroup->m_aName, aTileArtFilename);

	int ImageCount = m_vpImages.size();

	auto vUniqueColors = GetUniqueColors(Image);
	auto vaColorGroups = GroupColors(vUniqueColors);
	auto vColorImages = ColorGroupsToImages(vaColorGroups);
	char aImageName[IO_MAX_PATH_LENGTH];
	for(size_t i = 0; i < vColorImages.size(); i++)
	{
		str_format(aImageName, sizeof(aImageName), "%s %" PRIzu, aTileArtFilename, i + 1);
		std::shared_ptr<CLayerTiles> pLayer = AddLayerWithImage(this, pGroup, vColorImages[i], aImageName);
		SetTilelayerIndices(pLayer, vaColorGroups[i], Image);
	}
	auto IndexMap = SortImages();

	if(!IgnoreHistory)
	{
		m_EditorHistory.RecordAction(std::make_shared<CEditorActionTileArt>(this, ImageCount, pFilename, IndexMap));
	}

	Image.Free();
	OnModify();
}

void CEditor::TileArtCheckColors()
{
	auto vUniqueColors = GetUniqueColors(m_TileArtImageInfo);
	int NumColorGroups = std::ceil(vUniqueColors.size() / 255.0f);
	if(Map()->m_vpImages.size() + NumColorGroups >= 64)
	{
		m_PopupEventType = CEditor::POPEVENT_TILE_ART_TOO_MANY_COLORS;
		m_PopupEventActivated = true;
		m_TileArtImageInfo.Free();
	}
	else if(NumColorGroups > 1)
	{
		m_PopupEventType = CEditor::POPEVENT_TILE_ART_MANY_COLORS;
		m_PopupEventActivated = true;
	}
	else
	{
		Map()->AddTileArt(std::move(m_TileArtImageInfo), m_aTileArtFilename, false);
		OnDialogClose();
	}
}

bool CEditor::CallbackAddTileArt(const char *pFilepath, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;

	if(!pEditor->Graphics()->LoadPng(pEditor->m_TileArtImageInfo, pFilepath, StorageType))
	{
		pEditor->ShowFileDialogError("Failed to load image from file '%s'.", pFilepath);
		return false;
	}

	str_copy(pEditor->m_aTileArtFilename, pFilepath);
	if(pEditor->m_TileArtImageInfo.m_Width * pEditor->m_TileArtImageInfo.m_Height > 10'000)
	{
		pEditor->m_PopupEventType = CEditor::POPEVENT_TILE_ART_BIG_IMAGE;
		pEditor->m_PopupEventActivated = true;
		return false;
	}
	else
	{
		pEditor->TileArtCheckColors();
		return false;
	}
}
