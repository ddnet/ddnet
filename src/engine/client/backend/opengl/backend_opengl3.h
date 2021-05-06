// This file can be included several times.
#if(!defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL3_H)) || \
	(defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL3_H_AS_ES))

#if !defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL3_H)
#define ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL3_H
#endif

#if defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL3_H_AS_ES)
#define ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL3_H_AS_ES
#endif

#include <engine/client/backend_sdl.h>

#include "backend_opengl.h"

#define MAX_STREAM_BUFFER_COUNT 30

// takes care of opengl 3.3+ related rendering
class CCommandProcessorFragment_OpenGL3_3 : public CCommandProcessorFragment_OpenGL3
{
protected:
	bool m_UsePreinitializedVertexBuffer;

	int m_MaxQuadsAtOnce;
	static const int m_MaxQuadsPossible = 256;

	CGLSLPrimitiveProgram *m_pPrimitiveProgram;
	CGLSLPrimitiveProgram *m_pPrimitiveProgramTextured;
	CGLSLTileProgram *m_pBorderTileProgram;
	CGLSLTileProgram *m_pBorderTileProgramTextured;
	CGLSLTileProgram *m_pBorderTileLineProgram;
	CGLSLTileProgram *m_pBorderTileLineProgramTextured;
	CGLSLQuadProgram *m_pQuadProgram;
	CGLSLQuadProgram *m_pQuadProgramTextured;
	CGLSLTextProgram *m_pTextProgram;
	CGLSLPrimitiveExProgram *m_pPrimitiveExProgram;
	CGLSLPrimitiveExProgram *m_pPrimitiveExProgramTextured;
	CGLSLPrimitiveExProgram *m_pPrimitiveExProgramRotationless;
	CGLSLPrimitiveExProgram *m_pPrimitiveExProgramTexturedRotationless;
	CGLSLSpriteMultipleProgram *m_pSpriteProgramMultiple;

	TWGLuint m_LastProgramID;

	TWGLuint m_PrimitiveDrawVertexID[MAX_STREAM_BUFFER_COUNT];
	TWGLuint m_PrimitiveDrawVertexIDTex3D;
	TWGLuint m_PrimitiveDrawBufferID[MAX_STREAM_BUFFER_COUNT];
	TWGLuint m_PrimitiveDrawBufferIDTex3D;

	TWGLuint m_LastIndexBufferBound[MAX_STREAM_BUFFER_COUNT];

	int m_LastStreamBuffer;

	TWGLuint m_QuadDrawIndexBufferID;
	unsigned int m_CurrentIndicesInBuffer;

	void DestroyBufferContainer(int Index, bool DeleteBOs = true);

	void AppendIndices(unsigned int NewIndicesCount);

	struct SBufferContainer
	{
		SBufferContainer() :
			m_VertArrayID(0), m_LastIndexBufferBound(0) {}
		TWGLuint m_VertArrayID;
		TWGLuint m_LastIndexBufferBound;
		SBufferContainerInfo m_ContainerInfo;
	};
	std::vector<SBufferContainer> m_BufferContainers;

	std::vector<TWGLuint> m_BufferObjectIndices;

	CCommandBuffer::SColorf m_ClearColor;

	void InitPrimExProgram(CGLSLPrimitiveExProgram *pProgram, class CGLSLCompiler *pCompiler, class IStorage *pStorage, bool Textured, bool Rotationless);

	static int TexFormatToNewOpenGLFormat(int TexFormat);
	bool IsNewApi() override { return true; }

	void UseProgram(CGLSLTWProgram *pProgram);
	void UploadStreamBufferData(unsigned int PrimitiveType, const void *pVertices, size_t VertSize, unsigned int PrimitiveCount, bool AsTex3D = false);
	void RenderText(const CCommandBuffer::SState &State, int DrawNum, int TextTextureIndex, int TextOutlineTextureIndex, int TextureSize, const float *pTextColor, const float *pTextOutlineColor);

	bool Cmd_Init(const SCommand_Init *pCommand) override;
	void Cmd_Shutdown(const SCommand_Shutdown *pCommand) override;
	void Cmd_Texture_Update(const CCommandBuffer::SCommand_Texture_Update *pCommand) override;
	void Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand) override;
	void Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand) override;
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
	void Cmd_RenderBorderTileLine(const CCommandBuffer::SCommand_RenderBorderTileLine *pCommand) override;
	void Cmd_RenderQuadLayer(const CCommandBuffer::SCommand_RenderQuadLayer *pCommand) override;
	void Cmd_RenderText(const CCommandBuffer::SCommand_RenderText *pCommand) override;
	void Cmd_RenderTextStream(const CCommandBuffer::SCommand_RenderTextStream *pCommand) override;
	void Cmd_RenderQuadContainer(const CCommandBuffer::SCommand_RenderQuadContainer *pCommand) override;
	void Cmd_RenderQuadContainerEx(const CCommandBuffer::SCommand_RenderQuadContainerEx *pCommand) override;
	void Cmd_RenderQuadContainerAsSpriteMultiple(const CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple *pCommand) override;

public:
	CCommandProcessorFragment_OpenGL3_3() = default;
};

#endif
