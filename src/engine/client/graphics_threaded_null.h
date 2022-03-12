#ifndef ENGINE_CLIENT_GRAPHICS_THREADED_NULL_H
#define ENGINE_CLIENT_GRAPHICS_THREADED_NULL_H

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <cstddef>
#include <vector>

class CGraphics_ThreadedNull : public IEngineGraphics
{
public:
	CGraphics_ThreadedNull()
	{
		m_ScreenWidth = 800;
		m_ScreenHeight = 600;
		m_ScreenRefreshRate = 24;
		m_ScreenHiDPIScale = 1.0f;
	};

	void ClipEnable(int x, int y, int w, int h) override{};
	void ClipDisable() override{};

	void BlendNone() override{};
	void BlendNormal() override{};
	void BlendAdditive() override{};

	void WrapNormal() override{};
	void WrapClamp() override{};

	int MemoryUsage() const override { return 0; }

	void MapScreen(float TopLeftX, float TopLeftY, float BottomRightX, float BottomRightY) override{};
	void GetScreen(float *pTopLeftX, float *pTopLeftY, float *pBottomRightX, float *pBottomRightY) override
	{
		*pTopLeftX = 0;
		*pTopLeftY = 0;
		*pBottomRightX = 800;
		*pBottomRightY = 600;
	};

	void LinesBegin() override{};
	void LinesEnd() override{};
	void LinesDraw(const CLineItem *pArray, int Num) override{};

	int UnloadTexture(IGraphics::CTextureHandle *pIndex) override { return 0; }
	IGraphics::CTextureHandle LoadTextureRaw(int Width, int Height, int Format, const void *pData, int StoreFormat, int Flags, const char *pTexName = NULL) override { return CreateTextureHandle(0); }
	int LoadTextureRawSub(IGraphics::CTextureHandle TextureID, int x, int y, int Width, int Height, int Format, const void *pData) override { return 0; }

	CTextureHandle LoadSpriteTextureImpl(CImageInfo &FromImageInfo, int x, int y, int w, int h) { return CreateTextureHandle(0); }
	CTextureHandle LoadSpriteTexture(CImageInfo &FromImageInfo, struct CDataSprite *pSprite) override { return CreateTextureHandle(0); }
	CTextureHandle LoadSpriteTexture(CImageInfo &FromImageInfo, struct client_data7::CDataSprite *pSprite) override { return CreateTextureHandle(0); }

	bool IsImageSubFullyTransparent(CImageInfo &FromImageInfo, int x, int y, int w, int h) override { return false; }
	bool IsSpriteTextureFullyTransparent(CImageInfo &FromImageInfo, struct client_data7::CDataSprite *pSprite) override { return false; }

	// simple uncompressed RGBA loaders
	IGraphics::CTextureHandle LoadTexture(const char *pFilename, int StorageType, int StoreFormat, int Flags) override { return CreateTextureHandle(0); }
	int LoadPNG(CImageInfo *pImg, const char *pFilename, int StorageType) override { return 0; }
	void FreePNG(CImageInfo *pImg) override{};

	bool CheckImageDivisibility(const char *pFileName, CImageInfo &Img, int DivX, int DivY, bool AllowResize) override { return false; }
	bool IsImageFormatRGBA(const char *pFileName, CImageInfo &Img) override { return false; }

	void CopyTextureBufferSub(uint8_t *pDestBuffer, uint8_t *pSourceBuffer, int FullWidth, int FullHeight, int ColorChannelCount, int SubOffsetX, int SubOffsetY, int SubCopyWidth, int SubCopyHeight) override{};
	void CopyTextureFromTextureBufferSub(uint8_t *pDestBuffer, int DestWidth, int DestHeight, uint8_t *pSourceBuffer, int SrcWidth, int SrcHeight, int ColorChannelCount, int SrcSubOffsetX, int SrcSubOffsetY, int SrcSubCopyWidth, int SrcSubCopyHeight) override{};

	void TextureSet(CTextureHandle TextureID) override{};

	void Clear(float r, float g, float b) override{};

