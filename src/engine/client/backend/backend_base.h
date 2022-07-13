#ifndef ENGINE_CLIENT_BACKEND_BACKEND_BASE_H
#define ENGINE_CLIENT_BACKEND_BACKEND_BASE_H

#include <engine/graphics.h>

#include <engine/client/graphics_threaded.h>

#include <SDL_video.h>

#include <atomic>
#include <stddef.h>
#include <stdint.h>
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

class CCommandProcessorFragment_GLBase
{
protected:
	static size_t TexFormatToImageColorChannelCount(int TexFormat);
	static void *Resize(const unsigned char *pData, int Width, int Height, int NewWidth, int NewHeight, int BPP);

	static bool Texture2DTo3D(void *pImageBuffer, int ImageWidth, int ImageHeight, int ImageColorChannelCount, int SplitCountWidth, int SplitCountHeight, void *pTarget3DImageData, int &Target3DImageWidth, int &Target3DImageHeight);

	virtual bool GetPresentedImageData(uint32_t &Width, uint32_t &Height, uint32_t &Format, std::vector<uint8_t> &vDstData) = 0;

public:
	virtual ~CCommandProcessorFragment_GLBase() = default;
	virtual bool RunCommand(const CCommandBuffer::SCommand *pBaseCommand) = 0;

	virtual void StartCommands(size_t CommandCount, size_t EstimatedRenderCallCount) {}
	virtual void EndCommands() {}

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

		SDL_Window *m_pWindow = nullptr;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		char *m_pVendorString = nullptr;
		char *m_pVersionString = nullptr;
		char *m_pRendererString = nullptr;

		TTWGraphicsGPUList *m_pGPUList = nullptr;
	};

	struct SCommand_Init : public CCommandBuffer::SCommand
	{
		SCommand_Init() :
			SCommand(CMD_INIT) {}

		SDL_Window *m_pWindow = nullptr;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		class IStorage *m_pStorage = nullptr;
		std::atomic<uint64_t> *m_pTextureMemoryUsage = nullptr;
		std::atomic<uint64_t> *m_pBufferMemoryUsage = nullptr;
		std::atomic<uint64_t> *m_pStreamMemoryUsage = nullptr;
		std::atomic<uint64_t> *m_pStagingMemoryUsage = nullptr;

		TTWGraphicsGPUList *m_pGPUList = nullptr;

		TGLBackendReadPresentedImageData *m_pReadPresentedImageDataFunc = nullptr;

		SBackendCapabilites *m_pCapabilities = nullptr;
		int *m_pInitError = nullptr;

		const char **m_pErrStringPtr = nullptr;

		char *m_pVendorString = nullptr;
		char *m_pVersionString = nullptr;
		char *m_pRendererString = nullptr;

		int m_RequestedMajor = 0;
		int m_RequestedMinor = 0;
		int m_RequestedPatch = 0;

		EBackendType m_RequestedBackend = BACKEND_TYPE_OPENGL;

		int m_GlewMajor = 0;
		int m_GlewMinor = 0;
		int m_GlewPatch = 0;
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
