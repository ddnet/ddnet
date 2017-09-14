#version 330

layout (location = 0) in vec2 inVertex;
layout (location = 1) in vec2 inVertexTexCoord;
layout (location = 2) in vec4 inVertexColor;

uniform mat4 Pos;

noperspective out vec2 texCoord;
noperspective out vec4 vertColor;

void main()
{
	gl_Position = Pos * vec4(inVertex, -5.0, 1.0);
	texCoord = inVertexTexCoord;
	vertColor = inVertexColor;
}