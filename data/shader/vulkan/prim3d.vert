#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 inVertex;
layout (location = 1) in vec4 inVertexColor;
layout (location = 2) in vec3 inVertexTexCoord;

layout(push_constant) uniform SPosBO {
	layout(offset = 0) mat4x2 gPos;
} gPosBO;

layout (location = 0) noperspective out vec4 oVertColor;
#ifdef TW_TEXTURED
layout (location = 1) noperspective out vec3 oTexCoord;
#endif

void main()
{
	gl_Position = vec4(gPosBO.gPos * vec4(inVertex, 0.0, 1.0), 0.0, 1.0);
#ifdef TW_TEXTURED
	oTexCoord = inVertexTexCoord;
#endif
	oVertColor = inVertexColor;
}
