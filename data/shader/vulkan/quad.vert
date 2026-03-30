#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec4 inVertex;
layout (location = 1) in vec4 inColor;
#ifdef TW_QUAD_TEXTURED
layout (location = 2) in vec2 inVertexTexCoord;
#endif

#ifdef TW_QUAD_TEXTURED
#define UBOSetIndex 1
#else
#define UBOSetIndex 0
#endif

struct SQuadUniformEl {
	vec4 gVertColor;
	vec2 gOffset;
	float gRotation;
};

#ifndef TW_PUSH_CONST
#define TW_MAX_QUADS 256

layout (std140, set = UBOSetIndex, binding = 1) uniform SOffBO {
	uniform SQuadUniformEl gUniEls[TW_MAX_QUADS];
} gQuadBO;
#else
#define gQuadBO gPosBO
#define TmpQuadIndex 0
#endif

layout(push_constant) uniform SPosBO {
	layout(offset = 0) uniform mat4x2 gPos;
#ifdef TW_PUSH_CONST
	layout(offset = 32) uniform SQuadUniformEl gUniEls[1];
	layout(offset = 64) uniform int gQuadOffset;
#else
	layout(offset = 32) uniform int gQuadOffset;
#endif
} gPosBO;

layout (location = 0) noperspective out vec4 QuadColor;
#ifndef TW_PUSH_CONST
layout (location = 1) flat out int QuadIndex;
#endif
#ifdef TW_QUAD_TEXTURED
#ifndef TW_PUSH_CONST
layout (location = 2) noperspective out vec2 TexCoord;
#else
layout (location = 1) noperspective out vec2 TexCoord;
#endif
#endif

void main()
{
	vec2 FinalPos = vec2(inVertex.xy);

#ifndef TW_PUSH_CONST
	int TmpQuadIndex = int(gl_VertexIndex / 4) - gPosBO.gQuadOffset;
#endif

	if(gQuadBO.gUniEls[TmpQuadIndex].gRotation != 0.0)
	{
		float X = FinalPos.x - inVertex.z;
		float Y = FinalPos.y - inVertex.w;
		
		FinalPos.x = X * cos(gQuadBO.gUniEls[TmpQuadIndex].gRotation) - Y * sin(gQuadBO.gUniEls[TmpQuadIndex].gRotation) + inVertex.z;
		FinalPos.y = X * sin(gQuadBO.gUniEls[TmpQuadIndex].gRotation) + Y * cos(gQuadBO.gUniEls[TmpQuadIndex].gRotation) + inVertex.w;
	}

	FinalPos.x = FinalPos.x / 1024.0 + gQuadBO.gUniEls[TmpQuadIndex].gOffset.x;
	FinalPos.y = FinalPos.y / 1024.0 + gQuadBO.gUniEls[TmpQuadIndex].gOffset.y;

	gl_Position = vec4(gPosBO.gPos * vec4(FinalPos, 0.0, 1.0), 0.0, 1.0);
	QuadColor = inColor;
#ifndef TW_PUSH_CONST
	QuadIndex = TmpQuadIndex;
#endif
#ifdef TW_QUAD_TEXTURED
	TexCoord = inVertexTexCoord;
#endif
}
