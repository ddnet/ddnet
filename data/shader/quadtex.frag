#version 330

uniform sampler2D textureSampler;

uniform vec4 vertColor;

noperspective in vec2 texCoord;
noperspective in vec4 quadColor;

out vec4 FragClr;
void main()
{
	vec4 tex = texture(textureSampler, texCoord);
	FragClr = tex * quadColor * vertColor;	
}
