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
	CGLSLTWProgram() : m_LastTextureSampler(-1), m_LastIsTextured(-1) {}

	int m_LocPos;
	int m_LocIsTextured;
	int m_LocTextureSampler;

	int m_LastTextureSampler;
	int m_LastIsTextured;
};

class CGLSLTextProgram : public CGLSLTWProgram {
public:
	CGLSLTextProgram() : CGLSLTWProgram()
	{
		m_LastColor[0] = m_LastColor[1] = m_LastColor[2] = m_LastColor[3] = -1.f;
		m_LastOutlineColor[0] = m_LastOutlineColor[1] = m_LastOutlineColor[2] = m_LastOutlineColor[3] = -1.f;
		m_LastTextSampler = m_LastTextOutlineSampler = -1;
		m_LastTextureSize = -1;
	}

	int m_LocColor;
	int m_LocOutlineColor;
	int m_LocTextSampler;
	int m_LocTextOutlineSampler;
	int m_LocTextureSize;

	float m_LastColor[4];
	float m_LastOutlineColor[4];
	int m_LastTextSampler;
	int m_LastTextOutlineSampler;
	int m_LastTextureSize;
};

class CGLSLPrimitiveProgram : public CGLSLTWProgram {
public:
};

class CGLSLSpriteProgram : public CGLSLTWProgram {
public:
	CGLSLSpriteProgram() : CGLSLTWProgram()
	{
		m_LastRotation = 0.f;
		m_LastCenter[0] = m_LastCenter[1] = 0.f;
		m_LastVertciesColor[0] = m_LastVertciesColor[1] = m_LastVertciesColor[2] = m_LastVertciesColor[3] = -1.f;
	}

	int m_LocRotation;
	int m_LocCenter;
	int m_LocVertciesColor;

	float m_LastRotation;
	float m_LastCenter[2];
	float m_LastVertciesColor[4];
};

class CGLSLSpriteMultipleProgram : public CGLSLTWProgram {
public:
	CGLSLSpriteMultipleProgram() : CGLSLTWProgram()
	{
		m_LastCenter[0] = m_LastCenter[1] = 0.f;
		m_LastVertciesColor[0] = m_LastVertciesColor[1] = m_LastVertciesColor[2] = m_LastVertciesColor[3] = -1.f;
	}

	int m_LocRSP;
	int m_LocCenter;
	int m_LocVertciesColor;

	float m_LastCenter[2];
	float m_LastVertciesColor[4];
};

class CGLSLQuadProgram : public CGLSLTWProgram {
public:
	int m_LocColor;
	int m_LocOffset;
	int m_LocRotation;
};

class CGLSLTileProgram : public CGLSLTWProgram {
public:
	int m_LocColor;
	int m_LocLOD;
	int m_LocTexelOffset;

	int m_LastLOD;
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
