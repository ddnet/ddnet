#include "quadart.h"
#include "editor.h"
#include "editor_actions.h"

#include <game/editor/mapitems/image.h>

#include <algorithm>
#include <array>
#include <vector>

CQuadArt::CQuadArt(CQuadArtParameters Parameters, CImageInfo &&Img) :
	m_Parameters(Parameters), m_Img(std::move(Img))
{
	m_vVisitedPixels.resize(m_Img.m_Height * m_Img.m_Width, false);
}

CQuadArt::~CQuadArt()
{
	m_Img.Free();
}

ivec2 CQuadArt::GetOptimizedQuadSize(const ColorRGBA &Pixel, const ivec2 &Pos)
{
	ivec2 OptimizedSize(0, 0);
	ivec2 Size(0, 0);
	size_t ImgPixelSize = m_Parameters.m_ImagePixelSize;

	while(IsPixelOptimizable(Pos + Size, Pixel))
	{
		while(IsPixelOptimizable(Pos + Size, Pixel) && (!OptimizedSize.y || Size.y < OptimizedSize.y))
			Size.y += ImgPixelSize;

		if(!OptimizedSize.y || Size.y < OptimizedSize.y)
			OptimizedSize.y = Size.y;

		Size.y = 0;
		Size.x += ImgPixelSize;
		OptimizedSize.x = Size.x;
	}

	MarkPixelAsVisited(Pos, OptimizedSize);
	Size = OptimizedSize / ImgPixelSize;
	return Size;
}

size_t CQuadArt::FindSuperPixelSize(const ColorRGBA &Pixel, const ivec2 &Pos, const size_t CurrentSize)
{
	ivec2 Size(CurrentSize, CurrentSize);
	if(Pos.x + CurrentSize >= m_Img.m_Width || Pos.y + CurrentSize >= m_Img.m_Height)
	{
		MarkPixelAsVisited(Pos, Size);
		return CurrentSize;
	}

	for(int i = 0; i < 2; i++)
	{
		for(size_t j = 0; j < CurrentSize + 1; j++)
		{
			ivec2 CurrentPos = Pos;
			CurrentPos.x += i == 0 ? j : CurrentSize;
			CurrentPos.y += i == 0 ? CurrentSize : j;

			ColorRGBA CheckPixel = GetPixelClamped(CurrentPos);
			if(CurrentPos.x >= (int)m_Img.m_Width || CurrentPos.y >= (int)m_Img.m_Height || Pixel != CheckPixel)
			{
				MarkPixelAsVisited(Pos, Size);
				return CurrentSize;
			}
		}
	}

	return FindSuperPixelSize(Pixel, Pos, CurrentSize + 1);
}

ColorRGBA CQuadArt::GetPixelClamped(const ivec2 &Pos) const
{
	size_t x = std::clamp<size_t>(Pos.x, 0, m_Img.m_Width - 1);
	size_t y = std::clamp<size_t>(Pos.y, 0, m_Img.m_Height - 1);
	return m_Img.PixelColor(x, y);
}

bool CQuadArt::IsPixelOptimizable(const ivec2 &Pos, const ColorRGBA &Pixel) const
{
	if(Pos.x >= (int)m_Img.m_Width || Pos.y >= (int)m_Img.m_Height)
		return false;
	ColorRGBA CheckPixel = m_Img.PixelColor(Pos.x, Pos.y);
	return !m_vVisitedPixels[Pos.x + Pos.y * m_Img.m_Width] && CheckPixel.a > 0 && Pixel == CheckPixel;
}

void CQuadArt::MarkPixelAsVisited(const ivec2 &Pos, const ivec2 &Size)
{
	for(int y = Pos.y; y < Pos.y + Size.y; y++)
	{
		for(int x = Pos.x; x < Pos.x + Size.x; x++)
		{
			size_t Index = (size_t)(x + y * m_Img.m_Width);
			if(Index < m_vVisitedPixels.size())
				m_vVisitedPixels[Index] = true;
		}
	}
}

CQuad CQuadArt::CreateNewQuad(const vec2 &Pos, const ivec2 &Size, const ColorRGBA &Color) const
{
	CQuad Quad;
	Quad.m_PosEnv = Quad.m_ColorEnv = -1;
	Quad.m_PosEnvOffset = Quad.m_ColorEnvOffset = 0;
	int x = f2fx(Pos.x), y = f2fx(Pos.y), w = f2fx(Size.x / 2.f), h = f2fx(Size.y / 2.f);

	for(int i = 0; i < 2; ++i)
	{
		Quad.m_aPoints[i].y = y - h;
		Quad.m_aPoints[i + 2].y = y + h;
		Quad.m_aPoints[i * 2].x = x - w;
		Quad.m_aPoints[i * 2 + 1].x = x + w;
	}

	for(auto &QuadColor : Quad.m_aColors)
	{
		QuadColor.r = (int)(Color.r * 255);
		QuadColor.g = (int)(Color.g * 255);
		QuadColor.b = (int)(Color.b * 255);
		QuadColor.a = (int)(Color.a * 255);
	}

	Quad.m_aPoints[4].x = m_Parameters.m_Centralize ? i2fx(-1) : x;
	Quad.m_aPoints[4].y = m_Parameters.m_Centralize ? i2fx(-1) : y;

	for(int i = 0; i < 4; ++i)
	{
		Quad.m_aTexcoords[i].x = i2fx(i % 2);
		Quad.m_aTexcoords[i].y = i2fx(i / 2);
	}
	return Quad;
}

