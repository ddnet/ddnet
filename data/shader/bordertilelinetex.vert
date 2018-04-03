#version 330

layout (location = 0) in vec2 inVertex;
layout (location = 1) in vec2 inVertexTexCoord;
layout (location = 2) in ivec2 inVertexTexRightOrBottom;

uniform mat4x2 Pos;
uniform float TexelOffset;

uniform vec2 Dir;

noperspective out vec2 texCoord;

void main()
{
	vec4 VertPos = vec4(inVertex, 0.0, 1.0);
	VertPos.x += Dir.x * (gl_InstanceID+1);
	VertPos.y += Dir.y * (gl_InstanceID+1);
		
	gl_Position = vec4(Pos * VertPos, 0.0, 1.0);

	float tx = (inVertexTexCoord.x/(16.0));
	float ty = (inVertexTexCoord.y/(16.0));

	texCoord = vec2(tx + (inVertexTexRightOrBottom.x == 0 ? TexelOffset : -TexelOffset), ty + (inVertexTexRightOrBottom.y == 0 ? TexelOffset : -TexelOffset));
}
