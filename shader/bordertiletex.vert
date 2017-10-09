#version 330

layout (location = 0) in vec2 inVertex;
layout (location = 1) in vec2 inVertexTexCoord;
layout (location = 2) in ivec2 inVertexTexRightOrBottom;

uniform mat4x2 Pos;
uniform float LOD;

uniform vec2 Offset;
uniform vec2 Dir;
uniform int JumpIndex;

noperspective out vec2 texCoord;

void main()
{
	vec4 VertPos = vec4(inVertex, 0.0, 1.0);
	int XCount = gl_InstanceID - (int(gl_InstanceID/JumpIndex) * JumpIndex);
	int YCount = (int(gl_InstanceID/JumpIndex));
	VertPos.x += Offset.x + Dir.x * XCount;
	VertPos.y += Offset.y + Dir.y * YCount;
		
	gl_Position = vec4(Pos * VertPos, 0.0, 1.0);
	float F1 = -(0.5/(1024.0 * pow(0.5, (LOD+1.0))));
	float F2 = (0.5/(1024.0 * pow(0.5, (LOD+1.0))));
	float tx = (inVertexTexCoord.x/(16.0));
	float ty = (inVertexTexCoord.y/(16.0));
	texCoord = vec2(tx + (inVertexTexRightOrBottom.x == 0 ? F2 : F1), ty + (inVertexTexRightOrBottom.y == 0 ? F2 : F1));
}