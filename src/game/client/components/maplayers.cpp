/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/demo.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/layers.h>
#include <game/client/gameclient.h>
#include <game/client/component.h>
#include <game/client/render.h>

#include <game/client/components/camera.h>
#include <game/client/components/mapimages.h>

#include <game/generated/client_data.h>

#include "maplayers.h"

CMapLayers::CMapLayers(int t)
{
	m_Type = t;
	m_pLayers = 0;
	m_CurrentLocalTick = 0;
	m_LastLocalTick = 0;
	m_EnvelopeUpdate = false;
}

void CMapLayers::OnInit()
{
	m_pLayers = Layers();
	m_pImages = m_pClient->m_pMapimages;
}

void CMapLayers::EnvelopeUpdate()
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
		m_CurrentLocalTick = pInfo->m_CurrentTick;
		m_LastLocalTick = pInfo->m_CurrentTick;
		m_EnvelopeUpdate = true;
	}
}


void CMapLayers::MapScreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup, float Zoom)
{
	float Points[4];
	RenderTools()->MapscreenToWorld(CenterX, CenterY, pGroup->m_ParallaxX/100.0f, pGroup->m_ParallaxY/100.0f,
		pGroup->m_OffsetX, pGroup->m_OffsetY, Graphics()->ScreenAspect(), Zoom, Points);
	Graphics()->MapScreen(Points[0], Points[1], Points[2], Points[3]);
}

void CMapLayers::EnvelopeEval(float TimeOffset, int Env, float *pChannels, void *pUser)
{
	CMapLayers *pThis = (CMapLayers *)pUser;
	pChannels[0] = 0;
	pChannels[1] = 0;
	pChannels[2] = 0;
	pChannels[3] = 0;

	CEnvPoint *pPoints = 0;

	{
		int Start, Num;
		pThis->m_pLayers->Map()->GetType(MAPITEMTYPE_ENVPOINTS, &Start, &Num);
		if(Num)
			pPoints = (CEnvPoint *)pThis->m_pLayers->Map()->GetItem(Start, 0, 0);
	}

	int Start, Num;
	pThis->m_pLayers->Map()->GetType(MAPITEMTYPE_ENVELOPE, &Start, &Num);

	if(Env >= Num)
		return;

	CMapItemEnvelope *pItem = (CMapItemEnvelope *)pThis->m_pLayers->Map()->GetItem(Start+Env, 0, 0);

	static float s_Time = 0.0f;
	static float s_LastLocalTime = pThis->Client()->LocalTime();
	if(pThis->Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const IDemoPlayer::CInfo *pInfo = pThis->DemoPlayer()->BaseInfo();

		if(!pInfo->m_Paused || pThis->m_EnvelopeUpdate)
		{
			if(pThis->m_CurrentLocalTick != pInfo->m_CurrentTick)
			{
				pThis->m_LastLocalTick = pThis->m_CurrentLocalTick;
				pThis->m_CurrentLocalTick = pInfo->m_CurrentTick;
			}

			s_Time = mix(pThis->m_LastLocalTick / (float)pThis->Client()->GameTickSpeed(),
						pThis->m_CurrentLocalTick / (float)pThis->Client()->GameTickSpeed(),
						pThis->Client()->IntraGameTick());
		}

		pThis->RenderTools()->RenderEvalEnvelope(pPoints+pItem->m_StartPoint, pItem->m_NumPoints, 4, s_Time+TimeOffset, pChannels);
	}
	else
	{
		if(pThis->m_pClient->m_Snap.m_pGameInfoObj) // && !(pThis->m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED))
		{
			if(pItem->m_Version < 2 || pItem->m_Synchronized)
			{
				s_Time = mix((pThis->Client()->PrevGameTick()-pThis->m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick) / (float)pThis->Client()->GameTickSpeed(),
							(pThis->Client()->GameTick()-pThis->m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick) / (float)pThis->Client()->GameTickSpeed(),
							pThis->Client()->IntraGameTick());
			}
			else
				s_Time += pThis->Client()->LocalTime()-s_LastLocalTime;
		}
		pThis->RenderTools()->RenderEvalEnvelope(pPoints+pItem->m_StartPoint, pItem->m_NumPoints, 4, s_Time+TimeOffset, pChannels);
		s_LastLocalTime = pThis->Client()->LocalTime();
	}
}

//fix all alignments in any struct -- e.g. don't align to 8 bytes at 64bit code
#pragma pack(push, 1)
struct STmpTile{
	vec2 m_TopLeft;
	vec2 m_BottomLeft;
	vec2 m_TopRight;
	vec2 m_BottomRight;
};
struct STmpTileTexCoord{
	STmpTileTexCoord() {
		m_TexCoordTopLeftRightOrBottom[0] = m_TexCoordTopLeftRightOrBottom[1] = 0;
		m_TexCoordBottomLeftRightOrBottom[0] = 0; m_TexCoordBottomLeftRightOrBottom[1] = 1;
		m_TexCoordTopRightRightOrBottom[0] = 1; m_TexCoordTopRightRightOrBottom[1] = 0;
		m_TexCoordBottomRightRightOrBottom[0] = m_TexCoordBottomRightRightOrBottom[1] = 1;
	}
	unsigned char m_TexCoordTopLeft[2];
	unsigned char m_TexCoordTopLeftRightOrBottom[2];
	unsigned char m_TexCoordBottomLeft[2];
	unsigned char m_TexCoordBottomLeftRightOrBottom[2];
	unsigned char m_TexCoordTopRight[2];
	unsigned char m_TexCoordTopRightRightOrBottom[2];
	unsigned char m_TexCoordBottomRight[2];
	unsigned char m_TexCoordBottomRightRightOrBottom[2];
};

struct STmpTileIndex{
	unsigned int m_Indices[6];
};
#pragma pack(pop)

void FillTmpTileSpeedup(STmpTile* pTmpTile, STmpTileTexCoord* pTmpTex, unsigned char Flags, unsigned char Index, int x, int y, int Scale, CMapItemGroup* pGroup, short AngleRotate){
	if(pTmpTex){
		unsigned char x0 = 0;
		unsigned char y0 = 0;
		unsigned char x1 = 16;
		unsigned char y1 = 0;
		unsigned char x2 = 16;
		unsigned char y2 = 16;
		unsigned char x3 = 0;
		unsigned char y3 = 16;
		
		unsigned char bx0 = 0;
		unsigned char by0 = 0;
		unsigned char bx1 = 1;
		unsigned char by1 = 0;
		unsigned char bx2 = 1;
		unsigned char by2 = 1;
		unsigned char bx3 = 0;
		unsigned char by3 = 1;
	
		pTmpTex->m_TexCoordTopLeft[0] = x0;
		pTmpTex->m_TexCoordTopLeft[1] = y0;
		pTmpTex->m_TexCoordBottomLeft[0] = x3;
		pTmpTex->m_TexCoordBottomLeft[1] = y3;
		pTmpTex->m_TexCoordTopRight[0] = x1;
		pTmpTex->m_TexCoordTopRight[1] = y1;
		pTmpTex->m_TexCoordBottomRight[0] = x2;
		pTmpTex->m_TexCoordBottomRight[1] = y2;
	
		pTmpTex->m_TexCoordTopLeftRightOrBottom[0] = bx0;
		pTmpTex->m_TexCoordTopLeftRightOrBottom[1] = by0;
		pTmpTex->m_TexCoordBottomLeftRightOrBottom[0] = bx3;
		pTmpTex->m_TexCoordBottomLeftRightOrBottom[1] = by3;
		pTmpTex->m_TexCoordTopRightRightOrBottom[0] = bx1;
		pTmpTex->m_TexCoordTopRightRightOrBottom[1] = by1;
		pTmpTex->m_TexCoordBottomRightRightOrBottom[0] = bx2;
		pTmpTex->m_TexCoordBottomRightRightOrBottom[1] = by2;
	}
	
	//same as in rotate from Graphics()
	float Angle = (float)AngleRotate*(3.14159265f/180.0f);
	float c = cosf(Angle);
	float s = sinf(Angle);
	float xR, yR;
	int i;
	
	int ScaleSmaller = 2;
	pTmpTile->m_TopLeft.x = x*Scale + ScaleSmaller;
	pTmpTile->m_TopLeft.y = y*Scale + ScaleSmaller;
	pTmpTile->m_BottomLeft.x = x*Scale + ScaleSmaller;
	pTmpTile->m_BottomLeft.y = y*Scale + Scale - ScaleSmaller;
	pTmpTile->m_TopRight.x = x*Scale + Scale - ScaleSmaller;
	pTmpTile->m_TopRight.y = y*Scale + ScaleSmaller;
	pTmpTile->m_BottomRight.x = x*Scale + Scale - ScaleSmaller;
	pTmpTile->m_BottomRight.y = y*Scale + Scale - ScaleSmaller;
	
	float* pTmpTileVertices = (float*)pTmpTile;

	vec2 Center;
	Center.x = pTmpTile->m_TopLeft.x + (Scale-ScaleSmaller)/2.f;
	Center.y = pTmpTile->m_TopLeft.y + (Scale-ScaleSmaller)/2.f;
	for(i = 0; i < 4*2; i++)
	{
		xR = pTmpTileVertices[i*2] - Center.x;
		yR = pTmpTileVertices[i*2+1] - Center.y;
		pTmpTileVertices[i*2] = xR * c - yR * s + Center.x;
		pTmpTileVertices[i*2+1] = xR * s + yR * c + Center.y;
	}
}


