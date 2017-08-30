/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/tl/array.h>
#include <base/system.h>
#include <base/color.h>
#include <time.h>

#if defined(CONF_FAMILY_UNIX)
#include <pthread.h>
#endif

#include <engine/shared/datafile.h>
#include <engine/shared/config.h>
#include <engine/client.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/gamecore.h>
#include <game/localization.h>
#include <game/client/lineinput.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/generated/client_data.h>

#include "auto_map.h"
#include "editor.h"

const char* vanillaImages[] = {"bg_cloud1", "bg_cloud2", "bg_cloud3",
	"desert_doodads", "desert_main", "desert_mountains", "desert_mountains2",
	"desert_sun", "generic_deathtiles", "generic_unhookable", "grass_doodads",
	"grass_main", "jungle_background", "jungle_deathtiles", "jungle_doodads",
	"jungle_main", "jungle_midground", "jungle_unhookables", "moon", "mountains",
	"snow", "stars", "sun", "winter_doodads", "winter_main", "winter_mountains",
	"winter_mountains2", "winter_mountains3"};

int CEditor::ms_CheckerTexture;
int CEditor::ms_BackgroundTexture;
int CEditor::ms_CursorTexture;
int CEditor::ms_EntitiesTexture;
const void* CEditor::ms_pUiGotContext;

int CEditor::ms_FrontTexture;
int CEditor::ms_TeleTexture;
int CEditor::ms_SpeedupTexture;
int CEditor::ms_SwitchTexture;
int CEditor::ms_TuneTexture;

vec3 CEditor::ms_PickerColor;
int CEditor::ms_SVPicker;
int CEditor::ms_HuePicker;


enum
{
	BUTTON_CONTEXT=1,
};

CEditorImage::~CEditorImage()
{
	m_pEditor->Graphics()->UnloadTexture(m_TexID);
	if(m_pData)
	{
		mem_free(m_pData);
		m_pData = 0;
	}
}

CEditorSound::~CEditorSound()
{
	m_pEditor->Sound()->UnloadSample(m_SoundID);
	if(m_pData)
	{
		mem_free(m_pData);
		m_pData = 0x0;
	}
}

CLayerGroup::CLayerGroup()
{
	m_aName[0] = 0;
	m_Visible = true;
	m_SaveToMap = true;
	m_Collapse = false;
	m_GameGroup = false;
	m_OffsetX = 0;
	m_OffsetY = 0;
	m_ParallaxX = 100;
	m_ParallaxY = 100;

	m_UseClipping = 0;
	m_ClipX = 0;
	m_ClipY = 0;
	m_ClipW = 0;
	m_ClipH = 0;
}

CLayerGroup::~CLayerGroup()
{
	Clear();
}

void CLayerGroup::Convert(CUIRect *pRect)
{
	pRect->x += m_OffsetX;
	pRect->y += m_OffsetY;
}

void CLayerGroup::Mapping(float *pPoints)
{
	m_pMap->m_pEditor->RenderTools()->MapscreenToWorld(
		m_pMap->m_pEditor->m_WorldOffsetX, m_pMap->m_pEditor->m_WorldOffsetY,
		m_ParallaxX/100.0f, m_ParallaxY/100.0f,
		m_OffsetX, m_OffsetY,
		m_pMap->m_pEditor->Graphics()->ScreenAspect(), m_pMap->m_pEditor->m_WorldZoom, pPoints);

	pPoints[0] += m_pMap->m_pEditor->m_EditorOffsetX;
	pPoints[1] += m_pMap->m_pEditor->m_EditorOffsetY;
	pPoints[2] += m_pMap->m_pEditor->m_EditorOffsetX;
	pPoints[3] += m_pMap->m_pEditor->m_EditorOffsetY;
}

void CLayerGroup::MapScreen()
{
	float aPoints[4];
	Mapping(aPoints);
	m_pMap->m_pEditor->Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);
}

void CLayerGroup::Render()
{
	MapScreen();
	IGraphics *pGraphics = m_pMap->m_pEditor->Graphics();

	if(m_UseClipping)
	{
		float aPoints[4];
		m_pMap->m_pGameGroup->Mapping(aPoints);
		float x0 = (m_ClipX - aPoints[0]) / (aPoints[2]-aPoints[0]);
		float y0 = (m_ClipY - aPoints[1]) / (aPoints[3]-aPoints[1]);
		float x1 = ((m_ClipX+m_ClipW) - aPoints[0]) / (aPoints[2]-aPoints[0]);
		float y1 = ((m_ClipY+m_ClipH) - aPoints[1]) / (aPoints[3]-aPoints[1]);

		pGraphics->ClipEnable((int)(x0*pGraphics->ScreenWidth()), (int)(y0*pGraphics->ScreenHeight()),
			(int)((x1-x0)*pGraphics->ScreenWidth()), (int)((y1-y0)*pGraphics->ScreenHeight()));
	}

	for(int i = 0; i < m_lLayers.size(); i++)
	{
		if(m_lLayers[i]->m_Visible && m_lLayers[i] != m_pMap->m_pGameLayer
		&& m_lLayers[i] != m_pMap->m_pFrontLayer && m_lLayers[i] != m_pMap->m_pTeleLayer
		&& m_lLayers[i] != m_pMap->m_pSpeedupLayer && m_lLayers[i] != m_pMap->m_pSwitchLayer && m_lLayers[i] != m_pMap->m_pTuneLayer)
		{
			if(m_pMap->m_pEditor->m_ShowDetail || !(m_lLayers[i]->m_Flags&LAYERFLAG_DETAIL))
				m_lLayers[i]->Render();
		}
	}

	pGraphics->ClipDisable();
}

void CLayerGroup::AddLayer(CLayer *l)
{
	m_pMap->m_Modified = true;
	m_lLayers.add(l);
}

void CLayerGroup::DeleteLayer(int Index)
{
	if(Index < 0 || Index >= m_lLayers.size()) return;
	delete m_lLayers[Index];
	m_lLayers.remove_index(Index);
	m_pMap->m_Modified = true;
	m_pMap->m_UndoModified++;
}

void CLayerGroup::GetSize(float *w, float *h)
{
	*w = 0; *h = 0;
	for(int i = 0; i < m_lLayers.size(); i++)
	{
		float lw, lh;
		m_lLayers[i]->GetSize(&lw, &lh);
		*w = max(*w, lw);
		*h = max(*h, lh);
	}
}

int CLayerGroup::SwapLayers(int Index0, int Index1)
{
	if(Index0 < 0 || Index0 >= m_lLayers.size()) return Index0;
	if(Index1 < 0 || Index1 >= m_lLayers.size()) return Index0;
	if(Index0 == Index1) return Index0;
	m_pMap->m_Modified = true;
	m_pMap->m_UndoModified++;
	swap(m_lLayers[Index0], m_lLayers[Index1]);
	return Index1;
}

void CEditorImage::AnalyseTileFlags()
{
	mem_zero(m_aTileFlags, sizeof(m_aTileFlags));

	int tw = m_Width/16; // tilesizes
	int th = m_Height/16;
	if ( tw == th && m_Format == CImageInfo::FORMAT_RGBA )
	{
		unsigned char *pPixelData = (unsigned char *)m_pData;

		int TileID = 0;
		for(int ty = 0; ty < 16; ty++)
			for(int tx = 0; tx < 16; tx++, TileID++)
			{
				bool Opaque = true;
				for(int x = 0; x < tw; x++)
					for(int y = 0; y < th; y++)
					{
						int p = (ty*tw+y)*m_Width + tx*tw+x;
						if(pPixelData[p*4+3] < 250)
						{
							Opaque = false;
							break;
						}
					}

				if(Opaque)
					m_aTileFlags[TileID] |= TILEFLAG_OPAQUE;
			}
	}

}

void CEditor::EnvelopeEval(float TimeOffset, int Env, float *pChannels, void *pUser)
{
	CEditor *pThis = (CEditor *)pUser;
	if(Env < 0 || Env >= pThis->m_Map.m_lEnvelopes.size())
	{
		pChannels[0] = 0;
		pChannels[1] = 0;
		pChannels[2] = 0;
		pChannels[3] = 0;
		return;
	}

	CEnvelope *e = pThis->m_Map.m_lEnvelopes[Env];
	float t = pThis->m_AnimateTime+TimeOffset;
	t *= pThis->m_AnimateSpeed;
	e->Eval(t, pChannels);
}

/********************************************************
 OTHER
*********************************************************/

// copied from gc_menu.cpp, should be more generalized
//extern int ui_do_edit_box(void *id, const CUIRect *rect, char *str, int str_size, float font_size, bool hidden=false);
int CEditor::DoEditBox(void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *Offset, bool Hidden, int Corners)
{
	int Inside = UI()->MouseInside(pRect);
	bool ReturnValue = false;
	bool UpdateOffset = false;
	static int s_AtIndex = 0;
	static bool s_DoScroll = false;
	static float s_ScrollStart = 0.0f;

	FontSize *= UI()->Scale();

	if(UI()->LastActiveItem() == pID)
	{
		m_EditBoxActive = 2;
		int Len = str_length(pStr);
		if(Len == 0)
			s_AtIndex = 0;

		if(Inside && UI()->MouseButton(0))
		{
			s_DoScroll = true;
			s_ScrollStart = UI()->MouseX();
			int MxRel = (int)(UI()->MouseX() - pRect->x);

			for(int i = 1; i <= Len; i++)
			{
				if(TextRender()->TextWidth(0, FontSize, pStr, i) - *Offset > MxRel)
				{
					s_AtIndex = i - 1;
					break;
				}

				if(i == Len)
					s_AtIndex = Len;
			}
		}
		else if(!UI()->MouseButton(0))
			s_DoScroll = false;
		else if(s_DoScroll)
		{
			// do scrolling
			if(UI()->MouseX() < pRect->x && s_ScrollStart-UI()->MouseX() > 10.0f)
			{
				s_AtIndex = max(0, s_AtIndex-1);
				s_ScrollStart = UI()->MouseX();
				UpdateOffset = true;
			}
			else if(UI()->MouseX() > pRect->x+pRect->w && UI()->MouseX()-s_ScrollStart > 10.0f)
			{
				s_AtIndex = min(Len, s_AtIndex+1);
				s_ScrollStart = UI()->MouseX();
				UpdateOffset = true;
			}
		}

		for(int i = 0; i < Input()->NumEvents(); i++)
		{
			Len = str_length(pStr);
			int NumChars = Len;
			ReturnValue |= CLineInput::Manipulate(Input()->GetEvent(i), pStr, StrSize, StrSize, &Len, &s_AtIndex, &NumChars);
		}
	}

	bool JustGotActive = false;

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
		{
			s_AtIndex = min(s_AtIndex, str_length(pStr));
			s_DoScroll = false;
			UI()->SetActiveItem(0);
		}
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			if (UI()->LastActiveItem() != pID)
				JustGotActive = true;
			UI()->SetActiveItem(pID);
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	CUIRect Textbox = *pRect;
	RenderTools()->DrawUIRect(&Textbox, vec4(1, 1, 1, 0.5f), Corners, 3.0f);
	Textbox.VMargin(2.0f, &Textbox);

	const char *pDisplayStr = pStr;
	char aStars[128];

	if(Hidden)
	{
		unsigned s = str_length(pStr);
		if(s >= sizeof(aStars))
			s = sizeof(aStars)-1;
		for(unsigned int i = 0; i < s; ++i)
			aStars[i] = '*';
		aStars[s] = 0;
		pDisplayStr = aStars;
	}

	// check if the text has to be moved
	if(UI()->LastActiveItem() == pID && !JustGotActive && (UpdateOffset || Input()->NumEvents()))
	{
		float w = TextRender()->TextWidth(0, FontSize, pDisplayStr, s_AtIndex);
		if(w-*Offset > Textbox.w)
		{
			// move to the left
			float wt = TextRender()->TextWidth(0, FontSize, pDisplayStr, -1);
			do
			{
				*Offset += min(wt-*Offset-Textbox.w, Textbox.w/3);
			}
			while(w-*Offset > Textbox.w);
		}
		else if(w-*Offset < 0.0f)
		{
			// move to the right
			do
			{
				*Offset = max(0.0f, *Offset-Textbox.w/3);
			}
			while(w-*Offset < 0.0f);
		}
	}
	UI()->ClipEnable(pRect);
	Textbox.x -= *Offset;

	UI()->DoLabel(&Textbox, pDisplayStr, FontSize, -1);

	// render the cursor
	if(UI()->LastActiveItem() == pID && !JustGotActive)
	{
		float w = TextRender()->TextWidth(0, FontSize, pDisplayStr, s_AtIndex);
		Textbox = *pRect;
		Textbox.VSplitLeft(2.0f, 0, &Textbox);
		Textbox.x += (w-*Offset-TextRender()->TextWidth(0, FontSize, "|", -1)/2);

		if((2*time_get()/time_freq()) % 2)	// make it blink
			UI()->DoLabel(&Textbox, "|", FontSize, -1);
	}
	UI()->ClipDisable();

	return ReturnValue;
}

vec4 CEditor::ButtonColorMul(const void *pID)
{
	if(UI()->ActiveItem() == pID)
		return vec4(1,1,1,0.5f);
	else if(UI()->HotItem() == pID)
		return vec4(1,1,1,1.5f);
	return vec4(1,1,1,1);
}

float CEditor::UiDoScrollbarV(const void *pID, const CUIRect *pRect, float Current)
{
	CUIRect Handle;
	static float s_OffsetY;
	pRect->HSplitTop(33, &Handle, 0);

	Handle.y += (pRect->h-Handle.h)*Current;

	// logic
	float Ret = Current;
	int Inside = UI()->MouseInside(&Handle);

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
			UI()->SetActiveItem(0);

		float Min = pRect->y;
		float Max = pRect->h-Handle.h;
		float Cur = UI()->MouseY()-s_OffsetY;
		Ret = (Cur-Min)/Max;
		if(Ret < 0.0f) Ret = 0.0f;
		if(Ret > 1.0f) Ret = 1.0f;
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pID);
			s_OffsetY = UI()->MouseY()-Handle.y;
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	// render
	CUIRect Rail;
	pRect->VMargin(5.0f, &Rail);
	RenderTools()->DrawUIRect(&Rail, vec4(1,1,1,0.25f), 0, 0.0f);

	CUIRect Slider = Handle;
	Slider.w = Rail.x-Slider.x;
	RenderTools()->DrawUIRect(&Slider, vec4(1,1,1,0.25f), CUI::CORNER_L, 2.5f);
	Slider.x = Rail.x+Rail.w;
	RenderTools()->DrawUIRect(&Slider, vec4(1,1,1,0.25f), CUI::CORNER_R, 2.5f);

	Slider = Handle;
	Slider.Margin(5.0f, &Slider);
	RenderTools()->DrawUIRect(&Slider, vec4(1,1,1,0.25f)*ButtonColorMul(pID), CUI::CORNER_ALL, 2.5f);

	return Ret;
}

vec4 CEditor::GetButtonColor(const void *pID, int Checked)
{
	if(Checked < 0)
		return vec4(0,0,0,0.5f);

	switch(Checked)
	{
	case 7: // selected + game layers
		if(UI()->HotItem() == pID)
			return vec4(1,0,0,0.4f);
		return vec4(1,0,0,0.2f);

	case 6: // game layers
		if(UI()->HotItem() == pID)
			return vec4(1,1,1,0.4f);
		return vec4(1,1,1,0.2f);

	case 5: // selected + image/sound should be embedded
		if(UI()->HotItem() == pID)
			return vec4(1,0,0,0.75f);
		return vec4(1,0,0,0.5f);

	case 4: // image/sound should be embedded
		if(UI()->HotItem() == pID)
			return vec4(1,0,0,1.0f);
		return vec4(1,0,0,0.875f);

	case 3: // selected + unused image/sound
		if(UI()->HotItem() == pID)
			return vec4(1,0,1,0.75f);
		return vec4(1,0,1,0.5f);

	case 2: // unused image/sound
		if(UI()->HotItem() == pID)
			return vec4(0,0,1,0.75f);
		return vec4(0,0,1,0.5f);

	case 1: // selected
		if(UI()->HotItem() == pID)
			return vec4(1,0,0,0.75f);
		return vec4(1,0,0,0.5f);

	default: // regular
		if(UI()->HotItem() == pID)
			return vec4(1,1,1,0.75f);
		return vec4(1,1,1,0.5f);
	}
}

int CEditor::DoButton_Editor_Common(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(UI()->MouseInside(pRect))
	{
		if(Flags&BUTTON_CONTEXT)
			ms_pUiGotContext = pID;
		if(m_pTooltip)
			m_pTooltip = pToolTip;
	}

	if(UI()->HotItem() == pID && pToolTip)
		m_pTooltip = (const char *)pToolTip;

	return UI()->DoButtonLogic(pID, pText, Checked, pRect);

	// Draw here
	//return UI()->DoButton(id, text, checked, r, draw_func, 0);
}


