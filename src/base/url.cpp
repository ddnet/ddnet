#include "url.h"

#include "str.h"

#include <algorithm>

constexpr std::size_t DOMAIN_LABEL_MAX_LENGTH = 63; // DNS limit for each dot-separated label.
constexpr std::size_t TLD_MIN_LENGTH = 2;
constexpr std::size_t PORT_MAX_DIGITS = 5;
constexpr unsigned char FIRST_NON_ASCII_BYTE = 0x80; // Bytes with the high bit set are not ASCII.
constexpr char ASCII_DELETE = 0x7f;

CUrlMatch::CUrlMatch(size_t OffsetChar, size_t OffsetBytes, size_t Length, bool HasScheme, const char *pText, std::string_view DomainName) :
	m_OffsetChar(OffsetChar), m_OffsetBytes(OffsetBytes), m_Length(Length), m_HasScheme(HasScheme)
{
	str_truncate(m_aText, sizeof(m_aText), pText, std::min(Length, sizeof(m_aText) - 1));
	str_truncate(m_aDomainName, sizeof(m_aDomainName), DomainName.data(), std::min(DomainName.size(), sizeof(m_aDomainName) - 1));
}

static bool url_is_alpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Do not detect suffixes of email addresses, invalid hosts or unsupported schemes.
static bool url_blocks_start(char c)
{
	return url_is_alpha(c) || str_isnum(c) || c == '_' || c == '-' || c == '@' || c == '.' || c == '%' || c == '/' || c == '\\' || static_cast<unsigned char>(c) >= FIRST_NON_ASCII_BYTE;
}

static bool is_known_tld(std::string_view Tld)
{
	for(std::string_view AllowedTld : {"com", "org", "net"})
		if(Tld.size() == AllowedTld.size() && str_comp_nocase_num(Tld.data(), AllowedTld.data(), AllowedTld.size()) == 0)
			return true;
	return false;
}

static bool url_is_valid_domain_name(std::string_view Name, std::string_view *pDomainName)
{
	const std::size_t DotPos = Name.rfind('.');
	if(DotPos == std::string_view::npos)
		return false; // No "<word>.<tld>" shape

	const std::string_view Word = Name.substr(0, DotPos);
	const std::string_view Tld = Name.substr(DotPos + 1);

	if(Tld.size() < TLD_MIN_LENGTH || Tld.size() > DOMAIN_LABEL_MAX_LENGTH)
		return false;
	for(char c : Tld)
		if(!url_is_alpha(c))
			return false;

	if(Word.empty())
		return false;

	std::size_t LabelLength = 0; // Length of the label being read
	char prev = '.';
	std::size_t Offset = 0;
	for(char c : Word)
	{
		if(c == '.')
		{
			if(LabelLength == 0 || LabelLength > DOMAIN_LABEL_MAX_LENGTH || prev == '-')
				return false; // Empty label, or label ends with '-'
			Offset += LabelLength + 1;
			LabelLength = 0;
		}
		else if(url_is_alpha(c) || str_isnum(c))
		{
			++LabelLength;
		}
		else if(c == '-')
		{
			if(LabelLength == 0)
				return false; // Label starts with '-'
			++LabelLength;
		}
		else
		{
			return false; // Space, '_', '/', ':', non-ASCII, NUL, ...
		}
		prev = c;
	}

	// Last label non-empty, not ending in '-'
	if(LabelLength == 0 || LabelLength > DOMAIN_LABEL_MAX_LENGTH || prev == '-')
		return false;

	if(pDomainName)
		*pDomainName = std::string_view(Name.data() + Offset, LabelLength + Tld.length() + 1);

	return true;
}

