#include <engine/demo.h>
#include <engine/sound.h>

#include <game/client/components/camera.h>
#include <game/client/components/maplayers.h> // envelope
#include <game/client/components/sounds.h>

#include <game/client/gameclient.h>

#include <game/layers.h>
#include <game/mapitems.h>

#include "mapsounds.h"

CMapSounds::CMapSounds()
{
	m_Count = 0;
}

void CMapSounds::OnMapLoad()
{
	IMap *pMap = Kernel()->RequestInterface<IMap>();

	Clear();

	// load samples
	int Start;
	pMap->GetType(MAPITEMTYPE_SOUND, &Start, &m_Count);

	// load new samples
	for(int i = 0; i < m_Count; i++)
	{
		m_aSounds[i] = 0;

		CMapItemSound *pSound = (CMapItemSound *)pMap->GetItem(Start + i, 0, 0);
		if(pSound->m_External)
		{
			char aBuf[IO_MAX_PATH_LENGTH];
			char *pName = (char *)pMap->GetData(pSound->m_SoundName);
			str_format(aBuf, sizeof(aBuf), "mapres/%s.opus", pName);
			m_aSounds[i] = Sound()->LoadOpus(aBuf);
		}
		else
		{
			void *pData = pMap->GetData(pSound->m_SoundData);
			m_aSounds[i] = Sound()->LoadOpusFromMem(pData, pSound->m_SoundDataSize);
			pMap->UnloadData(pSound->m_SoundData);
		}
	}

	// enqueue sound sources
	m_vSourceQueue.clear();
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
					CSourceQueueEntry source;
					source.m_Sound = pSoundLayer->m_Sound;
					source.m_pSource = &pSources[i];
					source.m_HighDetail = pLayer->m_Flags & LAYERFLAG_DETAIL;

					if(!source.m_pSource || source.m_Sound == -1)
						continue;

					m_vSourceQueue.push_back(source);
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

						float OffsetX = 0, OffsetY = 0;

						if(Voice.m_pSource->m_PosEnv >= 0)
						{
							ColorRGBA Channels;
							CMapLayers::EnvelopeEval(Voice.m_pSource->m_PosEnvOffset, Voice.m_pSource->m_PosEnv, Channels, &m_pClient->m_MapLayersBackGround);
							OffsetX = Channels.r;
							OffsetY = Channels.g;
						}

						float x = fx2f(Voice.m_pSource->m_Position.x) + OffsetX;
						float y = fx2f(Voice.m_pSource->m_Position.y) + OffsetY;

						x += Center.x * (1.0f - pGroup->m_ParallaxX / 100.0f);
						y += Center.y * (1.0f - pGroup->m_ParallaxY / 100.0f);

						x -= pGroup->m_OffsetX;
						y -= pGroup->m_OffsetY;

						Sound()->SetVoiceLocation(Voice.m_Voice, x, y);

						if(Voice.m_pSource->m_SoundEnv >= 0)
						{
							ColorRGBA Channels;
							CMapLayers::EnvelopeEval(Voice.m_pSource->m_SoundEnvOffset, Voice.m_pSource->m_SoundEnv, Channels, &m_pClient->m_MapLayersBackGround);
							float Volume = clamp(Channels.r, 0.0f, 1.0f);

							Sound()->SetVoiceVolume(Voice.m_Voice, Volume);
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
