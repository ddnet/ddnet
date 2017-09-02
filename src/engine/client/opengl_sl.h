#pragma once

#include "GL/glew.h"

class CGLSL {
public:
	bool LoadShader(const char* pFile, int Type);
	void DeleteShader();
	
	bool IsLoaded();
	GLuint GetShaderID();
	
	CGLSL();
private:
	GLuint m_ShaderID;
	int m_Type;
	bool m_IsLoaded;
};