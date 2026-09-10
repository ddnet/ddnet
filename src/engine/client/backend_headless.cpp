#if defined(CONF_BACKEND_VULKAN)

#include "backend_headless.h"

#include "backend/vulkan/backend_vulkan.h"
#include "graphics_threaded.h"

#include <base/dbg.h>
#include <base/log.h>
#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

// ------------ CCommandProcessor_Headless

CCommandProcessor_Headless::CCommandProcessor_Headless()
{
	m_pBackend = CreateVulkanCommandProcessorFragment();
}

CCommandProcessor_Headless::~CCommandProcessor_Headless()
{
	delete m_pBackend;
}

void CCommandProcessor_Headless::RunBuffer(CCommandBuffer *pBuffer)
{
	m_pBackend->StartCommands(pBuffer->m_CommandCount, pBuffer->m_RenderCallCount);

	for(const CCommandBuffer::SCommand *pCommand = pBuffer->Head(); pCommand; pCommand = pCommand->m_pNext)
	{
		const ERunCommandReturnTypes Res = m_pBackend->RunCommand(pCommand);
		if(Res == ERunCommandReturnTypes::RUN_COMMAND_COMMAND_HANDLED)
			continue;
		if(Res == ERunCommandReturnTypes::RUN_COMMAND_COMMAND_ERROR)
		{
			m_Error = m_pBackend->GetError();
			return;
		}
		if(Res == ERunCommandReturnTypes::RUN_COMMAND_COMMAND_WARNING)
		{
			m_Warning = m_pBackend->GetWarning();
			return;
		}

		if(m_General.RunCommand(pCommand))
			continue;

		dbg_assert_failed("Unknown graphics command %d", pCommand->m_Cmd);
	}

	m_pBackend->EndCommands();
}

const SGfxErrorContainer &CCommandProcessor_Headless::GetError() const
{
	return m_Error;
}

void CCommandProcessor_Headless::ErroneousCleanup()
{
	m_pBackend->ErroneousCleanup();
}

const SGfxWarningContainer &CCommandProcessor_Headless::GetWarning() const
{
	return m_Warning;
}

// ------------ CGraphicsBackend_Headless

CGraphicsBackend_Headless::CGraphicsBackend_Headless(TTranslateFunc &&TranslateFunc) :
	CGraphicsBackend_Threaded(std::move(TranslateFunc))
{
}

