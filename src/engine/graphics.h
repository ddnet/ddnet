/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_GRAPHICS_H
#define ENGINE_GRAPHICS_H

#include "kernel.h"

#include <vector>
#define GRAPHICS_TYPE_UNSIGNED_BYTE 0x1401
#define GRAPHICS_TYPE_UNSIGNED_SHORT 0x1403
#define GRAPHICS_TYPE_INT 0x1404
#define GRAPHICS_TYPE_UNSIGNED_INT 0x1405
#define GRAPHICS_TYPE_FLOAT 0x1406
struct SBufferContainerInfo
{
	int m_Stride;

	// the attributes of the container
	struct SAttribute
	{
		int m_DataTypeCount;
		unsigned int m_Type;
		bool m_Normalized;
		void* m_pOffset;

		//0: float, 1:integer
		unsigned int m_FuncType;

		int m_VertBufferBindingIndex;
	};
	std::vector<SAttribute> m_Attributes;
};

struct SQuadRenderInfo
{
	float m_aColor[4];
	float m_aOffsets[2];
	float m_Rotation;
};

class CImageInfo
{
public:
	enum
	{
		FORMAT_AUTO=-1,
		FORMAT_RGB=0,
		FORMAT_RGBA=1,
		FORMAT_ALPHA=2,
	};

	/* Variable: width
		Contains the width of the image */
	int m_Width;

	/* Variable: height
		Contains the height of the image */
	int m_Height;

	/* Variable: format
		Contains the format of the image. See <Image Formats> for more information. */
	int m_Format;

	/* Variable: data
		Pointer to the image data. */
	void *m_pData;
};

/*
	Structure: CVideoMode
*/
class CVideoMode
{
public:
	int m_Width, m_Height;
	int m_Red, m_Green, m_Blue;
};

class IGraphics : public IInterface
{
	MACRO_INTERFACE("graphics", 0)
protected:
	int m_ScreenWidth;
	int m_ScreenHeight;
	int m_DesktopScreenWidth;
	int m_DesktopScreenHeight;
public:
	/* Constants: Texture Loading Flags
		TEXLOAD_NORESAMPLE - Prevents the texture from any resampling
	*/
	enum
	{
		TEXLOAD_NORESAMPLE = 1,
		TEXLOAD_NOMIPMAPS = 2,
	};

	int ScreenWidth() const { return m_ScreenWidth; }
	int ScreenHeight() const { return m_ScreenHeight; }
	float ScreenAspect() const { return (float)ScreenWidth()/(float)ScreenHeight(); }

	virtual bool Fullscreen(bool State) = 0;
	virtual void SetWindowBordered(bool State) = 0;
	virtual bool SetWindowScreen(int Index) = 0;
	virtual bool SetVSync(bool State) = 0;
	virtual int GetWindowScreen() = 0;
	virtual void Resize(int w, int h) = 0;

	virtual void Clear(float r, float g, float b) = 0;

	virtual void ClipEnable(int x, int y, int w, int h) = 0;
	virtual void ClipDisable() = 0;

	virtual void MapScreen(float TopLeftX, float TopLeftY, float BottomRightX, float BottomRightY) = 0;
	virtual void GetScreen(float *pTopLeftX, float *pTopLeftY, float *pBottomRightX, float *pBottomRightY) = 0;

	// TODO: These should perhaps not be virtuals
	virtual void BlendNone() = 0;
	virtual void BlendNormal() = 0;
	virtual void BlendAdditive() = 0;
	virtual void WrapNormal() = 0;
	virtual void WrapClamp() = 0;
	virtual int MemoryUsage() const = 0;

	virtual int LoadPNG(CImageInfo *pImg, const char *pFilename, int StorageType) = 0;
	virtual int UnloadTexture(int Index) = 0;
	virtual int LoadTextureRaw(int Width, int Height, int Format, const void *pData, int StoreFormat, int Flags) = 0;
	virtual int LoadTexture(const char *pFilename, int StorageType, int StoreFormat, int Flags) = 0;
	virtual int LoadTextureRawSub(int TextureID, int x, int y, int Width, int Height, int Format, const void *pData) = 0;
	virtual void TextureSet(int TextureID) = 0;

