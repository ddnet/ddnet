#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/render.h>

#include "trails.h"
#include <game/client/gameclient.h>

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
		ClearHistory(i);
}
void CTrails::ClearHistory(int ClientId)
{
	for(int i = 0; i < 200; ++i)
		m_History[ClientId][i] = {{}, -1};
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
		m_History[ClientId][GameTick % 200] = {
			mix(PrevServerPos, CurServerPos, IntraTick),
			GameTick,
		};

		// // NOTE: this is kind of a hack to fix 25tps. This fixes flickering when using the speed mode
		// m_History[ClientId][(GameTick + 1) % 200] = m_History[ClientId][GameTick % 200];
		// m_History[ClientId][(GameTick + 2) % 200] = m_History[ClientId][GameTick % 200];

		IGraphics::CLineItem LineItem;
		bool LineMode = g_Config.m_ClTeeTrailWidth == 0;

		float Alpha = g_Config.m_ClTeeTrailAlpha / 100.0f;
		// Taken from players.cpp
		if(ClientId == -2)
			Alpha *= g_Config.m_ClRaceGhostAlpha / 100.0f;
		else if(ClientId < 0 || m_pClient->IsOtherTeam(ClientId))
			Alpha *= g_Config.m_ClShowOthersAlpha / 100.0f;

		int TrailLength = g_Config.m_ClTeeTrailLength;
		float Width = g_Config.m_ClTeeTrailWidth;

		static std::vector<STrailPart> s_Trail;
		s_Trail.clear();

		// TODO: figure out why this is required
		if(!PredictPlayer)
			TrailLength += 2;
		bool TrailFull = false;
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
				if(i == TrailLength - 1)
					TrailFull = true;
			}
			else
			{
				if(m_History[ClientId][PosTick % 200].m_Tick != PosTick)
					continue;
				Part.Pos = m_History[ClientId][PosTick % 200].m_Pos;
				if(i == TrailLength - 2 || i == TrailLength - 3)
					TrailFull = true;
			}
			Part.UnmovedPos = Part.Pos;
			Part.Tick = PosTick;
			s_Trail.push_back(Part);
		}

		// Trim the ends if intratick is too big
		// this was not trivial to figure out
		int TrimTicks = (int)IntraTick;
		for(int i = 0; i < TrimTicks; i++)
			if((int)s_Trail.size() > 0)
				s_Trail.pop_back();

		// Stuff breaks if we have less than 3 points because we cannot calculate an angle between segments to preserve constant width
		// TODO: Pad the list with generated entries in the same direction as before
		if((int)s_Trail.size() < 3)
			continue;

		if(PredictPlayer)
			s_Trail.at(0).Pos = GameClient()->m_aClients[ClientId].m_RenderPos;
		else
			s_Trail.at(0).Pos = mix(PrevServerPos, CurServerPos, IntraTick);

		if(TrailFull)
			s_Trail.at(s_Trail.size() - 1).Pos = mix(s_Trail.at(s_Trail.size() - 1).Pos, s_Trail.at(s_Trail.size() - 2).Pos, std::fmod(IntraTick, 1.0f));

		// Set progress
		for(int i = 0; i < (int)s_Trail.size(); i++)
		{
			float Size = float(s_Trail.size() - 1 + TrimTicks);
			STrailPart &Part = s_Trail.at(i);
			if(i == 0)
				Part.Progress = 0.0f;
			else if(i == (int)s_Trail.size() - 1)
				Part.Progress = 1.0f;
			else
				Part.Progress = ((float)i + IntraTick - 1.0f) / (Size - 1.0f);

			switch(g_Config.m_ClTeeTrailColorMode)
			{
				case COLORMODE_SOLID:
					Part.Col = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClTeeTrailColor));
					break;
				case COLORMODE_TEE:
					if(TeeInfo.m_CustomColoredSkin)
						Part.Col = TeeInfo.m_ColorBody;
					else
						Part.Col = TeeInfo.m_BloodColor;
					break;
				case COLORMODE_RAINBOW:
				{
					float Cycle = (1.0f / TrailLength) * 0.5f;
					float Hue = std::fmod(((Part.Tick + 6361 * ClientId) % 1000000) * Cycle, 1.0f);
					Part.Col = color_cast<ColorRGBA>(ColorHSLA(Hue, 1.0f, 0.5f));
					break;
				}
				case COLORMODE_SPEED:
				{
					float Speed = 0.0f;
					if(s_Trail.size() > 3)
					{
						if(i < 2)
							Speed = distance(s_Trail.at(i + 2).UnmovedPos, Part.UnmovedPos) / std::abs(s_Trail.at(i + 2).Tick - Part.Tick);
						else
							Speed = distance(Part.UnmovedPos, s_Trail.at(i - 2).UnmovedPos) / std::abs(Part.Tick - s_Trail.at(i - 2).Tick);
					}
					Part.Col = color_cast<ColorRGBA>(ColorHSLA(65280 * ((int)(Speed * Speed / 12.5f) + 1)).UnclampLighting(ColorHSLA::DARKEST_LGT));
					break;
				}
				default:
					dbg_assert(false, "Invalid value for g_Config.m_ClTeeTrailColorMode");
					dbg_break();
			}

			Part.Col.a = Alpha;
			if(g_Config.m_ClTeeTrailFade)
				Part.Col.a *= 1.0 - Part.Progress;

			Part.Width = g_Config.m_ClTeeTrailWidth;
			if(g_Config.m_ClTeeTrailTaper)
				Part.Width = g_Config.m_ClTeeTrailWidth * (1.0 - Part.Progress);
		}

		// Remove duplicate elements (those with same Pos)
		auto NewEnd = std::unique(s_Trail.begin(), s_Trail.end());
		s_Trail.erase(NewEnd, s_Trail.end());

		if((int)s_Trail.size() < 3)
			continue;

		// Calculate the widths
		for(int i = 0; i < (int)s_Trail.size(); i++)
		{
			STrailPart &Part = s_Trail.at(i);
			vec2 PrevPos;
			vec2 Pos = s_Trail.at(i).Pos;
			vec2 NextPos;

			if(i == 0)
			{
				vec2 Direction = normalize(s_Trail.at(i + 1).Pos - Pos);
				PrevPos = Pos - Direction;
			}
			else
				PrevPos = s_Trail.at(i - 1).Pos;

			if(i == (int)s_Trail.size() - 1)
			{
				vec2 Direction = normalize(Pos - s_Trail.at(i - 1).Pos);
				NextPos = Pos + Direction;
			}
			else
				NextPos = s_Trail.at(i + 1).Pos;

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

			vec2 Top = Pos + PerpVec * TopScaled;
			vec2 Bot = Pos - PerpVec * BotScaled;
			Part.Top = Top;
			Part.Bot = Bot;

			// Bevel Cap
			if(dot(PrevDirection, NextDirection) < -0.25f)
			{
				Top = Pos + Tanget * Width;
				Bot = Pos - Tanget * Width;

				float Det = PrevDirection.x * NextDirection.y - PrevDirection.y * NextDirection.x;
				if(Det >= 0.0f)
				{
					Part.Top = Top;
					Part.Bot = Bot;
					if(i > 0)
						s_Trail.at(i).Flip = true;
				}
				else // <-Left Direction
				{
					Part.Top = Bot;
					Part.Bot = Top;
					if(i > 0)
						s_Trail.at(i).Flip = true;
				}
			}
		}

		if(LineMode)
			Graphics()->LinesBegin();
		else
			Graphics()->TrianglesBegin();

		// Draw the trail
		for(int i = 0; i < (int)s_Trail.size() - 1; i++)
		{
			const STrailPart &Part = s_Trail.at(i);
			const STrailPart &NextPart = s_Trail.at(i + 1);
			if(distance(Part.Pos, NextPart.Pos) > 120.0f)
				continue;

			if(LineMode)
			{
				Graphics()->SetColor(Part.Col);
				LineItem = IGraphics::CLineItem(Part.Pos.x, Part.Pos.y, NextPart.Pos.x, NextPart.Pos.y);
				Graphics()->LinesDraw(&LineItem, 1);
			}
			else
			{
				vec2 Top, Bot;
				if(Part.Flip)
				{
					Top = Part.Bot;
					Bot = Part.Top;
				}
				else
				{
 					Top = Part.Top;
					Bot = Part.Bot;
				}

				vec2 NextTop = NextPart.Top;
				vec2 NextBot = NextPart.Bot;

				Graphics()->SetColor4(NextPart.Col, NextPart.Col, Part.Col, Part.Col);
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
