#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform sampler2D gTextureSampler;

layout(push_constant) uniform SVertexColorBO {
#ifdef TW_PUSH_CONST
	layout(offset = 64) vec4 gVerticesColor;
#else
	layout(offset = 48) vec4 gVerticesColor;
#endif
} gColorBO;

layout (location = 0) noperspective in vec2 texCoord;
layout (location = 1) noperspective in vec4 vertColor;

layout (location = 0) out vec4 FragClr;

void main()
{
	vec4 tex = texture(gTextureSampler, texCoord);
	FragClr = tex * vertColor * gColorBO.gVerticesColor;
}
