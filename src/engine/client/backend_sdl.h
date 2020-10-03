#ifndef ENGINE_CLIENT_BACKEND_SDL_H
#define ENGINE_CLIENT_BACKEND_SDL_H

#include "SDL.h"
#include "SDL_opengl.h"

#include "graphics_threaded.h"

#include <base/tl/threading.h>

#include <atomic>

#if defined(CONF_PLATFORM_MACOSX)
#include <objc/objc-runtime.h>

class CAutoreleasePool
{
private:
	id m_Pool;

public:
	CAutoreleasePool()
	{
		Class NSAutoreleasePoolClass = (Class)objc_getClass("NSAutoreleasePool");
		m_Pool = class_createInstance(NSAutoreleasePoolClass, 0);
		SEL selector = sel_registerName("init");
		((id(*)(id, SEL))objc_msgSend)(m_Pool, selector);
	}

	~CAutoreleasePool()
	{
		SEL selector = sel_registerName("drain");
		((id(*)(id, SEL))objc_msgSend)(m_Pool, selector);
	}
};
#endif

// basic threaded backend, abstract, missing init and shutdown functions
class CGraphicsBackend_Threaded : public IGraphicsBackend
{
public:
	// constructed on the main thread, the rest of the functions is run on the render thread
	class ICommandProcessor
	{
	public:
		virtual ~ICommandProcessor() {}
		virtual void RunBuffer(CCommandBuffer *pBuffer) = 0;
	};

	CGraphicsBackend_Threaded();

	virtual void RunBuffer(CCommandBuffer *pBuffer);
	virtual bool IsIdle() const;
	virtual void WaitForIdle();

protected:
	void StartProcessor(ICommandProcessor *pProcessor);
	void StopProcessor();

private:
	ICommandProcessor *m_pProcessor;
	CCommandBuffer *volatile m_pBuffer;
	volatile bool m_Shutdown;
	semaphore m_Activity;
	semaphore m_BufferDone;
	void *m_pThread;

	static void ThreadFunc(void *pUser);
};

// takes care of implementation independent operations
class CCommandProcessorFragment_General
{
	void Cmd_Nop();
	void Cmd_Signal(const CCommandBuffer::SCommand_Signal *pCommand);

public:
	bool RunCommand(const CCommandBuffer::SCommand *pBaseCommand);
};

struct SBackendCapabilites
{
	bool m_TileBuffering;
	bool m_QuadBuffering;
	bool m_TextBuffering;
	bool m_QuadContainerBuffering;

	bool m_MipMapping;
	bool m_NPOTTextures;
	bool m_3DTextures;
	bool m_2DArrayTextures;
	bool m_2DArrayTexturesAsExtension;
	bool m_ShaderSupport;

	int m_ContextMajor;
	int m_ContextMinor;
	int m_ContextPatch;
};

class CGLSLProgram;
class CGLSLTWProgram;
class CGLSLPrimitiveProgram;
class CGLSLQuadProgram;
class CGLSLTileProgram;
class CGLSLTextProgram;
class CGLSLSpriteProgram;
class CGLSLSpriteMultipleProgram;

// takes care of opengl related rendering
class CCommandProcessorFragment_OpenGL
{
protected:
	struct CTexture
	{
		CTexture() :
			m_Tex(0), m_Tex2DArray(0), m_Sampler(0), m_Sampler2DArray(0) {}
		GLuint m_Tex;
		GLuint m_Tex2DArray; //or 3D texture as fallback
		GLuint m_Sampler;
		GLuint m_Sampler2DArray; //or 3D texture as fallback
		int m_LastWrapMode;

		int m_MemSize;

		int m_Width;
		int m_Height;
		int m_RescaleCount;
		float m_ResizeWidth;
		float m_ResizeHeight;
	};
	CTexture m_aTextures[CCommandBuffer::MAX_TEXTURES];
	std::atomic<int> *m_pTextureMemoryUsage;

