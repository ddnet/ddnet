#ifndef GAME_MAP_RENDER_LAYER_H
#define GAME_MAP_RENDER_LAYER_H

#include <cstdint>

using offset_ptr_size = char *;
using offset_ptr = uintptr_t;
using offset_ptr32 = unsigned int;

#include <base/color.h>

#include <engine/graphics.h>

#include <game/map/envelope_manager.h>
#include <game/map/render_component.h>
#include <game/map/render_map.h>
#include <game/mapitems.h>
#include <game/mapitems_ex.h>

#include <memory>
#include <optional>
#include <variant>
#include <vector>

class CMapLayers;
class CMapItemLayerTilemap;
class CMapItemLayerQuads;
class IMap;
class CMapImages;

typedef std::function<void(const char *pCaption, const char *pContent, int IncreaseCounter)> FRenderUploadCallback;

constexpr int BorderRenderDistance = 201;

class CClipRegion
{
public:
	CClipRegion() = default;
	CClipRegion(float X, float Y, float Width, float Height) :
		m_X(X), m_Y(Y), m_Width(Width), m_Height(Height) {}

	float m_X;
	float m_Y;
	float m_Width;
	float m_Height;
};

class CRenderLayerParams
{
public:
	int m_RenderType;
	int m_EntityOverlayVal;
	vec2 m_Center;
	float m_Zoom;
	bool m_RenderText;
	bool m_RenderInvalidTiles;
	bool m_TileAndQuadBuffering;
	bool m_RenderTileBorder;
	bool m_DebugRenderGroupClips;
	bool m_DebugRenderQuadClips;
	bool m_DebugRenderClusterClips;
	bool m_DebugRenderTileClips;
	bool m_DebugRenderTileBorderClips;
};

class CRenderLayer : public CRenderComponent
{
public:
	CRenderLayer(int GroupId, int LayerId, int Flags);
	virtual void OnInit(IGraphics *pGraphics, ITextRender *pTextRender, CRenderMap *pRenderMap, std::shared_ptr<CEnvelopeManager> &pEnvelopeManager, IMap *pMap, IMapImages *pMapImages, std::optional<FRenderUploadCallback> &FRenderUploadCallbackOptional);

	virtual void Init() = 0;
	virtual void Render(const CRenderLayerParams &Params) = 0;
	virtual bool DoRender(const CRenderLayerParams &Params) = 0;
	virtual bool IsValid() const { return true; }
	virtual bool IsGroup() const { return false; }
	virtual void Unload() = 0;

	bool IsVisibleInClipRegion(const std::optional<CClipRegion> &ClipRegion) const;
	int GetGroup() const { return m_GroupId; }

protected:
	int m_GroupId;
	int m_LayerId;
	int m_Flags;

	void UseTexture(IGraphics::CTextureHandle TextureHandle);
	virtual IGraphics::CTextureHandle GetTexture() const = 0;
	void RenderLoading() const;

	class IMap *m_pMap = nullptr;
	IMapImages *m_pMapImages = nullptr;
	std::shared_ptr<CEnvelopeManager> m_pEnvelopeManager;
	std::optional<FRenderUploadCallback> m_RenderUploadCallback;
	std::optional<CClipRegion> m_LayerClip;
};

class CRenderLayerGroup : public CRenderLayer
{
public:
	CRenderLayerGroup(int GroupId, CMapItemGroup *pGroup);
	~CRenderLayerGroup() override = default;
	void Init() override {}
	void Render(const CRenderLayerParams &Params) override;
	bool DoRender(const CRenderLayerParams &Params) override;
	bool IsValid() const override { return m_pGroup != nullptr; }
	bool IsGroup() const override { return true; }
	void Unload() override {}

protected:
	IGraphics::CTextureHandle GetTexture() const override { return IGraphics::CTextureHandle(); }

	CMapItemGroup *m_pGroup;
};

