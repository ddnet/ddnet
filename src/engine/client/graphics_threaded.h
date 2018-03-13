#pragma once

#include <engine/graphics.h>

#include <vector>

#define CMD_BUFFER_DATA_BUFFER_SIZE 1024*1024*2
#define CMD_BUFFER_CMD_BUFFER_SIZE 1024*256

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
			delete [] m_pData;
			m_pData = 0x0;
			m_Used = 0;
			m_Size = 0;
		}

		void Reset()
		{
			m_Used = 0;
		}

		void *Alloc(unsigned Requested)
		{
			if(Requested + m_Used > m_Size)
				return 0;
			void *pPtr = &m_pData[m_Used];
			m_Used += Requested;
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
		MAX_TEXTURES=1024*4,
		MAX_VERTICES=32*1024,
	};

	enum
	{
		// commadn groups
		CMDGROUP_CORE = 0, // commands that everyone has to implement
		CMDGROUP_PLATFORM_OPENGL = 10000, // commands specific to a platform
		CMDGROUP_PLATFORM_SDL = 20000,
		CMDGROUP_PLATFORM_OPENGL3_3 = 30000,

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
		
		//opengl 3.3 commands
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
		CMD_RENDER_QUAD_CONTAINER_SPRITE, // render a quad buffer container as sprite
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
	
	struct SPoint { float x, y; };
	struct STexCoord { float u, v; };
	struct SColorf { float r, g, b, a; };
	
	//use normalized color values
	struct SColor { unsigned char r, g, b, a; };

	struct SVertex
	{
		SPoint m_Pos;
		STexCoord m_Tex;
		SColor m_Color;
	};

	struct SCommand
	{
	public:
		SCommand(unsigned Cmd) : m_Cmd(Cmd), m_Size(0) {}
		unsigned m_Cmd;
		unsigned m_Size;
	};

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
		SCommand_Clear() : SCommand(CMD_CLEAR) {}
		SColorf m_Color;
	};

	struct SCommand_Signal : public SCommand
	{
		SCommand_Signal() : SCommand(CMD_SIGNAL) {}
		semaphore *m_pSemaphore;
	};

	struct SCommand_RunBuffer : public SCommand
	{
		SCommand_RunBuffer() : SCommand(CMD_RUNBUFFER) {}
		CCommandBuffer *m_pOtherBuffer;
	};

	struct SCommand_Render : public SCommand
	{
		SCommand_Render() : SCommand(CMD_RENDER) {}
		SState m_State;
		unsigned m_PrimType;
		unsigned m_PrimCount;
		SVertex *m_pVertices; // you should use the command buffer data to allocate vertices for this command
	};

	struct SCommand_CreateBufferObject : public SCommand
	{
		SCommand_CreateBufferObject() : SCommand(CMD_CREATE_BUFFER_OBJECT) {}

		int m_BufferIndex;

		void *m_pUploadData;
		size_t m_DataSize;
	};


	struct SCommand_RecreateBufferObject : public SCommand
	{
		SCommand_RecreateBufferObject() : SCommand(CMD_RECREATE_BUFFER_OBJECT) {}

		int m_BufferIndex;

		void *m_pUploadData;
		size_t m_DataSize;
	};

	struct SCommand_UpdateBufferObject : public SCommand
	{
		SCommand_UpdateBufferObject() : SCommand(CMD_UPDATE_BUFFER_OBJECT) {}

		int m_BufferIndex;

		void *m_pOffset;
		void *m_pUploadData;
		size_t m_DataSize;
	};

	struct SCommand_CopyBufferObject : public SCommand
	{
		SCommand_CopyBufferObject() : SCommand(CMD_COPY_BUFFER_OBJECT) {}

		int m_WriteBufferIndex;
		int m_ReadBufferIndex;

		size_t m_pReadOffset;
		size_t m_pWriteOffset;
		size_t m_CopySize;
	};

	struct SCommand_DeleteBufferObject : public SCommand
	{
		SCommand_DeleteBufferObject() : SCommand(CMD_DELETE_BUFFER_OBJECT) {}

		int m_BufferIndex;
	};

	struct SCommand_CreateBufferContainer : public SCommand
	{
		SCommand_CreateBufferContainer() : SCommand(CMD_CREATE_BUFFER_CONTAINER) {}

		int m_BufferContainerIndex;

		int m_Stride;

		int m_AttrCount;
		SBufferContainerInfo::SAttribute* m_Attributes;
	};

	struct SCommand_UpdateBufferContainer : public SCommand
	{
		SCommand_UpdateBufferContainer() : SCommand(CMD_UPDATE_BUFFER_CONTAINER) {}

		int m_BufferContainerIndex;

		int m_Stride;

		int m_AttrCount;
		SBufferContainerInfo::SAttribute* m_Attributes;
	};

	struct SCommand_DeleteBufferContainer : public SCommand
	{
		SCommand_DeleteBufferContainer() : SCommand(CMD_DELETE_BUFFER_CONTAINER) {}

		int m_BufferContainerIndex;
		bool m_DestroyAllBO;
	};

	struct SCommand_IndicesRequiredNumNotify : public SCommand
	{
		SCommand_IndicesRequiredNumNotify() : SCommand(CMD_INDICES_REQUIRED_NUM_NOTIFY) {}

		unsigned int m_RequiredIndicesNum;
	};
		
	struct SCommand_RenderTileLayer : public SCommand
	{
		SCommand_RenderTileLayer() : SCommand(CMD_RENDER_TILE_LAYER) {}
		SState m_State;
		SColorf m_Color; //the color of the whole tilelayer -- already envelopped
			
		//the char offset of all indices that should be rendered, and the amount of renders
		char** m_pIndicesOffsets;
		unsigned int *m_pDrawCount;
		
		int m_IndicesDrawNum;
		int m_BufferContainerIndex;
		int m_LOD;
	};
	
	struct SCommand_RenderBorderTile : public SCommand
	{
		SCommand_RenderBorderTile() : SCommand(CMD_RENDER_BORDER_TILE) {}
		SState m_State;
		SColorf m_Color; //the color of the whole tilelayer -- already envelopped
		char *m_pIndicesOffset; // you should use the command buffer data to allocate vertices for this command
		unsigned int m_DrawNum;
		int m_BufferContainerIndex;
		int m_LOD;
		
		float m_Offset[2];
		float m_Dir[2];
		int m_JumpIndex;
	};
	
	struct SCommand_RenderBorderTileLine : public SCommand
	{
		SCommand_RenderBorderTileLine() : SCommand(CMD_RENDER_BORDER_TILE_LINE) {}
		SState m_State;
		SColorf m_Color; //the color of the whole tilelayer -- already envelopped
		char *m_pIndicesOffset; // you should use the command buffer data to allocate vertices for this command
		unsigned int m_IndexDrawNum;
		unsigned int m_DrawNum;
		int m_BufferContainerIndex;
		int m_LOD;
		
		float m_Dir[2];
	};

	struct SCommand_RenderQuadLayer : public SCommand
	{
		SCommand_RenderQuadLayer() : SCommand(CMD_RENDER_QUAD_LAYER) {}
		SState m_State;

		int m_BufferContainerIndex;
		SQuadRenderInfo* m_pQuadInfo;
		int m_QuadNum;
	};

	struct SCommand_RenderText : public SCommand
	{
		SCommand_RenderText() : SCommand(CMD_RENDER_TEXT) {}
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
		SCommand_RenderTextStream() : SCommand(CMD_RENDER_TEXT_STREAM) {}
		SState m_State;
		
		SVertex *m_pVertices;
		int m_QuadNum;

		int m_TextureSize;

		int m_TextTextureIndex;
		int m_TextOutlineTextureIndex;

		float m_aTextOutlineColor[4];
	};

	struct SCommand_RenderQuadContainer : public SCommand
	{
		SCommand_RenderQuadContainer() : SCommand(CMD_RENDER_QUAD_CONTAINER) {}
		SState m_State;

		int m_BufferContainerIndex;

		unsigned int m_DrawNum;
		void *m_pOffset;
	};

	struct SCommand_RenderQuadContainerAsSprite : public SCommand
	{
		SCommand_RenderQuadContainerAsSprite() : SCommand(CMD_RENDER_QUAD_CONTAINER_SPRITE) {}
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
		SCommand_RenderQuadContainerAsSpriteMultiple() : SCommand(CMD_RENDER_QUAD_CONTAINER_SPRITE_MULTIPLE) {}
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
		SCommand_Screenshot() : SCommand(CMD_SCREENSHOT) {}
		CImageInfo *m_pImage; // processor will fill this out, the one who adds this command must free the data as well
	};

	struct SCommand_VideoModes : public SCommand
	{
		SCommand_VideoModes() : SCommand(CMD_VIDEOMODES) {}

		CVideoMode *m_pModes; // processor will fill this in
		int m_MaxModes; // maximum of modes the processor can write to the m_pModes
		int *m_pNumModes; // processor will write to this pointer
		int m_Screen;
	};

	struct SCommand_Swap : public SCommand
	{
		SCommand_Swap() : SCommand(CMD_SWAP) {}

		int m_Finish;
	};

	struct SCommand_VSync : public SCommand
	{
		SCommand_VSync() : SCommand(CMD_VSYNC) {}

		int m_VSync;
		bool *m_pRetOk;
	};

	struct SCommand_Resize : public SCommand
	{
		SCommand_Resize() : SCommand(CMD_RESIZE) {}

		int m_Width;
		int m_Height;
	};

	struct SCommand_Texture_Create : public SCommand
	{
		SCommand_Texture_Create() : SCommand(CMD_TEXTURE_CREATE) {}

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
		SCommand_Texture_Update() : SCommand(CMD_TEXTURE_UPDATE) {}

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
		SCommand_Texture_Destroy() : SCommand(CMD_TEXTURE_DESTROY) {}

		// texture information
		int m_Slot;
	};

	//
	CCommandBuffer(unsigned CmdBufferSize, unsigned DataBufferSize)
	: m_CmdBuffer(CmdBufferSize), m_DataBuffer(DataBufferSize)
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
		SCommand *pCmd = (SCommand *)m_CmdBuffer.Alloc(sizeof(Command));
		if(!pCmd)
			return false;
		mem_copy(pCmd, &Command, sizeof(Command));
		pCmd->m_Size = sizeof(Command);
		return true;
	}

	SCommand *GetCommand(unsigned *pIndex)
	{
		if(*pIndex >= m_CmdBuffer.DataUsed())
			return NULL;

		SCommand *pCommand = (SCommand *)&m_CmdBuffer.DataPtr()[*pIndex];
		*pIndex += pCommand->m_Size;
		return pCommand;
	}

	void Reset()
	{
		m_CmdBuffer.Reset();
		m_DataBuffer.Reset();
	}
};

