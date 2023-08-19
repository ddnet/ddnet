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
#include <engine/shared/filecollection.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/components/camera.h>
#include <game/client/components/menu_background.h>
#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/generated/client_data.h>
#include <game/localization.h>

#include "auto_map.h"
#include "editor.h"

#include <chrono>
#include <limits>
#include <type_traits>

using namespace FontIcons;

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

bool CEditor::IsVanillaImage(const char *pImage)
{
	return std::any_of(std::begin(VANILLA_IMAGES), std::end(VANILLA_IMAGES), [pImage](const char *pVanillaImage) { return str_comp(pImage, pVanillaImage) == 0; });
}

static const char *FILETYPE_EXTENSIONS[CEditor::NUM_FILETYPES] = {
	".map",
	".png",
	".opus"};

const void *CEditor::ms_pUiGotContext;

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
	m_vpLayers.clear();
	m_aName[0] = 0;
	m_Visible = true;
	m_Collapse = false;
	m_GameGroup = false;
	m_OffsetX = 0;
	m_OffsetY = 0;
	m_ParallaxX = 100;
	m_ParallaxY = 100;
	m_CustomParallaxZoom = 0;
	m_ParallaxZoom = 100;

	m_UseClipping = 0;
	m_ClipX = 0;
	m_ClipY = 0;
	m_ClipW = 0;
	m_ClipH = 0;
}

CLayerGroup::~CLayerGroup()
{
	m_vpLayers.clear();
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
		m_pMap->m_pEditor->MapView()->m_WorldOffset.x, m_pMap->m_pEditor->MapView()->m_WorldOffset.y,
		m_ParallaxX, m_ParallaxY, ParallaxZoom, m_OffsetX, m_OffsetY,
		m_pMap->m_pEditor->Graphics()->ScreenAspect(), m_pMap->m_pEditor->MapView()->m_WorldZoom, pPoints);

	pPoints[0] += m_pMap->m_pEditor->MapView()->m_EditorOffset.x;
	pPoints[1] += m_pMap->m_pEditor->MapView()->m_EditorOffset.y;
	pPoints[2] += m_pMap->m_pEditor->MapView()->m_EditorOffset.x;
	pPoints[3] += m_pMap->m_pEditor->MapView()->m_EditorOffset.y;
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
				std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
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
			std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
			if(pTiles->m_Game || pTiles->m_Front || pTiles->m_Tele || pTiles->m_Speedup || pTiles->m_Tune || pTiles->m_Switch)
			{
				pLayer->Render();
			}
		}
	}

	if(m_UseClipping)
		pGraphics->ClipDisable();
}

void CLayerGroup::AddLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pMap->OnModify();
	m_vpLayers.push_back(pLayer);
}

void CLayerGroup::DeleteLayer(int Index)
{
	if(Index < 0 || Index >= (int)m_vpLayers.size())
		return;
	m_vpLayers.erase(m_vpLayers.begin() + Index);
	m_pMap->OnModify();
}

void CLayerGroup::DuplicateLayer(int Index)
{
	if(Index < 0 || Index >= (int)m_vpLayers.size())
		return;

	std::shared_ptr<CLayer> pDup = m_vpLayers[Index]->Duplicate();
	m_vpLayers.insert(m_vpLayers.begin() + Index + 1, pDup);

	m_pMap->OnModify();
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
	m_pMap->OnModify();
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

	std::shared_ptr<CEnvelope> pEnv = pThis->m_Map.m_vpEnvelopes[Env];
	float t = pThis->m_AnimateTime;
	t *= pThis->m_AnimateSpeed;
	t += (TimeOffsetMillis / 1000.0f);
	pEnv->Eval(t, Channels);
}

/********************************************************
 OTHER
*********************************************************/

bool CEditor::DoEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners, const char *pToolTip)
{
	UpdateTooltip(pLineInput, pRect, pToolTip);
	return UI()->DoEditBox(pLineInput, pRect, FontSize, Corners);
}

bool CEditor::DoClearableEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners, const char *pToolTip)
{
	UpdateTooltip(pLineInput, pRect, pToolTip);
	return UI()->DoClearableEditBox(pLineInput, pRect, FontSize, Corners);
}

ColorRGBA CEditor::GetButtonColor(const void *pID, int Checked)
{
	if(Checked < 0)
		return ColorRGBA(0, 0, 0, 0.5f);

	switch(Checked)
	{
	case 8: // invisible
		return ColorRGBA(0, 0, 0, 0);
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

void CEditor::UpdateTooltip(const void *pID, const CUIRect *pRect, const char *pToolTip)
{
	if((UI()->MouseInside(pRect) && m_pTooltip) || (UI()->HotItem() == pID && pToolTip))
		m_pTooltip = pToolTip;
}

int CEditor::DoButton_Editor_Common(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(UI()->MouseInside(pRect))
	{
		if(Flags & BUTTON_CONTEXT)
			ms_pUiGotContext = pID;
	}

	UpdateTooltip(pID, pRect, pToolTip);
	return UI()->DoButtonLogic(pID, Checked, pRect);
}

int CEditor::DoButton_Editor(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	pRect->Draw(GetButtonColor(pID, Checked), IGraphics::CORNER_ALL, 3.0f);
	CUIRect NewRect = *pRect;
	UI()->DoLabel(&NewRect, pText, 10.0f, TEXTALIGN_MC);
	Checked %= 2;
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Env(const void *pID, const char *pText, int Checked, const CUIRect *pRect, const char *pToolTip, ColorRGBA BaseColor, int Corners)
{
	float Bright = Checked ? 1.0f : 0.5f;
	float Alpha = UI()->HotItem() == pID ? 1.0f : 0.75f;
	ColorRGBA Color = ColorRGBA(BaseColor.r * Bright, BaseColor.g * Bright, BaseColor.b * Bright, Alpha);

	pRect->Draw(Color, Corners, 3.0f);
	UI()->DoLabel(pRect, pText, 10.0f, TEXTALIGN_MC);
	Checked %= 2;
	return DoButton_Editor_Common(pID, pText, Checked, pRect, 0, pToolTip);
}

int CEditor::DoButton_File(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(Checked)
		pRect->Draw(GetButtonColor(pID, Checked), IGraphics::CORNER_ALL, 3.0f);

	CUIRect Rect;
	pRect->VMargin(5.0f, &Rect);
	UI()->DoLabel(&Rect, pText, 10.0f, TEXTALIGN_ML);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Menu(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	pRect->Draw(ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), IGraphics::CORNER_T, 3.0f);

	CUIRect Rect;
	pRect->VMargin(5.0f, &Rect);
	UI()->DoLabel(&Rect, pText, 10.0f, TEXTALIGN_ML);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_MenuItem(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(UI()->HotItem() == pID || Checked)
		pRect->Draw(GetButtonColor(pID, Checked), IGraphics::CORNER_ALL, 3.0f);

	CUIRect Rect;
	pRect->VMargin(5.0f, &Rect);

	SLabelProperties Props;
	Props.m_MaxWidth = Rect.w;
	Props.m_EllipsisAtEnd = true;
	UI()->DoLabel(&Rect, pText, 10.0f, TEXTALIGN_ML, Props);

	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Ex(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners, float FontSize)
{
	pRect->Draw(GetButtonColor(pID, Checked), Corners, 3.0f);

	CUIRect Rect;
	pRect->VMargin(pRect->w > 20.0f ? 5.0f : 0.0f, &Rect);

	SLabelProperties Props;
	Props.m_MaxWidth = Rect.w;
	Props.m_EllipsisAtEnd = true;
	UI()->DoLabel(&Rect, pText, FontSize, TEXTALIGN_MC, Props);

	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_FontIcon(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners, float FontSize)
{
	pRect->Draw(GetButtonColor(pID, Checked), Corners, 3.0f);

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	UI()->DoLabel(pRect, pText, FontSize, TEXTALIGN_MC);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_ButtonInc(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	pRect->Draw(GetButtonColor(pID, Checked), IGraphics::CORNER_R, 3.0f);
	UI()->DoLabel(pRect, pText ? pText : "+", 10.0f, TEXTALIGN_MC);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_ButtonDec(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	pRect->Draw(GetButtonColor(pID, Checked), IGraphics::CORNER_L, 3.0f);
	UI()->DoLabel(pRect, pText ? pText : "-", 10.0f, TEXTALIGN_MC);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_DraggableEx(const void *pID, const char *pText, int Checked, const CUIRect *pRect, bool *pClicked, bool *pAbrupted, int Flags, const char *pToolTip, int Corners, float FontSize)
{
	pRect->Draw(GetButtonColor(pID, Checked), Corners, 3.0f);

	CUIRect Rect;
	pRect->VMargin(pRect->w > 20.0f ? 5.0f : 0.0f, &Rect);

	SLabelProperties Props;
	Props.m_MaxWidth = Rect.w;
	Props.m_EllipsisAtEnd = true;
	UI()->DoLabel(&Rect, pText, FontSize, TEXTALIGN_MC, Props);

	if(UI()->MouseInside(pRect))
	{
		if(Flags & BUTTON_CONTEXT)
			ms_pUiGotContext = pID;
	}

	UpdateTooltip(pID, pRect, pToolTip);
	return UI()->DoDraggableButtonLogic(pID, Checked, pRect, pClicked, pAbrupted);
}

void CEditor::RenderGrid(const std::shared_ptr<CLayerGroup> &pGroup)
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

void CEditor::SnapToGrid(float &x, float &y)
{
	const int GridDistance = GetLineDistance() * m_GridFactor;
	x = (int)((x + (x >= 0 ? 1.0f : -1.0f) * GridDistance / 2) / GridDistance) * GridDistance;
	y = (int)((y + (y >= 0 ? 1.0f : -1.0f) * GridDistance / 2) / GridDistance) * GridDistance;
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

int CEditor::UiDoValueSelector(void *pID, CUIRect *pRect, const char *pLabel, int Current, int Min, int Max, int Step, float Scale, const char *pToolTip, bool IsDegree, bool IsHex, int Corners, ColorRGBA *pColor, bool ShowValue)
{
	// logic
	static float s_Value;
	static CLineInputNumber s_NumberInput;
	static bool s_TextMode = false;
	static void *s_pLastTextID = pID;
	const bool Inside = UI()->MouseInside(pRect);
	const int Base = IsHex ? 16 : 10;

	if(UI()->MouseButton(1) && UI()->HotItem() == pID)
	{
		s_pLastTextID = pID;
		s_TextMode = true;
		UI()->DisableMouseLock();
		s_NumberInput.SetInteger(Current, Base);
	}

	if(UI()->CheckActiveItem(pID))
	{
		if(!UI()->MouseButton(0))
		{
			UI()->DisableMouseLock();
			UI()->SetActiveItem(nullptr);
			s_TextMode = false;
		}
	}

	if(s_TextMode && s_pLastTextID == pID)
	{
		m_pTooltip = "Type your number";

		DoEditBox(&s_NumberInput, pRect, 10.0f);

		UI()->SetActiveItem(&s_NumberInput);

		if(Input()->KeyIsPressed(KEY_RETURN) || Input()->KeyIsPressed(KEY_KP_ENTER) ||
			((UI()->MouseButton(1) || UI()->MouseButton(0)) && !Inside))
		{
			Current = clamp(s_NumberInput.GetInteger(Base), Min, Max);
			UI()->DisableMouseLock();
			UI()->SetActiveItem(nullptr);
			s_TextMode = false;
		}

		if(Input()->KeyIsPressed(KEY_ESCAPE))
		{
			UI()->DisableMouseLock();
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
				if(Input()->ShiftIsPressed())
					s_Value += m_MouseDeltaX * 0.05f;
				else
					s_Value += m_MouseDeltaX;

				if(absolute(s_Value) >= Scale)
				{
					int Count = (int)(s_Value / Scale);
					s_Value = std::fmod(s_Value, Scale);
					Current += Step * Count;
					Current = clamp(Current, Min, Max);

					// Constrain to discrete steps
					if(Count > 0)
						Current = Current / Step * Step;
					else
						Current = std::ceil(Current / (float)Step) * Step;
				}
			}
			if(pToolTip && !s_TextMode)
				m_pTooltip = pToolTip;
		}
		else if(UI()->HotItem() == pID)
		{
			if(UI()->MouseButton(0))
			{
				UI()->SetActiveItem(pID);
				UI()->EnableMouseLock(pID);
				s_Value = 0;
			}
			if(pToolTip && !s_TextMode)
				m_pTooltip = pToolTip;
		}

		if(Inside)
			UI()->SetHotItem(pID);

		// render
		char aBuf[128];
		if(pLabel[0] != '\0')
		{
			if(ShowValue)
				str_format(aBuf, sizeof(aBuf), "%s %d", pLabel, Current);
			else
				str_copy(aBuf, pLabel);
		}
		else if(IsDegree)
			str_format(aBuf, sizeof(aBuf), "%d°", Current);
		else if(IsHex)
			str_format(aBuf, sizeof(aBuf), "#%06X", Current);
		else
			str_format(aBuf, sizeof(aBuf), "%d", Current);
		pRect->Draw(pColor ? *pColor : GetButtonColor(pID, 0), Corners, 5.0f);
		UI()->DoLabel(pRect, aBuf, 10, TEXTALIGN_MC);
	}

	if(!s_TextMode)
		s_NumberInput.Clear();

	return Current;
}

std::shared_ptr<CLayerGroup> CEditor::GetSelectedGroup() const
{
	if(m_SelectedGroup >= 0 && m_SelectedGroup < (int)m_Map.m_vpGroups.size())
		return m_Map.m_vpGroups[m_SelectedGroup];
	return nullptr;
}

std::shared_ptr<CLayer> CEditor::GetSelectedLayer(int Index) const
{
	std::shared_ptr<CLayerGroup> pGroup = GetSelectedGroup();
	if(!pGroup)
		return nullptr;

	if(Index < 0 || Index >= (int)m_vSelectedLayers.size())
		return nullptr;

	int LayerIndex = m_vSelectedLayers[Index];

	if(LayerIndex >= 0 && LayerIndex < (int)m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers.size())
		return pGroup->m_vpLayers[LayerIndex];
	return nullptr;
}

std::shared_ptr<CLayer> CEditor::GetSelectedLayerType(int Index, int Type) const
{
	std::shared_ptr<CLayer> pLayer = GetSelectedLayer(Index);
	if(pLayer && pLayer->m_Type == Type)
		return pLayer;
	return nullptr;
}

std::vector<CQuad *> CEditor::GetSelectedQuads()
{
	std::shared_ptr<CLayerQuads> pQuadLayer = std::static_pointer_cast<CLayerQuads>(GetSelectedLayerType(0, LAYERTYPE_QUADS));
	std::vector<CQuad *> vpQuads;
	if(!pQuadLayer)
		return vpQuads;
	vpQuads.resize(m_vSelectedQuads.size());
	for(int i = 0; i < (int)m_vSelectedQuads.size(); ++i)
		vpQuads[i] = &pQuadLayer->m_vQuads[m_vSelectedQuads[i]];
	return vpQuads;
}

std::vector<std::pair<CQuad *, int>> CEditor::GetSelectedQuadPoints()
{
	std::shared_ptr<CLayerQuads> pQuadLayer = std::static_pointer_cast<CLayerQuads>(GetSelectedLayerType(0, LAYERTYPE_QUADS));
	std::vector<std::pair<CQuad *, int>> vpQuads;
	if(!pQuadLayer)
		return vpQuads;
	vpQuads.reserve(m_vSelectedQuadPoints.size());
	for(auto [QuadIndex, SelectedPoints] : m_vSelectedQuadPoints)
		vpQuads.emplace_back(&pQuadLayer->m_vQuads[QuadIndex], SelectedPoints);
	return vpQuads;
}

CSoundSource *CEditor::GetSelectedSource()
{
	std::shared_ptr<CLayerSounds> pSounds = std::static_pointer_cast<CLayerSounds>(GetSelectedLayerType(0, LAYERTYPE_SOUNDS));
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
	m_vSelectedQuadPoints.clear();
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

void CEditor::ToggleSelectQuad(int Index)
{
	int ListIndex = FindSelectedQuadIndex(Index);
	if(ListIndex < 0)
		m_vSelectedQuads.push_back(Index);
	else
		m_vSelectedQuads.erase(m_vSelectedQuads.begin() + ListIndex);
}

void CEditor::DeselectQuads()
{
	m_vSelectedQuads.clear();
}

void CEditor::DeselectQuadPoints()
{
	m_vSelectedQuadPoints.clear();
}

void CEditor::SelectQuadPoint(int QuadIndex, int Index)
{
	m_vSelectedQuadPoints.clear();
	m_vSelectedQuadPoints.emplace_back(QuadIndex, 1 << Index);
}

void CEditor::ToggleSelectQuadPoint(int QuadIndex, int Index)
{
	int ListIndex = FindSelectedQuadPointIndex(QuadIndex);

	if(ListIndex >= 0)
	{
		m_vSelectedQuadPoints.at(ListIndex).second ^= 1 << Index;
		if(m_vSelectedQuadPoints.at(ListIndex).second == 0)
			m_vSelectedQuadPoints.erase(m_vSelectedQuadPoints.begin() + ListIndex);
	}
	else
		m_vSelectedQuadPoints.emplace_back(QuadIndex, 1 << Index);
}

void CEditor::DeleteSelectedQuads()
{
	std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(GetSelectedLayerType(0, LAYERTYPE_QUADS));
	if(!pLayer)
		return;

	for(int i = 0; i < (int)m_vSelectedQuads.size(); ++i)
	{
		pLayer->m_vQuads.erase(pLayer->m_vQuads.begin() + m_vSelectedQuads[i]);
		for(int j = i + 1; j < (int)m_vSelectedQuads.size(); ++j)
			if(m_vSelectedQuads[j] > m_vSelectedQuads[i])
				m_vSelectedQuads[j]--;

		int QuadIndex = m_vSelectedQuads.at(i);
		m_vSelectedQuads.erase(m_vSelectedQuads.begin() + i);

		int ListIndex = FindSelectedQuadPointIndex(QuadIndex);
		if(ListIndex >= 0)
			m_vSelectedQuadPoints.erase(m_vSelectedQuadPoints.begin() + ListIndex);

		i--;
	}
}

bool CEditor::IsQuadSelected(int Index) const
{
	return FindSelectedQuadIndex(Index) >= 0;
}

bool CEditor::IsQuadPointSelected(int QuadIndex, int Index) const
{
	int ListIndex = FindSelectedQuadPointIndex(QuadIndex);
	return ListIndex >= 0 && (m_vSelectedQuadPoints.at(ListIndex).second & (1 << Index));
}

int CEditor::FindSelectedQuadIndex(int Index) const
{
	for(size_t i = 0; i < m_vSelectedQuads.size(); ++i)
		if(m_vSelectedQuads[i] == Index)
			return i;
	return -1;
}

int CEditor::FindSelectedQuadPointIndex(int QuadIndex) const
{
	auto Iter = std::find_if(
		m_vSelectedQuadPoints.begin(),
		m_vSelectedQuadPoints.end(),
		[QuadIndex](auto Pair) { return Pair.first == QuadIndex; });

	if(Iter != m_vSelectedQuadPoints.end())
		return Iter - m_vSelectedQuadPoints.begin();
	else
		return -1;
}

int CEditor::FindEnvPointIndex(int Index, int Channel) const
{
	auto Iter = std::find(
		m_vSelectedEnvelopePoints.begin(),
		m_vSelectedEnvelopePoints.end(),
		std::pair(Index, Channel));

	if(Iter != m_vSelectedEnvelopePoints.end())
		return Iter - m_vSelectedEnvelopePoints.begin();
	else
		return -1;
}

void CEditor::SelectEnvPoint(int Index)
{
	m_vSelectedEnvelopePoints.clear();

	for(int c = 0; c < CEnvPoint::MAX_CHANNELS; c++)
		m_vSelectedEnvelopePoints.emplace_back(Index, c);
}

void CEditor::SelectEnvPoint(int Index, int Channel)
{
	DeselectEnvPoints();
	m_vSelectedEnvelopePoints.emplace_back(Index, Channel);
}

void CEditor::ToggleEnvPoint(int Index, int Channel)
{
	if(IsTangentSelected())
		DeselectEnvPoints();

	int ListIndex = FindEnvPointIndex(Index, Channel);

	if(ListIndex >= 0)
	{
		m_vSelectedEnvelopePoints.erase(m_vSelectedEnvelopePoints.begin() + ListIndex);
	}
	else
		m_vSelectedEnvelopePoints.emplace_back(Index, Channel);
}

bool CEditor::IsEnvPointSelected(int Index, int Channel) const
{
	int ListIndex = FindEnvPointIndex(Index, Channel);

	return ListIndex >= 0;
}

bool CEditor::IsEnvPointSelected(int Index) const
{
	auto Iter = std::find_if(
		m_vSelectedEnvelopePoints.begin(),
		m_vSelectedEnvelopePoints.end(),
		[&](auto pair) { return pair.first == Index; });

	return Iter != m_vSelectedEnvelopePoints.end();
}

void CEditor::DeselectEnvPoints()
{
	m_vSelectedEnvelopePoints.clear();
	m_SelectedTangentInPoint = std::pair(-1, -1);
	m_SelectedTangentOutPoint = std::pair(-1, -1);
}

void CEditor::SelectTangentOutPoint(int Index, int Channel)
{
	DeselectEnvPoints();
	m_SelectedTangentOutPoint = std::pair(Index, Channel);
}

bool CEditor::IsTangentOutPointSelected(int Index, int Channel) const
{
	return m_SelectedTangentOutPoint == std::pair(Index, Channel);
}

void CEditor::SelectTangentInPoint(int Index, int Channel)
{
	DeselectEnvPoints();
	m_SelectedTangentInPoint = std::pair(Index, Channel);
}

bool CEditor::IsTangentInPointSelected(int Index, int Channel) const
{
	return m_SelectedTangentInPoint == std::pair(Index, Channel);
}

bool CEditor::IsTangentInSelected() const
{
	return m_SelectedTangentInPoint != std::pair(-1, -1);
}

bool CEditor::IsTangentOutSelected() const
{
	return m_SelectedTangentOutPoint != std::pair(-1, -1);
}

bool CEditor::IsTangentSelected() const
{
	return IsTangentInSelected() || IsTangentOutSelected();
}

bool CEditor::CallbackOpenMap(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;
	if(pEditor->Load(pFileName, StorageType))
	{
		pEditor->m_ValidSaveFilename = StorageType == IStorage::TYPE_SAVE && (pEditor->m_pFileDialogPath == pEditor->m_aFileDialogCurrentFolder || (pEditor->m_pFileDialogPath == pEditor->m_aFileDialogCurrentLink && str_comp(pEditor->m_aFileDialogCurrentLink, "themes") == 0));
		pEditor->m_Dialog = DIALOG_NONE;
		return true;
	}
	else
	{
		pEditor->ShowFileDialogError("Failed to load map from file '%s'.", pFileName);
		return false;
	}
}

bool CEditor::CallbackAppendMap(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;
	if(pEditor->Append(pFileName, StorageType))
	{
		pEditor->m_Dialog = DIALOG_NONE;
		return true;
	}
	else
	{
		pEditor->m_aFileName[0] = 0;
		pEditor->ShowFileDialogError("Failed to load map from file '%s'.", pFileName);
		return false;
	}
}

bool CEditor::CallbackSaveMap(const char *pFileName, int StorageType, void *pUser)
{
	dbg_assert(StorageType == IStorage::TYPE_SAVE, "Saving only allowed for IStorage::TYPE_SAVE");

	CEditor *pEditor = static_cast<CEditor *>(pUser);
	char aBuf[IO_MAX_PATH_LENGTH];
	// add map extension
	if(!str_endswith(pFileName, ".map"))
	{
		str_format(aBuf, sizeof(aBuf), "%s.map", pFileName);
		pFileName = aBuf;
	}

	// Save map to specified file
	if(pEditor->Save(pFileName))
	{
		str_copy(pEditor->m_aFileName, pFileName);
		pEditor->m_ValidSaveFilename = true;
		pEditor->m_Map.m_Modified = false;
	}
	else
	{
		pEditor->ShowFileDialogError("Failed to save map to file '%s'.", pFileName);
		return false;
	}

	// Also update autosave if it's older than half the configured autosave interval, so we also have periodic backups.
	const float Time = pEditor->Client()->GlobalTime();
	if(g_Config.m_EdAutosaveInterval > 0 && pEditor->m_Map.m_LastSaveTime < Time && Time - pEditor->m_Map.m_LastSaveTime > 30 * g_Config.m_EdAutosaveInterval)
	{
		if(!pEditor->PerformAutosave())
			return false;
	}

	pEditor->m_Dialog = DIALOG_NONE;
	return true;
}

bool CEditor::CallbackSaveCopyMap(const char *pFileName, int StorageType, void *pUser)
{
	dbg_assert(StorageType == IStorage::TYPE_SAVE, "Saving only allowed for IStorage::TYPE_SAVE");

	CEditor *pEditor = static_cast<CEditor *>(pUser);
	char aBuf[IO_MAX_PATH_LENGTH];
	// add map extension
	if(!str_endswith(pFileName, ".map"))
	{
		str_format(aBuf, sizeof(aBuf), "%s.map", pFileName);
		pFileName = aBuf;
	}

	if(pEditor->Save(pFileName))
	{
		pEditor->m_Dialog = DIALOG_NONE;
		return true;
	}
	else
	{
		pEditor->ShowFileDialogError("Failed to save map to file '%s'.", pFileName);
		return false;
	}
}

void CEditor::DoToolbarLayers(CUIRect ToolBar)
{
	const bool ModPressed = Input()->ModifierIsPressed();
	const bool ShiftPressed = Input()->ShiftIsPressed();

	// handle shortcut for info button
	if(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_I) && ModPressed && !ShiftPressed)
	{
		if(m_ShowTileInfo == SHOW_TILE_HEXADECIMAL)
			m_ShowTileInfo = SHOW_TILE_DECIMAL;
		else if(m_ShowTileInfo != SHOW_TILE_OFF)
			m_ShowTileInfo = SHOW_TILE_OFF;
		else
			m_ShowTileInfo = SHOW_TILE_DECIMAL;
		m_ShowEnvelopePreview = SHOWENV_NONE;
	}

	// handle shortcut for hex button
	if(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_I) && ModPressed && ShiftPressed)
	{
		m_ShowTileInfo = m_ShowTileInfo == SHOW_TILE_HEXADECIMAL ? SHOW_TILE_OFF : SHOW_TILE_HEXADECIMAL;
		m_ShowEnvelopePreview = SHOWENV_NONE;
	}

	// handle shortcut for unused button
	if(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_U) && ModPressed)
		m_AllowPlaceUnusedTiles = !m_AllowPlaceUnusedTiles;

	CUIRect TB_Top, TB_Bottom;
	CUIRect Button;

	ToolBar.HSplitMid(&TB_Top, &TB_Bottom, 5.0f);

	// top line buttons
	{
		// detail button
		TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
		static int s_HqButton = 0;
		if(DoButton_Editor(&s_HqButton, "HD", m_ShowDetail, &Button, 0, "[ctrl+h] Toggle High Detail") ||
			(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_H) && ModPressed))
		{
			m_ShowDetail = !m_ShowDetail;
		}

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		// animation button
		TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
		static int s_AnimateButton = 0;
		if(DoButton_Editor(&s_AnimateButton, "Anim", m_Animate, &Button, 0, "[ctrl+m] Toggle animation") ||
			(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_M) && ModPressed))
		{
			m_AnimateStart = time_get();
			m_Animate = !m_Animate;
		}

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		// proof button
		TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
		static int s_ProofButton = 0;
		if(DoButton_Ex(&s_ProofButton, "Proof", m_ProofBorders != PROOF_BORDER_OFF, &Button, 0, "[ctrl+p] Toggles proof borders. These borders represent what a player maximum can see.", IGraphics::CORNER_L) ||
			(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_P) && ModPressed))
		{
			m_ProofBorders = m_ProofBorders == PROOF_BORDER_OFF ? PROOF_BORDER_INGAME : PROOF_BORDER_OFF;
		}

		TB_Top.VSplitLeft(14.0f, &Button, &TB_Top);
		static int s_ProofModeButton = 0;
		if(DoButton_FontIcon(&s_ProofModeButton, FONT_ICON_CIRCLE_CHEVRON_DOWN, 0, &Button, 0, "Select proof mode.", IGraphics::CORNER_R, 8.0f))
		{
			static SPopupMenuId s_PopupProofModeId;
			UI()->DoPopupMenu(&s_PopupProofModeId, Button.x, Button.y + Button.h, 60.0f, 36.0f, this, PopupProofMode);
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
			(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_G) && ModPressed && !ShiftPressed))
		{
			m_GridActive = !m_GridActive;
		}

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		// zoom group
		TB_Top.VSplitLeft(20.0f, &Button, &TB_Top);
		static int s_ZoomOutButton = 0;
		if(DoButton_FontIcon(&s_ZoomOutButton, "-", 0, &Button, 0, "[NumPad-] Zoom out", IGraphics::CORNER_L))
		{
			MapView()->m_Zoom.ChangeValue(50.0f);
		}

		TB_Top.VSplitLeft(25.0f, &Button, &TB_Top);
		static int s_ZoomNormalButton = 0;
		if(DoButton_FontIcon(&s_ZoomNormalButton, FONT_ICON_MAGNIFYING_GLASS, 0, &Button, 0, "[NumPad*] Zoom to normal and remove editor offset", IGraphics::CORNER_NONE))
		{
			MapView()->ResetZoom();
		}

		TB_Top.VSplitLeft(20.0f, &Button, &TB_Top);
		static int s_ZoomInButton = 0;
		if(DoButton_FontIcon(&s_ZoomInButton, "+", 0, &Button, 0, "[NumPad+] Zoom in", IGraphics::CORNER_R))
		{
			MapView()->m_Zoom.ChangeValue(-50.0f);
		}

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		// brush manipulation
		{
			int Enabled = m_pBrush->IsEmpty() ? -1 : 0;

			// flip buttons
			TB_Top.VSplitLeft(25.0f, &Button, &TB_Top);
			static int s_FlipXButton = 0;
			if(DoButton_FontIcon(&s_FlipXButton, FONT_ICON_ARROWS_LEFT_RIGHT, Enabled, &Button, 0, "[N] Flip brush horizontal", IGraphics::CORNER_L) || (Input()->KeyPress(KEY_N) && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr))
			{
				for(auto &pLayer : m_pBrush->m_vpLayers)
					pLayer->BrushFlipX();
			}

			TB_Top.VSplitLeft(25.0f, &Button, &TB_Top);
			static int s_FlipyButton = 0;
			if(DoButton_FontIcon(&s_FlipyButton, FONT_ICON_ARROWS_UP_DOWN, Enabled, &Button, 0, "[M] Flip brush vertical", IGraphics::CORNER_R) || (Input()->KeyPress(KEY_M) && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr))
			{
				for(auto &pLayer : m_pBrush->m_vpLayers)
					pLayer->BrushFlipY();
			}
			TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

			// rotate buttons
			TB_Top.VSplitLeft(25.0f, &Button, &TB_Top);
			static int s_RotationAmount = 90;
			bool TileLayer = false;
			// check for tile layers in brush selection
			for(auto &pLayer : m_pBrush->m_vpLayers)
				if(pLayer->m_Type == LAYERTYPE_TILES)
				{
					TileLayer = true;
					s_RotationAmount = maximum(90, (s_RotationAmount / 90) * 90);
					break;
				}

			static int s_CcwButton = 0;
			if(DoButton_FontIcon(&s_CcwButton, FONT_ICON_ARROW_ROTATE_LEFT, Enabled, &Button, 0, "[R] Rotates the brush counter clockwise", IGraphics::CORNER_ALL) || (Input()->KeyPress(KEY_R) && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr))
			{
				for(auto &pLayer : m_pBrush->m_vpLayers)
					pLayer->BrushRotate(-s_RotationAmount / 360.0f * pi * 2);
			}

			TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
			s_RotationAmount = UiDoValueSelector(&s_RotationAmount, &Button, "", s_RotationAmount, TileLayer ? 90 : 1, 359, TileLayer ? 90 : 1, TileLayer ? 10.0f : 2.0f, "Rotation of the brush in degrees. Use left mouse button to drag and change the value. Hold shift to be more precise.", true);

			TB_Top.VSplitLeft(25.0f, &Button, &TB_Top);
			static int s_CwButton = 0;
			if(DoButton_FontIcon(&s_CwButton, FONT_ICON_ARROW_ROTATE_RIGHT, Enabled, &Button, 0, "[T] Rotates the brush clockwise", IGraphics::CORNER_ALL) || (Input()->KeyPress(KEY_T) && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr))
			{
				for(auto &pLayer : m_pBrush->m_vpLayers)
					pLayer->BrushRotate(s_RotationAmount / 360.0f * pi * 2);
			}

			TB_Top.VSplitLeft(5.0f, &Button, &TB_Top);
		}

		// animation speed
		if(m_Animate)
		{
			TB_Top.VSplitLeft(20.0f, &Button, &TB_Top);
			static int s_AnimSlowerButton = 0;
			if(DoButton_FontIcon(&s_AnimSlowerButton, "-", 0, &Button, 0, "Decrease animation speed", IGraphics::CORNER_L))
			{
				if(m_AnimateSpeed > 0.5f)
					m_AnimateSpeed -= 0.5f;
			}

			TB_Top.VSplitLeft(25.0f, &Button, &TB_Top);
			static int s_AnimNormalButton = 0;
			if(DoButton_FontIcon(&s_AnimNormalButton, FONT_ICON_CIRCLE_PLAY, 0, &Button, 0, "Normal animation speed", 0))
				m_AnimateSpeed = 1.0f;

			TB_Top.VSplitLeft(20.0f, &Button, &TB_Top);
			static int s_AnimFasterButton = 0;
			if(DoButton_FontIcon(&s_AnimFasterButton, "+", 0, &Button, 0, "Increase animation speed", IGraphics::CORNER_R))
				m_AnimateSpeed += 0.5f;

			TB_Top.VSplitLeft(5.0f, &Button, &TB_Top);
		}

		// grid zoom
		if(m_GridActive)
		{
			TB_Top.VSplitLeft(20.0f, &Button, &TB_Top);
			static int s_GridIncreaseButton = 0;
			if(DoButton_FontIcon(&s_GridIncreaseButton, "-", 0, &Button, 0, "Decrease grid", IGraphics::CORNER_L))
			{
				if(m_GridFactor > 1)
					m_GridFactor--;
			}

			TB_Top.VSplitLeft(25.0f, &Button, &TB_Top);
			static int s_GridNormalButton = 0;
			if(DoButton_FontIcon(&s_GridNormalButton, FONT_ICON_BORDER_ALL, 0, &Button, 0, "Normal grid", IGraphics::CORNER_NONE))
				m_GridFactor = 1;

			TB_Top.VSplitLeft(20.0f, &Button, &TB_Top);
			static int s_GridDecreaseButton = 0;
			if(DoButton_FontIcon(&s_GridDecreaseButton, "+", 0, &Button, 0, "Increase grid", IGraphics::CORNER_R))
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
			int FocusButtonChecked;
			if(m_ProofBorders == PROOF_BORDER_MENU)
			{
				if(MapView()->m_WorldOffset == m_vMenuBackgroundPositions[m_CurrentMenuProofIndex])
					FocusButtonChecked = -1;
				else
					FocusButtonChecked = 1;
			}
			else
			{
				if(MapView()->m_WorldOffset == vec2(0, 0))
					FocusButtonChecked = -1;
				else
					FocusButtonChecked = 1;
			}
			if(DoButton_Editor(&s_RefocusButton, "Refocus", FocusButtonChecked, &Button, 0, "[HOME] Restore map focus") || (m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_HOME)))
			{
				if(m_ProofBorders == PROOF_BORDER_MENU)
					MapView()->m_WorldOffset = m_vMenuBackgroundPositions[m_CurrentMenuProofIndex];
				else
					MapView()->m_WorldOffset = vec2(0, 0);
			}
			TB_Bottom.VSplitLeft(5.0f, nullptr, &TB_Bottom);
		}

		// tile manipulation
		{
			// do tele/tune/switch/speedup button
			{
				std::shared_ptr<CLayerTiles> pS = std::static_pointer_cast<CLayerTiles>(GetSelectedLayerType(0, LAYERTYPE_TILES));
				if(pS)
				{
					const char *pButtonName = nullptr;
					CUI::FPopupMenuFunction pfnPopupFunc = nullptr;
					int Rows = 0;
					if(pS == m_Map.m_pSwitchLayer)
					{
						pButtonName = "Switch";
						pfnPopupFunc = PopupSwitch;
						Rows = 2;
					}
					else if(pS == m_Map.m_pSpeedupLayer)
					{
						pButtonName = "Speedup";
						pfnPopupFunc = PopupSpeedup;
						Rows = 3;
					}
					else if(pS == m_Map.m_pTuneLayer)
					{
						pButtonName = "Tune";
						pfnPopupFunc = PopupTune;
						Rows = 1;
					}
					else if(pS == m_Map.m_pTeleLayer)
					{
						pButtonName = "Tele";
						pfnPopupFunc = PopupTele;
						Rows = 1;
					}

					if(pButtonName != nullptr)
					{
						static char s_aButtonTooltip[64];
						str_format(s_aButtonTooltip, sizeof(s_aButtonTooltip), "[ctrl+t] %s", pButtonName);

						TB_Bottom.VSplitLeft(60.0f, &Button, &TB_Bottom);
						static int s_ModifierButton = 0;
						if(DoButton_Ex(&s_ModifierButton, pButtonName, 0, &Button, 0, s_aButtonTooltip, IGraphics::CORNER_ALL) || (m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && ModPressed && Input()->KeyPress(KEY_T)))
						{
							static SPopupMenuId s_PopupModifierId;
							if(!UI()->IsPopupOpen(&s_PopupModifierId))
							{
								UI()->DoPopupMenu(&s_PopupModifierId, Button.x, Button.y + Button.h, 120, 10.0f + Rows * 13.0f, this, pfnPopupFunc);
							}
						}
						TB_Bottom.VSplitLeft(5.0f, nullptr, &TB_Bottom);
					}
				}
			}
		}

		// do add quad/sound button
		std::shared_ptr<CLayer> pLayer = GetSelectedLayer(0);
		if(pLayer && (pLayer->m_Type == LAYERTYPE_QUADS || pLayer->m_Type == LAYERTYPE_SOUNDS))
		{
			TB_Bottom.VSplitLeft(60.0f, &Button, &TB_Bottom);

			bool Invoked = false;
			static int s_AddItemButton = 0;

			if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				Invoked = DoButton_Editor(&s_AddItemButton, "Add Quad", 0, &Button, 0, "[ctrl+q] Add a new quad") ||
					  (m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_Q) && ModPressed);
			}
			else if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				Invoked = DoButton_Editor(&s_AddItemButton, "Add Sound", 0, &Button, 0, "[ctrl+q] Add a new sound source") ||
					  (m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_Q) && ModPressed);
			}

			if(Invoked)
			{
				std::shared_ptr<CLayerGroup> pGroup = GetSelectedGroup();

				float aMapping[4];
				pGroup->Mapping(aMapping);
				int x = aMapping[0] + (aMapping[2] - aMapping[0]) / 2;
				int y = aMapping[1] + (aMapping[3] - aMapping[1]) / 2;
				if(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_Q) && ModPressed)
				{
					x += UI()->MouseWorldX() - (MapView()->m_WorldOffset.x * pGroup->m_ParallaxX / 100) - pGroup->m_OffsetX;
					y += UI()->MouseWorldY() - (MapView()->m_WorldOffset.y * pGroup->m_ParallaxY / 100) - pGroup->m_OffsetY;
				}

				if(pLayer->m_Type == LAYERTYPE_QUADS)
				{
					std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(pLayer);

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
					std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(pLayer);
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
				(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_D) && ModPressed && !ShiftPressed))
				m_BrushDrawDestructive = !m_BrushDrawDestructive;
			TB_Bottom.VSplitLeft(5.0f, &Button, &TB_Bottom);
		}
	}
}

