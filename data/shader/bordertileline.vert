#version 330

layout (location = 0) in vec2 inVertex;

uniform mat4x2 Pos;

uniform vec2 Dir;
uniform vec2 Offset;

void main()
{
	vec4 VertPos = vec4(inVertex.x + Offset.x, inVertex.y + Offset.y, 0.0, 1.0);
	VertPos.x += Dir.x * gl_InstanceID;
	VertPos.y += Dir.y * gl_InstanceID;
		
	gl_Position = vec4(Pos * VertPos, 0.0, 1.0);
}