class CRenderLayerTile : public CRenderLayer
{
public:
	CRenderLayerTile(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	~CRenderLayerTile() override = default;
	void Render(const CRenderLayerParams &Params) override;
	bool DoRender(const CRenderLayerParams &Params) override;
	void Init() override;
	void OnInit(IGraphics *pGraphics, ITextRender *pTextRender, CRenderMap *pRenderMap, std::shared_ptr<CEnvelopeManager> &pEnvelopeManager, IMap *pMap, IMapImages *pMapImages, std::optional<FRenderUploadCallback> &FRenderUploadCallbackOptional) override;

	virtual int GetDataIndex(unsigned int &TileSize) const;
	bool IsValid() const override { return GetRawData() != nullptr; }
	void Unload() override;

protected:
	virtual void *GetRawData() const;
	template<class T>
	T *GetData() const;

	virtual ColorRGBA GetRenderColor(const CRenderLayerParams &Params) const;
	virtual void InitTileData();
	virtual void GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const;
	IGraphics::CTextureHandle GetTexture() const override { return m_TextureHandle; }
	CTile *m_pTiles;

private:
	IGraphics::CTextureHandle m_TextureHandle;

protected:
	enum EBorderType
	{
		CORNER = 1,
		RIGHT = 2,
		LEFT = 4,
		TOP = 8,
		BOTTOM = 16,
	};

	class CTileVisual
	{
	public:
		CTileVisual() :
			m_IndexBufferByteOffset(0) {}

	private:
		offset_ptr32 m_IndexBufferByteOffset;

	public:
		bool DoDraw() const
		{
			return (m_IndexBufferByteOffset & 0x10000000) != 0;
		}

		void Draw(bool SetDraw)
		{
			m_IndexBufferByteOffset = (SetDraw ? 0x10000000 : (offset_ptr32)0) | (m_IndexBufferByteOffset & 0xEFFFFFFF);
		}

		offset_ptr IndexBufferByteOffset() const
		{
			return ((offset_ptr)(m_IndexBufferByteOffset & 0xEFFFFFFF) * 6 * sizeof(uint32_t));
		}

		void SetIndexBufferByteOffset(offset_ptr32 IndexBufferByteOff)
		{
			m_IndexBufferByteOffset = IndexBufferByteOff | (m_IndexBufferByteOffset & 0x10000000);
		}

		void AddIndexBufferByteOffset(offset_ptr32 IndexBufferByteOff)
		{
			m_IndexBufferByteOffset = ((m_IndexBufferByteOffset & 0xEFFFFFFF) + IndexBufferByteOff) | (m_IndexBufferByteOffset & 0x10000000);
		}
	};

	class CTileLayerVisuals;

	/*class IRenderTileLayerBorder : public CRenderComponent
	{
	public:
		IRenderTileLayerBorder(CClipRegion &ClipRegion) :
			m_BorderClip(ClipRegion) {}
		virtual ~IRenderTileLayerBorder() = default;
		virtual void Draw(const CTileLayerVisuals *pVisual, const ColorRGBA &Color, float ScreenX0, float ScreenY0, float ScreenX1, float ScreenY1) = 0;
		virtual constexpr int GetType() = 0;
		virtual bool Adjust() = 0;

		std::vector<CTileVisual> m_vBorder;
		CClipRegion m_BorderClip;
	};*/

	template<int Type>
	class CRenderTileLayerBorder : public CRenderComponent
	{
	public:
		CRenderTileLayerBorder(vec2 Offset, CClipRegion ClipRegion, size_t Size) :
			m_BorderClip(ClipRegion), m_Offset(Offset)
		{
			m_vBorder.resize(Size);
			m_FrontOffset = 0;
		}

		constexpr int GetType() { return Type; }

		void Draw(const CTileLayerVisuals *pVisual, const ColorRGBA &Color, float ScreenX0, float ScreenY0, float ScreenX1, float ScreenY1)
		{
			float BorderX = 0, BorderY = 0;
			if constexpr(Type & EBorderType::TOP)
			{
				if(ScreenY0 >= m_BorderClip.m_Y + m_BorderClip.m_Height)
					return;
				BorderY = m_BorderClip.m_Y + m_BorderClip.m_Height;
			}
			else if constexpr(Type & EBorderType::BOTTOM)
			{
				if(ScreenY1 <= m_BorderClip.m_Y)
					return;
				BorderY = m_BorderClip.m_Y;
			}

			if constexpr(Type & EBorderType::LEFT)
			{
				if(ScreenX0 >= m_BorderClip.m_X + m_BorderClip.m_Width)
					return;
				BorderX = m_BorderClip.m_X + m_BorderClip.m_Width;
			}

			else if constexpr(Type & EBorderType::RIGHT)
			{
				if(ScreenX1 <= m_BorderClip.m_X)
					return;
				BorderX = m_BorderClip.m_X;
			}

			// calculate how many tiles are needed to be drawn
			auto GetNumTiles = [](int Border, int TileOffset) -> float {
				// add an extra tile, for mouse movements
				return std::ceil(Border / 32.0f) - TileOffset + 1;
			};

			vec2 NumTiles(1.0f, 1.0f);
			if constexpr(Type & EBorderType::TOP)
				NumTiles.y = GetNumTiles(BorderY - ScreenY0, 0);
			else if constexpr(Type & EBorderType::BOTTOM)
				NumTiles.y = -GetNumTiles(BorderY - ScreenY1, pVisual->m_Height);
			if constexpr(Type & EBorderType::LEFT)
				NumTiles.x = GetNumTiles(BorderX - ScreenX0, 0);
			else if constexpr(Type & EBorderType::RIGHT)
				NumTiles.x = -GetNumTiles(BorderX - ScreenX1, pVisual->m_Width);

			if constexpr((Type & EBorderType::CORNER) != 0)
			{
				// corners only have one tile, which always is drawn
				offset_ptr_size pOffset = (offset_ptr_size)m_vBorder.data()->IndexBufferByteOffset();
				Graphics()->RenderBorderTiles(pVisual->m_BufferContainerIndex, Color, pOffset, m_Offset, NumTiles, 1u);
				return;
			}

			constexpr bool IsHorizontal = ((Type & (EBorderType::TOP | EBorderType::BOTTOM)) != 0);

			float StartScreen = IsHorizontal ? ScreenX0 : ScreenY0;
			float EndScreen = IsHorizontal ? ScreenX1 : ScreenY1;
			int StartIndex = (int)std::floor(StartScreen / 32.0f);
			int EndIndex = (int)std::ceil(EndScreen / 32.0f);

			// we deleted the front objects, because they contained no draws
			StartIndex -= m_FrontOffset;
			EndIndex -= m_FrontOffset;

			if(StartIndex >= (int)m_vBorder.size() || EndIndex < 0)
				return;

			StartIndex = std::max(StartIndex, 0);
			EndIndex = std::min(EndIndex, (int)m_vBorder.size() - 1);

			// probably impossible but safe
			if(EndIndex < StartIndex)
				return;

			const CTileVisual &StartVisual = m_vBorder[StartIndex];
			const CTileVisual &EndVisual = m_vBorder[EndIndex];

			unsigned int DrawNum = ((EndVisual.IndexBufferByteOffset() - StartVisual.IndexBufferByteOffset()) / (sizeof(unsigned int) * 6)) + (EndVisual.DoDraw() ? 1lu : 0lu);
			offset_ptr_size pOffset = (offset_ptr_size)StartVisual.IndexBufferByteOffset();
			Graphics()->RenderBorderTiles(pVisual->m_BufferContainerIndex, Color, pOffset, m_Offset, NumTiles, DrawNum);
		}

		bool Adjust()
		{
			// remove empty tiles from the back of the border
			while(!m_vBorder.empty() && !m_vBorder.back().DoDraw())
			{
				m_vBorder.pop_back();
			}

			// no tiles left
			if(m_vBorder.empty())
				return false;

			// corners need no clip region adjustment
			if constexpr(Type & EBorderType::CORNER)
				return true;

			// adjust clip region
			// TODO in theory we can remove the first TileId-1 tiles, but this is not trivial
			int FirstDrawId;
			for(FirstDrawId = 0; FirstDrawId < (int)m_vBorder.size(); ++FirstDrawId)
			{
				if(m_vBorder[FirstDrawId].DoDraw())
					break;
			}

			// this is guaranteed to have draws, since we 'pop_back-ed' the back above
			int LastDrawId = m_vBorder.size() - 1;
			if constexpr(Type & (EBorderType::TOP | EBorderType::BOTTOM))
			{
				m_BorderClip.m_X += FirstDrawId * 32.0f;
				m_BorderClip.m_Width = (LastDrawId - FirstDrawId + 1) * 32.0f;
			}
			else
			{
				m_BorderClip.m_Y += FirstDrawId * 32.0f;
				m_BorderClip.m_Height = (LastDrawId - FirstDrawId + 1) * 32.0f;
			}

			// remove empty front indices
			if(FirstDrawId > 0)
			{
				m_vBorder.erase(m_vBorder.begin(), m_vBorder.begin() + FirstDrawId - 1);
			}
			m_FrontOffset = FirstDrawId;
			return true;
		}

		std::vector<CTileVisual> m_vBorder;
		CClipRegion m_BorderClip;

	private:
		vec2 m_Offset;
		int m_FrontOffset;
	};

	class CTileLayerVisuals : public CRenderComponent
	{
	public:
		CTileLayerVisuals()
		{
			m_Width = 0;
			m_Height = 0;
			m_BufferContainerIndex = -1;
			m_IsTextured = false;
		}

		bool Init(unsigned int Width, unsigned int Height);
		void Unload();

		std::vector<CTileVisual> m_vTilesOfLayer;
		std::vector<std::variant<
			CRenderTileLayerBorder<EBorderType::CORNER | EBorderType::LEFT | EBorderType::TOP>,
			CRenderTileLayerBorder<EBorderType::CORNER | EBorderType::RIGHT | EBorderType::TOP>,
			CRenderTileLayerBorder<EBorderType::CORNER | EBorderType::LEFT | EBorderType::BOTTOM>,
			CRenderTileLayerBorder<EBorderType::CORNER | EBorderType::RIGHT | EBorderType::BOTTOM>,
			CRenderTileLayerBorder<EBorderType::TOP>,
			CRenderTileLayerBorder<EBorderType::BOTTOM>,
			CRenderTileLayerBorder<EBorderType::LEFT>,
			CRenderTileLayerBorder<EBorderType::RIGHT>>>
			m_vBorders;

		CTileVisual m_BorderKillTile; // end of map kill tile -- game layer only

		unsigned int m_Width;
		unsigned int m_Height;
		int m_BufferContainerIndex;
		bool m_IsTextured;
	};

	void UploadTileData(std::optional<CTileLayerVisuals> &VisualsOptional, int CurOverlay, bool AddAsSpeedup, bool IsGameLayer = false);

	virtual void RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params);
	virtual void RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params);

	void RenderTileLayer(const ColorRGBA &Color, const CRenderLayerParams &Params, CTileLayerVisuals *pTileLayerVisuals = nullptr);
	void RenderTileBorder(const ColorRGBA &Color, const CRenderLayerParams &Params, CTileLayerVisuals *pTileLayerVisuals);
	void RenderKillTileBorder(const ColorRGBA &Color);

	std::optional<CRenderLayerTile::CTileLayerVisuals> m_VisualTiles;
	CMapItemLayerTilemap *m_pLayerTilemap;
	ColorRGBA m_Color;
};

