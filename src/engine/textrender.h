/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_TEXTRENDER_H
#define ENGINE_TEXTRENDER_H
#include "kernel.h"

#include <base/color.h>
#include <engine/graphics.h>
#include <stdint.h>

enum
{
	TEXTFLAG_RENDER = 1,
	TEXTFLAG_ALLOW_NEWLINE = 2,
	TEXTFLAG_STOP_AT_END = 4
};

enum ETextRenderFlags
{
	TEXT_RENDER_FLAG_NO_X_BEARING = 1 << 0,
	TEXT_RENDER_FLAG_NO_Y_BEARING = 1 << 1,
	TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH = 1 << 2,
	TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT = 1 << 3,
	TEXT_RENDER_FLAG_KERNING = 1 << 4,
	TEXT_RENDER_FLAG_NO_OVERSIZE = 1 << 5,
	TEXT_RENDER_FLAG_NO_FIRST_CHARACTER_X_BEARING = 1 << 6,
	TEXT_RENDER_FLAG_NO_LAST_CHARACTER_ADVANCE = 1 << 7,
	TEXT_RENDER_FLAG_NO_AUTOMATIC_QUAD_UPLOAD = 1 << 8,
};

enum
{
	TEXT_FONT_ICON_FONT = 0,
};

class CFont;

class CTextCursor
{
public:
	int m_Flags;
	int m_LineCount;
	int m_GlyphCount;
	int m_CharCount;
	int m_MaxLines;

	float m_StartX;
	float m_StartY;
	float m_LineWidth;
	float m_X, m_Y;
	float m_MaxCharacterHeight;

	float m_LongestLineWidth;

	CFont *m_pFont;
	float m_FontSize;
	float m_AlignedFontSize;
};

struct STextRenderColor
{
	STextRenderColor() {}
	STextRenderColor(float r, float g, float b, float a)
	{
		Set(r, g, b, a);
	}
	STextRenderColor(const ColorRGBA &TextColorRGBA)
	{
		Set(TextColorRGBA.r, TextColorRGBA.g, TextColorRGBA.b, TextColorRGBA.a);
	}

	void Set(float r, float g, float b, float a)
	{
		m_R = r;
		m_G = g;
		m_B = b;
		m_A = a;
	}

	bool operator!=(const STextRenderColor &Other)
	{
		return m_R != Other.m_R || m_G != Other.m_G || m_B != Other.m_B || m_A != Other.m_A;
	}

	float m_R, m_G, m_B, m_A;
};

class ITextRender : public IInterface
{
	MACRO_INTERFACE("textrender", 0)
public:
	virtual void SetCursor(CTextCursor *pCursor, float x, float y, float FontSize, int Flags) = 0;
	virtual void MoveCursor(CTextCursor *pCursor, float x, float y) = 0;
	virtual void SetCursorPosition(CTextCursor *pCursor, float x, float y) = 0;

	virtual CFont *LoadFont(const char *pFilename, const unsigned char *pBuf, size_t Size) = 0;
	virtual bool LoadFallbackFont(CFont *pFont, const char *pFilename, const unsigned char *pBuf, size_t Size) = 0;
	virtual CFont *GetFont(int FontIndex) = 0;
	virtual CFont *GetFont(const char *pFilename) = 0;

	virtual void SetDefaultFont(CFont *pFont) = 0;
	virtual void SetCurFont(CFont *pFont) = 0;

	virtual void SetRenderFlags(unsigned int Flags) = 0;
	virtual unsigned int GetRenderFlags() = 0;

	ColorRGBA DefaultTextColor() { return ColorRGBA(1, 1, 1, 1); }
	ColorRGBA DefaultTextOutlineColor() { return ColorRGBA(0, 0, 0, 0.3f); }

	//
	virtual void TextEx(CTextCursor *pCursor, const char *pText, int Length) = 0;
	virtual int CreateTextContainer(CTextCursor *pCursor, const char *pText, int Length = -1) = 0;
	virtual void AppendTextContainer(CTextCursor *pCursor, int TextContainerIndex, const char *pText, int Length = -1) = 0;
	// just deletes and creates text container
	virtual void RecreateTextContainer(CTextCursor *pCursor, int TextContainerIndex, const char *pText, int Length = -1) = 0;
	virtual void RecreateTextContainerSoft(CTextCursor *pCursor, int TextContainerIndex, const char *pText, int Length = -1) = 0;
	virtual void SetTextContainerSelection(int TextContainerIndex, const char *pText, int CursorPos, int SelectionStart, int SelectionEnd) = 0;
	virtual void DeleteTextContainer(int TextContainerIndex) = 0;

	virtual void UploadTextContainer(int TextContainerIndex) = 0;

	virtual void RenderTextContainer(int TextContainerIndex, STextRenderColor *pTextColor, STextRenderColor *pTextOutlineColor) = 0;
	virtual void RenderTextContainer(int TextContainerIndex, STextRenderColor *pTextColor, STextRenderColor *pTextOutlineColor, float X, float Y) = 0;

	virtual void UploadEntityLayerText(void *pTexBuff, int ImageColorChannelCount, int TexWidth, int TexHeight, int TexSubWidth, int TexSubHeight, const char *pText, int Length, float x, float y, int FontHeight) = 0;
	virtual int AdjustFontSize(const char *pText, int TextLength, int MaxSize, int MaxWidth) = 0;
	virtual int CalculateTextWidth(const char *pText, int TextLength, int FontWidth, int FontHeight) = 0;

	// old foolish interface
	virtual void TextColor(float r, float g, float b, float a) = 0;
	virtual void TextColor(ColorRGBA rgb) = 0;
	virtual void TextOutlineColor(float r, float g, float b, float a) = 0;
	virtual void TextOutlineColor(ColorRGBA rgb) = 0;
	virtual void Text(void *pFontSetV, float x, float y, float Size, const char *pText, float LineWidth) = 0;
	virtual float TextWidth(void *pFontSetV, float Size, const char *pText, int StrLength, float LineWidth, float *pAlignedHeight = NULL, float *pMaxCharacterHeightInLine = NULL) = 0;
	virtual int TextLineCount(void *pFontSetV, float Size, const char *pText, float LineWidth) = 0;

	virtual ColorRGBA GetTextColor() = 0;
	virtual ColorRGBA GetTextOutlineColor() = 0;

	virtual void OnWindowResize() = 0;

	virtual float GetGlyphOffsetX(int FontSize, char TextCharacter) = 0;
};

class IEngineTextRender : public ITextRender
{
	MACRO_INTERFACE("enginetextrender", 0)
public:
	virtual void Init() = 0;
};

extern IEngineTextRender *CreateEngineTextRender();

#endif
