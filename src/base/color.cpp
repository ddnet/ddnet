#include "color.h"
#include "system.h"

template<typename T>
std::optional<T> color_parse(const char *pStr)
{
	if(!str_isallnum_hex(pStr))
		return {};

	const unsigned Num = str_toulong_base(pStr, 16);

	T Color;
	switch(str_length(pStr))
	{
	case 3:
		Color.x = (((Num >> 8) & 0x0F) + ((Num >> 4) & 0xF0)) / 255.0f;
		Color.y = (((Num >> 4) & 0x0F) + ((Num >> 0) & 0xF0)) / 255.0f;
		Color.z = (((Num >> 0) & 0x0F) + ((Num << 4) & 0xF0)) / 255.0f;
		Color.a = 1.0f;
		return Color;

	case 4:
		Color.x = (((Num >> 12) & 0x0F) + ((Num >> 8) & 0xF0)) / 255.0f;
		Color.y = (((Num >> 8) & 0x0F) + ((Num >> 4) & 0xF0)) / 255.0f;
		Color.z = (((Num >> 4) & 0x0F) + ((Num >> 0) & 0xF0)) / 255.0f;
		Color.a = (((Num >> 0) & 0x0F) + ((Num << 4) & 0xF0)) / 255.0f;
		return Color;

	case 6:
		Color.x = ((Num >> 16) & 0xFF) / 255.0f;
		Color.y = ((Num >> 8) & 0xFF) / 255.0f;
		Color.z = ((Num >> 0) & 0xFF) / 255.0f;
		Color.a = 1.0f;
		return Color;

	case 8:
		Color.x = ((Num >> 24) & 0xFF) / 255.0f;
		Color.y = ((Num >> 16) & 0xFF) / 255.0f;
		Color.z = ((Num >> 8) & 0xFF) / 255.0f;
		Color.a = ((Num >> 0) & 0xFF) / 255.0f;
		return Color;

	default:
		return {};
	}
}

template std::optional<ColorRGBA> color_parse<ColorRGBA>(const char *);

template<typename T>
std::optional<T> color_parse_console(const char *pStr, float DarkestLighting)
{
	if(str_isallnum(pStr) || ((pStr[0] == '-' || pStr[0] == '+') && str_isallnum(pStr + 1))) // Teeworlds Color (Packed HSL)
	{
		unsigned long Value = str_toulong_base(pStr, 10);
		if(Value == std::numeric_limits<unsigned long>::max())
			return std::nullopt;
		return ColorHSLA(Value, true).UnclampLighting(DarkestLighting);
	}
	else if(*pStr == '$') // Hex RGB/RGBA
	{
		auto ParsedColor = color_parse<ColorRGBA>(pStr + 1);
		if(ParsedColor)
			return color_cast<ColorHSLA>(ParsedColor.value());
		else
			return std::nullopt;
	}
	else if(!str_comp_nocase(pStr, "red"))
		return ColorHSLA(0.0f / 6.0f, 1.0f, 0.5f);
	else if(!str_comp_nocase(pStr, "yellow"))
		return ColorHSLA(1.0f / 6.0f, 1.0f, 0.5f);
	else if(!str_comp_nocase(pStr, "green"))
		return ColorHSLA(2.0f / 6.0f, 1.0f, 0.5f);
	else if(!str_comp_nocase(pStr, "cyan"))
		return ColorHSLA(3.0f / 6.0f, 1.0f, 0.5f);
	else if(!str_comp_nocase(pStr, "blue"))
		return ColorHSLA(4.0f / 6.0f, 1.0f, 0.5f);
	else if(!str_comp_nocase(pStr, "magenta"))
		return ColorHSLA(5.0f / 6.0f, 1.0f, 0.5f);
	else if(!str_comp_nocase(pStr, "white"))
		return ColorHSLA(0.0f, 0.0f, 1.0f);
	else if(!str_comp_nocase(pStr, "gray"))
		return ColorHSLA(0.0f, 0.0f, 0.5f);
	else if(!str_comp_nocase(pStr, "black"))
		return ColorHSLA(0.0f, 0.0f, 0.0f);

	return std::nullopt;
}

template std::optional<ColorHSLA> color_parse_console<ColorHSLA>(const char *, float);
