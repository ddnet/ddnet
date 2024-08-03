// This file can be included several times.
#if(!defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL3_H)) || \
	(defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL3_H_AS_ES))

#if !defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL3_H)
#define ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL3_H
#endif

#if defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL3_H_AS_ES)
#define ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL3_H_AS_ES
#endif

#include "backend_opengl.h"

class CGLSLPrimitiveExProgram;
class CGLSLQuadProgram;
class CGLSLSpriteMultipleProgram;
class CGLSLTextProgram;

#define MAX_STREAM_BUFFER_COUNT 10

// takes care of opengl 3.3+ related rendering
class CCommandProcessorFragment_OpenGL3_3 : public CCommandProcessorFragment_OpenGL3
{
protected:
	int m_MaxQuadsAtOnce;
	static const int ms_MaxQuadsPossible = 256;

	CGLSLPrimitiveProgram *m_pPrimitiveProgram;
	CGLSLPrimitiveProgram *m_pPrimitiveProgramTextured;
	CGLSLQuadProgram *m_pQuadProgram;
	CGLSLQuadProgram *m_pQuadProgramTextured;
	CGLSLTextProgram *m_pTextProgram;
	CGLSLPrimitiveExProgram *m_pPrimitiveExProgram;
	CGLSLPrimitiveExProgram *m_pPrimitiveExProgramTextured;
	CGLSLPrimitiveExProgram *m_pPrimitiveExProgramRotationless;
	CGLSLPrimitiveExProgram *m_pPrimitiveExProgramTexturedRotationless;
	CGLSLSpriteMultipleProgram *m_pSpriteProgramMultiple;

	TWGLuint m_LastProgramId;

	TWGLuint m_aPrimitiveDrawVertexId[MAX_STREAM_BUFFER_COUNT];
	TWGLuint m_PrimitiveDrawVertexIdTex3D;
	TWGLuint m_aPrimitiveDrawBufferId[MAX_STREAM_BUFFER_COUNT];
	TWGLuint m_PrimitiveDrawBufferIdTex3D;

	TWGLuint m_aLastIndexBufferBound[MAX_STREAM_BUFFER_COUNT];

	int m_LastStreamBuffer;

	TWGLuint m_QuadDrawIndexBufferId;
	unsigned int m_CurrentIndicesInBuffer;

	void DestroyBufferContainer(int Index, bool DeleteBOs = true);

	void AppendIndices(unsigned int NewIndicesCount);

	struct SBufferContainer
	{
		SBufferContainer() :
			m_VertArrayId(0), m_LastIndexBufferBound(0) {}
		TWGLuint m_VertArrayId;
		TWGLuint m_LastIndexBufferBound;
		SBufferContainerInfo m_ContainerInfo;
	};
	std::vector<SBufferContainer> m_vBufferContainers;

	std::vector<TWGLuint> m_vBufferObjectIndices;

	CCommandBuffer::SColorf m_ClearColor;

	void InitPrimExProgram(CGLSLPrimitiveExProgram *pProgram, class CGLSLCompiler *pCompiler, class IStorage *pStorage, bool Textured, bool Rotationless);

	bool IsNewApi() override { return true; }

	void UseProgram(CGLSLTWProgram *pProgram);
	void UploadStreamBufferData(unsigned int PrimitiveType, const void *pVertices, size_t VertSize, unsigned int PrimitiveCount, bool AsTex3D = false);
	void RenderText(const CCommandBuffer::SState &State, int DrawNum, int TextTextureIndex, int TextOutlineTextureIndex, int TextureSize, const ColorRGBA &TextColor, const ColorRGBA &TextOutlineColor);

	void TextureUpdate(int Slot, int X, int Y, int Width, int Height, int GLFormat, uint8_t *pTexData);
	void TextureCreate(int Slot, int Width, int Height, int GLFormat, int GLStoreFormat, int Flags, uint8_t *pTexData);

	bool Cmd_Init(const SCommand_Init *pCommand) override;
	void Cmd_Shutdown(const SCommand_Shutdown *pCommand) override;
	void Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand) override;
	void Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand) override;
	void Cmd_TextTexture_Update(const CCommandBuffer::SCommand_TextTexture_Update *pCommand) override;
	void Cmd_TextTextures_Destroy(const CCommandBuffer::SCommand_TextTextures_Destroy *pCommand) override;
	void Cmd_TextTextures_Create(const CCommandBuffer::SCommand_TextTextures_Create *pCommand) override;
	void Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand) override;
	void Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand) override;
	void Cmd_RenderTex3D(const CCommandBuffer::SCommand_RenderTex3D *pCommand) override;

	void Cmd_CreateBufferObject(const CCommandBuffer::SCommand_CreateBufferObject *pCommand) override;
	void Cmd_RecreateBufferObject(const CCommandBuffer::SCommand_RecreateBufferObject *pCommand) override;
	void Cmd_UpdateBufferObject(const CCommandBuffer::SCommand_UpdateBufferObject *pCommand) override;
	void Cmd_CopyBufferObject(const CCommandBuffer::SCommand_CopyBufferObject *pCommand) override;
	void Cmd_DeleteBufferObject(const CCommandBuffer::SCommand_DeleteBufferObject *pCommand) override;

	void Cmd_CreateBufferContainer(const CCommandBuffer::SCommand_CreateBufferContainer *pCommand) override;
	void Cmd_UpdateBufferContainer(const CCommandBuffer::SCommand_UpdateBufferContainer *pCommand) override;
	void Cmd_DeleteBufferContainer(const CCommandBuffer::SCommand_DeleteBufferContainer *pCommand) override;
	void Cmd_IndicesRequiredNumNotify(const CCommandBuffer::SCommand_IndicesRequiredNumNotify *pCommand) override;

	void Cmd_RenderTileLayer(const CCommandBuffer::SCommand_RenderTileLayer *pCommand) override;
	void Cmd_RenderBorderTile(const CCommandBuffer::SCommand_RenderBorderTile *pCommand) override;
	void Cmd_RenderQuadLayer(const CCommandBuffer::SCommand_RenderQuadLayer *pCommand) override;
	void Cmd_RenderText(const CCommandBuffer::SCommand_RenderText *pCommand) override;
	void Cmd_RenderQuadContainer(const CCommandBuffer::SCommand_RenderQuadContainer *pCommand) override;
	void Cmd_RenderQuadContainerEx(const CCommandBuffer::SCommand_RenderQuadContainerEx *pCommand) override;
	void Cmd_RenderQuadContainerAsSpriteMultiple(const CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple *pCommand) override;

public:
	CCommandProcessorFragment_OpenGL3_3() = default;
};

#endif
