// ChillerDragon 2022 - chillerbot

#include <engine/client/serverbrowser.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/pickup.h>

#include <base/terminalui.h>

#include <base/chillerbot/dbg_print.h>

#include <game/mapitems.h>

#include <csignal>

#include "terminalui.h"

#if defined(CONF_CURSES_CLIENT)

inline bool CTerminalUI::IsPlayerInfoAvailable(int ClientId) const
{
	const void *pPrevInfo = Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_PLAYERINFO, ClientId);
	const void *pInfo = Client()->SnapFindItem(IClient::SNAP_CURRENT, NETOBJTYPE_PLAYERINFO, ClientId);
	return pPrevInfo && pInfo;
}

void CTerminalUI::RenderPickup(const CNetObj_Pickup *pPrev, const CNetObj_Pickup *pCurrent, bool IsPredicted)
{
	// float IntraTick = IsPredicted ? Client()->PredIntraGameTick(g_Config.m_ClDummy) : Client()->IntraGameTick(g_Config.m_ClDummy);
	// vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), IntraTick);
	if(pCurrent->m_Type == POWERUP_HEALTH)
	{
		// TODO: render this ❤️
	}
}

void CTerminalUI::RenderItems()
{
	int SwitcherTeam = m_pClient->SwitchStateTeam();
	bool UsePredicted = GameClient()->Predict() && GameClient()->AntiPingGunfire();
	auto &aSwitchers = GameClient()->Switchers();
	bool IsSuper = m_pClient->IsLocalCharSuper();
	int Ticks = Client()->GameTick(g_Config.m_ClDummy) % Client()->GameTickSpeed();
	bool BlinkingPickup = (Ticks % 22) < 4;
	if(UsePredicted)
	{
		for(auto *pPickup = (CPickup *)GameClient()->m_PredictedWorld.FindFirst(CGameWorld::ENTTYPE_PICKUP); pPickup; pPickup = (CPickup *)pPickup->NextEntity())
		{
			if(!IsSuper && pPickup->m_Layer == LAYER_SWITCH && pPickup->m_Number > 0 && pPickup->m_Number < (int)aSwitchers.size() && !aSwitchers[pPickup->m_Number].m_aStatus[SwitcherTeam] && BlinkingPickup)
				continue;

			if(pPickup->InDDNetTile())
			{
				if(auto *pPrev = (CPickup *)GameClient()->m_PrevPredictedWorld.GetEntity(pPickup->GetId(), CGameWorld::ENTTYPE_PICKUP))
				{
					CNetObj_Pickup Data, Prev;
					pPickup->FillInfo(&Data);
					pPrev->FillInfo(&Prev);
					RenderPickup(&Prev, &Data, true);
				}
			}
		}
	}
	for(const CSnapEntities &Ent : m_pClient->SnapEntities())
	{
		const IClient::CSnapItem Item = Ent.m_Item;
		const void *pData = Item.m_pData;
		const CNetObj_EntityEx *pEntEx = Ent.m_pDataEx;

		bool Inactive = false;
		if(pEntEx)
			Inactive = !IsSuper && pEntEx->m_SwitchNumber > 0 && pEntEx->m_SwitchNumber < (int)aSwitchers.size() && !aSwitchers[pEntEx->m_SwitchNumber].m_aStatus[SwitcherTeam];

		if(Item.m_Type == NETOBJTYPE_PICKUP)
		{
			if(Inactive && BlinkingPickup)
				continue;
			if(UsePredicted)
			{
				auto *pPickup = (CPickup *)GameClient()->m_GameWorld.FindMatch(Item.m_Id, Item.m_Type, pData);
				if(pPickup && pPickup->InDDNetTile())
					continue;
			}
			const void *pPrev = Client()->SnapFindItem(IClient::SNAP_PREV, Item.m_Type, Item.m_Id);
			if(pPrev)
				RenderPickup((const CNetObj_Pickup *)pPrev, (const CNetObj_Pickup *)pData);
		}
	}
}

