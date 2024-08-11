#include "backend_opengl3.h"

#include <base/detect.h>

#if defined(BACKEND_AS_OPENGL_ES) || !defined(CONF_BACKEND_OPENGL_ES)

#ifndef BACKEND_AS_OPENGL_ES
#include <GL/glew.h>
#else
#include <GLES3/gl3.h>
#endif

#include <engine/client/backend_sdl.h>

#include <engine/client/backend/opengl/opengl_sl.h>
#include <engine/client/backend/opengl/opengl_sl_program.h>

#include <engine/gfx/image_manipulation.h>

#include <engine/client/backend/glsl_shader_compiler.h>

#ifdef CONF_WEBASM
// WebGL2 defines the type of a buffer at the first bind to a buffer target
// this is different to GLES 3 (https://www.khronos.org/registry/webgl/specs/latest/2.0/#5.1)
static constexpr GLenum BUFFER_INIT_INDEX_TARGET = GL_ELEMENT_ARRAY_BUFFER;
static constexpr GLenum BUFFER_INIT_VERTEX_TARGET = GL_ARRAY_BUFFER;
#else
static constexpr GLenum BUFFER_INIT_INDEX_TARGET = GL_COPY_WRITE_BUFFER;
static constexpr GLenum BUFFER_INIT_VERTEX_TARGET = GL_COPY_WRITE_BUFFER;
#endif

// ------------ CCommandProcessorFragment_OpenGL3_3
void CCommandProcessorFragment_OpenGL3_3::UseProgram(CGLSLTWProgram *pProgram)
{
	if(m_LastProgramId != pProgram->GetProgramId())
	{
		pProgram->UseProgram();
		m_LastProgramId = pProgram->GetProgramId();
	}
}

void CCommandProcessorFragment_OpenGL3_3::InitPrimExProgram(CGLSLPrimitiveExProgram *pProgram, CGLSLCompiler *pCompiler, IStorage *pStorage, bool Textured, bool Rotationless)
{
	CGLSL PrimitiveVertexShader;
	CGLSL PrimitiveFragmentShader;
	if(Textured)
		pCompiler->AddDefine("TW_TEXTURED", "");
	if(Rotationless)
		pCompiler->AddDefine("TW_ROTATIONLESS", "");
	PrimitiveVertexShader.LoadShader(pCompiler, pStorage, "shader/primex.vert", GL_VERTEX_SHADER);
	PrimitiveFragmentShader.LoadShader(pCompiler, pStorage, "shader/primex.frag", GL_FRAGMENT_SHADER);
	if(Textured || Rotationless)
		pCompiler->ClearDefines();

	pProgram->CreateProgram();
	pProgram->AddShader(&PrimitiveVertexShader);
	pProgram->AddShader(&PrimitiveFragmentShader);
	pProgram->LinkProgram();

	UseProgram(pProgram);

	pProgram->m_LocPos = pProgram->GetUniformLoc("gPos");
	pProgram->m_LocTextureSampler = pProgram->GetUniformLoc("gTextureSampler");
	pProgram->m_LocRotation = pProgram->GetUniformLoc("gRotation");
	pProgram->m_LocCenter = pProgram->GetUniformLoc("gCenter");
	pProgram->m_LocVertciesColor = pProgram->GetUniformLoc("gVerticesColor");

	pProgram->SetUniform(pProgram->m_LocRotation, 0.0f);
	float aCenter[2] = {0.f, 0.f};
	pProgram->SetUniformVec2(pProgram->m_LocCenter, 1, aCenter);
}

