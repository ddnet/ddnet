/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MAPLAYERS_H
#define GAME_CLIENT_COMPONENTS_MAPLAYERS_H
#include <game/client/component.h>

#include <base/color.h>

#include <cstdint>
#include <vector>

#define INDEX_BUFFER_GROUP_WIDTH 12
#define INDEX_BUFFER_GROUP_HEIGHT 9
#define INDEX_BORDER_BUFFER_GROUP_SIZE 20

typedef char *offset_ptr_size;
typedef uintptr_t offset_ptr;
typedef unsigned int offset_ptr32;

struct CMapItemGroup;
struct CMapItemLayerTilemap;
struct CMapItemLayerQuads;

class CMapLayers : public CComponent
{
	friend class CBackground;
	friend class CMenuBackground;

	CLayers *m_pLayers;
	class CMapImages *m_pImages;
	int m_Type;
	int m_CurrentLocalTick;
	int m_LastLocalTick;
	bool m_EnvelopeUpdate;

	bool m_OnlineOnly;

	struct STileLayerVisuals
	{
		STileLayerVisuals() :
			m_pTilesOfLayer(nullptr), m_pBorderTop(nullptr), m_pBorderLeft(nullptr), m_pBorderRight(nullptr), m_pBorderBottom(nullptr)
		{
			m_Width = 0;
			m_Height = 0;
			m_BufferContainerIndex = -1;
			m_IsTextured = false;
		}

		bool Init(unsigned int Width, unsigned int Height);

		~STileLayerVisuals();

		struct STileVisual
		{
			STileVisual() :
				m_IndexBufferByteOffset(0) {}

		private:
			offset_ptr32 m_IndexBufferByteOffset;

		public:
			bool DoDraw()
			{
				return (m_IndexBufferByteOffset & 0x00000001) != 0;
			}

			void Draw(bool SetDraw)
			{
				m_IndexBufferByteOffset = (SetDraw ? 0x00000001 : (offset_ptr32)0) | (m_IndexBufferByteOffset & 0xFFFFFFFE);
			}

			offset_ptr IndexBufferByteOffset()
			{
				return ((offset_ptr)(m_IndexBufferByteOffset & 0xFFFFFFFE));
			}

			void SetIndexBufferByteOffset(offset_ptr32 IndexBufferByteOff)
			{
				m_IndexBufferByteOffset = IndexBufferByteOff | (m_IndexBufferByteOffset & 0x00000001);
			}

			void AddIndexBufferByteOffset(offset_ptr32 IndexBufferByteOff)
			{
				m_IndexBufferByteOffset = ((m_IndexBufferByteOffset & 0xFFFFFFFE) + IndexBufferByteOff) | (m_IndexBufferByteOffset & 0x00000001);
			}
		};
		STileVisual *m_pTilesOfLayer;

		STileVisual m_BorderTopLeft;
		STileVisual m_BorderTopRight;
		STileVisual m_BorderBottomRight;
		STileVisual m_BorderBottomLeft;

		STileVisual m_BorderKillTile; //end of map kill tile -- game layer only

		STileVisual *m_pBorderTop;
		STileVisual *m_pBorderLeft;
		STileVisual *m_pBorderRight;
		STileVisual *m_pBorderBottom;

		unsigned int m_Width;
		unsigned int m_Height;
		int m_BufferContainerIndex;
		bool m_IsTextured;
	};
	std::vector<STileLayerVisuals *> m_vpTileLayerVisuals;

	struct SQuadLayerVisuals
	{
		SQuadLayerVisuals() :
			m_QuadNum(0), m_pQuadsOfLayer(nullptr), m_BufferContainerIndex(-1), m_IsTextured(false) {}

		struct SQuadVisual
		{
			SQuadVisual() :
				m_IndexBufferByteOffset(0) {}

			offset_ptr m_IndexBufferByteOffset;
		};

		int m_QuadNum;
		SQuadVisual *m_pQuadsOfLayer;

		int m_BufferContainerIndex;
		bool m_IsTextured;
	};
	std::vector<SQuadLayerVisuals *> m_vpQuadLayerVisuals;

	virtual class CCamera *GetCurCamera();

	void LayersOfGroupCount(CMapItemGroup *pGroup, int &TileLayerCount, int &QuadLayerCount, bool &PassedGameLayer);

	void RenderTileBorderCornerTiles(int WidthOffsetToOrigin, int HeightOffsetToOrigin, int TileCountWidth, int TileCountHeight, int BufferContainerIndex, float *pColor, offset_ptr_size IndexBufferOffset, float *pOffset, float *pDir);

public:
	enum
	{
		TYPE_BACKGROUND = 0,
		TYPE_BACKGROUND_FORCE,
		TYPE_FOREGROUND,
		TYPE_FULL_DESIGN,
		TYPE_ALL = -1,
	};

	CMapLayers(int Type, bool OnlineOnly = true);
	virtual ~CMapLayers();
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;
	virtual void OnRender() override;
	virtual void OnMapLoad() override;

	void RenderTileLayer(int LayerIndex, ColorRGBA *pColor, CMapItemLayerTilemap *pTileLayer, CMapItemGroup *pGroup);
	void RenderTileBorder(int LayerIndex, ColorRGBA *pColor, CMapItemLayerTilemap *pTileLayer, CMapItemGroup *pGroup, int BorderX0, int BorderY0, int BorderX1, int BorderY1, int ScreenWidthTileCount, int ScreenHeightTileCount);
	void RenderKillTileBorder(int LayerIndex, ColorRGBA *pColor, CMapItemLayerTilemap *pTileLayer, CMapItemGroup *pGroup);
	void RenderQuadLayer(int LayerIndex, CMapItemLayerQuads *pQuadLayer, CMapItemGroup *pGroup, bool ForceRender = false);

	void EnvelopeUpdate();

	static void EnvelopeEval(int TimeOffsetMillis, int Env, float *pChannels, void *pUser);
};

#endif
