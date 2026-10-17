/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "layer_quads.h"

#include "image.h"

#include <game/editor/editor.h>

#include <limits>

CLayerQuads::CLayerQuads(CEditorMap *pMap, std::uint64_t RetainedId) :
	CLayer(pMap, LAYERTYPE_QUADS, RetainedId)
{
	m_aName[0] = '\0';
	m_Image = Map()->ImageReference(-1);
}

CLayerQuads::CLayerQuads(const CLayerQuads &Other) :
	CLayer(Other),
	CLayerQuadsValues(Other)
{
	for(auto &Quad : m_vQuads)
		Quad.m_Id = Map()->AllocateObjectId();
}

CLayerQuads::~CLayerQuads() = default;

void CLayerQuads::Render(const CEditorMap *pRenderMap)
{
	if(pRenderMap->ImageIndex(m_Image) >= 0 && (size_t)pRenderMap->ImageIndex(m_Image) < pRenderMap->m_vpImages.size())
	{
		Graphics()->TextureSet(pRenderMap->m_vpImages[pRenderMap->ImageIndex(m_Image)]->m_Texture);
	}
	else
	{
		Graphics()->TextureClear();
	}

	for(const int Flags : {LAYERRENDERFLAG_OPAQUE, LAYERRENDERFLAG_TRANSPARENT})
	{
		if(Flags == LAYERRENDERFLAG_OPAQUE)
			Graphics()->BlendNone();
		else
			Graphics()->BlendNormal();
		Graphics()->TrianglesBegin();
		for(const auto &Quad : m_vQuads)
			Editor()->RenderMap()->RenderQuad(Quad.Export([&](CDocumentReference Reference) { return pRenderMap->EnvelopeIndex(Reference); }), Flags, &pRenderMap->m_EnvelopeEvaluator, 1.0f);
		Graphics()->TrianglesEnd();
	}
}

CQuadValues *CLayerQuads::NewQuad(int x, int y, int Width, int Height)
{
	Map()->OnModify();

	m_vQuads.emplace_back();
	m_vQuads.back().m_Id = Map()->AllocateObjectId();
	CQuadValues *pQuad = &m_vQuads[m_vQuads.size() - 1];

	pQuad->m_PosEnv = Map()->EnvelopeReference(-1);
	pQuad->m_ColorEnv = Map()->EnvelopeReference(-1);
	pQuad->m_PosEnvOffset = 0;
	pQuad->m_ColorEnvOffset = 0;

	Width /= 2;
	Height /= 2;
	pQuad->m_aPoints[0].x = i2fx(x - Width);
	pQuad->m_aPoints[0].y = i2fx(y - Height);
	pQuad->m_aPoints[1].x = i2fx(x + Width);
	pQuad->m_aPoints[1].y = i2fx(y - Height);
	pQuad->m_aPoints[2].x = i2fx(x - Width);
	pQuad->m_aPoints[2].y = i2fx(y + Height);
	pQuad->m_aPoints[3].x = i2fx(x + Width);
	pQuad->m_aPoints[3].y = i2fx(y + Height);

	pQuad->m_aPoints[4].x = i2fx(x); // pivot
	pQuad->m_aPoints[4].y = i2fx(y);

	pQuad->m_aTexcoords[0].x = i2fx(0);
	pQuad->m_aTexcoords[0].y = i2fx(0);

	pQuad->m_aTexcoords[1].x = i2fx(1);
	pQuad->m_aTexcoords[1].y = i2fx(0);

	pQuad->m_aTexcoords[2].x = i2fx(0);
	pQuad->m_aTexcoords[2].y = i2fx(1);

	pQuad->m_aTexcoords[3].x = i2fx(1);
	pQuad->m_aTexcoords[3].y = i2fx(1);

	std::fill(std::begin(pQuad->m_aColors), std::end(pQuad->m_aColors), CColor{255, 255, 255, 255});

	return pQuad;
}