// interface for the graphics backend
// all these functions are called on the main thread
class IGraphicsBackend
{
public:
	enum
	{
		INITFLAG_FULLSCREEN = 1,
		INITFLAG_VSYNC = 2,
		INITFLAG_RESIZABLE = 4,
		INITFLAG_BORDERLESS = 8,
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
	virtual void NotifyWindow() = 0;

	virtual void RunBuffer(CCommandBuffer *pBuffer) = 0;
	virtual bool IsIdle() const = 0;
	virtual void WaitForIdle() = 0;
	
	virtual bool IsOpenGL3_3() { return false; }
};

class CGraphics_Threaded : public IEngineGraphics
{
	enum
	{
		NUM_CMDBUFFERS = 2,

		MAX_VERTICES = 32*1024,
		MAX_TEXTURES = 1024*4,

		DRAWING_QUADS=1,
		DRAWING_LINES=2
	};

	CCommandBuffer::SState m_State;
	IGraphicsBackend *m_pBackend;
	bool m_UseOpenGL3_3;

	CCommandBuffer *m_apCommandBuffers[NUM_CMDBUFFERS];
	CCommandBuffer *m_pCommandBuffer;
	unsigned m_CurrentCommandBuffer;

	//
	class IStorage *m_pStorage;
	class IConsole *m_pConsole;

