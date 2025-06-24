#ifndef GAME_EDITOR_QUADART_H
#define GAME_EDITOR_QUADART_H

#include <base/types.h>
#include <engine/image.h>
#include <game/editor/mapitems/layer_quads.h>
#include <game/mapitems.h>
#include <memory>
#include <optional>

class CQuadArtParameters
{
public:
	int m_ImagePixelSize;
	int m_QuadPixelSize;
	bool m_Centralize;
	bool m_Optimize;
	char m_aFilename[IO_MAX_PATH_LENGTH];
};

class CQuadArt
{
public:
	CQuadArt(CQuadArtParameters Parameters, CImageInfo &&Img);
	~CQuadArt();
	bool Create(std::shared_ptr<CLayerQuads> &pQuadLayer);

private:
	ivec2 GetOptimizedQuadSize(const ColorRGBA &Pixel, const ivec2 &Pos);
	void MarkPixelAsVisited(const ivec2 &Pos, const ivec2 &Size);

	size_t FindSuperPixelSize(const ColorRGBA &Pixel, const ivec2 &Pos, const size_t CurrentSize);

	ColorRGBA GetPixelClamped(const ivec2 &Pos) const;
	bool IsPixelOptimizable(const ivec2 &Pos, const ColorRGBA &Pixel) const;

	CQuad CreateNewQuad(const vec2 &Pos, const ivec2 &Size, const ColorRGBA &Color) const;

	CQuadArtParameters m_Parameters;
	CImageInfo m_Img;
	std::vector<bool> m_vVisitedPixels;
};
#endif
