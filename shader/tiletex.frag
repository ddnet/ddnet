#version 330

uniform sampler2D textureSampler;

uniform vec4 vertColor;

noperspective in vec2 texCoord;
flat in float fragLOD;

out vec4 FragClr;
void main()
{
	vec4 tex = textureLod(textureSampler, texCoord, fragLOD);
	FragClr = tex * vertColor;	
}