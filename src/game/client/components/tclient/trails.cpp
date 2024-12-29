#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/render.h>

#include "trails.h"
#include <game/client/gameclient.h>

CTrails::CTrails()
{
}
bool CTrails::ShouldPredictPlayer(int ClientId)
{
	if(!GameClient()->Predict())
		return false;
	CCharacter *pChar = GameClient()->m_PredictedWorld.GetCharacterById(ClientId);
	if(GameClient()->Predict() && (ClientId == GameClient()->m_Snap.m_LocalClientId || (GameClient()->AntiPingPlayers() && !GameClient()->IsOtherTeam(ClientId))) && pChar)
		return true;
	return false;
}

void CTrails::ClearAllHistory()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
		for(int j = 0; j < 200; ++j)
			m_PositionTick[i][j] = -1;
	for(int i = 0; i < MAX_CLIENTS; ++i)
		for(int j = 0; j < 200; ++j)
			m_PositionHistory[i][j] = vec2(0, 0);
	for(int i = 0; i < MAX_CLIENTS; ++i)
		m_HistoryValid[i] = false;
}
void CTrails::ClearHistory(int ClientId)
{
	for(int j = 0; j < 200; ++j)
		m_PositionTick[ClientId][j] = -1;
	for(int j = 0; j < 200; ++j)
		m_PositionHistory[ClientId][j] = vec2(0, 0);
	m_HistoryValid[ClientId] = false;
}
void CTrails::OnReset()
{
	ClearAllHistory();
}
void CTrails::OnRender()
{
	if(!g_Config.m_ClTeeTrails)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!m_pClient->m_Snap.m_pGameInfoObj)
		return;

	Graphics()->TextureClear();
	GameClient()->RenderTools()->MapScreenToGroup(GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y, GameClient()->Layers()->GameGroup(), GameClient()->m_Camera.m_Zoom);

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(!m_pClient->m_Snap.m_aCharacters[ClientId].m_Active)
		{
			if(m_HistoryValid[ClientId])
				ClearHistory(ClientId);
			continue;
		}
		else
			m_HistoryValid[ClientId] = true;

		bool PredictPlayer = ShouldPredictPlayer(ClientId);

		// CCharacter *pChar;
		// if(PredictPlayer)
		//	pChar = GameClient()->m_PredictedWorld.GetCharacterById(ClientId);
		// else
		//	pChar = GameClient()->m_GameWorld.GetCharacterById(ClientId);
		// if (!pChar) {
		//	if(m_HistoryValid[ClientId])
		//		ClearHistory(ClientId);
		//	continue;
		// }

		int StartTick;
		const int GameTick = Client()->GameTick(g_Config.m_ClDummy);
		const int PredTick = Client()->PredGameTick(g_Config.m_ClDummy);
		float IntraTick;
		if(PredictPlayer)
		{
			StartTick = PredTick;
			IntraTick = Client()->PredIntraGameTick(g_Config.m_ClDummy);
		}
		else
		{
			StartTick = GameTick;
			IntraTick = Client()->IntraGameTick(g_Config.m_ClDummy) / 2.0f;
		}

		vec2 CurServerPos = vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_Y);
		vec2 PrevServerPos = vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev.m_Y);
		m_PositionHistory[ClientId][GameTick % 200] = mix(PrevServerPos, CurServerPos, Client()->IntraGameTick(g_Config.m_ClDummy));
		m_PositionTick[ClientId][GameTick % 200] = GameTick;

		// Slightly messy way to make the position of our player accurate without ugly logic in the loop
		vec2 SavedTempPredPos = GameClient()->m_aClients[ClientId].m_aPredPos[PredTick % 200];
		GameClient()->m_aClients[ClientId].m_aPredPos[PredTick % 200] = GameClient()->m_aClients[ClientId].m_RenderPos;

		IGraphics::CLineItem LineItem;
		Graphics()->LinesBegin();
		Graphics()->SetColor(ColorRGBA(0.0f, 1.0, 0.0f, 1.0f));

		vec2 PrevPrevPos;
		bool PrevPrevInit = false;
		vec2 PrevPos;
		bool PrevInit = false;
		vec2 Pos;
		bool Init = false;
		int TrailLength = 60;
		for(int i = 0; i < TrailLength; i++)
		{
			int PosTick = StartTick - i;
			bool Draw = false;
			bool LastDraw = false;
			if(PredictPlayer)
			{
				if(GameClient()->m_aClients[ClientId].m_aPredTick[PosTick % 200] != PosTick)
					continue;
				Pos = GameClient()->m_aClients[ClientId].m_aPredPos[PosTick % 200];
				Init = true;
				LastDraw = i == TrailLength - 1;
			}
			else
			{
				if(m_PositionTick[ClientId][PosTick % 200] != PosTick)
					continue;
				Pos = m_PositionHistory[ClientId][PosTick % 200];
				Init = true;
				LastDraw = (m_PositionTick[ClientId][(PosTick - 1) % 200] != PosTick - 1) && i > TrailLength - 3;
			}
			if(Init && PrevInit && distance(PrevPos, Pos) < 600.0f)
			{
				if(LastDraw)
					Pos = mix(Pos, PrevPos, IntraTick);
				LineItem = IGraphics::CLineItem(PrevPos.x, PrevPos.y, Pos.x, Pos.y);
				Graphics()->LinesDraw(&LineItem, 1);
			}
			if(PrevInit)
			{
				PrevPrevPos = PrevPos;
				PrevPrevInit = true;
			}
			if(Init)
			{
				PrevPos = Pos;
				PrevInit = true;
			}
		}
		Graphics()->LinesEnd();

		GameClient()->m_aClients[ClientId].m_aPredPos[PredTick % 200] = SavedTempPredPos;
		m_PositionHistory[ClientId][GameTick % 200] = CurServerPos;
	}
}