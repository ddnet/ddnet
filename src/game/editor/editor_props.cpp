#include "editor.h"

#include <engine/textrender.h>

#include <game/editor/mapitems/image.h>
#include <game/editor/mapitems/sound.h>

using namespace FontIcons;

const ColorRGBA CEditor::ms_DefaultPropColor = ColorRGBA(1, 1, 1, 0.5f);

int CEditor::DoProperties(CUIRect *pToolbox, CProperty *pProps, int *pIds, int *pNewVal, const std::vector<ColorRGBA> &vColors)
{
	auto Res = DoPropertiesWithState<int>(pToolbox, pProps, pIds, pNewVal, vColors);
	return Res.m_Value;
}

template<typename E>
SEditResult<E> CEditor::DoPropertiesWithState(CUIRect *pToolBox, CProperty *pProps, int *pIds, int *pNewVal, const std::vector<ColorRGBA> &vColors)
{
	int Change = -1;
	EEditState State = EEditState::NONE;

	for(int i = 0; pProps[i].m_pName; i++)
	{
		const ColorRGBA *pColor = i >= (int)vColors.size() ? &ms_DefaultPropColor : &vColors[i];

		CUIRect Slot;
		pToolBox->HSplitTop(13.0f, &Slot, pToolBox);
		CUIRect Label, Shifter;
		Slot.VSplitMid(&Label, &Shifter);
		Shifter.HMargin(1.0f, &Shifter);
		Ui()->DoLabel(&Label, pProps[i].m_pName, 10.0f, TEXTALIGN_ML);

		if(pProps[i].m_Type == PROPTYPE_INT)
		{
			CUIRect Inc, Dec;
			char aBuf[64];

			Shifter.VSplitRight(10.0f, &Shifter, &Inc);
			Shifter.VSplitLeft(10.0f, &Dec, &Shifter);
			str_format(aBuf, sizeof(aBuf), "%d", pProps[i].m_Value);
			auto NewValueRes = UiDoValueSelector((char *)&pIds[i], &Shifter, "", pProps[i].m_Value, pProps[i].m_Min, pProps[i].m_Max, 1, 1.0f, "Use left mouse button to drag and change the value. Hold shift to be more precise. Right click to edit as text.", false, false, 0, pColor);
			int NewValue = NewValueRes.m_Value;
			if(NewValue != pProps[i].m_Value || (NewValueRes.m_State != EEditState::NONE && NewValueRes.m_State != EEditState::EDITING))
			{
				*pNewVal = NewValue;
				Change = i;
				State = NewValueRes.m_State;
			}
			if(DoButton_FontIcon((char *)&pIds[i] + 1, FONT_ICON_MINUS, 0, &Dec, 0, "Decrease value.", IGraphics::CORNER_L, 7.0f))
			{
				*pNewVal = clamp(pProps[i].m_Value - 1, pProps[i].m_Min, pProps[i].m_Max);
				Change = i;
				State = EEditState::ONE_GO;
			}
			if(DoButton_FontIcon(((char *)&pIds[i]) + 2, FONT_ICON_PLUS, 0, &Inc, 0, "Increase value.", IGraphics::CORNER_R, 7.0f))
			{
				*pNewVal = clamp(pProps[i].m_Value + 1, pProps[i].m_Min, pProps[i].m_Max);
				Change = i;
				State = EEditState::ONE_GO;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_BOOL)
		{
			CUIRect No, Yes;
			Shifter.VSplitMid(&No, &Yes);
			if(DoButton_Ex(&pIds[i], "No", !pProps[i].m_Value, &No, 0, "", IGraphics::CORNER_L))
			{
				*pNewVal = 0;
				Change = i;
				State = EEditState::ONE_GO;
			}
			if(DoButton_Ex(((char *)&pIds[i]) + 1, "Yes", pProps[i].m_Value, &Yes, 0, "", IGraphics::CORNER_R))
			{
				*pNewVal = 1;
				Change = i;
				State = EEditState::ONE_GO;
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

			auto NewValueRes = UiDoValueSelector(&pIds[i], &Shifter, "", Value, pProps[i].m_Min, pProps[i].m_Max, Shift ? 1 : 45, Shift ? 1.0f : 10.0f, "Use left mouse button to drag and change the value. Hold shift to be more precise. Right click to edit as text.", false, false, 0);
			int NewValue = NewValueRes.m_Value;
			if(DoButton_FontIcon(&pIds[i] + 1, FONT_ICON_MINUS, 0, &Dec, 0, "Decrease value.", IGraphics::CORNER_L, 7.0f))
			{
				NewValue = (std::ceil((pProps[i].m_Value / (float)Step)) - 1) * Step;
				if(NewValue < 0)
					NewValue += 360;
				State = EEditState::ONE_GO;
			}
			if(DoButton_FontIcon(&pIds[i] + 2, FONT_ICON_PLUS, 0, &Inc, 0, "Increase value.", IGraphics::CORNER_R, 7.0f))
			{
				NewValue = (pProps[i].m_Value + Step) / Step * Step;
				State = EEditState::ONE_GO;
			}

			if(NewValue != pProps[i].m_Value || (NewValueRes.m_State != EEditState::NONE && NewValueRes.m_State != EEditState::EDITING))
			{
				*pNewVal = NewValue % 360;
				Change = i;
				State = NewValueRes.m_State;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_COLOR)
		{
			const auto &&SetColor = [&](ColorRGBA NewColor) {
				const int NewValue = NewColor.PackAlphaLast();
				if(NewValue != pProps[i].m_Value || m_ColorPickerPopupContext.m_State != EEditState::EDITING)
				{
					*pNewVal = NewValue;
					Change = i;
					State = m_ColorPickerPopupContext.m_State;
				}
			};
			DoColorPickerButton(&pIds[i], &Shifter, ColorRGBA::UnpackAlphaLast<ColorRGBA>(pProps[i].m_Value), SetColor);
		}
		else if(pProps[i].m_Type == PROPTYPE_IMAGE)
		{
			const char *pName;
			if(pProps[i].m_Value < 0)
				pName = "None";
			else
				pName = m_Map.m_vpImages[pProps[i].m_Value]->m_aName;

			if(DoButton_Ex(&pIds[i], pName, 0, &Shifter, 0, nullptr, IGraphics::CORNER_ALL))
				PopupSelectImageInvoke(pProps[i].m_Value, Ui()->MouseX(), Ui()->MouseY());

			int r = PopupSelectImageResult();
			if(r >= -1)
			{
				*pNewVal = r;
				Change = i;
				State = EEditState::ONE_GO;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_SHIFT)
		{
			CUIRect Left, Right, Up, Down;
			Shifter.VSplitMid(&Left, &Up, 2.0f);
			Left.VSplitLeft(10.0f, &Left, &Shifter);
			Shifter.VSplitRight(10.0f, &Shifter, &Right);
			Shifter.Draw(ColorRGBA(1, 1, 1, 0.5f), IGraphics::CORNER_NONE, 0.0f);
			Ui()->DoLabel(&Shifter, "X", 10.0f, TEXTALIGN_MC);
			Up.VSplitLeft(10.0f, &Up, &Shifter);
			Shifter.VSplitRight(10.0f, &Shifter, &Down);
			Shifter.Draw(ColorRGBA(1, 1, 1, 0.5f), IGraphics::CORNER_NONE, 0.0f);
			Ui()->DoLabel(&Shifter, "Y", 10.0f, TEXTALIGN_MC);
			if(DoButton_FontIcon(&pIds[i], FONT_ICON_MINUS, 0, &Left, 0, "Shift left.", IGraphics::CORNER_L, 7.0f))
			{
				*pNewVal = DIRECTION_LEFT;
				Change = i;
				State = EEditState::ONE_GO;
			}
			if(DoButton_FontIcon(((char *)&pIds[i]) + 3, FONT_ICON_PLUS, 0, &Right, 0, "Shift right.", IGraphics::CORNER_R, 7.0f))
			{
				*pNewVal = DIRECTION_RIGHT;
				Change = i;
				State = EEditState::ONE_GO;
			}
			if(DoButton_FontIcon(((char *)&pIds[i]) + 1, FONT_ICON_MINUS, 0, &Up, 0, "Shift up.", IGraphics::CORNER_L, 7.0f))
			{
				*pNewVal = DIRECTION_UP;
				Change = i;
				State = EEditState::ONE_GO;
			}
			if(DoButton_FontIcon(((char *)&pIds[i]) + 2, FONT_ICON_PLUS, 0, &Down, 0, "Shift down.", IGraphics::CORNER_R, 7.0f))
			{
				*pNewVal = DIRECTION_DOWN;
				Change = i;
				State = EEditState::ONE_GO;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_SOUND)
		{
			const char *pName;
			if(pProps[i].m_Value < 0)
				pName = "None";
			else
				pName = m_Map.m_vpSounds[pProps[i].m_Value]->m_aName;

			if(DoButton_Ex(&pIds[i], pName, 0, &Shifter, 0, nullptr, IGraphics::CORNER_ALL))
				PopupSelectSoundInvoke(pProps[i].m_Value, Ui()->MouseX(), Ui()->MouseY());

			int r = PopupSelectSoundResult();
			if(r >= -1)
			{
				*pNewVal = r;
				Change = i;
				State = EEditState::ONE_GO;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_AUTOMAPPER)
		{
			const char *pName;
			if(pProps[i].m_Value < 0 || pProps[i].m_Min < 0 || pProps[i].m_Min >= (int)m_Map.m_vpImages.size())
				pName = "None";
			else
				pName = m_Map.m_vpImages[pProps[i].m_Min]->m_AutoMapper.GetConfigName(pProps[i].m_Value);

			if(DoButton_Ex(&pIds[i], pName, 0, &Shifter, 0, nullptr, IGraphics::CORNER_ALL))
				PopupSelectConfigAutoMapInvoke(pProps[i].m_Value, Ui()->MouseX(), Ui()->MouseY());

			int r = PopupSelectConfigAutoMapResult();
			if(r >= -1)
			{
				*pNewVal = r;
				Change = i;
				State = EEditState::ONE_GO;
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

			auto NewValueRes = UiDoValueSelector((char *)&pIds[i], &Shifter, aBuf, CurValue, 0, m_Map.m_vpEnvelopes.size(), 1, 1.0f, "Select envelope.", false, false, IGraphics::CORNER_NONE);
			int NewVal = NewValueRes.m_Value;
			if(NewVal != CurValue || (NewValueRes.m_State != EEditState::NONE && NewValueRes.m_State != EEditState::EDITING))
			{
				*pNewVal = NewVal;
				Change = i;
				State = NewValueRes.m_State;
			}

			if(DoButton_FontIcon((char *)&pIds[i] + 1, FONT_ICON_MINUS, 0, &Dec, 0, "Select previous envelope.", IGraphics::CORNER_L, 7.0f))
			{
				*pNewVal = pProps[i].m_Value - 1;
				Change = i;
				State = EEditState::ONE_GO;
			}
			if(DoButton_FontIcon(((char *)&pIds[i]) + 2, FONT_ICON_PLUS, 0, &Inc, 0, "Select next envelope.", IGraphics::CORNER_R, 7.0f))
			{
				*pNewVal = pProps[i].m_Value + 1;
				Change = i;
				State = EEditState::ONE_GO;
			}
		}
	}

	return SEditResult<E>{State, static_cast<E>(Change)};
}

template SEditResult<ECircleShapeProp> CEditor::DoPropertiesWithState(CUIRect *, CProperty *, int *, int *, const std::vector<ColorRGBA> &);
template SEditResult<ERectangleShapeProp> CEditor::DoPropertiesWithState(CUIRect *, CProperty *, int *, int *, const std::vector<ColorRGBA> &);
template SEditResult<EGroupProp> CEditor::DoPropertiesWithState(CUIRect *, CProperty *, int *, int *, const std::vector<ColorRGBA> &);
template SEditResult<ELayerProp> CEditor::DoPropertiesWithState(CUIRect *, CProperty *, int *, int *, const std::vector<ColorRGBA> &);
template SEditResult<ELayerQuadsProp> CEditor::DoPropertiesWithState(CUIRect *, CProperty *, int *, int *, const std::vector<ColorRGBA> &);
template SEditResult<ETilesProp> CEditor::DoPropertiesWithState(CUIRect *, CProperty *, int *, int *, const std::vector<ColorRGBA> &);
template SEditResult<ETilesCommonProp> CEditor::DoPropertiesWithState(CUIRect *, CProperty *, int *, int *, const std::vector<ColorRGBA> &);
template SEditResult<ELayerSoundsProp> CEditor::DoPropertiesWithState(CUIRect *, CProperty *, int *, int *, const std::vector<ColorRGBA> &);
template SEditResult<EQuadProp> CEditor::DoPropertiesWithState(CUIRect *, CProperty *, int *, int *, const std::vector<ColorRGBA> &);
template SEditResult<EQuadPointProp> CEditor::DoPropertiesWithState(CUIRect *, CProperty *, int *, int *, const std::vector<ColorRGBA> &);
template SEditResult<ESoundProp> CEditor::DoPropertiesWithState(CUIRect *, CProperty *, int *, int *, const std::vector<ColorRGBA> &);
