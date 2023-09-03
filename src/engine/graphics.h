/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_GRAPHICS_H
#define ENGINE_GRAPHICS_H

#include "kernel.h"
#include "warning.h"

#include <base/color.h>
#include <stddef.h>
#include <stdint.h>

#include <functional>

#include <vector>
#define GRAPHICS_TYPE_UNSIGNED_BYTE 0x1401
#define GRAPHICS_TYPE_UNSIGNED_SHORT 0x1403
#define GRAPHICS_TYPE_INT 0x1404
#define GRAPHICS_TYPE_UNSIGNED_INT 0x1405
#define GRAPHICS_TYPE_FLOAT 0x1406
struct SBufferContainerInfo
{
	int m_Stride;
	int m_VertBufferBindingIndex;

	// the attributes of the container
	struct SAttribute
	{
		int m_DataTypeCount;
		unsigned int m_Type;
		bool m_Normalized;
		void *m_pOffset;

		//0: float, 1:integer
		unsigned int m_FuncType;
	};
	std::vector<SAttribute> m_vAttributes;
};

struct SQuadRenderInfo
{
	ColorRGBA m_Color;
	vec2 m_Offsets;
	float m_Rotation;
	// allows easier upload for uniform buffers because of the alignment requirements
	float m_Padding;
};

struct SGraphicTile
{
	vec2 m_TopLeft;
	vec2 m_TopRight;
	vec2 m_BottomRight;
	vec2 m_BottomLeft;
};

struct SGraphicTileTexureCoords
{
	vec3 m_TexCoordTopLeft;
	vec3 m_TexCoordTopRight;
	vec3 m_TexCoordBottomRight;
	vec3 m_TexCoordBottomLeft;
};

class CImageInfo
{
public:
	enum
	{
		FORMAT_AUTO = -1,
		FORMAT_RGB = 0,
		FORMAT_RGBA = 1,
		FORMAT_SINGLE_COMPONENT = 2,
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
	int m_CanvasWidth, m_CanvasHeight;
	int m_WindowWidth, m_WindowHeight;
	int m_RefreshRate;
	int m_Red, m_Green, m_Blue;
	uint32_t m_Format;
};

typedef vec2 GL_SPoint;
typedef vec2 GL_STexCoord;

struct GL_STexCoord3D
{
	GL_STexCoord3D &operator=(const GL_STexCoord &TexCoord)
	{
		u = TexCoord.u;
		v = TexCoord.v;
		return *this;
	}

	GL_STexCoord3D &operator=(const vec3 &TexCoord)
	{
		u = TexCoord.u;
		v = TexCoord.v;
		w = TexCoord.w;
		return *this;
	}

	float u, v, w;
};

typedef ColorRGBA GL_SColorf;
//use normalized color values
typedef vector4_base<unsigned char> GL_SColor;

struct GL_SVertex
{
	GL_SPoint m_Pos;
	GL_STexCoord m_Tex;
	GL_SColor m_Color;
};

struct GL_SVertexTex3D
{
	GL_SPoint m_Pos;
	GL_SColorf m_Color;
	GL_STexCoord3D m_Tex;
};

struct GL_SVertexTex3DStream
{
	GL_SPoint m_Pos;
	GL_SColor m_Color;
	GL_STexCoord3D m_Tex;
};

static constexpr size_t gs_GraphicsMaxQuadsRenderCount = 256;
static constexpr size_t gs_GraphicsMaxParticlesRenderCount = 512;

enum EGraphicsDriverAgeType
{
	GRAPHICS_DRIVER_AGE_TYPE_LEGACY = 0,
	GRAPHICS_DRIVER_AGE_TYPE_DEFAULT,
	GRAPHICS_DRIVER_AGE_TYPE_MODERN,

	GRAPHICS_DRIVER_AGE_TYPE_COUNT,
};

enum EBackendType
{
	BACKEND_TYPE_OPENGL = 0,
	BACKEND_TYPE_OPENGL_ES,
	BACKEND_TYPE_VULKAN,

