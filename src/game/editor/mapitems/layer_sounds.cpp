#include "layer_sounds.h"

#include <generated/client_data.h>

#include <game/editor/editor.h>
#include <game/editor/editor_actions.h>

static const float s_SourceVisualSize = 32.0f;

CLayerSounds::CLayerSounds(CEditor *pEditor) :
	CLayer(pEditor)
{
	m_Type = LAYERTYPE_SOUNDS;
	m_aName[0] = '\0';
	m_LayerSounds.m_Sound = -1;
}

CLayerSounds::CLayerSounds(const CLayerSounds &Other) :
	CLayer(Other)
{
	m_LayerSounds.m_Sound = Other.m_LayerSounds.m_Sound;
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
		ColorRGBA Offset = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		m_pEditor->EnvelopeEval(Source.m_PosEnvOffset, Source.m_PosEnv, Offset, 2);
		const vec2 Position = vec2(fx2f(Source.m_Position.x) + Offset.r, fx2f(Source.m_Position.y) + Offset.g);
		const float Falloff = Source.m_Falloff / 255.0f;

		switch(Source.m_Shape.m_Type)
		{
		case CSoundShape::SHAPE_CIRCLE:
		{
			m_pEditor->Graphics()->DrawCircle(Position.x, Position.y, Source.m_Shape.m_Circle.m_Radius, 32);
			if(Falloff > 0.0f)
			{
				m_pEditor->Graphics()->DrawCircle(Position.x, Position.y, Source.m_Shape.m_Circle.m_Radius * Falloff, 32);
			}
			break;
		}
		case CSoundShape::SHAPE_RECTANGLE:
		{
			const float Width = fx2f(Source.m_Shape.m_Rectangle.m_Width);
			const float Height = fx2f(Source.m_Shape.m_Rectangle.m_Height);
			m_pEditor->Graphics()->DrawRectExt(Position.x - Width / 2, Position.y - Height / 2, Width, Height, 0.0f, IGraphics::CORNER_NONE);
			if(Falloff > 0.0f)
			{
				m_pEditor->Graphics()->DrawRectExt(Position.x - Falloff * Width / 2, Position.y - Falloff * Height / 2, Width * Falloff, Height * Falloff, 0.0f, IGraphics::CORNER_NONE);
			}
			break;
		}
		}
	}

	Graphics()->QuadsEnd();

	// draw handles
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_AUDIO_SOURCE].m_Id);
	Graphics()->QuadsBegin();

	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	m_pEditor->Graphics()->SelectSprite(SPRITE_AUDIO_SOURCE);
	for(const auto &Source : m_vSources)
	{
		ColorRGBA Offset = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		m_pEditor->EnvelopeEval(Source.m_PosEnvOffset, Source.m_PosEnv, Offset, 2);
		const vec2 Position = vec2(fx2f(Source.m_Position.x) + Offset.r, fx2f(Source.m_Position.y) + Offset.g);
		m_pEditor->Graphics()->DrawSprite(Position.x, Position.y, m_pEditor->MapView()->ScaleLength(s_SourceVisualSize));
	}

	Graphics()->QuadsEnd();
}

CSoundSource *CLayerSounds::NewSource(int x, int y)
{
	m_pEditor->m_Map.OnModify();

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
	Rect.DrawOutline(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
}

int CLayerSounds::BrushGrab(std::shared_ptr<CLayerGroup> pBrush, CUIRect Rect)
{
	// create new layer
	std::shared_ptr<CLayerSounds> pGrabbed = std::make_shared<CLayerSounds>(m_pEditor);
	pGrabbed->m_LayerSounds.m_Sound = m_LayerSounds.m_Sound;
	pBrush->AddLayer(pGrabbed);

	for(const auto &Source : m_vSources)
	{
		float SourceX = fx2f(Source.m_Position.x);
		float SourceY = fx2f(Source.m_Position.y);

		if(SourceX > Rect.x && SourceX < Rect.x + Rect.w && SourceY > Rect.y && SourceY < Rect.y + Rect.h)
		{
			CSoundSource NewSource = Source;
			NewSource.m_Position.x -= f2fx(Rect.x);
			NewSource.m_Position.y -= f2fx(Rect.y);

			pGrabbed->m_vSources.push_back(NewSource);
		}
	}

	return pGrabbed->m_vSources.empty() ? 0 : 1;
}

void CLayerSounds::BrushPlace(std::shared_ptr<CLayer> pBrush, vec2 WorldPos)
{
	std::shared_ptr<CLayerSounds> pSoundLayer = std::static_pointer_cast<CLayerSounds>(pBrush);
	std::vector<CSoundSource> vAddedSources;
	for(const auto &Source : pSoundLayer->m_vSources)
	{
		CSoundSource NewSource = Source;
		NewSource.m_Position.x += f2fx(WorldPos.x);
		NewSource.m_Position.y += f2fx(WorldPos.y);

		m_vSources.push_back(NewSource);
		vAddedSources.push_back(NewSource);
	}
	m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionSoundPlace>(m_pEditor, m_pEditor->m_SelectedGroup, m_pEditor->m_vSelectedLayers[0], vAddedSources));
	m_pEditor->m_Map.OnModify();
}

CUi::EPopupMenuFunctionResult CLayerSounds::RenderProperties(CUIRect *pToolBox)
{
	CProperty aProps[] = {
		{"Sound", m_LayerSounds.m_Sound, PROPTYPE_SOUND, -1, 0},
		{nullptr},
	};

	static int s_aIds[(int)ELayerSoundsProp::NUM_PROPS] = {0};
	int NewVal = 0;
	auto [State, Prop] = m_pEditor->DoPropertiesWithState<ELayerSoundsProp>(pToolBox, aProps, s_aIds, &NewVal);
	if(Prop != ELayerSoundsProp::PROP_NONE && (State == EEditState::END || State == EEditState::ONE_GO))
	{
		m_pEditor->m_Map.OnModify();
	}

	static CLayerSoundsPropTracker s_Tracker(m_pEditor);
	s_Tracker.Begin(this, Prop, State);

	if(Prop == ELayerSoundsProp::PROP_SOUND)
	{
		if(NewVal >= 0)
			m_LayerSounds.m_Sound = NewVal % m_pEditor->m_Map.m_vpSounds.size();
		else
			m_LayerSounds.m_Sound = -1;
	}

	s_Tracker.End(Prop, State);

	return CUi::POPUP_KEEP_OPEN;
}

void CLayerSounds::ModifySoundIndex(const FIndexModifyFunction &IndexModifyFunction)
{
	IndexModifyFunction(&m_LayerSounds.m_Sound);
}

void CLayerSounds::ModifyEnvelopeIndex(const FIndexModifyFunction &IndexModifyFunction)
{
	for(auto &Source : m_vSources)
	{
		IndexModifyFunction(&Source.m_SoundEnv);
		IndexModifyFunction(&Source.m_PosEnv);
	}
}

std::shared_ptr<CLayer> CLayerSounds::Duplicate() const
{
	return std::make_shared<CLayerSounds>(*this);
}

const char *CLayerSounds::TypeName() const
{
	return "sounds";
}
