#ifndef ENGINE_CLIENT_BACKEND_NULL_BACKEND_NULL_H
#define ENGINE_CLIENT_BACKEND_NULL_BACKEND_NULL_H

#include <engine/client/backend/backend_base.h>

#if defined(__WIIU__)
#include <SDL_render.h>
#include <vector>
#endif

class CCommandProcessorFragment_Null : public CCommandProcessorFragment_GLBase
{
#if defined(__WIIU__)
	SDL_Renderer *m_pRenderer = nullptr;
	std::vector<SDL_Texture *> m_vpTextures;
	std::vector<SDL_Vertex> m_vVertices;
	std::vector<int> m_vIndices;
	int m_Width = 0;
	int m_Height = 0;
#endif

	bool GetPresentedImageData(uint32_t &Width, uint32_t &Height, CImageInfo::EImageFormat &Format, std::vector<uint8_t> &vDstData) override { return false; }
	ERunCommandReturnTypes RunCommand(const CCommandBuffer::SCommand *pBaseCommand) override;
	bool Cmd_Init(const SCommand_Init *pCommand);
	void Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand);
	void Cmd_TextTextures_Create(const CCommandBuffer::SCommand_TextTextures_Create *pCommand);
	void Cmd_TextTexture_Update(const CCommandBuffer::SCommand_TextTexture_Update *pCommand);
	void Cmd_Swap(const CCommandBuffer::SCommand_Swap *pCommand);
#if defined(__WIIU__)
	void Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand);
	void Cmd_TextTextures_Destroy(const CCommandBuffer::SCommand_TextTextures_Destroy *pCommand);
	void Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand);
	void Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand);
	void Cmd_Update_Viewport(const CCommandBuffer::SCommand_Update_Viewport *pCommand);
#endif
};

#endif
