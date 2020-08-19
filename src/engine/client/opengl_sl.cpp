#include "opengl_sl.h"
#include <engine/shared/linereader.h>
#include <engine/storage.h>
#include <vector>
#include <stdio.h>
#include <string>

bool CGLSL::LoadShader(CGLSLCompiler* pCompiler, IStorage *pStorage, const char *pFile, int Type)
{
	if (m_IsLoaded)
		return true;
	IOHANDLE f = pStorage->OpenFile(pFile, IOFLAG_READ, IStorage::TYPE_ALL);

	std::vector<std::string> Lines;
	if (f)
	{
		//add compiler specific values
		Lines.push_back(std::string("#version ") + std::string(std::to_string(pCompiler->m_OpenGLVersionMajor)) + std::string(std::to_string(pCompiler->m_OpenGLVersionMinor)) + std::string(std::to_string(pCompiler->m_OpenGLVersionPatch)) + std::string(" core\r\n"));

		for(CGLSLCompiler::SGLSLCompilerDefine& Define : pCompiler->m_Defines)
		{
			Lines.push_back(std::string("#define ") + Define.m_DefineName + std::string(" ") + Define.m_DefineValue + std::string("\r\n"));
		}

		CLineReader LineReader;
		LineReader.Init(f);
		char* ReadLine = NULL;
		while ((ReadLine = LineReader.Get()))
		{
			Lines.push_back(ReadLine);
			Lines.back().append("\r\n");
		}
		io_close(f);

		const char** ShaderCode = new const char*[Lines.size()];

		for (size_t i = 0; i < Lines.size(); ++i)
		{
			ShaderCode[i] = Lines[i].c_str();
		}

		GLuint shader = glCreateShader(Type);

		glShaderSource(shader, Lines.size(), ShaderCode, NULL);
		glCompileShader(shader);

		delete[] ShaderCode;

		int CompilationStatus;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &CompilationStatus);

		if (CompilationStatus == GL_FALSE)
		{
			char buff[3000];

			GLint maxLength = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

			glGetShaderInfoLog(shader, maxLength, &maxLength, buff);

			dbg_msg("GLSL", "%s", buff);
			glDeleteShader(shader);
			return false;
		}
		m_Type = Type;
		m_IsLoaded = true;

		m_ShaderID = shader;

		return true;
	}
	else return false;

}

void CGLSL::DeleteShader()
{
	if (!IsLoaded()) return;
	m_IsLoaded = false;
	glDeleteShader(m_ShaderID);
}

bool CGLSL::IsLoaded()
{
	return m_IsLoaded;
}

GLuint CGLSL::GetShaderID()
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

CGLSLCompiler::CGLSLCompiler(int OpenGLVersionMajor, int OpenGLVersionMinor, int OpenGLVersionPatch)
{
	m_OpenGLVersionMajor = OpenGLVersionMajor;
	m_OpenGLVersionMinor = OpenGLVersionMinor;
	m_OpenGLVersionPatch = OpenGLVersionPatch;
}

void CGLSLCompiler::AddDefine(const std::string& DefineName, const std::string& DefineValue)
{
	m_Defines.emplace_back(SGLSLCompilerDefine(DefineName, DefineValue));
}

void CGLSLCompiler::AddDefine(const char* pDefineName, const char* pDefineValue)
{
	AddDefine(std::string(pDefineName), std::string(pDefineValue));
}

void CGLSLCompiler::ClearDefines()
{
	m_Defines.clear();
}