	virtual void FlushVertices(bool KeepVertices = false) = 0;
	virtual void FlushTextVertices(int TextureSize, int TextTextureIndex, int TextOutlineTextureIndex, float* pOutlineTextColor) = 0;

	// specific render functions
	virtual void RenderTileLayer(int BufferContainerIndex, float* pColor, char** pOffsets, unsigned int* IndicedVertexDrawNum, size_t NumIndicesOffet) = 0;
	virtual void RenderBorderTiles(int BufferContainerIndex, float* pColor, char* pOffset, float* Offset, float* Dir, int JumpIndex, unsigned int DrawNum) = 0;
	virtual void RenderBorderTileLines(int BufferContainerIndex, float* pColor, char* pOffset, float* Dir, unsigned int IndexDrawNum, unsigned int RedrawNum) = 0;
	virtual void RenderQuadLayer(int BufferContainerIndex, SQuadRenderInfo* pQuadInfo, int QuadNum) = 0;
	virtual void RenderText(int BufferContainerIndex, int TextQuadNum, int TextureSize, int TextureTextIndex, int TextureTextOutlineIndex, float* pTextColor, float* pTextoutlineColor) = 0;
	
	// opengl 3.3 functions
	virtual int CreateBufferObject(size_t UploadDataSize, void* pUploadData) = 0;
	virtual void RecreateBufferObject(int BufferIndex, size_t UploadDataSize, void* pUploadData) = 0;
	virtual void UpdateBufferObject(int BufferIndex, size_t UploadDataSize, void* pUploadData, void* pOffset) = 0;
	virtual void CopyBufferObject(int WriteBufferIndex, int ReadBufferIndex, size_t WriteOffset, size_t ReadOffset, size_t CopyDataSize) = 0;
	virtual void DeleteBufferObject(int BufferIndex) = 0;

	virtual int CreateBufferContainer(struct SBufferContainerInfo *pContainerInfo) = 0;
	// destroying all buffer objects means, that all referenced VBOs are destroyed automatically, so the user does not need to save references to them
	virtual void DeleteBufferContainer(int ContainerIndex, bool DestroyAllBO = true) = 0;
	virtual void UpdateBufferContainer(int ContainerIndex, struct SBufferContainerInfo *pContainerInfo) = 0;
	virtual void IndicesNumRequiredNotify(unsigned int RequiredIndicesCount) = 0;

	virtual bool IsBufferingEnabled() = 0;

	struct CLineItem
	{
		float m_X0, m_Y0, m_X1, m_Y1;
		CLineItem() {}
		CLineItem(float x0, float y0, float x1, float y1) : m_X0(x0), m_Y0(y0), m_X1(x1), m_Y1(y1) {}
	};
	virtual void LinesBegin() = 0;
	virtual void LinesEnd() = 0;
	virtual void LinesDraw(const CLineItem *pArray, int Num) = 0;

	virtual void QuadsBegin() = 0;
	virtual void QuadsEnd() = 0;
	virtual void TextQuadsBegin() = 0;
	virtual void TextQuadsEnd(int TextureSize, int TextTextureIndex, int TextOutlineTextureIndex, float* pOutlineTextColor) = 0;
	virtual void QuadsEndKeepVertices() = 0;
	virtual void QuadsDrawCurrentVertices(bool KeepVertices = true) = 0;
	virtual void QuadsSetRotation(float Angle) = 0;
	virtual void QuadsSetSubset(float TopLeftU, float TopLeftV, float BottomRightU, float BottomRightV) = 0;
	virtual void QuadsSetSubsetFree(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3) = 0;
	
