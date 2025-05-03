#ifndef GAME_EDITOR_FONT_TYPER_H
#define GAME_EDITOR_FONT_TYPER_H

#include <base/vmath.h>
#include <engine/graphics.h>
#include <game/editor/mapitems/layer_tiles.h>

#include <chrono>
#include <memory>

#include "component.h"

class CFontTyper : public CEditorComponent
{
	enum
	{
		LETTER_OFFSET = 1,
		NUMBER_OFFSET = 54,
	};
	ivec2 m_TextIndex = ivec2(0, 0);
	int m_TextLineLen = 0;
	bool m_Active = false;
	std::chrono::nanoseconds m_CursorRenderTime;
	IGraphics::CTextureHandle m_CursorTextTexture;
	std::shared_ptr<class CLayer> m_pLastLayer;

	void SetCursor();
	void TextModeOff();
	void TextModeOn();
	void SetTile(ivec2 Pos, unsigned char Index, const std::shared_ptr<CLayerTiles> &pLayer);

public:
	void OnRender(CUIRect View) override;
	bool OnInput(const IInput::CEvent &Event) override;
	void OnInit(CEditor *pEditor) override;

	bool IsActive() const { return m_Active; }
};

#endif
