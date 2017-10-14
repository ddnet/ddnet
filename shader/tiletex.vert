#version 330

layout (location = 0) in vec2 inVertex;
layout (location = 1) in vec2 inVertexTexCoord;
layout (location = 2) in ivec2 inVertexTexRightOrBottom;

uniform mat4x2 Pos;
uniform float LOD;

noperspective out vec2 texCoord;
flat out float fragLOD;

void main()
{
	gl_Position = vec4(Pos * vec4(inVertex, 0.0, 1.0), 0.0, 1.0);
	float F1 = -(0.5/(1024.0 * pow(0.5, LOD)));
	float F2 = (0.5/(1024.0 * pow(0.5, LOD)));
	float tx = (inVertexTexCoord.x/(16.0));
	float ty = (inVertexTexCoord.y/(16.0));
	texCoord = vec2(tx + (inVertexTexRightOrBottom.x == 0 ? F2 : F1), ty + (inVertexTexRightOrBottom.y == 0 ? F2 : F1));
	fragLOD = LOD;
}