class CRenderLayerQuads : public CRenderLayer
{
public:
	CRenderLayerQuads(int GroupId, int LayerId, int Flags, CMapItemLayerQuads *pLayerQuads);
	void OnInit(IGraphics *pGraphics, ITextRender *pTextRender, CRenderMap *pRenderMap, std::shared_ptr<CEnvelopeManager> &pEnvelopeManager, IMap *pMap, IMapImages *pMapImages, std::optional<FRenderUploadCallback> &FRenderUploadCallbackOptional) override;
	void Init() override;
	bool IsValid() const override { return m_pLayerQuads->m_NumQuads > 0 && m_pQuads; }
	void Render(const CRenderLayerParams &Params) override;
	bool DoRender(const CRenderLayerParams &Params) override;
	void Unload() override;

protected:
	IGraphics::CTextureHandle GetTexture() const override { return m_TextureHandle; }

	class CQuadLayerVisuals : public CRenderComponent
	{
	public:
		CQuadLayerVisuals() :
			m_QuadNum(0), m_BufferContainerIndex(-1), m_IsTextured(false) {}
		void Unload();

		int m_QuadNum;
		int m_BufferContainerIndex;
		bool m_IsTextured;
	};
	void RenderQuadLayer(float Alpha, const CRenderLayerParams &Params);

