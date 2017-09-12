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
	
	struct STileLayerVisuals{
		~STileLayerVisuals() { m_IndexBufferGroups.clear(); }
		int m_VisualObjectIndex;
		bool m_IsTextured;
		
		struct SIndexBufferGroup{
			SIndexBufferGroup() : m_ByteOffset(0), m_NumTilesToRender(0) {}
			SIndexBufferGroup(char* Offset) : m_ByteOffset(Offset), m_NumTilesToRender(0) {}
			char* m_ByteOffset;
			unsigned int m_NumTilesToRender;
		};
		std::vector<SIndexBufferGroup> m_IndexBufferGroups; //we create 12*9 fields of tiles, and thats how the index buffer is build -- and store the byte offsets of each field
		SIndexBufferGroup m_BorderTopLeft;
		SIndexBufferGroup m_BorderBottomLeft;
		SIndexBufferGroup m_BorderTopRight;
		SIndexBufferGroup m_BorderBottomRight;
		
		SIndexBufferGroup m_BorderKillTile; //end of map kill tile -- game layer only
		
		std::vector<SIndexBufferGroup> m_TopBorderIndexBufferGroups; //for the border we create 20x1 fields
		std::vector<SIndexBufferGroup> m_LeftBorderIndexBufferGroups; //for the border we create 20x1 fields
		std::vector<SIndexBufferGroup> m_RightBorderIndexBufferGroups; //for the border we create 20x1 fields
		std::vector<SIndexBufferGroup> m_BottomBorderIndexBufferGroups; //for the border we create 20x1 fields
	};
	std::vector<STileLayerVisuals> m_TileLayerVisuals;
	
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