void CEditor::DoToolbarSounds(CUIRect ToolBar)
{
	CUIRect ToolBarTop, ToolBarBottom, Button;
	ToolBar.HSplitMid(&ToolBarTop, &ToolBarBottom, 5.0f);

	if(m_SelectedSound >= 0 && (size_t)m_SelectedSound < m_Map.m_vpSounds.size())
	{
		const std::shared_ptr<CEditorSound> pSelectedSound = m_Map.m_vpSounds[m_SelectedSound];

		// play/stop button
		{
			ToolBarBottom.VSplitLeft(ToolBarBottom.h, &Button, &ToolBarBottom);
			static int s_PlayStopButton;
			if(DoButton_FontIcon(&s_PlayStopButton, Sound()->IsPlaying(pSelectedSound->m_SoundID) ? FONT_ICON_STOP : FONT_ICON_PLAY, 0, &Button, 0, "Play/stop audio preview", IGraphics::CORNER_ALL) ||
				(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_SPACE)))
			{
				if(Sound()->IsPlaying(pSelectedSound->m_SoundID))
					Sound()->Stop(pSelectedSound->m_SoundID);
				else
					Sound()->Play(CSounds::CHN_GUI, pSelectedSound->m_SoundID, 0);
			}
		}

		// duration
		{
			ToolBarBottom.VSplitLeft(5.0f, nullptr, &ToolBarBottom);
			char aDuration[32];
			char aDurationLabel[64];
			str_time_float(Sound()->GetSampleDuration(pSelectedSound->m_SoundID), TIME_HOURS, aDuration, sizeof(aDuration));
			str_format(aDurationLabel, sizeof(aDurationLabel), "Duration: %s", aDuration);
			UI()->DoLabel(&ToolBarBottom, aDurationLabel, 12.0f, TEXTALIGN_ML);
		}
	}
}