	GLint m_MaxTexSize;

	bool m_Has2DArrayTextures;
	bool m_Has2DArrayTexturesAsExtension;
	GLenum m_2DArrayTarget;
	bool m_Has3DTextures;
	bool m_HasMipMaps;
	bool m_HasNPOTTextures;

	bool m_HasShaders;
	int m_LastBlendMode; //avoid all possible opengl state changes
	bool m_LastClipEnable;

	int m_OpenGLTextureLodBIAS;

public:
	enum
	{
		CMD_INIT = CCommandBuffer::CMDGROUP_PLATFORM_OPENGL,
		CMD_SHUTDOWN = CMD_INIT + 1,
	};

	struct SCommand_Init : public CCommandBuffer::SCommand
	{
		SCommand_Init() :
			SCommand(CMD_INIT) {}
		class IStorage *m_pStorage;
		std::atomic<int> *m_pTextureMemoryUsage;
		SBackendCapabilites *m_pCapabilities;
		int *m_pInitError;
	};

	struct SCommand_Shutdown : public CCommandBuffer::SCommand
	{
		SCommand_Shutdown() :
			SCommand(CMD_SHUTDOWN) {}
	};

protected:
	void SetState(const CCommandBuffer::SState &State, bool Use2DArrayTexture = false);
	virtual bool IsNewApi() { return false; }
	void DestroyTexture(int Slot);

	static int TexFormatToOpenGLFormat(int TexFormat);
	static int TexFormatToImageColorChannelCount(int TexFormat);
	static void *Resize(int Width, int Height, int NewWidth, int NewHeight, int Format, const unsigned char *pData);

	virtual void Cmd_Init(const SCommand_Init *pCommand);
	virtual void Cmd_Shutdown(const SCommand_Shutdown *pCommand) {}
	virtual void Cmd_Texture_Update(const CCommandBuffer::SCommand_Texture_Update *pCommand);
	virtual void Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand);
	virtual void Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand);
	virtual void Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand);
	virtual void Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand);
	virtual void Cmd_RenderTex3D(const CCommandBuffer::SCommand_RenderTex3D *pCommand) {}
	virtual void Cmd_Screenshot(const CCommandBuffer::SCommand_Screenshot *pCommand);

	virtual void Cmd_CreateBufferObject(const CCommandBuffer::SCommand_CreateBufferObject *pCommand) {}
	virtual void Cmd_RecreateBufferObject(const CCommandBuffer::SCommand_RecreateBufferObject *pCommand) {}
	virtual void Cmd_UpdateBufferObject(const CCommandBuffer::SCommand_UpdateBufferObject *pCommand) {}
	virtual void Cmd_CopyBufferObject(const CCommandBuffer::SCommand_CopyBufferObject *pCommand) {}
	virtual void Cmd_DeleteBufferObject(const CCommandBuffer::SCommand_DeleteBufferObject *pCommand) {}

	virtual void Cmd_CreateBufferContainer(const CCommandBuffer::SCommand_CreateBufferContainer *pCommand) {}
	virtual void Cmd_UpdateBufferContainer(const CCommandBuffer::SCommand_UpdateBufferContainer *pCommand) {}
	virtual void Cmd_DeleteBufferContainer(const CCommandBuffer::SCommand_DeleteBufferContainer *pCommand) {}
	virtual void Cmd_IndicesRequiredNumNotify(const CCommandBuffer::SCommand_IndicesRequiredNumNotify *pCommand) {}

	virtual void Cmd_RenderTileLayer(const CCommandBuffer::SCommand_RenderTileLayer *pCommand) {}
	virtual void Cmd_RenderBorderTile(const CCommandBuffer::SCommand_RenderBorderTile *pCommand) {}
	virtual void Cmd_RenderBorderTileLine(const CCommandBuffer::SCommand_RenderBorderTileLine *pCommand) {}
	virtual void Cmd_RenderQuadLayer(const CCommandBuffer::SCommand_RenderQuadLayer *pCommand) {}
	virtual void Cmd_RenderText(const CCommandBuffer::SCommand_RenderText *pCommand) {}
	virtual void Cmd_RenderTextStream(const CCommandBuffer::SCommand_RenderTextStream *pCommand) {}
	virtual void Cmd_RenderQuadContainer(const CCommandBuffer::SCommand_RenderQuadContainer *pCommand) {}
	virtual void Cmd_RenderQuadContainerAsSprite(const CCommandBuffer::SCommand_RenderQuadContainerAsSprite *pCommand) {}
	virtual void Cmd_RenderQuadContainerAsSpriteMultiple(const CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple *pCommand) {}