void FillTmpTile(STmpTile* pTmpTile, STmpTileTexCoord* pTmpTex, unsigned char Flags, unsigned char Index, int x, int y, int Scale, CMapItemGroup* pGroup){
	if(pTmpTex){
		unsigned char tx = Index%16;
		unsigned char ty = Index/16;
		unsigned char x0 = tx;
		unsigned char y0 = ty;
		unsigned char x1 = tx+1;
		unsigned char y1 = ty;
		unsigned char x2 = tx+1;
		unsigned char y2 = ty+1;
		unsigned char x3 = tx;
		unsigned char y3 = ty+1;
		
		unsigned char bx0 = 0;
		unsigned char by0 = 0;
		unsigned char bx1 = 1;
		unsigned char by1 = 0;
		unsigned char bx2 = 1;
		unsigned char by2 = 1;
		unsigned char bx3 = 0;
		unsigned char by3 = 1;
		
		

		if(Flags&TILEFLAG_VFLIP)
		{
			x0 = x2;
			x1 = x3;
			x2 = x3;
			x3 = x0;
			
			bx0 = bx2;
			bx1 = bx3;
			bx2 = bx3;
			bx3 = bx0;
		}

		if(Flags&TILEFLAG_HFLIP)
		{
			y0 = y3;
			y2 = y1;
			y3 = y1;
			y1 = y0;
			
			by0 = by3;
			by2 = by1;
			by3 = by1;
			by1 = by0;
		}

		if(Flags&TILEFLAG_ROTATE)
		{
			unsigned char Tmp = x0;
			x0 = x3;
			x3 = x2;
			x2 = x1;
			x1 = Tmp;
			Tmp = y0;
			y0 = y3;
			y3 = y2;
			y2 = y1;
			y1 = Tmp;
			
			Tmp = bx0;
			bx0 = bx3;
			bx3 = bx2;
			bx2 = bx1;
			bx1 = Tmp;
			Tmp = by0;
			by0 = by3;
			by3 = by2;
			by2 = by1;
			by1 = Tmp;
		}
	
		pTmpTex->m_TexCoordTopLeft[0] = x0;
		pTmpTex->m_TexCoordTopLeft[1] = y0;
		pTmpTex->m_TexCoordBottomLeft[0] = x3;
		pTmpTex->m_TexCoordBottomLeft[1] = y3;
		pTmpTex->m_TexCoordTopRight[0] = x1;
		pTmpTex->m_TexCoordTopRight[1] = y1;
		pTmpTex->m_TexCoordBottomRight[0] = x2;
		pTmpTex->m_TexCoordBottomRight[1] = y2;
	
		pTmpTex->m_TexCoordTopLeftRightOrBottom[0] = bx0;
		pTmpTex->m_TexCoordTopLeftRightOrBottom[1] = by0;
		pTmpTex->m_TexCoordBottomLeftRightOrBottom[0] = bx3;
		pTmpTex->m_TexCoordBottomLeftRightOrBottom[1] = by3;
		pTmpTex->m_TexCoordTopRightRightOrBottom[0] = bx1;
		pTmpTex->m_TexCoordTopRightRightOrBottom[1] = by1;
		pTmpTex->m_TexCoordBottomRightRightOrBottom[0] = bx2;
		pTmpTex->m_TexCoordBottomRightRightOrBottom[1] = by2;
	}
	
	pTmpTile->m_TopLeft.x = x*Scale;
	pTmpTile->m_TopLeft.y = y*Scale;
	pTmpTile->m_BottomLeft.x = x*Scale;
	pTmpTile->m_BottomLeft.y = y*Scale + Scale;
	pTmpTile->m_TopRight.x = x*Scale + Scale;
	pTmpTile->m_TopRight.y = y*Scale;
	pTmpTile->m_BottomRight.x = x*Scale + Scale;
	pTmpTile->m_BottomRight.y = y*Scale + Scale;
}