static void Rotate(const CPoint *pCenter, CPoint *pPoint, float Rotation)
{
	int x = pPoint->x - pCenter->x;
	int y = pPoint->y - pCenter->y;
	pPoint->x = (int)(x * std::cos(Rotation) - y * std::sin(Rotation) + pCenter->x);
	pPoint->y = (int)(x * std::sin(Rotation) + y * std::cos(Rotation) + pCenter->y);
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

	const bool IgnoreGrid = Input()->AltIsPressed();

	if(UI()->CheckActiveItem(pID))
	{
		if(m_MouseDeltaWx * m_MouseDeltaWx + m_MouseDeltaWy * m_MouseDeltaWy > 0.0f)
		{
			if(s_Operation == OP_MOVE)
			{
				float x = wx;
				float y = wy;
				if(m_GridActive && !IgnoreGrid)
					SnapToGrid(x, y);
				pSource->m_Position.x = f2fx(x);
				pSource->m_Position.y = f2fx(y);
			}
		}

		if(s_Operation == OP_CONTEXT_MENU)
		{
			if(!UI()->MouseButton(1))
			{
				if(m_vSelectedLayers.size() == 1)
				{
					static SPopupMenuId s_PopupSourceId;
					UI()->DoPopupMenu(&s_PopupSourceId, UI()->MouseX(), UI()->MouseY(), 120, 200, this, PopupSource);
					UI()->DisableMouseLock();
				}
				s_Operation = OP_NONE;
				UI()->SetActiveItem(nullptr);
			}
		}
		else
		{
			if(!UI()->MouseButton(0))
			{
				UI()->DisableMouseLock();
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
		OP_SELECT,
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
	static float s_MouseXStart = 0.0f;
	static float s_MouseYStart = 0.0f;
	static float s_RotateAngle = 0;
	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();

	// get pivot
	float CenterX = fx2f(pQuad->m_aPoints[4].x);
	float CenterY = fx2f(pQuad->m_aPoints[4].y);

	const bool IgnoreGrid = Input()->AltIsPressed();

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
			if(s_Operation == OP_SELECT)
			{
				float x = s_MouseXStart - UI()->MouseX();
				float y = s_MouseYStart - UI()->MouseY();

				if(x * x + y * y > 20.0f)
				{
					if(!IsQuadSelected(Index))
						SelectQuad(Index);

					if(Input()->ModifierIsPressed())
						s_Operation = OP_MOVE_PIVOT;
					else
						s_Operation = OP_MOVE_ALL;
				}
			}

			// check if we only should move pivot
			if(s_Operation == OP_MOVE_PIVOT)
			{
				float x = wx;
				float y = wy;
				if(m_GridActive && !IgnoreGrid)
					SnapToGrid(x, y);

				int OffsetX = f2fx(x) - pQuad->m_aPoints[4].x;
				int OffsetY = f2fx(y) - pQuad->m_aPoints[4].y;

				std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(GetSelectedLayerType(0, LAYERTYPE_QUADS));
				for(auto &Selected : m_vSelectedQuads)
				{
					CQuad *pCurrentQuad = &pLayer->m_vQuads[Selected];
					pCurrentQuad->m_aPoints[4].x += OffsetX;
					pCurrentQuad->m_aPoints[4].y += OffsetY;
				}
			}
			else if(s_Operation == OP_MOVE_ALL)
			{
				// move all points including pivot
				float x = wx;
				float y = wy;
				if(m_GridActive && !IgnoreGrid)
					SnapToGrid(x, y);

				int OffsetX = f2fx(x) - pQuad->m_aPoints[4].x;
				int OffsetY = f2fx(y) - pQuad->m_aPoints[4].y;

				std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(GetSelectedLayerType(0, LAYERTYPE_QUADS));
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
			else if(s_Operation == OP_ROTATE)
			{
				std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(GetSelectedLayerType(0, LAYERTYPE_QUADS));
				for(size_t i = 0; i < m_vSelectedQuads.size(); ++i)
				{
					CQuad *pCurrentQuad = &pLayer->m_vQuads[m_vSelectedQuads[i]];
					for(int v = 0; v < 4; v++)
					{
						pCurrentQuad->m_aPoints[v] = s_vvRotatePoints[i][v];
						Rotate(&pCurrentQuad->m_aPoints[4], &pCurrentQuad->m_aPoints[v], s_RotateAngle);
					}
				}

				if(Input()->ShiftIsPressed())
					s_RotateAngle += (m_MouseDeltaX)*0.0001f;
				else
					s_RotateAngle += (m_MouseDeltaX)*0.002f;
			}
		}

		if(s_Operation == OP_CONTEXT_MENU)
		{
			if(!UI()->MouseButton(1))
			{
				if(m_vSelectedLayers.size() == 1)
				{
					m_SelectedQuadIndex = FindSelectedQuadIndex(Index);

					static SPopupMenuId s_PopupQuadId;
					UI()->DoPopupMenu(&s_PopupQuadId, UI()->MouseX(), UI()->MouseY(), 120, 198, this, PopupQuad);
					UI()->DisableMouseLock();
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
					UI()->DisableMouseLock();
					m_Map.OnModify();
					DeleteSelectedQuads();
				}
				s_Operation = OP_NONE;
				UI()->SetActiveItem(nullptr);
			}
		}
		else if(s_Operation == OP_ROTATE)
		{
			if(UI()->MouseButton(0))
			{
				UI()->DisableMouseLock();
				s_Operation = OP_NONE;
				UI()->SetActiveItem(nullptr);
			}
			else if(UI()->MouseButton(1))
			{
				UI()->DisableMouseLock();
				s_Operation = OP_NONE;
				UI()->SetActiveItem(nullptr);

				// Reset points to old position
				std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(GetSelectedLayerType(0, LAYERTYPE_QUADS));
				for(size_t i = 0; i < m_vSelectedQuads.size(); ++i)
				{
					CQuad *pCurrentQuad = &pLayer->m_vQuads[m_vSelectedQuads[i]];
					for(int v = 0; v < 4; v++)
						pCurrentQuad->m_aPoints[v] = s_vvRotatePoints[i][v];
				}
			}
		}
		else
		{
			if(!UI()->MouseButton(0))
			{
				if(s_Operation == OP_SELECT)
				{
					if(Input()->ShiftIsPressed())
						ToggleSelectQuad(Index);
					else
						SelectQuad(Index);
				}

				UI()->DisableMouseLock();
				s_Operation = OP_NONE;
				UI()->SetActiveItem(nullptr);
			}
		}

		Graphics()->SetColor(1, 1, 1, 1);
	}
	else if(Input()->KeyPress(KEY_R) && !m_vSelectedQuads.empty())
	{
		UI()->EnableMouseLock(pID);
		UI()->SetActiveItem(pID);
		s_Operation = OP_ROTATE;
		s_RotateAngle = 0;

		std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(GetSelectedLayerType(0, LAYERTYPE_QUADS));
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
	else if(UI()->HotItem() == pID)
	{
		ms_pUiGotContext = pID;

		Graphics()->SetColor(1, 1, 1, 1);
		m_pTooltip = "Left mouse button to move. Hold ctrl to move pivot. Hold alt to ignore grid. Hold shift and right click to delete.";

		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pID);

			s_MouseXStart = UI()->MouseX();
			s_MouseYStart = UI()->MouseY();

			s_Operation = OP_SELECT;
		}
		else if(UI()->MouseButton(1))
		{
			if(Input()->ShiftIsPressed())
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

	// Handle copy/paste operations
	static bool s_Pasted = false;
	if(Input()->KeyPress(KEY_C) && Input()->ModifierIsPressed())
	{
		m_vCopyBuffer.clear();
		std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(GetSelectedLayerType(0, LAYERTYPE_QUADS));
		for(int Selected : m_vSelectedQuads)
			m_vCopyBuffer.push_back(pLayer->m_vQuads[Selected]);
	}
	else if(Input()->KeyPress(KEY_V) && Input()->ModifierIsPressed() && !s_Pasted && !m_vCopyBuffer.empty())
	{
		std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(GetSelectedLayerType(0, LAYERTYPE_QUADS));

		std::shared_ptr<CLayerQuads> pLayerQuads = std::make_shared<CLayerQuads>();
		pLayerQuads->m_pEditor = pLayer->m_pEditor;
		pLayerQuads->m_Image = pLayer->m_Image;

		int MinX = m_vCopyBuffer.front().m_aPoints[4].x;
		int MinY = m_vCopyBuffer.front().m_aPoints[4].y;
		for(auto Quad : m_vCopyBuffer)
		{
			MinX = minimum(MinX, Quad.m_aPoints[4].x);
			MinY = minimum(MinY, Quad.m_aPoints[4].y);
		}
		for(auto Quad : m_vCopyBuffer)
		{
			for(auto &Point : Quad.m_aPoints)
			{
				Point.x -= MinX;
				Point.y -= MinY;
			}
			pLayerQuads->m_vQuads.push_back(Quad);
		}

		m_pBrush->Clear();
		m_pBrush->AddLayer(pLayerQuads);

		DeselectQuads();
		DeselectQuadPoints();
	}
	else if(!Input()->ModifierIsPressed())
		s_Pasted = false;

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

	// draw selection background
	if(IsQuadPointSelected(QuadIndex, V))
	{
		Graphics()->SetColor(0, 0, 0, 1);
		IGraphics::CQuadItem QuadItem(px, py, 7.0f * m_MouseWScale, 7.0f * m_MouseWScale);
		Graphics()->QuadsDraw(&QuadItem, 1);
	}

	enum
	{
		OP_NONE = 0,
		OP_SELECT,
		OP_MOVEPOINT,
		OP_MOVEUV,
		OP_CONTEXT_MENU
	};

	static int s_Operation = OP_NONE;
	static float s_MouseXStart = 0.0f;
	static float s_MouseYStart = 0.0f;

	const bool IgnoreGrid = Input()->AltIsPressed();

	if(UI()->CheckActiveItem(pID))
	{
		if(m_MouseDeltaWx * m_MouseDeltaWx + m_MouseDeltaWy * m_MouseDeltaWy > 0.0f)
		{
			if(s_Operation == OP_SELECT)
			{
				float x = s_MouseXStart - UI()->MouseX();
				float y = s_MouseYStart - UI()->MouseY();

				if(x * x + y * y > 20.0f)
				{
					if(!IsQuadPointSelected(QuadIndex, V))
						SelectQuadPoint(QuadIndex, V);

					if(Input()->ShiftIsPressed())
					{
						s_Operation = OP_MOVEUV;
						UI()->EnableMouseLock(pID);
					}
					else
						s_Operation = OP_MOVEPOINT;
				}
			}

			if(s_Operation == OP_MOVEPOINT)
			{
				float x = wx;
				float y = wy;
				if(m_GridActive && !IgnoreGrid)
					SnapToGrid(x, y);

				int OffsetX = f2fx(x) - pQuad->m_aPoints[V].x;
				int OffsetY = f2fx(y) - pQuad->m_aPoints[V].y;

				for(auto [pSelectedQuad, SelectedPoints] : GetSelectedQuadPoints())
				{
					for(int m = 0; m < 4; m++)
					{
						if(SelectedPoints & (1 << m))
						{
							pSelectedQuad->m_aPoints[m].x += OffsetX;
							pSelectedQuad->m_aPoints[m].y += OffsetY;
						}
					}
				}
			}
			else if(s_Operation == OP_MOVEUV)
			{
				for(auto [pSelectedQuad, SelectedPoints] : GetSelectedQuadPoints())
				{
					for(int m = 0; m < 4; m++)
						if(SelectedPoints & (1 << m))
						{
							// 0,2;1,3 - line x
							// 0,1;2,3 - line y

							pSelectedQuad->m_aTexcoords[m].x += f2fx(m_MouseDeltaWx * 0.001f);
							pSelectedQuad->m_aTexcoords[(m + 2) % 4].x += f2fx(m_MouseDeltaWx * 0.001f);

							pSelectedQuad->m_aTexcoords[m].y += f2fx(m_MouseDeltaWy * 0.001f);
							pSelectedQuad->m_aTexcoords[m ^ 1].y += f2fx(m_MouseDeltaWy * 0.001f);
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
					m_SelectedQuadIndex = FindSelectedQuadPointIndex(QuadIndex);

					static SPopupMenuId s_PopupPointId;
					UI()->DoPopupMenu(&s_PopupPointId, UI()->MouseX(), UI()->MouseY(), 120, 75, this, PopupPoint);
				}
				UI()->SetActiveItem(nullptr);
			}
		}
		else
		{
			if(!UI()->MouseButton(0))
			{
				if(s_Operation == OP_SELECT)
				{
					if(Input()->ShiftIsPressed())
						ToggleSelectQuadPoint(QuadIndex, V);
					else
						SelectQuadPoint(QuadIndex, V);
				}

				UI()->DisableMouseLock();
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

			s_MouseXStart = UI()->MouseX();
			s_MouseYStart = UI()->MouseY();

			s_Operation = OP_SELECT;
		}
		else if(UI()->MouseButton(1))
		{
			s_Operation = OP_CONTEXT_MENU;

			UI()->SetActiveItem(pID);

			if(!IsQuadPointSelected(QuadIndex, V))
				SelectQuadPoint(QuadIndex, V);
		}
	}
	else
		Graphics()->SetColor(1, 0, 0, 1);

	IGraphics::CQuadItem QuadItem(px, py, 5.0f * m_MouseWScale, 5.0f * m_MouseWScale);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

float CEditor::TriangleArea(vec2 A, vec2 B, vec2 C)
{
	return absolute(((B.x - A.x) * (C.y - A.y) - (C.x - A.x) * (B.y - A.y)) * 0.5f);
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
	return Area > 0.f && absolute(TriangleArea(Point, A, B) + TriangleArea(Point, B, C) + TriangleArea(Point, C, A) - Area) < 0.000001f;
}

void CEditor::DoQuadKnife(int QuadIndex)
{
	std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(GetSelectedLayerType(0, LAYERTYPE_QUADS));
	CQuad *pQuad = &pLayer->m_vQuads[QuadIndex];

	const bool IgnoreGrid = Input()->AltIsPressed();
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
		vec2 OnGrid = vec2(std::round(Mouse.x / CellSize) * CellSize, std::round(Mouse.y / CellSize) * CellSize);

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
					float Distance = absolute(OnGrid.x - OnEdge.x);

					if(Distance < CellSize && (Distance < MinDistance || MinDistance < 0.f))
					{
						MinDistance = Distance;
						Point = OnEdge;
					}
				}

				if(in_range(OnGrid.x, Min.x, Max.x) && Max.x - Min.x > 0.0000001f)
				{
					vec2 OnEdge(OnGrid.x, v[i].y + (OnGrid.x - v[i].x) / (v[j].x - v[i].x) * (v[j].y - v[i].y));
					float Distance = absolute(OnGrid.y - OnEdge.y);

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

			pResult->m_aColors[i].r = (int)std::round(pQuad->m_aColors[0].r * WeightA + pQuad->m_aColors[3].r * WeightB + pQuad->m_aColors[t].r * WeightC);
			pResult->m_aColors[i].g = (int)std::round(pQuad->m_aColors[0].g * WeightA + pQuad->m_aColors[3].g * WeightB + pQuad->m_aColors[t].g * WeightC);
			pResult->m_aColors[i].b = (int)std::round(pQuad->m_aColors[0].b * WeightA + pQuad->m_aColors[3].b * WeightB + pQuad->m_aColors[t].b * WeightC);
			pResult->m_aColors[i].a = (int)std::round(pQuad->m_aColors[0].a * WeightA + pQuad->m_aColors[3].a * WeightB + pQuad->m_aColors[t].a * WeightC);

			pResult->m_aTexcoords[i].x = (int)std::round(pQuad->m_aTexcoords[0].x * WeightA + pQuad->m_aTexcoords[3].x * WeightB + pQuad->m_aTexcoords[t].x * WeightC);
			pResult->m_aTexcoords[i].y = (int)std::round(pQuad->m_aTexcoords[0].y * WeightA + pQuad->m_aTexcoords[3].y * WeightB + pQuad->m_aTexcoords[t].y * WeightC);

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
	std::shared_ptr<CEnvelope> *apEnvelope = new std::shared_ptr<CEnvelope>[Num];
	for(size_t i = 0; i < Num; i++)
		apEnvelope[i] = nullptr;

	for(size_t i = 0; i < Num; i++)
	{
		if((m_ShowEnvelopePreview == SHOWENV_SELECTED && vQuads[i].m_PosEnv == m_SelectedEnvelope) || m_ShowEnvelopePreview == SHOWENV_ALL)
			if(vQuads[i].m_PosEnv >= 0 && vQuads[i].m_PosEnv < (int)m_Map.m_vpEnvelopes.size())
				apEnvelope[i] = m_Map.m_vpEnvelopes[vQuads[i].m_PosEnv];
	}

	// Draw Lines
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(80.0f / 255, 150.0f / 255, 230.f / 255, 0.5f);
	for(size_t j = 0; j < Num; j++)
	{
		if(!apEnvelope[j])
			continue;

		// QuadParams
		const CPoint *pPivotPoint = &vQuads[j].m_aPoints[4];
		for(size_t i = 0; i < apEnvelope[j]->m_vPoints.size() - 1; i++)
		{
			ColorRGBA Result;
			apEnvelope[j]->Eval(apEnvelope[j]->m_vPoints[i].m_Time / 1000.0f + 0.000001f, Result);
			vec2 Pos0 = vec2(fx2f(pPivotPoint->x) + Result.r, fx2f(pPivotPoint->y) + Result.g);

			const int Steps = 15;
			for(int n = 1; n <= Steps; n++)
			{
				const float Time = mix(apEnvelope[j]->m_vPoints[i].m_Time, apEnvelope[j]->m_vPoints[i + 1].m_Time, (float)n / Steps);
				apEnvelope[j]->Eval(Time / 1000.0f - 0.000001f, Result);

				vec2 Pos1 = vec2(fx2f(pPivotPoint->x) + Result.r, fx2f(pPivotPoint->y) + Result.g);

				IGraphics::CLineItem Line = IGraphics::CLineItem(Pos0.x, Pos0.y, Pos1.x, Pos1.y);
				Graphics()->LinesDraw(&Line, 1);

				Pos0 = Pos1;
			}
		}
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->LinesEnd();

	// Draw Quads
	Graphics()->TextureSet(Texture);
	Graphics()->QuadsBegin();

	for(size_t j = 0; j < Num; j++)
	{
		if(!apEnvelope[j])
			continue;

		// QuadParams
		const CPoint *pPoints = vQuads[j].m_aPoints;

		for(size_t i = 0; i < apEnvelope[j]->m_vPoints.size(); i++)
		{
			// Calc Env Position
			float OffsetX = fx2f(apEnvelope[j]->m_vPoints[i].m_aValues[0]);
			float OffsetY = fx2f(apEnvelope[j]->m_vPoints[i].m_aValues[1]);
			float Rot = fx2f(apEnvelope[j]->m_vPoints[i].m_aValues[2]) / 360.0f * pi * 2;

			// Set Colours
			float Alpha = (m_SelectedQuadEnvelope == vQuads[j].m_PosEnv && IsEnvPointSelected(i)) ? 0.65f : 0.35f;
			IGraphics::CColorVertex aArray[4] = {
				IGraphics::CColorVertex(0, vQuads[j].m_aColors[0].r, vQuads[j].m_aColors[0].g, vQuads[j].m_aColors[0].b, Alpha),
				IGraphics::CColorVertex(1, vQuads[j].m_aColors[1].r, vQuads[j].m_aColors[1].g, vQuads[j].m_aColors[1].b, Alpha),
				IGraphics::CColorVertex(2, vQuads[j].m_aColors[2].r, vQuads[j].m_aColors[2].g, vQuads[j].m_aColors[2].b, Alpha),
				IGraphics::CColorVertex(3, vQuads[j].m_aColors[3].r, vQuads[j].m_aColors[3].g, vQuads[j].m_aColors[3].b, Alpha)};
			Graphics()->SetColorVertex(aArray, 4);

			// Rotation
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

			// Set Texture Coords
			Graphics()->QuadsSetSubsetFree(
				fx2f(vQuads[j].m_aTexcoords[0].x), fx2f(vQuads[j].m_aTexcoords[0].y),
				fx2f(vQuads[j].m_aTexcoords[1].x), fx2f(vQuads[j].m_aTexcoords[1].y),
				fx2f(vQuads[j].m_aTexcoords[2].x), fx2f(vQuads[j].m_aTexcoords[2].y),
				fx2f(vQuads[j].m_aTexcoords[3].x), fx2f(vQuads[j].m_aTexcoords[3].y));

			// Set Quad Coords & Draw
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
	std::shared_ptr<CEnvelope> pEnvelope = m_Map.m_vpEnvelopes[pQuad->m_PosEnv];
	void *pID = &pEnvelope->m_vPoints[PIndex];

	// get pivot
	float CenterX = fx2f(pQuad->m_aPoints[4].x) + fx2f(pEnvelope->m_vPoints[PIndex].m_aValues[0]);
	float CenterY = fx2f(pQuad->m_aPoints[4].y) + fx2f(pEnvelope->m_vPoints[PIndex].m_aValues[1]);

	const bool IgnoreGrid = Input()->AltIsPressed();

	if(UI()->CheckActiveItem(pID) && m_CurrentQuadIndex == QIndex)
	{
		if(s_Operation == OP_MOVE)
		{
			if(m_GridActive && !IgnoreGrid)
			{
				float x = wx;
				float y = wy;
				SnapToGrid(x, y);
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
			UI()->DisableMouseLock();
			s_Operation = OP_NONE;
			UI()->SetActiveItem(nullptr);
		}

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	else if(UI()->HotItem() == pID && m_CurrentQuadIndex == QIndex)
	{
		ms_pUiGotContext = pID;

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		m_pTooltip = "Left mouse button to move. Hold ctrl to rotate. Hold alt to ignore grid.";

		if(UI()->MouseButton(0))
		{
			if(Input()->ModifierIsPressed())
			{
				UI()->EnableMouseLock(pID);
				s_Operation = OP_ROTATE;

				SelectQuad(QIndex);
			}
			else
			{
				s_Operation = OP_MOVE;

				SelectQuad(QIndex);
			}

			SelectEnvPoint(PIndex);
			m_SelectedQuadEnvelope = pQuad->m_PosEnv;

			UI()->SetActiveItem(pID);

			s_LastWx = wx;
			s_LastWy = wy;
		}
		else
		{
			DeselectEnvPoints();
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
		if(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->ShiftIsPressed() && !Input()->ModifierIsPressed() && Input()->KeyPress(KEY_G))
		{
			const bool AnyHidden =
				!m_Map.m_pGameLayer->m_Visible ||
				(m_Map.m_pFrontLayer && !m_Map.m_pFrontLayer->m_Visible) ||
				(m_Map.m_pTeleLayer && !m_Map.m_pTeleLayer->m_Visible) ||
				(m_Map.m_pSpeedupLayer && !m_Map.m_pSpeedupLayer->m_Visible) ||
				(m_Map.m_pTuneLayer && !m_Map.m_pTuneLayer->m_Visible) ||
				(m_Map.m_pSwitchLayer && !m_Map.m_pSwitchLayer->m_Visible);
			m_Map.m_pGameLayer->m_Visible = AnyHidden;
			if(m_Map.m_pFrontLayer)
				m_Map.m_pFrontLayer->m_Visible = AnyHidden;
			if(m_Map.m_pTeleLayer)
				m_Map.m_pTeleLayer->m_Visible = AnyHidden;
			if(m_Map.m_pSpeedupLayer)
				m_Map.m_pSpeedupLayer->m_Visible = AnyHidden;
			if(m_Map.m_pTuneLayer)
				m_Map.m_pTuneLayer->m_Visible = AnyHidden;
			if(m_Map.m_pSwitchLayer)
				m_Map.m_pSwitchLayer->m_Visible = AnyHidden;
		}

		for(auto &pGroup : m_Map.m_vpGroups)
		{
			if(pGroup->m_Visible)
				pGroup->Render();
		}

		// render the game, tele, speedup, front, tune and switch above everything else
		if(m_Map.m_pGameGroup->m_Visible)
		{
			m_Map.m_pGameGroup->MapScreen();
			for(auto &pLayer : m_Map.m_pGameGroup->m_vpLayers)
			{
				if(pLayer->m_Visible && pLayer->IsEntitiesLayer())
					pLayer->Render();
			}
		}

		std::shared_ptr<CLayerTiles> pT = std::static_pointer_cast<CLayerTiles>(GetSelectedLayerType(0, LAYERTYPE_TILES));
		if(m_ShowTileInfo != SHOW_TILE_OFF && pT && pT->m_Visible && MapView()->m_Zoom.GetValue() <= 300.0f)
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
	const bool Inside = !m_GuiActive || UI()->MouseInside(&View);

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
		std::shared_ptr<CLayerTiles> pTileLayer = std::static_pointer_cast<CLayerTiles>(GetSelectedLayerType(0, LAYERTYPE_TILES));
		if(pTileLayer)
		{
			Graphics()->MapScreen(x, y, x + w, y + h);
			m_pTilesetPicker->m_Image = pTileLayer->m_Image;
			if(m_BrushColorEnabled)
			{
				m_pTilesetPicker->m_Color = pTileLayer->m_Color;
				m_pTilesetPicker->m_Color.a = 255;
			}
			else
			{
				m_pTilesetPicker->m_Color = {255, 255, 255, 255};
			}

			m_pTilesetPicker->m_Game = pTileLayer->m_Game;
			m_pTilesetPicker->m_Tele = pTileLayer->m_Tele;
			m_pTilesetPicker->m_Speedup = pTileLayer->m_Speedup;
			m_pTilesetPicker->m_Front = pTileLayer->m_Front;
			m_pTilesetPicker->m_Switch = pTileLayer->m_Switch;
			m_pTilesetPicker->m_Tune = pTileLayer->m_Tune;

			m_pTilesetPicker->Render(true);

			if(m_ShowTileInfo != SHOW_TILE_OFF)
				m_pTilesetPicker->ShowInfo();
		}
		else
		{
			std::shared_ptr<CLayerQuads> pQuadLayer = std::static_pointer_cast<CLayerQuads>(GetSelectedLayerType(0, LAYERTYPE_QUADS));
			if(pQuadLayer)
			{
				m_pQuadsetPicker->m_Image = pQuadLayer->m_Image;
				m_pQuadsetPicker->m_vQuads[0].m_aPoints[0].x = f2fx(View.x);
				m_pQuadsetPicker->m_vQuads[0].m_aPoints[0].y = f2fx(View.y);
				m_pQuadsetPicker->m_vQuads[0].m_aPoints[1].x = f2fx((View.x + View.w));
				m_pQuadsetPicker->m_vQuads[0].m_aPoints[1].y = f2fx(View.y);
				m_pQuadsetPicker->m_vQuads[0].m_aPoints[2].x = f2fx(View.x);
				m_pQuadsetPicker->m_vQuads[0].m_aPoints[2].y = f2fx((View.y + View.h));
				m_pQuadsetPicker->m_vQuads[0].m_aPoints[3].x = f2fx((View.x + View.w));
				m_pQuadsetPicker->m_vQuads[0].m_aPoints[3].y = f2fx((View.y + View.h));
				m_pQuadsetPicker->m_vQuads[0].m_aPoints[4].x = f2fx((View.x + View.w / 2));
				m_pQuadsetPicker->m_vQuads[0].m_aPoints[4].y = f2fx((View.y + View.h / 2));
				m_pQuadsetPicker->Render();
			}
		}
	}

	static int s_Operation = OP_NONE;

	// draw layer borders
	std::shared_ptr<CLayer> apEditLayers[128];
	size_t NumEditLayers = 0;

	if(m_ShowPicker && GetSelectedLayer(0) && GetSelectedLayer(0)->m_Type == LAYERTYPE_TILES)
	{
		apEditLayers[0] = m_pTilesetPicker;
		NumEditLayers++;
	}
	else if(m_ShowPicker)
	{
		apEditLayers[0] = m_pQuadsetPicker;
		NumEditLayers++;
	}
	else
	{
		// pick a type of layers to edit, preferring Tiles layers.
		int EditingType = -1;
		for(size_t i = 0; i < m_vSelectedLayers.size(); i++)
		{
			std::shared_ptr<CLayer> pLayer = GetSelectedLayer(i);
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

		std::shared_ptr<CLayerGroup> pGroup = GetSelectedGroup();
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
				if(Input()->ShiftIsPressed())
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
			bool IsQuadLayerSelected = GetSelectedLayerType(0, LAYERTYPE_QUADS) != nullptr;
			if(m_ShowPicker)
			{
				std::shared_ptr<CLayer> pLayer = GetSelectedLayer(0);
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
			else if(m_pBrush->IsEmpty() && IsQuadLayerSelected)
				m_pTooltip = "Hold shift to select multiple quads. Use ctrl + c and ctrl + v to copy and paste quads. Press R to rotate selected quads.";
			else if(m_pBrush->IsEmpty())
				m_pTooltip = "Use left mouse button to drag and create a brush. Use ctrl+right mouse to select layer.";
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
					if(!m_pBrush->IsEmpty())
					{
						// draw with brush
						for(size_t k = 0; k < NumEditLayers; k++)
						{
							size_t BrushIndex = k % m_pBrush->m_vpLayers.size();
							if(apEditLayers[k]->m_Type == m_pBrush->m_vpLayers[BrushIndex]->m_Type)
							{
								if(apEditLayers[k]->m_Type == LAYERTYPE_TILES)
								{
									std::shared_ptr<CLayerTiles> pLayer = std::static_pointer_cast<CLayerTiles>(apEditLayers[k]);
									std::shared_ptr<CLayerTiles> pBrushLayer = std::static_pointer_cast<CLayerTiles>(m_pBrush->m_vpLayers[BrushIndex]);

									if(pLayer->m_Tele <= pBrushLayer->m_Tele && pLayer->m_Speedup <= pBrushLayer->m_Speedup && pLayer->m_Front <= pBrushLayer->m_Front && pLayer->m_Game <= pBrushLayer->m_Game && pLayer->m_Switch <= pBrushLayer->m_Switch && pLayer->m_Tune <= pBrushLayer->m_Tune)
										pLayer->BrushDraw(pBrushLayer, wx, wy);
								}
								else
								{
									apEditLayers[k]->BrushDraw(m_pBrush->m_vpLayers[BrushIndex], wx, wy);
								}
							}
						}
					}
				}
				else if(s_Operation == OP_BRUSH_GRAB)
				{
					if(!UI()->MouseButton(0))
					{
						std::shared_ptr<CLayerQuads> pQuadLayer = std::static_pointer_cast<CLayerQuads>(GetSelectedLayerType(0, LAYERTYPE_QUADS));
						if(pQuadLayer)
						{
							if(!Input()->ShiftIsPressed())
							{
								DeselectQuads();
								DeselectQuadPoints();
							}

							for(size_t i = 0; i < pQuadLayer->m_vQuads.size(); i++)
							{
								CQuad *pQuad = &pQuadLayer->m_vQuads[i];

								for(size_t v = 0; v < 5; v++)
								{
									float px = fx2f(pQuad->m_aPoints[v].x);
									float py = fx2f(pQuad->m_aPoints[v].y);

									if(px > r.x && px < r.x + r.w && py > r.y && py < r.y + r.h)
									{
										if(v == 4)
											ToggleSelectQuad(i);
										else
											ToggleSelectQuadPoint(i, v);
									}
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
								Grabs += apEditLayers[k]->BrushGrab(m_pBrush, r);
							if(Grabs == 0)
								m_pBrush->Clear();

							DeselectQuads();
							DeselectQuadPoints();
						}
					}
					else
					{
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
							if(m_pBrush->m_vpLayers.size() != NumEditLayers)
								BrushIndex = 0;
							std::shared_ptr<CLayer> pBrush = m_pBrush->IsEmpty() ? nullptr : m_pBrush->m_vpLayers[BrushIndex];
							apEditLayers[k]->FillSelection(m_pBrush->IsEmpty(), pBrush, r);
						}
					}
					else
					{
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
					m_pBrush->Clear();
				}

				if(UI()->MouseButton(0) && s_Operation == OP_NONE && !m_QuadKnifeActive)
				{
					UI()->SetActiveItem(s_pEditorID);

					if(m_pBrush->IsEmpty())
						s_Operation = OP_BRUSH_GRAB;
					else
					{
						s_Operation = OP_BRUSH_DRAW;
						for(size_t k = 0; k < NumEditLayers; k++)
						{
							size_t BrushIndex = k;
							if(m_pBrush->m_vpLayers.size() != NumEditLayers)
								BrushIndex = 0;
							if(apEditLayers[k]->m_Type == m_pBrush->m_vpLayers[BrushIndex]->m_Type)
								apEditLayers[k]->BrushPlace(m_pBrush->m_vpLayers[BrushIndex], wx, wy);
						}
					}

					std::shared_ptr<CLayerTiles> pLayer = std::static_pointer_cast<CLayerTiles>(GetSelectedLayerType(0, LAYERTYPE_TILES));
					if(Input()->ShiftIsPressed() && pLayer)
						s_Operation = OP_BRUSH_PAINT;
				}

				if(!m_pBrush->IsEmpty())
				{
					m_pBrush->m_OffsetX = -(int)wx;
					m_pBrush->m_OffsetY = -(int)wy;
					for(const auto &pLayer : m_pBrush->m_vpLayers)
					{
						if(pLayer->m_Type == LAYERTYPE_TILES)
						{
							m_pBrush->m_OffsetX = -(int)(wx / 32.0f) * 32;
							m_pBrush->m_OffsetY = -(int)(wy / 32.0f) * 32;
							break;
						}
					}

					std::shared_ptr<CLayerGroup> pGroup = GetSelectedGroup();
					if(!m_ShowPicker && pGroup)
					{
						m_pBrush->m_OffsetX += pGroup->m_OffsetX;
						m_pBrush->m_OffsetY += pGroup->m_OffsetY;
						m_pBrush->m_ParallaxX = pGroup->m_ParallaxX;
						m_pBrush->m_ParallaxY = pGroup->m_ParallaxY;
						m_pBrush->m_ParallaxZoom = pGroup->m_ParallaxZoom;
						m_pBrush->Render();
						float w, h;
						m_pBrush->GetSize(&w, &h);

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
			if(!m_ShowPicker && m_pBrush->IsEmpty())
			{
				// fetch layers
				std::shared_ptr<CLayerGroup> pGroup = GetSelectedGroup();
				if(pGroup)
					pGroup->MapScreen();

				for(size_t k = 0; k < NumEditLayers; k++)
				{
					if(apEditLayers[k]->m_Type == LAYERTYPE_QUADS)
					{
						std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(apEditLayers[k]);

						if(m_ShowEnvelopePreview == SHOWENV_NONE)
							m_ShowEnvelopePreview = SHOWENV_ALL;

						if(m_QuadKnifeActive)
							DoQuadKnife(m_vSelectedQuads[m_SelectedQuadIndex]);
						else
						{
							SetHotQuadPoint(pLayer);

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
						std::shared_ptr<CLayerSounds> pLayer = std::static_pointer_cast<CLayerSounds>(apEditLayers[k]);

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

		// menu proof selection
		if(m_ProofBorders == PROOF_BORDER_MENU && !m_ShowPicker)
		{
			ResetMenuBackgroundPositions();
			for(int i = 0; i < (int)m_vMenuBackgroundPositions.size(); i++)
			{
				vec2 Pos = m_vMenuBackgroundPositions[i];
				Pos += MapView()->m_WorldOffset - m_vMenuBackgroundPositions[m_CurrentMenuProofIndex];
				Pos.y -= 3.0f;

				vec2 MousePos(m_MouseWorldNoParaX, m_MouseWorldNoParaY);
				if(distance(Pos, MousePos) <= 20.0f)
				{
					UI()->SetHotItem(&m_vMenuBackgroundPositions[i]);

					if(i != m_CurrentMenuProofIndex && UI()->CheckActiveItem(&m_vMenuBackgroundPositions[i]))
					{
						if(!UI()->MouseButton(0))
						{
							m_CurrentMenuProofIndex = i;
							MapView()->m_WorldOffset = m_vMenuBackgroundPositions[i];
							UI()->SetActiveItem(nullptr);
						}
					}
					else if(UI()->HotItem() == &m_vMenuBackgroundPositions[i])
					{
						char aTooltipPrefix[32] = "Switch proof position to";
						if(i == m_CurrentMenuProofIndex)
							str_copy(aTooltipPrefix, "Current proof position at");

						char aNumBuf[8];
						if(i < (TILE_TIME_CHECKPOINT_LAST - TILE_TIME_CHECKPOINT_FIRST))
							str_format(aNumBuf, sizeof(aNumBuf), "#%d", i + 1);
						else
							aNumBuf[0] = '\0';

						char aTooltipPositions[128];
						str_format(aTooltipPositions, sizeof(aTooltipPositions), "%s %s", m_vpMenuBackgroundPositionNames[i], aNumBuf);

						for(int k : m_vMenuBackgroundCollisions.at(i))
						{
							if(k == m_CurrentMenuProofIndex)
								str_copy(aTooltipPrefix, "Current proof position at");

							Pos = m_vMenuBackgroundPositions[k];
							Pos += MapView()->m_WorldOffset - m_vMenuBackgroundPositions[m_CurrentMenuProofIndex];
							Pos.y -= 3.0f;

							MousePos = vec2(m_MouseWorldNoParaX, m_MouseWorldNoParaY);
							if(distance(Pos, MousePos) > 20.0f)
								continue;

							if(i < (TILE_TIME_CHECKPOINT_LAST - TILE_TIME_CHECKPOINT_FIRST))
								str_format(aNumBuf, sizeof(aNumBuf), "#%d", k + 1);
							else
								aNumBuf[0] = '\0';

							char aTooltipPositionsCopy[128];
							str_copy(aTooltipPositionsCopy, aTooltipPositions);
							str_format(aTooltipPositions, sizeof(aTooltipPositions), "%s, %s %s", aTooltipPositionsCopy, m_vpMenuBackgroundPositionNames[k], aNumBuf);
						}
						str_format(m_aMenuBackgroundTooltip, sizeof(m_aMenuBackgroundTooltip), "%s %s", aTooltipPrefix, aTooltipPositions);

						m_pTooltip = m_aMenuBackgroundTooltip;
						if(UI()->MouseButton(0))
							UI()->SetActiveItem(&m_vMenuBackgroundPositions[i]);
					}
					break;
				}
			}
		}

		// do panning
		if(UI()->CheckActiveItem(s_pEditorID))
		{
			if(s_Operation == OP_PAN_WORLD)
				MapView()->m_WorldOffset -= vec2(m_MouseDeltaX, m_MouseDeltaY) * m_MouseWScale;
			else if(s_Operation == OP_PAN_EDITOR)
				MapView()->m_EditorOffset -= vec2(m_MouseDeltaX, m_MouseDeltaY) * m_MouseWScale;

			// release mouse
			if(!UI()->MouseButton(0))
			{
				s_Operation = OP_NONE;
				UI()->SetActiveItem(nullptr);
			}
		}
		if(!Input()->ShiftIsPressed() && !Input()->ModifierIsPressed() && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr)
		{
			float PanSpeed = 64.0f;
			if(Input()->KeyPress(KEY_A))
				MapView()->m_WorldOffset.x -= PanSpeed * m_MouseWScale;
			else if(Input()->KeyPress(KEY_D))
				MapView()->m_WorldOffset.x += PanSpeed * m_MouseWScale;
			if(Input()->KeyPress(KEY_W))
				MapView()->m_WorldOffset.y -= PanSpeed * m_MouseWScale;
			else if(Input()->KeyPress(KEY_S))
				MapView()->m_WorldOffset.y += PanSpeed * m_MouseWScale;
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
		std::shared_ptr<CLayerGroup> pGameGroup = m_Map.m_pGameGroup;
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
	if(m_ProofBorders != PROOF_BORDER_OFF && !m_ShowPicker)
	{
		std::shared_ptr<CLayerGroup> pGameGroup = m_Map.m_pGameGroup;
		pGameGroup->MapScreen();

		Graphics()->TextureClear();
		Graphics()->LinesBegin();

		// possible screen sizes (white border)
		float aLastPoints[4];
		float Start = 1.0f; // 9.0f/16.0f;
		float End = 16.0f / 9.0f;
		const int NumSteps = 20;
		for(int i = 0; i <= NumSteps; i++)
		{
			float aPoints[4];
			float Aspect = Start + (End - Start) * (i / (float)NumSteps);

			float Zoom = (m_ProofBorders == PROOF_BORDER_MENU) ? 0.7f : 1.0f;
			RenderTools()->MapScreenToWorld(
				MapView()->m_WorldOffset.x, MapView()->m_WorldOffset.y,
				100.0f, 100.0f, 100.0f, 0.0f, 0.0f, Aspect, Zoom, aPoints);

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
				const float aAspects[] = {4.0f / 3.0f, 16.0f / 10.0f, 5.0f / 4.0f, 16.0f / 9.0f};
				float Aspect = aAspects[i];

				float Zoom = (m_ProofBorders == PROOF_BORDER_MENU) ? 0.7f : 1.0f;
				RenderTools()->MapScreenToWorld(
					MapView()->m_WorldOffset.x, MapView()->m_WorldOffset.y,
					100.0f, 100.0f, 100.0f, 0.0f, 0.0f, Aspect, Zoom, aPoints);

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

		// tee position (blue circle) and other screen positions
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0, 0, 1, 0.3f);
			Graphics()->DrawCircle(MapView()->m_WorldOffset.x, MapView()->m_WorldOffset.y - 3.0f, 20.0f, 32);

			if(m_ProofBorders == PROOF_BORDER_MENU)
			{
				Graphics()->SetColor(0, 1, 0, 0.3f);

				std::set<int> indices;
				for(int i = 0; i < (int)m_vMenuBackgroundPositions.size(); i++)
					indices.insert(i);

				while(!indices.empty())
				{
					int i = *indices.begin();
					indices.erase(i);
					for(int k : m_vMenuBackgroundCollisions.at(i))
						indices.erase(k);

					vec2 Pos = m_vMenuBackgroundPositions[i];
					Pos += MapView()->m_WorldOffset - m_vMenuBackgroundPositions[m_CurrentMenuProofIndex];

					if(Pos == MapView()->m_WorldOffset)
						continue;

					Graphics()->DrawCircle(Pos.x, Pos.y - 3.0f, 20.0f, 32);
				}
			}

			Graphics()->QuadsEnd();
		}
	}

	if(!m_ShowPicker && m_ShowTileInfo != SHOW_TILE_OFF && m_ShowEnvelopePreview != SHOWENV_NONE && GetSelectedLayer(0) && GetSelectedLayer(0)->m_Type == LAYERTYPE_QUADS)
	{
		GetSelectedGroup()->MapScreen();

		std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(GetSelectedLayer(0));
		IGraphics::CTextureHandle Texture;
		if(pLayer->m_Image >= 0 && pLayer->m_Image < (int)m_Map.m_vpImages.size())
			Texture = m_Map.m_vpImages[pLayer->m_Image]->m_Texture;

		DoQuadEnvelopes(pLayer->m_vQuads, Texture);
		m_ShowEnvelopePreview = SHOWENV_NONE;
	}

	UI()->MapScreen();
}

void CEditor::SetHotQuadPoint(const std::shared_ptr<CLayerQuads> &pLayer)
{
	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();

	float MinDist = 500.0f;
	void *pMinPoint = nullptr;

	auto UpdateMinimum = [&](float px, float py, void *pID) {
		float dx = (px - wx) / m_MouseWScale;
		float dy = (py - wy) / m_MouseWScale;

		float CurrDist = dx * dx + dy * dy;
		if(CurrDist < MinDist)
		{
			MinDist = CurrDist;
			pMinPoint = pID;
			return true;
		}
		return false;
	};

	for(size_t i = 0; i < pLayer->m_vQuads.size(); i++)
	{
		CQuad &Quad = pLayer->m_vQuads.at(i);

		if(m_ShowTileInfo != SHOW_TILE_OFF && m_ShowEnvelopePreview != SHOWENV_NONE && Quad.m_PosEnv >= 0)
		{
			for(auto &EnvPoint : m_Map.m_vpEnvelopes[Quad.m_PosEnv]->m_vPoints)
			{
				float px = fx2f(Quad.m_aPoints[4].x) + fx2f(EnvPoint.m_aValues[0]);
				float py = fx2f(Quad.m_aPoints[4].y) + fx2f(EnvPoint.m_aValues[1]);
				if(UpdateMinimum(px, py, &EnvPoint))
					m_CurrentQuadIndex = i;
			}
		}

		for(auto &Point : Quad.m_aPoints)
			UpdateMinimum(fx2f(Point.x), fx2f(Point.y), &Point);
	}

	if(pMinPoint != nullptr)
		UI()->SetHotItem(pMinPoint);
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
		UI()->DoLabel(&Label, pProps[i].m_pName, 10.0f, TEXTALIGN_ML);

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
			const bool Shift = Input()->ShiftIsPressed();
			int Step = Shift ? 1 : 45;
			int Value = pProps[i].m_Value;

			int NewValue = UiDoValueSelector(&pIDs[i], &Shifter, "", Value, pProps[i].m_Min, pProps[i].m_Max, Shift ? 1 : 45, Shift ? 1.0f : 10.0f, "Use left mouse button to drag and change the value. Hold shift to be more precise. Rightclick to edit as text.", false, false, 0);
			if(DoButton_ButtonDec(&pIDs[i] + 1, nullptr, 0, &Dec, 0, "Decrease"))
			{
				NewValue = (std::ceil((pProps[i].m_Value / (float)Step)) - 1) * Step;
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
			const ColorRGBA ColorPick = ColorRGBA(
				((pProps[i].m_Value >> 24) & 0xff) / 255.0f,
				((pProps[i].m_Value >> 16) & 0xff) / 255.0f,
				((pProps[i].m_Value >> 8) & 0xff) / 255.0f,
				(pProps[i].m_Value & 0xff) / 255.0f);

			CUIRect ColorRect;
			Shifter.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f * UI()->ButtonColorMul(&pIDs[i])), IGraphics::CORNER_ALL, 5.0f);
			Shifter.Margin(1.0f, &ColorRect);
			ColorRect.Draw(ColorPick, IGraphics::CORNER_ALL, ColorRect.h / 2.0f);

			static CUI::SColorPickerPopupContext s_ColorPickerPopupContext;
			const int ButtonResult = DoButton_Editor_Common(&pIDs[i], nullptr, 0, &Shifter, 0, "Click to show the color picker. Shift+rightclick to copy color to clipboard. Shift+leftclick to paste color from clipboard.");
			if(Input()->ShiftIsPressed())
			{
				if(ButtonResult == 1)
				{
					const char *pClipboard = Input()->GetClipboardText();
					if(*pClipboard == '#' || *pClipboard == '$') // ignore leading # (web color format) and $ (console color format)
						++pClipboard;
					if(str_isallnum_hex(pClipboard))
					{
						std::optional<ColorRGBA> ParsedColor = color_parse<ColorRGBA>(pClipboard);
						if(ParsedColor)
						{
							*pNewVal = ParsedColor.value().PackAlphaLast();
							Change = i;
						}
					}
				}
				else if(ButtonResult == 2)
				{
					char aClipboard[9];
					str_format(aClipboard, sizeof(aClipboard), "%08X", ColorPick.PackAlphaLast());
					Input()->SetClipboardText(aClipboard);
				}
			}
			else if(ButtonResult > 0)
			{
				s_ColorPickerPopupContext.m_HsvaColor = color_cast<ColorHSVA>(ColorPick);
				s_ColorPickerPopupContext.m_Alpha = true;
				UI()->ShowPopupColorPicker(UI()->MouseX(), UI()->MouseY(), &s_ColorPickerPopupContext);
			}
			else if(UI()->IsPopupOpen(&s_ColorPickerPopupContext))
			{
				ColorRGBA c = color_cast<ColorRGBA>(s_ColorPickerPopupContext.m_HsvaColor);
				const int NewColor = ((int)(c.r * 255.0f) & 0xff) << 24 | ((int)(c.g * 255.0f) & 0xff) << 16 | ((int)(c.b * 255.0f) & 0xff) << 8 | ((int)(c.a * 255.0f) & 0xff);
				if(NewColor != pProps[i].m_Value)
				{
					*pNewVal = NewColor;
					Change = i;
				}
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_IMAGE)
		{
			const char *pName;
			if(pProps[i].m_Value < 0)
				pName = "None";
			else
				pName = m_Map.m_vpImages[pProps[i].m_Value]->m_aName;

			if(DoButton_Ex(&pIDs[i], pName, 0, &Shifter, 0, nullptr, IGraphics::CORNER_ALL))
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
			UI()->DoLabel(&Shifter, "X", 10.0f, TEXTALIGN_MC);
			Up.VSplitLeft(10.0f, &Up, &Shifter);
			Shifter.VSplitRight(10.0f, &Shifter, &Down);
			Shifter.Draw(ColorRGBA(1, 1, 1, 0.5f), 0, 0.0f);
			UI()->DoLabel(&Shifter, "Y", 10.0f, TEXTALIGN_MC);
			if(DoButton_ButtonDec(&pIDs[i], "-", 0, &Left, 0, "Left"))
			{
				*pNewVal = DIRECTION_LEFT;
				Change = i;
			}
			if(DoButton_ButtonInc(((char *)&pIDs[i]) + 3, "+", 0, &Right, 0, "Right"))
			{
				*pNewVal = DIRECTION_RIGHT;
				Change = i;
			}
			if(DoButton_ButtonDec(((char *)&pIDs[i]) + 1, "-", 0, &Up, 0, "Up"))
			{
				*pNewVal = DIRECTION_UP;
				Change = i;
			}
			if(DoButton_ButtonInc(((char *)&pIDs[i]) + 2, "+", 0, &Down, 0, "Down"))
			{
				*pNewVal = DIRECTION_DOWN;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_SOUND)
		{
			const char *pName;
			if(pProps[i].m_Value < 0)
				pName = "None";
			else
				pName = m_Map.m_vpSounds[pProps[i].m_Value]->m_aName;

			if(DoButton_Ex(&pIDs[i], pName, 0, &Shifter, 0, nullptr, IGraphics::CORNER_ALL))
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
			const char *pName;
			if(pProps[i].m_Value < 0 || pProps[i].m_Min < 0 || pProps[i].m_Min >= (int)m_Map.m_vpImages.size())
				pName = "None";
			else
				pName = m_Map.m_vpImages[pProps[i].m_Min]->m_AutoMapper.GetConfigName(pProps[i].m_Value);

			if(DoButton_Ex(&pIDs[i], pName, 0, &Shifter, 0, nullptr, IGraphics::CORNER_ALL))
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
			char aBuf[8];
			int CurValue = pProps[i].m_Value;

			Shifter.VSplitRight(10.0f, &Shifter, &Inc);
			Shifter.VSplitLeft(10.0f, &Dec, &Shifter);

			if(CurValue <= 0)
				str_copy(aBuf, "None:");
			else if(m_Map.m_vpEnvelopes[CurValue - 1]->m_aName[0])
			{
				str_format(aBuf, sizeof(aBuf), "%s:", m_Map.m_vpEnvelopes[CurValue - 1]->m_aName);
				if(!str_endswith(aBuf, ":"))
				{
					aBuf[sizeof(aBuf) - 2] = ':';
					aBuf[sizeof(aBuf) - 1] = '\0';
				}
			}
			else
				aBuf[0] = '\0';

			int NewVal = UiDoValueSelector((char *)&pIDs[i], &Shifter, aBuf, CurValue, 0, m_Map.m_vpEnvelopes.size(), 1, 1.0f, "Set Envelope", false, false, IGraphics::CORNER_NONE);
			if(NewVal != CurValue)
			{
				*pNewVal = NewVal;
				Change = i;
			}

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

	CUIRect UnscrolledLayersBox = LayersBox;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollbarWidth = 10.0f;
	ScrollParams.m_ScrollbarMargin = 3.0f;
	ScrollParams.m_ScrollUnit = RowHeight * 5.0f;
	s_ScrollRegion.Begin(&LayersBox, &ScrollOffset, &ScrollParams);
	LayersBox.y += ScrollOffset.y;

	enum
	{
		OP_NONE = 0,
		OP_CLICK,
		OP_LAYER_DRAG,
		OP_GROUP_DRAG
	};
	static int s_Operation = OP_NONE;
	static const void *s_pDraggedButton = 0;
	static float s_InitialMouseY = 0;
	static float s_InitialCutHeight = 0;
	int GroupAfterDraggedLayer = -1;
	int LayerAfterDraggedLayer = -1;
	bool DraggedPositionFound = false;
	bool MoveLayers = false;
	bool MoveGroup = false;
	bool StartDragLayer = false;
	bool StartDragGroup = false;
	bool AnyButtonActive = false;
	std::vector<int> vButtonsPerGroup;

	vButtonsPerGroup.reserve(m_Map.m_vpGroups.size());
	for(const std::shared_ptr<CLayerGroup> &pGroup : m_Map.m_vpGroups)
	{
		vButtonsPerGroup.push_back(pGroup->m_vpLayers.size() + 1);
	}

	if(!UI()->CheckActiveItem(s_pDraggedButton))
		s_Operation = OP_NONE;

	if(s_Operation == OP_LAYER_DRAG || s_Operation == OP_GROUP_DRAG)
	{
		float MinDraggableValue = UnscrolledLayersBox.y;
		float MaxDraggableValue = MinDraggableValue;
		for(int NumButtons : vButtonsPerGroup)
		{
			MaxDraggableValue += NumButtons * (RowHeight + 2.0f) + 5.0f;
		}
		MaxDraggableValue += ScrollOffset.y;

		if(s_Operation == OP_GROUP_DRAG)
		{
			MaxDraggableValue -= vButtonsPerGroup[m_SelectedGroup] * (RowHeight + 2.0f) + 5.0f;
		}
		else if(s_Operation == OP_LAYER_DRAG)
		{
			MinDraggableValue += RowHeight + 2.0f;
			MaxDraggableValue -= m_vSelectedLayers.size() * (RowHeight + 2.0f) + 5.0f;
		}

		UnscrolledLayersBox.HSplitTop(s_InitialCutHeight, nullptr, &UnscrolledLayersBox);
		UnscrolledLayersBox.y -= s_InitialMouseY - UI()->MouseY();

		UnscrolledLayersBox.y = clamp(UnscrolledLayersBox.y, MinDraggableValue, MaxDraggableValue);

		UnscrolledLayersBox.w = LayersBox.w;
	}

	static bool s_ScrollToSelectionNext = false;
	const bool ScrollToSelection = SelectLayerByTile() || s_ScrollToSelectionNext;
	s_ScrollToSelectionNext = false;

	// render layers
	for(int g = 0; g < (int)m_Map.m_vpGroups.size(); g++)
	{
		if(s_Operation == OP_LAYER_DRAG && g > 0 && !DraggedPositionFound && UI()->MouseY() < LayersBox.y + RowHeight / 2)
		{
			DraggedPositionFound = true;
			GroupAfterDraggedLayer = g;

			LayerAfterDraggedLayer = m_Map.m_vpGroups[g - 1]->m_vpLayers.size();

			CUIRect Slot;
			LayersBox.HSplitTop(m_vSelectedLayers.size() * (RowHeight + 2.0f), &Slot, &LayersBox);
			s_ScrollRegion.AddRect(Slot);
		}

		CUIRect Slot, VisibleToggle;
		if(s_Operation == OP_GROUP_DRAG)
		{
			if(g == m_SelectedGroup)
			{
				UnscrolledLayersBox.HSplitTop(RowHeight, &Slot, &UnscrolledLayersBox);
				UnscrolledLayersBox.HSplitTop(2.0f, nullptr, &UnscrolledLayersBox);
			}
			else if(!DraggedPositionFound && UI()->MouseY() < LayersBox.y + RowHeight * vButtonsPerGroup[g] / 2 + 3.0f)
			{
				DraggedPositionFound = true;
				GroupAfterDraggedLayer = g;

				CUIRect TmpSlot;
				if(m_Map.m_vpGroups[m_SelectedGroup]->m_Collapse)
					LayersBox.HSplitTop(RowHeight + 7.0f, &TmpSlot, &LayersBox);
				else
					LayersBox.HSplitTop(vButtonsPerGroup[m_SelectedGroup] * (RowHeight + 2.0f) + 5.0f, &TmpSlot, &LayersBox);
				s_ScrollRegion.AddRect(TmpSlot, false);
			}
		}
		if(s_Operation != OP_GROUP_DRAG || g != m_SelectedGroup)
		{
			LayersBox.HSplitTop(RowHeight, &Slot, &LayersBox);

			CUIRect TmpRect;
			LayersBox.HSplitTop(2.0f, &TmpRect, &LayersBox);
			s_ScrollRegion.AddRect(TmpRect);
		}

		if(s_ScrollRegion.AddRect(Slot))
		{
			Slot.VSplitLeft(15.0f, &VisibleToggle, &Slot);
			if(DoButton_FontIcon(&m_Map.m_vpGroups[g]->m_Visible, m_Map.m_vpGroups[g]->m_Visible ? FONT_ICON_EYE : FONT_ICON_EYE_SLASH, m_Map.m_vpGroups[g]->m_Collapse ? 1 : 0, &VisibleToggle, 0, "Toggle group visibility", IGraphics::CORNER_L, 8.0f))
				m_Map.m_vpGroups[g]->m_Visible = !m_Map.m_vpGroups[g]->m_Visible;

			str_format(aBuf, sizeof(aBuf), "#%d %s", g, m_Map.m_vpGroups[g]->m_aName);

			bool Clicked;
			bool Abrupted;
			if(int Result = DoButton_DraggableEx(&m_Map.m_vpGroups[g], aBuf, g == m_SelectedGroup, &Slot, &Clicked, &Abrupted,
				   BUTTON_CONTEXT, m_Map.m_vpGroups[g]->m_Collapse ? "Select group. Shift click to select all layers. Double click to expand." : "Select group. Shift click to select all layers. Double click to collapse.", IGraphics::CORNER_R))
			{
				AnyButtonActive = true;

				if(s_Operation == OP_NONE)
				{
					s_InitialMouseY = UI()->MouseY();
					s_InitialCutHeight = s_InitialMouseY - UnscrolledLayersBox.y;
					s_Operation = OP_CLICK;

					if(g != m_SelectedGroup)
						SelectLayer(0, g);
				}

				if(Abrupted)
					s_Operation = OP_NONE;

				if(s_Operation == OP_CLICK)
				{
					if(absolute(UI()->MouseY() - s_InitialMouseY) > 5)
						StartDragGroup = true;

					s_pDraggedButton = &m_Map.m_vpGroups[g];
				}

				if(s_Operation == OP_CLICK && Clicked)
				{
					if(g != m_SelectedGroup)
						SelectLayer(0, g);

					if(Input()->ShiftIsPressed() && m_SelectedGroup == g)
					{
						m_vSelectedLayers.clear();
						for(size_t i = 0; i < m_Map.m_vpGroups[g]->m_vpLayers.size(); i++)
						{
							AddSelectedLayer(i);
						}
					}

					if(Result == 2)
					{
						static SPopupMenuId s_PopupGroupId;
						UI()->DoPopupMenu(&s_PopupGroupId, UI()->MouseX(), UI()->MouseY(), 145, 256, this, PopupGroup);
					}

					if(!m_Map.m_vpGroups[g]->m_vpLayers.empty() && Input()->MouseDoubleClick())
						m_Map.m_vpGroups[g]->m_Collapse ^= 1;

					s_Operation = OP_NONE;
				}

				if(s_Operation == OP_GROUP_DRAG && Clicked)
					MoveGroup = true;
			}
			else if(s_pDraggedButton == &m_Map.m_vpGroups[g])
			{
				s_Operation = OP_NONE;
			}
		}

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

			if(s_Operation == OP_GROUP_DRAG && g == m_SelectedGroup)
			{
				UnscrolledLayersBox.HSplitTop(RowHeight + 2.0f, &Slot, &UnscrolledLayersBox);
			}
			else if(s_Operation == OP_LAYER_DRAG)
			{
				if(IsLayerSelected)
				{
					UnscrolledLayersBox.HSplitTop(RowHeight + 2.0f, &Slot, &UnscrolledLayersBox);
				}
				else
				{
					if(!DraggedPositionFound && UI()->MouseY() < LayersBox.y + RowHeight / 2)
					{
						DraggedPositionFound = true;
						GroupAfterDraggedLayer = g + 1;
						LayerAfterDraggedLayer = i;
						for(size_t j = 0; j < m_vSelectedLayers.size(); j++)
						{
							LayersBox.HSplitTop(RowHeight + 2.0f, nullptr, &LayersBox);
							s_ScrollRegion.AddRect(Slot);
						}
					}
					LayersBox.HSplitTop(RowHeight + 2.0f, &Slot, &LayersBox);
					if(!s_ScrollRegion.AddRect(Slot, ScrollToSelection && IsLayerSelected))
						continue;
				}
			}
			else
			{
				LayersBox.HSplitTop(RowHeight + 2.0f, &Slot, &LayersBox);
				if(!s_ScrollRegion.AddRect(Slot, ScrollToSelection && IsLayerSelected))
					continue;
			}

			Slot.HSplitTop(RowHeight, &Slot, nullptr);

			CUIRect Button;
			Slot.VSplitLeft(12.0f, nullptr, &Slot);
			Slot.VSplitLeft(15.0f, &VisibleToggle, &Button);

			if(DoButton_FontIcon(&m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Visible, m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Visible ? FONT_ICON_EYE : FONT_ICON_EYE_SLASH, 0, &VisibleToggle, 0, "Toggle layer visibility", IGraphics::CORNER_L, 8.0f))
				m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Visible = !m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Visible;

			if(m_Map.m_vpGroups[g]->m_vpLayers[i]->m_aName[0])
				str_copy(aBuf, m_Map.m_vpGroups[g]->m_vpLayers[i]->m_aName);
			else
			{
				if(m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Type == LAYERTYPE_TILES)
				{
					std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(m_Map.m_vpGroups[g]->m_vpLayers[i]);
					str_copy(aBuf, pTiles->m_Image >= 0 ? m_Map.m_vpImages[pTiles->m_Image]->m_aName : "Tiles");
				}
				else if(m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Type == LAYERTYPE_QUADS)
				{
					std::shared_ptr<CLayerQuads> pQuads = std::static_pointer_cast<CLayerQuads>(m_Map.m_vpGroups[g]->m_vpLayers[i]);
					str_copy(aBuf, pQuads->m_Image >= 0 ? m_Map.m_vpImages[pQuads->m_Image]->m_aName : "Quads");
				}
				else if(m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Type == LAYERTYPE_SOUNDS)
				{
					std::shared_ptr<CLayerSounds> pSounds = std::static_pointer_cast<CLayerSounds>(m_Map.m_vpGroups[g]->m_vpLayers[i]);
					str_copy(aBuf, pSounds->m_Sound >= 0 ? m_Map.m_vpSounds[pSounds->m_Sound]->m_aName : "Sounds");
				}
			}

			int Checked = IsLayerSelected ? 1 : 0;
			if(m_Map.m_vpGroups[g]->m_vpLayers[i]->IsEntitiesLayer())
			{
				Checked += 6;
			}

			bool Clicked;
			bool Abrupted;
			if(int Result = DoButton_DraggableEx(m_Map.m_vpGroups[g]->m_vpLayers[i].get(), aBuf, Checked, &Button, &Clicked, &Abrupted,
				   BUTTON_CONTEXT, "Select layer. Shift click to select multiple.", IGraphics::CORNER_R))
			{
				AnyButtonActive = true;

				if(s_Operation == OP_NONE)
				{
					s_InitialMouseY = UI()->MouseY();
					s_InitialCutHeight = s_InitialMouseY - UnscrolledLayersBox.y;
					s_Operation = OP_CLICK;

					if(!Input()->ShiftIsPressed() && !IsLayerSelected)
					{
						SelectLayer(i, g);
					}
				}

				if(Abrupted)
					s_Operation = OP_NONE;

				if(s_Operation == OP_CLICK)
				{
					if(absolute(UI()->MouseY() - s_InitialMouseY) > 5)
					{
						bool EntitiesLayerSelected = false;
						for(int k : m_vSelectedLayers)
						{
							if(m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers[k]->IsEntitiesLayer())
								EntitiesLayerSelected = true;
						}

						if(!EntitiesLayerSelected)
							StartDragLayer = true;
					}

					s_pDraggedButton = m_Map.m_vpGroups[g]->m_vpLayers[i].get();
				}

				if(s_Operation == OP_CLICK && Clicked)
				{
					static SLayerPopupContext s_LayerPopupContext = {};
					s_LayerPopupContext.m_pEditor = this;
					if(Result == 1)
					{
						if(Input()->ShiftIsPressed() && m_SelectedGroup == g)
						{
							auto Position = std::find(m_vSelectedLayers.begin(), m_vSelectedLayers.end(), i);
							if(Position != m_vSelectedLayers.end())
								m_vSelectedLayers.erase(Position);
							else
								AddSelectedLayer(i);
						}
						else if(!Input()->ShiftIsPressed())
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
									s_LayerPopupContext.m_vpLayers.push_back(std::static_pointer_cast<CLayerTiles>(m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers[m_vSelectedLayers[j]]));
								else
									AllTile = false;
							}

							// Don't allow editing if all selected layers are tile layers
							if(!AllTile)
								s_LayerPopupContext.m_vpLayers.clear();
						}
						else
							s_LayerPopupContext.m_vpLayers.clear();

						UI()->DoPopupMenu(&s_LayerPopupContext, UI()->MouseX(), UI()->MouseY(), 120, 270, &s_LayerPopupContext, PopupLayer);
					}

					s_Operation = OP_NONE;
				}

				if(s_Operation == OP_LAYER_DRAG && Clicked)
				{
					MoveLayers = true;
				}
			}
			else if(s_pDraggedButton == m_Map.m_vpGroups[g]->m_vpLayers[i].get())
			{
				s_Operation = OP_NONE;
			}
		}

		if(s_Operation != OP_GROUP_DRAG || g != m_SelectedGroup)
		{
			LayersBox.HSplitTop(5.0f, &Slot, &LayersBox);
			s_ScrollRegion.AddRect(Slot);
		}
	}

	if(!DraggedPositionFound && s_Operation == OP_LAYER_DRAG)
	{
		GroupAfterDraggedLayer = m_Map.m_vpGroups.size();
		LayerAfterDraggedLayer = m_Map.m_vpGroups[GroupAfterDraggedLayer - 1]->m_vpLayers.size();

		CUIRect TmpSlot;
		LayersBox.HSplitTop(m_vSelectedLayers.size() * (RowHeight + 2.0f), &TmpSlot, &LayersBox);
		s_ScrollRegion.AddRect(TmpSlot);
	}

	if(!DraggedPositionFound && s_Operation == OP_GROUP_DRAG)
	{
		GroupAfterDraggedLayer = m_Map.m_vpGroups.size();

		CUIRect TmpSlot;
		if(m_Map.m_vpGroups[m_SelectedGroup]->m_Collapse)
			LayersBox.HSplitTop(RowHeight + 7.0f, &TmpSlot, &LayersBox);
		else
			LayersBox.HSplitTop(vButtonsPerGroup[m_SelectedGroup] * (RowHeight + 2.0f) + 5.0f, &TmpSlot, &LayersBox);
		s_ScrollRegion.AddRect(TmpSlot, false);
	}

	if(MoveLayers && 1 <= GroupAfterDraggedLayer && GroupAfterDraggedLayer <= (int)m_Map.m_vpGroups.size())
	{
		std::vector<std::shared_ptr<CLayer>> &vpNewGroupLayers = m_Map.m_vpGroups[GroupAfterDraggedLayer - 1]->m_vpLayers;
		if(0 <= LayerAfterDraggedLayer && LayerAfterDraggedLayer <= (int)vpNewGroupLayers.size())
		{
			std::vector<std::shared_ptr<CLayer>> vpSelectedLayers;
			std::vector<std::shared_ptr<CLayer>> &vpSelectedGroupLayers = m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers;
			std::shared_ptr<CLayer> pNextLayer = nullptr;
			if(LayerAfterDraggedLayer < (int)vpNewGroupLayers.size())
				pNextLayer = vpNewGroupLayers[LayerAfterDraggedLayer];

			std::sort(m_vSelectedLayers.begin(), m_vSelectedLayers.end(), std::greater<>());
			for(int k : m_vSelectedLayers)
			{
				vpSelectedLayers.insert(vpSelectedLayers.begin(), vpSelectedGroupLayers[k]);
			}
			for(int k : m_vSelectedLayers)
			{
				vpSelectedGroupLayers.erase(vpSelectedGroupLayers.begin() + k);
			}

			auto InsertPosition = std::find(vpNewGroupLayers.begin(), vpNewGroupLayers.end(), pNextLayer);
			int InsertPositionIndex = InsertPosition - vpNewGroupLayers.begin();
			vpNewGroupLayers.insert(InsertPosition, vpSelectedLayers.begin(), vpSelectedLayers.end());

			int NumSelectedLayers = m_vSelectedLayers.size();
			m_vSelectedLayers.clear();
			for(int i = 0; i < NumSelectedLayers; i++)
				m_vSelectedLayers.push_back(InsertPositionIndex + i);

			m_SelectedGroup = GroupAfterDraggedLayer - 1;
			m_Map.OnModify();
		}
	}

	if(MoveGroup && 0 <= GroupAfterDraggedLayer && GroupAfterDraggedLayer <= (int)m_Map.m_vpGroups.size())
	{
		std::shared_ptr<CLayerGroup> pSelectedGroup = m_Map.m_vpGroups[m_SelectedGroup];
		std::shared_ptr<CLayerGroup> pNextGroup = nullptr;
		if(GroupAfterDraggedLayer < (int)m_Map.m_vpGroups.size())
			pNextGroup = m_Map.m_vpGroups[GroupAfterDraggedLayer];

		m_Map.m_vpGroups.erase(m_Map.m_vpGroups.begin() + m_SelectedGroup);

		auto InsertPosition = std::find(m_Map.m_vpGroups.begin(), m_Map.m_vpGroups.end(), pNextGroup);
		m_Map.m_vpGroups.insert(InsertPosition, pSelectedGroup);

		m_SelectedGroup = InsertPosition - m_Map.m_vpGroups.begin();
		m_Map.OnModify();
	}

	if(MoveLayers || MoveGroup)
		s_Operation = OP_NONE;
	if(StartDragLayer)
		s_Operation = OP_LAYER_DRAG;
	if(StartDragGroup)
		s_Operation = OP_GROUP_DRAG;

	if(s_Operation == OP_LAYER_DRAG || s_Operation == OP_GROUP_DRAG)
	{
		s_ScrollRegion.DoEdgeScrolling();
		UI()->SetActiveItem(s_pDraggedButton);
	}

	if(Input()->KeyPress(KEY_DOWN) && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && s_Operation == OP_NONE)
	{
		if(Input()->ShiftIsPressed())
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
		s_ScrollToSelectionNext = true;
	}
	if(Input()->KeyPress(KEY_UP) && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && s_Operation == OP_NONE)
	{
		if(Input()->ShiftIsPressed())
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
		s_ScrollToSelectionNext = true;
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

	if(!AnyButtonActive)
		s_Operation = OP_NONE;
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

				std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(m_Map.m_vpGroups[g]->m_vpLayers[l]);
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

bool CEditor::ReplaceImage(const char *pFileName, int StorageType, bool CheckDuplicate)
{
	// check if we have that image already
	char aBuf[128];
	IStorage::StripPathAndExtension(pFileName, aBuf, sizeof(aBuf));
	if(CheckDuplicate)
	{
		for(const auto &pImage : m_Map.m_vpImages)
		{
			if(!str_comp(pImage->m_aName, aBuf))
			{
				ShowFileDialogError("Image named '%s' was already added.", pImage->m_aName);
				return false;
			}
		}
	}

	CEditorImage ImgInfo(this);
	if(!Graphics()->LoadPNG(&ImgInfo, pFileName, StorageType))
	{
		ShowFileDialogError("Failed to load image from file '%s'.", pFileName);
		return false;
	}

	std::shared_ptr<CEditorImage> pImg = m_Map.m_vpImages[m_SelectedImage];
	Graphics()->UnloadTexture(&(pImg->m_Texture));
	free(pImg->m_pData);
	pImg->m_pData = nullptr;
	*pImg = ImgInfo;
	str_copy(pImg->m_aName, aBuf);
	pImg->m_External = IsVanillaImage(pImg->m_aName);

	if(!pImg->m_External && g_Config.m_ClEditorDilate == 1 && pImg->m_Format == CImageInfo::FORMAT_RGBA)
	{
		int ColorChannelCount = 0;
		if(ImgInfo.m_Format == CImageInfo::FORMAT_RGBA)
			ColorChannelCount = 4;

		DilateImage((unsigned char *)ImgInfo.m_pData, ImgInfo.m_Width, ImgInfo.m_Height, ColorChannelCount);
	}

	pImg->m_AutoMapper.Load(pImg->m_aName);
	int TextureLoadFlag = Graphics()->HasTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE;
	if(ImgInfo.m_Width % 16 != 0 || ImgInfo.m_Height % 16 != 0)
		TextureLoadFlag = 0;
	pImg->m_Texture = Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, CImageInfo::FORMAT_AUTO, TextureLoadFlag, pFileName);
	ImgInfo.m_pData = nullptr;
	SortImages();
	for(size_t i = 0; i < m_Map.m_vpImages.size(); ++i)
	{
		if(!str_comp(m_Map.m_vpImages[i]->m_aName, pImg->m_aName))
			m_SelectedImage = i;
	}
	m_Dialog = DIALOG_NONE;
	return true;
}

bool CEditor::ReplaceImageCallback(const char *pFileName, int StorageType, void *pUser)
{
	return static_cast<CEditor *>(pUser)->ReplaceImage(pFileName, StorageType, true);
}

bool CEditor::AddImage(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;

	// check if we have that image already
	char aBuf[128];
	IStorage::StripPathAndExtension(pFileName, aBuf, sizeof(aBuf));
	for(const auto &pImage : pEditor->m_Map.m_vpImages)
	{
		if(!str_comp(pImage->m_aName, aBuf))
		{
			pEditor->ShowFileDialogError("Image named '%s' was already added.", pImage->m_aName);
			return false;
		}
	}

	if(pEditor->m_Map.m_vpImages.size() >= 64) // hard limit for teeworlds
	{
		pEditor->m_PopupEventType = POPEVENT_IMAGE_MAX;
		pEditor->m_PopupEventActivated = true;
		return false;
	}

	CEditorImage ImgInfo(pEditor);
	if(!pEditor->Graphics()->LoadPNG(&ImgInfo, pFileName, StorageType))
	{
		pEditor->ShowFileDialogError("Failed to load image from file '%s'.", pFileName);
		return false;
	}

	std::shared_ptr<CEditorImage> pImg = std::make_shared<CEditorImage>(pEditor);
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
	str_copy(pImg->m_aName, aBuf);
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
	return true;
}

bool CEditor::AddSound(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;

	// check if we have that sound already
	char aBuf[128];
	IStorage::StripPathAndExtension(pFileName, aBuf, sizeof(aBuf));
	for(const auto &pSound : pEditor->m_Map.m_vpSounds)
	{
		if(!str_comp(pSound->m_aName, aBuf))
		{
			pEditor->ShowFileDialogError("Sound named '%s' was already added.", pSound->m_aName);
			return false;
		}
	}

	// load external
	void *pData;
	unsigned DataSize;
	if(!pEditor->Storage()->ReadFile(pFileName, StorageType, &pData, &DataSize))
	{
		pEditor->ShowFileDialogError("Failed to open sound file '%s'.", pFileName);
		return false;
	}

	// load sound
	const int SoundId = pEditor->Sound()->LoadOpusFromMem(pData, DataSize, true);
	if(SoundId == -1)
	{
		free(pData);
		pEditor->ShowFileDialogError("Failed to load sound from file '%s'.", pFileName);
		return false;
	}

	// add sound
	std::shared_ptr<CEditorSound> pSound = std::make_shared<CEditorSound>(pEditor);
	pSound->m_SoundID = SoundId;
	pSound->m_DataSize = DataSize;
	pSound->m_pData = pData;
	str_copy(pSound->m_aName, aBuf);
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
	return true;
}

bool CEditor::ReplaceSound(const char *pFileName, int StorageType, bool CheckDuplicate)
{
	// check if we have that sound already
	char aBuf[128];
	IStorage::StripPathAndExtension(pFileName, aBuf, sizeof(aBuf));
	if(CheckDuplicate)
	{
		for(const auto &pSound : m_Map.m_vpSounds)
		{
			if(!str_comp(pSound->m_aName, aBuf))
			{
				ShowFileDialogError("Sound named '%s' was already added.", pSound->m_aName);
				return false;
			}
		}
	}

	// load external
	void *pData;
	unsigned DataSize;
	if(!Storage()->ReadFile(pFileName, StorageType, &pData, &DataSize))
	{
		ShowFileDialogError("Failed to open sound file '%s'.", pFileName);
		return false;
	}

	// load sound
	const int SoundId = Sound()->LoadOpusFromMem(pData, DataSize, true);
	if(SoundId == -1)
	{
		free(pData);
		ShowFileDialogError("Failed to load sound from file '%s'.", pFileName);
		return false;
	}

	std::shared_ptr<CEditorSound> pSound = m_Map.m_vpSounds[m_SelectedSound];

	// unload sample
	Sound()->UnloadSample(pSound->m_SoundID);
	free(pSound->m_pData);

	// replace sound
	str_copy(pSound->m_aName, aBuf);
	pSound->m_SoundID = SoundId;
	pSound->m_pData = pData;
	pSound->m_DataSize = DataSize;

	m_Dialog = DIALOG_NONE;
	return true;
}

bool CEditor::ReplaceSoundCallback(const char *pFileName, int StorageType, void *pUser)
{
	return static_cast<CEditor *>(pUser)->ReplaceSound(pFileName, StorageType, true);
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

void CEditor::SortImages()
{
	static const auto &&s_ImageNameComparator = [](const std::shared_ptr<CEditorImage> &pLhs, const std::shared_ptr<CEditorImage> &pRhs) {
		return str_comp(pLhs->m_aName, pRhs->m_aName) < 0;
	};
	if(!std::is_sorted(m_Map.m_vpImages.begin(), m_Map.m_vpImages.end(), s_ImageNameComparator))
	{
		const std::vector<std::shared_ptr<CEditorImage>> vpTemp = m_Map.m_vpImages;
		std::vector<int> vSortedIndex;
		vSortedIndex.resize(vpTemp.size());

		std::sort(m_Map.m_vpImages.begin(), m_Map.m_vpImages.end(), s_ImageNameComparator);
		for(size_t OldIndex = 0; OldIndex < vpTemp.size(); OldIndex++)
		{
			for(size_t NewIndex = 0; NewIndex < m_Map.m_vpImages.size(); NewIndex++)
			{
				if(vpTemp[OldIndex] == m_Map.m_vpImages[NewIndex])
				{
					vSortedIndex[OldIndex] = NewIndex;
					break;
				}
			}
		}
		m_Map.ModifyImageIndex([vSortedIndex](int *pIndex) {
			if(*pIndex >= 0)
				*pIndex = vSortedIndex[*pIndex];
		});
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
	if(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && !m_Map.m_vpImages.empty())
	{
		if(Input()->KeyPress(KEY_DOWN))
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
		else if(Input()->KeyPress(KEY_UP))
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
	}

	for(int e = 0; e < 2; e++) // two passes, first embedded, then external
	{
		CUIRect Slot;
		ToolBox.HSplitTop(RowHeight + 3.0f, &Slot, &ToolBox);
		if(s_ScrollRegion.AddRect(Slot))
			UI()->DoLabel(&Slot, e == 0 ? "Embedded" : "External", 12.0f, TEXTALIGN_MC);

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
						return std::static_pointer_cast<CLayerQuads>(pLayer)->m_Image == i;
					else if(pLayer->m_Type == LAYERTYPE_TILES)
						return std::static_pointer_cast<CLayerTiles>(pLayer)->m_Image == i;
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

			if(int Result = DoButton_Ex(&m_Map.m_vpImages[i], m_Map.m_vpImages[i]->m_aName, Selected, &Slot,
				   BUTTON_CONTEXT, "Select image.", IGraphics::CORNER_ALL))
			{
				m_SelectedImage = i;

				if(Result == 2)
				{
					const std::shared_ptr<CEditorImage> pImg = m_Map.m_vpImages[m_SelectedImage];
					const int Height = pImg->m_External || IsVanillaImage(pImg->m_aName) ? 73 : 56;
					static SPopupMenuId s_PopupImageId;
					UI()->DoPopupMenu(&s_PopupImageId, UI()->MouseX(), UI()->MouseY(), 120, Height, this, PopupImage);
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
			InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_IMG, "Add Image", "Add", "mapres", false, AddImage, this);
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
	if(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && !m_Map.m_vpSounds.empty())
	{
		if(Input()->KeyPress(KEY_DOWN))
		{
			m_SelectedSound = (m_SelectedSound + 1) % m_Map.m_vpSounds.size();
			ScrollToSelection = true;
		}
		else if(Input()->KeyPress(KEY_UP))
		{
			m_SelectedSound = (m_SelectedSound + m_Map.m_vpSounds.size() - 1) % m_Map.m_vpSounds.size();
			ScrollToSelection = true;
		}
	}

	CUIRect Slot;
	ToolBox.HSplitTop(RowHeight + 3.0f, &Slot, &ToolBox);
	if(s_ScrollRegion.AddRect(Slot))
		UI()->DoLabel(&Slot, "Embedded", 12.0f, TEXTALIGN_MC);

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
					return std::static_pointer_cast<CLayerSounds>(pLayer)->m_Sound == i;
				return false;
			});
		});

		if(!SoundUsed)
			Selected += 2; // Sound is unused

		if(int Result = DoButton_Ex(&m_Map.m_vpSounds[i], m_Map.m_vpSounds[i]->m_aName, Selected, &Slot,
			   BUTTON_CONTEXT, "Select sound.", IGraphics::CORNER_ALL))
		{
			m_SelectedSound = i;

			if(Result == 2)
			{
				static SPopupMenuId s_PopupSoundId;
				UI()->DoPopupMenu(&s_PopupSoundId, UI()->MouseX(), UI()->MouseY(), 120, 56, this, PopupSound);
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

	// new sound
	static int s_AddSoundButton = 0;
	CUIRect AddSoundButton;
	ToolBox.HSplitTop(5.0f + RowHeight + 1.0f, &AddSoundButton, &ToolBox);
	if(s_ScrollRegion.AddRect(AddSoundButton))
	{
		AddSoundButton.HSplitTop(5.0f, nullptr, &AddSoundButton);
		AddSoundButton.HSplitTop(RowHeight, &AddSoundButton, nullptr);
		if(DoButton_Editor(&s_AddSoundButton, "Add", 0, &AddSoundButton, 0, "Load a new sound to use in the map"))
			InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_SOUND, "Add Sound", "Add", "mapres", false, AddSound, this);
	}
	s_ScrollRegion.End();
}

static int EditorListdirCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;
	if((pInfo->m_pName[0] == '.' && (pInfo->m_pName[1] == 0 ||
						(pInfo->m_pName[1] == '.' && pInfo->m_pName[2] == 0 && (pEditor->m_FileDialogShowingRoot || (!pEditor->m_FileDialogMultipleStorages && (!str_comp(pEditor->m_pFileDialogPath, "maps") || !str_comp(pEditor->m_pFileDialogPath, "mapres"))))))) ||
		(!IsDir && ((pEditor->m_FileDialogFileType == CEditor::FILETYPE_MAP && !str_endswith(pInfo->m_pName, ".map")) ||
				   (pEditor->m_FileDialogFileType == CEditor::FILETYPE_IMG && !str_endswith(pInfo->m_pName, ".png")) ||
				   (pEditor->m_FileDialogFileType == CEditor::FILETYPE_SOUND && !str_endswith(pInfo->m_pName, ".opus")))))
		return 0;

	CEditor::CFilelistItem Item;
	str_copy(Item.m_aFilename, pInfo->m_pName);
	if(IsDir)
		str_format(Item.m_aName, sizeof(Item.m_aName), "%s/", pInfo->m_pName);
	else
	{
		int LenEnding = pEditor->m_FileDialogFileType == CEditor::FILETYPE_SOUND ? 5 : 4;
		str_truncate(Item.m_aName, sizeof(Item.m_aName), pInfo->m_pName, str_length(pInfo->m_pName) - LenEnding);
	}
	Item.m_IsDir = IsDir != 0;
	Item.m_IsLink = false;
	Item.m_StorageType = StorageType;
	Item.m_TimeModified = pInfo->m_TimeModified;
	pEditor->m_vCompleteFileList.push_back(Item);

	return 0;
}

void CEditor::SortFilteredFileList()
{
	if(m_SortByFilename == 1)
	{
		std::sort(m_vpFilteredFileList.begin(), m_vpFilteredFileList.end(), CEditor::CompareFilenameAscending);
	}
	else
	{
		std::sort(m_vpFilteredFileList.begin(), m_vpFilteredFileList.end(), CEditor::CompareFilenameDescending);
	}

	if(m_SortByTimeModified == 1)
	{
		std::stable_sort(m_vpFilteredFileList.begin(), m_vpFilteredFileList.end(), CEditor::CompareTimeModifiedAscending);
	}
	else if(m_SortByTimeModified == -1)
	{
		std::stable_sort(m_vpFilteredFileList.begin(), m_vpFilteredFileList.end(), CEditor::CompareTimeModifiedDescending);
	}
}

void CEditor::RenderFileDialog()
{
	// GUI coordsys
	UI()->MapScreen();
	CUIRect View = *UI()->Screen();
	CUIRect Preview = {0.0f, 0.0f, 0.0f, 0.0f};
	float Width = View.w, Height = View.h;

	View.Draw(ColorRGBA(0, 0, 0, 0.25f), 0, 0);
	View.VMargin(150.0f, &View);
	View.HMargin(50.0f, &View);
	View.Draw(ColorRGBA(0, 0, 0, 0.75f), IGraphics::CORNER_ALL, 5.0f);
	View.Margin(10.0f, &View);

	CUIRect Title, FileBox, FileBoxLabel, ButtonBar, PathBox;
	View.HSplitTop(18.0f, &Title, &View);
	View.HSplitTop(5.0f, nullptr, &View); // some spacing
	View.HSplitBottom(14.0f, &View, &ButtonBar);
	View.HSplitBottom(10.0f, &View, nullptr); // some spacing
	View.HSplitBottom(14.0f, &View, &PathBox);
	View.HSplitBottom(5.0f, &View, nullptr); // some spacing
	View.HSplitBottom(14.0f, &View, &FileBox);
	FileBox.VSplitLeft(55.0f, &FileBoxLabel, &FileBox);
	View.HSplitBottom(10.0f, &View, nullptr); // some spacing
	if(m_FileDialogFileType == CEditor::FILETYPE_IMG || m_FileDialogFileType == CEditor::FILETYPE_SOUND)
		View.VSplitMid(&View, &Preview);

	// title bar
	if(!m_FileDialogShowingRoot)
	{
		CUIRect ButtonTimeModified, ButtonFileName;
		Title.VSplitRight(10.0f, &Title, nullptr);
		Title.VSplitRight(90.0f, &Title, &ButtonTimeModified);
		Title.VSplitRight(10.0f, &Title, nullptr);
		Title.VSplitRight(90.0f, &Title, &ButtonFileName);
		Title.VSplitRight(10.0f, &Title, nullptr);

		const char *aSortIndicator[3] = {"▼", "", "▲"};

		static int s_ButtonTimeModified = 0;
		char aBufLabelButtonTimeModified[64];
		str_format(aBufLabelButtonTimeModified, sizeof(aBufLabelButtonTimeModified), "Time modified %s", aSortIndicator[m_SortByTimeModified + 1]);
		if(DoButton_Editor(&s_ButtonTimeModified, aBufLabelButtonTimeModified, 0, &ButtonTimeModified, 0, "Sort by time modified"))
		{
			if(m_SortByTimeModified == 1)
			{
				m_SortByTimeModified = -1;
			}
			else if(m_SortByTimeModified == -1)
			{
				m_SortByTimeModified = 0;
			}
			else
			{
				m_SortByTimeModified = 1;
			}

			RefreshFilteredFileList();
		}

		static int s_ButtonFileName = 0;
		char aBufLabelButtonFilename[64];
		str_format(aBufLabelButtonFilename, sizeof(aBufLabelButtonFilename), "Filename %s", aSortIndicator[m_SortByFilename + 1]);
		if(DoButton_Editor(&s_ButtonFileName, aBufLabelButtonFilename, 0, &ButtonFileName, 0, "Sort by file name"))
		{
			if(m_SortByFilename == 1)
			{
				m_SortByFilename = -1;
				m_SortByTimeModified = 0;
			}
			else
			{
				m_SortByFilename = 1;
				m_SortByTimeModified = 0;
			}

			RefreshFilteredFileList();
		}
	}

	Title.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 4.0f);
	Title.VMargin(10.0f, &Title);
	UI()->DoLabel(&Title, m_pFileDialogTitle, 12.0f, TEXTALIGN_ML);

	// pathbox
	if(m_FilesSelectedIndex >= 0 && m_vpFilteredFileList[m_FilesSelectedIndex]->m_StorageType >= IStorage::TYPE_SAVE)
	{
		char aPath[IO_MAX_PATH_LENGTH], aBuf[128 + IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(m_vpFilteredFileList[m_FilesSelectedIndex]->m_StorageType, m_pFileDialogPath, aPath, sizeof(aPath));
		str_format(aBuf, sizeof(aBuf), "Current path: %s", aPath);
		UI()->DoLabel(&PathBox, aBuf, 10.0f, TEXTALIGN_ML);
	}

	const auto &&UpdateFileNameInput = [this]() {
		if(m_FilesSelectedIndex >= 0 && !m_vpFilteredFileList[m_FilesSelectedIndex]->m_IsDir)
		{
			char aNameWithoutExt[IO_MAX_PATH_LENGTH];
			fs_split_file_extension(m_vpFilteredFileList[m_FilesSelectedIndex]->m_aFilename, aNameWithoutExt, sizeof(aNameWithoutExt));
			m_FileDialogFileNameInput.Set(aNameWithoutExt);
		}
		else
			m_FileDialogFileNameInput.Clear();
	};

	// filebox
	static CListBox s_ListBox;
	s_ListBox.SetActive(!UI()->IsPopupOpen());

	if(m_FileDialogStorageType == IStorage::TYPE_SAVE)
	{
		UI()->DoLabel(&FileBoxLabel, "Filename:", 10.0f, TEXTALIGN_ML);
		if(DoEditBox(&m_FileDialogFileNameInput, &FileBox, 10.0f))
		{
			// remove '/' and '\'
			for(int i = 0; m_FileDialogFileNameInput.GetString()[i]; ++i)
			{
				if(m_FileDialogFileNameInput.GetString()[i] == '/' || m_FileDialogFileNameInput.GetString()[i] == '\\')
				{
					m_FileDialogFileNameInput.SetRange(m_FileDialogFileNameInput.GetString() + i + 1, i, m_FileDialogFileNameInput.GetLength());
					--i;
				}
			}
			m_FilesSelectedIndex = -1;
			m_aFilesSelectedName[0] = '\0';
			// find first valid entry, if it exists
			for(size_t i = 0; i < m_vpFilteredFileList.size(); i++)
			{
				if(str_comp_nocase(m_vpFilteredFileList[i]->m_aName, m_FileDialogFileNameInput.GetString()) == 0)
				{
					m_FilesSelectedIndex = i;
					str_copy(m_aFilesSelectedName, m_vpFilteredFileList[i]->m_aName);
					break;
				}
			}
			if(m_FilesSelectedIndex >= 0)
				s_ListBox.ScrollToSelected();
		}

		if(m_FileDialogOpening)
			UI()->SetActiveItem(&m_FileDialogFileNameInput);
	}
	else
	{
		// render search bar
		UI()->DoLabel(&FileBoxLabel, "Search:", 10.0f, TEXTALIGN_ML);
		if(m_FileDialogOpening)
			UI()->SetActiveItem(&m_FileDialogFilterInput);
		if(UI()->DoClearableEditBox(&m_FileDialogFilterInput, &FileBox, 10.0f))
		{
			RefreshFilteredFileList();
			if(m_vpFilteredFileList.empty())
			{
				m_FilesSelectedIndex = -1;
			}
			else if(m_FilesSelectedIndex == -1 || (!m_FileDialogFilterInput.IsEmpty() && !str_find_nocase(m_vpFilteredFileList[m_FilesSelectedIndex]->m_aName, m_FileDialogFilterInput.GetString())))
			{
				// we need to refresh selection
				m_FilesSelectedIndex = -1;
				for(size_t i = 0; i < m_vpFilteredFileList.size(); i++)
				{
					if(str_find_nocase(m_vpFilteredFileList[i]->m_aName, m_FileDialogFilterInput.GetString()))
					{
						m_FilesSelectedIndex = i;
						break;
					}
				}
				if(m_FilesSelectedIndex == -1)
				{
					// select first item
					m_FilesSelectedIndex = 0;
				}
			}
			if(m_FilesSelectedIndex >= 0)
				str_copy(m_aFilesSelectedName, m_vpFilteredFileList[m_FilesSelectedIndex]->m_aName);
			else
				m_aFilesSelectedName[0] = '\0';
			UpdateFileNameInput();
			s_ListBox.ScrollToSelected();
			m_FilePreviewState = PREVIEW_UNLOADED;
		}
	}

	m_FileDialogOpening = false;

	if(m_FilesSelectedIndex > -1)
	{
		if(m_FilePreviewState == PREVIEW_UNLOADED)
		{
			if(m_FileDialogFileType == CEditor::FILETYPE_IMG && str_endswith(m_vpFilteredFileList[m_FilesSelectedIndex]->m_aFilename, ".png"))
			{
				char aBuffer[IO_MAX_PATH_LENGTH];
				str_format(aBuffer, sizeof(aBuffer), "%s/%s", m_pFileDialogPath, m_vpFilteredFileList[m_FilesSelectedIndex]->m_aFilename);
				if(Graphics()->LoadPNG(&m_FilePreviewImageInfo, aBuffer, m_vpFilteredFileList[m_FilesSelectedIndex]->m_StorageType))
				{
					Graphics()->UnloadTexture(&m_FilePreviewImage);
					m_FilePreviewImage = Graphics()->LoadTextureRaw(m_FilePreviewImageInfo.m_Width, m_FilePreviewImageInfo.m_Height, m_FilePreviewImageInfo.m_Format, m_FilePreviewImageInfo.m_pData, m_FilePreviewImageInfo.m_Format, 0);
					Graphics()->FreePNG(&m_FilePreviewImageInfo);
					m_FilePreviewState = PREVIEW_LOADED;
				}
				else
				{
					m_FilePreviewState = PREVIEW_ERROR;
				}
			}
			else if(m_FileDialogFileType == CEditor::FILETYPE_SOUND && str_endswith(m_vpFilteredFileList[m_FilesSelectedIndex]->m_aFilename, ".opus"))
			{
				char aBuffer[IO_MAX_PATH_LENGTH];
				str_format(aBuffer, sizeof(aBuffer), "%s/%s", m_pFileDialogPath, m_vpFilteredFileList[m_FilesSelectedIndex]->m_aFilename);
				Sound()->UnloadSample(m_FilePreviewSound);
				m_FilePreviewSound = Sound()->LoadOpus(aBuffer, m_vpFilteredFileList[m_FilesSelectedIndex]->m_StorageType);
				m_FilePreviewState = m_FilePreviewSound == -1 ? PREVIEW_ERROR : PREVIEW_LOADED;
			}
		}

		if(m_FileDialogFileType == CEditor::FILETYPE_IMG)
		{
			Preview.Margin(10.0f, &Preview);
			if(m_FilePreviewState == PREVIEW_LOADED)
			{
				int w = m_FilePreviewImageInfo.m_Width;
				int h = m_FilePreviewImageInfo.m_Height;
				if(m_FilePreviewImageInfo.m_Width > Preview.w)
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
			else if(m_FilePreviewState == PREVIEW_ERROR)
			{
				SLabelProperties Props;
				Props.m_MaxWidth = Preview.w;
				UI()->DoLabel(&Preview, "Failed to load the image (check the local console for details).", 12.0f, TEXTALIGN_TL, Props);
			}
		}
		else if(m_FileDialogFileType == CEditor::FILETYPE_SOUND)
		{
			Preview.Margin(10.0f, &Preview);
			if(m_FilePreviewState == PREVIEW_LOADED)
			{
				CUIRect Button;
				Preview.HSplitTop(20.0f, &Preview, nullptr);
				Preview.VSplitLeft(Preview.h, &Button, &Preview);
				Preview.VSplitLeft(Preview.h / 4.0f, nullptr, &Preview);

				static int s_PlayStopButton;
				if(DoButton_FontIcon(&s_PlayStopButton, Sound()->IsPlaying(m_FilePreviewSound) ? FONT_ICON_STOP : FONT_ICON_PLAY, 0, &Button, 0, "Play/stop audio preview", IGraphics::CORNER_ALL))
				{
					if(Sound()->IsPlaying(m_FilePreviewSound))
						Sound()->Stop(m_FilePreviewSound);
					else
						Sound()->Play(CSounds::CHN_GUI, m_FilePreviewSound, 0);
				}

				char aDuration[32];
				char aDurationLabel[64];
				str_time_float(Sound()->GetSampleDuration(m_FilePreviewSound), TIME_HOURS, aDuration, sizeof(aDuration));
				str_format(aDurationLabel, sizeof(aDurationLabel), "Duration: %s", aDuration);
				UI()->DoLabel(&Preview, aDurationLabel, 12.0f, TEXTALIGN_ML);
			}
			else if(m_FilePreviewState == PREVIEW_ERROR)
			{
				SLabelProperties Props;
				Props.m_MaxWidth = Preview.w;
				UI()->DoLabel(&Preview, "Failed to load the sound (check the local console for details). Make sure you enabled sounds in the settings.", 12.0f, TEXTALIGN_TL, Props);
			}
		}
	}

	s_ListBox.DoStart(15.0f, m_vpFilteredFileList.size(), 1, 5, m_FilesSelectedIndex, &View, false);

	for(size_t i = 0; i < m_vpFilteredFileList.size(); i++)
	{
		const CListboxItem Item = s_ListBox.DoNextItem(m_vpFilteredFileList[i], m_FilesSelectedIndex >= 0 && (size_t)m_FilesSelectedIndex == i);
		if(!Item.m_Visible)
			continue;

		CUIRect Button, FileIcon, TimeModified;
		Item.m_Rect.VSplitLeft(Item.m_Rect.h, &FileIcon, &Button);
		Button.VSplitLeft(5.0f, nullptr, &Button);
		Button.VSplitRight(100.0f, &Button, &TimeModified);
		Button.VSplitRight(5.0f, &Button, nullptr);

		const char *pIconType;
		if(!m_vpFilteredFileList[i]->m_IsDir)
		{
			switch(m_FileDialogFileType)
			{
			case FILETYPE_MAP:
				pIconType = FONT_ICON_MAP;
				break;
			case FILETYPE_IMG:
				pIconType = FONT_ICON_IMAGE;
				break;
			case FILETYPE_SOUND:
				pIconType = FONT_ICON_MUSIC;
				break;
			default:
				pIconType = FONT_ICON_FILE;
			}
		}
		else
		{
			if(m_vpFilteredFileList[i]->m_IsLink || str_comp(m_vpFilteredFileList[i]->m_aFilename, "..") == 0)
				pIconType = FONT_ICON_FOLDER_TREE;
			else
				pIconType = FONT_ICON_FOLDER;
		}

		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
		UI()->DoLabel(&FileIcon, pIconType, 12.0f, TEXTALIGN_ML);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

		SLabelProperties Props;
		Props.m_MaxWidth = Button.w;
		Props.m_EllipsisAtEnd = true;
		UI()->DoLabel(&Button, m_vpFilteredFileList[i]->m_aName, 10.0f, TEXTALIGN_ML, Props);

		if(!m_vpFilteredFileList[i]->m_IsLink && str_comp(m_vpFilteredFileList[i]->m_aFilename, "..") != 0)
		{
			char aBufTimeModified[64];
			str_timestamp_ex(m_vpFilteredFileList[i]->m_TimeModified, aBufTimeModified, sizeof(aBufTimeModified), "%d.%m.%Y %H:%M");
			UI()->DoLabel(&TimeModified, aBufTimeModified, 10.0f, TEXTALIGN_MR);
		}
	}

	const int NewSelection = s_ListBox.DoEnd();
	if(NewSelection != m_FilesSelectedIndex)
	{
		m_FilesSelectedIndex = NewSelection;
		str_copy(m_aFilesSelectedName, m_vpFilteredFileList[m_FilesSelectedIndex]->m_aName);
		const bool WasChanged = m_FileDialogFileNameInput.WasChanged();
		UpdateFileNameInput();
		if(!WasChanged) // ensure that changed flag is not set if it wasn't previously set, as this would reset the selection after DoEditBox is called
			m_FileDialogFileNameInput.WasChanged(); // this clears the changed flag
		m_FilePreviewState = PREVIEW_UNLOADED;
	}

	const float ButtonSpacing = ButtonBar.w > 600.0f ? 40.0f : 10.0f;

	// the buttons
	static int s_OkButton = 0;
	static int s_CancelButton = 0;
	static int s_RefreshButton = 0;
	static int s_ShowDirectoryButton = 0;
	static int s_DeleteButton = 0;
	static int s_NewFolderButton = 0;

	CUIRect Button;
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	const bool IsDir = m_FilesSelectedIndex >= 0 && m_vpFilteredFileList[m_FilesSelectedIndex]->m_IsDir;
	if(DoButton_Editor(&s_OkButton, IsDir ? "Open" : m_pFileDialogButtonText, 0, &Button, 0, nullptr) || s_ListBox.WasItemActivated() || (s_ListBox.Active() && UI()->ConsumeHotkey(CUI::HOTKEY_ENTER)))
	{
		if(IsDir) // folder
		{
			m_FileDialogFilterInput.Clear();
			const bool ParentFolder = str_comp(m_vpFilteredFileList[m_FilesSelectedIndex]->m_aFilename, "..") == 0;
			if(ParentFolder) // parent folder
			{
				str_copy(m_aFilesSelectedName, fs_filename(m_pFileDialogPath));
				str_append(m_aFilesSelectedName, "/");
				if(fs_parent_dir(m_pFileDialogPath))
				{
					if(str_comp(m_pFileDialogPath, m_aFileDialogCurrentFolder) == 0)
					{
						m_FileDialogShowingRoot = true;
						if(m_FileDialogStorageType == IStorage::TYPE_ALL)
						{
							m_aFilesSelectedName[0] = '\0'; // will select first list item
						}
						else
						{
							Storage()->GetCompletePath(m_FileDialogStorageType, m_pFileDialogPath, m_aFilesSelectedName, sizeof(m_aFilesSelectedName));
							str_append(m_aFilesSelectedName, "/");
						}
					}
					else
					{
						m_pFileDialogPath = m_aFileDialogCurrentFolder; // leave the link
						str_copy(m_aFilesSelectedName, m_aFileDialogCurrentLink);
						str_append(m_aFilesSelectedName, "/");
					}
				}
			}
			else // sub folder
			{
				if(m_vpFilteredFileList[m_FilesSelectedIndex]->m_IsLink)
				{
					m_pFileDialogPath = m_aFileDialogCurrentLink; // follow the link
					str_copy(m_aFileDialogCurrentLink, m_vpFilteredFileList[m_FilesSelectedIndex]->m_aFilename);
				}
				else
				{
					char aTemp[IO_MAX_PATH_LENGTH];
					str_copy(aTemp, m_pFileDialogPath);
					str_format(m_pFileDialogPath, IO_MAX_PATH_LENGTH, "%s/%s", aTemp, m_vpFilteredFileList[m_FilesSelectedIndex]->m_aFilename);
				}
				if(m_FileDialogShowingRoot)
					m_FileDialogStorageType = m_vpFilteredFileList[m_FilesSelectedIndex]->m_StorageType;
				m_FileDialogShowingRoot = false;
			}
			FilelistPopulate(m_FileDialogStorageType, ParentFolder);
			UpdateFileNameInput();
		}
		else // file
		{
			const int StorageType = m_FilesSelectedIndex >= 0 ? m_vpFilteredFileList[m_FilesSelectedIndex]->m_StorageType : m_FileDialogStorageType;
			str_format(m_aFileSaveName, sizeof(m_aFileSaveName), "%s/%s", m_pFileDialogPath, m_FileDialogFileNameInput.GetString());
			if(!str_endswith(m_aFileSaveName, FILETYPE_EXTENSIONS[m_FileDialogFileType]))
				str_append(m_aFileSaveName, FILETYPE_EXTENSIONS[m_FileDialogFileType]);
			if(!str_comp(m_pFileDialogButtonText, "Save") && Storage()->FileExists(m_aFileSaveName, StorageType))
			{
				m_PopupEventType = m_pfnFileDialogFunc == &CallbackSaveCopyMap ? POPEVENT_SAVE_COPY : POPEVENT_SAVE;
				m_PopupEventActivated = true;
			}
			else if(m_pfnFileDialogFunc)
				m_pfnFileDialogFunc(m_aFileSaveName, StorageType, m_pFileDialogUser);
		}
	}

	ButtonBar.VSplitRight(ButtonSpacing, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	if(DoButton_Editor(&s_CancelButton, "Cancel", 0, &Button, 0, nullptr) || (s_ListBox.Active() && UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE)))
		m_Dialog = DIALOG_NONE;

	ButtonBar.VSplitRight(ButtonSpacing, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	if(DoButton_Editor(&s_RefreshButton, "Refresh", 0, &Button, 0, nullptr) || (s_ListBox.Active() && (Input()->KeyIsPressed(KEY_F5) || (Input()->ModifierIsPressed() && Input()->KeyIsPressed(KEY_R)))))
		FilelistPopulate(m_FileDialogLastPopulatedStorageType, true);

	ButtonBar.VSplitRight(ButtonSpacing, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(90.0f, &ButtonBar, &Button);
	if(DoButton_Editor(&s_ShowDirectoryButton, "Show directory", 0, &Button, 0, "Open the current directory in the file browser"))
	{
		char aOpenPath[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(m_FilesSelectedIndex >= 0 ? m_vpFilteredFileList[m_FilesSelectedIndex]->m_StorageType : IStorage::TYPE_SAVE, m_pFileDialogPath, aOpenPath, sizeof(aOpenPath));
		if(!open_file(aOpenPath))
		{
			ShowFileDialogError("Failed to open the directory '%s'.", aOpenPath);
		}
	}

	ButtonBar.VSplitRight(ButtonSpacing, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	static CUI::SConfirmPopupContext s_ConfirmDeletePopupContext;
	if(m_FilesSelectedIndex >= 0 && m_vpFilteredFileList[m_FilesSelectedIndex]->m_StorageType == IStorage::TYPE_SAVE && !m_vpFilteredFileList[m_FilesSelectedIndex]->m_IsLink && str_comp(m_vpFilteredFileList[m_FilesSelectedIndex]->m_aFilename, "..") != 0)
	{
		if(DoButton_Editor(&s_DeleteButton, "Delete", 0, &Button, 0, nullptr) || (s_ListBox.Active() && UI()->ConsumeHotkey(CUI::HOTKEY_DELETE)))
		{
			s_ConfirmDeletePopupContext.Reset();
			s_ConfirmDeletePopupContext.YesNoButtons();
			str_format(s_ConfirmDeletePopupContext.m_aMessage, sizeof(s_ConfirmDeletePopupContext.m_aMessage), "Are you sure that you want to delete the %s '%s/%s'?", IsDir ? "folder" : "file", m_pFileDialogPath, m_vpFilteredFileList[m_FilesSelectedIndex]->m_aFilename);
			UI()->ShowPopupConfirm(UI()->MouseX(), UI()->MouseY(), &s_ConfirmDeletePopupContext);
		}
		if(s_ConfirmDeletePopupContext.m_Result == CUI::SConfirmPopupContext::CONFIRMED)
		{
			char aDeleteFilePath[IO_MAX_PATH_LENGTH];
			str_format(aDeleteFilePath, sizeof(aDeleteFilePath), "%s/%s", m_pFileDialogPath, m_vpFilteredFileList[m_FilesSelectedIndex]->m_aFilename);
			if(IsDir)
			{
				if(Storage()->RemoveFolder(aDeleteFilePath, IStorage::TYPE_SAVE))
					FilelistPopulate(m_FileDialogLastPopulatedStorageType, true);
				else
					ShowFileDialogError("Failed to delete folder '%s'. Make sure it's empty first.", aDeleteFilePath);
			}
			else
			{
				if(Storage()->RemoveFile(aDeleteFilePath, IStorage::TYPE_SAVE))
					FilelistPopulate(m_FileDialogLastPopulatedStorageType, true);
				else
					ShowFileDialogError("Failed to delete file '%s'.", aDeleteFilePath);
			}
		}
		if(s_ConfirmDeletePopupContext.m_Result != CUI::SConfirmPopupContext::UNSET)
			s_ConfirmDeletePopupContext.Reset();
	}
	else
		s_ConfirmDeletePopupContext.Reset();

	if(!m_FileDialogShowingRoot && m_FileDialogStorageType == IStorage::TYPE_SAVE)
	{
		ButtonBar.VSplitLeft(70.0f, &Button, &ButtonBar);
		if(DoButton_Editor(&s_NewFolderButton, "New folder", 0, &Button, 0, nullptr))
		{
			m_FileDialogNewFolderNameInput.Clear();
			static SPopupMenuId s_PopupNewFolderId;
			constexpr float PopupWidth = 400.0f;
			constexpr float PopupHeight = 110.0f;
			UI()->DoPopupMenu(&s_PopupNewFolderId, Width / 2.0f - PopupWidth / 2.0f, Height / 2.0f - PopupHeight / 2.0f, PopupWidth, PopupHeight, this, PopupNewFolder);
			UI()->SetActiveItem(&m_FileDialogNewFolderNameInput);
		}
	}
}

void CEditor::RefreshFilteredFileList()
{
	m_vpFilteredFileList.clear();
	for(const CFilelistItem &Item : m_vCompleteFileList)
	{
		if(m_FileDialogFilterInput.IsEmpty() || str_find_nocase(Item.m_aName, m_FileDialogFilterInput.GetString()))
		{
			m_vpFilteredFileList.push_back(&Item);
		}
	}
	if(!m_FileDialogShowingRoot)
		SortFilteredFileList();
	if(!m_vpFilteredFileList.empty())
	{
		if(m_aFilesSelectedName[0])
		{
			for(size_t i = 0; i < m_vpFilteredFileList.size(); i++)
			{
				if(m_aFilesSelectedName[0] && str_comp(m_vpFilteredFileList[i]->m_aName, m_aFilesSelectedName) == 0)
				{
					m_FilesSelectedIndex = i;
					break;
				}
			}
		}
		m_FilesSelectedIndex = clamp<int>(m_FilesSelectedIndex, 0, m_vpFilteredFileList.size() - 1);
		str_copy(m_aFilesSelectedName, m_vpFilteredFileList[m_FilesSelectedIndex]->m_aName);
	}
	else
	{
		m_FilesSelectedIndex = -1;
		m_aFilesSelectedName[0] = '\0';
	}
}

void CEditor::FilelistPopulate(int StorageType, bool KeepSelection)
{
	m_FileDialogLastPopulatedStorageType = StorageType;
	m_vCompleteFileList.clear();
	if(m_FileDialogShowingRoot)
	{
		{
			CFilelistItem Item;
			str_copy(Item.m_aFilename, m_pFileDialogPath);
			str_copy(Item.m_aName, "All combined");
			Item.m_IsDir = true;
			Item.m_IsLink = true;
			Item.m_StorageType = IStorage::TYPE_ALL;
			Item.m_TimeModified = 0;
			m_vCompleteFileList.push_back(Item);
		}

		for(int CheckStorageType = IStorage::TYPE_SAVE; CheckStorageType < Storage()->NumPaths(); ++CheckStorageType)
		{
			if(Storage()->FolderExists(m_pFileDialogPath, CheckStorageType))
			{
				CFilelistItem Item;
				str_copy(Item.m_aFilename, m_pFileDialogPath);
				Storage()->GetCompletePath(CheckStorageType, m_pFileDialogPath, Item.m_aName, sizeof(Item.m_aName));
				str_append(Item.m_aName, "/", sizeof(Item.m_aName));
				Item.m_IsDir = true;
				Item.m_IsLink = true;
				Item.m_StorageType = CheckStorageType;
				Item.m_TimeModified = 0;
				m_vCompleteFileList.push_back(Item);
			}
		}
	}
	else
	{
		// Add links for downloadedmaps and themes
		if(!str_comp(m_pFileDialogPath, "maps"))
		{
			if(str_comp(m_pFileDialogButtonText, "Save") != 0 && Storage()->FolderExists("downloadedmaps", StorageType))
			{
				CFilelistItem Item;
				str_copy(Item.m_aFilename, "downloadedmaps");
				str_copy(Item.m_aName, "downloadedmaps/");
				Item.m_IsDir = true;
				Item.m_IsLink = true;
				Item.m_StorageType = StorageType;
				Item.m_TimeModified = 0;
				m_vCompleteFileList.push_back(Item);
			}

			if(Storage()->FolderExists("themes", StorageType))
			{
				CFilelistItem Item;
				str_copy(Item.m_aFilename, "themes");
				str_copy(Item.m_aName, "themes/");
				Item.m_IsDir = true;
				Item.m_IsLink = true;
				Item.m_StorageType = StorageType;
				Item.m_TimeModified = 0;
				m_vCompleteFileList.push_back(Item);
			}
		}
		Storage()->ListDirectoryInfo(StorageType, m_pFileDialogPath, EditorListdirCallback, this);
	}
	RefreshFilteredFileList();
	if(!KeepSelection)
	{
		m_FilesSelectedIndex = m_vpFilteredFileList.empty() ? -1 : 0;
		if(m_FilesSelectedIndex >= 0)
			str_copy(m_aFilesSelectedName, m_vpFilteredFileList[m_FilesSelectedIndex]->m_aName);
		else
			m_aFilesSelectedName[0] = '\0';
	}
	m_FilePreviewState = PREVIEW_UNLOADED;
}

void CEditor::InvokeFileDialog(int StorageType, int FileType, const char *pTitle, const char *pButtonText,
	const char *pBasePath, bool FilenameAsDefault,
	bool (*pfnFunc)(const char *pFileName, int StorageType, void *pUser), void *pUser)
{
	m_FileDialogStorageType = StorageType;
	if(m_FileDialogStorageType == IStorage::TYPE_ALL)
	{
		int NumStoragedWithFolder = 0;
		for(int CheckStorageType = IStorage::TYPE_SAVE; CheckStorageType < Storage()->NumPaths(); ++CheckStorageType)
		{
			if(Storage()->FolderExists(m_pFileDialogPath, CheckStorageType))
			{
				NumStoragedWithFolder++;
			}
		}
		m_FileDialogMultipleStorages = NumStoragedWithFolder > 1;
	}
	else
	{
		m_FileDialogMultipleStorages = false;
	}

	UI()->ClosePopupMenus();
	m_pFileDialogTitle = pTitle;
	m_pFileDialogButtonText = pButtonText;
	m_pfnFileDialogFunc = pfnFunc;
	m_pFileDialogUser = pUser;
	m_FileDialogFileNameInput.Clear();
	m_FileDialogFilterInput.Clear();
	m_aFileDialogCurrentFolder[0] = 0;
	m_aFileDialogCurrentLink[0] = 0;
	m_pFileDialogPath = m_aFileDialogCurrentFolder;
	m_FileDialogFileType = FileType;
	m_FilePreviewState = PREVIEW_UNLOADED;
	m_FileDialogOpening = true;
	m_FileDialogShowingRoot = false;

	if(FilenameAsDefault)
	{
		char aDefaultName[IO_MAX_PATH_LENGTH];
		fs_split_file_extension(fs_filename(m_aFileName), aDefaultName, sizeof(aDefaultName));
		m_FileDialogFileNameInput.Set(aDefaultName);
	}
	if(pBasePath)
		str_copy(m_aFileDialogCurrentFolder, pBasePath);

	FilelistPopulate(m_FileDialogStorageType);

	m_FileDialogOpening = true;
	m_Dialog = DIALOG_FILE;
}

void CEditor::ShowFileDialogError(const char *pFormat, ...)
{
	char aMessage[1024];
	va_list VarArgs;
	va_start(VarArgs, pFormat);
	str_format_v(aMessage, sizeof(aMessage), pFormat, VarArgs);
	va_end(VarArgs);

	auto ContextIterator = m_PopupMessageContexts.find(aMessage);
	CUI::SMessagePopupContext *pContext;
	if(ContextIterator != m_PopupMessageContexts.end())
	{
		pContext = ContextIterator->second;
		UI()->ClosePopupMenu(pContext);
	}
	else
	{
		pContext = new CUI::SMessagePopupContext();
		pContext->ErrorColor();
		str_copy(pContext->m_aMessage, aMessage);
		m_PopupMessageContexts[pContext->m_aMessage] = pContext;
	}
	UI()->ShowPopupMessage(UI()->MouseX(), UI()->MouseY(), pContext);
}

void CEditor::RenderModebar(CUIRect View)
{
	CUIRect Mentions, IngameMoved, ModeButton;
	View.HSplitTop(12.0f, &Mentions, &View);
	View.HSplitTop(12.0f, &IngameMoved, &View);
	View.HSplitTop(8.0f, nullptr, &ModeButton);
	ModeButton.VSplitLeft(65.0f, &ModeButton, nullptr);

	// mentions
	if(m_Mentions)
	{
		char aBuf[64];
		if(m_Mentions == 1)
			str_copy(aBuf, Localize("1 new mention"));
		else if(m_Mentions <= 9)
			str_format(aBuf, sizeof(aBuf), Localize("%d new mentions"), m_Mentions);
		else
			str_copy(aBuf, Localize("9+ new mentions"));

		TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		UI()->DoLabel(&Mentions, aBuf, 10.0f, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	// ingame moved warning
	if(m_IngameMoved)
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		UI()->DoLabel(&IngameMoved, Localize("Moved ingame"), 10.0f, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	// mode button
	{
		const char *pModeLabel = "";
		if(m_Mode == MODE_LAYERS)
			pModeLabel = "Layers";
		else if(m_Mode == MODE_IMAGES)
			pModeLabel = "Images";
		else if(m_Mode == MODE_SOUNDS)
			pModeLabel = "Sounds";
		else
			dbg_assert(false, "m_Mode invalid");

		static int s_ModeButton = 0;
		const int MouseButton = DoButton_Ex(&s_ModeButton, pModeLabel, 0, &ModeButton, 0, "Switch between images, sounds and layers management.", IGraphics::CORNER_T, 10.0f);
		if(MouseButton == 2 || (Input()->KeyPress(KEY_LEFT) && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr))
		{
			m_Mode = (m_Mode + NUM_MODES - 1) % NUM_MODES;
		}
		else if(MouseButton == 1 || (Input()->KeyPress(KEY_RIGHT) && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr))
		{
			m_Mode = (m_Mode + 1) % NUM_MODES;
		}
	}
}

void CEditor::RenderStatusbar(CUIRect View, CUIRect *pTooltipRect)
{
	const bool ButtonsDisabled = m_ShowPicker;

	CUIRect Button;
	View.VSplitRight(100.0f, &View, &Button);
	static int s_EnvelopeButton = 0;
	if(DoButton_Editor(&s_EnvelopeButton, "Envelopes", ButtonsDisabled ? -1 : m_ActiveExtraEditor == EXTRAEDITOR_ENVELOPES, &Button, 0, "Toggles the envelope editor.") == 1)
	{
		m_ActiveExtraEditor = m_ActiveExtraEditor == EXTRAEDITOR_ENVELOPES ? EXTRAEDITOR_NONE : EXTRAEDITOR_ENVELOPES;
	}

	View.VSplitRight(10.0f, &View, nullptr);
	View.VSplitRight(100.0f, &View, &Button);
	static int s_SettingsButton = 0;
	if(DoButton_Editor(&s_SettingsButton, "Server settings", ButtonsDisabled ? -1 : m_ActiveExtraEditor == EXTRAEDITOR_SERVER_SETTINGS, &Button, 0, "Toggles the server settings editor.") == 1)
	{
		m_ActiveExtraEditor = m_ActiveExtraEditor == EXTRAEDITOR_SERVER_SETTINGS ? EXTRAEDITOR_NONE : EXTRAEDITOR_SERVER_SETTINGS;
	}

	View.VSplitRight(10.0f, pTooltipRect, nullptr);
}

void CEditor::RenderTooltip(CUIRect TooltipRect)
{
	if(m_pTooltip == nullptr)
		return;

	char aBuf[512];
	if(ms_pUiGotContext && ms_pUiGotContext == UI()->HotItem())
		str_format(aBuf, sizeof(aBuf), "%s Right click for context menu.", m_pTooltip);
	else
		str_copy(aBuf, m_pTooltip);

	SLabelProperties Props;
	Props.m_MaxWidth = TooltipRect.w;
	Props.m_EllipsisAtEnd = true;
	UI()->DoLabel(&TooltipRect, aBuf, 10.0f, TEXTALIGN_ML, Props);
}

bool CEditor::IsEnvelopeUsed(int EnvelopeIndex) const
{
	for(const auto &pGroup : m_Map.m_vpGroups)
	{
		for(const auto &pLayer : pGroup->m_vpLayers)
		{
			if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(pLayer);
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
				std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(pLayer);
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
				std::shared_ptr<CLayerTiles> pLayerTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
				if(pLayerTiles->m_ColorEnv == EnvelopeIndex)
					return true;
			}
		}
	}
	return false;
}

void CEditor::RemoveUnusedEnvelopes()
{
	for(size_t Envelope = 0; Envelope < m_Map.m_vpEnvelopes.size();)
	{
		if(IsEnvelopeUsed(Envelope))
		{
			++Envelope;
		}
		else
		{
			m_Map.DeleteEnvelope(Envelope);
		}
	}
}

void CEditor::ZoomAdaptOffsetX(float ZoomFactor, const CUIRect &View)
{
	float PosX = g_Config.m_EdZoomTarget ? (UI()->MouseX() - View.x) / View.w : 0.5f;
	m_OffsetEnvelopeX = PosX - (PosX - m_OffsetEnvelopeX) * ZoomFactor;
}

void CEditor::UpdateZoomEnvelopeX(const CUIRect &View)
{
	float OldZoom = m_ZoomEnvelopeX.GetValue();
	if(m_ZoomEnvelopeX.UpdateValue())
		ZoomAdaptOffsetX(OldZoom / m_ZoomEnvelopeX.GetValue(), View);
}

void CEditor::ZoomAdaptOffsetY(float ZoomFactor, const CUIRect &View)
{
	float PosY = g_Config.m_EdZoomTarget ? 1.0f - (UI()->MouseY() - View.y) / View.h : 0.5f;
	m_OffsetEnvelopeY = PosY - (PosY - m_OffsetEnvelopeY) * ZoomFactor;
}

void CEditor::UpdateZoomEnvelopeY(const CUIRect &View)
{
	float OldZoom = m_ZoomEnvelopeY.GetValue();
	if(m_ZoomEnvelopeY.UpdateValue())
		ZoomAdaptOffsetY(OldZoom / m_ZoomEnvelopeY.GetValue(), View);
}

void CEditor::ResetZoomEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope, int ActiveChannels)
{
	pEnvelope->FindTopBottom(ActiveChannels);
	float Top = pEnvelope->m_Top;
	float Bottom = pEnvelope->m_Bottom;
	float EndTime = pEnvelope->EndTime();
	float ValueRange = absolute(Top - Bottom);

	if(ValueRange < m_ZoomEnvelopeY.GetMinValue())
	{
		// Set view to some sane default if range is too small
		m_OffsetEnvelopeY = 0.5f - ValueRange / m_ZoomEnvelopeY.GetMinValue() / 2.0f - Bottom / m_ZoomEnvelopeY.GetMinValue();
		m_ZoomEnvelopeY.SetValueInstant(m_ZoomEnvelopeY.GetMinValue());
	}
	else if(ValueRange > m_ZoomEnvelopeY.GetMaxValue())
	{
		m_OffsetEnvelopeY = -Bottom / m_ZoomEnvelopeY.GetMaxValue();
		m_ZoomEnvelopeY.SetValueInstant(m_ZoomEnvelopeY.GetMaxValue());
	}
	else
	{
		// calculate biggest possible spacing
		float SpacingFactor = minimum(1.25f, m_ZoomEnvelopeY.GetMaxValue() / ValueRange);
		m_ZoomEnvelopeY.SetValueInstant(SpacingFactor * ValueRange);
		float Space = 1.0f / SpacingFactor;
		float Spacing = (1.0f - Space) / 2.0f;

		if(Top >= 0 && Bottom >= 0)
			m_OffsetEnvelopeY = Spacing - Bottom / m_ZoomEnvelopeY.GetValue();
		else if(Top <= 0 && Bottom <= 0)
			m_OffsetEnvelopeY = Spacing - Bottom / m_ZoomEnvelopeY.GetValue();
		else
			m_OffsetEnvelopeY = Spacing + Space * absolute(Bottom) / ValueRange;
	}

	if(EndTime < m_ZoomEnvelopeX.GetMinValue())
	{
		m_OffsetEnvelopeX = 0.5f - EndTime / m_ZoomEnvelopeX.GetMinValue();
		m_ZoomEnvelopeX.SetValueInstant(m_ZoomEnvelopeX.GetMinValue());
	}
	else if(EndTime > m_ZoomEnvelopeX.GetMaxValue())
	{
		m_OffsetEnvelopeX = 0.0f;
		m_ZoomEnvelopeX.SetValueInstant(m_ZoomEnvelopeX.GetMaxValue());
	}
	else
	{
		float SpacingFactor = minimum(1.25f, m_ZoomEnvelopeX.GetMaxValue() / EndTime);
		m_ZoomEnvelopeX.SetValueInstant(SpacingFactor * EndTime);
		float Space = 1.0f / SpacingFactor;
		float Spacing = (1.0f - Space) / 2.0f;

		m_OffsetEnvelopeX = Spacing;
	}
}

float fxt2f(int t)
{
	return t / 1000.0f;
}

int f2fxt(float t)
{
	return static_cast<int>(t * 1000.0f);
}

float CEditor::ScreenToEnvelopeX(const CUIRect &View, float x) const
{
	return (x - View.x - View.w * m_OffsetEnvelopeX) / View.w * m_ZoomEnvelopeX.GetValue();
}

float CEditor::EnvelopeToScreenX(const CUIRect &View, float x) const
{
	return View.x + View.w * m_OffsetEnvelopeX + x / m_ZoomEnvelopeX.GetValue() * View.w;
}

float CEditor::ScreenToEnvelopeY(const CUIRect &View, float y) const
{
	return (View.h - y + View.y) / View.h * m_ZoomEnvelopeY.GetValue() - m_OffsetEnvelopeY * m_ZoomEnvelopeY.GetValue();
}

float CEditor::EnvelopeToScreenY(const CUIRect &View, float y) const
{
	return View.y + View.h - y / m_ZoomEnvelopeY.GetValue() * View.h - m_OffsetEnvelopeY * View.h;
}

float CEditor::ScreenToEnvelopeDX(const CUIRect &View, float dx)
{
	return dx / Graphics()->ScreenWidth() * UI()->Screen()->w / View.w * m_ZoomEnvelopeX.GetValue();
}

float CEditor::ScreenToEnvelopeDY(const CUIRect &View, float dy)
{
	return dy / Graphics()->ScreenHeight() * UI()->Screen()->h / View.h * m_ZoomEnvelopeY.GetValue();
}

void CEditor::RemoveTimeOffsetEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope)
{
	int TimeOffset = pEnvelope->m_vPoints[0].m_Time;
	for(auto &Point : pEnvelope->m_vPoints)
		Point.m_Time -= TimeOffset;

	m_OffsetEnvelopeX += fxt2f(TimeOffset) / m_ZoomEnvelopeX.GetValue();
};

static float ClampDelta(float Val, float Delta, float Min, float Max)
{
	if(Val + Delta <= Min)
		return Min - Val;
	if(Val + Delta >= Max)
		return Max - Val;
	return Delta;
}

class CTimeStep
{
public:
	template<class T>
	CTimeStep(T t)
	{
		if constexpr(std::is_same_v<T, std::chrono::milliseconds>)
			m_Unit = ETimeUnit::MILLISECONDS;
		else if constexpr(std::is_same_v<T, std::chrono::seconds>)
			m_Unit = ETimeUnit::SECONDS;
		else
			m_Unit = ETimeUnit::MINUTES;

		m_Value = t;
	}

	CTimeStep operator*(int k) const
	{
		return CTimeStep(m_Value * k, m_Unit);
	}

	CTimeStep operator-(const CTimeStep &Other)
	{
		return CTimeStep(m_Value - Other.m_Value, m_Unit);
	}

	void Format(char *pBuffer, size_t BufferSize)
	{
		int Milliseconds = m_Value.count() % 1000;
		int Seconds = std::chrono::duration_cast<std::chrono::seconds>(m_Value).count() % 60;
		int Minutes = std::chrono::duration_cast<std::chrono::minutes>(m_Value).count();

		switch(m_Unit)
		{
		case ETimeUnit::MILLISECONDS:
			if(Minutes != 0)
				str_format(pBuffer, BufferSize, "%d:%02d.%03dmin", Minutes, Seconds, Milliseconds);
			else if(Seconds != 0)
				str_format(pBuffer, BufferSize, "%d.%03ds", Seconds, Milliseconds);
			else
				str_format(pBuffer, BufferSize, "%dms", Milliseconds);
			break;
		case ETimeUnit::SECONDS:
			if(Minutes != 0)
				str_format(pBuffer, BufferSize, "%d:%02dmin", Minutes, Seconds);
			else
				str_format(pBuffer, BufferSize, "%ds", Seconds);
			break;
		case ETimeUnit::MINUTES:
			str_format(pBuffer, BufferSize, "%dmin", Minutes);
			break;
		}
	}

	float AsSeconds() const
	{
		return std::chrono::duration_cast<std::chrono::duration<float>>(m_Value).count();
	}

private:
	enum class ETimeUnit
	{
		MILLISECONDS,
		SECONDS,
		MINUTES
	} m_Unit;
	std::chrono::milliseconds m_Value;

	CTimeStep(std::chrono::milliseconds Value, ETimeUnit Unit)
	{
		m_Value = Value;
		m_Unit = Unit;
	}
};

void CEditor::SetHotEnvelopePoint(const CUIRect &View, const std::shared_ptr<CEnvelope> &pEnvelope, int ActiveChannels)
{
	if(!UI()->MouseInside(&View))
		return;

	float mx = UI()->MouseX();
	float my = UI()->MouseY();

	float MinDist = 200.0f;
	int *pMinPoint = nullptr;

	auto UpdateMinimum = [&](float px, float py, int *pID) {
		float dx = px - mx;
		float dy = py - my;

		float CurrDist = dx * dx + dy * dy;
		if(CurrDist < MinDist)
		{
			MinDist = CurrDist;
			pMinPoint = pID;
		}
	};

	for(size_t i = 0; i < pEnvelope->m_vPoints.size(); i++)
	{
		for(int c = pEnvelope->GetChannels() - 1; c >= 0; c--)
		{
			if(!(ActiveChannels & (1 << c)))
				continue;

			if(i > 0 && pEnvelope->m_vPoints[i - 1].m_Curvetype == CURVETYPE_BEZIER)
			{
				float px = EnvelopeToScreenX(View, fxt2f(pEnvelope->m_vPoints[i].m_Time + pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c]));
				float py = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c] + pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaY[c]));
				UpdateMinimum(px, py, &pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c]);
			}

			if(pEnvelope->m_vPoints[i].m_Curvetype == CURVETYPE_BEZIER)
			{
				float px = EnvelopeToScreenX(View, fxt2f(pEnvelope->m_vPoints[i].m_Time + pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c]));
				float py = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c] + pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaY[c]));
				UpdateMinimum(px, py, &pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c]);
			}

			float px = EnvelopeToScreenX(View, fxt2f(pEnvelope->m_vPoints[i].m_Time));
			float py = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c]));
			UpdateMinimum(px, py, &pEnvelope->m_vPoints[i].m_aValues[c]);
		}
	}

	if(pMinPoint != nullptr)
		UI()->SetHotItem(pMinPoint);
}

void CEditor::RenderEnvelopeEditor(CUIRect View)
{
	if(m_SelectedEnvelope < 0)
		m_SelectedEnvelope = 0;
	if(m_SelectedEnvelope >= (int)m_Map.m_vpEnvelopes.size())
		m_SelectedEnvelope = m_Map.m_vpEnvelopes.size() - 1;

	std::shared_ptr<CEnvelope> pEnvelope = nullptr;
	if(m_SelectedEnvelope >= 0 && m_SelectedEnvelope < (int)m_Map.m_vpEnvelopes.size())
		pEnvelope = m_Map.m_vpEnvelopes[m_SelectedEnvelope];

	enum
	{
		OP_NONE,
		OP_SELECT,
		OP_DRAG_POINT,
		OP_DRAG_POINT_X,
		OP_DRAG_POINT_Y,
		OP_CONTEXT_MENU,
		OP_BOX_SELECT,
		OP_SCALE
	};
	static int s_Operation = OP_NONE;
	static std::vector<float> s_vAccurateDragValuesX = {};
	static std::vector<float> s_vAccurateDragValuesY = {};
	static float s_MouseXStart = 0.0f;
	static float s_MouseYStart = 0.0f;

	CUIRect ToolBar, CurveBar, ColorBar, DragBar;
	View.HSplitTop(30.0f, &DragBar, nullptr);
	DragBar.y -= 2.0f;
	DragBar.w += 2.0f;
	DragBar.h += 4.0f;
	RenderExtraEditorDragBar(View, DragBar);
	View.HSplitTop(15.0f, &ToolBar, &View);
	View.HSplitTop(15.0f, &CurveBar, &View);
	ToolBar.Margin(2.0f, &ToolBar);
	CurveBar.Margin(2.0f, &CurveBar);

	bool CurrentEnvelopeSwitched = false;

	// do the toolbar
	static int s_ActiveChannels = 0xf;
	{
		CUIRect Button;
		std::shared_ptr<CEnvelope> pNewEnv = nullptr;

		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_NewSoundButton = 0;
		if(DoButton_Editor(&s_NewSoundButton, "Sound+", 0, &Button, 0, "Creates a new sound envelope"))
		{
			m_Map.OnModify();
			pNewEnv = m_Map.NewEnvelope(1);
		}

		ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_New4dButton = 0;
		if(DoButton_Editor(&s_New4dButton, "Color+", 0, &Button, 0, "Creates a new color envelope"))
		{
			m_Map.OnModify();
			pNewEnv = m_Map.NewEnvelope(4);
		}

		ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_New2dButton = 0;
		if(DoButton_Editor(&s_New2dButton, "Pos.+", 0, &Button, 0, "Creates a new position envelope"))
		{
			m_Map.OnModify();
			pNewEnv = m_Map.NewEnvelope(3);
		}

		if(m_SelectedEnvelope >= 0)
		{
			// Delete button
			ToolBar.VSplitRight(10.0f, &ToolBar, nullptr);
			ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
			static int s_DeleteButton = 0;
			if(DoButton_Editor(&s_DeleteButton, "✗", 0, &Button, 0, "Delete this envelope"))
			{
				m_Map.DeleteEnvelope(m_SelectedEnvelope);
				if(m_SelectedEnvelope >= (int)m_Map.m_vpEnvelopes.size())
					m_SelectedEnvelope = m_Map.m_vpEnvelopes.size() - 1;
				pEnvelope = m_SelectedEnvelope >= 0 ? m_Map.m_vpEnvelopes[m_SelectedEnvelope] : nullptr;
			}

			// Move right button
			ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
			ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
			static int s_MoveRightButton = 0;
			if(DoButton_Ex(&s_MoveRightButton, "→", 0, &Button, 0, "Move this envelope to the right", IGraphics::CORNER_R))
			{
				m_Map.SwapEnvelopes(m_SelectedEnvelope, m_SelectedEnvelope + 1);
				m_SelectedEnvelope = clamp<int>(m_SelectedEnvelope + 1, 0, m_Map.m_vpEnvelopes.size() - 1);
				pEnvelope = m_Map.m_vpEnvelopes[m_SelectedEnvelope];
			}

			// Move left button
			ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
			static int s_MoveLeftButton = 0;
			if(DoButton_Ex(&s_MoveLeftButton, "←", 0, &Button, 0, "Move this envelope to the left", IGraphics::CORNER_L))
			{
				m_Map.SwapEnvelopes(m_SelectedEnvelope - 1, m_SelectedEnvelope);
				m_SelectedEnvelope = clamp<int>(m_SelectedEnvelope - 1, 0, m_Map.m_vpEnvelopes.size() - 1);
				pEnvelope = m_Map.m_vpEnvelopes[m_SelectedEnvelope];
			}

			if(pEnvelope)
			{
				ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
				ToolBar.VSplitRight(20.0f, &ToolBar, &Button);
				static int s_ResetZoomButton = 0;
				if(DoButton_FontIcon(&s_ResetZoomButton, FONT_ICON_MAGNIFYING_GLASS, 0, &Button, 0, "Reset zoom to default value", IGraphics::CORNER_ALL, 9.0f))
					ResetZoomEnvelope(pEnvelope, s_ActiveChannels);
			}

			// Margin on the right side
			ToolBar.VSplitRight(7.0f, &ToolBar, nullptr);
		}

		if(pNewEnv) // add the default points
		{
			if(pNewEnv->GetChannels() == 4)
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
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%d/%d", m_SelectedEnvelope + 1, (int)m_Map.m_vpEnvelopes.size());

		ColorRGBA EnvColor = ColorRGBA(1, 1, 1, 0.5f);
		if(!m_Map.m_vpEnvelopes.empty())
		{
			EnvColor = IsEnvelopeUsed(m_SelectedEnvelope) ?
					   ColorRGBA(1, 0.7f, 0.7f, 0.5f) :
					   ColorRGBA(0.7f, 1, 0.7f, 0.5f);
		}

		static int s_EnvelopeSelector = 0;
		int NewValue = UiDoValueSelector(&s_EnvelopeSelector, &Shifter, aBuf, m_SelectedEnvelope + 1, 1, m_Map.m_vpEnvelopes.size(), 1, 1.0f, "Select Envelope", false, false, IGraphics::CORNER_NONE, &EnvColor, false);
		if(NewValue - 1 != m_SelectedEnvelope)
		{
			m_SelectedEnvelope = NewValue - 1;
			CurrentEnvelopeSwitched = true;
		}

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
			ToolBar.VSplitLeft(15.0f, nullptr, &ToolBar);
			ToolBar.VSplitLeft(40.0f, &Button, &ToolBar);
			UI()->DoLabel(&Button, "Name:", 10.0f, TEXTALIGN_MR);

			ToolBar.VSplitLeft(3.0f, nullptr, &ToolBar);
			ToolBar.VSplitLeft(ToolBar.w > ToolBar.h * 40 ? 80.0f : 60.0f, &Button, &ToolBar);

			static CLineInput s_NameInput;
			s_NameInput.SetBuffer(pEnvelope->m_aName, sizeof(pEnvelope->m_aName));
			if(DoEditBox(&s_NameInput, &Button, 10.0f, IGraphics::CORNER_ALL, "The name of the selected envelope"))
			{
				m_Map.OnModify();
			}
		}
	}

	bool ShowColorBar = false;
	if(pEnvelope && pEnvelope->GetChannels() == 4)
	{
		ShowColorBar = true;
		View.HSplitTop(20.0f, &ColorBar, &View);
		ColorBar.Margin(2.0f, &ColorBar);
	}

	RenderBackground(View, m_CheckerTexture, 32.0f, 0.1f);

	if(pEnvelope)
	{
		if(m_ResetZoomEnvelope)
		{
			m_ResetZoomEnvelope = false;
			ResetZoomEnvelope(pEnvelope, s_ActiveChannels);
		}

		static int s_EnvelopeEditorID = 0;

		ColorRGBA aColors[] = {ColorRGBA(1, 0.2f, 0.2f), ColorRGBA(0.2f, 1, 0.2f), ColorRGBA(0.2f, 0.2f, 1), ColorRGBA(1, 1, 0.2f)};

		CUIRect Button;

		ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);

		static const char *s_aapNames[4][CEnvPoint::MAX_CHANNELS] = {
			{"V", "", "", ""},
			{"", "", "", ""},
			{"X", "Y", "R", ""},
			{"R", "G", "B", "A"},
		};

		static const char *s_aapDescriptions[4][CEnvPoint::MAX_CHANNELS] = {
			{"Volume of the envelope", "", "", ""},
			{"", "", "", ""},
			{"X-axis of the envelope", "Y-axis of the envelope", "Rotation of the envelope", ""},
			{"Red value of the envelope", "Green value of the envelope", "Blue value of the envelope", "Alpha value of the envelope"},
		};

		static int s_aChannelButtons[CEnvPoint::MAX_CHANNELS] = {0};
		int Bit = 1;

		for(int i = 0; i < CEnvPoint::MAX_CHANNELS; i++, Bit <<= 1)
		{
			ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);
			if(i < pEnvelope->GetChannels())
			{
				int Corners = IGraphics::CORNER_NONE;
				if(pEnvelope->GetChannels() == 1)
					Corners = IGraphics::CORNER_ALL;
				else if(i == 0)
					Corners = IGraphics::CORNER_L;
				else if(i == pEnvelope->GetChannels() - 1)
					Corners = IGraphics::CORNER_R;

				if(DoButton_Env(&s_aChannelButtons[i], s_aapNames[pEnvelope->GetChannels() - 1][i], s_ActiveChannels & Bit, &Button, s_aapDescriptions[pEnvelope->GetChannels() - 1][i], aColors[i], Corners))
					s_ActiveChannels ^= Bit;
			}
		}

		// sync checkbox
		ToolBar.VSplitLeft(15.0f, nullptr, &ToolBar);
		ToolBar.VSplitLeft(12.0f, &Button, &ToolBar);
		static int s_SyncButton;
		if(DoButton_Editor(&s_SyncButton, pEnvelope->m_Synchronized ? "X" : "", 0, &Button, 0, "Synchronize envelope animation to game time (restarts when you touch the start line)"))
			pEnvelope->m_Synchronized = !pEnvelope->m_Synchronized;

		ToolBar.VSplitLeft(4.0f, nullptr, &ToolBar);
		ToolBar.VSplitLeft(40.0f, &Button, &ToolBar);
		UI()->DoLabel(&Button, "Sync.", 10.0f, TEXTALIGN_ML);

		if(UI()->MouseInside(&View))
		{
			UI()->SetHotItem(&s_EnvelopeEditorID);

			if(s_Operation == OP_NONE && (UI()->MouseButton(2) || (UI()->MouseButton(0) && Input()->ModifierIsPressed())))
			{
				m_OffsetEnvelopeX += UI()->MouseDeltaX() / Graphics()->ScreenWidth() * UI()->Screen()->w / View.w;
				m_OffsetEnvelopeY -= UI()->MouseDeltaY() / Graphics()->ScreenHeight() * UI()->Screen()->h / View.h;
			}
			if(Input()->ShiftIsPressed())
			{
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
					m_ZoomEnvelopeY.ChangeValue(0.1f * m_ZoomEnvelopeY.GetValue());
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
					m_ZoomEnvelopeY.ChangeValue(-0.1f * m_ZoomEnvelopeY.GetValue());
			}
			else
			{
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
					m_ZoomEnvelopeX.ChangeValue(0.1f * m_ZoomEnvelopeX.GetValue());
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
					m_ZoomEnvelopeX.ChangeValue(-0.1f * m_ZoomEnvelopeX.GetValue());
			}
		}

		if(UI()->HotItem() == &s_EnvelopeEditorID)
		{
			// do stuff
			if(UI()->MouseButton(0))
			{
				if(Input()->MouseDoubleClick())
				{
					// add point
					float Time = ScreenToEnvelopeX(View, UI()->MouseX());
					ColorRGBA Channels;
					if(in_range(Time, 0.0f, pEnvelope->EndTime()))
						pEnvelope->Eval(Time, Channels);
					else
						Channels = {0, 0, 0, 0};

					int FixedTime = std::round(Time * 1000.0f);
					bool TimeFound = false;
					for(CEnvPoint &Point : pEnvelope->m_vPoints)
					{
						if(Point.m_Time == FixedTime)
							TimeFound = true;
					}

					if(!TimeFound)
						pEnvelope->AddPoint(FixedTime,
							f2fx(Channels.r), f2fx(Channels.g),
							f2fx(Channels.b), f2fx(Channels.a));

					if(FixedTime < 0)
						RemoveTimeOffsetEnvelope(pEnvelope);
					m_Map.OnModify();
				}
				else if(s_Operation != OP_BOX_SELECT && !Input()->ModifierIsPressed())
				{
					static int s_BoxSelectID = 0;
					UI()->SetActiveItem(&s_BoxSelectID);
					s_Operation = OP_BOX_SELECT;
					s_MouseXStart = UI()->MouseX();
					s_MouseYStart = UI()->MouseY();
				}
			}

			m_ShowEnvelopePreview = SHOWENV_SELECTED;
			m_pTooltip = "Double-click to create a new point. Use shift to change the zoom axis. Press S to scale selected envelope points.";
		}

		UpdateZoomEnvelopeX(View);
		UpdateZoomEnvelopeY(View);

		{
			float UnitsPerLineY = 0.001f;
			static const float s_aUnitPerLineOptionsY[] = {0.005f, 0.01f, 0.025f, 0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 2 * 32.0f, 5 * 32.0f, 10 * 32.0f, 20 * 32.0f, 50 * 32.0f, 100 * 32.0f};
			for(float Value : s_aUnitPerLineOptionsY)
			{
				if(Value / m_ZoomEnvelopeY.GetValue() * View.h < 40.0f)
					UnitsPerLineY = Value;
			}
			int NumLinesY = m_ZoomEnvelopeY.GetValue() / static_cast<float>(UnitsPerLineY) + 1;

			UI()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.2f);

			float BaseValue = static_cast<int>(m_OffsetEnvelopeY * m_ZoomEnvelopeY.GetValue() / UnitsPerLineY) * UnitsPerLineY;
			for(int i = 0; i <= NumLinesY; i++)
			{
				float Value = UnitsPerLineY * i - BaseValue;
				IGraphics::CLineItem LineItem(View.x, EnvelopeToScreenY(View, Value), View.x + View.w, EnvelopeToScreenY(View, Value));
				Graphics()->LinesDraw(&LineItem, 1);
			}

			Graphics()->LinesEnd();

			UI()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.4f);
			for(int i = 0; i <= NumLinesY; i++)
			{
				float Value = UnitsPerLineY * i - BaseValue;
				char aValueBuffer[16];
				if(UnitsPerLineY >= 1.0f)
				{
					str_format(aValueBuffer, sizeof(aValueBuffer), "%d", static_cast<int>(Value));
				}
				else
				{
					str_format(aValueBuffer, sizeof(aValueBuffer), "%.3f", Value);
				}
				UI()->TextRender()->Text(View.x, EnvelopeToScreenY(View, Value) + 4.0f, 8.0f, aValueBuffer);
			}
			UI()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			UI()->ClipDisable();
		}

		{
			CTimeStep UnitsPerLineX = 1ms;
			static const CTimeStep s_aUnitPerLineOptionsX[] = {5ms, 10ms, 25ms, 50ms, 100ms, 250ms, 500ms, 1s, 2s, 5s, 10s, 15s, 30s, 1min};
			for(CTimeStep Value : s_aUnitPerLineOptionsX)
			{
				if(Value.AsSeconds() / m_ZoomEnvelopeX.GetValue() * View.w < 160.0f)
					UnitsPerLineX = Value;
			}
			int NumLinesX = m_ZoomEnvelopeX.GetValue() / static_cast<float>(UnitsPerLineX.AsSeconds()) + 1;

			UI()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.2f);

			CTimeStep BaseValue = UnitsPerLineX * static_cast<int>(m_OffsetEnvelopeX * m_ZoomEnvelopeX.GetValue() / UnitsPerLineX.AsSeconds());
			for(int i = 0; i <= NumLinesX; i++)
			{
				float Value = UnitsPerLineX.AsSeconds() * i - BaseValue.AsSeconds();
				IGraphics::CLineItem LineItem(EnvelopeToScreenX(View, Value), View.y, EnvelopeToScreenX(View, Value), View.y + View.h);
				Graphics()->LinesDraw(&LineItem, 1);
			}

			Graphics()->LinesEnd();

			UI()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.4f);
			for(int i = 0; i <= NumLinesX; i++)
			{
				CTimeStep Value = UnitsPerLineX * i - BaseValue;
				if(Value.AsSeconds() >= 0)
				{
					char aValueBuffer[16];
					Value.Format(aValueBuffer, sizeof(aValueBuffer));

					UI()->TextRender()->Text(EnvelopeToScreenX(View, Value.AsSeconds()) + 1.0f, View.y + View.h - 8.0f, 8.0f, aValueBuffer);
				}
			}
			UI()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			UI()->ClipDisable();
		}

		// render tangents for bezier curves
		{
			UI()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			for(int c = 0; c < pEnvelope->GetChannels(); c++)
			{
				if(!(s_ActiveChannels & (1 << c)))
					continue;

				for(int i = 0; i < (int)pEnvelope->m_vPoints.size(); i++)
				{
					float PosX = EnvelopeToScreenX(View, fxt2f(pEnvelope->m_vPoints[i].m_Time));
					float PosY = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c]));

					// Out-Tangent
					if(pEnvelope->m_vPoints[i].m_Curvetype == CURVETYPE_BEZIER)
					{
						float TangentX = EnvelopeToScreenX(View, fxt2f(pEnvelope->m_vPoints[i].m_Time + pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c]));
						float TangentY = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c] + pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaY[c]));

						if(IsTangentOutPointSelected(i, c))
							Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.4f);
						else
							Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 0.4f);

						IGraphics::CLineItem LineItem(TangentX, TangentY, PosX, PosY);
						Graphics()->LinesDraw(&LineItem, 1);
					}

					// In-Tangent
					if(i > 0 && pEnvelope->m_vPoints[i - 1].m_Curvetype == CURVETYPE_BEZIER)
					{
						float TangentX = EnvelopeToScreenX(View, fxt2f(pEnvelope->m_vPoints[i].m_Time + pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c]));
						float TangentY = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c] + pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaY[c]));

						if(IsTangentInPointSelected(i, c))
							Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.4f);
						else
							Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 0.4f);

						IGraphics::CLineItem LineItem(TangentX, TangentY, PosX, PosY);
						Graphics()->LinesDraw(&LineItem, 1);
					}
				}
			}
			Graphics()->LinesEnd();
			UI()->ClipDisable();
		}

		// render lines
		{
			float EndTimeTotal = maximum(0.000001f, pEnvelope->EndTime());
			float EndX = clamp(EnvelopeToScreenX(View, EndTimeTotal), View.x, View.x + View.w);
			float StartX = clamp(View.x + View.w * m_OffsetEnvelopeX, View.x, View.x + View.w);

			float EndTime = ScreenToEnvelopeX(View, EndX);
			float StartTime = ScreenToEnvelopeX(View, StartX);

			UI()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			for(int c = 0; c < pEnvelope->GetChannels(); c++)
			{
				if(s_ActiveChannels & (1 << c))
					Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 1);
				else
					Graphics()->SetColor(aColors[c].r * 0.5f, aColors[c].g * 0.5f, aColors[c].b * 0.5f, 1);

				int Steps = static_cast<int>(((EndX - StartX) / UI()->Screen()->w) * Graphics()->ScreenWidth());
				float StepTime = (EndTime - StartTime) / static_cast<float>(Steps);
				float StepSize = (EndX - StartX) / static_cast<float>(Steps);

				ColorRGBA Channels;
				pEnvelope->Eval(StartTime + StepTime, Channels);
				float PrevY = EnvelopeToScreenY(View, Channels[c]);
				for(int i = 2; i < Steps; i++)
				{
					pEnvelope->Eval(StartTime + i * StepTime, Channels);
					float CurrentY = EnvelopeToScreenY(View, Channels[c]);

					IGraphics::CLineItem LineItem(
						StartX + (i - 1) * StepSize,
						PrevY,
						StartX + i * StepSize,
						CurrentY);
					Graphics()->LinesDraw(&LineItem, 1);

					PrevY = CurrentY;
				}
			}
			Graphics()->LinesEnd();
			UI()->ClipDisable();
		}

		// render curve options
		{
			for(int i = 0; i < (int)pEnvelope->m_vPoints.size() - 1; i++)
			{
				float t0 = fxt2f(pEnvelope->m_vPoints[i].m_Time);
				float t1 = fxt2f(pEnvelope->m_vPoints[i + 1].m_Time);

				CUIRect CurveButton;
				CurveButton.x = EnvelopeToScreenX(View, t0 + (t1 - t0) * 0.5f);
				CurveButton.y = CurveBar.y;
				CurveButton.h = CurveBar.h;
				CurveButton.w = CurveBar.h;
				CurveButton.x -= CurveButton.w / 2.0f;
				const void *pID = &pEnvelope->m_vPoints[i].m_Curvetype;
				const char *apTypeName[] = {"N", "L", "S", "F", "M", "B"};
				const char *pTypeName = "!?";
				if(0 <= pEnvelope->m_vPoints[i].m_Curvetype && pEnvelope->m_vPoints[i].m_Curvetype < (int)std::size(apTypeName))
					pTypeName = apTypeName[pEnvelope->m_vPoints[i].m_Curvetype];

				if(CurveButton.x >= View.x)
				{
					if(DoButton_Editor(pID, pTypeName, 0, &CurveButton, 0, "Switch curve type (N = step, L = linear, S = slow, F = fast, M = smooth, B = bezier)"))
						pEnvelope->m_vPoints[i].m_Curvetype = (pEnvelope->m_vPoints[i].m_Curvetype + 1) % NUM_CURVETYPES;
				}
			}
		}

		// render colorbar
		if(ShowColorBar)
		{
			UI()->ClipEnable(&ColorBar);

			float StartX = maximum(EnvelopeToScreenX(View, 0), ColorBar.x);
			float EndX = EnvelopeToScreenX(View, pEnvelope->EndTime());
			CUIRect BackgroundView{
				StartX,
				ColorBar.y,
				minimum(EndX - StartX, ColorBar.x + ColorBar.w - StartX),
				ColorBar.h};
			RenderBackground(BackgroundView, m_CheckerTexture, 16.0f, 1.0f);

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

				float x0 = EnvelopeToScreenX(View, fxt2f(pEnvelope->m_vPoints[i].m_Time));
				float x1 = EnvelopeToScreenX(View, fxt2f(pEnvelope->m_vPoints[i + 1].m_Time));

				IGraphics::CQuadItem QuadItem(x0, ColorBar.y, x1 - x0, ColorBar.h);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
			}
			Graphics()->QuadsEnd();
			UI()->ClipDisable();
		}

		// render handles
		if(CurrentEnvelopeSwitched)
		{
			DeselectEnvPoints();
			m_ResetZoomEnvelope = true;
		}

		{
			if(s_Operation == OP_NONE)
				SetHotEnvelopePoint(View, pEnvelope, s_ActiveChannels);

			UI()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			for(int c = 0; c < pEnvelope->GetChannels(); c++)
			{
				if(!(s_ActiveChannels & (1 << c)))
					continue;

				for(size_t i = 0; i < pEnvelope->m_vPoints.size(); i++)
				{
					// point handle
					{
						CUIRect Final;
						Final.x = EnvelopeToScreenX(View, fxt2f(pEnvelope->m_vPoints[i].m_Time));
						Final.y = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c]));
						Final.x -= 2.0f;
						Final.y -= 2.0f;
						Final.w = 4.0f;
						Final.h = 4.0f;

						const void *pID = &pEnvelope->m_vPoints[i].m_aValues[c];

						if(IsEnvPointSelected(i, c))
						{
							Graphics()->SetColor(1, 1, 1, 1);
							CUIRect Background = {
								Final.x - 0.2f * Final.w,
								Final.y - 0.2f * Final.h,
								Final.w * 1.4f,
								Final.h * 1.4f};
							IGraphics::CQuadItem QuadItem(Background.x, Background.y, Background.w, Background.h);
							Graphics()->QuadsDrawTL(&QuadItem, 1);
						}

						if(UI()->CheckActiveItem(pID))
						{
							m_ShowEnvelopePreview = SHOWENV_SELECTED;

							if(s_Operation == OP_SELECT)
							{
								float dx = s_MouseXStart - UI()->MouseX();
								float dy = s_MouseYStart - UI()->MouseY();

								if(dx * dx + dy * dy > 20.0f)
								{
									s_Operation = OP_DRAG_POINT;

									if(!IsEnvPointSelected(i, c))
										SelectEnvPoint(i, c);
								}
							}

							if(s_Operation == OP_DRAG_POINT || s_Operation == OP_DRAG_POINT_X || s_Operation == OP_DRAG_POINT_Y)
							{
								if(Input()->ShiftIsPressed())
								{
									if(s_Operation == OP_DRAG_POINT || s_Operation == OP_DRAG_POINT_Y)
									{
										s_Operation = OP_DRAG_POINT_X;
										s_vAccurateDragValuesX.clear();
										for(auto [SelectedIndex, _] : m_vSelectedEnvelopePoints)
											s_vAccurateDragValuesX.push_back(pEnvelope->m_vPoints[SelectedIndex].m_Time);
									}
									else
									{
										float DeltaX = ScreenToEnvelopeDX(View, UI()->MouseDeltaX()) * (Input()->ModifierIsPressed() ? 50.0f : 1000.0f);

										for(size_t k = 0; k < m_vSelectedEnvelopePoints.size(); k++)
										{
											int SelectedIndex = m_vSelectedEnvelopePoints[k].first;
											int BoundLow = f2fxt(ScreenToEnvelopeX(View, View.x));
											int BoundHigh = f2fxt(ScreenToEnvelopeX(View, View.x + View.w));
											for(int j = 0; j < SelectedIndex; j++)
											{
												if(!IsEnvPointSelected(j))
													BoundLow = maximum(pEnvelope->m_vPoints[j].m_Time + 1, BoundLow);
											}
											for(int j = SelectedIndex + 1; j < (int)pEnvelope->m_vPoints.size(); j++)
											{
												if(!IsEnvPointSelected(j))
													BoundHigh = minimum(pEnvelope->m_vPoints[j].m_Time - 1, BoundHigh);
											}

											DeltaX = ClampDelta(s_vAccurateDragValuesX[k], DeltaX, BoundLow, BoundHigh);
										}
										for(size_t k = 0; k < m_vSelectedEnvelopePoints.size(); k++)
										{
											int SelectedIndex = m_vSelectedEnvelopePoints[k].first;
											s_vAccurateDragValuesX[k] += DeltaX;
											pEnvelope->m_vPoints[SelectedIndex].m_Time = std::round(s_vAccurateDragValuesX[k]);
										}
										for(size_t k = 0; k < m_vSelectedEnvelopePoints.size(); k++)
										{
											int SelectedIndex = m_vSelectedEnvelopePoints[k].first;
											if(SelectedIndex == 0 && pEnvelope->m_vPoints[SelectedIndex].m_Time != 0)
											{
												RemoveTimeOffsetEnvelope(pEnvelope);
												float Offset = s_vAccurateDragValuesX[k];
												for(auto &Value : s_vAccurateDragValuesX)
													Value -= Offset;
												break;
											}
										}
									}
								}
								else
								{
									if(s_Operation == OP_DRAG_POINT || s_Operation == OP_DRAG_POINT_X)
									{
										s_Operation = OP_DRAG_POINT_Y;
										s_vAccurateDragValuesY.clear();
										for(auto [SelectedIndex, SelectedChannel] : m_vSelectedEnvelopePoints)
											s_vAccurateDragValuesY.push_back(pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel]);
									}
									else
									{
										float DeltaY = ScreenToEnvelopeDY(View, UI()->MouseDeltaY()) * (Input()->ModifierIsPressed() ? 51.2f : 1024.0f);
										for(size_t k = 0; k < m_vSelectedEnvelopePoints.size(); k++)
										{
											auto [SelectedIndex, SelectedChannel] = m_vSelectedEnvelopePoints[k];
											s_vAccurateDragValuesY[k] -= DeltaY;
											pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::round(s_vAccurateDragValuesY[k]);

											if(pEnvelope->GetChannels() == 4)
											{
												pEnvelope->m_vPoints[i].m_aValues[c] = clamp(pEnvelope->m_vPoints[i].m_aValues[c], 0, 1024);
												s_vAccurateDragValuesY[k] = clamp<float>(s_vAccurateDragValuesY[k], 0, 1024);
											}
										}
									}
								}
							}

							if(s_Operation == OP_CONTEXT_MENU)
							{
								if(!UI()->MouseButton(1))
								{
									if(m_vSelectedEnvelopePoints.size() == 1)
									{
										m_UpdateEnvPointInfo = true;
										static SPopupMenuId s_PopupEnvPointId;
										UI()->DoPopupMenu(&s_PopupEnvPointId, UI()->MouseX(), UI()->MouseY(), 150, 56, this, PopupEnvPoint);
									}
									UI()->SetActiveItem(nullptr);
								}
							}
							else if(!UI()->MouseButton(0))
							{
								UI()->SetActiveItem(nullptr);
								m_SelectedQuadEnvelope = -1;

								if(s_Operation == OP_SELECT)
								{
									if(Input()->ShiftIsPressed())
										ToggleEnvPoint(i, c);
									else
										SelectEnvPoint(i, c);
								}

								s_Operation = OP_NONE;
								m_Map.OnModify();
							}

							Graphics()->SetColor(1, 1, 1, 1);
						}
						else if(UI()->HotItem() == pID)
						{
							if(UI()->MouseButton(0))
							{
								UI()->SetActiveItem(pID);
								s_Operation = OP_SELECT;
								m_SelectedQuadEnvelope = m_SelectedEnvelope;

								s_MouseXStart = UI()->MouseX();
								s_MouseYStart = UI()->MouseY();
							}
							else if(UI()->MouseButtonClicked(1))
							{
								if(Input()->ShiftIsPressed())
								{
									pEnvelope->m_vPoints.erase(pEnvelope->m_vPoints.begin() + i);
									m_Map.OnModify();
								}
								else
								{
									s_Operation = OP_CONTEXT_MENU;
									SelectEnvPoint(i, c);
									UI()->SetActiveItem(pID);
								}
							}

							m_ShowEnvelopePreview = SHOWENV_SELECTED;
							Graphics()->SetColor(1, 1, 1, 1);
							m_pTooltip = "Envelope point. Left mouse to drag. Hold ctrl to be more precise. Hold shift to alter time. Shift + right-click to delete.";
							ms_pUiGotContext = pID;
						}
						else
							Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 1.0f);

						IGraphics::CQuadItem QuadItem(Final.x, Final.y, Final.w, Final.h);
						Graphics()->QuadsDrawTL(&QuadItem, 1);
					}

					// tangent handles for bezier curves
					{
						// Out-Tangent handle
						if(pEnvelope->m_vPoints[i].m_Curvetype == CURVETYPE_BEZIER)
						{
							CUIRect Final;
							Final.x = EnvelopeToScreenX(View, fxt2f(pEnvelope->m_vPoints[i].m_Time + pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c]));
							Final.y = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c] + pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaY[c]));
							Final.x -= 2.0f;
							Final.y -= 2.0f;
							Final.w = 4.0f;
							Final.h = 4.0f;

							// handle logic
							const void *pID = &pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c];

							if(IsTangentOutPointSelected(i, c))
							{
								Graphics()->SetColor(1, 1, 1, 1);
								IGraphics::CFreeformItem FreeformItem(
									Final.x + Final.w / 2.0f,
									Final.y - 1,
									Final.x + Final.w / 2.0f,
									Final.y - 1,
									Final.x + Final.w + 1,
									Final.y + Final.h + 1,
									Final.x - 1,
									Final.y + Final.h + 1);
								Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
							}

							if(UI()->CheckActiveItem(pID))
							{
								m_ShowEnvelopePreview = SHOWENV_SELECTED;

								if(s_Operation == OP_SELECT)
								{
									float dx = s_MouseXStart - UI()->MouseX();
									float dy = s_MouseYStart - UI()->MouseY();

									if(dx * dx + dy * dy > 20.0f)
									{
										s_Operation = OP_DRAG_POINT;

										s_vAccurateDragValuesX = {static_cast<float>(pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c])};
										s_vAccurateDragValuesY = {static_cast<float>(pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaY[c])};

										if(!IsTangentOutPointSelected(i, c))
											SelectTangentOutPoint(i, c);
									}
								}

								if(s_Operation == OP_DRAG_POINT)
								{
									float DeltaX = ScreenToEnvelopeDX(View, UI()->MouseDeltaX()) * (Input()->ModifierIsPressed() ? 50.0f : 1000.0f);
									float DeltaY = ScreenToEnvelopeDY(View, UI()->MouseDeltaY()) * (Input()->ModifierIsPressed() ? 51.2f : 1024.0f);
									s_vAccurateDragValuesX[0] += DeltaX;
									s_vAccurateDragValuesY[0] -= DeltaY;

									pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c] = std::round(s_vAccurateDragValuesX[0]);
									pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaY[c] = std::round(s_vAccurateDragValuesY[0]);

									// clamp time value
									pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c] = clamp<int>(pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c], 0, f2fxt(ScreenToEnvelopeX(View, View.x + View.w)) - pEnvelope->m_vPoints[i].m_Time);
									s_vAccurateDragValuesX[0] = clamp<float>(s_vAccurateDragValuesX[0], 0, f2fxt(ScreenToEnvelopeX(View, View.x + View.w)) - pEnvelope->m_vPoints[i].m_Time);
								}

								if(s_Operation == OP_CONTEXT_MENU)
								{
									if(!UI()->MouseButton(1))
									{
										if(IsTangentOutPointSelected(i, c))
										{
											m_UpdateEnvPointInfo = true;
											static SPopupMenuId s_PopupEnvPointId;
											UI()->DoPopupMenu(&s_PopupEnvPointId, UI()->MouseX(), UI()->MouseY(), 150, 56, this, PopupEnvPoint);
										}
										UI()->SetActiveItem(nullptr);
									}
								}
								else if(!UI()->MouseButton(0))
								{
									UI()->SetActiveItem(nullptr);
									m_SelectedQuadEnvelope = -1;

									if(s_Operation == OP_SELECT)
										SelectTangentOutPoint(i, c);

									s_Operation = OP_NONE;
									m_Map.OnModify();
								}

								Graphics()->SetColor(1, 1, 1, 1);
							}
							else if(UI()->HotItem() == pID)
							{
								if(UI()->MouseButton(0))
								{
									UI()->SetActiveItem(pID);
									s_Operation = OP_SELECT;
									m_SelectedQuadEnvelope = m_SelectedEnvelope;

									s_MouseXStart = UI()->MouseX();
									s_MouseYStart = UI()->MouseY();
								}
								else if(UI()->MouseButtonClicked(1))
								{
									if(Input()->ShiftIsPressed())
									{
										SelectTangentOutPoint(i, c);
										pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c] = 0.0f;
										pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaY[c] = 0.0f;
										m_Map.OnModify();
									}
									else
									{
										s_Operation = OP_CONTEXT_MENU;
										SelectTangentOutPoint(i, c);
										UI()->SetActiveItem(pID);
									}
								}

								m_ShowEnvelopePreview = SHOWENV_SELECTED;
								Graphics()->SetColor(1, 1, 1, 1);
								m_pTooltip = "Bezier out-tangent. Left mouse to drag. Hold ctrl to be more precise. Shift + right-click to reset.";
								ms_pUiGotContext = pID;
							}
							else
								Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 1.0f);

							// draw triangle
							IGraphics::CFreeformItem FreeformItem(Final.x + Final.w / 2.0f, Final.y, Final.x + Final.w / 2.0f, Final.y, Final.x + Final.w, Final.y + Final.h, Final.x, Final.y + Final.h);
							Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
						}

						// In-Tangent handle
						if(i > 0 && pEnvelope->m_vPoints[i - 1].m_Curvetype == CURVETYPE_BEZIER)
						{
							CUIRect Final;
							Final.x = EnvelopeToScreenX(View, fxt2f(pEnvelope->m_vPoints[i].m_Time + pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c]));
							Final.y = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c] + pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaY[c]));
							Final.x -= 2.0f;
							Final.y -= 2.0f;
							Final.w = 4.0f;
							Final.h = 4.0f;

							// handle logic
							const void *pID = &pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c];

							if(IsTangentInPointSelected(i, c))
							{
								Graphics()->SetColor(1, 1, 1, 1);
								IGraphics::CFreeformItem FreeformItem(
									Final.x + Final.w / 2.0f,
									Final.y - 1,
									Final.x + Final.w / 2.0f,
									Final.y - 1,
									Final.x + Final.w + 1,
									Final.y + Final.h + 1,
									Final.x - 1,
									Final.y + Final.h + 1);
								Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
							}

							if(UI()->CheckActiveItem(pID))
							{
								m_ShowEnvelopePreview = SHOWENV_SELECTED;

								if(s_Operation == OP_SELECT)
								{
									float dx = s_MouseXStart - UI()->MouseX();
									float dy = s_MouseYStart - UI()->MouseY();

									if(dx * dx + dy * dy > 20.0f)
									{
										s_Operation = OP_DRAG_POINT;

										s_vAccurateDragValuesX = {static_cast<float>(pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c])};
										s_vAccurateDragValuesY = {static_cast<float>(pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaY[c])};

										if(!IsTangentInPointSelected(i, c))
											SelectTangentInPoint(i, c);
									}
								}

								if(s_Operation == OP_DRAG_POINT)
								{
									float DeltaX = ScreenToEnvelopeDX(View, UI()->MouseDeltaX()) * (Input()->ModifierIsPressed() ? 50.0f : 1000.0f);
									float DeltaY = ScreenToEnvelopeDY(View, UI()->MouseDeltaY()) * (Input()->ModifierIsPressed() ? 51.2f : 1024.0f);
									s_vAccurateDragValuesX[0] += DeltaX;
									s_vAccurateDragValuesY[0] -= DeltaY;

									pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c] = std::round(s_vAccurateDragValuesX[0]);
									pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaY[c] = std::round(s_vAccurateDragValuesY[0]);

									// clamp time value
									pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c] = clamp(pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c], f2fxt(ScreenToEnvelopeX(View, View.x)) - pEnvelope->m_vPoints[i].m_Time, 0);
									s_vAccurateDragValuesX[0] = clamp<float>(s_vAccurateDragValuesX[0], f2fxt(ScreenToEnvelopeX(View, View.x)) - pEnvelope->m_vPoints[i].m_Time, 0);
								}

								if(s_Operation == OP_CONTEXT_MENU)
								{
									if(!UI()->MouseButton(1))
									{
										if(IsTangentInPointSelected(i, c))
										{
											m_UpdateEnvPointInfo = true;
											static SPopupMenuId s_PopupEnvPointId;
											UI()->DoPopupMenu(&s_PopupEnvPointId, UI()->MouseX(), UI()->MouseY(), 150, 56, this, PopupEnvPoint);
										}
										UI()->SetActiveItem(nullptr);
									}
								}
								else if(!UI()->MouseButton(0))
								{
									UI()->SetActiveItem(nullptr);
									m_SelectedQuadEnvelope = -1;

									if(s_Operation == OP_SELECT)
										SelectTangentInPoint(i, c);

									s_Operation = OP_NONE;
									m_Map.OnModify();
								}

								Graphics()->SetColor(1, 1, 1, 1);
							}
							else if(UI()->HotItem() == pID)
							{
								if(UI()->MouseButton(0))
								{
									UI()->SetActiveItem(pID);
									s_Operation = OP_SELECT;
									m_SelectedQuadEnvelope = m_SelectedEnvelope;

									s_MouseXStart = UI()->MouseX();
									s_MouseYStart = UI()->MouseY();
								}
								else if(UI()->MouseButtonClicked(1))
								{
									if(Input()->ShiftIsPressed())
									{
										SelectTangentInPoint(i, c);
										pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c] = 0.0f;
										pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaY[c] = 0.0f;
										m_Map.OnModify();
									}
									else
									{
										s_Operation = OP_CONTEXT_MENU;
										SelectTangentInPoint(i, c);
										UI()->SetActiveItem(pID);
									}
								}

								m_ShowEnvelopePreview = SHOWENV_SELECTED;
								Graphics()->SetColor(1, 1, 1, 1);
								m_pTooltip = "Bezier in-tangent. Left mouse to drag. Hold ctrl to be more precise. Shift + right-click to reset.";
								ms_pUiGotContext = pID;
							}
							else
								Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 1.0f);

							// draw triangle
							IGraphics::CFreeformItem FreeformItem(Final.x + Final.w / 2.0f, Final.y, Final.x + Final.w / 2.0f, Final.y, Final.x + Final.w, Final.y + Final.h, Final.x, Final.y + Final.h);
							Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
						}
					}
				}
			}
			Graphics()->QuadsEnd();
			UI()->ClipDisable();
		}

		// handle scaling
		static float s_ScaleFactorX = 1.0f;
		static float s_ScaleFactorY = 1.0f;
		static float s_MidpointX = 0.0f;
		static float s_MidpointY = 0.0f;
		static std::vector<float> s_vInitialPositionsX;
		static std::vector<float> s_vInitialPositionsY;
		if(s_Operation == OP_NONE && Input()->KeyIsPressed(KEY_S) && !m_vSelectedEnvelopePoints.empty())
		{
			s_Operation = OP_SCALE;
			s_ScaleFactorX = 1.0f;
			s_ScaleFactorY = 1.0f;
			auto [FirstPointIndex, FirstPointChannel] = m_vSelectedEnvelopePoints.front();

			float MaximumX = pEnvelope->m_vPoints[FirstPointIndex].m_Time;
			float MinimumX = MaximumX;
			s_vInitialPositionsX.clear();
			for(auto [SelectedIndex, _] : m_vSelectedEnvelopePoints)
			{
				float Value = pEnvelope->m_vPoints[SelectedIndex].m_Time;
				s_vInitialPositionsX.push_back(Value);
				MaximumX = maximum(MaximumX, Value);
				MinimumX = minimum(MinimumX, Value);
			}
			s_MidpointX = (MaximumX - MinimumX) / 2.0f + MinimumX;

			float MaximumY = pEnvelope->m_vPoints[FirstPointIndex].m_aValues[FirstPointChannel];
			float MinimumY = MaximumY;
			s_vInitialPositionsY.clear();
			for(auto [SelectedIndex, SelectedChannel] : m_vSelectedEnvelopePoints)
			{
				float Value = pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel];
				s_vInitialPositionsY.push_back(Value);
				MaximumY = maximum(MaximumY, Value);
				MinimumY = minimum(MinimumY, Value);
			}
			s_MidpointY = (MaximumY - MinimumY) / 2.0f + MinimumY;
		}

		if(s_Operation == OP_SCALE)
		{
			m_pTooltip = "Press shift to scale the time. Press alt to scale along midpoint. Press ctrl to be more precise.";

			if(Input()->ShiftIsPressed())
			{
				s_ScaleFactorX += UI()->MouseDeltaX() / Graphics()->ScreenWidth() * (Input()->ModifierIsPressed() ? 0.5f : 10.0f);
				float Midpoint = Input()->AltIsPressed() ? s_MidpointX : 0.0f;
				for(size_t k = 0; k < m_vSelectedEnvelopePoints.size(); k++)
				{
					int SelectedIndex = m_vSelectedEnvelopePoints[k].first;
					int BoundLow = f2fxt(ScreenToEnvelopeX(View, View.x));
					int BoundHigh = f2fxt(ScreenToEnvelopeX(View, View.x + View.w));
					for(int j = 0; j < SelectedIndex; j++)
					{
						if(!IsEnvPointSelected(j))
							BoundLow = maximum(pEnvelope->m_vPoints[j].m_Time + 1, BoundLow);
					}
					for(int j = SelectedIndex + 1; j < (int)pEnvelope->m_vPoints.size(); j++)
					{
						if(!IsEnvPointSelected(j))
							BoundHigh = minimum(pEnvelope->m_vPoints[j].m_Time - 1, BoundHigh);
					}

					float Value = s_vInitialPositionsX[k];
					float ScaleBoundLow = (BoundLow - Midpoint) / (Value - Midpoint);
					float ScaleBoundHigh = (BoundHigh - Midpoint) / (Value - Midpoint);
					float ScaleBoundMin = minimum(ScaleBoundLow, ScaleBoundHigh);
					float ScaleBoundMax = maximum(ScaleBoundLow, ScaleBoundHigh);
					s_ScaleFactorX = clamp(s_ScaleFactorX, ScaleBoundMin, ScaleBoundMax);
				}

				for(size_t k = 0; k < m_vSelectedEnvelopePoints.size(); k++)
				{
					int SelectedIndex = m_vSelectedEnvelopePoints[k].first;
					float ScaleMinimum = s_vInitialPositionsX[k] - Midpoint > fxt2f(1) ? fxt2f(1) / (s_vInitialPositionsX[k] - Midpoint) : 0.0f;
					float ScaleFactor = maximum(ScaleMinimum, s_ScaleFactorX);
					pEnvelope->m_vPoints[SelectedIndex].m_Time = std::round((s_vInitialPositionsX[k] - Midpoint) * ScaleFactor + Midpoint);
				}
				for(size_t k = 1; k < pEnvelope->m_vPoints.size(); k++)
				{
					if(pEnvelope->m_vPoints[k].m_Time <= pEnvelope->m_vPoints[k - 1].m_Time)
						pEnvelope->m_vPoints[k].m_Time = pEnvelope->m_vPoints[k - 1].m_Time + 1;
				}
				for(auto [SelectedIndex, _] : m_vSelectedEnvelopePoints)
				{
					if(SelectedIndex == 0 && pEnvelope->m_vPoints[SelectedIndex].m_Time != 0)
					{
						float Offset = pEnvelope->m_vPoints[0].m_Time;
						RemoveTimeOffsetEnvelope(pEnvelope);
						s_MidpointX -= Offset;
						for(auto &Value : s_vInitialPositionsX)
							Value -= Offset;
						break;
					}
				}
			}
			else
			{
				s_ScaleFactorY -= UI()->MouseDeltaY() / Graphics()->ScreenHeight() * (Input()->ModifierIsPressed() ? 0.5f : 10.0f);
				for(size_t k = 0; k < m_vSelectedEnvelopePoints.size(); k++)
				{
					auto [SelectedIndex, SelectedChannel] = m_vSelectedEnvelopePoints[k];
					if(Input()->AltIsPressed())
						pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::round((s_vInitialPositionsY[k] - s_MidpointY) * s_ScaleFactorY + s_MidpointY);
					else
						pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::round(s_vInitialPositionsY[k] * s_ScaleFactorY);

					if(pEnvelope->GetChannels() == 4)
						pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = clamp(pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel], 0, 1024);
				}
			}

			if(UI()->MouseButton(0))
			{
				s_Operation = OP_NONE;
			}
			else if(UI()->MouseButton(1))
			{
				for(size_t k = 0; k < m_vSelectedEnvelopePoints.size(); k++)
				{
					int SelectedIndex = m_vSelectedEnvelopePoints[k].first;
					pEnvelope->m_vPoints[SelectedIndex].m_Time = std::round(s_vInitialPositionsX[k]);
				}
				for(size_t k = 0; k < m_vSelectedEnvelopePoints.size(); k++)
				{
					auto [SelectedIndex, SelectedChannel] = m_vSelectedEnvelopePoints[k];
					pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::round(s_vInitialPositionsY[k]);
				}
				RemoveTimeOffsetEnvelope(pEnvelope);
				s_Operation = OP_NONE;
			}
		}

		// handle box selection
		if(s_Operation == OP_BOX_SELECT)
		{
			IGraphics::CLineItem aLines[4] = {
				{s_MouseXStart, s_MouseYStart, UI()->MouseX(), s_MouseYStart},
				{s_MouseXStart, s_MouseYStart, s_MouseXStart, UI()->MouseY()},
				{s_MouseXStart, UI()->MouseY(), UI()->MouseX(), UI()->MouseY()},
				{UI()->MouseX(), s_MouseYStart, UI()->MouseX(), UI()->MouseY()}};
			UI()->ClipEnable(&View);
			Graphics()->LinesBegin();
			Graphics()->LinesDraw(aLines, std::size(aLines));
			Graphics()->LinesEnd();
			UI()->ClipDisable();

			if(!UI()->MouseButton(0))
			{
				s_Operation = OP_NONE;
				UI()->SetActiveItem(nullptr);

				float TimeStart = ScreenToEnvelopeX(View, s_MouseXStart);
				float TimeEnd = ScreenToEnvelopeX(View, UI()->MouseX());
				float ValueStart = ScreenToEnvelopeY(View, s_MouseYStart);
				float ValueEnd = ScreenToEnvelopeY(View, UI()->MouseY());

				float TimeMin = minimum(TimeStart, TimeEnd);
				float TimeMax = maximum(TimeStart, TimeEnd);
				float ValueMin = minimum(ValueStart, ValueEnd);
				float ValueMax = maximum(ValueStart, ValueEnd);

				if(!Input()->ShiftIsPressed())
					DeselectEnvPoints();

				for(int i = 0; i < (int)pEnvelope->m_vPoints.size(); i++)
				{
					for(int c = 0; c < CEnvPoint::MAX_CHANNELS; c++)
					{
						if(!(s_ActiveChannels & (1 << c)))
							continue;

						float Time = fxt2f(pEnvelope->m_vPoints[i].m_Time);
						float Value = fx2f(pEnvelope->m_vPoints[i].m_aValues[c]);

						if(in_range(Time, TimeMin, TimeMax) && in_range(Value, ValueMin, ValueMax))
							ToggleEnvPoint(i, c);
					}
				}
			}
		}
	}
}

