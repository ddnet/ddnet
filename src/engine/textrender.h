/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_TEXTRENDER_H
#define ENGINE_TEXTRENDER_H
#include "kernel.h"

enum
{
	TEXTFLAG_RENDER=1,
	TEXTFLAG_ALLOW_NEWLINE=2,
	TEXTFLAG_STOP_AT_END=4
};

enum ETextRenderFlags
{
	TEXT_RENDER_FLAG_NO_X_BEARING = 1<<0,
	TEXT_RENDER_FLAG_NO_Y_BEARING = 1<<1,
	TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH = 1<<2,
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
	int m_CharCount;
	int m_MaxLines;

	float m_StartX;
	float m_StartY;
	float m_LineWidth;
	float m_X, m_Y;

	CFont *m_pFont;
	float m_FontSize;
};

struct STextRenderColor
{
	STextRenderColor() {}
	STextRenderColor(float r, float g, float b, float a)
	{
		Set(r, g, b, a);
	}

	void Set(float r, float g, float b, float a)
	{
		m_R = r;
		m_G = g;
		m_B = b;
		m_A = a;
	}

	float m_R, m_G, m_B, m_A;
};

class ITextRender : public IInterface
{
	MACRO_INTERFACE("textrender", 0)
public:
	virtual void SetCursor(CTextCursor *pCursor, float x, float y, float FontSize, int Flags) = 0;

	virtual CFont *LoadFont(const char *pFilename) = 0;
	virtual CFont *GetFont(int FontIndex) = 0;
	virtual CFont *GetFont(const char *pFilename) = 0;
	virtual void DestroyFont(CFont *pFont) = 0;

	virtual void SetDefaultFont(CFont *pFont) = 0;
	virtual void SetCurFont(CFont *pFont) = 0;

	virtual void SetRenderFlags(unsigned int Flags) = 0;

	//
	virtual void TextEx(CTextCursor *pCursor, const char *pText, int Length) = 0;
	virtual int CreateTextContainer(CTextCursor *pCursor, const char *pText) = 0;
	virtual void AppendTextContainer(CTextCursor *pCursor, int TextContainerIndex, const char *pText) = 0;
	// just deletes and creates text container
	virtual void RecreateTextContainer(CTextCursor *pCursor, int TextContainerIndex, const char *pText) = 0;
	virtual void RecreateTextContainerSoft(CTextCursor *pCursor, int TextContainerIndex, const char *pText) = 0;
	virtual void SetTextContainerSelection(int TextContainerIndex, const char *pText, int CursorPos, int SelectionStart, int SelectionEnd) = 0;
	virtual void DeleteTextContainer(int TextContainerIndex) = 0;

	virtual void RenderTextContainer(int TextContainerIndex, STextRenderColor *pTextColor, STextRenderColor *pTextOutlineColor) = 0;
	virtual void RenderTextContainer(int TextContainerIndex, STextRenderColor *pTextColor, STextRenderColor *pTextOutlineColor, float X, float Y) = 0;

	virtual void UploadEntityLayerText(int TextureID, const char *pText, int Length, float x, float y, int Size, int MaxWidth, int MaxSize = -1, int MinSize = -1) = 0;

	// old foolish interface
	virtual void TextColor(float r, float g, float b, float a) = 0;
	virtual void TextOutlineColor(float r, float g, float b, float a) = 0;
	virtual void Text(void *pFontSetV, float x, float y, float Size, const char *pText, int MaxWidth) = 0;
	virtual float TextWidth(void *pFontSetV, float Size, const char *pText, int Length) = 0;
	virtual int TextLineCount(void *pFontSetV, float Size, const char *pText, float LineWidth) = 0;
};

class IEngineTextRender : public ITextRender
{
	MACRO_INTERFACE("enginetextrender", 0)
public:
	virtual void Init() = 0;
};

extern IEngineTextRender *CreateEngineTextRender();

#endif
