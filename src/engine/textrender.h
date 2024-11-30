/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_TEXTRENDER_H
#define ENGINE_TEXTRENDER_H
#include "kernel.h"

#include <base/color.h>

#include <engine/graphics.h>

#include <cstdint>
#include <memory>

enum
{
	TEXTFLAG_RENDER = 1 << 0,
	TEXTFLAG_DISALLOW_NEWLINE = 1 << 1,
	TEXTFLAG_STOP_AT_END = 1 << 2,
	TEXTFLAG_ELLIPSIS_AT_END = 1 << 3,
};

enum ETextAlignment
{
	TEXTALIGN_LEFT = 0,
	TEXTALIGN_CENTER = 1 << 1,
	TEXTALIGN_RIGHT = 1 << 2,
	TEXTALIGN_TOP = 0, // this is also 0, so the default alignment is top-left
	TEXTALIGN_MIDDLE = 1 << 3,
	TEXTALIGN_BOTTOM = 1 << 4,

	TEXTALIGN_TL = TEXTALIGN_TOP | TEXTALIGN_LEFT,
	TEXTALIGN_TC = TEXTALIGN_TOP | TEXTALIGN_CENTER,
	TEXTALIGN_TR = TEXTALIGN_TOP | TEXTALIGN_RIGHT,
	TEXTALIGN_ML = TEXTALIGN_MIDDLE | TEXTALIGN_LEFT,
	TEXTALIGN_MC = TEXTALIGN_MIDDLE | TEXTALIGN_CENTER,
	TEXTALIGN_MR = TEXTALIGN_MIDDLE | TEXTALIGN_RIGHT,
	TEXTALIGN_BL = TEXTALIGN_BOTTOM | TEXTALIGN_LEFT,
	TEXTALIGN_BC = TEXTALIGN_BOTTOM | TEXTALIGN_CENTER,
	TEXTALIGN_BR = TEXTALIGN_BOTTOM | TEXTALIGN_RIGHT,

	TEXTALIGN_MASK_HORIZONTAL = TEXTALIGN_LEFT | TEXTALIGN_CENTER | TEXTALIGN_RIGHT,
	TEXTALIGN_MASK_VERTICAL = TEXTALIGN_TOP | TEXTALIGN_MIDDLE | TEXTALIGN_BOTTOM,
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
	// text is only rendered once and then discarded (a hint for buffer creation)
	TEXT_RENDER_FLAG_ONE_TIME_USE = 1 << 9,
};

enum class EFontPreset
{
	DEFAULT_FONT,
	ICON_FONT,
};