void CMapLayers::OnMapLoad()
{
	if(!Graphics()->IsBufferingEnabled()) return;
	//clear everything and destroy all buffers
	if(m_TileLayerVisuals.size() != 0){
		int s = m_TileLayerVisuals.size();
		for(int i = 0; i < s; ++i){
			Graphics()->DestroyVisual(m_TileLayerVisuals[i].m_VisualObjectIndex);			
		}
		m_TileLayerVisuals.clear();
	}
	
	bool PassedGameLayer = false;
	//prepare all visuals for all tile layers	
	std::vector<STmpTile> tmpTiles;
	std::vector<STmpTileTexCoord> tmpTileTexCoords;
	std::vector<STmpTileIndex> tmpTileIndices;
	std::vector<STmpTileIndex> tmpBorderTopTileIndices;
	std::vector<STmpTileIndex> tmpBorderLeftTileIndices;
	std::vector<STmpTileIndex> tmpBorderRightTileIndices;
	std::vector<STmpTileIndex> tmpBorderBottomTileIndices;
	
	for(int g = 0; g < m_pLayers->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = m_pLayers->GetGroup(g);
		if(!pGroup)
		{
			dbg_msg("maplayers", "error group was null, group number = %d, total groups = %d", g, m_pLayers->NumGroups());
			dbg_msg("maplayers", "this is here to prevent a crash but the source of this is unknown, please report this for it to get fixed");
			dbg_msg("maplayers", "we need mapname and crc and the map that caused this if possible, and anymore info you think is relevant");
			continue;
		}
		
		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = m_pLayers->GetLayer(pGroup->m_StartLayer+l);
			bool IsFrontLayer = false;
			bool IsSwitchLayer = false;
			bool IsTeleLayer = false;
			bool IsSpeedupLayer = false;
			bool IsTuneLayer = false;
			bool IsGameLayer = false;
			bool IsEntityLayer = false;
			
			if(pLayer == (CMapItemLayer*)m_pLayers->GameLayer())
			{
				IsGameLayer = true;
				IsEntityLayer = true;
				PassedGameLayer = true;
			}
			
			if (pLayer == (CMapItemLayer*)m_pLayers->FrontLayer())
				IsEntityLayer = IsFrontLayer = true;

			if (pLayer == (CMapItemLayer*)m_pLayers->SwitchLayer())
				IsEntityLayer = IsSwitchLayer = true;

			if (pLayer == (CMapItemLayer*)m_pLayers->TeleLayer())
				IsEntityLayer = IsTeleLayer = true;

			if (pLayer == (CMapItemLayer*)m_pLayers->SpeedupLayer())
				IsEntityLayer = IsSpeedupLayer = true;

			if (pLayer == (CMapItemLayer*)m_pLayers->TuneLayer())
				IsEntityLayer = IsTuneLayer = true;
			
			if(m_Type <= TYPE_BACKGROUND_FORCE)
			{
				if(PassedGameLayer)
					return;
			}
			
			if(pLayer->m_Type == LAYERTYPE_TILES)
			{				
				bool DoTextureCoords = false;
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				if(pTMap->m_Image == -1)
				{
					if(IsEntityLayer)
						DoTextureCoords = true;
				}
				else
					DoTextureCoords = true;
				

				int DataIndex = 0;
				int TileSize = 0;
				int OverlayCount = 0;
				if (IsFrontLayer) {
					DataIndex = pTMap->m_Front;
					TileSize = sizeof(CTile);
				}
				else if (IsSwitchLayer) {
					DataIndex = pTMap->m_Switch;
					TileSize = sizeof(CSwitchTile);
					OverlayCount = 2;
				}
				else if (IsTeleLayer) {
					DataIndex = pTMap->m_Tele;
					TileSize = sizeof(CTeleTile);
					OverlayCount = 1;
				}
				else if (IsSpeedupLayer) {
					DataIndex = pTMap->m_Speedup;
					TileSize = sizeof(CSpeedupTile);
					OverlayCount = 2;
				}
				else if (IsTuneLayer) {
					DataIndex = pTMap->m_Tune;
					TileSize = sizeof(CTuneTile);
				}
				else {
					DataIndex = pTMap->m_Data;
					TileSize = sizeof(CTile);
				}
				unsigned int Size = m_pLayers->Map()->GetDataSize(DataIndex);
				void* pTiles = (void*)m_pLayers->Map()->GetData(DataIndex);

				if (Size >= pTMap->m_Width*pTMap->m_Height*TileSize)
				{
					int CurOverlay = 0;
					while(CurOverlay < OverlayCount + 1){
						//we can later just count the tile layers to get the idx in the vector
						m_TileLayerVisuals.push_back(STileLayerVisuals());
						STileLayerVisuals& Visuals = m_TileLayerVisuals.back();
						Visuals.m_IsTextured = DoTextureCoords;
						
						tmpTiles.clear();
						tmpTileIndices.clear();
						tmpTileTexCoords.clear();
						
						tmpBorderTopTileIndices.clear();
						tmpBorderLeftTileIndices.clear();
						tmpBorderRightTileIndices.clear();
						tmpBorderBottomTileIndices.clear();	
						
						//we go in steps of the index buffer groups width and heights
						int GroupX = 0;
						int GroupY = 0;
						int x = 0;
						int y = 0;
						while(GroupY * INDEX_BUFFER_GROUP_HEIGHT < pTMap->m_Height){
							for(y = GroupY * INDEX_BUFFER_GROUP_HEIGHT; y < pTMap->m_Height && y < (GroupY + 1) * INDEX_BUFFER_GROUP_HEIGHT; ++y)
							{
								for(x = GroupX * INDEX_BUFFER_GROUP_WIDTH; x < pTMap->m_Width && x < (GroupX + 1) * INDEX_BUFFER_GROUP_WIDTH; ++x)
								{
									unsigned char Index = 0;
									unsigned char Flags = 0;
									int AngleRotate = -1;
									if(IsEntityLayer){								
										if(IsGameLayer) { 
											Index = ((CTile*)pTiles)[y*pTMap->m_Width+x].m_Index;
											Flags = ((CTile*)pTiles)[y*pTMap->m_Width+x].m_Flags;
											if(!IsValidGameTile(Index)) continue;
										}
										if(IsFrontLayer) { 
											Index = ((CTile*)pTiles)[y*pTMap->m_Width+x].m_Index;
											Flags = ((CTile*)pTiles)[y*pTMap->m_Width+x].m_Flags;
											if(!IsValidFrontTile(Index)) continue;
										}
										if(IsSwitchLayer) {
											Flags = 0;
											Index = ((CSwitchTile*)pTiles)[y*pTMap->m_Width+x].m_Type;
											if(!IsValidSwitchTile(Index)) continue;
											if(CurOverlay == 0) {
												Flags = ((CSwitchTile*)pTiles)[y*pTMap->m_Width+x].m_Flags;
												if(Index == TILE_SWITCHTIMEDOPEN) Index = 8;
											}
											else if(CurOverlay == 1) Index = ((CSwitchTile*)pTiles)[y*pTMap->m_Width+x].m_Number;
											else if(CurOverlay == 2) Index = ((CSwitchTile*)pTiles)[y*pTMap->m_Width+x].m_Delay;
										}
										if(IsTeleLayer) {
											Index = ((CTeleTile*)pTiles)[y*pTMap->m_Width+x].m_Type;
											if(!IsValidTeleTile(Index)) continue;
											Flags = 0;
											if(CurOverlay == 1) {
												if(Index != TILE_TELECHECKIN && Index != TILE_TELECHECKINEVIL)
													Index = ((CTeleTile*)pTiles)[y*pTMap->m_Width+x].m_Number;
												else continue;
											}
										}
										if(IsSpeedupLayer) {
											Index = ((CSpeedupTile*)pTiles)[y*pTMap->m_Width+x].m_Type;
											if(!IsValidSpeedupTile(Index) || ((CSpeedupTile*)pTiles)[y*pTMap->m_Width+x].m_Force == 0) continue;
											Flags = 0;
											AngleRotate = ((CSpeedupTile*)pTiles)[y*pTMap->m_Width+x].m_Angle;
											if(CurOverlay == 1) Index = ((CSpeedupTile*)pTiles)[y*pTMap->m_Width+x].m_Force;
											else if(CurOverlay == 2) Index = ((CSpeedupTile*)pTiles)[y*pTMap->m_Width+x].m_MaxSpeed;
										}
										if(IsTuneLayer){							
											Index = ((CTuneTile*)pTiles)[y*pTMap->m_Width+x].m_Type;
											if(Index != TILE_TUNE1) continue;		
											Flags = 0;
										}
									} else {
										Index = ((CTile*)pTiles)[y*pTMap->m_Width+x].m_Index;
										Flags = ((CTile*)pTiles)[y*pTMap->m_Width+x].m_Flags;
									}
									if(Index)
									{
										int CurGroupX = x / INDEX_BUFFER_GROUP_WIDTH;
										int CurGroupY = y / INDEX_BUFFER_GROUP_HEIGHT;
										int GroupIDX = CurGroupX + CurGroupY * (((pTMap->m_Width + INDEX_BUFFER_GROUP_WIDTH - 1) / INDEX_BUFFER_GROUP_WIDTH));
										
										//the amount of tiles times 4 (4 vertices)
										int VertHandledCount = (tmpTiles.size()) * 4;
										int TilesHandledCount = tmpTiles.size();
										if(GroupIDX + 1 > Visuals.m_IndexBufferGroups.size()){
											size_t vSize = Visuals.m_IndexBufferGroups.size();
											for(int v = 0; v < ((GroupIDX + 1) - vSize); ++v){
												Visuals.m_IndexBufferGroups.push_back(STileLayerVisuals::SIndexBufferGroup((char*)(TilesHandledCount*6*sizeof(unsigned int))));
											}
										}
										//the last that was created is our buffergroup
										STileLayerVisuals::SIndexBufferGroup& BufferGroup = Visuals.m_IndexBufferGroups[GroupIDX];
										++BufferGroup.m_NumTilesToRender;
										
										tmpTiles.push_back(STmpTile());
										STmpTile& Tile = tmpTiles.back();
										STmpTileTexCoord* pTileTex = NULL;
										if(DoTextureCoords) {
											tmpTileTexCoords.push_back(STmpTileTexCoord());
											STmpTileTexCoord& TileTex = tmpTileTexCoords.back();
											pTileTex = &TileTex;
										}
										if(IsSpeedupLayer && CurOverlay == 0) FillTmpTileSpeedup(&Tile, pTileTex, Flags, 0, x, y, 32.f, pGroup, AngleRotate);
										else FillTmpTile(&Tile, pTileTex, Flags, Index, x, y, 32.f, pGroup);
										
										tmpTileIndices.push_back(STmpTileIndex());
										STmpTileIndex& Indices = tmpTileIndices.back();
										Indices.m_Indices[0] = VertHandledCount;
										Indices.m_Indices[1] = VertHandledCount + 1;
										Indices.m_Indices[2] = VertHandledCount + 2;
										Indices.m_Indices[3] = VertHandledCount + 1;
										Indices.m_Indices[4] = VertHandledCount + 2;
										Indices.m_Indices[5] = VertHandledCount + 3;
										
										if(x == 0){
											if(y == 0){
												Visuals.m_BorderTopLeft.m_ByteOffset = (char*)(TilesHandledCount*6*sizeof(unsigned int));
												Visuals.m_BorderTopLeft.m_NumTilesToRender = 1;
											} else if(y == pTMap->m_Height - 1){
												Visuals.m_BorderBottomLeft.m_ByteOffset = (char*)(TilesHandledCount*6*sizeof(unsigned int));
												Visuals.m_BorderBottomLeft.m_NumTilesToRender = 1;
											} else {
												int BorderGroupIDX = (y-1) / INDEX_BORDER_BUFFER_GROUP_SIZE;
												if(BorderGroupIDX >= Visuals.m_LeftBorderIndexBufferGroups.size()) {
													int vAdd = (BorderGroupIDX + 1) - Visuals.m_LeftBorderIndexBufferGroups.size();
													for(int w = 0; w < vAdd; ++w) Visuals.m_LeftBorderIndexBufferGroups.push_back(STileLayerVisuals::SIndexBufferGroup((char*)(tmpBorderLeftTileIndices.size()*sizeof(unsigned int) * 6)));
												}
												++Visuals.m_LeftBorderIndexBufferGroups[BorderGroupIDX].m_NumTilesToRender;
												tmpBorderLeftTileIndices.push_back(STmpTileIndex());
												STmpTileIndex& Indices = tmpBorderLeftTileIndices.back();
												Indices.m_Indices[0] = VertHandledCount;
												Indices.m_Indices[1] = VertHandledCount + 1;
												Indices.m_Indices[2] = VertHandledCount + 2;
												Indices.m_Indices[3] = VertHandledCount + 1;
												Indices.m_Indices[4] = VertHandledCount + 2;
												Indices.m_Indices[5] = VertHandledCount + 3;
											}
										} else if(x == pTMap->m_Width - 1){
											if(y == 0){
												Visuals.m_BorderTopRight.m_ByteOffset = (char*)(TilesHandledCount*6*sizeof(unsigned int));
												Visuals.m_BorderTopRight.m_NumTilesToRender = 1;
											} else if(y == pTMap->m_Height - 1){
												Visuals.m_BorderBottomRight.m_ByteOffset = (char*)(TilesHandledCount*6*sizeof(unsigned int));
												Visuals.m_BorderBottomRight.m_NumTilesToRender = 1;
											} else {
												int BorderGroupIDX = (y-1) / INDEX_BORDER_BUFFER_GROUP_SIZE;
												if(BorderGroupIDX >= Visuals.m_RightBorderIndexBufferGroups.size()) {
													int vAdd = (BorderGroupIDX + 1) - Visuals.m_RightBorderIndexBufferGroups.size();
													for(int w = 0; w < vAdd; ++w) Visuals.m_RightBorderIndexBufferGroups.push_back(STileLayerVisuals::SIndexBufferGroup((char*)(tmpBorderRightTileIndices.size()*sizeof(unsigned int) * 6)));
												}
												++Visuals.m_RightBorderIndexBufferGroups[BorderGroupIDX].m_NumTilesToRender;
												tmpBorderRightTileIndices.push_back(STmpTileIndex());
												STmpTileIndex& Indices = tmpBorderRightTileIndices.back();
												Indices.m_Indices[0] = VertHandledCount;
												Indices.m_Indices[1] = VertHandledCount + 1;
												Indices.m_Indices[2] = VertHandledCount + 2;
												Indices.m_Indices[3] = VertHandledCount + 1;
												Indices.m_Indices[4] = VertHandledCount + 2;
												Indices.m_Indices[5] = VertHandledCount + 3;
											}
										} else if(y == 0) {
											if(x > 0 && x < pTMap->m_Width - 1){
												int BorderGroupIDX = (x-1) / INDEX_BORDER_BUFFER_GROUP_SIZE;
												if(BorderGroupIDX >= Visuals.m_TopBorderIndexBufferGroups.size()) {
													int vAdd = (BorderGroupIDX + 1) - Visuals.m_TopBorderIndexBufferGroups.size();
													for(int w = 0; w < vAdd; ++w) Visuals.m_TopBorderIndexBufferGroups.push_back(STileLayerVisuals::SIndexBufferGroup((char*)(tmpBorderTopTileIndices.size()*sizeof(unsigned int) * 6)));
												}
												++Visuals.m_TopBorderIndexBufferGroups[BorderGroupIDX].m_NumTilesToRender;
												tmpBorderTopTileIndices.push_back(STmpTileIndex());
												STmpTileIndex& Indices = tmpBorderTopTileIndices.back();
												Indices.m_Indices[0] = VertHandledCount;
												Indices.m_Indices[1] = VertHandledCount + 1;
												Indices.m_Indices[2] = VertHandledCount + 2;
												Indices.m_Indices[3] = VertHandledCount + 1;
												Indices.m_Indices[4] = VertHandledCount + 2;
												Indices.m_Indices[5] = VertHandledCount + 3;
											}
										} else if(y == pTMap->m_Height - 1) {
											if(x > 0 && x < pTMap->m_Width - 1){
												int BorderGroupIDX = (x-1) / INDEX_BORDER_BUFFER_GROUP_SIZE;
												if(BorderGroupIDX >= Visuals.m_BottomBorderIndexBufferGroups.size()) {
													int vAdd = (BorderGroupIDX + 1) - Visuals.m_BottomBorderIndexBufferGroups.size();
													for(int w = 0; w < vAdd; ++w) Visuals.m_BottomBorderIndexBufferGroups.push_back(STileLayerVisuals::SIndexBufferGroup((char*)(tmpBorderBottomTileIndices.size()*sizeof(unsigned int) * 6)));
												}
												++Visuals.m_BottomBorderIndexBufferGroups[BorderGroupIDX].m_NumTilesToRender;
												tmpBorderBottomTileIndices.push_back(STmpTileIndex());
												STmpTileIndex& Indices = tmpBorderBottomTileIndices.back();
												Indices.m_Indices[0] = VertHandledCount;
												Indices.m_Indices[1] = VertHandledCount + 1;
												Indices.m_Indices[2] = VertHandledCount + 2;
												Indices.m_Indices[3] = VertHandledCount + 1;
												Indices.m_Indices[4] = VertHandledCount + 2;
												Indices.m_Indices[5] = VertHandledCount + 3;
											}
										}
										
									}
									//should we even create the skip anymore
									//x += pTiles[y*pTMap->m_Width+x].m_Skip;
								}
							}
							++GroupX;
							if(x >= pTMap->m_Width) {
								++GroupY;
								GroupX = 0;
							}
						}
												
						//append one kill tile to the gamelayer
						if(IsGameLayer){
							int VertHandledCount = (tmpTiles.size()) * 4;
							int TilesHandledCount = tmpTiles.size();
							tmpTiles.push_back(STmpTile());
							STmpTile& Tile = tmpTiles.back();
							STmpTileTexCoord* pTileTex = NULL;
							if(DoTextureCoords) {
								tmpTileTexCoords.push_back(STmpTileTexCoord());
								STmpTileTexCoord& TileTex = tmpTileTexCoords.back();
								pTileTex = &TileTex;
							} 
							FillTmpTile(&Tile, pTileTex, 0, TILE_DEATH, 0, 0, 32.f, pGroup);
							
							tmpTileIndices.push_back(STmpTileIndex());
							STmpTileIndex& Indices = tmpTileIndices.back();
							Indices.m_Indices[0] = VertHandledCount;
							Indices.m_Indices[1] = VertHandledCount + 1;
							Indices.m_Indices[2] = VertHandledCount + 2;
							Indices.m_Indices[3] = VertHandledCount + 1;
							Indices.m_Indices[4] = VertHandledCount + 2;
							Indices.m_Indices[5] = VertHandledCount + 3;
							
							Visuals.m_BorderKillTile.m_NumTilesToRender = 1;
							Visuals.m_BorderKillTile.m_ByteOffset = (char*)(TilesHandledCount * 6 * sizeof(unsigned int));
						}
						
						//check if we missed a buffergroup -- add it
						int GroupIDX = (pTMap->m_Width / INDEX_BUFFER_GROUP_WIDTH) + (pTMap->m_Height / INDEX_BUFFER_GROUP_HEIGHT) * (((pTMap->m_Width + INDEX_BUFFER_GROUP_WIDTH - 1) / INDEX_BUFFER_GROUP_WIDTH));
						if(GroupIDX + 1 > Visuals.m_IndexBufferGroups.size()){
							size_t vSize = Visuals.m_IndexBufferGroups.size();
							for(int v = 0; v < ((GroupIDX + 1) - vSize); ++v){
								Visuals.m_IndexBufferGroups.push_back(STileLayerVisuals::SIndexBufferGroup((char*)(tmpTiles.size()*6*sizeof(unsigned int))));
							}
						}
						
						//fix missing border groups and their offsets, after we know the tile num now
						int BorderGroupY = (pTMap->m_Height - 1) / INDEX_BORDER_BUFFER_GROUP_SIZE;
						if(BorderGroupY >= Visuals.m_RightBorderIndexBufferGroups.size()) {
							int vAdd = (BorderGroupY + 1) - Visuals.m_RightBorderIndexBufferGroups.size();
							for(int w = 0; w < vAdd; ++w) Visuals.m_RightBorderIndexBufferGroups.push_back(STileLayerVisuals::SIndexBufferGroup((char*)(tmpBorderRightTileIndices.size()*sizeof(unsigned int) * 6)));
						}
						if(BorderGroupY >= Visuals.m_LeftBorderIndexBufferGroups.size()) {
							int vAdd = (BorderGroupY + 1) - Visuals.m_LeftBorderIndexBufferGroups.size();
							for(int w = 0; w < vAdd; ++w) Visuals.m_LeftBorderIndexBufferGroups.push_back(STileLayerVisuals::SIndexBufferGroup((char*)(tmpBorderLeftTileIndices.size()*sizeof(unsigned int) * 6)));
						}
						int BorderGroupX = (pTMap->m_Width - 1) / INDEX_BORDER_BUFFER_GROUP_SIZE;
						if(BorderGroupX >= Visuals.m_BottomBorderIndexBufferGroups.size()) {
							int vAdd = (BorderGroupX + 1) - Visuals.m_BottomBorderIndexBufferGroups.size();
							for(int w = 0; w < vAdd; ++w) Visuals.m_BottomBorderIndexBufferGroups.push_back(STileLayerVisuals::SIndexBufferGroup((char*)(tmpBorderBottomTileIndices.size()*sizeof(unsigned int) * 6)));
						}
						if(BorderGroupX >= Visuals.m_TopBorderIndexBufferGroups.size()) {
							int vAdd = (BorderGroupX + 1) - Visuals.m_TopBorderIndexBufferGroups.size();
							for(int w = 0; w < vAdd; ++w) Visuals.m_TopBorderIndexBufferGroups.push_back(STileLayerVisuals::SIndexBufferGroup((char*)(tmpBorderTopTileIndices.size()*sizeof(unsigned int) * 6)));
						}
						
						size_t AdditionalOffset = 0;
						for (int w = 0; w < Visuals.m_TopBorderIndexBufferGroups.size(); ++w) {
							Visuals.m_TopBorderIndexBufferGroups[w].m_ByteOffset += (tmpTiles.size() * 6 * sizeof(unsigned int));
						}
						AdditionalOffset += (tmpBorderTopTileIndices.size() * sizeof(unsigned int) * 6);
						for (int w = 0; w < Visuals.m_LeftBorderIndexBufferGroups.size(); ++w) {
							Visuals.m_LeftBorderIndexBufferGroups[w].m_ByteOffset += (tmpTiles.size() * 6 * sizeof(unsigned int) + AdditionalOffset);
						}
						AdditionalOffset += (tmpBorderLeftTileIndices.size() * sizeof(unsigned int) * 6);
						for(int w = 0; w < Visuals.m_RightBorderIndexBufferGroups.size(); ++w){
							Visuals.m_RightBorderIndexBufferGroups[w].m_ByteOffset += (tmpTiles.size()*6*sizeof(unsigned int) + AdditionalOffset);
						}
						AdditionalOffset += (tmpBorderRightTileIndices.size()*sizeof(unsigned int) * 6);
						for(int w = 0; w < Visuals.m_BottomBorderIndexBufferGroups.size(); ++w){
							Visuals.m_BottomBorderIndexBufferGroups[w].m_ByteOffset += (tmpTiles.size()*6*sizeof(unsigned int) + AdditionalOffset);
						}
						
						//setup params
						float* pTmpTiles = (tmpTiles.size() == 0) ? NULL : (float*)&tmpTiles[0];
						unsigned char* pTmpTileTexCoords = (tmpTileTexCoords.size() == 0) ? NULL : (unsigned char*)&tmpTileTexCoords[0];
						unsigned int* pTmpTileIndices = (tmpTileIndices.size() == 0) ? NULL : (unsigned int*)&tmpTileIndices[0];
						
						Visuals.m_VisualObjectIndex = Graphics()->CreateVisualObjects(pTmpTiles, pTmpTileTexCoords, tmpTiles.size(), pTmpTileIndices, tmpTileIndices.size() * 6);
						
						if(Visuals.m_VisualObjectIndex != -1){
							//upload boarder stuff
							unsigned int* pTmpBorderTop = (tmpBorderTopTileIndices.size() == 0 ? NULL : (unsigned int*)&tmpBorderTopTileIndices[0]);
							unsigned int* pTmpBorderLeft = (tmpBorderLeftTileIndices.size() == 0 ? NULL : (unsigned int*)&tmpBorderLeftTileIndices[0]);
							unsigned int* pTmpBorderRight = (tmpBorderRightTileIndices.size() == 0 ? NULL : (unsigned int*)&tmpBorderRightTileIndices[0]);
							unsigned int* pTmpBorderBottom = (tmpBorderBottomTileIndices.size() == 0 ? NULL : (unsigned int*)&tmpBorderBottomTileIndices[0]);
							Graphics()->AppendAllIndices(pTmpBorderTop, tmpBorderTopTileIndices.size() * 6, Visuals.m_VisualObjectIndex);
							Graphics()->AppendAllIndices(pTmpBorderLeft, tmpBorderLeftTileIndices.size() * 6, Visuals.m_VisualObjectIndex);
							Graphics()->AppendAllIndices(pTmpBorderRight, tmpBorderRightTileIndices.size() * 6, Visuals.m_VisualObjectIndex);
							Graphics()->AppendAllIndices(pTmpBorderBottom, tmpBorderBottomTileIndices.size() * 6, Visuals.m_VisualObjectIndex);
						}
						
						++CurOverlay;
					}
				}
			}
		}
	}
}

