#version 450
#extension GL_ARB_separate_shader_objects : enable

#ifdef TW_TILE_TEXTURED
layout(binding = 0) uniform sampler2DArray gTextureSampler;
#endif

layout(push_constant) uniform SVertexColorBO {
	layout(offset = 64) uniform vec4 gVertColor;
} gColorBO;

#ifdef TW_TILE_TEXTURED
layout (location = 0) noperspective in vec3 TexCoord;
#endif

layout (location = 0) out vec4 FragClr;
void main()
{
#ifdef TW_TILE_TEXTURED
	vec4 TexColor = texture(gTextureSampler, TexCoord.xyz);
	FragClr = TexColor * gColorBO.gVertColor;
#else
	FragClr = gColorBO.gVertColor;
#endif
}
