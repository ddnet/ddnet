#pragma once

#include "engine/external/glew/GL/glew.h"

class CGLSL {
public:
	bool LoadShader(class IStorage *pStorage, const char *pFile, int Type);
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