	// special value to tell the backend to identify the current backend
	BACKEND_TYPE_AUTO,

	BACKEND_TYPE_COUNT,
};

struct STWGraphicGPU
{
	enum ETWGraphicsGPUType
	{
		GRAPHICS_GPU_TYPE_DISCRETE = 0,
		GRAPHICS_GPU_TYPE_INTEGRATED,
		GRAPHICS_GPU_TYPE_VIRTUAL,
		GRAPHICS_GPU_TYPE_CPU,

		// should stay at last position in this enum
		GRAPHICS_GPU_TYPE_INVALID,
	};

	struct STWGraphicGPUItem
	{
		char m_aName[256];
		ETWGraphicsGPUType m_GPUType;
	};
	std::vector<STWGraphicGPUItem> m_vGPUs;
	STWGraphicGPUItem m_AutoGPU;
};

typedef STWGraphicGPU TTWGraphicsGPUList;

typedef std::function<void(void *)> WINDOW_RESIZE_FUNC;

namespace client_data7 {
struct CDataSprite; // NOLINT(bugprone-forward-declaration-namespace)
}

typedef std::function<bool(uint32_t &Width, uint32_t &Height, uint32_t &Format, std::vector<uint8_t> &vDstData)> TGLBackendReadPresentedImageData;

class IGraphics : public IInterface
{
	MACRO_INTERFACE("graphics", 0)
protected:
	int m_ScreenWidth;
	int m_ScreenHeight;
	int m_ScreenRefreshRate;
	float m_ScreenHiDPIScale;

public:
	enum
	{
		TEXLOAD_NOMIPMAPS = 1 << 1,
		TEXLOAD_NO_COMPRESSION = 1 << 2,
		TEXLOAD_TO_3D_TEXTURE = (1 << 3),
		TEXLOAD_TO_2D_ARRAY_TEXTURE = (1 << 4),
		TEXLOAD_TO_3D_TEXTURE_SINGLE_LAYER = (1 << 5),
		TEXLOAD_TO_2D_ARRAY_TEXTURE_SINGLE_LAYER = (1 << 6),
		TEXLOAD_NO_2D_TEXTURE = (1 << 7),
	};

	class CTextureHandle
	{
		friend class IGraphics;
		int m_Id;

	public:
		CTextureHandle() :
			m_Id(-1)
		{
		}

		bool IsValid() const { return Id() >= 0; }
		int Id() const { return m_Id; }
		void Invalidate() { m_Id = -1; }
	};

	int ScreenWidth() const { return m_ScreenWidth; }
	int ScreenHeight() const { return m_ScreenHeight; }
	float ScreenAspect() const { return (float)ScreenWidth() / (float)ScreenHeight(); }
	float ScreenHiDPIScale() const { return m_ScreenHiDPIScale; }
	int WindowWidth() const { return m_ScreenWidth / m_ScreenHiDPIScale; }
	int WindowHeight() const { return m_ScreenHeight / m_ScreenHiDPIScale; }

	virtual void WarnPngliteIncompatibleImages(bool Warn) = 0;
	virtual void SetWindowParams(int FullscreenMode, bool IsBorderless, bool AllowResizing) = 0;
	virtual bool SetWindowScreen(int Index) = 0;
	virtual bool SetVSync(bool State) = 0;
	virtual bool SetMultiSampling(uint32_t ReqMultiSamplingCount, uint32_t &MultiSamplingCountBackend) = 0;
	virtual int GetWindowScreen() = 0;
	virtual void Move(int x, int y) = 0;
	virtual void Resize(int w, int h, int RefreshRate) = 0;
	virtual void GotResized(int w, int h, int RefreshRate) = 0;
	virtual void UpdateViewport(int X, int Y, int W, int H, bool ByResize) = 0;
	virtual void AddWindowResizeListener(WINDOW_RESIZE_FUNC pFunc, void *pUser) = 0;