void CLayerQuads::BrushSelecting(CUIRect Rect)
{
	Rect.DrawOutline(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
}

int CLayerQuads::BrushGrab(CLayerGroup *pBrush, CUIRect Rect)
{
	// create new layers
	std::shared_ptr<CLayerQuads> pGrabbed = std::make_shared<CLayerQuads>(pBrush->Map());
	pGrabbed->m_Image = m_Image;
	pBrush->AddLayer(pGrabbed);

	for(const auto &Quad : m_vQuads)
	{
		float PointX = fx2f(Quad.m_aPoints[4].x);
		float PointY = fx2f(Quad.m_aPoints[4].y);

		if(PointX > Rect.x && PointX < Rect.x + Rect.w && PointY > Rect.y && PointY < Rect.y + Rect.h)
		{
			CQuadValues NewQuad = Quad;
			NewQuad.m_Id = pGrabbed->Map()->AllocateObjectId();
			for(auto &Point : NewQuad.m_aPoints)
			{
				Point.x -= f2fx(Rect.x);
				Point.y -= f2fx(Rect.y);
			}

			pGrabbed->m_vQuads.push_back(NewQuad);
		}
	}

	return pGrabbed->m_vQuads.empty() ? 0 : 1;
}

void CLayerQuads::BrushPlace(CLayer *pBrush, vec2 WorldPos)
{
	if(m_Readonly)
		return;

	CLayerQuads *pQuadLayer = static_cast<CLayerQuads *>(pBrush);
	for(const auto &Quad : pQuadLayer->m_vQuads)
	{
		CQuadValues NewQuad = Quad;
		NewQuad.m_Id = Map()->AllocateObjectId();
		for(auto &Point : NewQuad.m_aPoints)
		{
			Point.x += f2fx(WorldPos.x);
			Point.y += f2fx(WorldPos.y);
		}

		m_vQuads.push_back(NewQuad);
	}
	Map()->OnModify();
}

void CLayerQuads::BrushFlipX()
{
	// calculate bounding box
	int LeftBound = std::numeric_limits<int>::max();
	int RightBound = std::numeric_limits<int>::min();
	for(auto &Quad : m_vQuads)
	{
		for(int PointId = 0; PointId < 4; ++PointId)
		{
			LeftBound = std::min(Quad.m_aPoints[PointId].x, LeftBound);
			RightBound = std::max(Quad.m_aPoints[PointId].x, RightBound);
		}
	}

	// flip box
	for(auto &Quad : m_vQuads)
	{
		for(auto &Point : Quad.m_aPoints)
		{
			Point.x = RightBound - (Point.x - LeftBound);
		}
	}
	Map()->OnModify();
}

void CLayerQuads::BrushFlipY()
{
	// calculate bounding box
	int TopBound = std::numeric_limits<int>::max();
	int BottomBound = std::numeric_limits<int>::min();
	for(auto &Quad : m_vQuads)
	{
		for(int PointId = 0; PointId < 4; ++PointId)
		{
			TopBound = std::min(Quad.m_aPoints[PointId].y, TopBound);
			BottomBound = std::max(Quad.m_aPoints[PointId].y, BottomBound);
		}
	}

	// flip box
	for(auto &Quad : m_vQuads)
	{
		for(auto &Point : Quad.m_aPoints)
		{
			Point.y = BottomBound - (Point.y - TopBound);
		}
	}
	Map()->OnModify();
}

static void Rotate(vec2 *pCenter, vec2 *pPoint, float Rotation)
{
	float x = pPoint->x - pCenter->x;
	float y = pPoint->y - pCenter->y;
	pPoint->x = x * std::cos(Rotation) - y * std::sin(Rotation) + pCenter->x;
	pPoint->y = x * std::sin(Rotation) + y * std::cos(Rotation) + pCenter->y;
}

void CLayerQuads::BrushRotate(float Amount)
{
	vec2 Center;
	GetSize(&Center.x, &Center.y);
	Center.x /= 2;
	Center.y /= 2;

	for(auto &Quad : m_vQuads)
	{
		for(auto &Point : Quad.m_aPoints)
		{
			vec2 Pos(fx2f(Point.x), fx2f(Point.y));
			Rotate(&Center, &Pos, Amount);
			Point.x = f2fx(Pos.x);
			Point.y = f2fx(Pos.y);
		}
	}
}

void CLayerQuads::GetSize(float *pWidth, float *pHeight)
{
	*pWidth = 0;
	*pHeight = 0;

	for(const auto &Quad : m_vQuads)
	{
		for(const auto &Point : Quad.m_aPoints)
		{
			*pWidth = std::max(*pWidth, fx2f(Point.x));
			*pHeight = std::max(*pHeight, fx2f(Point.y));
		}
	}
}

CUi::EPopupMenuFunctionResult CLayerQuads::RenderProperties(CUIRect *pToolBox)
{
	CProperty aProps[] = {
		{"Image", Map()->ImageIndex(m_Image), PROPTYPE_IMAGE, -1, 0},
		{nullptr},
	};

	static int s_aIds[(int)ELayerQuadsProp::NUM_PROPS] = {0};
	int NewVal = 0;
	auto [State, Prop] = Editor()->DoPropertiesWithState<ELayerQuadsProp>(pToolBox, aProps, s_aIds, &NewVal);
	if(Prop != ELayerQuadsProp::NONE && (State == EEditState::END || State == EEditState::ONE_GO))
	{
		Map()->OnModify();
	}

	if(Map()->m_DocumentHistory.BeginControl(s_aIds, "Edit quad layer", State))
	{
		Map()->m_DocumentHistory.Update(s_aIds, [&] {
			if(Prop == ELayerQuadsProp::IMAGE)
			{
				if(NewVal >= 0)
					m_Image = Map()->ImageReference(NewVal % Map()->m_vpImages.size());
				else
					m_Image = Map()->ImageReference(-1);
			}
		});
		Map()->m_DocumentHistory.EndControl(s_aIds, State);
	}

	return CUi::POPUP_KEEP_OPEN;
}

bool CLayerQuads::IsEnvelopeUsed(int EnvelopeIndex) const
{
	return std::any_of(m_vQuads.begin(), m_vQuads.end(), [&](const auto &Quad) {
		return Map()->EnvelopeIndex(Quad.m_PosEnv) == EnvelopeIndex || Map()->EnvelopeIndex(Quad.m_ColorEnv) == EnvelopeIndex;
	});
}

bool CLayerQuads::IsImageUsed(int ImageIndex) const
{
	return Map()->ImageIndex(m_Image) == ImageIndex;
}

void CLayerQuads::VisitImageReferences(const FDocumentReferenceFunction &ReferenceFunction)
{
	ReferenceFunction(m_Image);
}

void CLayerQuads::VisitEnvelopeReferences(const FDocumentReferenceFunction &ReferenceFunction)
{
	for(auto &Quad : m_vQuads)
	{
		ReferenceFunction(Quad.m_PosEnv);
		ReferenceFunction(Quad.m_ColorEnv);
	}
}

std::shared_ptr<CLayer> CLayerQuads::Duplicate() const
{
	return std::make_shared<CLayerQuads>(*this);
}

int CLayerQuads::SwapQuads(int Index0, int Index1)
{
	if(Index0 < 0 || Index0 >= (int)m_vQuads.size())
		return Index0;
	if(Index1 < 0 || Index1 >= (int)m_vQuads.size())
		return Index0;
	if(Index0 == Index1)
		return Index0;
	Map()->OnModify();
	std::swap(m_vQuads[Index0], m_vQuads[Index1]);
	return Index1;
}

const char *CLayerQuads::TypeName() const
{
	return "quads";
}

void CLayerQuads::OnAttach(CEditorMap *pMap)
{
	if(Map() != pMap)
	{
		for(auto &Element : m_vQuads)
			Element.m_Id = pMap->AllocateObjectId();
	}
	CLayer::OnAttach(pMap);
}
