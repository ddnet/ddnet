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

// Surely there must be a way to get this value somewhere but I can't find it
// Client()->IntraGameTick() doesn't work because it can be >1.0f and doesn't start from 0.0f. It's highly cursed
void CTrails::ManageServerIntraTick()
{
	static int Tick = 0;
	static float TimeBetweenTicks = 0.0f;
	static float TimeThisTick = 0.0f;
	if(Tick != Client()->GameTick(g_Config.m_ClDummy))
	{
		Tick = Client()->GameTick(g_Config.m_ClDummy);
		TimeBetweenTicks = TimeBetweenTicks * 0.9f + TimeThisTick * 0.1f;
		TimeThisTick = 0.0f;
	}
	TimeThisTick += Client()->RenderFrameTime();
	m_RealIntraTick = TimeThisTick / TimeBetweenTicks;
	m_RealIntraTick = std::clamp(m_RealIntraTick, 0.0f, 1.0f);
	// char aBuf[32];
	// str_format(aBuf, sizeof(aBuf), "%.2f", Client()->IntraGameTick(g_Config.m_ClDummy));
	// GameClient()->Echo(aBuf);
}

void CTrails::OnRender()
{
	if(!g_Config.m_ClTeeTrail)
		return;

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!m_pClient->m_Snap.m_pGameInfoObj)
		return;

	ManageServerIntraTick();
	Graphics()->TextureClear();
	GameClient()->RenderTools()->MapScreenToGroup(GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y, GameClient()->Layers()->GameGroup(), GameClient()->m_Camera.m_Zoom);

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		bool Local = m_pClient->m_Snap.m_LocalClientId == ClientId;
		if(!g_Config.m_ClTeeTrailOthers && !Local)
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
			IntraTick = m_RealIntraTick;
			IntraTick = Client()->IntraGameTick(g_Config.m_ClDummy);
		}

		vec2 CurServerPos = vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_Y);
		vec2 PrevServerPos = vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev.m_Y);
		m_PositionHistory[ClientId][GameTick % 200] = mix(PrevServerPos, CurServerPos, Client()->IntraGameTick(g_Config.m_ClDummy));
		m_PositionTick[ClientId][GameTick % 200] = GameTick;

		// Slightly messy way to make the position of our player accurate without ugly logic in the loop
		vec2 SavedTempPredPos = GameClient()->m_aClients[ClientId].m_aPredPos[PredTick % 200];
		GameClient()->m_aClients[ClientId].m_aPredPos[PredTick % 200] = GameClient()->m_aClients[ClientId].m_RenderPos;

		IGraphics::CLineItem LineItem;
		bool LineMode = g_Config.m_ClTeeTrailWidth == 0;

		float Alpha = g_Config.m_ClTeeTrailAlpha / 100.0f;
		ColorRGBA Col = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClTeeTrailColor));
		if(TeeInfo.m_CustomColoredSkin && g_Config.m_ClTeeTrailUseTeeColor)
			Col = TeeInfo.m_ColorBody;
		else if(g_Config.m_ClTeeTrailUseTeeColor)
			Col = TeeInfo.m_BloodColor;

		ColorRGBA FullCol = Col.WithAlpha(Alpha);
		ColorRGBA PrevCol = FullCol;
		Graphics()->SetColor(FullCol);
		vec2 PrevPrevPos = vec2(0, 0);
		int PrevPrevPosTick = 0;
		bool PrevPrevInit = false;
		vec2 PrevPos = vec2(0, 0);
		int PrevPosTick = 0;
		bool PrevInit = false;
		vec2 Pos = vec2(0, 0);
		bool Init = false;

		int TrailLength = g_Config.m_ClTeeTrailLength;

		vec2 PerpVec = vec2(0, 0);
		vec2 PrevPerpVec = vec2(0, 0);
		bool PerpInit = false;
		float Progress = 0.0f;
		vec2 PrevTop = vec2(0, 0);
		vec2 PrevBot = vec2(0, 0);
		bool WidthInit = false;
		float Width = g_Config.m_ClTeeTrailWidth;
		int DrawTick = -1;
		if(false)
		{
			if(!LineMode)
				Graphics()->TrianglesBegin();
			else
				Graphics()->LinesBegin();

			// vec2 PrevNormal = vec2(0, 0);
			bool FirstDraw = true;
			// This is a huge mess but it works fine, don't copy any of this
			for(int i = 0; i < TrailLength; i++)
			{
				int PosTick = StartTick - i;
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
					int LastDrawPos = PosTick;
					bool ValidFuture = (m_PositionTick[ClientId][(LastDrawPos - 2) % 200] == LastDrawPos - 2) ||
							   (m_PositionTick[ClientId][(LastDrawPos - 1) % 200] == LastDrawPos - 1);
					LastDraw = !ValidFuture || i >= TrailLength - 2;
				}
				if(Pos == PrevPos)
					continue;

				// int OffsetTick = 1;
				// if(!PredictPlayer)
				//	OffsetTick = 2;

				Progress = ((double)(StartTick - PosTick) + IntraTick) / (TrailLength - 1);
				float InverseProgress = 1.0f - Progress;

				if(g_Config.m_ClTeeTrailRainbow)
				{
					float Cycle = (1.0f / TrailLength) * 0.5f;
					float Hue = std::fmod(((PosTick + 6361 * ClientId) % 1000000) * Cycle, 1.0f);
					Col = color_cast<ColorRGBA>(ColorHSLA(Hue, 1.0f, 0.5f));
					FullCol = Col.WithAlpha(Alpha);
					Graphics()->SetColor(FullCol);
				}

				if(g_Config.m_ClTeeTrailTaper)
					Width = g_Config.m_ClTeeTrailWidth * InverseProgress;

				if(g_Config.m_ClTeeTrailFade)
				{
					float FadeAlpha = Alpha * InverseProgress;
					FullCol = Col.WithAlpha(FadeAlpha);
					Graphics()->SetColor(FullCol);
				}
				if(PrevPrevInit && distance(PrevPos, Pos) < 600.0f)
				{
					DrawTick = PosTick;
					if(LastDraw && LineMode)
						Pos = mix(Pos, PrevPos, std::fmod(IntraTick / 2.0f, 1.0f));
					else if(LastDraw && !PredictPlayer)
					{
						// PrevPos = mix(PrevPos, PrevPrevPos, IntraTick / 2.0f);
					}
					else if(LastDraw)
						PrevPos = mix(PrevPos, PrevPrevPos, IntraTick);
					if(LineMode)
					{
						LineItem = IGraphics::CLineItem(PrevPos.x, PrevPos.y, Pos.x, Pos.y);
						Graphics()->LinesDraw(&LineItem, 1);
					}
					vec2 PrevDirection = normalize(PrevPos - PrevPrevPos);
					vec2 Direction = normalize(Pos - PrevPos);
					vec2 Normal = vec2(-PrevDirection.y, PrevDirection.x);

					// if(!WidthInit)
					//	PrevNormal = Normal;

					vec2 Tanget = normalize(normalize(Pos - PrevPos) + normalize(PrevPos - PrevPrevPos));
					if(Tanget == vec2(0, 0))
						Tanget = Normal;
					PerpVec = vec2(-Tanget.y, Tanget.x);
					if(!PerpInit)
					{
						PrevPerpVec = Normal;
					}

					float ScaledWidth = Width /*/ dot(Normal, PerpVec)*/;
					float TopScaled = ScaledWidth;
					float BotScaled = ScaledWidth;
					if(dot(PrevDirection, Tanget) > 0.0f)
						TopScaled = std::min(Width * 3.0f, TopScaled);
					else
						BotScaled = std::min(Width * 3.0f, BotScaled);

					vec2 Top = PrevPos + PerpVec * TopScaled;
					vec2 Bot = PrevPos - PerpVec * BotScaled;
					vec2 TempTop = Top;
					vec2 TempBot = Bot;

					if(dot(PrevDirection, Direction) < -0.25f)
					{
						Top = PrevPos - Tanget * Width;
						Bot = PrevPos + Tanget * Width;
						TempTop = Top;
						TempBot = Bot;
						float Det = PrevDirection.x * Direction.y - PrevDirection.y * Direction.x;
						if(Det >= 0.0f)
						{
							vec2 TempPos = PrevTop;
							PrevTop = PrevBot;
							PrevBot = TempPos;
							ScaledWidth = 0.0f;
						}
						else
						{
							TempTop = Bot;
							TempBot = Top;
						}
					}
					// float MinGap = length(Normal - PrevNormal) * ScaledWidth;
					// if(distance(PrevPos, Pos) < MinGap)
					//	continue;

					if(!WidthInit)
					{
						Width = g_Config.m_ClTeeTrailWidth;
						PrevTop = PrevPrevPos + PrevPerpVec * Width;
						PrevBot = PrevPrevPos - PrevPerpVec * Width;
					}
					if(distance(PrevPos, PrevPrevPos) < 600.0f && !LineMode)
					{
						Graphics()->SetColor4(FullCol, FullCol, PrevCol, PrevCol);
						IGraphics::CFreeformItem FreeformItem(Top.x, Top.y, Bot.x, Bot.y, PrevTop.x, PrevTop.y, PrevBot.x, PrevBot.y);
						Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
					}
					PrevTop = TempTop;
					PrevBot = TempBot;
					// PrevNormal = Normal;
					WidthInit = true;

					PrevPerpVec = PerpVec, PerpInit = true;
				}
				if(PrevInit)
					PrevPrevPos = PrevPos, PrevPrevPosTick = PrevPosTick, PrevPrevInit = true;
				if(Init)
					PrevPos = Pos, PrevPosTick = PosTick, PrevInit = true;

				PrevCol = FullCol;

				// if(LastDraw)
				//	break;
			}

			if(!LineMode)
				Graphics()->TrianglesEnd();
			else
				Graphics()->LinesEnd();
		}

		if(true)
		{
			// ATTEMPT 2
			static std::vector<STrailPart> Trail;
			Trail.clear();

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
				Trail.push_back(Part);
			}

			// Trim the ends if intratick is too big
			// this was not trivial to figure out
			int TrimTicks = (int)IntraTick;
			for(int i = 0; i < TrimTicks; i++)
				Trail.pop_back();

			// Stuff breaks if we have less than 3 points because we cannot calculate an angle between segments to preserve constant width
			// TODO: Pad the list with generated entries in the same direction as before
			if((int)Trail.size() < 3)
				continue;

			float Trim = 0.0f;
			if(!PredictPlayer)
				Trim = 1.0f;

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
					Part.Progress = ((float)(i) + IntraTick - Trim) / (Size - Trim);

				Part.Col = Col;
				Part.Alpha = Alpha;
				if(g_Config.m_ClTeeTrailRainbow)
				{
					float Cycle = (1.0f / TrailLength) * 0.5f;
					float Hue = std::fmod(((StartTick - i + 6361 * ClientId) % 1000000) * Cycle, 1.0f);
					Part.Col = color_cast<ColorRGBA>(ColorHSLA(Hue, 1.0f, 0.5f));
				}
				if(g_Config.m_ClTeeTrailTaper)
					Width = g_Config.m_ClTeeTrailWidth * (1.0 - Part.Progress);
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

				PerpVec = vec2(-Tanget.y, Tanget.x);
				float ScaledWidth = Width / dot(Normal, PerpVec);
				// float TopScaled = ScaledWidth;
				// float BotScaled = ScaledWidth;
				// if(dot(PrevDirection, Tanget) > 0.0f)
				//	TopScaled = std::min(Width * 3.0f, TopScaled);
				// else
				//	BotScaled = std::min(Width * 3.0f, BotScaled);

				vec2 Top = Pos + PerpVec * ScaledWidth;
				vec2 Bot = Pos - PerpVec * ScaledWidth;
				vec2 TempTop = Top;
				vec2 TempBot = Bot;

				// Bevel Cap
				if(dot(PrevDirection, NextDirection) < -0.25f)
				{
					Top = Pos + Tanget * Width;
					Bot = Pos - Tanget * Width;
					TempTop = Top;
					TempBot = Bot;
					float Det = PrevDirection.x * NextDirection.y - PrevDirection.y * NextDirection.x;
					if(Det >= 0.0f)
					{
						// vec2 TempPos = PrevTop;
						// PrevTop = PrevBot;
						// PrevBot = TempPos;
						// if(i > 0)
						//	Trail.at(i - 1).Flip = true;
						if(i > 0)
							Trail.at(i).Flip = true;
					}
					else // <-Left
					{
						TempTop = Bot;
						TempBot = Top;
						if(i > 0)
							Trail.at(i).Flip = true;
					}
				}

				// float InverseProgress = 1.0 - Part.Progress;
				Part.Top = TempTop;
				Part.Bot = TempBot;
			}
			if(LineMode)
				Graphics()->LinesBegin();
			else
				Graphics()->TrianglesBegin();

			int SegmentCount = (int)Trail.size() - 1;
			// Draw the trail list
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
				Graphics()->SetColor(FullCol);
				if(LineMode)
				{
					Graphics()->SetColor(Trail.at(i).Col);
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
		GameClient()->m_aClients[ClientId].m_aPredPos[PredTick % 200] = SavedTempPredPos;
		m_PositionHistory[ClientId][GameTick % 200] = CurServerPos;
	}
}