	CCommandBuffer::SVertex m_aVertices[MAX_VERTICES];
	int m_NumVertices;

	CCommandBuffer::SColor m_aColor[4];
	CCommandBuffer::STexCoord m_aTexture[4];

	bool m_RenderEnable;

	float m_Rotation;
	int m_Drawing;
	bool m_DoScreenshot;
	char m_aScreenshotName[128];

	int m_InvalidTexture;

	int m_aTextureIndices[MAX_TEXTURES];
	int m_FirstFreeTexture;
	int m_TextureMemoryUsage;

	struct SVertexArrayInfo
	{
		SVertexArrayInfo() : m_FreeIndex(-1) {}
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

	void* AllocCommandBufferData(unsigned AllocSize);

	void AddVertices(int Count);
	void Rotate(const CCommandBuffer::SPoint &rCenter, CCommandBuffer::SVertex *pPoints, int NumPoints);

	void KickCommandBuffer();

	int IssueInit();
	int InitWindow();
public:
	CGraphics_Threaded();

	virtual void ClipEnable(int x, int y, int w, int h);
	virtual void ClipDisable();

	virtual void BlendNone();
	virtual void BlendNormal();
	virtual void BlendAdditive();

	virtual void WrapNormal();
	virtual void WrapClamp();

	virtual int MemoryUsage() const;

