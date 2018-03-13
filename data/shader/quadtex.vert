#version 330

layout (location = 0) in vec4 inVertex;
layout (location = 1) in vec2 inVertexTexCoord;
layout (location = 2) in vec4 inColor;

uniform mat4x2 Pos;

uniform vec2 Offset;
uniform float Rotation;

noperspective out vec2 texCoord;
noperspective out vec4 quadColor;

void main()
{
	vec2 FinalPos = vec2(inVertex.xy);

	if(Rotation != 0.0)
	{
		float X = FinalPos.x - inVertex.z;
		float Y = FinalPos.y - inVertex.w;
		
		FinalPos.x = X * cos(Rotation) - Y * sin(Rotation) + inVertex.z;
		FinalPos.y = X * sin(Rotation) + Y * cos(Rotation) + inVertex.w;
	}
	
	FinalPos.x = FinalPos.x / 1024.0 + Offset.x;
	FinalPos.y = FinalPos.y / 1024.0 + Offset.y;

	gl_Position = vec4(Pos * vec4(FinalPos, 0.0, 1.0), 0.0, 1.0);
	texCoord = inVertexTexCoord;
	quadColor = inColor;
}
