#include "opengl_sl.h"
#include <engine/shared/linereader.h>
#include <engine/storage.h>
#include <stdio.h>
#include <string>
#include <vector>

#include <engine/client/backend_sdl.h>

#include <engine/client/backend/glsl_shader_compiler.h>

#ifndef BACKEND_AS_OPENGL_ES
#include <GL/glew.h>
#else
#include <GLES3/gl3.h>
#endif

bool CGLSL::LoadShader(CGLSLCompiler *pCompiler, IStorage *pStorage, const char *pFile, int Type)
{
	if(m_IsLoaded)
		return true;
	IOHANDLE f = pStorage->OpenFile(pFile, IOFLAG_READ, IStorage::TYPE_ALL);

	std::vector<std::string> Lines;
	if(f)
	{
		EBackendType BackendType = pCompiler->m_IsOpenGLES ? BACKEND_TYPE_OPENGL_ES : BACKEND_TYPE_OPENGL;
		bool IsNewOpenGL = (BackendType == BACKEND_TYPE_OPENGL ? (pCompiler->m_OpenGLVersionMajor >= 4 || (pCompiler->m_OpenGLVersionMajor == 3 && pCompiler->m_OpenGLVersionMinor == 3)) : pCompiler->m_OpenGLVersionMajor >= 3);
		std::string GLShaderStringPostfix = std::string(" core\r\n");
		if(BackendType == BACKEND_TYPE_OPENGL_ES)
			GLShaderStringPostfix = std::string(" es\r\n");
		//add compiler specific values
		if(IsNewOpenGL)
			Lines.push_back(std::string("#version ") + std::string(std::to_string(pCompiler->m_OpenGLVersionMajor)) + std::string(std::to_string(pCompiler->m_OpenGLVersionMinor)) + std::string(std::to_string(pCompiler->m_OpenGLVersionPatch)) + GLShaderStringPostfix);
		else
		{
			if(pCompiler->m_OpenGLVersionMajor == 3)
			{
				if(pCompiler->m_OpenGLVersionMinor == 0)
					Lines.push_back(std::string("#version 130 \r\n"));
				if(pCompiler->m_OpenGLVersionMinor == 1)
					Lines.push_back(std::string("#version 140 \r\n"));
				if(pCompiler->m_OpenGLVersionMinor == 2)
					Lines.push_back(std::string("#version 150 \r\n"));
			}
			else if(pCompiler->m_OpenGLVersionMajor == 2)
			{
				if(pCompiler->m_OpenGLVersionMinor == 0)
					Lines.push_back(std::string("#version 110 \r\n"));
				if(pCompiler->m_OpenGLVersionMinor == 1)
					Lines.push_back(std::string("#version 120 \r\n"));
			}
		}

		if(BackendType == BACKEND_TYPE_OPENGL_ES)
		{
			if(Type == GL_FRAGMENT_SHADER)
			{
				Lines.push_back("precision highp float; \r\n");
				Lines.push_back("precision highp sampler2D; \r\n");
				Lines.push_back("precision highp sampler3D; \r\n");
				Lines.push_back("precision highp samplerCube; \r\n");
				Lines.push_back("precision highp samplerCubeShadow; \r\n");
				Lines.push_back("precision highp sampler2DShadow; \r\n");
				Lines.push_back("precision highp sampler2DArray; \r\n");
				Lines.push_back("precision highp sampler2DArrayShadow; \r\n");
			}
		}

		for(CGLSLCompiler::SGLSLCompilerDefine &Define : pCompiler->m_Defines)
		{
			Lines.push_back(std::string("#define ") + Define.m_DefineName + std::string(" ") + Define.m_DefineValue + std::string("\r\n"));
		}

		if(Type == GL_FRAGMENT_SHADER && !IsNewOpenGL && pCompiler->m_OpenGLVersionMajor <= 3 && pCompiler->m_HasTextureArray)
		{
			Lines.push_back(std::string("#extension GL_EXT_texture_array : enable\r\n"));
		}

		CLineReader LineReader;
		LineReader.Init(f);
		char *ReadLine = NULL;
		while((ReadLine = LineReader.Get()))
		{
			std::string Line;
			pCompiler->ParseLine(Line, ReadLine, Type == GL_FRAGMENT_SHADER ? GLSL_SHADER_COMPILER_TYPE_FRAGMENT : GLSL_SHADER_COMPILER_TYPE_VERTEX);
			Line.append("\r\n");
			Lines.push_back(Line);
		}
		io_close(f);

		const char **ShaderCode = new const char *[Lines.size()];

		for(size_t i = 0; i < Lines.size(); ++i)
		{
			ShaderCode[i] = Lines[i].c_str();
		}

		TWGLuint shader = glCreateShader(Type);

		glShaderSource(shader, Lines.size(), ShaderCode, NULL);
		glCompileShader(shader);

		delete[] ShaderCode;

		int CompilationStatus;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &CompilationStatus);

		if(CompilationStatus == GL_FALSE)
		{
			char buff[3000];

			TWGLint maxLength = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

			glGetShaderInfoLog(shader, maxLength, &maxLength, buff);

			dbg_msg("glsl", "%s: %s", pFile, buff);
			glDeleteShader(shader);
			return false;
		}
		m_Type = Type;
		m_IsLoaded = true;

		m_ShaderID = shader;

		return true;
	}
	else
		return false;
}

void CGLSL::DeleteShader()
{
	if(!IsLoaded())
		return;
	m_IsLoaded = false;
	glDeleteShader(m_ShaderID);
}

bool CGLSL::IsLoaded()
{
	return m_IsLoaded;
}

TWGLuint CGLSL::GetShaderID()
{
	return m_ShaderID;
}

CGLSL::CGLSL()
{
	m_IsLoaded = false;
}

CGLSL::~CGLSL()
{
	DeleteShader();
}
