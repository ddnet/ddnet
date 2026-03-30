#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 inVertex;
#ifdef TW_TILE_TEXTURED
layout (location = 1) in vec3 inVertexTexCoord;
#endif

layout(push_constant) uniform SPosBO {
	layout(offset = 0) uniform mat4x2 gPos;

#if defined(TW_TILE_BORDER) || defined(TW_TILE_BORDER_LINE)
	layout(offset = 32) uniform vec2 gDir;
	layout(offset = 40) uniform vec2 gOffset;
#endif

#if defined(TW_TILE_BORDER)
	layout(offset = 48) uniform int gJumpIndex;
#endif
} gPosBO;

#ifdef TW_TILE_TEXTURED
layout (location = 0) noperspective out vec3 TexCoord;
#endif

void main()
{
#if defined(TW_TILE_BORDER)
	vec4 VertPos = vec4(inVertex, 0.0, 1.0);
	int XCount = gl_InstanceIndex - (int(gl_InstanceIndex/gPosBO.gJumpIndex) * gPosBO.gJumpIndex);
	int YCount = (int(gl_InstanceIndex/gPosBO.gJumpIndex));
	VertPos.x += gPosBO.gOffset.x + gPosBO.gDir.x * float(XCount);
	VertPos.y += gPosBO.gOffset.y + gPosBO.gDir.y * float(YCount);
		
	gl_Position = vec4(gPosBO.gPos * VertPos, 0.0, 1.0);
#elif defined(TW_TILE_BORDER_LINE)
	vec4 VertPos = vec4(inVertex.x + gPosBO.gOffset.x, inVertex.y + gPosBO.gOffset.y, 0.0, 1.0);
	VertPos.x += gPosBO.gDir.x * float(gl_InstanceIndex);
	VertPos.y += gPosBO.gDir.y * float(gl_InstanceIndex);
		
	gl_Position = vec4(gPosBO.gPos * VertPos, 0.0, 1.0);
#else
	gl_Position = vec4(gPosBO.gPos * vec4(inVertex, 0.0, 1.0), 0.0, 1.0);
#endif

#ifdef TW_TILE_TEXTURED
	TexCoord = inVertexTexCoord;
#endif
}
