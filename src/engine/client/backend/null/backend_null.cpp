#include "backend_null.h"

#include <engine/client/backend_sdl.h>

#if defined(CONF_PLATFORM_EMSCRIPTEN)
#include <emscripten/emscripten.h>
#endif

bool CCommandProcessorFragment_Null::GetPresentedImageData(uint32_t &Width, uint32_t &Height, CImageInfo::EImageFormat &Format, std::vector<uint8_t> &vDstData)
{
	if(m_CanvasWidth == 0 || m_CanvasHeight == 0)
	{
		return false;
	}
	else
	{
		Width = m_CanvasWidth;
		Height = m_CanvasHeight;
		Format = CImageInfo::FORMAT_RGBA;
		vDstData.resize((size_t)Width * Height * CImageInfo::PixelSize(Format), 0x00);
		return true;
	}
}

ERunCommandReturnTypes CCommandProcessorFragment_Null::RunCommand(const CCommandBuffer::SCommand *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CCommandProcessorFragment_Null::CMD_INIT:
		Cmd_Init(static_cast<const SCommand_Init *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_UPDATE_VIEWPORT:
		Cmd_Update_Viewport(static_cast<const CCommandBuffer::SCommand_Update_Viewport *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_TEXTURE_CREATE:
		Cmd_Texture_Create(static_cast<const CCommandBuffer::SCommand_Texture_Create *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_TEXT_TEXTURES_CREATE:
		Cmd_TextTextures_Create(static_cast<const CCommandBuffer::SCommand_TextTextures_Create *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_TEXT_TEXTURE_UPDATE:
		Cmd_TextTexture_Update(static_cast<const CCommandBuffer::SCommand_TextTexture_Update *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_SWAP:
		Cmd_Swap(static_cast<const CCommandBuffer::SCommand_Swap *>(pBaseCommand));
		break;
	}
	return ERunCommandReturnTypes::RUN_COMMAND_COMMAND_HANDLED;
}

bool CCommandProcessorFragment_Null::Cmd_Init(const SCommand_Init *pCommand)
{
	*pCommand->m_pReadPresentedImageDataFunc = [this](uint32_t &Width, uint32_t &Height, CImageInfo::EImageFormat &Format, std::vector<uint8_t> &vDstData) {
		return GetPresentedImageData(Width, Height, Format, vDstData);
	};

	pCommand->m_pCapabilities->m_TileBuffering = false;
	pCommand->m_pCapabilities->m_QuadBuffering = false;
	pCommand->m_pCapabilities->m_TextBuffering = false;
	pCommand->m_pCapabilities->m_QuadContainerBuffering = false;

	pCommand->m_pCapabilities->m_MipMapping = false;
	pCommand->m_pCapabilities->m_NPOTTextures = false;
	pCommand->m_pCapabilities->m_3DTextures = false;
	pCommand->m_pCapabilities->m_2DArrayTextures = false;
	pCommand->m_pCapabilities->m_2DArrayTexturesAsExtension = false;
	pCommand->m_pCapabilities->m_ShaderSupport = false;

	pCommand->m_pCapabilities->m_TrianglesAsQuads = false;

	pCommand->m_pCapabilities->m_ContextMajor = 0;
	pCommand->m_pCapabilities->m_ContextMinor = 0;
	pCommand->m_pCapabilities->m_ContextPatch = 0;
	return false;
}

void CCommandProcessorFragment_Null::Cmd_Update_Viewport(const CCommandBuffer::SCommand_Update_Viewport *pCommand)
{
	if(pCommand->m_ByResize)
	{
		m_CanvasWidth = (uint32_t)pCommand->m_Width;
		m_CanvasHeight = (uint32_t)pCommand->m_Height;
	}
}

void CCommandProcessorFragment_Null::Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand)
{
	free(pCommand->m_pData);
}

void CCommandProcessorFragment_Null::Cmd_TextTextures_Create(const CCommandBuffer::SCommand_TextTextures_Create *pCommand)
{
	free(pCommand->m_pTextData);
	free(pCommand->m_pTextOutlineData);
}

void CCommandProcessorFragment_Null::Cmd_TextTexture_Update(const CCommandBuffer::SCommand_TextTexture_Update *pCommand)
{
	free(pCommand->m_pData);
}

void CCommandProcessorFragment_Null::Cmd_Swap(const CCommandBuffer::SCommand_Swap *pCommand)
{
#if defined(CONF_PLATFORM_EMSCRIPTEN)
	// Return control to the browser's main thread. This is normally done in SDL_GL_SwapWindow,
	// but with headless graphics we do not have a GL context to call this function.
	emscripten_sleep(0);
#endif
}
