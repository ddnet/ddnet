#ifdef TW_TILE_TEXTURED
#ifdef TW_TILE_3D_TEXTURED
uniform sampler3D gTextureSampler;
#else
uniform sampler2DArray gTextureSampler;
#endif
#endif

uniform vec4 gVertColor;

#ifdef TW_TILE_TEXTURED
noperspective centroid in vec3 TexCoord;
#endif

out vec4 FragClr;

void main()
{
#ifdef TW_TILE_TEXTURED
	vec3 realTexCoords = vec3(fract(TexCoord.xy), TexCoord.z);
	vec2 dx = dFdx(TexCoord.xy);
	vec2 dy = dFdy(TexCoord.xy);
	vec4 tex = textureGrad(gTextureSampler, realTexCoords, dx, dy);
	FragClr = tex * gVertColor;
#else
	FragClr = gVertColor;
#endif
}
