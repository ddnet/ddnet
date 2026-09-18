#ifndef ENGINE_CLIENT_BACKEND_HEADLESS_H
#define ENGINE_CLIENT_BACKEND_HEADLESS_H

#include <engine/client/backend/backend_base.h>
#include <engine/client/backend_sdl.h>
#include <engine/client/graphics_threaded.h>
#include <engine/graphics.h>

#include <atomic>
#include <cstdint>

class CCommandProcessor_Headless : public CGraphicsBackend_Threaded::ICommandProcessor
{
	CCommandProcessorFragment_GLBase *m_pBackend;
	CCommandProcessorFragment_General m_General;

	SGfxErrorContainer m_Error;
	SGfxWarningContainer m_Warning;

public:
	CCommandProcessor_Headless();
	~CCommandProcessor_Headless() override;
	void RunBuffer(CCommandBuffer *pBuffer) override;

	const SGfxErrorContainer &GetError() const override;
	void ErroneousCleanup() override;
	const SGfxWarningContainer &GetWarning() const override;
};

class CGraphicsBackend_Headless : public CGraphicsBackend_Threaded
{
	ICommandProcessor *m_pProcessor{nullptr};

	std::atomic<uint64_t> m_TextureMemoryUsage{0};
	std::atomic<uint64_t> m_BufferMemoryUsage{0};
	std::atomic<uint64_t> m_StreamMemoryUsage{0};
	std::atomic<uint64_t> m_StagingMemoryUsage{0};

	TTwGraphicsGpuList m_GpuList;
	TGLBackendReadPresentedImageData m_ReadPresentedImageDataFunc;

	SBackendCapabilities m_Capabilities;

	char m_aVendorString[256]{};
	char m_aVersionString[256]{};
	char m_aRendererString[256]{};
	char m_aErrorString[256]{};

public:
	CGraphicsBackend_Headless(TTranslateFunc &&TranslateFunc);
	int Init(const char *pName, int *pScreen, int *pWidth, int *pHeight, int *pRefreshRate, int *pFsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight, int *pCurrentWidth, int *pCurrentHeight, class IStorage *pStorage) override;
	int Shutdown() override;

	uint64_t TextureMemoryUsage() const override;
	uint64_t BufferMemoryUsage() const override;
	uint64_t StreamedMemoryUsage() const override;
	uint64_t StagingMemoryUsage() const override;

	const TTwGraphicsGpuList &GetGpus() const override;
	int GetNumScreens() const override { return 1; }
	const char *GetScreenName(int Screen) const override { return "Headless"; }

	void GetVideoModes(CVideoMode *pModes, int MaxModes, int *pNumModes, float HiDPIScale, int MaxWindowWidth, int MaxWindowHeight, int ScreenId) override;
	void GetCurrentVideoMode(CVideoMode &CurMode, float HiDPIScale, int MaxWindowWidth, int MaxWindowHeight, int ScreenId) override;

	void Minimize() override {}
	void SetWindowParams(int FullscreenMode, bool IsBorderless) override {}
	bool SetWindowScreen(int Index, bool MoveToCenter, ivec2 *pDesktopSize) override { return false; }
	bool UpdateDisplayMode(int Index, ivec2 *pDesktopSize) override { return false; }
	int GetWindowScreen() override { return 0; }
	int WindowActive() override { return 1; }
	int WindowOpen() override { return 1; }
	void SetWindowGrab(bool Grab) override {}
	bool ResizeWindow(int w, int h, int RefreshRate) override { return false; }
	void GetViewportSize(int &w, int &h) override
	{
		w = 0;
		h = 0;
	}
	void GetDisplayCutoutInsets(int &Left, int &Right) override
	{
		Left = 0;
		Right = 0;
	}
	void NotifyWindow() override {}
	bool IsScreenKeyboardShown() override { return false; }
	void WindowDestroyNtf(uint32_t WindowId) override {}
	void WindowCreateNtf(uint32_t WindowId) override {}

	bool GetDriverVersion(EGraphicsDriverAgeType DriverAgeType, int &Major, int &Minor, int &Patch, const char *&pName, EBackendType BackendType) override;
	bool IsConfigModernAPI() override { return true; }
	bool UseTrianglesAsQuad() override { return m_Capabilities.m_TrianglesAsQuads; }
	bool HasTileBuffering() override { return m_Capabilities.m_TileBuffering; }
	bool HasQuadBuffering() override { return m_Capabilities.m_QuadBuffering; }
	bool HasTextBuffering() override { return m_Capabilities.m_TextBuffering; }
	bool HasQuadContainerBuffering() override { return m_Capabilities.m_QuadContainerBuffering; }
	bool Uses2DTextureArrays() override { return m_Capabilities.m_2DArrayTextures; }
	bool HasTextureArraysSupport() override { return m_Capabilities.m_2DArrayTextures || m_Capabilities.m_3DTextures; }

	const char *GetErrorString() override { return m_aErrorString[0] ? m_aErrorString : nullptr; }
	const char *GetVendorString() override { return m_aVendorString; }
	const char *GetVersionString() override { return m_aVersionString; }
	const char *GetRendererString() override { return m_aRendererString; }

	TGLBackendReadPresentedImageData &GetReadPresentedImageDataFuncUnsafe() override;

	std::optional<int> ShowMessageBox(const IGraphics::CMessageBox &MessageBox) override;
};

#endif
