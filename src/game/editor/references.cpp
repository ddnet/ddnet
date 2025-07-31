#include "references.h"

void CLayerTilesEnvelopeReference::SetEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope, int EnvIndex)
{
	if(pEnvelope->Type() == CEnvelope::EType::COLOR)
	{
		m_pLayerTiles->m_ColorEnv = EnvIndex;
	}
}

void CLayerQuadsEnvelopeReference::SetEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope, int EnvIndex)
{
	for(auto &QuadIndex : m_vQuadIndices)
	{
		if(QuadIndex >= 0 && QuadIndex < (int)m_pLayerQuads->m_vQuads.size())
		{
			if(pEnvelope->Type() == CEnvelope::EType::COLOR)
				m_pLayerQuads->m_vQuads[QuadIndex].m_ColorEnv = EnvIndex;
			else if(pEnvelope->Type() == CEnvelope::EType::POSITION)
				m_pLayerQuads->m_vQuads[QuadIndex].m_PosEnv = EnvIndex;
		}
	}
}

void CLayerSoundEnvelopeReference::SetEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope, int EnvIndex)
{
	for(auto &SoundSourceIndex : m_vSoundSourceIndices)
	{
		if(SoundSourceIndex >= 0 && SoundSourceIndex <= (int)m_pLayerSounds->m_vSources.size())
		{
			if(pEnvelope->Type() == CEnvelope::EType::SOUND)
				m_pLayerSounds->m_vSources[SoundSourceIndex].m_SoundEnv = EnvIndex;
			else if(pEnvelope->Type() == CEnvelope::EType::POSITION)
				m_pLayerSounds->m_vSources[SoundSourceIndex].m_PosEnv = EnvIndex;
		}
	}
}
