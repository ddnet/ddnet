#ifndef BASE_LOG_COLOR_H
#define BASE_LOG_COLOR_H

#include "color.h"
#include "log.h"

#include <cstdint>

template<>
constexpr LOG_COLOR color_cast(const ColorRGBA &rgb)
{
	return LOG_COLOR{
		.r = (uint8_t)(rgb.r * 255.0f + 0.5f),
		.g = (uint8_t)(rgb.g * 255.0f + 0.5f),
		.b = (uint8_t)(rgb.b * 255.0f + 0.5f),
	};
}

#endif // BASE_LOG_COLOR_H