	void QuadsBegin() override{};
	void QuadsEnd() override{};
	void TextQuadsBegin() override{};
	void TextQuadsEnd(int TextureSize, int TextTextureIndex, int TextOutlineTextureIndex, float *pOutlineTextColor) override{};
	void QuadsTex3DBegin() override{};
	void QuadsTex3DEnd() override{};
	void TrianglesBegin() override{};
	void TrianglesEnd() override{};
	void QuadsEndKeepVertices() override{};
	void QuadsDrawCurrentVertices(bool KeepVertices = true) override{};
	void QuadsSetRotation(float Angle) override{};

	void SetColorVertex(const CColorVertex *pArray, int Num) override{};
	void SetColor(float r, float g, float b, float a) override{};
	void SetColor(ColorRGBA rgb) override{};
	void SetColor4(vec4 TopLeft, vec4 TopRight, vec4 BottomLeft, vec4 BottomRight) override{};

	// go through all vertices and change their color (only works for quads)
	void ChangeColorOfCurrentQuadVertices(float r, float g, float b, float a) override{};
	void ChangeColorOfQuadVertices(int QuadOffset, unsigned char r, unsigned char g, unsigned char b, unsigned char a) override{};

	void QuadsSetSubset(float TlU, float TlV, float BrU, float BrV) override{};
	void QuadsSetSubsetFree(
		float x0, float y0, float x1, float y1,
		float x2, float y2, float x3, float y3, int Index = -1) override{};

	void QuadsDraw(CQuadItem *pArray, int Num) override{};

	void QuadsDrawTL(const CQuadItem *pArray, int Num) override{};

	void QuadsTex3DDrawTL(const CQuadItem *pArray, int Num) override{};

	void QuadsDrawFreeform(const CFreeformItem *pArray, int Num) override{};
	void QuadsText(float x, float y, float Size, const char *pText) override{};

	GL_STexCoord m_aTexCoords[4];
	GL_SColor m_aColors[4];
	const GL_STexCoord *GetCurTextureCoordinates() override { return m_aTexCoords; }
	const GL_SColor *GetCurColor() override { return m_aColors; }

	int CreateQuadContainer(bool AutomaticUpload = true) override { return -1; }
	void QuadContainerChangeAutomaticUpload(int ContainerIndex, bool AutomaticUpload) override {}
	void QuadContainerUpload(int ContainerIndex) override{};
	void QuadContainerAddQuads(int ContainerIndex, CQuadItem *pArray, int Num) override{};
	void QuadContainerAddQuads(int ContainerIndex, CFreeformItem *pArray, int Num) override{};
	void QuadContainerReset(int ContainerIndex) override{};
	void DeleteQuadContainer(int ContainerIndex) override{};
	void RenderQuadContainer(int ContainerIndex, int QuadDrawNum) override{};
	void RenderQuadContainer(int ContainerIndex, int QuadOffset, int QuadDrawNum, bool ChangeWrapMode = true) override{};
	void RenderQuadContainerEx(int ContainerIndex, int QuadOffset, int QuadDrawNum, float X, float Y, float ScaleX = 1.f, float ScaleY = 1.f) override{};
	void RenderQuadContainerAsSprite(int ContainerIndex, int QuadOffset, float X, float Y, float ScaleX = 1.f, float ScaleY = 1.f) override{};
	void RenderQuadContainerAsSpriteMultiple(int ContainerIndex, int QuadOffset, int DrawCount, SRenderSpriteInfo *pRenderInfo) override{};

	void FlushVertices(bool KeepVertices = false) override{};
	void FlushTextVertices(int TextureSize, int TextTextureIndex, int TextOutlineTextureIndex, float *pOutlineTextColor) override{};
	void FlushVerticesTex3D() override{};

