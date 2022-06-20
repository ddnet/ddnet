#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 inVertex;
layout (location = 1) in vec2 inVertexTexCoord;
layout (location = 2) in vec4 inVertexColor;

layout(push_constant) uniform SPosBO {
	layout(offset = 0) mat4x2 gPos;
#ifndef TW_ROTATIONLESS
	layout(offset = 32) vec2 gCenter;
	layout(offset = 40) float gRotation;
#endif
} gPosBO;

layout (location = 0) noperspective out vec2 texCoord;
layout (location = 1) noperspective out vec4 vertColor;

void main()
{
	vec2 FinalPos = vec2(inVertex.xy);
#ifndef TW_ROTATIONLESS
	float X = FinalPos.x - gPosBO.gCenter.x;
	float Y = FinalPos.y - gPosBO.gCenter.y;
	
	FinalPos.x = X * cos(gPosBO.gRotation) - Y * sin(gPosBO.gRotation) + gPosBO.gCenter.x;
	FinalPos.y = X * sin(gPosBO.gRotation) + Y * cos(gPosBO.gRotation) + gPosBO.gCenter.y;
#endif

	gl_Position = vec4(gPosBO.gPos * vec4(FinalPos, 0.0, 1.0), 0.0, 1.0);
	texCoord = inVertexTexCoord;
	vertColor = inVertexColor;
}
