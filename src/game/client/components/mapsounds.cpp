
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

	// TODO: load samples

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
				CSoundSource *pSources = (CSoundSource *)Layers()->Map()->GetDataSwapped(pSoundLayer->m_Data);

				if(!pSources)
					continue;

				for(int i = 0; i < pSoundLayer->m_NumSources; i++)
					m_SourceQueue.add(&pSources[i]);
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
		CSoundSource *pSource = m_SourceQueue[i];

		if(!pSource)
		{
			m_SourceQueue.remove(pSource);
			continue;
		}

		if(pSource->m_TimeDelay*1000.0f <= Client()->LocalTime())
		{
			m_pClient->m_pSounds->PlaySampleAt(CSounds::CHN_AMBIENT, 1, 1.0f, vec2(fx2f(pSource->m_Position.x), fx2f(pSource->m_Position.y)), ISound::FLAG_LOOP);
			m_SourceQueue.remove(pSource);
		}
			
	}
}