	virtual void MapScreen(float TopLeftX, float TopLeftY, float BottomRightX, float BottomRightY);
	virtual void GetScreen(float *pTopLeftX, float *pTopLeftY, float *pBottomRightX, float *pBottomRightY);

	virtual void LinesBegin();
	virtual void LinesEnd();
	virtual void LinesDraw(const CLineItem *pArray, int Num);

	virtual int UnloadTexture(int Index);
	virtual int LoadTextureRaw(int Width, int Height, int Format, const void *pData, int StoreFormat, int Flags);
	virtual int LoadTextureRawSub(int TextureID, int x, int y, int Width, int Height, int Format, const void *pData);

	// simple uncompressed RGBA loaders
	virtual int LoadTexture(const char *pFilename, int StorageType, int StoreFormat, int Flags);
	virtual int LoadPNG(CImageInfo *pImg, const char *pFilename, int StorageType);

	void ScreenshotDirect();

	virtual void TextureSet(int TextureID);

	virtual void Clear(float r, float g, float b);

	virtual void QuadsBegin();
	virtual void QuadsEnd();
	virtual void TextQuadsBegin();
	virtual void TextQuadsEnd(int TextureSize, int TextTextureIndex, int TextOutlineTextureIndex, float* pOutlineTextColor);
	virtual void QuadsEndKeepVertices();
	virtual void QuadsDrawCurrentVertices(bool KeepVertices = true);
	virtual void QuadsSetRotation(float Angle);

	virtual void SetColorVertex(const CColorVertex *pArray, int Num);
	virtual void SetColor(float r, float g, float b, float a);

	// go through all vertices and change their color (only works for quads)
	virtual void ChangeColorOfCurrentQuadVertices(float r, float g, float b, float a);
	
	void SetColor(CCommandBuffer::SVertex *pVertex, int ColorIndex);

	virtual void QuadsSetSubset(float TlU, float TlV, float BrU, float BrV);
	virtual void QuadsSetSubsetFree(
		float x0, float y0, float x1, float y1,
		float x2, float y2, float x3, float y3);

	virtual void QuadsDraw(CQuadItem *pArray, int Num);
	virtual void QuadsDrawTL(const CQuadItem *pArray, int Num);
	virtual void QuadsDrawFreeform(const CFreeformItem *pArray, int Num);
	virtual void QuadsText(float x, float y, float Size, const char *pText);