std::vector<CUrlMatch> url_find_all(std::string_view Text)
{
	std::vector<CUrlMatch> vResult;
	std::size_t Pos = 0;
	std::size_t OffsetBytes = 0;
	std::size_t OffsetChars = 0;
	while(Pos < Text.size() && vResult.size() < URL_MAX_RESULTS)
	{
		// Start only at a token boundary, never in the middle of another host
		const std::size_t Start = Pos++;
		if(!(url_is_alpha(Text[Start]) || str_isnum(Text[Start])) || (Start > 0 && url_blocks_start(Text[Start - 1])))
			continue;

		std::size_t HostStart = Start;
		for(const char *pScheme : {"https://", "http://"})
		{
			const std::size_t Length = str_length(pScheme);
			if(Text.size() - Start >= Length && str_comp_nocase_num(Text.data() + Start, pScheme, Length) == 0)
			{
				HostStart += Length;
				break;
			}
		}

		const bool HasScheme = HostStart != Start;

		std::size_t End = HostStart;
		while(End < Text.size() && (url_is_alpha(Text[End]) || str_isnum(Text[End]) || Text[End] == '-' || Text[End] == '.'))
			++End;

		Pos = std::max(Pos, End);

		// A final dot belongs to the surrounding sentence, not the host
		std::size_t HostEnd = End;
		while(HostEnd > HostStart && Text[HostEnd - 1] == '.')
			--HostEnd;

		const std::string_view Host = Text.substr(HostStart, HostEnd - HostStart);
		std::string_view DomainName;
		if(!url_is_valid_domain_name(Host, &DomainName))
			continue;

		// Reject a valid-looking prefix of an invalid host or user-info section
		if(End < Text.size() && (Text[End] == '_' || Text[End] == '@' || Text[End] == '%' || static_cast<unsigned char>(Text[End]) >= FIRST_NON_ASCII_BYTE))
			continue;

		constexpr std::string_view WwwPrefix = "www.";
		const bool HasWww = Host.size() >= WwwPrefix.size() && str_comp_nocase_num(Host.data(), WwwPrefix.data(), WwwPrefix.size()) == 0;
		// Only bare domains need a common TLD to distinguish them from filenames
		if(!HasScheme && !HasWww && !is_known_tld(Host.substr(Host.rfind('.') + 1)))
			continue;

		End = HostEnd;
		if(End < Text.size() && Text[End] == ':')
		{
			const std::size_t PortStart = End + 1; // Skip the colon
			std::size_t PortEnd = PortStart;
			while(PortEnd < Text.size() && str_isnum(Text[PortEnd]))
				++PortEnd;
			const std::size_t PortLength = PortEnd - PortStart;
			const bool HasInvalidPortSuffix = PortLength > 0 && PortEnd < Text.size() &&
							  (url_is_alpha(Text[PortEnd]) || Text[PortEnd] == '_' || Text[PortEnd] == '-' || Text[PortEnd] == '@' || Text[PortEnd] == '%' || static_cast<unsigned char>(Text[PortEnd]) >= FIRST_NON_ASCII_BYTE);
			// Reject the whole candidate instead of truncating a long or malformed port
			if(PortLength > PORT_MAX_DIGITS || HasInvalidPortSuffix)
			{
				Pos = PortEnd;
				continue;
			}
			if(PortLength > 0)
				End = PortEnd;
		}
		if(End < Text.size() && (Text[End] == '/' || Text[End] == '?' || Text[End] == '#'))
		{
			const std::size_t PathStart = End++;
			while(End < Text.size() && (url_is_alpha(Text[End]) || str_isnum(Text[End]) || std::string_view("_.~%:=-?&#+@[]/").find(Text[End]) != std::string_view::npos))
				++End;
			Pos = End;
			// Trim sentence punctuation, keeping the initial '/', '?' or '#'
			while(End > PathStart + 1 && std::string_view(".?:&@[").find(Text[End - 1]) != std::string_view::npos)
				--End;
		}

		Pos = std::max(Pos, End);

		const std::size_t Length = End - Start;
		if(Length >= URL_MAX_LENGTH)
			continue;

		// Count each part of the prefix once, even when a line has many links
		OffsetChars += str_utf8_offset_bytes_to_chars(Text.data() + OffsetBytes, Start - OffsetBytes);
		OffsetBytes = Start;

		vResult.emplace_back(OffsetChars, Start, Length, HasScheme, Text.data() + Start, DomainName);
	}

	return vResult;
}