void CMapLayers::RenderTileLayer(int LayerIndex, vec4* pColor, CMapItemLayerTilemap* pTileLayer, CMapItemGroup* pGroup){
	STileLayerVisuals& Visuals = m_TileLayerVisuals[LayerIndex];
	if (Visuals.m_VisualObjectIndex == -1) return; //no visuals were created
	STileLayerVisuals::SIndexBufferGroup* BufferGroup = &Visuals.m_IndexBufferGroups[0];
	
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	
	float r=1, g=1, b=1, a=1;
	if(pTileLayer->m_ColorEnv >= 0)
	{
		float aChannels[4];
		EnvelopeEval(pTileLayer->m_ColorEnvOffset/1000.0f, pTileLayer->m_ColorEnv, aChannels, this);
		r = aChannels[0];
		g = aChannels[1];
		b = aChannels[2];
		a = aChannels[3];
	}
	
	int BorderX0, BorderY0, BorderX1, BorderY1;
	bool DrawBorder = false;
	
	int Y0 = BorderY0 = (int)(ScreenY0/32)-1;
	int X0 = BorderX0 = (int)(ScreenX0/32)-1;
	int Y1 = BorderY1 = (int)(ScreenY1/32)+1;
	int X1 = BorderX1 = (int)(ScreenX1/32)+1;
	
	if(X0 < 0) {
		X0 = 0;
		DrawBorder = true;
	}
	if(Y0 < 0) {
		Y0 = 0;
		DrawBorder = true;
	}
	if(X1 >= pTileLayer->m_Width) {
		X1 = pTileLayer->m_Width - 1;
		DrawBorder = true;
	}
	if(Y1 >= pTileLayer->m_Height) {
		Y1 = pTileLayer->m_Height - 1;
		DrawBorder = true;
	}
	
	bool DrawLayer = true;
	if(X1 < 0) DrawLayer = false;
	if(Y1 < 0) DrawLayer = false;
	if(X0 >= pTileLayer->m_Width) DrawLayer = false;
	if(Y0 >= pTileLayer->m_Height) DrawLayer = false;
	
	if(DrawLayer){
		//create the indice buffers we want to draw -- reuse them
		static std::vector<char*> IndexOffsets;
		static std::vector<unsigned int> DrawCounts;
		static unsigned long long maxRes = (IndexOffsets.max_size() > DrawCounts.max_size() ? DrawCounts.max_size() : IndexOffsets.max_size());
		
		IndexOffsets.clear();
		DrawCounts.clear();
		/*static char* IndexOffsets[100000];
		static unsigned int DrawCounts[100000];*/

		int GroupXStart = X0 / INDEX_BUFFER_GROUP_WIDTH;
		int GroupYStart = Y0 / INDEX_BUFFER_GROUP_HEIGHT;
		int GroupXEnd = X1 / INDEX_BUFFER_GROUP_WIDTH;
		int GroupYEnd = Y1 / INDEX_BUFFER_GROUP_HEIGHT;
		
		unsigned long long Reserve = absolute(GroupYEnd - GroupYStart) + 1;
		if(Reserve > maxRes) Reserve = maxRes;
		IndexOffsets.reserve(Reserve);
		DrawCounts.reserve(Reserve);
		
		int c = 0;
		
		for(int y = GroupYStart; y <= GroupYEnd; ++y){
			unsigned int NumTiles = 0;

			int x = GroupXStart;
			if (x > GroupXEnd) continue;
			int GroupIDX = x + y * (((pTileLayer->m_Width + INDEX_BUFFER_GROUP_WIDTH - 1) / INDEX_BUFFER_GROUP_WIDTH));
			IndexOffsets.push_back(BufferGroup[GroupIDX].m_ByteOffset);
			DrawCounts.push_back(0);
			//DrawCounts[c] = 0;
			//IndexOffsets[c] = BufferGroup[GroupIDX].m_ByteOffset;
			for(;x <= GroupXEnd; ++x){
				GroupIDX = x + y * (((pTileLayer->m_Width + INDEX_BUFFER_GROUP_WIDTH - 1) / INDEX_BUFFER_GROUP_WIDTH));
				NumTiles += BufferGroup[GroupIDX].m_NumTilesToRender * 6lu;			
			}
			DrawCounts[c++] = NumTiles ;
		}
		
		pColor->x *= r;
		pColor->y *= g;
		pColor->z *= b;
		pColor->w *= a;
		
		int DrawCount = IndexOffsets.size();
		if(DrawCount != 0) Graphics()->DrawVisualObject(Visuals.m_VisualObjectIndex, (float*)pColor, &IndexOffsets[0], &DrawCounts[0], DrawCount);
	}
	if(DrawBorder) DrawTileBorder(LayerIndex, pColor, pTileLayer, pGroup, BorderX0, BorderY0, BorderX1, BorderY1);
}

