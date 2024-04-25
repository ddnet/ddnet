#ifndef ENGINE_CLIENT_BACKEND_SDL_H
#define ENGINE_CLIENT_BACKEND_SDL_H

#include <SDL_video.h>

#include <base/detect.h>

#include <engine/graphics.h>

#include <engine/client/graphics_threaded.h>

#include <engine/client/backend/backend_base.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#if defined(CONF_PLATFORM_MACOS)
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
private:
	TTranslateFunc m_TranslateFunc;
	SGfxWarningContainer m_Warning;

public:
	// constructed on the main thread, the rest of the functions is run on the render thread
	class ICommandProcessor
	{
	public:
		virtual ~ICommandProcessor() = default;
		virtual void RunBuffer(CCommandBuffer *pBuffer) = 0;

		virtual const SGfxErrorContainer &GetError() const = 0;
		virtual void ErroneousCleanup() = 0;

		virtual const SGfxWarningContainer &GetWarning() const = 0;
	};

	CGraphicsBackend_Threaded(TTranslateFunc &&TranslateFunc);

	void RunBuffer(CCommandBuffer *pBuffer) override;
	void RunBufferSingleThreadedUnsafe(CCommandBuffer *pBuffer) override;
	bool IsIdle() const override;
	void WaitForIdle() override;

	void ProcessError(const SGfxErrorContainer &Error);

protected:
	void StartProcessor(ICommandProcessor *pProcessor);
	void StopProcessor();

	bool HasWarning()
	{
		return m_Warning.m_WarningType != GFX_WARNING_TYPE_NONE;
	}

private:
	ICommandProcessor *m_pProcessor;
	std::mutex m_BufferSwapMutex;
	std::condition_variable m_BufferSwapCond;
	CCommandBuffer *m_pBuffer;
	std::atomic_bool m_Shutdown;
	bool m_Started = false;
	std::atomic_bool m_BufferInProcess;
	void *m_pThread;

	static void ThreadFunc(void *pUser);

public:
	bool GetWarning(std::vector<std::string> &WarningStrings) override;
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

	// use quads as much as possible, even if the user config says otherwise
	bool m_TrianglesAsQuads;

	int m_ContextMajor;
	int m_ContextMinor;
	int m_ContextPatch;
};

// takes care of sdl related commands
class CCommandProcessorFragment_SDL
{
	// SDL stuff
	SDL_Window *m_pWindow = nullptr;
	SDL_GLContext m_GLContext = nullptr;

public:
	enum
	{
		CMD_INIT = CCommandBuffer::CMDGROUP_PLATFORM_SDL,
		CMD_SHUTDOWN,
	};

	struct SCommand_Init : public CCommandBuffer::SCommand
	{
		SCommand_Init() :
			SCommand(CMD_INIT) {}
		SDL_Window *m_pWindow;
		SDL_GLContext m_GLContext;
	};

	struct SCommand_Shutdown : public CCommandBuffer::SCommand
	{
		SCommand_Shutdown() :
			SCommand(CMD_SHUTDOWN) {}
	};

private:
	void Cmd_Init(const SCommand_Init *pCommand);
	void Cmd_Shutdown(const SCommand_Shutdown *pCommand);
	void Cmd_Swap(const CCommandBuffer::SCommand_Swap *pCommand);
	void Cmd_VSync(const CCommandBuffer::SCommand_VSync *pCommand);
	void Cmd_WindowCreateNtf(const CCommandBuffer::SCommand_WindowCreateNtf *pCommand);
	void Cmd_WindowDestroyNtf(const CCommandBuffer::SCommand_WindowDestroyNtf *pCommand);

public:
	CCommandProcessorFragment_SDL();

	bool RunCommand(const CCommandBuffer::SCommand *pBaseCommand);
};

// command processor implementation, uses the fragments to combine into one processor
class CCommandProcessor_SDL_GL : public CGraphicsBackend_Threaded::ICommandProcessor
{
	CCommandProcessorFragment_GLBase *m_pGLBackend;
	CCommandProcessorFragment_SDL m_SDL;
	CCommandProcessorFragment_General m_General;

	EBackendType m_BackendType;

	SGfxErrorContainer m_Error;
	SGfxWarningContainer m_Warning;

public:
	CCommandProcessor_SDL_GL(EBackendType BackendType, int GLMajor, int GLMinor, int GLPatch);
	virtual ~CCommandProcessor_SDL_GL();
	void RunBuffer(CCommandBuffer *pBuffer) override;

	const SGfxErrorContainer &GetError() const override;
	void ErroneousCleanup() override;

	const SGfxWarningContainer &GetWarning() const override;

	void HandleError();
	void HandleWarning();
};

