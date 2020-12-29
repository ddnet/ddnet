#ifndef ENGINE_CLIENT_GRAPHICS_THREADED_H
#define ENGINE_CLIENT_GRAPHICS_THREADED_H

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <cstddef>
#include <vector>

#define CMD_BUFFER_DATA_BUFFER_SIZE 1024 * 1024 * 2
#define CMD_BUFFER_CMD_BUFFER_SIZE 1024 * 256

class CCommandBuffer
{
	class CBuffer
	{
		unsigned char *m_pData;
		unsigned m_Size;
		unsigned m_Used;

	public:
		CBuffer(unsigned BufferSize)
		{
			m_Size = BufferSize;
			m_pData = new unsigned char[m_Size];
			m_Used = 0;
		}

		~CBuffer()
		{
			delete[] m_pData;
			m_pData = 0x0;
			m_Used = 0;
			m_Size = 0;
		}

		void Reset()
		{
			m_Used = 0;
		}

		void *Alloc(unsigned Requested, unsigned Alignment = alignof(std::max_align_t))
		{
			size_t Offset = reinterpret_cast<uintptr_t>(m_pData + m_Used) % Alignment;
			if(Offset)
				Offset = Alignment - Offset;

			if(Requested + Offset + m_Used > m_Size)
				return 0;

			void *pPtr = &m_pData[m_Used + Offset];
			m_Used += Requested + Offset;
			return pPtr;
		}

		unsigned char *DataPtr() { return m_pData; }
		unsigned DataSize() { return m_Size; }
		unsigned DataUsed() { return m_Used; }
	};

public:
	CBuffer m_CmdBuffer;
	CBuffer m_DataBuffer;

	enum
	{
		MAX_TEXTURES = 1024 * 8,
		MAX_VERTICES = 32 * 1024,
	};

	enum
	{
		// commadn groups
		CMDGROUP_CORE = 0, // commands that everyone has to implement
		CMDGROUP_PLATFORM_OPENGL = 10000, // commands specific to a platform
		CMDGROUP_PLATFORM_SDL = 20000,

		//
		CMD_NOP = CMDGROUP_CORE,

		//
		CMD_RUNBUFFER,

		// synchronization
		CMD_SIGNAL,

		// texture commands
		CMD_TEXTURE_CREATE,
		CMD_TEXTURE_DESTROY,
		CMD_TEXTURE_UPDATE,

		// rendering
		CMD_CLEAR,
		CMD_RENDER,
		CMD_RENDER_TEX3D,

		//opengl 2.0+ commands (some are just emulated and only exist in opengl 3.3+)
		CMD_CREATE_BUFFER_OBJECT, // create vbo
		CMD_RECREATE_BUFFER_OBJECT, // recreate vbo
		CMD_UPDATE_BUFFER_OBJECT, // update vbo
		CMD_COPY_BUFFER_OBJECT, // copy vbo to another
		CMD_DELETE_BUFFER_OBJECT, // delete vbo

		CMD_CREATE_BUFFER_CONTAINER, // create vao
		CMD_DELETE_BUFFER_CONTAINER, // delete vao
		CMD_UPDATE_BUFFER_CONTAINER, // update vao

		CMD_INDICES_REQUIRED_NUM_NOTIFY, // create indices that are required

		CMD_RENDER_TILE_LAYER, // render a tilelayer
		CMD_RENDER_BORDER_TILE, // render one tile multiple times
		CMD_RENDER_BORDER_TILE_LINE, // render an amount of tiles multiple times
		CMD_RENDER_QUAD_LAYER, // render a quad layer
		CMD_RENDER_TEXT, // render text
		CMD_RENDER_TEXT_STREAM, // render text stream
		CMD_RENDER_QUAD_CONTAINER, // render a quad buffer container
		CMD_RENDER_QUAD_CONTAINER_EX, // render a quad buffer container with extended parameters
		CMD_RENDER_QUAD_CONTAINER_SPRITE_MULTIPLE, // render a quad buffer container as sprite multiple times

		// swap
		CMD_SWAP,

		// misc
		CMD_VSYNC,
		CMD_SCREENSHOT,
		CMD_VIDEOMODES,
		CMD_RESIZE,

	};

	enum
	{
		TEXFORMAT_INVALID = 0,
		TEXFORMAT_RGB,
		TEXFORMAT_RGBA,
		TEXFORMAT_ALPHA,

		TEXFLAG_NOMIPMAPS = 1,
		TEXFLAG_COMPRESSED = 2,
		TEXFLAG_QUALITY = 4,
		TEXFLAG_TO_3D_TEXTURE = (1 << 3),
		TEXFLAG_TO_2D_ARRAY_TEXTURE = (1 << 4),
		TEXFLAG_TO_3D_TEXTURE_SINGLE_LAYER = (1 << 5),
		TEXFLAG_TO_2D_ARRAY_TEXTURE_SINGLE_LAYER = (1 << 6),
		TEXFLAG_NO_2D_TEXTURE = (1 << 7),
	};

	enum
	{
		//
		PRIMTYPE_INVALID = 0,
		PRIMTYPE_LINES,
		PRIMTYPE_QUADS,
		PRIMTYPE_TRIANGLES,
	};

	enum
	{
		BLEND_NONE = 0,
		BLEND_ALPHA,
		BLEND_ADDITIVE,
	};

	enum
	{
		WRAP_REPEAT = 0,
		WRAP_CLAMP,
	};