	virtual void WindowDestroyNtf(uint32_t WindowID) = 0;
	virtual void WindowCreateNtf(uint32_t WindowID) = 0;

	// ForceClearNow forces the backend to trigger a clear, even at performance cost, else it might be delayed by one frame
	virtual void Clear(float r, float g, float b, bool ForceClearNow = false) = 0;

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

	virtual uint64_t TextureMemoryUsage() const = 0;
	virtual uint64_t BufferMemoryUsage() const = 0;
	virtual uint64_t StreamedMemoryUsage() const = 0;
	virtual uint64_t StagingMemoryUsage() const = 0;

	virtual const TTWGraphicsGPUList &GetGPUs() const = 0;

	virtual int LoadPNG(CImageInfo *pImg, const char *pFilename, int StorageType) = 0;
	virtual void FreePNG(CImageInfo *pImg) = 0;

	virtual bool CheckImageDivisibility(const char *pFileName, CImageInfo &Img, int DivX, int DivY, bool AllowResize) = 0;
	virtual bool IsImageFormatRGBA(const char *pFileName, CImageInfo &Img) = 0;

	// destination and source buffer require to have the same width and height
	virtual void CopyTextureBufferSub(uint8_t *pDestBuffer, uint8_t *pSourceBuffer, int FullWidth, int FullHeight, int ColorChannelCount, int SubOffsetX, int SubOffsetY, int SubCopyWidth, int SubCopyHeight) = 0;

	// destination width must be equal to the subwidth of the source
	virtual void CopyTextureFromTextureBufferSub(uint8_t *pDestBuffer, int DestWidth, int DestHeight, uint8_t *pSourceBuffer, int SrcWidth, int SrcHeight, int ColorChannelCount, int SrcSubOffsetX, int SrcSubOffsetY, int SrcSubCopyWidth, int SrcSubCopyHeight) = 0;

	virtual int UnloadTexture(CTextureHandle *pIndex) = 0;
	virtual CTextureHandle LoadTextureRaw(int Width, int Height, int Format, const void *pData, int StoreFormat, int Flags, const char *pTexName = nullptr) = 0;
	virtual int LoadTextureRawSub(CTextureHandle TextureID, int x, int y, int Width, int Height, int Format, const void *pData) = 0;
	virtual CTextureHandle LoadTexture(const char *pFilename, int StorageType, int StoreFormat, int Flags) = 0;
	virtual void TextureSet(CTextureHandle Texture) = 0;
	void TextureClear() { TextureSet(CTextureHandle()); }

	// pTextData & pTextOutlineData are automatically free'd
	virtual bool LoadTextTextures(int Width, int Height, CTextureHandle &TextTexture, CTextureHandle &TextOutlineTexture, void *pTextData, void *pTextOutlineData) = 0;
	virtual bool UnloadTextTextures(CTextureHandle &TextTexture, CTextureHandle &TextOutlineTexture) = 0;
	virtual bool UpdateTextTexture(CTextureHandle TextureID, int x, int y, int Width, int Height, const void *pData) = 0;

	virtual CTextureHandle LoadSpriteTexture(CImageInfo &FromImageInfo, struct CDataSprite *pSprite) = 0;
	virtual CTextureHandle LoadSpriteTexture(CImageInfo &FromImageInfo, struct client_data7::CDataSprite *pSprite) = 0;

	virtual bool IsImageSubFullyTransparent(CImageInfo &FromImageInfo, int x, int y, int w, int h) = 0;
	virtual bool IsSpriteTextureFullyTransparent(CImageInfo &FromImageInfo, struct client_data7::CDataSprite *pSprite) = 0;

	virtual void FlushVertices(bool KeepVertices = false) = 0;
	virtual void FlushVerticesTex3D() = 0;