	std::optional<CRenderLayerQuads::CQuadLayerVisuals> m_VisualQuad;
	CMapItemLayerQuads *m_pLayerQuads;

	class CQuadCluster
	{
	public:
		bool m_Grouped;
		int m_StartIndex;
		int m_NumQuads;

		int m_PosEnv;
		float m_PosEnvOffset;
		int m_ColorEnv;
		float m_ColorEnvOffset;

		std::vector<SQuadRenderInfo> m_vQuadRenderInfo;
		std::optional<CClipRegion> m_ClipRegion;
	};
	void CalculateClipping(CQuadCluster &QuadCluster);
	bool CalculateQuadClipping(const CQuadCluster &QuadCluster, float aQuadOffsetMin[2], float aQuadOffsetMax[2]) const;

	std::vector<CQuadCluster> m_vQuadClusters;
	CQuad *m_pQuads;

private:
	IGraphics::CTextureHandle m_TextureHandle;
};

class CRenderLayerEntityBase : public CRenderLayerTile
{
public:
	CRenderLayerEntityBase(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	~CRenderLayerEntityBase() override = default;
	bool DoRender(const CRenderLayerParams &Params) override;

protected:
	ColorRGBA GetRenderColor(const CRenderLayerParams &Params) const override { return ColorRGBA(1.0f, 1.0f, 1.0f, Params.m_EntityOverlayVal / 100.0f); }
	IGraphics::CTextureHandle GetTexture() const override;
};

class CRenderLayerEntityGame final : public CRenderLayerEntityBase
{
public:
	CRenderLayerEntityGame(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	void Init() override;

protected:
	void RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params) override;
	void RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params) override;

private:
	ColorRGBA GetDeathBorderColor() const;
};

