#version 330

uniform int isTextured;
uniform sampler2D textureSampler;

noperspective in vec2 texCoord;
noperspective in vec4 vertColor;

out vec4 FragClr;
void main()
{
	if(isTextured == 1) {
		vec4 tex = texture(textureSampler, texCoord);
		FragClr = tex * vertColor;
	}
	else FragClr = vertColor;
}
