#version 330

uniform sampler2D textureSampler;

uniform vec4 vertColor;

noperspective in vec2 texCoord;

void main()
{
	vec4 tex = texture2D(textureSampler, texCoord);
	gl_FragColor = tex * vertColor;	
}