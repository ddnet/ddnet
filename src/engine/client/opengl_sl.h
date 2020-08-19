#ifndef ENGINE_CLIENT_OPENGL_SL_H
#define ENGINE_CLIENT_OPENGL_SL_H

#include <GL/glew.h>
#include <vector>
#include <string>

class CGLSLCompiler;

class CGLSL {
public:
	bool LoadShader(CGLSLCompiler* pCompiler, class IStorage *pStorage, const char *pFile, int Type);
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
		SGLSLCompilerDefine(const std::string& DefineName, const std::string& DefineValue)
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
public:
	CGLSLCompiler(int OpenGLVersionMajor, int OpenGLVersionMinor, int OpenGLVersionPatch);

	void AddDefine(const std::string& DefineName, const std::string& DefineValue);
	void AddDefine(const char* pDefineName, const char* pDefineValue);
	void ClearDefines();
};

#endif // ENGINE_CLIENT_OPENGL_SL_H
