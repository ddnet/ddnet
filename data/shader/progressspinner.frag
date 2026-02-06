uniform float gInnerRadius;
uniform float gArcStart;
uniform float gArcLen;
uniform vec4 gFilledColor;
uniform vec4 gUnfilledColor;

noperspective in vec2 texCoord;

out vec4 FragClr;

#define PI 3.14159265358979323846

void main()
{
	float dist = length(texCoord);

	// Ring SDF with smooth edges
	float ringAA = fwidth(dist);
	float outerMask = 1.0 - smoothstep(1.0 - ringAA, 1.0, dist);
	float innerMask = smoothstep(gInnerRadius, gInnerRadius + ringAA, dist);
	float ringMask = outerMask * innerMask;

	if(ringMask <= 0.0)
		discard;

	// Arc containment
	float arcMask;
	if(gArcLen >= 1.0)
	{
		arcMask = 1.0;
	}
	else if(gArcLen <= 0.0)
	{
		arcMask = 0.0;
	}
	else
	{
		vec2 dir = texCoord / dist;

		float endAngle = gArcStart + gArcLen * 2.0 * PI;
		vec2 startDir = vec2(sin(gArcStart), -cos(gArcStart));
		vec2 endDir = vec2(sin(endAngle), -cos(endAngle));

		float sinFromStart = dir.x * startDir.y - dir.y * startDir.x;
		float sinFromEnd = endDir.x * dir.y - endDir.y * dir.x;

		float aaWidth = max(length(dFdx(texCoord)), length(dFdy(texCoord))) / dist;

		float startEdge = smoothstep(-aaWidth, aaWidth, sinFromStart);
		float endEdge = smoothstep(-aaWidth, aaWidth, sinFromEnd);

		arcMask = gArcLen < 0.5
			? startEdge * endEdge
			: startEdge + endEdge - startEdge * endEdge;
	}

	vec4 color = mix(gUnfilledColor, gFilledColor, arcMask);
	FragClr = vec4(color.rgb, color.a * ringMask);
}
