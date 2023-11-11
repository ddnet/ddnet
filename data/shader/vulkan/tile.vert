#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 inVertex;
#ifdef TW_TILE_TEXTURED
layout (location = 1) in uvec4 inVertexTexCoord;
#endif

layout(push_constant) uniform SPosBO {
	layout(offset = 0) uniform mat4x2 gPos;
} gPosBO;

#ifdef TW_TILE_TEXTURED
layout (location = 0) noperspective out vec3 TexCoord;
#endif

void main()
{
	gl_Position = vec4(gPosBO.gPos * vec4(inVertex, 0.0, 1.0), 0.0, 1.0);

#ifdef TW_TILE_TEXTURED
	TexCoord = vec3(inVertexTexCoord.xyz);
#endif
}
