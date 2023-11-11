layout (location = 0) in vec2 inVertex;
#ifdef TW_TILE_TEXTURED
layout (location = 1) in uvec4 inVertexTexCoord;
#endif

uniform mat4x2 gPos;

uniform vec2 gOffset;
uniform vec2 gScale;

#ifdef TW_TILE_TEXTURED
noperspective out vec3 TexCoord;
#endif

void main()
{
	// scale then position vertex
	vec2 VertexPos = (inVertex * gScale) + gOffset;
	gl_Position = vec4(gPos * vec4(VertexPos, 0.0, 1.0), 0.0, 1.0);

#ifdef TW_TILE_TEXTURED
	// scale the texture coordinates too
	vec2 TexScale = gScale;
	if (float(inVertexTexCoord.w) > 0.0)
		TexScale = gScale.yx;
	TexCoord = vec3(vec2(inVertexTexCoord.xy) * TexScale, float(inVertexTexCoord.z));
#endif
}