public:
	CCommandProcessorFragment_OpenGL();
	virtual ~CCommandProcessorFragment_OpenGL() = default;

	bool RunCommand(const CCommandBuffer::SCommand *pBaseCommand);
};

class CCommandProcessorFragment_OpenGL2 : public CCommandProcessorFragment_OpenGL
{
	struct SBufferContainer
	{
		SBufferContainer() {}
		SBufferContainerInfo m_ContainerInfo;
	};
	std::vector<SBufferContainer> m_BufferContainers;

	GL_SVertexTex3D m_aStreamVertices[1024 * 4];

	struct SBufferObject
	{
		SBufferObject(GLuint BufferObjectID) :
			m_BufferObjectID(BufferObjectID)
		{
			m_pData = NULL;
			m_DataSize = 0;
		}
		GLuint m_BufferObjectID;
		void *m_pData;
		size_t m_DataSize;
	};

	std::vector<SBufferObject> m_BufferObjectIndices;

	bool DoAnalyzeStep(size_t StepN, size_t CheckCount, size_t VerticesCount, uint8_t aFakeTexture[], size_t SingleImageSize);
	bool IsTileMapAnalysisSucceeded();

	void RenderBorderTileEmulation(SBufferContainer &BufferContainer, const CCommandBuffer::SState &State, const float *pColor, const char *pBuffOffset, unsigned int DrawNum, const float *pOffset, const float *pDir, int JumpIndex);
	void RenderBorderTileLineEmulation(SBufferContainer &BufferContainer, const CCommandBuffer::SState &State, const float *pColor, const char *pBuffOffset, unsigned int IndexDrawNum, unsigned int DrawNum, const float *pOffset, const float *pDir);

	void UseProgram(CGLSLTWProgram *pProgram);

protected:
	void SetState(const CCommandBuffer::SState &State, CGLSLTWProgram *pProgram, bool Use2DArrayTextures = false);

	void Cmd_Init(const SCommand_Init *pCommand) override;

	void Cmd_RenderTex3D(const CCommandBuffer::SCommand_RenderTex3D *pCommand) override;

	void Cmd_CreateBufferObject(const CCommandBuffer::SCommand_CreateBufferObject *pCommand) override;
	void Cmd_RecreateBufferObject(const CCommandBuffer::SCommand_RecreateBufferObject *pCommand) override;
	void Cmd_UpdateBufferObject(const CCommandBuffer::SCommand_UpdateBufferObject *pCommand) override;
	void Cmd_CopyBufferObject(const CCommandBuffer::SCommand_CopyBufferObject *pCommand) override;
	void Cmd_DeleteBufferObject(const CCommandBuffer::SCommand_DeleteBufferObject *pCommand) override;

	void Cmd_CreateBufferContainer(const CCommandBuffer::SCommand_CreateBufferContainer *pCommand) override;
	void Cmd_UpdateBufferContainer(const CCommandBuffer::SCommand_UpdateBufferContainer *pCommand) override;
	void Cmd_DeleteBufferContainer(const CCommandBuffer::SCommand_DeleteBufferContainer *pCommand) override;
	void Cmd_IndicesRequiredNumNotify(const CCommandBuffer::SCommand_IndicesRequiredNumNotify *pCommand) override;

