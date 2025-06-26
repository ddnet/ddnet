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
	{
		return;
	}

	std::shared_ptr<CLayerGroup> pGroup = Editor()->GetSelectedGroup();
	if(!pGroup)
	{
		return;
	}

	pGroup->MapScreen();

	float aGroupPoints[4];
	pGroup->Mapping(aGroupPoints);

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	const int LineDistance = GridLineDistance();

	const int XOffset = aGroupPoints[0] / LineDistance;
	const int YOffset = aGroupPoints[1] / LineDistance;
	const int XGridOffset = XOffset % m_GridFactor;
	const int YGridOffset = YOffset % m_GridFactor;

	const int NumColumns = (int)std::ceil((ScreenX1 - ScreenX0) / LineDistance) + 1;
	const int NumRows = (int)std::ceil((ScreenY1 - ScreenY0) / LineDistance) + 1;

	Graphics()->TextureClear();

	IGraphics::CLineItemBatch LineItemBatch;
	if(m_GridFactor > 1)
	{
		Graphics()->LinesBatchBegin(&LineItemBatch);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.15f);
		for(int y = 0; y < NumRows; y++)
		{
			if((y + YGridOffset) % m_GridFactor == 0)
			{
				continue;
			}
			const float PosY = LineDistance * (y + YOffset);
			const IGraphics::CLineItem Line = IGraphics::CLineItem(ScreenX0, PosY, ScreenX1, PosY);
			Graphics()->LinesBatchDraw(&LineItemBatch, &Line, 1);
		}
		for(int x = 0; x < NumColumns; x++)
		{
			if((x + XGridOffset) % m_GridFactor == 0)
			{
				continue;
			}
			const float PosX = LineDistance * (x + XOffset);
			const IGraphics::CLineItem Line = IGraphics::CLineItem(PosX, ScreenY0, PosX, ScreenY1);
			Graphics()->LinesBatchDraw(&LineItemBatch, &Line, 1);
		}
		Graphics()->LinesBatchEnd(&LineItemBatch);
	}

	Graphics()->LinesBatchBegin(&LineItemBatch);
	Graphics()->SetColor(1.0f, 0.3f, 0.3f, 0.3f);
	for(int y = 0; y < NumRows; y++)
	{
		if((y + YGridOffset) % m_GridFactor != 0)
		{
			continue;
		}
		const float PosY = LineDistance * (y + YOffset);
		const IGraphics::CLineItem Line = IGraphics::CLineItem(ScreenX0, PosY, ScreenX1, PosY);
		Graphics()->LinesBatchDraw(&LineItemBatch, &Line, 1);
	}
	for(int x = 0; x < NumColumns; x++)
	{
		if((x + XGridOffset) % m_GridFactor != 0)
		{
			continue;
		}
		const float PosX = LineDistance * (x + XOffset);
		const IGraphics::CLineItem Line = IGraphics::CLineItem(PosX, ScreenY0, PosX, ScreenY1);
		Graphics()->LinesBatchDraw(&LineItemBatch, &Line, 1);
	}
	Graphics()->LinesBatchEnd(&LineItemBatch);
}

int CMapGrid::GridLineDistance() const
{
	const float ZoomValue = Editor()->MapView()->Zoom()->GetValue();
	if(ZoomValue <= 10.0f)
		return 4;
	else if(ZoomValue <= 50.0f)
		return 8;
	else if(ZoomValue <= 100.0f)
		return 16;
	else if(ZoomValue <= 250.0f)
		return 32;
	else if(ZoomValue <= 450.0f)
		return 64;
	else if(ZoomValue <= 850.0f)
		return 128;
	else if(ZoomValue <= 1550.0f)
		return 256;
	else
		return 512;
}

void CMapGrid::SnapToGrid(vec2 &Position) const
{
	const int GridDistance = GridLineDistance() * m_GridFactor;
	Position.x = (int)((Position.x + (Position.x >= 0 ? 1.0f : -1.0f) * GridDistance / 2) / GridDistance) * GridDistance;
	Position.y = (int)((Position.y + (Position.y >= 0 ? 1.0f : -1.0f) * GridDistance / 2) / GridDistance) * GridDistance;
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
	m_GridFactor = std::clamp(Factor, MIN_GRID_FACTOR, MAX_GRID_FACTOR);
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
	if(pMapGrid->Editor()->DoButton_Ex(&s_DefaultButton, "Default", 0, &Button, BUTTONFLAG_LEFT, "Reset to normal grid size.", IGraphics::CORNER_ALL))
	{
		pMapGrid->SetFactor(1);
	}

	return CUi::POPUP_KEEP_OPEN;
}
