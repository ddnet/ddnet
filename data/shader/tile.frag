#ifdef TW_TILE_TEXTURED
#ifdef TW_TILE_3D_TEXTURED
uniform sampler3D gTextureSampler;
#else
uniform sampler2DArray gTextureSampler;
#endif
#endif

uniform vec4 gVertColor;

#ifdef TW_TILE_TEXTURED
noperspective in vec3 TexCoord;
#endif

out vec4 FragClr;
void main()
{
#ifdef TW_TILE_TEXTURED
	vec4 TexColor = texture(gTextureSampler, TexCoord.xyz);
	FragClr = TexColor * gVertColor;
#else
	FragClr = gVertColor;
#endif
}
