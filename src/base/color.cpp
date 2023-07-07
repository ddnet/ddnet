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
