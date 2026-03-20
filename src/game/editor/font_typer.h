#ifndef GAME_EDITOR_FONT_TYPER_H
#define GAME_EDITOR_FONT_TYPER_H

#include "component.h"

#include <base/vmath.h>

#include <engine/graphics.h>

#include <game/client/ui.h>

#include <chrono>
#include <memory>

class CLayer;
class CLayerTiles;

class CFontTyper : public CEditorComponent
{
public:
	class CState
	{
	public:
		bool m_Active;
		ivec2 m_TextIndex;
		int m_TextLineLen;
		std::shared_ptr<CLayer> m_pLastLayer;
		int m_TilesPlacedSinceActivate;

		void Reset();
	};

	void OnInit(CEditor *pEditor) override;
	bool OnInput(const IInput::CEvent &Event) override;
	void Render();

	bool IsActive() const;

private:
	enum
	{
		LETTER_OFFSET = 1,
		NUMBER_OFFSET = 54,
	};
	std::chrono::nanoseconds m_CursorRenderTime;
	IGraphics::CTextureHandle m_CursorTextTexture;
	CUi::SConfirmPopupContext m_ConfirmActivatePopupContext;

	void SetCursor();
	void TextModeOff();
	void TextModeOn();
	void SetTile(ivec2 Pos, unsigned char Index, const std::shared_ptr<CLayerTiles> &pLayer);
};

#endif
