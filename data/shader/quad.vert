layout (location = 0) in vec4 inVertex;
layout (location = 1) in vec4 inColor;
#ifdef TW_QUAD_TEXTURED
layout (location = 2) in vec2 inVertexTexCoord;
#endif

uniform mat4x2 gPos;

uniform vec2 gOffsets[TW_MAX_QUADS];
uniform float gRotations[TW_MAX_QUADS];

uniform int gQuadOffset;

noperspective out vec4 QuadColor;
flat out int QuadIndex;
#ifdef TW_QUAD_TEXTURED
noperspective out vec2 TexCoord;
#endif

void main()
{
	vec2 FinalPos = vec2(inVertex.xy);
	
	int TmpQuadIndex = int(gl_VertexID / 4) - gQuadOffset;

	if(gRotations[TmpQuadIndex] != 0.0)
	{
		float X = FinalPos.x - inVertex.z;
		float Y = FinalPos.y - inVertex.w;
		
		FinalPos.x = X * cos(gRotations[TmpQuadIndex]) - Y * sin(gRotations[TmpQuadIndex]) + inVertex.z;
		FinalPos.y = X * sin(gRotations[TmpQuadIndex]) + Y * cos(gRotations[TmpQuadIndex]) + inVertex.w;
	}
	
	FinalPos.x = FinalPos.x / 1024.0 + gOffsets[TmpQuadIndex].x;
	FinalPos.y = FinalPos.y / 1024.0 + gOffsets[TmpQuadIndex].y;

	gl_Position = vec4(gPos * vec4(FinalPos, 0.0, 1.0), 0.0, 1.0);
	QuadColor = inColor;
	QuadIndex = TmpQuadIndex;
#ifdef TW_QUAD_TEXTURED
	TexCoord = inVertexTexCoord;
#endif
}
