#include "quad_knife.h"

#include "editor.h"
#include "editor_actions.h"

bool CQuadKnife::IsActive() const
{
	return Map()->m_QuadKnifeState.m_Active;
}

void CQuadKnife::Activate(int SelectedQuad)
{
	Map()->m_QuadKnifeState.m_Active = true;
	Map()->m_QuadKnifeState.m_Count = 0;
	Map()->m_QuadKnifeState.m_SelectedQuadIndex = SelectedQuad;
}

void CQuadKnife::Deactivate()
{
	Map()->m_QuadKnifeState.m_Active = false;
	Map()->m_QuadKnifeState.m_Count = 0;
	Map()->m_QuadKnifeState.m_SelectedQuadIndex = -1;

	auto &aPoints = Map()->m_QuadKnifeState.m_aPoints;
	std::fill(std::begin(aPoints), std::end(aPoints), vec2(0.0f, 0.0f));
}

static float TriangleArea(vec2 A, vec2 B, vec2 C)
{
	return absolute(((B.x - A.x) * (C.y - A.y) - (C.x - A.x) * (B.y - A.y)) * 0.5f);
}

static bool IsInTriangle(vec2 Point, vec2 A, vec2 B, vec2 C)
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

void CQuadKnife::DoSlice()
{
	if(Editor()->m_Dialog != DIALOG_NONE || Editor()->Ui()->IsPopupOpen())
	{
		return;
	}

	int QuadIndex = Map()->m_vSelectedQuads[Map()->m_QuadKnifeState.m_SelectedQuadIndex];
	std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(Map()->SelectedLayerType(0, LAYERTYPE_QUADS));
	CQuad *pQuad = &pLayer->m_vQuads[QuadIndex];

	const bool IgnoreGrid = Editor()->Input()->AltIsPressed();
	float SnapRadius = 4.f * Editor()->m_MouseWorldScale;

	vec2 Mouse = vec2(Editor()->Ui()->MouseWorldX(), Editor()->Ui()->MouseWorldY());
	vec2 Point = Mouse;

	vec2 v[4] = {
		vec2(fx2f(pQuad->m_aPoints[0].x), fx2f(pQuad->m_aPoints[0].y)),
		vec2(fx2f(pQuad->m_aPoints[1].x), fx2f(pQuad->m_aPoints[1].y)),
		vec2(fx2f(pQuad->m_aPoints[3].x), fx2f(pQuad->m_aPoints[3].y)),
		vec2(fx2f(pQuad->m_aPoints[2].x), fx2f(pQuad->m_aPoints[2].y))};

	str_copy(Editor()->m_aTooltip, "Left click inside the quad to select an area to slice. Hold alt to ignore grid. Right click to leave knife mode.");

	if(Editor()->Ui()->MouseButtonClicked(1))
	{
		Map()->m_QuadKnifeState.m_Active = false;
		return;
	}

	// Handle snapping
	if(Editor()->MapView()->MapGrid()->IsEnabled() && !IgnoreGrid)
	{
		float CellSize = Editor()->MapView()->MapGrid()->GridLineDistance();
		vec2 OnGrid = Mouse;
		Editor()->MapView()->MapGrid()->SnapToGrid(OnGrid);

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

	if(Editor()->Ui()->MouseButtonClicked(0) && ValidPosition)
	{
		Map()->m_QuadKnifeState.m_aPoints[Map()->m_QuadKnifeState.m_Count] = Point;
		Map()->m_QuadKnifeState.m_Count++;
	}

	if(Map()->m_QuadKnifeState.m_Count == 4)
	{
		auto &aPoints = Map()->m_QuadKnifeState.m_aPoints;
		if(IsInTriangle(aPoints[3], aPoints[0], aPoints[1], aPoints[2]) ||
			IsInTriangle(aPoints[1], aPoints[0], aPoints[2], aPoints[3]))
		{
			// Fix concave order
			std::swap(aPoints[0], aPoints[3]);
			std::swap(aPoints[1], aPoints[2]);
		}

		std::swap(aPoints[2], aPoints[3]);

		CQuad *pResult = pLayer->NewQuad(64, 64, 64, 64);
		pQuad = &pLayer->m_vQuads[QuadIndex];

		for(int i = 0; i < 4; i++)
		{
			int t = IsInTriangle(aPoints[i], v[0], v[3], v[2]) ? 2 : 1;

			vec2 A = vec2(fx2f(pQuad->m_aPoints[0].x), fx2f(pQuad->m_aPoints[0].y));
			vec2 B = vec2(fx2f(pQuad->m_aPoints[3].x), fx2f(pQuad->m_aPoints[3].y));
			vec2 C = vec2(fx2f(pQuad->m_aPoints[t].x), fx2f(pQuad->m_aPoints[t].y));

			float TriArea = TriangleArea(A, B, C);
			float WeightA = TriangleArea(aPoints[i], B, C) / TriArea;
			float WeightB = TriangleArea(aPoints[i], C, A) / TriArea;
			float WeightC = TriangleArea(aPoints[i], A, B) / TriArea;

			pResult->m_aColors[i].r = (int)std::round(pQuad->m_aColors[0].r * WeightA + pQuad->m_aColors[3].r * WeightB + pQuad->m_aColors[t].r * WeightC);
			pResult->m_aColors[i].g = (int)std::round(pQuad->m_aColors[0].g * WeightA + pQuad->m_aColors[3].g * WeightB + pQuad->m_aColors[t].g * WeightC);
			pResult->m_aColors[i].b = (int)std::round(pQuad->m_aColors[0].b * WeightA + pQuad->m_aColors[3].b * WeightB + pQuad->m_aColors[t].b * WeightC);
			pResult->m_aColors[i].a = (int)std::round(pQuad->m_aColors[0].a * WeightA + pQuad->m_aColors[3].a * WeightB + pQuad->m_aColors[t].a * WeightC);

			pResult->m_aTexcoords[i].x = (int)std::round(pQuad->m_aTexcoords[0].x * WeightA + pQuad->m_aTexcoords[3].x * WeightB + pQuad->m_aTexcoords[t].x * WeightC);
			pResult->m_aTexcoords[i].y = (int)std::round(pQuad->m_aTexcoords[0].y * WeightA + pQuad->m_aTexcoords[3].y * WeightB + pQuad->m_aTexcoords[t].y * WeightC);

			pResult->m_aPoints[i].x = f2fx(aPoints[i].x);
			pResult->m_aPoints[i].y = f2fx(aPoints[i].y);
		}

		pResult->m_aPoints[4].x = ((pResult->m_aPoints[0].x + pResult->m_aPoints[3].x) / 2 + (pResult->m_aPoints[1].x + pResult->m_aPoints[2].x) / 2) / 2;
		pResult->m_aPoints[4].y = ((pResult->m_aPoints[0].y + pResult->m_aPoints[3].y) / 2 + (pResult->m_aPoints[1].y + pResult->m_aPoints[2].y) / 2) / 2;

		Map()->m_QuadKnifeState.m_Count = 0;
		Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionNewQuad>(Map(), Map()->m_SelectedGroup, Map()->m_vSelectedLayers[0]));
	}

	// Render
	Graphics()->TextureClear();
	Graphics()->LinesBegin();

	IGraphics::CLineItem aEdges[] = {
		IGraphics::CLineItem(v[0].x, v[0].y, v[1].x, v[1].y),
		IGraphics::CLineItem(v[1].x, v[1].y, v[2].x, v[2].y),
		IGraphics::CLineItem(v[2].x, v[2].y, v[3].x, v[3].y),
		IGraphics::CLineItem(v[3].x, v[3].y, v[0].x, v[0].y)};

	Graphics()->SetColor(1.f, 0.5f, 0.f, 1.f);
	Graphics()->LinesDraw(aEdges, std::size(aEdges));

	IGraphics::CLineItem aLines[4];
	int LineCount = maximum(Map()->m_QuadKnifeState.m_Count - 1, 0);

	for(int i = 0; i < LineCount; i++)
		aLines[i] = IGraphics::CLineItem(Map()->m_QuadKnifeState.m_aPoints[i], Map()->m_QuadKnifeState.m_aPoints[i + 1]);

	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	Graphics()->LinesDraw(aLines, LineCount);

	if(ValidPosition)
	{
		if(Map()->m_QuadKnifeState.m_Count > 0)
		{
			IGraphics::CLineItem LineCurrent(Point, Map()->m_QuadKnifeState.m_aPoints[Map()->m_QuadKnifeState.m_Count - 1]);
			Graphics()->LinesDraw(&LineCurrent, 1);
		}

		if(Map()->m_QuadKnifeState.m_Count == 3)
		{
			IGraphics::CLineItem LineClose(Point, Map()->m_QuadKnifeState.m_aPoints[0]);
			Graphics()->LinesDraw(&LineClose, 1);
		}
	}

	Graphics()->LinesEnd();
	Graphics()->QuadsBegin();

	IGraphics::CQuadItem aMarkers[4];

	for(int i = 0; i < Map()->m_QuadKnifeState.m_Count; i++)
		aMarkers[i] = IGraphics::CQuadItem(Map()->m_QuadKnifeState.m_aPoints[i].x, Map()->m_QuadKnifeState.m_aPoints[i].y, 5.f * Editor()->m_MouseWorldScale, 5.f * Editor()->m_MouseWorldScale);

	Graphics()->SetColor(0.f, 0.f, 1.f, 1.f);
	Graphics()->QuadsDraw(aMarkers, Map()->m_QuadKnifeState.m_Count);

	if(ValidPosition)
	{
		IGraphics::CQuadItem MarkerCurrent(Point.x, Point.y, 5.f * Editor()->m_MouseWorldScale, 5.f * Editor()->m_MouseWorldScale);
		Graphics()->QuadsDraw(&MarkerCurrent, 1);
	}

	Graphics()->QuadsEnd();
}
