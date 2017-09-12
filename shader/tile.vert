#version 330

layout (location = 0) in vec2 inVertex;

uniform mat4 Pos;

void main()
{
	gl_Position = Pos * vec4(inVertex, -5.0, 1.0);
}