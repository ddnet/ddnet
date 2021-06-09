// This file can be included several times.
#if(!defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL_H)) || \
	(defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL_H_AS_ES))

#if !defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL_H)
#define ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL_H
#endif

#if defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL_H_AS_ES)
#define ENGINE_CLIENT_BACKEND_OPENGL_BACKEND_OPENGL_H_AS_ES
#endif

#include <engine/client/backend_sdl.h>

class CGLSLProgram;
class CGLSLTWProgram;
class CGLSLPrimitiveProgram;
class CGLSLQuadProgram;
class CGLSLTileProgram;
class CGLSLTextProgram;
class CGLSLPrimitiveExProgram;
class CGLSLSpriteMultipleProgram;

#if defined(BACKEND_AS_OPENGL_ES) && defined(CONF_BACKEND_OPENGL_ES3)
#define BACKEND_GL_MODERN_API 1
#endif

// takes care of opengl related rendering
class CCommandProcessorFragment_OpenGL : public CCommandProcessorFragment_OpenGLBase
{
protected:
	struct CTexture
	{
		CTexture() :
			m_Tex(0), m_Tex2DArray(0), m_Sampler(0), m_Sampler2DArray(0), m_LastWrapMode(CCommandBuffer::WRAP_REPEAT), m_MemSize(0), m_Width(0), m_Height(0), m_RescaleCount(0), m_ResizeWidth(0), m_ResizeHeight(0)
		{
		}

		TWGLuint m_Tex;
		TWGLuint m_Tex2DArray; //or 3D texture as fallback
		TWGLuint m_Sampler;
		TWGLuint m_Sampler2DArray; //or 3D texture as fallback
		int m_LastWrapMode;

		int m_MemSize;

		int m_Width;
		int m_Height;
		int m_RescaleCount;
		float m_ResizeWidth;
		float m_ResizeHeight;
	};
	std::vector<CTexture> m_Textures;
	std::atomic<int> *m_pTextureMemoryUsage;

	TWGLint m_MaxTexSize;

	bool m_Has2DArrayTextures;
	bool m_Has2DArrayTexturesAsExtension;
	TWGLenum m_2DArrayTarget;
	bool m_Has3DTextures;
	bool m_HasMipMaps;
	bool m_HasNPOTTextures;

	bool m_HasShaders;
	int m_LastBlendMode; //avoid all possible opengl state changes
	bool m_LastClipEnable;

	int m_OpenGLTextureLodBIAS;

	bool m_IsOpenGLES;

protected:
	bool IsTexturedState(const CCommandBuffer::SState &State);
	static bool Texture2DTo3D(void *pImageBuffer, int ImageWidth, int ImageHeight, int ImageColorChannelCount, int SplitCountWidth, int SplitCountHeight, void *pTarget3DImageData, int &Target3DImageWidth, int &Target3DImageHeight);

	bool InitOpenGL(const SCommand_Init *pCommand);

	void SetState(const CCommandBuffer::SState &State, bool Use2DArrayTexture = false);
	virtual bool IsNewApi() { return false; }
	void DestroyTexture(int Slot);

	static int TexFormatToOpenGLFormat(int TexFormat);
	static int TexFormatToImageColorChannelCount(int TexFormat);
	static void *Resize(int Width, int Height, int NewWidth, int NewHeight, int Format, const unsigned char *pData);

	virtual bool Cmd_Init(const SCommand_Init *pCommand);
	virtual void Cmd_Shutdown(const SCommand_Shutdown *pCommand) {}
	virtual void Cmd_Texture_Update(const CCommandBuffer::SCommand_Texture_Update *pCommand);
	virtual void Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand);
	virtual void Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand);
	virtual void Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand);
	virtual void Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand);
	virtual void Cmd_RenderTex3D(const CCommandBuffer::SCommand_RenderTex3D *pCommand) {}
	virtual void Cmd_Screenshot(const CCommandBuffer::SCommand_Screenshot *pCommand);

	virtual void Cmd_Update_Viewport(const CCommandBuffer::SCommand_Update_Viewport *pCommand);
	virtual void Cmd_Finish(const CCommandBuffer::SCommand_Finish *pCommand);

	virtual void Cmd_CreateBufferObject(const CCommandBuffer::SCommand_CreateBufferObject *pCommand) {}
	virtual void Cmd_RecreateBufferObject(const CCommandBuffer::SCommand_RecreateBufferObject *pCommand) {}
	virtual void Cmd_UpdateBufferObject(const CCommandBuffer::SCommand_UpdateBufferObject *pCommand) {}
	virtual void Cmd_CopyBufferObject(const CCommandBuffer::SCommand_CopyBufferObject *pCommand) {}
	virtual void Cmd_DeleteBufferObject(const CCommandBuffer::SCommand_DeleteBufferObject *pCommand) {}

	virtual void Cmd_CreateBufferContainer(const CCommandBuffer::SCommand_CreateBufferContainer *pCommand) {}
	virtual void Cmd_UpdateBufferContainer(const CCommandBuffer::SCommand_UpdateBufferContainer *pCommand) {}
	virtual void Cmd_DeleteBufferContainer(const CCommandBuffer::SCommand_DeleteBufferContainer *pCommand) {}
	virtual void Cmd_IndicesRequiredNumNotify(const CCommandBuffer::SCommand_IndicesRequiredNumNotify *pCommand) {}

	virtual void Cmd_RenderTileLayer(const CCommandBuffer::SCommand_RenderTileLayer *pCommand) {}
	virtual void Cmd_RenderBorderTile(const CCommandBuffer::SCommand_RenderBorderTile *pCommand) {}
	virtual void Cmd_RenderBorderTileLine(const CCommandBuffer::SCommand_RenderBorderTileLine *pCommand) {}
	virtual void Cmd_RenderQuadLayer(const CCommandBuffer::SCommand_RenderQuadLayer *pCommand) {}
	virtual void Cmd_RenderText(const CCommandBuffer::SCommand_RenderText *pCommand) {}
	virtual void Cmd_RenderTextStream(const CCommandBuffer::SCommand_RenderTextStream *pCommand) {}
	virtual void Cmd_RenderQuadContainer(const CCommandBuffer::SCommand_RenderQuadContainer *pCommand) {}
	virtual void Cmd_RenderQuadContainerEx(const CCommandBuffer::SCommand_RenderQuadContainerEx *pCommand) {}
	virtual void Cmd_RenderQuadContainerAsSpriteMultiple(const CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple *pCommand) {}

