#pragma once

#include <base/detect.h>

#if defined(CONF_PLATFORM_MACOSX)
#include <OpenGL/gl3.h>
#else
#include "GL/glew.h"
#endif

class CGLSL {
public:
	bool LoadShader(const char* pFile, int Type);
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