void CMapLayers::DrawTileBorder(int LayerIndex, vec4* pColor, CMapItemLayerTilemap* pTileLayer, CMapItemGroup* pGroup, int BorderX0, int BorderY0, int BorderX1, int BorderY1){
	STileLayerVisuals& Visuals = m_TileLayerVisuals[LayerIndex];
	
	if(BorderX0 < -300 + pGroup->m_OffsetX/32) BorderX0 = -300 + pGroup->m_OffsetX/32;
	if(BorderY0 < -300 + pGroup->m_OffsetY/32) BorderY0 = -300 + pGroup->m_OffsetY/32;
	if(BorderX1 >= pTileLayer->m_Width + 300 + pGroup->m_OffsetX/32) BorderX1 = pTileLayer->m_Width + 299 + pGroup->m_OffsetX/32;
	if(BorderY1 >= pTileLayer->m_Height + 300 + pGroup->m_OffsetY/32) BorderY1 = pTileLayer->m_Height + 299 + pGroup->m_OffsetY/32;	
	
	int Y0 = BorderY0;
	int X0 = BorderX0;
	int Y1 = BorderY1;
	int X1 = BorderX1;
	
	if(X0 < 1) X0 = 1;
	if(Y0 < 1) Y0 = 1;
	if(X1 >= pTileLayer->m_Width - 1) X1 = pTileLayer->m_Width - 2;
	if(Y1 >= pTileLayer->m_Height - 1) Y1 = pTileLayer->m_Height - 2;
	
	if(BorderX0 < 0){
		//draw corners on left side
		if(BorderY0 < 0){
			if (Visuals.m_BorderTopLeft.m_NumTilesToRender != 0) {
				vec2 Offset;
				Offset.x = BorderX0 * 32.f;
				Offset.y = BorderY0 * 32.f;
				vec2 Dir;
				Dir.x = 32.f;
				Dir.y = 32.f;

				int Count = (absolute(BorderX0)+1) * (absolute(BorderY0)+1) - 1;//dont draw the corner again

				Graphics()->DrawBorderTile(Visuals.m_VisualObjectIndex, (float*)pColor, Visuals.m_BorderTopLeft.m_ByteOffset, (float*)&Offset, (float*)&Dir, absolute(BorderX0)+1, Count);
			}
		}
		if(BorderY1 >= pTileLayer->m_Height){
			if (Visuals.m_BorderBottomLeft.m_NumTilesToRender != 0) {
				vec2 Offset;
				Offset.x = BorderX0 * 32.f;
				Offset.y = (BorderY1-pTileLayer->m_Height) * 32.f;
				vec2 Dir;
				Dir.x = 32.f;
				Dir.y = -32.f;

				int Count = (absolute(BorderX0)+1) * (absolute((BorderY1-pTileLayer->m_Height))+1) - 1;//dont draw the corner again

				Graphics()->DrawBorderTile(Visuals.m_VisualObjectIndex, (float*)pColor, Visuals.m_BorderBottomLeft.m_ByteOffset, (float*)&Offset, (float*)&Dir, absolute(BorderX0)+1, Count);
			}
		}
		//draw left border
		if(Y0 < pTileLayer->m_Height - 1 && Y1 > 0){
			int GroupStartIDX = (Y0-1) / INDEX_BORDER_BUFFER_GROUP_SIZE;
			int GroupEndIDX = (Y1-1) / INDEX_BORDER_BUFFER_GROUP_SIZE + 1;
			unsigned int DrawNum = 0;
			char* pOffset = Visuals.m_LeftBorderIndexBufferGroups[GroupStartIDX].m_ByteOffset;
			for(int i = GroupStartIDX; i < GroupEndIDX; ++i){
				DrawNum += Visuals.m_LeftBorderIndexBufferGroups[i].m_NumTilesToRender * 6;
			}
			vec2 Dir;
			Dir.x = -32.f;
			Dir.y = 0.f;
			Graphics()->DrawBorderTileLine(Visuals.m_VisualObjectIndex, (float*)pColor, pOffset, (float*)&Dir, DrawNum, absolute(BorderX0));
		}
	}
	
	if(BorderX1 >= pTileLayer->m_Width){
		//draw corners on right side
		if(BorderY0 < 0){
			if (Visuals.m_BorderTopRight.m_NumTilesToRender != 0) {
				vec2 Offset;
				Offset.x = (BorderX1-pTileLayer->m_Width) * 32.f;
				Offset.y = BorderY0 * 32.f;
				vec2 Dir;
				Dir.x = -32.f;
				Dir.y = 32.f;

				int Count = (absolute((BorderX1-pTileLayer->m_Width))+1) * (absolute(BorderY0)+1) - 1;//dont draw the corner again

				Graphics()->DrawBorderTile(Visuals.m_VisualObjectIndex, (float*)pColor, Visuals.m_BorderTopRight.m_ByteOffset, (float*)&Offset, (float*)&Dir, absolute((BorderX1-pTileLayer->m_Width))+1, Count);
			}
		}
		if(BorderY1 >= pTileLayer->m_Height){
			if (Visuals.m_BorderBottomRight.m_NumTilesToRender != 0) {
				vec2 Offset;
				Offset.x = (BorderX1-pTileLayer->m_Width) * 32.f;
				Offset.y = (BorderY1-pTileLayer->m_Height) * 32.f;
				vec2 Dir;
				Dir.x = -32.f;
				Dir.y = -32.f;

				int Count = (absolute((BorderX1-pTileLayer->m_Width))+1) * (absolute((BorderY1-pTileLayer->m_Height))+1) - 1;//dont draw the corner again

				Graphics()->DrawBorderTile(Visuals.m_VisualObjectIndex, (float*)pColor, Visuals.m_BorderBottomRight.m_ByteOffset, (float*)&Offset, (float*)&Dir, absolute((BorderX1-pTileLayer->m_Width))+1, Count);
			}
		}		
		//draw right border
		if(Y0 < pTileLayer->m_Height - 1 && Y1 > 0){
			int GroupStartIDX = (Y0-1) / INDEX_BORDER_BUFFER_GROUP_SIZE;
			int GroupEndIDX = (Y1-1) / INDEX_BORDER_BUFFER_GROUP_SIZE + 1;
			unsigned int DrawNum = 0;
			char* pOffset = Visuals.m_RightBorderIndexBufferGroups[GroupStartIDX].m_ByteOffset;
			for(int i = GroupStartIDX; i < GroupEndIDX; ++i){
				DrawNum += Visuals.m_RightBorderIndexBufferGroups[i].m_NumTilesToRender * 6;
			}
			vec2 Dir;
			Dir.x = 32.f;
			Dir.y = 0.f;
			Graphics()->DrawBorderTileLine(Visuals.m_VisualObjectIndex, (float*)pColor, pOffset, (float*)&Dir, DrawNum, absolute(BorderX1) - (pTileLayer->m_Width-1));
		}
	}
	if(BorderY0 < 0){
		//draw top border
		if(X0 < pTileLayer->m_Width - 1 && X1 > 0){
			int GroupStartIDX = (X0-1) / INDEX_BORDER_BUFFER_GROUP_SIZE;
			int GroupEndIDX = (X1-1) / INDEX_BORDER_BUFFER_GROUP_SIZE + 1;
			unsigned int DrawNum = 0;
			char* pOffset = Visuals.m_TopBorderIndexBufferGroups[GroupStartIDX].m_ByteOffset;
			for(int i = GroupStartIDX; i < GroupEndIDX; ++i){
				DrawNum += Visuals.m_TopBorderIndexBufferGroups[i].m_NumTilesToRender * 6;
			}
			vec2 Dir;
			Dir.x = 0.f;
			Dir.y = -32.f;
			Graphics()->DrawBorderTileLine(Visuals.m_VisualObjectIndex, (float*)pColor, pOffset, (float*)&Dir, DrawNum, absolute(BorderY0));
		}
	}
	if(BorderY1 >= pTileLayer->m_Height){
		//draw bottom border
		if(X0 < pTileLayer->m_Width - 1 && X1 > 0){
			int GroupStartIDX = (X0-1) / INDEX_BORDER_BUFFER_GROUP_SIZE;
			int GroupEndIDX = (X1-1) / INDEX_BORDER_BUFFER_GROUP_SIZE + 1;
			unsigned int DrawNum = 0;
			char* pOffset = Visuals.m_BottomBorderIndexBufferGroups[GroupStartIDX].m_ByteOffset;
			for(int i = GroupStartIDX; i < GroupEndIDX; ++i){
				DrawNum += Visuals.m_BottomBorderIndexBufferGroups[i].m_NumTilesToRender * 6;
			}
			vec2 Dir;
			Dir.x = 0.f;
			Dir.y = 32.f;
			Graphics()->DrawBorderTileLine(Visuals.m_VisualObjectIndex, (float*)pColor, pOffset, (float*)&Dir, DrawNum, absolute(BorderY1) - (pTileLayer->m_Height-1));
		}
	}
}


