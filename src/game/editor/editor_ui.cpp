#include <base/color.h>
#include <base/math.h>

#include <game/client/ui.h>
#include <game/editor/editor.h>
#include <game/editor/editor_ui.h>

void CEditor::UpdateTooltip(const void *pId, const CUIRect *pRect, const char *pToolTip)
{
	if(Ui()->MouseInside(pRect) && !pToolTip)
		str_copy(m_aTooltip, "");
	else if(Ui()->HotItem() == pId && pToolTip)
		str_copy(m_aTooltip, pToolTip);
}

ColorRGBA CEditor::GetButtonColor(const void *pId, int Checked)
{
	if(Checked < 0)
		return ColorRGBA(0, 0, 0, 0.5f);

	switch(Checked)
	{
	case EditorButtonChecked::DANGEROUS_ACTION:
		if(Ui()->HotItem() == pId)
			return ColorRGBA(1.0f, 0.0f, 0.0f, 0.75f);
		return ColorRGBA(1.0f, 0.0f, 0.0f, 0.5f);
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

int CEditor::DoButtonLogic(const void *pId, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(Ui()->MouseInside(pRect))
	{
		if(Flags & BUTTONFLAG_RIGHT)
			m_pUiGotContext = pId;
	}

	UpdateTooltip(pId, pRect, pToolTip);
	return Ui()->DoButtonLogic(pId, Checked, pRect, Flags);
}

int CEditor::DoButton_Editor(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	pRect->Draw(GetButtonColor(pId, Checked), IGraphics::CORNER_ALL, 3.0f);
	CUIRect NewRect = *pRect;
	Ui()->DoLabel(&NewRect, pText, 10.0f, TEXTALIGN_MC);
	Checked %= 2;
	return DoButtonLogic(pId, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Env(const void *pId, const char *pText, int Checked, const CUIRect *pRect, const char *pToolTip, ColorRGBA BaseColor, int Corners)
{
	float Bright = Checked ? 1.0f : 0.5f;
	float Alpha = Ui()->HotItem() == pId ? 1.0f : 0.75f;
	ColorRGBA Color = ColorRGBA(BaseColor.r * Bright, BaseColor.g * Bright, BaseColor.b * Bright, Alpha);

	pRect->Draw(Color, Corners, 3.0f);
	Ui()->DoLabel(pRect, pText, 10.0f, TEXTALIGN_MC);
	Checked %= 2;
	return DoButtonLogic(pId, Checked, pRect, BUTTONFLAG_LEFT, pToolTip);
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

	return DoButtonLogic(pId, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_FontIcon(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners, float FontSize)
{
	pRect->Draw(GetButtonColor(pId, Checked), Corners, 3.0f);

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	Ui()->DoLabel(pRect, pText, FontSize, TEXTALIGN_MC);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	return DoButtonLogic(pId, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_MenuItem(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if((Ui()->HotItem() == pId && Checked == 0) || Checked > 0)
		pRect->Draw(GetButtonColor(pId, Checked), IGraphics::CORNER_ALL, 3.0f);

	CUIRect Rect;
	pRect->VMargin(5.0f, &Rect);

	SLabelProperties Props;
	Props.m_MaxWidth = Rect.w;
	Props.m_EllipsisAtEnd = true;
	if(Checked < 0)
	{
		Props.SetColor(ColorRGBA(0.6f, 0.6f, 0.6f, 1.0f));
	}
	Ui()->DoLabel(&Rect, pText, 10.0f, TEXTALIGN_ML, Props);

	return DoButtonLogic(pId, Checked, pRect, Flags, pToolTip);
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
		if(Flags & BUTTONFLAG_RIGHT)
			m_pUiGotContext = pId;
	}

	UpdateTooltip(pId, pRect, pToolTip);
	return Ui()->DoDraggableButtonLogic(pId, Checked, pRect, pClicked, pAbrupted);
}

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
		str_copy(m_aTooltip, "Type your number. Press enter to confirm.");
		Ui()->SetActiveItem(&s_NumberInput);
		DoEditBox(&s_NumberInput, pRect, 10.0f, Corners);

		if(Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || ((Ui()->MouseButtonClicked(1) || Ui()->MouseButtonClicked(0)) && !Inside))
		{
			Current = std::clamp(s_NumberInput.GetInteger(Base), Min, Max);
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
					Current = std::clamp(Current, Min, Max);
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
	if(((Ui()->CheckActiveItem(pId) && Ui()->CheckMouseLock() && s_DidScroll) || s_pLastTextId == pId) && s_pEditing != pId)
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