	typedef GL_SPoint SPoint;
	typedef GL_STexCoord STexCoord;
	typedef GL_SColorf SColorf;
	typedef GL_SColor SColor;
	typedef GL_SVertex SVertex;
	typedef GL_SVertexTex3D SVertexTex3D;
	typedef GL_SVertexTex3DStream SVertexTex3DStream;

	struct SCommand
	{
	public:
		SCommand(unsigned Cmd) :
			m_Cmd(Cmd), m_pNext(nullptr) {}
		unsigned m_Cmd;
		SCommand *m_pNext;
	};
	SCommand *m_pCmdBufferHead;
	SCommand *m_pCmdBufferTail;

	struct SState
	{
		int m_BlendMode;
		int m_WrapMode;
		int m_Texture;
		SPoint m_ScreenTL;
		SPoint m_ScreenBR;

		// clip
		bool m_ClipEnable;
		int m_ClipX;
		int m_ClipY;
		int m_ClipW;
		int m_ClipH;
	};

	struct SCommand_Clear : public SCommand
	{
		SCommand_Clear() :
			SCommand(CMD_CLEAR) {}
		SColorf m_Color;
	};

	struct SCommand_Signal : public SCommand
	{
		SCommand_Signal() :
			SCommand(CMD_SIGNAL) {}
		CSemaphore *m_pSemaphore;
	};

	struct SCommand_RunBuffer : public SCommand
	{
		SCommand_RunBuffer() :
			SCommand(CMD_RUNBUFFER) {}
		CCommandBuffer *m_pOtherBuffer;
	};

	struct SCommand_Render : public SCommand
	{
		SCommand_Render() :
			SCommand(CMD_RENDER) {}
		SState m_State;
		unsigned m_PrimType;
		unsigned m_PrimCount;
		SVertex *m_pVertices; // you should use the command buffer data to allocate vertices for this command
	};

	struct SCommand_RenderTex3D : public SCommand
	{
		SCommand_RenderTex3D() :
			SCommand(CMD_RENDER_TEX3D) {}
		SState m_State;
		unsigned m_PrimType;
		unsigned m_PrimCount;
		SVertexTex3DStream *m_pVertices; // you should use the command buffer data to allocate vertices for this command
	};

	struct SCommand_CreateBufferObject : public SCommand
	{
		SCommand_CreateBufferObject() :
			SCommand(CMD_CREATE_BUFFER_OBJECT) {}

		int m_BufferIndex;

		bool m_DeletePointer;
		void *m_pUploadData;
		size_t m_DataSize;
	};

	struct SCommand_RecreateBufferObject : public SCommand
	{
		SCommand_RecreateBufferObject() :
			SCommand(CMD_RECREATE_BUFFER_OBJECT) {}

		int m_BufferIndex;

		bool m_DeletePointer;
		void *m_pUploadData;
		size_t m_DataSize;
	};

	struct SCommand_UpdateBufferObject : public SCommand
	{
		SCommand_UpdateBufferObject() :
			SCommand(CMD_UPDATE_BUFFER_OBJECT) {}

		int m_BufferIndex;

		bool m_DeletePointer;
		void *m_pOffset;
		void *m_pUploadData;
		size_t m_DataSize;
	};

	struct SCommand_CopyBufferObject : public SCommand
	{
		SCommand_CopyBufferObject() :
			SCommand(CMD_COPY_BUFFER_OBJECT) {}

		int m_WriteBufferIndex;
		int m_ReadBufferIndex;

		size_t m_pReadOffset;
		size_t m_pWriteOffset;
		size_t m_CopySize;
	};

	struct SCommand_DeleteBufferObject : public SCommand
	{
		SCommand_DeleteBufferObject() :
			SCommand(CMD_DELETE_BUFFER_OBJECT) {}

		int m_BufferIndex;
	};

	struct SCommand_CreateBufferContainer : public SCommand
	{
		SCommand_CreateBufferContainer() :
			SCommand(CMD_CREATE_BUFFER_CONTAINER) {}

		int m_BufferContainerIndex;

		int m_Stride;

		int m_AttrCount;
		SBufferContainerInfo::SAttribute *m_Attributes;
	};

	struct SCommand_UpdateBufferContainer : public SCommand
	{
		SCommand_UpdateBufferContainer() :
			SCommand(CMD_UPDATE_BUFFER_CONTAINER) {}

		int m_BufferContainerIndex;

		int m_Stride;

		int m_AttrCount;
		SBufferContainerInfo::SAttribute *m_Attributes;
	};

	struct SCommand_DeleteBufferContainer : public SCommand
	{
		SCommand_DeleteBufferContainer() :
			SCommand(CMD_DELETE_BUFFER_CONTAINER) {}

		int m_BufferContainerIndex;
		bool m_DestroyAllBO;
	};

	struct SCommand_IndicesRequiredNumNotify : public SCommand
	{
		SCommand_IndicesRequiredNumNotify() :
			SCommand(CMD_INDICES_REQUIRED_NUM_NOTIFY) {}

		unsigned int m_RequiredIndicesNum;
	};

	struct SCommand_RenderTileLayer : public SCommand
	{
		SCommand_RenderTileLayer() :
			SCommand(CMD_RENDER_TILE_LAYER) {}
		SState m_State;
		SColorf m_Color; //the color of the whole tilelayer -- already envelopped

