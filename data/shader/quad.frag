#version 330

uniform vec4 vertColor;

noperspective in vec4 quadColor;

out vec4 FragClr;
void main()
{
	FragClr = quadColor * vertColor;
}
