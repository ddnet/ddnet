#version 330

layout (location = 0) in vec2 inVertex;

uniform mat4x2 Pos;

void main()
{
	gl_Position = vec4(Pos * vec4(inVertex, 0.0, 1.0), 0.0, 1.0);
}
