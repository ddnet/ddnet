#include "envelope_state.h"

#include <base/dbg.h>
#include <base/time.h>

#include <game/client/gameclient.h>
#include <game/localization.h>

#include <chrono>

using namespace std::chrono_literals;

CEnvelopeState::CEnvelopeState(IMap *pMap, bool OnlineOnly)
{
	m_pEnvelopePoints = std::make_shared<CMapBasedEnvelopePointAccess>(pMap);
	m_OnlineOnly = OnlineOnly;
}

void CEnvelopeState::Update()
{
	// offline rendering (like menu background) relies on local time
	if(!m_OnlineOnly)
	{
		m_CachedTime = time_get_nanoseconds();
	}
	else
	{
		// online rendering
		if(GameClient()->m_Snap.m_pGameInfoObj)
		{
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
			const std::chrono::nanoseconds NanosPerTick = std::chrono::nanoseconds(1s) / static_cast<int64_t>(Client()->GameTickSpeed());
			m_CachedTime = std::chrono::duration_cast<std::chrono::nanoseconds>(TickRatio * NanosPerTick) + EnvelopeTick * NanosPerTick;
		}
		else
		{
			m_CachedTime = std::chrono::nanoseconds::zero();
		}
	}
}

void CEnvelopeState::EnvelopeEval(int TimeOffsetMillis, int EnvelopeIndex, ColorRGBA &Result, size_t Channels) const
{
	dbg_assert(m_pEnvelopePoints != nullptr, "Envelope state not initialized");

	if(EnvelopeIndex < 0)
	{
		return;
	}
	const std::vector<const CMapItemEnvelope *> &vpEnvelopes = m_pEnvelopePoints->Envelopes();
	if(EnvelopeIndex >= (int)vpEnvelopes.size())
	{
		return;
	}

	const CMapItemEnvelope *pEnvelope = vpEnvelopes[EnvelopeIndex];
	if(pEnvelope->m_Channels <= 0)
	{
		return;
	}
	Channels = std::min({Channels, (size_t)pEnvelope->m_Channels, (size_t)CEnvPoint::MAX_CHANNELS});

	m_pEnvelopePoints->SetPointsRange(pEnvelope->m_StartPoint, pEnvelope->m_NumPoints);
	if(m_pEnvelopePoints->NumPoints() == 0)
	{
		return;
	}

	CRenderMap::RenderEvalEnvelope(m_pEnvelopePoints.get(), m_CachedTime + std::chrono::milliseconds(TimeOffsetMillis), Result, Channels);
}