void CEditor::RenderServerSettingsEditor(CUIRect View, bool ShowServerSettingsEditorLast)
{
	// TODO: improve validation (https://github.com/ddnet/ddnet/issues/1406)
	// Returns true if the argument is a valid server setting
	const auto &&ValidateServerSetting = [](const char *pStr) {
		return str_find(pStr, " ") != nullptr;
	};

	static int s_CommandSelectedIndex = -1;
	static CListBox s_ListBox;
	s_ListBox.SetActive(m_Dialog == DIALOG_NONE && !UI()->IsPopupOpen());

	const bool GotSelection = s_ListBox.Active() && s_CommandSelectedIndex >= 0 && (size_t)s_CommandSelectedIndex < m_Map.m_vSettings.size();
	const bool CurrentInputValid = ValidateServerSetting(m_SettingsCommandInput.GetString());

	CUIRect ToolBar, Button, Label, List, DragBar;
	View.HSplitTop(22.0f, &DragBar, nullptr);
	DragBar.y -= 2.0f;
	DragBar.w += 2.0f;
	DragBar.h += 4.0f;
	RenderExtraEditorDragBar(View, DragBar);
	View.HSplitTop(20.0f, &ToolBar, &View);
	View.HSplitTop(2.0f, nullptr, &List);
	ToolBar.HMargin(2.0f, &ToolBar);

	// delete button
	ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
	ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
	static int s_DeleteButton = 0;
	if(DoButton_FontIcon(&s_DeleteButton, FONT_ICON_TRASH, GotSelection ? 0 : -1, &Button, 0, "[Delete] Delete the selected command from the command list.", IGraphics::CORNER_ALL, 9.0f) == 1 || (GotSelection && CLineInput::GetActiveInput() == nullptr && m_Dialog == DIALOG_NONE && UI()->ConsumeHotkey(CUI::HOTKEY_DELETE)))
	{
		m_Map.m_vSettings.erase(m_Map.m_vSettings.begin() + s_CommandSelectedIndex);
		if(s_CommandSelectedIndex >= (int)m_Map.m_vSettings.size())
			s_CommandSelectedIndex = m_Map.m_vSettings.size() - 1;
		if(s_CommandSelectedIndex >= 0)
			m_SettingsCommandInput.Set(m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand);
		m_Map.OnModify();
		s_ListBox.ScrollToSelected();
	}

	// move down button
	ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
	const bool CanMoveDown = GotSelection && s_CommandSelectedIndex < (int)m_Map.m_vSettings.size() - 1;
	static int s_DownButton = 0;
	if(DoButton_FontIcon(&s_DownButton, FONT_ICON_SORT_DOWN, CanMoveDown ? 0 : -1, &Button, 0, "[Alt+Down] Move the selected command down.", IGraphics::CORNER_R, 11.0f) == 1 || (CanMoveDown && Input()->AltIsPressed() && UI()->ConsumeHotkey(CUI::HOTKEY_DOWN)))
	{
		std::swap(m_Map.m_vSettings[s_CommandSelectedIndex], m_Map.m_vSettings[s_CommandSelectedIndex + 1]);
		s_CommandSelectedIndex++;
		m_Map.OnModify();
		s_ListBox.ScrollToSelected();
	}

	// move up button
	ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
	ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
	const bool CanMoveUp = GotSelection && s_CommandSelectedIndex > 0;
	static int s_UpButton = 0;
	if(DoButton_FontIcon(&s_UpButton, FONT_ICON_SORT_UP, CanMoveUp ? 0 : -1, &Button, 0, "[Alt+Up] Move the selected command up.", IGraphics::CORNER_L, 11.0f) == 1 || (CanMoveUp && Input()->AltIsPressed() && UI()->ConsumeHotkey(CUI::HOTKEY_UP)))
	{
		std::swap(m_Map.m_vSettings[s_CommandSelectedIndex], m_Map.m_vSettings[s_CommandSelectedIndex - 1]);
		s_CommandSelectedIndex--;
		m_Map.OnModify();
		s_ListBox.ScrollToSelected();
	}

	// update button
	ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
	const bool CanUpdate = GotSelection && CurrentInputValid && str_comp(m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand, m_SettingsCommandInput.GetString()) != 0;
	static int s_UpdateButton = 0;
	if(DoButton_FontIcon(&s_UpdateButton, FONT_ICON_PENCIL, CanUpdate ? 0 : -1, &Button, 0, "[Alt+Enter] Update the selected command based on the entered value.", IGraphics::CORNER_R, 9.0f) == 1 || (CanUpdate && Input()->AltIsPressed() && m_Dialog == DIALOG_NONE && UI()->ConsumeHotkey(CUI::HOTKEY_ENTER)))
	{
		bool Found = false;
		int i;
		for(i = 0; i < (int)m_Map.m_vSettings.size(); ++i)
		{
			if(i != s_CommandSelectedIndex && !str_comp(m_Map.m_vSettings[i].m_aCommand, m_SettingsCommandInput.GetString()))
			{
				Found = true;
				break;
			}
		}
		if(Found)
		{
			m_Map.m_vSettings.erase(m_Map.m_vSettings.begin() + s_CommandSelectedIndex);
			s_CommandSelectedIndex = i > s_CommandSelectedIndex ? i - 1 : i;
		}
		else
		{
			str_copy(m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand, m_SettingsCommandInput.GetString());
		}
		m_Map.OnModify();
		s_ListBox.ScrollToSelected();
		UI()->SetActiveItem(&m_SettingsCommandInput);
	}

	// add button
	ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
	ToolBar.VSplitRight(100.0f, &ToolBar, nullptr);
	const bool CanAdd = s_ListBox.Active() && CurrentInputValid;
	static int s_AddButton = 0;
	if(DoButton_FontIcon(&s_AddButton, FONT_ICON_PLUS, CanAdd ? 0 : -1, &Button, 0, "[Enter] Add a command to the command list.", IGraphics::CORNER_L) == 1 || (CanAdd && !Input()->AltIsPressed() && m_Dialog == DIALOG_NONE && UI()->ConsumeHotkey(CUI::HOTKEY_ENTER)))
	{
		bool Found = false;
		for(size_t i = 0; i < m_Map.m_vSettings.size(); ++i)
		{
			if(!str_comp(m_Map.m_vSettings[i].m_aCommand, m_SettingsCommandInput.GetString()))
			{
				s_CommandSelectedIndex = i;
				Found = true;
				break;
			}
		}

		if(!Found)
		{
			m_Map.m_vSettings.emplace_back(m_SettingsCommandInput.GetString());
			s_CommandSelectedIndex = m_Map.m_vSettings.size() - 1;
			m_Map.OnModify();
		}
		s_ListBox.ScrollToSelected();
		UI()->SetActiveItem(&m_SettingsCommandInput);
	}

	// command input (use remaining toolbar width)
	if(!ShowServerSettingsEditorLast) // Just activated
		UI()->SetActiveItem(&m_SettingsCommandInput);
	m_SettingsCommandInput.SetEmptyText("Command");
	DoClearableEditBox(&m_SettingsCommandInput, &ToolBar, 12.0f, IGraphics::CORNER_ALL, "Enter a server setting.");

	// command list
	s_ListBox.DoStart(15.0f, m_Map.m_vSettings.size(), 1, 3, s_CommandSelectedIndex, &List);

	for(size_t i = 0; i < m_Map.m_vSettings.size(); i++)
	{
		const CListboxItem Item = s_ListBox.DoNextItem(&m_Map.m_vSettings[i], s_CommandSelectedIndex >= 0 && (size_t)s_CommandSelectedIndex == i);
		if(!Item.m_Visible)
			continue;

		Item.m_Rect.VMargin(5.0f, &Label);

		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_EllipsisAtEnd = true;
		UI()->DoLabel(&Label, m_Map.m_vSettings[i].m_aCommand, 10.0f, TEXTALIGN_ML, Props);
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(s_CommandSelectedIndex != NewSelected)
	{
		s_CommandSelectedIndex = NewSelected;
		m_SettingsCommandInput.Set(m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand);
	}
}

void CEditor::RenderExtraEditorDragBar(CUIRect View, CUIRect DragBar)
{
	enum EDragOperation
	{
		OP_NONE,
		OP_DRAGGING,
		OP_CLICKED
	};
	static EDragOperation s_Operation = OP_NONE;
	static float s_InitialMouseY = 0.0f;
	static float s_InitialMouseOffsetY = 0.0f;

	bool Clicked;
	bool Abrupted;
	if(int Result = DoButton_DraggableEx(&s_Operation, "", 8, &DragBar, &Clicked, &Abrupted, 0, "Change the size of the editor by dragging."))
	{
		if(s_Operation == OP_NONE && Result == 1)
		{
			s_InitialMouseY = UI()->MouseY();
			s_InitialMouseOffsetY = UI()->MouseY() - DragBar.y;
			s_Operation = OP_CLICKED;
		}

		if(Clicked || Abrupted)
			s_Operation = OP_NONE;

		if(s_Operation == OP_CLICKED && absolute(UI()->MouseY() - s_InitialMouseY) > 5.0f)
			s_Operation = OP_DRAGGING;

		if(s_Operation == OP_DRAGGING)
			m_aExtraEditorSplits[(int)m_ActiveExtraEditor] = clamp(s_InitialMouseOffsetY + View.y + View.h - UI()->MouseY(), 100.0f, 400.0f);
	}
}

void CEditor::RenderMenubar(CUIRect MenuBar)
{
	SPopupMenuProperties PopupProperties;
	PopupProperties.m_Corners = IGraphics::CORNER_R | IGraphics::CORNER_B;

	CUIRect FileButton;
	static int s_FileButton = 0;
	MenuBar.VSplitLeft(60.0f, &FileButton, &MenuBar);
	if(DoButton_Menu(&s_FileButton, "File", 0, &FileButton, 0, nullptr))
	{
		static SPopupMenuId s_PopupMenuFileId;
		UI()->DoPopupMenu(&s_PopupMenuFileId, FileButton.x, FileButton.y + FileButton.h - 1.0f, 120.0f, 174.0f, this, PopupMenuFile, PopupProperties);
	}

	MenuBar.VSplitLeft(5.0f, nullptr, &MenuBar);

	CUIRect ToolsButton;
	static int s_ToolsButton = 0;
	MenuBar.VSplitLeft(60.0f, &ToolsButton, &MenuBar);
	if(DoButton_Menu(&s_ToolsButton, "Tools", 0, &ToolsButton, 0, nullptr))
	{
		static SPopupMenuId s_PopupMenuToolsId;
		UI()->DoPopupMenu(&s_PopupMenuToolsId, ToolsButton.x, ToolsButton.y + ToolsButton.h - 1.0f, 200.0f, 50.0f, this, PopupMenuTools, PopupProperties);
	}

	MenuBar.VSplitLeft(5.0f, nullptr, &MenuBar);

	CUIRect SettingsButton;
	static int s_SettingsButton = 0;
	MenuBar.VSplitLeft(60.0f, &SettingsButton, &MenuBar);
	if(DoButton_Menu(&s_SettingsButton, "Settings", 0, &SettingsButton, 0, nullptr))
	{
		static SPopupMenuId s_PopupMenuEntitiesId;
		UI()->DoPopupMenu(&s_PopupMenuEntitiesId, SettingsButton.x, SettingsButton.y + SettingsButton.h - 1.0f, 200.0f, 64.0f, this, PopupMenuSettings, PopupProperties);
	}

	CUIRect ChangedIndicator, Info, Close;
	MenuBar.VSplitLeft(5.0f, nullptr, &MenuBar);
	MenuBar.VSplitLeft(MenuBar.h, &ChangedIndicator, &MenuBar);
	MenuBar.VSplitRight(20.0f, &MenuBar, &Close);
	Close.VSplitLeft(5.0f, nullptr, &Close);
	MenuBar.VSplitLeft(MenuBar.w * 0.6f, &MenuBar, &Info);
	MenuBar.VSplitRight(5.0f, &MenuBar, nullptr);

	if(m_Map.m_Modified)
	{
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		UI()->DoLabel(&ChangedIndicator, FONT_ICON_CIRCLE, 8.0f, TEXTALIGN_MC);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		static int s_ChangedIndicator;
		DoButton_Editor_Common(&s_ChangedIndicator, "", 0, &ChangedIndicator, 0, "This map has unsaved changes"); // just for the tooltip, result unused
	}

	char aBuf[IO_MAX_PATH_LENGTH + 32];
	str_format(aBuf, sizeof(aBuf), "File: %s", m_aFileName);
	SLabelProperties Props;
	Props.m_MaxWidth = MenuBar.w;
	Props.m_EllipsisAtEnd = true;
	UI()->DoLabel(&MenuBar, aBuf, 10.0f, TEXTALIGN_ML, Props);

	char aTimeStr[6];
	str_timestamp_format(aTimeStr, sizeof(aTimeStr), "%H:%M");

	str_format(aBuf, sizeof(aBuf), "X: %.1f, Y: %.1f, Z: %.1f, A: %.1f, G: %i  %s", UI()->MouseWorldX() / 32.0f, UI()->MouseWorldY() / 32.0f, MapView()->m_Zoom.GetValue(), m_AnimateSpeed, m_GridFactor, aTimeStr);
	UI()->DoLabel(&Info, aBuf, 10.0f, TEXTALIGN_MR);

	static int s_CloseButton = 0;
	if(DoButton_Editor(&s_CloseButton, "×", 0, &Close, 0, "Exits from the editor") || (m_Dialog == DIALOG_NONE && !UI()->IsPopupOpen() && !m_PopupEventActivated && Input()->KeyPress(KEY_ESCAPE)))
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

	// render checker
	RenderBackground(View, m_CheckerTexture, 32.0f, 1.0f);

	CUIRect MenuBar, ModeBar, ToolBar, StatusBar, ExtraEditor, ToolBox;
	m_ShowPicker = Input()->KeyIsPressed(KEY_SPACE) && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && m_vSelectedLayers.size() == 1;

	if(m_GuiActive)
	{
		View.HSplitTop(16.0f, &MenuBar, &View);
		View.HSplitTop(53.0f, &ToolBar, &View);
		View.VSplitLeft(100.0f, &ToolBox, &View);
		View.HSplitBottom(16.0f, &View, &StatusBar);
		if(!m_ShowPicker && m_ActiveExtraEditor != EXTRAEDITOR_NONE)
			View.HSplitBottom(m_aExtraEditorSplits[(int)m_ActiveExtraEditor], &View, &ExtraEditor);
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

	if(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr)
	{
		// handle zoom hotkeys
		if(Input()->KeyPress(KEY_KP_MINUS))
			MapView()->m_Zoom.ChangeValue(50.0f);
		if(Input()->KeyPress(KEY_KP_PLUS))
			MapView()->m_Zoom.ChangeValue(-50.0f);
		if(Input()->KeyPress(KEY_KP_MULTIPLY))
			MapView()->ResetZoom();

		// handle brush save/load hotkeys
		for(int i = KEY_1; i <= KEY_0; i++)
		{
			if(Input()->KeyPress(i))
			{
				int Slot = i - KEY_1;
				if(Input()->ModifierIsPressed() && !m_pBrush->IsEmpty())
				{
					dbg_msg("editor", "saving current brush to %d", Slot);
					m_apSavedBrushes[Slot] = std::make_shared<CLayerGroup>(*m_pBrush);
				}
				else if(m_apSavedBrushes[Slot])
				{
					dbg_msg("editor", "loading brush from slot %d", Slot);
					m_pBrush = std::make_shared<CLayerGroup>(*m_apSavedBrushes[Slot]);
				}
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
		ToolBar.VSplitLeft(100.0f, &ModeBar, &ToolBar);

		RenderBackground(StatusBar, m_BackgroundTexture, 128.0f, Brightness);
		StatusBar.Margin(2.0f, &StatusBar);
	}

	// do the toolbar
	if(m_Mode == MODE_LAYERS)
		DoToolbarLayers(ToolBar);
	else if(m_Mode == MODE_SOUNDS)
		DoToolbarSounds(ToolBar);

	if(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr)
	{
		const bool ModPressed = Input()->ModifierIsPressed();
		const bool ShiftPressed = Input()->ShiftIsPressed();
		const bool AltPressed = Input()->AltIsPressed();
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
			InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_MAP, "Append map", "Append", "maps", false, CallbackAppendMap, this);
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
					InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_MAP, "Load map", "Load", "maps", false, CallbackOpenMap, this);
				}
			}
		}

		// ctrl+shift+alt+s to save copy
		if(Input()->KeyPress(KEY_S) && ModPressed && ShiftPressed && AltPressed)
			InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save", "maps", true, CallbackSaveCopyMap, this);
		// ctrl+shift+s to save as
		else if(Input()->KeyPress(KEY_S) && ModPressed && ShiftPressed)
			InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save", "maps", true, CallbackSaveMap, this);
		// ctrl+s to save
		else if(Input()->KeyPress(KEY_S) && ModPressed)
		{
			if(m_aFileName[0] && m_ValidSaveFilename)
			{
				if(!m_PopupEventWasActivated)
				{
					str_copy(m_aFileSaveName, m_aFileName);
					CallbackSaveMap(m_aFileSaveName, IStorage::TYPE_SAVE, this);
				}
			}
			else
				InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save", "maps", false, CallbackSaveMap, this);
		}
	}

	if(m_GuiActive)
	{
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

	CUIRect TooltipRect;
	if(m_GuiActive)
	{
		RenderMenubar(MenuBar);
		RenderModebar(ModeBar);
		if(!m_ShowPicker)
		{
			if(m_ActiveExtraEditor != EXTRAEDITOR_NONE)
			{
				RenderBackground(ExtraEditor, m_BackgroundTexture, 128.0f, Brightness);
				ExtraEditor.HMargin(2.0f, &ExtraEditor);
				ExtraEditor.VSplitRight(2.0f, &ExtraEditor, nullptr);
			}

			static bool s_ShowServerSettingsEditorLast = false;
			if(m_ActiveExtraEditor == EXTRAEDITOR_ENVELOPES)
			{
				RenderEnvelopeEditor(ExtraEditor);
			}
			else if(m_ActiveExtraEditor == EXTRAEDITOR_SERVER_SETTINGS)
			{
				RenderServerSettingsEditor(ExtraEditor, s_ShowServerSettingsEditorLast);
			}
			s_ShowServerSettingsEditorLast = m_ActiveExtraEditor == EXTRAEDITOR_SERVER_SETTINGS;
		}
		RenderStatusbar(StatusBar, &TooltipRect);
	}

	RenderPressedKeys(View);
	RenderSavingIndicator(View);

	if(m_Dialog == DIALOG_FILE)
	{
		static int s_NullUiTarget = 0;
		UI()->SetHotItem(&s_NullUiTarget);
		RenderFileDialog();
	}

	if(m_PopupEventActivated)
	{
		static SPopupMenuId s_PopupEventId;
		constexpr float PopupWidth = 400.0f;
		constexpr float PopupHeight = 150.0f;
		UI()->DoPopupMenu(&s_PopupEventId, Width / 2.0f - PopupWidth / 2.0f, Height / 2.0f - PopupHeight / 2.0f, PopupWidth, PopupHeight, this, PopupEvent);
		m_PopupEventActivated = false;
		m_PopupEventWasActivated = true;
	}

	if(m_Dialog == DIALOG_NONE && !UI()->IsPopupHovered() && (!m_GuiActive || UI()->MouseInside(&View)))
	{
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
			MapView()->m_Zoom.ChangeValue(20.0f);
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
			MapView()->m_Zoom.ChangeValue(-20.0f);
	}

	MapView()->UpdateZoom();

	UI()->RenderPopupMenus();
	FreeDynamicPopupMenus();

	// The tooltip can be set in popup menus so we have to render the tooltip after the popup menus.
	if(m_GuiActive)
		RenderTooltip(TooltipRect);

	RenderMousePointer();
}

