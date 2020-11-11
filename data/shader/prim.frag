uniform sampler2D gTextureSampler;

noperspective in vec2 texCoord;
noperspective in vec4 vertColor;

out vec4 FragClr;
void main()
{
#ifdef TW_TEXTURED
	vec4 tex = texture(gTextureSampler, texCoord);
	FragClr = tex * vertColor;
#else
	FragClr = vertColor;
#endif
}
