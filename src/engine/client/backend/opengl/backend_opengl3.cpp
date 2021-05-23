#include "backend_opengl3.h"

#include <base/detect.h>

#ifndef BACKEND_AS_OPENGL_ES
#include <GL/glew.h>
#else
#include <GLES3/gl3.h>
#endif

#include <engine/client/backend/opengl/opengl_sl.h>
#include <engine/client/backend/opengl/opengl_sl_program.h>

#include <engine/shared/image_manipulation.h>

#include <engine/client/backend/glsl_shader_compiler.h>

// ------------ CCommandProcessorFragment_OpenGL3_3
int CCommandProcessorFragment_OpenGL3_3::TexFormatToNewOpenGLFormat(int TexFormat)
{
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGB)
		return GL_RGB;
	if(TexFormat == CCommandBuffer::TEXFORMAT_ALPHA)
		return GL_RED;
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGBA)
		return GL_RGBA;
	return GL_RGBA;
}

void CCommandProcessorFragment_OpenGL3_3::UseProgram(CGLSLTWProgram *pProgram)
{
	if(m_LastProgramID != pProgram->GetProgramID())
	{
		pProgram->UseProgram();
		m_LastProgramID = pProgram->GetProgramID();
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
	float Center[2] = {0.f, 0.f};
	pProgram->SetUniformVec2(pProgram->m_LocCenter, 1, Center);
}

bool CCommandProcessorFragment_OpenGL3_3::Cmd_Init(const SCommand_Init *pCommand)
{
	if(!InitOpenGL(pCommand))
		return false;

	m_OpenGLTextureLodBIAS = g_Config.m_GfxOpenGLTextureLODBIAS;

	m_UseMultipleTextureUnits = g_Config.m_GfxEnableTextureUnitOptimization;
	if(!m_UseMultipleTextureUnits)
	{
		glActiveTexture(GL_TEXTURE0);
	}

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
	m_pBorderTileLineProgram = new CGLSLTileProgram;
	m_pBorderTileLineProgramTextured = new CGLSLTileProgram;
	m_pQuadProgram = new CGLSLQuadProgram;
	m_pQuadProgramTextured = new CGLSLQuadProgram;
	m_pTextProgram = new CGLSLTextProgram;
	m_pPrimitiveExProgram = new CGLSLPrimitiveExProgram;
	m_pPrimitiveExProgramTextured = new CGLSLPrimitiveExProgram;
	m_pPrimitiveExProgramRotationless = new CGLSLPrimitiveExProgram;
	m_pPrimitiveExProgramTexturedRotationless = new CGLSLPrimitiveExProgram;
	m_pSpriteProgramMultiple = new CGLSLSpriteMultipleProgram;
	m_LastProgramID = 0;

	CGLSLCompiler ShaderCompiler(g_Config.m_GfxOpenGLMajor, g_Config.m_GfxOpenGLMinor, g_Config.m_GfxOpenGLPatch, m_IsOpenGLES, m_OpenGLTextureLodBIAS / 1000.0f);

	GLint CapVal;
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &CapVal);

	m_MaxQuadsAtOnce = minimum<int>(((CapVal - 20) / (3 * 4)), m_MaxQuadsPossible);

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
		ShaderCompiler.AddDefine("TW_TILE_BORDER", "");
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pBorderTileProgram->CreateProgram();
		m_pBorderTileProgram->AddShader(&VertexShader);
		m_pBorderTileProgram->AddShader(&FragmentShader);
		m_pBorderTileProgram->LinkProgram();

		UseProgram(m_pBorderTileProgram);

		m_pBorderTileProgram->m_LocPos = m_pBorderTileProgram->GetUniformLoc("gPos");
		m_pBorderTileProgram->m_LocColor = m_pBorderTileProgram->GetUniformLoc("gVertColor");
		m_pBorderTileProgram->m_LocOffset = m_pBorderTileProgram->GetUniformLoc("gOffset");
		m_pBorderTileProgram->m_LocDir = m_pBorderTileProgram->GetUniformLoc("gDir");
		m_pBorderTileProgram->m_LocJumpIndex = m_pBorderTileProgram->GetUniformLoc("gJumpIndex");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		ShaderCompiler.AddDefine("TW_TILE_BORDER", "");
		ShaderCompiler.AddDefine("TW_TILE_TEXTURED", "");
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.frag", GL_FRAGMENT_SHADER);
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
		m_pBorderTileProgramTextured->m_LocDir = m_pBorderTileProgramTextured->GetUniformLoc("gDir");
		m_pBorderTileProgramTextured->m_LocJumpIndex = m_pBorderTileProgramTextured->GetUniformLoc("gJumpIndex");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		ShaderCompiler.AddDefine("TW_TILE_BORDER_LINE", "");
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pBorderTileLineProgram->CreateProgram();
		m_pBorderTileLineProgram->AddShader(&VertexShader);
		m_pBorderTileLineProgram->AddShader(&FragmentShader);
		m_pBorderTileLineProgram->LinkProgram();

		UseProgram(m_pBorderTileLineProgram);

		m_pBorderTileLineProgram->m_LocPos = m_pBorderTileLineProgram->GetUniformLoc("gPos");
		m_pBorderTileLineProgram->m_LocColor = m_pBorderTileLineProgram->GetUniformLoc("gVertColor");
		m_pBorderTileLineProgram->m_LocOffset = m_pBorderTileLineProgram->GetUniformLoc("gOffset");
		m_pBorderTileLineProgram->m_LocDir = m_pBorderTileLineProgram->GetUniformLoc("gDir");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		ShaderCompiler.AddDefine("TW_TILE_BORDER_LINE", "");
		ShaderCompiler.AddDefine("TW_TILE_TEXTURED", "");
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pBorderTileLineProgramTextured->CreateProgram();
		m_pBorderTileLineProgramTextured->AddShader(&VertexShader);
		m_pBorderTileLineProgramTextured->AddShader(&FragmentShader);
		m_pBorderTileLineProgramTextured->LinkProgram();

		UseProgram(m_pBorderTileLineProgramTextured);

		m_pBorderTileLineProgramTextured->m_LocPos = m_pBorderTileLineProgramTextured->GetUniformLoc("gPos");
		m_pBorderTileLineProgramTextured->m_LocTextureSampler = m_pBorderTileLineProgramTextured->GetUniformLoc("gTextureSampler");
		m_pBorderTileLineProgramTextured->m_LocColor = m_pBorderTileLineProgramTextured->GetUniformLoc("gVertColor");
		m_pBorderTileLineProgramTextured->m_LocOffset = m_pBorderTileLineProgramTextured->GetUniformLoc("gOffset");
		m_pBorderTileLineProgramTextured->m_LocDir = m_pBorderTileLineProgramTextured->GetUniformLoc("gDir");
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

		float Center[2] = {0.f, 0.f};
		m_pSpriteProgramMultiple->SetUniformVec2(m_pSpriteProgramMultiple->m_LocCenter, 1, Center);
	}

	m_LastStreamBuffer = 0;

	glGenBuffers(MAX_STREAM_BUFFER_COUNT, m_PrimitiveDrawBufferID);
	glGenVertexArrays(MAX_STREAM_BUFFER_COUNT, m_PrimitiveDrawVertexID);
	glGenBuffers(1, &m_PrimitiveDrawBufferIDTex3D);
	glGenVertexArrays(1, &m_PrimitiveDrawVertexIDTex3D);

	m_UsePreinitializedVertexBuffer = g_Config.m_GfxUsePreinitBuffer;

	for(int i = 0; i < MAX_STREAM_BUFFER_COUNT; ++i)
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_PrimitiveDrawBufferID[i]);
		glBindVertexArray(m_PrimitiveDrawVertexID[i]);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertex), 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertex), (void *)(sizeof(float) * 2));
		glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(CCommandBuffer::SVertex), (void *)(sizeof(float) * 4));

		if(m_UsePreinitializedVertexBuffer)
			glBufferData(GL_ARRAY_BUFFER, sizeof(CCommandBuffer::SVertex) * CCommandBuffer::MAX_VERTICES, NULL, GL_STREAM_DRAW);

		m_LastIndexBufferBound[i] = 0;
	}

	glBindBuffer(GL_ARRAY_BUFFER, m_PrimitiveDrawBufferIDTex3D);
	glBindVertexArray(m_PrimitiveDrawVertexIDTex3D);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertexTex3DStream), 0);
	glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(CCommandBuffer::SVertexTex3DStream), (void *)(sizeof(float) * 2));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertexTex3DStream), (void *)(sizeof(float) * 2 + sizeof(unsigned char) * 4));

	if(m_UsePreinitializedVertexBuffer)
		glBufferData(GL_ARRAY_BUFFER, sizeof(CCommandBuffer::SVertexTex3DStream) * CCommandBuffer::MAX_VERTICES, NULL, GL_STREAM_DRAW);

	//query the image max size only once
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_MaxTexSize);

	//query maximum of allowed textures
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &m_MaxTextureUnits);
	m_TextureSlotBoundToUnit.resize(m_MaxTextureUnits);
	for(int i = 0; i < m_MaxTextureUnits; ++i)
	{
		m_TextureSlotBoundToUnit[i].m_TextureSlot = -1;
		m_TextureSlotBoundToUnit[i].m_Is2DArray = false;
	}

	glBindVertexArray(0);
	glGenBuffers(1, &m_QuadDrawIndexBufferID);
	glBindBuffer(GL_COPY_WRITE_BUFFER, m_QuadDrawIndexBufferID);

	unsigned int Indices[CCommandBuffer::MAX_VERTICES / 4 * 6];
	int Primq = 0;
	for(int i = 0; i < CCommandBuffer::MAX_VERTICES / 4 * 6; i += 6)
	{
		Indices[i] = Primq;
		Indices[i + 1] = Primq + 1;
		Indices[i + 2] = Primq + 2;
		Indices[i + 3] = Primq;
		Indices[i + 4] = Primq + 2;
		Indices[i + 5] = Primq + 3;
		Primq += 4;
	}
	glBufferData(GL_COPY_WRITE_BUFFER, sizeof(unsigned int) * CCommandBuffer::MAX_VERTICES / 4 * 6, Indices, GL_STATIC_DRAW);

	m_CurrentIndicesInBuffer = CCommandBuffer::MAX_VERTICES / 4 * 6;

	m_Textures.resize(CCommandBuffer::MAX_TEXTURES);

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
	m_pBorderTileLineProgram->DeleteProgram();
	m_pBorderTileLineProgramTextured->DeleteProgram();
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

	//clean up everything
	delete m_pPrimitiveProgram;
	delete m_pPrimitiveProgramTextured;
	delete m_pBorderTileProgram;
	delete m_pBorderTileProgramTextured;
	delete m_pBorderTileLineProgram;
	delete m_pBorderTileLineProgramTextured;
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
	glDeleteBuffers(MAX_STREAM_BUFFER_COUNT, m_PrimitiveDrawBufferID);
	glDeleteBuffers(1, &m_QuadDrawIndexBufferID);
	glDeleteVertexArrays(MAX_STREAM_BUFFER_COUNT, m_PrimitiveDrawVertexID);
	glDeleteBuffers(1, &m_PrimitiveDrawBufferIDTex3D);
	glDeleteVertexArrays(1, &m_PrimitiveDrawVertexIDTex3D);

	for(int i = 0; i < (int)m_Textures.size(); ++i)
	{
		DestroyTexture(i);
	}

	for(size_t i = 0; i < m_BufferContainers.size(); ++i)
	{
		DestroyBufferContainer(i);
	}

	m_BufferContainers.clear();
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Texture_Update(const CCommandBuffer::SCommand_Texture_Update *pCommand)
{
	if(m_UseMultipleTextureUnits)
	{
		int Slot = pCommand->m_Slot % m_MaxTextureUnits;
		//just tell, that we using this texture now
		IsAndUpdateTextureSlotBound(Slot, pCommand->m_Slot);
		glActiveTexture(GL_TEXTURE0 + Slot);
		glBindSampler(Slot, m_Textures[pCommand->m_Slot].m_Sampler);
	}

	glBindTexture(GL_TEXTURE_2D, m_Textures[pCommand->m_Slot].m_Tex);

	void *pTexData = pCommand->m_pData;
	int Width = pCommand->m_Width;
	int Height = pCommand->m_Height;
	int X = pCommand->m_X;
	int Y = pCommand->m_Y;
	if(m_Textures[pCommand->m_Slot].m_RescaleCount > 0)
	{
		for(int i = 0; i < m_Textures[pCommand->m_Slot].m_RescaleCount; ++i)
		{
			Width >>= 1;
			Height >>= 1;

			X /= 2;
			Y /= 2;
		}

		void *pTmpData = Resize(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
		free(pTexData);
		pTexData = pTmpData;
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, X, Y, Width, Height,
		TexFormatToNewOpenGLFormat(pCommand->m_Format), GL_UNSIGNED_BYTE, pTexData);
	free(pTexData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand)
{
	int Slot = 0;
	if(m_UseMultipleTextureUnits)
	{
		Slot = pCommand->m_Slot % m_MaxTextureUnits;
		IsAndUpdateTextureSlotBound(Slot, pCommand->m_Slot);
		glActiveTexture(GL_TEXTURE0 + Slot);
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindSampler(Slot, 0);
	m_TextureSlotBoundToUnit[Slot].m_TextureSlot = -1;
	m_TextureSlotBoundToUnit[Slot].m_Is2DArray = false;
	DestroyTexture(pCommand->m_Slot);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand)
{
	int Width = pCommand->m_Width;
	int Height = pCommand->m_Height;
	void *pTexData = pCommand->m_pData;

	if(pCommand->m_Slot >= (int)m_Textures.size())
		m_Textures.resize(m_Textures.size() * 2);

	// resample if needed
	int RescaleCount = 0;
	if(pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGBA || pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGB || pCommand->m_Format == CCommandBuffer::TEXFORMAT_ALPHA)
	{
		if(Width > m_MaxTexSize || Height > m_MaxTexSize)
		{
			do
			{
				Width >>= 1;
				Height >>= 1;
				++RescaleCount;
			} while(Width > m_MaxTexSize || Height > m_MaxTexSize);

			void *pTmpData = Resize(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
			free(pTexData);
			pTexData = pTmpData;
		}
	}
	m_Textures[pCommand->m_Slot].m_Width = Width;
	m_Textures[pCommand->m_Slot].m_Height = Height;
	m_Textures[pCommand->m_Slot].m_RescaleCount = RescaleCount;

	int Oglformat = TexFormatToNewOpenGLFormat(pCommand->m_Format);
	int StoreOglformat = TexFormatToNewOpenGLFormat(pCommand->m_StoreFormat);
	if(StoreOglformat == GL_RED)
		StoreOglformat = GL_R8;

	int Slot = 0;
	if(m_UseMultipleTextureUnits)
	{
		Slot = pCommand->m_Slot % m_MaxTextureUnits;
		//just tell, that we using this texture now
		IsAndUpdateTextureSlotBound(Slot, pCommand->m_Slot);
		glActiveTexture(GL_TEXTURE0 + Slot);
		m_TextureSlotBoundToUnit[Slot].m_TextureSlot = -1;
		m_TextureSlotBoundToUnit[Slot].m_Is2DArray = false;
	}

	if((pCommand->m_Flags & CCommandBuffer::TEXFLAG_NO_2D_TEXTURE) == 0)
	{
		glGenTextures(1, &m_Textures[pCommand->m_Slot].m_Tex);
		glBindTexture(GL_TEXTURE_2D, m_Textures[pCommand->m_Slot].m_Tex);

		glGenSamplers(1, &m_Textures[pCommand->m_Slot].m_Sampler);
		glBindSampler(Slot, m_Textures[pCommand->m_Slot].m_Sampler);
	}

	if(pCommand->m_Flags & CCommandBuffer::TEXFLAG_NOMIPMAPS)
	{
		if((pCommand->m_Flags & CCommandBuffer::TEXFLAG_NO_2D_TEXTURE) == 0)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glSamplerParameteri(m_Textures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glSamplerParameteri(m_Textures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, StoreOglformat, Width, Height, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
		}
	}
	else
	{
		if((pCommand->m_Flags & CCommandBuffer::TEXFLAG_NO_2D_TEXTURE) == 0)
		{
			glSamplerParameteri(m_Textures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glSamplerParameteri(m_Textures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

#ifndef BACKEND_AS_OPENGL_ES
			if(m_OpenGLTextureLodBIAS != 0 && !m_IsOpenGLES)
				glSamplerParameterf(m_Textures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_LOD_BIAS, ((GLfloat)m_OpenGLTextureLodBIAS / 1000.0f));
#endif

			//prevent mipmap display bugs, when zooming out far
			if(Width >= 1024 && Height >= 1024)
			{
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 5.f);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 5);
			}
			glTexImage2D(GL_TEXTURE_2D, 0, StoreOglformat, Width, Height, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
			glGenerateMipmap(GL_TEXTURE_2D);
		}

		if((pCommand->m_Flags & (CCommandBuffer::TEXFLAG_TO_2D_ARRAY_TEXTURE | CCommandBuffer::TEXFLAG_TO_2D_ARRAY_TEXTURE_SINGLE_LAYER)) != 0)
		{
			glGenTextures(1, &m_Textures[pCommand->m_Slot].m_Tex2DArray);
			glBindTexture(GL_TEXTURE_2D_ARRAY, m_Textures[pCommand->m_Slot].m_Tex2DArray);

			glGenSamplers(1, &m_Textures[pCommand->m_Slot].m_Sampler2DArray);
			glBindSampler(Slot, m_Textures[pCommand->m_Slot].m_Sampler2DArray);
			glSamplerParameteri(m_Textures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glSamplerParameteri(m_Textures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glSamplerParameteri(m_Textures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(m_Textures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(m_Textures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);

#ifndef BACKEND_AS_OPENGL_ES
			if(m_OpenGLTextureLodBIAS != 0 && !m_IsOpenGLES)
				glSamplerParameterf(m_Textures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_LOD_BIAS, ((GLfloat)m_OpenGLTextureLodBIAS / 1000.0f));
#endif

			int ImageColorChannels = TexFormatToImageColorChannelCount(pCommand->m_Format);

			uint8_t *p3DImageData = NULL;

			bool IsSingleLayer = (pCommand->m_Flags & CCommandBuffer::TEXFLAG_TO_2D_ARRAY_TEXTURE_SINGLE_LAYER) != 0;

			if(!IsSingleLayer)
				p3DImageData = (uint8_t *)malloc((size_t)ImageColorChannels * Width * Height);
			int Image3DWidth, Image3DHeight;

			int ConvertWidth = Width;
			int ConvertHeight = Height;

			if(!IsSingleLayer)
			{
				if(ConvertWidth == 0 || (ConvertWidth % 16) != 0 || ConvertHeight == 0 || (ConvertHeight % 16) != 0)
				{
					dbg_msg("gfx", "3D/2D array texture was resized");
					int NewWidth = maximum<int>(HighestBit(ConvertWidth), 16);
					int NewHeight = maximum<int>(HighestBit(ConvertHeight), 16);
					uint8_t *pNewTexData = (uint8_t *)Resize(ConvertWidth, ConvertHeight, NewWidth, NewHeight, pCommand->m_Format, (const uint8_t *)pTexData);

					ConvertWidth = NewWidth;
					ConvertHeight = NewHeight;

					free(pTexData);
					pTexData = pNewTexData;
				}
			}

			if(IsSingleLayer || (Texture2DTo3D(pTexData, ConvertWidth, ConvertHeight, ImageColorChannels, 16, 16, p3DImageData, Image3DWidth, Image3DHeight)))
			{
				if(IsSingleLayer)
				{
					glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, StoreOglformat, ConvertWidth, ConvertHeight, 1, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
				}
				else
				{
					glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, StoreOglformat, Image3DWidth, Image3DHeight, 256, 0, Oglformat, GL_UNSIGNED_BYTE, p3DImageData);
				}
				glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
			}

			if(!IsSingleLayer)
				free(p3DImageData);
		}
	}

	// This is the initial value for the wrap modes
	m_Textures[pCommand->m_Slot].m_LastWrapMode = CCommandBuffer::WRAP_REPEAT;

	// calculate memory usage
	m_Textures[pCommand->m_Slot].m_MemSize = Width * Height * pCommand->m_PixelSize;
	while(Width > 2 && Height > 2)
	{
		Width >>= 1;
		Height >>= 1;
		m_Textures[pCommand->m_Slot].m_MemSize += Width * Height * pCommand->m_PixelSize;
	}
	m_pTextureMemoryUsage->store(m_pTextureMemoryUsage->load(std::memory_order_relaxed) + m_Textures[pCommand->m_Slot].m_MemSize, std::memory_order_relaxed);

	free(pTexData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand)
{
	if(pCommand->m_Color.r != m_ClearColor.r || pCommand->m_Color.g != m_ClearColor.g || pCommand->m_Color.b != m_ClearColor.b)
	{
		glClearColor(pCommand->m_Color.r, pCommand->m_Color.g, pCommand->m_Color.b, 0.0f);
		m_ClearColor = pCommand->m_Color;
	}
	glClear(GL_COLOR_BUFFER_BIT);
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
		glBindBuffer(GL_ARRAY_BUFFER, m_PrimitiveDrawBufferIDTex3D);
	else
		glBindBuffer(GL_ARRAY_BUFFER, m_PrimitiveDrawBufferID[m_LastStreamBuffer]);

	if(!m_UsePreinitializedVertexBuffer)
		glBufferData(GL_ARRAY_BUFFER, VertSize * Count, pVertices, GL_STREAM_DRAW);
	else
	{
		// This is better for some iGPUs. Probably due to not initializing a new buffer in the system memory again and again...(driver dependent)
		void *pData = glMapBufferRange(GL_ARRAY_BUFFER, 0, VertSize * Count, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

		mem_copy(pData, pVertices, VertSize * Count);

		glUnmapBuffer(GL_ARRAY_BUFFER);
	}
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand)
{
	CGLSLTWProgram *pProgram = m_pPrimitiveProgram;
	if(IsTexturedState(pCommand->m_State))
		pProgram = m_pPrimitiveProgramTextured;
	UseProgram(pProgram);
	SetState(pCommand->m_State, pProgram);

	UploadStreamBufferData(pCommand->m_PrimType, pCommand->m_pVertices, sizeof(CCommandBuffer::SVertex), pCommand->m_PrimCount);

	glBindVertexArray(m_PrimitiveDrawVertexID[m_LastStreamBuffer]);

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
		if(m_LastIndexBufferBound[m_LastStreamBuffer] != m_QuadDrawIndexBufferID)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
			m_LastIndexBufferBound[m_LastStreamBuffer] = m_QuadDrawIndexBufferID;
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

	glBindVertexArray(m_PrimitiveDrawVertexIDTex3D);

	switch(pCommand->m_PrimType)
	{
	// We don't support GL_QUADS due to core profile
	case CCommandBuffer::PRIMTYPE_LINES:
		glDrawArrays(GL_LINES, 0, pCommand->m_PrimCount * 2);
		break;
	case CCommandBuffer::PRIMTYPE_QUADS:
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		glDrawElements(GL_TRIANGLES, pCommand->m_PrimCount * 6, GL_UNSIGNED_INT, 0);
		break;
	default:
		dbg_msg("render", "unknown primtype %d\n", pCommand->m_PrimType);
	};
}

void CCommandProcessorFragment_OpenGL3_3::DestroyBufferContainer(int Index, bool DeleteBOs)
{
	SBufferContainer &BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID != 0)
		glDeleteVertexArrays(1, &BufferContainer.m_VertArrayID);

	// all buffer objects can deleted automatically, so the program doesn't need to deal with them (e.g. causing crashes because of driver bugs)
	if(DeleteBOs)
	{
		for(size_t i = 0; i < BufferContainer.m_ContainerInfo.m_Attributes.size(); ++i)
		{
			int VertBufferID = BufferContainer.m_ContainerInfo.m_Attributes[i].m_VertBufferBindingIndex;
			if(VertBufferID != -1)
			{
				for(auto &Attribute : BufferContainer.m_ContainerInfo.m_Attributes)
				{
					// set all equal ids to zero to not double delete
					if(VertBufferID == Attribute.m_VertBufferBindingIndex)
					{
						Attribute.m_VertBufferBindingIndex = -1;
					}
				}

				glDeleteBuffers(1, &m_BufferObjectIndices[VertBufferID]);
			}
		}
	}

	BufferContainer.m_LastIndexBufferBound = 0;
	BufferContainer.m_ContainerInfo.m_Attributes.clear();
}

void CCommandProcessorFragment_OpenGL3_3::AppendIndices(unsigned int NewIndicesCount)
{
	if(NewIndicesCount <= m_CurrentIndicesInBuffer)
		return;
	unsigned int AddCount = NewIndicesCount - m_CurrentIndicesInBuffer;
	unsigned int *Indices = new unsigned int[AddCount];
	int Primq = (m_CurrentIndicesInBuffer / 6) * 4;
	for(unsigned int i = 0; i < AddCount; i += 6)
	{
		Indices[i] = Primq;
		Indices[i + 1] = Primq + 1;
		Indices[i + 2] = Primq + 2;
		Indices[i + 3] = Primq;
		Indices[i + 4] = Primq + 2;
		Indices[i + 5] = Primq + 3;
		Primq += 4;
	}

	glBindBuffer(GL_COPY_READ_BUFFER, m_QuadDrawIndexBufferID);
	GLuint NewIndexBufferID;
	glGenBuffers(1, &NewIndexBufferID);
	glBindBuffer(GL_COPY_WRITE_BUFFER, NewIndexBufferID);
	GLsizeiptr size = sizeof(unsigned int);
	glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)NewIndicesCount * size, NULL, GL_STATIC_DRAW);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, (GLsizeiptr)m_CurrentIndicesInBuffer * size);
	glBufferSubData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)m_CurrentIndicesInBuffer * size, (GLsizeiptr)AddCount * size, Indices);
	glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
	glBindBuffer(GL_COPY_READ_BUFFER, 0);

	glDeleteBuffers(1, &m_QuadDrawIndexBufferID);
	m_QuadDrawIndexBufferID = NewIndexBufferID;

	for(unsigned int &i : m_LastIndexBufferBound)
		i = 0;
	for(auto &BufferContainer : m_BufferContainers)
	{
		BufferContainer.m_LastIndexBufferBound = 0;
	}

	m_CurrentIndicesInBuffer = NewIndicesCount;
	delete[] Indices;
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_CreateBufferObject(const CCommandBuffer::SCommand_CreateBufferObject *pCommand)
{
	void *pUploadData = pCommand->m_pUploadData;
	int Index = pCommand->m_BufferIndex;
	//create necessary space
	if((size_t)Index >= m_BufferObjectIndices.size())
	{
		for(int i = m_BufferObjectIndices.size(); i < Index + 1; ++i)
		{
			m_BufferObjectIndices.push_back(0);
		}
	}

	GLuint VertBufferID = 0;

	glGenBuffers(1, &VertBufferID);
	glBindBuffer(GL_COPY_WRITE_BUFFER, VertBufferID);
	glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)(pCommand->m_DataSize), pUploadData, GL_STATIC_DRAW);

	m_BufferObjectIndices[Index] = VertBufferID;

	if(pCommand->m_DeletePointer)
		free(pUploadData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RecreateBufferObject(const CCommandBuffer::SCommand_RecreateBufferObject *pCommand)
{
	void *pUploadData = pCommand->m_pUploadData;
	int Index = pCommand->m_BufferIndex;

	glBindBuffer(GL_COPY_WRITE_BUFFER, m_BufferObjectIndices[Index]);
	glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)(pCommand->m_DataSize), pUploadData, GL_STATIC_DRAW);

	if(pCommand->m_DeletePointer)
		free(pUploadData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_UpdateBufferObject(const CCommandBuffer::SCommand_UpdateBufferObject *pCommand)
{
	void *pUploadData = pCommand->m_pUploadData;
	int Index = pCommand->m_BufferIndex;

	glBindBuffer(GL_COPY_WRITE_BUFFER, m_BufferObjectIndices[Index]);
	glBufferSubData(GL_COPY_WRITE_BUFFER, (GLintptr)(pCommand->m_pOffset), (GLsizeiptr)(pCommand->m_DataSize), pUploadData);

	if(pCommand->m_DeletePointer)
		free(pUploadData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_CopyBufferObject(const CCommandBuffer::SCommand_CopyBufferObject *pCommand)
{
	int WriteIndex = pCommand->m_WriteBufferIndex;
	int ReadIndex = pCommand->m_ReadBufferIndex;

	glBindBuffer(GL_COPY_WRITE_BUFFER, m_BufferObjectIndices[WriteIndex]);
	glBindBuffer(GL_COPY_READ_BUFFER, m_BufferObjectIndices[ReadIndex]);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, (GLsizeiptr)(pCommand->m_pReadOffset), (GLsizeiptr)(pCommand->m_pWriteOffset), (GLsizeiptr)pCommand->m_CopySize);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_DeleteBufferObject(const CCommandBuffer::SCommand_DeleteBufferObject *pCommand)
{
	int Index = pCommand->m_BufferIndex;

	glDeleteBuffers(1, &m_BufferObjectIndices[Index]);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_CreateBufferContainer(const CCommandBuffer::SCommand_CreateBufferContainer *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	//create necessary space
	if((size_t)Index >= m_BufferContainers.size())
	{
		for(int i = m_BufferContainers.size(); i < Index + 1; ++i)
		{
			SBufferContainer Container;
			Container.m_ContainerInfo.m_Stride = 0;
			m_BufferContainers.push_back(Container);
		}
	}

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
	glGenVertexArrays(1, &BufferContainer.m_VertArrayID);
	glBindVertexArray(BufferContainer.m_VertArrayID);

	BufferContainer.m_LastIndexBufferBound = 0;

	for(int i = 0; i < pCommand->m_AttrCount; ++i)
	{
		glEnableVertexAttribArray((GLuint)i);

		glBindBuffer(GL_ARRAY_BUFFER, m_BufferObjectIndices[pCommand->m_Attributes[i].m_VertBufferBindingIndex]);

		SBufferContainerInfo::SAttribute &Attr = pCommand->m_Attributes[i];

		if(Attr.m_FuncType == 0)
			glVertexAttribPointer((GLuint)i, Attr.m_DataTypeCount, Attr.m_Type, (GLboolean)Attr.m_Normalized, pCommand->m_Stride, Attr.m_pOffset);
		else if(Attr.m_FuncType == 1)
			glVertexAttribIPointer((GLuint)i, Attr.m_DataTypeCount, Attr.m_Type, pCommand->m_Stride, Attr.m_pOffset);

		BufferContainer.m_ContainerInfo.m_Attributes.push_back(Attr);
	}

	BufferContainer.m_ContainerInfo.m_Stride = pCommand->m_Stride;
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_UpdateBufferContainer(const CCommandBuffer::SCommand_UpdateBufferContainer *pCommand)
{
	SBufferContainer &BufferContainer = m_BufferContainers[pCommand->m_BufferContainerIndex];

	glBindVertexArray(BufferContainer.m_VertArrayID);

	//disable all old attributes
	for(size_t i = 0; i < BufferContainer.m_ContainerInfo.m_Attributes.size(); ++i)
	{
		glDisableVertexAttribArray((GLuint)i);
	}
	BufferContainer.m_ContainerInfo.m_Attributes.clear();

	for(int i = 0; i < pCommand->m_AttrCount; ++i)
	{
		glEnableVertexAttribArray((GLuint)i);

		glBindBuffer(GL_ARRAY_BUFFER, m_BufferObjectIndices[pCommand->m_Attributes[i].m_VertBufferBindingIndex]);
		SBufferContainerInfo::SAttribute &Attr = pCommand->m_Attributes[i];
		if(Attr.m_FuncType == 0)
			glVertexAttribPointer((GLuint)i, Attr.m_DataTypeCount, Attr.m_Type, Attr.m_Normalized, pCommand->m_Stride, Attr.m_pOffset);
		else if(Attr.m_FuncType == 1)
			glVertexAttribIPointer((GLuint)i, Attr.m_DataTypeCount, Attr.m_Type, pCommand->m_Stride, Attr.m_pOffset);

		BufferContainer.m_ContainerInfo.m_Attributes.push_back(Attr);
	}

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
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	CGLSLTileProgram *pProgram = NULL;
	if(IsTexturedState(pCommand->m_State))
	{
		pProgram = m_pBorderTileProgramTextured;
	}
	else
		pProgram = m_pBorderTileProgram;
	UseProgram(pProgram);

	SetState(pCommand->m_State, pProgram, true);
	pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float *)&pCommand->m_Color);

	pProgram->SetUniformVec2(pProgram->m_LocOffset, 1, (float *)&pCommand->m_Offset);
	pProgram->SetUniformVec2(pProgram->m_LocDir, 1, (float *)&pCommand->m_Dir);
	pProgram->SetUniform(pProgram->m_LocJumpIndex, (int)pCommand->m_JumpIndex);

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
	}
	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, pCommand->m_pIndicesOffset, pCommand->m_DrawNum);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderBorderTileLine(const CCommandBuffer::SCommand_RenderBorderTileLine *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	CGLSLTileProgram *pProgram = NULL;
	if(IsTexturedState(pCommand->m_State))
	{
		pProgram = m_pBorderTileLineProgramTextured;
	}
	else
		pProgram = m_pBorderTileLineProgram;
	UseProgram(pProgram);

	SetState(pCommand->m_State, pProgram, true);
	pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float *)&pCommand->m_Color);
	pProgram->SetUniformVec2(pProgram->m_LocOffset, 1, (float *)&pCommand->m_Offset);
	pProgram->SetUniformVec2(pProgram->m_LocDir, 1, (float *)&pCommand->m_Dir);

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
	}
	glDrawElementsInstanced(GL_TRIANGLES, pCommand->m_IndexDrawNum, GL_UNSIGNED_INT, pCommand->m_pIndicesOffset, pCommand->m_DrawNum);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderTileLayer(const CCommandBuffer::SCommand_RenderTileLayer *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	if(pCommand->m_IndicesDrawNum == 0)
	{
		return; //nothing to draw
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

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
	}
	for(int i = 0; i < pCommand->m_IndicesDrawNum; ++i)
	{
		glDrawElements(GL_TRIANGLES, pCommand->m_pDrawCount[i], GL_UNSIGNED_INT, pCommand->m_pIndicesOffsets[i]);
	}
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderQuadLayer(const CCommandBuffer::SCommand_RenderQuadLayer *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	if(pCommand->m_QuadNum == 0)
	{
		return; //nothing to draw
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

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
	}

	int QuadsLeft = pCommand->m_QuadNum;
	size_t QuadOffset = 0;
	// the extra offset is not related to the information from the command, but an actual offset in the buffer
	size_t QuadOffsetExtra = pCommand->m_QuadOffset;

	vec4 aColors[m_MaxQuadsPossible];
	vec2 aOffsets[m_MaxQuadsPossible];
	float aRotations[m_MaxQuadsPossible];

	while(QuadsLeft > 0)
	{
		int ActualQuadCount = minimum<int>(QuadsLeft, m_MaxQuadsAtOnce);

		for(size_t i = 0; i < (size_t)ActualQuadCount; ++i)
		{
			mem_copy(&aColors[i], pCommand->m_pQuadInfo[i + QuadOffset].m_aColor, sizeof(vec4));
			mem_copy(&aOffsets[i], pCommand->m_pQuadInfo[i + QuadOffset].m_aOffsets, sizeof(vec2));
			mem_copy(&aRotations[i], &pCommand->m_pQuadInfo[i + QuadOffset].m_Rotation, sizeof(float));
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

void CCommandProcessorFragment_OpenGL3_3::RenderText(const CCommandBuffer::SState &State, int DrawNum, int TextTextureIndex, int TextOutlineTextureIndex, int TextureSize, const float *pTextColor, const float *pTextOutlineColor)
{
	if(DrawNum == 0)
	{
		return; //nothing to draw
	}

	UseProgram(m_pTextProgram);

	int SlotText = 0;
	int SlotTextOutline = 0;

	if(m_UseMultipleTextureUnits)
	{
		SlotText = TextTextureIndex % m_MaxTextureUnits;
		SlotTextOutline = TextOutlineTextureIndex % m_MaxTextureUnits;
		if(SlotText == SlotTextOutline)
			SlotTextOutline = (TextOutlineTextureIndex + 1) % m_MaxTextureUnits;

		if(!IsAndUpdateTextureSlotBound(SlotText, TextTextureIndex))
		{
			glActiveTexture(GL_TEXTURE0 + SlotText);
			glBindTexture(GL_TEXTURE_2D, m_Textures[TextTextureIndex].m_Tex);
			glBindSampler(SlotText, m_Textures[TextTextureIndex].m_Sampler);
		}
		if(!IsAndUpdateTextureSlotBound(SlotTextOutline, TextOutlineTextureIndex))
		{
			glActiveTexture(GL_TEXTURE0 + SlotTextOutline);
			glBindTexture(GL_TEXTURE_2D, m_Textures[TextOutlineTextureIndex].m_Tex);
			glBindSampler(SlotTextOutline, m_Textures[TextOutlineTextureIndex].m_Sampler);
		}
	}
	else
	{
		SlotText = 0;
		SlotTextOutline = 1;
		glBindTexture(GL_TEXTURE_2D, m_Textures[TextTextureIndex].m_Tex);
		glBindSampler(SlotText, m_Textures[TextTextureIndex].m_Sampler);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m_Textures[TextOutlineTextureIndex].m_Tex);
		glBindSampler(SlotTextOutline, m_Textures[TextOutlineTextureIndex].m_Sampler);
		glActiveTexture(GL_TEXTURE0);
	}

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

	if(m_pTextProgram->m_LastOutlineColor[0] != pTextOutlineColor[0] || m_pTextProgram->m_LastOutlineColor[1] != pTextOutlineColor[1] || m_pTextProgram->m_LastOutlineColor[2] != pTextOutlineColor[2] || m_pTextProgram->m_LastOutlineColor[3] != pTextOutlineColor[3])
	{
		m_pTextProgram->SetUniformVec4(m_pTextProgram->m_LocOutlineColor, 1, (float *)pTextOutlineColor);
		m_pTextProgram->m_LastOutlineColor[0] = pTextOutlineColor[0];
		m_pTextProgram->m_LastOutlineColor[1] = pTextOutlineColor[1];
		m_pTextProgram->m_LastOutlineColor[2] = pTextOutlineColor[2];
		m_pTextProgram->m_LastOutlineColor[3] = pTextOutlineColor[3];
	}

	if(m_pTextProgram->m_LastColor[0] != pTextColor[0] || m_pTextProgram->m_LastColor[1] != pTextColor[1] || m_pTextProgram->m_LastColor[2] != pTextColor[2] || m_pTextProgram->m_LastColor[3] != pTextColor[3])
	{
		m_pTextProgram->SetUniformVec4(m_pTextProgram->m_LocColor, 1, (float *)pTextColor);
		m_pTextProgram->m_LastColor[0] = pTextColor[0];
		m_pTextProgram->m_LastColor[1] = pTextColor[1];
		m_pTextProgram->m_LastColor[2] = pTextColor[2];
		m_pTextProgram->m_LastColor[3] = pTextColor[3];
	}

	glDrawElements(GL_TRIANGLES, DrawNum, GL_UNSIGNED_INT, (void *)(0));
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderText(const CCommandBuffer::SCommand_RenderText *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
	}

	RenderText(pCommand->m_State, pCommand->m_DrawNum, pCommand->m_TextTextureIndex, pCommand->m_TextOutlineTextureIndex, pCommand->m_TextureSize, pCommand->m_aTextColor, pCommand->m_aTextOutlineColor);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderTextStream(const CCommandBuffer::SCommand_RenderTextStream *pCommand)
{
	if(pCommand->m_PrimCount == 0)
	{
		return; //nothing to draw
	}

	UploadStreamBufferData(CCommandBuffer::PRIMTYPE_QUADS, pCommand->m_pVertices, sizeof(CCommandBuffer::SVertex), pCommand->m_PrimCount);

	glBindVertexArray(m_PrimitiveDrawVertexID[m_LastStreamBuffer]);
	if(m_LastIndexBufferBound[m_LastStreamBuffer] != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		m_LastIndexBufferBound[m_LastStreamBuffer] = m_QuadDrawIndexBufferID;
	}

	float aTextColor[4] = {1.f, 1.f, 1.f, 1.f};

	RenderText(pCommand->m_State, pCommand->m_PrimCount * 6, pCommand->m_TextTextureIndex, pCommand->m_TextOutlineTextureIndex, pCommand->m_TextureSize, aTextColor, pCommand->m_aTextOutlineColor);

	m_LastStreamBuffer = (m_LastStreamBuffer + 1 >= MAX_STREAM_BUFFER_COUNT ? 0 : m_LastStreamBuffer + 1);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderQuadContainer(const CCommandBuffer::SCommand_RenderQuadContainer *pCommand)
{
	if(pCommand->m_DrawNum == 0)
	{
		return; //nothing to draw
	}

	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
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
		return; //nothing to draw
	}

	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
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

	if(pCommand->m_Rotation != 0.0f && (pProgram->m_LastCenter[0] != pCommand->m_Center.x || pProgram->m_LastCenter[1] != pCommand->m_Center.y))
	{
		pProgram->SetUniformVec2(pProgram->m_LocCenter, 1, (float *)&pCommand->m_Center);
		pProgram->m_LastCenter[0] = pCommand->m_Center.x;
		pProgram->m_LastCenter[1] = pCommand->m_Center.y;
	}

	if(pProgram->m_LastRotation != pCommand->m_Rotation)
	{
		pProgram->SetUniform(pProgram->m_LocRotation, pCommand->m_Rotation);
		pProgram->m_LastRotation = pCommand->m_Rotation;
	}

	if(pProgram->m_LastVertciesColor[0] != pCommand->m_VertexColor.r || pProgram->m_LastVertciesColor[1] != pCommand->m_VertexColor.g || pProgram->m_LastVertciesColor[2] != pCommand->m_VertexColor.b || pProgram->m_LastVertciesColor[3] != pCommand->m_VertexColor.a)
	{
		pProgram->SetUniformVec4(pProgram->m_LocVertciesColor, 1, (float *)&pCommand->m_VertexColor);
		pProgram->m_LastVertciesColor[0] = pCommand->m_VertexColor.r;
		pProgram->m_LastVertciesColor[1] = pCommand->m_VertexColor.g;
		pProgram->m_LastVertciesColor[2] = pCommand->m_VertexColor.b;
		pProgram->m_LastVertciesColor[3] = pCommand->m_VertexColor.a;
	}

	glDrawElements(GL_TRIANGLES, pCommand->m_DrawNum, GL_UNSIGNED_INT, pCommand->m_pOffset);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderQuadContainerAsSpriteMultiple(const CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple *pCommand)
{
	if(pCommand->m_DrawNum == 0 || pCommand->m_DrawCount == 0)
	{
		return; //nothing to draw
	}

	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
	}

	UseProgram(m_pSpriteProgramMultiple);
	SetState(pCommand->m_State, m_pSpriteProgramMultiple);

	if((m_pSpriteProgramMultiple->m_LastCenter[0] != pCommand->m_Center.x || m_pSpriteProgramMultiple->m_LastCenter[1] != pCommand->m_Center.y))
	{
		m_pSpriteProgramMultiple->SetUniformVec2(m_pSpriteProgramMultiple->m_LocCenter, 1, (float *)&pCommand->m_Center);
		m_pSpriteProgramMultiple->m_LastCenter[0] = pCommand->m_Center.x;
		m_pSpriteProgramMultiple->m_LastCenter[1] = pCommand->m_Center.y;
	}

	if(m_pSpriteProgramMultiple->m_LastVertciesColor[0] != pCommand->m_VertexColor.r || m_pSpriteProgramMultiple->m_LastVertciesColor[1] != pCommand->m_VertexColor.g || m_pSpriteProgramMultiple->m_LastVertciesColor[2] != pCommand->m_VertexColor.b || m_pSpriteProgramMultiple->m_LastVertciesColor[3] != pCommand->m_VertexColor.a)
	{
		m_pSpriteProgramMultiple->SetUniformVec4(m_pSpriteProgramMultiple->m_LocVertciesColor, 1, (float *)&pCommand->m_VertexColor);
		m_pSpriteProgramMultiple->m_LastVertciesColor[0] = pCommand->m_VertexColor.r;
		m_pSpriteProgramMultiple->m_LastVertciesColor[1] = pCommand->m_VertexColor.g;
		m_pSpriteProgramMultiple->m_LastVertciesColor[2] = pCommand->m_VertexColor.b;
		m_pSpriteProgramMultiple->m_LastVertciesColor[3] = pCommand->m_VertexColor.a;
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
