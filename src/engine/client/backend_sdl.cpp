#include <base/detect.h>

#if defined(CONF_FAMILY_WINDOWS)
// For FlashWindowEx, FLASHWINFO, FLASHW_TRAY
#define _WIN32_WINNT 0x0501
#define WINVER 0x0501
#endif

#ifndef CONF_BACKEND_OPENGL_ES
#include <GL/glew.h>
#endif

#include <engine/storage.h>

#include "SDL.h"

#include "SDL_syswm.h"
#include <base/detect.h>
#include <base/math.h>
#include <cmath>
#include <cstdlib>

#include "SDL_hints.h"
#include "SDL_pixels.h"
#include "SDL_video.h"

#include <engine/shared/config.h>

#include <base/tl/threading.h>

#if defined(CONF_VIDEORECORDER)
#include "video.h"
#endif

#include "backend_sdl.h"

#if !defined(CONF_BACKEND_OPENGL_ES)
#include "backend/opengl/backend_opengl3.h"
#endif

#if defined(CONF_BACKEND_OPENGL_ES3) || defined(CONF_BACKEND_OPENGL_ES)
#include "backend/opengles/backend_opengles3.h"
#endif

#include "graphics_threaded.h"

#include <engine/shared/image_manipulation.h>

#include <engine/graphics.h>

#ifdef __MINGW32__
extern "C" {
int putenv(const char *);
}
#endif

// ------------ CGraphicsBackend_Threaded

void CGraphicsBackend_Threaded::ThreadFunc(void *pUser)
{
	auto *pSelf = (CGraphicsBackend_Threaded *)pUser;
	std::unique_lock<std::mutex> Lock(pSelf->m_BufferSwapMutex);
	// notify, that the thread started
	pSelf->m_Started = true;
	pSelf->m_BufferDoneCond.notify_all();
	while(!pSelf->m_Shutdown)
	{
		pSelf->m_BufferSwapCond.wait(Lock);
		if(pSelf->m_pBuffer)
		{
#ifdef CONF_PLATFORM_MACOS
			CAutoreleasePool AutoreleasePool;
#endif
			pSelf->m_pProcessor->RunBuffer(pSelf->m_pBuffer);

			pSelf->m_pBuffer = nullptr;
			pSelf->m_BufferInProcess.store(false, std::memory_order_relaxed);
			pSelf->m_BufferDoneCond.notify_all();

#if defined(CONF_VIDEORECORDER)
			if(IVideo::Current())
				IVideo::Current()->NextVideoFrameThread();
#endif
		}
	}
}

CGraphicsBackend_Threaded::CGraphicsBackend_Threaded()
{
	m_pBuffer = nullptr;
	m_pProcessor = nullptr;
	m_BufferInProcess.store(false, std::memory_order_relaxed);
}

void CGraphicsBackend_Threaded::StartProcessor(ICommandProcessor *pProcessor)
{
	m_Shutdown = false;
	m_pProcessor = pProcessor;
	std::unique_lock<std::mutex> Lock(m_BufferSwapMutex);
	m_Thread = thread_init(ThreadFunc, this, "Graphics thread");
	// wait for the thread to start
	m_BufferDoneCond.wait(Lock, [this]() -> bool { return m_Started; });
}

void CGraphicsBackend_Threaded::StopProcessor()
{
	m_Shutdown = true;
	{
		std::unique_lock<std::mutex> Lock(m_BufferSwapMutex);
		m_BufferSwapCond.notify_all();
	}
	thread_wait(m_Thread);
}

void CGraphicsBackend_Threaded::RunBuffer(CCommandBuffer *pBuffer)
{
	WaitForIdle();
	std::unique_lock<std::mutex> Lock(m_BufferSwapMutex);
	m_pBuffer = pBuffer;
	m_BufferInProcess.store(true, std::memory_order_relaxed);
	m_BufferSwapCond.notify_all();
}

bool CGraphicsBackend_Threaded::IsIdle() const
{
	return !m_BufferInProcess.load(std::memory_order_relaxed);
}

void CGraphicsBackend_Threaded::WaitForIdle()
{
	std::unique_lock<std::mutex> Lock(m_BufferSwapMutex);
	while(m_pBuffer != nullptr)
		m_BufferDoneCond.wait(Lock);
}

// ------------ CCommandProcessorFragment_General

void CCommandProcessorFragment_General::Cmd_Signal(const CCommandBuffer::SCommand_Signal *pCommand)
{
	pCommand->m_pSemaphore->Signal();
}

bool CCommandProcessorFragment_General::RunCommand(const CCommandBuffer::SCommand *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CCommandBuffer::CMD_NOP: break;
	case CCommandBuffer::CMD_SIGNAL: Cmd_Signal(static_cast<const CCommandBuffer::SCommand_Signal *>(pBaseCommand)); break;
	default: return false;
	}

	return true;
}

// ------------ CCommandProcessorFragment_SDL
void CCommandProcessorFragment_SDL::Cmd_Init(const SCommand_Init *pCommand)
{
	m_GLContext = pCommand->m_GLContext;
	m_pWindow = pCommand->m_pWindow;
	SDL_GL_MakeCurrent(m_pWindow, m_GLContext);
}

void CCommandProcessorFragment_SDL::Cmd_Shutdown(const SCommand_Shutdown *pCommand)
{
	SDL_GL_MakeCurrent(NULL, NULL);
}

void CCommandProcessorFragment_SDL::Cmd_Swap(const CCommandBuffer::SCommand_Swap *pCommand)
{
	SDL_GL_SwapWindow(m_pWindow);
}

void CCommandProcessorFragment_SDL::Cmd_VSync(const CCommandBuffer::SCommand_VSync *pCommand)
{
	*pCommand->m_pRetOk = SDL_GL_SetSwapInterval(pCommand->m_VSync) == 0;
}

