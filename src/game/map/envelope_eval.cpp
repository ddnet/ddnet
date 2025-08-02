#include <engine/map.h>
#include <game/client/gameclient.h>
#include <game/editor/editor.h>
#include <game/mapitems.h>

#include "envelope_eval.h"

using namespace std::chrono_literals;

CEnvelopeEvalGame::CEnvelopeEvalGame(CGameClient *pGameClient, IMap *pMap) :
	m_EnvelopePointAccess(pMap)
{
	OnInterfacesInit(pGameClient);
	m_pMap = pMap;
}

void CEnvelopeEvalGame::EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, bool OnlineOnly)
{
	std::shared_ptr<const CMapItemEnvelope> pItem = GetEnvelope(Env);
	if(!pItem || pItem->m_Channels <= 0)
		return;
	Channels = minimum<size_t>(Channels, pItem->m_Channels, CEnvPoint::MAX_CHANNELS);

	m_EnvelopePointAccess.SetPointsRange(pItem->m_StartPoint, pItem->m_NumPoints);
	if(m_EnvelopePointAccess.NumPoints() == 0)
		return;

	static std::chrono::nanoseconds s_Time{0};
	static auto s_LastLocalTime = time_get_nanoseconds();
	if(OnlineOnly && (pItem->m_Version < 2 || pItem->m_Synchronized))
	{
		if(GameClient()->m_Snap.m_pGameInfoObj)
		{
			// get the lerp of the current tick and prev
			const auto TickToNanoSeconds = std::chrono::nanoseconds(1s) / (int64_t)Client()->GameTickSpeed();
			const int MinTick = Client()->PrevGameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick;
			const int CurTick = Client()->GameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick;
			s_Time = std::chrono::nanoseconds((int64_t)(mix<double>(
									    0,
									    (CurTick - MinTick),
									    (double)Client()->IntraGameTick(g_Config.m_ClDummy)) *
								    TickToNanoSeconds.count())) +
				 MinTick * TickToNanoSeconds;
		}
	}
	else
	{
		const auto CurTime = time_get_nanoseconds();
		s_Time += CurTime - s_LastLocalTime;
		s_LastLocalTime = CurTime;
	}
	CRenderTools::RenderEvalEnvelope(&m_EnvelopePointAccess, s_Time + std::chrono::nanoseconds(std::chrono::milliseconds(TimeOffsetMillis)), Result, Channels);
}

std::shared_ptr<const CMapItemEnvelope> CEnvelopeEvalGame::GetEnvelope(int Env)
{
	int EnvStart, EnvNum;
	m_pMap->GetType(MAPITEMTYPE_ENVELOPE, &EnvStart, &EnvNum);
	if(Env < 0 || Env >= EnvNum)
		return nullptr;
	const CMapItemEnvelope *pItem = (CMapItemEnvelope *)m_pMap->GetItem(EnvStart + Env);

	// specify no-op deleter as this memory is not handled by the pointer
	return std::shared_ptr<const CMapItemEnvelope>(pItem, [](const CMapItemEnvelope *) {});
}

CEnvelopeEvalEditor::CEnvelopeEvalEditor(CEditor *pEditor)
{
	m_pEditor = pEditor;
}

void CEnvelopeEvalEditor::EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, bool OnlineOnly)
{
	if(Env < 0 || Env >= (int)m_pEditor->m_Map.m_vpEnvelopes.size())
		return;

	std::shared_ptr<CEnvelope> pEnv = m_pEditor->m_Map.m_vpEnvelopes[Env];
	float Time = m_pEditor->m_AnimateTime;
	Time *= m_pEditor->m_AnimateSpeed;
	Time += (TimeOffsetMillis / 1000.0f);
	pEnv->Eval(Time, Result, Channels);
}

const IEnvelopePointAccess &CEnvelopeEvalEditor::PointAccess(int Env) const
{
	std::shared_ptr<CEnvelope> pEnv = m_pEditor->m_Map.m_vpEnvelopes[Env];
	return pEnv->PointAccess();
}

std::shared_ptr<const CMapItemEnvelope> CEnvelopeEvalEditor::GetEnvelope(int Env)
{
	if(Env < 0 || Env >= (int)m_pEditor->m_Map.m_vpEnvelopes.size())
		return nullptr;

	std::shared_ptr<CMapItemEnvelope> Item = std::make_shared<CMapItemEnvelope>();
	Item->m_Version = 2;
	Item->m_Channels = m_pEditor->m_Map.m_vpEnvelopes[Env]->GetChannels();
	Item->m_StartPoint = 0;
	Item->m_NumPoints = m_pEditor->m_Map.m_vpEnvelopes[Env]->m_vPoints.size();
	Item->m_Synchronized = m_pEditor->m_Map.m_vpEnvelopes[Env]->m_Synchronized;
	StrToInts(Item->m_aName, std::size(Item->m_aName), m_pEditor->m_Map.m_vpEnvelopes[Env]->m_aName);
	return Item;
}
