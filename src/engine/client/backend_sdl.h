#ifndef ENGINE_CLIENT_BACKEND_SDL_H
#define ENGINE_CLIENT_BACKEND_SDL_H

#include "SDL.h"

#include <base/detect.h>

#include "graphics_defines.h"

#include "blocklist_driver.h"
#include "graphics_threaded.h"

#include <base/tl/threading.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
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
	std::mutex m_BufferSwapMutex;
	std::condition_variable m_BufferSwapCond;
	std::condition_variable m_BufferDoneCond;
	CCommandBuffer *m_pBuffer;
	std::atomic_bool m_Shutdown;
	bool m_Started = false;
	std::atomic_bool m_BufferInProcess;
	void *m_Thread;

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

enum EBackendType
{
	BACKEND_TYPE_OPENGL = 0,
	BACKEND_TYPE_OPENGL_ES,
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

class CCommandProcessorFragment_OpenGLBase
{
public:
	virtual ~CCommandProcessorFragment_OpenGLBase() = default;
	virtual bool RunCommand(const CCommandBuffer::SCommand *pBaseCommand) = 0;

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

		const char **m_pErrStringPtr;

		char *m_pVendorString;
		char *m_pVersionString;
		char *m_pRendererString;

		int m_RequestedMajor;
		int m_RequestedMinor;
		int m_RequestedPatch;

		EBackendType m_RequestedBackend;

		int m_GlewMajor;
		int m_GlewMinor;
		int m_GlewPatch;
	};

	struct SCommand_Shutdown : public CCommandBuffer::SCommand
	{
		SCommand_Shutdown() :
			SCommand(CMD_SHUTDOWN) {}
	};
};

// command processor impelementation, uses the fragments to combine into one processor
class CCommandProcessor_SDL_OpenGL : public CGraphicsBackend_Threaded::ICommandProcessor
{
	CCommandProcessorFragment_OpenGLBase *m_pOpenGL;
	CCommandProcessorFragment_SDL m_SDL;
	CCommandProcessorFragment_General m_General;

	EBackendType m_BackendType;

public:
	CCommandProcessor_SDL_OpenGL(EBackendType BackendType, int OpenGLMajor, int OpenGLMinor, int OpenGLPatch);
	virtual ~CCommandProcessor_SDL_OpenGL();
	virtual void RunBuffer(CCommandBuffer *pBuffer);
};

static constexpr size_t gs_GPUInfoStringSize = 256;

// graphics backend implemented with SDL and OpenGL
class CGraphicsBackend_SDL_OpenGL : public CGraphicsBackend_Threaded
{
	SDL_Window *m_pWindow = NULL;
	SDL_GLContext m_GLContext;
	ICommandProcessor *m_pProcessor;
	std::atomic<int> m_TextureMemoryUsage;
	int m_NumScreens;

	SBackendCapabilites m_Capabilites;

	char m_aVendorString[gs_GPUInfoStringSize] = {};
	char m_aVersionString[gs_GPUInfoStringSize] = {};
	char m_aRendererString[gs_GPUInfoStringSize] = {};

	bool m_UseNewOpenGL;
	EBackendType m_BackendType;

	char m_aErrorString[256];

	static EBackendType DetectBackend();
	static void ClampDriverVersion(EBackendType BackendType);

public:
	CGraphicsBackend_SDL_OpenGL();
	virtual int Init(const char *pName, int *Screen, int *pWidth, int *pHeight, int *pRefreshRate, int FsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight, int *pCurrentWidth, int *pCurrentHeight, class IStorage *pStorage);
	virtual int Shutdown();

	virtual int MemoryUsage() const;

	virtual int GetNumScreens() const { return m_NumScreens; }

	virtual void GetVideoModes(CVideoMode *pModes, int MaxModes, int *pNumModes, int HiDPIScale, int MaxWindowWidth, int MaxWindowHeight, int Screen);
	virtual void GetCurrentVideoMode(CVideoMode &CurMode, int HiDPIScale, int MaxWindowWidth, int MaxWindowHeight, int Screen);

	virtual void Minimize();
	virtual void Maximize();
	virtual void SetWindowParams(int FullscreenMode, bool IsBorderless, bool AllowResizing);
	virtual bool SetWindowScreen(int Index);
	virtual bool UpdateDisplayMode(int Index);
	virtual int GetWindowScreen();
	virtual int WindowActive();
	virtual int WindowOpen();
	virtual void SetWindowGrab(bool Grab);
	virtual bool ResizeWindow(int w, int h, int RefreshRate);
	virtual void GetViewportSize(int &w, int &h);
	virtual void NotifyWindow();

	virtual void WindowDestroyNtf(uint32_t WindowID);
	virtual void WindowCreateNtf(uint32_t WindowID);

	virtual void GetDriverVersion(EGraphicsDriverAgeType DriverAgeType, int &Major, int &Minor, int &Patch);
	virtual bool IsConfigModernAPI() { return IsModernAPI(m_BackendType); }
	virtual bool IsNewOpenGL() { return m_UseNewOpenGL; }
	virtual bool HasTileBuffering() { return m_Capabilites.m_TileBuffering; }
	virtual bool HasQuadBuffering() { return m_Capabilites.m_QuadBuffering; }
	virtual bool HasTextBuffering() { return m_Capabilites.m_TextBuffering; }
	virtual bool HasQuadContainerBuffering() { return m_Capabilites.m_QuadContainerBuffering; }
	virtual bool Has2DTextureArrays() { return m_Capabilites.m_2DArrayTextures; }

	virtual const char *GetErrorString()
	{
		if(m_aErrorString[0] != '\0')
			return m_aErrorString;

		return NULL;
	}

	virtual const char *GetVendorString()
	{
		return m_aVendorString;
	}

	virtual const char *GetVersionString()
	{
		return m_aVersionString;
	}

	virtual const char *GetRendererString()
	{
		return m_aRendererString;
	}

	static bool IsModernAPI(EBackendType BackendType);
};

#endif // ENGINE_CLIENT_BACKEND_SDL_H