void CCommandProcessorFragment_SDL::Cmd_WindowCreateNtf(const CCommandBuffer::SCommand_WindowCreateNtf *pCommand)
{
	m_pWindow = SDL_GetWindowFromID(pCommand->m_WindowID);
	// Android destroys windows when they are not visible, so we get the new one and work with that
	// The graphic context does not need to be recreated, just unbound see @see SCommand_WindowDestroyNtf
#ifdef CONF_PLATFORM_ANDROID
	SDL_GL_MakeCurrent(m_pWindow, m_GLContext);
	dbg_msg("gfx", "render surface created.");
#endif
}

void CCommandProcessorFragment_SDL::Cmd_WindowDestroyNtf(const CCommandBuffer::SCommand_WindowDestroyNtf *pCommand)
{
	// Unbind the graphic context from the window, so it does not get destroyed
#ifdef CONF_PLATFORM_ANDROID
	dbg_msg("gfx", "render surface destroyed.");
	SDL_GL_MakeCurrent(NULL, NULL);
#endif
}

CCommandProcessorFragment_SDL::CCommandProcessorFragment_SDL() = default;

bool CCommandProcessorFragment_SDL::RunCommand(const CCommandBuffer::SCommand *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CCommandBuffer::CMD_WINDOW_CREATE_NTF: Cmd_WindowCreateNtf(static_cast<const CCommandBuffer::SCommand_WindowCreateNtf *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_WINDOW_DESTROY_NTF: Cmd_WindowDestroyNtf(static_cast<const CCommandBuffer::SCommand_WindowDestroyNtf *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_SWAP: Cmd_Swap(static_cast<const CCommandBuffer::SCommand_Swap *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_VSYNC: Cmd_VSync(static_cast<const CCommandBuffer::SCommand_VSync *>(pBaseCommand)); break;
	case CMD_INIT: Cmd_Init(static_cast<const SCommand_Init *>(pBaseCommand)); break;
	case CMD_SHUTDOWN: Cmd_Shutdown(static_cast<const SCommand_Shutdown *>(pBaseCommand)); break;
	default: return false;
	}

	return true;
}

// ------------ CCommandProcessor_SDL_OpenGL

void CCommandProcessor_SDL_OpenGL::RunBuffer(CCommandBuffer *pBuffer)
{
	for(CCommandBuffer::SCommand *pCommand = pBuffer->Head(); pCommand; pCommand = pCommand->m_pNext)
	{
		if(m_pOpenGL->RunCommand(pCommand))
			continue;

		if(m_SDL.RunCommand(pCommand))
			continue;

		if(m_General.RunCommand(pCommand))
			continue;

		dbg_msg("gfx", "unknown command %d", pCommand->m_Cmd);
	}
}

CCommandProcessor_SDL_OpenGL::CCommandProcessor_SDL_OpenGL(EBackendType BackendType, int OpenGLMajor, int OpenGLMinor, int OpenGLPatch)
{
	m_BackendType = BackendType;

	if(BackendType == BACKEND_TYPE_OPENGL_ES)
	{
#if defined(CONF_BACKEND_OPENGL_ES) || defined(CONF_BACKEND_OPENGL_ES3)
		if(OpenGLMajor < 3)
		{
			m_pOpenGL = new CCommandProcessorFragment_OpenGLES();
		}
		else
		{
			m_pOpenGL = new CCommandProcessorFragment_OpenGLES3();
		}
#endif
	}
	else if(BackendType == BACKEND_TYPE_OPENGL)
	{
#if !defined(CONF_BACKEND_OPENGL_ES)
		if(OpenGLMajor < 2)
		{
			m_pOpenGL = new CCommandProcessorFragment_OpenGL();
		}
		if(OpenGLMajor == 2)
		{
			m_pOpenGL = new CCommandProcessorFragment_OpenGL2();
		}
		if(OpenGLMajor == 3 && OpenGLMinor == 0)
		{
			m_pOpenGL = new CCommandProcessorFragment_OpenGL3();
		}
		else if((OpenGLMajor == 3 && OpenGLMinor == 3) || OpenGLMajor >= 4)
		{
			m_pOpenGL = new CCommandProcessorFragment_OpenGL3_3();
		}
#endif
	}
}

CCommandProcessor_SDL_OpenGL::~CCommandProcessor_SDL_OpenGL()
{
	delete m_pOpenGL;
}

// ------------ CGraphicsBackend_SDL_OpenGL

static bool BackendInitGlew(EBackendType BackendType, int &GlewMajor, int &GlewMinor, int &GlewPatch)
{
	if(BackendType == BACKEND_TYPE_OPENGL)
	{
#ifndef CONF_BACKEND_OPENGL_ES
		//support graphic cards that are pretty old(and linux)
		glewExperimental = GL_TRUE;
#ifdef CONF_GLEW_HAS_CONTEXT_INIT
		if(GLEW_OK != glewContextInit())
#else
		if(GLEW_OK != glewInit())
#endif
			return false;

#ifdef GLEW_VERSION_4_6
		if(GLEW_VERSION_4_6)
		{
			GlewMajor = 4;
			GlewMinor = 6;
			GlewPatch = 0;
			return true;
		}
#endif
#ifdef GLEW_VERSION_4_5
		if(GLEW_VERSION_4_5)
		{
			GlewMajor = 4;
			GlewMinor = 5;
			GlewPatch = 0;
			return true;
		}
#endif
// Don't allow GL 3.3, if the driver doesn't support atleast OpenGL 4.5
#ifndef CONF_FAMILY_WINDOWS
		if(GLEW_VERSION_4_4)
		{
			GlewMajor = 4;
			GlewMinor = 4;
			GlewPatch = 0;
			return true;
		}
		if(GLEW_VERSION_4_3)
		{
			GlewMajor = 4;
			GlewMinor = 3;
			GlewPatch = 0;
			return true;
		}
		if(GLEW_VERSION_4_2)
		{
			GlewMajor = 4;
			GlewMinor = 2;
			GlewPatch = 0;
			return true;
		}
		if(GLEW_VERSION_4_1)
		{
			GlewMajor = 4;
			GlewMinor = 1;
			GlewPatch = 0;
			return true;
		}
		if(GLEW_VERSION_4_0)
		{
			GlewMajor = 4;
			GlewMinor = 0;
			GlewPatch = 0;
			return true;
		}
		if(GLEW_VERSION_3_3)
		{
			GlewMajor = 3;
			GlewMinor = 3;
			GlewPatch = 0;
			return true;
		}
#endif
		if(GLEW_VERSION_3_0)
		{
			GlewMajor = 3;
			GlewMinor = 0;
			GlewPatch = 0;
			return true;
		}
		if(GLEW_VERSION_2_1)
		{
			GlewMajor = 2;
			GlewMinor = 1;
			GlewPatch = 0;
			return true;
		}
		if(GLEW_VERSION_2_0)
		{
			GlewMajor = 2;
			GlewMinor = 0;
			GlewPatch = 0;
			return true;
		}
		if(GLEW_VERSION_1_5)
		{
			GlewMajor = 1;
			GlewMinor = 5;
			GlewPatch = 0;
			return true;
		}
		if(GLEW_VERSION_1_4)
		{
			GlewMajor = 1;
			GlewMinor = 4;
			GlewPatch = 0;
			return true;
		}
		if(GLEW_VERSION_1_3)
		{
			GlewMajor = 1;
			GlewMinor = 3;
			GlewPatch = 0;
			return true;
		}
		if(GLEW_VERSION_1_2_1)
		{
			GlewMajor = 1;
			GlewMinor = 2;
			GlewPatch = 1;
			return true;
		}
		if(GLEW_VERSION_1_2)
		{
			GlewMajor = 1;
			GlewMinor = 2;
			GlewPatch = 0;
			return true;
		}
		if(GLEW_VERSION_1_1)
		{
			GlewMajor = 1;
			GlewMinor = 1;
			GlewPatch = 0;
			return true;
		}
#endif
	}
	else if(BackendType == BACKEND_TYPE_OPENGL_ES)
	{
		// just assume the version we need
		GlewMajor = 3;
		GlewMinor = 0;
		GlewPatch = 0;

		return true;
	}

	return true;
}

static int IsVersionSupportedGlew(EBackendType BackendType, int VersionMajor, int VersionMinor, int VersionPatch, int GlewMajor, int GlewMinor, int GlewPatch)
{
	int InitError = 0;

	if(BackendType == BACKEND_TYPE_OPENGL)
	{
		if(VersionMajor >= 4 && GlewMajor < 4)
		{
			InitError = -1;
		}
		else if(VersionMajor >= 3 && GlewMajor < 3)
		{
			InitError = -1;
		}
		else if(VersionMajor == 3 && GlewMajor == 3)
		{
			if(VersionMinor >= 3 && GlewMinor < 3)
			{
				InitError = -1;
			}
			if(VersionMinor >= 2 && GlewMinor < 2)
			{
				InitError = -1;
			}
			if(VersionMinor >= 1 && GlewMinor < 1)
			{
				InitError = -1;
			}
			if(VersionMinor >= 0 && GlewMinor < 0)
			{
				InitError = -1;
			}
		}
		else if(VersionMajor >= 2 && GlewMajor < 2)
		{
			InitError = -1;
		}
		else if(VersionMajor == 2 && GlewMajor == 2)
		{
			if(VersionMinor >= 1 && GlewMinor < 1)
			{
				InitError = -1;
			}
			if(VersionMinor >= 0 && GlewMinor < 0)
			{
				InitError = -1;
			}
		}
		else if(VersionMajor >= 1 && GlewMajor < 1)
		{
			InitError = -1;
		}
		else if(VersionMajor == 1 && GlewMajor == 1)
		{
			if(VersionMinor >= 5 && GlewMinor < 5)
			{
				InitError = -1;
			}
			if(VersionMinor >= 4 && GlewMinor < 4)
			{
				InitError = -1;
			}
			if(VersionMinor >= 3 && GlewMinor < 3)
			{
				InitError = -1;
			}
			if(VersionMinor >= 2 && GlewMinor < 2)
			{
				InitError = -1;
			}
			else if(VersionMinor == 2 && GlewMinor == 2)
			{
				if(VersionPatch >= 1 && GlewPatch < 1)
				{
					InitError = -1;
				}
				if(VersionPatch >= 0 && GlewPatch < 0)
				{
					InitError = -1;
				}
			}
			if(VersionMinor >= 1 && GlewMinor < 1)
			{
				InitError = -1;
			}
			if(VersionMinor >= 0 && GlewMinor < 0)
			{
				InitError = -1;
			}
		}
	}

	return InitError;
}

EBackendType CGraphicsBackend_SDL_OpenGL::DetectBackend()
{
#ifndef CONF_BACKEND_OPENGL_ES
#ifdef CONF_BACKEND_OPENGL_ES3
	const char *pEnvDriver = getenv("DDNET_DRIVER");
	if(pEnvDriver && str_comp(pEnvDriver, "GLES") == 0)
		return BACKEND_TYPE_OPENGL_ES;
	else
		return BACKEND_TYPE_OPENGL;
#else
	return BACKEND_TYPE_OPENGL;
#endif
#else
	return BACKEND_TYPE_OPENGL_ES;
#endif
}

void CGraphicsBackend_SDL_OpenGL::ClampDriverVersion(EBackendType BackendType)
{
	if(BackendType == BACKEND_TYPE_OPENGL)
	{
		//clamp the versions to existing versions(only for OpenGL major <= 3)
		if(g_Config.m_GfxOpenGLMajor == 1)
		{
			g_Config.m_GfxOpenGLMinor = clamp(g_Config.m_GfxOpenGLMinor, 1, 5);
			if(g_Config.m_GfxOpenGLMinor == 2)
				g_Config.m_GfxOpenGLPatch = clamp(g_Config.m_GfxOpenGLPatch, 0, 1);
			else
				g_Config.m_GfxOpenGLPatch = 0;
		}
		else if(g_Config.m_GfxOpenGLMajor == 2)
		{
			g_Config.m_GfxOpenGLMinor = clamp(g_Config.m_GfxOpenGLMinor, 0, 1);
			g_Config.m_GfxOpenGLPatch = 0;
		}
		else if(g_Config.m_GfxOpenGLMajor == 3)
		{
			g_Config.m_GfxOpenGLMinor = clamp(g_Config.m_GfxOpenGLMinor, 0, 3);
			if(g_Config.m_GfxOpenGLMinor < 3)
				g_Config.m_GfxOpenGLMinor = 0;
			g_Config.m_GfxOpenGLPatch = 0;
		}
	}
	else if(BackendType == BACKEND_TYPE_OPENGL_ES)
	{
#if !defined(CONF_BACKEND_OPENGL_ES3)
		// Make sure GLES is set to 1.0 (which is equivalent to OpenGL 1.3), if its not set to >= 3.0(which is equivalent to OpenGL 3.3)
		if(g_Config.m_GfxOpenGLMajor < 3)
		{
			g_Config.m_GfxOpenGLMajor = 1;
			g_Config.m_GfxOpenGLMinor = 0;
			g_Config.m_GfxOpenGLPatch = 0;

			// GLES also doesnt know GL_QUAD
			g_Config.m_GfxQuadAsTriangle = 1;
		}
#else
		g_Config.m_GfxOpenGLMajor = 3;
		g_Config.m_GfxOpenGLMinor = 0;
		g_Config.m_GfxOpenGLPatch = 0;
#endif
	}
}

bool CGraphicsBackend_SDL_OpenGL::IsModernAPI(EBackendType BackendType)
{
	if(BackendType == BACKEND_TYPE_OPENGL)
		return (g_Config.m_GfxOpenGLMajor == 3 && g_Config.m_GfxOpenGLMinor == 3) || g_Config.m_GfxOpenGLMajor >= 4;
	else if(BackendType == BACKEND_TYPE_OPENGL_ES)
		return g_Config.m_GfxOpenGLMajor >= 3;

	return false;
}

void CGraphicsBackend_SDL_OpenGL::GetDriverVersion(EGraphicsDriverAgeType DriverAgeType, int &Major, int &Minor, int &Patch)
{
	if(m_BackendType == BACKEND_TYPE_OPENGL)
	{
		if(DriverAgeType == GRAPHICS_DRIVER_AGE_TYPE_LEGACY)
		{
			Major = 1;
			Minor = 4;
			Patch = 0;
		}
		else if(DriverAgeType == GRAPHICS_DRIVER_AGE_TYPE_DEFAULT)
		{
			Major = 3;
			Minor = 0;
			Patch = 0;
		}
		else if(DriverAgeType == GRAPHICS_DRIVER_AGE_TYPE_MODERN)
		{
			Major = 3;
			Minor = 3;
			Patch = 0;
		}
	}
	else if(m_BackendType == BACKEND_TYPE_OPENGL_ES)
	{
		if(DriverAgeType == GRAPHICS_DRIVER_AGE_TYPE_LEGACY)
		{
			Major = 1;
			Minor = 0;
			Patch = 0;
		}
		else if(DriverAgeType == GRAPHICS_DRIVER_AGE_TYPE_DEFAULT)
		{
			Major = 3;
			Minor = 0;
			Patch = 0;
		}
		else if(DriverAgeType == GRAPHICS_DRIVER_AGE_TYPE_MODERN)
		{
			Major = 3;
			Minor = 0;
			Patch = 0;
		}
	}
}

static void DisplayToVideoMode(CVideoMode *pVMode, SDL_DisplayMode *pMode, int HiDPIScale, int RefreshRate)
{
	pVMode->m_CanvasWidth = pMode->w * HiDPIScale;
	pVMode->m_CanvasHeight = pMode->h * HiDPIScale;
	pVMode->m_WindowWidth = pMode->w;
	pVMode->m_WindowHeight = pMode->h;
	pVMode->m_RefreshRate = RefreshRate;
	pVMode->m_Red = SDL_BITSPERPIXEL(pMode->format);
	pVMode->m_Green = SDL_BITSPERPIXEL(pMode->format);
	pVMode->m_Blue = SDL_BITSPERPIXEL(pMode->format);
	pVMode->m_Format = pMode->format;
}

void CGraphicsBackend_SDL_OpenGL::GetVideoModes(CVideoMode *pModes, int MaxModes, int *pNumModes, int HiDPIScale, int MaxWindowWidth, int MaxWindowHeight, int Screen)
{
	SDL_DisplayMode DesktopMode;
	int maxModes = SDL_GetNumDisplayModes(Screen);
	int numModes = 0;

	// Only collect fullscreen modes when requested, that makes sure in windowed mode no refresh rates are shown that aren't supported without
	// fullscreen anyway(except fullscreen desktop)
	bool IsFullscreenDestkop = m_pWindow != NULL && (((SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP) || g_Config.m_GfxFullscreen == 3);
	bool CollectFullscreenModes = m_pWindow == NULL || ((SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_FULLSCREEN) != 0 && !IsFullscreenDestkop);

	if(SDL_GetDesktopDisplayMode(Screen, &DesktopMode) < 0)
	{
		dbg_msg("gfx", "unable to get display mode: %s", SDL_GetError());
	}

	static constexpr int ModeCount = 256;
	SDL_DisplayMode aModes[ModeCount];
	int NumModes = 0;
	for(int i = 0; i < maxModes && NumModes < ModeCount; i++)
	{
		SDL_DisplayMode mode;
		if(SDL_GetDisplayMode(Screen, i, &mode) < 0)
		{
			dbg_msg("gfx", "unable to get display mode: %s", SDL_GetError());
			continue;
		}

		aModes[NumModes] = mode;
		++NumModes;
	}

	auto &&ModeInsert = [&](SDL_DisplayMode &mode) {
		if(numModes < MaxModes)
		{
			// if last mode was equal, ignore this one --- in fullscreen this can really only happen if the screen
			// supports different color modes
			// in non fullscren these are the modes that show different refresh rate, but are basically the same
			if(numModes > 0 && pModes[numModes - 1].m_WindowWidth == mode.w && pModes[numModes - 1].m_WindowHeight == mode.h && (pModes[numModes - 1].m_RefreshRate == mode.refresh_rate || (mode.refresh_rate != DesktopMode.refresh_rate && !CollectFullscreenModes)))
				return;

			DisplayToVideoMode(&pModes[numModes], &mode, HiDPIScale, !CollectFullscreenModes ? DesktopMode.refresh_rate : mode.refresh_rate);
			numModes++;
		}
	};

	for(int i = 0; i < NumModes; i++)
	{
		SDL_DisplayMode &mode = aModes[i];

		if(mode.w > MaxWindowWidth || mode.h > MaxWindowHeight)
			continue;

		ModeInsert(mode);

		if(IsFullscreenDestkop)
			break;

		if(numModes >= MaxModes)
			break;
	}
	*pNumModes = numModes;
}

void CGraphicsBackend_SDL_OpenGL::GetCurrentVideoMode(CVideoMode &CurMode, int HiDPIScale, int MaxWindowWidth, int MaxWindowHeight, int Screen)
{
	SDL_DisplayMode DPMode;
	// if "real" fullscreen, obtain the video mode for that
	if((SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN)
	{
		if(SDL_GetCurrentDisplayMode(Screen, &DPMode))
		{
			dbg_msg("gfx", "unable to get display mode: %s", SDL_GetError());
		}
	}
	else
	{
		if(SDL_GetDesktopDisplayMode(Screen, &DPMode) < 0)
		{
			dbg_msg("gfx", "unable to get display mode: %s", SDL_GetError());
		}
		else
		{
			int Width = 0;
			int Height = 0;
			SDL_GL_GetDrawableSize(m_pWindow, &Width, &Height);
			DPMode.w = Width;
			DPMode.h = Height;
		}
	}
	DisplayToVideoMode(&CurMode, &DPMode, HiDPIScale, DPMode.refresh_rate);
}

CGraphicsBackend_SDL_OpenGL::CGraphicsBackend_SDL_OpenGL()
{
	mem_zero(m_aErrorString, sizeof(m_aErrorString) / sizeof(m_aErrorString[0]));
}

int CGraphicsBackend_SDL_OpenGL::Init(const char *pName, int *pScreen, int *pWidth, int *pHeight, int *pRefreshRate, int FsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight, int *pCurrentWidth, int *pCurrentHeight, IStorage *pStorage)
{
	// print sdl version
	{
		SDL_version Compiled;
		SDL_version Linked;

		SDL_VERSION(&Compiled);
		SDL_GetVersion(&Linked);
		dbg_msg("sdl", "SDL version %d.%d.%d (compiled = %d.%d.%d)", Linked.major, Linked.minor, Linked.patch,
			Compiled.major, Compiled.minor, Compiled.patch);
	}

	if(!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if(SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		{
			dbg_msg("gfx", "unable to init SDL video: %s", SDL_GetError());
			return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_SDL_INIT_FAILED;
		}
	}

	m_BackendType = DetectBackend();

	ClampDriverVersion(m_BackendType);

	m_UseNewOpenGL = IsModernAPI(m_BackendType);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, g_Config.m_GfxOpenGLMajor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, g_Config.m_GfxOpenGLMinor);
	dbg_msg("gfx", "Created OpenGL %d.%d context.", g_Config.m_GfxOpenGLMajor, g_Config.m_GfxOpenGLMinor);

	if(m_BackendType == BACKEND_TYPE_OPENGL)
	{
		if(g_Config.m_GfxOpenGLMajor == 3 && g_Config.m_GfxOpenGLMinor == 0)
		{
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
		}
		else if(m_UseNewOpenGL)
		{
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		}
	}
	else if(m_BackendType == BACKEND_TYPE_OPENGL_ES)
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	}

	// set screen
	SDL_Rect ScreenPos;
	m_NumScreens = SDL_GetNumVideoDisplays();
	if(m_NumScreens > 0)
	{
		*pScreen = clamp(*pScreen, 0, m_NumScreens - 1);
		if(SDL_GetDisplayBounds(*pScreen, &ScreenPos) != 0)
		{
			dbg_msg("gfx", "unable to retrieve screen information: %s", SDL_GetError());
			return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_SDL_SCREEN_INFO_REQUEST_FAILED;
		}
	}
	else
	{
		dbg_msg("gfx", "unable to retrieve number of screens: %s", SDL_GetError());
		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_SDL_SCREEN_REQUEST_FAILED;
	}

	// store desktop resolution for settings reset button
	SDL_DisplayMode DisplayMode;
	if(SDL_GetDesktopDisplayMode(*pScreen, &DisplayMode))
	{
		dbg_msg("gfx", "unable to get desktop resolution: %s", SDL_GetError());
		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_SDL_SCREEN_RESOLUTION_REQUEST_FAILED;
	}

	bool IsDesktopChanged = *pDesktopWidth == 0 || *pDesktopHeight == 0 || *pDesktopWidth != DisplayMode.w || *pDesktopHeight != DisplayMode.h;

	*pDesktopWidth = DisplayMode.w;
	*pDesktopHeight = DisplayMode.h;

	// fetch supported video modes
	bool SupportedResolution = false;

	CVideoMode aModes[256];
	int ModesCount = 0;
	int IndexOfResolution = -1;
	GetVideoModes(aModes, sizeof(aModes) / sizeof(aModes[0]), &ModesCount, 1, *pDesktopWidth, *pDesktopHeight, *pScreen);

	for(int i = 0; i < ModesCount; i++)
	{
		if(*pWidth == aModes[i].m_WindowWidth && *pHeight == aModes[i].m_WindowHeight && (*pRefreshRate == aModes[i].m_RefreshRate || *pRefreshRate == 0))
		{
			SupportedResolution = true;
			IndexOfResolution = i;
			break;
		}
	}

	// set flags
	int SdlFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_INPUT_GRABBED | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS;
	if(Flags & IGraphicsBackend::INITFLAG_HIGHDPI)
		SdlFlags |= SDL_WINDOW_ALLOW_HIGHDPI;
	if(Flags & IGraphicsBackend::INITFLAG_RESIZABLE)
		SdlFlags |= SDL_WINDOW_RESIZABLE;
	if(Flags & IGraphicsBackend::INITFLAG_BORDERLESS)
		SdlFlags |= SDL_WINDOW_BORDERLESS;
	if(Flags & IGraphicsBackend::INITFLAG_FULLSCREEN)
		SdlFlags |= SDL_WINDOW_FULLSCREEN;
	else if(Flags & (IGraphicsBackend::INITFLAG_DESKTOP_FULLSCREEN))
		SdlFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

	bool IsFullscreen = (SdlFlags & SDL_WINDOW_FULLSCREEN) != 0 || g_Config.m_GfxFullscreen == 3;
	// use desktop resolution as default resolution, clamp resolution if users's display is smaller than we remembered
	// if the user starts in fullscreen, and the resolution was not found use the desktop one
	if((IsFullscreen && !SupportedResolution) || *pWidth == 0 || *pHeight == 0 || (IsDesktopChanged && (*pWidth > *pDesktopWidth || *pHeight > *pDesktopHeight)))
	{
		*pWidth = *pDesktopWidth;
		*pHeight = *pDesktopHeight;
		*pRefreshRate = DisplayMode.refresh_rate;
	}

	// if in fullscreen and refresh rate wasn't set yet, just use the one from the found list
	if(*pRefreshRate == 0 && SupportedResolution)
	{
		*pRefreshRate = aModes[IndexOfResolution].m_RefreshRate;
	}
	else if(*pRefreshRate == 0)
	{
		*pRefreshRate = DisplayMode.refresh_rate;
	}

	// set gl attributes
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	if(FsaaSamples)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, FsaaSamples);
	}
	else
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	}

	if(g_Config.m_InpMouseOld)
		SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");

	m_pWindow = SDL_CreateWindow(
		pName,
		SDL_WINDOWPOS_CENTERED_DISPLAY(*pScreen),
		SDL_WINDOWPOS_CENTERED_DISPLAY(*pScreen),
		*pWidth,
		*pHeight,
		SdlFlags);

	// set caption
	if(m_pWindow == NULL)
	{
		dbg_msg("gfx", "unable to create window: %s", SDL_GetError());
		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_SDL_WINDOW_CREATE_FAILED;
	}

	m_GLContext = SDL_GL_CreateContext(m_pWindow);

	if(m_GLContext == NULL)
	{
		SDL_DestroyWindow(m_pWindow);
		dbg_msg("gfx", "unable to create OpenGL context: %s", SDL_GetError());
		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_OPENGL_CONTEXT_FAILED;
	}

	int GlewMajor = 0;
	int GlewMinor = 0;
	int GlewPatch = 0;

	if(!BackendInitGlew(m_BackendType, GlewMajor, GlewMinor, GlewPatch))
	{
		SDL_GL_DeleteContext(m_GLContext);
		SDL_DestroyWindow(m_pWindow);
		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_UNKNOWN;
	}

	int InitError = 0;
	const char *pErrorStr = NULL;

	InitError = IsVersionSupportedGlew(m_BackendType, g_Config.m_GfxOpenGLMajor, g_Config.m_GfxOpenGLMinor, g_Config.m_GfxOpenGLPatch, GlewMajor, GlewMinor, GlewPatch);

	// SDL_GL_GetDrawableSize reports HiDPI resolution even with SDL_WINDOW_ALLOW_HIGHDPI not set, which is wrong
	if(SdlFlags & SDL_WINDOW_ALLOW_HIGHDPI)
		SDL_GL_GetDrawableSize(m_pWindow, pCurrentWidth, pCurrentHeight);
	else
		SDL_GetWindowSize(m_pWindow, pCurrentWidth, pCurrentHeight);

	SDL_GL_SetSwapInterval(Flags & IGraphicsBackend::INITFLAG_VSYNC ? 1 : 0);
	SDL_GL_MakeCurrent(NULL, NULL);

	if(InitError != 0)
	{
		SDL_GL_DeleteContext(m_GLContext);
		SDL_DestroyWindow(m_pWindow);

		// try setting to glew supported version
		g_Config.m_GfxOpenGLMajor = GlewMajor;
		g_Config.m_GfxOpenGLMinor = GlewMinor;
		g_Config.m_GfxOpenGLPatch = GlewPatch;

		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_OPENGL_VERSION_FAILED;
	}

	// start the command processor
	m_pProcessor = new CCommandProcessor_SDL_OpenGL(m_BackendType, g_Config.m_GfxOpenGLMajor, g_Config.m_GfxOpenGLMinor, g_Config.m_GfxOpenGLPatch);
	StartProcessor(m_pProcessor);

	// issue init commands for OpenGL and SDL
	CCommandBuffer CmdBuffer(1024, 512);
	//run sdl first to have the context in the thread
	CCommandProcessorFragment_SDL::SCommand_Init CmdSDL;
	CmdSDL.m_pWindow = m_pWindow;
	CmdSDL.m_GLContext = m_GLContext;
	CmdBuffer.AddCommandUnsafe(CmdSDL);
	RunBuffer(&CmdBuffer);
	WaitForIdle();
	CmdBuffer.Reset();

	if(InitError == 0)
	{
		CCommandProcessorFragment_OpenGLBase::SCommand_Init CmdOpenGL;
		CmdOpenGL.m_pTextureMemoryUsage = &m_TextureMemoryUsage;
		CmdOpenGL.m_pStorage = pStorage;
		CmdOpenGL.m_pCapabilities = &m_Capabilites;
		CmdOpenGL.m_pInitError = &InitError;
		CmdOpenGL.m_RequestedMajor = g_Config.m_GfxOpenGLMajor;
		CmdOpenGL.m_RequestedMinor = g_Config.m_GfxOpenGLMinor;
		CmdOpenGL.m_RequestedPatch = g_Config.m_GfxOpenGLPatch;
		CmdOpenGL.m_GlewMajor = GlewMajor;
		CmdOpenGL.m_GlewMinor = GlewMinor;
		CmdOpenGL.m_GlewPatch = GlewPatch;
		CmdOpenGL.m_pErrStringPtr = &pErrorStr;
		CmdOpenGL.m_pVendorString = m_aVendorString;
		CmdOpenGL.m_pVersionString = m_aVersionString;
		CmdOpenGL.m_pRendererString = m_aRendererString;
		CmdOpenGL.m_RequestedBackend = m_BackendType;
		CmdBuffer.AddCommandUnsafe(CmdOpenGL);

		RunBuffer(&CmdBuffer);
		WaitForIdle();
		CmdBuffer.Reset();
	}

	if(InitError != 0)
	{
		if(InitError != -2)
		{
			// shutdown the context, as it might have been initialized
			CCommandProcessorFragment_OpenGLBase::SCommand_Shutdown CmdGL;
			CmdBuffer.AddCommandUnsafe(CmdGL);
			RunBuffer(&CmdBuffer);
			WaitForIdle();
			CmdBuffer.Reset();
		}

		CCommandProcessorFragment_SDL::SCommand_Shutdown Cmd;
		CmdBuffer.AddCommandUnsafe(Cmd);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
		CmdBuffer.Reset();

		// stop and delete the processor
		StopProcessor();
		delete m_pProcessor;
		m_pProcessor = 0;

		SDL_GL_DeleteContext(m_GLContext);
		SDL_DestroyWindow(m_pWindow);

		// try setting to version string's supported version
		if(InitError == -2)
		{
			g_Config.m_GfxOpenGLMajor = m_Capabilites.m_ContextMajor;
			g_Config.m_GfxOpenGLMinor = m_Capabilites.m_ContextMinor;
			g_Config.m_GfxOpenGLPatch = m_Capabilites.m_ContextPatch;
		}

		if(pErrorStr != NULL)
		{
			str_copy(m_aErrorString, pErrorStr, sizeof(m_aErrorString) / sizeof(m_aErrorString[0]));
		}

		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_OPENGL_VERSION_FAILED;
	}

	{
		CCommandBuffer::SCommand_Update_Viewport CmdSDL;
		CmdSDL.m_X = 0;
		CmdSDL.m_Y = 0;

		CmdSDL.m_Width = *pCurrentWidth;
		CmdSDL.m_Height = *pCurrentHeight;
		CmdBuffer.AddCommandUnsafe(CmdSDL);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
		CmdBuffer.Reset();
	}

	// return
	return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_NONE;
}