void CEditor::RenderPressedKeys(CUIRect View)
{
	if(!g_Config.m_EdShowkeys)
		return;

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

void CEditor::RenderSavingIndicator(CUIRect View)
{
	if(m_WriterFinishJobs.empty())
		return;

	UI()->MapScreen();
	CUIRect Label;
	View.Margin(20.0f, &Label);
	UI()->DoLabel(&Label, "Saving…", 24.0f, TEXTALIGN_BR);
}

void CEditor::FreeDynamicPopupMenus()
{
	auto Iterator = m_PopupMessageContexts.begin();
	while(Iterator != m_PopupMessageContexts.end())
	{
		if(!UI()->IsPopupOpen(Iterator->second))
		{
			CUI::SMessagePopupContext *pContext = Iterator->second;
			Iterator = m_PopupMessageContexts.erase(Iterator);
			delete pContext;
		}
		else
			++Iterator;
	}
}

void CEditor::RenderMousePointer()
{
	if(!m_ShowMousePointer)
		return;

	Graphics()->WrapClamp();
	Graphics()->TextureSet(m_CursorTexture);
	Graphics()->QuadsBegin();
	if(ms_pUiGotContext == UI()->HotItem())
		Graphics()->SetColor(1, 0, 0, 1);
	IGraphics::CQuadItem QuadItem(UI()->MouseX(), UI()->MouseY(), 16.0f, 16.0f);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
	Graphics()->WrapNormal();
}

void CEditor::Reset(bool CreateDefault)
{
	UI()->ClosePopupMenus();
	m_Map.Clean();

	// create default layers
	if(CreateDefault)
	{
		m_EditorWasUsedBefore = true;
		m_Map.CreateDefault(GetEntitiesTexture());
	}

	SelectGameLayer();
	DeselectQuads();
	DeselectQuadPoints();
	m_SelectedEnvelope = 0;
	m_SelectedImage = 0;
	m_SelectedSound = 0;
	m_SelectedSource = -1;

	m_MouseDeltaX = 0;
	m_MouseDeltaY = 0;
	m_MouseDeltaWx = 0;
	m_MouseDeltaWy = 0;

	m_Map.m_Modified = false;
	m_Map.m_ModifiedAuto = false;
	m_Map.m_LastModifiedTime = -1.0f;
	m_Map.m_LastSaveTime = Client()->GlobalTime();
	m_Map.m_LastAutosaveUpdateTime = -1.0f;

	m_ShowEnvelopePreview = SHOWENV_NONE;
	m_ShiftBy = 1;

	m_ResetZoomEnvelope = true;
	m_SettingsCommandInput.Clear();
}

int CEditor::GetLineDistance() const
{
	if(MapView()->m_Zoom.GetValue() <= 100.0f)
		return 16;
	else if(MapView()->m_Zoom.GetValue() <= 250.0f)
		return 32;
	else if(MapView()->m_Zoom.GetValue() <= 450.0f)
		return 64;
	else if(MapView()->m_Zoom.GetValue() <= 850.0f)
		return 128;
	else if(MapView()->m_Zoom.GetValue() <= 1550.0f)
		return 256;
	else
		return 512;
}

void CEditorMap::OnModify()
{
	m_Modified = true;
	m_ModifiedAuto = true;
	m_LastModifiedTime = m_pEditor->Client()->GlobalTime();
}

void CEditorMap::DeleteEnvelope(int Index)
{
	if(Index < 0 || Index >= (int)m_vpEnvelopes.size())
		return;

	OnModify();

	VisitEnvelopeReferences([Index](int &ElementIndex) {
		if(ElementIndex == Index)
			ElementIndex = -1;
		else if(ElementIndex > Index)
			ElementIndex--;
	});

	m_vpEnvelopes.erase(m_vpEnvelopes.begin() + Index);
}

void CEditorMap::SwapEnvelopes(int Index0, int Index1)
{
	if(Index0 < 0 || Index0 >= (int)m_vpEnvelopes.size())
		return;
	if(Index1 < 0 || Index1 >= (int)m_vpEnvelopes.size())
		return;
	if(Index0 == Index1)
		return;

	OnModify();

	VisitEnvelopeReferences([Index0, Index1](int &ElementIndex) {
		if(ElementIndex == Index0)
			ElementIndex = Index1;
		else if(ElementIndex == Index1)
			ElementIndex = Index0;
	});

	std::swap(m_vpEnvelopes[Index0], m_vpEnvelopes[Index1]);
}

template<typename F>
void CEditorMap::VisitEnvelopeReferences(F &&Visitor)
{
	for(auto &pGroup : m_vpGroups)
	{
		for(auto &pLayer : pGroup->m_vpLayers)
		{
			if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(pLayer);
				for(auto &Quad : pLayerQuads->m_vQuads)
				{
					Visitor(Quad.m_PosEnv);
					Visitor(Quad.m_ColorEnv);
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				std::shared_ptr<CLayerTiles> pLayerTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
				Visitor(pLayerTiles->m_ColorEnv);
			}
			else if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(pLayer);
				for(auto &Source : pLayerSounds->m_vSources)
				{
					Visitor(Source.m_PosEnv);
					Visitor(Source.m_SoundEnv);
				}
			}
		}
	}
}

