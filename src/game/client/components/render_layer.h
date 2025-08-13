#ifndef GAME_CLIENT_COMPONENTS_RENDER_LAYER_H
#define GAME_CLIENT_COMPONENTS_RENDER_LAYER_H

#include <cstdint>

using offset_ptr_size = char *;
using offset_ptr = uintptr_t;
using offset_ptr32 = unsigned int;

#include <memory>
#include <optional>
#include <vector>

#include <base/color.h>
#include <engine/graphics.h>

#include <game/client/component.h>
#include <game/map/render_map.h>
#include <game/mapitems.h>
#include <game/mapitems_ex.h>

class CMapLayers;
class CMapItemLayerTilemap;
class CMapItemLayerQuads;
class IMap;
class CMapImages;

constexpr int BorderRenderDistance = 201;

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
};

class CRenderLayer : public CComponentInterfaces
{
public:
	CRenderLayer(int GroupId, int LayerId, int Flags);
	virtual ~CRenderLayer() = default;
	virtual void OnInit(CGameClient *pGameClient, IMap *pMap, CMapImages *pMapImages, std::shared_ptr<CMapBasedEnvelopePointAccess> &pEvelopePoints, bool OnlineOnly);

	virtual void Init() = 0;
	virtual void Render(const CRenderLayerParams &Params) = 0;
	virtual bool DoRender(const CRenderLayerParams &Params) const = 0;
	virtual bool IsValid() const { return true; }
	virtual bool IsGroup() const { return false; }
	virtual void Unload() = 0;

	int GetGroup() const { return m_GroupId; }

protected:
	static void EnvelopeEvalRenderLayer(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, void *pUser);

	int m_GroupId;
	int m_LayerId;
	int m_Flags;
	bool m_OnlineOnly;

	void UseTexture(IGraphics::CTextureHandle TextureHandle) const;
	virtual IGraphics::CTextureHandle GetTexture() const = 0;
	void RenderLoading() const;

	class IMap *m_pMap = nullptr;
	class CMapImages *m_pMapImages = nullptr;
	std::shared_ptr<CMapBasedEnvelopePointAccess> m_pEnvelopePoints;
};

class CRenderLayerGroup : public CRenderLayer
{
public:
	CRenderLayerGroup(int GroupId, CMapItemGroup *pGroup);
	virtual ~CRenderLayerGroup() = default;
	void Init() override {}
	void Render(const CRenderLayerParams &Params) override;
	bool DoRender(const CRenderLayerParams &Params) const override;
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
	virtual ~CRenderLayerTile() = default;
	void Render(const CRenderLayerParams &Params) override;
	bool DoRender(const CRenderLayerParams &Params) const override;
	void Init() override;
	void OnInit(CGameClient *pGameClient, IMap *pMap, CMapImages *pMapImages, std::shared_ptr<CMapBasedEnvelopePointAccess> &pEvelopePoints, bool OnlineOnly) override;

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
	class CTileLayerVisuals : public CComponentInterfaces
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

		std::vector<CTileVisual> m_vTilesOfLayer;

		CTileVisual m_BorderTopLeft;
		CTileVisual m_BorderTopRight;
		CTileVisual m_BorderBottomRight;
		CTileVisual m_BorderBottomLeft;

		CTileVisual m_BorderKillTile; // end of map kill tile -- game layer only

		std::vector<CTileVisual> m_vBorderTop;
		std::vector<CTileVisual> m_vBorderLeft;
		std::vector<CTileVisual> m_vBorderRight;
		std::vector<CTileVisual> m_vBorderBottom;

		unsigned int m_Width;
		unsigned int m_Height;
		int m_BufferContainerIndex;
		bool m_IsTextured;
	};

	void UploadTileData(std::optional<CTileLayerVisuals> &VisualsOptional, int CurOverlay, bool AddAsSpeedup, bool IsGameLayer = false);

	virtual void RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params);
	virtual void RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params);

	void RenderTileLayer(const ColorRGBA &Color, const CRenderLayerParams &Params, CTileLayerVisuals *pTileLayerVisuals = nullptr);
	void RenderTileBorder(const ColorRGBA &Color, int BorderX0, int BorderY0, int BorderX1, int BorderY1, CTileLayerVisuals *pTileLayerVisuals);
	void RenderKillTileBorder(const ColorRGBA &Color);

	std::optional<CRenderLayerTile::CTileLayerVisuals> m_VisualTiles;
	CMapItemLayerTilemap *m_pLayerTilemap;
	ColorRGBA m_Color;
};

class CRenderLayerQuads : public CRenderLayer
{
public:
	CRenderLayerQuads(int GroupId, int LayerId, IGraphics::CTextureHandle TextureHandle, int Flags, CMapItemLayerQuads *pLayerQuads);
	virtual void Init() override;
	bool IsValid() const override { return m_pLayerQuads->m_NumQuads > 0; }
	virtual void Render(const CRenderLayerParams &Params) override;
	virtual bool DoRender(const CRenderLayerParams &Params) const override;
	void Unload() override;

protected:
	virtual IGraphics::CTextureHandle GetTexture() const override { return m_TextureHandle; }
	void CalculateClipping();

	class CQuadLayerVisuals : public CComponentInterfaces
	{
	public:
		CQuadLayerVisuals() :
			m_QuadNum(0), m_BufferContainerIndex(-1), m_IsTextured(false) {}
		void Unload();

		int m_QuadNum;
		int m_BufferContainerIndex;
		bool m_IsTextured;
	};
	void RenderQuadLayer(bool ForceRender = false);

	std::optional<CRenderLayerQuads::CQuadLayerVisuals> m_VisualQuad;
	CMapItemLayerQuads *m_pLayerQuads;

	std::vector<SQuadRenderInfo> m_vQuadRenderInfo;

	bool m_Grouped;
	class CQuadRenderGroup
	{
	public:
		int m_PosEnv;
		float m_PosEnvOffset;
		int m_ColorEnv;
		float m_ColorEnvOffset;

		// quad clipping
		bool m_Clipped;
		float m_ClipX;
		float m_ClipY;
		float m_ClipWidth;
		float m_ClipHeight;
	} m_QuadRenderGroup;

private:
	IGraphics::CTextureHandle m_TextureHandle;
};

class CRenderLayerEntityBase : public CRenderLayerTile
{
public:
	CRenderLayerEntityBase(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	virtual ~CRenderLayerEntityBase() = default;
	bool DoRender(const CRenderLayerParams &Params) const override;

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
	void InitTileData() override;

protected:
	void RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params) override;
	void GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const override;

private:
	CTuneTile *m_pTuneTiles;
};
#endif
