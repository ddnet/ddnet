#ifndef ENGINE_CLIENT_BACKEND_BACKEND_BASE_H
#define ENGINE_CLIENT_BACKEND_BACKEND_BASE_H

#include <engine/graphics.h>

#include <engine/client/graphics_threaded.h>

#include <SDL_video.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct SBackendCapabilites;

enum EDebugGFXModes
{
	DEBUG_GFX_MODE_NONE = 0,
	DEBUG_GFX_MODE_MINIMUM,
	DEBUG_GFX_MODE_AFFECTS_PERFORMANCE,
	DEBUG_GFX_MODE_VERBOSE,
	DEBUG_GFX_MODE_ALL,
};

enum ERunCommandReturnTypes
{
	RUN_COMMAND_COMMAND_HANDLED = 0,
	RUN_COMMAND_COMMAND_UNHANDLED,
	RUN_COMMAND_COMMAND_WARNING,
	RUN_COMMAND_COMMAND_ERROR,
};

enum EGFXErrorType
{
	GFX_ERROR_TYPE_NONE = 0,
	GFX_ERROR_TYPE_INIT,
	GFX_ERROR_TYPE_OUT_OF_MEMORY_IMAGE,
	GFX_ERROR_TYPE_OUT_OF_MEMORY_BUFFER,
	GFX_ERROR_TYPE_OUT_OF_MEMORY_STAGING,
	GFX_ERROR_TYPE_RENDER_RECORDING,
	GFX_ERROR_TYPE_RENDER_CMD_FAILED,
	GFX_ERROR_TYPE_RENDER_SUBMIT_FAILED,
	GFX_ERROR_TYPE_SWAP_FAILED,
	GFX_ERROR_TYPE_UNKNOWN,
};

enum EGFXWarningType
{
	GFX_WARNING_TYPE_NONE = 0,
	GFX_WARNING_TYPE_INIT_FAILED,
	GFX_WARNING_TYPE_INIT_FAILED_MISSING_INTEGRATED_GPU_DRIVER,
	GFX_WARNING_LOW_ON_MEMORY,
	GFX_WARNING_MISSING_EXTENSION,
	GFX_WARNING_TYPE_UNKNOWN,
};

struct SGFXErrorContainer
{
	struct SError
	{
		bool m_RequiresTranslation;
		std::string m_Err;

		bool operator==(const SError &Other) const
		{
			return m_RequiresTranslation == Other.m_RequiresTranslation && m_Err == Other.m_Err;
		}
	};
	EGFXErrorType m_ErrorType = EGFXErrorType::GFX_ERROR_TYPE_NONE;
	std::vector<SError> m_vErrors;
};

struct SGFXWarningContainer
{
	EGFXWarningType m_WarningType = EGFXWarningType::GFX_WARNING_TYPE_NONE;
	std::vector<std::string> m_vWarnings;
};

class CCommandProcessorFragment_GLBase
{
protected:
	SGFXErrorContainer m_Error;
	SGFXWarningContainer m_Warning;

	static size_t TexFormatToImageColorChannelCount(int TexFormat);
	static void *Resize(const unsigned char *pData, int Width, int Height, int NewWidth, int NewHeight, int BPP);

	static bool Texture2DTo3D(void *pImageBuffer, int ImageWidth, int ImageHeight, int ImageColorChannelCount, int SplitCountWidth, int SplitCountHeight, void *pTarget3DImageData, int &Target3DImageWidth, int &Target3DImageHeight);

	virtual bool GetPresentedImageData(uint32_t &Width, uint32_t &Height, uint32_t &Format, std::vector<uint8_t> &vDstData) = 0;

public:
	virtual ~CCommandProcessorFragment_GLBase() = default;
	virtual ERunCommandReturnTypes RunCommand(const CCommandBuffer::SCommand *pBaseCommand) = 0;

	virtual void StartCommands(size_t CommandCount, size_t EstimatedRenderCallCount) {}
	virtual void EndCommands() {}

	const SGFXErrorContainer &GetError() { return m_Error; }
	virtual void ErroneousCleanup() {}

	const SGFXWarningContainer &GetWarning() { return m_Warning; }

	enum
	{
		CMD_PRE_INIT = CCommandBuffer::CMDGROUP_PLATFORM_GL,
		CMD_INIT,
		CMD_SHUTDOWN,
		CMD_POST_SHUTDOWN,
	};

	struct SCommand_PreInit : public CCommandBuffer::SCommand
	{
		SCommand_PreInit() :
			SCommand(CMD_PRE_INIT) {}

		SDL_Window *m_pWindow;
		uint32_t m_Width;
		uint32_t m_Height;

		char *m_pVendorString;
		char *m_pVersionString;
		char *m_pRendererString;

		TTWGraphicsGPUList *m_pGPUList;
	};

	struct SCommand_Init : public CCommandBuffer::SCommand
	{
		SCommand_Init() :
			SCommand(CMD_INIT) {}

		SDL_Window *m_pWindow;
		uint32_t m_Width;
		uint32_t m_Height;

		class IStorage *m_pStorage;
		std::atomic<uint64_t> *m_pTextureMemoryUsage;
		std::atomic<uint64_t> *m_pBufferMemoryUsage;
		std::atomic<uint64_t> *m_pStreamMemoryUsage;
		std::atomic<uint64_t> *m_pStagingMemoryUsage;

		TTWGraphicsGPUList *m_pGPUList;

		TGLBackendReadPresentedImageData *m_pReadPresentedImageDataFunc;

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

	struct SCommand_PostShutdown : public CCommandBuffer::SCommand
	{
		SCommand_PostShutdown() :
			SCommand(CMD_POST_SHUTDOWN) {}
	};
};

#endif
