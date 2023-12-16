#version 450
#extension GL_ARB_separate_shader_objects : enable

#ifdef TW_TILE_TEXTURED
layout(binding = 0) uniform sampler2DArray gTextureSampler;
#endif

layout(push_constant) uniform SVertexColorBO {
	layout(offset = 64) uniform vec4 gVertColor;
} gColorBO;

#ifdef TW_TILE_TEXTURED
layout (location = 0) noperspective centroid in vec3 TexCoord;
#endif

layout (location = 0) out vec4 FragClr;
void main()
{
#ifdef TW_TILE_TEXTURED
	vec3 realTexCoords = vec3(fract(TexCoord.xy), TexCoord.z);
	vec2 dx = dFdx(TexCoord.xy);
	vec2 dy = dFdy(TexCoord.xy);
	vec4 tex = textureGrad(gTextureSampler, realTexCoords, dx, dy);
	FragClr = tex * gColorBO.gVertColor;
#else
	FragClr = gColorBO.gVertColor;
#endif
}
