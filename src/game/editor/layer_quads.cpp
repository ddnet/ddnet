/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>

#include <engine/graphics.h>

#include "editor.h"
#include <game/client/render.h>

CLayerQuads::CLayerQuads()
{
	m_Type = LAYERTYPE_QUADS;
	m_aName[0] = '\0';
	m_Image = -1;
}

CLayerQuads::CLayerQuads(const CLayerQuads &Other) :
	CLayer(Other)
{
	m_Image = Other.m_Image;
	m_vQuads = Other.m_vQuads;
}

CLayerQuads::~CLayerQuads() = default;

void CLayerQuads::Render(bool QuadPicker)
{
	Graphics()->TextureClear();
	if(m_Image >= 0 && (size_t)m_Image < m_pEditor->m_Map.m_vpImages.size())
		Graphics()->TextureSet(m_pEditor->m_Map.m_vpImages[m_Image]->m_Texture);

	Graphics()->BlendNone();
	m_pEditor->RenderTools()->ForceRenderQuads(m_vQuads.data(), m_vQuads.size(), LAYERRENDERFLAG_OPAQUE, m_pEditor->EnvelopeEval, m_pEditor);
	Graphics()->BlendNormal();
	m_pEditor->RenderTools()->ForceRenderQuads(m_vQuads.data(), m_vQuads.size(), LAYERRENDERFLAG_TRANSPARENT, m_pEditor->EnvelopeEval, m_pEditor);
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

	pQuad->m_aColors[0].r = 255;
	pQuad->m_aColors[0].g = 255;
	pQuad->m_aColors[0].b = 255;
	pQuad->m_aColors[0].a = 255;
	pQuad->m_aColors[1].r = 255;
	pQuad->m_aColors[1].g = 255;
	pQuad->m_aColors[1].b = 255;
	pQuad->m_aColors[1].a = 255;
	pQuad->m_aColors[2].r = 255;
	pQuad->m_aColors[2].g = 255;
	pQuad->m_aColors[2].b = 255;
	pQuad->m_aColors[2].a = 255;
	pQuad->m_aColors[3].r = 255;
	pQuad->m_aColors[3].g = 255;
	pQuad->m_aColors[3].b = 255;
	pQuad->m_aColors[3].a = 255;

	return pQuad;
}

void CLayerQuads::BrushSelecting(CUIRect Rect)
{
	// draw selection rectangle
	IGraphics::CLineItem Array[4] = {
		IGraphics::CLineItem(Rect.x, Rect.y, Rect.x + Rect.w, Rect.y),
		IGraphics::CLineItem(Rect.x + Rect.w, Rect.y, Rect.x + Rect.w, Rect.y + Rect.h),
		IGraphics::CLineItem(Rect.x + Rect.w, Rect.y + Rect.h, Rect.x, Rect.y + Rect.h),
		IGraphics::CLineItem(Rect.x, Rect.y + Rect.h, Rect.x, Rect.y)};
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->LinesDraw(Array, 4);
	Graphics()->LinesEnd();
}

int CLayerQuads::BrushGrab(std::shared_ptr<CLayerGroup> pBrush, CUIRect Rect)
{
	// create new layers
	std::shared_ptr<CLayerQuads> pGrabbed = std::make_shared<CLayerQuads>();
	pGrabbed->m_pEditor = m_pEditor;
	pGrabbed->m_Image = m_Image;
	pBrush->AddLayer(pGrabbed);

	//dbg_msg("", "%f %f %f %f", rect.x, rect.y, rect.w, rect.h);
	for(const auto &Quad : m_vQuads)
	{
		float px = fx2f(Quad.m_aPoints[4].x);
		float py = fx2f(Quad.m_aPoints[4].y);

		if(px > Rect.x && px < Rect.x + Rect.w && py > Rect.y && py < Rect.y + Rect.h)
		{
			CQuad n = Quad;

			for(auto &Point : n.m_aPoints)
			{
				Point.x -= f2fx(Rect.x);
				Point.y -= f2fx(Rect.y);
			}

			pGrabbed->m_vQuads.push_back(n);
		}
	}

	return pGrabbed->m_vQuads.empty() ? 0 : 1;
}

void CLayerQuads::BrushPlace(std::shared_ptr<CLayer> pBrush, float wx, float wy)
{
	std::shared_ptr<CLayerQuads> pQuadLayer = std::static_pointer_cast<CLayerQuads>(pBrush);
	for(const auto &Quad : pQuadLayer->m_vQuads)
	{
		CQuad n = Quad;

		for(auto &Point : n.m_aPoints)
		{
			Point.x += f2fx(wx);
			Point.y += f2fx(wy);
		}

		m_vQuads.push_back(n);
	}
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

void Rotate(vec2 *pCenter, vec2 *pPoint, float Rotation)
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

CUI::EPopupMenuFunctionResult CLayerQuads::RenderProperties(CUIRect *pToolBox)
{
	enum
	{
		PROP_IMAGE = 0,
		NUM_PROPS,
	};

	CProperty aProps[] = {
		{"Image", m_Image, PROPTYPE_IMAGE, -1, 0},
		{nullptr},
	};

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = m_pEditor->DoProperties(pToolBox, aProps, s_aIds, &NewVal);
	if(Prop != -1)
	{
		m_pEditor->m_Map.OnModify();
	}

	if(Prop == PROP_IMAGE)
	{
		if(NewVal >= 0)
			m_Image = NewVal % m_pEditor->m_Map.m_vpImages.size();
		else
			m_Image = -1;
	}

	return CUI::POPUP_KEEP_OPEN;
}

void CLayerQuads::ModifyImageIndex(FIndexModifyFunction Func)
{
	Func(&m_Image);
}

void CLayerQuads::ModifyEnvelopeIndex(FIndexModifyFunction Func)
{
	for(auto &Quad : m_vQuads)
	{
		Func(&Quad.m_PosEnv);
		Func(&Quad.m_ColorEnv);
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
