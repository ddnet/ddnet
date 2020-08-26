#version 330

uniform sampler2D textSampler;
uniform sampler2D textOutlineSampler;

uniform vec4 vertColor;
uniform vec4 vertOutlineColor;

noperspective in vec2 texCoord;
noperspective in vec4 outVertColor;

out vec4 FragClr;
void main()
{
	vec4 textColor = vertColor * outVertColor * texture(textSampler, texCoord);
	vec4 textOutlineTex = vertOutlineColor * texture(textOutlineSampler, texCoord);
	
	// ratio between the two textures
	float OutlineBlend = (1.0 - textColor.a);	
	
	// since the outline is always black, or even if it has decent colors, it can be just added to the actual color
	// without loosing any or too much color
	
	// lerp isn't commutative, so add the color the fragment looses by lerping
	// this reduces the chance of false color calculation if the text is transparent
	
	// first get the right color
	vec4 textOutlineFrag = vec4(textOutlineTex.rgb * textOutlineTex.a, textOutlineTex.a) * OutlineBlend;
	vec3 textFrag = (textColor.rgb * textColor.a);
	vec3 finalFragColor = textOutlineFrag.rgb + textFrag;
	
	float RealAlpha = (textOutlineFrag.a + textColor.a);
	
	// discard transparent fragments
	if(RealAlpha == 0.0)
		discard;
	
	// simply add the color we will loose through blending
	FragClr = vec4(finalFragColor / RealAlpha, RealAlpha);
}