namespace FontIcons {
// Each font icon is named according to its official name in Font Awesome
MAYBE_UNUSED static const char *FONT_ICON_PLUS = "+";
MAYBE_UNUSED static const char *FONT_ICON_MINUS = "-";
MAYBE_UNUSED static const char *FONT_ICON_LOCK = "\xEF\x80\xA3";
MAYBE_UNUSED static const char *FONT_ICON_MAGNIFYING_GLASS = "\xEF\x80\x82";
MAYBE_UNUSED static const char *FONT_ICON_HEART = "\xEF\x80\x84";
MAYBE_UNUSED static const char *FONT_ICON_STAR = "\xEF\x80\x85";
MAYBE_UNUSED static const char *FONT_ICON_XMARK = "\xEF\x80\x8D";
MAYBE_UNUSED static const char *FONT_ICON_CIRCLE = "\xEF\x84\x91";
MAYBE_UNUSED static const char *FONT_ICON_ARROW_ROTATE_LEFT = "\xEF\x83\xA2";
MAYBE_UNUSED static const char *FONT_ICON_ARROW_ROTATE_RIGHT = "\xEF\x80\x9E";
MAYBE_UNUSED static const char *FONT_ICON_FLAG_CHECKERED = "\xEF\x84\x9E";
MAYBE_UNUSED static const char *FONT_ICON_BAN = "\xEF\x81\x9E";
MAYBE_UNUSED static const char *FONT_ICON_CIRCLE_CHEVRON_DOWN = "\xEF\x84\xBA";
MAYBE_UNUSED static const char *FONT_ICON_SQUARE_MINUS = "\xEF\x85\x86";
MAYBE_UNUSED static const char *FONT_ICON_SQUARE_PLUS = "\xEF\x83\xBE";
MAYBE_UNUSED static const char *FONT_ICON_SORT_UP = "\xEF\x83\x9E";
MAYBE_UNUSED static const char *FONT_ICON_SORT_DOWN = "\xEF\x83\x9D";

MAYBE_UNUSED static const char *FONT_ICON_HOUSE = "\xEF\x80\x95";
MAYBE_UNUSED static const char *FONT_ICON_NEWSPAPER = "\xEF\x87\xAA";
MAYBE_UNUSED static const char *FONT_ICON_POWER_OFF = "\xEF\x80\x91";
MAYBE_UNUSED static const char *FONT_ICON_GEAR = "\xEF\x80\x93";
MAYBE_UNUSED static const char *FONT_ICON_PEN_TO_SQUARE = "\xEF\x81\x84";
MAYBE_UNUSED static const char *FONT_ICON_CLAPPERBOARD = "\xEE\x84\xB1";
MAYBE_UNUSED static const char *FONT_ICON_EARTH_AMERICAS = "\xEF\x95\xBD";
MAYBE_UNUSED static const char *FONT_ICON_NETWORK_WIRED = "\xEF\x9B\xBF";
MAYBE_UNUSED static const char *FONT_ICON_LIST_UL = "\xEF\x83\x8A";
MAYBE_UNUSED static const char *FONT_ICON_INFO = "\xEF\x84\xA9";
MAYBE_UNUSED static const char *FONT_ICON_TERMINAL = "\xEF\x84\xA0";

MAYBE_UNUSED static const char *FONT_ICON_SLASH = "\xEF\x9C\x95";
MAYBE_UNUSED static const char *FONT_ICON_PLAY = "\xEF\x81\x8B";
MAYBE_UNUSED static const char *FONT_ICON_PAUSE = "\xEF\x81\x8C";
MAYBE_UNUSED static const char *FONT_ICON_STOP = "\xEF\x81\x8D";
MAYBE_UNUSED static const char *FONT_ICON_CHEVRON_LEFT = "\xEF\x81\x93";
MAYBE_UNUSED static const char *FONT_ICON_CHEVRON_RIGHT = "\xEF\x81\x94";
MAYBE_UNUSED static const char *FONT_ICON_CHEVRON_UP = "\xEF\x81\xB7";
MAYBE_UNUSED static const char *FONT_ICON_CHEVRON_DOWN = "\xEF\x81\xB8";
MAYBE_UNUSED static const char *FONT_ICON_BACKWARD = "\xEF\x81\x8A";
MAYBE_UNUSED static const char *FONT_ICON_FORWARD = "\xEF\x81\x8E";
MAYBE_UNUSED static const char *FONT_ICON_RIGHT_FROM_BRACKET = "\xEF\x8B\xB5";
MAYBE_UNUSED static const char *FONT_ICON_RIGHT_TO_BRACKET = "\xEF\x8B\xB6";
MAYBE_UNUSED static const char *FONT_ICON_ARROW_UP_RIGHT_FROM_SQUARE = "\xEF\x82\x8E";
MAYBE_UNUSED static const char *FONT_ICON_BACKWARD_STEP = "\xEF\x81\x88";
MAYBE_UNUSED static const char *FONT_ICON_FORWARD_STEP = "\xEF\x81\x91";
MAYBE_UNUSED static const char *FONT_ICON_BACKWARD_FAST = "\xEF\x81\x89";
MAYBE_UNUSED static const char *FONT_ICON_FORWARD_FAST = "\xEF\x81\x90";
MAYBE_UNUSED static const char *FONT_ICON_KEYBOARD = "\xE2\x8C\xA8";
MAYBE_UNUSED static const char *FONT_ICON_ELLIPSIS = "\xEF\x85\x81";

MAYBE_UNUSED static const char *FONT_ICON_FOLDER = "\xEF\x81\xBB";
MAYBE_UNUSED static const char *FONT_ICON_FOLDER_OPEN = "\xEF\x81\xBC";
MAYBE_UNUSED static const char *FONT_ICON_FOLDER_TREE = "\xEF\xA0\x82";
MAYBE_UNUSED static const char *FONT_ICON_FILM = "\xEF\x80\x88";
MAYBE_UNUSED static const char *FONT_ICON_VIDEO = "\xEF\x80\xBD";
MAYBE_UNUSED static const char *FONT_ICON_MAP = "\xEF\x89\xB9";
MAYBE_UNUSED static const char *FONT_ICON_IMAGE = "\xEF\x80\xBE";
MAYBE_UNUSED static const char *FONT_ICON_MUSIC = "\xEF\x80\x81";
MAYBE_UNUSED static const char *FONT_ICON_FILE = "\xEF\x85\x9B";

MAYBE_UNUSED static const char *FONT_ICON_PENCIL = "\xEF\x8C\x83";
MAYBE_UNUSED static const char *FONT_ICON_TRASH = "\xEF\x87\xB8";

MAYBE_UNUSED static const char *FONT_ICON_ARROWS_LEFT_RIGHT = "\xEF\x8C\xB7";
MAYBE_UNUSED static const char *FONT_ICON_ARROWS_UP_DOWN = "\xEF\x81\xBD";
MAYBE_UNUSED static const char *FONT_ICON_CIRCLE_PLAY = "\xEF\x85\x84";
MAYBE_UNUSED static const char *FONT_ICON_BORDER_ALL = "\xEF\xA1\x8C";
MAYBE_UNUSED static const char *FONT_ICON_EYE = "\xEF\x81\xAE";
MAYBE_UNUSED static const char *FONT_ICON_EYE_SLASH = "\xEF\x81\xB0";
MAYBE_UNUSED static const char *FONT_ICON_EYE_DROPPER = "\xEF\x87\xBB";

MAYBE_UNUSED static const char *FONT_ICON_DICE_ONE = "\xEF\x94\xA5";
MAYBE_UNUSED static const char *FONT_ICON_DICE_TWO = "\xEF\x94\xA8";
MAYBE_UNUSED static const char *FONT_ICON_DICE_THREE = "\xEF\x94\xA7";
MAYBE_UNUSED static const char *FONT_ICON_DICE_FOUR = "\xEF\x94\xA4";
MAYBE_UNUSED static const char *FONT_ICON_DICE_FIVE = "\xEF\x94\xA3";
MAYBE_UNUSED static const char *FONT_ICON_DICE_SIX = "\xEF\x94\xA6";

MAYBE_UNUSED static const char *FONT_ICON_LAYER_GROUP = "\xEF\x97\xBD";
MAYBE_UNUSED static const char *FONT_ICON_UNDO = "\xEF\x8B\xAA";
MAYBE_UNUSED static const char *FONT_ICON_REDO = "\xEF\x8B\xB9";

MAYBE_UNUSED static const char *FONT_ICON_ARROWS_ROTATE = "\xEF\x80\xA1";
MAYBE_UNUSED static const char *FONT_ICON_QUESTION = "?";

MAYBE_UNUSED static const char *FONT_ICON_CAMERA = "\xEF\x80\xB0";
} // end namespace FontIcons