	// specific render functions
	virtual void RenderTileLayer(int BufferContainerIndex, const ColorRGBA &Color, char **pOffsets, unsigned int *pIndicedVertexDrawNum, size_t NumIndicesOffset) = 0;
	virtual void RenderBorderTiles(int BufferContainerIndex, const ColorRGBA &Color, char *pIndexBufferOffset, const vec2 &Offset, const vec2 &Dir, int JumpIndex, unsigned int DrawNum) = 0;
	virtual void RenderBorderTileLines(int BufferContainerIndex, const ColorRGBA &Color, char *pIndexBufferOffset, const vec2 &Offset, const vec2 &Dir, unsigned int IndexDrawNum, unsigned int RedrawNum) = 0;
	virtual void RenderQuadLayer(int BufferContainerIndex, SQuadRenderInfo *pQuadInfo, int QuadNum, int QuadOffset) = 0;
	virtual void RenderText(int BufferContainerIndex, int TextQuadNum, int TextureSize, int TextureTextIndex, int TextureTextOutlineIndex, const ColorRGBA &TextColor, const ColorRGBA &TextOutlineColor) = 0;

	// opengl 3.3 functions

	enum EBufferObjectCreateFlags
	{
		// tell the backend that the buffer only needs to be valid for the span of one frame. Buffer size is not allowed to be bigger than GL_SVertex * MAX_VERTICES
		BUFFER_OBJECT_CREATE_FLAGS_ONE_TIME_USE_BIT = 1 << 0,
	};

	// if a pointer is passed as moved pointer, it requires to be allocated with malloc()
	virtual int CreateBufferObject(size_t UploadDataSize, void *pUploadData, int CreateFlags, bool IsMovedPointer = false) = 0;
	virtual void RecreateBufferObject(int BufferIndex, size_t UploadDataSize, void *pUploadData, int CreateFlags, bool IsMovedPointer = false) = 0;
	virtual void DeleteBufferObject(int BufferIndex) = 0;

	virtual int CreateBufferContainer(struct SBufferContainerInfo *pContainerInfo) = 0;
	// destroying all buffer objects means, that all referenced VBOs are destroyed automatically, so the user does not need to save references to them
	virtual void DeleteBufferContainer(int ContainerIndex, bool DestroyAllBO = true) = 0;
	virtual void IndicesNumRequiredNotify(unsigned int RequiredIndicesCount) = 0;

	// returns true if the driver age type is supported, passing BACKEND_TYPE_AUTO for BackendType will query the values for the currently used backend
	virtual bool GetDriverVersion(EGraphicsDriverAgeType DriverAgeType, int &Major, int &Minor, int &Patch, const char *&pName, EBackendType BackendType) = 0;
	virtual bool IsConfigModernAPI() = 0;
	virtual bool IsTileBufferingEnabled() = 0;
	virtual bool IsQuadBufferingEnabled() = 0;
	virtual bool IsTextBufferingEnabled() = 0;
	virtual bool IsQuadContainerBufferingEnabled() = 0;
	virtual bool HasTextureArrays() = 0;

	virtual const char *GetVendorString() = 0;
	virtual const char *GetVersionString() = 0;
	virtual const char *GetRendererString() = 0;

	struct CLineItem
	{
		float m_X0, m_Y0, m_X1, m_Y1;
		CLineItem() {}
		CLineItem(float x0, float y0, float x1, float y1) :
			m_X0(x0), m_Y0(y0), m_X1(x1), m_Y1(y1) {}
	};
	virtual void LinesBegin() = 0;
	virtual void LinesEnd() = 0;
	virtual void LinesDraw(const CLineItem *pArray, int Num) = 0;

	virtual void QuadsBegin() = 0;
	virtual void QuadsEnd() = 0;
	virtual void QuadsTex3DBegin() = 0;
	virtual void QuadsTex3DEnd() = 0;
	virtual void TrianglesBegin() = 0;
	virtual void TrianglesEnd() = 0;
	virtual void QuadsEndKeepVertices() = 0;
	virtual void QuadsDrawCurrentVertices(bool KeepVertices = true) = 0;
	virtual void QuadsSetRotation(float Angle) = 0;
	virtual void QuadsSetSubset(float TopLeftU, float TopLeftV, float BottomRightU, float BottomRightV) = 0;
	virtual void QuadsSetSubsetFree(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, int Index = -1) = 0;

