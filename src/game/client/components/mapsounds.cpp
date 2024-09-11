#include "mapsounds.h"

#include <base/log.h>

#include <engine/demo.h>
#include <engine/sound.h>

#include <game/client/components/camera.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/layers.h>
#include <game/localization.h>
#include <game/mapitems.h>

CMapSounds::CMapSounds()
{
	m_Count = 0;
}

void CMapSounds::Play(int SoundId)
{
	if(SoundId < 0 || SoundId >= m_Count)
		return;

	m_pClient->m_Sounds.PlaySample(CSounds::CHN_MAPSOUND, m_aSounds[SoundId], 1.0f, 0);
}

void CMapSounds::PlayAt(int SoundId, vec2 Pos)
{
	if(SoundId < 0 || SoundId >= m_Count)
		return;

	m_pClient->m_Sounds.PlaySampleAt(CSounds::CHN_MAPSOUND, m_aSounds[SoundId], 1.0f, Pos, 0);
}

void CMapSounds::OnMapLoad()
{
	IMap *pMap = Kernel()->RequestInterface<IMap>();

	Clear();

	if(!Sound()->IsSoundEnabled())
		return;

	// load samples
	int Start;
	pMap->GetType(MAPITEMTYPE_SOUND, &Start, &m_Count);

	m_Count = clamp<int>(m_Count, 0, MAX_MAPSOUNDS);

	// load new samples
	bool ShowWarning = false;
	for(int i = 0; i < m_Count; i++)
	{
		CMapItemSound *pSound = (CMapItemSound *)pMap->GetItem(Start + i);
		if(pSound->m_External)
		{
			const char *pName = pMap->GetDataString(pSound->m_SoundName);
			if(pName == nullptr || pName[0] == '\0')
			{
				log_error("mapsounds", "Failed to load map sound %d: failed to load name.", i);
				ShowWarning = true;
				continue;
			}

			char aBuf[IO_MAX_PATH_LENGTH];
			str_format(aBuf, sizeof(aBuf), "mapres/%s.opus", pName);
			m_aSounds[i] = Sound()->LoadOpus(aBuf);
			pMap->UnloadData(pSound->m_SoundName);
		}
		else
		{
			const int SoundDataSize = pMap->GetDataSize(pSound->m_SoundData);
			const void *pData = pMap->GetData(pSound->m_SoundData);
			m_aSounds[i] = Sound()->LoadOpusFromMem(pData, SoundDataSize);
			pMap->UnloadData(pSound->m_SoundData);
		}
		ShowWarning = ShowWarning || m_aSounds[i] == -1;
	}
	if(ShowWarning)
	{
		Client()->AddWarning(SWarning(Localize("Some map sounds could not be loaded. Check the local console for details.")));
	}

	// enqueue sound sources
	for(int g = 0; g < Layers()->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = Layers()->GetGroup(g);

		if(!pGroup)
			continue;

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = Layers()->GetLayer(pGroup->m_StartLayer + l);

			if(!pLayer)
				continue;

			if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				CMapItemLayerSounds *pSoundLayer = (CMapItemLayerSounds *)pLayer;

				if(pSoundLayer->m_Version < 1 || pSoundLayer->m_Version > CMapItemLayerSounds::CURRENT_VERSION)
					continue;

				if(pSoundLayer->m_Sound == -1)
					continue;

				CSoundSource *pSources = (CSoundSource *)Layers()->Map()->GetDataSwapped(pSoundLayer->m_Data);

				if(!pSources)
					continue;

				for(int i = 0; i < pSoundLayer->m_NumSources; i++)
				{
					CSourceQueueEntry Source;
					Source.m_Sound = pSoundLayer->m_Sound;
					Source.m_pSource = &pSources[i];
					Source.m_HighDetail = pLayer->m_Flags & LAYERFLAG_DETAIL;

					if(!Source.m_pSource || Source.m_Sound < 0 || Source.m_Sound >= m_Count)
						continue;

					m_vSourceQueue.push_back(Source);
				}
			}
		}
	}
}

