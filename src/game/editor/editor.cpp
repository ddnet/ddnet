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
#include <engine/gfx/image_loader.h>
#include <engine/gfx/image_manipulation.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/shared/filecollection.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/components/camera.h>
#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/generated/client_data.h>
#include <game/localization.h>

#include <game/editor/editor_history.h>
#include <game/editor/mapitems/image.h>
#include <game/editor/mapitems/sound.h>

#include "auto_map.h"
#include "editor.h"
#include "editor_actions.h"

#include <chrono>
#include <iterator>
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

void CEditor::EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, void *pUser)
{
	CEditor *pThis = (CEditor *)pUser;
	if(Env < 0 || Env >= (int)pThis->m_Map.m_vpEnvelopes.size())
		return;

	std::shared_ptr<CEnvelope> pEnv = pThis->m_Map.m_vpEnvelopes[Env];
	float Time = pThis->m_AnimateTime;
	Time *= pThis->m_AnimateSpeed;
	Time += (TimeOffsetMillis / 1000.0f);
	pEnv->Eval(Time, Result, Channels);
}

/********************************************************
 OTHER
*********************************************************/

bool CEditor::DoEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners, const char *pToolTip, const std::vector<STextColorSplit> &vColorSplits)
{
	UpdateTooltip(pLineInput, pRect, pToolTip);
	return Ui()->DoEditBox(pLineInput, pRect, FontSize, Corners, vColorSplits);
}

bool CEditor::DoClearableEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners, const char *pToolTip, const std::vector<STextColorSplit> &vColorSplits)
{
	UpdateTooltip(pLineInput, pRect, pToolTip);
	return Ui()->DoClearableEditBox(pLineInput, pRect, FontSize, Corners, vColorSplits);
}

ColorRGBA CEditor::GetButtonColor(const void *pId, int Checked)
{
	if(Checked < 0)
		return ColorRGBA(0, 0, 0, 0.5f);

	switch(Checked)
	{
	case 8: // invisible
		return ColorRGBA(0, 0, 0, 0);
	case 7: // selected + game layers
		if(Ui()->HotItem() == pId)
			return ColorRGBA(1, 0, 0, 0.4f);
		return ColorRGBA(1, 0, 0, 0.2f);

	case 6: // game layers
		if(Ui()->HotItem() == pId)
			return ColorRGBA(1, 1, 1, 0.4f);
		return ColorRGBA(1, 1, 1, 0.2f);

	case 5: // selected + image/sound should be embedded
		if(Ui()->HotItem() == pId)
			return ColorRGBA(1, 0, 0, 0.75f);
		return ColorRGBA(1, 0, 0, 0.5f);

	case 4: // image/sound should be embedded
		if(Ui()->HotItem() == pId)
			return ColorRGBA(1, 0, 0, 1.0f);
		return ColorRGBA(1, 0, 0, 0.875f);

	case 3: // selected + unused image/sound
		if(Ui()->HotItem() == pId)
			return ColorRGBA(1, 0, 1, 0.75f);
		return ColorRGBA(1, 0, 1, 0.5f);

	case 2: // unused image/sound
		if(Ui()->HotItem() == pId)
			return ColorRGBA(0, 0, 1, 0.75f);
		return ColorRGBA(0, 0, 1, 0.5f);

	case 1: // selected
		if(Ui()->HotItem() == pId)
			return ColorRGBA(1, 0, 0, 0.75f);
		return ColorRGBA(1, 0, 0, 0.5f);

	default: // regular
		if(Ui()->HotItem() == pId)
			return ColorRGBA(1, 1, 1, 0.75f);
		return ColorRGBA(1, 1, 1, 0.5f);
	}
}

void CEditor::UpdateTooltip(const void *pId, const CUIRect *pRect, const char *pToolTip)
{
	if(Ui()->MouseInside(pRect) && !pToolTip)
		str_copy(m_aTooltip, "");
	else if(Ui()->HotItem() == pId && pToolTip)
		str_copy(m_aTooltip, pToolTip);
}

int CEditor::DoButton_Editor_Common(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(Ui()->MouseInside(pRect))
	{
		if(Flags & BUTTON_CONTEXT)
			ms_pUiGotContext = pId;
	}

	UpdateTooltip(pId, pRect, pToolTip);
	return Ui()->DoButtonLogic(pId, Checked, pRect);
}

