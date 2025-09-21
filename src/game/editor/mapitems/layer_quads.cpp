/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "layer_quads.h"

#include <game/editor/editor.h>
#include <game/editor/editor_actions.h>

#include "image.h"

CLayerQuads::CLayerQuads(CEditor *pEditor) :
	CLayer(pEditor)
{
	m_Type = LAYERTYPE_QUADS;
	m_aName[0] = '\0';
	m_LayerQuads.m_Image = -1;
}

CLayerQuads::CLayerQuads(const CLayerQuads &Other) :
	CLayer(Other)
{
	m_LayerQuads.m_Image = Other.m_LayerQuads.m_Image;
	m_vQuads = Other.m_vQuads;
}

CLayerQuads::~CLayerQuads() = default;

void CLayerQuads::Render(bool QuadPicker)
{
	Graphics()->TextureClear();
	if(m_LayerQuads.m_Image >= 0 && (size_t)m_LayerQuads.m_Image < m_pEditor->m_Map.m_vpImages.size())
		Graphics()->TextureSet(m_pEditor->m_Map.m_vpImages[m_LayerQuads.m_Image]->m_Texture);

	Graphics()->BlendNone();
	m_pEditor->RenderMap()->ForceRenderQuads(m_vQuads.data(), m_vQuads.size(), LAYERRENDERFLAG_OPAQUE, m_pEditor);
	Graphics()->BlendNormal();
	m_pEditor->RenderMap()->ForceRenderQuads(m_vQuads.data(), m_vQuads.size(), LAYERRENDERFLAG_TRANSPARENT, m_pEditor);
}

CQuad *CLayerQuads::NewQuad(int x, int y, int Width, int Height)
{
	m_pEditor->m_Map.OnModify();

	m_vQuads.emplace_back();
	CQuad *pQuad = &m_vQuads[m_vQuads.size() - 1];

	pQuad->m_PosEnv = -1;
	pQuad->m_ColorEnv = -1;
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

int CLayerQuads::BrushGrab(std::shared_ptr<CLayerGroup> pBrush, CUIRect Rect)
{
	// create new layers
	std::shared_ptr<CLayerQuads> pGrabbed = std::make_shared<CLayerQuads>(m_pEditor);
	pGrabbed->m_LayerQuads.m_Image = m_LayerQuads.m_Image;
	pBrush->AddLayer(pGrabbed);

	for(const auto &Quad : m_vQuads)
	{
		float PointX = fx2f(Quad.m_aPoints[4].x);
		float PointY = fx2f(Quad.m_aPoints[4].y);

		if(PointX > Rect.x && PointX < Rect.x + Rect.w && PointY > Rect.y && PointY < Rect.y + Rect.h)
		{
			CQuad NewQuad = Quad;
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

void CLayerQuads::BrushPlace(std::shared_ptr<CLayer> pBrush, vec2 WorldPos)
{
	if(m_Readonly)
		return;

	std::shared_ptr<CLayerQuads> pQuadLayer = std::static_pointer_cast<CLayerQuads>(pBrush);
	std::vector<CQuad> vAddedQuads;
	for(const auto &Quad : pQuadLayer->m_vQuads)
	{
		CQuad NewQuad = Quad;
		for(auto &Point : NewQuad.m_aPoints)
		{
			Point.x += f2fx(WorldPos.x);
			Point.y += f2fx(WorldPos.y);
		}

		m_vQuads.push_back(NewQuad);
		vAddedQuads.push_back(NewQuad);
	}
	m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionQuadPlace>(m_pEditor, m_pEditor->m_SelectedGroup, m_pEditor->m_vSelectedLayers[0], vAddedQuads));
	m_pEditor->m_Map.OnModify();
}

void CLayerQuads::BrushFlipX()
{
	for(auto &Quad : m_vQuads)
	{
		std::swap(Quad.m_aPoints[0], Quad.m_aPoints[1]);
		std::swap(Quad.m_aPoints[2], Quad.m_aPoints[3]);
	}
	m_pEditor->m_Map.OnModify();
}

void CLayerQuads::BrushFlipY()
{
	for(auto &Quad : m_vQuads)
	{
		std::swap(Quad.m_aPoints[0], Quad.m_aPoints[2]);
		std::swap(Quad.m_aPoints[1], Quad.m_aPoints[3]);
	}
	m_pEditor->m_Map.OnModify();
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
			*pWidth = maximum(*pWidth, fx2f(Point.x));
			*pHeight = maximum(*pHeight, fx2f(Point.y));
		}
	}
}

CUi::EPopupMenuFunctionResult CLayerQuads::RenderProperties(CUIRect *pToolBox)
{
	CProperty aProps[] = {
		{"Image", m_LayerQuads.m_Image, PROPTYPE_IMAGE, -1, 0},
		{nullptr},
	};

	static int s_aIds[(int)ELayerQuadsProp::NUM_PROPS] = {0};
	int NewVal = 0;
	auto [State, Prop] = m_pEditor->DoPropertiesWithState<ELayerQuadsProp>(pToolBox, aProps, s_aIds, &NewVal);
	if(Prop != ELayerQuadsProp::PROP_NONE && (State == EEditState::END || State == EEditState::ONE_GO))
	{
		m_pEditor->m_Map.OnModify();
	}

	static CLayerQuadsPropTracker s_Tracker(m_pEditor);
	s_Tracker.Begin(this, Prop, State);

	if(Prop == ELayerQuadsProp::PROP_IMAGE)
	{
		if(NewVal >= 0)
			m_LayerQuads.m_Image = NewVal % m_pEditor->m_Map.m_vpImages.size();
		else
			m_LayerQuads.m_Image = -1;
	}

	s_Tracker.End(Prop, State);

	return CUi::POPUP_KEEP_OPEN;
}

void CLayerQuads::ModifyImageIndex(const FIndexModifyFunction &IndexModifyFunction)
{
	IndexModifyFunction(&m_LayerQuads.m_Image);
}

void CLayerQuads::ModifyEnvelopeIndex(const FIndexModifyFunction &IndexModifyFunction)
{
	for(auto &Quad : m_vQuads)
	{
		IndexModifyFunction(&Quad.m_PosEnv);
		IndexModifyFunction(&Quad.m_ColorEnv);
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
	m_pEditor->m_Map.OnModify();
	std::swap(m_vQuads[Index0], m_vQuads[Index1]);
	return Index1;
}

const char *CLayerQuads::TypeName() const
{
	return "quads";
}