	void Cmd_RenderTileLayer(const CCommandBuffer::SCommand_RenderTileLayer *pCommand) override;
	void Cmd_RenderBorderTile(const CCommandBuffer::SCommand_RenderBorderTile *pCommand) override;
	void Cmd_RenderBorderTileLine(const CCommandBuffer::SCommand_RenderBorderTileLine *pCommand) override;

	CGLSLTileProgram *m_pTileProgram;
	CGLSLTileProgram *m_pTileProgramTextured;
	CGLSLPrimitiveProgram *m_pPrimitive3DProgram;
	CGLSLPrimitiveProgram *m_pPrimitive3DProgramTextured;

	bool m_UseMultipleTextureUnits;

	GLint m_MaxTextureUnits;

	struct STextureBound
	{
		int m_TextureSlot;
		bool m_Is2DArray;
	};
	std::vector<STextureBound> m_TextureSlotBoundToUnit; //the texture index generated by loadtextureraw is stored in an index calculated by max texture units

	bool IsAndUpdateTextureSlotBound(int IDX, int Slot, bool Is2DArray = false);

public:
	CCommandProcessorFragment_OpenGL2() :
		CCommandProcessorFragment_OpenGL(), m_UseMultipleTextureUnits(false) {}
};

class CCommandProcessorFragment_OpenGL3 : public CCommandProcessorFragment_OpenGL2
{
};

#define MAX_STREAM_BUFFER_COUNT 30

// takes care of opengl 3.3+ related rendering
class CCommandProcessorFragment_OpenGL3_3 : public CCommandProcessorFragment_OpenGL3
{
	bool m_UsePreinitializedVertexBuffer;

	int m_MaxQuadsAtOnce;
	static const int m_MaxQuadsPossible = 256;

	CGLSLPrimitiveProgram *m_pPrimitiveProgram;
	CGLSLTileProgram *m_pBorderTileProgram;
	CGLSLTileProgram *m_pBorderTileProgramTextured;
	CGLSLTileProgram *m_pBorderTileLineProgram;
	CGLSLTileProgram *m_pBorderTileLineProgramTextured;
	CGLSLQuadProgram *m_pQuadProgram;
	CGLSLQuadProgram *m_pQuadProgramTextured;
	CGLSLTextProgram *m_pTextProgram;
	CGLSLSpriteProgram *m_pSpriteProgram;
	CGLSLSpriteMultipleProgram *m_pSpriteProgramMultiple;

	GLuint m_LastProgramID;

	GLuint m_PrimitiveDrawVertexID[MAX_STREAM_BUFFER_COUNT];
	GLuint m_PrimitiveDrawVertexIDTex3D;
	GLuint m_PrimitiveDrawBufferID[MAX_STREAM_BUFFER_COUNT];
	GLuint m_PrimitiveDrawBufferIDTex3D;

	GLuint m_LastIndexBufferBound[MAX_STREAM_BUFFER_COUNT];

	int m_LastStreamBuffer;

	GLuint m_QuadDrawIndexBufferID;
	unsigned int m_CurrentIndicesInBuffer;

	void DestroyBufferContainer(int Index, bool DeleteBOs = true);

	void AppendIndices(unsigned int NewIndicesCount);

	struct SBufferContainer
	{
		SBufferContainer() :
			m_VertArrayID(0), m_LastIndexBufferBound(0) {}
		GLuint m_VertArrayID;
		GLuint m_LastIndexBufferBound;
		SBufferContainerInfo m_ContainerInfo;
	};
	std::vector<SBufferContainer> m_BufferContainers;

	std::vector<GLuint> m_BufferObjectIndices;

	CCommandBuffer::SColorf m_ClearColor;

protected:
	static int TexFormatToNewOpenGLFormat(int TexFormat);
	bool IsNewApi() override { return true; }