void CMapSounds::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	bool DemoPlayerPaused = Client()->State() == IClient::STATE_DEMOPLAYBACK && DemoPlayer()->BaseInfo()->m_Paused;

	// enqueue sounds
	for(auto &Source : m_vSourceQueue)
	{
		static float s_Time = 0.0f;
		if(m_pClient->m_Snap.m_pGameInfoObj) // && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED))
		{
			s_Time = mix((Client()->PrevGameTick(g_Config.m_ClDummy) - m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick) / (float)Client()->GameTickSpeed(),
				(Client()->GameTick(g_Config.m_ClDummy) - m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick) / (float)Client()->GameTickSpeed(),
				Client()->IntraGameTick(g_Config.m_ClDummy));
		}
		float Offset = s_Time - Source.m_pSource->m_TimeDelay;
		if(!DemoPlayerPaused && Offset >= 0.0f && g_Config.m_SndEnable && (g_Config.m_GfxHighDetail || !Source.m_HighDetail))
		{
			if(Source.m_Voice.IsValid())
			{
				// currently playing, set offset
				Sound()->SetVoiceTimeOffset(Source.m_Voice, Offset);
			}
			else
			{
				// need to enqueue
				int Flags = 0;
				if(Source.m_pSource->m_Loop)
					Flags |= ISound::FLAG_LOOP;
				if(!Source.m_pSource->m_Pan)
					Flags |= ISound::FLAG_NO_PANNING;

				Source.m_Voice = m_pClient->m_Sounds.PlaySampleAt(CSounds::CHN_MAPSOUND, m_aSounds[Source.m_Sound], 1.0f, vec2(fx2f(Source.m_pSource->m_Position.x), fx2f(Source.m_pSource->m_Position.y)), Flags);
				Sound()->SetVoiceTimeOffset(Source.m_Voice, Offset);
				Sound()->SetVoiceFalloff(Source.m_Voice, Source.m_pSource->m_Falloff / 255.0f);
				switch(Source.m_pSource->m_Shape.m_Type)
				{
				case CSoundShape::SHAPE_CIRCLE:
				{
					Sound()->SetVoiceCircle(Source.m_Voice, Source.m_pSource->m_Shape.m_Circle.m_Radius);
					break;
				}

				case CSoundShape::SHAPE_RECTANGLE:
				{
					Sound()->SetVoiceRectangle(Source.m_Voice, fx2f(Source.m_pSource->m_Shape.m_Rectangle.m_Width), fx2f(Source.m_pSource->m_Shape.m_Rectangle.m_Height));
					break;
				}
				};
			}
		}
		else
		{
			// stop voice
			Sound()->StopVoice(Source.m_Voice);
			Source.m_Voice = ISound::CVoiceHandle();
		}
	}

	vec2 Center = m_pClient->m_Camera.m_Center;
	for(int g = 0; g < Layers()->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = Layers()->GetGroup(g);

		if(!pGroup)
			continue;

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = Layers()->GetLayer(pGroup->m_StartLayer + l);

			if(!pLayer)
				continue;

			if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				CMapItemLayerSounds *pSoundLayer = (CMapItemLayerSounds *)pLayer;

				if(pSoundLayer->m_Version < 1 || pSoundLayer->m_Version > CMapItemLayerSounds::CURRENT_VERSION)
					continue;

				CSoundSource *pSources = (CSoundSource *)Layers()->Map()->GetDataSwapped(pSoundLayer->m_Data);

				if(!pSources)
					continue;

				for(int s = 0; s < pSoundLayer->m_NumSources; s++)
				{
					for(auto &Voice : m_vSourceQueue)
					{
						if(Voice.m_pSource != &pSources[s])
							continue;

						if(!Voice.m_Voice.IsValid())
							continue;

						ColorRGBA Position = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
						CMapLayers::EnvelopeEval(Voice.m_pSource->m_PosEnvOffset, Voice.m_pSource->m_PosEnv, Position, 2, &m_pClient->m_MapLayersBackground);

						float x = fx2f(Voice.m_pSource->m_Position.x) + Position.r;
						float y = fx2f(Voice.m_pSource->m_Position.y) + Position.g;

						x += Center.x * (1.0f - pGroup->m_ParallaxX / 100.0f);
						y += Center.y * (1.0f - pGroup->m_ParallaxY / 100.0f);

						x -= pGroup->m_OffsetX;
						y -= pGroup->m_OffsetY;

						Sound()->SetVoiceLocation(Voice.m_Voice, x, y);

						ColorRGBA Volume = ColorRGBA(1.0f, 0.0f, 0.0f, 0.0f);
						CMapLayers::EnvelopeEval(Voice.m_pSource->m_SoundEnvOffset, Voice.m_pSource->m_SoundEnv, Volume, 1, &m_pClient->m_MapLayersBackground);
						if(Volume.r < 1.0f)
						{
							Sound()->SetVoiceVolume(Voice.m_Voice, Volume.r);
						}
					}
				}
			}
		}
	}
}

void CMapSounds::Clear()
{
	// unload all samples
	m_vSourceQueue.clear();
	for(int i = 0; i < m_Count; i++)
	{
		Sound()->UnloadSample(m_aSounds[i]);
		m_aSounds[i] = -1;
	}
	m_Count = 0;
}

void CMapSounds::OnStateChange(int NewState, int OldState)
{
	if(NewState < IClient::STATE_ONLINE)
		Clear();
}
