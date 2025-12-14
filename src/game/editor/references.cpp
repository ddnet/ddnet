#include "references.h"

#include <game/editor/mapitems/envelope.h>
#include <game/editor/mapitems/layer_quads.h>
#include <game/editor/mapitems/layer_sounds.h>
#include <game/editor/mapitems/layer_tiles.h>

void CLayerTilesEnvelopeReference::SetEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope, int EnvelopeIndex)
{
	if(pEnvelope->Type() == CEnvelope::EType::COLOR)
	{
		m_pLayerTiles->m_ColorEnv = EnvelopeIndex;
	}
}

void CLayerQuadsEnvelopeReference::SetEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope, int EnvelopeIndex)
{
	for(auto &QuadIndex : m_vQuadIndices)
	{
		if(QuadIndex >= 0 && QuadIndex < (int)m_pLayerQuads->m_vQuads.size())
		{
			if(pEnvelope->Type() == CEnvelope::EType::COLOR)
				m_pLayerQuads->m_vQuads[QuadIndex].m_ColorEnv = EnvelopeIndex;
			else if(pEnvelope->Type() == CEnvelope::EType::POSITION)
				m_pLayerQuads->m_vQuads[QuadIndex].m_PosEnv = EnvelopeIndex;
		}
	}
}

void CLayerSoundEnvelopeReference::SetEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope, int EnvelopeIndex)
{
	for(auto &SoundSourceIndex : m_vSoundSourceIndices)
	{
		if(SoundSourceIndex >= 0 && SoundSourceIndex <= (int)m_pLayerSounds->m_vSources.size())
		{
			if(pEnvelope->Type() == CEnvelope::EType::SOUND)
				m_pLayerSounds->m_vSources[SoundSourceIndex].m_SoundEnv = EnvelopeIndex;
			else if(pEnvelope->Type() == CEnvelope::EType::POSITION)
				m_pLayerSounds->m_vSources[SoundSourceIndex].m_PosEnv = EnvelopeIndex;
		}
	}
}