bool CCommandProcessorFragment_OpenGL3_3::Cmd_Init(const SCommand_Init *pCommand)
{
	if(!InitOpenGL(pCommand))
		return false;

	m_OpenGLTextureLodBIAS = g_Config.m_GfxGLTextureLODBIAS;

	glActiveTexture(GL_TEXTURE0);

	m_Has2DArrayTextures = true;
	m_Has2DArrayTexturesAsExtension = false;
	m_2DArrayTarget = GL_TEXTURE_2D_ARRAY;
	m_Has3DTextures = false;
	m_HasMipMaps = true;
	m_HasNPOTTextures = true;
	m_HasShaders = true;

	m_pTextureMemoryUsage = pCommand->m_pTextureMemoryUsage;
	m_pTextureMemoryUsage->store(0, std::memory_order_relaxed);
	m_LastBlendMode = CCommandBuffer::BLEND_ALPHA;
	m_LastClipEnable = false;
	m_pPrimitiveProgram = new CGLSLPrimitiveProgram;
	m_pPrimitiveProgramTextured = new CGLSLPrimitiveProgram;
	m_pTileProgram = new CGLSLTileProgram;
	m_pTileProgramTextured = new CGLSLTileProgram;
	m_pPrimitive3DProgram = new CGLSLPrimitiveProgram;
	m_pPrimitive3DProgramTextured = new CGLSLPrimitiveProgram;
	m_pBorderTileProgram = new CGLSLTileProgram;
	m_pBorderTileProgramTextured = new CGLSLTileProgram;
	m_pQuadProgram = new CGLSLQuadProgram;
	m_pQuadProgramTextured = new CGLSLQuadProgram;
	m_pTextProgram = new CGLSLTextProgram;
	m_pPrimitiveExProgram = new CGLSLPrimitiveExProgram;
	m_pPrimitiveExProgramTextured = new CGLSLPrimitiveExProgram;
	m_pPrimitiveExProgramRotationless = new CGLSLPrimitiveExProgram;
	m_pPrimitiveExProgramTexturedRotationless = new CGLSLPrimitiveExProgram;
	m_pSpriteProgramMultiple = new CGLSLSpriteMultipleProgram;
	m_LastProgramId = 0;

	CGLSLCompiler ShaderCompiler(g_Config.m_GfxGLMajor, g_Config.m_GfxGLMinor, g_Config.m_GfxGLPatch, m_IsOpenGLES, m_OpenGLTextureLodBIAS / 1000.0f);

	GLint CapVal;
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &CapVal);

	m_MaxQuadsAtOnce = minimum<int>(((CapVal - 20) / (3 * 4)), ms_MaxQuadsPossible);

	{
		CGLSL PrimitiveVertexShader;
		CGLSL PrimitiveFragmentShader;
		PrimitiveVertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/prim.vert", GL_VERTEX_SHADER);
		PrimitiveFragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/prim.frag", GL_FRAGMENT_SHADER);

		m_pPrimitiveProgram->CreateProgram();
		m_pPrimitiveProgram->AddShader(&PrimitiveVertexShader);
		m_pPrimitiveProgram->AddShader(&PrimitiveFragmentShader);
		m_pPrimitiveProgram->LinkProgram();

		UseProgram(m_pPrimitiveProgram);

		m_pPrimitiveProgram->m_LocPos = m_pPrimitiveProgram->GetUniformLoc("gPos");
		m_pPrimitiveProgram->m_LocTextureSampler = m_pPrimitiveProgram->GetUniformLoc("gTextureSampler");
	}
	{
		CGLSL PrimitiveVertexShader;
		CGLSL PrimitiveFragmentShader;
		ShaderCompiler.AddDefine("TW_TEXTURED", "");
		PrimitiveVertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/prim.vert", GL_VERTEX_SHADER);
		PrimitiveFragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/prim.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pPrimitiveProgramTextured->CreateProgram();
		m_pPrimitiveProgramTextured->AddShader(&PrimitiveVertexShader);
		m_pPrimitiveProgramTextured->AddShader(&PrimitiveFragmentShader);
		m_pPrimitiveProgramTextured->LinkProgram();

		UseProgram(m_pPrimitiveProgramTextured);

		m_pPrimitiveProgramTextured->m_LocPos = m_pPrimitiveProgramTextured->GetUniformLoc("gPos");
		m_pPrimitiveProgramTextured->m_LocTextureSampler = m_pPrimitiveProgramTextured->GetUniformLoc("gTextureSampler");
	}

	{
		CGLSL PrimitiveVertexShader;
		CGLSL PrimitiveFragmentShader;
		ShaderCompiler.AddDefine("TW_MODERN_GL", "");
		PrimitiveVertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/pipeline.vert", GL_VERTEX_SHADER);
		PrimitiveFragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/pipeline.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pPrimitive3DProgram->CreateProgram();
		m_pPrimitive3DProgram->AddShader(&PrimitiveVertexShader);
		m_pPrimitive3DProgram->AddShader(&PrimitiveFragmentShader);
		m_pPrimitive3DProgram->LinkProgram();

		UseProgram(m_pPrimitive3DProgram);

		m_pPrimitive3DProgram->m_LocPos = m_pPrimitive3DProgram->GetUniformLoc("gPos");
	}
	{
		CGLSL PrimitiveVertexShader;
		CGLSL PrimitiveFragmentShader;
		ShaderCompiler.AddDefine("TW_MODERN_GL", "");
		ShaderCompiler.AddDefine("TW_TEXTURED", "");
		if(!pCommand->m_pCapabilities->m_2DArrayTextures)
			ShaderCompiler.AddDefine("TW_3D_TEXTURED", "");
		PrimitiveVertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/pipeline.vert", GL_VERTEX_SHADER);
		PrimitiveFragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/pipeline.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pPrimitive3DProgramTextured->CreateProgram();
		m_pPrimitive3DProgramTextured->AddShader(&PrimitiveVertexShader);
		m_pPrimitive3DProgramTextured->AddShader(&PrimitiveFragmentShader);
		m_pPrimitive3DProgramTextured->LinkProgram();

		UseProgram(m_pPrimitive3DProgramTextured);

		m_pPrimitive3DProgramTextured->m_LocPos = m_pPrimitive3DProgramTextured->GetUniformLoc("gPos");
		m_pPrimitive3DProgramTextured->m_LocTextureSampler = m_pPrimitive3DProgramTextured->GetUniformLoc("gTextureSampler");
	}

	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.frag", GL_FRAGMENT_SHADER);

		m_pTileProgram->CreateProgram();
		m_pTileProgram->AddShader(&VertexShader);
		m_pTileProgram->AddShader(&FragmentShader);
		m_pTileProgram->LinkProgram();

		UseProgram(m_pTileProgram);

		m_pTileProgram->m_LocPos = m_pTileProgram->GetUniformLoc("gPos");
		m_pTileProgram->m_LocColor = m_pTileProgram->GetUniformLoc("gVertColor");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		ShaderCompiler.AddDefine("TW_TILE_TEXTURED", "");
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pTileProgramTextured->CreateProgram();
		m_pTileProgramTextured->AddShader(&VertexShader);
		m_pTileProgramTextured->AddShader(&FragmentShader);
		m_pTileProgramTextured->LinkProgram();

		UseProgram(m_pTileProgramTextured);

		m_pTileProgramTextured->m_LocPos = m_pTileProgramTextured->GetUniformLoc("gPos");
		m_pTileProgramTextured->m_LocTextureSampler = m_pTileProgramTextured->GetUniformLoc("gTextureSampler");
		m_pTileProgramTextured->m_LocColor = m_pTileProgramTextured->GetUniformLoc("gVertColor");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile_border.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile_border.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pBorderTileProgram->CreateProgram();
		m_pBorderTileProgram->AddShader(&VertexShader);
		m_pBorderTileProgram->AddShader(&FragmentShader);
		m_pBorderTileProgram->LinkProgram();

		UseProgram(m_pBorderTileProgram);

		m_pBorderTileProgram->m_LocPos = m_pBorderTileProgram->GetUniformLoc("gPos");
		m_pBorderTileProgram->m_LocColor = m_pBorderTileProgram->GetUniformLoc("gVertColor");
		m_pBorderTileProgram->m_LocOffset = m_pBorderTileProgram->GetUniformLoc("gOffset");
		m_pBorderTileProgram->m_LocScale = m_pBorderTileProgram->GetUniformLoc("gScale");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		ShaderCompiler.AddDefine("TW_TILE_TEXTURED", "");
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile_border.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile_border.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pBorderTileProgramTextured->CreateProgram();
		m_pBorderTileProgramTextured->AddShader(&VertexShader);
		m_pBorderTileProgramTextured->AddShader(&FragmentShader);
		m_pBorderTileProgramTextured->LinkProgram();

		UseProgram(m_pBorderTileProgramTextured);

		m_pBorderTileProgramTextured->m_LocPos = m_pBorderTileProgramTextured->GetUniformLoc("gPos");
		m_pBorderTileProgramTextured->m_LocTextureSampler = m_pBorderTileProgramTextured->GetUniformLoc("gTextureSampler");
		m_pBorderTileProgramTextured->m_LocColor = m_pBorderTileProgramTextured->GetUniformLoc("gVertColor");
		m_pBorderTileProgramTextured->m_LocOffset = m_pBorderTileProgramTextured->GetUniformLoc("gOffset");
		m_pBorderTileProgramTextured->m_LocScale = m_pBorderTileProgramTextured->GetUniformLoc("gScale");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		ShaderCompiler.AddDefine("TW_MAX_QUADS", std::to_string(m_MaxQuadsAtOnce).c_str());
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/quad.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/quad.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pQuadProgram->CreateProgram();
		m_pQuadProgram->AddShader(&VertexShader);
		m_pQuadProgram->AddShader(&FragmentShader);
		m_pQuadProgram->LinkProgram();

		UseProgram(m_pQuadProgram);

		m_pQuadProgram->m_LocPos = m_pQuadProgram->GetUniformLoc("gPos");
		m_pQuadProgram->m_LocColors = m_pQuadProgram->GetUniformLoc("gVertColors");
		m_pQuadProgram->m_LocRotations = m_pQuadProgram->GetUniformLoc("gRotations");
		m_pQuadProgram->m_LocOffsets = m_pQuadProgram->GetUniformLoc("gOffsets");
		m_pQuadProgram->m_LocQuadOffset = m_pQuadProgram->GetUniformLoc("gQuadOffset");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		ShaderCompiler.AddDefine("TW_QUAD_TEXTURED", "");
		ShaderCompiler.AddDefine("TW_MAX_QUADS", std::to_string(m_MaxQuadsAtOnce).c_str());
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/quad.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/quad.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pQuadProgramTextured->CreateProgram();
		m_pQuadProgramTextured->AddShader(&VertexShader);
		m_pQuadProgramTextured->AddShader(&FragmentShader);
		m_pQuadProgramTextured->LinkProgram();

		UseProgram(m_pQuadProgramTextured);

		m_pQuadProgramTextured->m_LocPos = m_pQuadProgramTextured->GetUniformLoc("gPos");
		m_pQuadProgramTextured->m_LocTextureSampler = m_pQuadProgramTextured->GetUniformLoc("gTextureSampler");
		m_pQuadProgramTextured->m_LocColors = m_pQuadProgramTextured->GetUniformLoc("gVertColors");
		m_pQuadProgramTextured->m_LocRotations = m_pQuadProgramTextured->GetUniformLoc("gRotations");
		m_pQuadProgramTextured->m_LocOffsets = m_pQuadProgramTextured->GetUniformLoc("gOffsets");
		m_pQuadProgramTextured->m_LocQuadOffset = m_pQuadProgramTextured->GetUniformLoc("gQuadOffset");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/text.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/text.frag", GL_FRAGMENT_SHADER);

		m_pTextProgram->CreateProgram();
		m_pTextProgram->AddShader(&VertexShader);
		m_pTextProgram->AddShader(&FragmentShader);
		m_pTextProgram->LinkProgram();

		UseProgram(m_pTextProgram);

		m_pTextProgram->m_LocPos = m_pTextProgram->GetUniformLoc("gPos");
		m_pTextProgram->m_LocTextureSampler = -1;
		m_pTextProgram->m_LocTextSampler = m_pTextProgram->GetUniformLoc("gTextSampler");
		m_pTextProgram->m_LocTextOutlineSampler = m_pTextProgram->GetUniformLoc("gTextOutlineSampler");
		m_pTextProgram->m_LocColor = m_pTextProgram->GetUniformLoc("gVertColor");
		m_pTextProgram->m_LocOutlineColor = m_pTextProgram->GetUniformLoc("gVertOutlineColor");
		m_pTextProgram->m_LocTextureSize = m_pTextProgram->GetUniformLoc("gTextureSize");
	}
	InitPrimExProgram(m_pPrimitiveExProgram, &ShaderCompiler, pCommand->m_pStorage, false, false);
	InitPrimExProgram(m_pPrimitiveExProgramTextured, &ShaderCompiler, pCommand->m_pStorage, true, false);
	InitPrimExProgram(m_pPrimitiveExProgramRotationless, &ShaderCompiler, pCommand->m_pStorage, false, true);
	InitPrimExProgram(m_pPrimitiveExProgramTexturedRotationless, &ShaderCompiler, pCommand->m_pStorage, true, true);
	{
		CGLSL PrimitiveVertexShader;
		CGLSL PrimitiveFragmentShader;
		PrimitiveVertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/spritemulti.vert", GL_VERTEX_SHADER);
		PrimitiveFragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/spritemulti.frag", GL_FRAGMENT_SHADER);

		m_pSpriteProgramMultiple->CreateProgram();
		m_pSpriteProgramMultiple->AddShader(&PrimitiveVertexShader);
		m_pSpriteProgramMultiple->AddShader(&PrimitiveFragmentShader);
		m_pSpriteProgramMultiple->LinkProgram();

		UseProgram(m_pSpriteProgramMultiple);

		m_pSpriteProgramMultiple->m_LocPos = m_pSpriteProgramMultiple->GetUniformLoc("gPos");
		m_pSpriteProgramMultiple->m_LocTextureSampler = m_pSpriteProgramMultiple->GetUniformLoc("gTextureSampler");
		m_pSpriteProgramMultiple->m_LocRSP = m_pSpriteProgramMultiple->GetUniformLoc("gRSP[0]");
		m_pSpriteProgramMultiple->m_LocCenter = m_pSpriteProgramMultiple->GetUniformLoc("gCenter");
		m_pSpriteProgramMultiple->m_LocVertciesColor = m_pSpriteProgramMultiple->GetUniformLoc("gVerticesColor");

		float aCenter[2] = {0.f, 0.f};
		m_pSpriteProgramMultiple->SetUniformVec2(m_pSpriteProgramMultiple->m_LocCenter, 1, aCenter);
	}

	m_LastStreamBuffer = 0;

	glGenBuffers(MAX_STREAM_BUFFER_COUNT, m_aPrimitiveDrawBufferId);
	glGenVertexArrays(MAX_STREAM_BUFFER_COUNT, m_aPrimitiveDrawVertexId);
	glGenBuffers(1, &m_PrimitiveDrawBufferIdTex3D);
	glGenVertexArrays(1, &m_PrimitiveDrawVertexIdTex3D);

	for(int i = 0; i < MAX_STREAM_BUFFER_COUNT; ++i)
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_aPrimitiveDrawBufferId[i]);
		glBindVertexArray(m_aPrimitiveDrawVertexId[i]);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertex), 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertex), (void *)(sizeof(float) * 2));
		glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(CCommandBuffer::SVertex), (void *)(sizeof(float) * 4));

		m_aLastIndexBufferBound[i] = 0;
	}

	glBindBuffer(GL_ARRAY_BUFFER, m_PrimitiveDrawBufferIdTex3D);
	glBindVertexArray(m_PrimitiveDrawVertexIdTex3D);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertexTex3DStream), 0);
	glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(CCommandBuffer::SVertexTex3DStream), (void *)(sizeof(float) * 2));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertexTex3DStream), (void *)(sizeof(float) * 2 + sizeof(unsigned char) * 4));

	// query the image max size only once
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_MaxTexSize);

	glBindVertexArray(0);
	glGenBuffers(1, &m_QuadDrawIndexBufferId);
	glBindBuffer(BUFFER_INIT_INDEX_TARGET, m_QuadDrawIndexBufferId);

	unsigned int aIndices[CCommandBuffer::MAX_VERTICES / 4 * 6];
	int Primq = 0;
	for(int i = 0; i < CCommandBuffer::MAX_VERTICES / 4 * 6; i += 6)
	{
		aIndices[i] = Primq;
		aIndices[i + 1] = Primq + 1;
		aIndices[i + 2] = Primq + 2;
		aIndices[i + 3] = Primq;
		aIndices[i + 4] = Primq + 2;
		aIndices[i + 5] = Primq + 3;
		Primq += 4;
	}
	glBufferData(BUFFER_INIT_INDEX_TARGET, sizeof(unsigned int) * CCommandBuffer::MAX_VERTICES / 4 * 6, aIndices, GL_STATIC_DRAW);

	m_CurrentIndicesInBuffer = CCommandBuffer::MAX_VERTICES / 4 * 6;

	m_vTextures.resize(CCommandBuffer::MAX_TEXTURES);

	m_ClearColor.r = m_ClearColor.g = m_ClearColor.b = -1.f;

	// fix the alignment to allow even 1byte changes, e.g. for alpha components
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	return true;
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Shutdown(const SCommand_Shutdown *pCommand)
{
	glUseProgram(0);

	m_pPrimitiveProgram->DeleteProgram();
	m_pPrimitiveProgramTextured->DeleteProgram();
	m_pBorderTileProgram->DeleteProgram();
	m_pBorderTileProgramTextured->DeleteProgram();
	m_pQuadProgram->DeleteProgram();
	m_pQuadProgramTextured->DeleteProgram();
	m_pTileProgram->DeleteProgram();
	m_pTileProgramTextured->DeleteProgram();
	m_pPrimitive3DProgram->DeleteProgram();
	m_pPrimitive3DProgramTextured->DeleteProgram();
	m_pTextProgram->DeleteProgram();
	m_pPrimitiveExProgram->DeleteProgram();
	m_pPrimitiveExProgramTextured->DeleteProgram();
	m_pPrimitiveExProgramRotationless->DeleteProgram();
	m_pPrimitiveExProgramTexturedRotationless->DeleteProgram();
	m_pSpriteProgramMultiple->DeleteProgram();

	// clean up everything
	delete m_pPrimitiveProgram;
	delete m_pPrimitiveProgramTextured;
	delete m_pBorderTileProgram;
	delete m_pBorderTileProgramTextured;
	delete m_pQuadProgram;
	delete m_pQuadProgramTextured;
	delete m_pTileProgram;
	delete m_pTileProgramTextured;
	delete m_pPrimitive3DProgram;
	delete m_pPrimitive3DProgramTextured;
	delete m_pTextProgram;
	delete m_pPrimitiveExProgram;
	delete m_pPrimitiveExProgramTextured;
	delete m_pPrimitiveExProgramRotationless;
	delete m_pPrimitiveExProgramTexturedRotationless;
	delete m_pSpriteProgramMultiple;

	glBindVertexArray(0);
	glDeleteBuffers(MAX_STREAM_BUFFER_COUNT, m_aPrimitiveDrawBufferId);
	glDeleteBuffers(1, &m_QuadDrawIndexBufferId);
	glDeleteVertexArrays(MAX_STREAM_BUFFER_COUNT, m_aPrimitiveDrawVertexId);
	glDeleteBuffers(1, &m_PrimitiveDrawBufferIdTex3D);
	glDeleteVertexArrays(1, &m_PrimitiveDrawVertexIdTex3D);

	for(int i = 0; i < (int)m_vTextures.size(); ++i)
	{
		DestroyTexture(i);
	}

	for(size_t i = 0; i < m_vBufferContainers.size(); ++i)
	{
		DestroyBufferContainer(i);
	}

	m_vBufferContainers.clear();
}

