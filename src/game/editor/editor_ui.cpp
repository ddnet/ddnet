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
		return ColorRGBA(1.0f, 0.0f, 0.0f, Ui()->HotItem() == pId ? 0.75f : 0.5f);
	case EditorButtonChecked::POSITIVE_ACTION:
		return ColorRGBA(0.0f, 1.0f, 0.0f, Ui()->HotItem() == pId ? 0.75f : 0.5f);
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

bool CEditor::DoDocumentEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, const char *pLabel, editor_history::ECategory Category)
{
	if(m_pDocumentTextInput == pLineInput &&
		(m_pDocumentTextMap != Map() || m_pDocumentTextBuffer != pLineInput->GetString()))
		FinishDocumentText(true);
	const bool Changed = DoEditBox(pLineInput, pRect, FontSize);
	if(pLineInput->IsActive())
	{
		if(m_pDocumentTextInput != pLineInput)
		{
			FinishDocumentText(true);
			if(!Map()->m_DocumentHistory.Begin(pLineInput, pLabel, Category))
			{
				pLineInput->Deactivate();
				return false;
			}
			m_pDocumentTextInput = pLineInput;
			m_pDocumentTextMap = Map();
			m_pDocumentTextBuffer = pLineInput->GetString();
		}
		m_DocumentTextRendered = true;
	}
	else if(m_pDocumentTextInput == pLineInput)
		FinishDocumentText(true);
	return Changed;
}

void CEditor::FinishDocumentText(bool Accept)
{
	if(!m_pDocumentTextInput)
		return;
	if(m_pDocumentTextMap->m_DocumentHistory.Owns(m_pDocumentTextInput))
	{
		if(Accept)
			m_pDocumentTextMap->m_DocumentHistory.Complete(m_pDocumentTextInput, editor_history::EEditCompletion::VALID_BLUR);
		else
			m_pDocumentTextMap->m_DocumentHistory.Cancel(editor_history::EEditCancellation::OWNER_DESTROYED);
	}
	m_pDocumentTextInput->Deactivate();
	m_pDocumentTextInput = nullptr;
	m_pDocumentTextMap = nullptr;
	m_pDocumentTextBuffer = nullptr;
}

bool CEditor::DoClearableEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners, const char *pToolTip, const std::vector<STextColorSplit> &vColorSplits)
{
	UpdateTooltip(pLineInput, pRect, pToolTip);
	return Ui()->DoClearableEditBox(pLineInput, pRect, FontSize, Corners, vColorSplits);
}

