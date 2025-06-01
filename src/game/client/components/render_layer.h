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

#include <game/client/render.h>
#include <game/mapitems.h>
#include <game/mapitems_ex.h>

class CMapLayers;
class CMapItemLayerTilemap;
class CMapItemLayerQuads;
class IMap;
class CMapImages;
class CRenderTools;
class IClient;
class CGameClient;

constexpr int BorderRenderDistance = 201;

class CRenderLayerParams
{
public:
	int m_RenderType;
	int EntityOverlayVal;
	vec2 m_Center;
};

class CRenderLayer
{
public:
	CRenderLayer(int GroupId, int LayerId, int Flags);
	virtual ~CRenderLayer() = default;
	void OnInit(IGraphics *pGraphics, IMap *pMap, CRenderTools *pRenderTools, CMapImages *pMapImages, std::shared_ptr<CMapBasedEnvelopePointAccess> &pEvelopePoints, IClient *pClient, CGameClient *pGameClient, bool OnlineOnly);

	virtual void Init() = 0;
	virtual void Render(const CRenderLayerParams &Params) = 0;
	virtual bool DoRender(const CRenderLayerParams &Params) const = 0;
	virtual bool IsValid() const { return true; }

	int GetGroup() const { return m_GroupId; }

protected:
	static void EnvelopeEvalRenderLayer(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, void *pUser);

	int m_GroupId;
	int m_LayerId;
	int m_Flags;
	bool m_OnlineOnly;

	void UseTexture(IGraphics::CTextureHandle TextureHandle) const;
	virtual IGraphics::CTextureHandle GetTexture() const = 0;
	const char *LoadingTitle() const;

	class IGraphics *m_pGraphics = nullptr;
	class IMap *m_pMap = nullptr;
	class CRenderTools *m_pRenderTools = nullptr;
	class CMapImages *m_pMapImages = nullptr;
	class IClient *m_pClient = nullptr;
	class CGameClient *m_pGameClient = nullptr;
	std::shared_ptr<CMapBasedEnvelopePointAccess> m_pEnvelopePoints;
};

class CRenderLayerTile : public CRenderLayer
{
public:
	CRenderLayerTile(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	virtual ~CRenderLayerTile() = default;
	void Render(const CRenderLayerParams &Params) override;
	bool DoRender(const CRenderLayerParams &Params) const override;
	void Init() override;

	virtual int GetDataIndex(unsigned int &TileSize) const;
	bool IsValid() const override { return GetRawData() != nullptr; }

protected:
	virtual void *GetRawData() const;
	template<class T>
	T *GetData() const;

	virtual ColorRGBA GetRenderColor(const CRenderLayerParams &Params) const;
	virtual void GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const;
	IGraphics::CTextureHandle GetTexture() const override { return m_TextureHandle; }

private:
	IGraphics::CTextureHandle m_TextureHandle;

protected:
	struct STileLayerVisuals
	{
		STileLayerVisuals() :
			m_pTilesOfLayer(nullptr)
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

		STileVisual *m_pTilesOfLayer;

		STileVisual m_BorderTopLeft;
		STileVisual m_BorderTopRight;
		STileVisual m_BorderBottomRight;
		STileVisual m_BorderBottomLeft;

		STileVisual m_BorderKillTile; // end of map kill tile -- game layer only

		std::vector<STileVisual> m_vBorderTop;
		std::vector<STileVisual> m_vBorderLeft;
		std::vector<STileVisual> m_vBorderRight;
		std::vector<STileVisual> m_vBorderBottom;

		unsigned int m_Width;
		unsigned int m_Height;
		int m_BufferContainerIndex;
		bool m_IsTextured;
	};

	void UploadTileData(std::optional<STileLayerVisuals> &VisualsOptional, int CurOverlay, bool AddAsSpeedup, bool IsGameLayer = false);

	virtual void RenderTileLayerWithTileBuffer(const ColorRGBA &Color);
	virtual void RenderTileLayerNoTileBuffer(const ColorRGBA &Color);

	void RenderTileLayer(const ColorRGBA &Color, STileLayerVisuals *pTileLayerVisuals = nullptr);
	void RenderTileBorder(const ColorRGBA &Color, int BorderX0, int BorderY0, int BorderX1, int BorderY1);
	void RenderKillTileBorder(const ColorRGBA &Color);

	std::optional<CRenderLayerTile::STileLayerVisuals> m_VisualTiles;
	CMapItemLayerTilemap *m_pLayerTilemap;
	ColorRGBA m_Color;
};

class CRenderLayerQuads : public CRenderLayer
{
public:
	CRenderLayerQuads(int GroupId, int LayerId, IGraphics::CTextureHandle TextureHandle, int Flags, CMapItemLayerQuads *pLayerQuads);
	virtual void Init() override;
	virtual void Render(const CRenderLayerParams &Params) override;
	virtual bool DoRender(const CRenderLayerParams &Params) const override;

protected:
	virtual IGraphics::CTextureHandle GetTexture() const override { return m_TextureHandle; }

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
	void RenderQuadLayer(bool ForceRender = false);