void CEditorMap::MakeGameLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pGameLayer = std::static_pointer_cast<CLayerGame>(pLayer);
	m_pGameLayer->m_pEditor = m_pEditor;
}

void CEditorMap::MakeGameGroup(std::shared_ptr<CLayerGroup> pGroup)
{
	m_pGameGroup = std::move(pGroup);
	m_pGameGroup->m_GameGroup = true;
	str_copy(m_pGameGroup->m_aName, "Game");
}

void CEditorMap::Clean()
{
	m_vpGroups.clear();
	m_vpEnvelopes.clear();
	m_vpImages.clear();
	m_vpSounds.clear();

	m_MapInfo.Reset();
	m_MapInfoTmp.Reset();

	m_vSettings.clear();

	m_pGameLayer = nullptr;
	m_pGameGroup = nullptr;

	m_Modified = false;
	m_ModifiedAuto = false;

	m_pTeleLayer = nullptr;
	m_pSpeedupLayer = nullptr;
	m_pFrontLayer = nullptr;
	m_pSwitchLayer = nullptr;
	m_pTuneLayer = nullptr;
}

void CEditorMap::CreateDefault(IGraphics::CTextureHandle EntitiesTexture)
{
	// add background
	std::shared_ptr<CLayerGroup> pGroup = NewGroup();
	pGroup->m_ParallaxX = 0;
	pGroup->m_ParallaxY = 0;
	pGroup->m_CustomParallaxZoom = 0;
	pGroup->m_ParallaxZoom = 0;
	std::shared_ptr<CLayerQuads> pLayer = std::make_shared<CLayerQuads>();
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
	MakeGameLayer(std::make_shared<CLayerGame>(50, 50));
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
	if(!m_FrontTexture.IsValid())
		m_FrontTexture = Graphics()->LoadTexture("editor/front.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, GetTextureUsageFlag());
	return m_FrontTexture;
}

IGraphics::CTextureHandle CEditor::GetTeleTexture()
{
	if(!m_TeleTexture.IsValid())
		m_TeleTexture = Graphics()->LoadTexture("editor/tele.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, GetTextureUsageFlag());
	return m_TeleTexture;
}

IGraphics::CTextureHandle CEditor::GetSpeedupTexture()
{
	if(!m_SpeedupTexture.IsValid())
		m_SpeedupTexture = Graphics()->LoadTexture("editor/speedup.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, GetTextureUsageFlag());
	return m_SpeedupTexture;
}

IGraphics::CTextureHandle CEditor::GetSwitchTexture()
{
	if(!m_SwitchTexture.IsValid())
		m_SwitchTexture = Graphics()->LoadTexture("editor/switch.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, GetTextureUsageFlag());
	return m_SwitchTexture;
}

IGraphics::CTextureHandle CEditor::GetTuneTexture()
{
	if(!m_TuneTexture.IsValid())
		m_TuneTexture = Graphics()->LoadTexture("editor/tune.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, GetTextureUsageFlag());
	return m_TuneTexture;
}

IGraphics::CTextureHandle CEditor::GetEntitiesTexture()
{
	if(!m_EntitiesTexture.IsValid())
		m_EntitiesTexture = Graphics()->LoadTexture("editor/entities/DDNet.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, GetTextureUsageFlag());
	return m_EntitiesTexture;
}

void CEditor::Init()
{
	m_pInput = Kernel()->RequestInterface<IInput>();
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pConfig = Kernel()->RequestInterface<IConfigManager>()->Values();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pGraphics = Kernel()->RequestInterface<IGraphics>();
	m_pTextRender = Kernel()->RequestInterface<ITextRender>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pSound = Kernel()->RequestInterface<ISound>();
	m_UI.Init(Kernel());
	m_UI.SetPopupMenuClosedCallback([this]() {
		m_PopupEventWasActivated = false;
	});
	m_RenderTools.Init(m_pGraphics, m_pTextRender);
	m_ZoomEnvelopeX.Init(this);
	m_ZoomEnvelopeY.Init(this);
	m_MapView.Init(this);
	m_Map.m_pEditor = this;

	m_CheckerTexture = Graphics()->LoadTexture("editor/checker.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	m_BackgroundTexture = Graphics()->LoadTexture("editor/background.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	m_CursorTexture = Graphics()->LoadTexture("editor/cursor.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);

	m_pTilesetPicker = std::make_shared<CLayerTiles>(16, 16);
	m_pTilesetPicker->m_pEditor = this;
	m_pTilesetPicker->MakePalette();
	m_pTilesetPicker->m_Readonly = true;

	m_pQuadsetPicker = std::make_shared<CLayerQuads>();
	m_pQuadsetPicker->m_pEditor = this;
	m_pQuadsetPicker->NewQuad(0, 0, 64, 64);
	m_pQuadsetPicker->m_Readonly = true;

	m_pBrush = std::make_shared<CLayerGroup>();
	m_pBrush->m_pMap = &m_Map;

	Reset(false);

	ResetMenuBackgroundPositions();
	m_vpMenuBackgroundPositionNames.resize(CMenuBackground::NUM_POS);
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_START] = "start";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_BROWSER_INTERNET] = "browser(internet)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_BROWSER_LAN] = "browser(lan)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_DEMOS] = "demos";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_NEWS] = "news";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_BROWSER_FAVORITES] = "favorites";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_LANGUAGE] = "settings(language)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_GENERAL] = "settings(general)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_PLAYER] = "settings(player)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_TEE] = "settings(tee)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_APPEARANCE] = "settings(appearance)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_CONTROLS] = "settings(controls)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_GRAPHICS] = "settings(graphics)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_SOUND] = "settings(sound)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_DDNET] = "settings(ddnet)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_ASSETS] = "settings(assets)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_BROWSER_CUSTOM0] = "custom(ddnet)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_BROWSER_CUSTOM1] = "custom(kog)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_BROWSER_CUSTOM2] = "custom(3)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_BROWSER_CUSTOM3] = "custom(4)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_RESERVED0] = "reserved settings(1)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_RESERVED1] = "reserved settings(2)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_RESERVED0] = "reserved(1)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_RESERVED1] = "reserved(2)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_RESERVED2] = "reserved(3)";
}

