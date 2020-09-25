#ifdef TW_TEXTURED
#ifdef TW_3D_TEXTURED
uniform sampler3D gTextureSampler;
#else
uniform sampler2DArray gTextureSampler;
#endif
#endif

#ifdef TW_MODERN_GL
#ifdef TW_TEXTURED
noperspective in vec3 oTexCoord;
#endif
noperspective in vec4 oVertColor;
out vec4 FragClr;
#endif

void main()
{
#ifdef TW_MODERN_GL
#ifdef TW_TEXTURED
	vec4 TexColor = texture(gTextureSampler, oTexCoord.xyz).rgba;
	FragClr = TexColor.rgba * oVertColor.rgba;
#else
	FragClr = oVertColor.rgba;
#endif
#else
#ifdef TW_TEXTURED
	vec4 TexColor = texture(gTextureSampler, gl_TexCoord[0].xyz).rgba;
	gl_FragColor = TexColor.rgba * gl_Color.rgba;
#else
	gl_FragColor = gl_Color.rgba;
#endif
#endif
}