enum ETextCursorSelectionMode
{
	// ignore any kind of selection
	TEXT_CURSOR_SELECTION_MODE_NONE = 0,
	// calculates the selection based on the mouse press and release cursor position
	TEXT_CURSOR_SELECTION_MODE_CALCULATE,
	// sets the selection based on the character start and end count(these values have to be decoded character offsets)
	TEXT_CURSOR_SELECTION_MODE_SET,
};

enum ETextCursorCursorMode
{
	// ignore any kind of cursor
	TEXT_CURSOR_CURSOR_MODE_NONE = 0,
	// calculates the cursor based on the mouse release cursor position
	TEXT_CURSOR_CURSOR_MODE_CALCULATE,
	// sets the cursor based on the current character (this value has to be decoded character offset)
	TEXT_CURSOR_CURSOR_MODE_SET,
};

struct STextBoundingBox
{
	float m_X;
	float m_Y;
	float m_W;
	float m_H;

	float Right() const { return m_X + m_W; }
	float Bottom() const { return m_Y + m_H; }
	vec2 Size() const { return vec2(m_W, m_H); }
	void MoveBy(vec2 Offset)
	{
		m_X += Offset.x;
		m_Y += Offset.y;
	}
};

// Allow to render multi colored text in one go without having to call TextEx() multiple times.
// Needed to allow multi colored multi line texts
struct STextColorSplit
{
	int m_CharIndex; // Which index within the text should the split occur
	int m_Length; // How long is the split
	ColorRGBA m_Color; // The color the text should be starting from m_CharIndex

