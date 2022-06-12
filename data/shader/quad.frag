#ifdef TW_QUAD_TEXTURED
uniform sampler2D gTextureSampler;
#endif

uniform vec4 gVertColors[TW_MAX_QUADS];

noperspective in vec4 QuadColor;
flat in int QuadIndex;
#ifdef TW_QUAD_TEXTURED
noperspective in vec2 TexCoord;
#endif

out vec4 FragClr;
void main()
{
#ifdef TW_QUAD_TEXTURED
	vec4 TexColor = texture(gTextureSampler, TexCoord);
	FragClr = TexColor * QuadColor * gVertColors[QuadIndex];
#else
	FragClr = QuadColor * gVertColors[QuadIndex];
#endif
}