void CEditor::PlaceBorderTiles()
{
	std::shared_ptr<CLayerTiles> pT = std::static_pointer_cast<CLayerTiles>(GetSelectedLayerType(0, LAYERTYPE_TILES));

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

void CEditor::HandleCursorMovement()
{
	static float s_MouseX = 0.0f;
	static float s_MouseY = 0.0f;

	float MouseRelX = 0.0f, MouseRelY = 0.0f;
	IInput::ECursorType CursorType = Input()->CursorRelative(&MouseRelX, &MouseRelY);
	if(CursorType != IInput::CURSOR_NONE)
		UI()->ConvertMouseMove(&MouseRelX, &MouseRelY, CursorType);

	m_MouseDeltaX += MouseRelX;
	m_MouseDeltaY += MouseRelY;

	if(!UI()->CheckMouseLock())
	{
		s_MouseX = clamp<float>(s_MouseX + MouseRelX, 0.0f, Graphics()->WindowWidth());
		s_MouseY = clamp<float>(s_MouseY + MouseRelY, 0.0f, Graphics()->WindowHeight());
	}

	// update positions for ui, but only update ui when rendering
	m_MouseX = UI()->Screen()->w * ((float)s_MouseX / Graphics()->WindowWidth());
	m_MouseY = UI()->Screen()->h * ((float)s_MouseY / Graphics()->WindowHeight());

	// fix correct world x and y
	std::shared_ptr<CLayerGroup> pGroup = GetSelectedGroup();
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

	for(const std::shared_ptr<CLayerGroup> &pGameGroup : m_Map.m_vpGroups)
	{
		if(!pGameGroup->m_GameGroup)
			continue;

		float aPoints[4];
		pGameGroup->Mapping(aPoints);

		float WorldWidth = aPoints[2] - aPoints[0];
		float WorldHeight = aPoints[3] - aPoints[1];

		m_MouseWorldNoParaX = aPoints[0] + WorldWidth * (s_MouseX / Graphics()->WindowWidth());
		m_MouseWorldNoParaY = aPoints[1] + WorldHeight * (s_MouseY / Graphics()->WindowHeight());
	}
}

void CEditor::DispatchInputEvents()
{
	for(size_t i = 0; i < Input()->NumEvents(); i++)
	{
		const IInput::CEvent &Event = Input()->GetEvent(i);
		if(!Input()->IsEventValid(Event))
			continue;
		UI()->OnInput(Event);
	}
}

void CEditor::HandleAutosave()
{
	const float Time = Client()->GlobalTime();
	const float LastAutosaveUpdateTime = m_Map.m_LastAutosaveUpdateTime;
	m_Map.m_LastAutosaveUpdateTime = Time;

	if(g_Config.m_EdAutosaveInterval == 0)
		return; // autosave disabled
	if(!m_Map.m_ModifiedAuto || m_Map.m_LastModifiedTime < 0.0f)
		return; // no unsaved changes

	// Add time to autosave timer if the editor was disabled for more than 10 seconds,
	// to prevent autosave from immediately activating when the editor is activated
	// after being deactivated for some time.
	if(LastAutosaveUpdateTime >= 0.0f && Time - LastAutosaveUpdateTime > 10.0f)
	{
		m_Map.m_LastSaveTime += Time - LastAutosaveUpdateTime;
	}

	// Check if autosave timer has expired.
	if(m_Map.m_LastSaveTime >= Time || Time - m_Map.m_LastSaveTime < 60 * g_Config.m_EdAutosaveInterval)
		return;

	// Wait for 5 seconds of no modification before saving, to prevent autosave
	// from immediately activating when a map is first modified or while user is
	// modifying the map, but don't delay the autosave for more than 1 minute.
	if(Time - m_Map.m_LastModifiedTime < 5.0f && Time - m_Map.m_LastSaveTime < 60 * (g_Config.m_EdAutosaveInterval + 1))
		return;

	PerformAutosave();
}

bool CEditor::PerformAutosave()
{
	char aDate[20];
	char aAutosavePath[IO_MAX_PATH_LENGTH];
	str_timestamp(aDate, sizeof(aDate));
	char aFileNameNoExt[IO_MAX_PATH_LENGTH];
	if(m_aFileName[0] == '\0')
	{
		str_copy(aFileNameNoExt, "unnamed");
	}
	else
	{
		const char *pFileName = fs_filename(m_aFileName);
		str_truncate(aFileNameNoExt, sizeof(aFileNameNoExt), pFileName, str_length(pFileName) - str_length(".map"));
	}
	str_format(aAutosavePath, sizeof(aAutosavePath), "maps/auto/%s_%s.map", aFileNameNoExt, aDate);

	m_Map.m_LastSaveTime = Client()->GlobalTime();
	if(Save(aAutosavePath))
	{
		m_Map.m_ModifiedAuto = false;
		// Clean up autosaves
		if(g_Config.m_EdAutosaveMax)
		{
			CFileCollection AutosavedMaps;
			AutosavedMaps.Init(Storage(), "maps/auto", aFileNameNoExt, ".map", g_Config.m_EdAutosaveMax);
		}
		return true;
	}
	else
	{
		ShowFileDialogError("Failed to automatically save map to file '%s'.", aAutosavePath);
		return false;
	}
}

void CEditor::HandleWriterFinishJobs()
{
	if(m_WriterFinishJobs.empty())
		return;

	std::shared_ptr<CDataFileWriterFinishJob> pJob = m_WriterFinishJobs.front();
	if(pJob->Status() != IJob::STATE_DONE)
		return;
	m_WriterFinishJobs.pop_front();

	char aBuf[2 * IO_MAX_PATH_LENGTH + 128];
	if(Storage()->FileExists(pJob->GetRealFileName(), IStorage::TYPE_SAVE) && !Storage()->RemoveFile(pJob->GetRealFileName(), IStorage::TYPE_SAVE))
	{
		str_format(aBuf, sizeof(aBuf), "Saving failed: Could not remove old map file '%s'.", pJob->GetRealFileName());
		ShowFileDialogError("%s", aBuf);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "editor/save", aBuf);
		return;
	}

	if(!Storage()->RenameFile(pJob->GetTempFileName(), pJob->GetRealFileName(), IStorage::TYPE_SAVE))
	{
		str_format(aBuf, sizeof(aBuf), "Saving failed: Could not move temporary map file '%s' to '%s'.", pJob->GetTempFileName(), pJob->GetRealFileName());
		ShowFileDialogError("%s", aBuf);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "editor/save", aBuf);
		return;
	}

	str_format(aBuf, sizeof(aBuf), "saving '%s' done", pJob->GetRealFileName());
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor/save", aBuf);

	// send rcon.. if we can
	if(Client()->RconAuthed())
	{
		CServerInfo CurrentServerInfo;
		Client()->GetServerInfo(&CurrentServerInfo);
		NETADDR ServerAddr = Client()->ServerAddress();
		const unsigned char aIpv4Localhost[16] = {127, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		const unsigned char aIpv6Localhost[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

		// and if we're on localhost
		if(!mem_comp(ServerAddr.ip, aIpv4Localhost, sizeof(aIpv4Localhost)) || !mem_comp(ServerAddr.ip, aIpv6Localhost, sizeof(aIpv6Localhost)))
		{
			char aMapName[128];
			IStorage::StripPathAndExtension(pJob->GetRealFileName(), aMapName, sizeof(aMapName));
			if(!str_comp(aMapName, CurrentServerInfo.m_aMap))
				Client()->Rcon("reload");
		}
	}
}

void CEditor::OnUpdate()
{
	CUIElementBase::Init(UI()); // update static pointer because game and editor use separate UI

	if(!m_EditorWasUsedBefore)
	{
		m_EditorWasUsedBefore = true;
		Reset();
	}

	HandleCursorMovement();
	DispatchInputEvents();
	HandleAutosave();
	HandleWriterFinishJobs();
}

void CEditor::OnRender()
{
	UI()->ResetMouseSlow();

	// toggle gui
	if(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_TAB))
		m_GuiActive = !m_GuiActive;

	if(Input()->KeyPress(KEY_F10))
		m_ShowMousePointer = false;

	if(m_Animate)
		m_AnimateTime = (time_get() - m_AnimateStart) / (float)time_freq();
	else
		m_AnimateTime = 0;

	ms_pUiGotContext = nullptr;
	UI()->StartCheck();

	UI()->Update(m_MouseX, m_MouseY, m_MouseDeltaX, m_MouseDeltaY, m_MouseWorldX, m_MouseWorldY);

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

	CLineInput::RenderCandidates();
}