	std::optional<CRenderLayerQuads::SQuadLayerVisuals> m_VisualQuad;
	CMapItemLayerQuads *m_pLayerQuads;

	std::vector<SQuadRenderInfo> m_vQuadRenderInfo;
	bool m_ContainsEnvelopes;

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
	ColorRGBA GetRenderColor(const CRenderLayerParams &Params) const override { return ColorRGBA(1.0f, 1.0f, 1.0f, Params.EntityOverlayVal / 100.0f); }
	IGraphics::CTextureHandle GetTexture() const override;
};

class CRenderLayerEntityGame final : public CRenderLayerEntityBase
{
public:
	CRenderLayerEntityGame(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	void Init() override;

protected:
	void RenderTileLayerWithTileBuffer(const ColorRGBA &Color) override;
	void RenderTileLayerNoTileBuffer(const ColorRGBA &Color) override;

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

protected:
	void RenderTileLayerWithTileBuffer(const ColorRGBA &Color) override;
	void RenderTileLayerNoTileBuffer(const ColorRGBA &Color) override;
	void GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const override;

private:
	std::optional<CRenderLayerTile::STileLayerVisuals> m_VisualTeleNumbers;
};

class CRenderLayerEntitySpeedup final : public CRenderLayerEntityBase
{
public:
	CRenderLayerEntitySpeedup(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	int GetDataIndex(unsigned int &TileSize) const override;
	void Init() override;

protected:
	void RenderTileLayerWithTileBuffer(const ColorRGBA &Color) override;
	void RenderTileLayerNoTileBuffer(const ColorRGBA &Color) override;
	void GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const override;
	IGraphics::CTextureHandle GetTexture() const override;

private:
	std::optional<CRenderLayerTile::STileLayerVisuals> m_VisualForce;
	std::optional<CRenderLayerTile::STileLayerVisuals> m_VisualMaxSpeed;
};

class CRenderLayerEntitySwitch final : public CRenderLayerEntityBase
{
public:
	CRenderLayerEntitySwitch(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	int GetDataIndex(unsigned int &TileSize) const override;
	void Init() override;

protected:
	void RenderTileLayerWithTileBuffer(const ColorRGBA &Color) override;
	void RenderTileLayerNoTileBuffer(const ColorRGBA &Color) override;
	void GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const override;
	IGraphics::CTextureHandle GetTexture() const override;

private:
	std::optional<CRenderLayerTile::STileLayerVisuals> m_VisualSwitchNumberTop;
	std::optional<CRenderLayerTile::STileLayerVisuals> m_VisualSwitchNumberBottom;
};

class CRenderLayerEntityTune final : public CRenderLayerEntityBase
{
public:
	CRenderLayerEntityTune(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	int GetDataIndex(unsigned int &TileSize) const override;

protected:
	void RenderTileLayerNoTileBuffer(const ColorRGBA &Color) override;
	void GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const override;
};
#endif
