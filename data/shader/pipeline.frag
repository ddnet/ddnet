#ifdef TW_TEXTURED
#ifdef TW_3D_TEXTURED
uniform sampler3D gTextureSampler;
#else
uniform sampler2DArray gTextureSampler;
#endif
#endif

void main()
{
#ifdef TW_TEXTURED
	vec4 TexColor = texture(gTextureSampler, gl_TexCoord[0].xyz).rgba;
	gl_FragColor = TexColor.rgba * gl_Color.rgba;
#else
	gl_FragColor = gl_Color.rgba;
#endif
}

