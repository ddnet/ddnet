#include "backend_null.h"

#include <engine/client/backend_sdl.h>

ERunCommandReturnTypes CCommandProcessorFragment_Null::RunCommand(const CCommandBuffer::SCommand *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CCommandProcessorFragment_Null::CMD_INIT:
		Cmd_Init(static_cast<const SCommand_Init *>(pBaseCommand));
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
	}
	return ERunCommandReturnTypes::RUN_COMMAND_COMMAND_HANDLED;
}

bool CCommandProcessorFragment_Null::Cmd_Init(const SCommand_Init *pCommand)
{
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
