
#include <game/generated/client_data.h>

#include "editor.h"

static const float s_SourceVisualSize = 32.0f;

CLayerSounds::CLayerSounds()
{
	m_Type = LAYERTYPE_SOUNDS;
	m_aName[0] = '\0';
	m_Sound = -1;
}

CLayerSounds::CLayerSounds(const CLayerSounds &Other) :
	CLayer(Other)
{
	m_Sound = Other.m_Sound;
	m_vSources = Other.m_vSources;
}

CLayerSounds::~CLayerSounds() = default;

void CLayerSounds::Render(bool Tileset)
{
	// TODO: nice texture
	Graphics()->TextureClear();
	Graphics()->BlendNormal();
	Graphics()->QuadsBegin();

	// draw falloff distance
	Graphics()->SetColor(0.6f, 0.8f, 1.0f, 0.4f);
	for(const auto &Source : m_vSources)
	{
		float OffsetX = 0;
		float OffsetY = 0;

		if(Source.m_PosEnv >= 0)
		{
			ColorRGBA Channels;
			m_pEditor->EnvelopeEval(Source.m_PosEnvOffset, Source.m_PosEnv, Channels, m_pEditor);
			OffsetX = Channels.r;
			OffsetY = Channels.g;
		}

		switch(Source.m_Shape.m_Type)
		{
		case CSoundShape::SHAPE_CIRCLE:
		{
			m_pEditor->Graphics()->DrawCircle(fx2f(Source.m_Position.x) + OffsetX, fx2f(Source.m_Position.y) + OffsetY,
				Source.m_Shape.m_Circle.m_Radius, 32);

			float Falloff = ((float)Source.m_Falloff / 255.0f);
			if(Falloff > 0.0f)
				m_pEditor->Graphics()->DrawCircle(fx2f(Source.m_Position.x) + OffsetX, fx2f(Source.m_Position.y) + OffsetY,
					Source.m_Shape.m_Circle.m_Radius * Falloff, 32);
			break;
		}
		case CSoundShape::SHAPE_RECTANGLE:
		{
			float Width = fx2f(Source.m_Shape.m_Rectangle.m_Width);
			float Height = fx2f(Source.m_Shape.m_Rectangle.m_Height);
			m_pEditor->Graphics()->DrawRectExt(fx2f(Source.m_Position.x) + OffsetX - Width / 2, fx2f(Source.m_Position.y) + OffsetY - Height / 2,
				Width, Height, 0.0f, IGraphics::CORNER_NONE);

			float Falloff = ((float)Source.m_Falloff / 255.0f);
			if(Falloff > 0.0f)
				m_pEditor->Graphics()->DrawRectExt(fx2f(Source.m_Position.x) + OffsetX - Falloff * Width / 2, fx2f(Source.m_Position.y) + OffsetY - Falloff * Height / 2,
					Width * Falloff, Height * Falloff, 0.0f, IGraphics::CORNER_NONE);
			break;
		}
		}
	}

	Graphics()->QuadsEnd();

	// draw handles
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_AUDIO_SOURCE].m_Id);
	Graphics()->QuadsBegin();

	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	m_pEditor->RenderTools()->SelectSprite(SPRITE_AUDIO_SOURCE);
	for(const auto &Source : m_vSources)
	{
		float OffsetX = 0;
		float OffsetY = 0;

		if(Source.m_PosEnv >= 0)
		{
			ColorRGBA Channels;
			m_pEditor->EnvelopeEval(Source.m_PosEnvOffset, Source.m_PosEnv, Channels, m_pEditor);
			OffsetX = Channels.r;
			OffsetY = Channels.g;
		}

		m_pEditor->RenderTools()->DrawSprite(fx2f(Source.m_Position.x) + OffsetX, fx2f(Source.m_Position.y) + OffsetY, s_SourceVisualSize * m_pEditor->m_WorldZoom);
	}

	Graphics()->QuadsEnd();
}

CSoundSource *CLayerSounds::NewSource(int x, int y)
{
	m_pEditor->m_Map.m_Modified = true;

	m_vSources.emplace_back();
	CSoundSource *pSource = &m_vSources[m_vSources.size() - 1];

	pSource->m_Position.x = f2fx(x);
	pSource->m_Position.y = f2fx(y);

	pSource->m_Loop = 1;
	pSource->m_Pan = 1;
	pSource->m_TimeDelay = 0;

	pSource->m_PosEnv = -1;
	pSource->m_PosEnvOffset = 0;
	pSource->m_SoundEnv = -1;
	pSource->m_SoundEnvOffset = 0;

	pSource->m_Falloff = 80;

	pSource->m_Shape.m_Type = CSoundShape::SHAPE_CIRCLE;
	pSource->m_Shape.m_Circle.m_Radius = 1500;

	return pSource;
}

void CLayerSounds::BrushSelecting(CUIRect Rect)
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

int CLayerSounds::BrushGrab(CLayerGroup *pBrush, CUIRect Rect)
{
	// create new layer
	CLayerSounds *pGrabbed = new CLayerSounds();
	pGrabbed->m_pEditor = m_pEditor;
	pGrabbed->m_Sound = m_Sound;
	pBrush->AddLayer(pGrabbed);

	for(const auto &Source : m_vSources)
	{
		float px = fx2f(Source.m_Position.x);
		float py = fx2f(Source.m_Position.y);

		if(px > Rect.x && px < Rect.x + Rect.w && py > Rect.y && py < Rect.y + Rect.h)
		{
			CSoundSource n = Source;

			n.m_Position.x -= f2fx(Rect.x);
			n.m_Position.y -= f2fx(Rect.y);

			pGrabbed->m_vSources.push_back(n);
		}
	}

	return pGrabbed->m_vSources.empty() ? 0 : 1;
}

void CLayerSounds::BrushPlace(CLayer *pBrush, float wx, float wy)
{
	CLayerSounds *pSoundLayer = (CLayerSounds *)pBrush;
	for(const auto &Source : pSoundLayer->m_vSources)
	{
		CSoundSource n = Source;

		n.m_Position.x += f2fx(wx);
		n.m_Position.y += f2fx(wy);

		m_vSources.push_back(n);
	}
	m_pEditor->m_Map.m_Modified = true;
}

int CLayerSounds::RenderProperties(CUIRect *pToolBox)
{
	//
	enum
	{
		PROP_SOUND = 0,
		NUM_PROPS,
	};

	CProperty aProps[] = {
		{"Sound", m_Sound, PROPTYPE_SOUND, -1, 0},
		{nullptr},
	};

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = m_pEditor->DoProperties(pToolBox, aProps, s_aIds, &NewVal);
	if(Prop != -1)
		m_pEditor->m_Map.m_Modified = true;

	if(Prop == PROP_SOUND)
	{
		if(NewVal >= 0)
			m_Sound = NewVal % m_pEditor->m_Map.m_vpSounds.size();
		else
			m_Sound = -1;
	}

	return 0;
}

void CLayerSounds::ModifySoundIndex(INDEX_MODIFY_FUNC Func)
{
	Func(&m_Sound);
}

void CLayerSounds::ModifyEnvelopeIndex(INDEX_MODIFY_FUNC Func)
{
	for(auto &Source : m_vSources)
	{
		Func(&Source.m_SoundEnv);
		Func(&Source.m_PosEnv);
	}
}

CLayer *CLayerSounds::Duplicate() const
{
	return new CLayerSounds(*this);
}
