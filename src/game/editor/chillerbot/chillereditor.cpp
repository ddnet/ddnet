#include <engine/console.h>
#include <engine/input.h>
#include <engine/keys.h>

#include <engine/external/json-parser/json.h>

#include <game/editor/editor.h>

#include "chillereditor.h"

CChillerEditor::CChillerEditor()
{
	m_EditorMode = CE_MODE_NONE;
	m_TextIndexX = -1;
	m_TextIndexY = -1;
	m_TextLineLen = 0;
	m_NextCursorBlink = 0;
	m_pEditor = 0;
	m_LetterOffset = 0;
	m_NumberOffset = 53;
	m_pLastLayer = 0;
}

void CChillerEditor::LoadMapresMetaFile(const char *pImage)
{
	char aFilepath[1024];
	str_format(aFilepath, sizeof(aFilepath), "mapres/%s.json", pImage);
	// read file data into buffer
	IOHANDLE File = m_pEditor->Storage()->OpenFile(aFilepath, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
		return;
	int FileSize = (int)io_length(File);
	char *pFileData = (char *)malloc(FileSize);
	io_read(File, pFileData, FileSize);
	io_close(File);

	// parse json data
	json_settings JsonSettings;
	mem_zero(&JsonSettings, sizeof(JsonSettings));
	char aError[256];
	json_value *pJsonData = json_parse_ex(&JsonSettings, pFileData, FileSize, aError);
	free(pFileData);

	if(pJsonData == 0)
	{
		m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, aFilepath, aError);
		return;
	}

	dbg_msg("chillerbot", "loading mapres meta data for %s", pImage);
	if((*pJsonData)["type"].type == json_string)
		dbg_msg("chillerbot", "type=%s", (const char *)(*pJsonData)["type"]);
	if((*pJsonData)["offset_alphabet"].type == json_integer)
	{
		dbg_msg("chillerbot", "alphabet=%ld", (long int)(*pJsonData)["offset_alphabet"].u.integer);
		m_LetterOffset = (*pJsonData)["offset_alphabet"].u.integer - 1;
	}
	if((*pJsonData)["offset_numbers"].type == json_integer)
	{
		dbg_msg("chillerbot", "numbers=%ld", (long int)(*pJsonData)["offset_numbers"].u.integer);
		m_NumberOffset = (*pJsonData)["offset_numbers"].u.integer - 1;
	}

	// clean up
	json_value_free(pJsonData);
}

