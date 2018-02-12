#ifndef ENGINE_CLIENT_OPENGL_SL_PROGRAM_H
#define ENGINE_CLIENT_OPENGL_SL_PROGRAM_H

#include <GL/glew.h>

class CGLSL;

class CGLSLProgram {
public:
	void CreateProgram();
	void DeleteProgram();
	
	bool AddShader(CGLSL* pShader);
	
	void LinkProgram();
	void UseProgram();
	GLuint GetProgramID();
	
	void DetachShader(CGLSL* pShader);
	void DetachShaderByID(GLuint ShaderID);
	void DetachAllShaders();
	
	//Support various types	
	void SetUniformVec2(int Loc, int Count, const float* Value);
	void SetUniformVec4(int Loc, int Count, const float* Value);
	void SetUniform(int Loc, const int Value);
	void SetUniform(int Loc, const unsigned int Value);
	void SetUniform(int Loc, const bool Value);
	void SetUniform(int Loc, const float Value);
	
	//for performance reason we do not use SetUniform with using strings... save the Locations of the variables instead
	int GetUniformLoc(const char* Name);
	
	CGLSLProgram();
	virtual ~CGLSLProgram();
	
protected:
	GLuint m_ProgramID;
	bool m_IsLinked;
};

class CGLSLTWProgram : public CGLSLProgram {
public:	
	int m_LocPos;
	int m_LocIsTextured;
	int m_LocTextureSampler;
};

class CGLSLQuadProgram : public CGLSLTWProgram {
public:
	
};

class CGLSLPrimitiveProgram : public CGLSLTWProgram {
public:
	
};

class CGLSLTileProgram : public CGLSLTWProgram {
public:
	int m_LocColor;
	int m_LocLOD;
};

class CGLSLBorderTileProgram : public CGLSLTileProgram {
public:
	int m_LocOffset;
	int m_LocDir;
	int m_LocNum;
	int m_LocJumpIndex;
};

class CGLSLBorderTileLineProgram : public CGLSLTileProgram {
public:
	int m_LocDir;
	int m_LocNum;
};

#endif // ENGINE_CLIENT_OPENGL_SL_PROGRAM_H
