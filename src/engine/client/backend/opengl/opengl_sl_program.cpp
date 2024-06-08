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
	m_ProgramId = glCreateProgram();
}

void CGLSLProgram::DeleteProgram()
{
	if(!m_IsLinked)
		return;
	m_IsLinked = false;
	glDeleteProgram(m_ProgramId);
}

bool CGLSLProgram::AddShader(CGLSL *pShader) const
{
	if(pShader->IsLoaded())
	{
		glAttachShader(m_ProgramId, pShader->GetShaderId());
		return true;
	}
	return false;
}

void CGLSLProgram::DetachShader(CGLSL *pShader) const
{
	if(pShader->IsLoaded())
	{
		DetachShaderById(pShader->GetShaderId());
	}
}

void CGLSLProgram::DetachShaderById(TWGLuint ShaderId) const
{
	glDetachShader(m_ProgramId, ShaderId);
}

void CGLSLProgram::LinkProgram()
{
	glLinkProgram(m_ProgramId);
	int LinkStatus;
	glGetProgramiv(m_ProgramId, GL_LINK_STATUS, &LinkStatus);
	m_IsLinked = LinkStatus == GL_TRUE;
	if(!m_IsLinked)
	{
		char aInfoLog[1024];
		char aFinalMessage[1536];
		int LogLength;
		glGetProgramInfoLog(m_ProgramId, 1024, &LogLength, aInfoLog);
		str_format(aFinalMessage, sizeof(aFinalMessage), "Error! Shader program wasn't linked! The linker returned:\n\n%s", aInfoLog);
		dbg_msg("glslprogram", "%s", aFinalMessage);
	}

	//detach all shaders attached to this program
	DetachAllShaders();
}

void CGLSLProgram::DetachAllShaders() const
{
	TWGLuint aShaders[100];
	GLsizei ReturnedCount = 0;
	while(true)
	{
		glGetAttachedShaders(m_ProgramId, 100, &ReturnedCount, aShaders);

		if(ReturnedCount > 0)
		{
			for(GLsizei i = 0; i < ReturnedCount; ++i)
			{
				DetachShaderById(aShaders[i]);
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

int CGLSLProgram::GetUniformLoc(const char *pName) const
{
	return glGetUniformLocation(m_ProgramId, pName);
}

void CGLSLProgram::UseProgram() const
{
	if(m_IsLinked)
		glUseProgram(m_ProgramId);
}

TWGLuint CGLSLProgram::GetProgramId() const
{
	return m_ProgramId;
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
