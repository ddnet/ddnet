#ifndef GAME_CLIENT_NUMBER_INPUT_H
#define GAME_CLIENT_NUMBER_INPUT_H

#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string_view>

/** Parses a complete integer draft, without accepting a valid prefix of bad input. */
inline std::optional<std::int64_t> ParseIntegerDraft(std::string_view Text, int Base)
{
	const auto Space = [](char Character) { return Character == ' ' || Character == '\t' || Character == '\n' || Character == '\r'; };
	while(!Text.empty() && Space(Text.front()))
		Text.remove_prefix(1);
	while(!Text.empty() && Space(Text.back()))
		Text.remove_suffix(1);
	if(Text.empty() || (Base != 10 && Base != 16))
		return std::nullopt;
	const bool Negative = Text.front() == '-';
	if(Negative || Text.front() == '+')
		Text.remove_prefix(1);
	if(Base == 16 && Text.size() >= 2 && Text.front() == '0' && (Text[1] == 'x' || Text[1] == 'X'))
		Text.remove_prefix(2);
	if(Text.empty())
		return std::nullopt;
	std::uint64_t Magnitude = 0;
	const auto Result = std::from_chars(Text.data(), Text.data() + Text.size(), Magnitude, Base);
	const auto Limit = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + (Negative ? 1 : 0);
	if(Result.ec != std::errc{} || Result.ptr != Text.data() + Text.size() || Magnitude > Limit)
		return std::nullopt;
	if(Negative && Magnitude == Limit)
		return std::numeric_limits<std::int64_t>::min();
	return Negative ? -static_cast<std::int64_t>(Magnitude) : static_cast<std::int64_t>(Magnitude);
}

/** Parses a finite floating-point draft only when every non-space character is valid. */
inline std::optional<float> ParseFloatDraft(std::string_view Text)
{
	const auto Space = [](char Character) { return Character == ' ' || Character == '\t' || Character == '\n' || Character == '\r'; };
	while(!Text.empty() && Space(Text.front()))
		Text.remove_prefix(1);
	while(!Text.empty() && Space(Text.back()))
		Text.remove_suffix(1);
	if(!Text.empty() && Text.front() == '+')
	{
		Text.remove_prefix(1);
		if(!Text.empty() && (Text.front() == '-' || Text.front() == '+'))
			return std::nullopt;
	}
	if(Text.empty() || Text.find_first_not_of("0123456789.eE+-") != std::string_view::npos)
		return std::nullopt;
	long double Parsed = 0.0L;
	// Floating from_chars is missing on supported older libc++ deployments.
	// A classic-locale stream preserves decimal syntax regardless of UI locale.
	std::istringstream Input{std::string(Text)};
	Input.imbue(std::locale::classic());
	// Read wider so library ERANGE handling does not reject float subnormals.
	Input >> std::noskipws >> Parsed;
	if(Input.fail() || Input.peek() != std::char_traits<char>::eof() || !std::isfinite(Parsed))
		return std::nullopt;
	constexpr float Maximum = std::numeric_limits<float>::max();
	const long double Magnitude = std::fabs(Parsed);
	if(Magnitude > Maximum)
	{
		// Values less than half an ulp above max still round to a finite float.
		const long double HalfUlp = static_cast<long double>(Maximum - std::nextafter(Maximum, 0.0f)) / 2;
		if(Magnitude >= static_cast<long double>(Maximum) + HalfUlp)
			return std::nullopt;
		return std::signbit(Parsed) ? -Maximum : Maximum;
	}
	const float Value = static_cast<float>(Parsed);
	// num_get may silently round an underflowing nonzero decimal to zero.
	if(Value == 0.0f)
		for(const char Character : Text.substr(0, Text.find_first_of("eE")))
			if(Character >= '1' && Character <= '9')
				return std::nullopt;
	return Value;
}

#endif