	void RenderTileLayer(int BufferContainerIndex, float *pColor, char **pOffsets, unsigned int *IndicedVertexDrawNum, size_t NumIndicesOffet) override{};
	void RenderBorderTiles(int BufferContainerIndex, float *pColor, char *pIndexBufferOffset, float *pOffset, float *pDir, int JumpIndex, unsigned int DrawNum) override{};
	void RenderBorderTileLines(int BufferContainerIndex, float *pColor, char *pIndexBufferOffset, float *pOffset, float *pDir, unsigned int IndexDrawNum, unsigned int RedrawNum) override{};
	void RenderQuadLayer(int BufferContainerIndex, SQuadRenderInfo *pQuadInfo, int QuadNum, int QuadOffset) override{};
	void RenderText(int BufferContainerIndex, int TextQuadNum, int TextureSize, int TextureTextIndex, int TextureTextOutlineIndex, float *pTextColor, float *pTextoutlineColor) override{};

	// opengl 3.3 functions
	int CreateBufferObject(size_t UploadDataSize, void *pUploadData, bool IsMovedPointer = false) override { return 0; }
	void RecreateBufferObject(int BufferIndex, size_t UploadDataSize, void *pUploadData, bool IsMovedPointer = false) override{};
	void UpdateBufferObject(int BufferIndex, size_t UploadDataSize, void *pUploadData, void *pOffset, bool IsMovedPointer = false) override{};
	void CopyBufferObject(int WriteBufferIndex, int ReadBufferIndex, size_t WriteOffset, size_t ReadOffset, size_t CopyDataSize) override{};
	void DeleteBufferObject(int BufferIndex) override{};

	int CreateBufferContainer(SBufferContainerInfo *pContainerInfo) override { return 0; }
	// destroying all buffer objects means, that all referenced VBOs are destroyed automatically, so the user does not need to save references to them
	void DeleteBufferContainer(int ContainerIndex, bool DestroyAllBO = true) override{};
	void UpdateBufferContainer(int ContainerIndex, SBufferContainerInfo *pContainerInfo) override{};
	void IndicesNumRequiredNotify(unsigned int RequiredIndicesCount) override{};

	int GetNumScreens() const override { return 0; }
	void Minimize() override{};
	void Maximize() override{};
	void SetWindowParams(int FullscreenMode, bool IsBorderless, bool AllowResizing) override{};
	bool SetWindowScreen(int Index) override { return false; }
	void Move(int x, int y) override{};
	void Resize(int w, int h, int RefreshRate) override{};
	void GotResized(int w, int h, int RefreshRate) override{};
	void AddWindowResizeListener(WINDOW_RESIZE_FUNC pFunc, void *pUser) override{};
	int GetWindowScreen() override { return 0; }

	void WindowDestroyNtf(uint32_t WindowID) override{};
	void WindowCreateNtf(uint32_t WindowID) override{};

	int WindowActive() override { return 1; }
	int WindowOpen() override { return 0; }

	void SetWindowGrab(bool Grab) override{};
	void NotifyWindow() override{};

	int Init() override { return 0; }
	void Shutdown() override{};

	void TakeScreenshot(const char *pFilename) override{};
	void TakeCustomScreenshot(const char *pFilename) override{};
	void Swap() override{};
	bool SetVSync(bool State) override { return false; }

	int GetVideoModes(CVideoMode *pModes, int MaxModes, int Screen) override { return 0; }

	virtual int GetDesktopScreenWidth() const { return g_Config.m_GfxDesktopWidth; }
	virtual int GetDesktopScreenHeight() const { return g_Config.m_GfxDesktopHeight; }

	// synchronization
	void InsertSignal(CSemaphore *pSemaphore) override{};
	bool IsIdle() const override { return true; }
	void WaitForIdle() override{};

	SWarning *GetCurWarning() override { return NULL; }

	void GetDriverVersion(EGraphicsDriverAgeType DriverAgeType, int &Major, int &Minor, int &Patch) override {}
	bool IsConfigModernAPI() override { return false; }
	bool IsTileBufferingEnabled() override { return false; }
	bool IsQuadBufferingEnabled() override { return false; }
	bool IsTextBufferingEnabled() override { return false; }
	bool IsQuadContainerBufferingEnabled() override { return false; }
	bool HasTextureArrays() override { return false; }

	const char *GetVendorString() override { return "headless"; }
	const char *GetVersionString() override { return "headless"; }
	const char *GetRendererString() override { return "headless"; }
};

#endif // ENGINE_CLIENT_GRAPHICS_THREADED_NULL_H
