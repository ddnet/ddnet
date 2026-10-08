#ifndef ENGINE_CLIENT_BACKEND_GLSL_SHADER_COMPILER_H
#define ENGINE_CLIENT_BACKEND_GLSL_SHADER_COMPILER_H

#include <string>
#include <vector>

class CGLSLCompiler
{
	friend class CGLSL;

public:
	enum class EShaderType
	{
		VERTEX,
		FRAGMENT,
	};

	enum class ETextureReplaceType
	{
		TEXTURE_2D,
		TEXTURE_3D,
		TEXTURE_2D_ARRAY,
	};

	CGLSLCompiler(int OpenGLVersionMajor, int OpenGLVersionMinor, int OpenGLVersionPatch, bool IsOpenGLES, float TextureLODBias);
	void SetHasTextureArray(bool TextureArray) { m_HasTextureArray = TextureArray; }
	void SetTextureReplaceType(ETextureReplaceType TextureReplaceType) { m_TextureReplaceType = TextureReplaceType; }

	void AddDefine(const char *pDefineName, const char *pDefineValue);
	void ClearDefines();

	void ParseLine(std::string &Line, const char *pReadLine, EShaderType Type) const;

private:
	void ParseLineOldOpenGL(std::string &Line, const char *pReadLine, EShaderType Type) const;
	void ParseLineGLES(std::string &Line, const char *pReadLine) const;

	class CDefine
	{
	public:
		CDefine(const char *pDefineName, const char *pDefineValue)
		{
			m_DefineName = pDefineName;
			m_DefineValue = pDefineValue;
		}
		std::string m_DefineName;
		std::string m_DefineValue;
	};

	std::vector<CDefine> m_vDefines;

	int m_OpenGLVersionMajor;
	int m_OpenGLVersionMinor;
	int m_OpenGLVersionPatch;

	bool m_IsOpenGLES;

	float m_TextureLODBias;

	bool m_HasTextureArray;
	ETextureReplaceType m_TextureReplaceType;
};

#endif
