/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_TEXTRENDER_NULL_H
#define ENGINE_TEXTRENDER_NULL_H

#include "textrender.h"

// ft2 texture
#include <ft2build.h>
#include FT_FREETYPE_H

struct CFontSizeData;

class CTextRenderNull : public IEngineTextRender
{
	STextContainer m_TextContainer;

	int GetFreeTextContainerIndex() { return 0; }
	void FreeTextContainerIndex(int Index) {}
	void FreeTextContainer(int Index) {}
	STextContainer &GetTextContainer(int Index) { return m_TextContainer; }
	int WordLength(const char *pText) { return 0; }
	virtual void SetRenderFlags(unsigned int Flags) override {}
	virtual unsigned int GetRenderFlags() override { return 0; }
	void Grow(unsigned char *pIn, unsigned char *pOut, int w, int h, int OutlineCount) {}
	IGraphics::CTextureHandle InitTexture(int Width, int Height, void *pUploadData = NULL) { return IGraphics::CTextureHandle(); }
	void UnloadTexture(IGraphics::CTextureHandle Index) {}
	void IncreaseFontTexture(CFont *pFont, int TextureIndex) {}
	int AdjustOutlineThicknessToFontSize(int OutlineThickness, int FontSize) { return 1; }
	void UploadGlyph(CFont *pFont, int TextureIndex, int PosX, int PosY, int Width, int Height, const unsigned char *pData) {}
	bool GetCharacterSpace(CFont *pFont, int TextureIndex, int Width, int Height, int &PosX, int &PosY) { return false; }
	void RenderGlyph(CFont *pFont, CFontSizeData *pSizeData, int Chr) {}
	struct SFontSizeChar *GetChar(CFont *pFont, CFontSizeData *pSizeData, int Chr) { return NULL; }
	float Kerning(CFont *pFont, FT_UInt GlyphIndexLeft, FT_UInt GlyphIndexRight) { return 1.0f; }

public:
	CTextRenderNull() {}
	virtual ~CTextRenderNull() override {}
	virtual void Init() override {}
	virtual CFont *LoadFont(const char *pFilename, const unsigned char *pBuf, size_t Size) override { return NULL; }
	virtual bool LoadFallbackFont(CFont *pFont, const char *pFilename, const unsigned char *pBuf, size_t Size) override { return false; }
	virtual CFont *GetFont(int FontIndex) override { return NULL; }
	CFont *GetFont(const char *pFilename) override { return NULL; }
	virtual void SetDefaultFont(CFont *pFont) override {}
	virtual void SetCurFont(CFont *pFont) override {}
	virtual void SetCursor(CTextCursor *pCursor, float x, float y, float FontSize, int Flags) override {}
	virtual void MoveCursor(CTextCursor *pCursor, float x, float y) override {}
	virtual void SetCursorPosition(CTextCursor *pCursor, float x, float y) override {}
	virtual void Text(void *pFontSetV, float x, float y, float Size, const char *pText, float LineWidth) override {}
	virtual float TextWidth(void *pFontSetV, float Size, const char *pText, int StrLength, float LineWidth, float *pAlignedHeight = NULL, float *pMaxCharacterHeightInLine = NULL) override { return 1.0f; }
	virtual int TextLineCount(void *pFontSetV, float Size, const char *pText, float LineWidth) override { return 0; }

	virtual void TextColor(float r, float g, float b, float a) override {}
	virtual void TextColor(ColorRGBA rgb) override {}
	virtual void TextOutlineColor(float r, float g, float b, float a) override {}
	virtual void TextOutlineColor(ColorRGBA rgb) override {}
	virtual void TextSelectionColor(float r, float g, float b, float a) override {}
	virtual void TextSelectionColor(ColorRGBA rgb) override {}

	virtual ColorRGBA GetTextColor() override { return ColorRGBA(); }
	virtual ColorRGBA GetTextOutlineColor() override { return ColorRGBA(); }
	virtual ColorRGBA GetTextSelectionColor() override { return ColorRGBA(); }

	virtual void TextEx(CTextCursor *pCursor, const char *pText, int Length) override {}

	virtual int CreateTextContainer(CTextCursor *pCursor, const char *pText, int Length = -1) override { return 0; }
	virtual void AppendTextContainer(CTextCursor *pCursor, int TextContainerIndex, const char *pText, int Length = -1) override {}
	virtual void RecreateTextContainer(CTextCursor *pCursor, int TextContainerIndex, const char *pText, int Length = -1) override {}
	virtual void RecreateTextContainerSoft(CTextCursor *pCursor, int TextContainerIndex, const char *pText, int Length = -1) override {}
	virtual void DeleteTextContainer(int TextContainerIndex) override {}
	virtual void UploadTextContainer(int TextContainerIndex) override {}
	virtual void RenderTextContainer(int TextContainerIndex, STextRenderColor *pTextColor, STextRenderColor *pTextOutlineColor) override {}
	virtual void RenderTextContainer(int TextContainerIndex, STextRenderColor *pTextColor, STextRenderColor *pTextOutlineColor, float X, float Y) override {}
	virtual void UploadEntityLayerText(void *pTexBuff, int ImageColorChannelCount, int TexWidth, int TexHeight, int TexSubWidth, int TexSubHeight, const char *pText, int Length, float x, float y, int FontSize) override {}
	virtual int AdjustFontSize(const char *pText, int TextLength, int MaxSize, int MaxWidth) override { return 1; }

	virtual float GetGlyphOffsetX(int FontSize, char TextCharacter) override { return 0; }
	virtual int CalculateTextWidth(const char *pText, int TextLength, int FontWidth, int FontHeight) override { return 1; }
	virtual bool SelectionToUTF8OffSets(const char *pText, int SelStart, int SelEnd, int &OffUTF8Start, int &OffUTF8End) override { return false; }
	virtual bool UTF8OffToDecodedOff(const char *pText, int UTF8Off, int &DecodedOff) override { return false; }
	virtual bool DecodedOffToUTF8Off(const char *pText, int DecodedOff, int &UTF8Off) override { return false; }
	virtual void OnWindowResize() override {}
};

#endif
