
#include <engine/engine.h>
#include <engine/sound.h>

#include <game/client/components/camera.h>
#include <game/client/components/maplayers.h> // envelope
#include <game/client/components/sounds.h>

#include "mapsounds.h"

CMapSounds::CMapSounds()
{
	m_Count = 0;
}

void CMapSounds::OnMapLoad()
{
	IMap *pMap = Kernel()->RequestInterface<IMap>();

	// unload all samples
	for(int i = 0; i < m_Count; i++)
	{
		Sound()->UnloadSample(m_aSounds[i]);
		m_aSounds[i] = -1;
	}
	m_Count = 0;

	// load samples
	int Start;
	pMap->GetType(MAPITEMTYPE_SOUND, &Start, &m_Count);

	// load new samples
	for(int i = 0; i < m_Count; i++)
	{
		m_aSounds[i] = 0;

		CMapItemSound *pSound = (CMapItemSound *)pMap->GetItem(Start+i, 0, 0);
		if(pSound->m_External)
		{
			char Buf[256];
			char *pName = (char *)pMap->GetData(pSound->m_SoundName);
			str_format(Buf, sizeof(Buf), "mapres/%s.wv", pName);
			m_aSounds[i] = Sound()->LoadWV(Buf);
		}
		else
		{
			void *pData = pMap->GetData(pSound->m_SoundData);
			m_aSounds[i] = Sound()->LoadWVFromMem(pData, pSound->m_SoundDataSize);
			pMap->UnloadData(pSound->m_SoundData);
		}
	}

	// enqueue sound sources
	m_lSourceQueue.clear();
	for(int g = 0; g < Layers()->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = Layers()->GetGroup(g);

		if(!pGroup)
			continue;

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = Layers()->GetLayer(pGroup->m_StartLayer+l);

			if(!pLayer)
				continue;

			if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				CMapItemLayerSounds *pSoundLayer = (CMapItemLayerSounds *)pLayer;

				if(pSoundLayer->m_Sound == -1)
					continue;

				CSoundSource *pSources = (CSoundSource *)Layers()->Map()->GetDataSwapped(pSoundLayer->m_Data);

				if(!pSources)
					continue;

				for(int i = 0; i < pSoundLayer->m_NumSources; i++) {
					// dont add sources which are too much delayed
					if(pSources[i].m_Loop || (Client()->LocalTime() >= pSources[i].m_TimeDelay + Sound()->GetSampleDuration(pSoundLayer->m_Sound)))
					{
						CSourceQueueEntry source;
						source.m_Sound = pSoundLayer->m_Sound;
						source.m_pSource = &pSources[i];
						m_lSourceQueue.add(source);
					}
						
				}
					
			}
		}
	}

	m_lVoices.clear();
}

void CMapSounds::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	// enqueue sounds
	for(int i = 0; i < m_lSourceQueue.size(); i++)
	{
		CSourceQueueEntry *pSource = &m_lSourceQueue[i];

		if(!pSource->m_pSource || pSource->m_Sound == -1)
		{
			m_lSourceQueue.remove_index(i);
			continue;
		}

		if(pSource->m_pSource->m_TimeDelay <= Client()->LocalTime())
		{
			int Flags = 0;
			if(pSource->m_pSource->m_Loop) Flags |= ISound::FLAG_LOOP;

			CSourceVoice Voice;
			Voice.m_pSource = pSource->m_pSource;

			Voice.m_Voice = m_pClient->m_pSounds->PlaySampleAt(CSounds::CHN_AMBIENT, m_aSounds[pSource->m_Sound], 1.0f, vec2(fx2f(pSource->m_pSource->m_Position.x), fx2f(pSource->m_pSource->m_Position.y)), Flags);

			Sound()->SetVoiceMaxDistance(Voice.m_Voice, pSource->m_pSource->m_FalloffDistance);

			m_lVoices.add(Voice);
			m_lSourceQueue.remove_index(i);
		}
	}

	vec2 Center = m_pClient->m_pCamera->m_Center;
	for(int g = 0; g < Layers()->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = Layers()->GetGroup(g);

		if(!pGroup)
			continue;

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = Layers()->GetLayer(pGroup->m_StartLayer+l);

			if(!pLayer)
				continue;

			if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				CMapItemLayerSounds *pSoundLayer = (CMapItemLayerSounds *)pLayer;

				CSoundSource *pSources = (CSoundSource *)Layers()->Map()->GetDataSwapped(pSoundLayer->m_Data);

				if(!pSources)
					continue;

				for(int s = 0; s < pSoundLayer->m_NumSources; s++) {
					for(int i = 0; i < m_lVoices.size(); i++)
					{
						CSourceVoice *pVoice = &m_lVoices[i];

						if(pVoice->m_pSource != &pSources[s])
							continue;

						if(!pVoice->m_Voice.IsValid())
							continue;

						float OffsetX = 0, OffsetY = 0;

						if(pVoice->m_pSource->m_PosEnv >= 0)
						{
							float aChannels[4];
							CMapLayers::EnvelopeEval(pVoice->m_pSource->m_PosEnvOffset/1000.0f, pVoice->m_pSource->m_PosEnv, aChannels, m_pClient->m_pMapLayersBackGround);
							OffsetX = aChannels[0];
							OffsetY = aChannels[1];
						}

						float x = fx2f(pVoice->m_pSource->m_Position.x)+OffsetX;
						float y = fx2f(pVoice->m_pSource->m_Position.y)+OffsetY;

						x += Center.x*(1.0f-pGroup->m_ParallaxX/100.0f);
						y += Center.y*(1.0f-pGroup->m_ParallaxY/100.0f);

						x -= pGroup->m_OffsetX; y -= pGroup->m_OffsetY;

						Sound()->SetVoiceLocation(pVoice->m_Voice, x, y);

						if(pVoice->m_pSource->m_SoundEnv >= 0)
						{
							float aChannels[4];
							CMapLayers::EnvelopeEval(pVoice->m_pSource->m_SoundEnvOffset/1000.0f, pVoice->m_pSource->m_SoundEnv, aChannels, m_pClient->m_pMapLayersBackGround);
							float Volume = clamp(aChannels[0], 0.0f, 1.0f);

							Sound()->SetVoiceVolume(pVoice->m_Voice, Volume);
						}
					}	
				}	
			}
		}
	}
}