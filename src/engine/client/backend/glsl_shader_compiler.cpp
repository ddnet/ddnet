#include "glsl_shader_compiler.h"

#include <base/system.h>
#include <engine/graphics.h>

CGLSLCompiler::CGLSLCompiler(int OpenGLVersionMajor, int OpenGLVersionMinor, int OpenGLVersionPatch, bool IsOpenGLES, float TextureLODBias)
{
	m_OpenGLVersionMajor = OpenGLVersionMajor;
	m_OpenGLVersionMinor = OpenGLVersionMinor;
	m_OpenGLVersionPatch = OpenGLVersionPatch;

	m_IsOpenGLES = IsOpenGLES;

	m_TextureLODBias = TextureLODBias;

	m_HasTextureArray = false;
	m_TextureReplaceType = 0;
}

void CGLSLCompiler::AddDefine(const std::string &DefineName, const std::string &DefineValue)
{
	m_vDefines.emplace_back(DefineName, DefineValue);
}

void CGLSLCompiler::AddDefine(const char *pDefineName, const char *pDefineValue)
{
	AddDefine(std::string(pDefineName), std::string(pDefineValue));
}

void CGLSLCompiler::ClearDefines()
{
	m_vDefines.clear();
}

void CGLSLCompiler::ParseLine(std::string &Line, const char *pReadLine, EGLSLShaderCompilerType Type)
{
	EBackendType BackendType = m_IsOpenGLES ? BACKEND_TYPE_OPENGL_ES : BACKEND_TYPE_OPENGL;
	bool IsNewOpenGL = (BackendType == BACKEND_TYPE_OPENGL ? (m_OpenGLVersionMajor >= 4 || (m_OpenGLVersionMajor == 3 && m_OpenGLVersionMinor == 3)) : m_OpenGLVersionMajor >= 3);
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

					if(*pBuff == ' ' && *(pBuff + 1) == 'i' && *(pBuff + 2) == 'n')
					{
						pBuff += 3;
						Line.append("attribute");
						Line.append(pBuff);
						return;
					}
					else
					{
						dbg_msg("shadercompiler", "Fix shader for older OpenGL versions.");
					}
				}
				else if(str_comp(aTmpStr, "noperspective") == 0 || str_comp(aTmpStr, "smooth") == 0 || str_comp(aTmpStr, "flat") == 0)
				{
					//search for 'in' or 'out'
					while(*pBuff && ((*pBuff != 'i' || *(pBuff + 1) != 'n') && (*pBuff != 'o' || (*(pBuff + 1) && *(pBuff + 1) != 'u') || *(pBuff + 2) != 't')))
					{
						// append anything that is inbetween noperspective & in/out vars
						Line.push_back(*pBuff);
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
						else if(*pBuff == 'o' && *(pBuff + 1) == 'u' && *(pBuff + 2) == 't')
						{
							pBuff += 3;
							Found = true;
						}
					}

					if(!Found)
					{
						dbg_msg("shadercompiler", "Fix shader for older OpenGL versions.");
					}

					Line.append("varying");
					Line.append(pBuff);
					return;
				}
				else if(str_comp(aTmpStr, "out") == 0 || str_comp(aTmpStr, "in") == 0)
				{
					if(Type == GLSL_SHADER_COMPILER_TYPE_FRAGMENT && str_comp(aTmpStr, "out") == 0)
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
		if(BackendType == BACKEND_TYPE_OPENGL_ES)
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

					if(str_comp(aTmpStr, "noperspective") == 0)
					{
						Line.append("smooth");
						Line.append(pBuff);
						return;
					}
					// since GLES doesn't support texture LOD bias as global state, use the shader function instead(since GLES 3.0 uses shaders only anyway)
					else if(str_comp(aTmpStr, "texture") == 0)
					{
						Line.append("texture");
						// check opening and closing brackets to find the end
						int CurBrackets = 1;
						while(*pBuff && *pBuff != '(')
						{
							Line.append(1, *pBuff);

							++pBuff;
						}

						if(*pBuff)
						{
							Line.append(1, *pBuff);
							++pBuff;
						}

						while(*pBuff)
						{
							if(*pBuff == '(')
								++CurBrackets;
							if(*pBuff == ')')
								--CurBrackets;

							if(CurBrackets == 0)
							{
								// found end
								Line.append(std::string(", ") + std::to_string(m_TextureLODBias) + ")");
								++pBuff;
								break;
							}
							else
								Line.append(1, *pBuff);
							++pBuff;
						}

						Line.append(pBuff);

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
			Line = pReadLine;
	}
}
