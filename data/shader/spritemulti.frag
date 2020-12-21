uniform sampler2D gTextureSampler;

uniform vec4 gVerticesColor;

noperspective in vec2 texCoord;
noperspective in vec4 vertColor;

out vec4 FragClr;
void main()
{
	vec4 tex = texture(gTextureSampler, texCoord);
	FragClr = tex * vertColor * gVerticesColor;
}
