#ifndef ENGINE_CLIENT_BACKEND_NULL_BACKEND_NULL_H
#define ENGINE_CLIENT_BACKEND_NULL_BACKEND_NULL_H

#include <engine/client/backend/backend_base.h>

class CCommandProcessorFragment_Null : public CCommandProcessorFragment_GLBase
{
	bool GetPresentedImageData(uint32_t &Width, uint32_t &Height, CImageInfo::EImageFormat &Format, std::vector<uint8_t> &vDstData) override { return false; };
	ERunCommandReturnTypes RunCommand(const CCommandBuffer::SCommand *pBaseCommand) override;
	bool Cmd_Init(const SCommand_Init *pCommand);
	virtual void Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand);
	virtual void Cmd_TextTextures_Create(const CCommandBuffer::SCommand_TextTextures_Create *pCommand);
	virtual void Cmd_TextTexture_Update(const CCommandBuffer::SCommand_TextTexture_Update *pCommand);
};

#endif