bool CQuadArt::Create(std::shared_ptr<CLayerQuads> &pQuadLayer)
{
	size_t MaxNewQuads = std::ceil((float)(m_Img.m_Width * m_Img.m_Height) / m_Parameters.m_ImagePixelSize);
	pQuadLayer->m_vQuads.clear();
	pQuadLayer->m_vQuads.reserve(MaxNewQuads);

	size_t ImgPixelSize = m_Parameters.m_ImagePixelSize;
	ivec2 Scale(1, 1);

	for(size_t y = 0; y < m_Img.m_Height; y += ImgPixelSize)
	{
		for(size_t x = 0; x < m_Img.m_Width; x += ImgPixelSize)
		{
			ivec2 ImgPos(x, y);
			ColorRGBA Pixel = GetPixelClamped(ImgPos);
			if(m_vVisitedPixels[x + y * m_Img.m_Width] || Pixel.a == 0.f)
				continue;

			if(m_Parameters.m_Optimize)
				Scale = GetOptimizedQuadSize(Pixel, ImgPos);

			ivec2 Size = Scale * m_Parameters.m_QuadPixelSize;
			vec2 Pos(((x / (float)ImgPixelSize) + (Scale.x / 2.f)) * m_Parameters.m_QuadPixelSize,
				((y / (float)ImgPixelSize) + (Scale.y / 2.f)) * m_Parameters.m_QuadPixelSize);

			CQuad Quad = CreateNewQuad(Pos, Size, Pixel);
			pQuadLayer->m_vQuads.emplace_back(Quad);
		}
	}
	pQuadLayer->m_vQuads.shrink_to_fit();
	return true;
}

void CEditor::AddQuadArt(bool IgnoreHistory)
{
	char aQuadArtName[IO_MAX_PATH_LENGTH];
	IStorage::StripPathAndExtension(m_QuadArtParameters.m_aFilename, aQuadArtName, sizeof(aQuadArtName));

	std::shared_ptr<CLayerGroup> pGroup = m_Map.NewGroup();
	str_copy(pGroup->m_aName, aQuadArtName);
	pGroup->m_UseClipping = true;
	pGroup->m_ClipX = -1;
	pGroup->m_ClipY = -1;
	pGroup->m_ClipH = std::ceil(m_QuadArtImageInfo.m_Height * 1.f * m_QuadArtParameters.m_QuadPixelSize / m_QuadArtParameters.m_ImagePixelSize) + 2;
	pGroup->m_ClipW = std::ceil(m_QuadArtImageInfo.m_Width * 1.f * m_QuadArtParameters.m_QuadPixelSize / m_QuadArtParameters.m_ImagePixelSize) + 2;

	std::shared_ptr<CLayerQuads> pLayer = std::make_shared<CLayerQuads>(this);
	str_copy(pLayer->m_aName, aQuadArtName);
	pGroup->AddLayer(pLayer);
	pLayer->m_Flags |= LAYERFLAG_DETAIL;

	CQuadArt QuadArt(m_QuadArtParameters, std::move(m_QuadArtImageInfo));
	QuadArt.Create(pLayer);

	if(!IgnoreHistory)
		m_EditorHistory.RecordAction(std::make_shared<CEditorActionQuadArt>(this, m_QuadArtParameters));

	m_Map.OnModify();
	m_Dialog = DIALOG_NONE;
}

bool CEditor::CallbackAddQuadArt(const char *pFilepath, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;

	pEditor->m_QuadArtParameters.m_ImagePixelSize = 1;
	pEditor->m_QuadArtParameters.m_QuadPixelSize = 4;
	pEditor->m_QuadArtParameters.m_Optimize = true;
	pEditor->m_QuadArtParameters.m_Centralize = false;

	if(!pEditor->Graphics()->LoadPng(pEditor->m_QuadArtImageInfo, pFilepath, StorageType))
	{
		pEditor->ShowFileDialogError("Failed to load image from file '%s'.", pFilepath);
		return false;
	}

	str_copy(pEditor->m_QuadArtParameters.m_aFilename, pFilepath);

	CUIRect View = *(pEditor->Ui()->Screen());

	static SPopupMenuId s_PopupQuadArtId;
	constexpr float PopupWidth = 400.0f;
	constexpr float PopupHeight = 120.0f;
	pEditor->Ui()->DoPopupMenu(&s_PopupQuadArtId, View.w / 2.0f - PopupWidth / 2.0f, View.h / 2.0f - PopupHeight / 2.0f, PopupWidth, PopupHeight, pEditor, PopupQuadArt);
	return false;
}