	struct CFreeformItem
	{
		float m_X0, m_Y0, m_X1, m_Y1, m_X2, m_Y2, m_X3, m_Y3;
		CFreeformItem() {}
		CFreeformItem(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3) :
			m_X0(x0), m_Y0(y0), m_X1(x1), m_Y1(y1), m_X2(x2), m_Y2(y2), m_X3(x3), m_Y3(y3) {}
	};

	struct CQuadItem
	{
		float m_X, m_Y, m_Width, m_Height;
		CQuadItem() {}
		CQuadItem(float x, float y, float w, float h) :
			m_X(x), m_Y(y), m_Width(w), m_Height(h) {}
		void Set(float x, float y, float w, float h)
		{
			m_X = x;
			m_Y = y;
			m_Width = w;
			m_Height = h;
		}

		CFreeformItem ToFreeForm() const
		{
			return CFreeformItem(m_X, m_Y, m_X + m_Width, m_Y, m_X, m_Y + m_Height, m_X + m_Width, m_Y + m_Height);
		}
	};
	virtual void QuadsDraw(CQuadItem *pArray, int Num) = 0;
	virtual void QuadsDrawTL(const CQuadItem *pArray, int Num) = 0;

	virtual void QuadsTex3DDrawTL(const CQuadItem *pArray, int Num) = 0;

	virtual const GL_STexCoord *GetCurTextureCoordinates() = 0;
	virtual const GL_SColor *GetCurColor() = 0;

	virtual int CreateQuadContainer(bool AutomaticUpload = true) = 0;
	virtual void QuadContainerChangeAutomaticUpload(int ContainerIndex, bool AutomaticUpload) = 0;
	virtual void QuadContainerUpload(int ContainerIndex) = 0;
	virtual int QuadContainerAddQuads(int ContainerIndex, CQuadItem *pArray, int Num) = 0;
	virtual int QuadContainerAddQuads(int ContainerIndex, CFreeformItem *pArray, int Num) = 0;
	virtual void QuadContainerReset(int ContainerIndex) = 0;
	virtual void DeleteQuadContainer(int ContainerIndex) = 0;
	virtual void RenderQuadContainer(int ContainerIndex, int QuadDrawNum) = 0;
	virtual void RenderQuadContainer(int ContainerIndex, int QuadOffset, int QuadDrawNum, bool ChangeWrapMode = true) = 0;
	virtual void RenderQuadContainerEx(int ContainerIndex, int QuadOffset, int QuadDrawNum, float X, float Y, float ScaleX = 1.f, float ScaleY = 1.f) = 0;
	virtual void RenderQuadContainerAsSprite(int ContainerIndex, int QuadOffset, float X, float Y, float ScaleX = 1.f, float ScaleY = 1.f) = 0;

	struct SRenderSpriteInfo
	{
		vec2 m_Pos;
		float m_Scale;
		float m_Rotation;
	};

	virtual void RenderQuadContainerAsSpriteMultiple(int ContainerIndex, int QuadOffset, int DrawCount, SRenderSpriteInfo *pRenderInfo) = 0;

	virtual void QuadsDrawFreeform(const CFreeformItem *pArray, int Num) = 0;
	virtual void QuadsText(float x, float y, float Size, const char *pText) = 0;

	enum
	{
		CORNER_NONE = 0,
		CORNER_TL = 1,
		CORNER_TR = 2,
		CORNER_BL = 4,
		CORNER_BR = 8,
		CORNER_ITL = 16,
		CORNER_ITR = 32,
		CORNER_IBL = 64,
		CORNER_IBR = 128,

		CORNER_T = CORNER_TL | CORNER_TR,
		CORNER_B = CORNER_BL | CORNER_BR,
		CORNER_R = CORNER_TR | CORNER_BR,
		CORNER_L = CORNER_TL | CORNER_BL,

