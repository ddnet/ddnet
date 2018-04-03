#version 330

layout (location = 0) in vec2 inVertex;
layout (location = 1) in vec2 inVertexTexCoord;
layout (location = 2) in vec4 inVertexColor;

uniform mat4x2 Pos;

uniform float Rotation;
uniform vec2 Center;

noperspective out vec2 texCoord;
noperspective out vec4 vertColor;

void main()
{
	vec2 FinalPos = vec2(inVertex.xy);
	if(Rotation != 0.0)
	{
		float X = FinalPos.x - Center.x;
		float Y = FinalPos.y - Center.y;
		
		FinalPos.x = X * cos(Rotation) - Y * sin(Rotation) + Center.x;
		FinalPos.y = X * sin(Rotation) + Y * cos(Rotation) + Center.y;
	}

	gl_Position = vec4(Pos * vec4(FinalPos, 0.0, 1.0), 0.0, 1.0);
	texCoord = inVertexTexCoord;
	vertColor = inVertexColor;
}
