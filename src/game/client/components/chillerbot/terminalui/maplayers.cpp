// ChillerDragon 2022 - chillerbot

#include <engine/client/serverbrowser.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>

#include <base/terminalui.h>

#include <game/mapitems.h>

#include <csignal>

#include "terminalui.h"

#if defined(CONF_CURSES_CLIENT)

inline bool CTerminalUI::IsPlayerInfoAvailable(int ClientID) const
{
	const void *pPrevInfo = Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_PLAYERINFO, ClientID);
	const void *pInfo = Client()->SnapFindItem(IClient::SNAP_CURRENT, NETOBJTYPE_PLAYERINFO, ClientID);
	return pPrevInfo && pInfo;
}

void CTerminalUI::RenderGame()
{
	m_NextRender--;
	if(m_NextRender > 0)
		return;
	m_NextRender = 60; // render every 60 what every this isy
	int mx = getmaxx(g_pGameWindow);
	int my = getmaxy(g_pGameWindow);
	int offY = 5;
	int offX = 2;
	if(my < 20)
		offY = 2;
	int width = minimum(128, mx - 3);
	int height = minimum(32, my - 2);
	if(height < 2)
		return;
	DrawBorders(g_pGameWindow, offX, offY - 1, width, height + 2);

	for(int i = 0; i < height; i++)
		mvwprintw(g_pGameWindow, offY + i, offX, "|%-*s|", width - 2, " loading ... ");

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
		wrefresh(g_pGameWindow);
	}
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
			for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
			{
				if(!m_pClient->m_Snap.m_aCharacters[ClientID].m_Active || !IsPlayerInfoAvailable(ClientID))
					continue;

				// dbg_msg("chiller", "%d / %d", m_pClient->m_aClients[ClientID].m_RenderCur.m_X, mx);
				int PlayerX = m_pClient->m_aClients[ClientID].m_RenderCur.m_X;
				int PlayerY = m_pClient->m_aClients[ClientID].m_RenderCur.m_Y;
				if(PlayerX > mx - 16 && PlayerX < mx + 16)
					if(PlayerY > my - 16 && PlayerY < my + 16)
						mvwprintw(g_pGameWindow, offY + renderY, offX + renderX, "o");
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
	/*
		aFrame

		First dimension is the height
		Second dimension is the width

		The second dimension is supposed to hold 64 letters max
		but has more byte size for utf8 characters
	*/
	char aFrame[32][MAX_FRAME_WIDTH * 4]; // tee aka center is at 8/16   y/x
	int aFrameLetterCount[32] = {0}; // TODO: do we need this? Should always be 32
	int aFrameByteCount[32] = {0};
	// init with spaces
	for(int i = 0; i < 32; i++)
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
			if(Index)
			{
				// dbg_msg("map", "draw tile=%d at x: %.2f y: %.2f w: %.2f h: %.2f", Index, x*Scale, y*Scale, Scale, Scale);
				// dbg_msg("map", "absolut tile x: %d y: %d       tee x: %.2f y: %.2f", renderX, renderY, static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X)/32.0f, static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_Y)/32.0f);
				// dbg_msg("map", "array pos  tile x: %d y: %d\n", renderX, renderY);
				if(renderX > 0 && renderX < 64 && renderY > 0 && renderY < 32)
				{
					if(Index == TILE_SOLID)
					{
						// TODO: use ▆
						// aFrame[renderY][renderX] = '#';
						str_append(aFrame[renderY], "█", sizeof(aFrame));
						aFrameByteCount[renderY] += (int)sizeof("█");
					}
					else if(Index == TILE_FREEZE)
					{
						// aFrame[renderY][renderX] = 'x'; // TODO: use ▒
						str_append(aFrame[renderY], "▒", sizeof(aFrame));
						aFrameByteCount[renderY] += (int)sizeof("▒");
					}
					else if(Index == TILE_UNFREEZE)
					{
						str_append(aFrame[renderY], "░", sizeof(aFrame));
						aFrameByteCount[renderY] += (int)sizeof("░");
					}
					else if(Index == TILE_DEATH)
					{
						str_append(aFrame[renderY], "x", sizeof(aFrame));
						aFrameByteCount[renderY] += (int)sizeof("x");
					}
					else if(Index == TILE_NOHOOK)
					{
						// 🮽
						str_append(aFrame[renderY], "🮽", sizeof(aFrame));
						aFrameByteCount[renderY] += (int)sizeof("🮽");
					}
					else
					{
						// aFrame[renderY][renderX] = '?';
						str_append(aFrame[renderY], " ", sizeof(aFrame));
						aFrameByteCount[renderY]++;
						// char aIndex[16];
						// str_format(aIndex, sizeof(aIndex), "[%d]", Index);
						// str_append(aFrame[renderY], aIndex, sizeof(aFrame));
						// aFrameByteCount[renderY] += str_length(aIndex);
					}
					rendered_tiles++;
					aFrameLetterCount[renderY]++;
				}
				// aFrame[15][32] = 'o'; // tee in center
			}
			else
			{
				str_append(aFrame[renderY], " ", sizeof(aFrame));
				aFrameLetterCount[renderY]++;
				aFrameByteCount[renderY]++;
				// dbg_msg("map", "skip at x: %.2f y: %.2f w: %.2f h: %.2f", x*Scale, y*Scale, Scale, Scale);
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
		mvwprintw(g_pGameWindow, offY + y, offX, "|%-*s|", (WinWidth - 2) + (aFrameByteCount[y] - aFrameLetterCount[y]), aFrame[y]);
	}
	// printf("------------- tiles: %d \n\n", rendered_tiles);
	// printf("%s\n-------------\n", aFrame[15]);
}

#endif
