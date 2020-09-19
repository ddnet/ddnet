uniform mat4x2 gPos;

void main()
{
	gl_Position = vec4(gPos * vec4(gl_Vertex.xy, 0.0, 1.0), 0.0, 1.0);
#ifdef TW_TEXTURED
	gl_TexCoord[0] = gl_MultiTexCoord0;
#endif
	gl_FrontColor = gl_Color.rgba;
	gl_BackColor = gl_Color.rgba;
}
