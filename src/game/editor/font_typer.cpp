#include <base/log.h>
#include <base/system.h>
#include <engine/keys.h>
#include <game/editor/editor_actions.h>

#include <algorithm>

#include "editor.h"
#include "font_typer.h"

using namespace std::chrono_literals;

void CFontTyper::OnInit(CEditor *pEditor)
{
	CEditorComponent::OnInit(pEditor);

	m_CursorTextTexture = pEditor->Graphics()->LoadTexture("editor/cursor_text.png", IStorage::TYPE_ALL, 0);
}

void CFontTyper::SetTile(ivec2 Pos, unsigned char Index, const std::shared_ptr<CLayerTiles> &pLayer)
{
	CTile Tile = {
		Index, // index
		0, // flags
		0, // skip
		0, // reserved
	};
	pLayer->SetTile(Pos.x, Pos.y, Tile);
	m_TilesPlacedSinceActivate++;
}

bool CFontTyper::OnInput(const IInput::CEvent &Event)
{
	std::shared_ptr<CLayerTiles> pLayer = std::static_pointer_cast<CLayerTiles>(Editor()->GetSelectedLayerType(0, LAYERTYPE_TILES));
	if(!pLayer)
	{
		if(IsActive())
			TextModeOff();
		return false;
	}
	if(pLayer->m_Image == -1)
		return false;

	if(!IsActive())
	{
		if(Event.m_Key == KEY_T && Input()->ModifierIsPressed() && !Ui()->IsPopupOpen() && Editor()->m_Dialog == DIALOG_NONE)
		{
			if(pLayer && pLayer->m_KnownTextModeLayer)
			{
				TextModeOn();
			}
			else
			{
				m_ConfirmActivatePopupContext.Reset();
				m_ConfirmActivatePopupContext.YesNoButtons();
				str_copy(m_ConfirmActivatePopupContext.m_aMessage, "Enable text mode? Pressing letters and numbers on your keyboard will place tiles.");
				Ui()->ShowPopupConfirm(Ui()->MouseX(), Ui()->MouseY(), &m_ConfirmActivatePopupContext);
			}
		}
		return false;
	}

	// only handle key down and not also key up
	if(!(Event.m_Flags & IInput::FLAG_PRESS))
		return false;

	// letters
	if(Event.m_Key >= KEY_A && Event.m_Key <= KEY_Z)
	{
		SetTile(m_TextIndex, Event.m_Key - KEY_A + LETTER_OFFSET, pLayer);
		m_TextIndex.x++;
		m_TextLineLen++;
	}
	// numbers
	if(Event.m_Key >= KEY_1 && Event.m_Key <= KEY_0)
	{
		SetTile(m_TextIndex, Event.m_Key - KEY_1 + NUMBER_OFFSET, pLayer);
		m_TextIndex.x++;
		m_TextLineLen++;
	}
	if(Event.m_Key >= KEY_KP_1 && Event.m_Key <= KEY_KP_0)
	{
		SetTile(m_TextIndex, Event.m_Key - KEY_KP_1 + NUMBER_OFFSET, pLayer);
		m_TextIndex.x++;
		m_TextLineLen++;
	}

	// deletion
	if(Event.m_Key == KEY_BACKSPACE)
	{
		m_TextIndex.x--;
		m_TextLineLen--;
		SetTile(m_TextIndex, 0, pLayer);
	}
	// space
	if(Event.m_Key == KEY_SPACE)
	{
		SetTile(m_TextIndex, 0, pLayer);
		m_TextIndex.x++;
		m_TextLineLen++;
	}
	// newline
	if(Event.m_Key == KEY_RETURN)
	{
		m_TextIndex.y++;
		m_TextIndex.x -= m_TextLineLen;
		m_TextLineLen = 0;
	}
	// arrow key navigation
	if(Event.m_Key == KEY_LEFT)
	{
		m_TextIndex.x--;
		m_TextLineLen--;
		if(Editor()->Input()->KeyIsPressed(KEY_LCTRL))
		{
			while(pLayer->GetTile(m_TextIndex.x, m_TextIndex.y).m_Index)
			{
				if(m_TextIndex.x < 1 || m_TextIndex.x > pLayer->m_Width - 2)
					break;
				m_TextIndex.x--;
				m_TextLineLen--;
			}
		}
	}
	if(Event.m_Key == KEY_RIGHT)
	{
		m_TextIndex.x++;
		m_TextLineLen++;
		if(Editor()->Input()->KeyIsPressed(KEY_LCTRL))
		{
			while(pLayer->GetTile(m_TextIndex.x, m_TextIndex.y).m_Index)
			{
				if(m_TextIndex.x < 1 || m_TextIndex.x > pLayer->m_Width - 2)
					break;
				m_TextIndex.x++;
				m_TextLineLen++;
			}
		}
	}
	if(Event.m_Key == KEY_UP)
		m_TextIndex.y--;
	if(Event.m_Key == KEY_DOWN)
		m_TextIndex.y++;
	m_TextIndex.x = std::clamp(m_TextIndex.x, 0, pLayer->m_Width - 1);
	m_TextIndex.y = std::clamp(m_TextIndex.y, 0, pLayer->m_Height - 1);
	m_CursorRenderTime = time_get_nanoseconds() - 501ms;
	float Dist = distance(
		vec2(m_TextIndex.x, m_TextIndex.y),
		(Editor()->MapView()->GetWorldOffset() + Editor()->MapView()->GetEditorOffset()) / 32);
	Dist /= Editor()->MapView()->GetWorldZoom();
	if(Dist > 10.0f)
	{
		Editor()->MapView()->SetWorldOffset(vec2(m_TextIndex.x, m_TextIndex.y) * 32 - Editor()->MapView()->GetEditorOffset());
	}

	return false;
}

