
#include <engine/engine.h>
#include <engine/sound.h>

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
			/*
			void *pData = pMap->GetData(pImg->m_ImageData);
			m_aTextures[i] = Graphics()->LoadTextureRaw(pImg->m_Width, pImg->m_Height, CImageInfo::FORMAT_RGBA, pData, CImageInfo::FORMAT_RGBA, 0);
			pMap->UnloadData(pImg->m_ImageData);
			*/
		}
	}

	// enqueue sound sources
	m_SourceQueue.clear();
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
					// TODO: use duration of the source sample
					if(pSources[i].m_Loop || (Client()->LocalTime()-pSources[i].m_TimeDelay) < 2.0f)
					{
						CSource source;
						source.m_Sound = pSoundLayer->m_Sound;
						source.m_pSource = &pSources[i];
						m_SourceQueue.add(source);
					}
						
				}
					
			}
		}
	}
}

void CMapSounds::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	// enqueue sounds
	for(int i = 0; i < m_SourceQueue.size(); i++)
	{
		CSource Source = m_SourceQueue[i];

		if(!Source.m_pSource || Source.m_Sound == -1)
		{
			m_SourceQueue.remove(Source);
			continue;
		}

		if(Source.m_pSource->m_TimeDelay <= Client()->LocalTime())
		{
			{
				int Flags = 0;
				if(Source.m_pSource->m_Loop) Flags |= ISound::FLAG_LOOP;

				if(Source.m_pSource->m_Global)
					m_pClient->m_pSounds->PlaySample(CSounds::CHN_AMBIENT, m_aSounds[Source.m_Sound], 1.0f, Flags);
				else
					m_pClient->m_pSounds->PlaySampleAt(CSounds::CHN_AMBIENT, m_aSounds[Source.m_Sound], 1.0f, vec2(fx2f(Source.m_pSource->m_Position.x), fx2f(Source.m_pSource->m_Position.y)), Flags);
			}

			m_SourceQueue.remove(Source);
		}			
	}
}