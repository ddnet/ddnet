#version 330

uniform int isTextured;
uniform sampler2D textureSampler;

smooth in vec2 texCoord;
smooth in vec4 vertColor;

void main()
{
	if(isTextured == 1) {
		vec4 tex = texture2D(textureSampler, texCoord);
		gl_FragColor = tex * vertColor;
	}
	else gl_FragColor = vertColor;
}