std::vector<CUrlMatch> url_find_all(const char *pText)
{
	return pText ? url_find_all(pText, str_length(pText)) : std::vector<CUrlMatch>{};
}

std::vector<CUrlMatch> url_find_all(const char *pText, std::size_t Length)
{
	if(!pText)
		return {};
	return url_find_all(std::string_view(pText, Length));
}

bool url_is_valid_domain_name(std::string_view Name)
{
	return url_is_valid_domain_name(Name, nullptr);
}

bool url_is_valid_domain_name(const char *pName, std::size_t Length)
{
	return pName && url_is_valid_domain_name(std::string_view(pName, Length));
}

static inline char ascii_lower(char c) noexcept { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c; }

bool url_check_domain_name(const char *pUrl, const char *pDomainName)
{
	constexpr std::size_t NPOS = std::string_view::npos;
	if(!pUrl || !pDomainName)
		return false;
	std::string_view Url(pUrl), Domain(pDomainName);

	if(Domain.size() > 1 && Domain.back() == '.')
		Domain.remove_suffix(1);
	if(!url_is_valid_domain_name(Domain))
		return false;

	// Spaces and ASCII control characters are never valid inside a URL
	for(char c : Url)
		if(static_cast<unsigned char>(c) <= ' ' || c == ASCII_DELETE)
			return false;

	// Optional scheme "<alpha>[alnum+-.]*://", restricted to web schemes
	constexpr std::string_view SchemeSeparator = "://";
	bool HasScheme = false;
	if(const std::size_t Sep = Url.find(SchemeSeparator); Sep != NPOS && Sep > 0)
	{
		const std::string_view Scheme = Url.substr(0, Sep);
		bool WellFormed = (ascii_lower(Scheme[0]) >= 'a' && ascii_lower(Scheme[0]) <= 'z');
		for(char c : Scheme)
		{
			const char l = ascii_lower(c);
			WellFormed = WellFormed && ((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9') || l == '+' || l == '-' || l == '.');
		}
		if(WellFormed)
		{ // Otherwise "://" belongs to the path/query: no scheme
			bool Web = false;
			for(const char *pAllowed : {"http", "https"})
				Web = Web || (Scheme.size() == (size_t)str_length(pAllowed) && str_comp_nocase_num(Scheme.data(), pAllowed, Scheme.size()) == 0);
			if(!Web)
				return false;
			Url.remove_prefix(Sep + SchemeSeparator.size());
			HasScheme = true;
		}
	}

	// Authority = everything up to the first '/', '?', '#' or '\'
	std::string_view Host = Url.substr(0, Url.find_first_of("/?#\\"));
	if(HasScheme)
	{
		if(const std::size_t at = Host.rfind('@'); at != NPOS)
			Host.remove_prefix(at + 1); // user-info
	}
	if(const std::size_t Colon = Host.rfind(':'); Colon != NPOS)
	{ // Port: digits only
		const std::string_view Port = Host.substr(Colon + 1);
		if(Port.size() > PORT_MAX_DIGITS)
			return false;
		for(char c : Port)
			if(c < '0' || c > '9')
				return false;
		Host = Host.substr(0, Colon);
	}
	if(!Host.empty() && Host.back() == '.')
		Host.remove_suffix(1);
	if(!url_is_valid_domain_name(Host))
		return false;

	// Host must end with `domain`, and the match must start on a label boundary
	if(Host.size() < Domain.size())
		return false;
	const std::size_t Offset = Host.size() - Domain.size();
	for(std::size_t i = 0; i < Domain.size(); ++i)
		if(ascii_lower(Host[Offset + i]) != ascii_lower(Domain[i]))
			return false;
	return Offset == 0 || Host[Offset - 1] == '.';
}
