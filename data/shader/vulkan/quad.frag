#version 450
#extension GL_ARB_separate_shader_objects : enable

#ifdef TW_QUAD_TEXTURED
layout (set = 0, binding = 0) uniform sampler2D gTextureSampler;
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
#define QuadIndex 0
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

layout (location = 0) noperspective in vec4 QuadColor;
#ifndef TW_PUSH_CONST
layout (location = 1) flat in int QuadIndex;
#endif
#ifdef TW_QUAD_TEXTURED
#ifndef TW_PUSH_CONST
layout (location = 2) noperspective in vec2 TexCoord;
#else
layout (location = 1) noperspective in vec2 TexCoord;
#endif
#endif

layout (location = 0) out vec4 FragClr;
void main()
{
#ifdef TW_QUAD_TEXTURED
	vec4 TexColor = texture(gTextureSampler, TexCoord);
	FragClr = TexColor * QuadColor * gQuadBO.gUniEls[QuadIndex].gVertColor;
#else
	FragClr = QuadColor * gQuadBO.gUniEls[QuadIndex].gVertColor;
#endif
}