int CEditor::DoButton_Editor(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	RenderTools()->DrawUIRect(pRect, GetButtonColor(pID, Checked), CUI::CORNER_ALL, 3.0f);
	CUIRect NewRect = *pRect;
	NewRect.y += NewRect.h/2.0f-7.0f;
	float tw = min(TextRender()->TextWidth(0, 10.0f, pText, -1), NewRect.w);
	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, NewRect.x + NewRect.w/2-tw/2, NewRect.y - 1.0f, 10.0f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
	Cursor.m_LineWidth = NewRect.w;
	TextRender()->TextEx(&Cursor, pText, -1);
	Checked %= 2;
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Env(const void *pID, const char *pText, int Checked, const CUIRect *pRect, const char *pToolTip, vec3 BaseColor)
{
	float Bright = Checked ? 1.0f : 0.5f;
	float Alpha = UI()->HotItem() == pID ? 1.0f : 0.75f;
	vec4 Color = vec4(BaseColor.r*Bright, BaseColor.g*Bright, BaseColor.b*Bright, Alpha);

	RenderTools()->DrawUIRect(pRect, Color, CUI::CORNER_ALL, 3.0f);
	CUIRect NewRect = *pRect;
	NewRect.y += NewRect.h/2.0f-7.0f;
	float tw = min(TextRender()->TextWidth(0, 10.0f, pText, -1), NewRect.w);
	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, NewRect.x + NewRect.w/2-tw/2, NewRect.y - 1.0f, 10.0f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
	Cursor.m_LineWidth = NewRect.w;
	TextRender()->TextEx(&Cursor, pText, -1);
	Checked %= 2;
	return DoButton_Editor_Common(pID, pText, Checked, pRect, 0, pToolTip);
}

int CEditor::DoButton_File(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(Checked)
		RenderTools()->DrawUIRect(pRect, GetButtonColor(pID, Checked), CUI::CORNER_ALL, 3.0f);

	CUIRect t = *pRect;
	t.VMargin(5.0f, &t);
	UI()->DoLabel(&t, pText, 10, -1, -1);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Menu(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	CUIRect r = *pRect;
	RenderTools()->DrawUIRect(&r, vec4(0.5f, 0.5f, 0.5f, 1.0f), CUI::CORNER_T, 3.0f);

	r = *pRect;
	r.VMargin(5.0f, &r);
	UI()->DoLabel(&r, pText, 10, -1, -1);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_MenuItem(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(UI()->HotItem() == pID || Checked)
		RenderTools()->DrawUIRect(pRect, GetButtonColor(pID, Checked), CUI::CORNER_ALL, 3.0f);

	CUIRect t = *pRect;
	t.VMargin(5.0f, &t);
	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, t.x, t.y - 1.0f, 10.0f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
	Cursor.m_LineWidth = t.w;
	TextRender()->TextEx(&Cursor, pText, -1);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Tab(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	RenderTools()->DrawUIRect(pRect, GetButtonColor(pID, Checked), CUI::CORNER_T, 5.0f);
	CUIRect NewRect = *pRect;
	NewRect.y += NewRect.h/2.0f-7.0f;
	UI()->DoLabel(&NewRect, pText, 10, 0, -1);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Ex(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners, float FontSize)
{
	RenderTools()->DrawUIRect(pRect, GetButtonColor(pID, Checked), Corners, 3.0f);
	CUIRect NewRect = *pRect;
	NewRect.HMargin(NewRect.h/2.0f-FontSize/2.0f-1.0f, &NewRect);
	UI()->DoLabel(&NewRect, pText, FontSize, 0, -1);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_ButtonInc(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	RenderTools()->DrawUIRect(pRect, GetButtonColor(pID, Checked), CUI::CORNER_R, 3.0f);
	UI()->DoLabel(pRect, pText?pText:"+", 10, 0, -1);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_ButtonDec(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	RenderTools()->DrawUIRect(pRect, GetButtonColor(pID, Checked), CUI::CORNER_L, 3.0f);
	UI()->DoLabel(pRect, pText?pText:"-", 10, 0, -1);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_ColorPicker(const void *pID, const CUIRect *pRect, vec4 *pColor, const char *pToolTip)
{
	RenderTools()->DrawUIRect(pRect, *pColor, 0, 0.0f);
	return DoButton_Editor_Common(pID, 0x0, 0, pRect, 0, pToolTip);
}

void CEditor::RenderGrid(CLayerGroup *pGroup)
{
	if(!m_GridActive)
		return;

	float aGroupPoints[4];
	pGroup->Mapping(aGroupPoints);

	float w = UI()->Screen()->w;
	float h = UI()->Screen()->h;

	int LineDistance = GetLineDistance();

	int XOffset = aGroupPoints[0]/LineDistance;
	int YOffset = aGroupPoints[1]/LineDistance;
	int XGridOffset = XOffset % m_GridFactor;
	int YGridOffset = YOffset % m_GridFactor;

	Graphics()->TextureSet(-1);
	Graphics()->LinesBegin();

	for(int i = 0; i < (int)w; i++)
	{
		if((i+YGridOffset) % m_GridFactor == 0)
			Graphics()->SetColor(1.0f, 0.3f, 0.3f, 0.3f);
		else
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.15f);

		IGraphics::CLineItem Line = IGraphics::CLineItem(LineDistance*XOffset, LineDistance*i+LineDistance*YOffset, w+aGroupPoints[2], LineDistance*i+LineDistance*YOffset);
		Graphics()->LinesDraw(&Line, 1);

		if((i+XGridOffset) % m_GridFactor == 0)
			Graphics()->SetColor(1.0f, 0.3f, 0.3f, 0.3f);
		else
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.15f);

		Line = IGraphics::CLineItem(LineDistance*i+LineDistance*XOffset, LineDistance*YOffset, LineDistance*i+LineDistance*XOffset, h+aGroupPoints[3]);
		Graphics()->LinesDraw(&Line, 1);
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->LinesEnd();
}

void CEditor::RenderBackground(CUIRect View, int Texture, float Size, float Brightness)
{
	Graphics()->TextureSet(Texture);
	Graphics()->BlendNormal();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Brightness, Brightness, Brightness, 1.0f);
	Graphics()->QuadsSetSubset(0,0, View.w/Size, View.h/Size);
	IGraphics::CQuadItem QuadItem(View.x, View.y, View.w, View.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

int CEditor::UiDoValueSelector(void *pID, CUIRect *pRect, const char *pLabel, int Current, int Min, int Max, int Step, float Scale, const char *pToolTip, bool IsDegree, bool IsHex, int Corners, vec4* Color)
{
	// logic
	static float s_Value;
	static char s_NumStr[64];
	static bool s_TextMode = false;
	static void* s_LastTextpID = pID;
	int Inside = UI()->MouseInside(pRect);

	if(UI()->MouseButton(1) && UI()->HotItem() == pID)
	{
		s_LastTextpID = pID;
		s_TextMode = true;
		if(IsHex)
			str_format(s_NumStr, sizeof(s_NumStr), "%06X", Current);
		else
			str_format(s_NumStr, sizeof(s_NumStr), "%d", Current);
	}

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
		{
			m_LockMouse = false;
			UI()->SetActiveItem(0);
			s_TextMode = false;
		}
	}

	if(s_TextMode && s_LastTextpID == pID)
	{
		m_pTooltip = "Type your number";

		static float s_NumberBoxID = 0;
		DoEditBox(&s_NumberBoxID, pRect, s_NumStr, sizeof(s_NumStr), 10.0f, &s_NumberBoxID);

		UI()->SetActiveItem(&s_NumberBoxID);

		if(Input()->KeyIsPressed(KEY_RETURN) || Input()->KeyIsPressed(KEY_KP_ENTER) ||
				((UI()->MouseButton(1) || UI()->MouseButton(0)) && !Inside))
		{
			if(IsHex)
				Current = clamp(str_toint_base(s_NumStr, 16), Min, Max);
			else
				Current = clamp(str_toint(s_NumStr), Min, Max);
			m_LockMouse = false;
			UI()->SetActiveItem(0);
			s_TextMode = false;
		}

		if(Input()->KeyIsPressed(KEY_ESCAPE))
		{
			m_LockMouse = false;
			UI()->SetActiveItem(0);
			s_TextMode = false;
		}
	}
	else
	{
		if(UI()->ActiveItem() == pID)
		{
			if(UI()->MouseButton(0))
			{
				if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
					s_Value += m_MouseDeltaX*0.05f;
				else
					s_Value += m_MouseDeltaX;

				if(absolute(s_Value) > Scale)
				{
					int Count = (int)(s_Value/Scale);
					s_Value = fmod(s_Value, Scale);
					Current += Step*Count;
					Current = clamp(Current, Min, Max);
				}
			}
			if(pToolTip && !s_TextMode)
				m_pTooltip = pToolTip;
		}
		else if(UI()->HotItem() == pID)
		{
			if(UI()->MouseButton(0))
			{
				m_LockMouse = true;
				s_Value = 0;
				UI()->SetActiveItem(pID);
			}
			if(pToolTip && !s_TextMode)
				m_pTooltip = pToolTip;
		}

		if(Inside)
			UI()->SetHotItem(pID);

		// render
		char aBuf[128];
		if(pLabel[0] != '\0')
			str_format(aBuf, sizeof(aBuf),"%s %d", pLabel, Current);
		else if(IsDegree)
			str_format(aBuf, sizeof(aBuf),"%d°", Current);
		else if(IsHex)
			str_format(aBuf, sizeof(aBuf),"#%06X", Current);
		else
			str_format(aBuf, sizeof(aBuf),"%d", Current);
		RenderTools()->DrawUIRect(pRect, Color ? *Color : GetButtonColor(pID, 0), Corners, 5.0f);
		pRect->y += pRect->h/2.0f-7.0f;
		UI()->DoLabel(pRect, aBuf, 10, 0, -1);
	}

	return Current;
}

CLayerGroup *CEditor::GetSelectedGroup()
{
	if(m_SelectedGroup >= 0 && m_SelectedGroup < m_Map.m_lGroups.size())
		return m_Map.m_lGroups[m_SelectedGroup];
	return 0x0;
}

CLayer *CEditor::GetSelectedLayer(int Index)
{
	CLayerGroup *pGroup = GetSelectedGroup();
	if(!pGroup)
		return 0x0;

	if(m_SelectedLayer >= 0 && m_SelectedLayer < m_Map.m_lGroups[m_SelectedGroup]->m_lLayers.size())
		return pGroup->m_lLayers[m_SelectedLayer];
	return 0x0;
}

CLayer *CEditor::GetSelectedLayerType(int Index, int Type)
{
	CLayer *p = GetSelectedLayer(Index);
	if(p && p->m_Type == Type)
		return p;
	return 0x0;
}

CQuad *CEditor::GetSelectedQuad()
{
	CLayerQuads *ql = (CLayerQuads *)GetSelectedLayerType(0, LAYERTYPE_QUADS);
	if(!ql)
		return 0;
	if(m_SelectedQuad >= 0 && m_SelectedQuad < ql->m_lQuads.size())
		return &ql->m_lQuads[m_SelectedQuad];
	return 0;
}

CSoundSource *CEditor::GetSelectedSource()
{
	CLayerSounds *pSounds = (CLayerSounds *)GetSelectedLayerType(0, LAYERTYPE_SOUNDS);
	if(!pSounds)
		return 0;
	if(m_SelectedSource >= 0 && m_SelectedSource < pSounds->m_lSources.size())
		return &pSounds->m_lSources[m_SelectedSource];
	return 0;
}

void CEditor::CallbackOpenMap(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor*)pUser;
	if(pEditor->Load(pFileName, StorageType))
	{
		pEditor->m_ValidSaveFilename = StorageType == IStorage::TYPE_SAVE && pEditor->m_pFileDialogPath == pEditor->m_aFileDialogCurrentFolder;
		pEditor->m_Dialog = DIALOG_NONE;
	}
}

void CEditor::CallbackAppendMap(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor*)pUser;
	if(pEditor->Append(pFileName, StorageType))
		pEditor->m_aFileName[0] = 0;
	else
		pEditor->SortImages();

	pEditor->m_Dialog = DIALOG_NONE;
}

void CEditor::CallbackSaveMap(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = static_cast<CEditor*>(pUser);
	char aBuf[1024];
	const int Length = str_length(pFileName);
	// add map extension
	if(Length <= 4 || pFileName[Length-4] != '.' || str_comp_nocase(pFileName+Length-3, "map"))
	{
		str_format(aBuf, sizeof(aBuf), "%s.map", pFileName);
		pFileName = aBuf;
	}

	if(pEditor->Save(pFileName))
	{
		str_copy(pEditor->m_aFileName, pFileName, sizeof(pEditor->m_aFileName));
		pEditor->m_ValidSaveFilename = StorageType == IStorage::TYPE_SAVE && pEditor->m_pFileDialogPath == pEditor->m_aFileDialogCurrentFolder;
		pEditor->m_Map.m_Modified = false;
		pEditor->m_Map.m_UndoModified = 0;
		pEditor->m_LastUndoUpdateTime = time_get();
	}

	pEditor->m_Dialog = DIALOG_NONE;
}

void CEditor::CallbackSaveCopyMap(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = static_cast<CEditor*>(pUser);
	char aBuf[1024];
	const int Length = str_length(pFileName);
	// add map extension
	if(Length <= 4 || pFileName[Length-4] != '.' || str_comp_nocase(pFileName+Length-3, "map"))
	{
		str_format(aBuf, sizeof(aBuf), "%s.map", pFileName);
		pFileName = aBuf;
	}

	if(pEditor->Save(pFileName))
	{
		pEditor->m_Map.m_Modified = false;
		pEditor->m_Map.m_UndoModified = 0;
		pEditor->m_LastUndoUpdateTime = time_get();
	}

	pEditor->m_Dialog = DIALOG_NONE;
}

void CEditor::DoToolbar(CUIRect ToolBar)
{
	CUIRect TB_Top, TB_Bottom;
	CUIRect Button;

	ToolBar.HSplitTop(ToolBar.h/2.0f, &TB_Top, &TB_Bottom);

	TB_Top.HSplitBottom(2.5f, &TB_Top, 0);
	TB_Bottom.HSplitTop(2.5f, 0, &TB_Bottom);

	// ctrl+o to open
	if(Input()->KeyPress(KEY_O) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL)) && m_Dialog == DIALOG_NONE)
	{
		if(HasUnsavedData())
		{
			if(!m_PopupEventWasActivated)
			{
				m_PopupEventType = POPEVENT_LOAD;
				m_PopupEventActivated = true;
			}
		}
		else
			InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_MAP, "Load map", "Load", "maps", "", CallbackOpenMap, this);
	}

	// ctrl+s to save
	if(Input()->KeyPress(KEY_S) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL)) && m_Dialog == DIALOG_NONE)
	{
		if(m_aFileName[0] && m_ValidSaveFilename)
		{
			if(!m_PopupEventWasActivated)
			{
				str_copy(m_aFileSaveName, m_aFileName, sizeof(m_aFileSaveName));
				CallbackSaveMap(m_aFileSaveName, IStorage::TYPE_SAVE, this);
			}
		}
		else
			InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save", "maps", "", CallbackSaveMap, this);
	}

	// ctrl+shift+s to save as
	if(Input()->KeyPress(KEY_S) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL)) && (Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT)) && m_Dialog == DIALOG_NONE)
		InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save", "maps", "", CallbackSaveMap, this);

	// ctrl+shift+alt+s to save as
	if(Input()->KeyPress(KEY_S) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL)) && (Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT)) && (Input()->KeyIsPressed(KEY_LALT) || Input()->KeyIsPressed(KEY_RALT)) && m_Dialog == DIALOG_NONE)
		InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save", "maps", "", CallbackSaveCopyMap, this);

	// ctrl+l to load
	if(Input()->KeyPress(KEY_L) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL)) && m_Dialog == DIALOG_NONE)
	{
		InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_MAP, "Load map", "Load", "maps", "", CallbackOpenMap, this);
	}

	// detail button
	TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
	static int s_HqButton = 0;
	if(DoButton_Editor(&s_HqButton, "HD", m_ShowDetail, &Button, 0, "[ctrl+h] Toggle High Detail") ||
		(Input()->KeyPress(KEY_H) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL))))
	{
		m_ShowDetail = !m_ShowDetail;
	}

	TB_Top.VSplitLeft(5.0f, 0, &TB_Top);

	// animation button
	TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
	static int s_AnimateButton = 0;
	if(DoButton_Editor(&s_AnimateButton, "Anim", m_Animate, &Button, 0, "[ctrl+m] Toggle animation") ||
		(Input()->KeyPress(KEY_M) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL))))
	{
		m_AnimateStart = time_get();
		m_Animate = !m_Animate;
	}

	TB_Top.VSplitLeft(5.0f, 0, &TB_Top);

	// proof button
	TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
	static int s_ProofButton = 0;
	if(DoButton_Editor(&s_ProofButton, "Proof", m_ProofBorders, &Button, 0, "[ctrl+p] Toggles proof borders. These borders represent what a player maximum can see.") ||
		(Input()->KeyPress(KEY_P) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL))))
	{
		m_ProofBorders = !m_ProofBorders;
	}

	TB_Top.VSplitLeft(5.0f, 0, &TB_Top);

	// grid button
	TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
	static int s_GridButton = 0;
	if(DoButton_Editor(&s_GridButton, "Grid", m_GridActive, &Button, 0, "Toggle Grid"))
	{
		m_GridActive = !m_GridActive;
	}

	TB_Top.VSplitLeft(5.0f, 0, &TB_Top);

	// tile info button
	TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
	static int s_TileInfoButton = 0;
	if(DoButton_Editor(&s_TileInfoButton, "Info", m_ShowTileInfo, &Button, 0, "[ctrl+i] Show tile informations") ||
		(Input()->KeyPress(KEY_I) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL))))
	{
		m_ShowTileInfo = !m_ShowTileInfo;
		m_ShowEnvelopePreview = 0;
	}

	TB_Top.VSplitLeft(5.0f, 0, &TB_Top);

	// allow place unused tiles button
	TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
	static int s_AllowPlaceUnusedTilesButton = 0;
	if(DoButton_Editor(&s_AllowPlaceUnusedTilesButton, "Unused", m_AllowPlaceUnusedTiles, &Button, 0, "[ctrl+u] Allow placing unused tiles") ||
		(Input()->KeyPress('u') && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL))))
	{
		m_AllowPlaceUnusedTiles = !m_AllowPlaceUnusedTiles;
	}

	TB_Top.VSplitLeft(10.0f, 0, &TB_Top);

	// zoom group
	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_ZoomOutButton = 0;
	if(DoButton_Ex(&s_ZoomOutButton, "ZO", 0, &Button, 0, "[NumPad-] Zoom out", CUI::CORNER_L))
		m_ZoomLevel += 50;

	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_ZoomNormalButton = 0;
	if(DoButton_Ex(&s_ZoomNormalButton, "1:1", 0, &Button, 0, "[NumPad*] Zoom to normal and remove editor offset", 0))
	{
		m_EditorOffsetX = 0;
		m_EditorOffsetY = 0;
		m_ZoomLevel = 100;
	}

	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_ZoomInButton = 0;
	if(DoButton_Ex(&s_ZoomInButton, "ZI", 0, &Button, 0, "[NumPad+] Zoom in", CUI::CORNER_R))
		m_ZoomLevel -= 50;

	TB_Top.VSplitLeft(10.0f, 0, &TB_Top);

	// animation speed
	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_AnimFasterButton = 0;
	if(DoButton_Ex(&s_AnimFasterButton, "A+", 0, &Button, 0, "Increase animation speed", CUI::CORNER_L))
		m_AnimateSpeed += 0.5f;

	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_AnimNormalButton = 0;
	if(DoButton_Ex(&s_AnimNormalButton, "1", 0, &Button, 0, "Normal animation speed", 0))
		m_AnimateSpeed = 1.0f;

	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_AnimSlowerButton = 0;
	if(DoButton_Ex(&s_AnimSlowerButton, "A-", 0, &Button, 0, "Decrease animation speed", CUI::CORNER_R))
	{
		if(m_AnimateSpeed > 0.5f)
			m_AnimateSpeed -= 0.5f;
	}

	TB_Top.VSplitLeft(10.0f, &Button, &TB_Top);


	// brush manipulation
	{
		int Enabled = m_Brush.IsEmpty()?-1:0;

		// flip buttons
		TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
		static int s_FlipXButton = 0;
		if(DoButton_Ex(&s_FlipXButton, "X/X", Enabled, &Button, 0, "[N] Flip brush horizontal", CUI::CORNER_L) || (Input()->KeyPress(KEY_N) && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0))
		{
			for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
				m_Brush.m_lLayers[i]->BrushFlipX();
		}

		TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
		static int s_FlipyButton = 0;
		if(DoButton_Ex(&s_FlipyButton, "Y/Y", Enabled, &Button, 0, "[M] Flip brush vertical", CUI::CORNER_R) || (Input()->KeyPress(KEY_M) && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0))
		{
			for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
				m_Brush.m_lLayers[i]->BrushFlipY();
		}

		// rotate buttons
		TB_Top.VSplitLeft(10.0f, &Button, &TB_Top);

		TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
		static int s_RotationAmount = 90;
		bool TileLayer = false;
		// check for tile layers in brush selection
		for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
			if(m_Brush.m_lLayers[i]->m_Type == LAYERTYPE_TILES)
			{
				TileLayer = true;
				s_RotationAmount = max(90, (s_RotationAmount/90)*90);
				break;
			}
		s_RotationAmount = UiDoValueSelector(&s_RotationAmount, &Button, "", s_RotationAmount, TileLayer?90:1, 359, TileLayer?90:1, TileLayer?10.0f:2.0f, "Rotation of the brush in degrees. Use left mouse button to drag and change the value. Hold shift to be more precise.", true);

		TB_Top.VSplitLeft(5.0f, &Button, &TB_Top);
		TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
		static int s_CcwButton = 0;
		if(DoButton_Ex(&s_CcwButton, "CCW", Enabled, &Button, 0, "[R] Rotates the brush counter clockwise", CUI::CORNER_L) || (Input()->KeyPress(KEY_R) && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0))
		{
			for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
				m_Brush.m_lLayers[i]->BrushRotate(-s_RotationAmount/360.0f*pi*2);
		}

		TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
		static int s_CwButton = 0;
		if(DoButton_Ex(&s_CwButton, "CW", Enabled, &Button, 0, "[T] Rotates the brush clockwise", CUI::CORNER_R) || (Input()->KeyPress(KEY_T) && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0))
		{
			for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
				m_Brush.m_lLayers[i]->BrushRotate(s_RotationAmount/360.0f*pi*2);
		}
	}

	// refocus button
	TB_Bottom.VSplitLeft(45.0f, &Button, &TB_Bottom);
	static int s_RefocusButton = 0;
	if(DoButton_Editor(&s_RefocusButton, "Refocus", m_WorldOffsetX&&m_WorldOffsetY?0:-1, &Button, 0, "[HOME] Restore map focus") || (m_EditBoxActive == 0 && Input()->KeyPress(KEY_HOME)))
	{
		m_WorldOffsetX = 0;
		m_WorldOffsetY = 0;
	}

	TB_Bottom.VSplitLeft(5.0f, 0, &TB_Bottom);

	// tile manipulation
	{
		TB_Bottom.VSplitLeft(45.0f, &Button, &TB_Bottom);
		static int s_BorderBut = 0;
		CLayerTiles *pT = (CLayerTiles *)GetSelectedLayerType(0, LAYERTYPE_TILES);

		// no border for tele layer, speedup, front and switch
		if(pT && (pT->m_Tele || pT->m_Speedup || pT->m_Switch || pT->m_Front || pT->m_Tune))
			pT = 0;

		if(DoButton_Editor(&s_BorderBut, "Border", pT?0:-1, &Button, 0, "Adds border tiles"))
		{
			if(pT)
				DoMapBorder();
		}

		TB_Bottom.VSplitLeft(10.0f, &Button, &TB_Bottom);

		// do tele/tune/switch/speedup button
		{
			TB_Bottom.VSplitLeft(60.0f, &Button, &TB_Bottom);
			{
				int (*pPopupFunc)(CEditor *peditor, CUIRect View) = NULL;
				const char *aButtonName = "Modifier";
				float Height = 0.0f;
				int Checked = -1;
				CLayerTiles *pS = (CLayerTiles *)GetSelectedLayerType(0, LAYERTYPE_TILES);
				if(pS)
				{
					if(pS == m_Map.m_pSwitchLayer)
					{
						aButtonName = "Switch";
						pPopupFunc = PopupSwitch;
						Height = 36;
						Checked = 0;
					}
					else if(pS == m_Map.m_pSpeedupLayer)
					{
						aButtonName = "Speedup";
						pPopupFunc = PopupSpeedup;
						Height = 53;
						Checked = 0;
					}
					else if(pS == m_Map.m_pTuneLayer)
					{
						aButtonName = "Tune";
						pPopupFunc = PopupTune;
						Height = 23;
						Checked = 0;
					}
					else if(pS == m_Map.m_pTeleLayer)
					{
						aButtonName = "Tele";
						pPopupFunc = PopupTele;
						Height = 23;
						Checked = 0;
					}
				}
				static int s_ModifierButton = 0;
				if(DoButton_Ex(&s_ModifierButton, aButtonName, Checked, &Button, 0, aButtonName, CUI::CORNER_ALL))
				{
					static int s_ModifierPopupID = 0;
					UiInvokePopupMenu(&s_ModifierPopupID, 0, UI()->MouseX(), UI()->MouseY(), 120, Height, pPopupFunc);
				}
			}
		}
	}

	TB_Bottom.VSplitLeft(10.0f, 0, &TB_Bottom);

	// grid zoom
	TB_Bottom.VSplitLeft(30.0f, &Button, &TB_Bottom);
	static int s_GridIncreaseButton = 0;
	if(DoButton_Ex(&s_GridIncreaseButton, "G-", 0, &Button, 0, "Decrease grid", CUI::CORNER_L))
	{
		if(m_GridFactor > 1)
			m_GridFactor--;
	}

	TB_Bottom.VSplitLeft(30.0f, &Button, &TB_Bottom);
	static int s_GridNormalButton = 0;
	if(DoButton_Ex(&s_GridNormalButton, "1", 0, &Button, 0, "Normal grid", 0))
		m_GridFactor = 1;

	TB_Bottom.VSplitLeft(30.0f, &Button, &TB_Bottom);

	static int s_GridDecreaseButton = 0;
	if(DoButton_Ex(&s_GridDecreaseButton, "G+", 0, &Button, 0, "Increase grid", CUI::CORNER_R))
	{
		if(m_GridFactor < 15)
			m_GridFactor++;
	}

	// sound source manipulation
	{
		// do add button
		TB_Bottom.VSplitLeft(10.0f, &Button, &TB_Bottom);
		TB_Bottom.VSplitLeft(60.0f, &Button, &TB_Bottom);
		static int s_NewButton = 0;

		CLayerSounds *pSoundLayer = (CLayerSounds *)GetSelectedLayerType(0, LAYERTYPE_SOUNDS);
		if(DoButton_Editor(&s_NewButton, "Add Sound", pSoundLayer?0:-1, &Button, 0, "Adds a new sound source"))
		{
			if(pSoundLayer)
			{
				float Mapping[4];
				CLayerGroup *g = GetSelectedGroup();
				g->Mapping(Mapping);
				int AddX = f2fx(Mapping[0] + (Mapping[2]-Mapping[0])/2);
				int AddY = f2fx(Mapping[1] + (Mapping[3]-Mapping[1])/2);

				CSoundSource *pSource = pSoundLayer->NewSource();
				pSource->m_Position.x += AddX;
				pSource->m_Position.y += AddY;
			}
		}
	}

	// quad manipulation
	{
		// do add button
		TB_Bottom.VSplitLeft(10.0f, &Button, &TB_Bottom);
		TB_Bottom.VSplitLeft(60.0f, &Button, &TB_Bottom);
		static int s_NewButton = 0;

		CLayerQuads *pQLayer = (CLayerQuads *)GetSelectedLayerType(0, LAYERTYPE_QUADS);
		//CLayerTiles *tlayer = (CLayerTiles *)get_selected_layer_type(0, LAYERTYPE_TILES);
		if(DoButton_Editor(&s_NewButton, "Add Quad", pQLayer?0:-1, &Button, 0, "Adds a new quad"))
		{
			if(pQLayer)
			{
				float Mapping[4];
				CLayerGroup *g = GetSelectedGroup();
				g->Mapping(Mapping);
				int AddX = f2fx(Mapping[0] + (Mapping[2]-Mapping[0])/2);
				int AddY = f2fx(Mapping[1] + (Mapping[3]-Mapping[1])/2);

				CQuad *q = pQLayer->NewQuad();
				for(int i = 0; i < 5; i++)
				{
					q->m_aPoints[i].x += AddX;
					q->m_aPoints[i].y += AddY;
				}
			}
		}
	}

}

static void Rotate(const CPoint *pCenter, CPoint *pPoint, float Rotation)
{
	int x = pPoint->x - pCenter->x;
	int y = pPoint->y - pCenter->y;
	pPoint->x = (int)(x * cosf(Rotation) - y * sinf(Rotation) + pCenter->x);
	pPoint->y = (int)(x * sinf(Rotation) + y * cosf(Rotation) + pCenter->y);
}