SEditResult<int> CEditor::UiDoValueSelector(const void *pId, CUIRect *pRect, const char *pLabel, int Current, int Min, int Max, int Step, float Scale, const char *pToolTip, bool IsDegree, bool IsHex, int Corners, const ColorRGBA *pColor, bool ShowValue)
{
	// logic
	auto &Selector = m_ValueSelector;
	bool Cancelled = false;

	const bool Inside = Ui()->MouseInside(pRect);
	const int Base = IsHex ? 16 : 10;

	if(Selector.m_pPointerId == pId && Selector.m_Button >= 0 && !Ui()->MouseButton(Selector.m_Button))
	{
		Ui()->DisableMouseLock();
		if(Ui()->CheckActiveItem(pId))
		{
			Ui()->SetActiveItem(nullptr);
		}
		if(Inside && ((Selector.m_Button == 0 && !Selector.m_DidScroll && Ui()->DoDoubleClickLogic(pId)) || Selector.m_Button == 1))
		{
			Selector.m_pTextId = pId;
			Selector.m_NumberInput.SetInteger(Current, Base);
			Selector.m_NumberInput.SelectAll();
			Selector.m_Invalid = false;
			Selector.m_pInvalidId = nullptr;
		}
		Selector.m_Button = -1;
		Selector.m_pPointerId = nullptr;
	}

	if(Selector.m_pTextId == pId)
	{
		str_copy(m_aTooltip, "Type your number. Press enter to confirm.");
		Ui()->SetActiveItem(&Selector.m_NumberInput);
		DoEditBox(&Selector.m_NumberInput, pRect, 10.0f, Corners);

		const auto Value = Selector.m_NumberInput.IntegerDraft(Base);
		if(Value)
		{
			Selector.m_Invalid = false;
			Selector.m_pInvalidId = nullptr;
		}
		const bool Blur = (Ui()->MouseButtonClicked(1) || Ui()->MouseButtonClicked(0)) && !Inside;
		if(Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || Blur || m_SettleDocumentInput)
		{
			if(Value)
				Current = static_cast<int>(std::clamp<int64_t>(*Value, Min, Max));
			else
			{
				Selector.m_Invalid = true;
				Selector.m_pInvalidId = pId;
			}
			if(Value || Blur || m_SettleDocumentInput)
			{
				Cancelled = !Value;
				Ui()->DisableMouseLock();
				Ui()->SetActiveItem(nullptr);
				Selector.m_NumberInput.Deactivate();
				Selector.m_pTextId = nullptr;
			}
		}

		if(Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
		{
			Cancelled = true;
			Ui()->DisableMouseLock();
			Ui()->SetActiveItem(nullptr);
			Selector.m_NumberInput.Deactivate();
			Selector.m_pTextId = nullptr;
			Selector.m_Invalid = false;
			Selector.m_pInvalidId = nullptr;
		}
	}
	else
	{
		if(Ui()->CheckActiveItem(pId))
		{
			if(Selector.m_Button == 0 && Ui()->MouseButton(0))
			{
				Selector.m_ScrollValue += Ui()->MouseDeltaX() * (Input()->ShiftIsPressed() ? 0.05f : 1.0f);

				if(absolute(Selector.m_ScrollValue) >= Scale)
				{
					int Count = (int)(Selector.m_ScrollValue / Scale);
					Selector.m_ScrollValue = std::fmod(Selector.m_ScrollValue, Scale);
					Current += Step * Count;
					Current = std::clamp(Current, Min, Max);
					Selector.m_DidScroll = true;

					// Constrain to discrete steps
					if(Count > 0)
						Current = Current / Step * Step;
					else
						Current = std::ceil(Current / (float)Step) * Step;
				}
			}

			if(pToolTip && Selector.m_pTextId != pId)
				str_copy(m_aTooltip, pToolTip);
		}
		else if(Ui()->HotItem() == pId)
		{
			if(Ui()->MouseButton(0))
			{
				Selector.m_Button = 0;
				Selector.m_pPointerId = pId;
				Selector.m_DidScroll = false;
				Selector.m_ScrollValue = 0.0f;
				Ui()->SetActiveItem(pId);
				Ui()->EnableMouseLock(pId);
			}
			else if(Ui()->MouseButton(1))
			{
				Selector.m_Button = 1;
				Selector.m_pPointerId = pId;
				Ui()->SetActiveItem(pId);
			}

			if(pToolTip && Selector.m_pTextId != pId)
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

	if(Selector.m_Invalid && Selector.m_pInvalidId == pId)
	{
		pRect->DrawOutline(ColorRGBA(1.0f, 0.3f, 0.3f, 1.0f));
		if(Inside)
			str_copy(m_aTooltip, "Invalid number. Enter a complete integer; the document value has not changed.");
	}
	if(Inside && !Ui()->MouseButton(0) && !Ui()->MouseButton(1))
		Ui()->SetHotItem(pId);

	EEditState State = EEditState::NONE;
	if(Selector.m_pEditingId == pId)
	{
		State = EEditState::EDITING;
	}
	if(((Ui()->CheckActiveItem(pId) && Ui()->CheckMouseLock() && Selector.m_DidScroll) || Selector.m_pTextId == pId) && Selector.m_pEditingId != pId)
	{
		State = EEditState::START;
		Selector.m_pEditingId = pId;
	}
	if(!Ui()->CheckMouseLock() && Selector.m_pTextId != pId && Selector.m_pEditingId == pId)
	{
		State = Cancelled ? EEditState::CANCELLED : EEditState::END;
		Selector.m_pEditingId = nullptr;
	}

	return SEditResult<int>{State, Current};
}

void CEditor::RenderBackground(CUIRect View, IGraphics::CTextureHandle Texture, float Size, float Brightness) const
{
	Graphics()->TextureSet(Texture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Brightness, Brightness, Brightness, 1.0f);
	Graphics()->QuadsSetSubset(0, 0, View.w / Size, View.h / Size);
	IGraphics::CQuadItem QuadItem(View.x, View.y, View.w, View.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}
