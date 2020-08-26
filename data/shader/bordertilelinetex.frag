#version 330

uniform sampler2D textureSampler;

uniform vec4 vertColor;
uniform float LOD;

noperspective in vec2 texCoord;

out vec4 FragClr;
void main()
{
	vec4 tex = textureLod(textureSampler, texCoord, LOD);
	FragClr = tex * vertColor;
}