		//the char offset of all indices that should be rendered, and the amount of renders
		char **m_pIndicesOffsets;
		unsigned int *m_pDrawCount;

		int m_IndicesDrawNum;
		int m_BufferContainerIndex;
	};

	struct SCommand_RenderBorderTile : public SCommand
	{
		SCommand_RenderBorderTile() :
			SCommand(CMD_RENDER_BORDER_TILE) {}
		SState m_State;
		SColorf m_Color; //the color of the whole tilelayer -- already envelopped
		char *m_pIndicesOffset; // you should use the command buffer data to allocate vertices for this command
		unsigned int m_DrawNum;
		int m_BufferContainerIndex;

		float m_Offset[2];
		float m_Dir[2];
		int m_JumpIndex;
	};

	struct SCommand_RenderBorderTileLine : public SCommand
	{
		SCommand_RenderBorderTileLine() :
			SCommand(CMD_RENDER_BORDER_TILE_LINE) {}
		SState m_State;
		SColorf m_Color; //the color of the whole tilelayer -- already envelopped
		char *m_pIndicesOffset; // you should use the command buffer data to allocate vertices for this command
		unsigned int m_IndexDrawNum;
		unsigned int m_DrawNum;
		int m_BufferContainerIndex;

		float m_Offset[2];
		float m_Dir[2];
	};

	struct SCommand_RenderQuadLayer : public SCommand
	{
		SCommand_RenderQuadLayer() :
			SCommand(CMD_RENDER_QUAD_LAYER) {}
		SState m_State;

		int m_BufferContainerIndex;
		SQuadRenderInfo *m_pQuadInfo;
		int m_QuadNum;
		int m_QuadOffset;
	};

	struct SCommand_RenderText : public SCommand
	{
		SCommand_RenderText() :
			SCommand(CMD_RENDER_TEXT) {}
		SState m_State;

		int m_BufferContainerIndex;
		int m_TextureSize;

		int m_TextTextureIndex;
		int m_TextOutlineTextureIndex;

		int m_DrawNum;
		float m_aTextColor[4];
		float m_aTextOutlineColor[4];
	};

	struct SCommand_RenderTextStream : public SCommand
	{
		SCommand_RenderTextStream() :
			SCommand(CMD_RENDER_TEXT_STREAM) {}
		SState m_State;

		SVertex *m_pVertices;
		unsigned m_PrimType;
		unsigned m_PrimCount;

		int m_TextureSize;

		int m_TextTextureIndex;
		int m_TextOutlineTextureIndex;

		float m_aTextOutlineColor[4];
	};

	struct SCommand_RenderQuadContainer : public SCommand
	{
		SCommand_RenderQuadContainer() :
			SCommand(CMD_RENDER_QUAD_CONTAINER) {}
		SState m_State;

		int m_BufferContainerIndex;

		unsigned int m_DrawNum;
		void *m_pOffset;
	};

	struct SCommand_RenderQuadContainerEx : public SCommand
	{
		SCommand_RenderQuadContainerEx() :
			SCommand(CMD_RENDER_QUAD_CONTAINER_EX) {}
		SState m_State;

		int m_BufferContainerIndex;

		float m_Rotation;
		SPoint m_Center;

		SColorf m_VertexColor;

		unsigned int m_DrawNum;
		void *m_pOffset;
	};

	struct SCommand_RenderQuadContainerAsSpriteMultiple : public SCommand
	{
		SCommand_RenderQuadContainerAsSpriteMultiple() :
			SCommand(CMD_RENDER_QUAD_CONTAINER_SPRITE_MULTIPLE) {}
		SState m_State;

		int m_BufferContainerIndex;

		IGraphics::SRenderSpriteInfo *m_pRenderInfo;

		SPoint m_Center;
		SColorf m_VertexColor;

		unsigned int m_DrawNum;
		unsigned int m_DrawCount;
		void *m_pOffset;
	};

	struct SCommand_Screenshot : public SCommand
	{
		SCommand_Screenshot() :
			SCommand(CMD_SCREENSHOT) {}
		CImageInfo *m_pImage; // processor will fill this out, the one who adds this command must free the data as well
	};

	struct SCommand_VideoModes : public SCommand
	{
		SCommand_VideoModes() :
			SCommand(CMD_VIDEOMODES) {}

		CVideoMode *m_pModes; // processor will fill this in
		int m_MaxModes; // maximum of modes the processor can write to the m_pModes
		int *m_pNumModes; // processor will write to this pointer
		int m_Screen;
	};

	struct SCommand_Swap : public SCommand
	{
		SCommand_Swap() :
			SCommand(CMD_SWAP) {}

		int m_Finish;
	};

	struct SCommand_VSync : public SCommand
	{
		SCommand_VSync() :
			SCommand(CMD_VSYNC) {}

		int m_VSync;
		bool *m_pRetOk;
	};

	struct SCommand_Resize : public SCommand
	{
		SCommand_Resize() :
			SCommand(CMD_RESIZE) {}

		int m_Width;
		int m_Height;
	};

	struct SCommand_Texture_Create : public SCommand
	{
		SCommand_Texture_Create() :
			SCommand(CMD_TEXTURE_CREATE) {}

		// texture information
		int m_Slot;

		int m_Width;
		int m_Height;
		int m_PixelSize;
		int m_Format;
		int m_StoreFormat;
		int m_Flags;
		void *m_pData; // will be freed by the command processor
	};

