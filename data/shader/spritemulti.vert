layout (location = 0) in vec2 inVertex;
layout (location = 1) in vec2 inVertexTexCoord;
layout (location = 2) in vec4 inVertexColor;

uniform mat4x2 gPos;

uniform vec4 gRSP[228];
uniform vec2 gCenter;

noperspective out vec2 texCoord;
noperspective out vec4 vertColor;

void main()
{
	vec2 FinalPos = vec2(inVertex.xy);
	if(gRSP[gl_InstanceID].w != 0.0)
	{
		float X = FinalPos.x - gCenter.x;
		float Y = FinalPos.y - gCenter.y;
		
		FinalPos.x = X * cos(gRSP[gl_InstanceID].w) - Y * sin(gRSP[gl_InstanceID].w) + gCenter.x;
		FinalPos.y = X * sin(gRSP[gl_InstanceID].w) + Y * cos(gRSP[gl_InstanceID].w) + gCenter.y;
	}
	
	FinalPos.x *= gRSP[gl_InstanceID].z;
	FinalPos.y *= gRSP[gl_InstanceID].z;
		
	FinalPos.x += gRSP[gl_InstanceID].x;
	FinalPos.y += gRSP[gl_InstanceID].y;

	gl_Position = vec4(gPos * vec4(FinalPos, 0.0, 1.0), 0.0, 1.0);
	texCoord = inVertexTexCoord;
	vertColor = inVertexColor;
}
