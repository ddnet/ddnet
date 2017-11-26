#version 330

layout (location = 0) in vec2 inVertex;

uniform mat4x2 Pos;

uniform vec2 Dir;

void main()
{
	vec4 VertPos = vec4(inVertex, 0.0, 1.0);
	VertPos.x += Dir.x * (gl_InstanceID+1);
	VertPos.y += Dir.y * (gl_InstanceID+1);
		
	gl_Position = vec4(Pos * VertPos, 0.0, 1.0);
}