	STextColorSplit(int CharIndex, int Length, const ColorRGBA &Color) :
		m_CharIndex(CharIndex), m_Length(Length), m_Color(Color) {}
};

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

	float m_FontSize;
	float m_AlignedFontSize;
	float m_LineSpacing;
	float m_AlignedLineSpacing;

	ETextCursorSelectionMode m_CalculateSelectionMode;
	float m_SelectionHeightFactor;

	// these coordinates are respected if selection mode is set to calculate @see ETextCursorSelectionMode
	vec2 m_PressMouse;
	// these coordinates are respected if selection/cursor mode is set to calculate @see ETextCursorSelectionMode / @see ETextCursorCursorMode
	vec2 m_ReleaseMouse;

	// note m_SelectionStart can be bigger than m_SelectionEnd, depending on how the mouse cursor was dragged
	// also note, that these are the character offsets decoded
	int m_SelectionStart;
	int m_SelectionEnd;

	ETextCursorCursorMode m_CursorMode;
	bool m_ForceCursorRendering;
	// note this is the decoded character offset
	int m_CursorCharacter;
	vec2 m_CursorRenderedPosition;

	// Color splits of the cursor to allow multicolored text
	std::vector<STextColorSplit> m_vColorSplits;

	float Height() const
	{
		return m_LineCount * (m_AlignedFontSize + m_AlignedLineSpacing);
	}

	STextBoundingBox BoundingBox() const
	{
		return {m_StartX, m_StartY, m_LongestLineWidth, Height()};
	}

	void Reset()
	{
		m_Flags = 0;
		m_LineCount = 0;
		m_GlyphCount = 0;
		m_CharCount = 0;
		m_MaxLines = 0;
		m_StartX = 0;
		m_StartY = 0;
		m_LineWidth = 0;
		m_X = 0;
		m_Y = 0;
		m_MaxCharacterHeight = 0;
		m_LongestLineWidth = 0;
		m_FontSize = 0;
		m_AlignedFontSize = 0;
		m_LineSpacing = 0;
		m_CalculateSelectionMode = TEXT_CURSOR_SELECTION_MODE_NONE;
		m_SelectionHeightFactor = 0;
		m_PressMouse = vec2();
		m_ReleaseMouse = vec2();
		m_SelectionStart = 0;
		m_SelectionEnd = 0;
		m_CursorMode = TEXT_CURSOR_CURSOR_MODE_NONE;
		m_ForceCursorRendering = false;
		m_CursorCharacter = 0;
		m_CursorRenderedPosition = vec2();
		m_vColorSplits.clear();
	}
};

struct STextContainerUsages
{
	int m_Dummy = 0;
};

struct STextContainerIndex
{
	int m_Index;
	std::shared_ptr<STextContainerUsages> m_UseCount =
		std::make_shared<STextContainerUsages>(STextContainerUsages());

	STextContainerIndex() { Reset(); }
	bool Valid() const { return m_Index >= 0; }
	void Reset() { m_Index = -1; }
};

struct STextSizeProperties
{
	float *m_pHeight = nullptr;
	float *m_pAlignedFontSize = nullptr;
	float *m_pMaxCharacterHeightInLine = nullptr;
	int *m_pLineCount = nullptr;
};

class ITextRender : public IInterface
{
	MACRO_INTERFACE("textrender")
public:
	virtual void SetCursor(CTextCursor *pCursor, float x, float y, float FontSize, int Flags) const = 0;
	virtual void MoveCursor(CTextCursor *pCursor, float x, float y) const = 0;
	virtual void SetCursorPosition(CTextCursor *pCursor, float x, float y) const = 0;

	virtual bool LoadFonts() = 0;
	virtual void SetFontPreset(EFontPreset FontPreset) = 0;
	virtual void SetFontLanguageVariant(const char *pLanguageFile) = 0;

