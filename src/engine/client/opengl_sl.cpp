#include "opengl_sl.h"
#include <engine/shared/linereader.h>
#include <engine/shared/storage.h>
#include <vector>
#include <stdio.h>
#include <string>

bool CGLSL::LoadShader(IStorage* pStorage, const char* pFile, int Type)
{
	if (m_IsLoaded)
		return true;
	IOHANDLE f = pStorage->OpenFile(pFile, IOFLAG_READ, IStorage::TYPE_ALL);
	
	std::vector<std::string> Lines;
	if (f)
	{
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
