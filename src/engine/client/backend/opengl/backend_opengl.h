// This file can be included several times.
#if(!defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL_H)) || \
	(defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL_H_AS_ES))

#if !defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL_H)
#define ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL_H
#endif

#if defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL_H_AS_ES)
#define ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL_H_AS_ES
#endif

#include <base/system.h>

#include <engine/client/graphics_defines.h>

#include <engine/client/backend/backend_base.h>

class CGLSLTWProgram;
class CGLSLPrimitiveProgram;
class CGLSLTileProgram;

#if defined(BACKEND_AS_OPENGL_ES) && defined(CONF_BACKEND_OPENGL_ES3)
#define BACKEND_GL_MODERN_API 1
#endif

// takes care of opengl related rendering
class CCommandProcessorFragment_OpenGL : public CCommandProcessorFragment_GLBase
{
protected:
	struct CTexture
	{
		CTexture() :
			m_Tex(0), m_Tex2DArray(0), m_Sampler(0), m_Sampler2DArray(0), m_LastWrapMode(CCommandBuffer::WRAP_REPEAT), m_MemSize(0), m_Width(0), m_Height(0), m_RescaleCount(0), m_ResizeWidth(0), m_ResizeHeight(0)
		{
		}

		TWGLuint m_Tex;
		TWGLuint m_Tex2DArray; // or 3D texture as fallback
		TWGLuint m_Sampler;
		TWGLuint m_Sampler2DArray; // or 3D texture as fallback
		int m_LastWrapMode;

		int m_MemSize;

		int m_Width;
		int m_Height;
		int m_RescaleCount;
		float m_ResizeWidth;
		float m_ResizeHeight;
	};
	std::vector<CTexture> m_vTextures;
	std::atomic<uint64_t> *m_pTextureMemoryUsage;

	uint32_t m_CanvasWidth = 0;
	uint32_t m_CanvasHeight = 0;

	TWGLint m_MaxTexSize;

	bool m_Has2DArrayTextures;
	bool m_Has2DArrayTexturesAsExtension;
	TWGLenum m_2DArrayTarget;
	bool m_Has3DTextures;
	bool m_HasMipMaps;
	bool m_HasNPOTTextures;

	bool m_HasShaders;
	int m_LastBlendMode; // avoid all possible opengl state changes
	bool m_LastClipEnable;

	int m_OpenGLTextureLodBIAS;

	bool m_IsOpenGLES;

	bool IsTexturedState(const CCommandBuffer::SState &State);

	bool InitOpenGL(const SCommand_Init *pCommand);

	void SetState(const CCommandBuffer::SState &State, bool Use2DArrayTexture = false);
	virtual bool IsNewApi() { return false; }
	void DestroyTexture(int Slot);

	bool GetPresentedImageData(uint32_t &Width, uint32_t &Height, CImageInfo::EImageFormat &Format, std::vector<uint8_t> &vDstData) override;

	static size_t GLFormatToPixelSize(int GLFormat);

	void TextureUpdate(int Slot, int X, int Y, int Width, int Height, int GLFormat, uint8_t *pTexData);
	void TextureCreate(int Slot, int Width, int Height, int GLFormat, int GLStoreFormat, int Flags, uint8_t *pTexData);

