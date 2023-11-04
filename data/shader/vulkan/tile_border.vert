#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 inVertex;
#ifdef TW_TILE_TEXTURED
layout (location = 1) in uvec4 inVertexTexCoord;
#endif

layout(push_constant) uniform SPosBO {
	layout(offset = 0) uniform mat4x2 gPos;

	layout(offset = 32) uniform vec2 gOffset;
	layout(offset = 40) uniform vec2 gScale;
} gPosBO;

#ifdef TW_TILE_TEXTURED
layout (location = 0) noperspective out vec3 TexCoord;
#endif

void main()
{
	// scale then position vertex
	vec2 VertexPos = (inVertex * gPosBO.gScale) + gPosBO.gOffset;
	gl_Position = vec4(gPosBO.gPos * vec4(VertexPos, 0.0, 1.0), 0.0, 1.0);

#ifdef TW_TILE_TEXTURED
	// scale the texture coordinates too
	vec2 TexScale = gPosBO.gScale;
	if (inVertexTexCoord.w > 0)
		TexScale = gPosBO.gScale.yx;
	TexCoord = vec3(vec2(inVertexTexCoord.xy) * TexScale, float(inVertexTexCoord.z));
#endif
}