		CORNER_IT = CORNER_ITL | CORNER_ITR,
		CORNER_IB = CORNER_IBL | CORNER_IBR,
		CORNER_IR = CORNER_ITR | CORNER_IBR,
		CORNER_IL = CORNER_ITL | CORNER_IBL,

		CORNER_ALL = CORNER_T | CORNER_B,
		CORNER_INV_ALL = CORNER_IT | CORNER_IB
	};
	virtual void DrawRectExt(float x, float y, float w, float h, float r, int Corners) = 0;
	virtual void DrawRectExt4(float x, float y, float w, float h, ColorRGBA ColorTopLeft, ColorRGBA ColorTopRight, ColorRGBA ColorBottomLeft, ColorRGBA ColorBottomRight, float r, int Corners) = 0;
	virtual int CreateRectQuadContainer(float x, float y, float w, float h, float r, int Corners) = 0;
	virtual void DrawRect(float x, float y, float w, float h, ColorRGBA Color, int Corners, float Rounding) = 0;
	virtual void DrawRect4(float x, float y, float w, float h, ColorRGBA ColorTopLeft, ColorRGBA ColorTopRight, ColorRGBA ColorBottomLeft, ColorRGBA ColorBottomRight, int Corners, float Rounding) = 0;
	virtual void DrawCircle(float CenterX, float CenterY, float Radius, int Segments) = 0;

	struct CColorVertex
	{
		int m_Index;
		float m_R, m_G, m_B, m_A;
		CColorVertex() {}
		CColorVertex(int i, float r, float g, float b, float a) :
			m_Index(i), m_R(r), m_G(g), m_B(b), m_A(a) {}
	};
	virtual void SetColorVertex(const CColorVertex *pArray, int Num) = 0;
	virtual void SetColor(float r, float g, float b, float a) = 0;
	virtual void SetColor(ColorRGBA Color) = 0;
	virtual void SetColor4(ColorRGBA TopLeft, ColorRGBA TopRight, ColorRGBA BottomLeft, ColorRGBA BottomRight) = 0;
	virtual void ChangeColorOfCurrentQuadVertices(float r, float g, float b, float a) = 0;
	virtual void ChangeColorOfQuadVertices(int QuadOffset, unsigned char r, unsigned char g, unsigned char b, unsigned char a) = 0;

	virtual void TakeScreenshot(const char *pFilename) = 0;
	virtual void TakeCustomScreenshot(const char *pFilename) = 0;
	virtual int GetVideoModes(CVideoMode *pModes, int MaxModes, int Screen) = 0;

	virtual void Swap() = 0;
	virtual int GetNumScreens() const = 0;

	// synchronization
	virtual void InsertSignal(class CSemaphore *pSemaphore) = 0;
	virtual bool IsIdle() const = 0;
	virtual void WaitForIdle() = 0;

	virtual void SetWindowGrab(bool Grab) = 0;
	virtual void NotifyWindow() = 0;

	// be aware that this function should only be called from the graphics thread, and even then you should really know what you are doing
	// this function always returns the pixels in RGB
	virtual TGLBackendReadPresentedImageData &GetReadPresentedImageDataFuncUnsafe() = 0;

	virtual SWarning *GetCurWarning() = 0;

protected:
	inline CTextureHandle CreateTextureHandle(int Index)
	{
		CTextureHandle Tex;
		Tex.m_Id = Index;
		return Tex;
	}
};

class IEngineGraphics : public IGraphics
{
	MACRO_INTERFACE("enginegraphics", 0)
public:
	virtual int Init() = 0;
	virtual void Shutdown() override = 0;

	virtual void Minimize() = 0;
	virtual void Maximize() = 0;

	virtual int WindowActive() = 0;
	virtual int WindowOpen() = 0;
};

extern IEngineGraphics *CreateEngineGraphics();
extern IEngineGraphics *CreateEngineGraphicsThreaded();

#endif
