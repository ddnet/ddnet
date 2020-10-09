#ifndef ENGINE_CLIENT_OPENGL_SL_H
#define ENGINE_CLIENT_OPENGL_SL_H

#include <GL/glew.h>
#include <string>
#include <vector>

class CGLSLCompiler;

class CGLSL
{
public:
	bool LoadShader(CGLSLCompiler *pCompiler, class IStorage *pStorage, const char *pFile, int Type);
	void DeleteShader();

	bool IsLoaded();
	GLuint GetShaderID();

	CGLSL();
	virtual ~CGLSL();

private:
	GLuint m_ShaderID;
	int m_Type;
	bool m_IsLoaded;
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

	std::vector<SGLSLCompilerDefine> m_Defines;

	int m_OpenGLVersionMajor;
	int m_OpenGLVersionMinor;
	int m_OpenGLVersionPatch;

	bool m_HasTextureArray;
	int m_TextureReplaceType; // @see EGLSLCompilerTextureReplaceType
public:
	CGLSLCompiler(int OpenGLVersionMajor, int OpenGLVersionMinor, int OpenGLVersionPatch);
	void SetHasTextureArray(bool TextureArray) { m_HasTextureArray = TextureArray; }
	void SetTextureReplaceType(int TextureReplaceType) { m_TextureReplaceType = TextureReplaceType; }

	void AddDefine(const std::string &DefineName, const std::string &DefineValue);
	void AddDefine(const char *pDefineName, const char *pDefineValue);
	void ClearDefines();

	void ParseLine(std::string &Line, const char *pReadLine, int Type);

	enum EGLSLCompilerTextureReplaceType
	{
		GLSL_COMPILER_TEXTURE_REPLACE_TYPE_2D = 0,
		GLSL_COMPILER_TEXTURE_REPLACE_TYPE_3D,
		GLSL_COMPILER_TEXTURE_REPLACE_TYPE_2D_ARRAY,
	};
};

#endif // ENGINE_CLIENT_OPENGL_SL_H
