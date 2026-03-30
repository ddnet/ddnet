layout (location = 0) in vec2 inVertex;
#ifdef TW_TILE_TEXTURED
layout (location = 1) in vec3 inVertexTexCoord;
#endif

uniform mat4x2 gPos;

#if defined(TW_TILE_BORDER) || defined(TW_TILE_BORDER_LINE)
uniform vec2 gDir;
uniform vec2 gOffset;
#endif

#if defined(TW_TILE_BORDER)
uniform int gJumpIndex;
#endif

#ifdef TW_TILE_TEXTURED
noperspective out vec3 TexCoord;
#endif

void main()
{
#if defined(TW_TILE_BORDER)
	vec4 VertPos = vec4(inVertex, 0.0, 1.0);
	int XCount = gl_InstanceID - (int(gl_InstanceID/gJumpIndex) * gJumpIndex);
	int YCount = (int(gl_InstanceID/gJumpIndex));
	VertPos.x += gOffset.x + gDir.x * float(XCount);
	VertPos.y += gOffset.y + gDir.y * float(YCount);
		
	gl_Position = vec4(gPos * VertPos, 0.0, 1.0);
#elif defined(TW_TILE_BORDER_LINE)
	vec4 VertPos = vec4(inVertex.x + gOffset.x, inVertex.y + gOffset.y, 0.0, 1.0);
	VertPos.x += gDir.x * float(gl_InstanceID);
	VertPos.y += gDir.y * float(gl_InstanceID);
		
	gl_Position = vec4(gPos * VertPos, 0.0, 1.0);
#else
	gl_Position = vec4(gPos * vec4(inVertex, 0.0, 1.0), 0.0, 1.0);
#endif

#ifdef TW_TILE_TEXTURED
	TexCoord = inVertexTexCoord;
#endif
}