	struct SCommand_Texture_Update : public SCommand
	{
		SCommand_Texture_Update() :
			SCommand(CMD_TEXTURE_UPDATE) {}

		// texture information
		int m_Slot;

		int m_X;
		int m_Y;
		int m_Width;
		int m_Height;
		int m_Format;
		void *m_pData; // will be freed by the command processor
	};

	struct SCommand_Texture_Destroy : public SCommand
	{
		SCommand_Texture_Destroy() :
			SCommand(CMD_TEXTURE_DESTROY) {}

		// texture information
		int m_Slot;
	};

	//
	CCommandBuffer(unsigned CmdBufferSize, unsigned DataBufferSize) :
		m_CmdBuffer(CmdBufferSize), m_DataBuffer(DataBufferSize), m_pCmdBufferHead(nullptr), m_pCmdBufferTail(nullptr)
	{
	}

	void *AllocData(unsigned WantedSize)
	{
		return m_DataBuffer.Alloc(WantedSize);
	}

	template<class T>
	bool AddCommand(const T &Command)
	{
		// make sure that we don't do something stupid like ->AddCommand(&Cmd);
		(void)static_cast<const SCommand *>(&Command);

		// allocate and copy the command into the buffer
		T *pCmd = (T *)m_CmdBuffer.Alloc(sizeof(*pCmd), alignof(T));
		if(!pCmd)
			return false;
		*pCmd = Command;
		pCmd->m_pNext = nullptr;

		if(m_pCmdBufferTail)
			m_pCmdBufferTail->m_pNext = pCmd;
		if(!m_pCmdBufferHead)
			m_pCmdBufferHead = pCmd;
		m_pCmdBufferTail = pCmd;

		return true;
	}

	SCommand *Head()
	{
		return m_pCmdBufferHead;
	}

	void Reset()
	{
		m_pCmdBufferHead = m_pCmdBufferTail = nullptr;
		m_CmdBuffer.Reset();
		m_DataBuffer.Reset();
	}
};

enum EGraphicsBackendErrorCodes
{
	GRAPHICS_BACKEND_ERROR_CODE_UNKNOWN = -1,
	GRAPHICS_BACKEND_ERROR_CODE_NONE = 0,
	GRAPHICS_BACKEND_ERROR_CODE_OPENGL_CONTEXT_FAILED,
	GRAPHICS_BACKEND_ERROR_CODE_OPENGL_VERSION_FAILED,
	GRAPHICS_BACKEND_ERROR_CODE_SDL_INIT_FAILED,
	GRAPHICS_BACKEND_ERROR_CODE_SDL_SCREEN_REQUEST_FAILED,
	GRAPHICS_BACKEND_ERROR_CODE_SDL_SCREEN_INFO_REQUEST_FAILED,
	GRAPHICS_BACKEND_ERROR_CODE_SDL_SCREEN_RESOLUTION_REQUEST_FAILED,
	GRAPHICS_BACKEND_ERROR_CODE_SDL_WINDOW_CREATE_FAILED,
};

// interface for the graphics backend
// all these functions are called on the main thread
class IGraphicsBackend
{
public:
	enum
	{
		INITFLAG_FULLSCREEN = 1 << 0,
		INITFLAG_VSYNC = 1 << 1,
		INITFLAG_RESIZABLE = 1 << 2,
		INITFLAG_BORDERLESS = 1 << 3,
		INITFLAG_HIGHDPI = 1 << 4,
	};

	virtual ~IGraphicsBackend() {}

	virtual int Init(const char *pName, int *Screen, int *pWidth, int *pHeight, int FsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight, int *pCurrentWidth, int *pCurrentHeight, class IStorage *pStorage) = 0;
	virtual int Shutdown() = 0;

	virtual int MemoryUsage() const = 0;

	virtual int GetNumScreens() const = 0;

	virtual void Minimize() = 0;
	virtual void Maximize() = 0;
	virtual bool Fullscreen(bool State) = 0;
	virtual void SetWindowBordered(bool State) = 0;
	virtual bool SetWindowScreen(int Index) = 0;
	virtual int GetWindowScreen() = 0;
	virtual int WindowActive() = 0;
	virtual int WindowOpen() = 0;
	virtual void SetWindowGrab(bool Grab) = 0;
	virtual void ResizeWindow(int w, int h) = 0;
	virtual void GetViewportSize(int &w, int &h) = 0;
	virtual void NotifyWindow() = 0;

	virtual void RunBuffer(CCommandBuffer *pBuffer) = 0;
	virtual bool IsIdle() const = 0;
	virtual void WaitForIdle() = 0;

	virtual bool IsNewOpenGL() { return false; }
	virtual bool HasTileBuffering() { return false; }
	virtual bool HasQuadBuffering() { return false; }
	virtual bool HasTextBuffering() { return false; }
	virtual bool HasQuadContainerBuffering() { return false; }
	virtual bool Has2DTextureArrays() { return false; }
	virtual const char *GetErrorString() { return NULL; }
};

class CGraphics_Threaded : public IEngineGraphics
{
	enum
	{
		NUM_CMDBUFFERS = 2,

		DRAWING_QUADS = 1,
		DRAWING_LINES = 2,
		DRAWING_TRIANGLES = 3
	};

