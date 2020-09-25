#include "opengl_sl.h"
#include <engine/shared/linereader.h>
#include <engine/storage.h>
#include <stdio.h>
#include <string>
#include <vector>

bool CGLSL::LoadShader(CGLSLCompiler *pCompiler, IStorage *pStorage, const char *pFile, int Type)
{
	if(m_IsLoaded)
		return true;
	IOHANDLE f = pStorage->OpenFile(pFile, IOFLAG_READ, IStorage::TYPE_ALL);

	std::vector<std::string> Lines;
	if(f)
	{
		bool IsNewOpenGL = pCompiler->m_OpenGLVersionMajor >= 4 || (pCompiler->m_OpenGLVersionMajor == 3 && pCompiler->m_OpenGLVersionMinor == 3);
		//add compiler specific values
		if(IsNewOpenGL)
			Lines.push_back(std::string("#version ") + std::string(std::to_string(pCompiler->m_OpenGLVersionMajor)) + std::string(std::to_string(pCompiler->m_OpenGLVersionMinor)) + std::string(std::to_string(pCompiler->m_OpenGLVersionPatch)) + std::string(" core\r\n"));
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
			pCompiler->ParseLine(Line, ReadLine, Type);
			Line.append("\r\n");
			Lines.push_back(Line);
		}
		io_close(f);

		const char **ShaderCode = new const char *[Lines.size()];

		for(size_t i = 0; i < Lines.size(); ++i)
		{
			ShaderCode[i] = Lines[i].c_str();
		}

		GLuint shader = glCreateShader(Type);

		glShaderSource(shader, Lines.size(), ShaderCode, NULL);
		glCompileShader(shader);

		delete[] ShaderCode;

		int CompilationStatus;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &CompilationStatus);

		if(CompilationStatus == GL_FALSE)
		{
			char buff[3000];

			GLint maxLength = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

			glGetShaderInfoLog(shader, maxLength, &maxLength, buff);

			dbg_msg("GLSL", "%s: %s", pFile, buff);
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

	m_HasTextureArray = false;
	m_TextureReplaceType = 0;
}

void CGLSLCompiler::AddDefine(const std::string &DefineName, const std::string &DefineValue)
{
	m_Defines.emplace_back(SGLSLCompilerDefine(DefineName, DefineValue));
}

void CGLSLCompiler::AddDefine(const char *pDefineName, const char *pDefineValue)
{
	AddDefine(std::string(pDefineName), std::string(pDefineValue));
}

void CGLSLCompiler::ClearDefines()
{
	m_Defines.clear();
}

void CGLSLCompiler::ParseLine(std::string &Line, const char *pReadLine, int Type)
{
	bool IsNewOpenGL = m_OpenGLVersionMajor >= 4 || (m_OpenGLVersionMajor == 3 && m_OpenGLVersionMinor == 3);
	if(!IsNewOpenGL)
	{
		const char *pBuff = pReadLine;
		char aTmpStr[1024];
		size_t TmpStrSize = 0;
		while(*pBuff)
		{
			while(*pBuff && str_isspace(*pBuff))
			{
				Line.append(1, *pBuff);
				++pBuff;
			}

			while(*pBuff && !str_isspace(*pBuff) && *pBuff != '(' && *pBuff != '.')
			{
				aTmpStr[TmpStrSize++] = *pBuff;
				++pBuff;
			}

			if(TmpStrSize > 0)
			{
				aTmpStr[TmpStrSize] = 0;
				TmpStrSize = 0;
				if(str_comp(aTmpStr, "layout") == 0)
				{
					//search for ' in'
					while(*pBuff && (*pBuff != ' ' || (*(pBuff + 1) && *(pBuff + 1) != 'i') || *(pBuff + 2) != 'n'))
					{
						++pBuff;
					}

					if(*pBuff && *pBuff == ' ' && *(pBuff + 1) && *(pBuff + 1) == 'i' && *(pBuff + 2) == 'n')
					{
						pBuff += 3;
						Line.append("attribute");
						Line.append(pBuff);
						return;
					}
					else
					{
						dbg_msg("Shader compiler", "Fix shader for older OpenGL versions.");
					}
				}
				else if(str_comp(aTmpStr, "noperspective") == 0 || str_comp(aTmpStr, "smooth") == 0 || str_comp(aTmpStr, "flat") == 0)
				{
					//search for 'in' or 'out'
					while(*pBuff && ((*pBuff != 'i' || *(pBuff + 1) != 'n') && (*pBuff != 'o' || (*(pBuff + 1) && *(pBuff + 1) != 'u') || *(pBuff + 2) != 't')))
					{
						++pBuff;
					}

					bool Found = false;
					if(*pBuff)
					{
						if(*pBuff == 'i' && *(pBuff + 1) == 'n')
						{
							pBuff += 2;
							Found = true;
						}
						else if(*pBuff == 'o' && *(pBuff + 1) && *(pBuff + 1) == 'u' && *(pBuff + 2) == 't')
						{
							pBuff += 3;
							Found = true;
						}
					}

					if(!Found)
					{
						dbg_msg("Shader compiler", "Fix shader for older OpenGL versions.");
					}

					Line.append("varying");
					Line.append(pBuff);
					return;
				}
				else if(str_comp(aTmpStr, "out") == 0 || str_comp(aTmpStr, "in") == 0)
				{
					if(Type == GL_FRAGMENT_SHADER && str_comp(aTmpStr, "out") == 0)
						return;
					Line.append("varying");
					Line.append(pBuff);
					return;
				}
				else if(str_comp(aTmpStr, "FragClr") == 0)
				{
					Line.append("gl_FragColor");
					Line.append(pBuff);
					return;
				}
				else if(str_comp(aTmpStr, "texture") == 0)
				{
					if(m_TextureReplaceType == GLSL_COMPILER_TEXTURE_REPLACE_TYPE_2D)
						Line.append("texture2D");
					else if(m_TextureReplaceType == GLSL_COMPILER_TEXTURE_REPLACE_TYPE_3D)
						Line.append("texture3D");
					else if(m_TextureReplaceType == GLSL_COMPILER_TEXTURE_REPLACE_TYPE_2D_ARRAY)
						Line.append("texture2DArray");
					std::string RestLine;
					ParseLine(RestLine, pBuff, Type);
					Line.append(RestLine);
					return;
				}
				else
				{
					Line.append(aTmpStr);
				}
			}

			if(*pBuff)
			{
				Line.append(1, *pBuff);
				++pBuff;
			}
		}
	}
	else
	{
		Line = pReadLine;
	}
}