	virtual void SetRenderFlags(unsigned Flags) = 0;
	virtual unsigned GetRenderFlags() const = 0;

	ColorRGBA DefaultTextColor() const { return ColorRGBA(1, 1, 1, 1); }
	ColorRGBA DefaultTextOutlineColor() const { return ColorRGBA(0, 0, 0, 0.3f); }
	ColorRGBA DefaultTextSelectionColor() const { return ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f); }

	//
	virtual void TextEx(CTextCursor *pCursor, const char *pText, int Length = -1) = 0;
	virtual bool CreateTextContainer(STextContainerIndex &TextContainerIndex, CTextCursor *pCursor, const char *pText, int Length = -1) = 0;
	virtual void AppendTextContainer(STextContainerIndex TextContainerIndex, CTextCursor *pCursor, const char *pText, int Length = -1) = 0;
	// either creates a new text container or appends to a existing one
	virtual bool CreateOrAppendTextContainer(STextContainerIndex &TextContainerIndex, CTextCursor *pCursor, const char *pText, int Length = -1) = 0;
	// just deletes and creates text container
	virtual void RecreateTextContainer(STextContainerIndex &TextContainerIndex, CTextCursor *pCursor, const char *pText, int Length = -1) = 0;
	virtual void RecreateTextContainerSoft(STextContainerIndex &TextContainerIndex, CTextCursor *pCursor, const char *pText, int Length = -1) = 0;
	virtual void DeleteTextContainer(STextContainerIndex &TextContainerIndex) = 0;

	virtual void UploadTextContainer(STextContainerIndex TextContainerIndex) = 0;

	virtual void RenderTextContainer(STextContainerIndex TextContainerIndex, const ColorRGBA &TextColor, const ColorRGBA &TextOutlineColor) = 0;
	virtual void RenderTextContainer(STextContainerIndex TextContainerIndex, const ColorRGBA &TextColor, const ColorRGBA &TextOutlineColor, float X, float Y) = 0;

	virtual STextBoundingBox GetBoundingBoxTextContainer(STextContainerIndex TextContainerIndex) = 0;

	virtual void UploadEntityLayerText(const CImageInfo &TextImage, int TexSubWidth, int TexSubHeight, const char *pText, int Length, float x, float y, int FontSize) = 0;
	virtual int AdjustFontSize(const char *pText, int TextLength, int MaxSize, int MaxWidth) const = 0;
	virtual float GetGlyphOffsetX(int FontSize, char TextCharacter) const = 0;
	virtual int CalculateTextWidth(const char *pText, int TextLength, int FontWidth, int FontSize) const = 0;

	// old foolish interface
	virtual void TextColor(float r, float g, float b, float a) = 0;
	virtual void TextColor(ColorRGBA Color) = 0;
	virtual void TextOutlineColor(float r, float g, float b, float a) = 0;
	virtual void TextOutlineColor(ColorRGBA Color) = 0;
	virtual void TextSelectionColor(float r, float g, float b, float a) = 0;
	virtual void TextSelectionColor(ColorRGBA Color) = 0;
	virtual void Text(float x, float y, float Size, const char *pText, float LineWidth = -1.0f) = 0;
	virtual float TextWidth(float Size, const char *pText, int StrLength = -1, float LineWidth = -1.0f, int Flags = 0, const STextSizeProperties &TextSizeProps = {}) = 0;
	virtual STextBoundingBox TextBoundingBox(float Size, const char *pText, int StrLength = -1, float LineWidth = -1.0f, float LineSpacing = 0.0f, int Flags = 0) = 0;

	virtual ColorRGBA GetTextColor() const = 0;
	virtual ColorRGBA GetTextOutlineColor() const = 0;
	virtual ColorRGBA GetTextSelectionColor() const = 0;

	virtual void OnPreWindowResize() = 0;
	virtual void OnWindowResize() = 0;
};

class IEngineTextRender : public ITextRender
{
	MACRO_INTERFACE("enginetextrender")
public:
	virtual void Init() = 0;
	virtual void Shutdown() override = 0;
};

extern IEngineTextRender *CreateEngineTextRender();

#endif
