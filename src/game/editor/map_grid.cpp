#include "map_grid.h"

#include <engine/keys.h>

#include "editor.h"

static constexpr int MIN_GRID_FACTOR = 1;
static constexpr int MAX_GRID_FACTOR = 15;

void CMapGrid::OnReset()
{
	m_GridActive = false;
	m_GridFactor = 1;
}

void CMapGrid::OnRender(CUIRect View)
{
	if(!m_GridActive)
		return;

	std::shared_ptr<CLayerGroup> pGroup = Editor()->GetSelectedGroup();
	if(pGroup)
	{
		pGroup->MapScreen();

		float aGroupPoints[4];
		pGroup->Mapping(aGroupPoints);

		const CUIRect *pScreen = Ui()->Screen();

		int LineDistance = GridLineDistance();

		int XOffset = aGroupPoints[0] / LineDistance;
		int YOffset = aGroupPoints[1] / LineDistance;
		int XGridOffset = XOffset % m_GridFactor;
		int YGridOffset = YOffset % m_GridFactor;

		Graphics()->TextureClear();
		Graphics()->LinesBegin();

		for(int i = 0; i < (int)pScreen->w; i++)
		{
			if((i + YGridOffset) % m_GridFactor == 0)
				Graphics()->SetColor(1.0f, 0.3f, 0.3f, 0.3f);
			else
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.15f);

			IGraphics::CLineItem Line = IGraphics::CLineItem(LineDistance * XOffset, LineDistance * i + LineDistance * YOffset, pScreen->w + aGroupPoints[2], LineDistance * i + LineDistance * YOffset);
			Graphics()->LinesDraw(&Line, 1);

			if((i + XGridOffset) % m_GridFactor == 0)
				Graphics()->SetColor(1.0f, 0.3f, 0.3f, 0.3f);
			else
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.15f);

			Line = IGraphics::CLineItem(LineDistance * i + LineDistance * XOffset, LineDistance * YOffset, LineDistance * i + LineDistance * XOffset, pScreen->h + aGroupPoints[3]);
			Graphics()->LinesDraw(&Line, 1);
		}
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->LinesEnd();
	}
}

int CMapGrid::GridLineDistance() const
{
	if(Editor()->MapView()->Zoom()->GetValue() <= 10.0f)
		return 4;
	else if(Editor()->MapView()->Zoom()->GetValue() <= 50.0f)
		return 8;
	else if(Editor()->MapView()->Zoom()->GetValue() <= 100.0f)
		return 16;
	else if(Editor()->MapView()->Zoom()->GetValue() <= 250.0f)
		return 32;
	else if(Editor()->MapView()->Zoom()->GetValue() <= 450.0f)
		return 64;
	else if(Editor()->MapView()->Zoom()->GetValue() <= 850.0f)
		return 128;
	else if(Editor()->MapView()->Zoom()->GetValue() <= 1550.0f)
		return 256;
	else
		return 512;
}

void CMapGrid::SnapToGrid(float &x, float &y) const
{
	const int GridDistance = GridLineDistance() * m_GridFactor;
	x = (int)((x + (x >= 0 ? 1.0f : -1.0f) * GridDistance / 2) / GridDistance) * GridDistance;
	y = (int)((y + (y >= 0 ? 1.0f : -1.0f) * GridDistance / 2) / GridDistance) * GridDistance;
}

bool CMapGrid::IsEnabled() const
{
	return m_GridActive;
}

void CMapGrid::Toggle()
{
	m_GridActive = !m_GridActive;
}

int CMapGrid::Factor() const
{
	return m_GridFactor;
}

void CMapGrid::SetFactor(int Factor)
{
	m_GridFactor = clamp(Factor, MIN_GRID_FACTOR, MAX_GRID_FACTOR);
}

void CMapGrid::DoSettingsPopup(vec2 Position)
{
	Ui()->DoPopupMenu(&m_PopupGridSettingsId, Position.x, Position.y, 120.0f, 37.0f, this, PopupGridSettings);
}

CUi::EPopupMenuFunctionResult CMapGrid::PopupGridSettings(void *pContext, CUIRect View, bool Active)
{
	CMapGrid *pMapGrid = static_cast<CMapGrid *>(pContext);

	enum
	{
		PROP_SIZE = 0,
		NUM_PROPS,
	};
	CProperty aProps[] = {
		{"Size", pMapGrid->Factor(), PROPTYPE_INT, MIN_GRID_FACTOR, MAX_GRID_FACTOR},
		{nullptr},
	};

	static int s_aIds[NUM_PROPS];
	int NewVal;
	int Prop = pMapGrid->Editor()->DoProperties(&View, aProps, s_aIds, &NewVal);

	if(Prop == PROP_SIZE)
	{
		pMapGrid->SetFactor(NewVal);
	}

	CUIRect Button;
	View.HSplitBottom(12.0f, &View, &Button);

	static char s_DefaultButton;
	if(pMapGrid->Editor()->DoButton_Ex(&s_DefaultButton, "Default", 0, &Button, 0, "Reset to normal grid size.", IGraphics::CORNER_ALL))
	{
		pMapGrid->SetFactor(1);
	}

	return CUi::POPUP_KEEP_OPEN;
}
