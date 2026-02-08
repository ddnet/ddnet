#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 inVertex;
layout (location = 1) in vec2 inVertexTexCoord;
layout (location = 2) in vec4 inVertexColor;

layout(push_constant) uniform SPosBO {
	layout(offset = 0) mat4x2 gPos;
} gPosBO;

layout (location = 0) noperspective out vec2 texCoord;
layout (location = 1) noperspective out vec4 vertColor;

void main()
{
	gl_Position = vec4(gPosBO.gPos * vec4(inVertex, 0.0, 1.0), 0.0, 1.0);
	texCoord = inVertexTexCoord;
	vertColor = vec4(inVertexColor);
}
