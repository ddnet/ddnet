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