	CCommandBuffer::SState m_State;
	IGraphicsBackend *m_pBackend;
	bool m_OpenGLTileBufferingEnabled;
	bool m_OpenGLQuadBufferingEnabled;
	bool m_OpenGLTextBufferingEnabled;
	bool m_OpenGLQuadContainerBufferingEnabled;
	bool m_OpenGLHasTextureArrays;
	bool m_IsNewOpenGL;

	CCommandBuffer *m_apCommandBuffers[NUM_CMDBUFFERS];
	CCommandBuffer *m_pCommandBuffer;
	unsigned m_CurrentCommandBuffer;

	//
	class IStorage *m_pStorage;
	class IConsole *m_pConsole;

	int m_CurIndex;

	CCommandBuffer::SVertex m_aVertices[CCommandBuffer::MAX_VERTICES];
	CCommandBuffer::SVertexTex3DStream m_aVerticesTex3D[CCommandBuffer::MAX_VERTICES];
	int m_NumVertices;

	CCommandBuffer::SColor m_aColor[4];
	CCommandBuffer::STexCoord m_aTexture[4];

	bool m_RenderEnable;

	float m_Rotation;
	int m_Drawing;
	bool m_DoScreenshot;
	char m_aScreenshotName[128];

	CTextureHandle m_InvalidTexture;

	std::vector<int> m_TextureIndices;
	int m_FirstFreeTexture;
	int m_TextureMemoryUsage;

	std::vector<uint8_t> m_SpriteHelper;

	std::vector<SWarning> m_Warnings;

	struct SVertexArrayInfo
	{
		SVertexArrayInfo() :
			m_FreeIndex(-1) {}
		// keep a reference to them, so we can free their IDs
		std::vector<int> m_AssociatedBufferObjectIndices;

		int m_FreeIndex;
	};
	std::vector<SVertexArrayInfo> m_VertexArrayInfo;
	int m_FirstFreeVertexArrayInfo;

	std::vector<int> m_BufferObjectIndices;
	int m_FirstFreeBufferObjectIndex;

	struct SQuadContainer
	{
		SQuadContainer()
		{
			m_Quads.clear();
			m_QuadBufferObjectIndex = m_QuadBufferContainerIndex = -1;
			m_FreeIndex = -1;
		}

		struct SQuad
		{
			CCommandBuffer::SVertex m_aVertices[4];
		};

		std::vector<SQuad> m_Quads;

		int m_QuadBufferObjectIndex;
		int m_QuadBufferContainerIndex;

		int m_FreeIndex;
	};
	std::vector<SQuadContainer> m_QuadContainers;
	int m_FirstFreeQuadContainer;

	struct SWindowResizeListener
	{
		SWindowResizeListener(WINDOW_RESIZE_FUNC pFunc, void *pUser) :
			m_pFunc(pFunc), m_pUser(pUser) {}
		WINDOW_RESIZE_FUNC m_pFunc;
		void *m_pUser;
	};
	std::vector<SWindowResizeListener> m_ResizeListeners;

	void *AllocCommandBufferData(unsigned AllocSize);

	void AddVertices(int Count);
	void AddVertices(int Count, CCommandBuffer::SVertex *pVertices);
	void AddVertices(int Count, CCommandBuffer::SVertexTex3DStream *pVertices);

	template<typename TName>
	void Rotate(const CCommandBuffer::SPoint &rCenter, TName *pPoints, int NumPoints)
	{
		float c = cosf(m_Rotation);
		float s = sinf(m_Rotation);
		float x, y;
		int i;

		TName *pVertices = pPoints;
		for(i = 0; i < NumPoints; i++)
		{
			x = pVertices[i].m_Pos.x - rCenter.x;
			y = pVertices[i].m_Pos.y - rCenter.y;
			pVertices[i].m_Pos.x = x * c - y * s + rCenter.x;
			pVertices[i].m_Pos.y = x * s + y * c + rCenter.y;
		}
	}

	void KickCommandBuffer();

	void AddBackEndWarningIfExists();

	int IssueInit();
	int InitWindow();

public:
	CGraphics_Threaded();

	void ClipEnable(int x, int y, int w, int h) override;
	void ClipDisable() override;

	void BlendNone() override;
	void BlendNormal() override;
	void BlendAdditive() override;

	void WrapNormal() override;
	void WrapClamp() override;

	int MemoryUsage() const override;

	void MapScreen(float TopLeftX, float TopLeftY, float BottomRightX, float BottomRightY) override;
	void GetScreen(float *pTopLeftX, float *pTopLeftY, float *pBottomRightX, float *pBottomRightY) override;

	void LinesBegin() override;
	void LinesEnd() override;
	void LinesDraw(const CLineItem *pArray, int Num) override;

	int UnloadTexture(IGraphics::CTextureHandle Index) override;
	int UnloadTextureNew(CTextureHandle &TextureHandle) override;
	IGraphics::CTextureHandle LoadTextureRaw(int Width, int Height, int Format, const void *pData, int StoreFormat, int Flags, const char *pTexName = NULL) override;
	int LoadTextureRawSub(IGraphics::CTextureHandle TextureID, int x, int y, int Width, int Height, int Format, const void *pData) override;

	CTextureHandle LoadSpriteTextureImpl(CImageInfo &FromImageInfo, int x, int y, int w, int h);
	CTextureHandle LoadSpriteTexture(CImageInfo &FromImageInfo, struct CDataSprite *pSprite) override;
	CTextureHandle LoadSpriteTexture(CImageInfo &FromImageInfo, struct client_data7::CDataSprite *pSprite) override;