void CChillerEditor::Init(class CEditor *pEditor)
{
	m_pEditor = pEditor;
	m_CursorTextTexture = pEditor->Graphics()->LoadTexture("editor/cursor_text.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
}

void CChillerEditor::SetCursor()
{
	float wx = m_pEditor->UI()->MouseWorldX();
	float wy = m_pEditor->UI()->MouseWorldY();
	m_TextIndexX = (int)(wx / 32);
	m_TextIndexY = (int)(wy / 32);
	m_TextLineLen = 0;
}

void CChillerEditor::ExitTextMode()
{
	if(m_pEditor->m_Dialog == -1)
		m_pEditor->m_Dialog = DIALOG_NONE;
	m_EditorMode = CE_MODE_NONE;
}

void CChillerEditor::DoMapEditor()
{
	// debug
	if(m_pEditor->Input()->KeyPress(KEY_D))
		dbg_msg("chillerbot", "editormode=%d dialog=%d editbox=%d", m_EditorMode, m_pEditor->m_Dialog, m_pEditor->m_EditBoxActive);
	if(m_pEditor->UI()->MouseButton(0))
		SetCursor();
	if(m_pEditor->Input()->KeyPress(KEY_T) && m_EditorMode != CE_MODE_TEXT && m_pEditor->m_Dialog == DIALOG_NONE && m_pEditor->m_EditBoxActive == 0)
	{
		m_EditorMode = CE_MODE_TEXT;
		m_pEditor->m_Dialog = -1; // hack to not close editor when pressing Escape
		SetCursor();
		CLayerTiles *pLayer = (CLayerTiles *)m_pEditor->GetSelectedLayerType(0, LAYERTYPE_TILES);
		if(pLayer->m_Image >= 0 && pLayer->m_Image < (int)m_pEditor->m_Map.m_lImages.size())
			LoadMapresMetaFile(m_pEditor->m_Map.m_lImages[pLayer->m_Image]->m_aName);
	}
	if(m_EditorMode == CE_MODE_TEXT)
	{
		m_pEditor->m_pTooltip = "Type on your keyboard to insert letters. Press Escape to end text mode.";
		CLayerTiles *pLayer = (CLayerTiles *)m_pEditor->GetSelectedLayerType(0, LAYERTYPE_TILES);
		// exit if selected layer changes
		if(m_pLastLayer && m_pLastLayer != pLayer)
		{
			ExitTextMode();
			m_pLastLayer = pLayer;
			return;
		}
		// exit if dialog or edit box pops up
		if(m_pEditor->m_Dialog != -1 || m_pEditor->m_EditBoxActive)
		{
			ExitTextMode();
			return;
		}
		m_pLastLayer = pLayer;
		m_TextIndexX = clamp(m_TextIndexX, 0, pLayer->m_Width - 1);
		m_TextIndexY = clamp(m_TextIndexY, 0, pLayer->m_Height - 1);
		if(time_get() > m_NextCursorBlink)
		{
			m_NextCursorBlink = time_get() + time_freq() / 1.5;
			m_DrawCursor ^= true;
		}
		if(m_DrawCursor)
		{
			CLayerGroup *g = m_pEditor->GetSelectedGroup();
			g->MapScreen();
			m_pEditor->Graphics()->WrapClamp();
			m_pEditor->Graphics()->TextureSet(m_CursorTextTexture);
			m_pEditor->Graphics()->QuadsBegin();
			m_pEditor->Graphics()->SetColor(1, 1, 1, 1);
			IGraphics::CQuadItem QuadItem(m_TextIndexX * 32, m_TextIndexY * 32, 32.0f, 32.0f);
			m_pEditor->Graphics()->QuadsDrawTL(&QuadItem, 1);
			m_pEditor->Graphics()->QuadsEnd();
			m_pEditor->Graphics()->WrapNormal();
			m_pEditor->Graphics()->MapScreen(m_pEditor->UI()->Screen()->x, m_pEditor->UI()->Screen()->y, m_pEditor->UI()->Screen()->w, m_pEditor->UI()->Screen()->h);
		}
		// handle key presses
		IInput::CEvent e = m_pEditor->Input()->GetEvent(0);
		if(!m_pEditor->Input()->IsEventValid(&e))
			return;
		if(m_pEditor->Input()->KeyPress(e.m_Key))
			return;
		// escape -> end text mode
		if(e.m_Key == KEY_ESCAPE)
		{
			ExitTextMode();
			return;
		}
		// letters
		if(e.m_Key > 3 && e.m_Key < 30)
		{
			CTile Tile;
			Tile.m_Index = e.m_Key - 3 + m_LetterOffset;
			pLayer->SetTile(m_TextIndexX++, m_TextIndexY, Tile);
			m_TextLineLen++;
		}
		// numbers
		if(e.m_Key >= 30 && e.m_Key < 40)
		{
			CTile Tile;
			Tile.m_Index = e.m_Key - 29 + m_NumberOffset;
			pLayer->SetTile(m_TextIndexX++, m_TextIndexY, Tile);
			m_TextLineLen++;
		}
		// deletion
		if(e.m_Key == KEY_BACKSPACE)
		{
			CTile Tile;
			Tile.m_Index = 0;
			pLayer->SetTile(--m_TextIndexX, m_TextIndexY, Tile);
			m_TextLineLen--;
		}
		// space
		if(e.m_Key == KEY_SPACE)
		{
			CTile Tile;
			Tile.m_Index = 0;
			pLayer->SetTile(m_TextIndexX++, m_TextIndexY, Tile);
			m_TextLineLen++;
		}
		// newline
		if(e.m_Key == KEY_RETURN)
		{
			m_TextIndexY++;
			m_TextIndexX -= m_TextLineLen;
			m_TextLineLen = 0;
		}
		// arrow key navigation
		if(e.m_Key == KEY_LEFT)
		{
			m_TextIndexX--;
			m_TextLineLen--;
			if(m_pEditor->Input()->KeyIsPressed(KEY_LCTRL))
			{
				while(pLayer->GetTile(m_TextIndexX, m_TextIndexY).m_Index)
				{
					if(m_TextIndexX < 1 || m_TextIndexX > pLayer->m_Width - 2)
						break;
					if(e.m_Key != KEY_LEFT)
						break;
					m_TextIndexX--;
					m_TextLineLen--;
				}
			}
		}
		if(e.m_Key == KEY_RIGHT)
		{
			m_TextIndexX++;
			m_TextLineLen++;
			if(m_pEditor->Input()->KeyIsPressed(KEY_LCTRL))
			{
				while(pLayer->GetTile(m_TextIndexX, m_TextIndexY).m_Index)
				{
					if(m_TextIndexX < 1 || m_TextIndexX > pLayer->m_Width - 2)
						break;
					if(e.m_Key != KEY_RIGHT)
						break;
					m_TextIndexX++;
					m_TextLineLen++;
				}
			}
		}
		if(e.m_Key == KEY_UP)
			m_TextIndexY--;
		if(e.m_Key == KEY_DOWN)
			m_TextIndexY++;
		m_NextCursorBlink = time_get() + time_freq();
		m_DrawCursor = true;
		float dist = distance(
			vec2(m_TextIndexX, m_TextIndexY),
			vec2((m_pEditor->m_WorldOffsetX + m_pEditor->m_EditorOffsetX) / 32, (m_pEditor->m_WorldOffsetY + m_pEditor->m_EditorOffsetY) / 32));
		dist /= m_pEditor->m_WorldZoom;
		if(dist > 10.0f)
		{
			m_pEditor->m_WorldOffsetX = m_TextIndexX * 32 - m_pEditor->m_EditorOffsetX;
			m_pEditor->m_WorldOffsetY = m_TextIndexY * 32 - m_pEditor->m_EditorOffsetY;
		}
		dbg_msg("chillerbot", "key=%d dialog=%d", e.m_Key, m_pEditor->m_Dialog);
	}
}
