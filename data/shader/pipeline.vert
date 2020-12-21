#ifdef TW_MODERN_GL
layout (location = 0) in vec2 inVertex;
layout (location = 1) in vec4 inVertexColor;
layout (location = 2) in vec3 inVertexTexCoord;
#endif

uniform mat4x2 gPos;

#ifdef TW_MODERN_GL
#ifdef TW_TEXTURED
noperspective out vec3 oTexCoord;
#endif
noperspective out vec4 oVertColor;
#endif

void main()
{
#ifdef TW_MODERN_GL
	gl_Position = vec4(gPos * vec4(inVertex, 0.0, 1.0), 0.0, 1.0);
#ifdef TW_TEXTURED
	oTexCoord = inVertexTexCoord;
#endif
	oVertColor = inVertexColor;
#else
	gl_Position = vec4(gPos * vec4(gl_Vertex.xy, 0.0, 1.0), 0.0, 1.0);
#ifdef TW_TEXTURED
	gl_TexCoord[0] = gl_MultiTexCoord0;
#endif
	gl_FrontColor = gl_Color.rgba;
	gl_BackColor = gl_Color.rgba;
#endif
}
