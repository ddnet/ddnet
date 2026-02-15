#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform SFragConstBO {
	layout(offset = 32) float gInnerRadius;
	layout(offset = 36) float gArcStart;
	layout(offset = 40) float gArcLen;
	layout(offset = 44) float gPadding;
	layout(offset = 48) vec4 gFilledColor;
	layout(offset = 64) vec4 gUnfilledColor;
} gFragConst;

layout (location = 0) noperspective in vec2 texCoord;
layout (location = 1) noperspective in vec4 vertColor;

layout (location = 0) out vec4 FragClr;

#define PI 3.14159265358979323846

void main()
{
	float dist = length(texCoord);

	// Ring SDF with smooth edges
	float ringAA = fwidth(dist);
	float outerMask = 1.0 - smoothstep(1.0 - ringAA * 0.5, 1.0 + ringAA * 0.5, dist);
	float innerMask = smoothstep(gFragConst.gInnerRadius - ringAA * 0.5, gFragConst.gInnerRadius + ringAA * 0.5, dist);
	float ringMask = outerMask * innerMask;

	if(ringMask <= 0.0)
		discard;

	// Arc containment
	float arcMask;
	if(gFragConst.gArcLen >= 1.0)
	{
		arcMask = 1.0;
	}
	else if(gFragConst.gArcLen <= 0.0)
	{
		arcMask = 0.0;
	}
	else
	{
		vec2 dir = texCoord / dist;

		float endAngle = gFragConst.gArcStart + gFragConst.gArcLen * 2.0 * PI;
		vec2 startDir = vec2(sin(gFragConst.gArcStart), -cos(gFragConst.gArcStart));
		vec2 endDir = vec2(sin(endAngle), -cos(endAngle));

		float sinFromStart = startDir.x * dir.y - startDir.y * dir.x;
		float sinFromEnd = dir.x * endDir.y - dir.y * endDir.x;

		float aaWidth = max(length(dFdx(texCoord)), length(dFdy(texCoord))) / dist;

		float startEdge = smoothstep(-aaWidth, aaWidth, sinFromStart);
		float endEdge = smoothstep(-aaWidth, aaWidth, sinFromEnd);

		arcMask = gFragConst.gArcLen < 0.5
			? startEdge * endEdge
			: startEdge + endEdge - startEdge * endEdge;
	}

	vec4 color = mix(gFragConst.gUnfilledColor, gFragConst.gFilledColor, arcMask);
	FragClr = vec4(color.rgb, color.a * ringMask);
}