	virtual bool Cmd_Init(const SCommand_Init *pCommand);
	virtual void Cmd_Shutdown(const SCommand_Shutdown *pCommand) {}
	virtual void Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand);
	virtual void Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand);
	virtual void Cmd_TextTexture_Update(const CCommandBuffer::SCommand_TextTexture_Update *pCommand);
	virtual void Cmd_TextTextures_Destroy(const CCommandBuffer::SCommand_TextTextures_Destroy *pCommand);
	virtual void Cmd_TextTextures_Create(const CCommandBuffer::SCommand_TextTextures_Create *pCommand);
	virtual void Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand);
	virtual void Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand);
	virtual void Cmd_RenderTex3D(const CCommandBuffer::SCommand_RenderTex3D *pCommand) { dbg_assert(false, "Call of unsupported Cmd_RenderTex3D"); }
	virtual void Cmd_ReadPixel(const CCommandBuffer::SCommand_TrySwapAndReadPixel *pCommand);
	virtual void Cmd_Screenshot(const CCommandBuffer::SCommand_TrySwapAndScreenshot *pCommand);

	virtual void Cmd_Update_Viewport(const CCommandBuffer::SCommand_Update_Viewport *pCommand);

	virtual void Cmd_CreateBufferObject(const CCommandBuffer::SCommand_CreateBufferObject *pCommand) { dbg_assert(false, "Call of unsupported Cmd_CreateBufferObject"); }
	virtual void Cmd_RecreateBufferObject(const CCommandBuffer::SCommand_RecreateBufferObject *pCommand) { dbg_assert(false, "Call of unsupported Cmd_RecreateBufferObject"); }
	virtual void Cmd_UpdateBufferObject(const CCommandBuffer::SCommand_UpdateBufferObject *pCommand) { dbg_assert(false, "Call of unsupported Cmd_UpdateBufferObject"); }
	virtual void Cmd_CopyBufferObject(const CCommandBuffer::SCommand_CopyBufferObject *pCommand) { dbg_assert(false, "Call of unsupported Cmd_CopyBufferObject"); }
	virtual void Cmd_DeleteBufferObject(const CCommandBuffer::SCommand_DeleteBufferObject *pCommand) { dbg_assert(false, "Call of unsupported Cmd_DeleteBufferObject"); }

	virtual void Cmd_CreateBufferContainer(const CCommandBuffer::SCommand_CreateBufferContainer *pCommand) { dbg_assert(false, "Call of unsupported Cmd_CreateBufferContainer"); }
	virtual void Cmd_UpdateBufferContainer(const CCommandBuffer::SCommand_UpdateBufferContainer *pCommand) { dbg_assert(false, "Call of unsupported Cmd_UpdateBufferContainer"); }
	virtual void Cmd_DeleteBufferContainer(const CCommandBuffer::SCommand_DeleteBufferContainer *pCommand) { dbg_assert(false, "Call of unsupported Cmd_DeleteBufferContainer"); }
	virtual void Cmd_IndicesRequiredNumNotify(const CCommandBuffer::SCommand_IndicesRequiredNumNotify *pCommand) { dbg_assert(false, "Call of unsupported Cmd_IndicesRequiredNumNotify"); }

	virtual void Cmd_RenderTileLayer(const CCommandBuffer::SCommand_RenderTileLayer *pCommand) { dbg_assert(false, "Call of unsupported Cmd_RenderTileLayer"); }
	virtual void Cmd_RenderBorderTile(const CCommandBuffer::SCommand_RenderBorderTile *pCommand) { dbg_assert(false, "Call of unsupported Cmd_RenderBorderTile"); }
	virtual void Cmd_RenderQuadLayer(const CCommandBuffer::SCommand_RenderQuadLayer *pCommand) { dbg_assert(false, "Call of unsupported Cmd_RenderQuadLayer"); }
	virtual void Cmd_RenderText(const CCommandBuffer::SCommand_RenderText *pCommand) { dbg_assert(false, "Call of unsupported Cmd_RenderText"); }
	virtual void Cmd_RenderQuadContainer(const CCommandBuffer::SCommand_RenderQuadContainer *pCommand) { dbg_assert(false, "Call of unsupported Cmd_RenderQuadContainer"); }
	virtual void Cmd_RenderQuadContainerEx(const CCommandBuffer::SCommand_RenderQuadContainerEx *pCommand) { dbg_assert(false, "Call of unsupported Cmd_RenderQuadContainerEx"); }
	virtual void Cmd_RenderQuadContainerAsSpriteMultiple(const CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple *pCommand) { dbg_assert(false, "Call of unsupported Cmd_RenderQuadContainerAsSpriteMultiple"); }

public:
	CCommandProcessorFragment_OpenGL();
	virtual ~CCommandProcessorFragment_OpenGL() = default;

	ERunCommandReturnTypes RunCommand(const CCommandBuffer::SCommand *pBaseCommand) override;
};

class CCommandProcessorFragment_OpenGL2 : public CCommandProcessorFragment_OpenGL
{
	struct SBufferContainer
	{
		SBufferContainerInfo m_ContainerInfo;
	};
	std::vector<SBufferContainer> m_vBufferContainers;

#ifndef BACKEND_AS_OPENGL_ES
	GL_SVertexTex3D m_aStreamVertices[1024 * 4];
#endif

	struct SBufferObject
	{
		SBufferObject(TWGLuint BufferObjectId) :
			m_BufferObjectId(BufferObjectId)
		{
			m_pData = NULL;
			m_DataSize = 0;
		}
		TWGLuint m_BufferObjectId;
		uint8_t *m_pData;
		size_t m_DataSize;
	};

	std::vector<SBufferObject> m_vBufferObjectIndices;

#ifndef BACKEND_GL_MODERN_API
	bool DoAnalyzeStep(size_t CheckCount, size_t VerticesCount, uint8_t aFakeTexture[], size_t SingleImageSize);
	bool IsTileMapAnalysisSucceeded();
#endif

	void UseProgram(CGLSLTWProgram *pProgram);

protected:
	void SetState(const CCommandBuffer::SState &State, CGLSLTWProgram *pProgram, bool Use2DArrayTextures = false);

#ifndef BACKEND_GL_MODERN_API
	bool Cmd_Init(const SCommand_Init *pCommand) override;
	void Cmd_Shutdown(const SCommand_Shutdown *pCommand) override;

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
#endif

	CGLSLTileProgram *m_pTileProgram;
	CGLSLTileProgram *m_pTileProgramTextured;
	CGLSLTileProgram *m_pBorderTileProgram;
	CGLSLTileProgram *m_pBorderTileProgramTextured;
	CGLSLPrimitiveProgram *m_pPrimitive3DProgram;
	CGLSLPrimitiveProgram *m_pPrimitive3DProgramTextured;
};

class CCommandProcessorFragment_OpenGL3 : public CCommandProcessorFragment_OpenGL2
{
};

#if defined(BACKEND_AS_OPENGL_ES) && defined(CONF_BACKEND_OPENGL_ES3)
#undef BACKEND_GL_MODERN_API
#endif

#endif
