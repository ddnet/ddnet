#include "envelope_state.h"

#include <game/client/gameclient.h>
#include <game/localization.h>

#include <chrono>

using namespace std::chrono_literals;

CEnvelopeState::CEnvelopeState(IMap *pMap, bool OnlineOnly) :
	m_pMap(pMap)
{
	m_pEnvelopePoints = std::make_shared<CMapBasedEnvelopePointAccess>(m_pMap);
	m_OnlineOnly = OnlineOnly;
}

void CEnvelopeState::EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels)
{
	using namespace std::chrono;

	if(!m_pMap)
		return;

	int EnvStart, EnvNum;
	m_pMap->GetType(MAPITEMTYPE_ENVELOPE, &EnvStart, &EnvNum);
	if(Env < 0 || Env >= EnvNum)
		return;

	const CMapItemEnvelope *pItem = (CMapItemEnvelope *)m_pMap->GetItem(EnvStart + Env);
	if(pItem->m_Channels <= 0)
		return;
	Channels = minimum<size_t>(Channels, pItem->m_Channels, CEnvPoint::MAX_CHANNELS);

	m_pEnvelopePoints->SetPointsRange(pItem->m_StartPoint, pItem->m_NumPoints);
	if(m_pEnvelopePoints->NumPoints() == 0)
		return;

	nanoseconds Time;

	// offline rendering (like menu background) relies on local time
	if(!m_OnlineOnly)
	{
		Time = time_get_nanoseconds();
	}
	else
	{
		// online rendering
		if(GameClient()->m_Snap.m_pGameInfoObj)
		{
			static const nanoseconds s_NanosPerTick = nanoseconds(1s) / static_cast<int64_t>(Client()->GameTickSpeed());

			// get the lerp of the current tick and prev
			int EnvelopeTick;
			double TickRatio;
			if(Client()->State() == IClient::STATE_DEMOPLAYBACK || !g_Config.m_ClPredict ||
				(GameClient()->m_Snap.m_SpecInfo.m_Active && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW))
			{
				EnvelopeTick = Client()->PrevGameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick;
				const int CurTick = Client()->GameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick;
				TickRatio = mix<double>(0, CurTick - EnvelopeTick, (double)Client()->IntraGameTick(g_Config.m_ClDummy));
			}
			else
			{
				EnvelopeTick = Client()->PredGameTick(g_Config.m_ClDummy) - 1 - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick;
				TickRatio = (double)Client()->PredIntraGameTick(g_Config.m_ClDummy);
			}
			Time = duration_cast<nanoseconds>(TickRatio * s_NanosPerTick) + EnvelopeTick * s_NanosPerTick;
		}
		else
		{
			Time = nanoseconds::zero();
		}
	}

	CRenderMap::RenderEvalEnvelope(m_pEnvelopePoints.get(), Time + milliseconds(TimeOffsetMillis), Result, Channels);
}