void CMapLayers::DrawKillTileBorder(int LayerIndex, vec4* pColor, CMapItemLayerTilemap* pTileLayer, CMapItemGroup* pGroup){
	STileLayerVisuals& Visuals = m_TileLayerVisuals[LayerIndex];
	if (Visuals.m_VisualObjectIndex == -1) return; //no visuals were created
	
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	
	bool DrawBorder = false;
	
	int BorderY0 = (int)(ScreenY0/32)-1;
	int BorderX0 = (int)(ScreenX0/32)-1;
	int BorderY1 = (int)(ScreenY1/32)+1;
	int BorderX1 = (int)(ScreenX1/32)+1;
	
	if(BorderX0 < -201) DrawBorder = true;
	if(BorderY0 < -201) DrawBorder = true;
	if(BorderX1 >= pTileLayer->m_Width + 201) DrawBorder = true;
	if(BorderY1 >= pTileLayer->m_Height + 201) DrawBorder = true;
	
	if(!DrawBorder) return;
	if (Visuals.m_BorderKillTile.m_NumTilesToRender == 0) return;
	
	if(BorderX0 < -300) BorderX0 = -300;
	if(BorderY0 < -300) BorderY0 = -300;
	if(BorderX1 >= pTileLayer->m_Width + 300) BorderX1 = pTileLayer->m_Width + 300;
	if(BorderY1 >= pTileLayer->m_Height + 300) BorderY1 = pTileLayer->m_Height + 300;	
	
	if(BorderX0 < -201){
		vec2 Offset;
		Offset.x = BorderX0 * 32.f;
		Offset.y = BorderY0 * 32.f;
		vec2 Dir;
		Dir.x = 32.f;
		Dir.y = 32.f;

		int Count = (absolute(BorderX0) - 201) * ((absolute(BorderY1))+(absolute(BorderY0)));

		Graphics()->DrawBorderTile(Visuals.m_VisualObjectIndex, (float*)pColor, Visuals.m_BorderKillTile.m_ByteOffset, (float*)&Offset, (float*)&Dir, (absolute(BorderX0) - 201), Count);
	}
	if(BorderY0 < -201){
		vec2 Offset;
		int OffX0 = (BorderX0 < -201 ? -201 : BorderX0);
		int OffX1 = (BorderX1 >= pTileLayer->m_Width + 201 ? pTileLayer->m_Width + 201 : BorderX1);
		Offset.x = OffX0 * 32.f;
		Offset.y = BorderY0 * 32.f;
		vec2 Dir;
		Dir.x = 32.f;
		Dir.y = 32.f;

		int Count = ((absolute(OffX1)) + (absolute(OffX0))) * (absolute(BorderY0)-201);

		Graphics()->DrawBorderTile(Visuals.m_VisualObjectIndex, (float*)pColor, Visuals.m_BorderKillTile.m_ByteOffset, (float*)&Offset, (float*)&Dir, ((absolute(OffX1)) + (absolute(OffX0))), Count);
	}
	if(BorderX1 >= pTileLayer->m_Width + 201){
		vec2 Offset;
		Offset.x = (pTileLayer->m_Width + 201) * 32.f;
		Offset.y = BorderY0 * 32.f;
		vec2 Dir;
		Dir.x = 32.f;
		Dir.y = 32.f;

		int Count = ((absolute(BorderX1)) - (pTileLayer->m_Width + 201)) * ((absolute(BorderY1))+(absolute(BorderY0)));

		Graphics()->DrawBorderTile(Visuals.m_VisualObjectIndex, (float*)pColor, Visuals.m_BorderKillTile.m_ByteOffset, (float*)&Offset, (float*)&Dir, ((absolute(BorderX1)) - (pTileLayer->m_Width + 201)), Count);
	}
	if(BorderY1 >= pTileLayer->m_Height + 201){
		vec2 Offset;
		int OffX0 = (BorderX0 < -201 ? -201 : BorderX0);
		int OffX1 = (BorderX1 >= pTileLayer->m_Width + 201 ? pTileLayer->m_Width + 201 : BorderX1);
		Offset.x = OffX0 * 32.f;
		Offset.y = (pTileLayer->m_Height + 201) * 32.f;
		vec2 Dir;
		Dir.x = 32.f;
		Dir.y = 32.f;

		int Count = ((absolute(OffX1)) + (absolute(OffX0))) * ((absolute(BorderY1))-(pTileLayer->m_Height + 200));

		Graphics()->DrawBorderTile(Visuals.m_VisualObjectIndex, (float*)pColor, Visuals.m_BorderKillTile.m_ByteOffset, (float*)&Offset, (float*)&Dir, ((absolute(OffX1)) + (absolute(OffX0))), Count);
	}
}

int CMapLayers::TileLayersOfGroup(CMapItemGroup* pGroup){	
	int TileLayerCounter = 0;
	bool PassedGameLayer = false;
	for(int l = 0; l < pGroup->m_NumLayers; l++)
	{
		CMapItemLayer *pLayer = m_pLayers->GetLayer(pGroup->m_StartLayer+l);
		bool Render = false;
		bool IsGameLayer = false;
		bool IsFrontLayer = false;
		bool IsSwitchLayer = false;
		bool IsTeleLayer = false;
		bool IsSpeedupLayer = false;
		bool IsTuneLayer = false;
		bool IsEntityLayer = false;

		if(pLayer == (CMapItemLayer*)m_pLayers->GameLayer())
		{
			IsEntityLayer = IsGameLayer = true;
			PassedGameLayer = true;
		}

		if(pLayer == (CMapItemLayer*)m_pLayers->FrontLayer())
			IsEntityLayer = IsFrontLayer = true;

		if(pLayer == (CMapItemLayer*)m_pLayers->SwitchLayer())
			IsEntityLayer = IsSwitchLayer = true;

		if(pLayer == (CMapItemLayer*)m_pLayers->TeleLayer())
			IsEntityLayer = IsTeleLayer = true;

		if(pLayer == (CMapItemLayer*)m_pLayers->SpeedupLayer())
			IsEntityLayer = IsSpeedupLayer = true;

		if(pLayer == (CMapItemLayer*)m_pLayers->TuneLayer())
			IsEntityLayer = IsTuneLayer = true;
		
		else if(m_Type <= TYPE_BACKGROUND_FORCE)
		{
			if(PassedGameLayer)
				return TileLayerCounter;
		}
		
		if(pLayer->m_Type == LAYERTYPE_TILES)
		{
			CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
			int DataIndex = 0;
			int TileSize = 0;
			int TileLayerAndOverlayCount = 0;
			if (IsFrontLayer) {
				DataIndex = pTMap->m_Front;
				TileSize = sizeof(CTile);
				TileLayerAndOverlayCount = 1;
			}
			else if (IsSwitchLayer) {
				DataIndex = pTMap->m_Switch;
				TileSize = sizeof(CSwitchTile);
				TileLayerAndOverlayCount = 3;
			}
			else if (IsTeleLayer) {
				DataIndex = pTMap->m_Tele;
				TileSize = sizeof(CTeleTile);
				TileLayerAndOverlayCount = 2;
			}
			else if (IsSpeedupLayer) {
				DataIndex = pTMap->m_Speedup;
				TileSize = sizeof(CSpeedupTile);
				TileLayerAndOverlayCount = 3;
			}
			else if (IsTuneLayer) {
				DataIndex = pTMap->m_Tune;
				TileSize = sizeof(CTuneTile);
				TileLayerAndOverlayCount = 1;
			}
			else {
				DataIndex = pTMap->m_Data;
				TileSize = sizeof(CTile);
				TileLayerAndOverlayCount = 1;
			}

			unsigned int Size = m_pLayers->Map()->GetDataSize(DataIndex);
			if (Size >= pTMap->m_Width*pTMap->m_Height*TileSize)
			{
				TileLayerCounter += TileLayerAndOverlayCount;
			}
		}
	}
	return TileLayerCounter;
}