void CTerminalUI::RenderGame()
{
	m_NextRender--;
	if(m_NextRender > 0)
		return;
	m_NextRender = 60; // render every 60 what every this isy
	int mx = getmaxx(g_GameWindow.m_pCursesWin);
	int my = getmaxy(g_GameWindow.m_pCursesWin);
	int offY = 5;
	int offX = 2;
	if(my < 20)
		offY = 2;
	int width = minimum(128, mx - 3);
	int height = minimum(32, my - 2);
	if(height < 2)
		return;
	g_GameWindow.DrawBorders(offX, offY - 1, width, height + 2);

	for(int i = 0; i < height; i++)
		mvwprintw(g_GameWindow.m_pCursesWin, offY + i, offX, "|%-*s|", width - 2, " loading ... ");

	CMapItemLayer *pLayer = (CMapItemLayer *)Layers()->GameLayer();
	if(!pLayer)
		return;

	CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
	CTile *pTiles = (CTile *)Layers()->Map()->GetData(pTMap->m_Data);
	unsigned int Size = Layers()->Map()->GetDataSize(pTMap->m_Data);
	if(Size >= pTMap->m_Width * pTMap->m_Height * sizeof(CTile))
	{
		vec4 Color = vec4(pTMap->m_Color.r / 255.0f, pTMap->m_Color.g / 255.0f, pTMap->m_Color.b / 255.0f, pTMap->m_Color.a / 255.0f);
		RenderTilemap(pTiles, offX, offY, width, height, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, 0,
			pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
		RenderPlayers(offX, offY, pTMap->m_Width, pTMap->m_Height);
	}
	RenderItems();
	wrefresh(g_GameWindow.m_pCursesWin);
}

void CTerminalUI::RenderPlayers(int offX, int offY, int w, int h)
{
	float Scale = 1.0; // chillerbot tile size is always one character
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	int render_dist = 16; // render tiles surroundign the tee
	ScreenX0 = (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X) / 32.0f) - (render_dist);
	ScreenY0 = (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_Y) / 32.0f) - (render_dist / 2);
	ScreenX1 = (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X) / 32.0f) + (render_dist);
	ScreenY1 = (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_Y) / 32.0f) + (render_dist / 2);
	// dbg_msg("screen", "x0: %2.f y0: %.2f x1: %.2f y1: %.2f", ScreenX0, ScreenY0, ScreenX1, ScreenY1);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	for(int y = StartY; y < EndY; y++)
	{
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(!"idk what this is")
			{
				if(mx < 0)
					mx = 0;
				if(mx >= w)
					mx = w - 1;
				if(my < 0)
					my = 0;
				if(my >= h)
					my = h - 1;
			}
			else
			{
				if(mx < 0)
					continue; // mx = 0;
				if(mx >= w)
					continue; // mx = w-1;
				if(my < 0)
					continue; // my = 0;
				if(my >= h)
					continue; // my = h-1;
			}

			int renderX = (x * Scale) - (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X) / 32.0f);
			int renderY = (y * Scale) - (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_Y) / 32.0f);
			renderX += 32;
			renderY += 16;
			mx *= 32;
			my *= 32;
			my += 64;
			for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
			{
				if(!m_pClient->m_Snap.m_aCharacters[ClientId].m_Active || !IsPlayerInfoAvailable(ClientId))
					continue;

				// int curX = m_pClient->m_aClients[ClientId].m_RenderCur.m_X;
				// DBG_II(curX, mx);

				int PlayerX = m_pClient->m_aClients[ClientId].m_RenderCur.m_X;
				int PlayerY = m_pClient->m_aClients[ClientId].m_RenderCur.m_Y;
				const char *pPlayerSkin = "o";
				if(m_pClient->m_Snap.m_aCharacters[ClientId].m_Cur.m_Weapon == WEAPON_NINJA)
					pPlayerSkin = "ø";
				if(PlayerX > mx - 16 && PlayerX < mx + 16)
					if(PlayerY > my - 16 && PlayerY < my + 16)
						mvwprintw(g_GameWindow.m_pCursesWin, offY + renderY, offX + renderX, "%s", pPlayerSkin);
			}
		}
	}
}

