// This file can be included several times.
#if(!defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_OPENGL_SL_PROGRAM_H)) || \
	(defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_OPENGL_SL_PROGRAM_H_AS_ES))

#if !defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_OPENGL_SL_PROGRAM_H)
#define ENGINE_CLIENT_BACKEND_OPENGL_OPENGL_SL_PROGRAM_H
#endif

#if defined(BACKEND_AS_OPENGL_ES) && !defined(ENGINE_CLIENT_BACKEND_OPENGL_OPENGL_SL_PROGRAM_H_AS_ES)
#define ENGINE_CLIENT_BACKEND_OPENGL_OPENGL_SL_PROGRAM_H_AS_ES
#endif

#include <base/color.h>
#include <base/vmath.h>
#include <engine/client/graphics_defines.h>

class CGLSL;

class CGLSLProgram
{
public:
	void CreateProgram();
	void DeleteProgram();

	bool AddShader(CGLSL *pShader) const;

	void LinkProgram();
	void UseProgram() const;
	TWGLuint GetProgramId() const;

	void DetachShader(CGLSL *pShader) const;
	void DetachShaderById(TWGLuint ShaderId) const;
	void DetachAllShaders() const;

	//Support various types
	void SetUniformVec2(int Loc, int Count, const float *pValue);
	void SetUniformVec4(int Loc, int Count, const float *pValue);
	void SetUniform(int Loc, int Value);
	void SetUniform(int Loc, bool Value);
	void SetUniform(int Loc, float Value);
	void SetUniform(int Loc, int Count, const float *pValues);

	//for performance reason we do not use SetUniform with using strings... save the Locations of the variables instead
	int GetUniformLoc(const char *pName) const;

	CGLSLProgram();
	virtual ~CGLSLProgram();

protected:
	TWGLuint m_ProgramId;
	bool m_IsLinked;
};

class CGLSLTWProgram : public CGLSLProgram
{
public:
	CGLSLTWProgram() :
		m_LocPos(-1), m_LocTextureSampler(-1), m_LastTextureSampler(-1), m_LastIsTextured(-1)
	{
		m_LastScreenTL = m_LastScreenBR = vec2(-1, -1);
	}

	int m_LocPos;
	int m_LocTextureSampler;

	int m_LastTextureSampler;
	int m_LastIsTextured;
	vec2 m_LastScreenTL;
	vec2 m_LastScreenBR;
};

class CGLSLTextProgram : public CGLSLTWProgram
{
public:
	CGLSLTextProgram()

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

	ColorRGBA m_LastColor;
	ColorRGBA m_LastOutlineColor;
	int m_LastTextSampler;
	int m_LastTextOutlineSampler;
	int m_LastTextureSize;
};

class CGLSLPrimitiveProgram : public CGLSLTWProgram
{
public:
};

class CGLSLPrimitiveExProgram : public CGLSLTWProgram
{
public:
	CGLSLPrimitiveExProgram()

	{
		m_LastRotation = 0.f;
		m_LastCenter = vec2(0, 0);
		m_LastVerticesColor = ColorRGBA(-1, -1, -1, -1);
	}

	int m_LocRotation;
	int m_LocCenter;
	int m_LocVertciesColor;

	float m_LastRotation;
	vec2 m_LastCenter;
	ColorRGBA m_LastVerticesColor;
};

class CGLSLSpriteMultipleProgram : public CGLSLTWProgram
{
public:
	CGLSLSpriteMultipleProgram()

	{
		m_LastCenter = vec2(0, 0);
		m_LastVerticesColor = ColorRGBA(-1, -1, -1, -1);
	}

	int m_LocRSP;
	int m_LocCenter;
	int m_LocVertciesColor;

	vec2 m_LastCenter;
	ColorRGBA m_LastVerticesColor;
};

class CGLSLQuadProgram : public CGLSLTWProgram
{
public:
	int m_LocColors;
	int m_LocOffsets;
	int m_LocRotations;
	int m_LocQuadOffset;
};

class CGLSLTileProgram : public CGLSLTWProgram
{
public:
	CGLSLTileProgram() :
		m_LocColor(-1), m_LocOffset(-1), m_LocDir(-1), m_LocNum(-1), m_LocJumpIndex(-1) {}

	int m_LocColor;
	int m_LocOffset;
	int m_LocDir;
	int m_LocScale = -1;
	int m_LocNum;
	int m_LocJumpIndex;
};

#endif