static constexpr size_t gs_GpuInfoStringSize = 256;

// graphics backend implemented with SDL and the graphics library @see EBackendType
class CGraphicsBackend_SDL_GL : public CGraphicsBackend_Threaded
{
	SDL_Window *m_pWindow = nullptr;
	SDL_GLContext m_GLContext = nullptr;
	ICommandProcessor *m_pProcessor = nullptr;
	std::atomic<uint64_t> m_TextureMemoryUsage{0};
	std::atomic<uint64_t> m_BufferMemoryUsage{0};
	std::atomic<uint64_t> m_StreamMemoryUsage{0};
	std::atomic<uint64_t> m_StagingMemoryUsage{0};

	TTwGraphicsGpuList m_GpuList;

	TGLBackendReadPresentedImageData m_ReadPresentedImageDataFunc;

	int m_NumScreens;

	SBackendCapabilites m_Capabilites;

	char m_aVendorString[gs_GpuInfoStringSize] = {};
	char m_aVersionString[gs_GpuInfoStringSize] = {};
	char m_aRendererString[gs_GpuInfoStringSize] = {};

	EBackendType m_BackendType = BACKEND_TYPE_AUTO;

	char m_aErrorString[256];

	static EBackendType DetectBackend();
	static void ClampDriverVersion(EBackendType BackendType);

public:
	CGraphicsBackend_SDL_GL(TTranslateFunc &&TranslateFunc);
	int Init(const char *pName, int *pScreen, int *pWidth, int *pHeight, int *pRefreshRate, int *pFsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight, int *pCurrentWidth, int *pCurrentHeight, class IStorage *pStorage) override;
	int Shutdown() override;

	uint64_t TextureMemoryUsage() const override;
	uint64_t BufferMemoryUsage() const override;
	uint64_t StreamedMemoryUsage() const override;
	uint64_t StagingMemoryUsage() const override;

	const TTwGraphicsGpuList &GetGpus() const override;

	int GetNumScreens() const override { return m_NumScreens; }
	const char *GetScreenName(int Screen) const override;

	void GetVideoModes(CVideoMode *pModes, int MaxModes, int *pNumModes, int HiDPIScale, int MaxWindowWidth, int MaxWindowHeight, int ScreenId) override;
	void GetCurrentVideoMode(CVideoMode &CurMode, int HiDPIScale, int MaxWindowWidth, int MaxWindowHeight, int ScreenId) override;

	void Minimize() override;
	void Maximize() override;
	void SetWindowParams(int FullscreenMode, bool IsBorderless) override;
	bool SetWindowScreen(int Index) override;
	bool UpdateDisplayMode(int Index) override;
	int GetWindowScreen() override;
	int WindowActive() override;
	int WindowOpen() override;
	void SetWindowGrab(bool Grab) override;
	bool ResizeWindow(int w, int h, int RefreshRate) override;
	void GetViewportSize(int &w, int &h) override;
	void NotifyWindow() override;

	void WindowDestroyNtf(uint32_t WindowId) override;
	void WindowCreateNtf(uint32_t WindowId) override;

	bool GetDriverVersion(EGraphicsDriverAgeType DriverAgeType, int &Major, int &Minor, int &Patch, const char *&pName, EBackendType BackendType) override;
	bool IsConfigModernAPI() override { return IsModernAPI(m_BackendType); }
	bool UseTrianglesAsQuad() override { return m_Capabilites.m_TrianglesAsQuads; }
	bool HasTileBuffering() override { return m_Capabilites.m_TileBuffering; }
	bool HasQuadBuffering() override { return m_Capabilites.m_QuadBuffering; }
	bool HasTextBuffering() override { return m_Capabilites.m_TextBuffering; }
	bool HasQuadContainerBuffering() override { return m_Capabilites.m_QuadContainerBuffering; }
	bool Uses2DTextureArrays() override { return m_Capabilites.m_2DArrayTextures; }
	bool HasTextureArraysSupport() override { return m_Capabilites.m_2DArrayTextures || m_Capabilites.m_3DTextures; }

	const char *GetErrorString() override
	{
		if(m_aErrorString[0] != '\0')
			return m_aErrorString;

		return NULL;
	}

	const char *GetVendorString() override
	{
		return m_aVendorString;
	}

	const char *GetVersionString() override
	{
		return m_aVersionString;
	}

	const char *GetRendererString() override
	{
		return m_aRendererString;
	}

	TGLBackendReadPresentedImageData &GetReadPresentedImageDataFuncUnsafe() override;

	bool ShowMessageBox(unsigned Type, const char *pTitle, const char *pMsg) override;

	static bool IsModernAPI(EBackendType BackendType);
};

#endif // ENGINE_CLIENT_BACKEND_SDL_H