void CEditor::DoSoundSource(CSoundSource *pSource, int Index)
{
	enum
	{
		OP_NONE=0,
		OP_MOVE,
		OP_CONTEXT_MENU,
	};

	void *pID = &pSource->m_Position;

	static float s_LastWx;
	static float s_LastWy;
	static int s_Operation = OP_NONE;

	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();

	float CenterX = fx2f(pSource->m_Position.x);
	float CenterY = fx2f(pSource->m_Position.y);

	float dx = (CenterX - wx)/m_WorldZoom;
	float dy = (CenterY - wy)/m_WorldZoom;
	if(dx*dx+dy*dy < 50)
		UI()->SetHotItem(pID);

	bool IgnoreGrid;
	if(Input()->KeyIsPressed(KEY_LALT) || Input()->KeyIsPressed(KEY_RALT))
		IgnoreGrid = true;
	else
		IgnoreGrid = false;

	if(UI()->ActiveItem() == pID)
	{
		if(m_MouseDeltaWx*m_MouseDeltaWx+m_MouseDeltaWy*m_MouseDeltaWy > 0.5f)
		{
			if(s_Operation == OP_MOVE)
			{
				if(m_GridActive && !IgnoreGrid)
				{
					int LineDistance = GetLineDistance();

					float x = 0.0f;
					float y = 0.0f;
					if(wx >= 0)
						x = (int)((wx+(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);
					else
						x = (int)((wx-(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);
					if(wy >= 0)
						y = (int)((wy+(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);
					else
						y = (int)((wy-(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);

					pSource->m_Position.x = f2fx(x);
					pSource->m_Position.y = f2fx(y);
				}
				else
				{
					pSource->m_Position.x += f2fx(wx-s_LastWx);
					pSource->m_Position.y += f2fx(wy-s_LastWy);
				}
			}
		}

		s_LastWx = wx;
		s_LastWy = wy;

		if(s_Operation == OP_CONTEXT_MENU)
		{
			if(!UI()->MouseButton(1))
			{
				m_Map.m_UndoModified++;

				static int s_SourcePopupID = 0;
				UiInvokePopupMenu(&s_SourcePopupID, 0, UI()->MouseX(), UI()->MouseY(), 120, 200, PopupSource);
				m_LockMouse = false;
				s_Operation = OP_NONE;
				UI()->SetActiveItem(0);
			}
		}
		else
		{
			if(!UI()->MouseButton(0))
			{
				if(s_Operation == OP_MOVE)
				{
					m_Map.m_UndoModified++;
				}

				m_LockMouse = false;
				s_Operation = OP_NONE;
				UI()->SetActiveItem(0);
			}
		}

		Graphics()->SetColor(1,1,1,1);
	}
	else if(UI()->HotItem() == pID)
	{
		ms_pUiGotContext = pID;

		Graphics()->SetColor(1,1,1,1);
		m_pTooltip = "Left mouse button to move. Hold alt to ignore grid.";

		if(UI()->MouseButton(0))
		{
			s_Operation = OP_MOVE;

			UI()->SetActiveItem(pID);
			m_SelectedSource = Index;
			s_LastWx = wx;
			s_LastWy = wy;
		}

		if(UI()->MouseButton(1))
		{
			m_SelectedSource = Index;
			s_Operation = OP_CONTEXT_MENU;
			UI()->SetActiveItem(pID);
		}
	}
	else
	{
		Graphics()->SetColor(0,1,0,1);
	}

	IGraphics::CQuadItem QuadItem(CenterX, CenterY, 5.0f*m_WorldZoom, 5.0f*m_WorldZoom);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CEditor::DoQuad(CQuad *q, int Index)
{
	enum
	{
		OP_NONE=0,
		OP_MOVE_ALL,
		OP_MOVE_PIVOT,
		OP_ROTATE,
		OP_CONTEXT_MENU,
		OP_DELETE,
	};

	// some basic values
	void *pID = &q->m_aPoints[4]; // use pivot addr as id
	static CPoint s_RotatePoints[4];
	static float s_LastWx;
	static float s_LastWy;
	static int s_Operation = OP_NONE;
	static float s_RotateAngle = 0;
	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();

	// get pivot
	float CenterX = fx2f(q->m_aPoints[4].x);
	float CenterY = fx2f(q->m_aPoints[4].y);

	float dx = (CenterX - wx)/m_WorldZoom;
	float dy = (CenterY - wy)/m_WorldZoom;
	if(dx*dx+dy*dy < 50)
		UI()->SetHotItem(pID);

	bool IgnoreGrid;
	if(Input()->KeyIsPressed(KEY_LALT) || Input()->KeyIsPressed(KEY_RALT))
		IgnoreGrid = true;
	else
		IgnoreGrid = false;

	// draw selection background
	if(m_SelectedQuad == Index)
	{
		Graphics()->SetColor(0,0,0,1);
		IGraphics::CQuadItem QuadItem(CenterX, CenterY, 7.0f*m_WorldZoom, 7.0f*m_WorldZoom);
		Graphics()->QuadsDraw(&QuadItem, 1);
	}

	if(UI()->ActiveItem() == pID)
	{
		if(m_MouseDeltaWx*m_MouseDeltaWx+m_MouseDeltaWy*m_MouseDeltaWy > 0.5f)
		{
			// check if we only should move pivot
			if(s_Operation == OP_MOVE_PIVOT)
			{
				if(m_GridActive && !IgnoreGrid)
				{
					int LineDistance = GetLineDistance();

					float x = 0.0f;
					float y = 0.0f;
					if(wx >= 0)
						x = (int)((wx+(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);
					else
						x = (int)((wx-(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);
					if(wy >= 0)
						y = (int)((wy+(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);
					else
						y = (int)((wy-(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);

					q->m_aPoints[4].x = f2fx(x);
					q->m_aPoints[4].y = f2fx(y);
				}
				else
				{
					q->m_aPoints[4].x += f2fx(wx-s_LastWx);
					q->m_aPoints[4].y += f2fx(wy-s_LastWy);
				}
			}
			else if(s_Operation == OP_MOVE_ALL)
			{
				// move all points including pivot
				if(m_GridActive && !IgnoreGrid)
				{
					int LineDistance = GetLineDistance();

					float x = 0.0f;
					float y = 0.0f;
					if(wx >= 0)
						x = (int)((wx+(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);
					else
						x = (int)((wx-(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);
					if(wy >= 0)
						y = (int)((wy+(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);
					else
						y = (int)((wy-(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);

					int OldX = q->m_aPoints[4].x;
					int OldY = q->m_aPoints[4].y;
					q->m_aPoints[4].x = f2fx(x);
					q->m_aPoints[4].y = f2fx(y);
					int DiffX = q->m_aPoints[4].x - OldX;
					int DiffY = q->m_aPoints[4].y - OldY;

					for(int v = 0; v < 4; v++)
					{
						q->m_aPoints[v].x += DiffX;
						q->m_aPoints[v].y += DiffY;
					}
				}
				else
				{
					for(int v = 0; v < 5; v++)
					{
							q->m_aPoints[v].x += f2fx(wx-s_LastWx);
							q->m_aPoints[v].y += f2fx(wy-s_LastWy);
					}
				}
			}
			else if(s_Operation == OP_ROTATE)
			{
				for(int v = 0; v < 4; v++)
				{
					q->m_aPoints[v] = s_RotatePoints[v];
					Rotate(&q->m_aPoints[4], &q->m_aPoints[v], s_RotateAngle);
				}
			}
		}

		s_RotateAngle += (m_MouseDeltaX) * 0.002f;
		s_LastWx = wx;
		s_LastWy = wy;

		if(s_Operation == OP_CONTEXT_MENU)
		{
			if(!UI()->MouseButton(1))
			{
				m_Map.m_UndoModified++;

				static int s_QuadPopupID = 0;
				UiInvokePopupMenu(&s_QuadPopupID, 0, UI()->MouseX(), UI()->MouseY(), 120, 180, PopupQuad);
				m_LockMouse = false;
				s_Operation = OP_NONE;
				UI()->SetActiveItem(0);
			}
		}
		else if(s_Operation == OP_DELETE)
		{
			if(!UI()->MouseButton(1))
			{
				m_Map.m_UndoModified++;
				m_LockMouse = false;
				m_Map.m_Modified = true;
				CLayerQuads *pLayer = (CLayerQuads *)GetSelectedLayerType(0, LAYERTYPE_QUADS);
				if(pLayer)
					pLayer->m_lQuads.remove_index(m_SelectedQuad);
				s_Operation = OP_NONE;
				UI()->SetActiveItem(0);
			}
		}
		else
		{
			if(!UI()->MouseButton(0))
			{
				if(s_Operation == OP_ROTATE || s_Operation == OP_MOVE_ALL || s_Operation == OP_MOVE_PIVOT)
				{
					m_Map.m_UndoModified++;
				}

				m_LockMouse = false;
				s_Operation = OP_NONE;
				UI()->SetActiveItem(0);
			}
		}

		Graphics()->SetColor(1,1,1,1);
	}
	else if(UI()->HotItem() == pID)
	{
		ms_pUiGotContext = pID;

		Graphics()->SetColor(1,1,1,1);
		m_pTooltip = "Left mouse button to move. Hold shift to move pivot. Hold ctrl to rotate. Hold alt to ignore grid. Hold shift and right click to delete.";

		if(UI()->MouseButton(0))
		{
			if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
				s_Operation = OP_MOVE_PIVOT;
			else if(Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL))
			{
				m_LockMouse = true;
				s_Operation = OP_ROTATE;
				s_RotateAngle = 0;
				s_RotatePoints[0] = q->m_aPoints[0];
				s_RotatePoints[1] = q->m_aPoints[1];
				s_RotatePoints[2] = q->m_aPoints[2];
				s_RotatePoints[3] = q->m_aPoints[3];
			}
			else
				s_Operation = OP_MOVE_ALL;

			UI()->SetActiveItem(pID);
			if(m_SelectedQuad != Index)
				m_SelectedPoints = 0;
			m_SelectedQuad = Index;
			s_LastWx = wx;
			s_LastWy = wy;
		}

		if(UI()->MouseButton(1))
		{
			if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
			{
				if(m_SelectedQuad != Index)
					m_SelectedPoints = 0;
				m_SelectedQuad = Index;
				s_Operation = OP_DELETE;
				UI()->SetActiveItem(pID);
			}
			else
			{
				if(m_SelectedQuad != Index)
					m_SelectedPoints = 0;
				m_SelectedQuad = Index;
				s_Operation = OP_CONTEXT_MENU;
				UI()->SetActiveItem(pID);
			}
		}
	}
	else
		Graphics()->SetColor(0,1,0,1);

	IGraphics::CQuadItem QuadItem(CenterX, CenterY, 5.0f*m_WorldZoom, 5.0f*m_WorldZoom);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CEditor::DoQuadPoint(CQuad *pQuad, int QuadIndex, int V)
{
	void *pID = &pQuad->m_aPoints[V];

	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();

	float px = fx2f(pQuad->m_aPoints[V].x);
	float py = fx2f(pQuad->m_aPoints[V].y);

	float dx = (px - wx)/m_WorldZoom;
	float dy = (py - wy)/m_WorldZoom;
	if(dx*dx+dy*dy < 50)
		UI()->SetHotItem(pID);

	// draw selection background
	if(m_SelectedQuad == QuadIndex && m_SelectedPoints&(1<<V))
	{
		Graphics()->SetColor(0,0,0,1);
		IGraphics::CQuadItem QuadItem(px, py, 7.0f*m_WorldZoom, 7.0f*m_WorldZoom);
		Graphics()->QuadsDraw(&QuadItem, 1);
	}

	enum
	{
		OP_NONE=0,
		OP_MOVEPOINT,
		OP_MOVEUV,
		OP_CONTEXT_MENU
	};

	static bool s_Moved;
	static int s_Operation = OP_NONE;

	bool IgnoreGrid;
	if(Input()->KeyIsPressed(KEY_LALT) || Input()->KeyIsPressed(KEY_RALT))
		IgnoreGrid = true;
	else
		IgnoreGrid = false;

	if(UI()->ActiveItem() == pID)
	{
		float dx = m_MouseDeltaWx;
		float dy = m_MouseDeltaWy;
		if(!s_Moved)
		{
			if(dx*dx+dy*dy > 0.5f)
				s_Moved = true;
		}

		if(s_Moved)
		{
			if(s_Operation == OP_MOVEPOINT)
			{
				if(m_GridActive && !IgnoreGrid)
				{
					for(int m = 0; m < 4; m++)
						if(m_SelectedPoints&(1<<m))
						{
							int LineDistance = GetLineDistance();

							float x = 0.0f;
							float y = 0.0f;
							if(wx >= 0)
								x = (int)((wx+(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);
							else
								x = (int)((wx-(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);
							if(wy >= 0)
								y = (int)((wy+(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);
							else
								y = (int)((wy-(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);

							pQuad->m_aPoints[m].x = f2fx(x);
							pQuad->m_aPoints[m].y = f2fx(y);
						}
				}
				else
				{
					for(int m = 0; m < 4; m++)
						if(m_SelectedPoints&(1<<m))
						{
							pQuad->m_aPoints[m].x += f2fx(dx);
							pQuad->m_aPoints[m].y += f2fx(dy);
						}
				}
			}
			else if(s_Operation == OP_MOVEUV)
			{
				for(int m = 0; m < 4; m++)
					if(m_SelectedPoints&(1<<m))
					{
						// 0,2;1,3 - line x
						// 0,1;2,3 - line y

						pQuad->m_aTexcoords[m].x += f2fx(dx*0.001f);
						pQuad->m_aTexcoords[(m+2)%4].x += f2fx(dx*0.001f);

						pQuad->m_aTexcoords[m].y += f2fx(dy*0.001f);
						pQuad->m_aTexcoords[m^1].y += f2fx(dy*0.001f);
					}
			}
		}

		if(s_Operation == OP_CONTEXT_MENU)
		{
			if(!UI()->MouseButton(1))
			{
				m_Map.m_UndoModified++;

				static int s_PointPopupID = 0;
				UiInvokePopupMenu(&s_PointPopupID, 0, UI()->MouseX(), UI()->MouseY(), 120, 150, PopupPoint);
				UI()->SetActiveItem(0);
			}
		}
		else
		{
			if(!UI()->MouseButton(0))
			{
				if(!s_Moved)
				{
					if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
						m_SelectedPoints ^= 1<<V;
					else
						m_SelectedPoints = 1<<V;
				}

				if(s_Operation == OP_MOVEPOINT || s_Operation == OP_MOVEUV)
					m_Map.m_UndoModified++;

				m_LockMouse = false;
				UI()->SetActiveItem(0);
			}
		}

		Graphics()->SetColor(1,1,1,1);
	}
	else if(UI()->HotItem() == pID)
	{
		ms_pUiGotContext = pID;

		Graphics()->SetColor(1,1,1,1);
		m_pTooltip = "Left mouse button to move. Hold shift to move the texture. Hold alt to ignore grid.";

		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pID);
			s_Moved = false;
			if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
			{
				s_Operation = OP_MOVEUV;
				m_LockMouse = true;
			}
			else
				s_Operation = OP_MOVEPOINT;

			if(!(m_SelectedPoints&(1<<V)))
			{
				if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
					m_SelectedPoints |= 1<<V;
				else
					m_SelectedPoints = 1<<V;
				s_Moved = true;
			}

			m_SelectedQuad = QuadIndex;
		}
		else if(UI()->MouseButton(1))
		{
			s_Operation = OP_CONTEXT_MENU;
			m_SelectedQuad = QuadIndex;
			UI()->SetActiveItem(pID);
			if(!(m_SelectedPoints&(1<<V)))
			{
				if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
					m_SelectedPoints |= 1<<V;
				else
					m_SelectedPoints = 1<<V;
				s_Moved = true;
			}
		}
	}
	else
		Graphics()->SetColor(1,0,0,1);

	IGraphics::CQuadItem QuadItem(px, py, 5.0f*m_WorldZoom, 5.0f*m_WorldZoom);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CEditor::DoQuadEnvelopes(const array<CQuad> &lQuads, int TexID)
{
	int Num = lQuads.size();
	CEnvelope **apEnvelope = new CEnvelope*[Num];
	mem_zero(apEnvelope, sizeof(CEnvelope*)*Num);
	for(int i = 0; i < Num; i++)
	{
		if((m_ShowEnvelopePreview == 1 && lQuads[i].m_PosEnv == m_SelectedEnvelope) || m_ShowEnvelopePreview == 2)
			if(lQuads[i].m_PosEnv >= 0 && lQuads[i].m_PosEnv < m_Map.m_lEnvelopes.size())
				apEnvelope[i] = m_Map.m_lEnvelopes[lQuads[i].m_PosEnv];
	}

	//Draw Lines
	Graphics()->TextureSet(-1);
	Graphics()->LinesBegin();
	Graphics()->SetColor(80.0f/255, 150.0f/255, 230.f/255, 0.5f);
	for(int j = 0; j < Num; j++)
	{
		if(!apEnvelope[j])
			continue;

		//QuadParams
		const CPoint *pPoints = lQuads[j].m_aPoints;
		for(int i = 0; i < apEnvelope[j]->m_lPoints.size()-1; i++)
		{
			float OffsetX =  fx2f(apEnvelope[j]->m_lPoints[i].m_aValues[0]);
			float OffsetY = fx2f(apEnvelope[j]->m_lPoints[i].m_aValues[1]);
			vec2 Pos0 = vec2(fx2f(pPoints[4].x)+OffsetX, fx2f(pPoints[4].y)+OffsetY);

			OffsetX = fx2f(apEnvelope[j]->m_lPoints[i+1].m_aValues[0]);
			OffsetY = fx2f(apEnvelope[j]->m_lPoints[i+1].m_aValues[1]);
			vec2 Pos1 = vec2(fx2f(pPoints[4].x)+OffsetX, fx2f(pPoints[4].y)+OffsetY);

			IGraphics::CLineItem Line = IGraphics::CLineItem(Pos0.x, Pos0.y, Pos1.x, Pos1.y);
			Graphics()->LinesDraw(&Line, 1);
		}
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->LinesEnd();

	//Draw Quads
	Graphics()->TextureSet(TexID);
	Graphics()->QuadsBegin();

	for(int j = 0; j < Num; j++)
	{
		if(!apEnvelope[j])
			continue;

		//QuadParams
		const CPoint *pPoints = lQuads[j].m_aPoints;

		for(int i = 0; i < apEnvelope[j]->m_lPoints.size(); i++)
		{
			//Calc Env Position
			float OffsetX =  fx2f(apEnvelope[j]->m_lPoints[i].m_aValues[0]);
			float OffsetY = fx2f(apEnvelope[j]->m_lPoints[i].m_aValues[1]);
			float Rot = fx2f(apEnvelope[j]->m_lPoints[i].m_aValues[2])/360.0f*pi*2;

			//Set Colours
			float Alpha = (m_SelectedQuadEnvelope == lQuads[j].m_PosEnv && m_SelectedEnvelopePoint == i) ? 0.65f : 0.35f;
			IGraphics::CColorVertex aArray[4] = {
				IGraphics::CColorVertex(0, lQuads[j].m_aColors[0].r, lQuads[j].m_aColors[0].g, lQuads[j].m_aColors[0].b, Alpha),
				IGraphics::CColorVertex(1, lQuads[j].m_aColors[1].r, lQuads[j].m_aColors[1].g, lQuads[j].m_aColors[1].b, Alpha),
				IGraphics::CColorVertex(2, lQuads[j].m_aColors[2].r, lQuads[j].m_aColors[2].g, lQuads[j].m_aColors[2].b, Alpha),
				IGraphics::CColorVertex(3, lQuads[j].m_aColors[3].r, lQuads[j].m_aColors[3].g, lQuads[j].m_aColors[3].b, Alpha)};
			Graphics()->SetColorVertex(aArray, 4);

			//Rotation
			if(Rot != 0)
			{
				static CPoint aRotated[4];
				aRotated[0] = lQuads[j].m_aPoints[0];
				aRotated[1] = lQuads[j].m_aPoints[1];
				aRotated[2] = lQuads[j].m_aPoints[2];
				aRotated[3] = lQuads[j].m_aPoints[3];
				pPoints = aRotated;

				Rotate(&lQuads[j].m_aPoints[4], &aRotated[0], Rot);
				Rotate(&lQuads[j].m_aPoints[4], &aRotated[1], Rot);
				Rotate(&lQuads[j].m_aPoints[4], &aRotated[2], Rot);
				Rotate(&lQuads[j].m_aPoints[4], &aRotated[3], Rot);
			}

			//Set Texture Coords
			Graphics()->QuadsSetSubsetFree(
				fx2f(lQuads[j].m_aTexcoords[0].x), fx2f(lQuads[j].m_aTexcoords[0].y),
				fx2f(lQuads[j].m_aTexcoords[1].x), fx2f(lQuads[j].m_aTexcoords[1].y),
				fx2f(lQuads[j].m_aTexcoords[2].x), fx2f(lQuads[j].m_aTexcoords[2].y),
				fx2f(lQuads[j].m_aTexcoords[3].x), fx2f(lQuads[j].m_aTexcoords[3].y)
			);

			//Set Quad Coords & Draw
			IGraphics::CFreeformItem Freeform(
				fx2f(pPoints[0].x)+OffsetX, fx2f(pPoints[0].y)+OffsetY,
				fx2f(pPoints[1].x)+OffsetX, fx2f(pPoints[1].y)+OffsetY,
				fx2f(pPoints[2].x)+OffsetX, fx2f(pPoints[2].y)+OffsetY,
				fx2f(pPoints[3].x)+OffsetX, fx2f(pPoints[3].y)+OffsetY);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}
	}
	Graphics()->QuadsEnd();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();

	// Draw QuadPoints
	for(int j = 0; j < Num; j++)
	{
		if(!apEnvelope[j])
			continue;

		//QuadParams
		for(int i = 0; i < apEnvelope[j]->m_lPoints.size()-1; i++)
			DoQuadEnvPoint(&lQuads[j], j, i);
	}
	Graphics()->QuadsEnd();
	delete[] apEnvelope;
}

void CEditor::DoQuadEnvPoint(const CQuad *pQuad, int QIndex, int PIndex)
{
	enum
	{
		OP_NONE=0,
		OP_MOVE,
		OP_ROTATE,
	};

	// some basic values
	static float s_LastWx;
	static float s_LastWy;
	static int s_Operation = OP_NONE;
	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();
	CEnvelope *pEnvelope = m_Map.m_lEnvelopes[pQuad->m_PosEnv];
	void *pID = &pEnvelope->m_lPoints[PIndex];
	static int s_ActQIndex = -1;

	// get pivot
	float CenterX = fx2f(pQuad->m_aPoints[4].x)+fx2f(pEnvelope->m_lPoints[PIndex].m_aValues[0]);
	float CenterY = fx2f(pQuad->m_aPoints[4].y)+fx2f(pEnvelope->m_lPoints[PIndex].m_aValues[1]);

	float dx = (CenterX - wx)/m_WorldZoom;
	float dy = (CenterY - wy)/m_WorldZoom;
	if(dx*dx+dy*dy < 50.0f && UI()->ActiveItem() == 0)
	{
		UI()->SetHotItem(pID);
		s_ActQIndex = QIndex;
	}

	bool IgnoreGrid;
	if(Input()->KeyIsPressed(KEY_LALT) || Input()->KeyIsPressed(KEY_RALT))
		IgnoreGrid = true;
	else
		IgnoreGrid = false;

	if(UI()->ActiveItem() == pID && s_ActQIndex == QIndex)
	{
		if(s_Operation == OP_MOVE)
		{
			if(m_GridActive && !IgnoreGrid)
			{
				int LineDistance = GetLineDistance();

				float x = 0.0f;
				float y = 0.0f;
				if(wx >= 0)
					x = (int)((wx+(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);
				else
					x = (int)((wx-(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);
				if(wy >= 0)
					y = (int)((wy+(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);
				else
					y = (int)((wy-(LineDistance/2)*m_GridFactor)/(LineDistance*m_GridFactor)) * (LineDistance*m_GridFactor);

				pEnvelope->m_lPoints[PIndex].m_aValues[0] = f2fx(x)-pQuad->m_aPoints[4].x;
				pEnvelope->m_lPoints[PIndex].m_aValues[1] = f2fx(y)-pQuad->m_aPoints[4].y;
			}
			else
			{
				pEnvelope->m_lPoints[PIndex].m_aValues[0] += f2fx(wx-s_LastWx);
				pEnvelope->m_lPoints[PIndex].m_aValues[1] += f2fx(wy-s_LastWy);
			}
		}
		else if(s_Operation == OP_ROTATE)
			pEnvelope->m_lPoints[PIndex].m_aValues[2] += 10*m_MouseDeltaX;

		s_LastWx = wx;
		s_LastWy = wy;

		if(!UI()->MouseButton(0))
		{
			m_LockMouse = false;
			s_Operation = OP_NONE;
			UI()->SetActiveItem(0);
		}

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	else if(UI()->HotItem() == pID && s_ActQIndex == QIndex)
	{
		ms_pUiGotContext = pID;

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		m_pTooltip = "Left mouse button to move. Hold ctrl to rotate. Hold alt to ignore grid.";

		if(UI()->MouseButton(0))
		{
			if(Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL))
			{
				m_LockMouse = true;
				s_Operation = OP_ROTATE;
			}
			else
				s_Operation = OP_MOVE;

			m_SelectedEnvelopePoint = PIndex;
			m_SelectedQuadEnvelope = pQuad->m_PosEnv;

			UI()->SetActiveItem(pID);
			if(m_SelectedQuad != QIndex)
				m_SelectedPoints = 0;
			m_SelectedQuad = QIndex;
			s_LastWx = wx;
			s_LastWy = wy;
		}
		else
		{
			m_SelectedEnvelopePoint = -1;
			m_SelectedQuadEnvelope = -1;
		}
	}
	else
		Graphics()->SetColor(0.0f, 1.0f, 0.0f, 1.0f);

	IGraphics::CQuadItem QuadItem(CenterX, CenterY, 5.0f*m_WorldZoom, 5.0f*m_WorldZoom);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CEditor::DoMapEditor(CUIRect View, CUIRect ToolBar)
{
	// render all good stuff
	if(!m_ShowPicker)
	{
		for(int g = 0; g < m_Map.m_lGroups.size(); g++)
		{// don't render the front, tele, speedup and switch layer now we will do it later to make them on top of others
			if(
					m_Map.m_lGroups[g] == (CLayerGroup *)m_Map.m_pFrontLayer ||
					m_Map.m_lGroups[g] == (CLayerGroup *)m_Map.m_pTeleLayer ||
					m_Map.m_lGroups[g] == (CLayerGroup *)m_Map.m_pSpeedupLayer ||
					m_Map.m_lGroups[g] == (CLayerGroup *)m_Map.m_pSwitchLayer ||
					m_Map.m_lGroups[g] == (CLayerGroup *)m_Map.m_pTuneLayer
					)
				continue;
			if(m_Map.m_lGroups[g]->m_Visible)
				m_Map.m_lGroups[g]->Render();
			//UI()->ClipEnable(&view);
		}

		// render the game, tele, speedup, front, tune and switch above everything else
		if(m_Map.m_pGameGroup->m_Visible)
		{
			m_Map.m_pGameGroup->MapScreen();
			for(int i = 0; i < m_Map.m_pGameGroup->m_lLayers.size(); i++)
			{
				if
				(
						m_Map.m_pGameGroup->m_lLayers[i]->m_Visible &&
						(
								m_Map.m_pGameGroup->m_lLayers[i] == m_Map.m_pGameLayer ||
								m_Map.m_pGameGroup->m_lLayers[i] == m_Map.m_pFrontLayer ||
								m_Map.m_pGameGroup->m_lLayers[i] == m_Map.m_pTeleLayer ||
								m_Map.m_pGameGroup->m_lLayers[i] == m_Map.m_pSpeedupLayer ||
								m_Map.m_pGameGroup->m_lLayers[i] == m_Map.m_pSwitchLayer ||
								m_Map.m_pGameGroup->m_lLayers[i] == m_Map.m_pTuneLayer
								)
								)
					m_Map.m_pGameGroup->m_lLayers[i]->Render();
			}
		}

		CLayerTiles *pT = static_cast<CLayerTiles *>(GetSelectedLayerType(0, LAYERTYPE_TILES));
		if(m_ShowTileInfo && pT && pT->m_Visible && m_ZoomLevel <= 300)
		{
			GetSelectedGroup()->MapScreen();
			pT->ShowInfo();
		}
	}
	else
	{
		// fix aspect ratio of the image in the picker
		float Max = min(View.w, View.h);
		View.w = View.h = Max;
	}

	static void *s_pEditorID = (void *)&s_pEditorID;
	int Inside = UI()->MouseInside(&View);

	// fetch mouse position
	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();
	float mx = UI()->MouseX();
	float my = UI()->MouseY();

	static float s_StartWx = 0;
	static float s_StartWy = 0;

	enum
	{
		OP_NONE=0,
		OP_BRUSH_GRAB,
		OP_BRUSH_DRAW,
		OP_BRUSH_PAINT,
		OP_PAN_WORLD,
		OP_PAN_EDITOR,
	};

	// remap the screen so it can display the whole tileset
	if(m_ShowPicker)
	{
		CUIRect Screen = *UI()->Screen();
		float Size = 32.0*16.0f;
		float w = Size*(Screen.w/View.w);
		float h = Size*(Screen.h/View.h);
		float x = -(View.x/Screen.w)*w;
		float y = -(View.y/Screen.h)*h;
		wx = x+w*mx/Screen.w;
		wy = y+h*my/Screen.h;
		CLayerTiles *t = (CLayerTiles *)GetSelectedLayerType(0, LAYERTYPE_TILES);
		if(t)
		{
			Graphics()->MapScreen(x, y, x+w, y+h);
			m_TilesetPicker.m_Image = t->m_Image;
			m_TilesetPicker.m_TexID = t->m_TexID;
			m_TilesetPicker.Render();
			if(m_ShowTileInfo)
				m_TilesetPicker.ShowInfo();
		}
		else
		{
			CLayerQuads *t = (CLayerQuads *)GetSelectedLayerType(0, LAYERTYPE_QUADS);
			if(t)
			{
				m_QuadsetPicker.m_Image = t->m_Image;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[0].x = (int)View.x << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[0].y = (int)View.y << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[1].x = (int)(View.x+View.w) << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[1].y = (int)View.y << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[2].x = (int)View.x << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[2].y = (int)(View.y+View.h) << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[3].x = (int)(View.x+View.w) << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[3].y = (int)(View.y+View.h) << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[4].x = (int)(View.x+View.w/2) << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[4].y = (int)(View.y+View.h/2) << 10;
				m_QuadsetPicker.Render();
			}
		}
	}

	static int s_Operation = OP_NONE;

	// draw layer borders
	CLayer *pEditLayers[16];
	int NumEditLayers = 0;
	NumEditLayers = 0;

	if(m_ShowPicker && GetSelectedLayer(0) && GetSelectedLayer(0)->m_Type == LAYERTYPE_TILES)
	{
		pEditLayers[0] = &m_TilesetPicker;
		NumEditLayers++;
	}
	else if (m_ShowPicker)
	{
		pEditLayers[0] = &m_QuadsetPicker;
		NumEditLayers++;
	}
	else
	{
		pEditLayers[0] = GetSelectedLayer(0);
		if(pEditLayers[0])
			NumEditLayers++;

		CLayerGroup *g = GetSelectedGroup();
		if(g)
		{
			g->MapScreen();

			RenderGrid(g);

			for(int i = 0; i < NumEditLayers; i++)
			{
				if(pEditLayers[i]->m_Type != LAYERTYPE_TILES)
					continue;

				float w, h;
				pEditLayers[i]->GetSize(&w, &h);

				IGraphics::CLineItem Array[4] = {
					IGraphics::CLineItem(0, 0, w, 0),
					IGraphics::CLineItem(w, 0, w, h),
					IGraphics::CLineItem(w, h, 0, h),
					IGraphics::CLineItem(0, h, 0, 0)};
				Graphics()->TextureSet(-1);
				Graphics()->LinesBegin();
				Graphics()->LinesDraw(Array, 4);
				Graphics()->LinesEnd();
			}
		}
	}

	if(Inside)
	{
		UI()->SetHotItem(s_pEditorID);

		// do global operations like pan and zoom
		if(UI()->ActiveItem() == 0 && (UI()->MouseButton(0) || UI()->MouseButton(2)))
		{
			s_StartWx = wx;
			s_StartWy = wy;

			if(Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL) || UI()->MouseButton(2))
			{
				if(Input()->KeyIsPressed(KEY_LSHIFT))
					s_Operation = OP_PAN_EDITOR;
				else
					s_Operation = OP_PAN_WORLD;
				UI()->SetActiveItem(s_pEditorID);
			}
			else
				s_Operation = OP_NONE;
		}

		// brush editing
		if(UI()->HotItem() == s_pEditorID)
		{
			if(m_Brush.IsEmpty())
				m_pTooltip = "Use left mouse button to drag and create a brush.";
			else
				m_pTooltip = "Use left mouse button to paint with the brush. Right button clears the brush.";

			if(UI()->ActiveItem() == s_pEditorID)
			{
				CUIRect r;
				r.x = s_StartWx;
				r.y = s_StartWy;
				r.w = wx-s_StartWx;
				r.h = wy-s_StartWy;
				if(r.w < 0)
				{
					r.x += r.w;
					r.w = -r.w;
				}

				if(r.h < 0)
				{
					r.y += r.h;
					r.h = -r.h;
				}

				if(s_Operation == OP_BRUSH_DRAW)
				{
					if(!m_Brush.IsEmpty())
					{
						// draw with brush
						for(int k = 0; k < NumEditLayers; k++)
						{
							if(pEditLayers[k]->m_Type == m_Brush.m_lLayers[0]->m_Type)
								pEditLayers[k]->BrushDraw(m_Brush.m_lLayers[0], wx, wy);
						}
					}
				}
				else if(s_Operation == OP_BRUSH_GRAB)
				{
					if(!UI()->MouseButton(0))
					{
						// grab brush
						char aBuf[256];
						str_format(aBuf, sizeof(aBuf),"grabbing %f %f %f %f", r.x, r.y, r.w, r.h);
						Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "editor", aBuf);

						// TODO: do all layers
						int Grabs = 0;
						for(int k = 0; k < NumEditLayers; k++)
							Grabs += pEditLayers[k]->BrushGrab(&m_Brush, r);
						if(Grabs == 0)
							m_Brush.Clear();
					}
					else
					{
						//editor.map.groups[selected_group]->mapscreen();
						for(int k = 0; k < NumEditLayers; k++)
							pEditLayers[k]->BrushSelecting(r);
						Graphics()->MapScreen(UI()->Screen()->x, UI()->Screen()->y, UI()->Screen()->w, UI()->Screen()->h);
					}
				}
				else if(s_Operation == OP_BRUSH_PAINT)
				{
					if(!UI()->MouseButton(0))
					{
						for(int k = 0; k < NumEditLayers; k++)
							pEditLayers[k]->FillSelection(m_Brush.IsEmpty(), m_Brush.m_lLayers[0], r);
					}
					else
					{
						//editor.map.groups[selected_group]->mapscreen();
						for(int k = 0; k < NumEditLayers; k++)
							pEditLayers[k]->BrushSelecting(r);
						Graphics()->MapScreen(UI()->Screen()->x, UI()->Screen()->y, UI()->Screen()->w, UI()->Screen()->h);
					}
				}
			}
			else
			{
				if(UI()->MouseButton(1))
					m_Brush.Clear();

				if(UI()->MouseButton(0) && s_Operation == OP_NONE)
				{
					UI()->SetActiveItem(s_pEditorID);

					if(m_Brush.IsEmpty())
						s_Operation = OP_BRUSH_GRAB;
					else
					{
						s_Operation = OP_BRUSH_DRAW;
						for(int k = 0; k < NumEditLayers; k++)
						{
							if(pEditLayers[k]->m_Type == m_Brush.m_lLayers[0]->m_Type)
								pEditLayers[k]->BrushPlace(m_Brush.m_lLayers[0], wx, wy);
						}

					}

					CLayerTiles *pLayer = (CLayerTiles*)GetSelectedLayerType(0, LAYERTYPE_TILES);
					if((Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT)) && pLayer)
						s_Operation = OP_BRUSH_PAINT;
				}

				if(!m_Brush.IsEmpty())
				{
					m_Brush.m_OffsetX = -(int)wx;
					m_Brush.m_OffsetY = -(int)wy;
					for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
					{
						if(m_Brush.m_lLayers[i]->m_Type == LAYERTYPE_TILES)
						{
							m_Brush.m_OffsetX = -(int)(wx/32.0f)*32;
							m_Brush.m_OffsetY = -(int)(wy/32.0f)*32;
							break;
						}
					}

					CLayerGroup *g = GetSelectedGroup();
					if(!m_ShowPicker && g)
					{
						m_Brush.m_OffsetX += g->m_OffsetX;
						m_Brush.m_OffsetY += g->m_OffsetY;
						m_Brush.m_ParallaxX = g->m_ParallaxX;
						m_Brush.m_ParallaxY = g->m_ParallaxY;
						m_Brush.Render();
						float w, h;
						m_Brush.GetSize(&w, &h);

						IGraphics::CLineItem Array[4] = {
							IGraphics::CLineItem(0, 0, w, 0),
							IGraphics::CLineItem(w, 0, w, h),
							IGraphics::CLineItem(w, h, 0, h),
							IGraphics::CLineItem(0, h, 0, 0)};
						Graphics()->TextureSet(-1);
						Graphics()->LinesBegin();
						Graphics()->LinesDraw(Array, 4);
						Graphics()->LinesEnd();
					}
				}
			}
		}

		// quad & sound editing
		{
			if(!m_ShowPicker && m_Brush.IsEmpty())
			{
				// fetch layers
				CLayerGroup *g = GetSelectedGroup();
				if(g)
					g->MapScreen();

				for(int k = 0; k < NumEditLayers; k++)
				{
					if(pEditLayers[k]->m_Type == LAYERTYPE_QUADS)
					{
						CLayerQuads *pLayer = (CLayerQuads *)pEditLayers[k];

						if(!m_ShowEnvelopePreview)
							m_ShowEnvelopePreview = 2;

						Graphics()->TextureSet(-1);
						Graphics()->QuadsBegin();
						for(int i = 0; i < pLayer->m_lQuads.size(); i++)
						{
							for(int v = 0; v < 4; v++)
								DoQuadPoint(&pLayer->m_lQuads[i], i, v);

							DoQuad(&pLayer->m_lQuads[i], i);
						}
						Graphics()->QuadsEnd();
					}

					if(pEditLayers[k]->m_Type == LAYERTYPE_SOUNDS)
					{
						CLayerSounds *pLayer = (CLayerSounds *)pEditLayers[k];

						Graphics()->TextureSet(-1);
						Graphics()->QuadsBegin();
						for(int i = 0; i < pLayer->m_lSources.size(); i++)
						{
							DoSoundSource(&pLayer->m_lSources[i], i);
						}
						Graphics()->QuadsEnd();
					}
				}

				Graphics()->MapScreen(UI()->Screen()->x, UI()->Screen()->y, UI()->Screen()->w, UI()->Screen()->h);
			}
		}

		// do panning
		if(UI()->ActiveItem() == s_pEditorID)
		{
			if(s_Operation == OP_PAN_WORLD)
			{
				m_WorldOffsetX -= m_MouseDeltaX*m_WorldZoom;
				m_WorldOffsetY -= m_MouseDeltaY*m_WorldZoom;
			}
			else if(s_Operation == OP_PAN_EDITOR)
			{
				m_EditorOffsetX -= m_MouseDeltaX*m_WorldZoom;
				m_EditorOffsetY -= m_MouseDeltaY*m_WorldZoom;
			}

			// release mouse
			if(!UI()->MouseButton(0))
			{
				if(s_Operation == OP_BRUSH_DRAW || s_Operation == OP_BRUSH_PAINT)
					m_Map.m_UndoModified++;

				s_Operation = OP_NONE;
				UI()->SetActiveItem(0);
			}
		}


	}
	else if(UI()->ActiveItem() == s_pEditorID)
	{
		// release mouse
		if(!UI()->MouseButton(0))
		{
			s_Operation = OP_NONE;
			UI()->SetActiveItem(0);
		}
	}

	if(!m_ShowPicker && GetSelectedGroup() && GetSelectedGroup()->m_UseClipping)
	{
		CLayerGroup *g = m_Map.m_pGameGroup;
		g->MapScreen();

		Graphics()->TextureSet(-1);
		Graphics()->LinesBegin();

			CUIRect r;
			r.x = GetSelectedGroup()->m_ClipX;
			r.y = GetSelectedGroup()->m_ClipY;
			r.w = GetSelectedGroup()->m_ClipW;
			r.h = GetSelectedGroup()->m_ClipH;

			IGraphics::CLineItem Array[4] = {
				IGraphics::CLineItem(r.x, r.y, r.x+r.w, r.y),
				IGraphics::CLineItem(r.x+r.w, r.y, r.x+r.w, r.y+r.h),
				IGraphics::CLineItem(r.x+r.w, r.y+r.h, r.x, r.y+r.h),
				IGraphics::CLineItem(r.x, r.y+r.h, r.x, r.y)};
			Graphics()->SetColor(1,0,0,1);
			Graphics()->LinesDraw(Array, 4);

		Graphics()->LinesEnd();
	}

	// render screen sizes
	if(m_ProofBorders && !m_ShowPicker)
	{
		CLayerGroup *g = m_Map.m_pGameGroup;
		g->MapScreen();

		Graphics()->TextureSet(-1);
		Graphics()->LinesBegin();

		// possible screen sizes (white border)
		float aLastPoints[4];
		float Start = 1.0f; //9.0f/16.0f;
		float End = 16.0f/9.0f;
		const int NumSteps = 20;
		for(int i = 0; i <= NumSteps; i++)
		{
			float aPoints[4];
			float Aspect = Start + (End-Start)*(i/(float)NumSteps);

			RenderTools()->MapscreenToWorld(
				m_WorldOffsetX, m_WorldOffsetY,
				1.0f, 1.0f, 0.0f, 0.0f, Aspect, 1.0f, aPoints);

			if(i == 0)
			{
				IGraphics::CLineItem Array[2] = {
					IGraphics::CLineItem(aPoints[0], aPoints[1], aPoints[2], aPoints[1]),
					IGraphics::CLineItem(aPoints[0], aPoints[3], aPoints[2], aPoints[3])};
				Graphics()->LinesDraw(Array, 2);
			}

			if(i != 0)
			{
				IGraphics::CLineItem Array[4] = {
					IGraphics::CLineItem(aPoints[0], aPoints[1], aLastPoints[0], aLastPoints[1]),
					IGraphics::CLineItem(aPoints[2], aPoints[1], aLastPoints[2], aLastPoints[1]),
					IGraphics::CLineItem(aPoints[0], aPoints[3], aLastPoints[0], aLastPoints[3]),
					IGraphics::CLineItem(aPoints[2], aPoints[3], aLastPoints[2], aLastPoints[3])};
				Graphics()->LinesDraw(Array, 4);
			}

			if(i == NumSteps)
			{
				IGraphics::CLineItem Array[2] = {
					IGraphics::CLineItem(aPoints[0], aPoints[1], aPoints[0], aPoints[3]),
					IGraphics::CLineItem(aPoints[2], aPoints[1], aPoints[2], aPoints[3])};
				Graphics()->LinesDraw(Array, 2);
			}

			mem_copy(aLastPoints, aPoints, sizeof(aPoints));
		}

		// two screen sizes (green and red border)
		{
			Graphics()->SetColor(1,0,0,1);
			for(int i = 0; i < 2; i++)
			{
				float aPoints[4];
				float aAspects[] = {4.0f/3.0f, 16.0f/10.0f, 5.0f/4.0f, 16.0f/9.0f};
				float Aspect = aAspects[i];

				RenderTools()->MapscreenToWorld(
					m_WorldOffsetX, m_WorldOffsetY,
					1.0f, 1.0f, 0.0f, 0.0f, Aspect, 1.0f, aPoints);

				CUIRect r;
				r.x = aPoints[0];
				r.y = aPoints[1];
				r.w = aPoints[2]-aPoints[0];
				r.h = aPoints[3]-aPoints[1];

				IGraphics::CLineItem Array[4] = {
					IGraphics::CLineItem(r.x, r.y, r.x+r.w, r.y),
					IGraphics::CLineItem(r.x+r.w, r.y, r.x+r.w, r.y+r.h),
					IGraphics::CLineItem(r.x+r.w, r.y+r.h, r.x, r.y+r.h),
					IGraphics::CLineItem(r.x, r.y+r.h, r.x, r.y)};
				Graphics()->LinesDraw(Array, 4);
				Graphics()->SetColor(0,1,0,1);
			}
		}
		Graphics()->LinesEnd();

		// tee position (blue circle)
		{
			Graphics()->TextureSet(-1);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0,0,1,0.3f);
			RenderTools()->DrawCircle(m_WorldOffsetX, m_WorldOffsetY, 20.0f, 32);
			Graphics()->QuadsEnd();
		}
	}

	if (!m_ShowPicker && m_ShowTileInfo && m_ShowEnvelopePreview != 0 && GetSelectedLayer(0) && GetSelectedLayer(0)->m_Type == LAYERTYPE_QUADS)
	{
		GetSelectedGroup()->MapScreen();

		CLayerQuads *pLayer = (CLayerQuads*)GetSelectedLayer(0);
		int TexID = -1;
		if(pLayer->m_Image >= 0 && pLayer->m_Image < m_Map.m_lImages.size())
			TexID = m_Map.m_lImages[pLayer->m_Image]->m_TexID;

		DoQuadEnvelopes(pLayer->m_lQuads, TexID);
		m_ShowEnvelopePreview = 0;
	}

	Graphics()->MapScreen(UI()->Screen()->x, UI()->Screen()->y, UI()->Screen()->w, UI()->Screen()->h);
	//UI()->ClipDisable();
}


int CEditor::DoProperties(CUIRect *pToolBox, CProperty *pProps, int *pIDs, int *pNewVal, vec4 Color)
{
	int Change = -1;

	for(int i = 0; pProps[i].m_pName; i++)
	{
		CUIRect Slot;
		pToolBox->HSplitTop(13.0f, &Slot, pToolBox);
		CUIRect Label, Shifter;
		Slot.VSplitMid(&Label, &Shifter);
		Shifter.HMargin(1.0f, &Shifter);
		UI()->DoLabel(&Label, pProps[i].m_pName, 10.0f, -1, -1);

		if(pProps[i].m_Type == PROPTYPE_INT_STEP)
		{
			CUIRect Inc, Dec;
			char aBuf[64];

			Shifter.VSplitRight(10.0f, &Shifter, &Inc);
			Shifter.VSplitLeft(10.0f, &Dec, &Shifter);
			str_format(aBuf, sizeof(aBuf),"%d", pProps[i].m_Value);
			int NewValue = UiDoValueSelector((char *) &pIDs[i], &Shifter, "", pProps[i].m_Value, pProps[i].m_Min, pProps[i].m_Max, 1, 1.0f, "Use left mouse button to drag and change the value. Hold shift to be more precise. Rightclick to edit as text.", false, false, 0, &Color);
			if (NewValue != pProps[i].m_Value)
			{
				*pNewVal = NewValue;
				Change = i;
			}
			if(DoButton_ButtonDec((char *) &pIDs[i] +1, 0, 0, &Dec, 0, "Decrease"))
			{
				*pNewVal = pProps[i].m_Value-1;
				Change = i;
			}
			if(DoButton_ButtonInc(((char *)&pIDs[i])+2, 0, 0, &Inc, 0, "Increase"))
			{
				*pNewVal = pProps[i].m_Value+1;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_BOOL)
		{
			CUIRect No, Yes;
			Shifter.VSplitMid(&No, &Yes);
			if(DoButton_ButtonDec(&pIDs[i], "No", !pProps[i].m_Value, &No, 0, ""))
			{
				*pNewVal = 0;
				Change = i;
			}
			if(DoButton_ButtonInc(((char *)&pIDs[i])+1, "Yes", pProps[i].m_Value, &Yes, 0, ""))
			{
				*pNewVal = 1;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_INT_SCROLL)
		{
			int NewValue = UiDoValueSelector(&pIDs[i], &Shifter, "", pProps[i].m_Value, pProps[i].m_Min, pProps[i].m_Max, 1, 1.0f, "Use left mouse button to drag and change the value. Hold shift to be more precise. Rightclick to edit as text.");
			if(NewValue != pProps[i].m_Value)
			{
				*pNewVal = NewValue;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_ANGLE_SCROLL)
		{
			CUIRect Inc, Dec;
			Shifter.VSplitRight(10.0f, &Shifter, &Inc);
			Shifter.VSplitLeft(10.0f, &Dec, &Shifter);
			bool Shift = Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT);
			int Value = pProps[i].m_Value;
			const void *activeItem = UI()->ActiveItem();
			if (!Shift && UI()->MouseButton(0) && (activeItem == &pIDs[i] || activeItem == &pIDs[i]+1 || activeItem == &pIDs[i]+2))
				Value = (Value / 45) * 45;
			int NewValue = UiDoValueSelector(&pIDs[i], &Shifter, "", Value, pProps[i].m_Min, Shift ? pProps[i].m_Max : 315, Shift ? 1 : 45, Shift ? 1.0f : 10.0f, "Use left mouse button to drag and change the value. Hold shift to be more precise. Rightclick to edit as text.", false, false, 0);
			if(NewValue != pProps[i].m_Value)
			{
				*pNewVal = NewValue;
				Change = i;
			}
			if (DoButton_ButtonDec(&pIDs[i]+1, 0, 0, &Dec, 0, "Decrease"))
			{
				if (!Shift)
					*pNewVal = pProps[i].m_Value - 45;
				else
					*pNewVal = pProps[i].m_Value - 1;
				Change = i;
			}
			if (DoButton_ButtonInc(&pIDs[i]+ 2, 0, 0, &Inc, 0, "Increase"))
			{
				if (!Shift)
					*pNewVal = pProps[i].m_Value + 45;
				else
					*pNewVal = pProps[i].m_Value + 1;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_COLOR)
		{
			static const char *s_paTexts[4] = {"R", "G", "B", "A"};
			static int s_aShift[] = {24, 16, 8, 0};
			int NewColor = 0;

			// extra space
			CUIRect ColorBox, ColorSlots;

			pToolBox->HSplitTop(3.0f*13.0f, &Slot, pToolBox);
			Slot.VSplitMid(&ColorBox, &ColorSlots);
			ColorBox.HMargin(1.0f, &ColorBox);
			ColorBox.VMargin(6.0f, &ColorBox);

			for(int c = 0; c < 4; c++)
			{
				int v = (pProps[i].m_Value >> s_aShift[c])&0xff;
				NewColor |= UiDoValueSelector(((char *)&pIDs[i])+c, &Shifter, s_paTexts[c], v, 0, 255, 1, 1.0f, "Use left mouse button to drag and change the color value. Hold shift to be more precise. Rightclick to edit as text.")<<s_aShift[c];

				if(c != 3)
				{
					ColorSlots.HSplitTop(13.0f, &Shifter, &ColorSlots);
					Shifter.HMargin(1.0f, &Shifter);
				}
			}

			// hex
			pToolBox->HSplitTop(13.0f, &Slot, pToolBox);
			Slot.VSplitMid(0x0, &Shifter);
			Shifter.HMargin(1.0f, &Shifter);

			int NewColorHex = pProps[i].m_Value&0xff;
			NewColorHex |= UiDoValueSelector(((char *)&pIDs[i]-1), &Shifter, "", (pProps[i].m_Value >> 8)&0xFFFFFF, 0, 0xFFFFFF, 1, 1.0f, "Use left mouse button to drag and change the color value. Hold shift to be more precise. Rightclick to edit as text.", false, true) << 8;

			// color picker
			vec4 Color = vec4(
				((pProps[i].m_Value >> s_aShift[0])&0xff)/255.0f,
				((pProps[i].m_Value >> s_aShift[1])&0xff)/255.0f,
				((pProps[i].m_Value >> s_aShift[2])&0xff)/255.0f,
				1.0f);

			static int s_ColorPicker, s_ColorPickerID;
			if(DoButton_ColorPicker(&s_ColorPicker, &ColorBox, &Color))
			{
				ms_PickerColor = RgbToHsv(vec3(Color.r, Color.g, Color.b));
				UiInvokePopupMenu(&s_ColorPickerID, 0, UI()->MouseX(), UI()->MouseY(), 180, 150, PopupColorPicker);
			}

			if(UI()->HotItem() == &ms_SVPicker || UI()->HotItem() == &ms_HuePicker)
			{
				vec3 c = HsvToRgb(ms_PickerColor);
				NewColor = ((int)(c.r * 255.0f)&0xff) << 24 | ((int)(c.g * 255.0f)&0xff) << 16 | ((int)(c.b * 255.0f)&0xff) << 8 | (pProps[i].m_Value&0xff);
			}

			//
			if(NewColor != pProps[i].m_Value)
			{
				*pNewVal = NewColor;
				Change = i;
			}
			else if(NewColorHex != pProps[i].m_Value)
			{
				*pNewVal = NewColorHex;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_IMAGE)
		{
			char aBuf[64];
			if(pProps[i].m_Value < 0)
				str_copy(aBuf, "None", sizeof(aBuf));
			else
				str_format(aBuf, sizeof(aBuf),"%s", m_Map.m_lImages[pProps[i].m_Value]->m_aName);

			if(DoButton_Editor(&pIDs[i], aBuf, 0, &Shifter, 0, 0))
				PopupSelectImageInvoke(pProps[i].m_Value, UI()->MouseX(), UI()->MouseY());

			int r = PopupSelectImageResult();
			if(r >= -1)
			{
				*pNewVal = r;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_SHIFT)
		{
			CUIRect Left, Right, Up, Down;
			Shifter.VSplitMid(&Left, &Up);
			Left.VSplitRight(1.0f, &Left, 0);
			Up.VSplitLeft(1.0f, 0, &Up);
			Left.VSplitLeft(10.0f, &Left, &Shifter);
			Shifter.VSplitRight(10.0f, &Shifter, &Right);
			RenderTools()->DrawUIRect(&Shifter, vec4(1,1,1,0.5f), 0, 0.0f);
			UI()->DoLabel(&Shifter, "X", 10.0f, 0, -1);
			Up.VSplitLeft(10.0f, &Up, &Shifter);
			Shifter.VSplitRight(10.0f, &Shifter, &Down);
			RenderTools()->DrawUIRect(&Shifter, vec4(1,1,1,0.5f), 0, 0.0f);
			UI()->DoLabel(&Shifter, "Y", 10.0f, 0, -1);
			if(DoButton_ButtonDec(&pIDs[i], "-", 0, &Left, 0, "Left"))
			{
				*pNewVal = 1;
				Change = i;
			}
			if(DoButton_ButtonInc(((char *)&pIDs[i])+3, "+", 0, &Right, 0, "Right"))
			{
				*pNewVal = 2;
				Change = i;
			}
			if(DoButton_ButtonDec(((char *)&pIDs[i])+1, "-", 0, &Up, 0, "Up"))
			{
				*pNewVal = 4;
				Change = i;
			}
			if(DoButton_ButtonInc(((char *)&pIDs[i])+2, "+", 0, &Down, 0, "Down"))
			{
				*pNewVal = 8;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_SOUND)
		{
			char aBuf[64];
			if(pProps[i].m_Value < 0)
				str_copy(aBuf, "None", sizeof(aBuf));
			else
				str_format(aBuf, sizeof(aBuf),"%s", m_Map.m_lSounds[pProps[i].m_Value]->m_aName);

			if(DoButton_Editor(&pIDs[i], aBuf, 0, &Shifter, 0, 0))
				PopupSelectSoundInvoke(pProps[i].m_Value, UI()->MouseX(), UI()->MouseY());

			int r = PopupSelectSoundResult();
			if(r >= -1)
			{
				*pNewVal = r;
				Change = i;
			}
		}
	}

	return Change;
}

void CEditor::RenderLayers(CUIRect ToolBox, CUIRect ToolBar, CUIRect View)
{
	CUIRect LayersBox = ToolBox;

	if(!m_GuiActive)
		return;

	CUIRect Slot, Button;
	char aBuf[64];

	float LayersHeight = 12.0f;	 // Height of AddGroup button
	static int s_ScrollBar = 0;
	static float s_ScrollValue = 0;

	for(int g = 0; g < m_Map.m_lGroups.size(); g++)
	{
		// Each group is 19.0f
		// Each layer is 14.0f
		LayersHeight += 19.0f;
		if(!m_Map.m_lGroups[g]->m_Collapse)
			LayersHeight += m_Map.m_lGroups[g]->m_lLayers.size() * 14.0f;
	}

	float ScrollDifference = LayersHeight - LayersBox.h;

	if(LayersHeight > LayersBox.h)	// Do we even need a scrollbar?
	{
		CUIRect Scroll;
		LayersBox.VSplitRight(15.0f, &LayersBox, &Scroll);
		LayersBox.VSplitRight(3.0f, &LayersBox, 0);	// extra spacing
		Scroll.HMargin(5.0f, &Scroll);
		s_ScrollValue = UiDoScrollbarV(&s_ScrollBar, &Scroll, s_ScrollValue);

		if(UI()->MouseInside(&Scroll) || UI()->MouseInside(&LayersBox))
		{
			int ScrollNum = (int)((LayersHeight-LayersBox.h)/15.0f)+1;
			if(ScrollNum > 0)
			{
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
					s_ScrollValue = clamp(s_ScrollValue - 1.0f/ScrollNum, 0.0f, 1.0f);
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
					s_ScrollValue = clamp(s_ScrollValue + 1.0f/ScrollNum, 0.0f, 1.0f);
			}
		}
	}

	float LayerStartAt = ScrollDifference * s_ScrollValue;
	if(LayerStartAt < 0.0f)
		LayerStartAt = 0.0f;

	float LayerStopAt = LayersHeight - ScrollDifference * (1 - s_ScrollValue);
	float LayerCur = 0;

	// render layers
	{
		for(int g = 0; g < m_Map.m_lGroups.size(); g++)
		{
			if(LayerCur > LayerStopAt)
				break;
			else if(LayerCur + m_Map.m_lGroups[g]->m_lLayers.size() * 14.0f + 19.0f < LayerStartAt)
			{
				LayerCur += m_Map.m_lGroups[g]->m_lLayers.size() * 14.0f + 19.0f;
				continue;
			}

			CUIRect VisibleToggle, SaveCheck;
			if(LayerCur >= LayerStartAt)
			{
				LayersBox.HSplitTop(12.0f, &Slot, &LayersBox);
				Slot.VSplitLeft(12, &VisibleToggle, &Slot);
				if(DoButton_Ex(&m_Map.m_lGroups[g]->m_Visible, m_Map.m_lGroups[g]->m_Visible?"V":"H", m_Map.m_lGroups[g]->m_Collapse ? 1 : 0, &VisibleToggle, 0, "Toggle group visibility", CUI::CORNER_L))
					m_Map.m_lGroups[g]->m_Visible = !m_Map.m_lGroups[g]->m_Visible;

				Slot.VSplitRight(12.0f, &Slot, &SaveCheck);
				if(DoButton_Ex(&m_Map.m_lGroups[g]->m_SaveToMap, "S", m_Map.m_lGroups[g]->m_SaveToMap, &SaveCheck, 0, "Enable/disable group for saving", CUI::CORNER_R))
					if(!m_Map.m_lGroups[g]->m_GameGroup)
						m_Map.m_lGroups[g]->m_SaveToMap = !m_Map.m_lGroups[g]->m_SaveToMap;

				str_format(aBuf, sizeof(aBuf),"#%d %s", g, m_Map.m_lGroups[g]->m_aName);
				float FontSize = 10.0f;
				while(TextRender()->TextWidth(0, FontSize, aBuf, -1) > Slot.w)
					FontSize--;
				if(int Result = DoButton_Ex(&m_Map.m_lGroups[g], aBuf, g==m_SelectedGroup, &Slot,
					BUTTON_CONTEXT, m_Map.m_lGroups[g]->m_Collapse ? "Select group. Double click to expand." : "Select group. Double click to collapse.", 0, FontSize))
				{
					m_SelectedGroup = g;
					m_SelectedLayer = 0;

					static int s_GroupPopupId = 0;
					if(Result == 2)
						UiInvokePopupMenu(&s_GroupPopupId, 0, UI()->MouseX(), UI()->MouseY(), 145, 230, PopupGroup);

					if(m_Map.m_lGroups[g]->m_lLayers.size() && Input()->MouseDoubleClick())
						m_Map.m_lGroups[g]->m_Collapse ^= 1;
				}
				LayersBox.HSplitTop(2.0f, &Slot, &LayersBox);
			}
			LayerCur += 14.0f;

			for(int i = 0; i < m_Map.m_lGroups[g]->m_lLayers.size(); i++)
			{
				if(LayerCur > LayerStopAt)
					break;
				else if(LayerCur < LayerStartAt)
				{
					LayerCur += 14.0f;
					continue;
				}

				if(m_Map.m_lGroups[g]->m_Collapse)
					continue;

				//visible
				LayersBox.HSplitTop(12.0f, &Slot, &LayersBox);
				Slot.VSplitLeft(12.0f, 0, &Button);
				Button.VSplitLeft(15, &VisibleToggle, &Button);

				if(DoButton_Ex(&m_Map.m_lGroups[g]->m_lLayers[i]->m_Visible, m_Map.m_lGroups[g]->m_lLayers[i]->m_Visible?"V":"H", 0, &VisibleToggle, 0, "Toggle layer visibility", CUI::CORNER_L))
					m_Map.m_lGroups[g]->m_lLayers[i]->m_Visible = !m_Map.m_lGroups[g]->m_lLayers[i]->m_Visible;

				Button.VSplitRight(12.0f, &Button, &SaveCheck);
				if(DoButton_Ex(&m_Map.m_lGroups[g]->m_lLayers[i]->m_SaveToMap, "S", m_Map.m_lGroups[g]->m_lLayers[i]->m_SaveToMap, &SaveCheck, 0, "Enable/disable layer for saving", CUI::CORNER_R))
					if(m_Map.m_lGroups[g]->m_lLayers[i] != m_Map.m_pGameLayer)
						m_Map.m_lGroups[g]->m_lLayers[i]->m_SaveToMap = !m_Map.m_lGroups[g]->m_lLayers[i]->m_SaveToMap;

				if(m_Map.m_lGroups[g]->m_lLayers[i]->m_aName[0])
					str_format(aBuf, sizeof(aBuf), "%s", m_Map.m_lGroups[g]->m_lLayers[i]->m_aName);
				else if(m_Map.m_lGroups[g]->m_lLayers[i]->m_Type == LAYERTYPE_TILES)
					str_copy(aBuf, "Tiles", sizeof(aBuf));
				else
					str_copy(aBuf, "Quads", sizeof(aBuf));

				float FontSize = 10.0f;
				while(TextRender()->TextWidth(0, FontSize, aBuf, -1) > Button.w)
					FontSize--;
				int Checked = g == m_SelectedGroup && i == m_SelectedLayer;
				if(m_Map.m_lGroups[g]->m_lLayers[i] == m_Map.m_pGameLayer ||
					m_Map.m_lGroups[g]->m_lLayers[i] == m_Map.m_pFrontLayer ||
					m_Map.m_lGroups[g]->m_lLayers[i] == m_Map.m_pSwitchLayer ||
					m_Map.m_lGroups[g]->m_lLayers[i] == m_Map.m_pTuneLayer ||
					m_Map.m_lGroups[g]->m_lLayers[i] == m_Map.m_pSpeedupLayer ||
					m_Map.m_lGroups[g]->m_lLayers[i] == m_Map.m_pTeleLayer)
				{
					Checked += 6;
				}
				if(int Result = DoButton_Ex(m_Map.m_lGroups[g]->m_lLayers[i], aBuf, Checked, &Button,
					BUTTON_CONTEXT, "Select layer.", 0, FontSize))
				{
					m_SelectedLayer = i;
					m_SelectedGroup = g;
					static int s_LayerPopupID = 0;
					if(Result == 2)
						UiInvokePopupMenu(&s_LayerPopupID, 0, UI()->MouseX(), UI()->MouseY(), 120, 280, PopupLayer);
				}

				LayerCur += 14.0f;
				LayersBox.HSplitTop(2.0f, &Slot, &LayersBox);
			}
			if(LayerCur > LayerStartAt && LayerCur < LayerStopAt)
				LayersBox.HSplitTop(5.0f, &Slot, &LayersBox);
			LayerCur += 5.0f;
		}
	}

	if(LayerCur <= LayerStopAt)
	{
		LayersBox.HSplitTop(12.0f, &Slot, &LayersBox);

		static int s_NewGroupButton = 0;
		if(DoButton_Editor(&s_NewGroupButton, "Add group", 0, &Slot, 0, "Adds a new group"))
		{
			m_Map.NewGroup();
			m_SelectedGroup = m_Map.m_lGroups.size()-1;
		}
	}
}

void CEditor::ReplaceImage(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;
	CEditorImage ImgInfo(pEditor);
	if(!pEditor->Graphics()->LoadPNG(&ImgInfo, pFileName, StorageType))
		return;

	CEditorImage *pImg = pEditor->m_Map.m_lImages[pEditor->m_SelectedImage];
	int External = pImg->m_External;
	pEditor->Graphics()->UnloadTexture(pImg->m_TexID);
	if(pImg->m_pData)
	{
		mem_free(pImg->m_pData);
		pImg->m_pData = 0;
	}
	*pImg = ImgInfo;
	pImg->m_External = External;
	IStorage::StripPathAndExtension(pFileName, pImg->m_aName, sizeof(pImg->m_aName));
	pImg->m_AutoMapper.Load(pImg->m_aName);
	pImg->m_TexID = pEditor->Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, CImageInfo::FORMAT_AUTO, 0);
	ImgInfo.m_pData = 0;
	pEditor->SortImages();
	for(int i = 0; i < pEditor->m_Map.m_lImages.size(); ++i)
	{
		if(!str_comp(pEditor->m_Map.m_lImages[i]->m_aName, pImg->m_aName))
			pEditor->m_SelectedImage = i;
	}
	pEditor->m_Dialog = DIALOG_NONE;
}

void CEditor::AddImage(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;
	CEditorImage ImgInfo(pEditor);
	if(!pEditor->Graphics()->LoadPNG(&ImgInfo, pFileName, StorageType))
		return;

	// check if we have that image already
	char aBuf[128];
	IStorage::StripPathAndExtension(pFileName, aBuf, sizeof(aBuf));
	for(int i = 0; i < pEditor->m_Map.m_lImages.size(); ++i)
	{
		if(!str_comp(pEditor->m_Map.m_lImages[i]->m_aName, aBuf))
			return;
	}

	CEditorImage *pImg = new CEditorImage(pEditor);
	*pImg = ImgInfo;
	pImg->m_TexID = pEditor->Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, CImageInfo::FORMAT_AUTO, 0);
	ImgInfo.m_pData = 0;
	pImg->m_External = 1;	// external by default
	str_copy(pImg->m_aName, aBuf, sizeof(pImg->m_aName));
	pImg->m_AutoMapper.Load(pImg->m_aName);
	pEditor->m_Map.m_lImages.add(pImg);
	pEditor->SortImages();
	if(pEditor->m_SelectedImage > -1 && pEditor->m_SelectedImage < pEditor->m_Map.m_lImages.size())
	{
		for(int i = 0; i <= pEditor->m_SelectedImage; ++i)
			if(!str_comp(pEditor->m_Map.m_lImages[i]->m_aName, aBuf))
			{
				pEditor->m_SelectedImage++;
				break;
			}
	}
	pEditor->m_Dialog = DIALOG_NONE;
}

void CEditor::AddSound(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;

	// check if we have that sound already
	char aBuf[128];
	IStorage::StripPathAndExtension(pFileName, aBuf, sizeof(aBuf));
	for(int i = 0; i < pEditor->m_Map.m_lSounds.size(); ++i)
	{
		if(!str_comp(pEditor->m_Map.m_lSounds[i]->m_aName, aBuf))
			return;
	}

	// load external
	IOHANDLE SoundFile = pEditor->Storage()->OpenFile(pFileName, IOFLAG_READ, StorageType);
	if(!SoundFile)
	{
		dbg_msg("sound/opus", "failed to open file. filename='%s'", pFileName);
		return;
	}

	// read the whole file into memory
	int DataSize = io_length(SoundFile);

	if(DataSize <= 0)
	{
		io_close(SoundFile);
		dbg_msg("sound/opus", "failed to open file. filename='%s'", pFileName);
		return;
	}

	void *pData = mem_alloc((unsigned) DataSize, 1);
	io_read(SoundFile, pData, (unsigned) DataSize);
	io_close(SoundFile);

	// load sound
	int SoundId = pEditor->Sound()->LoadOpusFromMem(pData, (unsigned) DataSize, true);
	if(SoundId == -1)
		return;

	// add sound
	CEditorSound *pSound = new CEditorSound(pEditor);
	pSound->m_SoundID = SoundId;
	pSound->m_External = 1;	// external by default
	pSound->m_DataSize = (unsigned) DataSize;
	pSound->m_pData = pData;
	str_copy(pSound->m_aName, aBuf, sizeof(pSound->m_aName));
	pEditor->m_Map.m_lSounds.add(pSound);

	if(pEditor->m_SelectedSound > -1 && pEditor->m_SelectedSound < pEditor->m_Map.m_lSounds.size())
	{
		for(int i = 0; i <= pEditor->m_SelectedSound; ++i)
			if(!str_comp(pEditor->m_Map.m_lSounds[i]->m_aName, aBuf))
			{
				pEditor->m_SelectedSound++;
				break;
			}
	}

	pEditor->m_Dialog = DIALOG_NONE;
}

void CEditor::ReplaceSound(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;

	// load external
	IOHANDLE SoundFile = pEditor->Storage()->OpenFile(pFileName, IOFLAG_READ, StorageType);
	if(!SoundFile)
	{
		dbg_msg("sound/opus", "failed to open file. filename='%s'", pFileName);
		return;
	}

	// read the whole file into memory
	int DataSize = io_length(SoundFile);

	if(DataSize <= 0)
	{
		io_close(SoundFile);
		dbg_msg("sound/opus", "failed to open file. filename='%s'", pFileName);
		return;
	}

	void *pData = mem_alloc((unsigned) DataSize, 1);
	io_read(SoundFile, pData, (unsigned) DataSize);
	io_close(SoundFile);

	CEditorSound *pSound = pEditor->m_Map.m_lSounds[pEditor->m_SelectedSound];
	int External = pSound->m_External;

	// unload sample
	pEditor->Sound()->UnloadSample(pSound->m_SoundID);
	if(pSound->m_pData)
	{
		mem_free(pSound->m_pData);
		pSound->m_pData = 0x0;
	}

	// replace sound
	pSound->m_External = External;
	IStorage::StripPathAndExtension(pFileName, pSound->m_aName, sizeof(pSound->m_aName));
	pSound->m_SoundID = pEditor->Sound()->LoadOpusFromMem(pData, (unsigned) DataSize, true);
	pSound->m_pData = pData;
	pSound->m_DataSize = DataSize;

	pEditor->m_Dialog = DIALOG_NONE;
}


static int gs_ModifyIndexDeletedIndex;
static void ModifyIndexDeleted(int *pIndex)
{
	if(*pIndex == gs_ModifyIndexDeletedIndex)
		*pIndex = -1;
	else if(*pIndex > gs_ModifyIndexDeletedIndex)
		*pIndex = *pIndex - 1;
}

int CEditor::PopupImage(CEditor *pEditor, CUIRect View)
{
	static int s_ReplaceButton = 0;
	static int s_RemoveButton = 0;

	CUIRect Slot;
	View.HSplitTop(2.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	CEditorImage *pImg = pEditor->m_Map.m_lImages[pEditor->m_SelectedImage];

	static int s_ExternalButton = 0;
	if(pImg->m_External)
	{
		if(pEditor->DoButton_MenuItem(&s_ExternalButton, "Embed", 0, &Slot, 0, "Embeds the image into the map file."))
		{
			pImg->m_External = 0;
			return 1;
		}
	}
	else
	{
		if(pEditor->DoButton_MenuItem(&s_ExternalButton, "Make external", 0, &Slot, 0, "Removes the image from the map file."))
		{
			pImg->m_External = 1;
			return 1;
		}
	}

	View.HSplitTop(10.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_ReplaceButton, "Replace", 0, &Slot, 0, "Replaces the image with a new one"))
	{
		pEditor->InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_IMG, "Replace Image", "Replace", "mapres", "", ReplaceImage, pEditor);
		return 1;
	}

	View.HSplitTop(10.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_RemoveButton, "Remove", 0, &Slot, 0, "Removes the image from the map"))
	{
		delete pImg;
		pEditor->m_Map.m_lImages.remove_index(pEditor->m_SelectedImage);
		gs_ModifyIndexDeletedIndex = pEditor->m_SelectedImage;
		pEditor->m_Map.ModifyImageIndex(ModifyIndexDeleted);
		return 1;
	}

	return 0;
}

int CEditor::PopupSound(CEditor *pEditor, CUIRect View)
{
	static int s_ReplaceButton = 0;
	static int s_RemoveButton = 0;

	CUIRect Slot;
	View.HSplitTop(2.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	CEditorSound *pSound = pEditor->m_Map.m_lSounds[pEditor->m_SelectedSound];

	static int s_ExternalButton = 0;
	if(pSound->m_External)
	{
		if(pEditor->DoButton_MenuItem(&s_ExternalButton, "Embed", 0, &Slot, 0, "Embeds the sound into the map file."))
		{
			pSound->m_External = 0;
			return 1;
		}
	}
	else
	{
		if(pEditor->DoButton_MenuItem(&s_ExternalButton, "Make external", 0, &Slot, 0, "Removes the sound from the map file."))
		{
			pSound->m_External = 1;
			return 1;
		}
	}


	View.HSplitTop(10.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_ReplaceButton, "Replace", 0, &Slot, 0, "Replaces the sound with a new one"))
	{
		pEditor->InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_SOUND, "Replace sound", "Replace", "mapres", "", ReplaceSound, pEditor);
		return 1;
	}

	View.HSplitTop(10.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_RemoveButton, "Remove", 0, &Slot, 0, "Removes the sound from the map"))
	{
		delete pSound;
		pEditor->m_Map.m_lSounds.remove_index(pEditor->m_SelectedSound);
		gs_ModifyIndexDeletedIndex = pEditor->m_SelectedSound;
		pEditor->m_Map.ModifySoundIndex(ModifyIndexDeleted);
		return 1;
	}

	return 0;
}

static int CompareImageName(const void *pObject1, const void *pObject2)
{
	CEditorImage *pImage1 = *(CEditorImage**)pObject1;
	CEditorImage *pImage2 = *(CEditorImage**)pObject2;

	return str_comp(pImage1->m_aName, pImage2->m_aName);
}

static int *gs_pSortedIndex = 0;
static void ModifySortedIndex(int *pIndex)
{
	if(*pIndex > -1)
		*pIndex = gs_pSortedIndex[*pIndex];
}

void CEditor::SortImages()
{
	bool Sorted = true;
	for(int i = 1; i < m_Map.m_lImages.size(); i++)
		if( str_comp(m_Map.m_lImages[i]->m_aName, m_Map.m_lImages[i-1]->m_aName) < 0 )
		{
			Sorted = false;
			break;
		}

	if(!Sorted)
	{
		array<CEditorImage*> lTemp = array<CEditorImage*>(m_Map.m_lImages);
		gs_pSortedIndex = new int[lTemp.size()];

		qsort(m_Map.m_lImages.base_ptr(), m_Map.m_lImages.size(), sizeof(CEditorImage*), CompareImageName);

		for(int OldIndex = 0; OldIndex < lTemp.size(); OldIndex++)
			for(int NewIndex = 0; NewIndex < m_Map.m_lImages.size(); NewIndex++)
				if(lTemp[OldIndex] == m_Map.m_lImages[NewIndex])
					gs_pSortedIndex[OldIndex] = NewIndex;

		m_Map.ModifyImageIndex(ModifySortedIndex);

		delete [] gs_pSortedIndex;
		gs_pSortedIndex = 0;
	}
}


void CEditor::RenderImages(CUIRect ToolBox, CUIRect ToolBar, CUIRect View)
{
	static int s_ScrollBar = 0;
	static float s_ScrollValue = 0;
	float ImagesHeight = 30.0f + 14.0f * m_Map.m_lImages.size() + 27.0f;
	float ScrollDifference = ImagesHeight - ToolBox.h;

	if(!m_GuiActive)
		return;

	if(ImagesHeight > ToolBox.h)	// Do we even need a scrollbar?
	{
		CUIRect Scroll;
		ToolBox.VSplitRight(15.0f, &ToolBox, &Scroll);
		ToolBox.VSplitRight(3.0f, &ToolBox, 0);	// extra spacing
		Scroll.HMargin(5.0f, &Scroll);
		s_ScrollValue = UiDoScrollbarV(&s_ScrollBar, &Scroll, s_ScrollValue);

		if(UI()->MouseInside(&Scroll) || UI()->MouseInside(&ToolBox))
		{
			int ScrollNum = (int)((ImagesHeight-ToolBox.h)/14.0f)+1;
			if(ScrollNum > 0)
			{
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
					s_ScrollValue = clamp(s_ScrollValue - 1.0f/ScrollNum, 0.0f, 1.0f);
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
					s_ScrollValue = clamp(s_ScrollValue + 1.0f/ScrollNum, 0.0f, 1.0f);
			}
		}
	}

	float ImageStartAt = ScrollDifference * s_ScrollValue;
	if(ImageStartAt < 0.0f)
		ImageStartAt = 0.0f;

	float ImageStopAt = ImagesHeight - ScrollDifference * (1 - s_ScrollValue);
	float ImageCur = 0.0f;

	for(int e = 0; e < 2; e++) // two passes, first embedded, then external
	{
		CUIRect Slot;

		if(ImageCur > ImageStopAt)
			break;
		else if(ImageCur >= ImageStartAt)
		{

			ToolBox.HSplitTop(15.0f, &Slot, &ToolBox);
			if(e == 0)
				UI()->DoLabel(&Slot, "Embedded", 12.0f, 0);
			else
				UI()->DoLabel(&Slot, "External", 12.0f, 0);
		}
		ImageCur += 15.0f;

		for(int i = 0; i < m_Map.m_lImages.size(); i++)
		{
			if((e && !m_Map.m_lImages[i]->m_External) ||
				(!e && m_Map.m_lImages[i]->m_External))
			{
				continue;
			}

			if(ImageCur > ImageStopAt)
				break;
			else if(ImageCur < ImageStartAt)
			{
				ImageCur += 14.0f;
				continue;
			}
			ImageCur += 14.0f;

			char aBuf[128];
			str_copy(aBuf, m_Map.m_lImages[i]->m_aName, sizeof(aBuf));
			ToolBox.HSplitTop(12.0f, &Slot, &ToolBox);

			int Selected = m_SelectedImage == i;

			for(int x = 0; x < m_Map.m_lGroups.size(); ++x)
				for(int j = 0; j < m_Map.m_lGroups[x]->m_lLayers.size(); ++j)
					if(m_Map.m_lGroups[x]->m_lLayers[j]->m_Type == LAYERTYPE_QUADS)
					{
						CLayerQuads *pLayer = static_cast<CLayerQuads *>(m_Map.m_lGroups[x]->m_lLayers[j]);
						if(pLayer->m_Image == i)
							goto done;
					}
					else if(m_Map.m_lGroups[x]->m_lLayers[j]->m_Type == LAYERTYPE_TILES)
					{
						CLayerTiles *pLayer = static_cast<CLayerTiles *>(m_Map.m_lGroups[x]->m_lLayers[j]);
						if(pLayer->m_Image == i)
							goto done;
					}

			Selected += 2; // Image is unused
			done:
			if(Selected < 2 && e == 1)
			{
				bool Found = false;
				for(unsigned int k = 0; k < sizeof(vanillaImages) / sizeof(vanillaImages[0]); k++)
				{
					if(str_comp(m_Map.m_lImages[i]->m_aName, vanillaImages[k]) == 0)
					{
						Found = true;
						break;
					}
				}
				if(!Found)
					Selected += 4; // Image should be embedded
			}

			float FontSize = 10.0f;
			while(TextRender()->TextWidth(0, FontSize, aBuf, -1) > Slot.w)
				FontSize--;

			if(int Result = DoButton_Ex(&m_Map.m_lImages[i], aBuf, Selected, &Slot,
				BUTTON_CONTEXT, "Select image", 0, FontSize))
			{
				m_SelectedImage = i;

				static int s_PopupImageID = 0;
				if(Result == 2)
					UiInvokePopupMenu(&s_PopupImageID, 0, UI()->MouseX(), UI()->MouseY(), 120, 80, PopupImage);
			}

			ToolBox.HSplitTop(2.0f, 0, &ToolBox);
		}

		// separator
		ToolBox.HSplitTop(5.0f, &Slot, &ToolBox);
		ImageCur += 5.0f;
		IGraphics::CLineItem LineItem(Slot.x, Slot.y+Slot.h/2, Slot.x+Slot.w, Slot.y+Slot.h/2);
		Graphics()->TextureSet(-1);
		Graphics()->LinesBegin();
		Graphics()->LinesDraw(&LineItem, 1);
		Graphics()->LinesEnd();
	}

	// render image
	int i = m_SelectedImage;
	if(i < m_Map.m_lImages.size())
	{
		CUIRect r;
		View.Margin(10.0f, &r);
		if(r.h < r.w)
			r.w = r.h;
		else
			r.h = r.w;
		float Max = (float)(max(m_Map.m_lImages[i]->m_Width, m_Map.m_lImages[i]->m_Height));
		r.w *= m_Map.m_lImages[i]->m_Width/Max;
		r.h *= m_Map.m_lImages[i]->m_Height/Max;
		Graphics()->TextureSet(m_Map.m_lImages[i]->m_TexID);
		Graphics()->BlendNormal();
		Graphics()->WrapClamp();
		Graphics()->QuadsBegin();
		IGraphics::CQuadItem QuadItem(r.x, r.y, r.w, r.h);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();
		Graphics()->WrapNormal();
	}
	//if(ImageCur + 27.0f > ImageStopAt)
	//	return;

	CUIRect Slot;
	ToolBox.HSplitTop(5.0f, &Slot, &ToolBox);

	// new image
	static int s_NewImageButton = 0;
	ToolBox.HSplitTop(12.0f, &Slot, &ToolBox);
	if(DoButton_Editor(&s_NewImageButton, "Add", 0, &Slot, 0, "Load a new image to use in the map"))
		InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_IMG, "Add Image", "Add", "mapres", "", AddImage, this);
}

void CEditor::RenderSounds(CUIRect ToolBox, CUIRect ToolBar, CUIRect View)
{
	static int s_ScrollBar = 0;
	static float s_ScrollValue = 0;
	float SoundsHeight = 30.0f + 14.0f * m_Map.m_lSounds.size() + 27.0f;
	float ScrollDifference = SoundsHeight - ToolBox.h;

	if(!m_GuiActive)
		return;

	if(SoundsHeight > ToolBox.h)	// Do we even need a scrollbar?
	{
		CUIRect Scroll;
		ToolBox.VSplitRight(15.0f, &ToolBox, &Scroll);
		ToolBox.VSplitRight(3.0f, &ToolBox, 0);	// extra spacing
		Scroll.HMargin(5.0f, &Scroll);
		s_ScrollValue = UiDoScrollbarV(&s_ScrollBar, &Scroll, s_ScrollValue);

		if(UI()->MouseInside(&Scroll) || UI()->MouseInside(&ToolBox))
		{
			int ScrollNum = (int)((SoundsHeight-ToolBox.h)/14.0f)+1;
			if(ScrollNum > 0)
			{
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
					s_ScrollValue = clamp(s_ScrollValue - 1.0f/ScrollNum, 0.0f, 1.0f);
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
					s_ScrollValue = clamp(s_ScrollValue + 1.0f/ScrollNum, 0.0f, 1.0f);
			}
		}
	}

	float SoundStartAt = ScrollDifference * s_ScrollValue;
	if(SoundStartAt < 0.0f)
		SoundStartAt = 0.0f;

	float SoundStopAt = SoundsHeight - ScrollDifference * (1 - s_ScrollValue);
	float SoundCur = 0.0f;

	for(int e = 0; e < 2; e++) // two passes, first embedded, then external
	{
		CUIRect Slot;

		if(SoundCur > SoundStopAt)
			break;
		else if(SoundCur >= SoundStartAt)
		{

			ToolBox.HSplitTop(15.0f, &Slot, &ToolBox);
			if(e == 0)
				UI()->DoLabel(&Slot, "Embedded", 12.0f, 0);
			else
				UI()->DoLabel(&Slot, "External", 12.0f, 0);
		}
		SoundCur += 15.0f;

		for(int i = 0; i < m_Map.m_lSounds.size(); i++)
		{
			if((e && !m_Map.m_lSounds[i]->m_External) ||
				(!e && m_Map.m_lSounds[i]->m_External))
			{
				continue;
			}

			if(SoundCur > SoundStopAt)
				break;
			else if(SoundCur < SoundStartAt)
			{
				SoundCur += 14.0f;
				continue;
			}
			SoundCur += 14.0f;

			char aBuf[128];
			str_copy(aBuf, m_Map.m_lSounds[i]->m_aName, sizeof(aBuf));
			ToolBox.HSplitTop(12.0f, &Slot, &ToolBox);


			int Selected = m_SelectedSound == i;
			for(int x = 0; x < m_Map.m_lGroups.size(); ++x)
				for(int j = 0; j < m_Map.m_lGroups[x]->m_lLayers.size(); ++j)
					if(m_Map.m_lGroups[x]->m_lLayers[j]->m_Type == LAYERTYPE_SOUNDS)
					{
						CLayerSounds *pLayer = static_cast<CLayerSounds *>(m_Map.m_lGroups[x]->m_lLayers[j]);
						if(pLayer->m_Sound == i)
							goto done;
					}

			Selected += 2; // Sound is unused
			done:
			if(Selected < 2 && e == 1)
				Selected += 4; // Sound should be embedded

			float FontSize = 10.0f;
			while(TextRender()->TextWidth(0, FontSize, aBuf, -1) > Slot.w)
				FontSize--;

			if(int Result = DoButton_Ex(&m_Map.m_lSounds[i], aBuf, Selected, &Slot,
				BUTTON_CONTEXT, "Select sound", 0, FontSize))
			{
				m_SelectedSound = i;

				static int s_PopupSoundID = 0;
				if(Result == 2)
					UiInvokePopupMenu(&s_PopupSoundID, 0, UI()->MouseX(), UI()->MouseY(), 120, 80, PopupSound);
			}

			ToolBox.HSplitTop(2.0f, 0, &ToolBox);
		}

		// separator
		ToolBox.HSplitTop(5.0f, &Slot, &ToolBox);
		SoundCur += 5.0f;
		IGraphics::CLineItem LineItem(Slot.x, Slot.y+Slot.h/2, Slot.x+Slot.w, Slot.y+Slot.h/2);
		Graphics()->TextureSet(-1);
		Graphics()->LinesBegin();
		Graphics()->LinesDraw(&LineItem, 1);
		Graphics()->LinesEnd();
	}

	CUIRect Slot;
	ToolBox.HSplitTop(5.0f, &Slot, &ToolBox);

	// new Sound
	static int s_NewSoundButton = 0;
	ToolBox.HSplitTop(12.0f, &Slot, &ToolBox);
	if(DoButton_Editor(&s_NewSoundButton, "Add", 0, &Slot, 0, "Load a new sound to use in the map"))
		InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_SOUND, "Add Sound", "Add", "mapres", "", AddSound, this);
}


static int EditorListdirCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor*)pUser;
	int Length = str_length(pName);
	if((pName[0] == '.' && (pName[1] == 0 ||
		(pName[1] == '.' && pName[2] == 0 && (!str_comp(pEditor->m_pFileDialogPath, "maps") || !str_comp(pEditor->m_pFileDialogPath, "mapres"))))) ||
		(!IsDir && ((pEditor->m_FileDialogFileType == CEditor::FILETYPE_MAP && (Length < 4 || str_comp(pName+Length-4, ".map"))) ||
		(pEditor->m_FileDialogFileType == CEditor::FILETYPE_IMG && (Length < 4 || str_comp(pName+Length-4, ".png"))) ||
		(pEditor->m_FileDialogFileType == CEditor::FILETYPE_SOUND && (Length < 5 || str_comp(pName+Length-5, ".opus"))))))
		return 0;

	CEditor::CFilelistItem Item;
	str_copy(Item.m_aFilename, pName, sizeof(Item.m_aFilename));
	if(IsDir)
		str_format(Item.m_aName, sizeof(Item.m_aName), "%s/", pName);
	else
	{
		if(pEditor->m_FileDialogFileType == CEditor::FILETYPE_SOUND)
			str_copy(Item.m_aName, pName, min(static_cast<int>(sizeof(Item.m_aName)), Length-4));
		else
			str_copy(Item.m_aName, pName, min(static_cast<int>(sizeof(Item.m_aName)), Length-3));
	}
	Item.m_IsDir = IsDir != 0;
	Item.m_IsLink = false;
	Item.m_StorageType = StorageType;
	pEditor->m_FileList.add(Item);

	return 0;
}

void CEditor::AddFileDialogEntry(int Index, CUIRect *pView)
{
	m_FilesCur++;
	if(m_FilesCur-1 < m_FilesStartAt || m_FilesCur >= m_FilesStopAt)
		return;

	CUIRect Button, FileIcon;
	pView->HSplitTop(15.0f, &Button, pView);
	pView->HSplitTop(2.0f, 0, pView);
	Button.VSplitLeft(Button.h, &FileIcon, &Button);
	Button.VSplitLeft(5.0f, 0, &Button);

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_FILEICONS].m_Id);
	Graphics()->QuadsBegin();
	RenderTools()->SelectSprite(m_FileList[Index].m_IsDir?SPRITE_FILE_FOLDER:SPRITE_FILE_MAP2);
	IGraphics::CQuadItem QuadItem(FileIcon.x, FileIcon.y, FileIcon.w, FileIcon.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	if(DoButton_File(&m_FileList[Index], m_FileList[Index].m_aName, m_FilesSelectedIndex == Index, &Button, 0, 0))
	{
		if(!m_FileList[Index].m_IsDir)
			str_copy(m_aFileDialogFileName, m_FileList[Index].m_aFilename, sizeof(m_aFileDialogFileName));
		else
			m_aFileDialogFileName[0] = 0;
		m_FilesSelectedIndex = Index;
		m_FilePreviewImage = 0;

		if(Input()->MouseDoubleClick())
			m_aFileDialogActivate = true;
	}
}

void CEditor::RenderFileDialog()
{
	// GUI coordsys
	Graphics()->MapScreen(UI()->Screen()->x, UI()->Screen()->y, UI()->Screen()->w, UI()->Screen()->h);
	CUIRect View = *UI()->Screen();
	CUIRect Preview;
	float Width = View.w, Height = View.h;

	RenderTools()->DrawUIRect(&View, vec4(0,0,0,0.25f), 0, 0);
	View.VMargin(150.0f, &View);
	View.HMargin(50.0f, &View);
	RenderTools()->DrawUIRect(&View, vec4(0,0,0,0.75f), CUI::CORNER_ALL, 5.0f);
	View.Margin(10.0f, &View);

	CUIRect Title, FileBox, FileBoxLabel, ButtonBar, Scroll, PathBox;
	View.HSplitTop(18.0f, &Title, &View);
	View.HSplitTop(5.0f, 0, &View); // some spacing
	View.HSplitBottom(14.0f, &View, &ButtonBar);
	View.HSplitBottom(10.0f, &View, 0); // some spacing
	View.HSplitBottom(14.0f, &View, &PathBox);
	View.HSplitBottom(5.0f, &View, 0); // some spacing
	View.HSplitBottom(14.0f, &View, &FileBox);
	FileBox.VSplitLeft(55.0f, &FileBoxLabel, &FileBox);
	View.HSplitBottom(10.0f, &View, 0); // some spacing
	if (m_FileDialogFileType == CEditor::FILETYPE_IMG)
		View.VSplitMid(&View, &Preview);
	View.VSplitRight(15.0f, &View, &Scroll);

	// title
	RenderTools()->DrawUIRect(&Title, vec4(1, 1, 1, 0.25f), CUI::CORNER_ALL, 4.0f);
	Title.VMargin(10.0f, &Title);
	UI()->DoLabel(&Title, m_pFileDialogTitle, 12.0f, -1, -1);

	// pathbox
	char aPath[128], aBuf[128];
	if(m_FilesSelectedIndex != -1)
		Storage()->GetCompletePath(m_FileList[m_FilesSelectedIndex].m_StorageType, m_pFileDialogPath, aPath, sizeof(aPath));
	else
		aPath[0] = 0;
	str_format(aBuf, sizeof(aBuf), "Current path: %s", aPath);
	UI()->DoLabel(&PathBox, aBuf, 10.0f, -1, -1);

	if(m_FileDialogStorageType == IStorage::TYPE_SAVE)
	{
		// filebox
		static float s_FileBoxID = 0;
		UI()->DoLabel(&FileBoxLabel, "Filename:", 10.0f, -1, -1);
		if(DoEditBox(&s_FileBoxID, &FileBox, m_aFileDialogFileName, sizeof(m_aFileDialogFileName), 10.0f, &s_FileBoxID))
		{
			// remove '/' and '\'
			for(int i = 0; m_aFileDialogFileName[i]; ++i)
				if(m_aFileDialogFileName[i] == '/' || m_aFileDialogFileName[i] == '\\')
					str_copy(&m_aFileDialogFileName[i], &m_aFileDialogFileName[i+1], (int)(sizeof(m_aFileDialogFileName))-i);
			m_FilesSelectedIndex = -1;
		}
	}
	else
	{
		//searchbox
		FileBox.VSplitRight(250, &FileBox, 0);
		CUIRect ClearBox;
		FileBox.VSplitRight(15, &FileBox, &ClearBox);

		static float s_SearchBoxID = 0;
		UI()->DoLabel(&FileBoxLabel, "Search:", 10.0f, -1, -1);
		DoEditBox(&s_SearchBoxID, &FileBox, m_aFileDialogSearchText, sizeof(m_aFileDialogSearchText), 10.0f, &s_SearchBoxID,false,CUI::CORNER_L);

		// clearSearchbox button
		{
			static int s_ClearButton = 0;
			RenderTools()->DrawUIRect(&ClearBox, vec4(1, 1, 1, 0.33f)*ButtonColorMul(&s_ClearButton), CUI::CORNER_R, 3.0f);
			UI()->DoLabel(&ClearBox, "×", 10.0f, 0);
			if (UI()->DoButtonLogic(&s_ClearButton, "×", 0, &ClearBox))
			{
				m_aFileDialogSearchText[0] = 0;
				UI()->SetActiveItem(&s_SearchBoxID);
			}
		}
	}

	int Num = (int)(View.h/17.0f)+1;
	static int ScrollBar = 0;
	Scroll.HMargin(5.0f, &Scroll);
	m_FileDialogScrollValue = UiDoScrollbarV(&ScrollBar, &Scroll, m_FileDialogScrollValue);

	int ScrollNum = m_FileList.size()-Num+1;
	if(ScrollNum > 0)
	{
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
			m_FileDialogScrollValue -= 3.0f/ScrollNum;
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
			m_FileDialogScrollValue += 3.0f/ScrollNum;
	}
	else
		ScrollNum = 0;

	if(m_FilesSelectedIndex > -1)
	{
		for(int i = 0; i < Input()->NumEvents(); i++)
		{
			int NewIndex = -1;
			if(Input()->GetEvent(i).m_Flags&IInput::FLAG_PRESS)
			{
				if(Input()->GetEvent(i).m_Key == KEY_DOWN) NewIndex = m_FilesSelectedIndex + 1;
				if(Input()->GetEvent(i).m_Key == KEY_UP) NewIndex = m_FilesSelectedIndex - 1;
			}
			if(NewIndex > -1 && NewIndex < m_FileList.size())
			{
				//scroll
				float IndexY = View.y - m_FileDialogScrollValue*ScrollNum*17.0f + NewIndex*17.0f;
				int Scroll = View.y > IndexY ? -1 : View.y+View.h < IndexY+17.0f ? 1 : 0;
				if(Scroll)
				{
					if(Scroll < 0)
						m_FileDialogScrollValue = ((float)(NewIndex)+0.5f)/ScrollNum;
					else
						m_FileDialogScrollValue = ((float)(NewIndex-Num)+2.5f)/ScrollNum;
				}

				if(!m_FileList[NewIndex].m_IsDir)
					str_copy(m_aFileDialogFileName, m_FileList[NewIndex].m_aFilename, sizeof(m_aFileDialogFileName));
				else
					m_aFileDialogFileName[0] = 0;
				m_FilesSelectedIndex = NewIndex;
				m_FilePreviewImage = 0;
			}
		}

		if (m_FileDialogFileType == CEditor::FILETYPE_IMG && m_FilePreviewImage == 0 && m_FilesSelectedIndex > -1)
		{
			int Length = str_length(m_FileList[m_FilesSelectedIndex].m_aFilename);
			if (Length >= 4 && !str_comp(m_FileList[m_FilesSelectedIndex].m_aFilename+Length-4, ".png"))
			{
				char aBuffer[1024];
				str_format(aBuffer, sizeof(aBuffer), "%s/%s", m_pFileDialogPath, m_FileList[m_FilesSelectedIndex].m_aFilename);

				if(Graphics()->LoadPNG(&m_FilePreviewImageInfo, aBuffer, IStorage::TYPE_ALL))
				{
					m_FilePreviewImage = Graphics()->LoadTextureRaw(m_FilePreviewImageInfo.m_Width, m_FilePreviewImageInfo.m_Height, m_FilePreviewImageInfo.m_Format, m_FilePreviewImageInfo.m_pData, m_FilePreviewImageInfo.m_Format, IGraphics::TEXLOAD_NORESAMPLE);
					mem_free(m_FilePreviewImageInfo.m_pData);
				}
			}
		}
		if (m_FilePreviewImage)
		{
			int w = m_FilePreviewImageInfo.m_Width;
			int h = m_FilePreviewImageInfo.m_Height;
			if (m_FilePreviewImageInfo.m_Width > Preview.w)
			{
				h = m_FilePreviewImageInfo.m_Height * Preview.w / m_FilePreviewImageInfo.m_Width;
				w = Preview.w;
			}
			if (h > Preview.h)
			{
				w = w * Preview.h / h,
				h = Preview.h;
			}

			Graphics()->TextureSet(m_FilePreviewImage);
			Graphics()->BlendNormal();
			Graphics()->QuadsBegin();
			IGraphics::CQuadItem QuadItem(Preview.x, Preview.y, w, h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}
	}

	for(int i = 0; i < Input()->NumEvents(); i++)
	{
		if(Input()->GetEvent(i).m_Flags&IInput::FLAG_PRESS)
		{
			if(Input()->GetEvent(i).m_Key == KEY_RETURN || Input()->GetEvent(i).m_Key == KEY_KP_ENTER)
				m_aFileDialogActivate = true;
		}
	}

	if(m_FileDialogScrollValue < 0) m_FileDialogScrollValue = 0;
	if(m_FileDialogScrollValue > 1) m_FileDialogScrollValue = 1;

	m_FilesStartAt = (int)(ScrollNum*m_FileDialogScrollValue);
	if(m_FilesStartAt < 0)
		m_FilesStartAt = 0;

	m_FilesStopAt = m_FilesStartAt+Num;

	m_FilesCur = 0;

	// set clipping
	UI()->ClipEnable(&View);

	for(int i = 0; i < m_FileList.size(); i++)
		if (!m_aFileDialogSearchText[0] || str_find_nocase (m_FileList[i].m_aName, m_aFileDialogSearchText))
		AddFileDialogEntry(i, &View);

	// disable clipping again
	UI()->ClipDisable();

	// the buttons
	static int s_OkButton = 0;
	static int s_CancelButton = 0;
	static int s_NewFolderButton = 0;
	static int s_MapInfoButton = 0;

	CUIRect Button;
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	bool IsDir = m_FilesSelectedIndex >= 0 && m_FileList[m_FilesSelectedIndex].m_IsDir;
	if(DoButton_Editor(&s_OkButton, IsDir ? "Open" : m_pFileDialogButtonText, 0, &Button, 0, 0) || m_aFileDialogActivate)
	{
		m_aFileDialogActivate = false;
		if(IsDir)	// folder
		{
			if(str_comp(m_FileList[m_FilesSelectedIndex].m_aFilename, "..") == 0)	// parent folder
			{
				if(fs_parent_dir(m_pFileDialogPath))
					m_pFileDialogPath = m_aFileDialogCurrentFolder;	// leave the link
			}
			else	// sub folder
			{
				if(m_FileList[m_FilesSelectedIndex].m_IsLink)
				{
					m_pFileDialogPath = m_aFileDialogCurrentLink;	// follow the link
					str_copy(m_aFileDialogCurrentLink, m_FileList[m_FilesSelectedIndex].m_aFilename, sizeof(m_aFileDialogCurrentLink));
				}
				else
				{
					char aTemp[MAX_PATH_LENGTH];
					str_copy(aTemp, m_pFileDialogPath, sizeof(aTemp));
					str_format(m_pFileDialogPath, MAX_PATH_LENGTH, "%s/%s", aTemp, m_FileList[m_FilesSelectedIndex].m_aFilename);
				}
			}
			FilelistPopulate(!str_comp(m_pFileDialogPath, "maps") || !str_comp(m_pFileDialogPath, "mapres") ? m_FileDialogStorageType :
				m_FileList[m_FilesSelectedIndex].m_StorageType);
			if(m_FilesSelectedIndex >= 0 && !m_FileList[m_FilesSelectedIndex].m_IsDir)
				str_copy(m_aFileDialogFileName, m_FileList[m_FilesSelectedIndex].m_aFilename, sizeof(m_aFileDialogFileName));
			else
				m_aFileDialogFileName[0] = 0;
		}
		else // file
		{
			str_format(m_aFileSaveName, sizeof(m_aFileSaveName), "%s/%s", m_pFileDialogPath, m_aFileDialogFileName);
			if(!str_comp(m_pFileDialogButtonText, "Save"))
			{
				IOHANDLE File = Storage()->OpenFile(m_aFileSaveName, IOFLAG_READ, IStorage::TYPE_SAVE);
				if(File)
				{
					io_close(File);
					m_PopupEventType = POPEVENT_SAVE;
					m_PopupEventActivated = true;
				}
				else
					if(m_pfnFileDialogFunc)
						m_pfnFileDialogFunc(m_aFileSaveName, m_FilesSelectedIndex >= 0 ? m_FileList[m_FilesSelectedIndex].m_StorageType : m_FileDialogStorageType, m_pFileDialogUser);
			}
			else
				if(m_pfnFileDialogFunc)
					m_pfnFileDialogFunc(m_aFileSaveName, m_FilesSelectedIndex >= 0 ? m_FileList[m_FilesSelectedIndex].m_StorageType : m_FileDialogStorageType, m_pFileDialogUser);
		}
	}

	ButtonBar.VSplitRight(40.0f, &ButtonBar, &Button);
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	if(DoButton_Editor(&s_CancelButton, "Cancel", 0, &Button, 0, 0) || Input()->KeyIsPressed(KEY_ESCAPE))
		m_Dialog = DIALOG_NONE;

	if(m_FileDialogStorageType == IStorage::TYPE_SAVE)
	{
		ButtonBar.VSplitLeft(40.0f, 0, &ButtonBar);
		ButtonBar.VSplitLeft(70.0f, &Button, &ButtonBar);
		if(DoButton_Editor(&s_NewFolderButton, "New folder", 0, &Button, 0, 0))
		{
			m_FileDialogNewFolderName[0] = 0;
			m_FileDialogErrString[0] = 0;
			static int s_NewFolderPopupID = 0;
			UiInvokePopupMenu(&s_NewFolderPopupID, 0, Width/2.0f-200.0f, Height/2.0f-100.0f, 400.0f, 200.0f, PopupNewFolder);
			UI()->SetActiveItem(0);
		}
	}

	if(m_FileDialogStorageType == IStorage::TYPE_SAVE)
	{
		ButtonBar.VSplitLeft(40.0f, 0, &ButtonBar);
		ButtonBar.VSplitLeft(70.0f, &Button, &ButtonBar);
		if(DoButton_Editor(&s_MapInfoButton, "Map details", 0, &Button, 0, 0))
		{
			str_copy(m_Map.m_MapInfo.m_aAuthorTmp, m_Map.m_MapInfo.m_aAuthor, sizeof(m_Map.m_MapInfo.m_aAuthorTmp));
			str_copy(m_Map.m_MapInfo.m_aVersionTmp, m_Map.m_MapInfo.m_aVersion, sizeof(m_Map.m_MapInfo.m_aVersionTmp));
			str_copy(m_Map.m_MapInfo.m_aCreditsTmp, m_Map.m_MapInfo.m_aCredits, sizeof(m_Map.m_MapInfo.m_aCreditsTmp));
			str_copy(m_Map.m_MapInfo.m_aLicenseTmp, m_Map.m_MapInfo.m_aLicense, sizeof(m_Map.m_MapInfo.m_aLicenseTmp));
			static int s_MapInfoPopupID = 0;
			UiInvokePopupMenu(&s_MapInfoPopupID, 0, Width/2.0f-200.0f, Height/2.0f-100.0f, 400.0f, 200.0f, PopupMapInfo);
			UI()->SetActiveItem(0);
		}
	}
}

void CEditor::FilelistPopulate(int StorageType)
{
	m_FileList.clear();
	if(m_FileDialogStorageType != IStorage::TYPE_SAVE && !str_comp(m_pFileDialogPath, "maps"))
	{
		CFilelistItem Item;
		str_copy(Item.m_aFilename, "downloadedmaps", sizeof(Item.m_aFilename));
		str_copy(Item.m_aName, "downloadedmaps/", sizeof(Item.m_aName));
		Item.m_IsDir = true;
		Item.m_IsLink = true;
		Item.m_StorageType = IStorage::TYPE_SAVE;
		m_FileList.add(Item);
	}
	Storage()->ListDirectory(StorageType, m_pFileDialogPath, EditorListdirCallback, this);
	m_FilesSelectedIndex = m_FileList.size() ? 0 : -1;
	m_FilePreviewImage = 0;
	m_aFileDialogActivate = false;

	if(m_FilesSelectedIndex >= 0)
		str_copy(m_aFileDialogFileName, m_FileList[m_FilesSelectedIndex].m_aFilename, sizeof(m_aFileDialogFileName));
	else
		m_aFileDialogFileName[0] = 0;
}

void CEditor::InvokeFileDialog(int StorageType, int FileType, const char *pTitle, const char *pButtonText,
	const char *pBasePath, const char *pDefaultName,
	void (*pfnFunc)(const char *pFileName, int StorageType, void *pUser), void *pUser)
{
	m_FileDialogStorageType = StorageType;
	m_pFileDialogTitle = pTitle;
	m_pFileDialogButtonText = pButtonText;
	m_pfnFileDialogFunc = pfnFunc;
	m_pFileDialogUser = pUser;
	m_aFileDialogFileName[0] = 0;
	m_aFileDialogSearchText[0] = 0;
	m_aFileDialogCurrentFolder[0] = 0;
	m_aFileDialogCurrentLink[0] = 0;
	m_pFileDialogPath = m_aFileDialogCurrentFolder;
	m_FileDialogFileType = FileType;
	m_FileDialogScrollValue = 0.0f;
	m_FilePreviewImage = 0;

	if(pDefaultName)
		str_copy(m_aFileDialogFileName, pDefaultName, sizeof(m_aFileDialogFileName));
	if(pBasePath)
		str_copy(m_aFileDialogCurrentFolder, pBasePath, sizeof(m_aFileDialogCurrentFolder));

	FilelistPopulate(m_FileDialogStorageType);

	m_Dialog = DIALOG_FILE;
}



void CEditor::RenderModebar(CUIRect View)
{
	CUIRect Button;

	// mode buttons
	{
		View.VSplitLeft(65.0f, &Button, &View);
		Button.HSplitTop(30.0f, 0, &Button);
		static int s_Button = 0;
		const char *pButName = "";

		if(m_Mode == MODE_LAYERS)
			pButName = "Layers";
		else if(m_Mode == MODE_IMAGES)
			pButName = "Images";
		else if(m_Mode == MODE_SOUNDS)
			pButName = "Sounds";

		int MouseButton = DoButton_Tab(&s_Button, pButName, 0, &Button, 0, "Switch between images, sounds and layers managment.");
		if(MouseButton == 2)
		{
			if (m_Mode == MODE_LAYERS)
				m_Mode = MODE_SOUNDS;
			else if(m_Mode == MODE_IMAGES)
				m_Mode = MODE_LAYERS;
			else
				m_Mode = MODE_IMAGES;
		}
		else if(MouseButton == 1)
		{
			if (m_Mode == MODE_LAYERS)
				m_Mode = MODE_IMAGES;
			else if(m_Mode == MODE_IMAGES)
				m_Mode = MODE_SOUNDS;
			else
				m_Mode = MODE_LAYERS;
		}
	}

	View.VSplitLeft(5.0f, 0, &View);
}

void CEditor::RenderStatusbar(CUIRect View)
{
	CUIRect Button;
	View.VSplitRight(60.0f, &View, &Button);
	static int s_EnvelopeButton = 0;
	int MouseButton = DoButton_Editor(&s_EnvelopeButton, "Envelopes", m_ShowEnvelopeEditor, &Button, 0, "Toggles the envelope editor.");
	if(MouseButton == 2)
		m_ShowEnvelopeEditor = (m_ShowEnvelopeEditor+3)%4;
	else if(MouseButton == 1)
		m_ShowEnvelopeEditor = (m_ShowEnvelopeEditor+1)%4;

	if(MouseButton)
	{
		m_ShowServerSettingsEditor = false;
	}

	View.VSplitRight(100.0f, &View, &Button);
	Button.VSplitRight(10.0f, &Button, 0);
	static int s_SettingsButton = 0;
	if(DoButton_Editor(&s_SettingsButton, "Server settings", m_ShowServerSettingsEditor, &Button, 0, "Toggles the server settings editor."))
	{
		m_ShowEnvelopeEditor = 0;
		m_ShowServerSettingsEditor ^= 1;
	}

	if (g_Config.m_ClEditorUndo)
	{
		View.VSplitRight(5.0f, &View, &Button);
		View.VSplitRight(60.0f, &View, &Button);
		static int s_UndolistButton = 0;
		if(DoButton_Editor(&s_UndolistButton, "Undolist", m_ShowUndo, &Button, 0, "Toggles the undo list."))
			m_ShowUndo = (m_ShowUndo + 1) % 2;
	}

	if(m_pTooltip)
	{
		if(ms_pUiGotContext && ms_pUiGotContext == UI()->HotItem())
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "%s Right click for context menu.", m_pTooltip);
			UI()->DoLabel(&View, aBuf, 10.0f, -1, -1);
		}
		else
			UI()->DoLabel(&View, m_pTooltip, 10.0f, -1, -1);
	}
}

void CEditor::RenderUndoList(CUIRect View)
{
	CUIRect List, Preview, Scroll, Button;
	View.VSplitMid(&List, &Preview);
	List.VSplitRight(15.0f, &List, &Scroll);
	//int Num = (int)(List.h/17.0f)+1;
	static int ScrollBar = 0;
	Scroll.HMargin(5.0f, &Scroll);
	m_UndoScrollValue = UiDoScrollbarV(&ScrollBar, &Scroll, m_UndoScrollValue);

	float TopY = List.y;
	float Height = List.h;
	UI()->ClipEnable(&List);
	int ClickedIndex = -1;
	int HoveredIndex = -1;
	int ScrollNum = m_lUndoSteps.size() - List.h / 17.0f;
	if (ScrollNum < 0)
		ScrollNum = 0;
	List.y -= m_UndoScrollValue*ScrollNum*17.0f;
	for (int i = 0; i < m_lUndoSteps.size(); i++)
	{
		List.HSplitTop(17.0f, &Button, &List);
		if (List.y < TopY)
			continue;
		if (List.y - 17.0f > TopY + Height)
			break;
		if(DoButton_Editor(&m_lUndoSteps[i].m_ButtonId, m_lUndoSteps[i].m_aName, 0, &Button, 0, "Undo to this step"))
			ClickedIndex = i;
		if (UI()->HotItem() == &m_lUndoSteps[i].m_ButtonId)
			HoveredIndex = i;
	}
	UI()->ClipDisable();
	if (ClickedIndex != -1)
	{
		char aBuffer[1024];
		str_format(aBuffer, sizeof(aBuffer), "editor/undo_%i", m_lUndoSteps[HoveredIndex].m_FileNum);
		m_Map.Load(m_pStorage, aBuffer, IStorage::TYPE_SAVE);
		m_Map.m_UndoModified = 0;
		m_LastUndoUpdateTime = time_get();
	}
	if (HoveredIndex != -1)
	{
		if (m_lUndoSteps[HoveredIndex].m_PreviewImage == 0)
		{
			char aBuffer[1024];
			str_format(aBuffer, sizeof(aBuffer), "editor/undo_%i.png", m_lUndoSteps[HoveredIndex].m_FileNum);
			m_lUndoSteps[HoveredIndex].m_PreviewImage = Graphics()->LoadTexture(aBuffer, IStorage::TYPE_SAVE, CImageInfo::FORMAT_RGB, IGraphics::TEXLOAD_NORESAMPLE);
		}
		if (m_lUndoSteps[HoveredIndex].m_PreviewImage)
		{
			Graphics()->TextureSet(m_lUndoSteps[HoveredIndex].m_PreviewImage);
			Graphics()->BlendNormal();
			Graphics()->QuadsBegin();
			IGraphics::CQuadItem QuadItem(Preview.x, Preview.y, Preview.w, Preview.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}
	}
}

void CEditor::RenderEnvelopeEditor(CUIRect View)
{
	if(m_SelectedEnvelope < 0) m_SelectedEnvelope = 0;
	if(m_SelectedEnvelope >= m_Map.m_lEnvelopes.size()) m_SelectedEnvelope = m_Map.m_lEnvelopes.size()-1;

	CEnvelope *pEnvelope = 0;
	if(m_SelectedEnvelope >= 0 && m_SelectedEnvelope < m_Map.m_lEnvelopes.size())
		pEnvelope = m_Map.m_lEnvelopes[m_SelectedEnvelope];

	CUIRect ToolBar, CurveBar, ColorBar;
	View.HSplitTop(15.0f, &ToolBar, &View);
	View.HSplitTop(15.0f, &CurveBar, &View);
	ToolBar.Margin(2.0f, &ToolBar);
	CurveBar.Margin(2.0f, &CurveBar);

	bool CurrentEnvelopeSwitched = false;

	// do the toolbar
	{
		CUIRect Button;
		CEnvelope *pNewEnv = 0;


		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_NewSoundButton = 0;
		if(DoButton_Editor(&s_NewSoundButton, "Sound+", 0, &Button, 0, "Creates a new sound envelope"))
		{
			m_Map.m_Modified = true;
			m_Map.m_UndoModified++;
			pNewEnv = m_Map.NewEnvelope(1);
		}

		ToolBar.VSplitRight(5.0f, &ToolBar, &Button);
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_New4dButton = 0;
		if(DoButton_Editor(&s_New4dButton, "Color+", 0, &Button, 0, "Creates a new color envelope"))
		{
			m_Map.m_Modified = true;
			m_Map.m_UndoModified++;
			pNewEnv = m_Map.NewEnvelope(4);
		}

		ToolBar.VSplitRight(5.0f, &ToolBar, &Button);
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_New2dButton = 0;
		if(DoButton_Editor(&s_New2dButton, "Pos.+", 0, &Button, 0, "Creates a new position envelope"))
		{
			m_Map.m_Modified = true;
			m_Map.m_UndoModified++;
			pNewEnv = m_Map.NewEnvelope(3);
		}

		// Delete button
		if(m_SelectedEnvelope >= 0)
		{
			ToolBar.VSplitRight(10.0f, &ToolBar, &Button);
			ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
			static int s_DelButton = 0;
			if(DoButton_Editor(&s_DelButton, "Delete", 0, &Button, 0, "Delete this envelope"))
			{
				m_Map.m_Modified = true;
				m_Map.m_UndoModified++;
				m_Map.DeleteEnvelope(m_SelectedEnvelope);
				if(m_SelectedEnvelope >= m_Map.m_lEnvelopes.size())
					m_SelectedEnvelope = m_Map.m_lEnvelopes.size()-1;
				pEnvelope = m_SelectedEnvelope >= 0 ? m_Map.m_lEnvelopes[m_SelectedEnvelope] : 0;
			}
		}

		if(pNewEnv) // add the default points
		{
			if(pNewEnv->m_Channels == 4)
			{
				pNewEnv->AddPoint(0, f2fx(1.0f), f2fx(1.0f), f2fx(1.0f), f2fx(1.0f));
				pNewEnv->AddPoint(1000, f2fx(1.0f), f2fx(1.0f), f2fx(1.0f), f2fx(1.0f));
			}
			else
			{
				pNewEnv->AddPoint(0, 0);
				pNewEnv->AddPoint(1000, 0);
			}
		}

		CUIRect Shifter, Inc, Dec;
		ToolBar.VSplitLeft(60.0f, &Shifter, &ToolBar);
		Shifter.VSplitRight(15.0f, &Shifter, &Inc);
		Shifter.VSplitLeft(15.0f, &Dec, &Shifter);
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf),"%d/%d", m_SelectedEnvelope+1, m_Map.m_lEnvelopes.size());
		RenderTools()->DrawUIRect(&Shifter, vec4(1,1,1,0.5f), 0, 0.0f);
		UI()->DoLabel(&Shifter, aBuf, 10.0f, 0, -1);

		static int s_PrevButton = 0;
		if(DoButton_ButtonDec(&s_PrevButton, 0, 0, &Dec, 0, "Previous Envelope"))
		{
			m_SelectedEnvelope--;
			if(m_SelectedEnvelope < 0)
				m_SelectedEnvelope = m_Map.m_lEnvelopes.size() - 1;
			CurrentEnvelopeSwitched = true;
		}

		static int s_NextButton = 0;
		if(DoButton_ButtonInc(&s_NextButton, 0, 0, &Inc, 0, "Next Envelope"))
		{
			m_SelectedEnvelope++;
			if(m_SelectedEnvelope >= m_Map.m_lEnvelopes.size())
				m_SelectedEnvelope = 0;
			CurrentEnvelopeSwitched = true;
		}

		if(pEnvelope)
		{
			ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);
			ToolBar.VSplitLeft(35.0f, &Button, &ToolBar);
			UI()->DoLabel(&Button, "Name:", 10.0f, -1, -1);

			ToolBar.VSplitLeft(80.0f, &Button, &ToolBar);

			static float s_NameBox = 0;
			if(DoEditBox(&s_NameBox, &Button, pEnvelope->m_aName, sizeof(pEnvelope->m_aName), 10.0f, &s_NameBox))
			{
				m_Map.m_Modified = true;
				m_Map.m_UndoModified++;
			}
		}
	}

	bool ShowColorBar = false;
	if(pEnvelope && pEnvelope->m_Channels == 4)
	{
		ShowColorBar = true;
		View.HSplitTop(20.0f, &ColorBar, &View);
		ColorBar.Margin(2.0f, &ColorBar);
		RenderBackground(ColorBar, ms_CheckerTexture, 16.0f, 1.0f);
	}

	RenderBackground(View, ms_CheckerTexture, 32.0f, 0.1f);

	if(pEnvelope)
	{
		static array<int> Selection;
		static int sEnvelopeEditorID = 0;
		static int s_ActiveChannels = 0xf;

		vec3 aColors[] = {vec3(1,0.2f,0.2f), vec3(0.2f,1,0.2f), vec3(0.2f,0.2f,1), vec3(1,1,0.2f)};

		if(pEnvelope)
		{
			CUIRect Button;

			ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);

			static const char *s_paNames[4][4] = {
				{"V", "", "", ""},
				{"", "", "", ""},
				{"X", "Y", "R", ""},
				{"R", "G", "B", "A"},
			};

			const char *paDescriptions[4][4] = {
				{"Volume of the envelope", "", "", ""},
				{"", "", "", ""},
				{"X-axis of the envelope", "Y-axis of the envelope", "Rotation of the envelope", ""},
				{"Red value of the envelope", "Green value of the envelope", "Blue value of the envelope", "Alpha value of the envelope"},
			};

			static int s_aChannelButtons[4] = {0};
			int Bit = 1;
			//ui_draw_button_func draw_func;

			for(int i = 0; i < pEnvelope->m_Channels; i++, Bit<<=1)
			{
				ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);

				/*if(i == 0) draw_func = draw_editor_button_l;
				else if(i == envelope->channels-1) draw_func = draw_editor_button_r;
				else draw_func = draw_editor_button_m;*/
				if(DoButton_Env(&s_aChannelButtons[i], s_paNames[pEnvelope->m_Channels-1][i], s_ActiveChannels&Bit, &Button, paDescriptions[pEnvelope->m_Channels-1][i], aColors[i]))
					s_ActiveChannels ^= Bit;
			}

			// sync checkbox
			ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);
			ToolBar.VSplitLeft(12.0f, &Button, &ToolBar);
			static int s_SyncButton;
			if(DoButton_Editor(&s_SyncButton, pEnvelope->m_Synchronized?"X":"", 0, &Button, 0, "Synchronize envelope animation to game time (restarts when you touch the start line)"))
				pEnvelope->m_Synchronized = !pEnvelope->m_Synchronized;

			ToolBar.VSplitLeft(4.0f, &Button, &ToolBar);
			ToolBar.VSplitLeft(80.0f, &Button, &ToolBar);
			UI()->DoLabel(&Button, "Synchronized", 10.0f, -1, -1);
		}

		float EndTime = pEnvelope->EndTime();
		if(EndTime < 1)
			EndTime = 1;

		pEnvelope->FindTopBottom(s_ActiveChannels);
		float Top = pEnvelope->m_Top;
		float Bottom = pEnvelope->m_Bottom;

		if(Top < 1)
			Top = 1;
		if(Bottom >= 0)
			Bottom = 0;

		float TimeScale = EndTime/View.w;
		float ValueScale = (Top-Bottom)/View.h;

		if(UI()->MouseInside(&View))
			UI()->SetHotItem(&sEnvelopeEditorID);

		if(UI()->HotItem() == &sEnvelopeEditorID)
		{
			// do stuff
			if(pEnvelope)
			{
				if(UI()->MouseButtonClicked(1))
				{
					// add point
					int Time = (int)(((UI()->MouseX()-View.x)*TimeScale)*1000.0f);
					//float env_y = (UI()->MouseY()-view.y)/TimeScale;
					float aChannels[4];
					pEnvelope->Eval(Time / 1000.0f, aChannels);
					pEnvelope->AddPoint(Time,
						f2fx(aChannels[0]), f2fx(aChannels[1]),
						f2fx(aChannels[2]), f2fx(aChannels[3]));
					m_Map.m_Modified = true;
					m_Map.m_UndoModified++;
				}

				m_ShowEnvelopePreview = 1;
				m_pTooltip = "Press right mouse button to create a new point";
			}
		}

		// render lines
		{
			UI()->ClipEnable(&View);
			Graphics()->TextureSet(-1);
			Graphics()->LinesBegin();
			for(int c = 0; c < pEnvelope->m_Channels; c++)
			{
				if(s_ActiveChannels&(1<<c))
					Graphics()->SetColor(aColors[c].r,aColors[c].g,aColors[c].b,1);
				else
					Graphics()->SetColor(aColors[c].r*0.5f,aColors[c].g*0.5f,aColors[c].b*0.5f,1);

				float PrevX = 0;
				float aResults[4];
				pEnvelope->Eval(0.000001f, aResults);
				float PrevValue = aResults[c];

				int Steps = (int)((View.w/UI()->Screen()->w) * Graphics()->ScreenWidth());
				for(int i = 1; i <= Steps; i++)
				{
					float a = i/(float)Steps;
					pEnvelope->Eval(a*EndTime, aResults);
					float v = aResults[c];
					v = (v-Bottom)/(Top-Bottom);

					IGraphics::CLineItem LineItem(View.x + PrevX*View.w, View.y+View.h - PrevValue*View.h, View.x + a*View.w, View.y+View.h - v*View.h);
					Graphics()->LinesDraw(&LineItem, 1);
					PrevX = a;
					PrevValue = v;
				}
			}
			Graphics()->LinesEnd();
			UI()->ClipDisable();
		}

		// render curve options
		{
			for(int i = 0; i < pEnvelope->m_lPoints.size()-1; i++)
			{
				float t0 = pEnvelope->m_lPoints[i].m_Time/1000.0f/EndTime;
				float t1 = pEnvelope->m_lPoints[i+1].m_Time/1000.0f/EndTime;

				//dbg_msg("", "%f", end_time);

				CUIRect v;
				v.x = CurveBar.x + (t0+(t1-t0)*0.5f) * CurveBar.w;
				v.y = CurveBar.y;
				v.h = CurveBar.h;
				v.w = CurveBar.h;
				v.x -= v.w/2;
				void *pID = &pEnvelope->m_lPoints[i].m_Curvetype;
				const char *paTypeName[] = {
					"N", "L", "S", "F", "M"
					};

				if(DoButton_Editor(pID, paTypeName[pEnvelope->m_lPoints[i].m_Curvetype], 0, &v, 0, "Switch curve type"))
					pEnvelope->m_lPoints[i].m_Curvetype = (pEnvelope->m_lPoints[i].m_Curvetype+1)%NUM_CURVETYPES;
			}
		}

		// render colorbar
		if(ShowColorBar)
		{
			Graphics()->TextureSet(-1);
			Graphics()->QuadsBegin();
			for(int i = 0; i < pEnvelope->m_lPoints.size()-1; i++)
			{
				float r0 = fx2f(pEnvelope->m_lPoints[i].m_aValues[0]);
				float g0 = fx2f(pEnvelope->m_lPoints[i].m_aValues[1]);
				float b0 = fx2f(pEnvelope->m_lPoints[i].m_aValues[2]);
				float a0 = fx2f(pEnvelope->m_lPoints[i].m_aValues[3]);
				float r1 = fx2f(pEnvelope->m_lPoints[i+1].m_aValues[0]);
				float g1 = fx2f(pEnvelope->m_lPoints[i+1].m_aValues[1]);
				float b1 = fx2f(pEnvelope->m_lPoints[i+1].m_aValues[2]);
				float a1 = fx2f(pEnvelope->m_lPoints[i+1].m_aValues[3]);

				IGraphics::CColorVertex Array[4] = {IGraphics::CColorVertex(0, r0, g0, b0, a0),
													IGraphics::CColorVertex(1, r1, g1, b1, a1),
													IGraphics::CColorVertex(2, r1, g1, b1, a1),
													IGraphics::CColorVertex(3, r0, g0, b0, a0)};
				Graphics()->SetColorVertex(Array, 4);

				float x0 = pEnvelope->m_lPoints[i].m_Time/1000.0f/EndTime;
//				float y0 = (fx2f(envelope->points[i].values[c])-bottom)/(top-bottom);
				float x1 = pEnvelope->m_lPoints[i+1].m_Time/1000.0f/EndTime;
				//float y1 = (fx2f(envelope->points[i+1].values[c])-bottom)/(top-bottom);
				CUIRect v;
				v.x = ColorBar.x + x0*ColorBar.w;
				v.y = ColorBar.y;
				v.w = (x1-x0)*ColorBar.w;
				v.h = ColorBar.h;

				IGraphics::CQuadItem QuadItem(v.x, v.y, v.w, v.h);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
			}
			Graphics()->QuadsEnd();
		}

		// render handles

		// keep track of last Env
		static void* s_pID = 0;

		// chars for textinput
		static char s_aStrCurTime[32] = "0.000";
		static char s_aStrCurValue[32] = "0.000";

		if(CurrentEnvelopeSwitched)
		{
			s_pID = 0;

			// update displayed text
			str_format(s_aStrCurTime, sizeof(s_aStrCurTime), "0.000");
			str_format(s_aStrCurValue, sizeof(s_aStrCurValue), "0.000");
		}

		{
			int CurrentValue = 0, CurrentTime = 0;

			Graphics()->TextureSet(-1);
			Graphics()->QuadsBegin();
			for(int c = 0; c < pEnvelope->m_Channels; c++)
			{
				if(!(s_ActiveChannels&(1<<c)))
					continue;

				for(int i = 0; i < pEnvelope->m_lPoints.size(); i++)
				{
					float x0 = pEnvelope->m_lPoints[i].m_Time/1000.0f/EndTime;
					float y0 = (fx2f(pEnvelope->m_lPoints[i].m_aValues[c])-Bottom)/(Top-Bottom);
					CUIRect Final;
					Final.x = View.x + x0*View.w;
					Final.y = View.y+View.h - y0*View.h;
					Final.x -= 2.0f;
					Final.y -= 2.0f;
					Final.w = 4.0f;
					Final.h = 4.0f;

					void *pID = &pEnvelope->m_lPoints[i].m_aValues[c];

					if(UI()->MouseInside(&Final))
						UI()->SetHotItem(pID);

					float ColorMod = 1.0f;

					if(UI()->ActiveItem() == pID)
					{
						if(!UI()->MouseButton(0))
						{
							m_SelectedQuadEnvelope = -1;
							m_SelectedEnvelopePoint = -1;

							UI()->SetActiveItem(0);
						}
						else
						{
							if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
							{
								if(i != 0)
								{
									if((Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL)))
										pEnvelope->m_lPoints[i].m_Time += (int)((m_MouseDeltaX));
									else
										pEnvelope->m_lPoints[i].m_Time += (int)((m_MouseDeltaX*TimeScale)*1000.0f);
									if(pEnvelope->m_lPoints[i].m_Time < pEnvelope->m_lPoints[i-1].m_Time)
										pEnvelope->m_lPoints[i].m_Time = pEnvelope->m_lPoints[i-1].m_Time + 1;
									if(i+1 != pEnvelope->m_lPoints.size() && pEnvelope->m_lPoints[i].m_Time > pEnvelope->m_lPoints[i+1].m_Time)
										pEnvelope->m_lPoints[i].m_Time = pEnvelope->m_lPoints[i+1].m_Time - 1;
								}
							}
							else
							{
								if((Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL)))
									pEnvelope->m_lPoints[i].m_aValues[c] -= f2fx(m_MouseDeltaY*0.001f);
								else
									pEnvelope->m_lPoints[i].m_aValues[c] -= f2fx(m_MouseDeltaY*ValueScale);
							}

							if (m_SelectedEnvelopePoint != i)
								m_Map.m_UndoModified++;

							m_SelectedQuadEnvelope = m_SelectedEnvelope;
							m_ShowEnvelopePreview = 1;
							m_SelectedEnvelopePoint = i;
							m_Map.m_Modified = true;
						}

						ColorMod = 100.0f;
						Graphics()->SetColor(1,1,1,1);
					}
					else if(UI()->HotItem() == pID)
					{
						if(UI()->MouseButton(0))
						{
							Selection.clear();
							Selection.add(i);
							UI()->SetActiveItem(pID);
							// track it
							s_pID = pID;
						}

						// remove point
						if(UI()->MouseButtonClicked(1))
						{
							if(s_pID == pID)
							{
								s_pID = 0;

								// update displayed text
								str_format(s_aStrCurTime, sizeof(s_aStrCurTime), "0.000");
								str_format(s_aStrCurValue, sizeof(s_aStrCurValue), "0.000");
							}

							pEnvelope->m_lPoints.remove_index(i);
							m_Map.m_Modified = true;
							m_Map.m_UndoModified++;
						}

						m_ShowEnvelopePreview = 1;
						ColorMod = 100.0f;
						Graphics()->SetColor(1,0.75f,0.75f,1);
						m_pTooltip = "Left mouse to drag. Hold ctrl to be more precise. Hold shift to alter time point aswell. Right click to delete.";
					}

					if(pID == s_pID && (Input()->KeyIsPressed(KEY_RETURN) || Input()->KeyIsPressed(KEY_KP_ENTER)))
					{
						if(i != 0)
						{
							pEnvelope->m_lPoints[i].m_Time = str_tofloat(s_aStrCurTime) * 1000.0f;

							if(pEnvelope->m_lPoints[i].m_Time < pEnvelope->m_lPoints[i-1].m_Time)
								pEnvelope->m_lPoints[i].m_Time = pEnvelope->m_lPoints[i-1].m_Time + 1;
							if(i+1 != pEnvelope->m_lPoints.size() && pEnvelope->m_lPoints[i].m_Time > pEnvelope->m_lPoints[i+1].m_Time)
								pEnvelope->m_lPoints[i].m_Time = pEnvelope->m_lPoints[i+1].m_Time - 1;
						}
						else
							pEnvelope->m_lPoints[i].m_Time = 0.0f;

						str_format(s_aStrCurTime, sizeof(s_aStrCurTime), "%.3f", pEnvelope->m_lPoints[i].m_Time/1000.0f);

						pEnvelope->m_lPoints[i].m_aValues[c] = f2fx(str_tofloat(s_aStrCurValue));
					}

					if(UI()->ActiveItem() == pID/* || UI()->HotItem() == pID*/)
					{
						CurrentTime = pEnvelope->m_lPoints[i].m_Time;
						CurrentValue = pEnvelope->m_lPoints[i].m_aValues[c];

						// update displayed text
						str_format(s_aStrCurTime, sizeof(s_aStrCurTime), "%.3f", CurrentTime/1000.0f);
						str_format(s_aStrCurValue, sizeof(s_aStrCurValue), "%.3f", fx2f(CurrentValue));
					}

					if (m_SelectedQuadEnvelope == m_SelectedEnvelope && m_SelectedEnvelopePoint == i)
						Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
					else
						Graphics()->SetColor(aColors[c].r*ColorMod, aColors[c].g*ColorMod, aColors[c].b*ColorMod, 1.0f);
					IGraphics::CQuadItem QuadItem(Final.x, Final.y, Final.w, Final.h);
					Graphics()->QuadsDrawTL(&QuadItem, 1);
				}
			}
			Graphics()->QuadsEnd();

			static float s_ValNumber = 0;
			static float s_TimeNumber = 0;

			CUIRect ToolBar1;
			CUIRect ToolBar2;

			ToolBar.VSplitMid(&ToolBar1, &ToolBar2);
			if (ToolBar.w > ToolBar.h * 21)
			{
				ToolBar1.VMargin(3.0f, &ToolBar1);
				ToolBar2.VMargin(3.0f, &ToolBar2);

				CUIRect Label1;
				CUIRect Label2;

				ToolBar1.VSplitMid(&Label1, &ToolBar1);
				ToolBar2.VSplitMid(&Label2, &ToolBar2);

				UI()->DoLabel(&Label1, "Value:", 10.0f, -1, -1);
				UI()->DoLabel(&Label2, "Time (in s):", 10.0f, -1, -1);
			}

			DoEditBox(&s_ValNumber, &ToolBar1, s_aStrCurValue, sizeof(s_aStrCurValue), 10.0f, &s_ValNumber);
			DoEditBox(&s_TimeNumber, &ToolBar2, s_aStrCurTime, sizeof(s_aStrCurTime), 10.0f, &s_TimeNumber);
		}
	}
}

void CEditor::RenderServerSettingsEditor(CUIRect View)
{
	static int s_CommandSelectedIndex = -1;

	CUIRect ToolBar;
	View.HSplitTop(20.0f, &ToolBar, &View);
	ToolBar.Margin(2.0f, &ToolBar);

	// do the toolbar
	{
		CUIRect Button;

		// command line
		ToolBar.VSplitLeft(5.0f, 0, &Button);
		UI()->DoLabel(&Button, "Command:", 12.0f, -1);

		Button.VSplitLeft(70.0f, 0, &Button);
		Button.VSplitLeft(180.0f, &Button, 0);
		DoEditBox(&m_CommandBox, &Button, m_aSettingsCommand, sizeof(m_aSettingsCommand), 12.0f, &m_CommandBox);

		// buttons
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_AddButton = 0;
		if(DoButton_Editor(&s_AddButton, "Add", 0, &Button, 0, "Add a command to command list.")
			|| ((Input()->KeyPress(KEY_RETURN) || Input()->KeyPress(KEY_KP_ENTER)) && UI()->LastActiveItem() == &m_CommandBox))
		{
			if(m_aSettingsCommand[0] != 0 && str_find(m_aSettingsCommand, " "))
			{
				bool Found = false;
				for(int i = 0; i < m_Map.m_lSettings.size(); i++)
					if(!str_comp(m_Map.m_lSettings[i].m_aCommand, m_aSettingsCommand))
					{
						Found = true;
						break;
					}

				if(!Found)
				{
					CEditorMap::CSetting Setting;
					str_copy(Setting.m_aCommand, m_aSettingsCommand, sizeof(Setting.m_aCommand));
					m_Map.m_lSettings.add(Setting);
				}
			}
		}

		if(m_Map.m_lSettings.size())
		{
			ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
			Button.VSplitRight(5.0f, &Button, 0);
			static int s_AddButton = 0;
			if(DoButton_Editor(&s_AddButton, "Del", 0, &Button, 0, "Delete a command from the command list.")
				|| Input()->KeyPress(KEY_DELETE))
				if(s_CommandSelectedIndex > -1 && s_CommandSelectedIndex < m_Map.m_lSettings.size())
					m_Map.m_lSettings.remove_index(s_CommandSelectedIndex);
		}
	}

	View.HSplitTop(2.0f, 0, &View);
	RenderBackground(View, ms_CheckerTexture, 32.0f, 0.1f);

	CUIRect ListBox;
	View.Margin(1.0f, &ListBox);

	float ListHeight = 17.0f * m_Map.m_lSettings.size();
	static int s_ScrollBar = 0;
	static float s_ScrollValue = 0;

	float ScrollDifference = ListHeight - ListBox.h;

	if(ListHeight > ListBox.h)	// Do we even need a scrollbar?
	{
		CUIRect Scroll;
		ListBox.VSplitRight(15.0f, &ListBox, &Scroll);
		ListBox.VSplitRight(3.0f, &ListBox, 0);	// extra spacing
		Scroll.HMargin(5.0f, &Scroll);
		s_ScrollValue = UiDoScrollbarV(&s_ScrollBar, &Scroll, s_ScrollValue);

		if(UI()->MouseInside(&Scroll) || UI()->MouseInside(&ListBox))
		{
			int ScrollNum = (int)((ListHeight-ListBox.h)/17.0f)+1;
			if(ScrollNum > 0)
			{
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
					s_ScrollValue = clamp(s_ScrollValue - 1.0f/ScrollNum, 0.0f, 1.0f);
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
					s_ScrollValue = clamp(s_ScrollValue + 1.0f/ScrollNum, 0.0f, 1.0f);
			}
			else
				ScrollNum = 0;
		}
	}

	float ListStartAt = ScrollDifference * s_ScrollValue;
	if(ListStartAt < 0.0f)
		ListStartAt = 0.0f;

	float ListStopAt = ListHeight - ScrollDifference * (1 - s_ScrollValue);
	float ListCur = 0;

	UI()->ClipEnable(&ListBox);
	for(int i = 0; i < m_Map.m_lSettings.size(); i++)
	{
		if(ListCur > ListStopAt)
			break;

		if(ListCur >= ListStartAt)
		{
			CUIRect Button;
			ListBox.HSplitTop(15.0f, &Button, &ListBox);
			ListBox.HSplitTop(2.0f, 0, &ListBox);
			Button.VSplitLeft(5.0f, 0, &Button);

			if(DoButton_MenuItem(&m_Map.m_lSettings[i], m_Map.m_lSettings[i].m_aCommand, s_CommandSelectedIndex == i, &Button, 0, 0))
				s_CommandSelectedIndex = i;
		}
		ListCur += 17.0f;
	}
	UI()->ClipDisable();
}

int CEditor::PopupMenuFile(CEditor *pEditor, CUIRect View)
{
	static int s_NewMapButton = 0;
	static int s_SaveButton = 0;
	static int s_SaveAsButton = 0;
	static int s_SaveCopyButton = 0;
	static int s_OpenButton = 0;
	static int s_OpenCurrentMapButton = 0;
	static int s_AppendButton = 0;
	static int s_ExitButton = 0;

	CUIRect Slot;
	View.HSplitTop(2.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_NewMapButton, "New", 0, &Slot, 0, "Creates a new map"))
	{
		if(pEditor->HasUnsavedData())
		{
			pEditor->m_PopupEventType = POPEVENT_NEW;
			pEditor->m_PopupEventActivated = true;
		}
		else
		{
			pEditor->Reset();
			pEditor->m_aFileName[0] = 0;
		}
		return 1;
	}

	View.HSplitTop(10.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_OpenButton, "Load", 0, &Slot, 0, "Opens a map for editing"))
	{
		if(pEditor->HasUnsavedData())
		{
			pEditor->m_PopupEventType = POPEVENT_LOAD;
			pEditor->m_PopupEventActivated = true;
		}
		else
			pEditor->InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_MAP, "Load map", "Load", "maps", "", pEditor->CallbackOpenMap, pEditor);
		return 1;
	}

	View.HSplitTop(2.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_OpenCurrentMapButton, "Load Current Map", 0, &Slot, 0, "Opens the current in game map for editing"))
	{
		if(pEditor->HasUnsavedData())
		{
			pEditor->m_PopupEventType = POPEVENT_LOADCURRENT;
			pEditor->m_PopupEventActivated = true;
		}
		else
		{
			pEditor->LoadCurrentMap();
		}
		return 1;
	}

	View.HSplitTop(10.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_AppendButton, "Append", 0, &Slot, 0, "Opens a map and adds everything from that map to the current one"))
	{
		pEditor->InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_MAP, "Append map", "Append", "maps", "", pEditor->CallbackAppendMap, pEditor);
		return 1;
	}

	View.HSplitTop(10.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_SaveButton, "Save", 0, &Slot, 0, "Saves the current map (ctrl+s)"))
	{
		if(pEditor->m_aFileName[0] && pEditor->m_ValidSaveFilename)
		{
			str_copy(pEditor->m_aFileSaveName, pEditor->m_aFileName, sizeof(pEditor->m_aFileSaveName));
			pEditor->m_PopupEventType = POPEVENT_SAVE;
			pEditor->m_PopupEventActivated = true;
		}
		else
			pEditor->InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save", "maps", "", pEditor->CallbackSaveMap, pEditor);
		return 1;
	}

	View.HSplitTop(2.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_SaveAsButton, "Save As", 0, &Slot, 0, "Saves the current map under a new name (ctrl+shift+s)"))
	{
		pEditor->InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save", "maps", "", pEditor->CallbackSaveMap, pEditor);
		return 1;
	}

	View.HSplitTop(2.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_SaveCopyButton, "Save Copy", 0, &Slot, 0, "Saves a copy of the current map under a new name (ctrl+shift+alt+s)"))
	{
		pEditor->InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save", "maps", "", pEditor->CallbackSaveCopyMap, pEditor);
		return 1;
	}

	View.HSplitTop(10.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_ExitButton, "Exit", 0, &Slot, 0, "Exits from the editor"))
	{
		if(pEditor->HasUnsavedData())
		{
			pEditor->m_PopupEventType = POPEVENT_EXIT;
			pEditor->m_PopupEventActivated = true;
		}
		else
			g_Config.m_ClEditor = 0;
		return 1;
	}

	return 0;
}

void CEditor::RenderMenubar(CUIRect MenuBar)
{
	static CUIRect s_File /*, view, help*/;

	MenuBar.VSplitLeft(60.0f, &s_File, &MenuBar);
	if(DoButton_Menu(&s_File, "File", 0, &s_File, 0, 0))
		UiInvokePopupMenu(&s_File, 1, s_File.x, s_File.y+s_File.h-1.0f, 120, 160, PopupMenuFile, this);

	/*
	menubar.VSplitLeft(5.0f, 0, &menubar);
	menubar.VSplitLeft(60.0f, &view, &menubar);
	if(do_editor_button(&view, "View", 0, &view, draw_editor_button_menu, 0, 0))
		(void)0;

	menubar.VSplitLeft(5.0f, 0, &menubar);
	menubar.VSplitLeft(60.0f, &help, &menubar);
	if(do_editor_button(&help, "Help", 0, &help, draw_editor_button_menu, 0, 0))
		(void)0;
		*/

	CUIRect Info, Close;
	MenuBar.VSplitLeft(40.0f, 0, &MenuBar);
	MenuBar.VSplitRight(20.0f, &MenuBar, &Close);
	Close.VSplitLeft(5.0f, 0, &Close);
	MenuBar.VSplitLeft(MenuBar.w*0.75f, &MenuBar, &Info);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "File: %s", m_aFileName);
	UI()->DoLabel(&MenuBar, aBuf, 10.0f, -1, -1);

	time_t rawtime;
	struct tm *timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);

	str_format(aBuf, sizeof(aBuf), "X: %i, Y: %i, Z: %i, A: %.1f, G: %i  %02d:%02d", (int)UI()->MouseWorldX()/32, (int)UI()->MouseWorldY()/32, m_ZoomLevel, m_AnimateSpeed, m_GridFactor, timeinfo->tm_hour, timeinfo->tm_min);
	UI()->DoLabel(&Info, aBuf, 10.0f, 1, -1);

	static int s_CloseButton = 0;
	if(DoButton_Editor(&s_CloseButton, "×", 0, &Close, 0, "Exits from the editor"))
	{
		if(HasUnsavedData())
		{
			m_PopupEventType = POPEVENT_EXIT;
			m_PopupEventActivated = true;
		}
		else
			g_Config.m_ClEditor = 0;
	}
}

void CEditor::Render()
{
	// basic start
	Graphics()->Clear(1.0f, 0.0f, 1.0f);
	CUIRect View = *UI()->Screen();
	Graphics()->MapScreen(UI()->Screen()->x, UI()->Screen()->y, UI()->Screen()->w, UI()->Screen()->h);

	float Width = View.w;
	float Height = View.h;

	// reset tip
	m_pTooltip = 0;

	if(m_EditBoxActive)
		--m_EditBoxActive;

	// render checker
	RenderBackground(View, ms_CheckerTexture, 32.0f, 1.0f);

	CUIRect MenuBar, CModeBar, ToolBar, StatusBar, ExtraEditor, UndoList, ToolBox;
	m_ShowPicker = Input()->KeyIsPressed(KEY_SPACE) != 0 && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0 && UI()->LastActiveItem() != &m_CommandBox;

	if(m_GuiActive)
	{

		View.HSplitTop(16.0f, &MenuBar, &View);
		View.HSplitTop(53.0f, &ToolBar, &View);
		View.VSplitLeft(100.0f, &ToolBox, &View);
		View.HSplitBottom(16.0f, &View, &StatusBar);

		if(m_ShowEnvelopeEditor && !m_ShowPicker)
		{
			float Size = 125.0f;
			if(m_ShowEnvelopeEditor == 2)
				Size *= 2.0f;
			else if(m_ShowEnvelopeEditor == 3)
				Size *= 3.0f;
			View.HSplitBottom(Size, &View, &ExtraEditor);
		}
		if (m_ShowUndo && !m_ShowPicker)
		{
			View.HSplitBottom(250.0f, &View, &UndoList);
		}

		if(m_ShowServerSettingsEditor && !m_ShowPicker)
			View.HSplitBottom(250.0f, &View, &ExtraEditor);
	}

	//	a little hack for now
	if(m_Mode == MODE_LAYERS)
		DoMapEditor(View, ToolBar);

	// do zooming
	if(Input()->KeyPress(KEY_KP_MINUS) && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0)
		m_ZoomLevel += 50;
	if(Input()->KeyPress(KEY_KP_PLUS) && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0)
		m_ZoomLevel -= 50;
	if(Input()->KeyPress(KEY_KP_MULTIPLY) && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0)
	{
		m_EditorOffsetX = 0;
		m_EditorOffsetY = 0;
		m_ZoomLevel = 100;
	}
	if(m_Dialog == DIALOG_NONE && UI()->MouseInside(&View))
	{
		// Determines in which direction to zoom.
		int Zoom = 0;
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
			Zoom--;
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
			Zoom++;

		if(Zoom != 0)
		{
			float OldLevel = m_ZoomLevel;
			m_ZoomLevel = clamp(m_ZoomLevel + Zoom * 20, 50, 2000);
			if(g_Config.m_EdZoomTarget)
				ZoomMouseTarget((float)m_ZoomLevel / OldLevel);
		}
	}

	m_ZoomLevel = clamp(m_ZoomLevel, 50, 2000);
	m_WorldZoom = m_ZoomLevel/100.0f;
	float Brightness = 0.25f;

	if(m_GuiActive)
	{
		RenderBackground(MenuBar, ms_BackgroundTexture, 128.0f, Brightness*0);
		MenuBar.Margin(2.0f, &MenuBar);

		RenderBackground(ToolBox, ms_BackgroundTexture, 128.0f, Brightness);
		ToolBox.Margin(2.0f, &ToolBox);

		RenderBackground(ToolBar, ms_BackgroundTexture, 128.0f, Brightness);
		ToolBar.Margin(2.0f, &ToolBar);
		ToolBar.VSplitLeft(100.0f, &CModeBar, &ToolBar);

		RenderBackground(StatusBar, ms_BackgroundTexture, 128.0f, Brightness);
		StatusBar.Margin(2.0f, &StatusBar);
	}
	else
	{
		ToolBar.y = -300;
	}

	// do the toolbar
	if(m_Mode == MODE_LAYERS)
		DoToolbar(ToolBar);

	if(m_GuiActive)
	{
		if(m_ShowEnvelopeEditor || m_ShowServerSettingsEditor)
		{
			RenderBackground(ExtraEditor, ms_BackgroundTexture, 128.0f, Brightness);
			ExtraEditor.Margin(2.0f, &ExtraEditor);
		}
		if(m_ShowUndo)
		{
			RenderBackground(UndoList, ms_BackgroundTexture, 128.0f, Brightness);
			UndoList.Margin(2.0f, &UndoList);
		}
	}


	if(m_Mode == MODE_LAYERS)
		RenderLayers(ToolBox, ToolBar, View);
	else if(m_Mode == MODE_IMAGES)
		RenderImages(ToolBox, ToolBar, View);
	else if(m_Mode == MODE_SOUNDS)
		RenderSounds(ToolBox, ToolBar, View);

	Graphics()->MapScreen(UI()->Screen()->x, UI()->Screen()->y, UI()->Screen()->w, UI()->Screen()->h);

	if(m_GuiActive)
	{
		RenderMenubar(MenuBar);

		RenderModebar(CModeBar);
		if(!m_ShowPicker)
		{
			if(m_ShowEnvelopeEditor)
				RenderEnvelopeEditor(ExtraEditor);
			if(m_ShowUndo)
				RenderUndoList(UndoList);
			if(m_ShowServerSettingsEditor)
				RenderServerSettingsEditor(ExtraEditor);
		}
	}

	if(m_Dialog == DIALOG_FILE)
	{
		static int s_NullUiTarget = 0;
		UI()->SetHotItem(&s_NullUiTarget);
		RenderFileDialog();
	}

	if(m_PopupEventActivated)
	{
		static int s_PopupID = 0;
		UiInvokePopupMenu(&s_PopupID, 0, Width/2.0f-200.0f, Height/2.0f-100.0f, 400.0f, 200.0f, PopupEvent);
		m_PopupEventActivated = false;
		m_PopupEventWasActivated = true;
	}


	UiDoPopupMenu();

	if(m_GuiActive)
		RenderStatusbar(StatusBar);

	//
	if(g_Config.m_EdShowkeys)
	{
		Graphics()->MapScreen(UI()->Screen()->x, UI()->Screen()->y, UI()->Screen()->w, UI()->Screen()->h);
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, View.x+10, View.y+View.h-24-10, 24.0f, TEXTFLAG_RENDER);

		int NKeys = 0;
		for(int i = 0; i < KEY_LAST; i++)
		{
			if(Input()->KeyIsPressed(i))
			{
				if(NKeys)
					TextRender()->TextEx(&Cursor, " + ", -1);
				TextRender()->TextEx(&Cursor, Input()->KeyName(i), -1);
				NKeys++;
			}
		}
	}

	if(m_ShowMousePointer)
	{
		// render butt ugly mouse cursor
		float mx = UI()->MouseX();
		float my = UI()->MouseY();
		Graphics()->TextureSet(ms_CursorTexture);
		Graphics()->QuadsBegin();
		if(ms_pUiGotContext == UI()->HotItem())
			Graphics()->SetColor(1,0,0,1);
		IGraphics::CQuadItem QuadItem(mx,my, 16.0f, 16.0f);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}
}

static int UndoStepsListdirCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	IStorage *pStorage = (IStorage *)pUser;
	if (str_comp_nocase_num(pName, "undo_", 5) == 0)
	{
		char aBuffer[1024];
		pStorage->GetCompletePath(IStorage::TYPE_SAVE, "editor/", aBuffer, sizeof(aBuffer));
		str_append(aBuffer, pName, sizeof(aBuffer));
		fs_remove(aBuffer);
	}
	return 0;
}

void CEditor::Reset(bool CreateDefault)
{
	m_Map.Clean();

	//delete undo file
	char aBuffer[1024];
	m_pStorage->GetCompletePath(IStorage::TYPE_SAVE, "editor/", aBuffer, sizeof(aBuffer));
	fs_listdir(aBuffer, UndoStepsListdirCallback, 0, m_pStorage);
	m_lUndoSteps.clear();

	// create default layers
	if(CreateDefault)
		m_Map.CreateDefault(ms_EntitiesTexture);

	m_SelectedLayer = 0;
	m_SelectedGroup = 0;
	m_SelectedQuad = -1;
	m_SelectedPoints = 0;
	m_SelectedEnvelope = 0;
	m_SelectedImage = 0;
	m_SelectedSound = 0;
	m_SelectedSource = -1;

	m_WorldOffsetX = 0;
	m_WorldOffsetY = 0;
	m_EditorOffsetX = 0.0f;
	m_EditorOffsetY = 0.0f;

	m_WorldZoom = 1.0f;
	m_ZoomLevel = 200;

	m_MouseDeltaX = 0;
	m_MouseDeltaY = 0;
	m_MouseDeltaWx = 0;
	m_MouseDeltaWy = 0;

	m_Map.m_Modified = false;
	m_Map.m_UndoModified = 0;
	m_LastUndoUpdateTime = time_get();
	m_UndoRunning = false;

	m_ShowEnvelopePreview = 0;
	m_ShiftBy = 1;

	m_Map.m_Modified = false;
	m_Map.m_UndoModified = 0;
	m_LastUndoUpdateTime = time_get();
}

int CEditor::GetLineDistance()
{
	int LineDistance = 512;

	if(m_ZoomLevel <= 100)
		LineDistance = 16;
	else if(m_ZoomLevel <= 250)
		LineDistance = 32;
	else if(m_ZoomLevel <= 450)
		LineDistance = 64;
	else if(m_ZoomLevel <= 850)
		LineDistance = 128;
	else if(m_ZoomLevel <= 1550)
		LineDistance = 256;

	return LineDistance;
}

void CEditor::ZoomMouseTarget(float ZoomFactor)
{
	// zoom to the current mouse position
	// get absolute mouse position
	float aPoints[4];
	RenderTools()->MapscreenToWorld(
		m_WorldOffsetX, m_WorldOffsetY,
		1.0f, 1.0f, 0.0f, 0.0f, Graphics()->ScreenAspect(), m_WorldZoom, aPoints);

	float WorldWidth = aPoints[2]-aPoints[0];
	float WorldHeight = aPoints[3]-aPoints[1];

	float Mwx = aPoints[0] + WorldWidth * (UI()->MouseX()/UI()->Screen()->w);
	float Mwy = aPoints[1] + WorldHeight * (UI()->MouseY()/UI()->Screen()->h);

	// adjust camera
	m_WorldOffsetX += (Mwx-m_WorldOffsetX) * (1-ZoomFactor);
	m_WorldOffsetY += (Mwy-m_WorldOffsetY) * (1-ZoomFactor);
}

void CEditorMap::DeleteEnvelope(int Index)
{
	if(Index < 0 || Index >= m_lEnvelopes.size())
		return;

	m_Modified = true;
	m_UndoModified++;

	// fix links between envelopes and quads
	for(int i = 0; i < m_lGroups.size(); ++i)
		for(int j = 0; j < m_lGroups[i]->m_lLayers.size(); ++j)
			if(m_lGroups[i]->m_lLayers[j]->m_Type == LAYERTYPE_QUADS)
			{
				CLayerQuads *pLayer = static_cast<CLayerQuads *>(m_lGroups[i]->m_lLayers[j]);
				for(int k = 0; k < pLayer->m_lQuads.size(); ++k)
				{
					if(pLayer->m_lQuads[k].m_PosEnv == Index)
						pLayer->m_lQuads[k].m_PosEnv = -1;
					else if(pLayer->m_lQuads[k].m_PosEnv > Index)
						pLayer->m_lQuads[k].m_PosEnv--;
					if(pLayer->m_lQuads[k].m_ColorEnv == Index)
						pLayer->m_lQuads[k].m_ColorEnv = -1;
					else if(pLayer->m_lQuads[k].m_ColorEnv > Index)
						pLayer->m_lQuads[k].m_ColorEnv--;
				}
			}
			else if(m_lGroups[i]->m_lLayers[j]->m_Type == LAYERTYPE_TILES)
			{
				CLayerTiles *pLayer = static_cast<CLayerTiles *>(m_lGroups[i]->m_lLayers[j]);
				if(pLayer->m_ColorEnv == Index)
					pLayer->m_ColorEnv = -1;
				if(pLayer->m_ColorEnv > Index)
					pLayer->m_ColorEnv--;
			}

	m_lEnvelopes.remove_index(Index);
}

void CEditorMap::MakeGameLayer(CLayer *pLayer)
{
	m_pGameLayer = (CLayerGame *)pLayer;
	m_pGameLayer->m_pEditor = m_pEditor;
	m_pGameLayer->m_TexID = m_pEditor->ms_EntitiesTexture;
}

void CEditorMap::MakeGameGroup(CLayerGroup *pGroup)
{
	m_pGameGroup = pGroup;
	m_pGameGroup->m_GameGroup = true;
	str_copy(m_pGameGroup->m_aName, "Game", sizeof(m_pGameGroup->m_aName));
}



void CEditorMap::Clean()
{
	m_lGroups.delete_all();
	m_lEnvelopes.delete_all();
	m_lImages.delete_all();
	m_lSounds.delete_all();

	m_MapInfo.Reset();

	m_lSettings.clear();

	m_pGameLayer = 0x0;
	m_pGameGroup = 0x0;

	m_Modified = false;

	m_pTeleLayer = 0x0;
	m_pSpeedupLayer = 0x0;
	m_pFrontLayer = 0x0;
	m_pSwitchLayer = 0x0;
	m_pTuneLayer = 0x0;
}

void CEditorMap::CreateDefault(int EntitiesTexture)
{
	// add background
	CLayerGroup *pGroup = NewGroup();
	pGroup->m_ParallaxX = 0;
	pGroup->m_ParallaxY = 0;
	CLayerQuads *pLayer = new CLayerQuads;
	pLayer->m_pEditor = m_pEditor;
	CQuad *pQuad = pLayer->NewQuad();
	const int Width = 800000;
	const int Height = 600000;
	pQuad->m_aPoints[0].x = pQuad->m_aPoints[2].x = -Width;
	pQuad->m_aPoints[1].x = pQuad->m_aPoints[3].x = Width;
	pQuad->m_aPoints[0].y = pQuad->m_aPoints[1].y = -Height;
	pQuad->m_aPoints[2].y = pQuad->m_aPoints[3].y = Height;
	pQuad->m_aColors[0].r = pQuad->m_aColors[1].r = 94;
	pQuad->m_aColors[0].g = pQuad->m_aColors[1].g = 132;
	pQuad->m_aColors[0].b = pQuad->m_aColors[1].b = 174;
	pQuad->m_aColors[2].r = pQuad->m_aColors[3].r = 204;
	pQuad->m_aColors[2].g = pQuad->m_aColors[3].g = 232;
	pQuad->m_aColors[2].b = pQuad->m_aColors[3].b = 255;
	pGroup->AddLayer(pLayer);

	// add game layer and reset front, tele, speedup, tune and switch layer pointers
	MakeGameGroup(NewGroup());
	MakeGameLayer(new CLayerGame(50, 50));
	m_pGameGroup->AddLayer(m_pGameLayer);

	m_pFrontLayer = 0x0;
	m_pTeleLayer = 0x0;
	m_pSpeedupLayer = 0x0;
	m_pSwitchLayer = 0x0;
	m_pTuneLayer = 0x0;
}

void CEditor::Init()
{
	m_pInput = Kernel()->RequestInterface<IInput>();
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pGraphics = Kernel()->RequestInterface<IGraphics>();
	m_pTextRender = Kernel()->RequestInterface<ITextRender>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pSound = Kernel()->RequestInterface<ISound>();
	m_RenderTools.m_pGraphics = m_pGraphics;
	m_RenderTools.m_pUI = &m_UI;
	m_UI.SetGraphics(m_pGraphics, m_pTextRender);
	m_Map.m_pEditor = this;

	ms_CheckerTexture = Graphics()->LoadTexture("editor/checker.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	ms_BackgroundTexture = Graphics()->LoadTexture("editor/background.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	ms_CursorTexture = Graphics()->LoadTexture("editor/cursor.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	ms_EntitiesTexture = Graphics()->LoadTexture("editor/entities.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);

	ms_FrontTexture = Graphics()->LoadTexture("editor/front.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	ms_TeleTexture = Graphics()->LoadTexture("editor/tele.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	ms_SpeedupTexture = Graphics()->LoadTexture("editor/speedup.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	ms_SwitchTexture = Graphics()->LoadTexture("editor/switch.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	ms_TuneTexture = Graphics()->LoadTexture("editor/tune.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);

	m_TilesetPicker.m_pEditor = this;
	m_TilesetPicker.MakePalette();
	m_TilesetPicker.m_Readonly = true;

	m_QuadsetPicker.m_pEditor = this;
	m_QuadsetPicker.NewQuad();
	m_QuadsetPicker.m_Readonly = true;

	m_Brush.m_pMap = &m_Map;

	Reset();
	m_Map.m_Modified = false;
	m_Map.m_UndoModified = 0;
	m_LastUndoUpdateTime = time_get();

	ms_PickerColor = vec3(1.0f, 0.0f, 0.0f);
}

void CEditor::DoMapBorder()
{
	CLayerTiles *pT = (CLayerTiles *)GetSelectedLayerType(0, LAYERTYPE_TILES);

	for(int i = 0; i < pT->m_Width*2; ++i)
		pT->m_pTiles[i].m_Index = 1;

	for(int i = 0; i < pT->m_Width*pT->m_Height; ++i)
	{
		if(i%pT->m_Width < 2 || i%pT->m_Width > pT->m_Width-3)
			pT->m_pTiles[i].m_Index = 1;
	}

	for(int i = (pT->m_Width*(pT->m_Height-2)); i < pT->m_Width*pT->m_Height; ++i)
		pT->m_pTiles[i].m_Index = 1;
}

void CEditor::CreateUndoStep()
{
	void *CreateThread = thread_init(CreateUndoStepThread, this);
	thread_detach(CreateThread);
}

void CEditor::CreateUndoStepThread(void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;

	CUndo NewStep;
	str_timestamp(NewStep.m_aName, sizeof(NewStep.m_aName));
	if (pEditor->m_lUndoSteps.size())
		NewStep.m_FileNum = pEditor->m_lUndoSteps[pEditor->m_lUndoSteps.size() - 1].m_FileNum + 1;
	else
		NewStep.m_FileNum = 0;
	NewStep.m_PreviewImage = 0;

	char aBuffer[1024];
	str_format(aBuffer, sizeof(aBuffer), "editor/undo_%i.png", NewStep.m_FileNum);
	pEditor->Graphics()->TakeCustomScreenshot(aBuffer);

	str_format(aBuffer, sizeof(aBuffer), "editor/undo_%i", NewStep.m_FileNum);
	pEditor->Save(aBuffer);

	pEditor->m_lUndoSteps.add(NewStep);
	pEditor->m_UndoRunning = false;
}

void CEditor::UpdateAndRender()
{
	static float s_MouseX = 0.0f;
	static float s_MouseY = 0.0f;

	if(m_Animate)
		m_AnimateTime = (time_get()-m_AnimateStart)/(float)time_freq();
	else
		m_AnimateTime = 0;
	ms_pUiGotContext = 0;

	// handle mouse movement
	float mx, my, Mwx, Mwy;
	float rx, ry;
	{
		Input()->MouseRelative(&rx, &ry);
#if defined(__ANDROID__)
		float tx, ty;
		tx = s_MouseX;
		ty = s_MouseY;

		s_MouseX = (rx / (float)Graphics()->ScreenWidth()) * UI()->Screen()->w;
		s_MouseY = (ry / (float)Graphics()->ScreenHeight()) * UI()->Screen()->h;

		s_MouseX = clamp(s_MouseX, 0.0f, UI()->Screen()->w);
		s_MouseY = clamp(s_MouseY, 0.0f, UI()->Screen()->h);

		m_MouseDeltaX = s_MouseX - m_OldMouseX;
		m_MouseDeltaY = s_MouseY - m_OldMouseY;
		m_OldMouseX = tx;
		m_OldMouseY = ty;
#else
		UI()->ConvertMouseMove(&rx, &ry);
		m_MouseDeltaX = rx;
		m_MouseDeltaY = ry;

		if(!m_LockMouse)
		{
			s_MouseX += rx;
			s_MouseY += ry;
		}

		s_MouseX = clamp(s_MouseX, 0.0f, UI()->Screen()->w);
		s_MouseY = clamp(s_MouseY, 0.0f, UI()->Screen()->h);
#endif

		// update the ui
		mx = s_MouseX;
		my = s_MouseY;
		Mwx = 0;
		Mwy = 0;

		// fix correct world x and y
		CLayerGroup *g = GetSelectedGroup();
		if(g)
		{
			float aPoints[4];
			g->Mapping(aPoints);

			float WorldWidth = aPoints[2]-aPoints[0];
			float WorldHeight = aPoints[3]-aPoints[1];

			Mwx = aPoints[0] + WorldWidth * (s_MouseX/UI()->Screen()->w);
			Mwy = aPoints[1] + WorldHeight * (s_MouseY/UI()->Screen()->h);
			m_MouseDeltaWx = m_MouseDeltaX*(WorldWidth / UI()->Screen()->w);
			m_MouseDeltaWy = m_MouseDeltaY*(WorldHeight / UI()->Screen()->h);
		}

		int Buttons = 0;
		if(Input()->KeyIsPressed(KEY_MOUSE_1)) Buttons |= 1;
		if(Input()->KeyIsPressed(KEY_MOUSE_2)) Buttons |= 2;
		if(Input()->KeyIsPressed(KEY_MOUSE_3)) Buttons |= 4;

#if defined(__ANDROID__)
	static int ButtonsOneFrameDelay = 0; // For Android touch input

	UI()->Update(mx,my,Mwx,Mwy,ButtonsOneFrameDelay);
	ButtonsOneFrameDelay = Buttons;
#else
		UI()->Update(mx,my,Mwx,Mwy,Buttons);
#endif
	}

	// toggle gui
	if(Input()->KeyPress(KEY_TAB))
		m_GuiActive = !m_GuiActive;

	if(Input()->KeyPress(KEY_F10))
		m_ShowMousePointer = false;

	if (g_Config.m_ClEditorUndo)
	{
		// Screenshot at most every 5 seconds, at least every 60
		if ((m_LastUndoUpdateTime + time_freq() * 60 < time_get() && m_Map.m_UndoModified)
		||  (m_LastUndoUpdateTime + time_freq() *  5 < time_get() && m_Map.m_UndoModified >= 10))
		{
			m_Map.m_UndoModified = 0;
			m_LastUndoUpdateTime = time_get();

			if (!m_UndoRunning)
			{
				m_UndoRunning = true;
				CreateUndoStep();
			}
		}
	}
	Render();

	if(Input()->KeyPress(KEY_F10))
	{
		Graphics()->TakeScreenshot(0);
		m_ShowMousePointer = true;
	}

	Input()->Clear();
}

IEditor *CreateEditor() { return new CEditor; }

// DDRace

void CEditorMap::MakeTeleLayer(CLayer *pLayer)
{
	m_pTeleLayer = (CLayerTele *)pLayer;
	m_pTeleLayer->m_pEditor = m_pEditor;
	m_pTeleLayer->m_TexID = m_pEditor->ms_TeleTexture;
}

void CEditorMap::MakeSpeedupLayer(CLayer *pLayer)
{
	m_pSpeedupLayer = (CLayerSpeedup *)pLayer;
	m_pSpeedupLayer->m_pEditor = m_pEditor;
	m_pSpeedupLayer->m_TexID = m_pEditor->ms_SpeedupTexture;
}

void CEditorMap::MakeFrontLayer(CLayer *pLayer)
{
	m_pFrontLayer = (CLayerFront *)pLayer;
	m_pFrontLayer->m_pEditor = m_pEditor;
	m_pFrontLayer->m_TexID = m_pEditor->ms_FrontTexture;
}

void CEditorMap::MakeSwitchLayer(CLayer *pLayer)
{
	m_pSwitchLayer = (CLayerSwitch *)pLayer;
	m_pSwitchLayer->m_pEditor = m_pEditor;
	m_pSwitchLayer->m_TexID = m_pEditor->ms_SwitchTexture;
}

void CEditorMap::MakeTuneLayer(CLayer *pLayer)
{
	m_pTuneLayer = (CLayerTune *)pLayer;
	m_pTuneLayer->m_pEditor = m_pEditor;
	m_pTuneLayer->m_TexID = m_pEditor->ms_TuneTexture;
}
