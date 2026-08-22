#include <base/dbg.h>
#include <base/log.h>
#include <base/math.h>
#include <base/sphore.h>
#include <base/str.h>
#include <base/thread.h>

#include <engine/client/backend_sdl.h>
#include <engine/client/graphics_threaded.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#if defined(CONF_VIDEORECORDER) && !defined(BACKEND_NO_SDL)
#include <engine/shared/video.h>
#endif

// ------------ CGraphicsBackend_Threaded

#if !defined(CONF_PLATFORM_EMSCRIPTEN)
void CGraphicsBackend_Threaded::ThreadFunc(void *pUser)
{
	auto *pSelf = (CGraphicsBackend_Threaded *)pUser;
	std::unique_lock<std::mutex> Lock(pSelf->m_BufferSwapMutex);
	pSelf->m_Started = true;
	pSelf->m_BufferSwapCond.notify_all();
	while(!pSelf->m_Shutdown)
	{
		pSelf->m_BufferSwapCond.wait(Lock, [&pSelf] { return pSelf->m_pBuffer != nullptr || pSelf->m_Shutdown; });
		if(pSelf->m_pBuffer)
		{
#ifdef CONF_PLATFORM_MACOS
			CAutoreleasePool AutoreleasePool;
#endif
			pSelf->m_pProcessor->RunBuffer(pSelf->m_pBuffer);
			pSelf->m_pBuffer = nullptr;
			pSelf->m_BufferInProcess.store(false, std::memory_order_relaxed);
			pSelf->m_BufferSwapCond.notify_all();
#if defined(CONF_VIDEORECORDER) && !defined(BACKEND_NO_SDL)
			if(IVideo::Current())
				IVideo::Current()->NextVideoFrameThread();
#endif
		}
	}
}
#endif

CGraphicsBackend_Threaded::CGraphicsBackend_Threaded(TTranslateFunc &&TranslateFunc) :
	m_TranslateFunc(std::move(TranslateFunc))
{
	m_pProcessor = nullptr;
	m_Shutdown = true;
#if !defined(CONF_PLATFORM_EMSCRIPTEN)
	m_pBuffer = nullptr;
	m_BufferInProcess.store(false, std::memory_order_relaxed);
#endif
}

void CGraphicsBackend_Threaded::StartProcessor(ICommandProcessor *pProcessor)
{
	dbg_assert(m_Shutdown, "Processor was already not shut down.");
	m_Shutdown = false;
	m_pProcessor = pProcessor;
#if !defined(CONF_PLATFORM_EMSCRIPTEN)
	std::unique_lock<std::mutex> Lock(m_BufferSwapMutex);
	m_pThread = thread_init(ThreadFunc, this, "Graphics thread");
	m_BufferSwapCond.wait(Lock, [this]() -> bool { return m_Started; });
#endif
}

void CGraphicsBackend_Threaded::StopProcessor()
{
	dbg_assert(!m_Shutdown, "Processor was already shut down.");
	m_Shutdown = true;
#if defined(CONF_PLATFORM_EMSCRIPTEN)
	m_Warning = m_pProcessor->GetWarning();
#else
	{
		std::unique_lock<std::mutex> Lock(m_BufferSwapMutex);
		m_Warning = m_pProcessor->GetWarning();
		m_BufferSwapCond.notify_all();
	}
	thread_wait(m_pThread);
#endif
}

void CGraphicsBackend_Threaded::RunBuffer(CCommandBuffer *pBuffer)
{
	SGfxErrorContainer Error;
#if defined(CONF_PLATFORM_EMSCRIPTEN)
	Error = m_pProcessor->GetError();
	if(Error.m_ErrorType == GFX_ERROR_TYPE_NONE)
	{
		RunBufferSingleThreadedUnsafe(pBuffer);
#if defined(CONF_VIDEORECORDER)
		if(IVideo::Current())
			IVideo::Current()->NextVideoFrameThread();
#endif
	}
#else
	WaitForIdle();
	{
		std::unique_lock<std::mutex> Lock(m_BufferSwapMutex);
		Error = m_pProcessor->GetError();
		if(Error.m_ErrorType == GFX_ERROR_TYPE_NONE)
		{
			m_pBuffer = pBuffer;
			m_BufferInProcess.store(true, std::memory_order_relaxed);
			m_BufferSwapCond.notify_all();
		}
	}
#endif
	if(Error.m_ErrorType != GFX_ERROR_TYPE_NONE)
	{
		ProcessError(Error);
	}
}

void CGraphicsBackend_Threaded::RunBufferSingleThreadedUnsafe(CCommandBuffer *pBuffer)
{
	m_pProcessor->RunBuffer(pBuffer);
}

bool CGraphicsBackend_Threaded::IsIdle() const
{
#if defined(CONF_PLATFORM_EMSCRIPTEN)
	return true;
#else
	return !m_BufferInProcess.load(std::memory_order_relaxed);
#endif
}

void CGraphicsBackend_Threaded::WaitForIdle()
{
#if !defined(CONF_PLATFORM_EMSCRIPTEN)
	std::unique_lock<std::mutex> Lock(m_BufferSwapMutex);
	m_BufferSwapCond.wait(Lock, [this]() { return m_pBuffer == nullptr; });
#endif
}

void CGraphicsBackend_Threaded::ProcessError(const SGfxErrorContainer &Error)
{
	m_FatalError = "";
	for(const auto &ErrStr : Error.m_vErrors)
	{
		if(!m_FatalError.empty())
		{
			m_FatalError.append("\n");
		}
		if(ErrStr.m_RequiresTranslation)
			m_FatalError.append(m_TranslateFunc(ErrStr.m_Err.c_str(), ""));
		else
			m_FatalError.append(ErrStr.m_Err);
	}
	std::string LogMessage = "Graphics Error:\n" + m_FatalError;
	dbg_assert_failed("%s", LogMessage.c_str());
}

const char *CGraphicsBackend_Threaded::GetFatalError() const
{
	return m_FatalError.c_str();
}

bool CGraphicsBackend_Threaded::GetWarning(std::vector<std::string> &WarningStrings)
{
	if(m_Warning.m_WarningType != GFX_WARNING_TYPE_NONE)
	{
		m_Warning.m_WarningType = GFX_WARNING_TYPE_NONE;
		WarningStrings = m_Warning.m_vWarnings;
		return true;
	}
	return false;
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
	case CCommandBuffer::CMD_SIGNAL: Cmd_Signal(static_cast<const CCommandBuffer::SCommand_Signal *>(pBaseCommand)); break;
	default: return false;
	}
	return true;
}
