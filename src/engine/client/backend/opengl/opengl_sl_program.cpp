#include "opengl_sl_program.h"
#include "opengl_sl.h"
#include <base/system.h>

#include <base/detect.h>

#if defined(BACKEND_AS_OPENGL_ES) || !defined(CONF_BACKEND_OPENGL_ES)

#ifndef BACKEND_AS_OPENGL_ES
#include <GL/glew.h>
#else
#include <GLES3/gl3.h>
#endif

void CGLSLProgram::CreateProgram()
{
	m_ProgramID = glCreateProgram();
}

void CGLSLProgram::DeleteProgram()
{
	if(!m_IsLinked)
		return;
	m_IsLinked = false;
	glDeleteProgram(m_ProgramID);
}

bool CGLSLProgram::AddShader(CGLSL *pShader)
{
	if(pShader->IsLoaded())
	{
		glAttachShader(m_ProgramID, pShader->GetShaderID());
		return true;
	}
	return false;
}

void CGLSLProgram::DetachShader(CGLSL *pShader)
{
	if(pShader->IsLoaded())
	{
		DetachShaderByID(pShader->GetShaderID());
	}
}

void CGLSLProgram::DetachShaderByID(TWGLuint ShaderID)
{
	glDetachShader(m_ProgramID, ShaderID);
}

void CGLSLProgram::LinkProgram()
{
	glLinkProgram(m_ProgramID);
	int LinkStatus;
	glGetProgramiv(m_ProgramID, GL_LINK_STATUS, &LinkStatus);
	m_IsLinked = LinkStatus == GL_TRUE;
	if(!m_IsLinked)
	{
		char aInfoLog[1024];
		char aFinalMessage[1536];
		int iLogLength;
		glGetProgramInfoLog(m_ProgramID, 1024, &iLogLength, aInfoLog);
		str_format(aFinalMessage, sizeof(aFinalMessage), "Error! Shader program wasn't linked! The linker returned:\n\n%s", aInfoLog);
		dbg_msg("glslprogram", "%s", aFinalMessage);
	}

	//detach all shaders attached to this program
	DetachAllShaders();
}

void CGLSLProgram::DetachAllShaders()
{
	TWGLuint aShaders[100];
	GLsizei ReturnedCount = 0;
	while(true)
	{
		glGetAttachedShaders(m_ProgramID, 100, &ReturnedCount, aShaders);

		if(ReturnedCount > 0)
		{
			for(GLsizei i = 0; i < ReturnedCount; ++i)
			{
				DetachShaderByID(aShaders[i]);
			}
		}

		if(ReturnedCount < 100)
			break;
	}
}

void CGLSLProgram::SetUniformVec4(int Loc, int Count, const float *pValues)
{
	glUniform4fv(Loc, Count, pValues);
}

void CGLSLProgram::SetUniformVec2(int Loc, int Count, const float *pValues)
{
	glUniform2fv(Loc, Count, pValues);
}

void CGLSLProgram::SetUniform(int Loc, int Count, const float *pValues)
{
	glUniform1fv(Loc, Count, pValues);
}

void CGLSLProgram::SetUniform(int Loc, const int Value)
{
	glUniform1i(Loc, Value);
}

void CGLSLProgram::SetUniform(int Loc, const float Value)
{
	glUniform1f(Loc, Value);
}

void CGLSLProgram::SetUniform(int Loc, const bool Value)
{
	glUniform1i(Loc, (int)Value);
}

int CGLSLProgram::GetUniformLoc(const char *pName)
{
	return glGetUniformLocation(m_ProgramID, pName);
}

void CGLSLProgram::UseProgram()
{
	if(m_IsLinked)
		glUseProgram(m_ProgramID);
}

TWGLuint CGLSLProgram::GetProgramID()
{
	return m_ProgramID;
}

CGLSLProgram::CGLSLProgram()
{
	m_IsLinked = false;
}

CGLSLProgram::~CGLSLProgram()
{
	DeleteProgram();
}

#endif