	void UseProgram(CGLSLTWProgram *pProgram);
	void UploadStreamBufferData(unsigned int PrimitiveType, const void *pVertices, size_t VertSize, unsigned int PrimitiveCount, bool AsTex3D = false);
	void RenderText(const CCommandBuffer::SState &State, int DrawNum, int TextTextureIndex, int TextOutlineTextureIndex, int TextureSize, const float *pTextColor, const float *pTextOutlineColor);

	void Cmd_Init(const SCommand_Init *pCommand) override;
	void Cmd_Shutdown(const SCommand_Shutdown *pCommand) override;
	void Cmd_Texture_Update(const CCommandBuffer::SCommand_Texture_Update *pCommand) override;
	void Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand) override;
	void Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand) override;
	void Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand) override;
	void Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand) override;
	void Cmd_RenderTex3D(const CCommandBuffer::SCommand_RenderTex3D *pCommand) override;
	void Cmd_Screenshot(const CCommandBuffer::SCommand_Screenshot *pCommand) override;

	void Cmd_CreateBufferObject(const CCommandBuffer::SCommand_CreateBufferObject *pCommand) override;
	void Cmd_RecreateBufferObject(const CCommandBuffer::SCommand_RecreateBufferObject *pCommand) override;
	void Cmd_UpdateBufferObject(const CCommandBuffer::SCommand_UpdateBufferObject *pCommand) override;
	void Cmd_CopyBufferObject(const CCommandBuffer::SCommand_CopyBufferObject *pCommand) override;
	void Cmd_DeleteBufferObject(const CCommandBuffer::SCommand_DeleteBufferObject *pCommand) override;

	void Cmd_CreateBufferContainer(const CCommandBuffer::SCommand_CreateBufferContainer *pCommand) override;
	void Cmd_UpdateBufferContainer(const CCommandBuffer::SCommand_UpdateBufferContainer *pCommand) override;
	void Cmd_DeleteBufferContainer(const CCommandBuffer::SCommand_DeleteBufferContainer *pCommand) override;
	void Cmd_IndicesRequiredNumNotify(const CCommandBuffer::SCommand_IndicesRequiredNumNotify *pCommand) override;

	void Cmd_RenderTileLayer(const CCommandBuffer::SCommand_RenderTileLayer *pCommand) override;
	void Cmd_RenderBorderTile(const CCommandBuffer::SCommand_RenderBorderTile *pCommand) override;
	void Cmd_RenderBorderTileLine(const CCommandBuffer::SCommand_RenderBorderTileLine *pCommand) override;
	void Cmd_RenderQuadLayer(const CCommandBuffer::SCommand_RenderQuadLayer *pCommand) override;
	void Cmd_RenderText(const CCommandBuffer::SCommand_RenderText *pCommand) override;
	void Cmd_RenderTextStream(const CCommandBuffer::SCommand_RenderTextStream *pCommand) override;
	void Cmd_RenderQuadContainer(const CCommandBuffer::SCommand_RenderQuadContainer *pCommand) override;
	void Cmd_RenderQuadContainerAsSprite(const CCommandBuffer::SCommand_RenderQuadContainerAsSprite *pCommand) override;
	void Cmd_RenderQuadContainerAsSpriteMultiple(const CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple *pCommand) override;

public:
	CCommandProcessorFragment_OpenGL3_3() = default;
};

// takes care of sdl related commands
class CCommandProcessorFragment_SDL
{
	// SDL stuff
	SDL_Window *m_pWindow;
	SDL_GLContext m_GLContext;

public:
	enum
	{
		CMD_INIT = CCommandBuffer::CMDGROUP_PLATFORM_SDL,
		CMD_UPDATE_VIEWPORT,
		CMD_SHUTDOWN,
	};

	struct SCommand_Init : public CCommandBuffer::SCommand
	{
		SCommand_Init() :
			SCommand(CMD_INIT) {}
		SDL_Window *m_pWindow;
		SDL_GLContext m_GLContext;
		SBackendCapabilites *m_pCapabilities;