void CEditor::OnActivate()
{
	ResetMentions();
	ResetIngameMoved();
}

void CEditor::OnWindowResize()
{
	UI()->OnWindowResize();
}

void CEditor::LoadCurrentMap()
{
	if(Load(m_pClient->GetCurrentMapPath(), IStorage::TYPE_SAVE))
	{
		m_ValidSaveFilename = true;
	}
	else
	{
		Load(m_pClient->GetCurrentMapPath(), IStorage::TYPE_ALL);
		m_ValidSaveFilename = false;
	}

	CGameClient *pGameClient = (CGameClient *)Kernel()->RequestInterface<IGameClient>();
	vec2 Center = pGameClient->m_Camera.m_Center;

	MapView()->m_WorldOffset = Center;
}

IEditor *CreateEditor() { return new CEditor; }

void CEditor::ResetMenuBackgroundPositions()
{
	std::array<vec2, CMenuBackground::NUM_POS> aBackgroundPositions = GenerateMenuBackgroundPositions();
	m_vMenuBackgroundPositions.assign(aBackgroundPositions.begin(), aBackgroundPositions.end());

	if(m_Map.m_pGameLayer)
	{
		for(int y = 0; y < m_Map.m_pGameLayer->m_Height; ++y)
		{
			for(int x = 0; x < m_Map.m_pGameLayer->m_Width; ++x)
			{
				CTile Tile = m_Map.m_pGameLayer->GetTile(x, y);
				if(Tile.m_Index >= TILE_TIME_CHECKPOINT_FIRST && Tile.m_Index <= TILE_TIME_CHECKPOINT_LAST)
				{
					int ArrayIndex = clamp<int>((Tile.m_Index - TILE_TIME_CHECKPOINT_FIRST), 0, CMenuBackground::NUM_POS);
					m_vMenuBackgroundPositions[ArrayIndex] = vec2(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
				}

				x += Tile.m_Skip;
			}
		}
	}

	m_vMenuBackgroundCollisions.clear();
	m_vMenuBackgroundCollisions.resize(m_vMenuBackgroundPositions.size());
	for(size_t i = 0; i < m_vMenuBackgroundPositions.size(); i++)
	{
		for(size_t j = i + 1; j < m_vMenuBackgroundPositions.size(); j++)
		{
			if(i != j && distance(m_vMenuBackgroundPositions[i], m_vMenuBackgroundPositions[j]) < 0.001f)
				m_vMenuBackgroundCollisions.at(i).push_back(j);
		}
	}
}

// DDRace

void CEditorMap::MakeTeleLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pTeleLayer = std::static_pointer_cast<CLayerTele>(pLayer);
	m_pTeleLayer->m_pEditor = m_pEditor;
}

void CEditorMap::MakeSpeedupLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pSpeedupLayer = std::static_pointer_cast<CLayerSpeedup>(pLayer);
	m_pSpeedupLayer->m_pEditor = m_pEditor;
}

void CEditorMap::MakeFrontLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pFrontLayer = std::static_pointer_cast<CLayerFront>(pLayer);
	m_pFrontLayer->m_pEditor = m_pEditor;
}

void CEditorMap::MakeSwitchLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pSwitchLayer = std::static_pointer_cast<CLayerSwitch>(pLayer);
	m_pSwitchLayer->m_pEditor = m_pEditor;
}

void CEditorMap::MakeTuneLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pTuneLayer = std::static_pointer_cast<CLayerTune>(pLayer);
	m_pTuneLayer->m_pEditor = m_pEditor;
}
