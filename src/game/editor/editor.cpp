/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <algorithm>

#include <base/color.h>
#include <base/system.h>

#if defined(CONF_FAMILY_UNIX)
#include <pthread.h>
#endif

#include <engine/client.h>
#include <engine/console.h>
#include <engine/gfx/image_manipulation.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/components/camera.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>
#include <game/generated/client_data.h>
#include <game/localization.h>

#include "auto_map.h"
#include "editor.h"

#include <limits>

static const char *VANILLA_IMAGES[] = {
	"bg_cloud1",
	"bg_cloud2",
	"bg_cloud3",
	"desert_doodads",
	"desert_main",
	"desert_mountains",
	"desert_mountains2",
	"desert_sun",
	"generic_deathtiles",
	"generic_unhookable",
	"grass_doodads",
	"grass_main",
	"jungle_background",
	"jungle_deathtiles",
	"jungle_doodads",
	"jungle_main",
	"jungle_midground",
	"jungle_unhookables",
	"moon",
	"mountains",
	"snow",
	"stars",
	"sun",
	"winter_doodads",
	"winter_main",
	"winter_mountains",
	"winter_mountains2",
	"winter_mountains3"};

static bool IsVanillaImage(const char *pImage)
{
	return std::any_of(std::begin(VANILLA_IMAGES), std::end(VANILLA_IMAGES), [pImage](const char *pVanillaImage) { return str_comp(pImage, pVanillaImage) == 0; });
}

const void *CEditor::ms_pUiGotContext;

ColorHSVA CEditor::ms_PickerColor;
int CEditor::ms_SVPicker;
int CEditor::ms_HuePicker;

enum
{
	BUTTON_CONTEXT = 1,
};

CEditorImage::~CEditorImage()
{
	m_pEditor->Graphics()->UnloadTexture(&m_Texture);
	free(m_pData);
	m_pData = nullptr;
}

CEditorSound::~CEditorSound()
{
	m_pEditor->Sound()->UnloadSample(m_SoundID);
	free(m_pData);
	m_pData = nullptr;
}

CLayerGroup::CLayerGroup()
{
	m_aName[0] = 0;
	m_Visible = true;
	m_Collapse = false;
	m_GameGroup = false;
	m_OffsetX = 0;
	m_OffsetY = 0;
	m_ParallaxX = 100;
	m_ParallaxY = 100;
	m_ParallaxZoom = 100;

	m_UseClipping = 0;
	m_ClipX = 0;
	m_ClipY = 0;
	m_ClipW = 0;
	m_ClipH = 0;
}

template<typename T>
static void DeleteAll(std::vector<T> &vList)
{
	for(auto &Item : vList)
		delete Item;
	vList.clear();
}

CLayerGroup::~CLayerGroup()
{
	DeleteAll(m_vpLayers);
}

void CLayerGroup::Convert(CUIRect *pRect)
{
	pRect->x += m_OffsetX;
	pRect->y += m_OffsetY;
}

void CLayerGroup::Mapping(float *pPoints)
{
	float ParallaxZoom = m_pMap->m_pEditor->m_PreviewZoom ? m_ParallaxZoom : 100.0f;

	m_pMap->m_pEditor->RenderTools()->MapScreenToWorld(
		m_pMap->m_pEditor->m_WorldOffsetX, m_pMap->m_pEditor->m_WorldOffsetY,
		m_ParallaxX, m_ParallaxY, ParallaxZoom, m_OffsetX, m_OffsetY,
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
		float x0 = (m_ClipX - aPoints[0]) / (aPoints[2] - aPoints[0]);
		float y0 = (m_ClipY - aPoints[1]) / (aPoints[3] - aPoints[1]);
		float x1 = ((m_ClipX + m_ClipW) - aPoints[0]) / (aPoints[2] - aPoints[0]);
		float y1 = ((m_ClipY + m_ClipH) - aPoints[1]) / (aPoints[3] - aPoints[1]);

		pGraphics->ClipEnable((int)(x0 * pGraphics->ScreenWidth()), (int)(y0 * pGraphics->ScreenHeight()),
			(int)((x1 - x0) * pGraphics->ScreenWidth()), (int)((y1 - y0) * pGraphics->ScreenHeight()));
	}

	for(auto &pLayer : m_vpLayers)
	{
		if(pLayer->m_Visible)
		{
			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CLayerTiles *pTiles = static_cast<CLayerTiles *>(pLayer);
				if(pTiles->m_Game || pTiles->m_Front || pTiles->m_Tele || pTiles->m_Speedup || pTiles->m_Tune || pTiles->m_Switch)
					continue;
			}
			if(m_pMap->m_pEditor->m_ShowDetail || !(pLayer->m_Flags & LAYERFLAG_DETAIL))
				pLayer->Render();
		}
	}

	for(auto &pLayer : m_vpLayers)
	{
		if(pLayer->m_Visible && pLayer->m_Type == LAYERTYPE_TILES && pLayer != m_pMap->m_pGameLayer && pLayer != m_pMap->m_pFrontLayer && pLayer != m_pMap->m_pTeleLayer && pLayer != m_pMap->m_pSpeedupLayer && pLayer != m_pMap->m_pSwitchLayer && pLayer != m_pMap->m_pTuneLayer)
		{
			CLayerTiles *pTiles = static_cast<CLayerTiles *>(pLayer);
			if(pTiles->m_Game || pTiles->m_Front || pTiles->m_Tele || pTiles->m_Speedup || pTiles->m_Tune || pTiles->m_Switch)
			{
				pLayer->Render();
			}
		}
	}

	pGraphics->ClipDisable();
}

void CLayerGroup::AddLayer(CLayer *pLayer)
{
	m_pMap->m_Modified = true;
	m_vpLayers.push_back(pLayer);
}

void CLayerGroup::DeleteLayer(int Index)
{
	if(Index < 0 || Index >= (int)m_vpLayers.size())
		return;
	delete m_vpLayers[Index];
	m_vpLayers.erase(m_vpLayers.begin() + Index);
	m_pMap->m_Modified = true;
}

void CLayerGroup::DuplicateLayer(int Index)
{
	if(Index < 0 || Index >= (int)m_vpLayers.size())
		return;

	auto *pDup = m_vpLayers[Index]->Duplicate();
	m_vpLayers.insert(m_vpLayers.begin() + Index + 1, pDup);

	m_pMap->m_Modified = true;
}

void CLayerGroup::GetSize(float *pWidth, float *pHeight) const
{
	*pWidth = 0;
	*pHeight = 0;
	for(const auto &pLayer : m_vpLayers)
	{
		float lw, lh;
		pLayer->GetSize(&lw, &lh);
		*pWidth = maximum(*pWidth, lw);
		*pHeight = maximum(*pHeight, lh);
	}
}

int CLayerGroup::SwapLayers(int Index0, int Index1)
{
	if(Index0 < 0 || Index0 >= (int)m_vpLayers.size())
		return Index0;
	if(Index1 < 0 || Index1 >= (int)m_vpLayers.size())
		return Index0;
	if(Index0 == Index1)
		return Index0;
	m_pMap->m_Modified = true;
	std::swap(m_vpLayers[Index0], m_vpLayers[Index1]);
	return Index1;
}

void CEditorImage::AnalyseTileFlags()
{
	mem_zero(m_aTileFlags, sizeof(m_aTileFlags));

	int tw = m_Width / 16; // tilesizes
	int th = m_Height / 16;
	if(tw == th && m_Format == CImageInfo::FORMAT_RGBA)
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
						int p = (ty * tw + y) * m_Width + tx * tw + x;
						if(pPixelData[p * 4 + 3] < 250)
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

void CEditor::EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Channels, void *pUser)
{
	CEditor *pThis = (CEditor *)pUser;
	if(Env < 0 || Env >= (int)pThis->m_Map.m_vpEnvelopes.size())
	{
		Channels = ColorRGBA();
		return;
	}

	CEnvelope *pEnv = pThis->m_Map.m_vpEnvelopes[Env];
	float t = pThis->m_AnimateTime;
	t *= pThis->m_AnimateSpeed;
	t += (TimeOffsetMillis / 1000.0f);
	pEnv->Eval(t, Channels);
}

/********************************************************
 OTHER
*********************************************************/

bool CEditor::DoEditBox(void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *pOffset, bool Hidden, int Corners)
{
	if(UI()->LastActiveItem() == pID)
		m_EditBoxActive = 2;
	return UI()->DoEditBox(pID, pRect, pStr, StrSize, FontSize, pOffset, Hidden, Corners);
}

bool CEditor::DoClearableEditBox(void *pID, void *pClearID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *pOffset, bool Hidden, int Corners)
{
	if(UI()->LastActiveItem() == pID)
		m_EditBoxActive = 2;
	return UI()->DoClearableEditBox(pID, pClearID, pRect, pStr, StrSize, FontSize, pOffset, Hidden, Corners);
}

ColorRGBA CEditor::GetButtonColor(const void *pID, int Checked)
{
	if(Checked < 0)
		return ColorRGBA(0, 0, 0, 0.5f);

	switch(Checked)
	{
	case 7: // selected + game layers
		if(UI()->HotItem() == pID)
			return ColorRGBA(1, 0, 0, 0.4f);
		return ColorRGBA(1, 0, 0, 0.2f);

	case 6: // game layers
		if(UI()->HotItem() == pID)
			return ColorRGBA(1, 1, 1, 0.4f);
		return ColorRGBA(1, 1, 1, 0.2f);

	case 5: // selected + image/sound should be embedded
		if(UI()->HotItem() == pID)
			return ColorRGBA(1, 0, 0, 0.75f);
		return ColorRGBA(1, 0, 0, 0.5f);

	case 4: // image/sound should be embedded
		if(UI()->HotItem() == pID)
			return ColorRGBA(1, 0, 0, 1.0f);
		return ColorRGBA(1, 0, 0, 0.875f);

	case 3: // selected + unused image/sound
		if(UI()->HotItem() == pID)
			return ColorRGBA(1, 0, 1, 0.75f);
		return ColorRGBA(1, 0, 1, 0.5f);

	case 2: // unused image/sound
		if(UI()->HotItem() == pID)
			return ColorRGBA(0, 0, 1, 0.75f);
		return ColorRGBA(0, 0, 1, 0.5f);

	case 1: // selected
		if(UI()->HotItem() == pID)
			return ColorRGBA(1, 0, 0, 0.75f);
		return ColorRGBA(1, 0, 0, 0.5f);

	default: // regular
		if(UI()->HotItem() == pID)
			return ColorRGBA(1, 1, 1, 0.75f);
		return ColorRGBA(1, 1, 1, 0.5f);
	}
}

int CEditor::DoButton_Editor_Common(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(UI()->MouseInside(pRect))
	{
		if(Flags & BUTTON_CONTEXT)
			ms_pUiGotContext = pID;
		if(m_pTooltip)
			m_pTooltip = pToolTip;
	}

	if(UI()->HotItem() == pID && pToolTip)
		m_pTooltip = pToolTip;

	return UI()->DoButtonLogic(pID, Checked, pRect);

	// Draw here
	//return UI()->DoButton(id, text, checked, r, draw_func, 0);
}