void CCommandProcessorFragment_OpenGL3_3::TextureUpdate(int Slot, int X, int Y, int Width, int Height, int GLFormat, uint8_t *pTexData)
{
	glBindTexture(GL_TEXTURE_2D, m_vTextures[Slot].m_Tex);

	if(m_vTextures[Slot].m_RescaleCount > 0)
	{
		for(int i = 0; i < m_vTextures[Slot].m_RescaleCount; ++i)
		{
			Width >>= 1;
			Height >>= 1;

			X /= 2;
			Y /= 2;
		}

		uint8_t *pTmpData = ResizeImage(pTexData, Width, Height, Width, Height, GLFormatToPixelSize(GLFormat));
		free(pTexData);
		pTexData = pTmpData;
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, X, Y, Width, Height, GLFormat, GL_UNSIGNED_BYTE, pTexData);
	free(pTexData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand)
{
	int Slot = 0;
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindSampler(Slot, 0);
	DestroyTexture(pCommand->m_Slot);
}

void CCommandProcessorFragment_OpenGL3_3::TextureCreate(int Slot, int Width, int Height, int GLFormat, int GLStoreFormat, int Flags, uint8_t *pTexData)
{
	while(Slot >= (int)m_vTextures.size())
		m_vTextures.resize(m_vTextures.size() * 2);

	// resample if needed
	int RescaleCount = 0;
	if(GLFormat == GL_RGBA)
	{
		if(Width > m_MaxTexSize || Height > m_MaxTexSize)
		{
			do
			{
				Width >>= 1;
				Height >>= 1;
				++RescaleCount;
			} while(Width > m_MaxTexSize || Height > m_MaxTexSize);

			uint8_t *pTmpData = ResizeImage(pTexData, Width, Height, Width, Height, GLFormatToPixelSize(GLFormat));
			free(pTexData);
			pTexData = pTmpData;
		}
	}
	m_vTextures[Slot].m_Width = Width;
	m_vTextures[Slot].m_Height = Height;
	m_vTextures[Slot].m_RescaleCount = RescaleCount;

	if(GLStoreFormat == GL_RED)
		GLStoreFormat = GL_R8;
	const size_t PixelSize = GLFormatToPixelSize(GLFormat);

	int SamplerSlot = 0;

	if((Flags & CCommandBuffer::TEXFLAG_NO_2D_TEXTURE) == 0)
	{
		glGenTextures(1, &m_vTextures[Slot].m_Tex);
		glBindTexture(GL_TEXTURE_2D, m_vTextures[Slot].m_Tex);

		glGenSamplers(1, &m_vTextures[Slot].m_Sampler);
		glBindSampler(SamplerSlot, m_vTextures[Slot].m_Sampler);
	}

	if(Flags & CCommandBuffer::TEXFLAG_NOMIPMAPS)
	{
		if((Flags & CCommandBuffer::TEXFLAG_NO_2D_TEXTURE) == 0)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glSamplerParameteri(m_vTextures[Slot].m_Sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glSamplerParameteri(m_vTextures[Slot].m_Sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GLStoreFormat, Width, Height, 0, GLFormat, GL_UNSIGNED_BYTE, pTexData);
		}
	}
	else
	{
		if((Flags & CCommandBuffer::TEXFLAG_NO_2D_TEXTURE) == 0)
		{
			glSamplerParameteri(m_vTextures[Slot].m_Sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glSamplerParameteri(m_vTextures[Slot].m_Sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

#ifndef BACKEND_AS_OPENGL_ES
			if(m_OpenGLTextureLodBIAS != 0 && !m_IsOpenGLES)
				glSamplerParameterf(m_vTextures[Slot].m_Sampler, GL_TEXTURE_LOD_BIAS, ((GLfloat)m_OpenGLTextureLodBIAS / 1000.0f));
#endif

			// prevent mipmap display bugs, when zooming out far
			if(Width >= 1024 && Height >= 1024)
			{
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 5.f);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 5);
			}
			glTexImage2D(GL_TEXTURE_2D, 0, GLStoreFormat, Width, Height, 0, GLFormat, GL_UNSIGNED_BYTE, pTexData);
			glGenerateMipmap(GL_TEXTURE_2D);
		}

		if((Flags & (CCommandBuffer::TEXFLAG_TO_2D_ARRAY_TEXTURE)) != 0)
		{
			glGenTextures(1, &m_vTextures[Slot].m_Tex2DArray);
			glBindTexture(GL_TEXTURE_2D_ARRAY, m_vTextures[Slot].m_Tex2DArray);

			glGenSamplers(1, &m_vTextures[Slot].m_Sampler2DArray);
			glBindSampler(SamplerSlot, m_vTextures[Slot].m_Sampler2DArray);
			glSamplerParameteri(m_vTextures[Slot].m_Sampler2DArray, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glSamplerParameteri(m_vTextures[Slot].m_Sampler2DArray, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glSamplerParameteri(m_vTextures[Slot].m_Sampler2DArray, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(m_vTextures[Slot].m_Sampler2DArray, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(m_vTextures[Slot].m_Sampler2DArray, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);

#ifndef BACKEND_AS_OPENGL_ES
			if(m_OpenGLTextureLodBIAS != 0 && !m_IsOpenGLES)
				glSamplerParameterf(m_vTextures[Slot].m_Sampler2DArray, GL_TEXTURE_LOD_BIAS, ((GLfloat)m_OpenGLTextureLodBIAS / 1000.0f));
#endif

			uint8_t *p3DImageData = static_cast<uint8_t *>(malloc((size_t)Width * Height * PixelSize));
			int Image3DWidth, Image3DHeight;

			int ConvertWidth = Width;
			int ConvertHeight = Height;

			if(ConvertWidth == 0 || (ConvertWidth % 16) != 0 || ConvertHeight == 0 || (ConvertHeight % 16) != 0)
			{
				dbg_msg("gfx", "3D/2D array texture was resized");
				int NewWidth = maximum<int>(HighestBit(ConvertWidth), 16);
				int NewHeight = maximum<int>(HighestBit(ConvertHeight), 16);
				uint8_t *pNewTexData = ResizeImage(pTexData, ConvertWidth, ConvertHeight, NewWidth, NewHeight, GLFormatToPixelSize(GLFormat));

				ConvertWidth = NewWidth;
				ConvertHeight = NewHeight;

				free(pTexData);
				pTexData = pNewTexData;
			}

			if(Texture2DTo3D(pTexData, ConvertWidth, ConvertHeight, PixelSize, 16, 16, p3DImageData, Image3DWidth, Image3DHeight))
			{
				glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GLStoreFormat, Image3DWidth, Image3DHeight, 256, 0, GLFormat, GL_UNSIGNED_BYTE, p3DImageData);
				glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
			}

			free(p3DImageData);
		}
	}

	// This is the initial value for the wrap modes
	m_vTextures[Slot].m_LastWrapMode = CCommandBuffer::WRAP_REPEAT;

	// calculate memory usage
	m_vTextures[Slot].m_MemSize = (size_t)Width * Height * PixelSize;
	while(Width > 2 && Height > 2)
	{
		Width >>= 1;
		Height >>= 1;
		m_vTextures[Slot].m_MemSize += (size_t)Width * Height * PixelSize;
	}
	m_pTextureMemoryUsage->store(m_pTextureMemoryUsage->load(std::memory_order_relaxed) + m_vTextures[Slot].m_MemSize, std::memory_order_relaxed);

	free(pTexData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand)
{
	TextureCreate(pCommand->m_Slot, pCommand->m_Width, pCommand->m_Height, GL_RGBA, GL_RGBA, pCommand->m_Flags, pCommand->m_pData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_TextTexture_Update(const CCommandBuffer::SCommand_TextTexture_Update *pCommand)
{
	TextureUpdate(pCommand->m_Slot, pCommand->m_X, pCommand->m_Y, pCommand->m_Width, pCommand->m_Height, GL_RED, pCommand->m_pData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_TextTextures_Destroy(const CCommandBuffer::SCommand_TextTextures_Destroy *pCommand)
{
	DestroyTexture(pCommand->m_Slot);
	DestroyTexture(pCommand->m_SlotOutline);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_TextTextures_Create(const CCommandBuffer::SCommand_TextTextures_Create *pCommand)
{
	TextureCreate(pCommand->m_Slot, pCommand->m_Width, pCommand->m_Height, GL_RED, GL_RED, CCommandBuffer::TEXFLAG_NOMIPMAPS, pCommand->m_pTextData);
	TextureCreate(pCommand->m_SlotOutline, pCommand->m_Width, pCommand->m_Height, GL_RED, GL_RED, CCommandBuffer::TEXFLAG_NOMIPMAPS, pCommand->m_pTextOutlineData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand)
{
	// if clip is still active, force disable it for clearing, enable it again afterwards
	bool ClipWasEnabled = m_LastClipEnable;
	if(ClipWasEnabled)
	{
		glDisable(GL_SCISSOR_TEST);
	}
	if(pCommand->m_Color.r != m_ClearColor.r || pCommand->m_Color.g != m_ClearColor.g || pCommand->m_Color.b != m_ClearColor.b)
	{
		glClearColor(pCommand->m_Color.r, pCommand->m_Color.g, pCommand->m_Color.b, 0.0f);
		m_ClearColor = pCommand->m_Color;
	}
	glClear(GL_COLOR_BUFFER_BIT);
	if(ClipWasEnabled)
	{
		glEnable(GL_SCISSOR_TEST);
	}
}

void CCommandProcessorFragment_OpenGL3_3::UploadStreamBufferData(unsigned int PrimitiveType, const void *pVertices, size_t VertSize, unsigned int PrimitiveCount, bool AsTex3D)
{
	int Count = 0;
	switch(PrimitiveType)
	{
	case CCommandBuffer::PRIMTYPE_LINES:
		Count = PrimitiveCount * 2;
		break;
	case CCommandBuffer::PRIMTYPE_TRIANGLES:
		Count = PrimitiveCount * 3;
		break;
	case CCommandBuffer::PRIMTYPE_QUADS:
		Count = PrimitiveCount * 4;
		break;
	default:
		return;
	};

	if(AsTex3D)
		glBindBuffer(GL_ARRAY_BUFFER, m_PrimitiveDrawBufferIdTex3D);
	else
		glBindBuffer(GL_ARRAY_BUFFER, m_aPrimitiveDrawBufferId[m_LastStreamBuffer]);

	glBufferData(GL_ARRAY_BUFFER, VertSize * Count, pVertices, GL_STREAM_DRAW);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand)
{
	CGLSLTWProgram *pProgram = m_pPrimitiveProgram;
	if(IsTexturedState(pCommand->m_State))
		pProgram = m_pPrimitiveProgramTextured;
	UseProgram(pProgram);
	SetState(pCommand->m_State, pProgram);

	UploadStreamBufferData(pCommand->m_PrimType, pCommand->m_pVertices, sizeof(CCommandBuffer::SVertex), pCommand->m_PrimCount);

	glBindVertexArray(m_aPrimitiveDrawVertexId[m_LastStreamBuffer]);

	switch(pCommand->m_PrimType)
	{
	// We don't support GL_QUADS due to core profile
	case CCommandBuffer::PRIMTYPE_LINES:
		glDrawArrays(GL_LINES, 0, pCommand->m_PrimCount * 2);
		break;
	case CCommandBuffer::PRIMTYPE_TRIANGLES:
		glDrawArrays(GL_TRIANGLES, 0, pCommand->m_PrimCount * 3);
		break;
	case CCommandBuffer::PRIMTYPE_QUADS:
		if(m_aLastIndexBufferBound[m_LastStreamBuffer] != m_QuadDrawIndexBufferId)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferId);
			m_aLastIndexBufferBound[m_LastStreamBuffer] = m_QuadDrawIndexBufferId;
		}
		glDrawElements(GL_TRIANGLES, pCommand->m_PrimCount * 6, GL_UNSIGNED_INT, 0);
		break;
	default:
		dbg_msg("render", "unknown primtype %d\n", pCommand->m_PrimType);
	};

	m_LastStreamBuffer = (m_LastStreamBuffer + 1 >= MAX_STREAM_BUFFER_COUNT ? 0 : m_LastStreamBuffer + 1);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderTex3D(const CCommandBuffer::SCommand_RenderTex3D *pCommand)
{
	CGLSLPrimitiveProgram *pProg = m_pPrimitive3DProgram;
	if(IsTexturedState(pCommand->m_State))
		pProg = m_pPrimitive3DProgramTextured;
	UseProgram(pProg);
	SetState(pCommand->m_State, pProg, true);

	UploadStreamBufferData(pCommand->m_PrimType, pCommand->m_pVertices, sizeof(CCommandBuffer::SVertexTex3DStream), pCommand->m_PrimCount, true);

	glBindVertexArray(m_PrimitiveDrawVertexIdTex3D);

	switch(pCommand->m_PrimType)
	{
	// We don't support GL_QUADS due to core profile
	case CCommandBuffer::PRIMTYPE_LINES:
		glDrawArrays(GL_LINES, 0, pCommand->m_PrimCount * 2);
		break;
	case CCommandBuffer::PRIMTYPE_QUADS:
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferId);
		glDrawElements(GL_TRIANGLES, pCommand->m_PrimCount * 6, GL_UNSIGNED_INT, 0);
		break;
	default:
		dbg_msg("render", "unknown primtype %d\n", pCommand->m_PrimType);
	};
}

void CCommandProcessorFragment_OpenGL3_3::DestroyBufferContainer(int Index, bool DeleteBOs)
{
	SBufferContainer &BufferContainer = m_vBufferContainers[Index];
	if(BufferContainer.m_VertArrayId != 0)
		glDeleteVertexArrays(1, &BufferContainer.m_VertArrayId);

	// all buffer objects can deleted automatically, so the program doesn't need to deal with them (e.g. causing crashes because of driver bugs)
	if(DeleteBOs)
	{
		int VertBufferId = BufferContainer.m_ContainerInfo.m_VertBufferBindingIndex;
		if(VertBufferId != -1)
		{
			glDeleteBuffers(1, &m_vBufferObjectIndices[VertBufferId]);
		}
	}

	BufferContainer.m_LastIndexBufferBound = 0;
	BufferContainer.m_ContainerInfo.m_vAttributes.clear();
}

void CCommandProcessorFragment_OpenGL3_3::AppendIndices(unsigned int NewIndicesCount)
{
	if(NewIndicesCount <= m_CurrentIndicesInBuffer)
		return;
	unsigned int AddCount = NewIndicesCount - m_CurrentIndicesInBuffer;
	unsigned int *pIndices = new unsigned int[AddCount];
	int Primq = (m_CurrentIndicesInBuffer / 6) * 4;
	for(unsigned int i = 0; i < AddCount; i += 6)
	{
		pIndices[i] = Primq;
		pIndices[i + 1] = Primq + 1;
		pIndices[i + 2] = Primq + 2;
		pIndices[i + 3] = Primq;
		pIndices[i + 4] = Primq + 2;
		pIndices[i + 5] = Primq + 3;
		Primq += 4;
	}

	glBindBuffer(GL_COPY_READ_BUFFER, m_QuadDrawIndexBufferId);
	GLuint NewIndexBufferId;
	glGenBuffers(1, &NewIndexBufferId);
	glBindBuffer(BUFFER_INIT_INDEX_TARGET, NewIndexBufferId);
	GLsizeiptr size = sizeof(unsigned int);
	glBufferData(BUFFER_INIT_INDEX_TARGET, (GLsizeiptr)NewIndicesCount * size, NULL, GL_STATIC_DRAW);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, BUFFER_INIT_INDEX_TARGET, 0, 0, (GLsizeiptr)m_CurrentIndicesInBuffer * size);
	glBufferSubData(BUFFER_INIT_INDEX_TARGET, (GLsizeiptr)m_CurrentIndicesInBuffer * size, (GLsizeiptr)AddCount * size, pIndices);
	glBindBuffer(BUFFER_INIT_INDEX_TARGET, 0);
	glBindBuffer(GL_COPY_READ_BUFFER, 0);

	glDeleteBuffers(1, &m_QuadDrawIndexBufferId);
	m_QuadDrawIndexBufferId = NewIndexBufferId;

	for(unsigned int &i : m_aLastIndexBufferBound)
		i = 0;
	for(auto &BufferContainer : m_vBufferContainers)
	{
		BufferContainer.m_LastIndexBufferBound = 0;
	}

	m_CurrentIndicesInBuffer = NewIndicesCount;
	delete[] pIndices;
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_CreateBufferObject(const CCommandBuffer::SCommand_CreateBufferObject *pCommand)
{
	void *pUploadData = pCommand->m_pUploadData;
	int Index = pCommand->m_BufferIndex;
	// create necessary space
	if((size_t)Index >= m_vBufferObjectIndices.size())
	{
		for(int i = m_vBufferObjectIndices.size(); i < Index + 1; ++i)
		{
			m_vBufferObjectIndices.push_back(0);
		}
	}

	GLuint VertBufferId = 0;

	glGenBuffers(1, &VertBufferId);
	glBindBuffer(BUFFER_INIT_VERTEX_TARGET, VertBufferId);
	glBufferData(BUFFER_INIT_VERTEX_TARGET, (GLsizeiptr)(pCommand->m_DataSize), pUploadData, GL_STATIC_DRAW);

	m_vBufferObjectIndices[Index] = VertBufferId;

	if(pCommand->m_DeletePointer)
		free(pUploadData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RecreateBufferObject(const CCommandBuffer::SCommand_RecreateBufferObject *pCommand)
{
	void *pUploadData = pCommand->m_pUploadData;
	int Index = pCommand->m_BufferIndex;

	glBindBuffer(BUFFER_INIT_VERTEX_TARGET, m_vBufferObjectIndices[Index]);
	glBufferData(BUFFER_INIT_VERTEX_TARGET, (GLsizeiptr)(pCommand->m_DataSize), pUploadData, GL_STATIC_DRAW);

	if(pCommand->m_DeletePointer)
		free(pUploadData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_UpdateBufferObject(const CCommandBuffer::SCommand_UpdateBufferObject *pCommand)
{
	void *pUploadData = pCommand->m_pUploadData;
	int Index = pCommand->m_BufferIndex;

	glBindBuffer(BUFFER_INIT_VERTEX_TARGET, m_vBufferObjectIndices[Index]);
	glBufferSubData(BUFFER_INIT_VERTEX_TARGET, (GLintptr)(pCommand->m_pOffset), (GLsizeiptr)(pCommand->m_DataSize), pUploadData);

	if(pCommand->m_DeletePointer)
		free(pUploadData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_CopyBufferObject(const CCommandBuffer::SCommand_CopyBufferObject *pCommand)
{
	int WriteIndex = pCommand->m_WriteBufferIndex;
	int ReadIndex = pCommand->m_ReadBufferIndex;

	glBindBuffer(GL_COPY_WRITE_BUFFER, m_vBufferObjectIndices[WriteIndex]);
	glBindBuffer(GL_COPY_READ_BUFFER, m_vBufferObjectIndices[ReadIndex]);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, (GLsizeiptr)(pCommand->m_ReadOffset), (GLsizeiptr)(pCommand->m_WriteOffset), (GLsizeiptr)pCommand->m_CopySize);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_DeleteBufferObject(const CCommandBuffer::SCommand_DeleteBufferObject *pCommand)
{
	int Index = pCommand->m_BufferIndex;

	glDeleteBuffers(1, &m_vBufferObjectIndices[Index]);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_CreateBufferContainer(const CCommandBuffer::SCommand_CreateBufferContainer *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	// create necessary space
	if((size_t)Index >= m_vBufferContainers.size())
	{
		for(int i = m_vBufferContainers.size(); i < Index + 1; ++i)
		{
			SBufferContainer Container;
			Container.m_ContainerInfo.m_Stride = 0;
			Container.m_ContainerInfo.m_VertBufferBindingIndex = -1;
			m_vBufferContainers.push_back(Container);
		}
	}

	SBufferContainer &BufferContainer = m_vBufferContainers[Index];
	glGenVertexArrays(1, &BufferContainer.m_VertArrayId);
	glBindVertexArray(BufferContainer.m_VertArrayId);

	BufferContainer.m_LastIndexBufferBound = 0;

	for(size_t i = 0; i < pCommand->m_AttrCount; ++i)
	{
		glEnableVertexAttribArray((GLuint)i);

		glBindBuffer(GL_ARRAY_BUFFER, m_vBufferObjectIndices[pCommand->m_VertBufferBindingIndex]);

		SBufferContainerInfo::SAttribute &Attr = pCommand->m_pAttributes[i];

		if(Attr.m_FuncType == 0)
			glVertexAttribPointer((GLuint)i, Attr.m_DataTypeCount, Attr.m_Type, (GLboolean)Attr.m_Normalized, pCommand->m_Stride, Attr.m_pOffset);
		else if(Attr.m_FuncType == 1)
			glVertexAttribIPointer((GLuint)i, Attr.m_DataTypeCount, Attr.m_Type, pCommand->m_Stride, Attr.m_pOffset);

		BufferContainer.m_ContainerInfo.m_vAttributes.push_back(Attr);
	}

	BufferContainer.m_ContainerInfo.m_VertBufferBindingIndex = pCommand->m_VertBufferBindingIndex;
	BufferContainer.m_ContainerInfo.m_Stride = pCommand->m_Stride;
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_UpdateBufferContainer(const CCommandBuffer::SCommand_UpdateBufferContainer *pCommand)
{
	SBufferContainer &BufferContainer = m_vBufferContainers[pCommand->m_BufferContainerIndex];

	glBindVertexArray(BufferContainer.m_VertArrayId);

	// disable all old attributes
	for(size_t i = 0; i < BufferContainer.m_ContainerInfo.m_vAttributes.size(); ++i)
	{
		glDisableVertexAttribArray((GLuint)i);
	}
	BufferContainer.m_ContainerInfo.m_vAttributes.clear();

	for(size_t i = 0; i < pCommand->m_AttrCount; ++i)
	{
		glEnableVertexAttribArray((GLuint)i);

		glBindBuffer(GL_ARRAY_BUFFER, m_vBufferObjectIndices[pCommand->m_VertBufferBindingIndex]);
		SBufferContainerInfo::SAttribute &Attr = pCommand->m_pAttributes[i];
		if(Attr.m_FuncType == 0)
			glVertexAttribPointer((GLuint)i, Attr.m_DataTypeCount, Attr.m_Type, Attr.m_Normalized, pCommand->m_Stride, Attr.m_pOffset);
		else if(Attr.m_FuncType == 1)
			glVertexAttribIPointer((GLuint)i, Attr.m_DataTypeCount, Attr.m_Type, pCommand->m_Stride, Attr.m_pOffset);

		BufferContainer.m_ContainerInfo.m_vAttributes.push_back(Attr);
	}

	BufferContainer.m_ContainerInfo.m_VertBufferBindingIndex = pCommand->m_VertBufferBindingIndex;
	BufferContainer.m_ContainerInfo.m_Stride = pCommand->m_Stride;
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_DeleteBufferContainer(const CCommandBuffer::SCommand_DeleteBufferContainer *pCommand)
{
	DestroyBufferContainer(pCommand->m_BufferContainerIndex, pCommand->m_DestroyAllBO);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_IndicesRequiredNumNotify(const CCommandBuffer::SCommand_IndicesRequiredNumNotify *pCommand)
{
	if(pCommand->m_RequiredIndicesNum > m_CurrentIndicesInBuffer)
		AppendIndices(pCommand->m_RequiredIndicesNum);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderBorderTile(const CCommandBuffer::SCommand_RenderBorderTile *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	// if space not there return
	if((size_t)Index >= m_vBufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_vBufferContainers[Index];
	if(BufferContainer.m_VertArrayId == 0)
		return;

	CGLSLTileProgram *pProgram = NULL;
	if(IsTexturedState(pCommand->m_State))
		pProgram = m_pBorderTileProgramTextured;
	else
		pProgram = m_pBorderTileProgram;
	UseProgram(pProgram);

	SetState(pCommand->m_State, pProgram, true);
	pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float *)&pCommand->m_Color);

	pProgram->SetUniformVec2(pProgram->m_LocOffset, 1, (float *)&pCommand->m_Offset);
	pProgram->SetUniformVec2(pProgram->m_LocScale, 1, (float *)&pCommand->m_Scale);

	glBindVertexArray(BufferContainer.m_VertArrayId);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferId)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferId);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferId;
	}
	glDrawElements(GL_TRIANGLES, pCommand->m_DrawNum * 6, GL_UNSIGNED_INT, pCommand->m_pIndicesOffset);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderTileLayer(const CCommandBuffer::SCommand_RenderTileLayer *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	// if space not there return
	if((size_t)Index >= m_vBufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_vBufferContainers[Index];
	if(BufferContainer.m_VertArrayId == 0)
		return;

	if(pCommand->m_IndicesDrawNum == 0)
	{
		return; // nothing to draw
	}

	CGLSLTileProgram *pProgram = NULL;
	if(IsTexturedState(pCommand->m_State))
	{
		pProgram = m_pTileProgramTextured;
	}
	else
		pProgram = m_pTileProgram;

	UseProgram(pProgram);

	SetState(pCommand->m_State, pProgram, true);
	pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float *)&pCommand->m_Color);

	glBindVertexArray(BufferContainer.m_VertArrayId);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferId)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferId);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferId;
	}
	for(int i = 0; i < pCommand->m_IndicesDrawNum; ++i)
	{
		glDrawElements(GL_TRIANGLES, pCommand->m_pDrawCount[i], GL_UNSIGNED_INT, pCommand->m_pIndicesOffsets[i]);
	}
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderQuadLayer(const CCommandBuffer::SCommand_RenderQuadLayer *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	// if space not there return
	if((size_t)Index >= m_vBufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_vBufferContainers[Index];
	if(BufferContainer.m_VertArrayId == 0)
		return;

	if(pCommand->m_QuadNum == 0)
	{
		return; // nothing to draw
	}

	CGLSLQuadProgram *pProgram = NULL;
	if(IsTexturedState(pCommand->m_State))
	{
		pProgram = m_pQuadProgramTextured;
	}
	else
		pProgram = m_pQuadProgram;

	UseProgram(pProgram);
	SetState(pCommand->m_State, pProgram);

	glBindVertexArray(BufferContainer.m_VertArrayId);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferId)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferId);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferId;
	}

	int QuadsLeft = pCommand->m_QuadNum;
	size_t QuadOffset = 0;
	// the extra offset is not related to the information from the command, but an actual offset in the buffer
	size_t QuadOffsetExtra = pCommand->m_QuadOffset;

	vec4 aColors[ms_MaxQuadsPossible];
	vec2 aOffsets[ms_MaxQuadsPossible];
	float aRotations[ms_MaxQuadsPossible];

	while(QuadsLeft > 0)
	{
		int ActualQuadCount = minimum<int>(QuadsLeft, m_MaxQuadsAtOnce);

		for(size_t i = 0; i < (size_t)ActualQuadCount; ++i)
		{
			aColors[i] = pCommand->m_pQuadInfo[i + QuadOffset].m_Color;
			aOffsets[i] = pCommand->m_pQuadInfo[i + QuadOffset].m_Offsets;
			aRotations[i] = pCommand->m_pQuadInfo[i + QuadOffset].m_Rotation;
		}

		pProgram->SetUniformVec4(pProgram->m_LocColors, ActualQuadCount, (float *)aColors);
		pProgram->SetUniformVec2(pProgram->m_LocOffsets, ActualQuadCount, (float *)aOffsets);
		pProgram->SetUniform(pProgram->m_LocRotations, ActualQuadCount, (float *)aRotations);
		pProgram->SetUniform(pProgram->m_LocQuadOffset, (int)(QuadOffset + QuadOffsetExtra));
		glDrawElements(GL_TRIANGLES, ActualQuadCount * 6, GL_UNSIGNED_INT, (void *)((QuadOffset + QuadOffsetExtra) * 6 * sizeof(unsigned int)));

		QuadsLeft -= ActualQuadCount;
		QuadOffset += (size_t)ActualQuadCount;
	}
}

void CCommandProcessorFragment_OpenGL3_3::RenderText(const CCommandBuffer::SState &State, int DrawNum, int TextTextureIndex, int TextOutlineTextureIndex, int TextureSize, const ColorRGBA &TextColor, const ColorRGBA &TextOutlineColor)
{
	if(DrawNum == 0)
	{
		return; // nothing to draw
	}

	UseProgram(m_pTextProgram);

	int SlotText = 0;
	int SlotTextOutline = 1;
	glBindTexture(GL_TEXTURE_2D, m_vTextures[TextTextureIndex].m_Tex);
	glBindSampler(SlotText, m_vTextures[TextTextureIndex].m_Sampler);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_vTextures[TextOutlineTextureIndex].m_Tex);
	glBindSampler(SlotTextOutline, m_vTextures[TextOutlineTextureIndex].m_Sampler);
	glActiveTexture(GL_TEXTURE0);

	if(m_pTextProgram->m_LastTextSampler != SlotText)
	{
		m_pTextProgram->SetUniform(m_pTextProgram->m_LocTextSampler, SlotText);
		m_pTextProgram->m_LastTextSampler = SlotText;
	}

	if(m_pTextProgram->m_LastTextOutlineSampler != SlotTextOutline)
	{
		m_pTextProgram->SetUniform(m_pTextProgram->m_LocTextOutlineSampler, SlotTextOutline);
		m_pTextProgram->m_LastTextOutlineSampler = SlotTextOutline;
	}

	SetState(State, m_pTextProgram);

	if(m_pTextProgram->m_LastTextureSize != TextureSize)
	{
		m_pTextProgram->SetUniform(m_pTextProgram->m_LocTextureSize, (float)TextureSize);
		m_pTextProgram->m_LastTextureSize = TextureSize;
	}

	if(m_pTextProgram->m_LastOutlineColor != TextOutlineColor)
	{
		m_pTextProgram->SetUniformVec4(m_pTextProgram->m_LocOutlineColor, 1, (float *)&TextOutlineColor);
		m_pTextProgram->m_LastOutlineColor = TextOutlineColor;
	}

	if(m_pTextProgram->m_LastColor != TextColor)
	{
		m_pTextProgram->SetUniformVec4(m_pTextProgram->m_LocColor, 1, (float *)&TextColor);
		m_pTextProgram->m_LastColor = TextColor;
	}

	glDrawElements(GL_TRIANGLES, DrawNum, GL_UNSIGNED_INT, (void *)(0));
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderText(const CCommandBuffer::SCommand_RenderText *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	// if space not there return
	if((size_t)Index >= m_vBufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_vBufferContainers[Index];
	if(BufferContainer.m_VertArrayId == 0)
		return;

	glBindVertexArray(BufferContainer.m_VertArrayId);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferId)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferId);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferId;
	}

	RenderText(pCommand->m_State, pCommand->m_DrawNum, pCommand->m_TextTextureIndex, pCommand->m_TextOutlineTextureIndex, pCommand->m_TextureSize, pCommand->m_TextColor, pCommand->m_TextOutlineColor);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderQuadContainer(const CCommandBuffer::SCommand_RenderQuadContainer *pCommand)
{
	if(pCommand->m_DrawNum == 0)
	{
		return; // nothing to draw
	}

	int Index = pCommand->m_BufferContainerIndex;
	// if space not there return
	if((size_t)Index >= m_vBufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_vBufferContainers[Index];
	if(BufferContainer.m_VertArrayId == 0)
		return;

	glBindVertexArray(BufferContainer.m_VertArrayId);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferId)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferId);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferId;
	}

	CGLSLTWProgram *pProgram = m_pPrimitiveProgram;
	if(IsTexturedState(pCommand->m_State))
		pProgram = m_pPrimitiveProgramTextured;
	UseProgram(pProgram);
	SetState(pCommand->m_State, pProgram);

	glDrawElements(GL_TRIANGLES, pCommand->m_DrawNum, GL_UNSIGNED_INT, pCommand->m_pOffset);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderQuadContainerEx(const CCommandBuffer::SCommand_RenderQuadContainerEx *pCommand)
{
	if(pCommand->m_DrawNum == 0)
	{
		return; // nothing to draw
	}

	int Index = pCommand->m_BufferContainerIndex;
	// if space not there return
	if((size_t)Index >= m_vBufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_vBufferContainers[Index];
	if(BufferContainer.m_VertArrayId == 0)
		return;

	glBindVertexArray(BufferContainer.m_VertArrayId);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferId)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferId);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferId;
	}

	CGLSLPrimitiveExProgram *pProgram = m_pPrimitiveExProgramRotationless;
	if(IsTexturedState(pCommand->m_State))
	{
		if(pCommand->m_Rotation != 0.0f)
			pProgram = m_pPrimitiveExProgramTextured;
		else
			pProgram = m_pPrimitiveExProgramTexturedRotationless;
	}
	else
	{
		if(pCommand->m_Rotation != 0.0f)
			pProgram = m_pPrimitiveExProgram;
	}

	UseProgram(pProgram);
	SetState(pCommand->m_State, pProgram);

	if(pCommand->m_Rotation != 0.0f && pProgram->m_LastCenter != pCommand->m_Center)
	{
		pProgram->SetUniformVec2(pProgram->m_LocCenter, 1, (float *)&pCommand->m_Center);
		pProgram->m_LastCenter = pCommand->m_Center;
	}

	if(pProgram->m_LastRotation != pCommand->m_Rotation)
	{
		pProgram->SetUniform(pProgram->m_LocRotation, pCommand->m_Rotation);
		pProgram->m_LastRotation = pCommand->m_Rotation;
	}

	if(pProgram->m_LastVerticesColor != pCommand->m_VertexColor)
	{
		pProgram->SetUniformVec4(pProgram->m_LocVertciesColor, 1, (float *)&pCommand->m_VertexColor);
		pProgram->m_LastVerticesColor = pCommand->m_VertexColor;
	}

	glDrawElements(GL_TRIANGLES, pCommand->m_DrawNum, GL_UNSIGNED_INT, pCommand->m_pOffset);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderQuadContainerAsSpriteMultiple(const CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple *pCommand)
{
	if(pCommand->m_DrawNum == 0 || pCommand->m_DrawCount == 0)
	{
		return; // nothing to draw
	}

	int Index = pCommand->m_BufferContainerIndex;
	// if space not there return
	if((size_t)Index >= m_vBufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_vBufferContainers[Index];
	if(BufferContainer.m_VertArrayId == 0)
		return;

	glBindVertexArray(BufferContainer.m_VertArrayId);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferId)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferId);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferId;
	}

	UseProgram(m_pSpriteProgramMultiple);
	SetState(pCommand->m_State, m_pSpriteProgramMultiple);

	if(m_pSpriteProgramMultiple->m_LastCenter != pCommand->m_Center)
	{
		m_pSpriteProgramMultiple->SetUniformVec2(m_pSpriteProgramMultiple->m_LocCenter, 1, (float *)&pCommand->m_Center);
		m_pSpriteProgramMultiple->m_LastCenter = pCommand->m_Center;
	}

	if(m_pSpriteProgramMultiple->m_LastVerticesColor != pCommand->m_VertexColor)
	{
		m_pSpriteProgramMultiple->SetUniformVec4(m_pSpriteProgramMultiple->m_LocVertciesColor, 1, (float *)&pCommand->m_VertexColor);
		m_pSpriteProgramMultiple->m_LastVerticesColor = pCommand->m_VertexColor;
	}

	int DrawCount = pCommand->m_DrawCount;
	size_t RenderOffset = 0;

	// 4 for the center (always use vec4) and 16 for the matrix(just to be sure), 4 for the sampler and vertex color
	const int RSPCount = 256 - 4 - 16 - 8;

	while(DrawCount > 0)
	{
		int UniformCount = (DrawCount > RSPCount ? RSPCount : DrawCount);

		m_pSpriteProgramMultiple->SetUniformVec4(m_pSpriteProgramMultiple->m_LocRSP, UniformCount, (float *)(pCommand->m_pRenderInfo + RenderOffset));

		glDrawElementsInstanced(GL_TRIANGLES, pCommand->m_DrawNum, GL_UNSIGNED_INT, pCommand->m_pOffset, UniformCount);

		RenderOffset += RSPCount;
		DrawCount -= RSPCount;
	}
}

#endif
