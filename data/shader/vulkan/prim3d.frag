#version 450
#extension GL_ARB_separate_shader_objects : enable

#ifdef TW_TEXTURED
layout (binding = 0) uniform sampler2DArray gTextureSampler;
#endif

layout (location = 0) noperspective in vec4 oVertColor;
#ifdef TW_TEXTURED
layout (location = 1) noperspective in vec3 oTexCoord;
#endif

layout (location = 0) out vec4 FragClr;

void main()
{
#ifdef TW_TEXTURED
	vec4 TexColor = texture(gTextureSampler, oTexCoord.xyz).rgba;
	FragClr = TexColor.rgba * oVertColor.rgba;
#else
	FragClr = oVertColor.rgba;
#endif
}