public:
	CCommandProcessorFragment_OpenGL();
	virtual ~CCommandProcessorFragment_OpenGL() = default;

	virtual bool RunCommand(const CCommandBuffer::SCommand *pBaseCommand);
};

class CCommandProcessorFragment_OpenGL2 : public CCommandProcessorFragment_OpenGL
{
	struct SBufferContainer
	{
		SBufferContainer() {}
		SBufferContainerInfo m_ContainerInfo;
	};
	std::vector<SBufferContainer> m_BufferContainers;

#ifndef BACKEND_AS_OPENGL_ES
	GL_SVertexTex3D m_aStreamVertices[1024 * 4];
#endif

	struct SBufferObject
	{
		SBufferObject(TWGLuint BufferObjectID) :
			m_BufferObjectID(BufferObjectID)
		{
			m_pData = NULL;
			m_DataSize = 0;
		}
		TWGLuint m_BufferObjectID;
		void *m_pData;
		size_t m_DataSize;
	};

	std::vector<SBufferObject> m_BufferObjectIndices;

#ifndef BACKEND_GL_MODERN_API
	bool DoAnalyzeStep(size_t StepN, size_t CheckCount, size_t VerticesCount, uint8_t aFakeTexture[], size_t SingleImageSize);
	bool IsTileMapAnalysisSucceeded();

	void RenderBorderTileEmulation(SBufferContainer &BufferContainer, const CCommandBuffer::SState &State, const float *pColor, const char *pBuffOffset, unsigned int DrawNum, const float *pOffset, const float *pDir, int JumpIndex);
	void RenderBorderTileLineEmulation(SBufferContainer &BufferContainer, const CCommandBuffer::SState &State, const float *pColor, const char *pBuffOffset, unsigned int IndexDrawNum, unsigned int DrawNum, const float *pOffset, const float *pDir);
#endif

	void UseProgram(CGLSLTWProgram *pProgram);

protected:
	void SetState(const CCommandBuffer::SState &State, CGLSLTWProgram *pProgram, bool Use2DArrayTextures = false);

#ifndef BACKEND_GL_MODERN_API
	bool Cmd_Init(const SCommand_Init *pCommand) override;

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
#endif

	CGLSLTileProgram *m_pTileProgram;
	CGLSLTileProgram *m_pTileProgramTextured;
	CGLSLPrimitiveProgram *m_pPrimitive3DProgram;
	CGLSLPrimitiveProgram *m_pPrimitive3DProgramTextured;

	bool m_UseMultipleTextureUnits;

	TWGLint m_MaxTextureUnits;

	struct STextureBound
	{
		int m_TextureSlot;
		bool m_Is2DArray;
	};
	std::vector<STextureBound> m_TextureSlotBoundToUnit; //the texture index generated by loadtextureraw is stored in an index calculated by max texture units

	bool IsAndUpdateTextureSlotBound(int IDX, int Slot, bool Is2DArray = false);

public:
	CCommandProcessorFragment_OpenGL2() :
		CCommandProcessorFragment_OpenGL(), m_UseMultipleTextureUnits(false) {}
};

class CCommandProcessorFragment_OpenGL3 : public CCommandProcessorFragment_OpenGL2
{
};

#if defined(BACKEND_AS_OPENGL_ES) && defined(CONF_BACKEND_OPENGL_ES3)
#undef BACKEND_GL_MODERN_API
#endif

#endif