int CEditor::DoButton_Editor(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int AlignVert)
{
	pRect->Draw(GetButtonColor(pID, Checked), IGraphics::CORNER_ALL, 3.0f);
	CUIRect NewRect = *pRect;
	SLabelProperties Props;
	Props.m_AlignVertically = AlignVert;
	UI()->DoLabel(&NewRect, pText, 10.f, TEXTALIGN_CENTER, Props);
	Checked %= 2;
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Env(const void *pID, const char *pText, int Checked, const CUIRect *pRect, const char *pToolTip, ColorRGBA BaseColor)
{
	float Bright = Checked ? 1.0f : 0.5f;
	float Alpha = UI()->HotItem() == pID ? 1.0f : 0.75f;
	ColorRGBA Color = ColorRGBA(BaseColor.r * Bright, BaseColor.g * Bright, BaseColor.b * Bright, Alpha);

	pRect->Draw(Color, IGraphics::CORNER_ALL, 3.0f);
	UI()->DoLabel(pRect, pText, 10.f, TEXTALIGN_CENTER);
	Checked %= 2;
	return DoButton_Editor_Common(pID, pText, Checked, pRect, 0, pToolTip);
}

int CEditor::DoButton_File(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(Checked)
		pRect->Draw(GetButtonColor(pID, Checked), IGraphics::CORNER_ALL, 3.0f);

	CUIRect t = *pRect;
	t.VMargin(5.0f, &t);
	UI()->DoLabel(&t, pText, 10, TEXTALIGN_LEFT);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Menu(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	CUIRect r = *pRect;
	r.Draw(ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), IGraphics::CORNER_T, 3.0f);

	r = *pRect;
	r.VMargin(5.0f, &r);
	UI()->DoLabel(&r, pText, 10, TEXTALIGN_LEFT);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_MenuItem(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(UI()->HotItem() == pID || Checked)
		pRect->Draw(GetButtonColor(pID, Checked), IGraphics::CORNER_ALL, 3.0f);

	CUIRect t = *pRect;
	t.VMargin(5.0f, &t);
	UI()->DoLabel(&t, pText, 10, TEXTALIGN_LEFT);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Tab(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	pRect->Draw(GetButtonColor(pID, Checked), IGraphics::CORNER_T, 5.0f);
	CUIRect NewRect = *pRect;
	UI()->DoLabel(&NewRect, pText, 10, TEXTALIGN_CENTER);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Ex(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners, float FontSize, int AlignVert)
{
	pRect->Draw(GetButtonColor(pID, Checked), Corners, 3.0f);
	CUIRect NewRect = *pRect;
	SLabelProperties Props;
	Props.m_AlignVertically = AlignVert;
	UI()->DoLabel(&NewRect, pText, FontSize, TEXTALIGN_CENTER, Props);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_ButtonInc(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	pRect->Draw(GetButtonColor(pID, Checked), IGraphics::CORNER_R, 3.0f);
	UI()->DoLabel(pRect, pText ? pText : "+", 10, TEXTALIGN_CENTER);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_ButtonDec(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	pRect->Draw(GetButtonColor(pID, Checked), IGraphics::CORNER_L, 3.0f);
	UI()->DoLabel(pRect, pText ? pText : "-", 10, TEXTALIGN_CENTER);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_ColorPicker(const void *pID, const CUIRect *pRect, ColorRGBA *pColor, const char *pToolTip)
{
	pRect->Draw(*pColor, 0, 0.0f);
	return DoButton_Editor_Common(pID, nullptr, 0, pRect, 0, pToolTip);
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

	int XOffset = aGroupPoints[0] / LineDistance;
	int YOffset = aGroupPoints[1] / LineDistance;
	int XGridOffset = XOffset % m_GridFactor;
	int YGridOffset = YOffset % m_GridFactor;

	Graphics()->TextureClear();
	Graphics()->LinesBegin();

	for(int i = 0; i < (int)w; i++)
	{
		if((i + YGridOffset) % m_GridFactor == 0)
			Graphics()->SetColor(1.0f, 0.3f, 0.3f, 0.3f);
		else
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.15f);

		IGraphics::CLineItem Line = IGraphics::CLineItem(LineDistance * XOffset, LineDistance * i + LineDistance * YOffset, w + aGroupPoints[2], LineDistance * i + LineDistance * YOffset);
		Graphics()->LinesDraw(&Line, 1);

		if((i + XGridOffset) % m_GridFactor == 0)
			Graphics()->SetColor(1.0f, 0.3f, 0.3f, 0.3f);
		else
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.15f);

		Line = IGraphics::CLineItem(LineDistance * i + LineDistance * XOffset, LineDistance * YOffset, LineDistance * i + LineDistance * XOffset, h + aGroupPoints[3]);
		Graphics()->LinesDraw(&Line, 1);
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->LinesEnd();
}

void CEditor::RenderBackground(CUIRect View, IGraphics::CTextureHandle Texture, float Size, float Brightness)
{
	Graphics()->TextureSet(Texture);
	Graphics()->BlendNormal();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Brightness, Brightness, Brightness, 1.0f);
	Graphics()->QuadsSetSubset(0, 0, View.w / Size, View.h / Size);
	IGraphics::CQuadItem QuadItem(View.x, View.y, View.w, View.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

int CEditor::UiDoValueSelector(void *pID, CUIRect *pRect, const char *pLabel, int Current, int Min, int Max, int Step, float Scale, const char *pToolTip, bool IsDegree, bool IsHex, int Corners, ColorRGBA *pColor)
{
	// logic
	static float s_Value;
	static char s_aNumStr[64];
	static bool s_TextMode = false;
	static void *s_pLastTextpID = pID;
	const bool Inside = UI()->MouseInside(pRect);

	if(UI()->MouseButton(1) && UI()->HotItem() == pID)
	{
		s_pLastTextpID = pID;
		s_TextMode = true;
		m_LockMouse = false;
		if(IsHex)
			str_format(s_aNumStr, sizeof(s_aNumStr), "%06X", Current);
		else
			str_format(s_aNumStr, sizeof(s_aNumStr), "%d", Current);
	}

	if(UI()->CheckActiveItem(pID))
	{
		if(!UI()->MouseButton(0))
		{
			m_LockMouse = false;
			UI()->SetActiveItem(nullptr);
			s_TextMode = false;
		}
	}

	if(s_TextMode && s_pLastTextpID == pID)
	{
		m_pTooltip = "Type your number";

		static float s_NumberBoxID = 0;
		DoEditBox(&s_NumberBoxID, pRect, s_aNumStr, sizeof(s_aNumStr), 10.0f, &s_NumberBoxID, false, Corners);

		UI()->SetActiveItem(&s_NumberBoxID);

		if(Input()->KeyIsPressed(KEY_RETURN) || Input()->KeyIsPressed(KEY_KP_ENTER) ||
			((UI()->MouseButton(1) || UI()->MouseButton(0)) && !Inside))
		{
			if(IsHex)
				Current = clamp(str_toint_base(s_aNumStr, 16), Min, Max);
			else
				Current = clamp(str_toint(s_aNumStr), Min, Max);
			m_LockMouse = false;
			UI()->SetActiveItem(nullptr);
			s_TextMode = false;
		}

		if(Input()->KeyIsPressed(KEY_ESCAPE))
		{
			m_LockMouse = false;
			UI()->SetActiveItem(nullptr);
			s_TextMode = false;
		}
	}
	else
	{
		if(UI()->CheckActiveItem(pID))
		{
			if(UI()->MouseButton(0))
			{
				if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
					s_Value += m_MouseDeltaX * 0.05f;
				else
					s_Value += m_MouseDeltaX;

				if(absolute(s_Value) >= Scale)
				{
					int Count = (int)(s_Value / Scale);
					s_Value = fmod(s_Value, Scale);
					Current += Step * Count;
					Current = clamp(Current, Min, Max);

					// Constrain to discrete steps
					if(Count > 0)
						Current = Current / Step * Step;
					else
						Current = round_ceil(Current / (float)Step) * Step;
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
			str_format(aBuf, sizeof(aBuf), "%s %d", pLabel, Current);
		else if(IsDegree)
			str_format(aBuf, sizeof(aBuf), "%d°", Current);
		else if(IsHex)
			str_format(aBuf, sizeof(aBuf), "#%06X", Current);
		else
			str_format(aBuf, sizeof(aBuf), "%d", Current);
		pRect->Draw(pColor ? *pColor : GetButtonColor(pID, 0), Corners, 5.0f);
		UI()->DoLabel(pRect, aBuf, 10, TEXTALIGN_CENTER);
	}

	return Current;
}

CLayerGroup *CEditor::GetSelectedGroup() const
{
	if(m_SelectedGroup >= 0 && m_SelectedGroup < (int)m_Map.m_vpGroups.size())
		return m_Map.m_vpGroups[m_SelectedGroup];
	return nullptr;
}

CLayer *CEditor::GetSelectedLayer(int Index) const
{
	CLayerGroup *pGroup = GetSelectedGroup();
	if(!pGroup)
		return nullptr;

	if(Index < 0 || Index >= (int)m_vSelectedLayers.size())
		return nullptr;

	int LayerIndex = m_vSelectedLayers[Index];

	if(LayerIndex >= 0 && LayerIndex < (int)m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers.size())
		return pGroup->m_vpLayers[LayerIndex];
	return nullptr;
}

CLayer *CEditor::GetSelectedLayerType(int Index, int Type) const
{
	CLayer *p = GetSelectedLayer(Index);
	if(p && p->m_Type == Type)
		return p;
	return nullptr;
}

std::vector<CQuad *> CEditor::GetSelectedQuads()
{
	CLayerQuads *pQuadLayer = (CLayerQuads *)GetSelectedLayerType(0, LAYERTYPE_QUADS);
	std::vector<CQuad *> vpQuads;
	if(!pQuadLayer)
		return vpQuads;
	vpQuads.resize(m_vSelectedQuads.size());
	for(int i = 0; i < (int)m_vSelectedQuads.size(); ++i)
		vpQuads[i] = &pQuadLayer->m_vQuads[m_vSelectedQuads[i]];
	return vpQuads;
}

CSoundSource *CEditor::GetSelectedSource()
{
	CLayerSounds *pSounds = (CLayerSounds *)GetSelectedLayerType(0, LAYERTYPE_SOUNDS);
	if(!pSounds)
		return nullptr;
	if(m_SelectedSource >= 0 && m_SelectedSource < (int)pSounds->m_vSources.size())
		return &pSounds->m_vSources[m_SelectedSource];
	return nullptr;
}

void CEditor::SelectLayer(int LayerIndex, int GroupIndex)
{
	if(GroupIndex != -1)
		m_SelectedGroup = GroupIndex;

	m_vSelectedLayers.clear();
	m_vSelectedQuads.clear();
	AddSelectedLayer(LayerIndex);
}

void CEditor::AddSelectedLayer(int LayerIndex)
{
	m_vSelectedLayers.push_back(LayerIndex);

	m_QuadKnifeActive = false;
}

void CEditor::SelectQuad(int Index)
{
	m_vSelectedQuads.clear();
	m_vSelectedQuads.push_back(Index);
}

void CEditor::DeleteSelectedQuads()
{
	CLayerQuads *pLayer = (CLayerQuads *)GetSelectedLayerType(0, LAYERTYPE_QUADS);
	if(!pLayer)
		return;

	for(int i = 0; i < (int)m_vSelectedQuads.size(); ++i)
	{
		pLayer->m_vQuads.erase(pLayer->m_vQuads.begin() + m_vSelectedQuads[i]);
		for(int j = i + 1; j < (int)m_vSelectedQuads.size(); ++j)
			if(m_vSelectedQuads[j] > m_vSelectedQuads[i])
				m_vSelectedQuads[j]--;

		m_vSelectedQuads.erase(m_vSelectedQuads.begin() + i);
		i--;
	}
}

bool CEditor::IsQuadSelected(int Index) const
{
	return FindSelectedQuadIndex(Index) >= 0;
}

int CEditor::FindSelectedQuadIndex(int Index) const
{
	for(size_t i = 0; i < m_vSelectedQuads.size(); ++i)
		if(m_vSelectedQuads[i] == Index)
			return i;
	return -1;
}

bool CEditor::IsSpecialLayer(const CLayer *pLayer) const
{
	return m_Map.m_pGameLayer == pLayer || m_Map.m_pTeleLayer == pLayer || m_Map.m_pSpeedupLayer == pLayer || m_Map.m_pFrontLayer == pLayer || m_Map.m_pSwitchLayer == pLayer || m_Map.m_pTuneLayer == pLayer;
}

void CEditor::CallbackOpenMap(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;
	if(pEditor->Load(pFileName, StorageType))
	{
		pEditor->m_ValidSaveFilename = StorageType == IStorage::TYPE_SAVE && pEditor->m_pFileDialogPath == pEditor->m_aFileDialogCurrentFolder;
		pEditor->m_Dialog = DIALOG_NONE;
	}
}

void CEditor::CallbackAppendMap(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;
	if(pEditor->Append(pFileName, StorageType))
		pEditor->m_aFileName[0] = 0;
	else
		pEditor->SortImages();

	pEditor->m_Dialog = DIALOG_NONE;
}

void CEditor::CallbackSaveMap(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = static_cast<CEditor *>(pUser);
	char aBuf[1024];
	// add map extension
	if(!str_endswith(pFileName, ".map"))
	{
		str_format(aBuf, sizeof(aBuf), "%s.map", pFileName);
		pFileName = aBuf;
	}

	if(pEditor->Save(pFileName))
	{
		str_copy(pEditor->m_aFileName, pFileName, sizeof(pEditor->m_aFileName));
		pEditor->m_ValidSaveFilename = StorageType == IStorage::TYPE_SAVE && pEditor->m_pFileDialogPath == pEditor->m_aFileDialogCurrentFolder;
		pEditor->m_Map.m_Modified = false;
	}

	pEditor->m_Dialog = DIALOG_NONE;
}

void CEditor::CallbackSaveCopyMap(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = static_cast<CEditor *>(pUser);
	char aBuf[1024];
	// add map extension
	if(!str_endswith(pFileName, ".map"))
	{
		str_format(aBuf, sizeof(aBuf), "%s.map", pFileName);
		pFileName = aBuf;
	}

	if(pEditor->Save(pFileName))
	{
		pEditor->m_Map.m_Modified = false;
	}

	pEditor->m_Dialog = DIALOG_NONE;
}

static int EntitiesListdirCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;
	if(!IsDir && str_endswith(pName, ".png"))
	{
		std::string Name = pName;
		pEditor->m_vSelectEntitiesFiles.push_back(Name.substr(0, Name.length() - 4));
	}

	return 0;
}

void CEditor::DoToolbar(CUIRect ToolBar)
{
	bool ModPressed = Input()->ModifierIsPressed();
	bool ShiftPressed = Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT);

	CUIRect TB_Top, TB_Bottom;
	CUIRect Button;

	ToolBar.HSplitMid(&TB_Top, &TB_Bottom, 5.0f);

	// top line buttons
	{
		// detail button
		TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
		static int s_HqButton = 0;
		if(DoButton_Editor(&s_HqButton, "HD", m_ShowDetail, &Button, 0, "[ctrl+h] Toggle High Detail") ||
			(Input()->KeyPress(KEY_H) && ModPressed))
		{
			m_ShowDetail = !m_ShowDetail;
		}

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		// animation button
		TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
		static int s_AnimateButton = 0;
		if(DoButton_Editor(&s_AnimateButton, "Anim", m_Animate, &Button, 0, "[ctrl+m] Toggle animation") ||
			(Input()->KeyPress(KEY_M) && ModPressed))
		{
			m_AnimateStart = time_get();
			m_Animate = !m_Animate;
		}

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		// proof button
		TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
		static int s_ProofButton = 0;
		if(DoButton_Editor(&s_ProofButton, "Proof", m_ProofBorders, &Button, 0, "[ctrl+p] Toggles proof borders. These borders represent what a player maximum can see.") ||
			(Input()->KeyPress(KEY_P) && ModPressed))
		{
			m_ProofBorders = !m_ProofBorders;
		}

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		// zoom button
		TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
		static int s_ZoomButton = 0;
		if(DoButton_Editor(&s_ZoomButton, "Zoom", m_PreviewZoom, &Button, 0, "Toggles preview of how layers will be zoomed in-game"))
		{
			m_PreviewZoom = !m_PreviewZoom;
		}

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		// grid button
		TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
		static int s_GridButton = 0;
		if(DoButton_Editor(&s_GridButton, "Grid", m_GridActive, &Button, 0, "[ctrl+g] Toggle Grid") ||
			(Input()->KeyPress(KEY_G) && ModPressed))
		{
			m_GridActive = !m_GridActive;
		}

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		// tile info button
		TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
		static int s_TileInfoButton = 0;
		if(DoButton_Editor(&s_TileInfoButton, "Info", m_ShowTileInfo, &Button, 0, "[ctrl+i] Show tile information") ||
			(Input()->KeyPress(KEY_I) && ModPressed))
		{
			m_ShowTileInfo = !m_ShowTileInfo;
			m_ShowEnvelopePreview = SHOWENV_NONE;
		}

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		// allow place unused tiles button
		TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
		static int s_AllowPlaceUnusedTilesButton = 0;
		if(DoButton_Editor(&s_AllowPlaceUnusedTilesButton, "Unused", m_AllowPlaceUnusedTiles, &Button, 0, "[ctrl+u] Allow placing unused tiles") ||
			(Input()->KeyPress(KEY_U) && ModPressed))
		{
			m_AllowPlaceUnusedTiles = !m_AllowPlaceUnusedTiles;
		}

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
		static int s_ColorBrushButton = 0;
		if(DoButton_Editor(&s_ColorBrushButton, "Color", m_BrushColorEnabled, &Button, 0, "Toggle brush coloring"))
		{
			m_BrushColorEnabled = !m_BrushColorEnabled;
		}

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		TB_Top.VSplitLeft(45.0f, &Button, &TB_Top);
		static int s_EntitiesButtonID = 0;
		if(DoButton_Editor(&s_EntitiesButtonID, "Entities", 0, &Button, 0, "Choose game layer entities image for different gametypes"))
		{
			m_vSelectEntitiesFiles.clear();
			Storage()->ListDirectory(IStorage::TYPE_ALL, "editor/entities", EntitiesListdirCallback, this);
			std::sort(m_vSelectEntitiesFiles.begin(), m_vSelectEntitiesFiles.end());

			static int s_EntitiesPopupID = 0;
			UiInvokePopupMenu(&s_EntitiesPopupID, 0, Button.x, Button.y + 18.0f,
				250, m_vSelectEntitiesFiles.size() * 14 + 10, PopupEntities);
		}

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		// zoom group
		TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
		static int s_ZoomOutButton = 0;
		if(DoButton_Ex(&s_ZoomOutButton, "ZO", 0, &Button, 0, "[NumPad-] Zoom out", IGraphics::CORNER_L))
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
		if(DoButton_Ex(&s_ZoomInButton, "ZI", 0, &Button, 0, "[NumPad+] Zoom in", IGraphics::CORNER_R))
			m_ZoomLevel -= 50;

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		// brush manipulation
		{
			int Enabled = m_Brush.IsEmpty() ? -1 : 0;

			// flip buttons
			TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
			static int s_FlipXButton = 0;
			if(DoButton_Ex(&s_FlipXButton, "X/X", Enabled, &Button, 0, "[N] Flip brush horizontal", IGraphics::CORNER_L) || (Input()->KeyPress(KEY_N) && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0))
			{
				for(auto &pLayer : m_Brush.m_vpLayers)
					pLayer->BrushFlipX();
			}

			TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
			static int s_FlipyButton = 0;
			if(DoButton_Ex(&s_FlipyButton, "Y/Y", Enabled, &Button, 0, "[M] Flip brush vertical", IGraphics::CORNER_R) || (Input()->KeyPress(KEY_M) && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0))
			{
				for(auto &pLayer : m_Brush.m_vpLayers)
					pLayer->BrushFlipY();
			}

			// rotate buttons
			TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
			static int s_RotationAmount = 90;
			bool TileLayer = false;
			// check for tile layers in brush selection
			for(auto &pLayer : m_Brush.m_vpLayers)
				if(pLayer->m_Type == LAYERTYPE_TILES)
				{
					TileLayer = true;
					s_RotationAmount = maximum(90, (s_RotationAmount / 90) * 90);
					break;
				}
			s_RotationAmount = UiDoValueSelector(&s_RotationAmount, &Button, "", s_RotationAmount, TileLayer ? 90 : 1, 359, TileLayer ? 90 : 1, TileLayer ? 10.0f : 2.0f, "Rotation of the brush in degrees. Use left mouse button to drag and change the value. Hold shift to be more precise.", true);

			TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
			static int s_CcwButton = 0;
			if(DoButton_Ex(&s_CcwButton, "CCW", Enabled, &Button, 0, "[R] Rotates the brush counter clockwise", IGraphics::CORNER_L) || (Input()->KeyPress(KEY_R) && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0))
			{
				for(auto &pLayer : m_Brush.m_vpLayers)
					pLayer->BrushRotate(-s_RotationAmount / 360.0f * pi * 2);
			}

			TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
			static int s_CwButton = 0;
			if(DoButton_Ex(&s_CwButton, "CW", Enabled, &Button, 0, "[T] Rotates the brush clockwise", IGraphics::CORNER_R) || (Input()->KeyPress(KEY_T) && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0))
			{
				for(auto &pLayer : m_Brush.m_vpLayers)
					pLayer->BrushRotate(s_RotationAmount / 360.0f * pi * 2);
			}

			TB_Top.VSplitLeft(5.0f, &Button, &TB_Top);
		}

		// animation speed
		if(m_Animate)
		{
			TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
			static int s_AnimFasterButton = 0;
			if(DoButton_Ex(&s_AnimFasterButton, "A+", 0, &Button, 0, "Increase animation speed", IGraphics::CORNER_L))
				m_AnimateSpeed += 0.5f;

			TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
			static int s_AnimNormalButton = 0;
			if(DoButton_Ex(&s_AnimNormalButton, "1", 0, &Button, 0, "Normal animation speed", 0))
				m_AnimateSpeed = 1.0f;

			TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
			static int s_AnimSlowerButton = 0;
			if(DoButton_Ex(&s_AnimSlowerButton, "A-", 0, &Button, 0, "Decrease animation speed", IGraphics::CORNER_R))
			{
				if(m_AnimateSpeed > 0.5f)
					m_AnimateSpeed -= 0.5f;
			}

			TB_Top.VSplitLeft(5.0f, &Button, &TB_Top);
		}

		// grid zoom
		if(m_GridActive)
		{
			TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
			static int s_GridIncreaseButton = 0;
			if(DoButton_Ex(&s_GridIncreaseButton, "G-", 0, &Button, 0, "Decrease grid", IGraphics::CORNER_L))
			{
				if(m_GridFactor > 1)
					m_GridFactor--;
			}

			TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
			static int s_GridNormalButton = 0;
			if(DoButton_Ex(&s_GridNormalButton, "1", 0, &Button, 0, "Normal grid", 0))
				m_GridFactor = 1;

			TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);

			static int s_GridDecreaseButton = 0;
			if(DoButton_Ex(&s_GridDecreaseButton, "G+", 0, &Button, 0, "Increase grid", IGraphics::CORNER_R))
			{
				if(m_GridFactor < 15)
					m_GridFactor++;
			}
			TB_Top.VSplitLeft(5.0f, &Button, &TB_Top);
		}
	}

	// Bottom line buttons
	{
		// refocus button
		{
			TB_Bottom.VSplitLeft(45.0f, &Button, &TB_Bottom);
			static int s_RefocusButton = 0;
			if(DoButton_Editor(&s_RefocusButton, "Refocus", m_WorldOffsetX && m_WorldOffsetY ? 0 : -1, &Button, 0, "[HOME] Restore map focus") || (m_EditBoxActive == 0 && Input()->KeyPress(KEY_HOME)))
			{
				m_WorldOffsetX = 0;
				m_WorldOffsetY = 0;
			}
			TB_Bottom.VSplitLeft(5.0f, nullptr, &TB_Bottom);
		}

		// goto xy button
		{
			TB_Bottom.VSplitLeft(50.0, &Button, &TB_Bottom);
			static int s_GotoButton = 0;
			if(DoButton_Editor(&s_GotoButton, "Goto XY", 0, &Button, 0, "Go to a specified coordinate point on the map"))
			{
				static int s_ModifierPopupID = 0;
				if(!UiPopupExists(&s_ModifierPopupID))
				{
					UiInvokePopupMenu(&s_ModifierPopupID, 0, Button.x, Button.y + Button.h, 120, 52, PopupGoto);
				}
			}
			TB_Bottom.VSplitLeft(5.0f, nullptr, &TB_Bottom);
		}

		// tile manipulation
		{
			static int s_BorderBut = 0;
			CLayerTiles *pT = (CLayerTiles *)GetSelectedLayerType(0, LAYERTYPE_TILES);

			// no border for tele layer, speedup, front and switch
			if(pT && (pT->m_Tele || pT->m_Speedup || pT->m_Switch || pT->m_Front || pT->m_Tune))
				pT = nullptr;

			if(pT)
			{
				TB_Bottom.VSplitLeft(60.0f, &Button, &TB_Bottom);
				if(DoButton_Ex(&s_BorderBut, "Border", 0, &Button, 0, "Place tiles in a 2-tile wide border at the edges of the layer", IGraphics::CORNER_ALL))
				{
					m_PopupEventType = POPEVENT_PLACE_BORDER_TILES;
					m_PopupEventActivated = true;
				}
				TB_Bottom.VSplitLeft(5.0f, &Button, &TB_Bottom);
			}

			// do tele/tune/switch/speedup button
			{
				int (*pPopupFunc)(CEditor * peditor, CUIRect View, void *pContext) = nullptr;
				const char *pButtonName = nullptr;
				float Height = 0.0f;
				CLayerTiles *pS = (CLayerTiles *)GetSelectedLayerType(0, LAYERTYPE_TILES);
				if(pS)
				{
					if(pS == m_Map.m_pSwitchLayer)
					{
						pButtonName = "Switch";
						pPopupFunc = PopupSwitch;
						Height = 36;
					}
					else if(pS == m_Map.m_pSpeedupLayer)
					{
						pButtonName = "Speedup";
						pPopupFunc = PopupSpeedup;
						Height = 53;
					}
					else if(pS == m_Map.m_pTuneLayer)
					{
						pButtonName = "Tune";
						pPopupFunc = PopupTune;
						Height = 23;
					}
					else if(pS == m_Map.m_pTeleLayer)
					{
						pButtonName = "Tele";
						pPopupFunc = PopupTele;
						Height = 23;
					}

					if(pButtonName != nullptr)
					{
						static char aBuf[64];
						str_format(aBuf, sizeof(aBuf), "[ctrl+a] %s", pButtonName);

						TB_Bottom.VSplitLeft(60.0f, &Button, &TB_Bottom);
						static int s_ModifierButton = 0;
						if(DoButton_Ex(&s_ModifierButton, pButtonName, 0, &Button, 0, aBuf, IGraphics::CORNER_ALL) || (ModPressed && Input()->KeyPress(KEY_A)))
						{
							static int s_ModifierPopupID = 0;
							if(!UiPopupExists(&s_ModifierPopupID))
							{
								UiInvokePopupMenu(&s_ModifierPopupID, 0, Button.x, Button.y + Button.h, 120, Height, pPopupFunc);
							}
						}
						TB_Bottom.VSplitLeft(5.0f, nullptr, &TB_Bottom);
					}
				}
			}
		}

		// do add quad/sound button
		CLayer *pLayer = GetSelectedLayer(0);
		if(pLayer && (pLayer->m_Type == LAYERTYPE_QUADS || pLayer->m_Type == LAYERTYPE_SOUNDS))
		{
			TB_Bottom.VSplitLeft(60.0f, &Button, &TB_Bottom);

			bool Invoked = false;
			static int s_AddItemButton = 0;

			if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				Invoked = DoButton_Editor(&s_AddItemButton, "Add Quad", 0, &Button, 0, "[ctrl+q] Add a new quad") ||
					  (Input()->KeyPress(KEY_Q) && ModPressed);
			}
			else if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				Invoked = DoButton_Editor(&s_AddItemButton, "Add Sound", 0, &Button, 0, "[ctrl+q] Add a new sound source") ||
					  (Input()->KeyPress(KEY_Q) && ModPressed);
			}

			if(Invoked)
			{
				CLayerGroup *pGroup = GetSelectedGroup();

				float aMapping[4];
				pGroup->Mapping(aMapping);
				int x = aMapping[0] + (aMapping[2] - aMapping[0]) / 2;
				int y = aMapping[1] + (aMapping[3] - aMapping[1]) / 2;
				if(Input()->KeyPress(KEY_Q) && ModPressed)
				{
					x += UI()->MouseWorldX() - (m_WorldOffsetX * pGroup->m_ParallaxX / 100) - pGroup->m_OffsetX;
					y += UI()->MouseWorldY() - (m_WorldOffsetY * pGroup->m_ParallaxY / 100) - pGroup->m_OffsetY;
				}

				if(pLayer->m_Type == LAYERTYPE_QUADS)
				{
					CLayerQuads *pLayerQuads = (CLayerQuads *)pLayer;

					int Width = 64;
					int Height = 64;
					if(pLayerQuads->m_Image >= 0)
					{
						Width = m_Map.m_vpImages[pLayerQuads->m_Image]->m_Width;
						Height = m_Map.m_vpImages[pLayerQuads->m_Image]->m_Height;
					}

					pLayerQuads->NewQuad(x, y, Width, Height);
				}
				else if(pLayer->m_Type == LAYERTYPE_SOUNDS)
				{
					CLayerSounds *pLayerSounds = (CLayerSounds *)pLayer;
					pLayerSounds->NewSource(x, y);
				}
			}
			TB_Bottom.VSplitLeft(5.0f, &Button, &TB_Bottom);
		}

		// Brush draw mode button
		{
			TB_Bottom.VSplitLeft(65.0f, &Button, &TB_Bottom);
			static int s_BrushDrawModeButton = 0;
			if(DoButton_Editor(&s_BrushDrawModeButton, "Destructive", m_BrushDrawDestructive, &Button, 0, "[ctrl+d] Toggle brush draw mode") ||
				(Input()->KeyPress(KEY_D) && ModPressed && !ShiftPressed))
				m_BrushDrawDestructive = !m_BrushDrawDestructive;
			TB_Bottom.VSplitLeft(5.0f, &Button, &TB_Bottom);
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
		OP_NONE = 0,
		OP_MOVE,
		OP_CONTEXT_MENU,
	};

	void *pID = &pSource->m_Position;

	static int s_Operation = OP_NONE;

	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();

	float CenterX = fx2f(pSource->m_Position.x);
	float CenterY = fx2f(pSource->m_Position.y);

	float dx = (CenterX - wx) / m_MouseWScale;
	float dy = (CenterY - wy) / m_MouseWScale;
	if(dx * dx + dy * dy < 50)
		UI()->SetHotItem(pID);

	bool IgnoreGrid;
	IgnoreGrid = Input()->KeyIsPressed(KEY_LALT) || Input()->KeyIsPressed(KEY_RALT);

	if(UI()->CheckActiveItem(pID))
	{
		if(m_MouseDeltaWx * m_MouseDeltaWx + m_MouseDeltaWy * m_MouseDeltaWy > 0.0f)
		{
			if(s_Operation == OP_MOVE)
			{
				if(m_GridActive && !IgnoreGrid)
				{
					int LineDistance = GetLineDistance();

					float x = 0.0f;
					float y = 0.0f;
					if(wx >= 0)
						x = (int)((wx + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					else
						x = (int)((wx - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					if(wy >= 0)
						y = (int)((wy + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					else
						y = (int)((wy - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);

					pSource->m_Position.x = f2fx(x);
					pSource->m_Position.y = f2fx(y);
				}
				else
				{
					pSource->m_Position.x = f2fx(wx);
					pSource->m_Position.y = f2fx(wy);
				}
			}
		}

		if(s_Operation == OP_CONTEXT_MENU)
		{
			if(!UI()->MouseButton(1))
			{
				if(m_vSelectedLayers.size() == 1)
				{
					static int s_SourcePopupID = 0;
					UiInvokePopupMenu(&s_SourcePopupID, 0, UI()->MouseX(), UI()->MouseY(), 120, 200, PopupSource);
					m_LockMouse = false;
				}
				s_Operation = OP_NONE;
				UI()->SetActiveItem(nullptr);
			}
		}
		else
		{
			if(!UI()->MouseButton(0))
			{
				m_LockMouse = false;
				s_Operation = OP_NONE;
				UI()->SetActiveItem(nullptr);
			}
		}

		Graphics()->SetColor(1, 1, 1, 1);
	}
	else if(UI()->HotItem() == pID)
	{
		ms_pUiGotContext = pID;

		Graphics()->SetColor(1, 1, 1, 1);
		m_pTooltip = "Left mouse button to move. Hold alt to ignore grid.";

		if(UI()->MouseButton(0))
		{
			s_Operation = OP_MOVE;

			UI()->SetActiveItem(pID);
			m_SelectedSource = Index;
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
		Graphics()->SetColor(0, 1, 0, 1);
	}

	IGraphics::CQuadItem QuadItem(CenterX, CenterY, 5.0f * m_MouseWScale, 5.0f * m_MouseWScale);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CEditor::DoQuad(CQuad *pQuad, int Index)
{
	enum
	{
		OP_NONE = 0,
		OP_MOVE_ALL,
		OP_MOVE_PIVOT,
		OP_ROTATE,
		OP_CONTEXT_MENU,
		OP_DELETE,
	};

	// some basic values
	void *pID = &pQuad->m_aPoints[4]; // use pivot addr as id
	static std::vector<std::vector<CPoint>> s_vvRotatePoints;
	static int s_Operation = OP_NONE;
	static float s_RotateAngle = 0;
	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();

	// get pivot
	float CenterX = fx2f(pQuad->m_aPoints[4].x);
	float CenterY = fx2f(pQuad->m_aPoints[4].y);

	float dx = (CenterX - wx) / m_MouseWScale;
	float dy = (CenterY - wy) / m_MouseWScale;
	if(dx * dx + dy * dy < 50)
		UI()->SetHotItem(pID);

	const bool IgnoreGrid = Input()->KeyIsPressed(KEY_LALT) || Input()->KeyIsPressed(KEY_RALT);

	// draw selection background
	if(IsQuadSelected(Index))
	{
		Graphics()->SetColor(0, 0, 0, 1);
		IGraphics::CQuadItem QuadItem(CenterX, CenterY, 7.0f * m_MouseWScale, 7.0f * m_MouseWScale);
		Graphics()->QuadsDraw(&QuadItem, 1);
	}

	if(UI()->CheckActiveItem(pID))
	{
		if(m_MouseDeltaWx * m_MouseDeltaWx + m_MouseDeltaWy * m_MouseDeltaWy > 0.0f)
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
						x = (int)((wx + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					else
						x = (int)((wx - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					if(wy >= 0)
						y = (int)((wy + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					else
						y = (int)((wy - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);

					pQuad->m_aPoints[4].x = f2fx(x);
					pQuad->m_aPoints[4].y = f2fx(y);
				}
				else
				{
					pQuad->m_aPoints[4].x = f2fx(wx);
					pQuad->m_aPoints[4].y = f2fx(wy);
				}
			}
			else if(s_Operation == OP_MOVE_ALL)
			{
				CLayerQuads *pLayer = (CLayerQuads *)GetSelectedLayerType(0, LAYERTYPE_QUADS);
				// move all points including pivot
				if(m_GridActive && !IgnoreGrid)
				{
					int LineDistance = GetLineDistance();

					float x = 0.0f;
					float y = 0.0f;
					if(wx >= 0)
						x = (int)((wx + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					else
						x = (int)((wx - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					if(wy >= 0)
						y = (int)((wy + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					else
						y = (int)((wy - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);

					int OffsetX = f2fx(x) - pQuad->m_aPoints[4].x;
					int OffsetY = f2fx(y) - pQuad->m_aPoints[4].y;

					for(auto &Selected : m_vSelectedQuads)
					{
						CQuad *pCurrentQuad = &pLayer->m_vQuads[Selected];
						for(auto &Point : pCurrentQuad->m_aPoints)
						{
							Point.x += OffsetX;
							Point.y += OffsetY;
						}
					}
				}
				else
				{
					int OffsetX = f2fx(wx) - pQuad->m_aPoints[4].x;
					int OffsetY = f2fx(wy) - pQuad->m_aPoints[4].y;

					for(auto &Selected : m_vSelectedQuads)
					{
						CQuad *pCurrentQuad = &pLayer->m_vQuads[Selected];
						for(auto &Point : pCurrentQuad->m_aPoints)
						{
							Point.x += OffsetX;
							Point.y += OffsetY;
						}
					}
				}
			}
			else if(s_Operation == OP_ROTATE)
			{
				CLayerQuads *pLayer = (CLayerQuads *)GetSelectedLayerType(0, LAYERTYPE_QUADS);
				for(size_t i = 0; i < m_vSelectedQuads.size(); ++i)
				{
					CQuad *pCurrentQuad = &pLayer->m_vQuads[m_vSelectedQuads[i]];
					for(int v = 0; v < 4; v++)
					{
						pCurrentQuad->m_aPoints[v] = s_vvRotatePoints[i][v];
						Rotate(&pCurrentQuad->m_aPoints[4], &pCurrentQuad->m_aPoints[v], s_RotateAngle);
					}
				}
			}
		}

		s_RotateAngle += (m_MouseDeltaX)*0.002f;

		if(s_Operation == OP_CONTEXT_MENU)
		{
			if(!UI()->MouseButton(1))
			{
				if(m_vSelectedLayers.size() == 1)
				{
					m_SelectedQuadIndex = FindSelectedQuadIndex(Index);

					static int s_QuadPopupID = 0;
					UiInvokePopupMenu(&s_QuadPopupID, 0, UI()->MouseX(), UI()->MouseY(), 120, 198, PopupQuad);
					m_LockMouse = false;
				}
				s_Operation = OP_NONE;
				UI()->SetActiveItem(nullptr);
			}
		}
		else if(s_Operation == OP_DELETE)
		{
			if(!UI()->MouseButton(1))
			{
				if(m_vSelectedLayers.size() == 1)
				{
					m_LockMouse = false;
					m_Map.m_Modified = true;
					DeleteSelectedQuads();
				}
				s_Operation = OP_NONE;
				UI()->SetActiveItem(nullptr);
			}
		}
		else
		{
			if(!UI()->MouseButton(0))
			{
				m_LockMouse = false;
				s_Operation = OP_NONE;
				UI()->SetActiveItem(nullptr);
			}
		}

		Graphics()->SetColor(1, 1, 1, 1);
	}
	else if(UI()->HotItem() == pID)
	{
		ms_pUiGotContext = pID;

		Graphics()->SetColor(1, 1, 1, 1);
		m_pTooltip = "Left mouse button to move. Hold shift to move pivot. Hold ctrl to rotate. Hold alt to ignore grid. Hold shift and right click to delete.";

		if(UI()->MouseButton(0))
		{
			if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
			{
				s_Operation = OP_MOVE_PIVOT;

				SelectQuad(Index);
			}
			else if(Input()->ModifierIsPressed())
			{
				m_LockMouse = true;
				s_Operation = OP_ROTATE;
				s_RotateAngle = 0;

				if(!IsQuadSelected(Index))
					SelectQuad(Index);

				CLayerQuads *pLayer = (CLayerQuads *)GetSelectedLayerType(0, LAYERTYPE_QUADS);
				s_vvRotatePoints.clear();
				s_vvRotatePoints.resize(m_vSelectedQuads.size());
				for(size_t i = 0; i < m_vSelectedQuads.size(); ++i)
				{
					CQuad *pCurrentQuad = &pLayer->m_vQuads[m_vSelectedQuads[i]];

					s_vvRotatePoints[i].resize(4);
					s_vvRotatePoints[i][0] = pCurrentQuad->m_aPoints[0];
					s_vvRotatePoints[i][1] = pCurrentQuad->m_aPoints[1];
					s_vvRotatePoints[i][2] = pCurrentQuad->m_aPoints[2];
					s_vvRotatePoints[i][3] = pCurrentQuad->m_aPoints[3];
				}
			}
			else
			{
				s_Operation = OP_MOVE_ALL;

				if(!IsQuadSelected(Index))
					SelectQuad(Index);
			}

			UI()->SetActiveItem(pID);
		}

		if(UI()->MouseButton(1))
		{
			if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
			{
				s_Operation = OP_DELETE;

				if(!IsQuadSelected(Index))
					SelectQuad(Index);

				UI()->SetActiveItem(pID);
			}
			else
			{
				s_Operation = OP_CONTEXT_MENU;

				if(!IsQuadSelected(Index))
					SelectQuad(Index);

				UI()->SetActiveItem(pID);
			}
		}
	}
	else
		Graphics()->SetColor(0, 1, 0, 1);

	IGraphics::CQuadItem QuadItem(CenterX, CenterY, 5.0f * m_MouseWScale, 5.0f * m_MouseWScale);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CEditor::DoQuadPoint(CQuad *pQuad, int QuadIndex, int V)
{
	void *pID = &pQuad->m_aPoints[V];

	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();

	float px = fx2f(pQuad->m_aPoints[V].x);
	float py = fx2f(pQuad->m_aPoints[V].y);

	float dx = (px - wx) / m_MouseWScale;
	float dy = (py - wy) / m_MouseWScale;
	if(dx * dx + dy * dy < 50)
		UI()->SetHotItem(pID);

	// draw selection background
	if(IsQuadSelected(QuadIndex) && m_SelectedPoints & (1 << V))
	{
		Graphics()->SetColor(0, 0, 0, 1);
		IGraphics::CQuadItem QuadItem(px, py, 7.0f * m_MouseWScale, 7.0f * m_MouseWScale);
		Graphics()->QuadsDraw(&QuadItem, 1);
	}

	enum
	{
		OP_NONE = 0,
		OP_MOVEPOINT,
		OP_MOVEUV,
		OP_CONTEXT_MENU
	};

	static bool s_Moved;
	static int s_Operation = OP_NONE;

	const bool IgnoreGrid = Input()->KeyIsPressed(KEY_LALT) || Input()->KeyIsPressed(KEY_RALT);

	if(UI()->CheckActiveItem(pID))
	{
		if(!s_Moved)
		{
			if(m_MouseDeltaWx * m_MouseDeltaWx + m_MouseDeltaWy * m_MouseDeltaWy > 0.0f)
				s_Moved = true;
		}

		if(s_Moved)
		{
			if(s_Operation == OP_MOVEPOINT)
			{
				CLayerQuads *pLayer = (CLayerQuads *)GetSelectedLayerType(0, LAYERTYPE_QUADS);
				if(m_GridActive && !IgnoreGrid)
				{
					int LineDistance = GetLineDistance();

					float x = 0.0f;
					float y = 0.0f;
					if(wx >= 0)
						x = (int)((wx + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					else
						x = (int)((wx - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					if(wy >= 0)
						y = (int)((wy + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					else
						y = (int)((wy - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);

					int OffsetX = f2fx(x) - pQuad->m_aPoints[V].x;
					int OffsetY = f2fx(y) - pQuad->m_aPoints[V].y;

					for(auto &Selected : m_vSelectedQuads)
					{
						CQuad *pCurrentQuad = &pLayer->m_vQuads[Selected];
						for(int m = 0; m < 4; m++)
							if(m_SelectedPoints & (1 << m))
							{
								pCurrentQuad->m_aPoints[m].x += OffsetX;
								pCurrentQuad->m_aPoints[m].y += OffsetY;
							}
					}
				}
				else
				{
					int OffsetX = f2fx(wx) - pQuad->m_aPoints[V].x;
					int OffsetY = f2fx(wy) - pQuad->m_aPoints[V].y;

					for(auto &Selected : m_vSelectedQuads)
					{
						CQuad *pCurrentQuad = &pLayer->m_vQuads[Selected];
						for(int m = 0; m < 4; m++)
							if(m_SelectedPoints & (1 << m))
							{
								pCurrentQuad->m_aPoints[m].x += OffsetX;
								pCurrentQuad->m_aPoints[m].y += OffsetY;
							}
					}
				}
			}
			else if(s_Operation == OP_MOVEUV)
			{
				CLayerQuads *pLayer = (CLayerQuads *)GetSelectedLayerType(0, LAYERTYPE_QUADS);
				for(auto &Selected : m_vSelectedQuads)
				{
					CQuad *pCurrentQuad = &pLayer->m_vQuads[Selected];
					for(int m = 0; m < 4; m++)
						if(m_SelectedPoints & (1 << m))
						{
							// 0,2;1,3 - line x
							// 0,1;2,3 - line y

							pCurrentQuad->m_aTexcoords[m].x += f2fx(m_MouseDeltaWx * 0.001f);
							pCurrentQuad->m_aTexcoords[(m + 2) % 4].x += f2fx(m_MouseDeltaWx * 0.001f);

							pCurrentQuad->m_aTexcoords[m].y += f2fx(m_MouseDeltaWy * 0.001f);
							pCurrentQuad->m_aTexcoords[m ^ 1].y += f2fx(m_MouseDeltaWy * 0.001f);
						}
				}
			}
		}

		if(s_Operation == OP_CONTEXT_MENU)
		{
			if(!UI()->MouseButton(1))
			{
				if(m_vSelectedLayers.size() == 1)
				{
					m_SelectedQuadPoint = V;
					m_SelectedQuadIndex = FindSelectedQuadIndex(QuadIndex);

					static int s_PointPopupID = 0;
					UiInvokePopupMenu(&s_PointPopupID, 0, UI()->MouseX(), UI()->MouseY(), 120, 150, PopupPoint);
				}
				UI()->SetActiveItem(nullptr);
			}
		}
		else
		{
			if(!UI()->MouseButton(0))
			{
				if(!s_Moved)
				{
					if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
						m_SelectedPoints ^= 1 << V;
					else
						m_SelectedPoints = 1 << V;
				}

				m_LockMouse = false;
				UI()->SetActiveItem(nullptr);
			}
		}

		Graphics()->SetColor(1, 1, 1, 1);
	}
	else if(UI()->HotItem() == pID)
	{
		ms_pUiGotContext = pID;

		Graphics()->SetColor(1, 1, 1, 1);
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

			if(!(m_SelectedPoints & (1 << V)))
			{
				if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
					m_SelectedPoints |= 1 << V;
				else
					m_SelectedPoints = 1 << V;
				s_Moved = true;
			}

			if(!IsQuadSelected(QuadIndex))
				SelectQuad(QuadIndex);
		}
		else if(UI()->MouseButton(1))
		{
			s_Operation = OP_CONTEXT_MENU;

			if(!IsQuadSelected(QuadIndex))
				SelectQuad(QuadIndex);

			UI()->SetActiveItem(pID);
			if(!(m_SelectedPoints & (1 << V)))
			{
				if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
					m_SelectedPoints |= 1 << V;
				else
					m_SelectedPoints = 1 << V;
				s_Moved = true;
			}
		}
	}
	else
		Graphics()->SetColor(1, 0, 0, 1);

	IGraphics::CQuadItem QuadItem(px, py, 5.0f * m_MouseWScale, 5.0f * m_MouseWScale);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

float CEditor::TriangleArea(vec2 A, vec2 B, vec2 C)
{
	return abs(((B.x - A.x) * (C.y - A.y) - (C.x - A.x) * (B.y - A.y)) * 0.5f);
}

bool CEditor::IsInTriangle(vec2 Point, vec2 A, vec2 B, vec2 C)
{
	// Normalize to increase precision
	vec2 Min(minimum(A.x, B.x, C.x), minimum(A.y, B.y, C.y));
	vec2 Max(maximum(A.x, B.x, C.x), maximum(A.y, B.y, C.y));
	vec2 Size(Max.x - Min.x, Max.y - Min.y);

	if(Size.x < 0.0000001f || Size.y < 0.0000001f)
		return false;

	vec2 Normal(1.f / Size.x, 1.f / Size.y);

	A = (A - Min) * Normal;
	B = (B - Min) * Normal;
	C = (C - Min) * Normal;
	Point = (Point - Min) * Normal;

	float Area = TriangleArea(A, B, C);
	return Area > 0.f && abs(TriangleArea(Point, A, B) + TriangleArea(Point, B, C) + TriangleArea(Point, C, A) - Area) < 0.000001f;
}

void CEditor::DoQuadKnife(int QuadIndex)
{
	CLayerQuads *pLayer = (CLayerQuads *)GetSelectedLayerType(0, LAYERTYPE_QUADS);
	CQuad *pQuad = &pLayer->m_vQuads[QuadIndex];

	bool IgnoreGrid = Input()->KeyIsPressed(KEY_LALT) || Input()->KeyIsPressed(KEY_RALT);
	float SnapRadius = 4.f * m_MouseWScale;

	vec2 Mouse = vec2(UI()->MouseWorldX(), UI()->MouseWorldY());
	vec2 Point = Mouse;

	vec2 v[4] = {
		vec2(fx2f(pQuad->m_aPoints[0].x), fx2f(pQuad->m_aPoints[0].y)),
		vec2(fx2f(pQuad->m_aPoints[1].x), fx2f(pQuad->m_aPoints[1].y)),
		vec2(fx2f(pQuad->m_aPoints[3].x), fx2f(pQuad->m_aPoints[3].y)),
		vec2(fx2f(pQuad->m_aPoints[2].x), fx2f(pQuad->m_aPoints[2].y))};

	m_pTooltip = "Left click inside the quad to select an area to slice. Hold alt to ignore grid. Right click to leave knife mode";

	if(UI()->MouseButtonClicked(1))
	{
		m_QuadKnifeActive = false;
		return;
	}

	// Handle snapping
	if(m_GridActive && !IgnoreGrid)
	{
		float CellSize = (float)GetLineDistance();
		vec2 OnGrid = vec2(roundf(Mouse.x / CellSize) * CellSize, roundf(Mouse.y / CellSize) * CellSize);

		if(IsInTriangle(OnGrid, v[0], v[1], v[2]) || IsInTriangle(OnGrid, v[0], v[3], v[2]))
			Point = OnGrid;
		else
		{
			float MinDistance = -1.f;

			for(int i = 0; i < 4; i++)
			{
				int j = (i + 1) % 4;
				vec2 Min(minimum(v[i].x, v[j].x), minimum(v[i].y, v[j].y));
				vec2 Max(maximum(v[i].x, v[j].x), maximum(v[i].y, v[j].y));

				if(in_range(OnGrid.y, Min.y, Max.y) && Max.y - Min.y > 0.0000001f)
				{
					vec2 OnEdge(v[i].x + (OnGrid.y - v[i].y) / (v[j].y - v[i].y) * (v[j].x - v[i].x), OnGrid.y);
					float Distance = abs(OnGrid.x - OnEdge.x);

					if(Distance < CellSize && (Distance < MinDistance || MinDistance < 0.f))
					{
						MinDistance = Distance;
						Point = OnEdge;
					}
				}

				if(in_range(OnGrid.x, Min.x, Max.x) && Max.x - Min.x > 0.0000001f)
				{
					vec2 OnEdge(OnGrid.x, v[i].y + (OnGrid.x - v[i].x) / (v[j].x - v[i].x) * (v[j].y - v[i].y));
					float Distance = abs(OnGrid.y - OnEdge.y);

					if(Distance < CellSize && (Distance < MinDistance || MinDistance < 0.f))
					{
						MinDistance = Distance;
						Point = OnEdge;
					}
				}
			}
		}
	}
	else
	{
		float MinDistance = -1.f;

		// Try snapping to corners
		for(const auto &x : v)
		{
			float Distance = distance(Mouse, x);

			if(Distance <= SnapRadius && (Distance < MinDistance || MinDistance < 0.f))
			{
				MinDistance = Distance;
				Point = x;
			}
		}

		if(MinDistance < 0.f)
		{
			// Try snapping to edges
			for(int i = 0; i < 4; i++)
			{
				int j = (i + 1) % 4;
				vec2 s(v[j] - v[i]);

				float t = ((Mouse.x - v[i].x) * s.x + (Mouse.y - v[i].y) * s.y) / (s.x * s.x + s.y * s.y);

				if(in_range(t, 0.f, 1.f))
				{
					vec2 OnEdge = vec2((v[i].x + t * s.x), (v[i].y + t * s.y));
					float Distance = distance(Mouse, OnEdge);

					if(Distance <= SnapRadius && (Distance < MinDistance || MinDistance < 0.f))
					{
						MinDistance = Distance;
						Point = OnEdge;
					}
				}
			}
		}
	}

	bool ValidPosition = IsInTriangle(Point, v[0], v[1], v[2]) || IsInTriangle(Point, v[0], v[3], v[2]);

	if(UI()->MouseButtonClicked(0) && ValidPosition)
	{
		m_aQuadKnifePoints[m_QuadKnifeCount] = Point;
		m_QuadKnifeCount++;
	}

	if(m_QuadKnifeCount == 4)
	{
		if(IsInTriangle(m_aQuadKnifePoints[3], m_aQuadKnifePoints[0], m_aQuadKnifePoints[1], m_aQuadKnifePoints[2]) ||
			IsInTriangle(m_aQuadKnifePoints[1], m_aQuadKnifePoints[0], m_aQuadKnifePoints[2], m_aQuadKnifePoints[3]))
		{
			// Fix concave order
			std::swap(m_aQuadKnifePoints[0], m_aQuadKnifePoints[3]);
			std::swap(m_aQuadKnifePoints[1], m_aQuadKnifePoints[2]);
		}

		std::swap(m_aQuadKnifePoints[2], m_aQuadKnifePoints[3]);

		CQuad *pResult = pLayer->NewQuad(64, 64, 64, 64);
		pQuad = &pLayer->m_vQuads[QuadIndex];

		for(int i = 0; i < 4; i++)
		{
			int t = IsInTriangle(m_aQuadKnifePoints[i], v[0], v[3], v[2]) ? 2 : 1;

			vec2 A = vec2(fx2f(pQuad->m_aPoints[0].x), fx2f(pQuad->m_aPoints[0].y));
			vec2 B = vec2(fx2f(pQuad->m_aPoints[3].x), fx2f(pQuad->m_aPoints[3].y));
			vec2 C = vec2(fx2f(pQuad->m_aPoints[t].x), fx2f(pQuad->m_aPoints[t].y));

			float TriArea = TriangleArea(A, B, C);
			float WeightA = TriangleArea(m_aQuadKnifePoints[i], B, C) / TriArea;
			float WeightB = TriangleArea(m_aQuadKnifePoints[i], C, A) / TriArea;
			float WeightC = TriangleArea(m_aQuadKnifePoints[i], A, B) / TriArea;

			pResult->m_aColors[i].r = (int)round(pQuad->m_aColors[0].r * WeightA + pQuad->m_aColors[3].r * WeightB + pQuad->m_aColors[t].r * WeightC);
			pResult->m_aColors[i].g = (int)round(pQuad->m_aColors[0].g * WeightA + pQuad->m_aColors[3].g * WeightB + pQuad->m_aColors[t].g * WeightC);
			pResult->m_aColors[i].b = (int)round(pQuad->m_aColors[0].b * WeightA + pQuad->m_aColors[3].b * WeightB + pQuad->m_aColors[t].b * WeightC);
			pResult->m_aColors[i].a = (int)round(pQuad->m_aColors[0].a * WeightA + pQuad->m_aColors[3].a * WeightB + pQuad->m_aColors[t].a * WeightC);

			pResult->m_aTexcoords[i].x = (int)round(pQuad->m_aTexcoords[0].x * WeightA + pQuad->m_aTexcoords[3].x * WeightB + pQuad->m_aTexcoords[t].x * WeightC);
			pResult->m_aTexcoords[i].y = (int)round(pQuad->m_aTexcoords[0].y * WeightA + pQuad->m_aTexcoords[3].y * WeightB + pQuad->m_aTexcoords[t].y * WeightC);

			pResult->m_aPoints[i].x = f2fx(m_aQuadKnifePoints[i].x);
			pResult->m_aPoints[i].y = f2fx(m_aQuadKnifePoints[i].y);
		}

		pResult->m_aPoints[4].x = ((pResult->m_aPoints[0].x + pResult->m_aPoints[3].x) / 2 + (pResult->m_aPoints[1].x + pResult->m_aPoints[2].x) / 2) / 2;
		pResult->m_aPoints[4].y = ((pResult->m_aPoints[0].y + pResult->m_aPoints[3].y) / 2 + (pResult->m_aPoints[1].y + pResult->m_aPoints[2].y) / 2) / 2;

		m_QuadKnifeCount = 0;
	}

	// Render
	Graphics()->TextureClear();
	Graphics()->LinesBegin();

	IGraphics::CLineItem aEdges[4] = {
		IGraphics::CLineItem(v[0].x, v[0].y, v[1].x, v[1].y),
		IGraphics::CLineItem(v[1].x, v[1].y, v[2].x, v[2].y),
		IGraphics::CLineItem(v[2].x, v[2].y, v[3].x, v[3].y),
		IGraphics::CLineItem(v[3].x, v[3].y, v[0].x, v[0].y)};

	Graphics()->SetColor(1.f, 0.5f, 0.f, 1.f);
	Graphics()->LinesDraw(aEdges, 4);

	IGraphics::CLineItem aLines[4];
	int LineCount = maximum(m_QuadKnifeCount - 1, 0);

	for(int i = 0; i < LineCount; i++)
		aLines[i] = IGraphics::CLineItem(m_aQuadKnifePoints[i].x, m_aQuadKnifePoints[i].y, m_aQuadKnifePoints[i + 1].x, m_aQuadKnifePoints[i + 1].y);

	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	Graphics()->LinesDraw(aLines, LineCount);

	if(ValidPosition)
	{
		if(m_QuadKnifeCount > 0)
		{
			IGraphics::CLineItem LineCurrent(Point.x, Point.y, m_aQuadKnifePoints[m_QuadKnifeCount - 1].x, m_aQuadKnifePoints[m_QuadKnifeCount - 1].y);
			Graphics()->LinesDraw(&LineCurrent, 1);
		}

		if(m_QuadKnifeCount == 3)
		{
			IGraphics::CLineItem LineClose(Point.x, Point.y, m_aQuadKnifePoints[0].x, m_aQuadKnifePoints[0].y);
			Graphics()->LinesDraw(&LineClose, 1);
		}
	}

	Graphics()->LinesEnd();
	Graphics()->QuadsBegin();

	IGraphics::CQuadItem aMarkers[4];

	for(int i = 0; i < m_QuadKnifeCount; i++)
		aMarkers[i] = IGraphics::CQuadItem(m_aQuadKnifePoints[i].x, m_aQuadKnifePoints[i].y, 5.f * m_MouseWScale, 5.f * m_MouseWScale);

	Graphics()->SetColor(0.f, 0.f, 1.f, 1.f);
	Graphics()->QuadsDraw(aMarkers, m_QuadKnifeCount);

	if(ValidPosition)
	{
		IGraphics::CQuadItem MarkerCurrent(Point.x, Point.y, 5.f * m_MouseWScale, 5.f * m_MouseWScale);
		Graphics()->QuadsDraw(&MarkerCurrent, 1);
	}

	Graphics()->QuadsEnd();
}

void CEditor::DoQuadEnvelopes(const std::vector<CQuad> &vQuads, IGraphics::CTextureHandle Texture)
{
	size_t Num = vQuads.size();
	CEnvelope **apEnvelope = new CEnvelope *[Num];
	mem_zero(apEnvelope, sizeof(CEnvelope *) * Num); // NOLINT(bugprone-sizeof-expression)
	for(size_t i = 0; i < Num; i++)
	{
		if((m_ShowEnvelopePreview == SHOWENV_SELECTED && vQuads[i].m_PosEnv == m_SelectedEnvelope) || m_ShowEnvelopePreview == SHOWENV_ALL)
			if(vQuads[i].m_PosEnv >= 0 && vQuads[i].m_PosEnv < (int)m_Map.m_vpEnvelopes.size())
				apEnvelope[i] = m_Map.m_vpEnvelopes[vQuads[i].m_PosEnv];
	}

	//Draw Lines
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(80.0f / 255, 150.0f / 255, 230.f / 255, 0.5f);
	for(size_t j = 0; j < Num; j++)
	{
		if(!apEnvelope[j])
			continue;

		//QuadParams
		const CPoint *pPoints = vQuads[j].m_aPoints;
		for(size_t i = 0; i < apEnvelope[j]->m_vPoints.size() - 1; i++)
		{
			float OffsetX = fx2f(apEnvelope[j]->m_vPoints[i].m_aValues[0]);
			float OffsetY = fx2f(apEnvelope[j]->m_vPoints[i].m_aValues[1]);
			vec2 Pos0 = vec2(fx2f(pPoints[4].x) + OffsetX, fx2f(pPoints[4].y) + OffsetY);

			OffsetX = fx2f(apEnvelope[j]->m_vPoints[i + 1].m_aValues[0]);
			OffsetY = fx2f(apEnvelope[j]->m_vPoints[i + 1].m_aValues[1]);
			vec2 Pos1 = vec2(fx2f(pPoints[4].x) + OffsetX, fx2f(pPoints[4].y) + OffsetY);

			IGraphics::CLineItem Line = IGraphics::CLineItem(Pos0.x, Pos0.y, Pos1.x, Pos1.y);
			Graphics()->LinesDraw(&Line, 1);
		}
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->LinesEnd();

	//Draw Quads
	Graphics()->TextureSet(Texture);
	Graphics()->QuadsBegin();

	for(size_t j = 0; j < Num; j++)
	{
		if(!apEnvelope[j])
			continue;

		//QuadParams
		const CPoint *pPoints = vQuads[j].m_aPoints;

		for(size_t i = 0; i < apEnvelope[j]->m_vPoints.size(); i++)
		{
			//Calc Env Position
			float OffsetX = fx2f(apEnvelope[j]->m_vPoints[i].m_aValues[0]);
			float OffsetY = fx2f(apEnvelope[j]->m_vPoints[i].m_aValues[1]);
			float Rot = fx2f(apEnvelope[j]->m_vPoints[i].m_aValues[2]) / 360.0f * pi * 2;

			//Set Colours
			float Alpha = (m_SelectedQuadEnvelope == vQuads[j].m_PosEnv && m_SelectedEnvelopePoint == (int)i) ? 0.65f : 0.35f;
			IGraphics::CColorVertex aArray[4] = {
				IGraphics::CColorVertex(0, vQuads[j].m_aColors[0].r, vQuads[j].m_aColors[0].g, vQuads[j].m_aColors[0].b, Alpha),
				IGraphics::CColorVertex(1, vQuads[j].m_aColors[1].r, vQuads[j].m_aColors[1].g, vQuads[j].m_aColors[1].b, Alpha),
				IGraphics::CColorVertex(2, vQuads[j].m_aColors[2].r, vQuads[j].m_aColors[2].g, vQuads[j].m_aColors[2].b, Alpha),
				IGraphics::CColorVertex(3, vQuads[j].m_aColors[3].r, vQuads[j].m_aColors[3].g, vQuads[j].m_aColors[3].b, Alpha)};
			Graphics()->SetColorVertex(aArray, 4);

			//Rotation
			if(Rot != 0)
			{
				static CPoint aRotated[4];
				aRotated[0] = vQuads[j].m_aPoints[0];
				aRotated[1] = vQuads[j].m_aPoints[1];
				aRotated[2] = vQuads[j].m_aPoints[2];
				aRotated[3] = vQuads[j].m_aPoints[3];
				pPoints = aRotated;

				Rotate(&vQuads[j].m_aPoints[4], &aRotated[0], Rot);
				Rotate(&vQuads[j].m_aPoints[4], &aRotated[1], Rot);
				Rotate(&vQuads[j].m_aPoints[4], &aRotated[2], Rot);
				Rotate(&vQuads[j].m_aPoints[4], &aRotated[3], Rot);
			}

			//Set Texture Coords
			Graphics()->QuadsSetSubsetFree(
				fx2f(vQuads[j].m_aTexcoords[0].x), fx2f(vQuads[j].m_aTexcoords[0].y),
				fx2f(vQuads[j].m_aTexcoords[1].x), fx2f(vQuads[j].m_aTexcoords[1].y),
				fx2f(vQuads[j].m_aTexcoords[2].x), fx2f(vQuads[j].m_aTexcoords[2].y),
				fx2f(vQuads[j].m_aTexcoords[3].x), fx2f(vQuads[j].m_aTexcoords[3].y));

			//Set Quad Coords & Draw
			IGraphics::CFreeformItem Freeform(
				fx2f(pPoints[0].x) + OffsetX, fx2f(pPoints[0].y) + OffsetY,
				fx2f(pPoints[1].x) + OffsetX, fx2f(pPoints[1].y) + OffsetY,
				fx2f(pPoints[2].x) + OffsetX, fx2f(pPoints[2].y) + OffsetY,
				fx2f(pPoints[3].x) + OffsetX, fx2f(pPoints[3].y) + OffsetY);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}
	}
	Graphics()->QuadsEnd();
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	// Draw QuadPoints
	for(size_t j = 0; j < Num; j++)
	{
		if(!apEnvelope[j])
			continue;

		for(size_t i = 0; i < apEnvelope[j]->m_vPoints.size(); i++)
			DoQuadEnvPoint(&vQuads[j], j, i);
	}
	Graphics()->QuadsEnd();
	delete[] apEnvelope;
}

void CEditor::DoQuadEnvPoint(const CQuad *pQuad, int QIndex, int PIndex)
{
	enum
	{
		OP_NONE = 0,
		OP_MOVE,
		OP_ROTATE,
	};

	// some basic values
	static float s_LastWx = 0;
	static float s_LastWy = 0;
	static int s_Operation = OP_NONE;
	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();
	CEnvelope *pEnvelope = m_Map.m_vpEnvelopes[pQuad->m_PosEnv];
	void *pID = &pEnvelope->m_vPoints[PIndex];
	static int s_CurQIndex = -1;

	// get pivot
	float CenterX = fx2f(pQuad->m_aPoints[4].x) + fx2f(pEnvelope->m_vPoints[PIndex].m_aValues[0]);
	float CenterY = fx2f(pQuad->m_aPoints[4].y) + fx2f(pEnvelope->m_vPoints[PIndex].m_aValues[1]);

	float dx = (CenterX - wx) / m_MouseWScale;
	float dy = (CenterY - wy) / m_MouseWScale;
	if(dx * dx + dy * dy < 50.0f && UI()->CheckActiveItem(nullptr))
	{
		UI()->SetHotItem(pID);
		s_CurQIndex = QIndex;
	}

	const bool IgnoreGrid = Input()->KeyIsPressed(KEY_LALT) || Input()->KeyIsPressed(KEY_RALT);

	if(UI()->CheckActiveItem(pID) && s_CurQIndex == QIndex)
	{
		if(s_Operation == OP_MOVE)
		{
			if(m_GridActive && !IgnoreGrid)
			{
				int LineDistance = GetLineDistance();

				float x = 0.0f;
				float y = 0.0f;
				if(wx >= 0)
					x = (int)((wx + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
				else
					x = (int)((wx - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
				if(wy >= 0)
					y = (int)((wy + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
				else
					y = (int)((wy - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);

				pEnvelope->m_vPoints[PIndex].m_aValues[0] = f2fx(x) - pQuad->m_aPoints[4].x;
				pEnvelope->m_vPoints[PIndex].m_aValues[1] = f2fx(y) - pQuad->m_aPoints[4].y;
			}
			else
			{
				pEnvelope->m_vPoints[PIndex].m_aValues[0] += f2fx(wx - s_LastWx);
				pEnvelope->m_vPoints[PIndex].m_aValues[1] += f2fx(wy - s_LastWy);
			}
		}
		else if(s_Operation == OP_ROTATE)
			pEnvelope->m_vPoints[PIndex].m_aValues[2] += 10 * m_MouseDeltaX;

		s_LastWx = wx;
		s_LastWy = wy;

		if(!UI()->MouseButton(0))
		{
			m_LockMouse = false;
			s_Operation = OP_NONE;
			UI()->SetActiveItem(nullptr);
		}

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	else if(UI()->HotItem() == pID && s_CurQIndex == QIndex)
	{
		ms_pUiGotContext = pID;

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		m_pTooltip = "Left mouse button to move. Hold ctrl to rotate. Hold alt to ignore grid.";

		if(UI()->MouseButton(0))
		{
			if(Input()->ModifierIsPressed())
			{
				m_LockMouse = true;
				s_Operation = OP_ROTATE;

				SelectQuad(QIndex);
			}
			else
			{
				s_Operation = OP_MOVE;

				SelectQuad(QIndex);
			}

			m_SelectedEnvelopePoint = PIndex;
			m_SelectedQuadEnvelope = pQuad->m_PosEnv;

			UI()->SetActiveItem(pID);

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

	IGraphics::CQuadItem QuadItem(CenterX, CenterY, 5.0f * m_MouseWScale, 5.0f * m_MouseWScale);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CEditor::DoMapEditor(CUIRect View)
{
	// render all good stuff
	if(!m_ShowPicker)
	{
		for(auto &pGroup : m_Map.m_vpGroups)
		{ // don't render the front, tele, speedup and switch layer now we will do it later to make them on top of others
			if(
				pGroup == (CLayerGroup *)m_Map.m_pFrontLayer ||
				pGroup == (CLayerGroup *)m_Map.m_pTeleLayer ||
				pGroup == (CLayerGroup *)m_Map.m_pSpeedupLayer ||
				pGroup == (CLayerGroup *)m_Map.m_pSwitchLayer ||
				pGroup == (CLayerGroup *)m_Map.m_pTuneLayer)
				continue;
			if(pGroup->m_Visible)
				pGroup->Render();
			//UI()->ClipEnable(&view);
		}

		// render the game, tele, speedup, front, tune and switch above everything else
		if(m_Map.m_pGameGroup->m_Visible)
		{
			m_Map.m_pGameGroup->MapScreen();
			for(auto &pLayer : m_Map.m_pGameGroup->m_vpLayers)
			{
				if(
					pLayer->m_Visible &&
					(pLayer == m_Map.m_pGameLayer ||
						pLayer == m_Map.m_pFrontLayer ||
						pLayer == m_Map.m_pTeleLayer ||
						pLayer == m_Map.m_pSpeedupLayer ||
						pLayer == m_Map.m_pSwitchLayer ||
						pLayer == m_Map.m_pTuneLayer))
					pLayer->Render();
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
		float Max = minimum(View.w, View.h);
		View.w = View.h = Max;
	}

	static void *s_pEditorID = (void *)&s_pEditorID;
	const bool Inside = UI()->MouseInside(&View);

	// fetch mouse position
	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();
	float mx = UI()->MouseX();
	float my = UI()->MouseY();

	static float s_StartWx = 0;
	static float s_StartWy = 0;

	enum
	{
		OP_NONE = 0,
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
		float Size = 32.0f * 16.0f;
		float w = Size * (Screen.w / View.w);
		float h = Size * (Screen.h / View.h);
		float x = -(View.x / Screen.w) * w;
		float y = -(View.y / Screen.h) * h;
		wx = x + w * mx / Screen.w;
		wy = y + h * my / Screen.h;
		CLayerTiles *pTileLayer = (CLayerTiles *)GetSelectedLayerType(0, LAYERTYPE_TILES);
		if(pTileLayer)
		{
			Graphics()->MapScreen(x, y, x + w, y + h);
			m_TilesetPicker.m_Image = pTileLayer->m_Image;
			m_TilesetPicker.m_Texture = pTileLayer->m_Texture;
			if(m_BrushColorEnabled)
			{
				m_TilesetPicker.m_Color = pTileLayer->m_Color;
				m_TilesetPicker.m_Color.a = 255;
			}
			else
			{
				m_TilesetPicker.m_Color = {255, 255, 255, 255};
			}

			m_TilesetPicker.m_Game = pTileLayer->m_Game;
			m_TilesetPicker.m_Tele = pTileLayer->m_Tele;
			m_TilesetPicker.m_Speedup = pTileLayer->m_Speedup;
			m_TilesetPicker.m_Front = pTileLayer->m_Front;
			m_TilesetPicker.m_Switch = pTileLayer->m_Switch;
			m_TilesetPicker.m_Tune = pTileLayer->m_Tune;

			m_TilesetPicker.Render(true);

			if(m_ShowTileInfo)
				m_TilesetPicker.ShowInfo();
		}
		else
		{
			CLayerQuads *pQuadLayer = (CLayerQuads *)GetSelectedLayerType(0, LAYERTYPE_QUADS);
			if(pQuadLayer)
			{
				m_QuadsetPicker.m_Image = pQuadLayer->m_Image;
				m_QuadsetPicker.m_vQuads[0].m_aPoints[0].x = f2fx(View.x);
				m_QuadsetPicker.m_vQuads[0].m_aPoints[0].y = f2fx(View.y);
				m_QuadsetPicker.m_vQuads[0].m_aPoints[1].x = f2fx((View.x + View.w));
				m_QuadsetPicker.m_vQuads[0].m_aPoints[1].y = f2fx(View.y);
				m_QuadsetPicker.m_vQuads[0].m_aPoints[2].x = f2fx(View.x);
				m_QuadsetPicker.m_vQuads[0].m_aPoints[2].y = f2fx((View.y + View.h));
				m_QuadsetPicker.m_vQuads[0].m_aPoints[3].x = f2fx((View.x + View.w));
				m_QuadsetPicker.m_vQuads[0].m_aPoints[3].y = f2fx((View.y + View.h));
				m_QuadsetPicker.m_vQuads[0].m_aPoints[4].x = f2fx((View.x + View.w / 2));
				m_QuadsetPicker.m_vQuads[0].m_aPoints[4].y = f2fx((View.y + View.h / 2));
				m_QuadsetPicker.Render();
			}
		}
	}

	static int s_Operation = OP_NONE;

	// draw layer borders
	CLayer *apEditLayers[128];
	size_t NumEditLayers = 0;

	if(m_ShowPicker && GetSelectedLayer(0) && GetSelectedLayer(0)->m_Type == LAYERTYPE_TILES)
	{
		apEditLayers[0] = &m_TilesetPicker;
		NumEditLayers++;
	}
	else if(m_ShowPicker)
	{
		apEditLayers[0] = &m_QuadsetPicker;
		NumEditLayers++;
	}
	else
	{
		// pick a type of layers to edit, prefering Tiles layers.
		int EditingType = -1;
		for(size_t i = 0; i < m_vSelectedLayers.size(); i++)
		{
			CLayer *pLayer = GetSelectedLayer(i);
			if(pLayer && (EditingType == -1 || pLayer->m_Type == LAYERTYPE_TILES))
			{
				EditingType = pLayer->m_Type;
				if(EditingType == LAYERTYPE_TILES)
					break;
			}
		}
		for(size_t i = 0; i < m_vSelectedLayers.size() && NumEditLayers < 128; i++)
		{
			apEditLayers[NumEditLayers] = GetSelectedLayerType(i, EditingType);
			if(apEditLayers[NumEditLayers])
			{
				NumEditLayers++;
			}
		}

		CLayerGroup *pGroup = GetSelectedGroup();
		if(pGroup)
		{
			pGroup->MapScreen();

			RenderGrid(pGroup);

			for(size_t i = 0; i < NumEditLayers; i++)
			{
				if(apEditLayers[i]->m_Type != LAYERTYPE_TILES)
					continue;

				float w, h;
				apEditLayers[i]->GetSize(&w, &h);

				IGraphics::CLineItem Array[4] = {
					IGraphics::CLineItem(0, 0, w, 0),
					IGraphics::CLineItem(w, 0, w, h),
					IGraphics::CLineItem(w, h, 0, h),
					IGraphics::CLineItem(0, h, 0, 0)};
				Graphics()->TextureClear();
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
		if(UI()->CheckActiveItem(nullptr) && (UI()->MouseButton(0) || UI()->MouseButton(2)))
		{
			s_StartWx = wx;
			s_StartWy = wy;

			if(Input()->ModifierIsPressed() || UI()->MouseButton(2))
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
			int Layer = NUM_LAYERS;
			if(m_ShowPicker)
			{
				CLayer *pLayer = GetSelectedLayer(0);
				if(pLayer == m_Map.m_pGameLayer)
					Layer = LAYER_GAME;
				else if(pLayer == m_Map.m_pFrontLayer)
					Layer = LAYER_FRONT;
				else if(pLayer == m_Map.m_pSwitchLayer)
					Layer = LAYER_SWITCH;
				else if(pLayer == m_Map.m_pTeleLayer)
					Layer = LAYER_TELE;
				else if(pLayer == m_Map.m_pSpeedupLayer)
					Layer = LAYER_SPEEDUP;
				else if(pLayer == m_Map.m_pTuneLayer)
					Layer = LAYER_TUNE;
			}
			if(m_ShowPicker && Layer != NUM_LAYERS)
			{
				if(m_SelectEntitiesImage == "DDNet")
					m_pTooltip = Explain(EXPLANATION_DDNET, (int)wx / 32 + (int)wy / 32 * 16, Layer);
				else if(m_SelectEntitiesImage == "FNG")
					m_pTooltip = Explain(EXPLANATION_FNG, (int)wx / 32 + (int)wy / 32 * 16, Layer);
				else if(m_SelectEntitiesImage == "Vanilla")
					m_pTooltip = Explain(EXPLANATION_VANILLA, (int)wx / 32 + (int)wy / 32 * 16, Layer);
			}
			else if(m_Brush.IsEmpty())
				m_pTooltip = "Use left mouse button to drag and create a brush. Hold shift to select multiple quads. Use ctrl+right mouse to select layer.";
			else
				m_pTooltip = "Use left mouse button to paint with the brush. Right button clears the brush.";

			if(UI()->CheckActiveItem(s_pEditorID))
			{
				CUIRect r;
				r.x = s_StartWx;
				r.y = s_StartWy;
				r.w = wx - s_StartWx;
				r.h = wy - s_StartWy;
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
						for(size_t k = 0; k < NumEditLayers; k++)
						{
							size_t BrushIndex = k % m_Brush.m_vpLayers.size();
							if(apEditLayers[k]->m_Type == m_Brush.m_vpLayers[BrushIndex]->m_Type)
							{
								if(apEditLayers[k]->m_Type == LAYERTYPE_TILES)
								{
									CLayerTiles *pLayer = (CLayerTiles *)apEditLayers[k];
									CLayerTiles *pBrushLayer = (CLayerTiles *)m_Brush.m_vpLayers[BrushIndex];

									if(pLayer->m_Tele <= pBrushLayer->m_Tele && pLayer->m_Speedup <= pBrushLayer->m_Speedup && pLayer->m_Front <= pBrushLayer->m_Front && pLayer->m_Game <= pBrushLayer->m_Game && pLayer->m_Switch <= pBrushLayer->m_Switch && pLayer->m_Tune <= pBrushLayer->m_Tune)
										pLayer->BrushDraw(pBrushLayer, wx, wy);
								}
								else
								{
									apEditLayers[k]->BrushDraw(m_Brush.m_vpLayers[BrushIndex], wx, wy);
								}
							}
						}
					}
				}
				else if(s_Operation == OP_BRUSH_GRAB)
				{
					if(!UI()->MouseButton(0))
					{
						if(Input()->KeyIsPressed(KEY_LSHIFT))
						{
							CLayerQuads *pQuadLayer = (CLayerQuads *)GetSelectedLayerType(0, LAYERTYPE_QUADS);
							if(pQuadLayer)
							{
								for(size_t i = 0; i < pQuadLayer->m_vQuads.size(); i++)
								{
									CQuad *pQuad = &pQuadLayer->m_vQuads[i];
									float px = fx2f(pQuad->m_aPoints[4].x);
									float py = fx2f(pQuad->m_aPoints[4].y);

									if(px > r.x && px < r.x + r.w && py > r.y && py < r.y + r.h)
										if(!IsQuadSelected(i))
											m_vSelectedQuads.push_back(i);
								}
							}
						}
						else
						{
							// grab brush
							char aBuf[256];
							str_format(aBuf, sizeof(aBuf), "grabbing %f %f %f %f", r.x, r.y, r.w, r.h);
							Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "editor", aBuf);

							// TODO: do all layers
							int Grabs = 0;
							for(size_t k = 0; k < NumEditLayers; k++)
								Grabs += apEditLayers[k]->BrushGrab(&m_Brush, r);
							if(Grabs == 0)
								m_Brush.Clear();

							for(auto &pLayer : m_Brush.m_vpLayers)
								pLayer->m_BrushRefCount = 1;

							m_vSelectedQuads.clear();
							m_SelectedPoints = 0;
						}
					}
					else
					{
						//editor.map.groups[selected_group]->mapscreen();
						for(size_t k = 0; k < NumEditLayers; k++)
							apEditLayers[k]->BrushSelecting(r);
						UI()->MapScreen();
					}
				}
				else if(s_Operation == OP_BRUSH_PAINT)
				{
					if(!UI()->MouseButton(0))
					{
						for(size_t k = 0; k < NumEditLayers; k++)
						{
							size_t BrushIndex = k;
							if(m_Brush.m_vpLayers.size() != NumEditLayers)
								BrushIndex = 0;
							CLayer *pBrush = m_Brush.IsEmpty() ? nullptr : m_Brush.m_vpLayers[BrushIndex];
							apEditLayers[k]->FillSelection(m_Brush.IsEmpty(), pBrush, r);
						}
					}
					else
					{
						//editor.map.groups[selected_group]->mapscreen();
						for(size_t k = 0; k < NumEditLayers; k++)
							apEditLayers[k]->BrushSelecting(r);
						UI()->MapScreen();
					}
				}
			}
			else
			{
				if(UI()->MouseButton(1))
				{
					for(auto &pLayer : m_Brush.m_vpLayers)
					{
						if(pLayer->m_BrushRefCount == 1)
							delete pLayer;
					}
					m_Brush.Clear();
				}

				if(UI()->MouseButton(0) && s_Operation == OP_NONE && !m_QuadKnifeActive)
				{
					UI()->SetActiveItem(s_pEditorID);

					if(m_Brush.IsEmpty())
						s_Operation = OP_BRUSH_GRAB;
					else
					{
						s_Operation = OP_BRUSH_DRAW;
						for(size_t k = 0; k < NumEditLayers; k++)
						{
							size_t BrushIndex = k;
							if(m_Brush.m_vpLayers.size() != NumEditLayers)
								BrushIndex = 0;
							if(apEditLayers[k]->m_Type == m_Brush.m_vpLayers[BrushIndex]->m_Type)
								apEditLayers[k]->BrushPlace(m_Brush.m_vpLayers[BrushIndex], wx, wy);
						}
					}

					CLayerTiles *pLayer = (CLayerTiles *)GetSelectedLayerType(0, LAYERTYPE_TILES);
					if((Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT)) && pLayer)
						s_Operation = OP_BRUSH_PAINT;
				}

				if(!m_Brush.IsEmpty())
				{
					m_Brush.m_OffsetX = -(int)wx;
					m_Brush.m_OffsetY = -(int)wy;
					for(const auto &pLayer : m_Brush.m_vpLayers)
					{
						if(pLayer->m_Type == LAYERTYPE_TILES)
						{
							m_Brush.m_OffsetX = -(int)(wx / 32.0f) * 32;
							m_Brush.m_OffsetY = -(int)(wy / 32.0f) * 32;
							break;
						}
					}

					CLayerGroup *pGroup = GetSelectedGroup();
					if(!m_ShowPicker && pGroup)
					{
						m_Brush.m_OffsetX += pGroup->m_OffsetX;
						m_Brush.m_OffsetY += pGroup->m_OffsetY;
						m_Brush.m_ParallaxX = pGroup->m_ParallaxX;
						m_Brush.m_ParallaxY = pGroup->m_ParallaxY;
						m_Brush.m_ParallaxZoom = pGroup->m_ParallaxZoom;
						m_Brush.Render();
						float w, h;
						m_Brush.GetSize(&w, &h);

						IGraphics::CLineItem Array[4] = {
							IGraphics::CLineItem(0, 0, w, 0),
							IGraphics::CLineItem(w, 0, w, h),
							IGraphics::CLineItem(w, h, 0, h),
							IGraphics::CLineItem(0, h, 0, 0)};
						Graphics()->TextureClear();
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
				CLayerGroup *pGroup = GetSelectedGroup();
				if(pGroup)
					pGroup->MapScreen();

				for(size_t k = 0; k < NumEditLayers; k++)
				{
					if(apEditLayers[k]->m_Type == LAYERTYPE_QUADS)
					{
						CLayerQuads *pLayer = (CLayerQuads *)apEditLayers[k];

						if(m_ShowEnvelopePreview == SHOWENV_NONE)
							m_ShowEnvelopePreview = SHOWENV_ALL;

						if(m_QuadKnifeActive)
							DoQuadKnife(m_vSelectedQuads[m_SelectedQuadIndex]);
						else
						{
							Graphics()->TextureClear();
							Graphics()->QuadsBegin();
							for(size_t i = 0; i < pLayer->m_vQuads.size(); i++)
							{
								for(int v = 0; v < 4; v++)
									DoQuadPoint(&pLayer->m_vQuads[i], i, v);

								DoQuad(&pLayer->m_vQuads[i], i);
							}
							Graphics()->QuadsEnd();
						}
					}

					if(apEditLayers[k]->m_Type == LAYERTYPE_SOUNDS)
					{
						CLayerSounds *pLayer = (CLayerSounds *)apEditLayers[k];

						Graphics()->TextureClear();
						Graphics()->QuadsBegin();
						for(size_t i = 0; i < pLayer->m_vSources.size(); i++)
						{
							DoSoundSource(&pLayer->m_vSources[i], i);
						}
						Graphics()->QuadsEnd();
					}
				}

				UI()->MapScreen();
			}
		}

		// do panning
		if(UI()->CheckActiveItem(s_pEditorID))
		{
			if(s_Operation == OP_PAN_WORLD)
			{
				m_WorldOffsetX -= m_MouseDeltaX * m_MouseWScale;
				m_WorldOffsetY -= m_MouseDeltaY * m_MouseWScale;
			}
			else if(s_Operation == OP_PAN_EDITOR)
			{
				m_EditorOffsetX -= m_MouseDeltaX * m_MouseWScale;
				m_EditorOffsetY -= m_MouseDeltaY * m_MouseWScale;
			}

			// release mouse
			if(!UI()->MouseButton(0))
			{
				s_Operation = OP_NONE;
				UI()->SetActiveItem(nullptr);
			}
		}
		if(!Input()->KeyIsPressed(KEY_LSHIFT) && !Input()->KeyIsPressed(KEY_RSHIFT) &&
			!Input()->ModifierIsPressed() &&
			m_Dialog == DIALOG_NONE && m_EditBoxActive == 0)
		{
			float PanSpeed = 64.0f;
			if(Input()->KeyPress(KEY_A))
				m_WorldOffsetX -= PanSpeed * m_MouseWScale;
			else if(Input()->KeyPress(KEY_D))
				m_WorldOffsetX += PanSpeed * m_MouseWScale;
			if(Input()->KeyPress(KEY_W))
				m_WorldOffsetY -= PanSpeed * m_MouseWScale;
			else if(Input()->KeyPress(KEY_S))
				m_WorldOffsetY += PanSpeed * m_MouseWScale;
		}
	}
	else if(UI()->CheckActiveItem(s_pEditorID))
	{
		// release mouse
		if(!UI()->MouseButton(0))
		{
			s_Operation = OP_NONE;
			UI()->SetActiveItem(nullptr);
		}
	}

	if(!m_ShowPicker && GetSelectedGroup() && GetSelectedGroup()->m_UseClipping)
	{
		CLayerGroup *pGameGroup = m_Map.m_pGameGroup;
		pGameGroup->MapScreen();

		Graphics()->TextureClear();
		Graphics()->LinesBegin();

		CUIRect r;
		r.x = GetSelectedGroup()->m_ClipX;
		r.y = GetSelectedGroup()->m_ClipY;
		r.w = GetSelectedGroup()->m_ClipW;
		r.h = GetSelectedGroup()->m_ClipH;

		IGraphics::CLineItem Array[4] = {
			IGraphics::CLineItem(r.x, r.y, r.x + r.w, r.y),
			IGraphics::CLineItem(r.x + r.w, r.y, r.x + r.w, r.y + r.h),
			IGraphics::CLineItem(r.x + r.w, r.y + r.h, r.x, r.y + r.h),
			IGraphics::CLineItem(r.x, r.y + r.h, r.x, r.y)};
		Graphics()->SetColor(1, 0, 0, 1);
		Graphics()->LinesDraw(Array, 4);

		Graphics()->LinesEnd();
	}

	// render screen sizes
	if(m_ProofBorders && !m_ShowPicker)
	{
		CLayerGroup *pGameGroup = m_Map.m_pGameGroup;
		pGameGroup->MapScreen();

		Graphics()->TextureClear();
		Graphics()->LinesBegin();

		// possible screen sizes (white border)
		float aLastPoints[4];
		float Start = 1.0f; //9.0f/16.0f;
		float End = 16.0f / 9.0f;
		const int NumSteps = 20;
		for(int i = 0; i <= NumSteps; i++)
		{
			float aPoints[4];
			float Aspect = Start + (End - Start) * (i / (float)NumSteps);

			RenderTools()->MapScreenToWorld(
				m_WorldOffsetX, m_WorldOffsetY,
				100.0f, 100.0f, 100.0f, 0.0f, 0.0f, Aspect, 1.0f, aPoints);

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
			Graphics()->SetColor(1, 0, 0, 1);
			for(int i = 0; i < 2; i++)
			{
				float aPoints[4];
				float aAspects[] = {4.0f / 3.0f, 16.0f / 10.0f, 5.0f / 4.0f, 16.0f / 9.0f};
				float Aspect = aAspects[i];

				RenderTools()->MapScreenToWorld(
					m_WorldOffsetX, m_WorldOffsetY,
					100.0f, 100.0f, 100.0f, 0.0f, 0.0f, Aspect, 1.0f, aPoints);

				CUIRect r;
				r.x = aPoints[0];
				r.y = aPoints[1];
				r.w = aPoints[2] - aPoints[0];
				r.h = aPoints[3] - aPoints[1];

				IGraphics::CLineItem Array[4] = {
					IGraphics::CLineItem(r.x, r.y, r.x + r.w, r.y),
					IGraphics::CLineItem(r.x + r.w, r.y, r.x + r.w, r.y + r.h),
					IGraphics::CLineItem(r.x + r.w, r.y + r.h, r.x, r.y + r.h),
					IGraphics::CLineItem(r.x, r.y + r.h, r.x, r.y)};
				Graphics()->LinesDraw(Array, 4);
				Graphics()->SetColor(0, 1, 0, 1);
			}
		}
		Graphics()->LinesEnd();

		// tee position (blue circle)
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0, 0, 1, 0.3f);
			Graphics()->DrawCircle(m_WorldOffsetX, m_WorldOffsetY - 3.0f, 20.0f, 32);
			Graphics()->QuadsEnd();
		}
	}

	if(!m_ShowPicker && m_ShowTileInfo && m_ShowEnvelopePreview != SHOWENV_NONE && GetSelectedLayer(0) && GetSelectedLayer(0)->m_Type == LAYERTYPE_QUADS)
	{
		GetSelectedGroup()->MapScreen();

		CLayerQuads *pLayer = (CLayerQuads *)GetSelectedLayer(0);
		IGraphics::CTextureHandle Texture;
		if(pLayer->m_Image >= 0 && pLayer->m_Image < (int)m_Map.m_vpImages.size())
			Texture = m_Map.m_vpImages[pLayer->m_Image]->m_Texture;

		DoQuadEnvelopes(pLayer->m_vQuads, Texture);
		m_ShowEnvelopePreview = SHOWENV_NONE;
	}

	UI()->MapScreen();
	//UI()->ClipDisable();
}

float CEditor::ScaleFontSize(char *pText, int TextSize, float FontSize, int Width)
{
	while(TextRender()->TextWidth(nullptr, FontSize, pText, -1, -1.0f) > Width)
	{
		if(FontSize > 6.0f)
		{
			FontSize--;
		}
		else
		{
			pText[str_length(pText) - 4] = '\0';
			str_append(pText, "…", TextSize);
		}
	}
	return FontSize;
}

int CEditor::DoProperties(CUIRect *pToolBox, CProperty *pProps, int *pIDs, int *pNewVal, ColorRGBA Color)
{
	int Change = -1;

	for(int i = 0; pProps[i].m_pName; i++)
	{
		CUIRect Slot;
		pToolBox->HSplitTop(13.0f, &Slot, pToolBox);
		CUIRect Label, Shifter;
		Slot.VSplitMid(&Label, &Shifter);
		Shifter.HMargin(1.0f, &Shifter);
		UI()->DoLabel(&Label, pProps[i].m_pName, 10.0f, TEXTALIGN_LEFT);

		if(pProps[i].m_Type == PROPTYPE_INT_STEP)
		{
			CUIRect Inc, Dec;
			char aBuf[64];

			Shifter.VSplitRight(10.0f, &Shifter, &Inc);
			Shifter.VSplitLeft(10.0f, &Dec, &Shifter);
			str_format(aBuf, sizeof(aBuf), "%d", pProps[i].m_Value);
			int NewValue = UiDoValueSelector((char *)&pIDs[i], &Shifter, "", pProps[i].m_Value, pProps[i].m_Min, pProps[i].m_Max, 1, 1.0f, "Use left mouse button to drag and change the value. Hold shift to be more precise. Rightclick to edit as text.", false, false, 0, &Color);
			if(NewValue != pProps[i].m_Value)
			{
				*pNewVal = NewValue;
				Change = i;
			}
			if(DoButton_ButtonDec((char *)&pIDs[i] + 1, nullptr, 0, &Dec, 0, "Decrease"))
			{
				*pNewVal = pProps[i].m_Value - 1;
				Change = i;
			}
			if(DoButton_ButtonInc(((char *)&pIDs[i]) + 2, nullptr, 0, &Inc, 0, "Increase"))
			{
				*pNewVal = pProps[i].m_Value + 1;
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
			if(DoButton_ButtonInc(((char *)&pIDs[i]) + 1, "Yes", pProps[i].m_Value, &Yes, 0, ""))
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
			int Step = Shift ? 1 : 45;
			int Value = pProps[i].m_Value;

			int NewValue = UiDoValueSelector(&pIDs[i], &Shifter, "", Value, pProps[i].m_Min, pProps[i].m_Max, Shift ? 1 : 45, Shift ? 1.0f : 10.0f, "Use left mouse button to drag and change the value. Hold shift to be more precise. Rightclick to edit as text.", false, false, 0);
			if(DoButton_ButtonDec(&pIDs[i] + 1, nullptr, 0, &Dec, 0, "Decrease"))
			{
				NewValue = (round_ceil((pProps[i].m_Value / (float)Step)) - 1) * Step;
				if(NewValue < 0)
					NewValue += 360;
			}
			if(DoButton_ButtonInc(&pIDs[i] + 2, nullptr, 0, &Inc, 0, "Increase"))
				NewValue = (pProps[i].m_Value + Step) / Step * Step;

			if(NewValue != pProps[i].m_Value)
			{
				*pNewVal = NewValue % 360;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_COLOR)
		{
			static const char *s_apTexts[4] = {"R", "G", "B", "A"};
			static int s_aShift[] = {24, 16, 8, 0};
			int NewColor = 0;

			// extra space
			CUIRect ColorBox, ColorSlots;

			pToolBox->HSplitTop(3.0f * 13.0f, &Slot, pToolBox);
			Slot.VSplitMid(&ColorBox, &ColorSlots);
			ColorBox.HMargin(1.0f, &ColorBox);
			ColorBox.VMargin(6.0f, &ColorBox);

			for(int c = 0; c < 4; c++)
			{
				int v = (pProps[i].m_Value >> s_aShift[c]) & 0xff;
				NewColor |= UiDoValueSelector(((char *)&pIDs[i]) + c, &Shifter, s_apTexts[c], v, 0, 255, 1, 1.0f, "Use left mouse button to drag and change the color value. Hold shift to be more precise. Rightclick to edit as text.") << s_aShift[c];

				if(c != 3)
				{
					ColorSlots.HSplitTop(13.0f, &Shifter, &ColorSlots);
					Shifter.HMargin(1.0f, &Shifter);
				}
			}

			// hex
			pToolBox->HSplitTop(13.0f, &Slot, pToolBox);
			Slot.VSplitMid(nullptr, &Shifter);
			Shifter.HMargin(1.0f, &Shifter);

			int NewColorHex = pProps[i].m_Value & 0xff;
			NewColorHex |= UiDoValueSelector(((char *)&pIDs[i] - 1), &Shifter, "", (pProps[i].m_Value >> 8) & 0xFFFFFF, 0, 0xFFFFFF, 1, 1.0f, "Use left mouse button to drag and change the color value. Hold shift to be more precise. Rightclick to edit as text.", false, true) << 8;

			// color picker
			ColorRGBA ColorPick = ColorRGBA(
				((pProps[i].m_Value >> s_aShift[0]) & 0xff) / 255.0f,
				((pProps[i].m_Value >> s_aShift[1]) & 0xff) / 255.0f,
				((pProps[i].m_Value >> s_aShift[2]) & 0xff) / 255.0f,
				1.0f);

			static int s_ColorPicker, s_ColorPickerID;
			if(DoButton_ColorPicker(&s_ColorPicker, &ColorBox, &ColorPick))
			{
				ms_PickerColor = color_cast<ColorHSVA>(ColorPick);
				UiInvokePopupMenu(&s_ColorPickerID, 0, UI()->MouseX(), UI()->MouseY(), 180, 150, PopupColorPicker);
			}

			if(UI()->HotItem() == &ms_SVPicker || UI()->HotItem() == &ms_HuePicker)
			{
				ColorRGBA c = color_cast<ColorRGBA>(ms_PickerColor);
				NewColor = ((int)(c.r * 255.0f) & 0xff) << 24 | ((int)(c.g * 255.0f) & 0xff) << 16 | ((int)(c.b * 255.0f) & 0xff) << 8 | (pProps[i].m_Value & 0xff);
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
				str_format(aBuf, sizeof(aBuf), "%s", m_Map.m_vpImages[pProps[i].m_Value]->m_aName);

			float FontSize = ScaleFontSize(aBuf, sizeof(aBuf), 10.0f, Shifter.w);
			if(DoButton_Ex(&pIDs[i], aBuf, 0, &Shifter, 0, nullptr, IGraphics::CORNER_ALL, FontSize))
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
			Shifter.VSplitMid(&Left, &Up, 2.0f);
			Left.VSplitLeft(10.0f, &Left, &Shifter);
			Shifter.VSplitRight(10.0f, &Shifter, &Right);
			Shifter.Draw(ColorRGBA(1, 1, 1, 0.5f), 0, 0.0f);
			UI()->DoLabel(&Shifter, "X", 10.0f, TEXTALIGN_CENTER);
			Up.VSplitLeft(10.0f, &Up, &Shifter);
			Shifter.VSplitRight(10.0f, &Shifter, &Down);
			Shifter.Draw(ColorRGBA(1, 1, 1, 0.5f), 0, 0.0f);
			UI()->DoLabel(&Shifter, "Y", 10.0f, TEXTALIGN_CENTER);
			if(DoButton_ButtonDec(&pIDs[i], "-", 0, &Left, 0, "Left"))
			{
				*pNewVal = 1;
				Change = i;
			}
			if(DoButton_ButtonInc(((char *)&pIDs[i]) + 3, "+", 0, &Right, 0, "Right"))
			{
				*pNewVal = 2;
				Change = i;
			}
			if(DoButton_ButtonDec(((char *)&pIDs[i]) + 1, "-", 0, &Up, 0, "Up"))
			{
				*pNewVal = 4;
				Change = i;
			}
			if(DoButton_ButtonInc(((char *)&pIDs[i]) + 2, "+", 0, &Down, 0, "Down"))
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
				str_format(aBuf, sizeof(aBuf), "%s", m_Map.m_vpSounds[pProps[i].m_Value]->m_aName);

			float FontSize = ScaleFontSize(aBuf, sizeof(aBuf), 10.0f, Shifter.w);
			if(DoButton_Ex(&pIDs[i], aBuf, 0, &Shifter, 0, nullptr, IGraphics::CORNER_ALL, FontSize))
				PopupSelectSoundInvoke(pProps[i].m_Value, UI()->MouseX(), UI()->MouseY());

			int r = PopupSelectSoundResult();
			if(r >= -1)
			{
				*pNewVal = r;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_AUTOMAPPER)
		{
			char aBuf[64];
			if(pProps[i].m_Value < 0 || pProps[i].m_Min < 0 || pProps[i].m_Min >= (int)m_Map.m_vpImages.size())
				str_copy(aBuf, "None", sizeof(aBuf));
			else
				str_format(aBuf, sizeof(aBuf), "%s", m_Map.m_vpImages[pProps[i].m_Min]->m_AutoMapper.GetConfigName(pProps[i].m_Value));

			float FontSize = ScaleFontSize(aBuf, sizeof(aBuf), 10.0f, Shifter.w);
			if(DoButton_Ex(&pIDs[i], aBuf, 0, &Shifter, 0, nullptr, IGraphics::CORNER_ALL, FontSize))
				PopupSelectConfigAutoMapInvoke(pProps[i].m_Value, UI()->MouseX(), UI()->MouseY());

			int r = PopupSelectConfigAutoMapResult();
			if(r >= -1)
			{
				*pNewVal = r;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_ENVELOPE)
		{
			CUIRect Inc, Dec;
			char aBuf[64];
			int CurValue = pProps[i].m_Value;

			Shifter.VSplitRight(10.0f, &Shifter, &Inc);
			Shifter.VSplitLeft(10.0f, &Dec, &Shifter);

			if(CurValue <= 0)
				str_copy(aBuf, "None", sizeof(aBuf));
			else if(m_Map.m_vpEnvelopes[CurValue - 1]->m_aName[0])
				str_format(aBuf, sizeof(aBuf), "%d: %s", CurValue, m_Map.m_vpEnvelopes[CurValue - 1]->m_aName);
			else
				str_format(aBuf, sizeof(aBuf), "%d", CurValue);

			float FontSize = ScaleFontSize(aBuf, sizeof(aBuf), 10.0f, Shifter.w);
			Shifter.Draw(Color, 0, 5.0f);
			UI()->DoLabel(&Shifter, aBuf, FontSize, TEXTALIGN_CENTER);

			if(DoButton_ButtonDec((char *)&pIDs[i] + 1, nullptr, 0, &Dec, 0, "Previous Envelope"))
			{
				*pNewVal = pProps[i].m_Value - 1;
				Change = i;
			}
			if(DoButton_ButtonInc(((char *)&pIDs[i]) + 2, nullptr, 0, &Inc, 0, "Next Envelope"))
			{
				*pNewVal = pProps[i].m_Value + 1;
				Change = i;
			}
		}
	}

	return Change;
}

void CEditor::RenderLayers(CUIRect LayersBox)
{
	const float RowHeight = 12.0f;
	char aBuf[64];

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollbarWidth = 10.0f;
	ScrollParams.m_ScrollbarMargin = 3.0f;
	ScrollParams.m_ScrollUnit = RowHeight * 5.0f;
	s_ScrollRegion.Begin(&LayersBox, &ScrollOffset, &ScrollParams);
	LayersBox.y += ScrollOffset.y;

	const bool ScrollToSelection = SelectLayerByTile();

	// render layers
	for(int g = 0; g < (int)m_Map.m_vpGroups.size(); g++)
	{
		CUIRect Slot, VisibleToggle;
		LayersBox.HSplitTop(RowHeight, &Slot, &LayersBox);
		if(s_ScrollRegion.AddRect(Slot))
		{
			Slot.VSplitLeft(12.0f, &VisibleToggle, &Slot);
			if(DoButton_Ex(&m_Map.m_vpGroups[g]->m_Visible, m_Map.m_vpGroups[g]->m_Visible ? "V" : "H", m_Map.m_vpGroups[g]->m_Collapse ? 1 : 0, &VisibleToggle, 0, "Toggle group visibility", IGraphics::CORNER_L, 10.0f, 0))
				m_Map.m_vpGroups[g]->m_Visible = !m_Map.m_vpGroups[g]->m_Visible;

			str_format(aBuf, sizeof(aBuf), "#%d %s", g, m_Map.m_vpGroups[g]->m_aName);
			float FontSize = 10.0f;
			while(TextRender()->TextWidth(nullptr, FontSize, aBuf, -1, -1.0f) > Slot.w && FontSize >= 7.0f)
				FontSize--;
			if(int Result = DoButton_Ex(&m_Map.m_vpGroups[g], aBuf, g == m_SelectedGroup, &Slot,
				   BUTTON_CONTEXT, m_Map.m_vpGroups[g]->m_Collapse ? "Select group. Shift click to select all layers. Double click to expand." : "Select group. Shift click to select all layers. Double click to collapse.", IGraphics::CORNER_R, FontSize))
			{
				if(g != m_SelectedGroup)
					SelectLayer(0, g);

				if((Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT)) && m_SelectedGroup == g)
				{
					m_vSelectedLayers.clear();
					for(size_t i = 0; i < m_Map.m_vpGroups[g]->m_vpLayers.size(); i++)
					{
						AddSelectedLayer(i);
					}
				}

				static int s_GroupPopupId = 0;
				if(Result == 2)
					UiInvokePopupMenu(&s_GroupPopupId, 0, UI()->MouseX(), UI()->MouseY(), 145, 256, PopupGroup);

				if(!m_Map.m_vpGroups[g]->m_vpLayers.empty() && Input()->MouseDoubleClick())
					m_Map.m_vpGroups[g]->m_Collapse ^= 1;
			}
		}

		LayersBox.HSplitTop(2.0f, &Slot, &LayersBox);
		s_ScrollRegion.AddRect(Slot);

		for(int i = 0; i < (int)m_Map.m_vpGroups[g]->m_vpLayers.size(); i++)
		{
			if(m_Map.m_vpGroups[g]->m_Collapse)
				continue;

			bool IsLayerSelected = false;
			if(m_SelectedGroup == g)
			{
				for(const auto &Selected : m_vSelectedLayers)
				{
					if(Selected == i)
					{
						IsLayerSelected = true;
						break;
					}
				}
			}

			LayersBox.HSplitTop(RowHeight + 2.0f, &Slot, &LayersBox);
			if(!s_ScrollRegion.AddRect(Slot, ScrollToSelection && IsLayerSelected))
				continue;
			Slot.HSplitTop(RowHeight, &Slot, nullptr);

			CUIRect Button;
			Slot.VSplitLeft(12.0f, nullptr, &Slot);
			Slot.VSplitLeft(15.0f, &VisibleToggle, &Button);

			if(DoButton_Ex(&m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Visible, m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Visible ? "V" : "H", 0, &VisibleToggle, 0, "Toggle layer visibility", IGraphics::CORNER_L, 10.0f, 0))
				m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Visible = !m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Visible;

			if(m_Map.m_vpGroups[g]->m_vpLayers[i]->m_aName[0])
				str_copy(aBuf, m_Map.m_vpGroups[g]->m_vpLayers[i]->m_aName, sizeof(aBuf));
			else
			{
				if(m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Type == LAYERTYPE_TILES)
				{
					CLayerTiles *pTiles = (CLayerTiles *)m_Map.m_vpGroups[g]->m_vpLayers[i];
					str_copy(aBuf, pTiles->m_Image >= 0 ? m_Map.m_vpImages[pTiles->m_Image]->m_aName : "Tiles", sizeof(aBuf));
				}
				else if(m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Type == LAYERTYPE_QUADS)
				{
					CLayerQuads *pQuads = (CLayerQuads *)m_Map.m_vpGroups[g]->m_vpLayers[i];
					str_copy(aBuf, pQuads->m_Image >= 0 ? m_Map.m_vpImages[pQuads->m_Image]->m_aName : "Quads", sizeof(aBuf));
				}
				else if(m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Type == LAYERTYPE_SOUNDS)
				{
					CLayerSounds *pSounds = (CLayerSounds *)m_Map.m_vpGroups[g]->m_vpLayers[i];
					str_copy(aBuf, pSounds->m_Sound >= 0 ? m_Map.m_vpSounds[pSounds->m_Sound]->m_aName : "Sounds", sizeof(aBuf));
				}
				if(str_length(aBuf) > 11)
					str_format(aBuf, sizeof(aBuf), "%.8s...", aBuf);
			}

			float FontSize = 10.0f;
			while(TextRender()->TextWidth(nullptr, FontSize, aBuf, -1, -1.0f) > Button.w && FontSize >= 7.0f)
				FontSize--;

			int Checked = IsLayerSelected ? 1 : 0;
			if(m_Map.m_vpGroups[g]->m_vpLayers[i] == m_Map.m_pGameLayer ||
				m_Map.m_vpGroups[g]->m_vpLayers[i] == m_Map.m_pFrontLayer ||
				m_Map.m_vpGroups[g]->m_vpLayers[i] == m_Map.m_pSwitchLayer ||
				m_Map.m_vpGroups[g]->m_vpLayers[i] == m_Map.m_pTuneLayer ||
				m_Map.m_vpGroups[g]->m_vpLayers[i] == m_Map.m_pSpeedupLayer ||
				m_Map.m_vpGroups[g]->m_vpLayers[i] == m_Map.m_pTeleLayer)
			{
				Checked += 6;
			}
			if(int Result = DoButton_Ex(m_Map.m_vpGroups[g]->m_vpLayers[i], aBuf, Checked, &Button,
				   BUTTON_CONTEXT, "Select layer. Shift click to select multiple.", IGraphics::CORNER_R, FontSize))
			{
				static CLayerPopupContext s_LayerPopupContext = {};
				if(Result == 1)
				{
					if((Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT)) && m_SelectedGroup == g)
					{
						auto Position = std::find(m_vSelectedLayers.begin(), m_vSelectedLayers.end(), i);
						if(Position != m_vSelectedLayers.end())
							m_vSelectedLayers.erase(Position);
						else
							AddSelectedLayer(i);
					}
					else if(!(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT)))
					{
						SelectLayer(i, g);
					}
				}
				else if(Result == 2)
				{
					if(!IsLayerSelected)
					{
						SelectLayer(i, g);
					}

					if(m_vSelectedLayers.size() > 1)
					{
						bool AllTile = true;
						for(size_t j = 0; AllTile && j < m_vSelectedLayers.size(); j++)
						{
							if(m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers[m_vSelectedLayers[j]]->m_Type == LAYERTYPE_TILES)
								s_LayerPopupContext.m_vpLayers.push_back((CLayerTiles *)m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers[m_vSelectedLayers[j]]);
							else
								AllTile = false;
						}

						// Don't allow editing if all selected layers are tile layers
						if(!AllTile)
							s_LayerPopupContext.m_vpLayers.clear();
					}
					else
						s_LayerPopupContext.m_vpLayers.clear();

					UiInvokePopupMenu(&s_LayerPopupContext, 0, UI()->MouseX(), UI()->MouseY(), 120, 320, PopupLayer, &s_LayerPopupContext);
				}
			}
		}

		LayersBox.HSplitTop(5.0f, &Slot, &LayersBox);
		s_ScrollRegion.AddRect(Slot);
	}

	if(Input()->KeyPress(KEY_DOWN) && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0)
	{
		if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
		{
			if(m_vSelectedLayers[m_vSelectedLayers.size() - 1] < (int)m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers.size() - 1)
				AddSelectedLayer(m_vSelectedLayers[m_vSelectedLayers.size() - 1] + 1);
		}
		else
		{
			int CurrentLayer = 0;
			for(const auto &Selected : m_vSelectedLayers)
				CurrentLayer = maximum(Selected, CurrentLayer);
			SelectLayer(CurrentLayer);

			if(m_vSelectedLayers[0] < (int)m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers.size() - 1)
			{
				SelectLayer(m_vSelectedLayers[0] + 1);
			}
			else
			{
				for(size_t Group = m_SelectedGroup + 1; Group < m_Map.m_vpGroups.size(); Group++)
				{
					if(!m_Map.m_vpGroups[Group]->m_vpLayers.empty())
					{
						SelectLayer(0, Group);
						break;
					}
				}
			}
		}
	}
	if(Input()->KeyPress(KEY_UP) && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0)
	{
		if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
		{
			if(m_vSelectedLayers[m_vSelectedLayers.size() - 1] > 0)
				AddSelectedLayer(m_vSelectedLayers[m_vSelectedLayers.size() - 1] - 1);
		}
		else
		{
			int CurrentLayer = std::numeric_limits<int>::max();
			for(const auto &Selected : m_vSelectedLayers)
				CurrentLayer = minimum(Selected, CurrentLayer);
			SelectLayer(CurrentLayer);

			if(m_vSelectedLayers[0] > 0)
			{
				SelectLayer(m_vSelectedLayers[0] - 1);
			}
			else
			{
				for(int Group = m_SelectedGroup - 1; Group >= 0; Group--)
				{
					if(!m_Map.m_vpGroups[Group]->m_vpLayers.empty())
					{
						SelectLayer(m_Map.m_vpGroups[Group]->m_vpLayers.size() - 1, Group);
						break;
					}
				}
			}
		}
	}

	CUIRect AddGroupButton;
	LayersBox.HSplitTop(RowHeight + 1.0f, &AddGroupButton, &LayersBox);
	if(s_ScrollRegion.AddRect(AddGroupButton))
	{
		AddGroupButton.HSplitTop(RowHeight, &AddGroupButton, 0);
		static int s_AddGroupButton = 0;
		if(DoButton_Editor(&s_AddGroupButton, "Add group", 0, &AddGroupButton, IGraphics::CORNER_R, "Adds a new group"))
		{
			m_Map.NewGroup();
			m_SelectedGroup = m_Map.m_vpGroups.size() - 1;
		}
	}

	s_ScrollRegion.End();
}

bool CEditor::SelectLayerByTile()
{
	// ctrl+rightclick a map index to select the layer that has a tile there
	static bool s_CtrlClick = false;
	static int s_Selected = 0;
	int MatchedGroup = -1;
	int MatchedLayer = -1;
	int Matches = 0;
	bool IsFound = false;
	if(UI()->MouseButton(1) && Input()->ModifierIsPressed())
	{
		if(s_CtrlClick)
			return false;
		s_CtrlClick = true;
		for(size_t g = 0; g < m_Map.m_vpGroups.size(); g++)
		{
			for(size_t l = 0; l < m_Map.m_vpGroups[g]->m_vpLayers.size(); l++)
			{
				if(IsFound)
					continue;
				if(m_Map.m_vpGroups[g]->m_vpLayers[l]->m_Type != LAYERTYPE_TILES)
					continue;

				CLayerTiles *pTiles = (CLayerTiles *)m_Map.m_vpGroups[g]->m_vpLayers[l];
				int x = (int)UI()->MouseWorldX() / 32 + m_Map.m_vpGroups[g]->m_OffsetX;
				int y = (int)UI()->MouseWorldY() / 32 + m_Map.m_vpGroups[g]->m_OffsetY;
				if(x < 0 || x >= pTiles->m_Width)
					continue;
				if(y < 0 || y >= pTiles->m_Height)
					continue;
				CTile Tile = pTiles->GetTile(x, y);
				if(Tile.m_Index)
				{
					if(MatchedGroup == -1)
					{
						MatchedGroup = g;
						MatchedLayer = l;
					}
					if(++Matches > s_Selected)
					{
						s_Selected++;
						MatchedGroup = g;
						MatchedLayer = l;
						IsFound = true;
					}
				}
			}
		}
		if(MatchedGroup != -1 && MatchedLayer != -1)
		{
			if(!IsFound)
				s_Selected = 1;
			SelectLayer(MatchedLayer, MatchedGroup);
			return true;
		}
	}
	else
		s_CtrlClick = false;
	return false;
}

void CEditor::ReplaceImage(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;
	CEditorImage ImgInfo(pEditor);
	if(!pEditor->Graphics()->LoadPNG(&ImgInfo, pFileName, StorageType))
		return;

	CEditorImage *pImg = pEditor->m_Map.m_vpImages[pEditor->m_SelectedImage];
	pEditor->Graphics()->UnloadTexture(&(pImg->m_Texture));
	free(pImg->m_pData);
	pImg->m_pData = nullptr;
	*pImg = ImgInfo;
	IStorage::StripPathAndExtension(pFileName, pImg->m_aName, sizeof(pImg->m_aName));
	pImg->m_External = IsVanillaImage(pImg->m_aName);

	if(!pImg->m_External && g_Config.m_ClEditorDilate == 1 && pImg->m_Format == CImageInfo::FORMAT_RGBA)
	{
		int ColorChannelCount = 0;
		if(ImgInfo.m_Format == CImageInfo::FORMAT_RGBA)
			ColorChannelCount = 4;

		DilateImage((unsigned char *)ImgInfo.m_pData, ImgInfo.m_Width, ImgInfo.m_Height, ColorChannelCount);
	}

	pImg->m_AutoMapper.Load(pImg->m_aName);
	int TextureLoadFlag = pEditor->Graphics()->HasTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE;
	if(ImgInfo.m_Width % 16 != 0 || ImgInfo.m_Height % 16 != 0)
		TextureLoadFlag = 0;
	pImg->m_Texture = pEditor->Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, CImageInfo::FORMAT_AUTO, TextureLoadFlag, pFileName);
	ImgInfo.m_pData = nullptr;
	pEditor->SortImages();
	for(size_t i = 0; i < pEditor->m_Map.m_vpImages.size(); ++i)
	{
		if(!str_comp(pEditor->m_Map.m_vpImages[i]->m_aName, pImg->m_aName))
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
	for(const auto &pImage : pEditor->m_Map.m_vpImages)
	{
		if(!str_comp(pImage->m_aName, aBuf))
			return;
	}

	if(pEditor->m_Map.m_vpImages.size() >= 64) // hard limit for teeworlds
	{
		pEditor->m_PopupEventType = pEditor->POPEVENT_IMAGE_MAX;
		pEditor->m_PopupEventActivated = true;
		return;
	}

	CEditorImage *pImg = new CEditorImage(pEditor);
	*pImg = ImgInfo;
	pImg->m_External = IsVanillaImage(aBuf);

	if(!pImg->m_External && g_Config.m_ClEditorDilate == 1 && pImg->m_Format == CImageInfo::FORMAT_RGBA)
	{
		int ColorChannelCount = 0;
		if(ImgInfo.m_Format == CImageInfo::FORMAT_RGBA)
			ColorChannelCount = 4;

		DilateImage((unsigned char *)ImgInfo.m_pData, ImgInfo.m_Width, ImgInfo.m_Height, ColorChannelCount);
	}

	int TextureLoadFlag = pEditor->Graphics()->HasTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE;
	if(ImgInfo.m_Width % 16 != 0 || ImgInfo.m_Height % 16 != 0)
		TextureLoadFlag = 0;
	pImg->m_Texture = pEditor->Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, CImageInfo::FORMAT_AUTO, TextureLoadFlag, pFileName);
	ImgInfo.m_pData = nullptr;
	str_copy(pImg->m_aName, aBuf, sizeof(pImg->m_aName));
	pImg->m_AutoMapper.Load(pImg->m_aName);
	pEditor->m_Map.m_vpImages.push_back(pImg);
	pEditor->SortImages();
	if(pEditor->m_SelectedImage >= 0 && (size_t)pEditor->m_SelectedImage < pEditor->m_Map.m_vpImages.size())
	{
		for(int i = 0; i <= pEditor->m_SelectedImage; ++i)
			if(!str_comp(pEditor->m_Map.m_vpImages[i]->m_aName, aBuf))
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
	for(const auto &pSound : pEditor->m_Map.m_vpSounds)
	{
		if(!str_comp(pSound->m_aName, aBuf))
			return;
	}

	// load external
	void *pData;
	unsigned DataSize;
	if(!pEditor->Storage()->ReadFile(pFileName, StorageType, &pData, &DataSize))
	{
		dbg_msg("sound/opus", "failed to open file. filename='%s'", pFileName);
		return;
	}

	// load sound
	int SoundId = pEditor->Sound()->LoadOpusFromMem(pData, (unsigned)DataSize, true);
	if(SoundId == -1)
	{
		free(pData);
		return;
	}

	// add sound
	CEditorSound *pSound = new CEditorSound(pEditor);
	pSound->m_SoundID = SoundId;
	pSound->m_DataSize = DataSize;
	pSound->m_pData = pData;
	str_copy(pSound->m_aName, aBuf, sizeof(pSound->m_aName));
	pEditor->m_Map.m_vpSounds.push_back(pSound);

	if(pEditor->m_SelectedSound >= 0 && (size_t)pEditor->m_SelectedSound < pEditor->m_Map.m_vpSounds.size())
	{
		for(int i = 0; i <= pEditor->m_SelectedSound; ++i)
			if(!str_comp(pEditor->m_Map.m_vpSounds[i]->m_aName, aBuf))
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
	void *pData;
	unsigned DataSize;
	if(!pEditor->Storage()->ReadFile(pFileName, StorageType, &pData, &DataSize))
	{
		dbg_msg("sound/opus", "failed to open file. filename='%s'", pFileName);
		return;
	}

	CEditorSound *pSound = pEditor->m_Map.m_vpSounds[pEditor->m_SelectedSound];

	// unload sample
	pEditor->Sound()->UnloadSample(pSound->m_SoundID);
	free(pSound->m_pData);
	pSound->m_pData = nullptr;

	// replace sound
	IStorage::StripPathAndExtension(pFileName, pSound->m_aName, sizeof(pSound->m_aName));
	pSound->m_SoundID = pEditor->Sound()->LoadOpusFromMem(pData, (unsigned)DataSize, true);
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

int CEditor::PopupImage(CEditor *pEditor, CUIRect View, void *pContext)
{
	static int s_ReaddButton = 0;
	static int s_ReplaceButton = 0;
	static int s_RemoveButton = 0;

	CUIRect Slot;
	View.HSplitTop(2.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	CEditorImage *pImg = pEditor->m_Map.m_vpImages[pEditor->m_SelectedImage];

	static int s_ExternalButton = 0;
	if(pImg->m_External)
	{
		if(pEditor->DoButton_MenuItem(&s_ExternalButton, "Embed", 0, &Slot, 0, "Embeds the image into the map file."))
		{
			pImg->m_External = 0;
			return 1;
		}
		View.HSplitTop(5.0f, &Slot, &View);
		View.HSplitTop(12.0f, &Slot, &View);
	}
	else if(IsVanillaImage(pImg->m_aName))
	{
		if(pEditor->DoButton_MenuItem(&s_ExternalButton, "Make external", 0, &Slot, 0, "Removes the image from the map file."))
		{
			pImg->m_External = 1;
			return 1;
		}
		View.HSplitTop(5.0f, &Slot, &View);
		View.HSplitTop(12.0f, &Slot, &View);
	}

	if(pEditor->DoButton_MenuItem(&s_ReaddButton, "Readd", 0, &Slot, 0, "Reloads the image from mapres folder"))
	{
		bool bIsExternal = pImg->m_External;
		char aBuffer[1024];
		str_format(aBuffer, sizeof(aBuffer), "mapres/%s.png", pImg->m_aName);
		pEditor->ReplaceImage(aBuffer, IStorage::TYPE_ALL, pEditor);
		pImg->m_External = bIsExternal;
		return 1;
	}

	View.HSplitTop(5.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_ReplaceButton, "Replace", 0, &Slot, 0, "Replaces the image with a new one"))
	{
		pEditor->InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_IMG, "Replace Image", "Replace", "mapres", "", ReplaceImage, pEditor);
		return 1;
	}

	View.HSplitTop(5.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_RemoveButton, "Remove", 0, &Slot, 0, "Removes the image from the map"))
	{
		delete pImg;
		pEditor->m_Map.m_vpImages.erase(pEditor->m_Map.m_vpImages.begin() + pEditor->m_SelectedImage);
		gs_ModifyIndexDeletedIndex = pEditor->m_SelectedImage;
		pEditor->m_Map.ModifyImageIndex(ModifyIndexDeleted);
		return 1;
	}

	return 0;
}

int CEditor::PopupSound(CEditor *pEditor, CUIRect View, void *pContext)
{
	static int s_ReaddButton = 0;
	static int s_ReplaceButton = 0;
	static int s_RemoveButton = 0;

	CUIRect Slot;
	View.HSplitTop(2.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	CEditorSound *pSound = pEditor->m_Map.m_vpSounds[pEditor->m_SelectedSound];

	if(pEditor->DoButton_MenuItem(&s_ReaddButton, "Readd", 0, &Slot, 0, "Reloads the sound from mapres folder"))
	{
		char aBuffer[1024];
		str_format(aBuffer, sizeof(aBuffer), "mapres/%s.opus", pSound->m_aName);
		pEditor->ReplaceSound(aBuffer, IStorage::TYPE_ALL, pEditor);
		return 1;
	}

	View.HSplitTop(5.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_ReplaceButton, "Replace", 0, &Slot, 0, "Replaces the sound with a new one"))
	{
		pEditor->InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_SOUND, "Replace sound", "Replace", "mapres", "", ReplaceSound, pEditor);
		return 1;
	}

	View.HSplitTop(5.0f, &Slot, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_RemoveButton, "Remove", 0, &Slot, 0, "Removes the sound from the map"))
	{
		delete pSound;
		pEditor->m_Map.m_vpSounds.erase(pEditor->m_Map.m_vpSounds.begin() + pEditor->m_SelectedSound);
		gs_ModifyIndexDeletedIndex = pEditor->m_SelectedSound;
		pEditor->m_Map.ModifySoundIndex(ModifyIndexDeleted);
		return 1;
	}

	return 0;
}

void CEditor::SelectGameLayer()
{
	for(size_t g = 0; g < m_Map.m_vpGroups.size(); g++)
	{
		for(size_t i = 0; i < m_Map.m_vpGroups[g]->m_vpLayers.size(); i++)
		{
			if(m_Map.m_vpGroups[g]->m_vpLayers[i] == m_Map.m_pGameLayer)
			{
				SelectLayer(i, g);
				return;
			}
		}
	}
}

static bool ImageNameLess(const CEditorImage *const &a, const CEditorImage *const &b)
{
	return str_comp(a->m_aName, b->m_aName) < 0;
}

static int *gs_pSortedIndex = nullptr;
static void ModifySortedIndex(int *pIndex)
{
	if(*pIndex >= 0)
		*pIndex = gs_pSortedIndex[*pIndex];
}

void CEditor::SortImages()
{
	if(!std::is_sorted(m_Map.m_vpImages.begin(), m_Map.m_vpImages.end(), ImageNameLess))
	{
		std::vector<CEditorImage *> vpTemp = m_Map.m_vpImages;
		gs_pSortedIndex = new int[vpTemp.size()];

		std::sort(m_Map.m_vpImages.begin(), m_Map.m_vpImages.end(), ImageNameLess);
		for(size_t OldIndex = 0; OldIndex < vpTemp.size(); OldIndex++)
		{
			for(size_t NewIndex = 0; NewIndex < m_Map.m_vpImages.size(); NewIndex++)
			{
				if(vpTemp[OldIndex] == m_Map.m_vpImages[NewIndex])
				{
					gs_pSortedIndex[OldIndex] = NewIndex;
					break;
				}
			}
		}
		m_Map.ModifyImageIndex(ModifySortedIndex);

		delete[] gs_pSortedIndex;
		gs_pSortedIndex = nullptr;
	}
}

void CEditor::RenderImagesList(CUIRect ToolBox)
{
	const float RowHeight = 12.0f;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollbarWidth = 10.0f;
	ScrollParams.m_ScrollbarMargin = 3.0f;
	ScrollParams.m_ScrollUnit = RowHeight * 5;
	s_ScrollRegion.Begin(&ToolBox, &ScrollOffset, &ScrollParams);
	ToolBox.y += ScrollOffset.y;

	bool ScrollToSelection = false;
	if(Input()->KeyPress(KEY_DOWN) && m_Dialog == DIALOG_NONE)
	{
		int OldImage = m_SelectedImage;
		m_SelectedImage = clamp(m_SelectedImage, 0, (int)m_Map.m_vpImages.size() - 1);
		for(size_t i = m_SelectedImage + 1; i < m_Map.m_vpImages.size(); i++)
		{
			if(m_Map.m_vpImages[i]->m_External == m_Map.m_vpImages[m_SelectedImage]->m_External)
			{
				m_SelectedImage = i;
				break;
			}
		}
		if(m_SelectedImage == OldImage && !m_Map.m_vpImages[m_SelectedImage]->m_External)
		{
			for(size_t i = 0; i < m_Map.m_vpImages.size(); i++)
			{
				if(m_Map.m_vpImages[i]->m_External)
				{
					m_SelectedImage = i;
					break;
				}
			}
		}
		ScrollToSelection = OldImage != m_SelectedImage;
	}
	if(Input()->KeyPress(KEY_UP) && m_Dialog == DIALOG_NONE)
	{
		int OldImage = m_SelectedImage;
		m_SelectedImage = clamp(m_SelectedImage, 0, (int)m_Map.m_vpImages.size() - 1);
		for(int i = m_SelectedImage - 1; i >= 0; i--)
		{
			if(m_Map.m_vpImages[i]->m_External == m_Map.m_vpImages[m_SelectedImage]->m_External)
			{
				m_SelectedImage = i;
				break;
			}
		}
		if(m_SelectedImage == OldImage && m_Map.m_vpImages[m_SelectedImage]->m_External)
		{
			for(int i = (int)m_Map.m_vpImages.size() - 1; i >= 0; i--)
			{
				if(!m_Map.m_vpImages[i]->m_External)
				{
					m_SelectedImage = i;
					break;
				}
			}
		}
		ScrollToSelection = OldImage != m_SelectedImage;
	}

	for(int e = 0; e < 2; e++) // two passes, first embedded, then external
	{
		CUIRect Slot;
		ToolBox.HSplitTop(RowHeight + 3.0f, &Slot, &ToolBox);
		if(s_ScrollRegion.AddRect(Slot))
			UI()->DoLabel(&Slot, e == 0 ? "Embedded" : "External", 12.0f, TEXTALIGN_CENTER);

		for(int i = 0; i < (int)m_Map.m_vpImages.size(); i++)
		{
			if((e && !m_Map.m_vpImages[i]->m_External) ||
				(!e && m_Map.m_vpImages[i]->m_External))
			{
				continue;
			}

			ToolBox.HSplitTop(RowHeight + 2.0f, &Slot, &ToolBox);
			int Selected = m_SelectedImage == i;
			if(!s_ScrollRegion.AddRect(Slot, Selected && ScrollToSelection))
				continue;
			Slot.HSplitTop(RowHeight, &Slot, nullptr);

			const bool ImageUsed = std::any_of(m_Map.m_vpGroups.cbegin(), m_Map.m_vpGroups.cend(), [i](const auto &pGroup) {
				return std::any_of(pGroup->m_vpLayers.cbegin(), pGroup->m_vpLayers.cend(), [i](const auto &pLayer) {
					if(pLayer->m_Type == LAYERTYPE_QUADS)
						return static_cast<CLayerQuads *>(pLayer)->m_Image == i;
					else if(pLayer->m_Type == LAYERTYPE_TILES)
						return static_cast<CLayerTiles *>(pLayer)->m_Image == i;
					return false;
				});
			});

			if(!ImageUsed)
				Selected += 2; // Image is unused

			if(Selected < 2 && e == 1)
			{
				if(!IsVanillaImage(m_Map.m_vpImages[i]->m_aName))
				{
					Selected += 4; // Image should be embedded
				}
			}

			float FontSize = 10.0f;
			while(TextRender()->TextWidth(nullptr, FontSize, m_Map.m_vpImages[i]->m_aName, -1, -1.0f) > Slot.w)
				FontSize--;

			if(int Result = DoButton_Ex(&m_Map.m_vpImages[i], m_Map.m_vpImages[i]->m_aName, Selected, &Slot,
				   BUTTON_CONTEXT, "Select image.", IGraphics::CORNER_ALL, FontSize))
			{
				m_SelectedImage = i;

				static int s_PopupImageID = 0;
				if(Result == 2)
				{
					CEditorImage *pImg = m_Map.m_vpImages[m_SelectedImage];
					int Height;
					if(pImg->m_External || IsVanillaImage(pImg->m_aName))
						Height = 73;
					else
						Height = 60;
					UiInvokePopupMenu(&s_PopupImageID, 0, UI()->MouseX(), UI()->MouseY(), 120, Height, PopupImage);
				}
			}
		}

		// separator
		ToolBox.HSplitTop(5.0f, &Slot, &ToolBox);
		if(s_ScrollRegion.AddRect(Slot))
		{
			IGraphics::CLineItem LineItem(Slot.x, Slot.y + Slot.h / 2, Slot.x + Slot.w, Slot.y + Slot.h / 2);
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			Graphics()->LinesDraw(&LineItem, 1);
			Graphics()->LinesEnd();
		}
	}

	// new image
	static int s_AddImageButton = 0;
	CUIRect AddImageButton;
	ToolBox.HSplitTop(5.0f + RowHeight + 1.0f, &AddImageButton, &ToolBox);
	if(s_ScrollRegion.AddRect(AddImageButton))
	{
		AddImageButton.HSplitTop(5.0f, nullptr, &AddImageButton);
		AddImageButton.HSplitTop(RowHeight, &AddImageButton, nullptr);
		if(DoButton_Editor(&s_AddImageButton, "Add", 0, &AddImageButton, 0, "Load a new image to use in the map"))
			InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_IMG, "Add Image", "Add", "mapres", "", AddImage, this);
	}
	s_ScrollRegion.End();
}

void CEditor::RenderSelectedImage(CUIRect View)
{
	if(m_SelectedImage < 0 || (size_t)m_SelectedImage >= m_Map.m_vpImages.size())
		return;

	View.Margin(10.0f, &View);
	if(View.h < View.w)
		View.w = View.h;
	else
		View.h = View.w;
	float Max = maximum<float>(m_Map.m_vpImages[m_SelectedImage]->m_Width, m_Map.m_vpImages[m_SelectedImage]->m_Height);
	View.w *= m_Map.m_vpImages[m_SelectedImage]->m_Width / Max;
	View.h *= m_Map.m_vpImages[m_SelectedImage]->m_Height / Max;
	Graphics()->TextureSet(m_Map.m_vpImages[m_SelectedImage]->m_Texture);
	Graphics()->BlendNormal();
	Graphics()->WrapClamp();
	Graphics()->QuadsBegin();
	IGraphics::CQuadItem QuadItem(View.x, View.y, View.w, View.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
	Graphics()->WrapNormal();
}

void CEditor::RenderSounds(CUIRect ToolBox)
{
	const float RowHeight = 12.0f;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollbarWidth = 10.0f;
	ScrollParams.m_ScrollbarMargin = 3.0f;
	ScrollParams.m_ScrollUnit = RowHeight * 5;
	s_ScrollRegion.Begin(&ToolBox, &ScrollOffset, &ScrollParams);
	ToolBox.y += ScrollOffset.y;

	bool ScrollToSelection = false;
	if(Input()->KeyPress(KEY_DOWN) && m_Dialog == DIALOG_NONE)
	{
		m_SelectedSound = (m_SelectedSound + 1) % m_Map.m_vpSounds.size();
		ScrollToSelection = true;
	}
	if(Input()->KeyPress(KEY_UP) && m_Dialog == DIALOG_NONE)
	{
		m_SelectedSound = (m_SelectedSound + m_Map.m_vpSounds.size() - 1) % m_Map.m_vpSounds.size();
		ScrollToSelection = true;
	}

	CUIRect Slot;
	ToolBox.HSplitTop(RowHeight + 3.0f, &Slot, &ToolBox);
	if(s_ScrollRegion.AddRect(Slot))
		UI()->DoLabel(&Slot, "Embedded", 12.0f, TEXTALIGN_CENTER);

	for(int i = 0; i < (int)m_Map.m_vpSounds.size(); i++)
	{
		ToolBox.HSplitTop(RowHeight + 2.0f, &Slot, &ToolBox);
		int Selected = m_SelectedSound == i;
		if(!s_ScrollRegion.AddRect(Slot, Selected && ScrollToSelection))
			continue;
		Slot.HSplitTop(RowHeight, &Slot, nullptr);

		const bool SoundUsed = std::any_of(m_Map.m_vpGroups.cbegin(), m_Map.m_vpGroups.cend(), [i](const auto &pGroup) {
			return std::any_of(pGroup->m_vpLayers.cbegin(), pGroup->m_vpLayers.cend(), [i](const auto &pLayer) {
				if(pLayer->m_Type == LAYERTYPE_SOUNDS)
					return static_cast<CLayerSounds *>(pLayer)->m_Sound == i;
				return false;
			});
		});

		if(!SoundUsed)
			Selected += 2; // Sound is unused

		float FontSize = 10.0f;
		while(TextRender()->TextWidth(nullptr, FontSize, m_Map.m_vpSounds[i]->m_aName, -1, -1.0f) > Slot.w)
			FontSize--;

		if(int Result = DoButton_Ex(&m_Map.m_vpSounds[i], m_Map.m_vpSounds[i]->m_aName, Selected, &Slot,
			   BUTTON_CONTEXT, "Select sound.", IGraphics::CORNER_ALL, FontSize))
		{
			m_SelectedSound = i;

			static int s_PopupSoundID = 0;
			if(Result == 2)
				UiInvokePopupMenu(&s_PopupSoundID, 0, UI()->MouseX(), UI()->MouseY(), 120, 43, PopupSound);
		}
	}

	// separator
	ToolBox.HSplitTop(5.0f, &Slot, &ToolBox);
	if(s_ScrollRegion.AddRect(Slot))
	{
		IGraphics::CLineItem LineItem(Slot.x, Slot.y + Slot.h / 2, Slot.x + Slot.w, Slot.y + Slot.h / 2);
		Graphics()->TextureClear();
		Graphics()->LinesBegin();
		Graphics()->LinesDraw(&LineItem, 1);
		Graphics()->LinesEnd();
	}

	// new sound
	static int s_AddSoundButton = 0;
	CUIRect AddSoundButton;
	ToolBox.HSplitTop(5.0f + RowHeight + 1.0f, &AddSoundButton, &ToolBox);
	if(s_ScrollRegion.AddRect(AddSoundButton))
	{
		AddSoundButton.HSplitTop(5.0f, nullptr, &AddSoundButton);
		AddSoundButton.HSplitTop(RowHeight, &AddSoundButton, nullptr);
		if(DoButton_Editor(&s_AddSoundButton, "Add", 0, &AddSoundButton, 0, "Load a new sound to use in the map"))
			InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_SOUND, "Add Sound", "Add", "mapres", "", AddSound, this);
	}
	s_ScrollRegion.End();
}

static int EditorListdirCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;
	if((pName[0] == '.' && (pName[1] == 0 ||
				       (pName[1] == '.' && pName[2] == 0 && (!str_comp(pEditor->m_pFileDialogPath, "maps") || !str_comp(pEditor->m_pFileDialogPath, "mapres"))))) ||
		(!IsDir && ((pEditor->m_FileDialogFileType == CEditor::FILETYPE_MAP && !str_endswith(pName, ".map")) ||
				   (pEditor->m_FileDialogFileType == CEditor::FILETYPE_IMG && !str_endswith(pName, ".png")) ||
				   (pEditor->m_FileDialogFileType == CEditor::FILETYPE_SOUND && !str_endswith(pName, ".opus")))))
		return 0;

	CEditor::CFilelistItem Item;
	str_copy(Item.m_aFilename, pName, sizeof(Item.m_aFilename));
	if(IsDir)
		str_format(Item.m_aName, sizeof(Item.m_aName), "%s/", pName);
	else
	{
		int LenEnding = pEditor->m_FileDialogFileType == CEditor::FILETYPE_SOUND ? 5 : 4;
		str_truncate(Item.m_aName, sizeof(Item.m_aName), pName, str_length(pName) - LenEnding);
	}
	Item.m_IsDir = IsDir != 0;
	Item.m_IsLink = false;
	Item.m_StorageType = StorageType;
	pEditor->m_vFileList.push_back(Item);

	return 0;
}

void CEditor::AddFileDialogEntry(int Index, CUIRect *pView)
{
	m_FilesCur++;
	if(m_FilesCur - 1 < m_FilesStartAt || m_FilesCur >= m_FilesStopAt)
		return;

	CUIRect Button, FileIcon;
	pView->HSplitTop(15.0f, &Button, pView);
	pView->HSplitTop(2.0f, nullptr, pView);
	Button.VSplitLeft(Button.h, &FileIcon, &Button);
	Button.VSplitLeft(5.0f, nullptr, &Button);

	const char *pIconType;

	if(!m_vFileList[Index].m_IsDir)
	{
		switch(m_FileDialogFileType)
		{
		case FILETYPE_MAP:
			pIconType = "\xEF\x89\xB9";
			break;
		case FILETYPE_IMG:
			pIconType = "\xEF\x80\xBE";
			break;
		case FILETYPE_SOUND:
			pIconType = "\xEF\x80\x81";
			break;
		default:
			pIconType = "";
		}
	}
	else
	{
		if(str_comp(m_vFileList[Index].m_aFilename, "..") == 0)
			pIconType = "\xEF\xA0\x82";
		else
			pIconType = "\xEF\x81\xBB";
	}

	TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
	UI()->DoLabel(&FileIcon, pIconType, 12.0f, TEXTALIGN_LEFT);
	TextRender()->SetCurFont(nullptr);

	if(DoButton_File(&m_vFileList[Index], m_vFileList[Index].m_aName, m_FilesSelectedIndex == Index, &Button, 0, nullptr))
	{
		if(!m_vFileList[Index].m_IsDir)
			str_copy(m_aFileDialogFileName, m_vFileList[Index].m_aFilename, sizeof(m_aFileDialogFileName));
		else
			m_aFileDialogFileName[0] = 0;
		m_PreviewImageIsLoaded = false;
		m_FileDialogActivate |= Index == m_FilesSelectedIndex && Input()->MouseDoubleClick();
		m_FilesSelectedIndex = Index;
	}
}

void CEditor::RenderFileDialog()
{
	// GUI coordsys
	UI()->MapScreen();
	CUIRect View = *UI()->Screen();
	CUIRect Preview;
	float Width = View.w, Height = View.h;

	View.Draw(ColorRGBA(0, 0, 0, 0.25f), 0, 0);
	View.VMargin(150.0f, &View);
	View.HMargin(50.0f, &View);
	View.Draw(ColorRGBA(0, 0, 0, 0.75f), IGraphics::CORNER_ALL, 5.0f);
	View.Margin(10.0f, &View);

	CUIRect Title, FileBox, FileBoxLabel, ButtonBar, Scroll, PathBox;
	View.HSplitTop(18.0f, &Title, &View);
	View.HSplitTop(5.0f, nullptr, &View); // some spacing
	View.HSplitBottom(14.0f, &View, &ButtonBar);
	View.HSplitBottom(10.0f, &View, nullptr); // some spacing
	View.HSplitBottom(14.0f, &View, &PathBox);
	View.HSplitBottom(5.0f, &View, nullptr); // some spacing
	View.HSplitBottom(14.0f, &View, &FileBox);
	FileBox.VSplitLeft(55.0f, &FileBoxLabel, &FileBox);
	View.HSplitBottom(10.0f, &View, nullptr); // some spacing
	if(m_FileDialogFileType == CEditor::FILETYPE_IMG)
		View.VSplitMid(&View, &Preview);
	View.VSplitRight(20.0f, &View, &Scroll);

	// title
	Title.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 4.0f);
	Title.VMargin(10.0f, &Title);
	UI()->DoLabel(&Title, m_pFileDialogTitle, 12.0f, TEXTALIGN_LEFT);

	// pathbox
	char aPath[128], aBuf[128];
	if(m_FilesSelectedIndex != -1)
		Storage()->GetCompletePath(m_vFileList[m_FilesSelectedIndex].m_StorageType, m_pFileDialogPath, aPath, sizeof(aPath));
	else
		aPath[0] = 0;
	str_format(aBuf, sizeof(aBuf), "Current path: %s", aPath);
	UI()->DoLabel(&PathBox, aBuf, 10.0f, TEXTALIGN_LEFT);

	if(m_FileDialogStorageType == IStorage::TYPE_SAVE)
	{
		// filebox
		UI()->DoLabel(&FileBoxLabel, "Filename:", 10.0f, TEXTALIGN_LEFT);
		static float s_FileBoxID = 0;
		if(DoEditBox(&s_FileBoxID, &FileBox, m_aFileDialogFileName, sizeof(m_aFileDialogFileName), 10.0f, &s_FileBoxID))
		{
			// remove '/' and '\'
			for(int i = 0; m_aFileDialogFileName[i]; ++i)
				if(m_aFileDialogFileName[i] == '/' || m_aFileDialogFileName[i] == '\\')
					str_copy(&m_aFileDialogFileName[i], &m_aFileDialogFileName[i + 1], (int)(sizeof(m_aFileDialogFileName)) - i);
			m_FilesSelectedIndex = -1;
		}

		if(m_FileDialogOpening)
			UI()->SetActiveItem(&s_FileBoxID);
	}
	else
	{
		//searchbox
		FileBox.VSplitRight(250, &FileBox, nullptr);
		CUIRect ClearBox;
		FileBox.VSplitRight(15, &FileBox, &ClearBox);

		UI()->DoLabel(&FileBoxLabel, "Search:", 10.0f, TEXTALIGN_LEFT);
		str_copy(m_aFileDialogPrevSearchText, m_aFileDialogSearchText, sizeof(m_aFileDialogPrevSearchText));
		static float s_SearchBoxID = 0;
		DoEditBox(&s_SearchBoxID, &FileBox, m_aFileDialogSearchText, sizeof(m_aFileDialogSearchText), 10.0f, &s_SearchBoxID, false, IGraphics::CORNER_L);
		if(m_FileDialogOpening)
			UI()->SetActiveItem(&s_SearchBoxID);

		// clearSearchbox button
		{
			static int s_ClearButton = 0;
			ClearBox.Draw(ColorRGBA(1, 1, 1, 0.33f * UI()->ButtonColorMul(&s_ClearButton)), IGraphics::CORNER_R, 3.0f);
			UI()->DoLabel(&ClearBox, "×", 10.0f, TEXTALIGN_CENTER);
			if(UI()->DoButtonLogic(&s_ClearButton, 0, &ClearBox))
			{
				m_aFileDialogSearchText[0] = 0;
				UI()->SetActiveItem(&s_SearchBoxID);
			}
		}

		if(str_comp(m_aFileDialogPrevSearchText, m_aFileDialogSearchText))
			m_FileDialogScrollValue = 0.0f;
	}

	m_FileDialogOpening = false;

	int Num = (int)(View.h / 17.0f) + 1;
	m_FileDialogScrollValue = UI()->DoScrollbarV(&m_FileDialogScrollValue, &Scroll, m_FileDialogScrollValue);

	int ScrollNum = 0;
	for(size_t i = 0; i < m_vFileList.size(); i++)
	{
		m_vFileList[i].m_IsVisible = false;
		if(!m_aFileDialogSearchText[0] || str_utf8_find_nocase(m_vFileList[i].m_aName, m_aFileDialogSearchText))
		{
			AddFileDialogEntry(i, &View);
			m_vFileList[i].m_IsVisible = true;
			ScrollNum++;
		}
	}
	ScrollNum -= Num - 1;

	if(ScrollNum > 0)
	{
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
			m_FileDialogScrollValue -= 3.0f / ScrollNum;
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
			m_FileDialogScrollValue += 3.0f / ScrollNum;
	}
	else
		ScrollNum = 0;

	if(m_FilesSelectedIndex > -1)
	{
		if(!m_vFileList[m_FilesSelectedIndex].m_IsVisible)
		{
			m_FilesSelectedIndex = 0;
		}

		for(int i = 0; i < Input()->NumEvents(); i++)
		{
			int NewIndex = -1;
			if(Input()->GetEvent(i).m_Flags & IInput::FLAG_PRESS)
			{
				if(Input()->GetEvent(i).m_Key == KEY_DOWN)
				{
					for(NewIndex = m_FilesSelectedIndex + 1; NewIndex < (int)m_vFileList.size(); NewIndex++)
					{
						if(m_vFileList[NewIndex].m_IsVisible)
							break;
					}
				}
				if(Input()->GetEvent(i).m_Key == KEY_UP)
				{
					for(NewIndex = m_FilesSelectedIndex - 1; NewIndex >= 0; NewIndex--)
					{
						if(m_vFileList[NewIndex].m_IsVisible)
							break;
					}
				}
			}
			if(NewIndex > -1 && NewIndex < (int)m_vFileList.size())
			{
				//scroll
				float IndexY = View.y - m_FileDialogScrollValue * ScrollNum * 17.0f + NewIndex * 17.0f;
				int ScrollPos = View.y > IndexY ? -1 : View.y + View.h < IndexY + 17.0f ? 1 : 0;
				if(ScrollPos)
				{
					if(ScrollPos < 0)
						m_FileDialogScrollValue = ((float)(NewIndex) + 0.5f) / ScrollNum;
					else
						m_FileDialogScrollValue = ((float)(NewIndex - Num) + 2.5f) / ScrollNum;
				}

				if(!m_vFileList[NewIndex].m_IsDir)
					str_copy(m_aFileDialogFileName, m_vFileList[NewIndex].m_aFilename, sizeof(m_aFileDialogFileName));
				else
					m_aFileDialogFileName[0] = 0;
				m_FilesSelectedIndex = NewIndex;
				m_PreviewImageIsLoaded = false;
			}
		}

		if(m_FileDialogFileType == CEditor::FILETYPE_IMG && !m_PreviewImageIsLoaded && m_FilesSelectedIndex > -1)
		{
			if(str_endswith(m_vFileList[m_FilesSelectedIndex].m_aFilename, ".png"))
			{
				char aBuffer[1024];
				str_format(aBuffer, sizeof(aBuffer), "%s/%s", m_pFileDialogPath, m_vFileList[m_FilesSelectedIndex].m_aFilename);

				if(Graphics()->LoadPNG(&m_FilePreviewImageInfo, aBuffer, IStorage::TYPE_ALL))
				{
					m_FilePreviewImage = Graphics()->LoadTextureRaw(m_FilePreviewImageInfo.m_Width, m_FilePreviewImageInfo.m_Height, m_FilePreviewImageInfo.m_Format, m_FilePreviewImageInfo.m_pData, m_FilePreviewImageInfo.m_Format, 0);
					CImageInfo DummyInfo = m_FilePreviewImageInfo;
					m_FilePreviewImageInfo.m_pData = nullptr;
					Graphics()->FreePNG(&DummyInfo);
					m_PreviewImageIsLoaded = true;
				}
			}
		}
		if(m_FileDialogFileType == CEditor::FILETYPE_IMG && m_PreviewImageIsLoaded)
		{
			int w = m_FilePreviewImageInfo.m_Width;
			int h = m_FilePreviewImageInfo.m_Height;
			if(m_FilePreviewImageInfo.m_Width > Preview.w) // NOLINT(clang-analyzer-core.UndefinedBinaryOperatorResult)
			{
				h = m_FilePreviewImageInfo.m_Height * Preview.w / m_FilePreviewImageInfo.m_Width;
				w = Preview.w;
			}
			if(h > Preview.h)
			{
				w = w * Preview.h / h;
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
		if(Input()->GetEvent(i).m_Flags & IInput::FLAG_PRESS && !UiPopupOpen() && !m_PopupEventActivated)
		{
			if(Input()->GetEvent(i).m_Key == KEY_RETURN || Input()->GetEvent(i).m_Key == KEY_KP_ENTER)
				m_FileDialogActivate = true;
		}
	}

	if(m_FileDialogScrollValue < 0)
		m_FileDialogScrollValue = 0;
	if(m_FileDialogScrollValue > 1)
		m_FileDialogScrollValue = 1;

	m_FilesStartAt = (int)(ScrollNum * m_FileDialogScrollValue);
	if(m_FilesStartAt < 0)
		m_FilesStartAt = 0;

	m_FilesStopAt = m_FilesStartAt + Num;

	m_FilesCur = 0;

	// set clipping
	UI()->ClipEnable(&View);

	// disable clipping again
	UI()->ClipDisable();

	// the buttons
	static int s_OkButton = 0;
	static int s_CancelButton = 0;
	static int s_NewFolderButton = 0;
	static int s_MapInfoButton = 0;

	CUIRect Button;
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	bool IsDir = m_FilesSelectedIndex >= 0 && m_vFileList[m_FilesSelectedIndex].m_IsDir;
	if(DoButton_Editor(&s_OkButton, IsDir ? "Open" : m_pFileDialogButtonText, 0, &Button, 0, nullptr) || m_FileDialogActivate)
	{
		m_FileDialogActivate = false;
		if(IsDir) // folder
		{
			if(str_comp(m_vFileList[m_FilesSelectedIndex].m_aFilename, "..") == 0) // parent folder
			{
				if(fs_parent_dir(m_pFileDialogPath))
					m_pFileDialogPath = m_aFileDialogCurrentFolder; // leave the link
			}
			else // sub folder
			{
				if(m_vFileList[m_FilesSelectedIndex].m_IsLink)
				{
					m_pFileDialogPath = m_aFileDialogCurrentLink; // follow the link
					str_copy(m_aFileDialogCurrentLink, m_vFileList[m_FilesSelectedIndex].m_aFilename, sizeof(m_aFileDialogCurrentLink));
				}
				else
				{
					char aTemp[IO_MAX_PATH_LENGTH];
					str_copy(aTemp, m_pFileDialogPath, sizeof(aTemp));
					str_format(m_pFileDialogPath, IO_MAX_PATH_LENGTH, "%s/%s", aTemp, m_vFileList[m_FilesSelectedIndex].m_aFilename);
				}
			}
			FilelistPopulate(!str_comp(m_pFileDialogPath, "maps") || !str_comp(m_pFileDialogPath, "mapres") ? m_FileDialogStorageType :
															  m_vFileList[m_FilesSelectedIndex].m_StorageType);
			if(m_FilesSelectedIndex >= 0 && !m_vFileList[m_FilesSelectedIndex].m_IsDir)
				str_copy(m_aFileDialogFileName, m_vFileList[m_FilesSelectedIndex].m_aFilename, sizeof(m_aFileDialogFileName));
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
				else if(m_pfnFileDialogFunc)
					m_pfnFileDialogFunc(m_aFileSaveName, m_FilesSelectedIndex >= 0 ? m_vFileList[m_FilesSelectedIndex].m_StorageType : m_FileDialogStorageType, m_pFileDialogUser);
			}
			else if(m_pfnFileDialogFunc)
				m_pfnFileDialogFunc(m_aFileSaveName, m_FilesSelectedIndex >= 0 ? m_vFileList[m_FilesSelectedIndex].m_StorageType : m_FileDialogStorageType, m_pFileDialogUser);
		}
	}

	ButtonBar.VSplitRight(40.0f, &ButtonBar, &Button);
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	if(DoButton_Editor(&s_CancelButton, "Cancel", 0, &Button, 0, nullptr) || Input()->KeyIsPressed(KEY_ESCAPE))
		m_Dialog = DIALOG_NONE;

	if(m_FileDialogStorageType == IStorage::TYPE_SAVE)
	{
		ButtonBar.VSplitLeft(70.0f, &Button, &ButtonBar);
		if(DoButton_Editor(&s_NewFolderButton, "New folder", 0, &Button, 0, nullptr))
		{
			m_aFileDialogNewFolderName[0] = 0;
			m_aFileDialogErrString[0] = 0;
			static int s_NewFolderPopupID = 0;
			UiInvokePopupMenu(&s_NewFolderPopupID, 0, Width / 2.0f - 200.0f, Height / 2.0f - 100.0f, 400.0f, 200.0f, PopupNewFolder);
			UI()->SetActiveItem(nullptr);
		}
	}

	if(m_FileDialogStorageType == IStorage::TYPE_SAVE)
	{
		ButtonBar.VSplitLeft(40.0f, nullptr, &ButtonBar);
		ButtonBar.VSplitLeft(70.0f, &Button, &ButtonBar);
		if(DoButton_Editor(&s_MapInfoButton, "Map details", 0, &Button, 0, nullptr))
		{
			str_copy(m_Map.m_MapInfo.m_aAuthorTmp, m_Map.m_MapInfo.m_aAuthor, sizeof(m_Map.m_MapInfo.m_aAuthorTmp));
			str_copy(m_Map.m_MapInfo.m_aVersionTmp, m_Map.m_MapInfo.m_aVersion, sizeof(m_Map.m_MapInfo.m_aVersionTmp));
			str_copy(m_Map.m_MapInfo.m_aCreditsTmp, m_Map.m_MapInfo.m_aCredits, sizeof(m_Map.m_MapInfo.m_aCreditsTmp));
			str_copy(m_Map.m_MapInfo.m_aLicenseTmp, m_Map.m_MapInfo.m_aLicense, sizeof(m_Map.m_MapInfo.m_aLicenseTmp));
			static int s_MapInfoPopupID = 0;
			UiInvokePopupMenu(&s_MapInfoPopupID, 0, Width / 2.0f - 200.0f, Height / 2.0f - 100.0f, 400.0f, 200.0f, PopupMapInfo);
			UI()->SetActiveItem(nullptr);
		}
	}
}

void CEditor::FilelistPopulate(int StorageType)
{
	m_vFileList.clear();
	if(m_FileDialogStorageType != IStorage::TYPE_SAVE && !str_comp(m_pFileDialogPath, "maps"))
	{
		CFilelistItem Item;
		str_copy(Item.m_aFilename, "downloadedmaps", sizeof(Item.m_aFilename));
		str_copy(Item.m_aName, "downloadedmaps/", sizeof(Item.m_aName));
		Item.m_IsDir = true;
		Item.m_IsLink = true;
		Item.m_StorageType = IStorage::TYPE_SAVE;
		m_vFileList.push_back(Item);
	}
	Storage()->ListDirectory(StorageType, m_pFileDialogPath, EditorListdirCallback, this);
	std::sort(m_vFileList.begin(), m_vFileList.end());
	m_FilesSelectedIndex = m_vFileList.empty() ? -1 : 0;
	m_PreviewImageIsLoaded = false;
	m_FileDialogActivate = false;

	if(m_FilesSelectedIndex >= 0 && !m_vFileList[m_FilesSelectedIndex].m_IsDir)
		str_copy(m_aFileDialogFileName, m_vFileList[m_FilesSelectedIndex].m_aFilename, sizeof(m_aFileDialogFileName));
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
	m_PreviewImageIsLoaded = false;
	m_FileDialogOpening = true;

	if(pDefaultName)
		str_copy(m_aFileDialogFileName, pDefaultName, sizeof(m_aFileDialogFileName));
	if(pBasePath)
		str_copy(m_aFileDialogCurrentFolder, pBasePath, sizeof(m_aFileDialogCurrentFolder));

	FilelistPopulate(m_FileDialogStorageType);

	m_FileDialogOpening = true;
	m_Dialog = DIALOG_FILE;
}

void CEditor::RenderModebar(CUIRect View)
{
	CUIRect Button;

	// mode buttons
	{
		View.VSplitLeft(65.0f, &Button, &View);
		Button.HSplitTop(30.0f, nullptr, &Button);
		static int s_Button = 0;
		const char *pButName = "";

		if(m_Mode == MODE_LAYERS)
			pButName = "Layers";
		else if(m_Mode == MODE_IMAGES)
			pButName = "Images";
		else if(m_Mode == MODE_SOUNDS)
			pButName = "Sounds";

		int MouseButton = DoButton_Tab(&s_Button, pButName, 0, &Button, 0, "Switch between images, sounds and layers management.");
		if(MouseButton == 2 || (Input()->KeyPress(KEY_LEFT) && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0))
		{
			if(m_Mode == MODE_LAYERS)
				m_Mode = MODE_SOUNDS;
			else if(m_Mode == MODE_IMAGES)
				m_Mode = MODE_LAYERS;
			else
				m_Mode = MODE_IMAGES;
		}
		else if(MouseButton == 1 || (Input()->KeyPress(KEY_RIGHT) && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0))
		{
			if(m_Mode == MODE_LAYERS)
				m_Mode = MODE_IMAGES;
			else if(m_Mode == MODE_IMAGES)
				m_Mode = MODE_SOUNDS;
			else
				m_Mode = MODE_LAYERS;
		}
	}

	View.VSplitLeft(5.0f, nullptr, &View);
}

void CEditor::RenderStatusbar(CUIRect View)
{
	CUIRect Button;
	View.VSplitRight(60.0f, &View, &Button);
	static int s_EnvelopeButton = 0;
	int MouseButton = DoButton_Editor(&s_EnvelopeButton, "Envelopes", m_ShowEnvelopeEditor, &Button, 0, "Toggles the envelope editor.");
	if(MouseButton == 2)
		m_ShowEnvelopeEditor = (m_ShowEnvelopeEditor + 3) % 4;
	else if(MouseButton == 1)
		m_ShowEnvelopeEditor = (m_ShowEnvelopeEditor + 1) % 4;

	if(MouseButton)
	{
		m_ShowServerSettingsEditor = false;
	}

	View.VSplitRight(100.0f, &View, &Button);
	Button.VSplitRight(10.0f, &Button, nullptr);
	static int s_SettingsButton = 0;
	if(DoButton_Editor(&s_SettingsButton, "Server settings", m_ShowServerSettingsEditor, &Button, 0, "Toggles the server settings editor."))
	{
		m_ShowEnvelopeEditor = 0;
		m_ShowServerSettingsEditor ^= 1;
	}

	if(m_pTooltip)
	{
		char aBuf[512];
		if(ms_pUiGotContext && ms_pUiGotContext == UI()->HotItem())
			str_format(aBuf, sizeof(aBuf), "%s Right click for context menu.", m_pTooltip);
		else
			str_copy(aBuf, m_pTooltip, sizeof(aBuf));

		float FontSize = ScaleFontSize(aBuf, sizeof(aBuf), 10.0f, View.w);
		SLabelProperties Props;
		Props.m_MaxWidth = View.w;
		UI()->DoLabel(&View, aBuf, FontSize, TEXTALIGN_LEFT, Props);
	}
}

bool CEditor::IsEnvelopeUsed(int EnvelopeIndex) const
{
	for(const auto &pGroup : m_Map.m_vpGroups)
	{
		for(const auto &pLayer : pGroup->m_vpLayers)
		{
			if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				CLayerQuads *pLayerQuads = (CLayerQuads *)pLayer;
				for(const auto &Quad : pLayerQuads->m_vQuads)
				{
					if(Quad.m_PosEnv == EnvelopeIndex || Quad.m_ColorEnv == EnvelopeIndex)
					{
						return true;
					}
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				CLayerSounds *pLayerSounds = (CLayerSounds *)pLayer;
				for(const auto &Source : pLayerSounds->m_vSources)
				{
					if(Source.m_PosEnv == EnvelopeIndex || Source.m_SoundEnv == EnvelopeIndex)
					{
						return true;
					}
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CLayerTiles *pLayerTiles = (CLayerTiles *)pLayer;
				if(pLayerTiles->m_ColorEnv == EnvelopeIndex)
					return true;
			}
		}
	}
	return false;
}

void CEditor::RenderEnvelopeEditor(CUIRect View)
{
	if(m_SelectedEnvelope < 0)
		m_SelectedEnvelope = 0;
	if(m_SelectedEnvelope >= (int)m_Map.m_vpEnvelopes.size())
		m_SelectedEnvelope = m_Map.m_vpEnvelopes.size() - 1;

	CEnvelope *pEnvelope = nullptr;
	if(m_SelectedEnvelope >= 0 && m_SelectedEnvelope < (int)m_Map.m_vpEnvelopes.size())
		pEnvelope = m_Map.m_vpEnvelopes[m_SelectedEnvelope];

	CUIRect ToolBar, CurveBar, ColorBar;
	View.HSplitTop(15.0f, &ToolBar, &View);
	View.HSplitTop(15.0f, &CurveBar, &View);
	ToolBar.Margin(2.0f, &ToolBar);
	CurveBar.Margin(2.0f, &CurveBar);

	bool CurrentEnvelopeSwitched = false;

	// do the toolbar
	{
		CUIRect Button;
		CEnvelope *pNewEnv = nullptr;

		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_NewSoundButton = 0;
		if(DoButton_Editor(&s_NewSoundButton, "Sound+", 0, &Button, 0, "Creates a new sound envelope"))
		{
			m_Map.m_Modified = true;
			pNewEnv = m_Map.NewEnvelope(1);
		}

		ToolBar.VSplitRight(5.0f, &ToolBar, &Button);
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_New4dButton = 0;
		if(DoButton_Editor(&s_New4dButton, "Color+", 0, &Button, 0, "Creates a new color envelope"))
		{
			m_Map.m_Modified = true;
			pNewEnv = m_Map.NewEnvelope(4);
		}

		ToolBar.VSplitRight(5.0f, &ToolBar, &Button);
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_New2dButton = 0;
		if(DoButton_Editor(&s_New2dButton, "Pos.+", 0, &Button, 0, "Creates a new position envelope"))
		{
			m_Map.m_Modified = true;
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
				m_Map.DeleteEnvelope(m_SelectedEnvelope);
				if(m_SelectedEnvelope >= (int)m_Map.m_vpEnvelopes.size())
					m_SelectedEnvelope = m_Map.m_vpEnvelopes.size() - 1;
				pEnvelope = m_SelectedEnvelope >= 0 ? m_Map.m_vpEnvelopes[m_SelectedEnvelope] : nullptr;
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
		str_format(aBuf, sizeof(aBuf), "%d/%d", m_SelectedEnvelope + 1, (int)m_Map.m_vpEnvelopes.size());

		ColorRGBA EnvColor = ColorRGBA(1, 1, 1, 0.5f);
		if(!m_Map.m_vpEnvelopes.empty())
		{
			EnvColor = IsEnvelopeUsed(m_SelectedEnvelope) ?
					   ColorRGBA(1, 0.7f, 0.7f, 0.5f) :
					   ColorRGBA(0.7f, 1, 0.7f, 0.5f);
		}

		Shifter.Draw(EnvColor, 0, 0.0f);
		UI()->DoLabel(&Shifter, aBuf, 10.0f, TEXTALIGN_CENTER);

		static int s_PrevButton = 0;
		if(DoButton_ButtonDec(&s_PrevButton, nullptr, 0, &Dec, 0, "Previous Envelope"))
		{
			m_SelectedEnvelope--;
			if(m_SelectedEnvelope < 0)
				m_SelectedEnvelope = m_Map.m_vpEnvelopes.size() - 1;
			CurrentEnvelopeSwitched = true;
		}

		static int s_NextButton = 0;
		if(DoButton_ButtonInc(&s_NextButton, nullptr, 0, &Inc, 0, "Next Envelope"))
		{
			m_SelectedEnvelope++;
			if(m_SelectedEnvelope >= (int)m_Map.m_vpEnvelopes.size())
				m_SelectedEnvelope = 0;
			CurrentEnvelopeSwitched = true;
		}

		if(pEnvelope)
		{
			ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);
			ToolBar.VSplitLeft(35.0f, &Button, &ToolBar);
			UI()->DoLabel(&Button, "Name:", 10.0f, TEXTALIGN_LEFT);

			ToolBar.VSplitLeft(80.0f, &Button, &ToolBar);

			static float s_NameBox = 0;
			if(DoEditBox(&s_NameBox, &Button, pEnvelope->m_aName, sizeof(pEnvelope->m_aName), 10.0f, &s_NameBox))
			{
				m_Map.m_Modified = true;
			}
		}
	}

	bool ShowColorBar = false;
	if(pEnvelope && pEnvelope->m_Channels == 4)
	{
		ShowColorBar = true;
		View.HSplitTop(20.0f, &ColorBar, &View);
		ColorBar.Margin(2.0f, &ColorBar);
		RenderBackground(ColorBar, m_CheckerTexture, 16.0f, 1.0f);
	}

	RenderBackground(View, m_CheckerTexture, 32.0f, 0.1f);

	if(pEnvelope)
	{
		static std::vector<int> s_vSelection;
		static int s_EnvelopeEditorID = 0;
		static int s_ActiveChannels = 0xf;

		ColorRGBA aColors[] = {ColorRGBA(1, 0.2f, 0.2f), ColorRGBA(0.2f, 1, 0.2f), ColorRGBA(0.2f, 0.2f, 1), ColorRGBA(1, 1, 0.2f)};

		if(pEnvelope)
		{
			CUIRect Button;

			ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);

			static const char *s_aapNames[4][4] = {
				{"V", "", "", ""},
				{"", "", "", ""},
				{"X", "Y", "R", ""},
				{"R", "G", "B", "A"},
			};

			static const char *s_aapDescriptions[4][4] = {
				{"Volume of the envelope", "", "", ""},
				{"", "", "", ""},
				{"X-axis of the envelope", "Y-axis of the envelope", "Rotation of the envelope", ""},
				{"Red value of the envelope", "Green value of the envelope", "Blue value of the envelope", "Alpha value of the envelope"},
			};

			static int s_aChannelButtons[4] = {0};
			int Bit = 1;

			for(int i = 0; i < pEnvelope->m_Channels; i++, Bit <<= 1)
			{
				ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);

				if(DoButton_Env(&s_aChannelButtons[i], s_aapNames[pEnvelope->m_Channels - 1][i], s_ActiveChannels & Bit, &Button, s_aapDescriptions[pEnvelope->m_Channels - 1][i], aColors[i]))
					s_ActiveChannels ^= Bit;
			}

			// sync checkbox
			ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);
			ToolBar.VSplitLeft(12.0f, &Button, &ToolBar);
			static int s_SyncButton;
			if(DoButton_Editor(&s_SyncButton, pEnvelope->m_Synchronized ? "X" : "", 0, &Button, 0, "Synchronize envelope animation to game time (restarts when you touch the start line)"))
				pEnvelope->m_Synchronized = !pEnvelope->m_Synchronized;

			ToolBar.VSplitLeft(4.0f, &Button, &ToolBar);
			ToolBar.VSplitLeft(80.0f, &Button, &ToolBar);
			UI()->DoLabel(&Button, "Synchronized", 10.0f, TEXTALIGN_LEFT);
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

		float TimeScale = EndTime / View.w;
		float ValueScale = (Top - Bottom) / View.h;

		if(UI()->MouseInside(&View))
			UI()->SetHotItem(&s_EnvelopeEditorID);

		if(UI()->HotItem() == &s_EnvelopeEditorID)
		{
			// do stuff
			if(pEnvelope)
			{
				if(UI()->MouseButtonClicked(1))
				{
					// add point
					int Time = (int)(((UI()->MouseX() - View.x) * TimeScale) * 1000.0f);
					//float env_y = (UI()->MouseY()-view.y)/TimeScale;
					ColorRGBA Channels;
					pEnvelope->Eval(Time / 1000.0f, Channels);
					pEnvelope->AddPoint(Time,
						f2fx(Channels.r), f2fx(Channels.g),
						f2fx(Channels.b), f2fx(Channels.a));
					m_Map.m_Modified = true;
				}

				m_ShowEnvelopePreview = SHOWENV_SELECTED;
				m_pTooltip = "Press right mouse button to create a new point";
			}
		}

		// render lines
		{
			UI()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			for(int c = 0; c < pEnvelope->m_Channels; c++)
			{
				if(s_ActiveChannels & (1 << c))
					Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 1);
				else
					Graphics()->SetColor(aColors[c].r * 0.5f, aColors[c].g * 0.5f, aColors[c].b * 0.5f, 1);

				float PrevX = 0;
				ColorRGBA Channels;
				pEnvelope->Eval(0.000001f, Channels);
				float PrevValue = Channels[c];

				int Steps = (int)((View.w / UI()->Screen()->w) * Graphics()->ScreenWidth());
				for(int i = 1; i <= Steps; i++)
				{
					float a = i / (float)Steps;
					pEnvelope->Eval(a * EndTime, Channels);
					float v = Channels[c];
					v = (v - Bottom) / (Top - Bottom);

					IGraphics::CLineItem LineItem(View.x + PrevX * View.w, View.y + View.h - PrevValue * View.h, View.x + a * View.w, View.y + View.h - v * View.h);
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
			for(int i = 0; i < (int)pEnvelope->m_vPoints.size() - 1; i++)
			{
				float t0 = pEnvelope->m_vPoints[i].m_Time / 1000.0f / EndTime;
				float t1 = pEnvelope->m_vPoints[i + 1].m_Time / 1000.0f / EndTime;

				//dbg_msg("", "%f", end_time);

				CUIRect v;
				v.x = CurveBar.x + (t0 + (t1 - t0) * 0.5f) * CurveBar.w;
				v.y = CurveBar.y;
				v.h = CurveBar.h;
				v.w = CurveBar.h;
				v.x -= v.w / 2;
				void *pID = &pEnvelope->m_vPoints[i].m_Curvetype;
				const char *apTypeName[] = {
					"N", "L", "S", "F", "M"};
				const char *pTypeName = "Invalid";
				if(0 <= pEnvelope->m_vPoints[i].m_Curvetype && pEnvelope->m_vPoints[i].m_Curvetype < (int)std::size(apTypeName))
					pTypeName = apTypeName[pEnvelope->m_vPoints[i].m_Curvetype];
				if(DoButton_Editor(pID, pTypeName, 0, &v, 0, "Switch curve type"))
					pEnvelope->m_vPoints[i].m_Curvetype = (pEnvelope->m_vPoints[i].m_Curvetype + 1) % NUM_CURVETYPES;
			}
		}

		// render colorbar
		if(ShowColorBar)
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			for(int i = 0; i < (int)pEnvelope->m_vPoints.size() - 1; i++)
			{
				float r0 = fx2f(pEnvelope->m_vPoints[i].m_aValues[0]);
				float g0 = fx2f(pEnvelope->m_vPoints[i].m_aValues[1]);
				float b0 = fx2f(pEnvelope->m_vPoints[i].m_aValues[2]);
				float a0 = fx2f(pEnvelope->m_vPoints[i].m_aValues[3]);
				float r1 = fx2f(pEnvelope->m_vPoints[i + 1].m_aValues[0]);
				float g1 = fx2f(pEnvelope->m_vPoints[i + 1].m_aValues[1]);
				float b1 = fx2f(pEnvelope->m_vPoints[i + 1].m_aValues[2]);
				float a1 = fx2f(pEnvelope->m_vPoints[i + 1].m_aValues[3]);

				IGraphics::CColorVertex Array[4] = {IGraphics::CColorVertex(0, r0, g0, b0, a0),
					IGraphics::CColorVertex(1, r1, g1, b1, a1),
					IGraphics::CColorVertex(2, r1, g1, b1, a1),
					IGraphics::CColorVertex(3, r0, g0, b0, a0)};
				Graphics()->SetColorVertex(Array, 4);

				float x0 = pEnvelope->m_vPoints[i].m_Time / 1000.0f / EndTime;
				//				float y0 = (fx2f(envelope->points[i].values[c])-bottom)/(top-bottom);
				float x1 = pEnvelope->m_vPoints[i + 1].m_Time / 1000.0f / EndTime;
				//float y1 = (fx2f(envelope->points[i+1].values[c])-bottom)/(top-bottom);
				CUIRect v;
				v.x = ColorBar.x + x0 * ColorBar.w;
				v.y = ColorBar.y;
				v.w = (x1 - x0) * ColorBar.w;
				v.h = ColorBar.h;

				IGraphics::CQuadItem QuadItem(v.x, v.y, v.w, v.h);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
			}
			Graphics()->QuadsEnd();
		}

		// render handles

		// keep track of last Env
		static void *s_pID = nullptr;

		// chars for textinput
		static char s_aStrCurTime[32] = "0.000";
		static char s_aStrCurValue[32] = "0.000";

		if(CurrentEnvelopeSwitched)
		{
			s_pID = nullptr;

			// update displayed text
			str_format(s_aStrCurTime, sizeof(s_aStrCurTime), "0.000");
			str_format(s_aStrCurValue, sizeof(s_aStrCurValue), "0.000");
		}

		{
			int CurrentValue = 0, CurrentTime = 0;

			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			for(int c = 0; c < pEnvelope->m_Channels; c++)
			{
				if(!(s_ActiveChannels & (1 << c)))
					continue;

				for(size_t i = 0; i < pEnvelope->m_vPoints.size(); i++)
				{
					float x0 = pEnvelope->m_vPoints[i].m_Time / 1000.0f / EndTime;
					float y0 = (fx2f(pEnvelope->m_vPoints[i].m_aValues[c]) - Bottom) / (Top - Bottom);
					CUIRect Final;
					Final.x = View.x + x0 * View.w;
					Final.y = View.y + View.h - y0 * View.h;
					Final.x -= 2.0f;
					Final.y -= 2.0f;
					Final.w = 4.0f;
					Final.h = 4.0f;

					void *pID = &pEnvelope->m_vPoints[i].m_aValues[c];

					if(UI()->MouseInside(&Final))
						UI()->SetHotItem(pID);

					float ColorMod = 1.0f;

					if(UI()->CheckActiveItem(pID))
					{
						if(!UI()->MouseButton(0))
						{
							m_SelectedQuadEnvelope = -1;
							m_SelectedEnvelopePoint = -1;

							UI()->SetActiveItem(nullptr);
						}
						else
						{
							if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
							{
								if(i != 0)
								{
									if(Input()->ModifierIsPressed())
										pEnvelope->m_vPoints[i].m_Time += (int)((m_MouseDeltaX));
									else
										pEnvelope->m_vPoints[i].m_Time += (int)((m_MouseDeltaX * TimeScale) * 1000.0f);
									if(pEnvelope->m_vPoints[i].m_Time < pEnvelope->m_vPoints[i - 1].m_Time)
										pEnvelope->m_vPoints[i].m_Time = pEnvelope->m_vPoints[i - 1].m_Time + 1;
									if(i + 1 != pEnvelope->m_vPoints.size() && pEnvelope->m_vPoints[i].m_Time > pEnvelope->m_vPoints[i + 1].m_Time)
										pEnvelope->m_vPoints[i].m_Time = pEnvelope->m_vPoints[i + 1].m_Time - 1;
								}
							}
							else
							{
								if(Input()->ModifierIsPressed())
									pEnvelope->m_vPoints[i].m_aValues[c] -= f2fx(m_MouseDeltaY * 0.001f);
								else
									pEnvelope->m_vPoints[i].m_aValues[c] -= f2fx(m_MouseDeltaY * ValueScale);
							}

							m_SelectedQuadEnvelope = m_SelectedEnvelope;
							m_ShowEnvelopePreview = SHOWENV_SELECTED;
							m_SelectedEnvelopePoint = i;
							m_Map.m_Modified = true;
						}

						ColorMod = 100.0f;
						Graphics()->SetColor(1, 1, 1, 1);
					}
					else if(UI()->HotItem() == pID)
					{
						if(UI()->MouseButton(0))
						{
							s_vSelection.clear();
							s_vSelection.push_back(i);
							UI()->SetActiveItem(pID);
							// track it
							s_pID = pID;
						}

						// remove point
						if(UI()->MouseButtonClicked(1))
						{
							if(s_pID == pID)
							{
								s_pID = nullptr;

								// update displayed text
								str_format(s_aStrCurTime, sizeof(s_aStrCurTime), "0.000");
								str_format(s_aStrCurValue, sizeof(s_aStrCurValue), "0.000");
							}

							pEnvelope->m_vPoints.erase(pEnvelope->m_vPoints.begin() + i);
							m_Map.m_Modified = true;
						}

						m_ShowEnvelopePreview = SHOWENV_SELECTED;
						ColorMod = 100.0f;
						Graphics()->SetColor(1, 0.75f, 0.75f, 1);
						m_pTooltip = "Left mouse to drag. Hold ctrl to be more precise. Hold shift to alter time point as well. Right click to delete.";
					}

					if(pID == s_pID && (Input()->KeyIsPressed(KEY_RETURN) || Input()->KeyIsPressed(KEY_KP_ENTER)))
					{
						if(i != 0)
						{
							pEnvelope->m_vPoints[i].m_Time = str_tofloat(s_aStrCurTime) * 1000.0f;

							if(pEnvelope->m_vPoints[i].m_Time < pEnvelope->m_vPoints[i - 1].m_Time)
								pEnvelope->m_vPoints[i].m_Time = pEnvelope->m_vPoints[i - 1].m_Time + 1;
							if(i + 1 != pEnvelope->m_vPoints.size() && pEnvelope->m_vPoints[i].m_Time > pEnvelope->m_vPoints[i + 1].m_Time)
								pEnvelope->m_vPoints[i].m_Time = pEnvelope->m_vPoints[i + 1].m_Time - 1;
						}
						else
							pEnvelope->m_vPoints[i].m_Time = 0.0f;

						str_format(s_aStrCurTime, sizeof(s_aStrCurTime), "%.3f", pEnvelope->m_vPoints[i].m_Time / 1000.0f);

						pEnvelope->m_vPoints[i].m_aValues[c] = f2fx(str_tofloat(s_aStrCurValue));
					}

					if(UI()->CheckActiveItem(pID))
					{
						CurrentTime = pEnvelope->m_vPoints[i].m_Time;
						CurrentValue = pEnvelope->m_vPoints[i].m_aValues[c];

						// update displayed text
						str_format(s_aStrCurTime, sizeof(s_aStrCurTime), "%.3f", CurrentTime / 1000.0f);
						str_format(s_aStrCurValue, sizeof(s_aStrCurValue), "%.3f", fx2f(CurrentValue));
					}

					if(m_SelectedQuadEnvelope == m_SelectedEnvelope && m_SelectedEnvelopePoint == (int)i)
						Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
					else
						Graphics()->SetColor(aColors[c].r * ColorMod, aColors[c].g * ColorMod, aColors[c].b * ColorMod, 1.0f);
					IGraphics::CQuadItem QuadItem(Final.x, Final.y, Final.w, Final.h);
					Graphics()->QuadsDrawTL(&QuadItem, 1);
				}
			}
			Graphics()->QuadsEnd();

			CUIRect ToolBar1;
			CUIRect ToolBar2;

			ToolBar.VSplitMid(&ToolBar1, &ToolBar2);
			if(ToolBar.w > ToolBar.h * 21)
			{
				ToolBar1.VMargin(3.0f, &ToolBar1);
				ToolBar2.VMargin(3.0f, &ToolBar2);

				CUIRect Label1;
				CUIRect Label2;

				ToolBar1.VSplitMid(&Label1, &ToolBar1);
				ToolBar2.VSplitMid(&Label2, &ToolBar2);

				UI()->DoLabel(&Label1, "Value:", 10.0f, TEXTALIGN_LEFT);
				UI()->DoLabel(&Label2, "Time (in s):", 10.0f, TEXTALIGN_LEFT);
			}

			static float s_ValNumber = 0;
			DoEditBox(&s_ValNumber, &ToolBar1, s_aStrCurValue, sizeof(s_aStrCurValue), 10.0f, &s_ValNumber);
			static float s_TimeNumber = 0;
			DoEditBox(&s_TimeNumber, &ToolBar2, s_aStrCurTime, sizeof(s_aStrCurTime), 10.0f, &s_TimeNumber);
		}
	}
}

void CEditor::RenderServerSettingsEditor(CUIRect View, bool ShowServerSettingsEditorLast)
{
	static int s_CommandSelectedIndex = -1;

	CUIRect ToolBar;
	View.HSplitTop(20.0f, &ToolBar, &View);
	ToolBar.Margin(2.0f, &ToolBar);

	// do the toolbar
	{
		CUIRect Button;

		// command line
		ToolBar.VSplitLeft(5.0f, nullptr, &Button);
		UI()->DoLabel(&Button, "Command:", 12.0f, TEXTALIGN_LEFT);

		Button.VSplitLeft(70.0f, nullptr, &Button);
		Button.VSplitLeft(180.0f, &Button, nullptr);
		static int s_ClearButton = 0;
		DoClearableEditBox(&m_CommandBox, &s_ClearButton, &Button, m_aSettingsCommand, sizeof(m_aSettingsCommand), 12.0f, &m_CommandBox, false, IGraphics::CORNER_ALL);

		if(!ShowServerSettingsEditorLast) // Just activated
			UI()->SetActiveItem(&m_CommandBox);

		// buttons
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_AddButton = 0;
		if(DoButton_Editor(&s_AddButton, "Add", 0, &Button, 0, "Add a command to command list.") || ((Input()->KeyPress(KEY_RETURN) || Input()->KeyPress(KEY_KP_ENTER)) && UI()->LastActiveItem() == &m_CommandBox && m_Dialog == DIALOG_NONE))
		{
			if(m_aSettingsCommand[0] != 0 && str_find(m_aSettingsCommand, " "))
			{
				bool Found = false;
				for(const auto &Setting : m_Map.m_vSettings)
					if(!str_comp(Setting.m_aCommand, m_aSettingsCommand))
					{
						Found = true;
						break;
					}

				if(!Found)
				{
					CEditorMap::CSetting Setting;
					str_copy(Setting.m_aCommand, m_aSettingsCommand, sizeof(Setting.m_aCommand));
					m_Map.m_vSettings.push_back(Setting);
					s_CommandSelectedIndex = m_Map.m_vSettings.size() - 1;
				}
			}
			UI()->SetActiveItem(&m_CommandBox);
		}

		if(!m_Map.m_vSettings.empty() && s_CommandSelectedIndex >= 0 && (size_t)s_CommandSelectedIndex < m_Map.m_vSettings.size())
		{
			ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
			Button.VSplitRight(5.0f, &Button, nullptr);
			static int s_ModButton = 0;
			if(DoButton_Editor(&s_ModButton, "Mod", 0, &Button, 0, "Modify a command from the command list.") || (Input()->KeyPress(KEY_M) && UI()->LastActiveItem() != &m_CommandBox && m_Dialog == DIALOG_NONE))
			{
				if(str_comp(m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand, m_aSettingsCommand) != 0 && m_aSettingsCommand[0] != 0 && str_find(m_aSettingsCommand, " "))
				{
					bool Found = false;
					int i;
					for(i = 0; i < (int)m_Map.m_vSettings.size(); i++)
						if(i != s_CommandSelectedIndex && !str_comp(m_Map.m_vSettings[i].m_aCommand, m_aSettingsCommand))
						{
							Found = true;
							break;
						}
					if(Found)
					{
						m_Map.m_vSettings.erase(m_Map.m_vSettings.begin() + s_CommandSelectedIndex);
						s_CommandSelectedIndex = i > s_CommandSelectedIndex ? i - 1 : i;
					}
					else
					{
						str_copy(m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand, m_aSettingsCommand, sizeof(m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand));
					}
				}
				UI()->SetActiveItem(&m_CommandBox);
			}

			ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
			Button.VSplitRight(5.0f, &Button, nullptr);
			static int s_DelButton = 0;
			if(DoButton_Editor(&s_DelButton, "Del", 0, &Button, 0, "Delete a command from the command list.") || (Input()->KeyPress(KEY_DELETE) && UI()->LastActiveItem() != &m_CommandBox && m_Dialog == DIALOG_NONE))
			{
				m_Map.m_vSettings.erase(m_Map.m_vSettings.begin() + s_CommandSelectedIndex);
				if(s_CommandSelectedIndex >= (int)m_Map.m_vSettings.size())
					s_CommandSelectedIndex = m_Map.m_vSettings.size() - 1;
				if(s_CommandSelectedIndex >= 0)
					str_copy(m_aSettingsCommand, m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand, sizeof(m_aSettingsCommand));
				UI()->SetActiveItem(&m_CommandBox);
			}

			ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
			Button.VSplitRight(5.0f, &Button, nullptr);
			static int s_DownButton = 0;
			if(s_CommandSelectedIndex < (int)m_Map.m_vSettings.size() - 1 && DoButton_Editor(&s_DownButton, "▼", 0, &Button, 0, "Move command down"))
			{
				std::swap(m_Map.m_vSettings[s_CommandSelectedIndex], m_Map.m_vSettings[s_CommandSelectedIndex + 1]);
				s_CommandSelectedIndex++;
			}

			ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
			Button.VSplitRight(5.0f, &Button, nullptr);
			static int s_UpButton = 0;
			if(s_CommandSelectedIndex > 0 && DoButton_Editor(&s_UpButton, "▲", 0, &Button, 0, "Move command up"))
			{
				std::swap(m_Map.m_vSettings[s_CommandSelectedIndex], m_Map.m_vSettings[s_CommandSelectedIndex - 1]);
				s_CommandSelectedIndex--;
			}
		}
	}

	View.HSplitTop(2.0f, nullptr, &View);
	RenderBackground(View, m_CheckerTexture, 32.0f, 0.1f);

	CUIRect ListBox;
	View.Margin(1.0f, &ListBox);

	const float ButtonHeight = 15.0f;
	const float ButtonMargin = 2.0f;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = (ButtonHeight + ButtonMargin) * 5.0f;
	s_ScrollRegion.Begin(&ListBox, &ScrollOffset, &ScrollParams);
	ListBox.y += ScrollOffset.y;

	for(size_t i = 0; i < m_Map.m_vSettings.size(); i++)
	{
		CUIRect Button;
		ListBox.HSplitTop(ButtonHeight, &Button, &ListBox);
		ListBox.HSplitTop(ButtonMargin, nullptr, &ListBox);
		Button.VSplitLeft(5.0f, nullptr, &Button);
		if(s_ScrollRegion.AddRect(Button))
		{
			if(DoButton_MenuItem(&m_Map.m_vSettings[i], m_Map.m_vSettings[i].m_aCommand, s_CommandSelectedIndex >= 0 && (size_t)s_CommandSelectedIndex == i, &Button, 0, nullptr))
			{
				s_CommandSelectedIndex = i;
				str_copy(m_aSettingsCommand, m_Map.m_vSettings[i].m_aCommand);
				UI()->SetActiveItem(&m_CommandBox);
			}
		}
	}

	s_ScrollRegion.End();
}

int CEditor::PopupMenuFile(CEditor *pEditor, CUIRect View, void *pContext)
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
	if(pEditor->DoButton_MenuItem(&s_NewMapButton, "New", 0, &Slot, 0, "Creates a new map (ctrl+n)"))
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
	if(pEditor->DoButton_MenuItem(&s_OpenButton, "Load", 0, &Slot, 0, "Opens a map for editing (ctrl+l)"))
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
	if(pEditor->DoButton_MenuItem(&s_OpenCurrentMapButton, "Load Current Map", 0, &Slot, 0, "Opens the current in game map for editing (ctrl+alt+l)"))
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
	if(pEditor->DoButton_MenuItem(&s_AppendButton, "Append", 0, &Slot, 0, "Opens a map and adds everything from that map to the current one (ctrl+a)"))
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
	CUIRect FileButton;
	static int s_FileButton = 0;
	MenuBar.VSplitLeft(60.0f, &FileButton, &MenuBar);
	if(DoButton_Menu(&s_FileButton, "File", 0, &FileButton, 0, nullptr))
		UiInvokePopupMenu(&s_FileButton, 1, FileButton.x, FileButton.y + FileButton.h - 1.0f, 120, 160, PopupMenuFile, this);

	CUIRect Info, Close;
	MenuBar.VSplitLeft(40.0f, nullptr, &MenuBar);
	MenuBar.VSplitRight(20.0f, &MenuBar, &Close);
	Close.VSplitLeft(5.0f, nullptr, &Close);
	MenuBar.VSplitLeft(MenuBar.w * 0.75f, &MenuBar, &Info);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "File: %s", m_aFileName);
	UI()->DoLabel(&MenuBar, aBuf, 10.0f, TEXTALIGN_LEFT);

	char aTimeStr[6];
	str_timestamp_format(aTimeStr, sizeof(aTimeStr), "%H:%M");

	str_format(aBuf, sizeof(aBuf), "X: %i, Y: %i, Z: %i, A: %.1f, G: %i  %s", (int)UI()->MouseWorldX() / 32, (int)UI()->MouseWorldY() / 32, m_ZoomLevel, m_AnimateSpeed, m_GridFactor, aTimeStr);
	UI()->DoLabel(&Info, aBuf, 10.0f, TEXTALIGN_RIGHT);

	static int s_CloseButton = 0;
	if(DoButton_Editor(&s_CloseButton, "×", 0, &Close, 0, "Exits from the editor", 0) || (m_Dialog == DIALOG_NONE && !UiPopupOpen() && !m_PopupEventActivated && Input()->KeyPress(KEY_ESCAPE)))
		g_Config.m_ClEditor = 0;
}

void CEditor::Render()
{
	// basic start
	Graphics()->Clear(0.0f, 0.0f, 0.0f);
	CUIRect View = *UI()->Screen();
	UI()->MapScreen();

	float Width = View.w;
	float Height = View.h;

	// reset tip
	m_pTooltip = nullptr;

	if(m_EditBoxActive)
		--m_EditBoxActive;

	// render checker
	RenderBackground(View, m_CheckerTexture, 32.0f, 1.0f);

	CUIRect MenuBar, CModeBar, ToolBar, StatusBar, ExtraEditor, ToolBox;
	m_ShowPicker = Input()->KeyIsPressed(KEY_SPACE) != 0 && m_Dialog == DIALOG_NONE && m_EditBoxActive == 0 && UI()->LastActiveItem() != &m_CommandBox && m_vSelectedLayers.size() == 1;

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

		if(m_ShowServerSettingsEditor && !m_ShowPicker)
			View.HSplitBottom(250.0f, &View, &ExtraEditor);
	}
	else
	{
		// hack to get keyboard inputs from toolbar even when GUI is not active
		ToolBar.x = -100;
		ToolBar.y = -100;
		ToolBar.w = 50;
		ToolBar.h = 50;
	}

	//	a little hack for now
	if(m_Mode == MODE_LAYERS)
		DoMapEditor(View);

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

	for(int i = KEY_1; i <= KEY_0; i++)
	{
		if(m_Dialog != DIALOG_NONE || m_EditBoxActive != 0)
			break;

		if(Input()->KeyPress(i))
		{
			int Slot = i - KEY_1;
			if(Input()->ModifierIsPressed() && !m_Brush.IsEmpty())
			{
				dbg_msg("editor", "saving current brush to %d", Slot);
				if(m_apSavedBrushes[Slot])
				{
					CLayerGroup *pPrev = m_apSavedBrushes[Slot];
					for(auto &pLayer : pPrev->m_vpLayers)
					{
						if(pLayer->m_BrushRefCount == 1)
							delete pLayer;
						else
							pLayer->m_BrushRefCount--;
					}
				}
				delete m_apSavedBrushes[Slot];
				m_apSavedBrushes[Slot] = new CLayerGroup(m_Brush);

				for(auto &pLayer : m_apSavedBrushes[Slot]->m_vpLayers)
					pLayer->m_BrushRefCount++;
			}
			else if(m_apSavedBrushes[Slot])
			{
				dbg_msg("editor", "loading brush from slot %d", Slot);

				CLayerGroup *pNew = m_apSavedBrushes[Slot];
				for(auto &pLayer : pNew->m_vpLayers)
					pLayer->m_BrushRefCount++;

				m_Brush = *pNew;
			}
		}
	}

	float Brightness = 0.25f;

	if(m_GuiActive)
	{
		RenderBackground(MenuBar, m_BackgroundTexture, 128.0f, Brightness * 0);
		MenuBar.Margin(2.0f, &MenuBar);

		RenderBackground(ToolBox, m_BackgroundTexture, 128.0f, Brightness);
		ToolBox.Margin(2.0f, &ToolBox);

		RenderBackground(ToolBar, m_BackgroundTexture, 128.0f, Brightness);
		ToolBar.Margin(2.0f, &ToolBar);
		ToolBar.VSplitLeft(100.0f, &CModeBar, &ToolBar);

		RenderBackground(StatusBar, m_BackgroundTexture, 128.0f, Brightness);
		StatusBar.Margin(2.0f, &StatusBar);
	}

	// show mentions
	if(m_GuiActive && m_Mentions)
	{
		char aBuf[16];
		if(m_Mentions == 1)
		{
			str_copy(aBuf, Localize("1 new mention"), sizeof(aBuf));
		}
		else if(m_Mentions <= 9)
		{
			str_format(aBuf, sizeof(aBuf), Localize("%d new mentions"), m_Mentions);
		}
		else
		{
			str_copy(aBuf, Localize("9+ new mentions"), sizeof(aBuf));
		}

		TextRender()->TextColor(1.0f, 0.0f, 0.0f, 1.0f);
		TextRender()->Text(nullptr, 5.0f, 27.0f, 10.0f, aBuf, -1.0f);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	// do the toolbar
	if(m_Mode == MODE_LAYERS)
		DoToolbar(ToolBar);

	if(m_Dialog == DIALOG_NONE)
	{
		bool ModPressed = Input()->ModifierIsPressed();
		bool ShiftPressed = Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT);
		bool AltPressed = Input()->KeyIsPressed(KEY_LALT) || Input()->KeyIsPressed(KEY_RALT);
		// ctrl+n to create new map
		if(Input()->KeyPress(KEY_N) && ModPressed)
		{
			if(HasUnsavedData())
			{
				if(!m_PopupEventWasActivated)
				{
					m_PopupEventType = POPEVENT_NEW;
					m_PopupEventActivated = true;
				}
			}
			else
			{
				Reset();
				m_aFileName[0] = 0;
			}
		}
		// ctrl+a to append map
		if(Input()->KeyPress(KEY_A) && ModPressed)
		{
			InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_MAP, "Append map", "Append", "maps", "", CallbackAppendMap, this);
		}
		// ctrl+o or ctrl+l to open
		if((Input()->KeyPress(KEY_O) || Input()->KeyPress(KEY_L)) && ModPressed)
		{
			if(ShiftPressed)
			{
				if(HasUnsavedData())
				{
					if(!m_PopupEventWasActivated)
					{
						m_PopupEventType = POPEVENT_LOADCURRENT;
						m_PopupEventActivated = true;
					}
				}
				else
				{
					LoadCurrentMap();
				}
			}
			else
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
				{
					InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_MAP, "Load map", "Load", "maps", "", CallbackOpenMap, this);
				}
			}
		}

		// ctrl+shift+alt+s to save as
		if(Input()->KeyPress(KEY_S) && ModPressed && ShiftPressed && AltPressed)
			InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save", "maps", "", CallbackSaveCopyMap, this);
		// ctrl+shift+s to save as
		else if(Input()->KeyPress(KEY_S) && ModPressed && ShiftPressed)
			InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save", "maps", "", CallbackSaveMap, this);
		// ctrl+s to save
		else if(Input()->KeyPress(KEY_S) && ModPressed)
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
	}

	if(m_GuiActive)
	{
		if(!m_ShowPicker)
		{
			if(m_ShowEnvelopeEditor || m_ShowServerSettingsEditor)
			{
				RenderBackground(ExtraEditor, m_BackgroundTexture, 128.0f, Brightness);
				ExtraEditor.Margin(2.0f, &ExtraEditor);
			}
		}

		if(m_Mode == MODE_LAYERS)
			RenderLayers(ToolBox);
		else if(m_Mode == MODE_IMAGES)
		{
			RenderImagesList(ToolBox);
			RenderSelectedImage(View);
		}
		else if(m_Mode == MODE_SOUNDS)
			RenderSounds(ToolBox);
	}

	UI()->MapScreen();

	if(m_GuiActive)
	{
		RenderMenubar(MenuBar);

		RenderModebar(CModeBar);
		if(!m_ShowPicker)
		{
			if(m_ShowEnvelopeEditor)
				RenderEnvelopeEditor(ExtraEditor);
			static bool s_ShowServerSettingsEditorLast = false;
			if(m_ShowServerSettingsEditor)
			{
				RenderServerSettingsEditor(ExtraEditor, s_ShowServerSettingsEditorLast);
			}
			s_ShowServerSettingsEditorLast = m_ShowServerSettingsEditor;
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
		UiInvokePopupMenu(&s_PopupID, 0, Width / 2.0f - 200.0f, Height / 2.0f - 100.0f, 400.0f, 200.0f, PopupEvent);
		m_PopupEventActivated = false;
		m_PopupEventWasActivated = true;
	}

	UiDoPopupMenu();

	if(m_Dialog == DIALOG_NONE && !m_MouseInsidePopup && UI()->MouseInside(&View))
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
			m_ZoomLevel = maximum(m_ZoomLevel + Zoom * 20, 10);
			if(g_Config.m_ClLimitMaxZoomLevel)
				m_ZoomLevel = minimum(m_ZoomLevel, 2000);
			if(g_Config.m_EdZoomTarget)
				ZoomMouseTarget((float)m_ZoomLevel / OldLevel);
		}
	}

	m_WorldZoom = m_ZoomLevel / 100.0f;

	if(m_GuiActive)
		RenderStatusbar(StatusBar);

	//
	if(g_Config.m_EdShowkeys)
	{
		UI()->MapScreen();
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, View.x + 10, View.y + View.h - 24 - 10, 24.0f, TEXTFLAG_RENDER);

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
		Graphics()->WrapClamp();
		Graphics()->TextureSet(m_CursorTexture);
		Graphics()->QuadsBegin();
		if(ms_pUiGotContext == UI()->HotItem())
			Graphics()->SetColor(1, 0, 0, 1);
		IGraphics::CQuadItem QuadItem(mx, my, 16.0f, 16.0f);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();
		Graphics()->WrapNormal();
	}

	m_MouseInsidePopup = false;
}

void CEditor::Reset(bool CreateDefault)
{
	m_Map.Clean();

	//delete undo file
	char aBuffer[1024];
	m_pStorage->GetCompletePath(IStorage::TYPE_SAVE, "editor/", aBuffer, sizeof(aBuffer));

	mem_zero(m_apSavedBrushes, sizeof m_apSavedBrushes);

	// create default layers
	if(CreateDefault)
	{
		m_EditorWasUsedBefore = true;
		m_Map.CreateDefault(GetEntitiesTexture());
	}

	SelectGameLayer();
	m_vSelectedQuads.clear();
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

	m_ShowEnvelopePreview = SHOWENV_NONE;
	m_ShiftBy = 1;

	m_Map.m_Modified = false;
}

int CEditor::GetLineDistance() const
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
	RenderTools()->MapScreenToWorld(
		m_WorldOffsetX, m_WorldOffsetY,
		100.0f, 100.0f, 100.0f, 0.0f, 0.0f, Graphics()->ScreenAspect(), m_WorldZoom, aPoints);

	float WorldWidth = aPoints[2] - aPoints[0];
	float WorldHeight = aPoints[3] - aPoints[1];

	float Mwx = aPoints[0] + WorldWidth * (UI()->MouseX() / UI()->Screen()->w);
	float Mwy = aPoints[1] + WorldHeight * (UI()->MouseY() / UI()->Screen()->h);

	// adjust camera
	m_WorldOffsetX += (Mwx - m_WorldOffsetX) * (1 - ZoomFactor);
	m_WorldOffsetY += (Mwy - m_WorldOffsetY) * (1 - ZoomFactor);
}

void CEditor::Goto(float X, float Y)
{
	m_WorldOffsetX = X * 32;
	m_WorldOffsetY = Y * 32;
}

void CEditorMap::DeleteEnvelope(int Index)
{
	if(Index < 0 || Index >= (int)m_vpEnvelopes.size())
		return;

	m_Modified = true;

	// fix links between envelopes and quads
	for(auto &pGroup : m_vpGroups)
		for(auto &pLayer : pGroup->m_vpLayers)
			if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				CLayerQuads *pLayerQuads = static_cast<CLayerQuads *>(pLayer);
				for(auto &Quad : pLayerQuads->m_vQuads)
				{
					if(Quad.m_PosEnv == Index)
						Quad.m_PosEnv = -1;
					else if(Quad.m_PosEnv > Index)
						Quad.m_PosEnv--;
					if(Quad.m_ColorEnv == Index)
						Quad.m_ColorEnv = -1;
					else if(Quad.m_ColorEnv > Index)
						Quad.m_ColorEnv--;
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CLayerTiles *pLayerTiles = static_cast<CLayerTiles *>(pLayer);
				if(pLayerTiles->m_ColorEnv == Index)
					pLayerTiles->m_ColorEnv = -1;
				if(pLayerTiles->m_ColorEnv > Index)
					pLayerTiles->m_ColorEnv--;
			}
			else if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				CLayerSounds *pLayerSounds = static_cast<CLayerSounds *>(pLayer);
				for(auto &Source : pLayerSounds->m_vSources)
				{
					if(Source.m_PosEnv == Index)
						Source.m_PosEnv = -1;
					else if(Source.m_PosEnv > Index)
						Source.m_PosEnv--;
					if(Source.m_SoundEnv == Index)
						Source.m_SoundEnv = -1;
					else if(Source.m_SoundEnv > Index)
						Source.m_SoundEnv--;
				}
			}

	m_vpEnvelopes.erase(m_vpEnvelopes.begin() + Index);
}

void CEditorMap::MakeGameLayer(CLayer *pLayer)
{
	m_pGameLayer = (CLayerGame *)pLayer;
	m_pGameLayer->m_pEditor = m_pEditor;
	m_pGameLayer->m_Texture = m_pEditor->GetEntitiesTexture();
}

void CEditorMap::MakeGameGroup(CLayerGroup *pGroup)
{
	m_pGameGroup = pGroup;
	m_pGameGroup->m_GameGroup = true;
	str_copy(m_pGameGroup->m_aName, "Game", sizeof(m_pGameGroup->m_aName));
}

void CEditorMap::Clean()
{
	for(auto &pGroup : m_vpGroups)
	{
		DeleteAll(pGroup->m_vpLayers);
	}
	DeleteAll(m_vpGroups);
	DeleteAll(m_vpEnvelopes);
	DeleteAll(m_vpImages);
	DeleteAll(m_vpSounds);

	m_MapInfo.Reset();

	m_vSettings.clear();

	m_pGameLayer = nullptr;
	m_pGameGroup = nullptr;

	m_Modified = false;

	m_pTeleLayer = nullptr;
	m_pSpeedupLayer = nullptr;
	m_pFrontLayer = nullptr;
	m_pSwitchLayer = nullptr;
	m_pTuneLayer = nullptr;
}

void CEditorMap::CreateDefault(IGraphics::CTextureHandle EntitiesTexture)
{
	// add background
	CLayerGroup *pGroup = NewGroup();
	pGroup->m_ParallaxX = 0;
	pGroup->m_ParallaxY = 0;
	pGroup->m_CustomParallaxZoom = 0;
	pGroup->m_ParallaxZoom = 0;
	CLayerQuads *pLayer = new CLayerQuads;
	pLayer->m_pEditor = m_pEditor;
	CQuad *pQuad = pLayer->NewQuad(0, 0, 1600, 1200);
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

	m_pFrontLayer = nullptr;
	m_pTeleLayer = nullptr;
	m_pSpeedupLayer = nullptr;
	m_pSwitchLayer = nullptr;
	m_pTuneLayer = nullptr;
}

int CEditor::GetTextureUsageFlag()
{
	return Graphics()->HasTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE;
}

IGraphics::CTextureHandle CEditor::GetFrontTexture()
{
	int TextureLoadFlag = GetTextureUsageFlag();

	if(!m_FrontTexture.IsValid())
		m_FrontTexture = Graphics()->LoadTexture("editor/front.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, TextureLoadFlag);
	return m_FrontTexture;
}

IGraphics::CTextureHandle CEditor::GetTeleTexture()
{
	int TextureLoadFlag = GetTextureUsageFlag();
	if(!m_TeleTexture.IsValid())
		m_TeleTexture = Graphics()->LoadTexture("editor/tele.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, TextureLoadFlag);
	return m_TeleTexture;
}

IGraphics::CTextureHandle CEditor::GetSpeedupTexture()
{
	int TextureLoadFlag = GetTextureUsageFlag();
	if(!m_SpeedupTexture.IsValid())
		m_SpeedupTexture = Graphics()->LoadTexture("editor/speedup.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, TextureLoadFlag);
	return m_SpeedupTexture;
}

IGraphics::CTextureHandle CEditor::GetSwitchTexture()
{
	int TextureLoadFlag = GetTextureUsageFlag();
	if(!m_SwitchTexture.IsValid())
		m_SwitchTexture = Graphics()->LoadTexture("editor/switch.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, TextureLoadFlag);
	return m_SwitchTexture;
}

IGraphics::CTextureHandle CEditor::GetTuneTexture()
{
	int TextureLoadFlag = GetTextureUsageFlag();
	if(!m_TuneTexture.IsValid())
		m_TuneTexture = Graphics()->LoadTexture("editor/tune.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, TextureLoadFlag);
	return m_TuneTexture;
}

IGraphics::CTextureHandle CEditor::GetEntitiesTexture()
{
	int TextureLoadFlag = GetTextureUsageFlag();
	if(!m_EntitiesTexture.IsValid())
		m_EntitiesTexture = Graphics()->LoadTexture("editor/entities/DDNet.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, TextureLoadFlag);
	return m_EntitiesTexture;
}

void CEditor::Init()
{
	m_pInput = Kernel()->RequestInterface<IInput>();
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pConfig = Kernel()->RequestInterface<IConfigManager>()->Values();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pGraphics = Kernel()->RequestInterface<IGraphics>();
	m_pTextRender = Kernel()->RequestInterface<ITextRender>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pSound = Kernel()->RequestInterface<ISound>();
	m_UI.Init(Kernel());
	m_RenderTools.Init(m_pGraphics, m_pTextRender);
	m_Map.m_pEditor = this;

	m_CheckerTexture = Graphics()->LoadTexture("editor/checker.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	m_BackgroundTexture = Graphics()->LoadTexture("editor/background.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	m_CursorTexture = Graphics()->LoadTexture("editor/cursor.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);

	m_TilesetPicker.m_pEditor = this;
	m_TilesetPicker.MakePalette();
	m_TilesetPicker.m_Readonly = true;

	m_QuadsetPicker.m_pEditor = this;
	m_QuadsetPicker.NewQuad(0, 0, 64, 64);
	m_QuadsetPicker.m_Readonly = true;

	m_Brush.m_pMap = &m_Map;

	Reset(false);
	m_Map.m_Modified = false;

	ms_PickerColor = ColorHSVA(1.0f, 0.0f, 0.0f);
}

void CEditor::PlaceBorderTiles()
{
	CLayerTiles *pT = (CLayerTiles *)GetSelectedLayerType(0, LAYERTYPE_TILES);

	for(int i = 0; i < pT->m_Width * 2; ++i)
		pT->m_pTiles[i].m_Index = 1;

	for(int i = 0; i < pT->m_Width * pT->m_Height; ++i)
	{
		if(i % pT->m_Width < 2 || i % pT->m_Width > pT->m_Width - 3)
			pT->m_pTiles[i].m_Index = 1;
	}

	for(int i = (pT->m_Width * (pT->m_Height - 2)); i < pT->m_Width * pT->m_Height; ++i)
		pT->m_pTiles[i].m_Index = 1;
}

void CEditor::OnUpdate()
{
	CUIElementBase::Init(UI()); // update static pointer because game and editor use separate UI

	if(!m_EditorWasUsedBefore)
	{
		m_EditorWasUsedBefore = true;
		Reset();
	}

	for(int i = 0; i < Input()->NumEvents(); i++)
		UI()->OnInput(Input()->GetEvent(i));

	// handle cursor movement
	{
		static float s_MouseX = 0.0f;
		static float s_MouseY = 0.0f;

		float MouseRelX = 0.0f, MouseRelY = 0.0f;
		IInput::ECursorType CursorType = Input()->CursorRelative(&MouseRelX, &MouseRelY);
		if(CursorType != IInput::CURSOR_NONE)
			UI()->ConvertMouseMove(&MouseRelX, &MouseRelY, CursorType);
		UI()->ResetMouseSlow();

		m_MouseDeltaX += MouseRelX;
		m_MouseDeltaY += MouseRelY;

		if(!m_LockMouse)
		{
			s_MouseX = clamp<float>(s_MouseX + MouseRelX, 0.0f, Graphics()->WindowWidth());
			s_MouseY = clamp<float>(s_MouseY + MouseRelY, 0.0f, Graphics()->WindowHeight());
		}

		// update positions for ui, but only update ui when rendering
		m_MouseX = UI()->Screen()->w * ((float)s_MouseX / Graphics()->WindowWidth());
		m_MouseY = UI()->Screen()->h * ((float)s_MouseY / Graphics()->WindowHeight());

		// fix correct world x and y
		CLayerGroup *pGroup = GetSelectedGroup();
		if(pGroup)
		{
			float aPoints[4];
			pGroup->Mapping(aPoints);

			float WorldWidth = aPoints[2] - aPoints[0];
			float WorldHeight = aPoints[3] - aPoints[1];

			m_MouseWScale = WorldWidth / Graphics()->WindowWidth();

			m_MouseWorldX = aPoints[0] + WorldWidth * (s_MouseX / Graphics()->WindowWidth());
			m_MouseWorldY = aPoints[1] + WorldHeight * (s_MouseY / Graphics()->WindowHeight());
			m_MouseDeltaWx = m_MouseDeltaX * (WorldWidth / Graphics()->WindowWidth());
			m_MouseDeltaWy = m_MouseDeltaY * (WorldHeight / Graphics()->WindowHeight());
		}
		else
		{
			m_MouseWorldX = 0.0f;
			m_MouseWorldY = 0.0f;
		}
	}
}

void CEditor::OnRender()
{
	// toggle gui
	if(Input()->KeyPress(KEY_TAB))
		m_GuiActive = !m_GuiActive;

	if(Input()->KeyPress(KEY_F10))
		m_ShowMousePointer = false;

	if(m_Animate)
		m_AnimateTime = (time_get() - m_AnimateStart) / (float)time_freq();
	else
		m_AnimateTime = 0;

	ms_pUiGotContext = nullptr;
	UI()->StartCheck();

	UI()->Update(m_MouseX, m_MouseY, m_MouseWorldX, m_MouseWorldY);

	Render();

	m_MouseDeltaX = 0.0f;
	m_MouseDeltaY = 0.0f;
	m_MouseDeltaWx = 0.0f;
	m_MouseDeltaWy = 0.0f;

	if(Input()->KeyPress(KEY_F10))
	{
		Graphics()->TakeScreenshot(nullptr);
		m_ShowMousePointer = true;
	}

	UI()->FinishCheck();
	UI()->ClearHotkeys();
	Input()->Clear();
}

void CEditor::LoadCurrentMap()
{
	Load(m_pClient->GetCurrentMapPath(), IStorage::TYPE_ALL);
	m_ValidSaveFilename = true;

	CGameClient *pGameClient = (CGameClient *)Kernel()->RequestInterface<IGameClient>();
	vec2 Center = pGameClient->m_Camera.m_Center;

	m_WorldOffsetX = Center.x;
	m_WorldOffsetY = Center.y;
}

IEditor *CreateEditor() { return new CEditor; }

// DDRace

void CEditorMap::MakeTeleLayer(CLayer *pLayer)
{
	m_pTeleLayer = (CLayerTele *)pLayer;
	m_pTeleLayer->m_pEditor = m_pEditor;
	m_pTeleLayer->m_Texture = m_pEditor->GetTeleTexture();
}

void CEditorMap::MakeSpeedupLayer(CLayer *pLayer)
{
	m_pSpeedupLayer = (CLayerSpeedup *)pLayer;
	m_pSpeedupLayer->m_pEditor = m_pEditor;
	m_pSpeedupLayer->m_Texture = m_pEditor->GetSpeedupTexture();
}

void CEditorMap::MakeFrontLayer(CLayer *pLayer)
{
	m_pFrontLayer = (CLayerFront *)pLayer;
	m_pFrontLayer->m_pEditor = m_pEditor;
	m_pFrontLayer->m_Texture = m_pEditor->GetFrontTexture();
}

void CEditorMap::MakeSwitchLayer(CLayer *pLayer)
{
	m_pSwitchLayer = (CLayerSwitch *)pLayer;
	m_pSwitchLayer->m_pEditor = m_pEditor;
	m_pSwitchLayer->m_Texture = m_pEditor->GetSwitchTexture();
}

void CEditorMap::MakeTuneLayer(CLayer *pLayer)
{
	m_pTuneLayer = (CLayerTune *)pLayer;
	m_pTuneLayer->m_pEditor = m_pEditor;
	m_pTuneLayer->m_Texture = m_pEditor->GetTuneTexture();
}
