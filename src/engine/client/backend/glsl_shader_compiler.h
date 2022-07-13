#ifndef ENGINE_CLIENT_BACKEND_GLSL_SHADER_COMPILER_H
#define ENGINE_CLIENT_BACKEND_GLSL_SHADER_COMPILER_H

#include <string>
#include <vector>

enum EGLSLShaderCompilerType
{
	GLSL_SHADER_COMPILER_TYPE_VERTEX = 0,
	GLSL_SHADER_COMPILER_TYPE_FRAGMENT,
};

class CGLSLCompiler
{
private:
	friend class CGLSL;

	struct SGLSLCompilerDefine
	{
		SGLSLCompilerDefine(const std::string &DefineName, const std::string &DefineValue)
		{
			m_DefineName = DefineName;
			m_DefineValue = DefineValue;
		}
		std::string m_DefineName;
		std::string m_DefineValue;
	};

	std::vector<SGLSLCompilerDefine> m_vDefines;

	int m_OpenGLVersionMajor = 0;
	int m_OpenGLVersionMinor = 0;
	int m_OpenGLVersionPatch = 0;

	bool m_IsOpenGLES = false;

	float m_TextureLODBias = 0;

	bool m_HasTextureArray = false;
	int m_TextureReplaceType = 0; // @see EGLSLCompilerTextureReplaceType
public:
	CGLSLCompiler(int OpenGLVersionMajor, int OpenGLVersionMinor, int OpenGLVersionPatch, bool IsOpenGLES, float TextureLODBias);
	void SetHasTextureArray(bool TextureArray) { m_HasTextureArray = TextureArray; }
	void SetTextureReplaceType(int TextureReplaceType) { m_TextureReplaceType = TextureReplaceType; }

	void AddDefine(const std::string &DefineName, const std::string &DefineValue);
	void AddDefine(const char *pDefineName, const char *pDefineValue);
	void ClearDefines();

	void ParseLine(std::string &Line, const char *pReadLine, EGLSLShaderCompilerType Type);

	enum EGLSLCompilerTextureReplaceType
	{
		GLSL_COMPILER_TEXTURE_REPLACE_TYPE_2D = 0,
		GLSL_COMPILER_TEXTURE_REPLACE_TYPE_3D,
		GLSL_COMPILER_TEXTURE_REPLACE_TYPE_2D_ARRAY,
	};
};

#endif