	struct CQuadItem
	{
		float m_X, m_Y, m_Width, m_Height;
		CQuadItem() {}
		CQuadItem(float x, float y, float w, float h) : m_X(x), m_Y(y), m_Width(w), m_Height(h) {}
		void Set(float x, float y, float w, float h) { m_X = x; m_Y = y; m_Width = w; m_Height = h; }
	};
	virtual void QuadsDraw(CQuadItem *pArray, int Num) = 0;
	virtual void QuadsDrawTL(const CQuadItem *pArray, int Num) = 0;

	struct CFreeformItem
	{
		float m_X0, m_Y0, m_X1, m_Y1, m_X2, m_Y2, m_X3, m_Y3;
		CFreeformItem() {}
		CFreeformItem(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3)
			: m_X0(x0), m_Y0(y0), m_X1(x1), m_Y1(y1), m_X2(x2), m_Y2(y2), m_X3(x3), m_Y3(y3) {}
	};

	virtual int CreateQuadContainer() = 0;
	virtual void QuadContainerAddQuads(int ContainerIndex, CQuadItem *pArray, int Num) = 0;
	virtual void QuadContainerAddQuads(int ContainerIndex, CFreeformItem *pArray, int Num) = 0;
	virtual void QuadContainerReset(int ContainerIndex) = 0;
	virtual void DeleteQuadContainer(int ContainerIndex) = 0;
	virtual void RenderQuadContainer(int ContainerIndex, int QuadDrawNum) = 0;
	virtual void RenderQuadContainer(int ContainerIndex, int QuadOffset, int QuadDrawNum) = 0;
	virtual void RenderQuadContainerAsSprite(int ContainerIndex, int QuadOffset, float X, float Y, float ScaleX = 1.f, float ScaleY = 1.f) = 0;

	struct SRenderSpriteInfo
	{
		float m_Pos[2];
		float m_Scale;
		float m_Rotation;
	};

	virtual void RenderQuadContainerAsSpriteMultiple(int ContainerIndex, int QuadOffset, int DrawCount, SRenderSpriteInfo *pRenderInfo) = 0;

	virtual void QuadsDrawFreeform(const CFreeformItem *pArray, int Num) = 0;
	virtual void QuadsText(float x, float y, float Size, const char *pText) = 0;

	struct CColorVertex
	{
		int m_Index;
		float m_R, m_G, m_B, m_A;
		CColorVertex() {}
		CColorVertex(int i, float r, float g, float b, float a) : m_Index(i), m_R(r), m_G(g), m_B(b), m_A(a) {}
	};
	virtual void SetColorVertex(const CColorVertex *pArray, int Num) = 0;
	virtual void SetColor(float r, float g, float b, float a) = 0;
	virtual void ChangeColorOfCurrentQuadVertices(float r, float g, float b, float a) = 0;

	virtual void TakeScreenshot(const char *pFilename) = 0;
	virtual void TakeCustomScreenshot(const char *pFilename) = 0;
	virtual int GetVideoModes(CVideoMode *pModes, int MaxModes, int Screen) = 0;

	virtual void Swap() = 0;
	virtual int GetNumScreens() const = 0;

	// synchronization
	virtual void InsertSignal(class semaphore *pSemaphore) = 0;
	virtual bool IsIdle() = 0;
	virtual void WaitForIdle() = 0;

	virtual void SetWindowGrab(bool Grab) = 0;
	virtual void NotifyWindow() = 0;
};

class IEngineGraphics : public IGraphics
{
	MACRO_INTERFACE("enginegraphics", 0)
public:
	virtual int Init() = 0;
	virtual void Shutdown() = 0;

	virtual void Minimize() = 0;
	virtual void Maximize() = 0;

	virtual int WindowActive() = 0;
	virtual int WindowOpen() = 0;
};

extern IEngineGraphics *CreateEngineGraphics();
extern IEngineGraphics *CreateEngineGraphicsThreaded();

#endif