void CTerminalUI::RenderTilemap(CTile *pTiles, int offX, int offY, int WinWidth, int WinHeight, int w, int h, float Scale, vec4 Color, int RenderFlags, int ColorEnv, int ColorEnvOffset)
{
	if(!m_pClient->m_Snap.m_pLocalCharacter) // TODO: also render map if tee is dead and also add possibility to spectate
		return;
	if(WinWidth < 3)
		return;
	if(WinHeight < 3)
		return;

	static const int MAX_FRAME_WIDTH = 64;
	static const int MAX_FRAME_HEIGHT = 32;
	/*
		aFrame

		First dimension is the height
		Second dimension is the width

		The second dimension is supposed to hold 64 letters max
		but has more byte size for utf8 characters
	*/
	char aFrame[MAX_FRAME_HEIGHT][MAX_FRAME_WIDTH * 4]; // tee aka center is at 8/16   y/x
	int aFrameLetterCount[MAX_FRAME_HEIGHT] = {0}; // TODO: do we need this? Should always be 32
	int aFrameByteCount[MAX_FRAME_HEIGHT] = {0};
	// init with spaces
	for(int i = 0; i < MAX_FRAME_HEIGHT; i++)
	{
		mem_zero(aFrame[i], sizeof(aFrame[i]));
		// str_format(aFrame[i], sizeof(aFrame[i]), "%*s", (int)sizeof(aFrame[i]), " ");
	}
	int rendered_tiles = 0;
	// dbg_msg("render", "teeX: %.2f", static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X)/32.0f);
	Scale = 1.0; // chillerbot tile size is always one character
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	int render_dist = 16; // render tiles surroundign the tee
	ScreenX0 = (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X) / 32.0f) - (render_dist);
	ScreenY0 = (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_Y) / 32.0f) - (render_dist / 2);
	ScreenX1 = (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X) / 32.0f) + (render_dist);
	ScreenY1 = (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_Y) / 32.0f) + (render_dist / 2);
	// dbg_msg("screen", "x0: %2.f y0: %.2f x1: %.2f y1: %.2f", ScreenX0, ScreenY0, ScreenX1, ScreenY1);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	for(int y = StartY; y < EndY; y++)
	{
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(!"idk what this is")
			{
				if(mx < 0)
					mx = 0;
				if(mx >= w)
					mx = w - 1;
				if(my < 0)
					my = 0;
				if(my >= h)
					my = h - 1;
			}
			else
			{
				if(mx < 0)
					continue; // mx = 0;
				if(mx >= w)
					continue; // mx = w-1;
				if(my < 0)
					continue; // my = 0;
				if(my >= h)
					continue; // my = h-1;
			}

			int c = mx + my * w;
			unsigned char Index = pTiles[c].m_Index;
			int renderX = (x * Scale) - (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X) / 32.0f);
			int renderY = (y * Scale) - (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_Y) / 32.0f);
			// dbg_msg("x", "mx=%d my=%d renderX=%d renderY=%d", mx, my, renderX, renderY);
			renderX += 32;
			renderY += 16;
			bool InValidBounds = true;
			if(renderX >= MAX_FRAME_WIDTH || renderX < 0)
				InValidBounds = false;
			if(renderY >= MAX_FRAME_HEIGHT || renderY < 0)
				InValidBounds = false;
			// if(aFrameByteCount[renderY] >= MAX_FRAME_WIDTH - (int)str_length("█"))
			// 	InValidBounds = false;
			if(InValidBounds)
			{
				if(Index)
				{
					// dbg_msg("map", "draw tile=%d at x: %.2f y: %.2f w: %.2f h: %.2f", Index, x*Scale, y*Scale, Scale, Scale);
					// dbg_msg("map", "absolut tile x: %d y: %d       tee x: %.2f y: %.2f", renderX, renderY, static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X)/32.0f, static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_Y)/32.0f);
					// dbg_msg("map", "array pos  tile x: %d y: %d\n", renderX, renderY);
					if(Index == TILE_SOLID)
					{
						// TODO: use ▆
						// aFrame[renderY][renderX] = '#';
						str_append(aFrame[renderY], m_aTileSolidTexture, sizeof(aFrame[renderY]));
						aFrameByteCount[renderY] += (int)str_length(m_aTileSolidTexture);
					}
					else if(Index == TILE_FREEZE)
					{
						// aFrame[renderY][renderX] = 'x'; // TODO: use ▒
						str_append(aFrame[renderY], "▒", sizeof(aFrame[renderY]));
						aFrameByteCount[renderY] += (int)str_length("▒");
					}
					else if(Index == TILE_UNFREEZE)
					{
						str_append(aFrame[renderY], "░", sizeof(aFrame[renderY]));
						aFrameByteCount[renderY] += (int)str_length("░");
					}
					else if(Index == TILE_DEATH)
					{
						str_append(aFrame[renderY], "x", sizeof(aFrame[renderY]));
						aFrameByteCount[renderY] += (int)str_length("x");
					}
					else if(Index == TILE_NOHOOK)
					{
						// 🮽
						str_append(aFrame[renderY], m_aTileUnhookTexture, sizeof(aFrame[renderY]));
						aFrameByteCount[renderY] += (int)str_length(m_aTileUnhookTexture);
					}
					else if(Index == TILE_THROUGH || Index == TILE_THROUGH_CUT || Index == TILE_THROUGH_ALL || Index == TILE_THROUGH_DIR)
					{
						str_append(aFrame[renderY], "🔳", sizeof(aFrame[renderY]));
						aFrameByteCount[renderY] += (int)str_length("🔳");
					}
					else
					{
						// aFrame[renderY][renderX] = '?';
						str_append(aFrame[renderY], " ", sizeof(aFrame[renderY]));
						aFrameByteCount[renderY]++;
						// char aIndex[16];
						// str_format(aIndex, sizeof(aIndex), "[%d]", Index);
						// str_append(aFrame[renderY], aIndex, sizeof(aFrame[renderY]));
						// aFrameByteCount[renderY] += str_length(aIndex);
					}
					rendered_tiles++;
					aFrameLetterCount[renderY]++;
					// aFrame[15][32] = 'o'; // tee in center
				}
				else
				{
					str_append(aFrame[renderY], " ", sizeof(aFrame[renderY]));
					aFrameLetterCount[renderY]++;
					aFrameByteCount[renderY]++;
					// dbg_msg("map", "skip at x: %.2f y: %.2f w: %.2f h: %.2f", x*Scale, y*Scale, Scale, Scale);
				}
			}
			x += pTiles[c].m_Skip;
		}
	}
	if(rendered_tiles == 0)
		return;
	// render frame
	int RenderToHeight = minimum(32, WinHeight);
	for(int y = 0; y < RenderToHeight; y++)
	{
		// printf("%s\n", aFrame[y]);
		if(WinWidth < (int)sizeof(WinWidth))
			aFrame[y][WinWidth - 2] = '\0';
		mvwprintw(g_GameWindow.m_pCursesWin, offY + y, offX, "|%-*s|", (WinWidth - 2) + (aFrameByteCount[y] - aFrameLetterCount[y]), aFrame[y]);
	}
	// printf("------------- tiles: %d \n\n", rendered_tiles);
	// printf("%s\n-------------\n", aFrame[15]);
}

#endif