	bool IsImageSubFullyTransparent(CImageInfo &FromImageInfo, int x, int y, int w, int h) override;
	bool IsSpriteTextureFullyTransparent(CImageInfo &FromImageInfo, struct client_data7::CDataSprite *pSprite) override;

	// simple uncompressed RGBA loaders
	IGraphics::CTextureHandle LoadTexture(const char *pFilename, int StorageType, int StoreFormat, int Flags) override;
	int LoadPNG(CImageInfo *pImg, const char *pFilename, int StorageType) override;
	void FreePNG(CImageInfo *pImg) override;

	bool CheckImageDivisibility(const char *pFileName, CImageInfo &Img, int DivX, int DivY, bool AllowResize) override;

	void CopyTextureBufferSub(uint8_t *pDestBuffer, uint8_t *pSourceBuffer, int FullWidth, int FullHeight, int ColorChannelCount, int SubOffsetX, int SubOffsetY, int SubCopyWidth, int SubCopyHeight) override;
	void CopyTextureFromTextureBufferSub(uint8_t *pDestBuffer, int DestWidth, int DestHeight, uint8_t *pSourceBuffer, int SrcWidth, int SrcHeight, int ColorChannelCount, int SrcSubOffsetX, int SrcSubOffsetY, int SrcSubCopyWidth, int SrcSubCopyHeight) override;

	void ScreenshotDirect();

	void TextureSet(CTextureHandle TextureID) override;

	void Clear(float r, float g, float b) override;

	void QuadsBegin() override;
	void QuadsEnd() override;
	void TextQuadsBegin() override;
	void TextQuadsEnd(int TextureSize, int TextTextureIndex, int TextOutlineTextureIndex, float *pOutlineTextColor) override;
	void QuadsTex3DBegin() override;
	void QuadsTex3DEnd() override;
	void TrianglesBegin() override;
	void TrianglesEnd() override;
	void QuadsEndKeepVertices() override;
	void QuadsDrawCurrentVertices(bool KeepVertices = true) override;
	void QuadsSetRotation(float Angle) override;

	template<typename TName>
	void SetColor(TName *pVertex, int ColorIndex)
	{
		TName *pVert = pVertex;
		pVert->m_Color.r = m_aColor[ColorIndex].r;
		pVert->m_Color.g = m_aColor[ColorIndex].g;
		pVert->m_Color.b = m_aColor[ColorIndex].b;
		pVert->m_Color.a = m_aColor[ColorIndex].a;
	}

	void SetColorVertex(const CColorVertex *pArray, int Num) override;
	void SetColor(float r, float g, float b, float a) override;
	void SetColor(ColorRGBA rgb) override;
	void SetColor4(vec4 TopLeft, vec4 TopRight, vec4 BottomLeft, vec4 BottomRight) override;

	// go through all vertices and change their color (only works for quads)
	void ChangeColorOfCurrentQuadVertices(float r, float g, float b, float a) override;
	void ChangeColorOfQuadVertices(int QuadOffset, unsigned char r, unsigned char g, unsigned char b, unsigned char a) override;

	void QuadsSetSubset(float TlU, float TlV, float BrU, float BrV) override;
	void QuadsSetSubsetFree(
		float x0, float y0, float x1, float y1,
		float x2, float y2, float x3, float y3, int Index = -1) override;

	void QuadsDraw(CQuadItem *pArray, int Num) override;