class CRenderLayerEntityFront final : public CRenderLayerEntityBase
{
public:
	CRenderLayerEntityFront(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	int GetDataIndex(unsigned int &TileSize) const override;
};

class CRenderLayerEntityTele final : public CRenderLayerEntityBase
{
public:
	CRenderLayerEntityTele(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	int GetDataIndex(unsigned int &TileSize) const override;
	void Init() override;
	void InitTileData() override;
	void Unload() override;

protected:
	void RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params) override;
	void RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params) override;
	void GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const override;

private:
	std::optional<CRenderLayerTile::CTileLayerVisuals> m_VisualTeleNumbers;
	CTeleTile *m_pTeleTiles;
};

class CRenderLayerEntitySpeedup final : public CRenderLayerEntityBase
{
public:
	CRenderLayerEntitySpeedup(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	int GetDataIndex(unsigned int &TileSize) const override;
	void Init() override;
	void InitTileData() override;
	void Unload() override;

protected:
	void RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params) override;
	void RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params) override;
	void GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const override;
	IGraphics::CTextureHandle GetTexture() const override;

private:
	std::optional<CRenderLayerTile::CTileLayerVisuals> m_VisualForce;
	std::optional<CRenderLayerTile::CTileLayerVisuals> m_VisualMaxSpeed;
	CSpeedupTile *m_pSpeedupTiles;
};

class CRenderLayerEntitySwitch final : public CRenderLayerEntityBase
{
public:
	CRenderLayerEntitySwitch(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	int GetDataIndex(unsigned int &TileSize) const override;
	void Init() override;
	void InitTileData() override;
	void Unload() override;

protected:
	void RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params) override;
	void RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params) override;
	void GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const override;
	IGraphics::CTextureHandle GetTexture() const override;

private:
	std::optional<CRenderLayerTile::CTileLayerVisuals> m_VisualSwitchNumberTop;
	std::optional<CRenderLayerTile::CTileLayerVisuals> m_VisualSwitchNumberBottom;
	CSwitchTile *m_pSwitchTiles;
};

class CRenderLayerEntityTune final : public CRenderLayerEntityBase
{
public:
	CRenderLayerEntityTune(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	int GetDataIndex(unsigned int &TileSize) const override;
	void Init() override;
	void InitTileData() override;

protected:
	void RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params) override;
	void GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const override;
	IGraphics::CTextureHandle GetTexture() const override;

private:
	CTuneTile *m_pTuneTiles;
	mutable CTuneColorMapper m_TuneColorMapper;
};
#endif
