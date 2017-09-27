/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MAPLAYERS_H
#define GAME_CLIENT_COMPONENTS_MAPLAYERS_H
#include <game/client/component.h>

#include <vector>


#define INDEX_BUFFER_GROUP_WIDTH 12
#define INDEX_BUFFER_GROUP_HEIGHT 9
#define INDEX_BORDER_BUFFER_GROUP_SIZE 20

class CMapLayers : public CComponent
{
	friend class CBackground;

	CLayers *m_pLayers;
	class CMapImages *m_pImages;
	int m_Type;
	int m_CurrentLocalTick;
	int m_LastLocalTick;
	bool m_EnvelopeUpdate;

	void MapScreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup, float Zoom = 1.0f);
		
	struct STileLayerVisuals
	{
		STileLayerVisuals() : m_TilesOfLayer(NULL), m_BorderTop(NULL), m_BorderLeft(NULL), m_BorderRight(NULL), m_BorderBottom(NULL) 
		{
			m_Width = 0;
			m_Height = 0;
			m_VisualObjectsIndex = -1;
			m_IsTextured = false;
		}		
		
		bool Init(unsigned int Width, unsigned int Height);
		
		~STileLayerVisuals();	
		
		struct STileVisual
		{
			STileVisual() : m_IndexBufferByteOffset(0), m_TilesHandledCount(0), m_Draw(false)  { }
			char* m_IndexBufferByteOffset;
			unsigned int m_TilesHandledCount; //number of tiles that were handled before this tile + this tile if added
			bool m_Draw;
		};
		STileVisual** m_TilesOfLayer;
		
		STileVisual m_BorderTopLeft;
		STileVisual m_BorderTopRight;
		STileVisual m_BorderBottomRight;
		STileVisual m_BorderBottomLeft;
		
		STileVisual m_BorderKillTile; //end of map kill tile -- game layer only
		
		STileVisual* m_BorderTop;
		STileVisual* m_BorderLeft;
		STileVisual* m_BorderRight;
		STileVisual* m_BorderBottom;
		
		unsigned int m_Width;
		unsigned int m_Height;
		int m_VisualObjectsIndex;
		bool m_IsTextured;
	};
	std::vector<STileLayerVisuals*> m_TileLayerVisuals;
	
	int TileLayersOfGroup(CMapItemGroup* pGroup);
public:
	enum
	{
		TYPE_BACKGROUND=0,
		TYPE_BACKGROUND_FORCE,
		TYPE_FOREGROUND,
	};

	CMapLayers(int Type);
	virtual void OnInit();
	virtual void OnRender();
	virtual void OnMapLoad();
	
	void RenderTileLayer(int LayerIndex, vec4* pColor, CMapItemLayerTilemap* pTileLayer, CMapItemGroup* pGroup);
	void DrawTileBorder(int LayerIndex, vec4* pColor, CMapItemLayerTilemap* pTileLayer, CMapItemGroup* pGroup, int BorderX0, int BorderY0, int BorderX1, int BorderY1);
	void DrawKillTileBorder(int LayerIndex, vec4* pColor, CMapItemLayerTilemap* pTileLayer, CMapItemGroup* pGroup);
	
	void EnvelopeUpdate();

	static void EnvelopeEval(float TimeOffset, int Env, float *pChannels, void *pUser);
};

#endif
