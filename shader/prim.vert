#version 330

layout (location = 0) in vec2 inVertex;
layout (location = 1) in vec2 inVertexTexCoord;
layout (location = 2) in vec4 inVertexColor;

uniform mat4x2 Pos;

noperspective out vec2 texCoord;
noperspective out vec4 vertColor;

void main()
{
	gl_Position = vec4(Pos * vec4(inVertex, 0.0, 1.0), 0.0, 1.0);
	texCoord = inVertexTexCoord;
	vertColor = inVertexColor;
}