		int *m_pInitError;

		int m_RequestedMajor;
		int m_RequestedMinor;
		int m_RequestedPatch;

		int m_GlewMajor;
		int m_GlewMinor;
		int m_GlewPatch;
	};

	struct SCommand_Update_Viewport : public CCommandBuffer::SCommand
	{
		SCommand_Update_Viewport() :
			SCommand(CMD_UPDATE_VIEWPORT) {}
		int m_X;
		int m_Y;
		int m_Width;
		int m_Height;
	};

	struct SCommand_Shutdown : public CCommandBuffer::SCommand
	{
		SCommand_Shutdown() :
			SCommand(CMD_SHUTDOWN) {}
	};

private:
	void Cmd_Init(const SCommand_Init *pCommand);
	void Cmd_Update_Viewport(const SCommand_Update_Viewport *pCommand);
	void Cmd_Shutdown(const SCommand_Shutdown *pCommand);
	void Cmd_Swap(const CCommandBuffer::SCommand_Swap *pCommand);
	void Cmd_VSync(const CCommandBuffer::SCommand_VSync *pCommand);
	void Cmd_Resize(const CCommandBuffer::SCommand_Resize *pCommand);
	void Cmd_VideoModes(const CCommandBuffer::SCommand_VideoModes *pCommand);

public:
	CCommandProcessorFragment_SDL();

	bool RunCommand(const CCommandBuffer::SCommand *pBaseCommand);
};

// command processor impelementation, uses the fragments to combine into one processor
class CCommandProcessor_SDL_OpenGL : public CGraphicsBackend_Threaded::ICommandProcessor
{
	CCommandProcessorFragment_OpenGL *m_pOpenGL;
	CCommandProcessorFragment_SDL m_SDL;
	CCommandProcessorFragment_General m_General;

public:
	CCommandProcessor_SDL_OpenGL(int OpenGLMajor, int OpenGLMinor, int OpenGLPatch);
	virtual ~CCommandProcessor_SDL_OpenGL();
	virtual void RunBuffer(CCommandBuffer *pBuffer);
};

// graphics backend implemented with SDL and OpenGL
class CGraphicsBackend_SDL_OpenGL : public CGraphicsBackend_Threaded
{
	SDL_Window *m_pWindow;
	SDL_GLContext m_GLContext;
	ICommandProcessor *m_pProcessor;
	std::atomic<int> m_TextureMemoryUsage;
	int m_NumScreens;

	SBackendCapabilites m_Capabilites;

	bool m_UseNewOpenGL;

public:
	virtual int Init(const char *pName, int *Screen, int *pWidth, int *pHeight, int FsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight, int *pCurrentWidth, int *pCurrentHeight, class IStorage *pStorage);
	virtual int Shutdown();

	virtual int MemoryUsage() const;

	virtual int GetNumScreens() const { return m_NumScreens; }

	virtual void Minimize();
	virtual void Maximize();
	virtual bool Fullscreen(bool State);
	virtual void SetWindowBordered(bool State); // on=true/off=false
	virtual bool SetWindowScreen(int Index);
	virtual int GetWindowScreen();
	virtual int WindowActive();
	virtual int WindowOpen();
	virtual void SetWindowGrab(bool Grab);
	virtual void NotifyWindow();

	virtual bool IsNewOpenGL() { return m_UseNewOpenGL; }
	virtual bool HasTileBuffering() { return m_Capabilites.m_TileBuffering; }
	virtual bool HasQuadBuffering() { return m_Capabilites.m_QuadBuffering; }
	virtual bool HasTextBuffering() { return m_Capabilites.m_TextBuffering; }
	virtual bool HasQuadContainerBuffering() { return m_Capabilites.m_QuadContainerBuffering; }
	virtual bool Has2DTextureArrays() { return m_Capabilites.m_2DArrayTextures; }
};

#endif // ENGINE_CLIENT_BACKEND_SDL_H