void CFontTyper::TextModeOn()
{
	std::shared_ptr<CLayerTiles> pLayer = std::static_pointer_cast<CLayerTiles>(Editor()->GetSelectedLayerType(0, LAYERTYPE_TILES));
	if(!pLayer)
		return;
	if(pLayer->m_Image == -1)
		return;

	SetCursor();
	m_TilesPlacedSinceActivate = 0;
	m_Active = true;
	pLayer->m_KnownTextModeLayer = true;

	// hack to not show picker when pressing space
	Editor()->m_Dialog = DIALOG_PSEUDO_FONT_TYPER;
}

void CFontTyper::TextModeOff()
{
	if(Editor()->m_Dialog == DIALOG_PSEUDO_FONT_TYPER)
		Editor()->m_Dialog = DIALOG_NONE;
	if(m_TilesPlacedSinceActivate)
		Editor()->m_EditorHistory.RecordAction(std::make_shared<CEditorBrushDrawAction>(Editor(), Editor()->m_SelectedGroup), "Font typer");
	m_TilesPlacedSinceActivate = 0;
	m_Active = false;
	m_pLastLayer = nullptr;
}

void CFontTyper::SetCursor()
{
	m_TextIndex.x = (int)(Ui()->MouseWorldX() / 32);
	m_TextIndex.y = (int)(Ui()->MouseWorldY() / 32);
	m_TextLineLen = 0;
	m_CursorRenderTime = time_get_nanoseconds() - 501ms;
}

void CFontTyper::OnRender(CUIRect View)
{
	if(m_ConfirmActivatePopupContext.m_Result == CUi::SConfirmPopupContext::CONFIRMED)
	{
		TextModeOn();
		m_ConfirmActivatePopupContext.Reset();
	}
	if(m_ConfirmActivatePopupContext.m_Result != CUi::SConfirmPopupContext::UNSET)
		m_ConfirmActivatePopupContext.Reset();

	if(!IsActive())
		return;

	if(Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
		TextModeOff();
	str_copy(Editor()->m_aTooltip, "Type on your keyboard to insert letters and numbers. Press Escape to end text mode.");

	std::shared_ptr<CLayerTiles> pLayer = std::static_pointer_cast<CLayerTiles>(Editor()->GetSelectedLayerType(0, LAYERTYPE_TILES));
	if(!pLayer)
		return;

	// exit if selected layer changes
	if(m_pLastLayer && m_pLastLayer != pLayer)
	{
		TextModeOff();
		m_pLastLayer = pLayer;
		return;
	}
	// exit if dialog or edit box pops up
	if(Editor()->m_Dialog != DIALOG_PSEUDO_FONT_TYPER || CLineInput::GetActiveInput())
	{
		TextModeOff();
		return;
	}
	m_pLastLayer = pLayer;
	m_TextIndex.x = std::clamp(m_TextIndex.x, 0, pLayer->m_Width - 1);
	m_TextIndex.y = std::clamp(m_TextIndex.y, 0, pLayer->m_Height - 1);

	const auto CurTime = time_get_nanoseconds();
	if((CurTime - m_CursorRenderTime) > 1s)
		m_CursorRenderTime = time_get_nanoseconds();
	if((CurTime - m_CursorRenderTime) > 500ms)
	{
		std::shared_ptr<CLayerGroup> pGroup = Editor()->GetSelectedGroup();
		pGroup->MapScreen();
		Editor()->Graphics()->WrapClamp();
		Editor()->Graphics()->TextureSet(m_CursorTextTexture);
		Editor()->Graphics()->QuadsBegin();
		Editor()->Graphics()->SetColor(1, 1, 1, 1);
		IGraphics::CQuadItem QuadItem(m_TextIndex.x * 32, m_TextIndex.y * 32, 32.0f, 32.0f);
		Editor()->Graphics()->QuadsDrawTL(&QuadItem, 1);
		Editor()->Graphics()->QuadsEnd();
		Editor()->Graphics()->WrapNormal();
		Editor()->Ui()->MapScreen();
	}
}