int CEditor::DoButton_Editor(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	pRect->Draw(GetButtonColor(pId, Checked), IGraphics::CORNER_ALL, 3.0f);
	CUIRect NewRect = *pRect;
	Ui()->DoLabel(&NewRect, pText, 10.0f, TEXTALIGN_MC);
	Checked %= 2;
	return DoButton_Editor_Common(pId, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Env(const void *pId, const char *pText, int Checked, const CUIRect *pRect, const char *pToolTip, ColorRGBA BaseColor, int Corners)
{
	float Bright = Checked ? 1.0f : 0.5f;
	float Alpha = Ui()->HotItem() == pId ? 1.0f : 0.75f;
	ColorRGBA Color = ColorRGBA(BaseColor.r * Bright, BaseColor.g * Bright, BaseColor.b * Bright, Alpha);

	pRect->Draw(Color, Corners, 3.0f);
	Ui()->DoLabel(pRect, pText, 10.0f, TEXTALIGN_MC);
	Checked %= 2;
	return DoButton_Editor_Common(pId, pText, Checked, pRect, 0, pToolTip);
}

int CEditor::DoButton_MenuItem(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(Ui()->HotItem() == pId || Checked)
		pRect->Draw(GetButtonColor(pId, Checked), IGraphics::CORNER_ALL, 3.0f);

	CUIRect Rect;
	pRect->VMargin(5.0f, &Rect);

	SLabelProperties Props;
	Props.m_MaxWidth = Rect.w;
	Props.m_EllipsisAtEnd = true;
	Ui()->DoLabel(&Rect, pText, 10.0f, TEXTALIGN_ML, Props);

	return DoButton_Editor_Common(pId, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Ex(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners, float FontSize, int Align)
{
	pRect->Draw(GetButtonColor(pId, Checked), Corners, 3.0f);

	CUIRect Rect;
	pRect->VMargin(((Align & TEXTALIGN_MASK_HORIZONTAL) == TEXTALIGN_CENTER) ? 1.0f : 5.0f, &Rect);

	SLabelProperties Props;
	Props.m_MaxWidth = Rect.w;
	Props.m_EllipsisAtEnd = true;
	Ui()->DoLabel(&Rect, pText, FontSize, Align, Props);

	return DoButton_Editor_Common(pId, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_FontIcon(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners, float FontSize)
{
	pRect->Draw(GetButtonColor(pId, Checked), Corners, 3.0f);

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	Ui()->DoLabel(pRect, pText, FontSize, TEXTALIGN_MC);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	return DoButton_Editor_Common(pId, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_DraggableEx(const void *pId, const char *pText, int Checked, const CUIRect *pRect, bool *pClicked, bool *pAbrupted, int Flags, const char *pToolTip, int Corners, float FontSize)
{
	pRect->Draw(GetButtonColor(pId, Checked), Corners, 3.0f);

	CUIRect Rect;
	pRect->VMargin(pRect->w > 20.0f ? 5.0f : 0.0f, &Rect);

	SLabelProperties Props;
	Props.m_MaxWidth = Rect.w;
	Props.m_EllipsisAtEnd = true;
	Ui()->DoLabel(&Rect, pText, FontSize, TEXTALIGN_MC, Props);

	if(Ui()->MouseInside(pRect))
	{
		if(Flags & BUTTON_CONTEXT)
			ms_pUiGotContext = pId;
	}

	UpdateTooltip(pId, pRect, pToolTip);
	return Ui()->DoDraggableButtonLogic(pId, Checked, pRect, pClicked, pAbrupted);
}

void CEditor::RenderBackground(CUIRect View, IGraphics::CTextureHandle Texture, float Size, float Brightness) const
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

SEditResult<int> CEditor::UiDoValueSelector(void *pId, CUIRect *pRect, const char *pLabel, int Current, int Min, int Max, int Step, float Scale, const char *pToolTip, bool IsDegree, bool IsHex, int Corners, const ColorRGBA *pColor, bool ShowValue)
{
	// logic
	static bool s_DidScroll = false;
	static float s_ScrollValue = 0.0f;
	static CLineInputNumber s_NumberInput;
	static int s_ButtonUsed = -1;
	static void *s_pLastTextId = nullptr;

	const bool Inside = Ui()->MouseInside(pRect);
	const int Base = IsHex ? 16 : 10;

	if(Ui()->HotItem() == pId && s_ButtonUsed >= 0 && !Ui()->MouseButton(s_ButtonUsed))
	{
		Ui()->DisableMouseLock();
		if(Ui()->CheckActiveItem(pId))
		{
			Ui()->SetActiveItem(nullptr);
		}
		if(Inside && ((s_ButtonUsed == 0 && !s_DidScroll && Ui()->DoDoubleClickLogic(pId)) || s_ButtonUsed == 1))
		{
			s_pLastTextId = pId;
			s_NumberInput.SetInteger(Current, Base);
			s_NumberInput.SelectAll();
		}
		s_ButtonUsed = -1;
	}

	if(s_pLastTextId == pId)
	{
		str_copy(m_aTooltip, "Type your number");
		Ui()->SetActiveItem(&s_NumberInput);
		DoEditBox(&s_NumberInput, pRect, 10.0f, Corners);

		if(Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || ((Ui()->MouseButtonClicked(1) || Ui()->MouseButtonClicked(0)) && !Inside))
		{
			Current = clamp(s_NumberInput.GetInteger(Base), Min, Max);
			Ui()->DisableMouseLock();
			Ui()->SetActiveItem(nullptr);
			s_pLastTextId = nullptr;
		}

		if(Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
		{
			Ui()->DisableMouseLock();
			Ui()->SetActiveItem(nullptr);
			s_pLastTextId = nullptr;
		}
	}
	else
	{
		if(Ui()->CheckActiveItem(pId))
		{
			if(s_ButtonUsed == 0 && Ui()->MouseButton(0))
			{
				s_ScrollValue += Ui()->MouseDeltaX() * (Input()->ShiftIsPressed() ? 0.05f : 1.0f);

				if(absolute(s_ScrollValue) >= Scale)
				{
					int Count = (int)(s_ScrollValue / Scale);
					s_ScrollValue = std::fmod(s_ScrollValue, Scale);
					Current += Step * Count;
					Current = clamp(Current, Min, Max);
					s_DidScroll = true;

					// Constrain to discrete steps
					if(Count > 0)
						Current = Current / Step * Step;
					else
						Current = std::ceil(Current / (float)Step) * Step;
				}
			}

			if(pToolTip && s_pLastTextId != pId)
				str_copy(m_aTooltip, pToolTip);
		}
		else if(Ui()->HotItem() == pId)
		{
			if(Ui()->MouseButton(0))
			{
				s_ButtonUsed = 0;
				s_DidScroll = false;
				s_ScrollValue = 0.0f;
				Ui()->SetActiveItem(pId);
				Ui()->EnableMouseLock(pId);
			}
			else if(Ui()->MouseButton(1))
			{
				s_ButtonUsed = 1;
				Ui()->SetActiveItem(pId);
			}

			if(pToolTip && s_pLastTextId != pId)
				str_copy(m_aTooltip, pToolTip);
		}

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
		pRect->Draw(pColor ? *pColor : GetButtonColor(pId, 0), Corners, 3.0f);
		Ui()->DoLabel(pRect, aBuf, 10, TEXTALIGN_MC);
	}

	if(Inside && !Ui()->MouseButton(0) && !Ui()->MouseButton(1))
		Ui()->SetHotItem(pId);

	static const void *s_pEditing = nullptr;
	EEditState State = EEditState::NONE;
	if(s_pEditing == pId)
	{
		State = EEditState::EDITING;
	}
	if(((Ui()->CheckActiveItem(pId) && Ui()->CheckMouseLock()) || s_pLastTextId == pId) && s_pEditing != pId)
	{
		State = EEditState::START;
		s_pEditing = pId;
	}
	if(!Ui()->CheckMouseLock() && s_pLastTextId != pId && s_pEditing == pId)
	{
		State = EEditState::END;
		s_pEditing = nullptr;
	}

	return SEditResult<int>{State, Current};
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

CSoundSource *CEditor::GetSelectedSource() const
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
	DeselectQuads();
	DeselectQuadPoints();
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
	m_SelectedQuadPoints = 0;
}

void CEditor::SelectQuadPoint(int QuadIndex, int Index)
{
	SelectQuad(QuadIndex);
	m_SelectedQuadPoints = 1 << Index;
}

void CEditor::ToggleSelectQuadPoint(int QuadIndex, int Index)
{
	if(IsQuadPointSelected(QuadIndex, Index))
	{
		m_SelectedQuadPoints ^= 1 << Index;
	}
	else
	{
		if(!IsQuadSelected(QuadIndex))
		{
			ToggleSelectQuad(QuadIndex);
		}

		if(!(m_SelectedQuadPoints & 1 << Index))
		{
			m_SelectedQuadPoints ^= 1 << Index;
		}
	}
}

void CEditor::DeleteSelectedQuads()
{
	std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(GetSelectedLayerType(0, LAYERTYPE_QUADS));
	if(!pLayer)
		return;

	std::vector<int> vSelectedQuads(m_vSelectedQuads);
	std::vector<CQuad> vDeletedQuads;

	for(int i = 0; i < (int)m_vSelectedQuads.size(); ++i)
	{
		auto const &Quad = pLayer->m_vQuads[m_vSelectedQuads[i]];
		vDeletedQuads.push_back(Quad);

		pLayer->m_vQuads.erase(pLayer->m_vQuads.begin() + m_vSelectedQuads[i]);
		for(int j = i + 1; j < (int)m_vSelectedQuads.size(); ++j)
			if(m_vSelectedQuads[j] > m_vSelectedQuads[i])
				m_vSelectedQuads[j]--;

		m_vSelectedQuads.erase(m_vSelectedQuads.begin() + i);
		i--;
	}

	m_EditorHistory.RecordAction(std::make_shared<CEditorActionDeleteQuad>(this, m_SelectedGroup, m_vSelectedLayers[0], vSelectedQuads, vDeletedQuads));
}

bool CEditor::IsQuadSelected(int Index) const
{
	return FindSelectedQuadIndex(Index) >= 0;
}

bool CEditor::IsQuadCornerSelected(int Index) const
{
	return m_SelectedQuadPoints & (1 << Index);
}

bool CEditor::IsQuadPointSelected(int QuadIndex, int Index) const
{
	return IsQuadSelected(QuadIndex) && IsQuadCornerSelected(Index);
}

int CEditor::FindSelectedQuadIndex(int Index) const
{
	for(size_t i = 0; i < m_vSelectedQuads.size(); ++i)
		if(m_vSelectedQuads[i] == Index)
			return i;
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

std::pair<int, int> CEditor::EnvGetSelectedTimeAndValue() const
{
	if(m_SelectedEnvelope < 0 || m_SelectedEnvelope >= (int)m_Map.m_vpEnvelopes.size())
		return {};

	std::shared_ptr<CEnvelope> pEnvelope = m_Map.m_vpEnvelopes[m_SelectedEnvelope];
	int CurrentTime;
	int CurrentValue;
	if(IsTangentInSelected())
	{
		auto [SelectedIndex, SelectedChannel] = m_SelectedTangentInPoint;

		CurrentTime = pEnvelope->m_vPoints[SelectedIndex].m_Time + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aInTangentDeltaX[SelectedChannel];
		CurrentValue = pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aInTangentDeltaY[SelectedChannel];
	}
	else if(IsTangentOutSelected())
	{
		auto [SelectedIndex, SelectedChannel] = m_SelectedTangentOutPoint;

		CurrentTime = pEnvelope->m_vPoints[SelectedIndex].m_Time + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aOutTangentDeltaX[SelectedChannel];
		CurrentValue = pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aOutTangentDeltaY[SelectedChannel];
	}
	else
	{
		auto [SelectedIndex, SelectedChannel] = m_vSelectedEnvelopePoints.front();

		CurrentTime = pEnvelope->m_vPoints[SelectedIndex].m_Time;
		CurrentValue = pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel];
	}

	return std::pair<int, int>{CurrentTime, CurrentValue};
}

void CEditor::SelectNextLayer()
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

void CEditor::SelectPreviousLayer()
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

bool CEditor::CallbackOpenMap(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;
	if(pEditor->Load(pFileName, StorageType))
	{
		pEditor->m_ValidSaveFilename = StorageType == IStorage::TYPE_SAVE && (pEditor->m_pFileDialogPath == pEditor->m_aFileDialogCurrentFolder || (pEditor->m_pFileDialogPath == pEditor->m_aFileDialogCurrentLink && str_comp(pEditor->m_aFileDialogCurrentLink, "themes") == 0));
		if(pEditor->m_Dialog == DIALOG_FILE)
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

bool CEditor::CallbackSaveImage(const char *pFileName, int StorageType, void *pUser)
{
	dbg_assert(StorageType == IStorage::TYPE_SAVE, "Saving only allowed for IStorage::TYPE_SAVE");

	CEditor *pEditor = static_cast<CEditor *>(pUser);
	char aBuf[IO_MAX_PATH_LENGTH];

	// add file extension
	if(!str_endswith(pFileName, ".png"))
	{
		str_format(aBuf, sizeof(aBuf), "%s.png", pFileName);
		pFileName = aBuf;
	}

	std::shared_ptr<CEditorImage> pImg = pEditor->m_Map.m_vpImages[pEditor->m_SelectedImage];

	if(CImageLoader::SavePng(pEditor->Storage()->OpenFile(pFileName, IOFLAG_WRITE, StorageType), pFileName, *pImg))
	{
		pEditor->m_Dialog = DIALOG_NONE;
		return true;
	}
	else
	{
		pEditor->ShowFileDialogError("Failed to write image to file '%s'.", pFileName);
		return false;
	}
}

bool CEditor::CallbackSaveSound(const char *pFileName, int StorageType, void *pUser)
{
	dbg_assert(StorageType == IStorage::TYPE_SAVE, "Saving only allowed for IStorage::TYPE_SAVE");

	CEditor *pEditor = static_cast<CEditor *>(pUser);
	char aBuf[IO_MAX_PATH_LENGTH];

	// add file extension
	if(!str_endswith(pFileName, ".opus"))
	{
		str_format(aBuf, sizeof(aBuf), "%s.opus", pFileName);
		pFileName = aBuf;
	}
	std::shared_ptr<CEditorSound> pSound = pEditor->m_Map.m_vpSounds[pEditor->m_SelectedSound];
	IOHANDLE File = pEditor->Storage()->OpenFile(pFileName, IOFLAG_WRITE, StorageType);

	if(File)
	{
		io_write(File, pSound->m_pData, pSound->m_DataSize);
		io_close(File);
		pEditor->OnDialogClose();
		pEditor->m_Dialog = DIALOG_NONE;
		return true;
	}
	pEditor->ShowFileDialogError("Failed to open file '%s'.", pFileName);
	return false;
}

bool CEditor::CallbackCustomEntities(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;

	char aBuf[IO_MAX_PATH_LENGTH];
	IStorage::StripPathAndExtension(pFileName, aBuf, sizeof(aBuf));

	if(std::find(pEditor->m_vSelectEntitiesFiles.begin(), pEditor->m_vSelectEntitiesFiles.end(), std::string(aBuf)) != pEditor->m_vSelectEntitiesFiles.end())
	{
		pEditor->ShowFileDialogError("Custom entities cannot have the same name as default entities.");
		return false;
	}

	CImageInfo ImgInfo;
	if(!pEditor->Graphics()->LoadPng(ImgInfo, pFileName, StorageType))
	{
		pEditor->ShowFileDialogError("Failed to load image from file '%s'.", pFileName);
		return false;
	}

	pEditor->m_SelectEntitiesImage = aBuf;
	pEditor->m_AllowPlaceUnusedTiles = -1;
	pEditor->m_PreventUnusedTilesWasWarned = false;

	pEditor->Graphics()->UnloadTexture(&pEditor->m_EntitiesTexture);
	pEditor->m_EntitiesTexture = pEditor->Graphics()->LoadTextureRawMove(ImgInfo, pEditor->GetTextureUsageFlag());

	pEditor->m_Dialog = DIALOG_NONE;
	return true;
}

void CEditor::DoAudioPreview(CUIRect View, const void *pPlayPauseButtonId, const void *pStopButtonId, const void *pSeekBarId, int SampleId)
{
	CUIRect Button, SeekBar;
	// play/pause button
	{
		View.VSplitLeft(View.h, &Button, &View);
		if(DoButton_FontIcon(pPlayPauseButtonId, Sound()->IsPlaying(SampleId) ? FONT_ICON_PAUSE : FONT_ICON_PLAY, 0, &Button, 0, "Play/pause audio preview", IGraphics::CORNER_ALL) ||
			(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_SPACE)))
		{
			if(Sound()->IsPlaying(SampleId))
			{
				Sound()->Pause(SampleId);
			}
			else
			{
				if(SampleId != m_ToolbarPreviewSound && m_ToolbarPreviewSound >= 0 && Sound()->IsPlaying(m_ToolbarPreviewSound))
					Sound()->Pause(m_ToolbarPreviewSound);

				Sound()->Play(CSounds::CHN_GUI, SampleId, ISound::FLAG_PREVIEW, 1.0f);
			}
		}
	}
	// stop button
	{
		View.VSplitLeft(2.0f, nullptr, &View);
		View.VSplitLeft(View.h, &Button, &View);
		if(DoButton_FontIcon(pStopButtonId, FONT_ICON_STOP, 0, &Button, 0, "Stop audio preview", IGraphics::CORNER_ALL))
		{
			Sound()->Stop(SampleId);
		}
	}
	// do seekbar
	{
		View.VSplitLeft(5.0f, nullptr, &View);
		const float Cut = std::min(View.w, 200.0f);
		View.VSplitLeft(Cut, &SeekBar, &View);
		SeekBar.HMargin(2.5f, &SeekBar);

		const float Rounding = 5.0f;

		char aBuffer[64];
		const float CurrentTime = Sound()->GetSampleCurrentTime(SampleId);
		const float TotalTime = Sound()->GetSampleTotalTime(SampleId);

		// draw seek bar
		SeekBar.Draw(ColorRGBA(0, 0, 0, 0.5f), IGraphics::CORNER_ALL, Rounding);

		// draw filled bar
		const float Amount = CurrentTime / TotalTime;
		CUIRect FilledBar = SeekBar;
		FilledBar.w = 2 * Rounding + (FilledBar.w - 2 * Rounding) * Amount;
		FilledBar.Draw(ColorRGBA(1, 1, 1, 0.5f), IGraphics::CORNER_ALL, Rounding);

		// draw time
		char aCurrentTime[32];
		str_time_float(CurrentTime, TIME_HOURS, aCurrentTime, sizeof(aCurrentTime));
		char aTotalTime[32];
		str_time_float(TotalTime, TIME_HOURS, aTotalTime, sizeof(aTotalTime));
		str_format(aBuffer, sizeof(aBuffer), "%s / %s", aCurrentTime, aTotalTime);
		Ui()->DoLabel(&SeekBar, aBuffer, SeekBar.h * 0.70f, TEXTALIGN_MC);

		// do the logic
		const bool Inside = Ui()->MouseInside(&SeekBar);

		if(Ui()->CheckActiveItem(pSeekBarId))
		{
			if(!Ui()->MouseButton(0))
			{
				Ui()->SetActiveItem(nullptr);
			}
			else
			{
				const float AmountSeek = clamp((Ui()->MouseX() - SeekBar.x - Rounding) / (SeekBar.w - 2 * Rounding), 0.0f, 1.0f);
				Sound()->SetSampleCurrentTime(SampleId, AmountSeek);
			}
		}
		else if(Ui()->HotItem() == pSeekBarId)
		{
			if(Ui()->MouseButton(0))
				Ui()->SetActiveItem(pSeekBarId);
		}

		if(Inside && !Ui()->MouseButton(0))
			Ui()->SetHotItem(pSeekBarId);
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
		TB_Top.VSplitLeft(25.0f, &Button, &TB_Top);
		static char s_AnimateButton;
		if(DoButton_FontIcon(&s_AnimateButton, FONT_ICON_CIRCLE_PLAY, m_Animate, &Button, 0, "[ctrl+m] Toggle animation", IGraphics::CORNER_L) ||
			(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_M) && ModPressed))
		{
			m_AnimateStart = time_get();
			m_Animate = !m_Animate;
		}

		// animation settings button
		TB_Top.VSplitLeft(14.0f, &Button, &TB_Top);
		static char s_AnimateSettingsButton;
		if(DoButton_FontIcon(&s_AnimateSettingsButton, FONT_ICON_CIRCLE_CHEVRON_DOWN, 0, &Button, 0, "Change animation settings.", IGraphics::CORNER_R, 8.0f))
		{
			m_AnimateUpdatePopup = true;
			static SPopupMenuId s_PopupAnimateSettingsId;
			Ui()->DoPopupMenu(&s_PopupAnimateSettingsId, Button.x, Button.y + Button.h, 150.0f, 37.0f, this, PopupAnimateSettings);
		}

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		// proof button
		TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
		if(DoButton_Ex(&m_QuickActionProof, m_QuickActionProof.Label(), m_QuickActionProof.Active(), &Button, 0, m_QuickActionProof.Description(), IGraphics::CORNER_L))
		{
			m_QuickActionProof.Call();
		}

		TB_Top.VSplitLeft(14.0f, &Button, &TB_Top);
		static int s_ProofModeButton = 0;
		if(DoButton_FontIcon(&s_ProofModeButton, FONT_ICON_CIRCLE_CHEVRON_DOWN, 0, &Button, 0, "Select proof mode.", IGraphics::CORNER_R, 8.0f))
		{
			static SPopupMenuId s_PopupProofModeId;
			Ui()->DoPopupMenu(&s_PopupProofModeId, Button.x, Button.y + Button.h, 60.0f, 36.0f, this, PopupProofMode);
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
		TB_Top.VSplitLeft(25.0f, &Button, &TB_Top);
		static int s_GridButton = 0;
		if(DoButton_FontIcon(&s_GridButton, FONT_ICON_BORDER_ALL, m_QuickActionToggleGrid.Active(), &Button, 0, m_QuickActionToggleGrid.Description(), IGraphics::CORNER_L) ||
			(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_G) && ModPressed && !ShiftPressed))
		{
			m_QuickActionToggleGrid.Call();
		}

		// grid settings button
		TB_Top.VSplitLeft(14.0f, &Button, &TB_Top);
		static char s_GridSettingsButton;
		if(DoButton_FontIcon(&s_GridSettingsButton, FONT_ICON_CIRCLE_CHEVRON_DOWN, 0, &Button, 0, "Change grid settings.", IGraphics::CORNER_R, 8.0f))
		{
			MapView()->MapGrid()->DoSettingsPopup(vec2(Button.x, Button.y + Button.h));
		}

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		// zoom group
		TB_Top.VSplitLeft(20.0f, &Button, &TB_Top);
		static int s_ZoomOutButton = 0;
		if(DoButton_FontIcon(&s_ZoomOutButton, FONT_ICON_MINUS, 0, &Button, 0, m_QuickActionZoomOut.Description(), IGraphics::CORNER_L))
		{
			m_QuickActionZoomOut.Call();
		}

		TB_Top.VSplitLeft(25.0f, &Button, &TB_Top);
		static int s_ZoomNormalButton = 0;
		if(DoButton_FontIcon(&s_ZoomNormalButton, FONT_ICON_MAGNIFYING_GLASS, 0, &Button, 0, m_QuickActionResetZoom.Description(), IGraphics::CORNER_NONE))
		{
			m_QuickActionResetZoom.Call();
		}

		TB_Top.VSplitLeft(20.0f, &Button, &TB_Top);
		static int s_ZoomInButton = 0;
		if(DoButton_FontIcon(&s_ZoomInButton, FONT_ICON_PLUS, 0, &Button, 0, m_QuickActionZoomIn.Description(), IGraphics::CORNER_R))
		{
			m_QuickActionZoomIn.Call();
		}

		TB_Top.VSplitLeft(5.0f, nullptr, &TB_Top);

		// undo/redo group
		TB_Top.VSplitLeft(25.0f, &Button, &TB_Top);
		static int s_UndoButton = 0;
		if(DoButton_FontIcon(&s_UndoButton, FONT_ICON_UNDO, m_EditorHistory.CanUndo() - 1, &Button, 0, "[ctrl+z] Undo last action", IGraphics::CORNER_L))
		{
			m_EditorHistory.Undo();
		}

		TB_Top.VSplitLeft(25.0f, &Button, &TB_Top);
		static int s_RedoButton = 0;
		if(DoButton_FontIcon(&s_RedoButton, FONT_ICON_REDO, m_EditorHistory.CanRedo() - 1, &Button, 0, "[ctrl+y] Redo last action", IGraphics::CORNER_R))
		{
			m_EditorHistory.Redo();
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
			if(DoButton_FontIcon(&s_CcwButton, FONT_ICON_ARROW_ROTATE_LEFT, Enabled, &Button, 0, "[R] Rotates the brush counter clockwise", IGraphics::CORNER_L) || (Input()->KeyPress(KEY_R) && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr))
			{
				for(auto &pLayer : m_pBrush->m_vpLayers)
					pLayer->BrushRotate(-s_RotationAmount / 360.0f * pi * 2);
			}

			TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
			auto RotationAmountRes = UiDoValueSelector(&s_RotationAmount, &Button, "", s_RotationAmount, TileLayer ? 90 : 1, 359, TileLayer ? 90 : 1, TileLayer ? 10.0f : 2.0f, "Rotation of the brush in degrees. Use left mouse button to drag and change the value. Hold shift to be more precise.", true, false, IGraphics::CORNER_NONE);
			s_RotationAmount = RotationAmountRes.m_Value;

			TB_Top.VSplitLeft(25.0f, &Button, &TB_Top);
			static int s_CwButton = 0;
			if(DoButton_FontIcon(&s_CwButton, FONT_ICON_ARROW_ROTATE_RIGHT, Enabled, &Button, 0, "[T] Rotates the brush clockwise", IGraphics::CORNER_R) || (Input()->KeyPress(KEY_T) && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr))
			{
				for(auto &pLayer : m_pBrush->m_vpLayers)
					pLayer->BrushRotate(s_RotationAmount / 360.0f * pi * 2);
			}
		}

		// Color pipette and palette
		{
			const float PipetteButtonWidth = 30.0f;
			const float ColorPickerButtonWidth = 20.0f;
			const float Spacing = 2.0f;
			const size_t NumColorsShown = clamp<int>(round_to_int((TB_Top.w - PipetteButtonWidth - 40.0f) / (ColorPickerButtonWidth + Spacing)), 1, std::size(m_aSavedColors));

			CUIRect ColorPalette;
			TB_Top.VSplitRight(NumColorsShown * (ColorPickerButtonWidth + Spacing) + PipetteButtonWidth, &TB_Top, &ColorPalette);

			// Pipette button
			static char s_PipetteButton;
			ColorPalette.VSplitLeft(PipetteButtonWidth, &Button, &ColorPalette);
			ColorPalette.VSplitLeft(Spacing, nullptr, &ColorPalette);
			if(DoButton_FontIcon(&s_PipetteButton, FONT_ICON_EYE_DROPPER, m_QuickActionPipette.Active(), &Button, 0, m_QuickActionPipette.Description(), IGraphics::CORNER_ALL) ||
				(CLineInput::GetActiveInput() == nullptr && ModPressed && ShiftPressed && Input()->KeyPress(KEY_C)))
			{
				m_QuickActionPipette.Call();
			}

			// Palette color pickers
			for(size_t i = 0; i < NumColorsShown; ++i)
			{
				ColorPalette.VSplitLeft(ColorPickerButtonWidth, &Button, &ColorPalette);
				ColorPalette.VSplitLeft(Spacing, nullptr, &ColorPalette);
				const auto &&SetColor = [&](ColorRGBA NewColor) {
					m_aSavedColors[i] = NewColor;
				};
				DoColorPickerButton(&m_aSavedColors[i], &Button, m_aSavedColors[i], SetColor);
			}
		}
	}

	// Bottom line buttons
	{
		// refocus button
		{
			TB_Bottom.VSplitLeft(50.0f, &Button, &TB_Bottom);
			int FocusButtonChecked = MapView()->IsFocused() ? -1 : 1;
			if(DoButton_Editor(&m_QuickActionRefocus, m_QuickActionRefocus.Label(), FocusButtonChecked, &Button, 0, m_QuickActionRefocus.Description()) || (m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_HOME)))
				m_QuickActionRefocus.Call();
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
					CUi::FPopupMenuFunction pfnPopupFunc = nullptr;
					int Rows = 0;
					int ExtraWidth = 0;
					if(pS == m_Map.m_pSwitchLayer)
					{
						pButtonName = "Switch";
						pfnPopupFunc = PopupSwitch;
						Rows = 3;
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
						Rows = m_TeleNumbers.size() + 1;
						ExtraWidth = 50;
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
							if(!Ui()->IsPopupOpen(&s_PopupModifierId))
							{
								Ui()->DoPopupMenu(&s_PopupModifierId, Button.x, Button.y + Button.h, 120 + ExtraWidth, 10.0f + Rows * 13.0f, this, pfnPopupFunc);
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

			if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				if(DoButton_Editor(&m_QuickActionAddQuad, m_QuickActionAddQuad.Label(), 0, &Button, 0, m_QuickActionAddQuad.Description()) ||
					(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_Q) && ModPressed))
				{
					m_QuickActionAddQuad.Call();
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				if(DoButton_Editor(&m_QuickActionAddSound, m_QuickActionAddSound.Label(), 0, &Button, 0, m_QuickActionAddSound.Description()) ||
					(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_Q) && ModPressed))
				{
					m_QuickActionAddSound.Call();
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

void CEditor::DoToolbarImages(CUIRect ToolBar)
{
	CUIRect ToolBarTop, ToolBarBottom;
	ToolBar.HSplitMid(&ToolBarTop, &ToolBarBottom, 5.0f);

	if(m_SelectedImage >= 0 && (size_t)m_SelectedImage < m_Map.m_vpImages.size())
	{
		const std::shared_ptr<CEditorImage> pSelectedImage = m_Map.m_vpImages[m_SelectedImage];
		char aLabel[64];
		str_format(aLabel, sizeof(aLabel), "Size: %" PRIzu " × %" PRIzu, pSelectedImage->m_Width, pSelectedImage->m_Height);
		Ui()->DoLabel(&ToolBarBottom, aLabel, 12.0f, TEXTALIGN_ML);
	}
}

void CEditor::DoToolbarSounds(CUIRect ToolBar)
{
	CUIRect ToolBarTop, ToolBarBottom;
	ToolBar.HSplitMid(&ToolBarTop, &ToolBarBottom, 5.0f);

	if(m_SelectedSound >= 0 && (size_t)m_SelectedSound < m_Map.m_vpSounds.size())
	{
		const std::shared_ptr<CEditorSound> pSelectedSound = m_Map.m_vpSounds[m_SelectedSound];
		if(pSelectedSound->m_SoundId != m_ToolbarPreviewSound && m_ToolbarPreviewSound >= 0 && Sound()->IsPlaying(m_ToolbarPreviewSound))
			Sound()->Stop(m_ToolbarPreviewSound);
		m_ToolbarPreviewSound = pSelectedSound->m_SoundId;
	}
	else
	{
		m_ToolbarPreviewSound = -1;
	}

	if(m_ToolbarPreviewSound >= 0)
	{
		static int s_PlayPauseButton, s_StopButton, s_SeekBar = 0;
		DoAudioPreview(ToolBarBottom, &s_PlayPauseButton, &s_StopButton, &s_SeekBar, m_ToolbarPreviewSound);
	}
}

static void Rotate(const CPoint *pCenter, CPoint *pPoint, float Rotation)
{
	int x = pPoint->x - pCenter->x;
	int y = pPoint->y - pCenter->y;
	pPoint->x = (int)(x * std::cos(Rotation) - y * std::sin(Rotation) + pCenter->x);
	pPoint->y = (int)(x * std::sin(Rotation) + y * std::cos(Rotation) + pCenter->y);
}

void CEditor::DoSoundSource(int LayerIndex, CSoundSource *pSource, int Index)
{
	void *pId = &pSource->m_Position;

	static ESoundSourceOp s_Operation = ESoundSourceOp::OP_NONE;

	float wx = Ui()->MouseWorldX();
	float wy = Ui()->MouseWorldY();

	float CenterX = fx2f(pSource->m_Position.x);
	float CenterY = fx2f(pSource->m_Position.y);

	float dx = (CenterX - wx) / m_MouseWorldScale;
	float dy = (CenterY - wy) / m_MouseWorldScale;
	if(dx * dx + dy * dy < 50)
		Ui()->SetHotItem(pId);

	const bool IgnoreGrid = Input()->AltIsPressed();
	static CSoundSourceOperationTracker s_Tracker(this);

	if(s_Operation == ESoundSourceOp::OP_NONE)
	{
		if(!Ui()->MouseButton(0))
			s_Tracker.End();
	}

	if(Ui()->CheckActiveItem(pId))
	{
		if(s_Operation != ESoundSourceOp::OP_NONE)
		{
			s_Tracker.Begin(pSource, s_Operation, LayerIndex);
		}

		if(m_MouseDeltaWorld != vec2(0.0f, 0.0f))
		{
			if(s_Operation == ESoundSourceOp::OP_MOVE)
			{
				float x = wx;
				float y = wy;
				if(MapView()->MapGrid()->IsEnabled() && !IgnoreGrid)
					MapView()->MapGrid()->SnapToGrid(x, y);
				pSource->m_Position.x = f2fx(x);
				pSource->m_Position.y = f2fx(y);
			}
		}

		if(s_Operation == ESoundSourceOp::OP_CONTEXT_MENU)
		{
			if(!Ui()->MouseButton(1))
			{
				if(m_vSelectedLayers.size() == 1)
				{
					static SPopupMenuId s_PopupSourceId;
					Ui()->DoPopupMenu(&s_PopupSourceId, Ui()->MouseX(), Ui()->MouseY(), 120, 200, this, PopupSource);
					Ui()->DisableMouseLock();
				}
				s_Operation = ESoundSourceOp::OP_NONE;
				Ui()->SetActiveItem(nullptr);
			}
		}
		else
		{
			if(!Ui()->MouseButton(0))
			{
				Ui()->DisableMouseLock();
				s_Operation = ESoundSourceOp::OP_NONE;
				Ui()->SetActiveItem(nullptr);
			}
		}

		Graphics()->SetColor(1, 1, 1, 1);
	}
	else if(Ui()->HotItem() == pId)
	{
		ms_pUiGotContext = pId;

		Graphics()->SetColor(1, 1, 1, 1);
		str_copy(m_aTooltip, "Left mouse button to move. Hold alt to ignore grid.");

		if(Ui()->MouseButton(0))
		{
			s_Operation = ESoundSourceOp::OP_MOVE;

			Ui()->SetActiveItem(pId);
			m_SelectedSource = Index;
		}

		if(Ui()->MouseButton(1))
		{
			m_SelectedSource = Index;
			s_Operation = ESoundSourceOp::OP_CONTEXT_MENU;
			Ui()->SetActiveItem(pId);
		}
	}
	else
	{
		Graphics()->SetColor(0, 1, 0, 1);
	}

	IGraphics::CQuadItem QuadItem(CenterX, CenterY, 5.0f * m_MouseWorldScale, 5.0f * m_MouseWorldScale);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CEditor::PreparePointDrag(const std::shared_ptr<CLayerQuads> &pLayer, CQuad *pQuad, int QuadIndex, int PointIndex)
{
	m_QuadDragOriginalPoints[QuadIndex][PointIndex] = pQuad->m_aPoints[PointIndex];
}

void CEditor::DoPointDrag(const std::shared_ptr<CLayerQuads> &pLayer, CQuad *pQuad, int QuadIndex, int PointIndex, int OffsetX, int OffsetY)
{
	pQuad->m_aPoints[PointIndex].x = m_QuadDragOriginalPoints[QuadIndex][PointIndex].x + OffsetX;
	pQuad->m_aPoints[PointIndex].y = m_QuadDragOriginalPoints[QuadIndex][PointIndex].y + OffsetY;
}

CEditor::EAxis CEditor::GetDragAxis(int OffsetX, int OffsetY) const
{
	if(Input()->ShiftIsPressed())
		if(absolute(OffsetX) < absolute(OffsetY))
			return EAxis::AXIS_Y;
		else
			return EAxis::AXIS_X;
	else
		return EAxis::AXIS_NONE;
}

void CEditor::DrawAxis(EAxis Axis, CPoint &OriginalPoint, CPoint &Point) const
{
	if(Axis == EAxis::AXIS_NONE)
		return;

	Graphics()->SetColor(1, 0, 0.1f, 1);
	if(Axis == EAxis::AXIS_X)
	{
		IGraphics::CQuadItem Line(fx2f(OriginalPoint.x + Point.x) / 2.0f, fx2f(OriginalPoint.y), fx2f(Point.x - OriginalPoint.x), 1.0f * m_MouseWorldScale);
		Graphics()->QuadsDraw(&Line, 1);
	}
	else if(Axis == EAxis::AXIS_Y)
	{
		IGraphics::CQuadItem Line(fx2f(OriginalPoint.x), fx2f(OriginalPoint.y + Point.y) / 2.0f, 1.0f * m_MouseWorldScale, fx2f(Point.y - OriginalPoint.y));
		Graphics()->QuadsDraw(&Line, 1);
	}

	// Draw ghost of original point
	IGraphics::CQuadItem QuadItem(fx2f(OriginalPoint.x), fx2f(OriginalPoint.y), 5.0f * m_MouseWorldScale, 5.0f * m_MouseWorldScale);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CEditor::ComputePointAlignments(const std::shared_ptr<CLayerQuads> &pLayer, CQuad *pQuad, int QuadIndex, int PointIndex, int OffsetX, int OffsetY, std::vector<SAlignmentInfo> &vAlignments, bool Append) const
{
	if(!Append)
		vAlignments.clear();
	if(!g_Config.m_EdAlignQuads)
		return;

	bool GridEnabled = MapView()->MapGrid()->IsEnabled() && !Input()->AltIsPressed();

	// Perform computation from the original position of this point
	int Threshold = f2fx(maximum(5.0f, 10.0f * m_MouseWorldScale));
	CPoint OrigPoint = m_QuadDragOriginalPoints.at(QuadIndex)[PointIndex];
	// Get the "current" point by applying the offset
	CPoint Point = OrigPoint + ivec2(OffsetX, OffsetY);

	// Save smallest diff on both axis to only keep closest alignments
	int SmallestDiffX = Threshold + 1, SmallestDiffY = Threshold + 1;
	// Store both axis alignments in separate vectors
	std::vector<SAlignmentInfo> vAlignmentsX, vAlignmentsY;

	// Check if we can align/snap to a specific point
	auto &&CheckAlignment = [&](CPoint *pQuadPoint) {
		int DX = pQuadPoint->x - Point.x;
		int DY = pQuadPoint->y - Point.y;
		int DiffX = absolute(DX);
		int DiffY = absolute(DY);

		if(DiffX <= Threshold && (!GridEnabled || DiffX == 0))
		{
			// Only store alignments that have the smallest difference
			if(DiffX < SmallestDiffX)
			{
				vAlignmentsX.clear();
				SmallestDiffX = DiffX;
			}

			// We can have multiple alignments having the same difference/distance
			if(DiffX == SmallestDiffX)
			{
				vAlignmentsX.push_back(SAlignmentInfo{
					*pQuadPoint, // Aligned point
					{OrigPoint.y}, // Value that can change (which is not snapped), original position
					EAxis::AXIS_Y, // The alignment axis
					PointIndex, // The index of the point
					DX,
				});
			}
		}

		if(DiffY <= Threshold && (!GridEnabled || DiffY == 0))
		{
			// Only store alignments that have the smallest difference
			if(DiffY < SmallestDiffY)
			{
				vAlignmentsY.clear();
				SmallestDiffY = DiffY;
			}

			if(DiffY == SmallestDiffY)
			{
				vAlignmentsY.push_back(SAlignmentInfo{
					*pQuadPoint,
					{OrigPoint.x},
					EAxis::AXIS_X,
					PointIndex,
					DY,
				});
			}
		}
	};

	// Iterate through all the quads of the current layer
	// Check alignment with each point of the quad (corners & pivot)
	// Compute an AABB (Axis Aligned Bounding Box) to get the center of the quad
	// Check alignment with the center of the quad
	for(size_t i = 0; i < pLayer->m_vQuads.size(); i++)
	{
		auto *pCurrentQuad = &pLayer->m_vQuads[i];
		CPoint Min = pCurrentQuad->m_aPoints[0];
		CPoint Max = pCurrentQuad->m_aPoints[0];

		for(int v = 0; v < 5; v++)
		{
			CPoint *pQuadPoint = &pCurrentQuad->m_aPoints[v];

			if(v != 4)
			{ // Don't use pivot to compute AABB
				if(pQuadPoint->x < Min.x)
					Min.x = pQuadPoint->x;
				if(pQuadPoint->y < Min.y)
					Min.y = pQuadPoint->y;
				if(pQuadPoint->x > Max.x)
					Max.x = pQuadPoint->x;
				if(pQuadPoint->y > Max.y)
					Max.y = pQuadPoint->y;
			}

			// Don't check alignment with current point
			if(pQuadPoint == &pQuad->m_aPoints[PointIndex])
				continue;

			// Don't check alignment with other selected points
			bool IsCurrentPointSelected = IsQuadSelected(i) && (IsQuadCornerSelected(v) || (v == PointIndex && PointIndex == 4));
			if(IsCurrentPointSelected)
				continue;

			CheckAlignment(pQuadPoint);
		}

		// Don't check alignment with center of selected quads
		if(!IsQuadSelected(i))
		{
			CPoint Center = (Min + Max) / 2.0f;
			CheckAlignment(&Center);
		}
	}

	// Finally concatenate both alignment vectors into the output
	vAlignments.reserve(vAlignmentsX.size() + vAlignmentsY.size());
	vAlignments.insert(vAlignments.end(), vAlignmentsX.begin(), vAlignmentsX.end());
	vAlignments.insert(vAlignments.end(), vAlignmentsY.begin(), vAlignmentsY.end());
}

void CEditor::ComputePointsAlignments(const std::shared_ptr<CLayerQuads> &pLayer, bool Pivot, int OffsetX, int OffsetY, std::vector<SAlignmentInfo> &vAlignments) const
{
	// This method is used to compute alignments from selected points
	// and only apply the closest alignment on X and Y to the offset.

	vAlignments.clear();
	std::vector<SAlignmentInfo> vAllAlignments;

	for(int Selected : m_vSelectedQuads)
	{
		CQuad *pQuad = &pLayer->m_vQuads[Selected];

		if(!Pivot)
		{
			for(int m = 0; m < 4; m++)
			{
				if(IsQuadPointSelected(Selected, m))
				{
					ComputePointAlignments(pLayer, pQuad, Selected, m, OffsetX, OffsetY, vAllAlignments, true);
				}
			}
		}
		else
		{
			ComputePointAlignments(pLayer, pQuad, Selected, 4, OffsetX, OffsetY, vAllAlignments, true);
		}
	}

	int SmallestDiffX, SmallestDiffY;
	SmallestDiffX = SmallestDiffY = std::numeric_limits<int>::max();

	std::vector<SAlignmentInfo> vAlignmentsX, vAlignmentsY;

	for(const auto &Alignment : vAllAlignments)
	{
		int AbsDiff = absolute(Alignment.m_Diff);
		if(Alignment.m_Axis == EAxis::AXIS_X)
		{
			if(AbsDiff < SmallestDiffY)
			{
				SmallestDiffY = AbsDiff;
				vAlignmentsY.clear();
			}
			if(AbsDiff == SmallestDiffY)
				vAlignmentsY.emplace_back(Alignment);
		}
		else if(Alignment.m_Axis == EAxis::AXIS_Y)
		{
			if(AbsDiff < SmallestDiffX)
			{
				SmallestDiffX = AbsDiff;
				vAlignmentsX.clear();
			}
			if(AbsDiff == SmallestDiffX)
				vAlignmentsX.emplace_back(Alignment);
		}
	}

	vAlignments.reserve(vAlignmentsX.size() + vAlignmentsY.size());
	vAlignments.insert(vAlignments.end(), vAlignmentsX.begin(), vAlignmentsX.end());
	vAlignments.insert(vAlignments.end(), vAlignmentsY.begin(), vAlignmentsY.end());
}

void CEditor::ComputeAABBAlignments(const std::shared_ptr<CLayerQuads> &pLayer, const SAxisAlignedBoundingBox &AABB, int OffsetX, int OffsetY, std::vector<SAlignmentInfo> &vAlignments) const
{
	vAlignments.clear();
	if(!g_Config.m_EdAlignQuads)
		return;

	// This method is a bit different than the point alignment in the way where instead of trying to aling 1 point to all quads,
	// we try to align 5 points to all quads, these 5 points being 5 points of an AABB.
	// Otherwise, the concept is the same, we use the original position of the AABB to make the computations.
	int Threshold = f2fx(maximum(5.0f, 10.0f * m_MouseWorldScale));
	int SmallestDiffX = Threshold + 1, SmallestDiffY = Threshold + 1;
	std::vector<SAlignmentInfo> vAlignmentsX, vAlignmentsY;

	bool GridEnabled = MapView()->MapGrid()->IsEnabled() && !Input()->AltIsPressed();

	auto &&CheckAlignment = [&](CPoint &Aligned, int Point) {
		CPoint ToCheck = AABB.m_aPoints[Point] + ivec2(OffsetX, OffsetY);
		int DX = Aligned.x - ToCheck.x;
		int DY = Aligned.y - ToCheck.y;
		int DiffX = absolute(DX);
		int DiffY = absolute(DY);

		if(DiffX <= Threshold && (!GridEnabled || DiffX == 0))
		{
			if(DiffX < SmallestDiffX)
			{
				SmallestDiffX = DiffX;
				vAlignmentsX.clear();
			}

			if(DiffX == SmallestDiffX)
			{
				vAlignmentsX.push_back(SAlignmentInfo{
					Aligned,
					{AABB.m_aPoints[Point].y},
					EAxis::AXIS_Y,
					Point,
					DX,
				});
			}
		}

		if(DiffY <= Threshold && (!GridEnabled || DiffY == 0))
		{
			if(DiffY < SmallestDiffY)
			{
				SmallestDiffY = DiffY;
				vAlignmentsY.clear();
			}

			if(DiffY == SmallestDiffY)
			{
				vAlignmentsY.push_back(SAlignmentInfo{
					Aligned,
					{AABB.m_aPoints[Point].x},
					EAxis::AXIS_X,
					Point,
					DY,
				});
			}
		}
	};

	auto &&CheckAABBAlignment = [&](CPoint &QuadMin, CPoint &QuadMax) {
		CPoint QuadCenter = (QuadMin + QuadMax) / 2.0f;
		CPoint aQuadPoints[5] = {
			QuadMin, // Top left
			{QuadMax.x, QuadMin.y}, // Top right
			{QuadMin.x, QuadMax.y}, // Bottom left
			QuadMax, // Bottom right
			QuadCenter,
		};

		// Check all points with all the other points
		for(auto &QuadPoint : aQuadPoints)
		{
			// i is the quad point which is "aligned" and that we want to compare with
			for(int j = 0; j < 5; j++)
			{
				// j is the point we try to align
				CheckAlignment(QuadPoint, j);
			}
		}
	};

	// Iterate through all quads of the current layer
	// Compute AABB of all quads and check if the dragged AABB can be aligned to this AABB.
	for(size_t i = 0; i < pLayer->m_vQuads.size(); i++)
	{
		auto *pCurrentQuad = &pLayer->m_vQuads[i];
		if(IsQuadSelected(i)) // Don't check with other selected quads
			continue;

		// Get AABB of this quad
		CPoint QuadMin = pCurrentQuad->m_aPoints[0], QuadMax = pCurrentQuad->m_aPoints[0];
		for(int v = 1; v < 4; v++)
		{
			QuadMin.x = minimum(QuadMin.x, pCurrentQuad->m_aPoints[v].x);
			QuadMin.y = minimum(QuadMin.y, pCurrentQuad->m_aPoints[v].y);
			QuadMax.x = maximum(QuadMax.x, pCurrentQuad->m_aPoints[v].x);
			QuadMax.y = maximum(QuadMax.y, pCurrentQuad->m_aPoints[v].y);
		}

		CheckAABBAlignment(QuadMin, QuadMax);
	}

	// Finally, concatenate both alignment vectors into the output
	vAlignments.reserve(vAlignmentsX.size() + vAlignmentsY.size());
	vAlignments.insert(vAlignments.end(), vAlignmentsX.begin(), vAlignmentsX.end());
	vAlignments.insert(vAlignments.end(), vAlignmentsY.begin(), vAlignmentsY.end());
}

void CEditor::DrawPointAlignments(const std::vector<SAlignmentInfo> &vAlignments, int OffsetX, int OffsetY) const
{
	if(!g_Config.m_EdAlignQuads)
		return;

	// Drawing an alignment is easy, we convert fixed to float for the aligned point coords
	// and we also convert the "changing" value after applying the offset (which might be edited to actually align the value with the alignment).
	Graphics()->SetColor(1, 0, 0.1f, 1);
	for(const SAlignmentInfo &Alignment : vAlignments)
	{
		// We don't use IGraphics::CLineItem to draw because we don't want to stop QuadsBegin(), quads work just fine.
		if(Alignment.m_Axis == EAxis::AXIS_X)
		{ // Alignment on X axis is same Y values but different X values
			IGraphics::CQuadItem Line(fx2f(Alignment.m_AlignedPoint.x), fx2f(Alignment.m_AlignedPoint.y), fx2f(Alignment.m_X + OffsetX - Alignment.m_AlignedPoint.x), 1.0f * m_MouseWorldScale);
			Graphics()->QuadsDrawTL(&Line, 1);
		}
		else if(Alignment.m_Axis == EAxis::AXIS_Y)
		{ // Alignment on Y axis is same X values but different Y values
			IGraphics::CQuadItem Line(fx2f(Alignment.m_AlignedPoint.x), fx2f(Alignment.m_AlignedPoint.y), 1.0f * m_MouseWorldScale, fx2f(Alignment.m_Y + OffsetY - Alignment.m_AlignedPoint.y));
			Graphics()->QuadsDrawTL(&Line, 1);
		}
	}
}

void CEditor::DrawAABB(const SAxisAlignedBoundingBox &AABB, int OffsetX, int OffsetY) const
{
	// Drawing an AABB is simply converting the points from fixed to float
	// Then making lines out of quads and drawing them
	vec2 TL = {fx2f(AABB.m_aPoints[SAxisAlignedBoundingBox::POINT_TL].x + OffsetX), fx2f(AABB.m_aPoints[SAxisAlignedBoundingBox::POINT_TL].y + OffsetY)};
	vec2 TR = {fx2f(AABB.m_aPoints[SAxisAlignedBoundingBox::POINT_TR].x + OffsetX), fx2f(AABB.m_aPoints[SAxisAlignedBoundingBox::POINT_TR].y + OffsetY)};
	vec2 BL = {fx2f(AABB.m_aPoints[SAxisAlignedBoundingBox::POINT_BL].x + OffsetX), fx2f(AABB.m_aPoints[SAxisAlignedBoundingBox::POINT_BL].y + OffsetY)};
	vec2 BR = {fx2f(AABB.m_aPoints[SAxisAlignedBoundingBox::POINT_BR].x + OffsetX), fx2f(AABB.m_aPoints[SAxisAlignedBoundingBox::POINT_BR].y + OffsetY)};
	vec2 Center = {fx2f(AABB.m_aPoints[SAxisAlignedBoundingBox::POINT_CENTER].x + OffsetX), fx2f(AABB.m_aPoints[SAxisAlignedBoundingBox::POINT_CENTER].y + OffsetY)};

	// We don't use IGraphics::CLineItem to draw because we don't want to stop QuadsBegin(), quads work just fine.
	IGraphics::CQuadItem Lines[4] = {
		{TL.x, TL.y, TR.x - TL.x, 1.0f * m_MouseWorldScale},
		{TL.x, TL.y, 1.0f * m_MouseWorldScale, BL.y - TL.y},
		{TR.x, TR.y, 1.0f * m_MouseWorldScale, BR.y - TR.y},
		{BL.x, BL.y, BR.x - BL.x, 1.0f * m_MouseWorldScale},
	};
	Graphics()->SetColor(1, 0, 1, 1);
	Graphics()->QuadsDrawTL(Lines, 4);

	IGraphics::CQuadItem CenterQuad(Center.x, Center.y, 5.0f * m_MouseWorldScale, 5.0f * m_MouseWorldScale);
	Graphics()->QuadsDraw(&CenterQuad, 1);
}

void CEditor::QuadSelectionAABB(const std::shared_ptr<CLayerQuads> &pLayer, SAxisAlignedBoundingBox &OutAABB)
{
	// Compute an englobing AABB of the current selection of quads
	CPoint Min{
		std::numeric_limits<int>::max(),
		std::numeric_limits<int>::max(),
	};
	CPoint Max{
		std::numeric_limits<int>::min(),
		std::numeric_limits<int>::min(),
	};
	for(int Selected : m_vSelectedQuads)
	{
		CQuad *pQuad = &pLayer->m_vQuads[Selected];
		for(int i = 0; i < 4; i++)
		{
			auto *pPoint = &pQuad->m_aPoints[i];
			Min.x = minimum(Min.x, pPoint->x);
			Min.y = minimum(Min.y, pPoint->y);
			Max.x = maximum(Max.x, pPoint->x);
			Max.y = maximum(Max.y, pPoint->y);
		}
	}
	CPoint Center = (Min + Max) / 2.0f;
	CPoint aPoints[SAxisAlignedBoundingBox::NUM_POINTS] = {
		Min, // Top left
		{Max.x, Min.y}, // Top right
		{Min.x, Max.y}, // Bottom left
		Max, // Bottom right
		Center,
	};
	mem_copy(OutAABB.m_aPoints, aPoints, sizeof(CPoint) * SAxisAlignedBoundingBox::NUM_POINTS);
}

void CEditor::ApplyAlignments(const std::vector<SAlignmentInfo> &vAlignments, int &OffsetX, int &OffsetY)
{
	if(vAlignments.empty())
		return;

	// Find X and Y aligment
	const int *pAlignedX = nullptr;
	const int *pAlignedY = nullptr;

	// To Find the alignments we simply iterate through the vector of alignments and find the first
	// X and Y alignments.
	// Then, we use the saved m_Diff to adjust the offset
	int AdjustX = 0, AdjustY = 0;
	for(const SAlignmentInfo &Alignment : vAlignments)
	{
		if(Alignment.m_Axis == EAxis::AXIS_X && !pAlignedY)
		{
			pAlignedY = &Alignment.m_AlignedPoint.y;
			AdjustY = Alignment.m_Diff;
		}
		else if(Alignment.m_Axis == EAxis::AXIS_Y && !pAlignedX)
		{
			pAlignedX = &Alignment.m_AlignedPoint.x;
			AdjustX = Alignment.m_Diff;
		}
	}

	// Adjust offset
	OffsetX += AdjustX;
	OffsetY += AdjustY;
}

void CEditor::ApplyAxisAlignment(int &OffsetX, int &OffsetY) const
{
	// This is used to preserve axis alignment when pressing `Shift`
	// Should be called before any other computation
	EAxis Axis = GetDragAxis(OffsetX, OffsetY);
	OffsetX = ((Axis == EAxis::AXIS_NONE || Axis == EAxis::AXIS_X) ? OffsetX : 0);
	OffsetY = ((Axis == EAxis::AXIS_NONE || Axis == EAxis::AXIS_Y) ? OffsetY : 0);
}

void CEditor::DoQuad(int LayerIndex, const std::shared_ptr<CLayerQuads> &pLayer, CQuad *pQuad, int Index)
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
	void *pId = &pQuad->m_aPoints[4]; // use pivot addr as id
	static std::vector<std::vector<CPoint>> s_vvRotatePoints;
	static int s_Operation = OP_NONE;
	static float s_MouseXStart = 0.0f;
	static float s_MouseYStart = 0.0f;
	static float s_RotateAngle = 0;
	float wx = Ui()->MouseWorldX();
	float wy = Ui()->MouseWorldY();
	static CPoint s_OriginalPosition;
	static std::vector<SAlignmentInfo> s_PivotAlignments; // Alignments per pivot per quad
	static std::vector<SAlignmentInfo> s_vAABBAlignments; // Alignments for one AABB (single quad or selection of multiple quads)
	static SAxisAlignedBoundingBox s_SelectionAABB; // Selection AABB
	static ivec2 s_LastOffset; // Last offset, stored as static so we can use it to draw every frame

	// get pivot
	float CenterX = fx2f(pQuad->m_aPoints[4].x);
	float CenterY = fx2f(pQuad->m_aPoints[4].y);

	const bool IgnoreGrid = Input()->AltIsPressed();

	auto &&GetDragOffset = [&]() -> ivec2 {
		float x = wx;
		float y = wy;
		if(MapView()->MapGrid()->IsEnabled() && !IgnoreGrid)
			MapView()->MapGrid()->SnapToGrid(x, y);

		int OffsetX = f2fx(x) - s_OriginalPosition.x;
		int OffsetY = f2fx(y) - s_OriginalPosition.y;

		return {OffsetX, OffsetY};
	};

	// draw selection background
	if(IsQuadSelected(Index))
	{
		Graphics()->SetColor(0, 0, 0, 1);
		IGraphics::CQuadItem QuadItem(CenterX, CenterY, 7.0f * m_MouseWorldScale, 7.0f * m_MouseWorldScale);
		Graphics()->QuadsDraw(&QuadItem, 1);
	}

	if(Ui()->CheckActiveItem(pId))
	{
		if(m_MouseDeltaWorld != vec2(0.0f, 0.0f))
		{
			if(s_Operation == OP_SELECT)
			{
				float x = s_MouseXStart - Ui()->MouseX();
				float y = s_MouseYStart - Ui()->MouseY();

				if(x * x + y * y > 20.0f)
				{
					if(!IsQuadSelected(Index))
						SelectQuad(Index);

					s_OriginalPosition = pQuad->m_aPoints[4];

					if(Input()->ShiftIsPressed())
					{
						s_Operation = OP_MOVE_PIVOT;
						// When moving, we need to save the original position of all selected pivots
						for(int Selected : m_vSelectedQuads)
						{
							CQuad *pCurrentQuad = &pLayer->m_vQuads[Selected];
							PreparePointDrag(pLayer, pCurrentQuad, Selected, 4);
						}
					}
					else
					{
						s_Operation = OP_MOVE_ALL;
						// When moving, we need to save the original position of all selected quads points
						for(int Selected : m_vSelectedQuads)
						{
							CQuad *pCurrentQuad = &pLayer->m_vQuads[Selected];
							for(size_t v = 0; v < 5; v++)
								PreparePointDrag(pLayer, pCurrentQuad, Selected, v);
						}
						// And precompute AABB of selection since it will not change during drag
						if(g_Config.m_EdAlignQuads)
							QuadSelectionAABB(pLayer, s_SelectionAABB);
					}
				}
			}

			// check if we only should move pivot
			if(s_Operation == OP_MOVE_PIVOT)
			{
				m_QuadTracker.BeginQuadTrack(pLayer, m_vSelectedQuads, -1, LayerIndex);

				s_LastOffset = GetDragOffset(); // Update offset
				ApplyAxisAlignment(s_LastOffset.x, s_LastOffset.y); // Apply axis alignment to the offset

				ComputePointsAlignments(pLayer, true, s_LastOffset.x, s_LastOffset.y, s_PivotAlignments);
				ApplyAlignments(s_PivotAlignments, s_LastOffset.x, s_LastOffset.y);

				for(auto &Selected : m_vSelectedQuads)
				{
					CQuad *pCurrentQuad = &pLayer->m_vQuads[Selected];
					DoPointDrag(pLayer, pCurrentQuad, Selected, 4, s_LastOffset.x, s_LastOffset.y);
				}
			}
			else if(s_Operation == OP_MOVE_ALL)
			{
				m_QuadTracker.BeginQuadTrack(pLayer, m_vSelectedQuads, -1, LayerIndex);

				// Compute drag offset
				s_LastOffset = GetDragOffset();
				ApplyAxisAlignment(s_LastOffset.x, s_LastOffset.y);

				// Then compute possible alignments with the selection AABB
				ComputeAABBAlignments(pLayer, s_SelectionAABB, s_LastOffset.x, s_LastOffset.y, s_vAABBAlignments);
				// Apply alignments before drag
				ApplyAlignments(s_vAABBAlignments, s_LastOffset.x, s_LastOffset.y);
				// Then do the drag
				for(int Selected : m_vSelectedQuads)
				{
					CQuad *pCurrentQuad = &pLayer->m_vQuads[Selected];
					for(int v = 0; v < 5; v++)
						DoPointDrag(pLayer, pCurrentQuad, Selected, v, s_LastOffset.x, s_LastOffset.y);
				}
			}
			else if(s_Operation == OP_ROTATE)
			{
				m_QuadTracker.BeginQuadTrack(pLayer, m_vSelectedQuads, -1, LayerIndex);

				for(size_t i = 0; i < m_vSelectedQuads.size(); ++i)
				{
					CQuad *pCurrentQuad = &pLayer->m_vQuads[m_vSelectedQuads[i]];
					for(int v = 0; v < 4; v++)
					{
						pCurrentQuad->m_aPoints[v] = s_vvRotatePoints[i][v];
						Rotate(&pCurrentQuad->m_aPoints[4], &pCurrentQuad->m_aPoints[v], s_RotateAngle);
					}
				}

				s_RotateAngle += Ui()->MouseDeltaX() * (Input()->ShiftIsPressed() ? 0.0001f : 0.002f);
			}
		}

		// Draw axis and aligments when moving
		if(s_Operation == OP_MOVE_PIVOT || s_Operation == OP_MOVE_ALL)
		{
			EAxis Axis = GetDragAxis(s_LastOffset.x, s_LastOffset.y);
			DrawAxis(Axis, s_OriginalPosition, pQuad->m_aPoints[4]);

			str_copy(m_aTooltip, "Hold shift to keep alignment on one axis.");
		}

		if(s_Operation == OP_MOVE_PIVOT)
			DrawPointAlignments(s_PivotAlignments, s_LastOffset.x, s_LastOffset.y);

		if(s_Operation == OP_MOVE_ALL)
		{
			DrawPointAlignments(s_vAABBAlignments, s_LastOffset.x, s_LastOffset.y);

			if(g_Config.m_EdShowQuadsRect)
				DrawAABB(s_SelectionAABB, s_LastOffset.x, s_LastOffset.y);
		}

		if(s_Operation == OP_CONTEXT_MENU)
		{
			if(!Ui()->MouseButton(1))
			{
				if(m_vSelectedLayers.size() == 1)
				{
					m_SelectedQuadIndex = FindSelectedQuadIndex(Index);

					static SPopupMenuId s_PopupQuadId;
					Ui()->DoPopupMenu(&s_PopupQuadId, Ui()->MouseX(), Ui()->MouseY(), 120, 222, this, PopupQuad);
					Ui()->DisableMouseLock();
				}
				s_Operation = OP_NONE;
				Ui()->SetActiveItem(nullptr);
			}
		}
		else if(s_Operation == OP_DELETE)
		{
			if(!Ui()->MouseButton(1))
			{
				if(m_vSelectedLayers.size() == 1)
				{
					Ui()->DisableMouseLock();
					m_Map.OnModify();
					DeleteSelectedQuads();
				}
				s_Operation = OP_NONE;
				Ui()->SetActiveItem(nullptr);
			}
		}
		else if(s_Operation == OP_ROTATE)
		{
			if(Ui()->MouseButton(0))
			{
				Ui()->DisableMouseLock();
				s_Operation = OP_NONE;
				Ui()->SetActiveItem(nullptr);
				m_QuadTracker.EndQuadTrack();
			}
			else if(Ui()->MouseButton(1))
			{
				Ui()->DisableMouseLock();
				s_Operation = OP_NONE;
				Ui()->SetActiveItem(nullptr);

				// Reset points to old position
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
			if(!Ui()->MouseButton(0))
			{
				if(s_Operation == OP_SELECT)
				{
					if(Input()->ShiftIsPressed())
						ToggleSelectQuad(Index);
					else
						SelectQuad(Index);
				}
				else if(s_Operation == OP_MOVE_PIVOT || s_Operation == OP_MOVE_ALL)
				{
					m_QuadTracker.EndQuadTrack();
				}

				Ui()->DisableMouseLock();
				s_Operation = OP_NONE;
				Ui()->SetActiveItem(nullptr);

				s_LastOffset = ivec2();
				s_OriginalPosition = ivec2();
				s_vAABBAlignments.clear();
				s_PivotAlignments.clear();
			}
		}

		Graphics()->SetColor(1, 1, 1, 1);
	}
	else if(Input()->KeyPress(KEY_R) && !m_vSelectedQuads.empty() && m_Dialog == DIALOG_NONE)
	{
		Ui()->EnableMouseLock(pId);
		Ui()->SetActiveItem(pId);
		s_Operation = OP_ROTATE;
		s_RotateAngle = 0;

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
	else if(Ui()->HotItem() == pId)
	{
		ms_pUiGotContext = pId;

		Graphics()->SetColor(1, 1, 1, 1);
		str_copy(m_aTooltip, "Left mouse button to move. Hold shift to move pivot. Hold alt to ignore grid. Hold shift and right click to delete.");

		if(Ui()->MouseButton(0))
		{
			Ui()->SetActiveItem(pId);

			s_MouseXStart = Ui()->MouseX();
			s_MouseYStart = Ui()->MouseY();

			s_Operation = OP_SELECT;
		}
		else if(Ui()->MouseButtonClicked(1))
		{
			if(Input()->ShiftIsPressed())
			{
				s_Operation = OP_DELETE;

				if(!IsQuadSelected(Index))
					SelectQuad(Index);

				Ui()->SetActiveItem(pId);
			}
			else
			{
				s_Operation = OP_CONTEXT_MENU;

				if(!IsQuadSelected(Index))
					SelectQuad(Index);

				Ui()->SetActiveItem(pId);
			}
		}
	}
	else
		Graphics()->SetColor(0, 1, 0, 1);

	IGraphics::CQuadItem QuadItem(CenterX, CenterY, 5.0f * m_MouseWorldScale, 5.0f * m_MouseWorldScale);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CEditor::DoQuadPoint(int LayerIndex, const std::shared_ptr<CLayerQuads> &pLayer, CQuad *pQuad, int QuadIndex, int V)
{
	void *pId = &pQuad->m_aPoints[V];

	float wx = Ui()->MouseWorldX();
	float wy = Ui()->MouseWorldY();

	float px = fx2f(pQuad->m_aPoints[V].x);
	float py = fx2f(pQuad->m_aPoints[V].y);

	// draw selection background
	if(IsQuadPointSelected(QuadIndex, V))
	{
		Graphics()->SetColor(0, 0, 0, 1);
		IGraphics::CQuadItem QuadItem(px, py, 7.0f * m_MouseWorldScale, 7.0f * m_MouseWorldScale);
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
	static CPoint s_OriginalPoint;
	static std::vector<SAlignmentInfo> s_Alignments; // Alignments
	static ivec2 s_LastOffset;

	const bool IgnoreGrid = Input()->AltIsPressed();

	auto &&GetDragOffset = [&]() -> ivec2 {
		float x = wx;
		float y = wy;
		if(MapView()->MapGrid()->IsEnabled() && !IgnoreGrid)
			MapView()->MapGrid()->SnapToGrid(x, y);

		int OffsetX = f2fx(x) - s_OriginalPoint.x;
		int OffsetY = f2fx(y) - s_OriginalPoint.y;

		return {OffsetX, OffsetY};
	};

	if(Ui()->CheckActiveItem(pId))
	{
		if(m_MouseDeltaWorld != vec2(0.0f, 0.0f))
		{
			if(s_Operation == OP_SELECT)
			{
				float x = s_MouseXStart - Ui()->MouseX();
				float y = s_MouseYStart - Ui()->MouseY();

				if(x * x + y * y > 20.0f)
				{
					if(!IsQuadPointSelected(QuadIndex, V))
						SelectQuadPoint(QuadIndex, V);

					if(Input()->ShiftIsPressed())
					{
						s_Operation = OP_MOVEUV;
						Ui()->EnableMouseLock(pId);
					}
					else
					{
						s_Operation = OP_MOVEPOINT;
						// Save original positions before moving
						s_OriginalPoint = pQuad->m_aPoints[V];
						for(int Selected : m_vSelectedQuads)
						{
							for(int m = 0; m < 4; m++)
								if(IsQuadPointSelected(Selected, m))
									PreparePointDrag(pLayer, &pLayer->m_vQuads[Selected], Selected, m);
						}
					}
				}
			}

			if(s_Operation == OP_MOVEPOINT)
			{
				m_QuadTracker.BeginQuadTrack(pLayer, m_vSelectedQuads, -1, LayerIndex);

				s_LastOffset = GetDragOffset(); // Update offset
				ApplyAxisAlignment(s_LastOffset.x, s_LastOffset.y); // Apply axis alignment to offset

				ComputePointsAlignments(pLayer, false, s_LastOffset.x, s_LastOffset.y, s_Alignments);
				ApplyAlignments(s_Alignments, s_LastOffset.x, s_LastOffset.y);

				for(int Selected : m_vSelectedQuads)
				{
					for(int m = 0; m < 4; m++)
					{
						if(IsQuadPointSelected(Selected, m))
						{
							DoPointDrag(pLayer, &pLayer->m_vQuads[Selected], Selected, m, s_LastOffset.x, s_LastOffset.y);
						}
					}
				}
			}
			else if(s_Operation == OP_MOVEUV)
			{
				int SelectedPoints = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);

				m_QuadTracker.BeginQuadPointPropTrack(pLayer, m_vSelectedQuads, SelectedPoints, -1, LayerIndex);
				m_QuadTracker.AddQuadPointPropTrack(EQuadPointProp::PROP_TEX_U);
				m_QuadTracker.AddQuadPointPropTrack(EQuadPointProp::PROP_TEX_V);

				for(int Selected : m_vSelectedQuads)
				{
					CQuad *pSelectedQuad = &pLayer->m_vQuads[Selected];
					for(int m = 0; m < 4; m++)
					{
						if(IsQuadPointSelected(Selected, m))
						{
							// 0,2;1,3 - line x
							// 0,1;2,3 - line y

							pSelectedQuad->m_aTexcoords[m].x += f2fx(m_MouseDeltaWorld.x * 0.001f);
							pSelectedQuad->m_aTexcoords[(m + 2) % 4].x += f2fx(m_MouseDeltaWorld.x * 0.001f);

							pSelectedQuad->m_aTexcoords[m].y += f2fx(m_MouseDeltaWorld.y * 0.001f);
							pSelectedQuad->m_aTexcoords[m ^ 1].y += f2fx(m_MouseDeltaWorld.y * 0.001f);
						}
					}
				}
			}
		}

		// Draw axis and alignments when dragging
		if(s_Operation == OP_MOVEPOINT)
		{
			Graphics()->SetColor(1, 0, 0.1f, 1);

			// Axis
			EAxis Axis = GetDragAxis(s_LastOffset.x, s_LastOffset.y);
			DrawAxis(Axis, s_OriginalPoint, pQuad->m_aPoints[V]);

			// Alignments
			DrawPointAlignments(s_Alignments, s_LastOffset.x, s_LastOffset.y);

			str_copy(m_aTooltip, "Hold shift to keep alignment on one axis.");
		}

		if(s_Operation == OP_CONTEXT_MENU)
		{
			if(!Ui()->MouseButton(1))
			{
				if(m_vSelectedLayers.size() == 1)
				{
					if(!IsQuadSelected(QuadIndex))
						SelectQuad(QuadIndex);

					m_SelectedQuadPoint = V;
					m_SelectedQuadIndex = FindSelectedQuadIndex(QuadIndex);

					static SPopupMenuId s_PopupPointId;
					Ui()->DoPopupMenu(&s_PopupPointId, Ui()->MouseX(), Ui()->MouseY(), 120, 75, this, PopupPoint);
				}
				Ui()->SetActiveItem(nullptr);
			}
		}
		else
		{
			if(!Ui()->MouseButton(0))
			{
				if(s_Operation == OP_SELECT)
				{
					if(Input()->ShiftIsPressed())
						ToggleSelectQuadPoint(QuadIndex, V);
					else
						SelectQuadPoint(QuadIndex, V);
				}

				if(s_Operation == OP_MOVEPOINT)
				{
					m_QuadTracker.EndQuadTrack();
				}
				else if(s_Operation == OP_MOVEUV)
				{
					m_QuadTracker.EndQuadPointPropTrackAll();
				}

				Ui()->DisableMouseLock();
				Ui()->SetActiveItem(nullptr);
			}
		}

		Graphics()->SetColor(1, 1, 1, 1);
	}
	else if(Ui()->HotItem() == pId)
	{
		ms_pUiGotContext = pId;

		Graphics()->SetColor(1, 1, 1, 1);
		str_copy(m_aTooltip, "Left mouse button to move. Hold shift to move the texture. Hold alt to ignore grid.");

		if(Ui()->MouseButton(0))
		{
			Ui()->SetActiveItem(pId);

			s_MouseXStart = Ui()->MouseX();
			s_MouseYStart = Ui()->MouseY();

			s_Operation = OP_SELECT;
		}
		else if(Ui()->MouseButton(1))
		{
			s_Operation = OP_CONTEXT_MENU;

			Ui()->SetActiveItem(pId);

			if(!IsQuadPointSelected(QuadIndex, V))
				SelectQuadPoint(QuadIndex, V);
		}
	}
	else
		Graphics()->SetColor(1, 0, 0, 1);

	IGraphics::CQuadItem QuadItem(px, py, 5.0f * m_MouseWorldScale, 5.0f * m_MouseWorldScale);
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
	float SnapRadius = 4.f * m_MouseWorldScale;

	vec2 Mouse = vec2(Ui()->MouseWorldX(), Ui()->MouseWorldY());
	vec2 Point = Mouse;

	vec2 v[4] = {
		vec2(fx2f(pQuad->m_aPoints[0].x), fx2f(pQuad->m_aPoints[0].y)),
		vec2(fx2f(pQuad->m_aPoints[1].x), fx2f(pQuad->m_aPoints[1].y)),
		vec2(fx2f(pQuad->m_aPoints[3].x), fx2f(pQuad->m_aPoints[3].y)),
		vec2(fx2f(pQuad->m_aPoints[2].x), fx2f(pQuad->m_aPoints[2].y))};

	str_copy(m_aTooltip, "Left click inside the quad to select an area to slice. Hold alt to ignore grid. Right click to leave knife mode");

	if(Ui()->MouseButtonClicked(1))
	{
		m_QuadKnifeActive = false;
		return;
	}

	// Handle snapping
	if(MapView()->MapGrid()->IsEnabled() && !IgnoreGrid)
	{
		float CellSize = MapView()->MapGrid()->GridLineDistance();
		vec2 OnGrid(Mouse.x, Mouse.y);
		MapView()->MapGrid()->SnapToGrid(OnGrid.x, OnGrid.y);

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

	if(Ui()->MouseButtonClicked(0) && ValidPosition)
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
		m_EditorHistory.RecordAction(std::make_shared<CEditorActionNewQuad>(this, m_SelectedGroup, m_vSelectedLayers[0]));
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
		aMarkers[i] = IGraphics::CQuadItem(m_aQuadKnifePoints[i].x, m_aQuadKnifePoints[i].y, 5.f * m_MouseWorldScale, 5.f * m_MouseWorldScale);

	Graphics()->SetColor(0.f, 0.f, 1.f, 1.f);
	Graphics()->QuadsDraw(aMarkers, m_QuadKnifeCount);

	if(ValidPosition)
	{
		IGraphics::CQuadItem MarkerCurrent(Point.x, Point.y, 5.f * m_MouseWorldScale, 5.f * m_MouseWorldScale);
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
			ColorRGBA Result = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
			apEnvelope[j]->Eval(apEnvelope[j]->m_vPoints[i].m_Time / 1000.0f + 0.000001f, Result, 2);
			vec2 Pos0 = vec2(fx2f(pPivotPoint->x) + Result.r, fx2f(pPivotPoint->y) + Result.g);

			const int Steps = 15;
			for(int n = 1; n <= Steps; n++)
			{
				const float Time = mix(apEnvelope[j]->m_vPoints[i].m_Time, apEnvelope[j]->m_vPoints[i + 1].m_Time, (float)n / Steps);
				Result = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
				apEnvelope[j]->Eval(Time / 1000.0f - 0.000001f, Result, 2);

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
			CPoint aRotated[4];
			if(Rot != 0)
			{
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
	float wx = Ui()->MouseWorldX();
	float wy = Ui()->MouseWorldY();
	std::shared_ptr<CEnvelope> pEnvelope = m_Map.m_vpEnvelopes[pQuad->m_PosEnv];
	void *pId = &pEnvelope->m_vPoints[PIndex];

	// get pivot
	float CenterX = fx2f(pQuad->m_aPoints[4].x) + fx2f(pEnvelope->m_vPoints[PIndex].m_aValues[0]);
	float CenterY = fx2f(pQuad->m_aPoints[4].y) + fx2f(pEnvelope->m_vPoints[PIndex].m_aValues[1]);

	const bool IgnoreGrid = Input()->AltIsPressed();

	if(Ui()->CheckActiveItem(pId) && m_CurrentQuadIndex == QIndex)
	{
		if(s_Operation == OP_MOVE)
		{
			if(MapView()->MapGrid()->IsEnabled() && !IgnoreGrid)
			{
				float x = wx;
				float y = wy;
				MapView()->MapGrid()->SnapToGrid(x, y);
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
			pEnvelope->m_vPoints[PIndex].m_aValues[2] += 10 * Ui()->MouseDeltaX();

		s_LastWx = wx;
		s_LastWy = wy;

		if(!Ui()->MouseButton(0))
		{
			Ui()->DisableMouseLock();
			s_Operation = OP_NONE;
			Ui()->SetActiveItem(nullptr);
		}

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	else if(Ui()->HotItem() == pId && m_CurrentQuadIndex == QIndex)
	{
		ms_pUiGotContext = pId;

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		str_copy(m_aTooltip, "Left mouse button to move. Hold ctrl to rotate. Hold alt to ignore grid.");

		if(Ui()->MouseButton(0))
		{
			if(Input()->ModifierIsPressed())
			{
				Ui()->EnableMouseLock(pId);
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

			Ui()->SetActiveItem(pId);

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

	IGraphics::CQuadItem QuadItem(CenterX, CenterY, 5.0f * m_MouseWorldScale, 5.0f * m_MouseWorldScale);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CEditor::DoMapEditor(CUIRect View)
{
	// render all good stuff
	if(!m_ShowPicker)
	{
		MapView()->RenderMap();
	}
	else
	{
		// fix aspect ratio of the image in the picker
		float Max = minimum(View.w, View.h);
		View.w = View.h = Max;
	}

	const bool Inside = Ui()->MouseInside(&View);

	// fetch mouse position
	float wx = Ui()->MouseWorldX();
	float wy = Ui()->MouseWorldY();
	float mx = Ui()->MouseX();
	float my = Ui()->MouseY();

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
		CUIRect Screen = *Ui()->Screen();
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
	std::pair<int, std::shared_ptr<CLayer>> apEditLayers[128];
	size_t NumEditLayers = 0;

	if(m_ShowPicker && GetSelectedLayer(0) && GetSelectedLayer(0)->m_Type == LAYERTYPE_TILES)
	{
		apEditLayers[0] = {0, m_pTilesetPicker};
		NumEditLayers++;
	}
	else if(m_ShowPicker)
	{
		apEditLayers[0] = {0, m_pQuadsetPicker};
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
			apEditLayers[NumEditLayers] = {m_vSelectedLayers[i], GetSelectedLayerType(i, EditingType)};
			if(apEditLayers[NumEditLayers].second)
			{
				NumEditLayers++;
			}
		}

		MapView()->RenderGroupBorder();
		MapView()->MapGrid()->OnRender(View);
	}

	const bool ShouldPan = Ui()->HotItem() == &m_MapEditorId && ((Input()->ModifierIsPressed() && Ui()->MouseButton(0)) || Ui()->MouseButton(2));
	if(m_pContainerPanned == &m_MapEditorId)
	{
		// do panning
		if(ShouldPan)
		{
			if(Input()->ShiftIsPressed())
				s_Operation = OP_PAN_EDITOR;
			else
				s_Operation = OP_PAN_WORLD;
			Ui()->SetActiveItem(&m_MapEditorId);
		}
		else
			s_Operation = OP_NONE;

		if(s_Operation == OP_PAN_WORLD)
			MapView()->OffsetWorld(-Ui()->MouseDelta() * m_MouseWorldScale);
		else if(s_Operation == OP_PAN_EDITOR)
			MapView()->OffsetEditor(-Ui()->MouseDelta() * m_MouseWorldScale);

		if(s_Operation == OP_NONE)
			m_pContainerPanned = nullptr;
	}

	if(Inside)
	{
		Ui()->SetHotItem(&m_MapEditorId);

		// do global operations like pan and zoom
		if(Ui()->CheckActiveItem(nullptr) && (Ui()->MouseButton(0) || Ui()->MouseButton(2)))
		{
			s_StartWx = wx;
			s_StartWy = wy;

			if(ShouldPan && m_pContainerPanned == nullptr)
				m_pContainerPanned = &m_MapEditorId;
		}

		// brush editing
		if(Ui()->HotItem() == &m_MapEditorId)
		{
			if(m_ShowPicker)
			{
				std::shared_ptr<CLayer> pLayer = GetSelectedLayer(0);
				int Layer;
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
				else
					Layer = NUM_LAYERS;

				EExplanation Explanation;
				if(m_SelectEntitiesImage == "DDNet")
					Explanation = EExplanation::DDNET;
				else if(m_SelectEntitiesImage == "FNG")
					Explanation = EExplanation::FNG;
				else if(m_SelectEntitiesImage == "Race")
					Explanation = EExplanation::RACE;
				else if(m_SelectEntitiesImage == "Vanilla")
					Explanation = EExplanation::VANILLA;
				else if(m_SelectEntitiesImage == "blockworlds")
					Explanation = EExplanation::BLOCKWORLDS;
				else
					Explanation = EExplanation::NONE;

				if(Layer != NUM_LAYERS)
				{
					const char *pExplanation = Explain(Explanation, (int)wx / 32 + (int)wy / 32 * 16, Layer);
					if(pExplanation)
						str_copy(m_aTooltip, pExplanation);
				}
			}
			else if(m_pBrush->IsEmpty() && GetSelectedLayerType(0, LAYERTYPE_QUADS) != nullptr)
				str_copy(m_aTooltip, "Use left mouse button to drag and create a brush. Hold shift to select multiple quads. Press R to rotate selected quads. Use ctrl+right mouse to select layer.");
			else if(m_pBrush->IsEmpty())
			{
				if(g_Config.m_EdLayerSelector)
					str_copy(m_aTooltip, "Use left mouse button to drag and create a brush. Use ctrl+right click to select layer for hovered tile.");
				else
					str_copy(m_aTooltip, "Use left mouse button to drag and create a brush.");
			}
			else
				str_copy(m_aTooltip, "Use left mouse button to paint with the brush. Right button clears the brush.");

			if(Ui()->CheckActiveItem(&m_MapEditorId))
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
							if(apEditLayers[k].second->m_Type == m_pBrush->m_vpLayers[BrushIndex]->m_Type)
							{
								if(apEditLayers[k].second->m_Type == LAYERTYPE_TILES)
								{
									std::shared_ptr<CLayerTiles> pLayer = std::static_pointer_cast<CLayerTiles>(apEditLayers[k].second);
									std::shared_ptr<CLayerTiles> pBrushLayer = std::static_pointer_cast<CLayerTiles>(m_pBrush->m_vpLayers[BrushIndex]);

									if(pLayer->m_Tele <= pBrushLayer->m_Tele && pLayer->m_Speedup <= pBrushLayer->m_Speedup && pLayer->m_Front <= pBrushLayer->m_Front && pLayer->m_Game <= pBrushLayer->m_Game && pLayer->m_Switch <= pBrushLayer->m_Switch && pLayer->m_Tune <= pBrushLayer->m_Tune)
										pLayer->BrushDraw(pBrushLayer, vec2(wx, wy));
								}
								else
								{
									apEditLayers[k].second->BrushDraw(m_pBrush->m_vpLayers[BrushIndex], vec2(wx, wy));
								}
							}
						}
					}
				}
				else if(s_Operation == OP_BRUSH_GRAB)
				{
					if(!Ui()->MouseButton(0))
					{
						std::shared_ptr<CLayerQuads> pQuadLayer = std::static_pointer_cast<CLayerQuads>(GetSelectedLayerType(0, LAYERTYPE_QUADS));
						if(Input()->ShiftIsPressed() && pQuadLayer)
						{
							DeselectQuads();
							for(size_t i = 0; i < pQuadLayer->m_vQuads.size(); i++)
							{
								const CQuad &Quad = pQuadLayer->m_vQuads[i];
								vec2 Position = vec2(fx2f(Quad.m_aPoints[4].x), fx2f(Quad.m_aPoints[4].y));
								if(r.Inside(Position) && !IsQuadSelected(i))
									ToggleSelectQuad(i);
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
								Grabs += apEditLayers[k].second->BrushGrab(m_pBrush, r);
							if(Grabs == 0)
								m_pBrush->Clear();

							DeselectQuads();
							DeselectQuadPoints();
						}
					}
					else
					{
						for(size_t k = 0; k < NumEditLayers; k++)
							apEditLayers[k].second->BrushSelecting(r);
						Ui()->MapScreen();
					}
				}
				else if(s_Operation == OP_BRUSH_PAINT)
				{
					if(!Ui()->MouseButton(0))
					{
						for(size_t k = 0; k < NumEditLayers; k++)
						{
							size_t BrushIndex = k;
							if(m_pBrush->m_vpLayers.size() != NumEditLayers)
								BrushIndex = 0;
							std::shared_ptr<CLayer> pBrush = m_pBrush->IsEmpty() ? nullptr : m_pBrush->m_vpLayers[BrushIndex];
							apEditLayers[k].second->FillSelection(m_pBrush->IsEmpty(), pBrush, r);
						}
						std::shared_ptr<IEditorAction> Action = std::make_shared<CEditorBrushDrawAction>(this, m_SelectedGroup);
						m_EditorHistory.RecordAction(Action);
					}
					else
					{
						for(size_t k = 0; k < NumEditLayers; k++)
							apEditLayers[k].second->BrushSelecting(r);
						Ui()->MapScreen();
					}
				}
			}
			else
			{
				if(Ui()->MouseButton(1))
				{
					m_pBrush->Clear();
				}

				if(!Input()->ModifierIsPressed() && Ui()->MouseButton(0) && s_Operation == OP_NONE && !m_QuadKnifeActive)
				{
					Ui()->SetActiveItem(&m_MapEditorId);

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

							if(apEditLayers[k].second->m_Type == m_pBrush->m_vpLayers[BrushIndex]->m_Type)
								apEditLayers[k].second->BrushPlace(m_pBrush->m_vpLayers[BrushIndex], vec2(wx, wy));
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
					auto &[LayerIndex, pEditLayer] = apEditLayers[k];

					if(pEditLayer->m_Type == LAYERTYPE_QUADS)
					{
						std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(pEditLayer);

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
									DoQuadPoint(LayerIndex, pLayer, &pLayer->m_vQuads[i], i, v);

								DoQuad(LayerIndex, pLayer, &pLayer->m_vQuads[i], i);
							}
							Graphics()->QuadsEnd();
						}
					}

					if(pEditLayer->m_Type == LAYERTYPE_SOUNDS)
					{
						std::shared_ptr<CLayerSounds> pLayer = std::static_pointer_cast<CLayerSounds>(pEditLayer);

						Graphics()->TextureClear();
						Graphics()->QuadsBegin();
						for(size_t i = 0; i < pLayer->m_vSources.size(); i++)
						{
							DoSoundSource(LayerIndex, &pLayer->m_vSources[i], i);
						}
						Graphics()->QuadsEnd();
					}
				}

				Ui()->MapScreen();
			}
		}

		// menu proof selection
		if(MapView()->ProofMode()->IsModeMenu() && !m_ShowPicker)
		{
			MapView()->ProofMode()->ResetMenuBackgroundPositions();
			for(int i = 0; i < (int)MapView()->ProofMode()->m_vMenuBackgroundPositions.size(); i++)
			{
				vec2 Pos = MapView()->ProofMode()->m_vMenuBackgroundPositions[i];
				Pos += MapView()->GetWorldOffset() - MapView()->ProofMode()->m_vMenuBackgroundPositions[MapView()->ProofMode()->m_CurrentMenuProofIndex];
				Pos.y -= 3.0f;

				if(distance(Pos, m_MouseWorldNoParaPos) <= 20.0f)
				{
					Ui()->SetHotItem(&MapView()->ProofMode()->m_vMenuBackgroundPositions[i]);

					if(i != MapView()->ProofMode()->m_CurrentMenuProofIndex && Ui()->CheckActiveItem(&MapView()->ProofMode()->m_vMenuBackgroundPositions[i]))
					{
						if(!Ui()->MouseButton(0))
						{
							MapView()->ProofMode()->m_CurrentMenuProofIndex = i;
							MapView()->SetWorldOffset(MapView()->ProofMode()->m_vMenuBackgroundPositions[i]);
							Ui()->SetActiveItem(nullptr);
						}
					}
					else if(Ui()->HotItem() == &MapView()->ProofMode()->m_vMenuBackgroundPositions[i])
					{
						char aTooltipPrefix[32] = "Switch proof position to";
						if(i == MapView()->ProofMode()->m_CurrentMenuProofIndex)
							str_copy(aTooltipPrefix, "Current proof position at");

						char aNumBuf[8];
						if(i < (TILE_TIME_CHECKPOINT_LAST - TILE_TIME_CHECKPOINT_FIRST))
							str_format(aNumBuf, sizeof(aNumBuf), "#%d", i + 1);
						else
							aNumBuf[0] = '\0';

						char aTooltipPositions[128];
						str_format(aTooltipPositions, sizeof(aTooltipPositions), "%s %s", MapView()->ProofMode()->m_vpMenuBackgroundPositionNames[i], aNumBuf);

						for(int k : MapView()->ProofMode()->m_vMenuBackgroundCollisions.at(i))
						{
							if(k == MapView()->ProofMode()->m_CurrentMenuProofIndex)
								str_copy(aTooltipPrefix, "Current proof position at");

							Pos = MapView()->ProofMode()->m_vMenuBackgroundPositions[k];
							Pos += MapView()->GetWorldOffset() - MapView()->ProofMode()->m_vMenuBackgroundPositions[MapView()->ProofMode()->m_CurrentMenuProofIndex];
							Pos.y -= 3.0f;

							if(distance(Pos, m_MouseWorldNoParaPos) > 20.0f)
								continue;

							if(i < (TILE_TIME_CHECKPOINT_LAST - TILE_TIME_CHECKPOINT_FIRST))
								str_format(aNumBuf, sizeof(aNumBuf), "#%d", k + 1);
							else
								aNumBuf[0] = '\0';

							char aTooltipPositionsCopy[128];
							str_copy(aTooltipPositionsCopy, aTooltipPositions);
							str_format(aTooltipPositions, sizeof(aTooltipPositions), "%s, %s %s", aTooltipPositionsCopy, MapView()->ProofMode()->m_vpMenuBackgroundPositionNames[k], aNumBuf);
						}
						str_format(m_aMenuBackgroundTooltip, sizeof(m_aMenuBackgroundTooltip), "%s %s", aTooltipPrefix, aTooltipPositions);

						str_copy(m_aTooltip, m_aMenuBackgroundTooltip);
						if(Ui()->MouseButton(0))
							Ui()->SetActiveItem(&MapView()->ProofMode()->m_vMenuBackgroundPositions[i]);
					}
					break;
				}
			}
		}

		if(Ui()->CheckActiveItem(&m_MapEditorId) && m_pContainerPanned == nullptr)
		{
			// release mouse
			if(!Ui()->MouseButton(0))
			{
				if(s_Operation == OP_BRUSH_DRAW)
				{
					std::shared_ptr<IEditorAction> Action = std::make_shared<CEditorBrushDrawAction>(this, m_SelectedGroup);

					if(!Action->IsEmpty()) // Avoid recording tile draw action when placing quads only
						m_EditorHistory.RecordAction(Action);
				}

				s_Operation = OP_NONE;
				Ui()->SetActiveItem(nullptr);
			}
		}
		if(!Input()->ModifierIsPressed() && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr)
		{
			float PanSpeed = Input()->ShiftIsPressed() ? 200.0f : 64.0f;
			if(Input()->KeyPress(KEY_A))
				MapView()->OffsetWorld({-PanSpeed * m_MouseWorldScale, 0});
			else if(Input()->KeyPress(KEY_D))
				MapView()->OffsetWorld({PanSpeed * m_MouseWorldScale, 0});
			if(Input()->KeyPress(KEY_W))
				MapView()->OffsetWorld({0, -PanSpeed * m_MouseWorldScale});
			else if(Input()->KeyPress(KEY_S))
				MapView()->OffsetWorld({0, PanSpeed * m_MouseWorldScale});
		}
	}
	else if(Ui()->CheckActiveItem(&m_MapEditorId) && m_pContainerPanned == nullptr)
	{
		// release mouse
		if(!Ui()->MouseButton(0))
		{
			s_Operation = OP_NONE;
			Ui()->SetActiveItem(nullptr);
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

	MapView()->ProofMode()->RenderScreenSizes();

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

	Ui()->MapScreen();
}

void CEditor::SetHotQuadPoint(const std::shared_ptr<CLayerQuads> &pLayer)
{
	float wx = Ui()->MouseWorldX();
	float wy = Ui()->MouseWorldY();

	float MinDist = 500.0f;
	void *pMinPoint = nullptr;

	auto UpdateMinimum = [&](float px, float py, void *pId) {
		float dx = (px - wx) / m_MouseWorldScale;
		float dy = (py - wy) / m_MouseWorldScale;

		float CurrDist = dx * dx + dy * dy;
		if(CurrDist < MinDist)
		{
			MinDist = CurrDist;
			pMinPoint = pId;
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
		Ui()->SetHotItem(pMinPoint);
}

void CEditor::DoColorPickerButton(const void *pId, const CUIRect *pRect, ColorRGBA Color, const std::function<void(ColorRGBA Color)> &SetColor)
{
	CUIRect ColorRect;
	pRect->Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f * Ui()->ButtonColorMul(pId)), IGraphics::CORNER_ALL, 3.0f);
	pRect->Margin(1.0f, &ColorRect);
	ColorRect.Draw(Color, IGraphics::CORNER_ALL, 3.0f);

	const int ButtonResult = DoButton_Editor_Common(pId, nullptr, 0, pRect, 0, "Click to show the color picker. Shift+rightclick to copy color to clipboard. Shift+leftclick to paste color from clipboard.");
	if(Input()->ShiftIsPressed())
	{
		if(ButtonResult == 1)
		{
			std::string Clipboard = Input()->GetClipboardText();
			if(Clipboard[0] == '#' || Clipboard[0] == '$') // ignore leading # (web color format) and $ (console color format)
				Clipboard = Clipboard.substr(1);
			if(str_isallnum_hex(Clipboard.c_str()))
			{
				std::optional<ColorRGBA> ParsedColor = color_parse<ColorRGBA>(Clipboard.c_str());
				if(ParsedColor)
				{
					m_ColorPickerPopupContext.m_State = EEditState::ONE_GO;
					SetColor(ParsedColor.value());
				}
			}
		}
		else if(ButtonResult == 2)
		{
			char aClipboard[9];
			str_format(aClipboard, sizeof(aClipboard), "%08X", Color.PackAlphaLast());
			Input()->SetClipboardText(aClipboard);
		}
	}
	else if(ButtonResult > 0)
	{
		if(m_ColorPickerPopupContext.m_ColorMode == CUi::SColorPickerPopupContext::MODE_UNSET)
			m_ColorPickerPopupContext.m_ColorMode = CUi::SColorPickerPopupContext::MODE_RGBA;
		m_ColorPickerPopupContext.m_RgbaColor = Color;
		m_ColorPickerPopupContext.m_HslaColor = color_cast<ColorHSLA>(Color);
		m_ColorPickerPopupContext.m_HsvaColor = color_cast<ColorHSVA>(m_ColorPickerPopupContext.m_HslaColor);
		m_ColorPickerPopupContext.m_Alpha = true;
		m_pColorPickerPopupActiveId = pId;
		Ui()->ShowPopupColorPicker(Ui()->MouseX(), Ui()->MouseY(), &m_ColorPickerPopupContext);
	}

	if(Ui()->IsPopupOpen(&m_ColorPickerPopupContext))
	{
		if(m_pColorPickerPopupActiveId == pId)
			SetColor(m_ColorPickerPopupContext.m_RgbaColor);
	}
	else
	{
		m_pColorPickerPopupActiveId = nullptr;
		if(m_ColorPickerPopupContext.m_State == EEditState::EDITING)
		{
			ColorRGBA c = color_cast<ColorRGBA>(m_ColorPickerPopupContext.m_HsvaColor);
			m_ColorPickerPopupContext.m_State = EEditState::END;
			SetColor(c);
			m_ColorPickerPopupContext.m_State = EEditState::NONE;
		}
	}
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
	static int s_PreviousOperation = OP_NONE;
	static const void *s_pDraggedButton = nullptr;
	static float s_InitialMouseY = 0;
	static float s_InitialCutHeight = 0;
	constexpr float MinDragDistance = 5.0f;
	int GroupAfterDraggedLayer = -1;
	int LayerAfterDraggedLayer = -1;
	bool DraggedPositionFound = false;
	bool MoveLayers = false;
	bool MoveGroup = false;
	bool StartDragLayer = false;
	bool StartDragGroup = false;
	std::vector<int> vButtonsPerGroup;

	auto SetOperation = [](int Operation) {
		if(Operation != s_Operation)
		{
			s_PreviousOperation = s_Operation;
			s_Operation = Operation;
			if(Operation == OP_NONE)
			{
				s_pDraggedButton = nullptr;
			}
		}
	};

	vButtonsPerGroup.reserve(m_Map.m_vpGroups.size());
	for(const std::shared_ptr<CLayerGroup> &pGroup : m_Map.m_vpGroups)
	{
		vButtonsPerGroup.push_back(pGroup->m_vpLayers.size() + 1);
	}

	if(s_pDraggedButton != nullptr && Ui()->ActiveItem() != s_pDraggedButton)
	{
		SetOperation(OP_NONE);
	}

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
		UnscrolledLayersBox.y -= s_InitialMouseY - Ui()->MouseY();

		UnscrolledLayersBox.y = clamp(UnscrolledLayersBox.y, MinDraggableValue, MaxDraggableValue);

		UnscrolledLayersBox.w = LayersBox.w;
	}

	static bool s_ScrollToSelectionNext = false;
	const bool ScrollToSelection = LayerSelector()->SelectByTile() || s_ScrollToSelectionNext;
	s_ScrollToSelectionNext = false;

	// render layers
	for(int g = 0; g < (int)m_Map.m_vpGroups.size(); g++)
	{
		if(s_Operation == OP_LAYER_DRAG && g > 0 && !DraggedPositionFound && Ui()->MouseY() < LayersBox.y + RowHeight / 2)
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
			else if(!DraggedPositionFound && Ui()->MouseY() < LayersBox.y + RowHeight * vButtonsPerGroup[g] / 2 + 3.0f)
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

			const int MouseClick = DoButton_FontIcon(&m_Map.m_vpGroups[g]->m_Visible, m_Map.m_vpGroups[g]->m_Visible ? FONT_ICON_EYE : FONT_ICON_EYE_SLASH, m_Map.m_vpGroups[g]->m_Collapse ? 1 : 0, &VisibleToggle, 0, "Left click to toggle visibility. Right click to show this group only.", IGraphics::CORNER_L, 8.0f);
			if(MouseClick == 1)
			{
				m_Map.m_vpGroups[g]->m_Visible = !m_Map.m_vpGroups[g]->m_Visible;
			}
			else if(MouseClick == 2)
			{
				if(Input()->ShiftIsPressed())
				{
					if(g != m_SelectedGroup)
						SelectLayer(0, g);
				}

				int NumActive = 0;
				for(auto &Group : m_Map.m_vpGroups)
				{
					if(Group == m_Map.m_vpGroups[g])
					{
						Group->m_Visible = true;
						continue;
					}

					if(Group->m_Visible)
					{
						Group->m_Visible = false;
						NumActive++;
					}
				}
				if(NumActive == 0)
				{
					for(auto &Group : m_Map.m_vpGroups)
					{
						Group->m_Visible = true;
					}
				}
			}

			str_format(aBuf, sizeof(aBuf), "#%d %s", g, m_Map.m_vpGroups[g]->m_aName);

			bool Clicked;
			bool Abrupted;
			if(int Result = DoButton_DraggableEx(m_Map.m_vpGroups[g].get(), aBuf, g == m_SelectedGroup, &Slot, &Clicked, &Abrupted,
				   BUTTON_CONTEXT, m_Map.m_vpGroups[g]->m_Collapse ? "Select group. Shift click to select all layers. Double click to expand." : "Select group. Shift click to select all layers. Double click to collapse.", IGraphics::CORNER_R))
			{
				if(s_Operation == OP_NONE)
				{
					s_InitialMouseY = Ui()->MouseY();
					s_InitialCutHeight = s_InitialMouseY - UnscrolledLayersBox.y;
					SetOperation(OP_CLICK);

					if(g != m_SelectedGroup)
						SelectLayer(0, g);
				}

				if(Abrupted)
				{
					SetOperation(OP_NONE);
				}

				if(s_Operation == OP_CLICK && absolute(Ui()->MouseY() - s_InitialMouseY) > MinDragDistance)
				{
					StartDragGroup = true;
					s_pDraggedButton = m_Map.m_vpGroups[g].get();
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
						Ui()->DoPopupMenu(&s_PopupGroupId, Ui()->MouseX(), Ui()->MouseY(), 145, 256, this, PopupGroup);
					}

					if(!m_Map.m_vpGroups[g]->m_vpLayers.empty() && Ui()->DoDoubleClickLogic(m_Map.m_vpGroups[g].get()))
						m_Map.m_vpGroups[g]->m_Collapse ^= 1;

					SetOperation(OP_NONE);
				}

				if(s_Operation == OP_GROUP_DRAG && Clicked)
					MoveGroup = true;
			}
			else if(s_pDraggedButton == m_Map.m_vpGroups[g].get())
			{
				SetOperation(OP_NONE);
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
					if(!DraggedPositionFound && Ui()->MouseY() < LayersBox.y + RowHeight / 2)
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

			const int MouseClick = DoButton_FontIcon(&m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Visible, m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Visible ? FONT_ICON_EYE : FONT_ICON_EYE_SLASH, 0, &VisibleToggle, 0, "Left click to toggle visibility. Right click to show only this layer within its group.", IGraphics::CORNER_L, 8.0f);
			if(MouseClick == 1)
			{
				m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Visible = !m_Map.m_vpGroups[g]->m_vpLayers[i]->m_Visible;
			}
			else if(MouseClick == 2)
			{
				if(Input()->ShiftIsPressed())
				{
					if(!IsLayerSelected)
						SelectLayer(i, g);
				}

				int NumActive = 0;
				for(auto &Layer : m_Map.m_vpGroups[g]->m_vpLayers)
				{
					if(Layer == m_Map.m_vpGroups[g]->m_vpLayers[i])
					{
						Layer->m_Visible = true;
						continue;
					}

					if(Layer->m_Visible)
					{
						Layer->m_Visible = false;
						NumActive++;
					}
				}
				if(NumActive == 0)
				{
					for(auto &Layer : m_Map.m_vpGroups[g]->m_vpLayers)
					{
						Layer->m_Visible = true;
					}
				}
			}

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
				if(s_Operation == OP_NONE)
				{
					s_InitialMouseY = Ui()->MouseY();
					s_InitialCutHeight = s_InitialMouseY - UnscrolledLayersBox.y;

					SetOperation(OP_CLICK);

					if(!Input()->ShiftIsPressed() && !IsLayerSelected)
					{
						SelectLayer(i, g);
					}
				}

				if(Abrupted)
				{
					SetOperation(OP_NONE);
				}

				if(s_Operation == OP_CLICK && absolute(Ui()->MouseY() - s_InitialMouseY) > MinDragDistance)
				{
					bool EntitiesLayerSelected = false;
					for(int k : m_vSelectedLayers)
					{
						if(m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers[k]->IsEntitiesLayer())
							EntitiesLayerSelected = true;
					}

					if(!EntitiesLayerSelected)
						StartDragLayer = true;

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
						s_LayerPopupContext.m_vpLayers.clear();
						s_LayerPopupContext.m_vLayerIndices.clear();

						if(!IsLayerSelected)
						{
							SelectLayer(i, g);
						}

						if(m_vSelectedLayers.size() > 1)
						{
							bool AllTile = true;
							for(size_t j = 0; AllTile && j < m_vSelectedLayers.size(); j++)
							{
								int LayerIndex = m_vSelectedLayers[j];
								if(m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers[LayerIndex]->m_Type == LAYERTYPE_TILES)
								{
									s_LayerPopupContext.m_vpLayers.push_back(std::static_pointer_cast<CLayerTiles>(m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers[m_vSelectedLayers[j]]));
									s_LayerPopupContext.m_vLayerIndices.push_back(LayerIndex);
								}
								else
									AllTile = false;
							}

							// Don't allow editing if all selected layers are not tile layers
							if(!AllTile)
							{
								s_LayerPopupContext.m_vpLayers.clear();
								s_LayerPopupContext.m_vLayerIndices.clear();
							}
						}

						Ui()->DoPopupMenu(&s_LayerPopupContext, Ui()->MouseX(), Ui()->MouseY(), 120, 270, &s_LayerPopupContext, PopupLayer);
					}

					SetOperation(OP_NONE);
				}

				if(s_Operation == OP_LAYER_DRAG && Clicked)
				{
					MoveLayers = true;
				}
			}
			else if(s_pDraggedButton == m_Map.m_vpGroups[g]->m_vpLayers[i].get())
			{
				SetOperation(OP_NONE);
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

		auto Pos = std::find(m_Map.m_vpGroups.begin(), m_Map.m_vpGroups.end(), pSelectedGroup);
		m_SelectedGroup = Pos - m_Map.m_vpGroups.begin();

		m_Map.OnModify();
	}

	static int s_InitialGroupIndex;
	static std::vector<int> s_vInitialLayerIndices;

	if(MoveLayers || MoveGroup)
	{
		SetOperation(OP_NONE);
	}
	if(StartDragLayer)
	{
		SetOperation(OP_LAYER_DRAG);
		s_InitialGroupIndex = m_SelectedGroup;
		s_vInitialLayerIndices = std::vector(m_vSelectedLayers);
	}
	if(StartDragGroup)
	{
		s_InitialGroupIndex = m_SelectedGroup;
		SetOperation(OP_GROUP_DRAG);
	}

	if(s_Operation == OP_LAYER_DRAG || s_Operation == OP_GROUP_DRAG)
	{
		if(s_pDraggedButton == nullptr)
		{
			SetOperation(OP_NONE);
		}
		else
		{
			s_ScrollRegion.DoEdgeScrolling();
			Ui()->SetActiveItem(s_pDraggedButton);
		}
	}

	if(Input()->KeyPress(KEY_DOWN) && m_Dialog == DIALOG_NONE && !Ui()->IsPopupOpen() && CLineInput::GetActiveInput() == nullptr && s_Operation == OP_NONE)
	{
		if(Input()->ShiftIsPressed())
		{
			if(m_vSelectedLayers[m_vSelectedLayers.size() - 1] < (int)m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers.size() - 1)
				AddSelectedLayer(m_vSelectedLayers[m_vSelectedLayers.size() - 1] + 1);
		}
		else
		{
			SelectNextLayer();
		}
		s_ScrollToSelectionNext = true;
	}
	if(Input()->KeyPress(KEY_UP) && m_Dialog == DIALOG_NONE && !Ui()->IsPopupOpen() && CLineInput::GetActiveInput() == nullptr && s_Operation == OP_NONE)
	{
		if(Input()->ShiftIsPressed())
		{
			if(m_vSelectedLayers[m_vSelectedLayers.size() - 1] > 0)
				AddSelectedLayer(m_vSelectedLayers[m_vSelectedLayers.size() - 1] - 1);
		}
		else
		{
			SelectPreviousLayer();
		}

		s_ScrollToSelectionNext = true;
	}

	CUIRect AddGroupButton, CollapseAllButton;
	LayersBox.HSplitTop(RowHeight + 1.0f, &AddGroupButton, &LayersBox);
	if(s_ScrollRegion.AddRect(AddGroupButton))
	{
		AddGroupButton.HSplitTop(RowHeight, &AddGroupButton, 0);
		if(DoButton_Editor(&m_QuickActionAddGroup, m_QuickActionAddGroup.Label(), 0, &AddGroupButton, IGraphics::CORNER_R, m_QuickActionAddGroup.Description()))
		{
			m_QuickActionAddGroup.Call();
		}
	}

	LayersBox.HSplitTop(5.0f, nullptr, &LayersBox);
	LayersBox.HSplitTop(RowHeight + 1.0f, &CollapseAllButton, &LayersBox);
	if(s_ScrollRegion.AddRect(CollapseAllButton))
	{
		unsigned long TotalCollapsed = 0;
		for(const auto &pGroup : m_Map.m_vpGroups)
		{
			if(pGroup->m_Collapse)
			{
				TotalCollapsed++;
			}
		}

		const char *pActionText = TotalCollapsed == m_Map.m_vpGroups.size() ? "Expand all" : "Collapse all";

		CollapseAllButton.HSplitTop(RowHeight, &CollapseAllButton, 0);
		static int s_CollapseAllButton = 0;
		if(DoButton_Editor(&s_CollapseAllButton, pActionText, 0, &CollapseAllButton, IGraphics::CORNER_R, "Expand or collapse all groups"))
		{
			for(const auto &pGroup : m_Map.m_vpGroups)
			{
				if(TotalCollapsed == m_Map.m_vpGroups.size())
					pGroup->m_Collapse = false;
				else
					pGroup->m_Collapse = true;
			}
		}
	}

	s_ScrollRegion.End();

	if(s_Operation == OP_NONE)
	{
		if(s_PreviousOperation == OP_GROUP_DRAG)
		{
			s_PreviousOperation = OP_NONE;
			m_EditorHistory.RecordAction(std::make_shared<CEditorActionEditGroupProp>(this, m_SelectedGroup, EGroupProp::PROP_ORDER, s_InitialGroupIndex, m_SelectedGroup));
		}
		else if(s_PreviousOperation == OP_LAYER_DRAG)
		{
			if(s_InitialGroupIndex != m_SelectedGroup)
			{
				m_EditorHistory.RecordAction(std::make_shared<CEditorActionEditLayersGroupAndOrder>(this, s_InitialGroupIndex, s_vInitialLayerIndices, m_SelectedGroup, m_vSelectedLayers));
			}
			else
			{
				std::vector<std::shared_ptr<IEditorAction>> vpActions;
				std::vector<int> vLayerIndices = m_vSelectedLayers;
				std::sort(vLayerIndices.begin(), vLayerIndices.end());
				std::sort(s_vInitialLayerIndices.begin(), s_vInitialLayerIndices.end());
				for(int k = 0; k < (int)vLayerIndices.size(); k++)
				{
					int LayerIndex = vLayerIndices[k];
					vpActions.push_back(std::make_shared<CEditorActionEditLayerProp>(this, m_SelectedGroup, LayerIndex, ELayerProp::PROP_ORDER, s_vInitialLayerIndices[k], LayerIndex));
				}
				m_EditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(this, vpActions, nullptr, true));
			}
			s_PreviousOperation = OP_NONE;
		}
	}
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

	CImageInfo ImgInfo;
	if(!Graphics()->LoadPng(ImgInfo, pFileName, StorageType))
	{
		ShowFileDialogError("Failed to load image from file '%s'.", pFileName);
		return false;
	}

	std::shared_ptr<CEditorImage> pImg = m_Map.m_vpImages[m_SelectedImage];
	Graphics()->UnloadTexture(&(pImg->m_Texture));
	pImg->Free();
	pImg->m_Width = ImgInfo.m_Width;
	pImg->m_Height = ImgInfo.m_Height;
	pImg->m_Format = ImgInfo.m_Format;
	pImg->m_pData = ImgInfo.m_pData;
	str_copy(pImg->m_aName, aBuf);
	pImg->m_External = IsVanillaImage(pImg->m_aName);

	ConvertToRgba(*pImg);
	if(g_Config.m_ClEditorDilate == 1)
	{
		DilateImage(*pImg);
	}

	pImg->m_AutoMapper.Load(pImg->m_aName);
	int TextureLoadFlag = Graphics()->Uses2DTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE;
	if(pImg->m_Width % 16 != 0 || pImg->m_Height % 16 != 0)
		TextureLoadFlag = 0;
	pImg->m_Texture = Graphics()->LoadTextureRaw(*pImg, TextureLoadFlag, pFileName);

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

	if(pEditor->m_Map.m_vpImages.size() >= MAX_MAPIMAGES)
	{
		pEditor->m_PopupEventType = POPEVENT_IMAGE_MAX;
		pEditor->m_PopupEventActivated = true;
		return false;
	}

	CImageInfo ImgInfo;
	if(!pEditor->Graphics()->LoadPng(ImgInfo, pFileName, StorageType))
	{
		pEditor->ShowFileDialogError("Failed to load image from file '%s'.", pFileName);
		return false;
	}

	std::shared_ptr<CEditorImage> pImg = std::make_shared<CEditorImage>(pEditor);
	pImg->m_Width = ImgInfo.m_Width;
	pImg->m_Height = ImgInfo.m_Height;
	pImg->m_Format = ImgInfo.m_Format;
	pImg->m_pData = ImgInfo.m_pData;
	pImg->m_External = IsVanillaImage(aBuf);

	ConvertToRgba(*pImg);
	if(g_Config.m_ClEditorDilate == 1)
	{
		DilateImage(*pImg);
	}

	int TextureLoadFlag = pEditor->Graphics()->Uses2DTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE;
	if(pImg->m_Width % 16 != 0 || pImg->m_Height % 16 != 0)
		TextureLoadFlag = 0;
	pImg->m_Texture = pEditor->Graphics()->LoadTextureRaw(*pImg, TextureLoadFlag, pFileName);
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

	if(pEditor->m_Map.m_vpSounds.size() >= MAX_MAPSOUNDS)
	{
		pEditor->m_PopupEventType = POPEVENT_SOUND_MAX;
		pEditor->m_PopupEventActivated = true;
		return false;
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
	pSound->m_SoundId = SoundId;
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

	pEditor->OnDialogClose();
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
	Sound()->UnloadSample(pSound->m_SoundId);
	free(pSound->m_pData);

	// replace sound
	str_copy(pSound->m_aName, aBuf);
	pSound->m_SoundId = SoundId;
	pSound->m_pData = pData;
	pSound->m_DataSize = DataSize;

	OnDialogClose();
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

std::vector<int> CEditor::SortImages()
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

		return vSortedIndex;
	}

	return std::vector<int>();
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
			Ui()->DoLabel(&Slot, e == 0 ? "Embedded" : "External", 12.0f, TEXTALIGN_MC);

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
					const int Height = pImg->m_External ? 73 : 107;
					static SPopupMenuId s_PopupImageId;
					Ui()->DoPopupMenu(&s_PopupImageId, Ui()->MouseX(), Ui()->MouseY(), 140, Height, this, PopupImage);
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
		if(DoButton_Editor(&s_AddImageButton, m_QuickActionAddImage.Label(), 0, &AddImageButton, 0, m_QuickActionAddImage.Description()))
			m_QuickActionAddImage.Call();
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
		Ui()->DoLabel(&Slot, "Embedded", 12.0f, TEXTALIGN_MC);

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
				Ui()->DoPopupMenu(&s_PopupSoundId, Ui()->MouseX(), Ui()->MouseY(), 140, 90, this, PopupSound);
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
	Ui()->MapScreen();
	CUIRect View = *Ui()->Screen();
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
	Ui()->DoLabel(&Title, m_pFileDialogTitle, 12.0f, TEXTALIGN_ML);

	// pathbox
	if(m_FilesSelectedIndex >= 0 && m_vpFilteredFileList[m_FilesSelectedIndex]->m_StorageType >= IStorage::TYPE_SAVE)
	{
		char aPath[IO_MAX_PATH_LENGTH], aBuf[128 + IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(m_vpFilteredFileList[m_FilesSelectedIndex]->m_StorageType, m_pFileDialogPath, aPath, sizeof(aPath));
		str_format(aBuf, sizeof(aBuf), "Current path: %s", aPath);
		Ui()->DoLabel(&PathBox, aBuf, 10.0f, TEXTALIGN_ML);
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
	s_ListBox.SetActive(!Ui()->IsPopupOpen());

	if(m_FileDialogStorageType == IStorage::TYPE_SAVE)
	{
		Ui()->DoLabel(&FileBoxLabel, "Filename:", 10.0f, TEXTALIGN_ML);
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
			Ui()->SetActiveItem(&m_FileDialogFileNameInput);
	}
	else
	{
		// render search bar
		Ui()->DoLabel(&FileBoxLabel, "Search:", 10.0f, TEXTALIGN_ML);
		if(m_FileDialogOpening || (Input()->KeyPress(KEY_F) && Input()->ModifierIsPressed()))
		{
			Ui()->SetActiveItem(&m_FileDialogFilterInput);
			m_FileDialogFilterInput.SelectAll();
		}
		if(Ui()->DoClearableEditBox(&m_FileDialogFilterInput, &FileBox, 10.0f))
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
				CImageInfo PreviewImageInfo;
				if(Graphics()->LoadPng(PreviewImageInfo, aBuffer, m_vpFilteredFileList[m_FilesSelectedIndex]->m_StorageType))
				{
					Graphics()->UnloadTexture(&m_FilePreviewImage);
					m_FilePreviewImageWidth = PreviewImageInfo.m_Width;
					m_FilePreviewImageHeight = PreviewImageInfo.m_Height;
					m_FilePreviewImage = Graphics()->LoadTextureRawMove(PreviewImageInfo, 0, aBuffer);
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
				CUIRect PreviewLabel, PreviewImage;
				Preview.HSplitTop(20.0f, &PreviewLabel, &PreviewImage);

				char aLabel[64];
				str_format(aLabel, sizeof(aLabel), "Size: %d × %d", m_FilePreviewImageWidth, m_FilePreviewImageHeight);
				Ui()->DoLabel(&PreviewLabel, aLabel, 12.0f, TEXTALIGN_ML);

				int w = m_FilePreviewImageWidth;
				int h = m_FilePreviewImageHeight;
				if(m_FilePreviewImageWidth > PreviewImage.w)
				{
					h = m_FilePreviewImageHeight * PreviewImage.w / m_FilePreviewImageWidth;
					w = PreviewImage.w;
				}
				if(h > PreviewImage.h)
				{
					w = w * PreviewImage.h / h;
					h = PreviewImage.h;
				}

				Graphics()->TextureSet(m_FilePreviewImage);
				Graphics()->BlendNormal();
				Graphics()->QuadsBegin();
				IGraphics::CQuadItem QuadItem(PreviewImage.x, PreviewImage.y, w, h);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
				Graphics()->QuadsEnd();
			}
			else if(m_FilePreviewState == PREVIEW_ERROR)
			{
				SLabelProperties Props;
				Props.m_MaxWidth = Preview.w;
				Ui()->DoLabel(&Preview, "Failed to load the image (check the local console for details).", 12.0f, TEXTALIGN_TL, Props);
			}
		}
		else if(m_FileDialogFileType == CEditor::FILETYPE_SOUND)
		{
			Preview.Margin(10.0f, &Preview);
			if(m_FilePreviewState == PREVIEW_LOADED)
			{
				Preview.HSplitTop(20.0f, &Preview, nullptr);
				Preview.VSplitLeft(Preview.h / 4.0f, nullptr, &Preview);

				static int s_PlayPauseButton, s_StopButton, s_SeekBar = 0;
				DoAudioPreview(Preview, &s_PlayPauseButton, &s_StopButton, &s_SeekBar, m_FilePreviewSound);
			}
			else if(m_FilePreviewState == PREVIEW_ERROR)
			{
				SLabelProperties Props;
				Props.m_MaxWidth = Preview.w;
				Ui()->DoLabel(&Preview, "Failed to load the sound (check the local console for details). Make sure you enabled sounds in the settings.", 12.0f, TEXTALIGN_TL, Props);
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
		Ui()->DoLabel(&FileIcon, pIconType, 12.0f, TEXTALIGN_ML);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

		SLabelProperties Props;
		Props.m_MaxWidth = Button.w;
		Props.m_EllipsisAtEnd = true;
		Ui()->DoLabel(&Button, m_vpFilteredFileList[i]->m_aName, 10.0f, TEXTALIGN_ML, Props);

		if(!m_vpFilteredFileList[i]->m_IsLink && str_comp(m_vpFilteredFileList[i]->m_aFilename, "..") != 0)
		{
			char aBufTimeModified[64];
			str_timestamp_ex(m_vpFilteredFileList[i]->m_TimeModified, aBufTimeModified, sizeof(aBufTimeModified), "%d.%m.%Y %H:%M");
			Ui()->DoLabel(&TimeModified, aBufTimeModified, 10.0f, TEXTALIGN_MR);
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
	if(DoButton_Editor(&s_OkButton, IsDir ? "Open" : m_pFileDialogButtonText, 0, &Button, 0, nullptr) || s_ListBox.WasItemActivated() || (s_ListBox.Active() && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)))
	{
		if(IsDir) // folder
		{
			m_FileDialogFilterInput.Clear();
			Ui()->SetActiveItem(&m_FileDialogFilterInput);
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
			const bool SaveAction = m_FileDialogStorageType == IStorage::TYPE_SAVE;
			if(SaveAction && Storage()->FileExists(m_aFileSaveName, StorageType))
			{
				if(m_pfnFileDialogFunc == &CallbackSaveMap)
					m_PopupEventType = POPEVENT_SAVE;
				else if(m_pfnFileDialogFunc == &CallbackSaveCopyMap)
					m_PopupEventType = POPEVENT_SAVE_COPY;
				else if(m_pfnFileDialogFunc == &CallbackSaveImage)
					m_PopupEventType = POPEVENT_SAVE_IMG;
				else if(m_pfnFileDialogFunc == &CallbackSaveSound)
					m_PopupEventType = POPEVENT_SAVE_SOUND;
				else
					dbg_assert(false, "m_pfnFileDialogFunc unhandled for saving");
				m_PopupEventActivated = true;
			}
			else if(m_pfnFileDialogFunc && (SaveAction || m_FilesSelectedIndex >= 0))
			{
				m_pfnFileDialogFunc(m_aFileSaveName, StorageType, m_pFileDialogUser);
			}
		}
	}

	ButtonBar.VSplitRight(ButtonSpacing, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	if(DoButton_Editor(&s_CancelButton, "Cancel", 0, &Button, 0, nullptr) || (s_ListBox.Active() && Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE)))
	{
		OnDialogClose();
		m_Dialog = DIALOG_NONE;
	}

	ButtonBar.VSplitRight(ButtonSpacing, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	if(DoButton_Editor(&s_RefreshButton, "Refresh", 0, &Button, 0, nullptr) || (s_ListBox.Active() && (Input()->KeyIsPressed(KEY_F5) || (Input()->ModifierIsPressed() && Input()->KeyIsPressed(KEY_R)))))
		FilelistPopulate(m_FileDialogLastPopulatedStorageType, true);

	if(m_FilesSelectedIndex >= 0 && m_vpFilteredFileList[m_FilesSelectedIndex]->m_StorageType != IStorage::TYPE_ALL)
	{
		ButtonBar.VSplitRight(ButtonSpacing, &ButtonBar, nullptr);
		ButtonBar.VSplitRight(90.0f, &ButtonBar, &Button);
		if(DoButton_Editor(&s_ShowDirectoryButton, "Show directory", 0, &Button, 0, "Open the current directory in the file browser"))
		{
			char aOpenPath[IO_MAX_PATH_LENGTH];
			Storage()->GetCompletePath(m_FilesSelectedIndex >= 0 ? m_vpFilteredFileList[m_FilesSelectedIndex]->m_StorageType : IStorage::TYPE_SAVE, m_pFileDialogPath, aOpenPath, sizeof(aOpenPath));
			if(!Client()->ViewFile(aOpenPath))
			{
				ShowFileDialogError("Failed to open the directory '%s'.", aOpenPath);
			}
		}
	}

	ButtonBar.VSplitRight(ButtonSpacing, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	static CUi::SConfirmPopupContext s_ConfirmDeletePopupContext;
	if(m_FilesSelectedIndex >= 0 && m_vpFilteredFileList[m_FilesSelectedIndex]->m_StorageType == IStorage::TYPE_SAVE && !m_vpFilteredFileList[m_FilesSelectedIndex]->m_IsLink && str_comp(m_vpFilteredFileList[m_FilesSelectedIndex]->m_aFilename, "..") != 0)
	{
		if(DoButton_Editor(&s_DeleteButton, "Delete", 0, &Button, 0, nullptr) || (s_ListBox.Active() && Ui()->ConsumeHotkey(CUi::HOTKEY_DELETE)))
		{
			s_ConfirmDeletePopupContext.Reset();
			s_ConfirmDeletePopupContext.YesNoButtons();
			str_format(s_ConfirmDeletePopupContext.m_aMessage, sizeof(s_ConfirmDeletePopupContext.m_aMessage), "Are you sure that you want to delete the %s '%s/%s'?", IsDir ? "folder" : "file", m_pFileDialogPath, m_vpFilteredFileList[m_FilesSelectedIndex]->m_aFilename);
			Ui()->ShowPopupConfirm(Ui()->MouseX(), Ui()->MouseY(), &s_ConfirmDeletePopupContext);
		}
		if(s_ConfirmDeletePopupContext.m_Result == CUi::SConfirmPopupContext::CONFIRMED)
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
			UpdateFileNameInput();
		}
		if(s_ConfirmDeletePopupContext.m_Result != CUi::SConfirmPopupContext::UNSET)
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
			Ui()->DoPopupMenu(&s_PopupNewFolderId, Width / 2.0f - PopupWidth / 2.0f, Height / 2.0f - PopupHeight / 2.0f, PopupWidth, PopupHeight, this, PopupNewFolder);
			Ui()->SetActiveItem(&m_FileDialogNewFolderNameInput);
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
		int NumStoragesWithFolder = 0;
		for(int CheckStorageType = IStorage::TYPE_SAVE; CheckStorageType < Storage()->NumPaths(); ++CheckStorageType)
		{
			if(Storage()->FolderExists(pBasePath, CheckStorageType))
			{
				NumStoragesWithFolder++;
			}
		}
		m_FileDialogMultipleStorages = NumStoragesWithFolder > 1;
	}
	else
	{
		m_FileDialogMultipleStorages = false;
	}

	Ui()->ClosePopupMenus();
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
	CUi::SMessagePopupContext *pContext;
	if(ContextIterator != m_PopupMessageContexts.end())
	{
		pContext = ContextIterator->second;
		Ui()->ClosePopupMenu(pContext);
	}
	else
	{
		pContext = new CUi::SMessagePopupContext();
		pContext->ErrorColor();
		str_copy(pContext->m_aMessage, aMessage);
		m_PopupMessageContexts[pContext->m_aMessage] = pContext;
	}
	Ui()->ShowPopupMessage(Ui()->MouseX(), Ui()->MouseY(), pContext);
}

void CEditor::RenderModebar(CUIRect View)
{
	CUIRect Mentions, IngameMoved, ModeButtons, ModeButton;
	View.HSplitTop(12.0f, &Mentions, &View);
	View.HSplitTop(12.0f, &IngameMoved, &View);
	View.HSplitTop(8.0f, nullptr, &ModeButtons);
	const float Width = m_ToolBoxWidth - 5.0f;
	ModeButtons.VSplitLeft(Width, &ModeButtons, nullptr);
	const float ButtonWidth = Width / 3;

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
		Ui()->DoLabel(&Mentions, aBuf, 10.0f, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	// ingame moved warning
	if(m_IngameMoved)
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		Ui()->DoLabel(&IngameMoved, Localize("Moved ingame"), 10.0f, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	// mode buttons
	{
		ModeButtons.VSplitLeft(ButtonWidth, &ModeButton, &ModeButtons);
		static int s_LayersButton = 0;
		if(DoButton_FontIcon(&s_LayersButton, FONT_ICON_LAYER_GROUP, m_Mode == MODE_LAYERS, &ModeButton, 0, "Go to layers management.", IGraphics::CORNER_L))
		{
			m_Mode = MODE_LAYERS;
		}

		ModeButtons.VSplitLeft(ButtonWidth, &ModeButton, &ModeButtons);
		static int s_ImagesButton = 0;
		if(DoButton_FontIcon(&s_ImagesButton, FONT_ICON_IMAGE, m_Mode == MODE_IMAGES, &ModeButton, 0, "Go to images management.", IGraphics::CORNER_NONE))
		{
			m_Mode = MODE_IMAGES;
		}

		ModeButtons.VSplitLeft(ButtonWidth, &ModeButton, &ModeButtons);
		static int s_SoundsButton = 0;
		if(DoButton_FontIcon(&s_SoundsButton, FONT_ICON_MUSIC, m_Mode == MODE_SOUNDS, &ModeButton, 0, "Go to sounds management.", IGraphics::CORNER_R))
		{
			m_Mode = MODE_SOUNDS;
		}

		if(Input()->KeyPress(KEY_LEFT) && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr)
		{
			m_Mode = (m_Mode + NUM_MODES - 1) % NUM_MODES;
		}
		else if(Input()->KeyPress(KEY_RIGHT) && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr)
		{
			m_Mode = (m_Mode + 1) % NUM_MODES;
		}
	}
}

void CEditor::RenderStatusbar(CUIRect View, CUIRect *pTooltipRect)
{
	CUIRect Button;
	View.VSplitRight(100.0f, &View, &Button);
	if(DoButton_Editor(&m_QuickActionEnvelopes, m_QuickActionEnvelopes.Label(), m_QuickActionEnvelopes.Color(), &Button, 0, m_QuickActionEnvelopes.Description()) == 1)
	{
		m_QuickActionEnvelopes.Call();
	}

	View.VSplitRight(10.0f, &View, nullptr);
	View.VSplitRight(100.0f, &View, &Button);
	if(DoButton_Editor(&m_QuickActionServerSettings, m_QuickActionServerSettings.Label(), m_QuickActionServerSettings.Color(), &Button, 0, m_QuickActionServerSettings.Description()) == 1)
	{
		m_QuickActionServerSettings.Call();
	}

	View.VSplitRight(10.0f, &View, nullptr);
	View.VSplitRight(100.0f, &View, &Button);
	if(DoButton_Editor(&m_QuickActionHistory, m_QuickActionHistory.Label(), m_QuickActionHistory.Color(), &Button, 0, m_QuickActionHistory.Description()) == 1)
	{
		m_QuickActionHistory.Call();
	}

	View.VSplitRight(10.0f, pTooltipRect, nullptr);
}

void CEditor::RenderTooltip(CUIRect TooltipRect)
{
	if(str_comp(m_aTooltip, "") == 0)
		return;

	char aBuf[256];
	if(ms_pUiGotContext && ms_pUiGotContext == Ui()->HotItem())
		str_format(aBuf, sizeof(aBuf), "%s Right click for context menu.", m_aTooltip);
	else
		str_copy(aBuf, m_aTooltip);

	SLabelProperties Props;
	Props.m_MaxWidth = TooltipRect.w;
	Props.m_EllipsisAtEnd = true;
	Ui()->DoLabel(&TooltipRect, aBuf, 10.0f, TEXTALIGN_ML, Props);
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
	m_EnvelopeEditorHistory.BeginBulk();
	int DeletedCount = 0;
	for(size_t Envelope = 0; Envelope < m_Map.m_vpEnvelopes.size();)
	{
		if(IsEnvelopeUsed(Envelope))
		{
			++Envelope;
		}
		else
		{
			m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionEveloppeDelete>(this, Envelope));
			m_Map.DeleteEnvelope(Envelope);
			DeletedCount++;
		}
	}
	char aDisplay[256];
	str_format(aDisplay, sizeof(aDisplay), "Tool 'Remove unused envelopes': delete %d envelopes", DeletedCount);
	m_EnvelopeEditorHistory.EndBulk(aDisplay);
}

void CEditor::ZoomAdaptOffsetX(float ZoomFactor, const CUIRect &View)
{
	float PosX = g_Config.m_EdZoomTarget ? (Ui()->MouseX() - View.x) / View.w : 0.5f;
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
	float PosY = g_Config.m_EdZoomTarget ? 1.0f - (Ui()->MouseY() - View.y) / View.h : 0.5f;
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
	auto [Bottom, Top] = pEnvelope->GetValueRange(ActiveChannels);
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

float CEditor::ScreenToEnvelopeDX(const CUIRect &View, float DeltaX)
{
	return DeltaX / Graphics()->ScreenWidth() * Ui()->Screen()->w / View.w * m_ZoomEnvelopeX.GetValue();
}

float CEditor::ScreenToEnvelopeDY(const CUIRect &View, float DeltaY)
{
	return DeltaY / Graphics()->ScreenHeight() * Ui()->Screen()->h / View.h * m_ZoomEnvelopeY.GetValue();
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
	if(!Ui()->MouseInside(&View))
		return;

	float mx = Ui()->MouseX();
	float my = Ui()->MouseY();

	float MinDist = 200.0f;
	int *pMinPoint = nullptr;

	auto UpdateMinimum = [&](float px, float py, int *pId) {
		float dx = px - mx;
		float dy = py - my;

		float CurrDist = dx * dx + dy * dy;
		if(CurrDist < MinDist)
		{
			MinDist = CurrDist;
			pMinPoint = pId;
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
		Ui()->SetHotItem(pMinPoint);
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

	static EEnvelopeEditorOp s_Operation = EEnvelopeEditorOp::OP_NONE;
	static std::vector<float> s_vAccurateDragValuesX = {};
	static std::vector<float> s_vAccurateDragValuesY = {};
	static float s_MouseXStart = 0.0f;
	static float s_MouseYStart = 0.0f;

	static CLineInput s_NameInput;

	CUIRect ToolBar, CurveBar, ColorBar, DragBar;
	View.HSplitTop(30.0f, &DragBar, nullptr);
	DragBar.y -= 2.0f;
	DragBar.w += 2.0f;
	DragBar.h += 4.0f;
	DoEditorDragBar(View, &DragBar, EDragSide::SIDE_TOP, &m_aExtraEditorSplits[EXTRAEDITOR_ENVELOPES]);
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

		// redo button
		ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
		static int s_RedoButton = 0;
		if(DoButton_FontIcon(&s_RedoButton, FONT_ICON_REDO, m_EnvelopeEditorHistory.CanRedo() ? 0 : -1, &Button, 0, "[Ctrl+Y] Redo last action", IGraphics::CORNER_R, 11.0f) == 1)
		{
			m_EnvelopeEditorHistory.Redo();
		}

		// undo button
		ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
		ToolBar.VSplitRight(10.0f, &ToolBar, nullptr);
		static int s_UndoButton = 0;
		if(DoButton_FontIcon(&s_UndoButton, FONT_ICON_UNDO, m_EnvelopeEditorHistory.CanUndo() ? 0 : -1, &Button, 0, "[Ctrl+Z] Undo last action", IGraphics::CORNER_L, 11.0f) == 1)
		{
			m_EnvelopeEditorHistory.Undo();
		}

		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_NewSoundButton = 0;
		if(DoButton_Editor(&s_NewSoundButton, "Sound+", 0, &Button, 0, "Creates a new sound envelope"))
		{
			m_Map.OnModify();
			pNewEnv = m_Map.NewEnvelope(CEnvelope::EType::SOUND);
		}

		ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_New4dButton = 0;
		if(DoButton_Editor(&s_New4dButton, "Color+", 0, &Button, 0, "Creates a new color envelope"))
		{
			m_Map.OnModify();
			pNewEnv = m_Map.NewEnvelope(CEnvelope::EType::COLOR);
		}

		ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_New2dButton = 0;
		if(DoButton_Editor(&s_New2dButton, "Pos.+", 0, &Button, 0, "Creates a new position envelope"))
		{
			m_Map.OnModify();
			pNewEnv = m_Map.NewEnvelope(CEnvelope::EType::POSITION);
		}

		if(m_SelectedEnvelope >= 0)
		{
			// Delete button
			ToolBar.VSplitRight(10.0f, &ToolBar, nullptr);
			ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
			static int s_DeleteButton = 0;
			if(DoButton_Editor(&s_DeleteButton, "✗", 0, &Button, 0, "Delete this envelope"))
			{
				m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionEveloppeDelete>(this, m_SelectedEnvelope));
				m_Map.DeleteEnvelope(m_SelectedEnvelope);
				if(m_SelectedEnvelope >= (int)m_Map.m_vpEnvelopes.size())
					m_SelectedEnvelope = m_Map.m_vpEnvelopes.size() - 1;
				pEnvelope = m_SelectedEnvelope >= 0 ? m_Map.m_vpEnvelopes[m_SelectedEnvelope] : nullptr;
				m_Map.OnModify();
			}

			// Move right button
			ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
			ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
			static int s_MoveRightButton = 0;
			if(DoButton_Ex(&s_MoveRightButton, "→", 0, &Button, 0, "Move this envelope to the right", IGraphics::CORNER_R))
			{
				m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionEnvelopeEdit>(this, m_SelectedEnvelope, CEditorActionEnvelopeEdit::EEditType::ORDER, m_SelectedEnvelope, m_SelectedEnvelope + 1));
				m_Map.SwapEnvelopes(m_SelectedEnvelope, m_SelectedEnvelope + 1);
				m_SelectedEnvelope = clamp<int>(m_SelectedEnvelope + 1, 0, m_Map.m_vpEnvelopes.size() - 1);
				pEnvelope = m_Map.m_vpEnvelopes[m_SelectedEnvelope];
				m_Map.OnModify();
			}

			// Move left button
			ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
			static int s_MoveLeftButton = 0;
			if(DoButton_Ex(&s_MoveLeftButton, "←", 0, &Button, 0, "Move this envelope to the left", IGraphics::CORNER_L))
			{
				m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionEnvelopeEdit>(this, m_SelectedEnvelope, CEditorActionEnvelopeEdit::EEditType::ORDER, m_SelectedEnvelope, m_SelectedEnvelope - 1));
				m_Map.SwapEnvelopes(m_SelectedEnvelope - 1, m_SelectedEnvelope);
				m_SelectedEnvelope = clamp<int>(m_SelectedEnvelope - 1, 0, m_Map.m_vpEnvelopes.size() - 1);
				pEnvelope = m_Map.m_vpEnvelopes[m_SelectedEnvelope];
				m_Map.OnModify();
			}

			if(pEnvelope)
			{
				ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
				ToolBar.VSplitRight(20.0f, &ToolBar, &Button);
				static int s_ZoomOutButton = 0;
				if(DoButton_FontIcon(&s_ZoomOutButton, FONT_ICON_MINUS, 0, &Button, 0, "[NumPad-] Zoom out horizontally, hold shift to zoom vertically", IGraphics::CORNER_R, 9.0f))
				{
					if(Input()->ShiftIsPressed())
						m_ZoomEnvelopeY.ChangeValue(0.1f * m_ZoomEnvelopeY.GetValue());
					else
						m_ZoomEnvelopeX.ChangeValue(0.1f * m_ZoomEnvelopeX.GetValue());
				}

				ToolBar.VSplitRight(20.0f, &ToolBar, &Button);
				static int s_ResetZoomButton = 0;
				if(DoButton_FontIcon(&s_ResetZoomButton, FONT_ICON_MAGNIFYING_GLASS, 0, &Button, 0, "[NumPad*] Reset zoom to default value", IGraphics::CORNER_NONE, 9.0f))
					ResetZoomEnvelope(pEnvelope, s_ActiveChannels);

				ToolBar.VSplitRight(20.0f, &ToolBar, &Button);
				static int s_ZoomInButton = 0;
				if(DoButton_FontIcon(&s_ZoomInButton, FONT_ICON_PLUS, 0, &Button, 0, "[NumPad+] Zoom in horizontally, hold shift to zoom vertically", IGraphics::CORNER_L, 9.0f))
				{
					if(Input()->ShiftIsPressed())
						m_ZoomEnvelopeY.ChangeValue(-0.1f * m_ZoomEnvelopeY.GetValue());
					else
						m_ZoomEnvelopeX.ChangeValue(-0.1f * m_ZoomEnvelopeX.GetValue());
				}
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

			m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionEnvelopeAdd>(this, pNewEnv));
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
			EnvColor = IsEnvelopeUsed(m_SelectedEnvelope) ? ColorRGBA(1, 0.7f, 0.7f, 0.5f) : ColorRGBA(0.7f, 1, 0.7f, 0.5f);
		}

		static int s_EnvelopeSelector = 0;
		auto NewValueRes = UiDoValueSelector(&s_EnvelopeSelector, &Shifter, aBuf, m_SelectedEnvelope + 1, 1, m_Map.m_vpEnvelopes.size(), 1, 1.0f, "Select Envelope", false, false, IGraphics::CORNER_NONE, &EnvColor, false);
		int NewValue = NewValueRes.m_Value;
		if(NewValue - 1 != m_SelectedEnvelope)
		{
			m_SelectedEnvelope = NewValue - 1;
			CurrentEnvelopeSwitched = true;
		}

		static int s_PrevButton = 0;
		if(DoButton_FontIcon(&s_PrevButton, FONT_ICON_MINUS, 0, &Dec, 0, "Previous Envelope", IGraphics::CORNER_L, 7.0f))
		{
			m_SelectedEnvelope--;
			if(m_SelectedEnvelope < 0)
				m_SelectedEnvelope = m_Map.m_vpEnvelopes.size() - 1;
			CurrentEnvelopeSwitched = true;
		}

		static int s_NextButton = 0;
		if(DoButton_FontIcon(&s_NextButton, FONT_ICON_PLUS, 0, &Inc, 0, "Next Envelope", IGraphics::CORNER_R, 7.0f))
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
			Ui()->DoLabel(&Button, "Name:", 10.0f, TEXTALIGN_MR);

			ToolBar.VSplitLeft(3.0f, nullptr, &ToolBar);
			ToolBar.VSplitLeft(ToolBar.w > ToolBar.h * 40 ? 80.0f : 60.0f, &Button, &ToolBar);

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

		// toggle sync button
		ToolBar.VSplitLeft(15.0f, nullptr, &ToolBar);
		ToolBar.VSplitLeft(40.0f, &Button, &ToolBar);

		static int s_SyncButton;
		if(DoButton_Editor(&s_SyncButton, "Sync", pEnvelope->m_Synchronized, &Button, 0, "Synchronize envelope animation to game time (restarts when you touch the start line)"))
		{
			m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionEnvelopeEdit>(this, m_SelectedEnvelope, CEditorActionEnvelopeEdit::EEditType::SYNC, pEnvelope->m_Synchronized, !pEnvelope->m_Synchronized));
			pEnvelope->m_Synchronized = !pEnvelope->m_Synchronized;
			m_Map.OnModify();
		}

		static int s_EnvelopeEditorId = 0;
		static int s_EnvelopeEditorButtonUsed = -1;
		const bool ShouldPan = s_Operation == EEnvelopeEditorOp::OP_NONE && (Ui()->MouseButton(2) || (Ui()->MouseButton(0) && Input()->ModifierIsPressed()));
		if(m_pContainerPanned == &s_EnvelopeEditorId)
		{
			if(!ShouldPan)
				m_pContainerPanned = nullptr;
			else
			{
				m_OffsetEnvelopeX += Ui()->MouseDeltaX() / Graphics()->ScreenWidth() * Ui()->Screen()->w / View.w;
				m_OffsetEnvelopeY -= Ui()->MouseDeltaY() / Graphics()->ScreenHeight() * Ui()->Screen()->h / View.h;
			}
		}

		if(Ui()->MouseInside(&View) && m_Dialog == DIALOG_NONE)
		{
			Ui()->SetHotItem(&s_EnvelopeEditorId);

			if(ShouldPan && m_pContainerPanned == nullptr)
				m_pContainerPanned = &s_EnvelopeEditorId;

			if(Input()->KeyPress(KEY_KP_MULTIPLY))
				ResetZoomEnvelope(pEnvelope, s_ActiveChannels);
			if(Input()->ShiftIsPressed())
			{
				if(Input()->KeyPress(KEY_KP_MINUS))
					m_ZoomEnvelopeY.ChangeValue(0.1f * m_ZoomEnvelopeY.GetValue());
				if(Input()->KeyPress(KEY_KP_PLUS))
					m_ZoomEnvelopeY.ChangeValue(-0.1f * m_ZoomEnvelopeY.GetValue());
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
					m_ZoomEnvelopeY.ChangeValue(0.1f * m_ZoomEnvelopeY.GetValue());
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
					m_ZoomEnvelopeY.ChangeValue(-0.1f * m_ZoomEnvelopeY.GetValue());
			}
			else
			{
				if(Input()->KeyPress(KEY_KP_MINUS))
					m_ZoomEnvelopeX.ChangeValue(0.1f * m_ZoomEnvelopeX.GetValue());
				if(Input()->KeyPress(KEY_KP_PLUS))
					m_ZoomEnvelopeX.ChangeValue(-0.1f * m_ZoomEnvelopeX.GetValue());
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
					m_ZoomEnvelopeX.ChangeValue(0.1f * m_ZoomEnvelopeX.GetValue());
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
					m_ZoomEnvelopeX.ChangeValue(-0.1f * m_ZoomEnvelopeX.GetValue());
			}
		}

		if(Ui()->HotItem() == &s_EnvelopeEditorId)
		{
			// do stuff
			if(Ui()->MouseButton(0))
			{
				s_EnvelopeEditorButtonUsed = 0;
				if(s_Operation != EEnvelopeEditorOp::OP_BOX_SELECT && !Input()->ModifierIsPressed())
				{
					s_Operation = EEnvelopeEditorOp::OP_BOX_SELECT;
					s_MouseXStart = Ui()->MouseX();
					s_MouseYStart = Ui()->MouseY();
				}
			}
			else if(s_EnvelopeEditorButtonUsed == 0)
			{
				if(Ui()->DoDoubleClickLogic(&s_EnvelopeEditorId))
				{
					// add point
					float Time = ScreenToEnvelopeX(View, Ui()->MouseX());
					ColorRGBA Channels = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
					if(in_range(Time, 0.0f, pEnvelope->EndTime()))
						pEnvelope->Eval(Time, Channels, 4);

					int FixedTime = std::round(Time * 1000.0f);
					bool TimeFound = false;
					for(CEnvPoint &Point : pEnvelope->m_vPoints)
					{
						if(Point.m_Time == FixedTime)
							TimeFound = true;
					}

					if(!TimeFound)
						m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionAddEnvelopePoint>(this, m_SelectedEnvelope, FixedTime, Channels));

					if(FixedTime < 0)
						RemoveTimeOffsetEnvelope(pEnvelope);
					m_Map.OnModify();
				}
				s_EnvelopeEditorButtonUsed = -1;
			}

			m_ShowEnvelopePreview = SHOWENV_SELECTED;
			str_copy(m_aTooltip, "Double-click to create a new point. Use shift to change the zoom axis. Press S to scale selected envelope points.");
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
			int NumLinesY = m_ZoomEnvelopeY.GetValue() / UnitsPerLineY + 1;

			Ui()->ClipEnable(&View);
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

			Ui()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.4f);
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
				Ui()->TextRender()->Text(View.x, EnvelopeToScreenY(View, Value) + 4.0f, 8.0f, aValueBuffer);
			}
			Ui()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			Ui()->ClipDisable();
		}

		{
			using namespace std::chrono_literals;
			CTimeStep UnitsPerLineX = 1ms;
			static const CTimeStep s_aUnitPerLineOptionsX[] = {5ms, 10ms, 25ms, 50ms, 100ms, 250ms, 500ms, 1s, 2s, 5s, 10s, 15s, 30s, 1min};
			for(CTimeStep Value : s_aUnitPerLineOptionsX)
			{
				if(Value.AsSeconds() / m_ZoomEnvelopeX.GetValue() * View.w < 160.0f)
					UnitsPerLineX = Value;
			}
			int NumLinesX = m_ZoomEnvelopeX.GetValue() / UnitsPerLineX.AsSeconds() + 1;

			Ui()->ClipEnable(&View);
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

			Ui()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.4f);
			for(int i = 0; i <= NumLinesX; i++)
			{
				CTimeStep Value = UnitsPerLineX * i - BaseValue;
				if(Value.AsSeconds() >= 0)
				{
					char aValueBuffer[16];
					Value.Format(aValueBuffer, sizeof(aValueBuffer));

					Ui()->TextRender()->Text(EnvelopeToScreenX(View, Value.AsSeconds()) + 1.0f, View.y + View.h - 8.0f, 8.0f, aValueBuffer);
				}
			}
			Ui()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			Ui()->ClipDisable();
		}

		// render tangents for bezier curves
		{
			Ui()->ClipEnable(&View);
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
			Ui()->ClipDisable();
		}

		// render lines
		{
			float EndTimeTotal = maximum(0.000001f, pEnvelope->EndTime());
			float EndX = clamp(EnvelopeToScreenX(View, EndTimeTotal), View.x, View.x + View.w);
			float StartX = clamp(View.x + View.w * m_OffsetEnvelopeX, View.x, View.x + View.w);

			float EndTime = ScreenToEnvelopeX(View, EndX);
			float StartTime = ScreenToEnvelopeX(View, StartX);

			Ui()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			for(int c = 0; c < pEnvelope->GetChannels(); c++)
			{
				if(s_ActiveChannels & (1 << c))
					Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 1);
				else
					Graphics()->SetColor(aColors[c].r * 0.5f, aColors[c].g * 0.5f, aColors[c].b * 0.5f, 1);

				int Steps = static_cast<int>(((EndX - StartX) / Ui()->Screen()->w) * Graphics()->ScreenWidth());
				float StepTime = (EndTime - StartTime) / static_cast<float>(Steps);
				float StepSize = (EndX - StartX) / static_cast<float>(Steps);

				ColorRGBA Channels = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
				pEnvelope->Eval(StartTime, Channels, c + 1);
				float PrevY = EnvelopeToScreenY(View, Channels[c]);
				for(int i = 1; i < Steps; i++)
				{
					Channels = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
					pEnvelope->Eval(StartTime + i * StepTime, Channels, c + 1);
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
			Ui()->ClipDisable();
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
				const void *pId = &pEnvelope->m_vPoints[i].m_Curvetype;
				static const char *const TYPE_NAMES[NUM_CURVETYPES] = {"N", "L", "S", "F", "M", "B"};
				const char *pTypeName = "!?";
				if(0 <= pEnvelope->m_vPoints[i].m_Curvetype && pEnvelope->m_vPoints[i].m_Curvetype < (int)std::size(TYPE_NAMES))
					pTypeName = TYPE_NAMES[pEnvelope->m_vPoints[i].m_Curvetype];

				if(CurveButton.x >= View.x)
				{
					const int ButtonResult = DoButton_Editor(pId, pTypeName, 0, &CurveButton, BUTTON_CONTEXT, "Switch curve type (N = step, L = linear, S = slow, F = fast, M = smooth, B = bezier).");
					if(ButtonResult == 1)
					{
						const int PrevCurve = pEnvelope->m_vPoints[i].m_Curvetype;
						const int Direction = Input()->ShiftIsPressed() ? -1 : 1;
						pEnvelope->m_vPoints[i].m_Curvetype = (pEnvelope->m_vPoints[i].m_Curvetype + Direction + NUM_CURVETYPES) % NUM_CURVETYPES;

						m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionEnvelopeEditPoint>(this,
							m_SelectedEnvelope, i, 0, CEditorActionEnvelopeEditPoint::EEditType::CURVE_TYPE, PrevCurve, pEnvelope->m_vPoints[i].m_Curvetype));
						m_Map.OnModify();
					}
					else if(ButtonResult == 2)
					{
						m_PopupEnvelopeSelectedPoint = i;
						static SPopupMenuId s_PopupCurvetypeId;
						Ui()->DoPopupMenu(&s_PopupCurvetypeId, Ui()->MouseX(), Ui()->MouseY(), 80, NUM_CURVETYPES * 14.0f + 10.0f, this, PopupEnvelopeCurvetype);
					}
				}
			}
		}

		// render colorbar
		if(ShowColorBar)
		{
			Ui()->ClipEnable(&ColorBar);

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
			Ui()->ClipDisable();
		}

		// render handles
		if(CurrentEnvelopeSwitched)
		{
			DeselectEnvPoints();
			m_ResetZoomEnvelope = true;
		}

		{
			static SPopupMenuId s_PopupEnvPointId;
			const auto &&ShowPopupEnvPoint = [&]() {
				Ui()->DoPopupMenu(&s_PopupEnvPointId, Ui()->MouseX(), Ui()->MouseY(), 150, 56 + (pEnvelope->GetChannels() == 4 && !IsTangentSelected() ? 16.0f : 0.0f), this, PopupEnvPoint);
			};

			if(s_Operation == EEnvelopeEditorOp::OP_NONE)
			{
				SetHotEnvelopePoint(View, pEnvelope, s_ActiveChannels);
				if(!Ui()->MouseButton(0))
					m_EnvOpTracker.Stop(false);
			}
			else
			{
				m_EnvOpTracker.Begin(s_Operation);
			}

			Ui()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			for(int c = 0; c < pEnvelope->GetChannels(); c++)
			{
				if(!(s_ActiveChannels & (1 << c)))
					continue;

				for(int i = 0; i < (int)pEnvelope->m_vPoints.size(); i++)
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

						const void *pId = &pEnvelope->m_vPoints[i].m_aValues[c];

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

						if(Ui()->CheckActiveItem(pId))
						{
							m_ShowEnvelopePreview = SHOWENV_SELECTED;

							if(s_Operation == EEnvelopeEditorOp::OP_SELECT)
							{
								float dx = s_MouseXStart - Ui()->MouseX();
								float dy = s_MouseYStart - Ui()->MouseY();

								if(dx * dx + dy * dy > 20.0f)
								{
									s_Operation = EEnvelopeEditorOp::OP_DRAG_POINT;

									if(!IsEnvPointSelected(i, c))
										SelectEnvPoint(i, c);
								}
							}

							if(s_Operation == EEnvelopeEditorOp::OP_DRAG_POINT || s_Operation == EEnvelopeEditorOp::OP_DRAG_POINT_X || s_Operation == EEnvelopeEditorOp::OP_DRAG_POINT_Y)
							{
								if(Input()->ShiftIsPressed())
								{
									if(s_Operation == EEnvelopeEditorOp::OP_DRAG_POINT || s_Operation == EEnvelopeEditorOp::OP_DRAG_POINT_Y)
									{
										s_Operation = EEnvelopeEditorOp::OP_DRAG_POINT_X;
										s_vAccurateDragValuesX.clear();
										for(auto [SelectedIndex, _] : m_vSelectedEnvelopePoints)
											s_vAccurateDragValuesX.push_back(pEnvelope->m_vPoints[SelectedIndex].m_Time);
									}
									else
									{
										float DeltaX = ScreenToEnvelopeDX(View, Ui()->MouseDeltaX()) * (Input()->ModifierIsPressed() ? 50.0f : 1000.0f);

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
									if(s_Operation == EEnvelopeEditorOp::OP_DRAG_POINT || s_Operation == EEnvelopeEditorOp::OP_DRAG_POINT_X)
									{
										s_Operation = EEnvelopeEditorOp::OP_DRAG_POINT_Y;
										s_vAccurateDragValuesY.clear();
										for(auto [SelectedIndex, SelectedChannel] : m_vSelectedEnvelopePoints)
											s_vAccurateDragValuesY.push_back(pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel]);
									}
									else
									{
										float DeltaY = ScreenToEnvelopeDY(View, Ui()->MouseDeltaY()) * (Input()->ModifierIsPressed() ? 51.2f : 1024.0f);
										for(size_t k = 0; k < m_vSelectedEnvelopePoints.size(); k++)
										{
											auto [SelectedIndex, SelectedChannel] = m_vSelectedEnvelopePoints[k];
											s_vAccurateDragValuesY[k] -= DeltaY;
											pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::round(s_vAccurateDragValuesY[k]);

											if(pEnvelope->GetChannels() == 1 || pEnvelope->GetChannels() == 4)
											{
												pEnvelope->m_vPoints[i].m_aValues[c] = clamp(pEnvelope->m_vPoints[i].m_aValues[c], 0, 1024);
												s_vAccurateDragValuesY[k] = clamp<float>(s_vAccurateDragValuesY[k], 0, 1024);
											}
										}
									}
								}
							}

							if(s_Operation == EEnvelopeEditorOp::OP_CONTEXT_MENU)
							{
								if(!Ui()->MouseButton(1))
								{
									if(m_vSelectedEnvelopePoints.size() == 1)
									{
										m_UpdateEnvPointInfo = true;
										ShowPopupEnvPoint();
									}
									else if(m_vSelectedEnvelopePoints.size() > 1)
									{
										static SPopupMenuId s_PopupEnvPointMultiId;
										Ui()->DoPopupMenu(&s_PopupEnvPointMultiId, Ui()->MouseX(), Ui()->MouseY(), 80, 22, this, PopupEnvPointMulti);
									}
									Ui()->SetActiveItem(nullptr);
									s_Operation = EEnvelopeEditorOp::OP_NONE;
								}
							}
							else if(!Ui()->MouseButton(0))
							{
								Ui()->SetActiveItem(nullptr);
								m_SelectedQuadEnvelope = -1;

								if(s_Operation == EEnvelopeEditorOp::OP_SELECT)
								{
									if(Input()->ShiftIsPressed())
										ToggleEnvPoint(i, c);
									else
										SelectEnvPoint(i, c);
								}

								s_Operation = EEnvelopeEditorOp::OP_NONE;
								m_Map.OnModify();
							}

							Graphics()->SetColor(1, 1, 1, 1);
						}
						else if(Ui()->HotItem() == pId)
						{
							if(Ui()->MouseButton(0))
							{
								Ui()->SetActiveItem(pId);
								s_Operation = EEnvelopeEditorOp::OP_SELECT;
								m_SelectedQuadEnvelope = m_SelectedEnvelope;

								s_MouseXStart = Ui()->MouseX();
								s_MouseYStart = Ui()->MouseY();
							}
							else if(Ui()->MouseButtonClicked(1))
							{
								if(Input()->ShiftIsPressed())
								{
									m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionDeleteEnvelopePoint>(this, m_SelectedEnvelope, i));
								}
								else
								{
									s_Operation = EEnvelopeEditorOp::OP_CONTEXT_MENU;
									if(!IsEnvPointSelected(i, c))
										SelectEnvPoint(i, c);
									Ui()->SetActiveItem(pId);
								}
							}

							m_ShowEnvelopePreview = SHOWENV_SELECTED;
							Graphics()->SetColor(1, 1, 1, 1);
							str_copy(m_aTooltip, "Envelope point. Left mouse to drag. Hold ctrl to be more precise. Hold shift to alter time. Shift + right-click to delete.");
							ms_pUiGotContext = pId;
						}
						else
							Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 1.0f);

						IGraphics::CQuadItem QuadItem(Final.x, Final.y, Final.w, Final.h);
						Graphics()->QuadsDrawTL(&QuadItem, 1);
					}

					// tangent handles for bezier curves
					if(i >= 0 && i < (int)pEnvelope->m_vPoints.size())
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
							const void *pId = &pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c];

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

							if(Ui()->CheckActiveItem(pId))
							{
								m_ShowEnvelopePreview = SHOWENV_SELECTED;

								if(s_Operation == EEnvelopeEditorOp::OP_SELECT)
								{
									float dx = s_MouseXStart - Ui()->MouseX();
									float dy = s_MouseYStart - Ui()->MouseY();

									if(dx * dx + dy * dy > 20.0f)
									{
										s_Operation = EEnvelopeEditorOp::OP_DRAG_POINT;

										s_vAccurateDragValuesX = {static_cast<float>(pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c])};
										s_vAccurateDragValuesY = {static_cast<float>(pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaY[c])};

										if(!IsTangentOutPointSelected(i, c))
											SelectTangentOutPoint(i, c);
									}
								}

								if(s_Operation == EEnvelopeEditorOp::OP_DRAG_POINT)
								{
									float DeltaX = ScreenToEnvelopeDX(View, Ui()->MouseDeltaX()) * (Input()->ModifierIsPressed() ? 50.0f : 1000.0f);
									float DeltaY = ScreenToEnvelopeDY(View, Ui()->MouseDeltaY()) * (Input()->ModifierIsPressed() ? 51.2f : 1024.0f);
									s_vAccurateDragValuesX[0] += DeltaX;
									s_vAccurateDragValuesY[0] -= DeltaY;

									pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c] = std::round(s_vAccurateDragValuesX[0]);
									pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaY[c] = std::round(s_vAccurateDragValuesY[0]);

									// clamp time value
									pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c] = clamp<int>(pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c], 0, f2fxt(ScreenToEnvelopeX(View, View.x + View.w)) - pEnvelope->m_vPoints[i].m_Time);
									s_vAccurateDragValuesX[0] = clamp<float>(s_vAccurateDragValuesX[0], 0, f2fxt(ScreenToEnvelopeX(View, View.x + View.w)) - pEnvelope->m_vPoints[i].m_Time);
								}

								if(s_Operation == EEnvelopeEditorOp::OP_CONTEXT_MENU)
								{
									if(!Ui()->MouseButton(1))
									{
										if(IsTangentOutPointSelected(i, c))
										{
											m_UpdateEnvPointInfo = true;
											ShowPopupEnvPoint();
										}
										Ui()->SetActiveItem(nullptr);
										s_Operation = EEnvelopeEditorOp::OP_NONE;
									}
								}
								else if(!Ui()->MouseButton(0))
								{
									Ui()->SetActiveItem(nullptr);
									m_SelectedQuadEnvelope = -1;

									if(s_Operation == EEnvelopeEditorOp::OP_SELECT)
										SelectTangentOutPoint(i, c);

									s_Operation = EEnvelopeEditorOp::OP_NONE;
									m_Map.OnModify();
								}

								Graphics()->SetColor(1, 1, 1, 1);
							}
							else if(Ui()->HotItem() == pId)
							{
								if(Ui()->MouseButton(0))
								{
									Ui()->SetActiveItem(pId);
									s_Operation = EEnvelopeEditorOp::OP_SELECT;
									m_SelectedQuadEnvelope = m_SelectedEnvelope;

									s_MouseXStart = Ui()->MouseX();
									s_MouseYStart = Ui()->MouseY();
								}
								else if(Ui()->MouseButtonClicked(1))
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
										s_Operation = EEnvelopeEditorOp::OP_CONTEXT_MENU;
										SelectTangentOutPoint(i, c);
										Ui()->SetActiveItem(pId);
									}
								}

								m_ShowEnvelopePreview = SHOWENV_SELECTED;
								Graphics()->SetColor(1, 1, 1, 1);
								str_copy(m_aTooltip, "Bezier out-tangent. Left mouse to drag. Hold ctrl to be more precise. Shift + right-click to reset.");
								ms_pUiGotContext = pId;
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
							const void *pId = &pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c];

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

							if(Ui()->CheckActiveItem(pId))
							{
								m_ShowEnvelopePreview = SHOWENV_SELECTED;

								if(s_Operation == EEnvelopeEditorOp::OP_SELECT)
								{
									float dx = s_MouseXStart - Ui()->MouseX();
									float dy = s_MouseYStart - Ui()->MouseY();

									if(dx * dx + dy * dy > 20.0f)
									{
										s_Operation = EEnvelopeEditorOp::OP_DRAG_POINT;

										s_vAccurateDragValuesX = {static_cast<float>(pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c])};
										s_vAccurateDragValuesY = {static_cast<float>(pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaY[c])};

										if(!IsTangentInPointSelected(i, c))
											SelectTangentInPoint(i, c);
									}
								}

								if(s_Operation == EEnvelopeEditorOp::OP_DRAG_POINT)
								{
									float DeltaX = ScreenToEnvelopeDX(View, Ui()->MouseDeltaX()) * (Input()->ModifierIsPressed() ? 50.0f : 1000.0f);
									float DeltaY = ScreenToEnvelopeDY(View, Ui()->MouseDeltaY()) * (Input()->ModifierIsPressed() ? 51.2f : 1024.0f);
									s_vAccurateDragValuesX[0] += DeltaX;
									s_vAccurateDragValuesY[0] -= DeltaY;

									pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c] = std::round(s_vAccurateDragValuesX[0]);
									pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaY[c] = std::round(s_vAccurateDragValuesY[0]);

									// clamp time value
									pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c] = clamp(pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c], f2fxt(ScreenToEnvelopeX(View, View.x)) - pEnvelope->m_vPoints[i].m_Time, 0);
									s_vAccurateDragValuesX[0] = clamp<float>(s_vAccurateDragValuesX[0], f2fxt(ScreenToEnvelopeX(View, View.x)) - pEnvelope->m_vPoints[i].m_Time, 0);
								}

								if(s_Operation == EEnvelopeEditorOp::OP_CONTEXT_MENU)
								{
									if(!Ui()->MouseButton(1))
									{
										if(IsTangentInPointSelected(i, c))
										{
											m_UpdateEnvPointInfo = true;
											ShowPopupEnvPoint();
										}
										Ui()->SetActiveItem(nullptr);
										s_Operation = EEnvelopeEditorOp::OP_NONE;
									}
								}
								else if(!Ui()->MouseButton(0))
								{
									Ui()->SetActiveItem(nullptr);
									m_SelectedQuadEnvelope = -1;

									if(s_Operation == EEnvelopeEditorOp::OP_SELECT)
										SelectTangentInPoint(i, c);

									s_Operation = EEnvelopeEditorOp::OP_NONE;
									m_Map.OnModify();
								}

								Graphics()->SetColor(1, 1, 1, 1);
							}
							else if(Ui()->HotItem() == pId)
							{
								if(Ui()->MouseButton(0))
								{
									Ui()->SetActiveItem(pId);
									s_Operation = EEnvelopeEditorOp::OP_SELECT;
									m_SelectedQuadEnvelope = m_SelectedEnvelope;

									s_MouseXStart = Ui()->MouseX();
									s_MouseYStart = Ui()->MouseY();
								}
								else if(Ui()->MouseButtonClicked(1))
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
										s_Operation = EEnvelopeEditorOp::OP_CONTEXT_MENU;
										SelectTangentInPoint(i, c);
										Ui()->SetActiveItem(pId);
									}
								}

								m_ShowEnvelopePreview = SHOWENV_SELECTED;
								Graphics()->SetColor(1, 1, 1, 1);
								str_copy(m_aTooltip, "Bezier in-tangent. Left mouse to drag. Hold ctrl to be more precise. Shift + right-click to reset.");
								ms_pUiGotContext = pId;
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
			Ui()->ClipDisable();
		}

		// handle scaling
		static float s_ScaleFactorX = 1.0f;
		static float s_ScaleFactorY = 1.0f;
		static float s_MidpointX = 0.0f;
		static float s_MidpointY = 0.0f;
		static std::vector<float> s_vInitialPositionsX;
		static std::vector<float> s_vInitialPositionsY;
		if(s_Operation == EEnvelopeEditorOp::OP_NONE && !s_NameInput.IsActive() && Input()->KeyIsPressed(KEY_S) && !Input()->ModifierIsPressed() && !m_vSelectedEnvelopePoints.empty())
		{
			s_Operation = EEnvelopeEditorOp::OP_SCALE;
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

		if(s_Operation == EEnvelopeEditorOp::OP_SCALE)
		{
			str_copy(m_aTooltip, "Press shift to scale the time. Press alt to scale along midpoint. Press ctrl to be more precise.");

			if(Input()->ShiftIsPressed())
			{
				s_ScaleFactorX += Ui()->MouseDeltaX() / Graphics()->ScreenWidth() * (Input()->ModifierIsPressed() ? 0.5f : 10.0f);
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
				s_ScaleFactorY -= Ui()->MouseDeltaY() / Graphics()->ScreenHeight() * (Input()->ModifierIsPressed() ? 0.5f : 10.0f);
				for(size_t k = 0; k < m_vSelectedEnvelopePoints.size(); k++)
				{
					auto [SelectedIndex, SelectedChannel] = m_vSelectedEnvelopePoints[k];
					if(Input()->AltIsPressed())
						pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::round((s_vInitialPositionsY[k] - s_MidpointY) * s_ScaleFactorY + s_MidpointY);
					else
						pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::round(s_vInitialPositionsY[k] * s_ScaleFactorY);

					if(pEnvelope->GetChannels() == 1 || pEnvelope->GetChannels() == 4)
						pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = clamp(pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel], 0, 1024);
				}
			}

			if(Ui()->MouseButton(0))
			{
				s_Operation = EEnvelopeEditorOp::OP_NONE;
				m_EnvOpTracker.Stop(false);
			}
			else if(Ui()->MouseButton(1) || Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
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
				s_Operation = EEnvelopeEditorOp::OP_NONE;
			}
		}

		// handle box selection
		if(s_Operation == EEnvelopeEditorOp::OP_BOX_SELECT)
		{
			IGraphics::CLineItem aLines[4] = {
				{s_MouseXStart, s_MouseYStart, Ui()->MouseX(), s_MouseYStart},
				{s_MouseXStart, s_MouseYStart, s_MouseXStart, Ui()->MouseY()},
				{s_MouseXStart, Ui()->MouseY(), Ui()->MouseX(), Ui()->MouseY()},
				{Ui()->MouseX(), s_MouseYStart, Ui()->MouseX(), Ui()->MouseY()}};
			Ui()->ClipEnable(&View);
			Graphics()->LinesBegin();
			Graphics()->LinesDraw(aLines, std::size(aLines));
			Graphics()->LinesEnd();
			Ui()->ClipDisable();

			if(!Ui()->MouseButton(0))
			{
				s_Operation = EEnvelopeEditorOp::OP_NONE;
				Ui()->SetActiveItem(nullptr);

				float TimeStart = ScreenToEnvelopeX(View, s_MouseXStart);
				float TimeEnd = ScreenToEnvelopeX(View, Ui()->MouseX());
				float ValueStart = ScreenToEnvelopeY(View, s_MouseYStart);
				float ValueEnd = ScreenToEnvelopeY(View, Ui()->MouseY());

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

void CEditor::RenderEditorHistory(CUIRect View)
{
	enum EHistoryType
	{
		EDITOR_HISTORY,
		ENVELOPE_HISTORY,
		SERVER_SETTINGS_HISTORY
	};

	static EHistoryType s_HistoryType = EDITOR_HISTORY;
	static int s_ActionSelectedIndex = 0;
	static CListBox s_ListBox;
	s_ListBox.SetActive(m_Dialog == DIALOG_NONE && !Ui()->IsPopupOpen());

	const bool GotSelection = s_ListBox.Active() && s_ActionSelectedIndex >= 0 && (size_t)s_ActionSelectedIndex < m_Map.m_vSettings.size();

	CUIRect ToolBar, Button, Label, List, DragBar;
	View.HSplitTop(22.0f, &DragBar, nullptr);
	DragBar.y -= 2.0f;
	DragBar.w += 2.0f;
	DragBar.h += 4.0f;
	DoEditorDragBar(View, &DragBar, EDragSide::SIDE_TOP, &m_aExtraEditorSplits[EXTRAEDITOR_HISTORY]);
	View.HSplitTop(20.0f, &ToolBar, &View);
	View.HSplitTop(2.0f, nullptr, &List);
	ToolBar.HMargin(2.0f, &ToolBar);

	CUIRect TypeButtons, HistoryTypeButton;
	const int HistoryTypeBtnSize = 70.0f;
	ToolBar.VSplitLeft(3 * HistoryTypeBtnSize, &TypeButtons, &Label);

	// history type buttons
	{
		TypeButtons.VSplitLeft(HistoryTypeBtnSize, &HistoryTypeButton, &TypeButtons);
		static int s_EditorHistoryButton = 0;
		if(DoButton_Ex(&s_EditorHistoryButton, "Editor", s_HistoryType == EDITOR_HISTORY, &HistoryTypeButton, 0, "Show map editor history.", IGraphics::CORNER_L))
		{
			s_HistoryType = EDITOR_HISTORY;
		}

		TypeButtons.VSplitLeft(HistoryTypeBtnSize, &HistoryTypeButton, &TypeButtons);
		static int s_EnvelopeEditorHistoryButton = 0;
		if(DoButton_Ex(&s_EnvelopeEditorHistoryButton, "Envelope", s_HistoryType == ENVELOPE_HISTORY, &HistoryTypeButton, 0, "Show envelope editor history.", IGraphics::CORNER_NONE))
		{
			s_HistoryType = ENVELOPE_HISTORY;
		}

		TypeButtons.VSplitLeft(HistoryTypeBtnSize, &HistoryTypeButton, &TypeButtons);
		static int s_ServerSettingsHistoryButton = 0;
		if(DoButton_Ex(&s_ServerSettingsHistoryButton, "Settings", s_HistoryType == SERVER_SETTINGS_HISTORY, &HistoryTypeButton, 0, "Show server settings editor history.", IGraphics::CORNER_R))
		{
			s_HistoryType = SERVER_SETTINGS_HISTORY;
		}
	}

	SLabelProperties InfoProps;
	InfoProps.m_MaxWidth = ToolBar.w - 60.f;
	InfoProps.m_EllipsisAtEnd = true;
	Label.VSplitLeft(8.0f, nullptr, &Label);
	Ui()->DoLabel(&Label, "Editor history. Click on an action to undo all actions above.", 10.0f, TEXTALIGN_ML, InfoProps);

	CEditorHistory *pCurrentHistory;
	if(s_HistoryType == EDITOR_HISTORY)
		pCurrentHistory = &m_EditorHistory;
	else if(s_HistoryType == ENVELOPE_HISTORY)
		pCurrentHistory = &m_EnvelopeEditorHistory;
	else if(s_HistoryType == SERVER_SETTINGS_HISTORY)
		pCurrentHistory = &m_ServerSettingsHistory;
	else
		return;

	// delete button
	ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
	ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
	static int s_DeleteButton = 0;
	if(DoButton_FontIcon(&s_DeleteButton, FONT_ICON_TRASH, (!pCurrentHistory->m_vpUndoActions.empty() || !pCurrentHistory->m_vpRedoActions.empty()) ? 0 : -1, &Button, 0, "Clear the history.", IGraphics::CORNER_ALL, 9.0f) == 1 || (GotSelection && CLineInput::GetActiveInput() == nullptr && m_Dialog == DIALOG_NONE && Ui()->ConsumeHotkey(CUi::HOTKEY_DELETE)))
	{
		pCurrentHistory->Clear();
		s_ActionSelectedIndex = 0;
	}

	// actions list
	int RedoSize = (int)pCurrentHistory->m_vpRedoActions.size();
	int UndoSize = (int)pCurrentHistory->m_vpUndoActions.size();
	s_ActionSelectedIndex = RedoSize;
	s_ListBox.DoStart(15.0f, RedoSize + UndoSize, 1, 3, s_ActionSelectedIndex, &List);

	for(int i = 0; i < RedoSize; i++)
	{
		const CListboxItem Item = s_ListBox.DoNextItem(&pCurrentHistory->m_vpRedoActions[i], s_ActionSelectedIndex >= 0 && s_ActionSelectedIndex == i);
		if(!Item.m_Visible)
			continue;

		Item.m_Rect.VMargin(5.0f, &Label);

		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_EllipsisAtEnd = true;
		TextRender()->TextColor({.5f, .5f, .5f});
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
		Ui()->DoLabel(&Label, pCurrentHistory->m_vpRedoActions[i]->DisplayText(), 10.0f, TEXTALIGN_ML, Props);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	for(int i = 0; i < UndoSize; i++)
	{
		const CListboxItem Item = s_ListBox.DoNextItem(&pCurrentHistory->m_vpUndoActions[UndoSize - i - 1], s_ActionSelectedIndex >= RedoSize && s_ActionSelectedIndex == (i + RedoSize));
		if(!Item.m_Visible)
			continue;

		Item.m_Rect.VMargin(5.0f, &Label);

		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_EllipsisAtEnd = true;
		Ui()->DoLabel(&Label, pCurrentHistory->m_vpUndoActions[UndoSize - i - 1]->DisplayText(), 10.0f, TEXTALIGN_ML, Props);
	}

	{ // Base action "Loaded map" that cannot be undone
		static int s_BaseAction;
		const CListboxItem Item = s_ListBox.DoNextItem(&s_BaseAction, s_ActionSelectedIndex == RedoSize + UndoSize);
		if(Item.m_Visible)
		{
			Item.m_Rect.VMargin(5.0f, &Label);

			Ui()->DoLabel(&Label, "Loaded map", 10.0f, TEXTALIGN_ML);
		}
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(s_ActionSelectedIndex != NewSelected)
	{
		// Figure out if we should undo or redo some actions
		// Undo everything until the selected index
		if(NewSelected > s_ActionSelectedIndex)
		{
			for(int i = 0; i < (NewSelected - s_ActionSelectedIndex); i++)
			{
				pCurrentHistory->Undo();
			}
		}
		else
		{
			for(int i = 0; i < (s_ActionSelectedIndex - NewSelected); i++)
			{
				pCurrentHistory->Redo();
			}
		}
		s_ActionSelectedIndex = NewSelected;
	}
}

void CEditor::DoEditorDragBar(CUIRect View, CUIRect *pDragBar, EDragSide Side, float *pValue, float MinValue, float MaxValue)
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
	static float s_InitialMouseX = 0.0f;
	static float s_InitialMouseOffsetX = 0.0f;

	bool IsVertical = Side == EDragSide::SIDE_TOP || Side == EDragSide::SIDE_BOTTOM;

	if(Ui()->MouseInside(pDragBar) && Ui()->HotItem() == pDragBar)
		m_CursorType = IsVertical ? CURSOR_RESIZE_V : CURSOR_RESIZE_H;

	bool Clicked;
	bool Abrupted;
	if(int Result = DoButton_DraggableEx(pDragBar, "", 8, pDragBar, &Clicked, &Abrupted, 0, "Change the size of the editor by dragging."))
	{
		if(s_Operation == OP_NONE && Result == 1)
		{
			s_InitialMouseY = Ui()->MouseY();
			s_InitialMouseOffsetY = Ui()->MouseY() - pDragBar->y;
			s_InitialMouseX = Ui()->MouseX();
			s_InitialMouseOffsetX = Ui()->MouseX() - pDragBar->x;
			s_Operation = OP_CLICKED;
		}

		if(Clicked || Abrupted)
			s_Operation = OP_NONE;

		if(s_Operation == OP_CLICKED && absolute(IsVertical ? Ui()->MouseY() - s_InitialMouseY : Ui()->MouseX() - s_InitialMouseX) > 5.0f)
			s_Operation = OP_DRAGGING;

		if(s_Operation == OP_DRAGGING)
		{
			if(Side == EDragSide::SIDE_TOP)
				*pValue = clamp(s_InitialMouseOffsetY + View.y + View.h - Ui()->MouseY(), MinValue, MaxValue);
			else if(Side == EDragSide::SIDE_RIGHT)
				*pValue = clamp(Ui()->MouseX() - s_InitialMouseOffsetX - View.x + pDragBar->w, MinValue, MaxValue);
			else if(Side == EDragSide::SIDE_BOTTOM)
				*pValue = clamp(Ui()->MouseY() - s_InitialMouseOffsetY - View.y + pDragBar->h, MinValue, MaxValue);
			else if(Side == EDragSide::SIDE_LEFT)
				*pValue = clamp(s_InitialMouseOffsetX + View.x + View.w - Ui()->MouseX(), MinValue, MaxValue);

			m_CursorType = IsVertical ? CURSOR_RESIZE_V : CURSOR_RESIZE_H;
		}
	}
}

void CEditor::RenderMenubar(CUIRect MenuBar)
{
	SPopupMenuProperties PopupProperties;
	PopupProperties.m_Corners = IGraphics::CORNER_R | IGraphics::CORNER_B;

	CUIRect FileButton;
	static int s_FileButton = 0;
	MenuBar.VSplitLeft(60.0f, &FileButton, &MenuBar);
	if(DoButton_Ex(&s_FileButton, "File", 0, &FileButton, 0, nullptr, IGraphics::CORNER_T, EditorFontSizes::MENU, TEXTALIGN_ML))
	{
		static SPopupMenuId s_PopupMenuFileId;
		Ui()->DoPopupMenu(&s_PopupMenuFileId, FileButton.x, FileButton.y + FileButton.h - 1.0f, 120.0f, 174.0f, this, PopupMenuFile, PopupProperties);
	}

	MenuBar.VSplitLeft(5.0f, nullptr, &MenuBar);

	CUIRect ToolsButton;
	static int s_ToolsButton = 0;
	MenuBar.VSplitLeft(60.0f, &ToolsButton, &MenuBar);
	if(DoButton_Ex(&s_ToolsButton, "Tools", 0, &ToolsButton, 0, nullptr, IGraphics::CORNER_T, EditorFontSizes::MENU, TEXTALIGN_ML))
	{
		static SPopupMenuId s_PopupMenuToolsId;
		Ui()->DoPopupMenu(&s_PopupMenuToolsId, ToolsButton.x, ToolsButton.y + ToolsButton.h - 1.0f, 200.0f, 64.0f, this, PopupMenuTools, PopupProperties);
	}

	MenuBar.VSplitLeft(5.0f, nullptr, &MenuBar);

	CUIRect SettingsButton;
	static int s_SettingsButton = 0;
	MenuBar.VSplitLeft(60.0f, &SettingsButton, &MenuBar);
	if(DoButton_Ex(&s_SettingsButton, "Settings", 0, &SettingsButton, 0, nullptr, IGraphics::CORNER_T, EditorFontSizes::MENU, TEXTALIGN_ML))
	{
		static SPopupMenuId s_PopupMenuEntitiesId;
		Ui()->DoPopupMenu(&s_PopupMenuEntitiesId, SettingsButton.x, SettingsButton.y + SettingsButton.h - 1.0f, 210.0f, 120.0f, this, PopupMenuSettings, PopupProperties);
	}

	CUIRect ChangedIndicator, Info, Help, Close;
	MenuBar.VSplitLeft(5.0f, nullptr, &MenuBar);
	MenuBar.VSplitLeft(MenuBar.h, &ChangedIndicator, &MenuBar);
	MenuBar.VSplitRight(15.0f, &MenuBar, &Close);
	MenuBar.VSplitRight(5.0f, &MenuBar, nullptr);
	MenuBar.VSplitRight(15.0f, &MenuBar, &Help);
	MenuBar.VSplitRight(5.0f, &MenuBar, nullptr);
	MenuBar.VSplitLeft(MenuBar.w * 0.6f, &MenuBar, &Info);
	MenuBar.VSplitRight(5.0f, &MenuBar, nullptr);

	if(m_Map.m_Modified)
	{
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		Ui()->DoLabel(&ChangedIndicator, FONT_ICON_CIRCLE, 8.0f, TEXTALIGN_MC);
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
	Ui()->DoLabel(&MenuBar, aBuf, 10.0f, TEXTALIGN_ML, Props);

	char aTimeStr[6];
	str_timestamp_format(aTimeStr, sizeof(aTimeStr), "%H:%M");

	str_format(aBuf, sizeof(aBuf), "X: %.1f, Y: %.1f, Z: %.1f, A: %.1f, G: %i  %s", Ui()->MouseWorldX() / 32.0f, Ui()->MouseWorldY() / 32.0f, MapView()->Zoom()->GetValue(), m_AnimateSpeed, MapView()->MapGrid()->Factor(), aTimeStr);
	Ui()->DoLabel(&Info, aBuf, 10.0f, TEXTALIGN_MR);

	static int s_HelpButton = 0;
	if(DoButton_Editor(&s_HelpButton, "?", 0, &Help, 0, "[F1] Open the DDNet Wiki page for the Map Editor in a web browser") || (Input()->KeyPress(KEY_F1) && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr))
	{
		const char *pLink = Localize("https://wiki.ddnet.org/wiki/Mapping");
		if(!Client()->ViewLink(pLink))
		{
			ShowFileDialogError("Failed to open the link '%s' in the default web browser.", pLink);
		}
	}

	static int s_CloseButton = 0;
	if(DoButton_Editor(&s_CloseButton, "×", 0, &Close, 0, "Exits from the editor"))
	{
		OnClose();
		g_Config.m_ClEditor = 0;
	}
}

void CEditor::Render()
{
	// basic start
	Graphics()->Clear(0.0f, 0.0f, 0.0f);
	CUIRect View = *Ui()->Screen();
	Ui()->MapScreen();
	m_CursorType = CURSOR_NORMAL;

	float Width = View.w;
	float Height = View.h;

	// reset tip
	str_copy(m_aTooltip, "");

	// render checker
	RenderBackground(View, m_CheckerTexture, 32.0f, 1.0f);

	CUIRect MenuBar, ModeBar, ToolBar, StatusBar, ExtraEditor, ToolBox;
	m_ShowPicker = Input()->KeyIsPressed(KEY_SPACE) && m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && m_vSelectedLayers.size() == 1;

	if(m_GuiActive)
	{
		View.HSplitTop(16.0f, &MenuBar, &View);
		View.HSplitTop(53.0f, &ToolBar, &View);
		View.VSplitLeft(m_ToolBoxWidth, &ToolBox, &View);

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
		// handle undo/redo hotkeys
		if(Input()->KeyPress(KEY_Z) && Input()->ModifierIsPressed() && !Input()->ShiftIsPressed())
			UndoLastAction();
		if((Input()->KeyPress(KEY_Y) && Input()->ModifierIsPressed()) || (Input()->KeyPress(KEY_Z) && Input()->ModifierIsPressed() && Input()->ShiftIsPressed()))
			RedoLastAction();

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

	const float BackgroundBrightness = 0.26f;
	const float BackgroundScale = 80.0f;

	if(m_GuiActive)
	{
		RenderBackground(MenuBar, IGraphics::CTextureHandle(), BackgroundScale, 0.0f);
		MenuBar.Margin(2.0f, &MenuBar);

		RenderBackground(ToolBox, g_pData->m_aImages[IMAGE_BACKGROUND_NOISE].m_Id, BackgroundScale, BackgroundBrightness);
		ToolBox.Margin(2.0f, &ToolBox);

		RenderBackground(ToolBar, g_pData->m_aImages[IMAGE_BACKGROUND_NOISE].m_Id, BackgroundScale, BackgroundBrightness);
		ToolBar.Margin(2.0f, &ToolBar);
		ToolBar.VSplitLeft(m_ToolBoxWidth, &ModeBar, &ToolBar);

		RenderBackground(StatusBar, g_pData->m_aImages[IMAGE_BACKGROUND_NOISE].m_Id, BackgroundScale, BackgroundBrightness);
		StatusBar.Margin(2.0f, &StatusBar);
	}

	// do the toolbar
	if(m_Mode == MODE_LAYERS)
		DoToolbarLayers(ToolBar);
	else if(m_Mode == MODE_IMAGES)
		DoToolbarImages(ToolBar);
	else if(m_Mode == MODE_SOUNDS)
		DoToolbarSounds(ToolBar);

	if(m_Dialog == DIALOG_NONE)
	{
		const bool ModPressed = Input()->ModifierIsPressed();
		const bool ShiftPressed = Input()->ShiftIsPressed();
		const bool AltPressed = Input()->AltIsPressed();

		if(CLineInput::GetActiveInput() == nullptr)
		{
			// ctrl+a to append map
			if(Input()->KeyPress(KEY_A) && ModPressed)
			{
				InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_MAP, "Append map", "Append", "maps", false, CallbackAppendMap, this);
			}
		}

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
			m_QuickActionSaveAs.Call();
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
		CUIRect DragBar;
		ToolBox.VSplitRight(1.0f, &ToolBox, &DragBar);
		DragBar.x -= 2.0f;
		DragBar.w += 4.0f;
		DoEditorDragBar(ToolBox, &DragBar, EDragSide::SIDE_RIGHT, &m_ToolBoxWidth);

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

	Ui()->MapScreen();

	CUIRect TooltipRect;
	if(m_GuiActive)
	{
		RenderMenubar(MenuBar);
		RenderModebar(ModeBar);
		if(!m_ShowPicker)
		{
			if(m_ActiveExtraEditor != EXTRAEDITOR_NONE)
			{
				RenderBackground(ExtraEditor, g_pData->m_aImages[IMAGE_BACKGROUND_NOISE].m_Id, BackgroundScale, BackgroundBrightness);
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
			else if(m_ActiveExtraEditor == EXTRAEDITOR_HISTORY)
			{
				RenderEditorHistory(ExtraEditor);
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
		Ui()->SetHotItem(&s_NullUiTarget);
		RenderFileDialog();
	}
	else if(m_Dialog == DIALOG_MAPSETTINGS_ERROR)
	{
		static int s_NullUiTarget = 0;
		Ui()->SetHotItem(&s_NullUiTarget);
		RenderMapSettingsErrorDialog();
	}

	if(m_PopupEventActivated)
	{
		static SPopupMenuId s_PopupEventId;
		constexpr float PopupWidth = 400.0f;
		constexpr float PopupHeight = 150.0f;
		Ui()->DoPopupMenu(&s_PopupEventId, Width / 2.0f - PopupWidth / 2.0f, Height / 2.0f - PopupHeight / 2.0f, PopupWidth, PopupHeight, this, PopupEvent);
		m_PopupEventActivated = false;
		m_PopupEventWasActivated = true;
	}

	if(m_Dialog == DIALOG_NONE && !Ui()->IsPopupHovered() && Ui()->MouseInside(&View))
	{
		// handle zoom hotkeys
		if(Input()->KeyPress(KEY_KP_MINUS))
			MapView()->Zoom()->ChangeValue(50.0f);
		if(Input()->KeyPress(KEY_KP_PLUS))
			MapView()->Zoom()->ChangeValue(-50.0f);
		if(Input()->KeyPress(KEY_KP_MULTIPLY))
			MapView()->ResetZoom();

		if(m_pBrush->IsEmpty() || !Input()->ShiftIsPressed())
		{
			if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
				MapView()->Zoom()->ChangeValue(20.0f);
			if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
				MapView()->Zoom()->ChangeValue(-20.0f);
		}
		if(!m_pBrush->IsEmpty())
		{
			const bool HasTeleTiles = std::any_of(m_pBrush->m_vpLayers.begin(), m_pBrush->m_vpLayers.end(), [](auto pLayer) {
				return pLayer->m_Type == LAYERTYPE_TILES && std::static_pointer_cast<CLayerTiles>(pLayer)->m_Tele;
			});
			if(HasTeleTiles)
				str_copy(m_aTooltip, "Use shift+mousewheel up/down to adjust the tele number. Use ctrl+f to change current tele number to the first unused number.");

			if(Input()->ShiftIsPressed())
			{
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
					AdjustBrushSpecialTiles(false, -1);
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
					AdjustBrushSpecialTiles(false, 1);
			}

			// Use ctrl+f to replace number in brush with next free
			if(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_F))
				AdjustBrushSpecialTiles(true);
		}
	}

	for(CEditorComponent &Component : m_vComponents)
		Component.OnRender(View);

	MapView()->UpdateZoom();

	// Cancel color pipette with escape before closing popup menus with escape
	if(m_ColorPipetteActive && Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
	{
		m_ColorPipetteActive = false;
	}

	Ui()->RenderPopupMenus();
	FreeDynamicPopupMenus();

	UpdateColorPipette();

	if(m_Dialog == DIALOG_NONE && !m_PopupEventActivated && Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
	{
		OnClose();
		g_Config.m_ClEditor = 0;
	}

	// The tooltip can be set in popup menus so we have to render the tooltip after the popup menus.
	if(m_GuiActive)
		RenderTooltip(TooltipRect);

	RenderMousePointer();
}

void CEditor::RenderPressedKeys(CUIRect View)
{
	if(!g_Config.m_EdShowkeys)
		return;

	Ui()->MapScreen();
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

	const char *pText = "Saving…";
	const float FontSize = 24.0f;

	Ui()->MapScreen();
	CUIRect Label, Spinner;
	View.Margin(20.0f, &View);
	View.HSplitBottom(FontSize, nullptr, &View);
	View.VSplitRight(TextRender()->TextWidth(FontSize, pText) + 2.0f, &Spinner, &Label);
	Spinner.VSplitRight(Spinner.h, nullptr, &Spinner);
	Ui()->DoLabel(&Label, pText, FontSize, TEXTALIGN_MR);
	Ui()->RenderProgressSpinner(Spinner.Center(), 8.0f);
}

void CEditor::FreeDynamicPopupMenus()
{
	auto Iterator = m_PopupMessageContexts.begin();
	while(Iterator != m_PopupMessageContexts.end())
	{
		if(!Ui()->IsPopupOpen(Iterator->second))
		{
			CUi::SMessagePopupContext *pContext = Iterator->second;
			Iterator = m_PopupMessageContexts.erase(Iterator);
			delete pContext;
		}
		else
			++Iterator;
	}
}

void CEditor::UpdateColorPipette()
{
	if(!m_ColorPipetteActive)
		return;

	static char s_PipetteScreenButton;
	if(Ui()->HotItem() == &s_PipetteScreenButton)
	{
		// Read color one pixel to the top and left as we would otherwise not read the correct
		// color due to the cursor sprite being rendered over the current mouse position.
		const int PixelX = clamp<int>(round_to_int((Ui()->MouseX() - 1.0f) / Ui()->Screen()->w * Graphics()->ScreenWidth()), 0, Graphics()->ScreenWidth() - 1);
		const int PixelY = clamp<int>(round_to_int((Ui()->MouseY() - 1.0f) / Ui()->Screen()->h * Graphics()->ScreenHeight()), 0, Graphics()->ScreenHeight() - 1);
		Graphics()->ReadPixel(ivec2(PixelX, PixelY), &m_PipetteColor);
	}

	// Simulate button overlaying the entire screen to intercept all clicks for color pipette.
	const int ButtonResult = DoButton_Editor_Common(&s_PipetteScreenButton, "", 0, Ui()->Screen(), 0, "Left click to pick a color from the screen. Right click to cancel pipette mode.");
	// Don't handle clicks if we are panning, so the pipette stays active while panning.
	// Checking m_pContainerPanned alone is not enough, as this variable is reset when
	// panning ends before this function is called.
	if(m_pContainerPanned == nullptr && m_pContainerPannedLast == nullptr)
	{
		if(ButtonResult == 1)
		{
			char aClipboard[9];
			str_format(aClipboard, sizeof(aClipboard), "%08X", m_PipetteColor.PackAlphaLast());
			Input()->SetClipboardText(aClipboard);

			// Check if any of the saved colors is equal to the picked color and
			// bring it to the front of the list instead of adding a duplicate.
			int ShiftEnd = (int)std::size(m_aSavedColors) - 1;
			for(int i = 0; i < (int)std::size(m_aSavedColors); ++i)
			{
				if(m_aSavedColors[i].Pack() == m_PipetteColor.Pack())
				{
					ShiftEnd = i;
					break;
				}
			}
			for(int i = ShiftEnd; i > 0; --i)
			{
				m_aSavedColors[i] = m_aSavedColors[i - 1];
			}
			m_aSavedColors[0] = m_PipetteColor;
		}
		if(ButtonResult > 0)
		{
			m_ColorPipetteActive = false;
		}
	}
}

void CEditor::RenderMousePointer()
{
	if(!m_ShowMousePointer)
		return;

	constexpr float CursorSize = 16.0f;

	// Cursor
	Graphics()->WrapClamp();
	Graphics()->TextureSet(m_aCursorTextures[m_CursorType]);
	Graphics()->QuadsBegin();
	if(m_CursorType == CURSOR_RESIZE_V)
	{
		Graphics()->QuadsSetRotation(pi / 2.0f);
	}
	if(ms_pUiGotContext == Ui()->HotItem())
	{
		Graphics()->SetColor(1.0f, 0.0f, 0.0f, 1.0f);
	}
	const float CursorOffset = m_CursorType == CURSOR_RESIZE_V || m_CursorType == CURSOR_RESIZE_H ? -CursorSize / 2.0f : 0.0f;
	IGraphics::CQuadItem QuadItem(Ui()->MouseX() + CursorOffset, Ui()->MouseY() + CursorOffset, CursorSize, CursorSize);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
	Graphics()->WrapNormal();

	// Pipette color
	if(m_ColorPipetteActive)
	{
		CUIRect PipetteRect = {Ui()->MouseX() + CursorSize, Ui()->MouseY() + CursorSize, 80.0f, 20.0f};
		if(PipetteRect.x + PipetteRect.w + 2.0f > Ui()->Screen()->w)
		{
			PipetteRect.x = Ui()->MouseX() - PipetteRect.w - CursorSize / 2.0f;
		}
		if(PipetteRect.y + PipetteRect.h + 2.0f > Ui()->Screen()->h)
		{
			PipetteRect.y = Ui()->MouseY() - PipetteRect.h - CursorSize / 2.0f;
		}
		PipetteRect.Draw(ColorRGBA(0.2f, 0.2f, 0.2f, 0.7f), IGraphics::CORNER_ALL, 3.0f);

		CUIRect Pipette, Label;
		PipetteRect.VSplitLeft(PipetteRect.h, &Pipette, &Label);
		Pipette.Margin(2.0f, &Pipette);
		Pipette.Draw(m_PipetteColor, IGraphics::CORNER_ALL, 3.0f);

		char aLabel[8];
		str_format(aLabel, sizeof(aLabel), "#%06X", m_PipetteColor.PackAlphaLast(false));
		Ui()->DoLabel(&Label, aLabel, 10.0f, TEXTALIGN_MC);
	}
}

void CEditor::Reset(bool CreateDefault)
{
	Ui()->ClosePopupMenus();
	m_Map.Clean();

	for(CEditorComponent &Component : m_vComponents)
		Component.OnReset();

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

	m_pContainerPanned = nullptr;
	m_pContainerPannedLast = nullptr;

	m_Map.m_Modified = false;
	m_Map.m_ModifiedAuto = false;
	m_Map.m_LastModifiedTime = -1.0f;
	m_Map.m_LastSaveTime = Client()->GlobalTime();
	m_Map.m_LastAutosaveUpdateTime = -1.0f;

	m_ShowEnvelopePreview = SHOWENV_NONE;
	m_ShiftBy = 1;

	m_ResetZoomEnvelope = true;
	m_SettingsCommandInput.Clear();

	m_EditorHistory.Clear();
	m_EnvelopeEditorHistory.Clear();
	m_ServerSettingsHistory.Clear();

	m_QuadTracker.m_pEditor = this;

	m_EnvOpTracker.m_pEditor = this;
	m_EnvOpTracker.Reset();

	m_MapSettingsCommandContext.Reset();
}

int CEditor::GetTextureUsageFlag() const
{
	return Graphics()->Uses2DTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE;
}

IGraphics::CTextureHandle CEditor::GetFrontTexture()
{
	if(!m_FrontTexture.IsValid())
		m_FrontTexture = Graphics()->LoadTexture("editor/front.png", IStorage::TYPE_ALL, GetTextureUsageFlag());
	return m_FrontTexture;
}

IGraphics::CTextureHandle CEditor::GetTeleTexture()
{
	if(!m_TeleTexture.IsValid())
		m_TeleTexture = Graphics()->LoadTexture("editor/tele.png", IStorage::TYPE_ALL, GetTextureUsageFlag());
	return m_TeleTexture;
}

IGraphics::CTextureHandle CEditor::GetSpeedupTexture()
{
	if(!m_SpeedupTexture.IsValid())
		m_SpeedupTexture = Graphics()->LoadTexture("editor/speedup.png", IStorage::TYPE_ALL, GetTextureUsageFlag());
	return m_SpeedupTexture;
}

IGraphics::CTextureHandle CEditor::GetSwitchTexture()
{
	if(!m_SwitchTexture.IsValid())
		m_SwitchTexture = Graphics()->LoadTexture("editor/switch.png", IStorage::TYPE_ALL, GetTextureUsageFlag());
	return m_SwitchTexture;
}

IGraphics::CTextureHandle CEditor::GetTuneTexture()
{
	if(!m_TuneTexture.IsValid())
		m_TuneTexture = Graphics()->LoadTexture("editor/tune.png", IStorage::TYPE_ALL, GetTextureUsageFlag());
	return m_TuneTexture;
}

IGraphics::CTextureHandle CEditor::GetEntitiesTexture()
{
	if(!m_EntitiesTexture.IsValid())
		m_EntitiesTexture = Graphics()->LoadTexture("editor/entities/DDNet.png", IStorage::TYPE_ALL, GetTextureUsageFlag());
	return m_EntitiesTexture;
}

void CEditor::Init()
{
	m_pInput = Kernel()->RequestInterface<IInput>();
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	m_pConfig = m_pConfigManager->Values();
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
	m_ZoomEnvelopeX.OnInit(this);
	m_ZoomEnvelopeY.OnInit(this);
	m_Map.m_pEditor = this;

	m_vComponents.emplace_back(m_MapView);
	m_vComponents.emplace_back(m_MapSettingsBackend);
	m_vComponents.emplace_back(m_LayerSelector);
	m_vComponents.emplace_back(m_Prompt);
	for(CEditorComponent &Component : m_vComponents)
		Component.OnInit(this);

	m_CheckerTexture = Graphics()->LoadTexture("editor/checker.png", IStorage::TYPE_ALL);
	m_aCursorTextures[CURSOR_NORMAL] = Graphics()->LoadTexture("editor/cursor.png", IStorage::TYPE_ALL);
	m_aCursorTextures[CURSOR_RESIZE_H] = Graphics()->LoadTexture("editor/cursor_resize.png", IStorage::TYPE_ALL);
	m_aCursorTextures[CURSOR_RESIZE_V] = m_aCursorTextures[CURSOR_RESIZE_H];

	m_pTilesetPicker = std::make_shared<CLayerTiles>(this, 16, 16);
	m_pTilesetPicker->MakePalette();
	m_pTilesetPicker->m_Readonly = true;

	m_pQuadsetPicker = std::make_shared<CLayerQuads>(this);
	m_pQuadsetPicker->NewQuad(0, 0, 64, 64);
	m_pQuadsetPicker->m_Readonly = true;

	m_pBrush = std::make_shared<CLayerGroup>();
	m_pBrush->m_pMap = &m_Map;

	Reset(false);
}

void CEditor::PlaceBorderTiles()
{
	std::shared_ptr<CLayerTiles> pT = std::static_pointer_cast<CLayerTiles>(GetSelectedLayerType(0, LAYERTYPE_TILES));

	for(int i = 0; i < pT->m_Width * pT->m_Height; ++i)
	{
		if(i % pT->m_Width < 2 || i % pT->m_Width > pT->m_Width - 3 || i < pT->m_Width * 2 || i > pT->m_Width * (pT->m_Height - 2))
		{
			int x = i % pT->m_Width;
			int y = i / pT->m_Width;

			CTile Current = pT->m_pTiles[i];
			Current.m_Index = 1;
			pT->SetTile(x, y, Current);
		}
	}

	int GameGroupIndex = std::find(m_Map.m_vpGroups.begin(), m_Map.m_vpGroups.end(), m_Map.m_pGameGroup) - m_Map.m_vpGroups.begin();
	m_EditorHistory.RecordAction(std::make_shared<CEditorBrushDrawAction>(this, GameGroupIndex), "Tool 'Make borders'");

	m_Map.OnModify();
}

void CEditor::HandleCursorMovement()
{
	const vec2 UpdatedMousePos = Ui()->UpdatedMousePos();
	const vec2 UpdatedMouseDelta = Ui()->UpdatedMouseDelta();

	// fix correct world x and y
	const std::shared_ptr<CLayerGroup> pGroup = GetSelectedGroup();
	if(pGroup)
	{
		float aPoints[4];
		pGroup->Mapping(aPoints);

		float WorldWidth = aPoints[2] - aPoints[0];
		float WorldHeight = aPoints[3] - aPoints[1];

		m_MouseWorldScale = WorldWidth / Graphics()->WindowWidth();

		m_MouseWorldPos.x = aPoints[0] + WorldWidth * (UpdatedMousePos.x / Graphics()->WindowWidth());
		m_MouseWorldPos.y = aPoints[1] + WorldHeight * (UpdatedMousePos.y / Graphics()->WindowHeight());
		m_MouseDeltaWorld.x = UpdatedMouseDelta.x * (WorldWidth / Graphics()->WindowWidth());
		m_MouseDeltaWorld.y = UpdatedMouseDelta.y * (WorldHeight / Graphics()->WindowHeight());
	}
	else
	{
		m_MouseWorldPos = vec2(-1.0f, -1.0f);
		m_MouseDeltaWorld = vec2(0.0f, 0.0f);
	}

	m_MouseWorldNoParaPos = vec2(-1.0f, -1.0f);
	for(const std::shared_ptr<CLayerGroup> &pGameGroup : m_Map.m_vpGroups)
	{
		if(!pGameGroup->m_GameGroup)
			continue;

		float aPoints[4];
		pGameGroup->Mapping(aPoints);

		float WorldWidth = aPoints[2] - aPoints[0];
		float WorldHeight = aPoints[3] - aPoints[1];

		m_MouseWorldNoParaPos.x = aPoints[0] + WorldWidth * (UpdatedMousePos.x / Graphics()->WindowWidth());
		m_MouseWorldNoParaPos.y = aPoints[1] + WorldHeight * (UpdatedMousePos.y / Graphics()->WindowHeight());
	}

	OnMouseMove(UpdatedMousePos);
}

void CEditor::OnMouseMove(vec2 MousePos)
{
	m_vHoverTiles.clear();
	for(size_t g = 0; g < m_Map.m_vpGroups.size(); g++)
	{
		const std::shared_ptr<CLayerGroup> pGroup = m_Map.m_vpGroups[g];
		for(size_t l = 0; l < pGroup->m_vpLayers.size(); l++)
		{
			const std::shared_ptr<CLayer> pLayer = pGroup->m_vpLayers[l];
			int LayerType = pLayer->m_Type;
			if(LayerType != LAYERTYPE_TILES &&
				LayerType != LAYERTYPE_FRONT &&
				LayerType != LAYERTYPE_TELE &&
				LayerType != LAYERTYPE_SPEEDUP &&
				LayerType != LAYERTYPE_SWITCH &&
				LayerType != LAYERTYPE_TUNE)
				continue;

			std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
			pGroup->MapScreen();
			float aPoints[4];
			pGroup->Mapping(aPoints);
			float WorldWidth = aPoints[2] - aPoints[0];
			float WorldHeight = aPoints[3] - aPoints[1];
			CUIRect Rect;
			Rect.x = aPoints[0] + WorldWidth * (MousePos.x / Graphics()->WindowWidth());
			Rect.y = aPoints[1] + WorldHeight * (MousePos.y / Graphics()->WindowHeight());
			Rect.w = 0;
			Rect.h = 0;
			RECTi r;
			pTiles->Convert(Rect, &r);
			pTiles->Clamp(&r);
			int x = r.x;
			int y = r.y;

			if(x < 0 || x >= pTiles->m_Width)
				continue;
			if(y < 0 || y >= pTiles->m_Height)
				continue;
			CTile Tile = pTiles->GetTile(x, y);
			if(Tile.m_Index)
				m_vHoverTiles.emplace_back(
					g, l, x, y, Tile);
		}
	}
	Ui()->MapScreen();
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
	if(!pJob->Done())
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
	if(Client()->RconAuthed() && g_Config.m_EdAutoMapReload)
	{
		CServerInfo CurrentServerInfo;
		Client()->GetServerInfo(&CurrentServerInfo);

		NETADDR pAddr = Client()->ServerAddress();
		char aAddrStr[NETADDR_MAXSTRSIZE];
		net_addr_str(&Client()->ServerAddress(), aAddrStr, sizeof(aAddrStr), true);

		// and if we're on a local address
		bool IsLocalAddress = false;
		if(pAddr.ip[0] == 127 || pAddr.ip[0] == 10 || (pAddr.ip[0] == 192 && pAddr.ip[1] == 168) || (pAddr.ip[0] == 172 && (pAddr.ip[1] >= 16 && pAddr.ip[1] <= 31)))
			IsLocalAddress = true;

		if(str_startswith(aAddrStr, "[fe80:") || str_startswith(aAddrStr, "[::1"))
			IsLocalAddress = true;

		if(IsLocalAddress)
		{
			char aMapName[128];
			IStorage::StripPathAndExtension(pJob->GetRealFileName(), aMapName, sizeof(aMapName));
			if(!str_comp(aMapName, CurrentServerInfo.m_aMap))
				Client()->Rcon("hot_reload");
		}
	}
}

void CEditor::OnUpdate()
{
	CUIElementBase::Init(Ui()); // update static pointer because game and editor use separate UI

	if(!m_EditorWasUsedBefore)
	{
		m_EditorWasUsedBefore = true;
		Reset();
	}

	m_pContainerPannedLast = m_pContainerPanned;

	// handle mouse movement
	vec2 CursorRel = vec2(0.0f, 0.0f);
	IInput::ECursorType CursorType = Input()->CursorRelative(&CursorRel.x, &CursorRel.y);
	if(CursorType != IInput::CURSOR_NONE)
	{
		Ui()->ConvertMouseMove(&CursorRel.x, &CursorRel.y, CursorType);
		Ui()->OnCursorMove(CursorRel.x, CursorRel.y);
	}

	// handle key presses
	Input()->ConsumeEvents([&](const IInput::CEvent &Event) {
		for(CEditorComponent &Component : m_vComponents)
		{
			// Events with flag `FLAG_RELEASE` must always be forwarded to all components so keys being
			// released can be handled in all components also after some components have been disabled.
			if(Component.OnInput(Event) && (Event.m_Flags & ~IInput::FLAG_RELEASE) != 0)
				return;
		}
		Ui()->OnInput(Event);
	});

	HandleCursorMovement();
	HandleAutosave();
	HandleWriterFinishJobs();

	for(CEditorComponent &Component : m_vComponents)
		Component.OnUpdate();
}

void CEditor::OnRender()
{
	Ui()->ResetMouseSlow();

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
	Ui()->StartCheck();

	Ui()->Update(m_MouseWorldPos);

	Render();

	m_MouseDeltaWorld = vec2(0.0f, 0.0f);

	if(Input()->KeyPress(KEY_F10))
	{
		Graphics()->TakeScreenshot(nullptr);
		m_ShowMousePointer = true;
	}

	if(g_Config.m_Debug)
		Ui()->DebugRender(2.0f, Ui()->Screen()->h - 27.0f);

	Ui()->FinishCheck();
	Ui()->ClearHotkeys();
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
	Ui()->OnWindowResize();
}

void CEditor::OnClose()
{
	m_ColorPipetteActive = false;

	if(m_ToolbarPreviewSound >= 0 && Sound()->IsPlaying(m_ToolbarPreviewSound))
		Sound()->Pause(m_ToolbarPreviewSound);
	if(m_FilePreviewSound >= 0 && Sound()->IsPlaying(m_FilePreviewSound))
		Sound()->Pause(m_FilePreviewSound);
}

void CEditor::OnDialogClose()
{
	Graphics()->UnloadTexture(&m_FilePreviewImage);
	Sound()->UnloadSample(m_FilePreviewSound);
	m_FilePreviewSound = -1;
	m_FilePreviewState = PREVIEW_UNLOADED;
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

	MapView()->SetWorldOffset(Center);
}

bool CEditor::Save(const char *pFilename)
{
	// Check if file with this name is already being saved at the moment
	if(std::any_of(std::begin(m_WriterFinishJobs), std::end(m_WriterFinishJobs), [pFilename](const std::shared_ptr<CDataFileWriterFinishJob> &Job) { return str_comp(pFilename, Job->GetRealFileName()) == 0; }))
		return false;

	const auto &&ErrorHandler = [this](const char *pErrorMessage) {
		ShowFileDialogError("%s", pErrorMessage);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "editor/save", pErrorMessage);
	};
	return m_Map.Save(pFilename, ErrorHandler);
}

bool CEditor::HandleMapDrop(const char *pFileName, int StorageType)
{
	if(HasUnsavedData())
	{
		str_copy(m_aFileNamePending, pFileName);
		m_PopupEventType = CEditor::POPEVENT_LOADDROP;
		m_PopupEventActivated = true;
		return true;
	}
	else
	{
		return Load(pFileName, IStorage::TYPE_ALL_OR_ABSOLUTE);
	}
}

bool CEditor::Load(const char *pFileName, int StorageType)
{
	const auto &&ErrorHandler = [this](const char *pErrorMessage) {
		ShowFileDialogError("%s", pErrorMessage);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "editor/load", pErrorMessage);
	};

	Reset();
	bool Result = m_Map.Load(pFileName, StorageType, std::move(ErrorHandler));
	if(Result)
	{
		str_copy(m_aFileName, pFileName);
		SortImages();
		SelectGameLayer();

		for(CEditorComponent &Component : m_vComponents)
			Component.OnMapLoad();
	}
	else
	{
		m_aFileName[0] = 0;
	}
	return Result;
}

bool CEditor::Append(const char *pFileName, int StorageType, bool IgnoreHistory)
{
	CEditorMap NewMap;
	NewMap.m_pEditor = this;

	const auto &&ErrorHandler = [this](const char *pErrorMessage) {
		ShowFileDialogError("%s", pErrorMessage);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "editor/append", pErrorMessage);
	};
	if(!NewMap.Load(pFileName, StorageType, std::move(ErrorHandler)))
		return false;

	CEditorActionAppendMap::SPrevInfo Info{
		(int)m_Map.m_vpGroups.size(),
		(int)m_Map.m_vpImages.size(),
		(int)m_Map.m_vpSounds.size(),
		(int)m_Map.m_vpEnvelopes.size()};

	// Keep a map to check if specific indices have already been replaced to prevent
	// replacing those indices again when transfering images
	static std::map<int *, bool> s_ReplacedMap;
	static const auto &&s_ReplaceIndex = [](int ToReplace, int ReplaceWith) {
		return [ToReplace, ReplaceWith](int *pIndex) {
			if(*pIndex == ToReplace && !s_ReplacedMap[pIndex])
			{
				*pIndex = ReplaceWith;
				s_ReplacedMap[pIndex] = true;
			}
		};
	};

	const auto &&Rename = [&](const std::shared_ptr<CEditorImage> &pImage) {
		char aRenamed[IO_MAX_PATH_LENGTH];
		int DuplicateCount = 1;
		str_copy(aRenamed, pImage->m_aName);
		while(std::find_if(m_Map.m_vpImages.begin(), m_Map.m_vpImages.end(), [aRenamed](const std::shared_ptr<CEditorImage> &OtherImage) { return str_comp(OtherImage->m_aName, aRenamed) == 0; }) != m_Map.m_vpImages.end())
			str_format(aRenamed, sizeof(aRenamed), "%s (%d)", pImage->m_aName, DuplicateCount++); // Rename to "image_name (%d)"
		str_copy(pImage->m_aName, aRenamed);
	};

	// Transfer non-duplicate images
	s_ReplacedMap.clear();
	for(auto NewMapIt = NewMap.m_vpImages.begin(); NewMapIt != NewMap.m_vpImages.end(); ++NewMapIt)
	{
		auto pNewImage = *NewMapIt;
		auto NameIsTaken = [pNewImage](const std::shared_ptr<CEditorImage> &OtherImage) { return str_comp(pNewImage->m_aName, OtherImage->m_aName) == 0; };
		auto MatchInCurrentMap = std::find_if(m_Map.m_vpImages.begin(), m_Map.m_vpImages.end(), NameIsTaken);

		const bool IsDuplicate = MatchInCurrentMap != m_Map.m_vpImages.end();
		const int IndexToReplace = NewMapIt - NewMap.m_vpImages.begin();

		if(IsDuplicate)
		{
			// Check for image data
			const bool ImageDataEquals = (*MatchInCurrentMap)->DataEquals(*pNewImage);

			if(ImageDataEquals)
			{
				const int IndexToReplaceWith = MatchInCurrentMap - m_Map.m_vpImages.begin();

				dbg_msg("editor", "map already contains image %s with the same data, removing duplicate", pNewImage->m_aName);

				// In the new map, replace the index of the duplicate image to the index of the same in the current map.
				NewMap.ModifyImageIndex(s_ReplaceIndex(IndexToReplace, IndexToReplaceWith));
			}
			else
			{
				// Rename image and add it
				Rename(pNewImage);

				dbg_msg("editor", "map already contains image %s but contents of appended image is different. Renaming to %s", (*MatchInCurrentMap)->m_aName, pNewImage->m_aName);

				NewMap.ModifyImageIndex(s_ReplaceIndex(IndexToReplace, m_Map.m_vpImages.size()));
				m_Map.m_vpImages.push_back(pNewImage);
			}
		}
		else
		{
			NewMap.ModifyImageIndex(s_ReplaceIndex(IndexToReplace, m_Map.m_vpImages.size()));
			m_Map.m_vpImages.push_back(pNewImage);
		}
	}
	NewMap.m_vpImages.clear();

	// modify indices
	static const auto &&s_ModifyAddIndex = [](int AddAmount) {
		return [AddAmount](int *pIndex) {
			if(*pIndex >= 0)
				*pIndex += AddAmount;
		};
	};

	NewMap.ModifySoundIndex(s_ModifyAddIndex(m_Map.m_vpSounds.size()));
	NewMap.ModifyEnvelopeIndex(s_ModifyAddIndex(m_Map.m_vpEnvelopes.size()));

	// transfer sounds
	for(const auto &pSound : NewMap.m_vpSounds)
		m_Map.m_vpSounds.push_back(pSound);
	NewMap.m_vpSounds.clear();

	// transfer envelopes
	for(const auto &pEnvelope : NewMap.m_vpEnvelopes)
		m_Map.m_vpEnvelopes.push_back(pEnvelope);
	NewMap.m_vpEnvelopes.clear();

	// transfer groups
	for(const auto &pGroup : NewMap.m_vpGroups)
	{
		if(pGroup != NewMap.m_pGameGroup)
		{
			pGroup->m_pMap = &m_Map;
			m_Map.m_vpGroups.push_back(pGroup);
		}
	}
	NewMap.m_vpGroups.clear();

	// transfer server settings
	for(const auto &pSetting : NewMap.m_vSettings)
	{
		// Check if setting already exists
		bool AlreadyExists = false;
		for(const auto &pExistingSetting : m_Map.m_vSettings)
		{
			if(!str_comp(pExistingSetting.m_aCommand, pSetting.m_aCommand))
				AlreadyExists = true;
		}
		if(!AlreadyExists)
			m_Map.m_vSettings.push_back(pSetting);
	}

	NewMap.m_vSettings.clear();

	auto IndexMap = SortImages();

	if(!IgnoreHistory)
		m_EditorHistory.RecordAction(std::make_shared<CEditorActionAppendMap>(this, pFileName, Info, IndexMap));

	// all done \o/
	return true;
}

void CEditor::UndoLastAction()
{
	if(m_ActiveExtraEditor == EXTRAEDITOR_SERVER_SETTINGS)
		m_ServerSettingsHistory.Undo();
	else if(m_ActiveExtraEditor == EXTRAEDITOR_ENVELOPES)
		m_EnvelopeEditorHistory.Undo();
	else
		m_EditorHistory.Undo();
}

void CEditor::RedoLastAction()
{
	if(m_ActiveExtraEditor == EXTRAEDITOR_SERVER_SETTINGS)
		m_ServerSettingsHistory.Redo();
	else if(m_ActiveExtraEditor == EXTRAEDITOR_ENVELOPES)
		m_EnvelopeEditorHistory.Redo();
	else
		m_EditorHistory.Redo();
}

void CEditor::AdjustBrushSpecialTiles(bool UseNextFree, int Adjust)
{
	// Adjust m_Number field of tune, switch and tele tiles by `Adjust` if `UseNextFree` is false
	// If true, then use the next free number instead

	auto &&AdjustNumber = [Adjust](unsigned char &Number) {
		Number = ((Number + Adjust) - 1 + 255) % 255 + 1;
	};

	for(auto &pLayer : m_pBrush->m_vpLayers)
	{
		if(pLayer->m_Type != LAYERTYPE_TILES)
			continue;

		std::shared_ptr<CLayerTiles> pLayerTiles = std::static_pointer_cast<CLayerTiles>(pLayer);

		// Only handle tele, switch and tune layers
		if(pLayerTiles->m_Tele)
		{
			std::shared_ptr<CLayerTele> pTeleLayer = std::static_pointer_cast<CLayerTele>(pLayer);
			for(int y = 0; y < pTeleLayer->m_Height; y++)
			{
				for(int x = 0; x < pTeleLayer->m_Width; x++)
				{
					int i = y * pTeleLayer->m_Width + x;
					if(!IsValidTeleTile(pTeleLayer->m_pTiles[i].m_Index) || (!UseNextFree && !pTeleLayer->m_pTeleTile[i].m_Number))
						continue;

					if(UseNextFree)
					{
						pTeleLayer->m_pTeleTile[i].m_Number = FindNextFreeTeleNumber(pTeleLayer->m_pTiles[i].m_Index);
					}
					else
						AdjustNumber(pTeleLayer->m_pTeleTile[i].m_Number);

					if(IsTeleTileNumberUsedAny(pTeleLayer->m_pTiles[i].m_Index) &&
						m_TeleNumbers[pTeleLayer->m_pTiles[i].m_Index] != pTeleLayer->m_pTeleTile[i].m_Number)
					{
						if(!UseNextFree && Adjust == 0)
							pTeleLayer->m_pTeleTile[i].m_Number = m_TeleNumbers[pTeleLayer->m_pTiles[i].m_Index];
					}
				}
			}
		}
		else if(pLayerTiles->m_Tune)
		{
			if(!UseNextFree)
			{
				std::shared_ptr<CLayerTune> pTuneLayer = std::static_pointer_cast<CLayerTune>(pLayer);
				for(int y = 0; y < pTuneLayer->m_Height; y++)
				{
					for(int x = 0; x < pTuneLayer->m_Width; x++)
					{
						int i = y * pTuneLayer->m_Width + x;
						if(!IsValidTuneTile(pTuneLayer->m_pTiles[i].m_Index) || !pTuneLayer->m_pTuneTile[i].m_Number)
							continue;

						AdjustNumber(pTuneLayer->m_pTuneTile[i].m_Number);
					}
				}
			}
		}
		else if(pLayerTiles->m_Switch)
		{
			int NextFreeNumber = FindNextFreeSwitchNumber();

			std::shared_ptr<CLayerSwitch> pSwitchLayer = std::static_pointer_cast<CLayerSwitch>(pLayer);
			for(int y = 0; y < pSwitchLayer->m_Height; y++)
			{
				for(int x = 0; x < pSwitchLayer->m_Width; x++)
				{
					int i = y * pSwitchLayer->m_Width + x;
					if(!IsValidSwitchTile(pSwitchLayer->m_pTiles[i].m_Index) || (!UseNextFree && !pSwitchLayer->m_pSwitchTile[i].m_Number))
						continue;

					if(UseNextFree)
						pSwitchLayer->m_pSwitchTile[i].m_Number = NextFreeNumber;
					else
						AdjustNumber(pSwitchLayer->m_pSwitchTile[i].m_Number);
				}
			}
		}
	}
}

int CEditor::FindNextFreeSwitchNumber()
{
	int Number = -1;

	for(int i = 1; i <= 255; i++)
	{
		if(!m_Map.m_pSwitchLayer->ContainsElementWithId(i))
		{
			Number = i;
			break;
		}
	}
	return Number;
}

int CEditor::FindNextFreeTeleNumber(int Index)
{
	int Number = -1;
	for(int i = 1; i <= 255; i++)
	{
		if(!m_Map.m_pTeleLayer->ContainsElementWithId(i, Index))
		{
			Number = i;
			break;
		}
	}
	return Number;
}

IEditor *CreateEditor() { return new CEditor; }
