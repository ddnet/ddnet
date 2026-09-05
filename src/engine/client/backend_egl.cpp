#ifdef BACKEND_NO_SDL

#include "backend_egl.h"

#include "backend/opengl/backend_opengl3.h"
#include "graphics_threaded.h"

#include <base/dbg.h>
#include <base/log.h>
#include <base/sphore.h>
#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <GL/glew.h>

// ------------ CCommandProcessorFragment_EGL

void CCommandProcessorFragment_EGL::Cmd_Init(const SCommand_Init *pCommand)
{
	m_Display = pCommand->m_Display;
	m_Context = pCommand->m_Context;
	m_Surface = pCommand->m_Surface;
	eglMakeCurrent(m_Display, m_Surface, m_Surface, m_Context);
}

void CCommandProcessorFragment_EGL::Cmd_Shutdown(const SCommand_Shutdown *pCommand)
{
	(void)pCommand;
	eglMakeCurrent(m_Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void CCommandProcessorFragment_EGL::Cmd_Swap(const CCommandBuffer::SCommand_Swap *pCommand)
{
	(void)pCommand;
	eglSwapBuffers(m_Display, m_Surface);
}

void CCommandProcessorFragment_EGL::Cmd_VSync(const CCommandBuffer::SCommand_VSync *pCommand)
{
	*pCommand->m_pRetOk = eglSwapInterval(m_Display, pCommand->m_VSync) == EGL_TRUE;
}

bool CCommandProcessorFragment_EGL::RunCommand(const CCommandBuffer::SCommand *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CCommandProcessorFragment_GLBase::CMD_PRE_INIT:
	case CCommandProcessorFragment_GLBase::CMD_POST_SHUTDOWN:
		break;
	case CCommandBuffer::CMD_SWAP:
		Cmd_Swap(static_cast<const CCommandBuffer::SCommand_Swap *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_VSYNC:
		Cmd_VSync(static_cast<const CCommandBuffer::SCommand_VSync *>(pBaseCommand));
		break;
	case CMD_INIT:
		Cmd_Init(static_cast<const SCommand_Init *>(pBaseCommand));
		break;
	case CMD_SHUTDOWN:
		Cmd_Shutdown(static_cast<const SCommand_Shutdown *>(pBaseCommand));
		break;
	default:
		return false;
	}
	return true;
}

// ------------ CCommandProcessor_EGL_GL

CCommandProcessor_EGL_GL::CCommandProcessor_EGL_GL(EBackendType BackendType, int GLMajor, int GLMinor, int GLPatch)
{
	m_BackendType = BackendType;

	if(BackendType == BACKEND_TYPE_OPENGL)
	{
		if(GLMajor < 2)
			m_pGLBackend = new CCommandProcessorFragment_OpenGL();
		else if(GLMajor == 2)
			m_pGLBackend = new CCommandProcessorFragment_OpenGL2();
		else if(GLMajor == 3 && GLMinor == 0)
			m_pGLBackend = new CCommandProcessorFragment_OpenGL3();
		else if((GLMajor == 3 && GLMinor == 3) || GLMajor >= 4)
			m_pGLBackend = new CCommandProcessorFragment_OpenGL3_3();
		else
			m_pGLBackend = new CCommandProcessorFragment_OpenGL3_3();
	}
	else
	{
		dbg_assert_failed("Unsupported backend type for EGL: %d", (int)BackendType);
		m_pGLBackend = nullptr;
	}
}

CCommandProcessor_EGL_GL::~CCommandProcessor_EGL_GL()
{
	delete m_pGLBackend;
}

void CCommandProcessor_EGL_GL::RunBuffer(CCommandBuffer *pBuffer)
{
	m_pGLBackend->StartCommands(pBuffer->m_CommandCount, pBuffer->m_RenderCallCount);

	for(const CCommandBuffer::SCommand *pCommand = pBuffer->Head(); pCommand; pCommand = pCommand->m_pNext)
	{
		auto Res = m_pGLBackend->RunCommand(pCommand);
		if(Res == ERunCommandReturnTypes::RUN_COMMAND_COMMAND_HANDLED)
			continue;
		else if(Res == ERunCommandReturnTypes::RUN_COMMAND_COMMAND_ERROR)
		{
			m_Error = m_pGLBackend->GetError();
			HandleError();
			return;
		}
		else if(Res == ERunCommandReturnTypes::RUN_COMMAND_COMMAND_WARNING)
		{
			m_Warning = m_pGLBackend->GetWarning();
			HandleWarning();
			return;
		}

		if(m_EGL.RunCommand(pCommand))
			continue;

		if(m_General.RunCommand(pCommand))
			continue;

		dbg_assert_failed("Unknown graphics command %d", pCommand->m_Cmd);
	}

	m_pGLBackend->EndCommands();
}

const SGfxErrorContainer &CCommandProcessor_EGL_GL::GetError() const
{
	return m_Error;
}

void CCommandProcessor_EGL_GL::ErroneousCleanup()
{
	m_pGLBackend->ErroneousCleanup();
}

const SGfxWarningContainer &CCommandProcessor_EGL_GL::GetWarning() const
{
	return m_Warning;
}

void CCommandProcessor_EGL_GL::HandleError()
{
	m_Error = m_pGLBackend->GetError();
}

void CCommandProcessor_EGL_GL::HandleWarning()
{
	m_Warning = m_pGLBackend->GetWarning();
}

// ------------ CGraphicsBackend_EGL

CGraphicsBackend_EGL::CGraphicsBackend_EGL(TTranslateFunc &&TranslateFunc) :
	CGraphicsBackend_Threaded(std::move(TranslateFunc))
{
}

static bool BackendInitGlew(int &GlewMajor, int &GlewMinor, int &GlewPatch)
{
	glewExperimental = GL_TRUE;
#ifdef CONF_GLEW_HAS_CONTEXT_INIT
	const GLenum InitResult = glewContextInit();
	if(InitResult != GLEW_OK)
	{
		log_error("gfx", "Unable to init glew (glewContextInit): %s", glewGetErrorString(InitResult));
		return false;
	}
#else
	const GLenum InitResult = glewInit();
	// There is no GLX display when rendering headlessly through EGL, in which case glewInit
	// has already initialized the context with glewContextInit internally.
	if(InitResult != GLEW_OK && InitResult != GLEW_ERROR_NO_GLX_DISPLAY)
	{
		log_error("gfx", "Unable to init glew (glewInit): %s", glewGetErrorString(InitResult));
		return false;
	}
#endif

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

	GlewMajor = 1;
	GlewMinor = 1;
	GlewPatch = 0;
	return true;
}

int CGraphicsBackend_EGL::InitOpenGL(int Width, int Height, int Flags, IStorage *pStorage)
{
	(void)Flags;

	// Initialize EGL
	m_Display = EGL_NO_DISPLAY;

	// Prefer a surfaceless EGL display when available (works on headless systems).
	// EGL 1.5
#if defined(EGL_VERSION_1_5) && defined(EGL_PLATFORM_SURFACELESS_MESA)
	m_Display = eglGetPlatformDisplay(
		EGL_PLATFORM_SURFACELESS_MESA,
		EGL_DEFAULT_DISPLAY,
		nullptr);
#endif

	// EGL_EXT_platform_base
#if defined(EGL_EXT_platform_base) && defined(EGL_PLATFORM_SURFACELESS_MESA)
	if(m_Display == EGL_NO_DISPLAY)
	{
		auto pfnGetPlatformDisplay =
			(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");

		if(pfnGetPlatformDisplay)
		{
			m_Display = pfnGetPlatformDisplay(
				EGL_PLATFORM_SURFACELESS_MESA,
				EGL_DEFAULT_DISPLAY,
				nullptr);
		}
	}
#endif

	// Fall back to the default display.
	if(m_Display == EGL_NO_DISPLAY)
		m_Display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	if(m_Display == EGL_NO_DISPLAY)
	{
		log_error("gfx/egl", "Failed to get EGL display");
		return -1;
	}

	EGLint Major, Minor;
	if(!eglInitialize(m_Display, &Major, &Minor))
	{
		log_error("gfx/egl", "Failed to initialize EGL");
		return -1;
	}
	log_info("gfx/egl", "EGL version: %d.%d", Major, Minor);

	// Choose config
	const EGLint aConfigAttribs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_STENCIL_SIZE, 8,
		EGL_NONE};

	EGLint NumConfigs;
	EGLConfig Config;
	if(!eglChooseConfig(m_Display, aConfigAttribs, &Config, 1, &NumConfigs) || NumConfigs == 0)
	{
		log_error("gfx/egl", "Failed to choose EGL config");
		eglTerminate(m_Display);
		m_Display = nullptr;
		return -1;
	}

	// Create Pbuffer surface
	const EGLint aSurfaceAttribs[] = {
		EGL_WIDTH, Width,
		EGL_HEIGHT, Height,
		EGL_NONE};

	m_Surface = eglCreatePbufferSurface(m_Display, Config, aSurfaceAttribs);
	if(m_Surface == EGL_NO_SURFACE)
	{
		log_error("gfx/egl", "Failed to create EGL Pbuffer surface");
		eglTerminate(m_Display);
		m_Display = nullptr;
		return -1;
	}

	// Create OpenGL context
	eglBindAPI(EGL_OPENGL_API);
	const EGLint aContextAttribs[] = {
		EGL_CONTEXT_MAJOR_VERSION, g_Config.m_GfxGLMajor,
		EGL_CONTEXT_MINOR_VERSION, g_Config.m_GfxGLMinor,
		EGL_NONE};

	m_Context = eglCreateContext(m_Display, Config, EGL_NO_CONTEXT, aContextAttribs);
	if(m_Context == EGL_NO_CONTEXT)
	{
		log_error("gfx/egl", "Failed to create EGL context (trying without version)");
		// Try without specific version
		const EGLint aFallbackAttribs[] = {EGL_NONE};
		m_Context = eglCreateContext(m_Display, Config, EGL_NO_CONTEXT, aFallbackAttribs);
		if(m_Context == EGL_NO_CONTEXT)
		{
			log_error("gfx/egl", "Failed to create EGL context");
			eglDestroySurface(m_Display, m_Surface);
			eglTerminate(m_Display);
			m_Display = nullptr;
			m_Surface = nullptr;
			return -1;
		}
	}

	// Make context current on the main thread for GLEW init
	if(!eglMakeCurrent(m_Display, m_Surface, m_Surface, m_Context))
	{
		log_error("gfx/egl", "Failed to make EGL context current");
		eglDestroyContext(m_Display, m_Context);
		eglDestroySurface(m_Display, m_Surface);
		eglTerminate(m_Display);
		m_Display = nullptr;
		m_Context = nullptr;
		m_Surface = nullptr;
		return -1;
	}

	int GlewMajor = 0, GlewMinor = 0, GlewPatch = 0;
	if(!BackendInitGlew(GlewMajor, GlewMinor, GlewPatch))
	{
		eglMakeCurrent(m_Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroyContext(m_Display, m_Context);
		eglDestroySurface(m_Display, m_Surface);
		eglTerminate(m_Display);
		m_Display = nullptr;
		m_Context = nullptr;
		m_Surface = nullptr;
		return -1;
	}

	// Release context from main thread so it can be picked up by render thread
	eglMakeCurrent(m_Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

	// Create command processor
	m_pProcessor = new CCommandProcessor_EGL_GL(BACKEND_TYPE_OPENGL, GlewMajor, GlewMinor, GlewPatch);
	StartProcessor(m_pProcessor);

	// EGL Init (makes context current in render thread)
	{
		CCommandProcessorFragment_EGL::SCommand_Init CmdEGL;
		CmdEGL.m_Display = m_Display;
		CmdEGL.m_Context = m_Context;
		CmdEGL.m_Surface = m_Surface;
		CCommandBuffer CmdBuffer(1024, 512);
		CmdBuffer.AddCommandUnsafe(CmdEGL);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
		CmdBuffer.Reset();
	}

	// Pre-Init
	{
		CCommandBuffer CmdBuffer(1024, 512);
		CCommandProcessorFragment_GLBase::SCommand_PreInit CmdPre;
		CmdPre.m_pWindow = nullptr;
		CmdPre.m_Width = Width;
		CmdPre.m_Height = Height;
		CmdPre.m_pVendorString = m_aVendorString;
		CmdPre.m_pVersionString = m_aVersionString;
		CmdPre.m_pRendererString = m_aRendererString;
		CmdPre.m_pGpuList = &m_GpuList;
		CmdBuffer.AddCommandUnsafe(CmdPre);
		RunBufferSingleThreadedUnsafe(&CmdBuffer);
		CmdBuffer.Reset();
	}

	const char *pErrorStr = nullptr;
	int InitError = 0;
	{
		CCommandProcessorFragment_GLBase::SCommand_Init CmdGL;
		CmdGL.m_pWindow = nullptr;
		CmdGL.m_Width = Width;
		CmdGL.m_Height = Height;
		CmdGL.m_pTextureMemoryUsage = &m_TextureMemoryUsage;
		CmdGL.m_pBufferMemoryUsage = &m_BufferMemoryUsage;
		CmdGL.m_pStreamMemoryUsage = &m_StreamMemoryUsage;
		CmdGL.m_pStagingMemoryUsage = &m_StagingMemoryUsage;
		CmdGL.m_pGpuList = &m_GpuList;
		CmdGL.m_pReadPresentedImageDataFunc = &m_ReadPresentedImageDataFunc;
		CmdGL.m_pStorage = pStorage;
		CmdGL.m_pCapabilities = &m_Capabilities;
		CmdGL.m_pInitError = &InitError;
		CmdGL.m_RequestedMajor = g_Config.m_GfxGLMajor;
		CmdGL.m_RequestedMinor = g_Config.m_GfxGLMinor;
		CmdGL.m_RequestedPatch = g_Config.m_GfxGLPatch;
		CmdGL.m_GlewMajor = GlewMajor;
		CmdGL.m_GlewMinor = GlewMinor;
		CmdGL.m_GlewPatch = GlewPatch;
		CmdGL.m_pErrStringPtr = &pErrorStr;
		CmdGL.m_pVendorString = m_aVendorString;
		CmdGL.m_pVersionString = m_aVersionString;
		CmdGL.m_pRendererString = m_aRendererString;
		CmdGL.m_RequestedBackend = BACKEND_TYPE_OPENGL;

		CCommandBuffer CmdBuffer(1024, 512);
		CmdBuffer.AddCommandUnsafe(CmdGL);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
		CmdBuffer.Reset();
	}

	if(InitError != 0)
	{
		CCommandBuffer CmdBuffer(1024, 512);
		if(InitError != -2)
		{
			// shutdown the context, as it might have been initialized
			CCommandProcessorFragment_GLBase::SCommand_Shutdown CmdShutdown;
			CmdBuffer.AddCommandUnsafe(CmdShutdown);
			RunBuffer(&CmdBuffer);
			WaitForIdle();
			CmdBuffer.Reset();
		}
		StopProcessor();
		delete m_pProcessor;
		m_pProcessor = nullptr;
		return -1;
	}

	// Viewport update
	{
		CCommandBuffer::SCommand_Update_Viewport CmdViewport;
		CmdViewport.m_X = 0;
		CmdViewport.m_Y = 0;
		CmdViewport.m_Width = Width;
		CmdViewport.m_Height = Height;
		CmdViewport.m_DrawableWidth = Width;
		CmdViewport.m_DrawableHeight = Height;
		CmdViewport.m_ByResize = true;
		CCommandBuffer CmdBuffer(1024, 512);
		CmdBuffer.AddCommandUnsafe(CmdViewport);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
		CmdBuffer.Reset();
	}

	if(pErrorStr != nullptr)
	{
		str_copy(m_aErrorString, pErrorStr);
	}

	return 0;
}

int CGraphicsBackend_EGL::Init(const char *pName, int *pScreen, int *pWidth, int *pHeight, int *pRefreshRate, int *pFsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight, int *pCurrentWidth, int *pCurrentHeight, IStorage *pStorage)
{
	(void)pName;
	(void)pScreen;
	(void)pRefreshRate;
	(void)pFsaaSamples;
	(void)pDesktopWidth;
	(void)pDesktopHeight;

	if(*pWidth <= 0 || *pHeight <= 0)
	{
		*pWidth = 640;
		*pHeight = 480;
	}

	*pCurrentWidth = *pWidth;
	*pCurrentHeight = *pHeight;

	return InitOpenGL(*pWidth, *pHeight, Flags, pStorage);
}

int CGraphicsBackend_EGL::Shutdown()
{
	if(m_pProcessor != nullptr)
	{
		CCommandBuffer CmdBuffer(1024, 512);

		CCommandProcessorFragment_GLBase::SCommand_Shutdown CmdGL;
		CmdBuffer.AddCommandUnsafe(CmdGL);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
		CmdBuffer.Reset();

		CCommandProcessorFragment_EGL::SCommand_Shutdown CmdEGL;
		CmdBuffer.AddCommandUnsafe(CmdEGL);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
		CmdBuffer.Reset();

		CCommandProcessorFragment_GLBase::SCommand_PostShutdown CmdPost;
		CmdBuffer.AddCommandUnsafe(CmdPost);
		RunBufferSingleThreadedUnsafe(&CmdBuffer);
		CmdBuffer.Reset();

		StopProcessor();
		delete m_pProcessor;
		m_pProcessor = nullptr;
	}

	if(m_Context != nullptr)
	{
		eglDestroyContext(m_Display, m_Context);
		m_Context = nullptr;
	}
	if(m_Surface != nullptr)
	{
		eglDestroySurface(m_Display, m_Surface);
		m_Surface = nullptr;
	}
	if(m_Display != nullptr)
	{
		eglTerminate(m_Display);
		m_Display = nullptr;
	}
	return 0;
}

uint64_t CGraphicsBackend_EGL::TextureMemoryUsage() const
{
	return m_TextureMemoryUsage;
}

uint64_t CGraphicsBackend_EGL::BufferMemoryUsage() const
{
	return m_BufferMemoryUsage;
}

uint64_t CGraphicsBackend_EGL::StreamedMemoryUsage() const
{
	return m_StreamMemoryUsage;
}

uint64_t CGraphicsBackend_EGL::StagingMemoryUsage() const
{
	return m_StagingMemoryUsage;
}

const TTwGraphicsGpuList &CGraphicsBackend_EGL::GetGpus() const
{
	return m_GpuList;
}

void CGraphicsBackend_EGL::GetVideoModes(CVideoMode *pModes, int MaxModes, int *pNumModes, float HiDPIScale, int MaxWindowWidth, int MaxWindowHeight, int ScreenId)
{
	(void)pModes;
	(void)MaxModes;
	(void)HiDPIScale;
	(void)MaxWindowWidth;
	(void)MaxWindowHeight;
	(void)ScreenId;
	*pNumModes = 0;
}

void CGraphicsBackend_EGL::GetCurrentVideoMode(CVideoMode &CurMode, float HiDPIScale, int MaxWindowWidth, int MaxWindowHeight, int ScreenId)
{
	(void)HiDPIScale;
	(void)MaxWindowWidth;
	(void)MaxWindowHeight;
	(void)ScreenId;
	CurMode = CVideoMode{};
}

bool CGraphicsBackend_EGL::GetDriverVersion(EGraphicsDriverAgeType DriverAgeType, int &Major, int &Minor, int &Patch, const char *&pName, EBackendType BackendType)
{
	(void)DriverAgeType;
	(void)Major;
	(void)Minor;
	(void)Patch;
	(void)pName;
	(void)BackendType;
	return false;
}

TGLBackendReadPresentedImageData &CGraphicsBackend_EGL::GetReadPresentedImageDataFuncUnsafe()
{
	return m_ReadPresentedImageDataFunc;
}

IGraphicsBackend *CreateGraphicsBackend(TTranslateFunc &&TranslateFunc)
{
	return new CGraphicsBackend_EGL(std::move(TranslateFunc));
}
#endif // BACKEND_NO_SDL