int CGraphicsBackend_SDL_OpenGL::Shutdown()
{
	// issue a shutdown command
	CCommandBuffer CmdBuffer(1024, 512);
	CCommandProcessorFragment_OpenGLBase::SCommand_Shutdown CmdGL;
	CmdBuffer.AddCommandUnsafe(CmdGL);
	RunBuffer(&CmdBuffer);
	WaitForIdle();
	CmdBuffer.Reset();

	CCommandProcessorFragment_SDL::SCommand_Shutdown Cmd;
	CmdBuffer.AddCommandUnsafe(Cmd);
	RunBuffer(&CmdBuffer);
	WaitForIdle();
	CmdBuffer.Reset();

	// stop and delete the processor
	StopProcessor();
	delete m_pProcessor;
	m_pProcessor = 0;

	SDL_GL_DeleteContext(m_GLContext);
	SDL_DestroyWindow(m_pWindow);

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	return 0;
}

int CGraphicsBackend_SDL_OpenGL::MemoryUsage() const
{
	return m_TextureMemoryUsage;
}

void CGraphicsBackend_SDL_OpenGL::Minimize()
{
	SDL_MinimizeWindow(m_pWindow);
}

void CGraphicsBackend_SDL_OpenGL::Maximize()
{
	// TODO: SDL
}

