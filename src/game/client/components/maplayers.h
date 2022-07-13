/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MAPLAYERS_H
#define GAME_CLIENT_COMPONENTS_MAPLAYERS_H
#include <game/client/component.h>

#include <cstdint>
#include <vector>

#define INDEX_BUFFER_GROUP_WIDTH 12
#define INDEX_BUFFER_GROUP_HEIGHT 9
#define INDEX_BORDER_BUFFER_GROUP_SIZE 20

typedef char *offset_ptr_size;
typedef uintptr_t offset_ptr;
typedef unsigned int offset_ptr32;

class CCamera;
class CLayers;
class CMapImages;
class ColorRGBA;
struct CMapItemGroup;
struct CMapItemLayerTilemap;
struct CMapItemLayerQuads;

class CMapLayers : public CComponent
{
	friend class CBackground;
	friend class CMenuBackground;

	CLayers *m_pLayers = nullptr;
	CMapImages *m_pImages = nullptr;
	int m_Type = 0;
	int m_CurrentLocalTick = 0;
	int m_LastLocalTick = 0;
	bool m_EnvelopeUpdate = false;

	bool m_OnlineOnly = false;

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
			offset_ptr32 m_IndexBufferByteOffset = 0;

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
		STileVisual *m_pTilesOfLayer = nullptr;

		STileVisual m_BorderTopLeft;
		STileVisual m_BorderTopRight;
		STileVisual m_BorderBottomRight;
		STileVisual m_BorderBottomLeft;

		STileVisual m_BorderKillTile; //end of map kill tile -- game layer only

		STileVisual *m_pBorderTop = nullptr;
		STileVisual *m_pBorderLeft = nullptr;
		STileVisual *m_pBorderRight = nullptr;
		STileVisual *m_pBorderBottom = nullptr;

		unsigned int m_Width = 0;
		unsigned int m_Height = 0;
		int m_BufferContainerIndex = 0;
		bool m_IsTextured = false;
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

			offset_ptr m_IndexBufferByteOffset = 0;
		};

		int m_QuadNum = 0;
		SQuadVisual *m_pQuadsOfLayer = nullptr;

		int m_BufferContainerIndex = 0;
		bool m_IsTextured = false;
	};
	std::vector<SQuadLayerVisuals *> m_vpQuadLayerVisuals;

	virtual CCamera *GetCurCamera();

	void LayersOfGroupCount(CMapItemGroup *pGroup, int &TileLayerCount, int &QuadLayerCount, bool &PassedGameLayer);

	void RenderTileBorderCornerTiles(int WidthOffsetToOrigin, int HeightOffsetToOrigin, int TileCountWidth, int TileCountHeight, int BufferContainerIndex, const ColorRGBA &Color, offset_ptr_size IndexBufferOffset, const vec2 &Offset, const vec2 &Dir);

protected:
	virtual bool CanRenderMenuBackground() { return true; }

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

	void RenderTileLayer(int LayerIndex, ColorRGBA &Color, CMapItemLayerTilemap *pTileLayer, CMapItemGroup *pGroup);
	void RenderTileBorder(int LayerIndex, const ColorRGBA &Color, CMapItemLayerTilemap *pTileLayer, CMapItemGroup *pGroup, int BorderX0, int BorderY0, int BorderX1, int BorderY1, int ScreenWidthTileCount, int ScreenHeightTileCount);
	void RenderKillTileBorder(int LayerIndex, const ColorRGBA &Color, CMapItemLayerTilemap *pTileLayer, CMapItemGroup *pGroup);
	void RenderQuadLayer(int LayerIndex, CMapItemLayerQuads *pQuadLayer, CMapItemGroup *pGroup, bool ForceRender = false);

	void EnvelopeUpdate();

	static void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Channels, void *pUser);
};

#endif
