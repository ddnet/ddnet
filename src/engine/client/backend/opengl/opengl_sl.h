// This file can be included several times.
#if(!defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_OPENGL_SL_H)) || \
	(defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_OPENGL_SL_H_AS_ES))

#if !defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_OPENGL_SL_H)
#define ENGINE_CLIENT_BACKEND_OPENGL_OPENGL_SL_H
#endif

#if defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_OPENGL_SL_H_AS_ES)
#define ENGINE_CLIENT_BACKEND_OPENGL_OPENGL_SL_H_AS_ES
#endif

#include <engine/client/graphics_defines.h>

class CGLSLCompiler;

class CGLSL
{
public:
	bool LoadShader(CGLSLCompiler *pCompiler, class IStorage *pStorage, const char *pFile, int Type);
	void DeleteShader();

	bool IsLoaded();
	TWGLuint GetShaderID();

	CGLSL();
	virtual ~CGLSL();

private:
	TWGLuint m_ShaderID;
	int m_Type;
	bool m_IsLoaded;
};

#endif
