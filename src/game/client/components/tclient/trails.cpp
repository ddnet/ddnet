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
	if(!g_Config.m_ClTeeTrail)
		return;

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!m_pClient->m_Snap.m_pGameInfoObj)
		return;

	Graphics()->TextureClear();
	GameClient()->RenderTools()->MapScreenToGroup(GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y, GameClient()->Layers()->GameGroup(), GameClient()->m_Camera.m_Zoom);

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		bool Local = m_pClient->m_Snap.m_LocalClientId == ClientId;

		bool ZoomAllowed = GameClient()->m_Camera.ZoomAllowed();
		if(!g_Config.m_ClTeeTrailOthers && !Local)
			continue;

		if(!Local && !ZoomAllowed)
			continue;

		if(!m_pClient->m_Snap.m_aCharacters[ClientId].m_Active)
		{
			if(m_HistoryValid[ClientId])
				ClearHistory(ClientId);
			continue;
		}
		else
			m_HistoryValid[ClientId] = true;

		CTeeRenderInfo TeeInfo = m_pClient->m_aClients[ClientId].m_RenderInfo;

		bool PredictPlayer = ShouldPredictPlayer(ClientId);
		int StartTick;
		const int GameTick = Client()->GameTick(g_Config.m_ClDummy);
		const int PredTick = Client()->PredGameTick(g_Config.m_ClDummy);
		float IntraTick;
		if(PredictPlayer)
		{
			StartTick = PredTick;
			IntraTick = Client()->PredIntraGameTick(g_Config.m_ClDummy);
			if(g_Config.m_ClRemoveAnti)
			{
				StartTick = GameClient()->m_SmoothTick[g_Config.m_ClDummy];
				IntraTick = GameClient()->m_SmoothIntraTick[g_Config.m_ClDummy];
			}
			if(g_Config.m_ClUnpredOthersInFreeze && !Local && g_Config.m_ClAmIFrozen)
			{
				StartTick = GameTick;
			}
		}
		else
		{
			StartTick = GameTick;
			IntraTick = Client()->IntraGameTick(g_Config.m_ClDummy);
		}

		vec2 CurServerPos = vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_Y);
		vec2 PrevServerPos = vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev.m_Y);
		m_PositionTick[ClientId][GameTick % 200] = GameTick;
		m_PositionHistory[ClientId][GameTick % 200] = CurServerPos;

		IGraphics::CLineItem LineItem;
		bool LineMode = g_Config.m_ClTeeTrailWidth == 0;

		float Alpha = g_Config.m_ClTeeTrailAlpha / 100.0f;
		ColorRGBA Col = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClTeeTrailColor));
		if(TeeInfo.m_CustomColoredSkin && g_Config.m_ClTeeTrailUseTeeColor)
			Col = TeeInfo.m_ColorBody;
		else if(g_Config.m_ClTeeTrailUseTeeColor)
			Col = TeeInfo.m_BloodColor;

		ColorRGBA FullCol = Col.WithAlpha(Alpha);
		Graphics()->SetColor(FullCol);

		int TrailLength = g_Config.m_ClTeeTrailLength;
		float Width = g_Config.m_ClTeeTrailWidth;

		static std::vector<STrailPart> Trail;
		Trail.clear();

		// TODO: figure out why this is required
		if(!PredictPlayer)
			TrailLength += 2;

		// Fill trail list with initial positions
		for(int i = 0; i < TrailLength; i++)
		{
			STrailPart Part;
			int PosTick = StartTick - i;
			if(PredictPlayer)
			{
				if(GameClient()->m_aClients[ClientId].m_aPredTick[PosTick % 200] != PosTick)
					continue;
				Part.Pos = GameClient()->m_aClients[ClientId].m_aPredPos[PosTick % 200];
			}
			else
			{
				if(m_PositionTick[ClientId][PosTick % 200] != PosTick)
					continue;
				Part.Pos = m_PositionHistory[ClientId][PosTick % 200];
			}
			Part.Tick = PosTick;
			Part.UnMovedPos = Part.Pos;
			Trail.push_back(Part);
		}

		// Trim the ends if intratick is too big
		// this was not trivial to figure out
		int TrimTicks = (int)IntraTick;
		for(int i = 0; i < TrimTicks; i++)
			if((int)Trail.size() > 0)
				Trail.pop_back();

		// Stuff breaks if we have less than 3 points because we cannot calculate an angle between segments to preserve constant width
		// TODO: Pad the list with generated entries in the same direction as before
		if((int)Trail.size() < 3)
			continue;

		if(PredictPlayer)
			Trail.at(0).Pos = GameClient()->m_aClients[ClientId].m_RenderPos;
		else
			Trail.at(0).Pos = mix(PrevServerPos, CurServerPos, Client()->IntraGameTick(g_Config.m_ClDummy));

		Trail.at(Trail.size() - 1).Pos = mix(Trail.at(Trail.size() - 1).Pos, Trail.at(Trail.size() - 2).Pos, std::fmod(IntraTick, 1.0f));

		// Set progress
		for(int i = 0; i < (int)Trail.size(); i++)
		{
			float Size = float(Trail.size() - 1 + TrimTicks);
			STrailPart &Part = Trail.at(i);
			if(i == 0)
				Part.Progress = 0.0f;
			else if(i == (int)Trail.size() - 1)
				Part.Progress = 1.0f;
			else
				Part.Progress = ((float)(i) + IntraTick - 1.0f) / (Size - 1.0f);

			Part.Col = Col;
			Part.Alpha = Alpha;
			if(g_Config.m_ClTeeTrailRainbow)
			{
				float Cycle = (1.0f / TrailLength) * 0.5f;
				float Hue = std::fmod(((StartTick - i + 7247 * ClientId) % 1000000) * Cycle, 1.0f);
				Part.Col = color_cast<ColorRGBA>(ColorHSLA(Hue, 1.0f, 0.5f));
			}
			if(g_Config.m_ClTeeTrailSpeed)
			{
				float Speed = 0.0f;
				if(i == 0)
					Speed = distance(Trail.at(i + 1).UnMovedPos, Trail.at(i).UnMovedPos) / std::abs(Trail.at(i + 1).Tick - Trail.at(i).Tick);
				else
					Speed = distance(Part.UnMovedPos, Trail.at(i - 1).UnMovedPos) / std::abs(Part.Tick - Trail.at(i - 1).Tick);
				Part.Col = color_cast<ColorRGBA>(ColorHSLA(65280 * ((int)(Speed * Speed / 12.5f) + 1)).UnclampLighting(ColorHSLA::DARKEST_LGT));
			}

			Part.Width = g_Config.m_ClTeeTrailWidth;
			if(g_Config.m_ClTeeTrailTaper)
				Part.Width = g_Config.m_ClTeeTrailWidth * (1.0 - Part.Progress);
			if(g_Config.m_ClTeeTrailFade)
				Part.Alpha = Alpha * (1.0 - Part.Progress);
		}

		auto NewEnd = std::unique(Trail.begin(), Trail.end());
		Trail.erase(NewEnd, Trail.end());

		if((int)Trail.size() < 3)
			continue;

		// Calculate the widths
		for(int i = 0; i < (int)Trail.size(); i++)
		{
			STrailPart &Part = Trail.at(i);
			vec2 PrevPos;
			vec2 Pos = Trail.at(i).Pos;
			vec2 NextPos;

			if(i == 0)
			{
				vec2 Direction = normalize(Trail.at(i + 1).Pos - Pos);
				PrevPos = Pos - Direction;
			}
			else
				PrevPos = Trail.at(i - 1).Pos;

			if(i == (int)Trail.size() - 1)
			{
				vec2 Direction = normalize(Pos - Trail.at(i - 1).Pos);
				NextPos = Pos + Direction;
			}
			else
				NextPos = Trail.at(i + 1).Pos;

			vec2 NextDirection = normalize(NextPos - Pos);
			vec2 PrevDirection = normalize(Pos - PrevPos);

			vec2 Normal = vec2(-PrevDirection.y, PrevDirection.x);
			Part.Normal = Normal;
			vec2 Tanget = normalize(NextDirection + PrevDirection);
			if(Tanget == vec2(0, 0))
				Tanget = Normal;

			vec2 PerpVec = vec2(-Tanget.y, Tanget.x);
			Width = Part.Width;
			float ScaledWidth = Width / dot(Normal, PerpVec);
			float TopScaled = ScaledWidth;
			float BotScaled = ScaledWidth;
			if(dot(PrevDirection, Tanget) > 0.0f)
				TopScaled = std::min(Width * 3.0f, TopScaled);
			else
				BotScaled = std::min(Width * 3.0f, BotScaled);

			vec2 Top = Pos + PerpVec * ScaledWidth;
			vec2 Bot = Pos - PerpVec * ScaledWidth;
			Part.Top = Top;
			Part.Bot = Bot;

			// Bevel Cap
			if(dot(PrevDirection, NextDirection) < -0.25f)
			{
				Top = Pos + Tanget * Width;
				Bot = Pos - Tanget * Width;
				Part.Top = Top;
				Part.Bot = Bot;

				float Det = PrevDirection.x * NextDirection.y - PrevDirection.y * NextDirection.x;
				if(Det >= 0.0f)
				{
					if(i > 0)
						Trail.at(i).Flip = true;
				}
				else // <-Left Direction
				{
					Part.Top = Bot;
					Part.Bot = Top;
					if(i > 0)
						Trail.at(i).Flip = true;
				}
			}
		}

		if(LineMode)
			Graphics()->LinesBegin();
		else
			Graphics()->TrianglesBegin();

		int SegmentCount = (int)Trail.size() - 1;
		// Draw the trail
		for(int i = 0; i < SegmentCount; i++)
		{
			float InverseProgress = 1.0 - Trail.at(i).Progress;
			if(g_Config.m_ClTeeTrailFade)
			{
				float FadeAlpha = Alpha * InverseProgress;
				FullCol = Col.WithAlpha(FadeAlpha);
				Graphics()->SetColor(FullCol);
			}

			vec2 Pos = Trail.at(i).Pos;
			vec2 NextPos = Trail.at(i + 1).Pos;
			if(distance(Pos, NextPos) > 600.0f)
				continue;
			Graphics()->SetColor(FullCol);
			if(LineMode)
			{
				Graphics()->SetColor(Trail.at(i).Col.WithAlpha(Trail.at(i).Alpha));
				LineItem = IGraphics::CLineItem(Pos.x, Pos.y, NextPos.x, NextPos.y);
				Graphics()->LinesDraw(&LineItem, 1);
			}
			else
			{
				vec2 Top = Trail.at(i).Top;
				vec2 Bot = Trail.at(i).Bot;

				vec2 NextTop = Trail.at(i + 1).Top;
				vec2 NextBot = Trail.at(i + 1).Bot;
				if(Trail.at(i).Flip)
				{
					Top = Trail.at(i).Bot;
					Bot = Trail.at(i).Top;
				}

				ColorRGBA ColNow = Trail.at(i).Col.WithAlpha(Trail.at(i).Alpha);
				ColorRGBA ColNext = Trail.at(i + 1).Col.WithAlpha(Trail.at(i + 1).Alpha);
				Graphics()->SetColor4(ColNext, ColNext, ColNow, ColNow);
				// IGraphics::CFreeformItem FreeformItem(Top.x, Top.y, Bot.x, Bot.y, NextTop.x, NextTop.y, NextBot.x, NextBot.y);
				IGraphics::CFreeformItem FreeformItem(NextTop.x, NextTop.y, NextBot.x, NextBot.y, Top.x, Top.y, Bot.x, Bot.y);

				Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
			}
		}
		if(LineMode)
			Graphics()->LinesEnd();
		else
			Graphics()->TrianglesEnd();
	}
}
