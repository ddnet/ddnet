// ChillerDragon 2022 - chillerbot

#include <engine/client/serverbrowser.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>

#include <base/terminalui.h>

#include <game/mapitems.h>

#include <csignal>

#include "terminalui.h"

#if defined(CONF_CURSES_CLIENT)

void CTerminalUI::RenderGame()
{
	int mx = getmaxx(g_pLogWindow);
	int my = getmaxy(g_pLogWindow);
	int offY = 5;
	int offX = 2;
	if(my < 20)
		offY = 2;
	int width = minimum(128, mx - 3);
	int height = minimum(3, my - 2);
	if(height < 2)
		return;
	DrawBorders(g_pLogWindow, offX, offY - 1, width, height + 2);

	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf), "%*s", width / 2, " this is a preview  ");
	mvwprintw(g_pLogWindow, offY++, offX, "|%-*s|", width - 2, aBuf);
	mvwprintw(g_pLogWindow, offY++, offX, "|%-*s|", width - 2, aBuf);
	mvwprintw(g_pLogWindow, offY++, offX, "|%-*s|", width - 2, aBuf);

	CMapItemLayer *pLayer = (CMapItemLayer *)Layers()->GameLayer();
	if(!pLayer)
		return;

	CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
	CTile *pTiles = (CTile *)Layers()->Map()->GetData(pTMap->m_Data);
	unsigned int Size = Layers()->Map()->GetDataSize(pTMap->m_Data);
	if(Size >= pTMap->m_Width * pTMap->m_Height * sizeof(CTile))
	{
		vec4 Color = vec4(pTMap->m_Color.r / 255.0f, pTMap->m_Color.g / 255.0f, pTMap->m_Color.b / 255.0f, pTMap->m_Color.a / 255.0f);
		RenderTilemap(pTiles, width, height, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, 0,
			this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
	}
}

void CTerminalUI::RenderTilemap(CTile *pTiles, int WinWidth, int WinHeight, int w, int h, float Scale, vec4 Color, int RenderFlags, void *pUser, int ColorEnv, int ColorEnvOffset)
{
	if(!m_pClient->m_Snap.m_pLocalCharacter) // TODO: also render map if tee is dead and also add possibility to spectate
		return;

	char aFrame[32][64]; // tee aka center is at 8/16   y/x
	for(int i = 0; i < 32; i++)
	{
		str_copy(aFrame[i], "                               ", sizeof(aFrame[i]));
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

	// calculate the final pixelsize for the tiles
	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / (ScreenX1 - ScreenX0) * 16; // 16 = chillerbot screen width
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	// adjust the texture shift according to mipmap level
	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

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
			if(Index)
			{
				unsigned char Flags = pTiles[c].m_Flags;
				bool Render = true;

				if(Render)
				{
					int tx = Index % 16;
					int ty = Index / 16;
					int Px0 = tx * (1024 / 16);
					int Py0 = ty * (1024 / 16);
					int Px1 = Px0 + (1024 / 16) - 1;
					int Py1 = Py0 + (1024 / 16) - 1;

					float x0 = Nudge + Px0 / TexSize + Frac;
					float y0 = Nudge + Py0 / TexSize + Frac;
					float x1 = Nudge + Px1 / TexSize - Frac;
					float y1 = Nudge + Py0 / TexSize + Frac;
					float x2 = Nudge + Px1 / TexSize - Frac;
					float y2 = Nudge + Py1 / TexSize - Frac;
					float x3 = Nudge + Px0 / TexSize + Frac;
					float y3 = Nudge + Py1 / TexSize - Frac;

					if(Flags & TILEFLAG_VFLIP)
					{
						x0 = x2;
						x1 = x3;
						x2 = x3;
						x3 = x0;
					}

					if(Flags & TILEFLAG_HFLIP)
					{
						y0 = y3;
						y2 = y1;
						y3 = y1;
						y1 = y0;
					}

					if(Flags & TILEFLAG_ROTATE)
					{
						float Tmp = x0;
						x0 = x3;
						x3 = x2;
						x2 = x1;
						x1 = Tmp;
						Tmp = y0;
						y0 = y3;
						y3 = y2;
						y2 = y1;
						y1 = Tmp;
					}
					// dbg_msg("map", "draw tile=%d at x: %.2f y: %.2f w: %.2f h: %.2f", Index, x*Scale, y*Scale, Scale, Scale);
					int renderX = (x * Scale) - (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X) / 32.0f);
					int renderY = (y * Scale) - (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_Y) / 32.0f);
					// dbg_msg("map", "absolut tile x: %d y: %d       tee x: %.2f y: %.2f", renderX, renderY, static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X)/32.0f, static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_Y)/32.0f);
					renderX += 32;
					renderY += 16;
					// dbg_msg("map", "array pos  tile x: %d y: %d\n", renderX, renderY);
					if(renderX > 0 && renderX < 64 && renderY > 0 && renderY < 32)
					{
						aFrame[renderY][renderX] = '#';
						rendered_tiles++;
					}
					aFrame[16][32] = 'o'; // tee in center
					aFrame[15][32] = 'o'; // tee in center
					aFrame[17][32] = 'o'; // tee in center
				}
			}
			else
			{
				// dbg_msg("map", "skip at x: %.2f y: %.2f w: %.2f h: %.2f", x*Scale, y*Scale, Scale, Scale);
			}
			x += pTiles[c].m_Skip;
		}
	}
	if(rendered_tiles == 0)
		return;

	m_NextRender--;
	if(m_NextRender == 0)
		return;
	m_NextRender = 60; // render every 60 what every this isy
	// system("clear");
	// render frame
	int offY = 2; // TODO: pass this as arg to this func
	int offX = 2; // TODO: pass this as arg to this func
	for(int y = 0; y < 32; y++)
	{
		// printf("%s\n", aFrame[y]);
		aFrame[y][WinWidth - 2] = '\0';
		mvwprintw(g_pLogWindow, offY + y, offX, "|%-*s|", WinWidth - 2, aFrame[y]);
	}
	// printf("------------- tiles: %d \n\n", rendered_tiles);
	// printf("%s\n-------------\n", aFrame[15]);
}

#endif