	virtual int CreateQuadContainer();
	virtual void QuadContainerAddQuads(int ContainerIndex, CQuadItem *pArray, int Num);
	virtual void QuadContainerAddQuads(int ContainerIndex, CFreeformItem *pArray, int Num);
	virtual void QuadContainerReset(int ContainerIndex);
	virtual void DeleteQuadContainer(int ContainerIndex);
	virtual void RenderQuadContainer(int ContainerIndex, int QuadDrawNum);
	virtual void RenderQuadContainer(int ContainerIndex, int QuadOffset, int QuadDrawNum);
	virtual void RenderQuadContainerAsSprite(int ContainerIndex, int QuadOffset, float X, float Y, float ScaleX = 1.f, float ScaleY = 1.f);
	virtual void RenderQuadContainerAsSpriteMultiple(int ContainerIndex, int QuadOffset, int DrawCount, SRenderSpriteInfo *pRenderInfo);

	virtual void FlushVertices(bool KeepVertices = false);
	virtual void FlushTextVertices(int TextureSize, int TextTextureIndex, int TextOutlineTextureIndex, float *pOutlineTextColor);

	virtual void RenderTileLayer(int BufferContainerIndex, float *pColor, char** pOffsets, unsigned int *IndicedVertexDrawNum, size_t NumIndicesOffet);
	virtual void RenderBorderTiles(int BufferContainerIndex, float *pColor, char *pOffset, float *Offset, float *Dir, int JumpIndex, unsigned int DrawNum);
	virtual void RenderBorderTileLines(int BufferContainerIndex, float *pColor, char *pOffset, float *Dir, unsigned int IndexDrawNum, unsigned int RedrawNum);
	virtual void RenderQuadLayer(int BufferContainerIndex, SQuadRenderInfo* pQuadInfo, int QuadNum);
	virtual void RenderText(int BufferContainerIndex, int TextQuadNum, int TextureSize, int TextureTextIndex, int TextureTextOutlineIndex, float* pTextColor, float* pTextoutlineColor);

	// opengl 3.3 functions
	virtual int CreateBufferObject(size_t UploadDataSize, void* pUploadData);
	virtual void RecreateBufferObject(int BufferIndex, size_t UploadDataSize, void* pUploadData);
	virtual void UpdateBufferObject(int BufferIndex, size_t UploadDataSize, void* pUploadData, void* pOffset);
	virtual void CopyBufferObject(int WriteBufferIndex, int ReadBufferIndex, size_t WriteOffset, size_t ReadOffset, size_t CopyDataSize);
	virtual void DeleteBufferObject(int BufferIndex);

	virtual int CreateBufferContainer(SBufferContainerInfo *pContainerInfo);
	// destroying all buffer objects means, that all referenced VBOs are destroyed automatically, so the user does not need to save references to them
	virtual void DeleteBufferContainer(int ContainerIndex, bool DestroyAllBO = true);
	virtual void UpdateBufferContainer(int ContainerIndex, SBufferContainerInfo *pContainerInfo);
	virtual void IndicesNumRequiredNotify(unsigned int RequiredIndicesCount);


	virtual int GetNumScreens() const;
	virtual void Minimize();
	virtual void Maximize();
	virtual bool Fullscreen(bool State);
	virtual void SetWindowBordered(bool State);
	virtual bool SetWindowScreen(int Index);
	virtual void Resize(int w, int h);
	virtual int GetWindowScreen();

	virtual int WindowActive();
	virtual int WindowOpen();

	virtual void SetWindowGrab(bool Grab);
	virtual void NotifyWindow();

	virtual int Init();
	virtual void Shutdown();

	virtual void TakeScreenshot(const char *pFilename);
	virtual void TakeCustomScreenshot(const char *pFilename);
	virtual void Swap();
	virtual bool SetVSync(bool State);

	virtual int GetVideoModes(CVideoMode *pModes, int MaxModes, int Screen);

	virtual int GetDesktopScreenWidth() { return m_DesktopScreenWidth; }
	virtual int GetDesktopScreenHeight() { return m_DesktopScreenHeight; }

	// synchronization
	virtual void InsertSignal(semaphore *pSemaphore);
	virtual bool IsIdle();
	virtual void WaitForIdle();
	
	virtual bool IsBufferingEnabled() { return m_UseOpenGL3_3; }
};

extern IGraphicsBackend *CreateGraphicsBackend();
