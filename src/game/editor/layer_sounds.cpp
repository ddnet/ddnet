
#include <game/generated/client_data.h>

#include "editor.h"

static const float s_SourceVisualSize = 32.0f;

CLayerSounds::CLayerSounds()
{
	m_Type = LAYERTYPE_SOUNDS;
	str_copy(m_aName, "Sounds", sizeof(m_aName));
	m_Sound = -1;
}

CLayerSounds::~CLayerSounds()
{
}

void CLayerSounds::Render()
{
	// TODO: nice texture
	Graphics()->TextureSet(-1);
	Graphics()->BlendNormal();
	Graphics()->QuadsBegin();

	// draw falloff distance
	Graphics()->SetColor(0.6f, 0.8f, 1.0f, 0.4f);
	for(int i = 0; i < m_lSources.size(); i++)
	{
		CSoundSource *pSource = &m_lSources[i];

		float OffsetX = 0;
		float OffsetY = 0;

		if(pSource->m_PosEnv >= 0)
		{
			float aChannels[4];
			m_pEditor->EnvelopeEval(pSource->m_PosEnvOffset/1000.0f, pSource->m_PosEnv, aChannels, m_pEditor);
			OffsetX = aChannels[0];
			OffsetY = aChannels[1];
		}

		switch(pSource->m_Shape.m_Type)
		{
		case CSoundShape::SHAPE_CIRCLE:
			{
				m_pEditor->RenderTools()->DrawCircle(fx2f(pSource->m_Position.x)+OffsetX, fx2f(pSource->m_Position.y)+OffsetY,
													pSource->m_Shape.m_Circle.m_Radius, 32);

				float Falloff = ((float)pSource->m_Falloff/255.0f);
				if(Falloff > 0.0f)
					m_pEditor->RenderTools()->DrawCircle(fx2f(pSource->m_Position.x)+OffsetX, fx2f(pSource->m_Position.y)+OffsetY,
													pSource->m_Shape.m_Circle.m_Radius*Falloff, 32);
				break;
			}
		case CSoundShape::SHAPE_RECTANGLE:
			{
				float Width = fx2f(pSource->m_Shape.m_Rectangle.m_Width);
				float Height = fx2f(pSource->m_Shape.m_Rectangle.m_Height);
				m_pEditor->RenderTools()->DrawRoundRect(fx2f(pSource->m_Position.x)+OffsetX - Width/2, fx2f(pSource->m_Position.y)+OffsetY - Height/2,
														Width, Height, 0.0f);

				float Falloff = ((float)pSource->m_Falloff/255.0f);
				if(Falloff > 0.0f)
					m_pEditor->RenderTools()->DrawRoundRect(fx2f(pSource->m_Position.x)+OffsetX - Falloff*Width/2, fx2f(pSource->m_Position.y)+OffsetY - Falloff*Height/2,
														Width*Falloff, Height*Falloff, 0.0f);
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
	for(int i = 0; i < m_lSources.size(); i++)
	{
		CSoundSource *pSource = &m_lSources[i];

		float OffsetX = 0;
		float OffsetY = 0;

		if(pSource->m_PosEnv >= 0)
		{
			float aChannels[4];
			m_pEditor->EnvelopeEval(pSource->m_PosEnvOffset/1000.0f, pSource->m_PosEnv, aChannels, m_pEditor);
			OffsetX = aChannels[0];
			OffsetY = aChannels[1];
		}

		m_pEditor->RenderTools()->DrawSprite(fx2f(pSource->m_Position.x)+OffsetX, fx2f(pSource->m_Position.y)+OffsetY, s_SourceVisualSize*m_pEditor->m_WorldZoom);
	}

	Graphics()->QuadsEnd();
}

CSoundSource *CLayerSounds::NewSource()
{
	m_pEditor->m_Map.m_Modified = true;

	CSoundSource *pSource = &m_lSources[m_lSources.add(CSoundSource())];

	pSource->m_Position.x = 0;
	pSource->m_Position.y = 0;

	pSource->m_Loop = 1;
	pSource->m_TimeDelay = 0;

	pSource->m_PosEnv = -1;
	pSource->m_PosEnvOffset = 0;
	pSource->m_SoundEnv = -1;
	pSource->m_SoundEnvOffset = 0;

	pSource->m_Falloff = 255;
	/*
	pSource->m_Shape.m_Type = CSoundShape::SHAPE_CIRCLE;
	pSource->m_Shape.m_Circle.m_Radius = 1500;
	*/

	pSource->m_Shape.m_Type = CSoundShape::SHAPE_RECTANGLE;
	pSource->m_Shape.m_Rectangle.m_Width = f2fx(1500.0f);
	pSource->m_Shape.m_Rectangle.m_Height = f2fx(1000.0f);

	return pSource;
}

void CLayerSounds::BrushSelecting(CUIRect Rect)
{
	// draw selection rectangle
	IGraphics::CLineItem Array[4] = {
		IGraphics::CLineItem(Rect.x, Rect.y, Rect.x+Rect.w, Rect.y),
		IGraphics::CLineItem(Rect.x+Rect.w, Rect.y, Rect.x+Rect.w, Rect.y+Rect.h),
		IGraphics::CLineItem(Rect.x+Rect.w, Rect.y+Rect.h, Rect.x, Rect.y+Rect.h),
		IGraphics::CLineItem(Rect.x, Rect.y+Rect.h, Rect.x, Rect.y)};
	Graphics()->TextureSet(-1);
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

	for(int i = 0; i < m_lSources.size(); i++)
	{
		CSoundSource *pSource = &m_lSources[i];
		float px = fx2f(pSource->m_Position.x);
		float py = fx2f(pSource->m_Position.y);

		if(px > Rect.x && px < Rect.x+Rect.w && py > Rect.y && py < Rect.y+Rect.h)
		{
			CSoundSource n;
			n = *pSource;

			n.m_Position.x -= f2fx(Rect.x);
			n.m_Position.y -= f2fx(Rect.y);

			pGrabbed->m_lSources.add(n);
		}
	}

	return pGrabbed->m_lSources.size()?1:0;
}

void CLayerSounds::BrushPlace(CLayer *pBrush, float wx, float wy)
{
	CLayerSounds *l = (CLayerSounds *)pBrush;
	for(int i = 0; i < l->m_lSources.size(); i++)
	{
		CSoundSource n = l->m_lSources[i];

		n.m_Position.x += f2fx(wx);
		n.m_Position.y += f2fx(wy);

		m_lSources.add(n);
	}
	m_pEditor->m_Map.m_Modified = true;
}

int CLayerSounds::RenderProperties(CUIRect *pToolBox)
{
	//
	enum
	{
		PROP_SOUND=0,
		NUM_PROPS,
	};

	CProperty aProps[] = {
		{"Sound", m_Sound, PROPTYPE_SOUND, -1, 0},
		{0},
	};

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = m_pEditor->DoProperties(pToolBox, aProps, s_aIds, &NewVal);
	if(Prop != -1)
		m_pEditor->m_Map.m_Modified = true;

	if(Prop == PROP_SOUND)
	{
		if(NewVal >= 0)
			m_Sound = NewVal%m_pEditor->m_Map.m_lSounds.size();
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
	for(int i = 0; i < m_lSources.size(); i++)
		Func(&m_lSources[i].m_SoundEnv);
}