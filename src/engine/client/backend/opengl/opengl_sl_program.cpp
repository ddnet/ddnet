#include "opengl_sl_program.h"

#include "opengl_sl.h"

#include <base/detect.h>
#include <base/log.h>
#include <base/system.h>

#include <string>

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

bool CGLSLProgram::LinkProgram()
{
	glLinkProgram(m_ProgramId);
	TWGLint LinkStatus;
	glGetProgramiv(m_ProgramId, GL_LINK_STATUS, &LinkStatus);
	m_IsLinked = LinkStatus == GL_TRUE;
	if(!m_IsLinked)
	{
		TWGLint LogLength = 0;
		glGetProgramiv(m_ProgramId, GL_INFO_LOG_LENGTH, &LogLength);
		if(LogLength > 0)
		{
			std::string Log(LogLength, '\0');
			glGetProgramInfoLog(m_ProgramId, Log.size(), nullptr, &Log.front());
			if(Log[Log.size() - 2] == '\n')
			{
				Log[Log.size() - 2] = '\0';
			}
			log_error("gfx/opengl/shader", "Failed to link shader program '%d'. The linker returned:\n%s", m_ProgramId, Log.c_str());
		}
		else
		{
			log_error("gfx/opengl/shader", "Failed to link shader program '%d'. The linker did not return an error.", m_ProgramId);
		}
	}

	DetachAllShaders();
	return m_IsLinked;
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
