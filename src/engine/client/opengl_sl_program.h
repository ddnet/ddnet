#pragma once

#include "GL/glew.h"

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
	
	//Support various types	
	void SetUniformVec4(int Loc, int Count, const float* Value);
	void SetUniform(int Loc, const int Value);
	void SetUniform(int Loc, const unsigned int Value);
	void SetUniform(int Loc, const bool Value);
	void SetUniform(int Loc, const float Value);
	
	//for performance reason we do not use SetUniform with using strings... save the Locations of the variables instead
	int GetUniformLoc(const char* Name);
	
	CGLSLProgram();
	~CGLSLProgram();
	
protected:
	GLuint m_ProgramID;
	bool m_IsLinked;
};

class CGLSLQuadProgram : public CGLSLProgram {
public:
	int m_LocPos;
	int m_LocIsTextured;
	int m_LocTextureSampler;
	
};