void CMapLayers::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	CUIRect Screen;
	Graphics()->GetScreen(&Screen.x, &Screen.y, &Screen.w, &Screen.h);

	vec2 Center = m_pClient->m_pCamera->m_Center;

	bool PassedGameLayer = false;
	int TileLayerCounter = 0;

	for(int g = 0; g < m_pLayers->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = m_pLayers->GetGroup(g);

		if(!pGroup)
		{
			dbg_msg("maplayers", "error group was null, group number = %d, total groups = %d", g, m_pLayers->NumGroups());
			dbg_msg("maplayers", "this is here to prevent a crash but the source of this is unknown, please report this for it to get fixed");
			dbg_msg("maplayers", "we need mapname and crc and the map that caused this if possible, and anymore info you think is relevant");
			continue;
		}

		if(!g_Config.m_GfxNoclip && pGroup->m_Version >= 2 && pGroup->m_UseClipping)
		{
			// set clipping
			float Points[4];
			MapScreenToGroup(Center.x, Center.y, m_pLayers->GameGroup(), m_pClient->m_pCamera->m_Zoom);
			Graphics()->GetScreen(&Points[0], &Points[1], &Points[2], &Points[3]);
			float x0 = (pGroup->m_ClipX - Points[0]) / (Points[2]-Points[0]);
			float y0 = (pGroup->m_ClipY - Points[1]) / (Points[3]-Points[1]);
			float x1 = ((pGroup->m_ClipX+pGroup->m_ClipW) - Points[0]) / (Points[2]-Points[0]);
			float y1 = ((pGroup->m_ClipY+pGroup->m_ClipH) - Points[1]) / (Points[3]-Points[1]);

			if (x1 < 0.0f || x0 > 1.0f || y1 < 0.0f || y0 > 1.0f) {
				//check tile layer count of this group
				TileLayerCounter += TileLayersOfGroup(pGroup);
				continue;
			}

			Graphics()->ClipEnable((int)(x0*Graphics()->ScreenWidth()), (int)(y0*Graphics()->ScreenHeight()),
				(int)((x1-x0)*Graphics()->ScreenWidth()), (int)((y1-y0)*Graphics()->ScreenHeight()));
		}

		if(!g_Config.m_ClZoomBackgroundLayers && !pGroup->m_ParallaxX && !pGroup->m_ParallaxY) {
			MapScreenToGroup(Center.x, Center.y, pGroup, 1.0);
		} else
			MapScreenToGroup(Center.x, Center.y, pGroup, m_pClient->m_pCamera->m_Zoom);

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = m_pLayers->GetLayer(pGroup->m_StartLayer+l);
			bool Render = false;
			bool IsGameLayer = false;
			bool IsFrontLayer = false;
			bool IsSwitchLayer = false;
			bool IsTeleLayer = false;
			bool IsSpeedupLayer = false;
			bool IsTuneLayer = false;
			bool IsEntityLayer = false;

			if(pLayer == (CMapItemLayer*)m_pLayers->GameLayer())
			{
				IsEntityLayer = IsGameLayer = true;
				PassedGameLayer = true;
			}

			if(pLayer == (CMapItemLayer*)m_pLayers->FrontLayer())
				IsEntityLayer = IsFrontLayer = true;

			if(pLayer == (CMapItemLayer*)m_pLayers->SwitchLayer())
				IsEntityLayer = IsSwitchLayer = true;

			if(pLayer == (CMapItemLayer*)m_pLayers->TeleLayer())
				IsEntityLayer = IsTeleLayer = true;

			if(pLayer == (CMapItemLayer*)m_pLayers->SpeedupLayer())
				IsEntityLayer = IsSpeedupLayer = true;

			if(pLayer == (CMapItemLayer*)m_pLayers->TuneLayer())
				IsEntityLayer = IsTuneLayer = true;
			
			if(m_Type == -1)
				Render = true;
			else if(m_Type <= TYPE_BACKGROUND_FORCE)
			{
				if(PassedGameLayer)
					return;
				Render = true;
				
				if(m_Type == TYPE_BACKGROUND_FORCE){
					if(pLayer->m_Type == LAYERTYPE_TILES && !g_Config.m_ClBackgroundShowTilesLayers)
						continue;
				}
			}
			else // TYPE_FOREGROUND
			{
				if(PassedGameLayer && !IsGameLayer)
					Render = true;
			}

			if(Render && pLayer->m_Type == LAYERTYPE_TILES && Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyIsPressed(KEY_LSHIFT) && Input()->KeyPress(KEY_KP_0))
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				CTile *pTiles = (CTile *)m_pLayers->Map()->GetData(pTMap->m_Data);
				CServerInfo CurrentServerInfo;
				Client()->GetServerInfo(&CurrentServerInfo);
				char aFilename[256];
				str_format(aFilename, sizeof(aFilename), "dumps/tilelayer_dump_%s-%d-%d-%dx%d.txt", CurrentServerInfo.m_aMap, g, l, pTMap->m_Width, pTMap->m_Height);
				IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
				if(File)
				{
					for(int y = 0; y < pTMap->m_Height; y++)
					{
						for(int x = 0; x < pTMap->m_Width; x++)
							io_write(File, &(pTiles[y*pTMap->m_Width + x].m_Index), sizeof(pTiles[y*pTMap->m_Width + x].m_Index));
						io_write_newline(File);
					}
					io_close(File);
				}
			}

			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				int DataIndex = 0;
				int TileSize = 0;
				int TileLayerAndOverlayCount = 0;
				if (IsFrontLayer) {
					DataIndex = pTMap->m_Front;
					TileSize = sizeof(CTile);
					TileLayerAndOverlayCount = 1;
				}
				else if (IsSwitchLayer) {
					DataIndex = pTMap->m_Switch;
					TileSize = sizeof(CSwitchTile);
					TileLayerAndOverlayCount = 3;
				}
				else if (IsTeleLayer) {
					DataIndex = pTMap->m_Tele;
					TileSize = sizeof(CTeleTile);
					TileLayerAndOverlayCount = 2;
				}
				else if (IsSpeedupLayer) {
					DataIndex = pTMap->m_Speedup;
					TileSize = sizeof(CSpeedupTile);
					TileLayerAndOverlayCount = 3;
				}
				else if (IsTuneLayer) {
					DataIndex = pTMap->m_Tune;
					TileSize = sizeof(CTuneTile);
					TileLayerAndOverlayCount = 1;
				}
				else {
					DataIndex = pTMap->m_Data;
					TileSize = sizeof(CTile);
					TileLayerAndOverlayCount = 1;
				}

				unsigned int Size = m_pLayers->Map()->GetDataSize(DataIndex);
				if (Size >= pTMap->m_Width*pTMap->m_Height*TileSize)
				{
					TileLayerCounter += TileLayerAndOverlayCount;
				}
			}

			// skip rendering if detail layers if not wanted, or is entity layer and we are a background map
			if ((pLayer->m_Flags&LAYERFLAG_DETAIL && !g_Config.m_GfxHighDetail && !IsGameLayer) || (m_Type == TYPE_BACKGROUND_FORCE && IsEntityLayer))
				continue;
			
			if((Render && g_Config.m_ClOverlayEntities < 100 && !IsGameLayer && !IsFrontLayer && !IsSwitchLayer && !IsTeleLayer && !IsSpeedupLayer && !IsTuneLayer) || (g_Config.m_ClOverlayEntities && IsGameLayer) || (m_Type == TYPE_BACKGROUND_FORCE))
			{
				if(pLayer->m_Type == LAYERTYPE_TILES)
				{
					CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
					if(pTMap->m_Image == -1)
					{
						if(!IsGameLayer)
							Graphics()->TextureSet(-1);
						else
							Graphics()->TextureSet(m_pImages->GetEntities());
					}
					else
						Graphics()->TextureSet(m_pImages->Get(pTMap->m_Image));

					CTile *pTiles = (CTile *)m_pLayers->Map()->GetData(pTMap->m_Data);
					unsigned int Size = m_pLayers->Map()->GetDataSize(pTMap->m_Data);

					if (Size >= pTMap->m_Width*pTMap->m_Height*sizeof(CTile))
					{
						vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f);
						if(IsGameLayer && g_Config.m_ClOverlayEntities)
							Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*g_Config.m_ClOverlayEntities/100.0f);
						else if(!IsGameLayer && g_Config.m_ClOverlayEntities && !(m_Type == TYPE_BACKGROUND_FORCE))
							Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*(100-g_Config.m_ClOverlayEntities)/100.0f);
						if(!Graphics()->IsBufferingEnabled()) {							
							Graphics()->BlendNone();
							RenderTools()->RenderTilemap(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE,
															EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
							
							Graphics()->BlendNormal();
							
							// draw kill tiles outside the entity clipping rectangle
							if(IsGameLayer)
							{								
								// slow blinking to hint that it's not a part of the map
								double Seconds = time_get()/(double)time_freq();
								vec4 ColorHint = vec4(1.0f, 1.0f, 1.0f, 0.3f + 0.7f*(1.0+sin(2.0f*pi*Seconds/3.f))/2.0f);
								
								RenderTools()->RenderTileRectangle(-201, -201, pTMap->m_Width+402, pTMap->m_Height+402,
																   0, TILE_DEATH, // display air inside, death outside
																   32.0f, Color*ColorHint, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT,
																   EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
							}
							
							RenderTools()->RenderTilemap(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT,
															EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
						} else {
							Graphics()->BlendNormal();
							// draw kill tiles outside the entity clipping rectangle
							if (IsGameLayer)
							{
								// slow blinking to hint that it's not a part of the map
								double Seconds = time_get() / (double)time_freq();
								vec4 ColorHint = vec4(1.0f, 1.0f, 1.0f, 0.3f + 0.7f*(1.0 + sin(2.0f*pi*Seconds / 3.f)) / 2.0f);

								vec4 ColorKill(Color.x*ColorHint.x,Color.y*ColorHint.y,Color.z*ColorHint.z,Color.w*ColorHint.w);
								DrawKillTileBorder(TileLayerCounter-1, &ColorKill, pTMap, pGroup);
							}							
							RenderTileLayer(TileLayerCounter-1, &Color, pTMap, pGroup);
						}
					}
				}
				else if(pLayer->m_Type == LAYERTYPE_QUADS)
				{
					CMapItemLayerQuads *pQLayer = (CMapItemLayerQuads *)pLayer;
					if(pQLayer->m_Image == -1)
						Graphics()->TextureSet(-1);
					else
						Graphics()->TextureSet(m_pImages->Get(pQLayer->m_Image));

					CQuad *pQuads = (CQuad *)m_pLayers->Map()->GetDataSwapped(pQLayer->m_Data);
					if(m_Type == TYPE_BACKGROUND_FORCE) {
						if(g_Config.m_ClShowQuads){
							//Graphics()->BlendNone();
							//RenderTools()->ForceRenderQuads(pQuads, pQLayer->m_NumQuads, LAYERRENDERFLAG_OPAQUE, EnvelopeEval, this, 1.f);
							Graphics()->BlendNormal();
							RenderTools()->ForceRenderQuads(pQuads, pQLayer->m_NumQuads, LAYERRENDERFLAG_TRANSPARENT, EnvelopeEval, this, 1.f);						
						}						
					} else {
						//Graphics()->BlendNone();
						//RenderTools()->RenderQuads(pQuads, pQLayer->m_NumQuads, LAYERRENDERFLAG_OPAQUE, EnvelopeEval, this);
						Graphics()->BlendNormal();
						RenderTools()->RenderQuads(pQuads, pQLayer->m_NumQuads, LAYERRENDERFLAG_TRANSPARENT, EnvelopeEval, this);						
					}
				}
			}
			else if(Render && g_Config.m_ClOverlayEntities && IsFrontLayer)
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				Graphics()->TextureSet(m_pImages->GetEntities());

				CTile *pFrontTiles = (CTile *)m_pLayers->Map()->GetData(pTMap->m_Front);
				unsigned int Size = m_pLayers->Map()->GetDataSize(pTMap->m_Front);

				if (Size >= pTMap->m_Width*pTMap->m_Height*sizeof(CTile))
				{
					vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*g_Config.m_ClOverlayEntities/100.0f);						
					if(!Graphics()->IsBufferingEnabled()) {	
						Graphics()->BlendNone();
						RenderTools()->RenderTilemap(pFrontTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE,
								EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
						Graphics()->BlendNormal();
						RenderTools()->RenderTilemap(pFrontTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT,
								EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset); 
					} else {
						Graphics()->BlendNormal();					
						RenderTileLayer(TileLayerCounter-1, &Color, pTMap, pGroup);
					}
				}
			}
			else if(Render && g_Config.m_ClOverlayEntities && IsSwitchLayer)
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				Graphics()->TextureSet(m_pImages->GetEntities());

				CSwitchTile *pSwitchTiles = (CSwitchTile *)m_pLayers->Map()->GetData(pTMap->m_Switch);
				unsigned int Size = m_pLayers->Map()->GetDataSize(pTMap->m_Switch);

				if (Size >= pTMap->m_Width*pTMap->m_Height*sizeof(CSwitchTile))
				{
					vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*g_Config.m_ClOverlayEntities/100.0f);
					if(!Graphics()->IsBufferingEnabled()) {
						Graphics()->BlendNone();
						RenderTools()->RenderSwitchmap(pSwitchTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE);
						Graphics()->BlendNormal();
						RenderTools()->RenderSwitchmap(pSwitchTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT);
						RenderTools()->RenderSwitchOverlay(pSwitchTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, g_Config.m_ClOverlayEntities/100.0f);
					} else {
						Graphics()->BlendNormal();					
						RenderTileLayer(TileLayerCounter-3, &Color, pTMap, pGroup);
						Graphics()->TextureSet(m_pImages->GetOverlayBottom());
						RenderTileLayer(TileLayerCounter-2, &Color, pTMap, pGroup);
						Graphics()->TextureSet(m_pImages->GetOverlayTop());
						RenderTileLayer(TileLayerCounter-1, &Color, pTMap, pGroup);
					}
				}
			}
			else if(Render && g_Config.m_ClOverlayEntities && IsTeleLayer)
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				Graphics()->TextureSet(m_pImages->GetEntities());

				CTeleTile *pTeleTiles = (CTeleTile *)m_pLayers->Map()->GetData(pTMap->m_Tele);
				unsigned int Size = m_pLayers->Map()->GetDataSize(pTMap->m_Tele);

				if (Size >= pTMap->m_Width*pTMap->m_Height*sizeof(CTeleTile))
				{
					vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*g_Config.m_ClOverlayEntities/100.0f);
					if(!Graphics()->IsBufferingEnabled()) {
						Graphics()->BlendNone();
						RenderTools()->RenderTelemap(pTeleTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE);
						Graphics()->BlendNormal();
						RenderTools()->RenderTelemap(pTeleTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT);
						RenderTools()->RenderTeleOverlay(pTeleTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, g_Config.m_ClOverlayEntities/100.0f);
					} else {
						Graphics()->BlendNormal();
						RenderTileLayer(TileLayerCounter-2, &Color, pTMap, pGroup);
						Graphics()->TextureSet(m_pImages->GetOverlayCenter());
						RenderTileLayer(TileLayerCounter-1, &Color, pTMap, pGroup);
					}
				}
			}
			else if(Render && g_Config.m_ClOverlayEntities && IsSpeedupLayer)
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				Graphics()->TextureSet(m_pImages->GetEntities());

				CSpeedupTile *pSpeedupTiles = (CSpeedupTile *)m_pLayers->Map()->GetData(pTMap->m_Speedup);
				unsigned int Size = m_pLayers->Map()->GetDataSize(pTMap->m_Speedup);

				if (Size >= pTMap->m_Width*pTMap->m_Height*sizeof(CSpeedupTile))
				{
					vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*g_Config.m_ClOverlayEntities/100.0f);
					if(!Graphics()->IsBufferingEnabled()) {
						Graphics()->BlendNone();
						RenderTools()->RenderSpeedupmap(pSpeedupTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE);
						Graphics()->BlendNormal();
						RenderTools()->RenderSpeedupmap(pSpeedupTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT);
						RenderTools()->RenderSpeedupOverlay(pSpeedupTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, g_Config.m_ClOverlayEntities/100.0f);
					} else {
						Graphics()->BlendNormal();
						// draw arrow
						Graphics()->TextureSet(g_pData->m_aImages[IMAGE_SPEEDUP_ARROW].m_Id);
						RenderTileLayer(TileLayerCounter-3, &Color, pTMap, pGroup);
						Graphics()->TextureSet(m_pImages->GetOverlayBottom());
						RenderTileLayer(TileLayerCounter-2, &Color, pTMap, pGroup);
						Graphics()->TextureSet(m_pImages->GetOverlayTop());
						RenderTileLayer(TileLayerCounter-1, &Color, pTMap, pGroup);
					}
				}
			}
			else if(Render && g_Config.m_ClOverlayEntities && IsTuneLayer)
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				Graphics()->TextureSet(m_pImages->GetEntities());

				CTuneTile *pTuneTiles = (CTuneTile *)m_pLayers->Map()->GetData(pTMap->m_Tune);
				unsigned int Size = m_pLayers->Map()->GetDataSize(pTMap->m_Tune);

				if (Size >= pTMap->m_Width*pTMap->m_Height*sizeof(CTuneTile))
				{
					vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*g_Config.m_ClOverlayEntities/100.0f);
					if(!Graphics()->IsBufferingEnabled()) {
						Graphics()->BlendNone();
						RenderTools()->RenderTunemap(pTuneTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE);
						Graphics()->BlendNormal();
						RenderTools()->RenderTunemap(pTuneTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT);
						//RenderTools()->RenderTuneOverlay(pTuneTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, g_Config.m_ClOverlayEntities/100.0f);
					} else {
						Graphics()->BlendNormal();					
						RenderTileLayer(TileLayerCounter-1, &Color, pTMap, pGroup);
					}
				}
			}
		}
		if(!g_Config.m_GfxNoclip)
			Graphics()->ClipDisable();
	}

	if(!g_Config.m_GfxNoclip)
		Graphics()->ClipDisable();

	// reset the screen like it was before
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
}