	template<typename TName>
	void QuadsDrawTLImpl(TName *pVertices, const CQuadItem *pArray, int Num)
	{
		CCommandBuffer::SPoint Center;

		dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsDrawTL without begin");

		if(g_Config.m_GfxQuadAsTriangle && !m_IsNewOpenGL)
		{
			for(int i = 0; i < Num; ++i)
			{
				// first triangle
				pVertices[m_NumVertices + 6 * i].m_Pos.x = pArray[i].m_X;
				pVertices[m_NumVertices + 6 * i].m_Pos.y = pArray[i].m_Y;
				pVertices[m_NumVertices + 6 * i].m_Tex = m_aTexture[0];
				SetColor(&pVertices[m_NumVertices + 6 * i], 0);

				pVertices[m_NumVertices + 6 * i + 1].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
				pVertices[m_NumVertices + 6 * i + 1].m_Pos.y = pArray[i].m_Y;
				pVertices[m_NumVertices + 6 * i + 1].m_Tex = m_aTexture[1];
				SetColor(&pVertices[m_NumVertices + 6 * i + 1], 1);

				pVertices[m_NumVertices + 6 * i + 2].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
				pVertices[m_NumVertices + 6 * i + 2].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
				pVertices[m_NumVertices + 6 * i + 2].m_Tex = m_aTexture[2];
				SetColor(&pVertices[m_NumVertices + 6 * i + 2], 2);

				// second triangle
				pVertices[m_NumVertices + 6 * i + 3].m_Pos.x = pArray[i].m_X;
				pVertices[m_NumVertices + 6 * i + 3].m_Pos.y = pArray[i].m_Y;
				pVertices[m_NumVertices + 6 * i + 3].m_Tex = m_aTexture[0];
				SetColor(&pVertices[m_NumVertices + 6 * i + 3], 0);

				pVertices[m_NumVertices + 6 * i + 4].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
				pVertices[m_NumVertices + 6 * i + 4].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
				pVertices[m_NumVertices + 6 * i + 4].m_Tex = m_aTexture[2];
				SetColor(&pVertices[m_NumVertices + 6 * i + 4], 2);

				pVertices[m_NumVertices + 6 * i + 5].m_Pos.x = pArray[i].m_X;
				pVertices[m_NumVertices + 6 * i + 5].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
				pVertices[m_NumVertices + 6 * i + 5].m_Tex = m_aTexture[3];
				SetColor(&pVertices[m_NumVertices + 6 * i + 5], 3);

				if(m_Rotation != 0)
				{
					Center.x = pArray[i].m_X + pArray[i].m_Width / 2;
					Center.y = pArray[i].m_Y + pArray[i].m_Height / 2;

					Rotate(Center, &pVertices[m_NumVertices + 6 * i], 6);
				}
			}

			AddVertices(3 * 2 * Num, pVertices);
		}
		else
		{
			for(int i = 0; i < Num; ++i)
			{
				pVertices[m_NumVertices + 4 * i].m_Pos.x = pArray[i].m_X;
				pVertices[m_NumVertices + 4 * i].m_Pos.y = pArray[i].m_Y;
				pVertices[m_NumVertices + 4 * i].m_Tex = m_aTexture[0];
				SetColor(&pVertices[m_NumVertices + 4 * i], 0);

				pVertices[m_NumVertices + 4 * i + 1].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
				pVertices[m_NumVertices + 4 * i + 1].m_Pos.y = pArray[i].m_Y;
				pVertices[m_NumVertices + 4 * i + 1].m_Tex = m_aTexture[1];
				SetColor(&pVertices[m_NumVertices + 4 * i + 1], 1);

				pVertices[m_NumVertices + 4 * i + 2].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
				pVertices[m_NumVertices + 4 * i + 2].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
				pVertices[m_NumVertices + 4 * i + 2].m_Tex = m_aTexture[2];
				SetColor(&pVertices[m_NumVertices + 4 * i + 2], 2);

				pVertices[m_NumVertices + 4 * i + 3].m_Pos.x = pArray[i].m_X;
				pVertices[m_NumVertices + 4 * i + 3].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
				pVertices[m_NumVertices + 4 * i + 3].m_Tex = m_aTexture[3];
				SetColor(&pVertices[m_NumVertices + 4 * i + 3], 3);

				if(m_Rotation != 0)
				{
					Center.x = pArray[i].m_X + pArray[i].m_Width / 2;
					Center.y = pArray[i].m_Y + pArray[i].m_Height / 2;

					Rotate(Center, &pVertices[m_NumVertices + 4 * i], 4);
				}
			}

			AddVertices(4 * Num, pVertices);
		}
	}

	void QuadsDrawTL(const CQuadItem *pArray, int Num) override;

	void QuadsTex3DDrawTL(const CQuadItem *pArray, int Num) override;

	void QuadsDrawFreeform(const CFreeformItem *pArray, int Num) override;
	void QuadsText(float x, float y, float Size, const char *pText) override;

	int CreateQuadContainer() override;
	void QuadContainerUpload(int ContainerIndex) override;
	void QuadContainerAddQuads(int ContainerIndex, CQuadItem *pArray, int Num) override;
	void QuadContainerAddQuads(int ContainerIndex, CFreeformItem *pArray, int Num) override;
	void QuadContainerReset(int ContainerIndex) override;
	void DeleteQuadContainer(int ContainerIndex) override;
	void RenderQuadContainer(int ContainerIndex, int QuadDrawNum) override;
	void RenderQuadContainer(int ContainerIndex, int QuadOffset, int QuadDrawNum) override;
	void RenderQuadContainerEx(int ContainerIndex, int QuadOffset, int QuadDrawNum, float X, float Y, float ScaleX = 1.f, float ScaleY = 1.f) override;
	void RenderQuadContainerAsSprite(int ContainerIndex, int QuadOffset, float X, float Y, float ScaleX = 1.f, float ScaleY = 1.f) override;
	void RenderQuadContainerAsSpriteMultiple(int ContainerIndex, int QuadOffset, int DrawCount, SRenderSpriteInfo *pRenderInfo) override;

	template<typename TName>
	void FlushVerticesImpl(bool KeepVertices, int &PrimType, int &PrimCount, int &NumVerts, TName &Command, size_t VertSize)
	{
		Command.m_pVertices = NULL;
		if(m_NumVertices == 0)
			return;

		NumVerts = m_NumVertices;

		if(!KeepVertices)
			m_NumVertices = 0;

		if(m_Drawing == DRAWING_QUADS)
		{
			if(g_Config.m_GfxQuadAsTriangle && !m_IsNewOpenGL)
			{
				PrimType = CCommandBuffer::PRIMTYPE_TRIANGLES;
				PrimCount = NumVerts / 3;
			}
			else
			{
				PrimType = CCommandBuffer::PRIMTYPE_QUADS;
				PrimCount = NumVerts / 4;
			}
		}
		else if(m_Drawing == DRAWING_LINES)
		{
			PrimType = CCommandBuffer::PRIMTYPE_LINES;
			PrimCount = NumVerts / 2;
		}
		else if(m_Drawing == DRAWING_TRIANGLES)
		{
			PrimType = CCommandBuffer::PRIMTYPE_TRIANGLES;
			PrimCount = NumVerts / 3;
		}
		else
			return;

		Command.m_pVertices = (decltype(Command.m_pVertices))m_pCommandBuffer->AllocData(VertSize * NumVerts);
		if(Command.m_pVertices == NULL)
		{
			// kick command buffer and try again
			KickCommandBuffer();

			Command.m_pVertices = (decltype(Command.m_pVertices))m_pCommandBuffer->AllocData(VertSize * NumVerts);
			if(Command.m_pVertices == NULL)
			{
				dbg_msg("graphics", "failed to allocate data for vertices");
				return;
			}
		}

		Command.m_State = m_State;

		Command.m_PrimType = PrimType;
		Command.m_PrimCount = PrimCount;

		// check if we have enough free memory in the commandbuffer
		if(!m_pCommandBuffer->AddCommand(Command))
		{
			// kick command buffer and try again
			KickCommandBuffer();

			Command.m_pVertices = (decltype(Command.m_pVertices))m_pCommandBuffer->AllocData(VertSize * NumVerts);
			if(Command.m_pVertices == NULL)
			{
				dbg_msg("graphics", "failed to allocate data for vertices");
				return;
			}

			if(!m_pCommandBuffer->AddCommand(Command))
			{
				dbg_msg("graphics", "failed to allocate memory for render command");
				return;
			}
		}
	}

