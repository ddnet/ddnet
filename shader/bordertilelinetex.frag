#version 330

uniform sampler2D textureSampler;

uniform vec4 vertColor;

noperspective in vec2 texCoord;

out vec4 FragClr;
void main()
{
	vec4 tex = texture(textureSampler, texCoord);
	FragClr = tex * vertColor;
}