#ifndef ENGINE_CLIENT_OPENGL_SL_H
#define ENGINE_CLIENT_OPENGL_SL_H

#include <GL/glew.h>

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

#endif // ENGINE_CLIENT_OPENGL_SL_H
