#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D gTextSampler;
layout(binding = 1) uniform sampler2D gTextOutlineSampler;

layout(push_constant) uniform SFragConstBO {
	layout(offset = 48) uniform vec4 gVertColor;
	layout(offset = 64) uniform vec4 gVertOutlineColor;
} gFragConst;

layout (location = 0) noperspective in vec2 texCoord;
layout (location = 1) noperspective in vec4 outVertColor;

layout(location = 0) out vec4 FragClr;

void main()
{
	vec4 textColor = gFragConst.gVertColor * outVertColor * vec4(1.0, 1.0, 1.0, texture(gTextSampler, texCoord).r);
	vec4 textOutlineTex = gFragConst.gVertOutlineColor * vec4(1.0, 1.0, 1.0, texture(gTextOutlineSampler, texCoord).r);

	// ratio between the two textures
	float OutlineBlend = (1.0 - textColor.a);

	// since the outline is always black, or even if it has decent colors, it can be just added to the actual color
	// without losing any or too much color
	
	// lerp isn't commutative, so add the color the fragment looses by lerping
	// this reduces the chance of false color calculation if the text is transparent
	
	// first get the right color
	vec4 textOutlineFrag = vec4(textOutlineTex.rgb * textOutlineTex.a, textOutlineTex.a) * OutlineBlend;
	vec3 textFrag = (textColor.rgb * textColor.a);
	vec3 finalFragColor = textOutlineFrag.rgb + textFrag;
	
	float RealAlpha = (textOutlineFrag.a + textColor.a);
	
	// simply add the color we will loose through blending
	if(RealAlpha > 0.0)
		FragClr = vec4(finalFragColor / RealAlpha, RealAlpha);
	else
		FragClr = vec4(0.0, 0.0, 0.0, 0.0);
}