int CGraphicsBackend_Headless::Init(const char *pName, int *pScreen, int *pWidth, int *pHeight, int *pRefreshRate, int *pFsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight, int *pCurrentWidth, int *pCurrentHeight, IStorage *pStorage)
{
	if(*pWidth <= 0 || *pHeight <= 0)
	{
		*pWidth = 640;
		*pHeight = 480;
	}
	*pCurrentWidth = *pWidth;
	*pCurrentHeight = *pHeight;

	const uint32_t Width = *pWidth;
	const uint32_t Height = *pHeight;

	m_pProcessor = new CCommandProcessor_Headless();
	StartProcessor(m_pProcessor);

	{
		CCommandProcessorFragment_GLBase::SCommand_PreInit CmdPre;
		CmdPre.m_pWindow = nullptr;
		CmdPre.m_Headless = true;
		CmdPre.m_Width = Width;
		CmdPre.m_Height = Height;
		CmdPre.m_pVendorString = m_aVendorString;
		CmdPre.m_pVersionString = m_aVersionString;
		CmdPre.m_pRendererString = m_aRendererString;
		CmdPre.m_pGpuList = &m_GpuList;

		CCommandBuffer CmdBuffer(1024, 512);
		CmdBuffer.AddCommandUnsafe(CmdPre);
		RunBufferSingleThreadedUnsafe(&CmdBuffer);
		CmdBuffer.Reset();
	}

	const char *pErrorStr = nullptr;
	int InitError = 0;
	{
		CCommandProcessorFragment_GLBase::SCommand_Init CmdInit;
		CmdInit.m_pWindow = nullptr;
		CmdInit.m_Width = Width;
		CmdInit.m_Height = Height;
		CmdInit.m_pTextureMemoryUsage = &m_TextureMemoryUsage;
		CmdInit.m_pBufferMemoryUsage = &m_BufferMemoryUsage;
		CmdInit.m_pStreamMemoryUsage = &m_StreamMemoryUsage;
		CmdInit.m_pStagingMemoryUsage = &m_StagingMemoryUsage;
		CmdInit.m_pGpuList = &m_GpuList;
		CmdInit.m_pReadPresentedImageDataFunc = &m_ReadPresentedImageDataFunc;
		CmdInit.m_pStorage = pStorage;
		CmdInit.m_pCapabilities = &m_Capabilities;
		CmdInit.m_pInitError = &InitError;
		CmdInit.m_RequestedMajor = BACKEND_VULKAN_VERSION_MAJOR;
		CmdInit.m_RequestedMinor = BACKEND_VULKAN_VERSION_MINOR;
		CmdInit.m_RequestedPatch = 0;
		CmdInit.m_GlewMajor = 0;
		CmdInit.m_GlewMinor = 0;
		CmdInit.m_GlewPatch = 0;
		CmdInit.m_pErrStringPtr = &pErrorStr;
		CmdInit.m_pVendorString = m_aVendorString;
		CmdInit.m_pVersionString = m_aVersionString;
		CmdInit.m_pRendererString = m_aRendererString;
		CmdInit.m_RequestedBackend = BACKEND_TYPE_VULKAN;

		CCommandBuffer CmdBuffer(1024, 512);
		CmdBuffer.AddCommandUnsafe(CmdInit);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
		CmdBuffer.Reset();
	}

	if(pErrorStr != nullptr)
	{
		str_copy(m_aErrorString, pErrorStr);
	}

	if(InitError != 0)
	{
		CCommandBuffer CmdBuffer(1024, 512);
		CCommandProcessorFragment_GLBase::SCommand_Shutdown CmdShutdown;
		CmdBuffer.AddCommandUnsafe(CmdShutdown);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
		CmdBuffer.Reset();

		StopProcessor();
		delete m_pProcessor;
		m_pProcessor = nullptr;
		return -1;
	}

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

	return 0;
}

int CGraphicsBackend_Headless::Shutdown()
{
	if(m_pProcessor != nullptr)
	{
		CCommandBuffer CmdBuffer(1024, 512);

		CCommandProcessorFragment_GLBase::SCommand_Shutdown CmdShutdown;
		CmdBuffer.AddCommandUnsafe(CmdShutdown);
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

	return 0;
}

uint64_t CGraphicsBackend_Headless::TextureMemoryUsage() const
{
	return m_TextureMemoryUsage;
}

uint64_t CGraphicsBackend_Headless::BufferMemoryUsage() const
{
	return m_BufferMemoryUsage;
}

uint64_t CGraphicsBackend_Headless::StreamedMemoryUsage() const
{
	return m_StreamMemoryUsage;
}

uint64_t CGraphicsBackend_Headless::StagingMemoryUsage() const
{
	return m_StagingMemoryUsage;
}

const TTwGraphicsGpuList &CGraphicsBackend_Headless::GetGpus() const
{
	return m_GpuList;
}

void CGraphicsBackend_Headless::GetVideoModes(CVideoMode *pModes, int MaxModes, int *pNumModes, float HiDPIScale, int MaxWindowWidth, int MaxWindowHeight, int ScreenId)
{
	*pNumModes = 0;
}

void CGraphicsBackend_Headless::GetCurrentVideoMode(CVideoMode &CurMode, float HiDPIScale, int MaxWindowWidth, int MaxWindowHeight, int ScreenId)
{
	CurMode = CVideoMode{};
}

bool CGraphicsBackend_Headless::GetDriverVersion(EGraphicsDriverAgeType DriverAgeType, int &Major, int &Minor, int &Patch, const char *&pName, EBackendType BackendType)
{
	return false;
}

TGLBackendReadPresentedImageData &CGraphicsBackend_Headless::GetReadPresentedImageDataFuncUnsafe()
{
	return m_ReadPresentedImageDataFunc;
}

std::optional<int> CGraphicsBackend_Headless::ShowMessageBox(const IGraphics::CMessageBox &MessageBox)
{
	log_error("gfx/headless", "Unhandled message box: %s: %s", MessageBox.m_pTitle, MessageBox.m_pMessage);
	return std::nullopt;
}

#endif