	void FlushVertices(bool KeepVertices = false) override;
	void FlushTextVertices(int TextureSize, int TextTextureIndex, int TextOutlineTextureIndex, float *pOutlineTextColor) override;
	void FlushVerticesTex3D() override;

	void RenderTileLayer(int BufferContainerIndex, float *pColor, char **pOffsets, unsigned int *IndicedVertexDrawNum, size_t NumIndicesOffet) override;
	void RenderBorderTiles(int BufferContainerIndex, float *pColor, char *pIndexBufferOffset, float *pOffset, float *pDir, int JumpIndex, unsigned int DrawNum) override;
	void RenderBorderTileLines(int BufferContainerIndex, float *pColor, char *pIndexBufferOffset, float *pOffset, float *pDir, unsigned int IndexDrawNum, unsigned int RedrawNum) override;
	void RenderQuadLayer(int BufferContainerIndex, SQuadRenderInfo *pQuadInfo, int QuadNum, int QuadOffset) override;
	void RenderText(int BufferContainerIndex, int TextQuadNum, int TextureSize, int TextureTextIndex, int TextureTextOutlineIndex, float *pTextColor, float *pTextoutlineColor) override;

	// opengl 3.3 functions
	int CreateBufferObject(size_t UploadDataSize, void *pUploadData, bool IsMovedPointer = false) override;
	void RecreateBufferObject(int BufferIndex, size_t UploadDataSize, void *pUploadData, bool IsMovedPointer = false) override;
	void UpdateBufferObject(int BufferIndex, size_t UploadDataSize, void *pUploadData, void *pOffset, bool IsMovedPointer = false) override;
	void CopyBufferObject(int WriteBufferIndex, int ReadBufferIndex, size_t WriteOffset, size_t ReadOffset, size_t CopyDataSize) override;
	void DeleteBufferObject(int BufferIndex) override;

	int CreateBufferContainer(SBufferContainerInfo *pContainerInfo) override;
	// destroying all buffer objects means, that all referenced VBOs are destroyed automatically, so the user does not need to save references to them
	void DeleteBufferContainer(int ContainerIndex, bool DestroyAllBO = true) override;
	void UpdateBufferContainer(int ContainerIndex, SBufferContainerInfo *pContainerInfo) override;
	void IndicesNumRequiredNotify(unsigned int RequiredIndicesCount) override;

	int GetNumScreens() const override;
	void Minimize() override;
	void Maximize() override;
	bool Fullscreen(bool State) override;
	void SetWindowBordered(bool State) override;
	bool SetWindowScreen(int Index) override;
	void Resize(int w, int h, bool SetWindowSize = false) override;
	void AddWindowResizeListener(WINDOW_RESIZE_FUNC pFunc, void *pUser) override;
	int GetWindowScreen() override;

	int WindowActive() override;
	int WindowOpen() override;

	void SetWindowGrab(bool Grab) override;
	void NotifyWindow() override;

	int Init() override;
	void Shutdown() override;

	void TakeScreenshot(const char *pFilename) override;
	void TakeCustomScreenshot(const char *pFilename) override;
	void Swap() override;
	bool SetVSync(bool State) override;

	int GetVideoModes(CVideoMode *pModes, int MaxModes, int Screen) override;

	virtual int GetDesktopScreenWidth() { return m_DesktopScreenWidth; }
	virtual int GetDesktopScreenHeight() { return m_DesktopScreenHeight; }

	// synchronization
	void InsertSignal(CSemaphore *pSemaphore) override;
	bool IsIdle() override;
	void WaitForIdle() override;

	SWarning *GetCurWarning() override;

	bool IsTileBufferingEnabled() override { return m_OpenGLTileBufferingEnabled; }
	bool IsQuadBufferingEnabled() override { return m_OpenGLQuadBufferingEnabled; }
	bool IsTextBufferingEnabled() override { return m_OpenGLTextBufferingEnabled; }
	bool IsQuadContainerBufferingEnabled() override { return m_OpenGLQuadContainerBufferingEnabled; }
	bool HasTextureArrays() override { return m_OpenGLHasTextureArrays; }
};

extern IGraphicsBackend *CreateGraphicsBackend();

#endif // ENGINE_CLIENT_GRAPHICS_THREADED_H