void CGraphicsBackend_SDL_OpenGL::SetWindowParams(int FullscreenMode, bool IsBorderless, bool AllowResizing)
{
	if(FullscreenMode > 0)
	{
		bool IsDesktopFullscreen = FullscreenMode == 2;
#ifndef CONF_FAMILY_WINDOWS
		//  special mode for windows only
		IsDesktopFullscreen |= FullscreenMode == 3;
#endif
		if(FullscreenMode == 1)
		{
#if defined(CONF_PLATFORM_MACOS) || defined(CONF_PLATFORM_HAIKU)
			// Todo SDL: remove this when fixed (game freezes when losing focus in fullscreen)
			SDL_SetWindowFullscreen(m_pWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
#else
			SDL_SetWindowFullscreen(m_pWindow, SDL_WINDOW_FULLSCREEN);
#endif
			SDL_SetWindowResizable(m_pWindow, SDL_TRUE);
		}
		else if(IsDesktopFullscreen)
		{
			SDL_SetWindowFullscreen(m_pWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
			SDL_SetWindowResizable(m_pWindow, SDL_TRUE);
		}
		else
		{
			SDL_SetWindowFullscreen(m_pWindow, 0);
			SDL_SetWindowBordered(m_pWindow, SDL_TRUE);
			SDL_SetWindowResizable(m_pWindow, SDL_FALSE);
			SDL_DisplayMode DPMode;
			if(SDL_GetDesktopDisplayMode(g_Config.m_GfxScreen, &DPMode) < 0)
			{
				dbg_msg("gfx", "unable to get display mode: %s", SDL_GetError());
			}
			else
			{
				ResizeWindow(DPMode.w, DPMode.h, DPMode.refresh_rate);
				SDL_SetWindowPosition(m_pWindow, SDL_WINDOWPOS_CENTERED_DISPLAY(g_Config.m_GfxScreen), SDL_WINDOWPOS_CENTERED_DISPLAY(g_Config.m_GfxScreen));
			}
		}
	}
	else
	{
		SDL_SetWindowFullscreen(m_pWindow, 0);
		SDL_SetWindowBordered(m_pWindow, SDL_bool(!IsBorderless));
		SDL_SetWindowResizable(m_pWindow, SDL_TRUE);
	}
}

bool CGraphicsBackend_SDL_OpenGL::SetWindowScreen(int Index)
{
	if(Index < 0 || Index >= m_NumScreens)
	{
		return false;
	}

	SDL_Rect ScreenPos;
	if(SDL_GetDisplayBounds(Index, &ScreenPos) != 0)
	{
		return false;
	}

	SDL_SetWindowPosition(m_pWindow,
		SDL_WINDOWPOS_CENTERED_DISPLAY(Index),
		SDL_WINDOWPOS_CENTERED_DISPLAY(Index));

	return UpdateDisplayMode(Index);
}

bool CGraphicsBackend_SDL_OpenGL::UpdateDisplayMode(int Index)
{
	SDL_DisplayMode DisplayMode;
	if(SDL_GetDesktopDisplayMode(Index, &DisplayMode) < 0)
	{
		dbg_msg("gfx", "unable to get display mode: %s", SDL_GetError());
		return false;
	}

	g_Config.m_GfxDesktopWidth = DisplayMode.w;
	g_Config.m_GfxDesktopHeight = DisplayMode.h;

	return true;
}

int CGraphicsBackend_SDL_OpenGL::GetWindowScreen()
{
	return SDL_GetWindowDisplayIndex(m_pWindow);
}

int CGraphicsBackend_SDL_OpenGL::WindowActive()
{
	return m_pWindow && SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_INPUT_FOCUS;
}

int CGraphicsBackend_SDL_OpenGL::WindowOpen()
{
	return m_pWindow && SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_SHOWN;
}

void CGraphicsBackend_SDL_OpenGL::SetWindowGrab(bool Grab)
{
	SDL_SetWindowGrab(m_pWindow, Grab ? SDL_TRUE : SDL_FALSE);
}

bool CGraphicsBackend_SDL_OpenGL::ResizeWindow(int w, int h, int RefreshRate)
{
	// don't call resize events when the window is at fullscreen desktop
	if(!m_pWindow || (SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP)
		return false;

	// if the window is at fullscreen use SDL_SetWindowDisplayMode instead, suggested by SDL
	if(SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_FULLSCREEN)
	{
#ifdef CONF_FAMILY_WINDOWS
		// in windows make the window windowed mode first, this prevents strange window glitches (other games probably do something similar)
		SetWindowParams(0, 1, true);
#endif
		SDL_DisplayMode SetMode = {};
		SDL_DisplayMode ClosestMode = {};
		SetMode.format = 0;
		SetMode.w = w;
		SetMode.h = h;
		SetMode.refresh_rate = RefreshRate;
		SDL_SetWindowDisplayMode(m_pWindow, SDL_GetClosestDisplayMode(g_Config.m_GfxScreen, &SetMode, &ClosestMode));
#ifdef CONF_FAMILY_WINDOWS
		// now change it back to fullscreen, this will restore the above set state, bcs SDL saves fullscreen modes appart from other video modes (as of SDL 2.0.16)
		// see implementation of SDL_SetWindowDisplayMode
		SetWindowParams(1, 0, true);
#endif
		return true;
	}
	else
	{
		SDL_SetWindowSize(m_pWindow, w, h);
		if(SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_MAXIMIZED)
			// remove maximize flag
			SDL_RestoreWindow(m_pWindow);
	}

	return false;
}

void CGraphicsBackend_SDL_OpenGL::GetViewportSize(int &w, int &h)
{
	SDL_GL_GetDrawableSize(m_pWindow, &w, &h);
}

void CGraphicsBackend_SDL_OpenGL::NotifyWindow()
{
#if SDL_MAJOR_VERSION > 2 || (SDL_MAJOR_VERSION == 2 && SDL_PATCHLEVEL >= 16)
	if(SDL_FlashWindow(m_pWindow, SDL_FlashOperation::SDL_FLASH_UNTIL_FOCUSED) != 0)
	{
		// fails if SDL hasn't implemented it
		return;
	}
#endif
}

void CGraphicsBackend_SDL_OpenGL::WindowDestroyNtf(uint32_t WindowID)
{
}

void CGraphicsBackend_SDL_OpenGL::WindowCreateNtf(uint32_t WindowID)
{
	m_pWindow = SDL_GetWindowFromID(WindowID);
}

IGraphicsBackend *CreateGraphicsBackend() { return new CGraphicsBackend_SDL_OpenGL; }
