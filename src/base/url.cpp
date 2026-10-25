#include "url.h"

#include "str.h"

#include <unordered_set>

// ---------------------------------------------------------------------------
// Allowed TLDs for URLs written WITHOUT a scheme. Lower-case, 2..63 letters, unique.
//
// Deliberately NOT listed, because they are also very common file extensions
// (they are still accepted with a scheme or a "www." prefix):
//   py md sh rs cc so pl pm ps ml mk sv tf zip mov mobi
// ---------------------------------------------------------------------------
constexpr std::string_view URL_ALLOWED_TLDS[] = {
	// --- country codes ---
	"ac",
	"ad",
	"ae",
	"af",
	"ag",
	"ai",
	"al",
	"am",
	"ao",
	"aq",
	"ar",
	"as",
	"at",
	"au",
	"aw",
	"ax",
	"az",
	"ba",
	"bb",
	"bd",
	"be",
	"bf",
	"bg",
	"bh",
	"bi",
	"bj",
	"bm",
	"bn",
	"bo",
	"br",
	"bs",
	"bt",
	"bw",
	"by",
	"bz",
	"ca",
	"cd",
	"cf",
	"cg",
	"ch",
	"ci",
	"ck",
	"cl",
	"cm",
	"cn",
	"co",
	"cr",
	"cu",
	"cv",
	"cw",
	"cx",
	"cy",
	"cz",
	"de",
	"dj",
	"dk",
	"dm",
	"do",
	"dz",
	"ec",
	"ee",
	"eg",
	"er",
	"es",
	"et",
	"eu",
	"fi",
	"fj",
	"fk",
	"fm",
	"fo",
	"fr",
	"ga",
	"gd",
	"ge",
	"gf",
	"gg",
	"gh",
	"gi",
	"gl",
	"gm",
	"gn",
	"gp",
	"gq",
	"gr",
	"gs",
	"gt",
	"gu",
	"gw",
	"gy",
	"hk",
	"hm",
	"hn",
	"hr",
	"ht",
	"hu",
	"id",
	"ie",
	"il",
	"im",
	"in",
	"io",
	"iq",
	"ir",
	"is",
	"it",
	"je",
	"jm",
	"jo",
	"jp",
	"ke",
	"kg",
	"kh",
	"ki",
	"km",
	"kn",
	"kp",
	"kr",
	"kw",
	"ky",
	"kz",
	"la",
	"lb",
	"lc",
	"li",
	"lk",
	"lr",
	"ls",
	"lt",
	"lu",
	"lv",
	"ly",
	"ma",
	"mc",
	"me",
	"mg",
	"mh",
	"mm",
	"mn",
	"mo",
	"mp",
	"mq",
	"mr",
	"ms",
	"mt",
	"mu",
	"mv",
	"mw",
	"mx",
	"my",
	"mz",
	"na",
	"nc",
	"ne",
	"nf",
	"ng",
	"ni",
	"nl",
	"no",
	"np",
	"nr",
	"nu",
	"nz",
	"om",
	"pa",
	"pe",
	"pf",
	"pg",
	"ph",
	"pk",
	"pn",
	"pr",
	"pt",
	"pw",
	"qa",
	"re",
	"ro",
	"ru",
	"rw",
	"sa",
	"sb",
	"sc",
	"sd",
	"se",
	"sg",
	"si",
	"sk",
	"sl",
	"sm",
	"sn",
	"sr",
	"ss",
	"st",
	"su",
	"sx",
	"sy",
	"sz",
	"tc",
	"td",
	"tg",
	"th",
	"tj",
	"tk",
	"tl",
	"tm",
	"tn",
	"to",
	"tr",
	"tt",
	"tv",
	"tw",
	"tz",
	"ua",
	"ug",
	"uk",
	"us",
	"uy",
	"uz",
	"va",
	"vc",
	"ve",
	"vg",
	"vi",
	"vn",
	"vu",
	"wf",
	"ws",
	"ye",
	"yt",
	"za",
	"zm",
	"zw",
	// --- classic / sponsored ---
	"com",
	"org",
	"net",
	"edu",
	"gov",
	"mil",
	"int",
	"info",
	"biz",
	"name",
	"pro",
	"asia",
	"tel",
	"travel",
	"jobs",
	"museum",
	"coop",
	"aero",
	"cat",
	"post",
	"xxx",
	// --- popular generic TLDs ---
	"academy",
	"accountant",
	"accountants",
	"actor",
	"agency",
	"airforce",
	"apartments",
	"app",
	"art",
	"attorney",
	"auction",
	"audio",
	"auto",
	"bar",
	"bargains",
	"bike",
	"bingo",
	"bio",
	"black",
	"blog",
	"blue",
	"boutique",
	"build",
	"builders",
	"business",
	"buzz",
	"cafe",
	"camera",
	"camp",
	"capital",
	"car",
	"cards",
	"care",
	"careers",
	"cash",
	"casino",
	"catering",
	"center",
	"ceo",
	"chat",
	"cheap",
	"church",
	"city",
	"claims",
	"cleaning",
	"click",
	"clinic",
	"clothing",
	"cloud",
	"club",
	"coach",
	"codes",
	"coffee",
	"college",
	"community",
	"company",
	"computer",
	"condos",
	"construction",
	"consulting",
	"contractors",
	"cooking",
	"cool",
	"coupons",
	"courses",
	"credit",
	"creditcard",
	"cricket",
	"cruises",
	"dance",
	"date",
	"dating",
	"deals",
	"degree",
	"delivery",
	"democrat",
	"dental",
	"dentist",
	"design",
	"dev",
	"diamonds",
	"diet",
	"digital",
	"direct",
	"directory",
	"discount",
	"doctor",
	"dog",
	"domains",
	"download",
	"earth",
	"eco",
	"education",
	"email",
	"energy",
	"engineer",
	"engineering",
	"enterprises",
	"equipment",
	"estate",
	"events",
	"exchange",
	"expert",
	"exposed",
	"express",
	"fail",
	"faith",
	"family",
	"fan",
	"fans",
	"farm",
	"fashion",
	"finance",
	"financial",
	"fish",
	"fishing",
	"fit",
	"fitness",
	"flights",
	"florist",
	"flowers",
	"fly",
	"food",
	"football",
	"forsale",
	"foundation",
	"fun",
	"fund",
	"furniture",
	"futbol",
	"fyi",
	"gallery",
	"game",
	"games",
	"garden",
	"gift",
	"gifts",
	"gives",
	"glass",
	"global",
	"gmbh",
	"gold",
	"golf",
	"graphics",
	"gratis",
	"green",
	"gripe",
	"group",
	"guide",
	"guitars",
	"guru",
	"haus",
	"health",
	"healthcare",
	"help",
	"hockey",
	"holdings",
	"holiday",
	"homes",
	"horse",
	"hospital",
	"host",
	"hosting",
	"house",
	"how",
	"immo",
	"immobilien",
	"inc",
	"industries",
	"ink",
	"institute",
	"insure",
	"insurance",
	"international",
	"investments",
	"irish",
	"jewelry",
	"juegos",
	"kaufen",
	"kim",
	"kitchen",
	"kiwi",
	"land",
	"lawyer",
	"lease",
	"legal",
	"lgbt",
	"life",
	"lighting",
	"limited",
	"limo",
	"link",
	"live",
	"llc",
	"loan",
	"loans",
	"lol",
	"london",
	"love",
	"ltd",
	"luxury",
	"maison",
	"management",
	"market",
	"marketing",
	"markets",
	"mba",
	"media",
	"memorial",
	"men",
	"menu",
	"moda",
	"mom",
	"money",
	"mortgage",
	"movie",
	"network",
	"news",
	"ninja",
	"nyc",
	"observer",
	"one",
	"onl",
	"online",
	"ooo",
	"organic",
	"page",
	"paris",
	"partners",
	"parts",
	"party",
	"pet",
	"photo",
	"photography",
	"photos",
	"physio",
	"pics",
	"pictures",
	"pink",
	"pizza",
	"place",
	"plumbing",
	"plus",
	"poker",
	"press",
	"productions",
	"properties",
	"property",
	"pub",
	"qpon",
	"quebec",
	"racing",
	"recipes",
	"red",
	"rehab",
	"reise",
	"reisen",
	"rent",
	"rentals",
	"repair",
	"report",
	"republican",
	"rest",
	"restaurant",
	"review",
	"reviews",
	"rich",
	"rip",
	"rocks",
	"rodeo",
	"run",
	"sale",
	"salon",
	"sarl",
	"school",
	"schule",
	"science",
	"security",
	"services",
	"sex",
	"sexy",
	"shiksha",
	"shoes",
	"shop",
	"shopping",
	"show",
	"singles",
	"site",
	"ski",
	"soccer",
	"social",
	"software",
	"solar",
	"solutions",
	"soy",
	"space",
	"sport",
	"store",
	"stream",
	"studio",
	"study",
	"style",
	"sucks",
	"supplies",
	"supply",
	"support",
	"surf",
	"surgery",
	"systems",
	"tax",
	"taxi",
	"team",
	"tech",
	"technology",
	"tennis",
	"theater",
	"tienda",
	"tips",
	"tires",
	"today",
	"tokyo",
	"tools",
	"top",
	"tours",
	"town",
	"toys",
	"trade",
	"trading",
	"training",
	"tube",
	"university",
	"uno",
	"vacations",
	"vegas",
	"ventures",
	"vet",
	"viajes",
	"video",
	"villas",
	"vin",
	"vip",
	"vision",
	"vodka",
	"vote",
	"voting",
	"voyage",
	"wang",
	"watch",
	"webcam",
	"web",
	"website",
	"wed",
	"wedding",
	"wiki",
	"win",
	"wine",
	"work",
	"works",
	"world",
	"wtf",
	"xin",
	"xyz",
	"yoga",
	"zone",
	// --- regional / city ---
	"africa",
	"alsace",
	"amsterdam",
	"bayern",
	"berlin",
	"brussels",
	"bzh",
	"cologne",
	"cymru",
	"eus",
	"gal",
	"hamburg",
	"istanbul",
	"koeln",
	"miami",
	"moscow",
	"scot",
	"swiss",
	"tirol",
	"vlaanderen",
	"wales",
	"wien",
};

// Characters that mean a match starting right after them is really the middle of
// another token (e.g. "user@example.com", "my_site.com", "foo-bar.com").
inline static bool url_blocks_start(char c) noexcept
{
	const unsigned char u = static_cast<unsigned char>(c);
	return std::isalnum(u) || c == '_' || c == '-' || c == '@' || c == '.' || c == '%';
}

// Checks if the TLDs is one of URL_ALLOWED_TLDS
inline bool is_known_tld(const char *pTld, size_t TldLength)
{
	static const std::unordered_set<std::string_view> set(std::begin(URL_ALLOWED_TLDS), std::end(URL_ALLOWED_TLDS));
	char aBuf[64]; // TLDs are at most 63 characters
	if(TldLength < 2 || TldLength >= sizeof(aBuf))
		return false;
	str_utf8_tolower(pTld, aBuf, TldLength + 1);
	return set.find(std::string_view(aBuf, TldLength)) != set.end();
}

// Check if a regex candidate really looks like a URL
inline bool url_is_plausible(std::string_view m, bool *pHasScheme)
{
	if(!pHasScheme)
	{
		bool HasScheme = false;
		pHasScheme = &HasScheme;
	}

	*pHasScheme = false;
	const char *pUrl = m.data();
	for(const char *pScheme : {"https://", "http://"})
	{
		const char *pUrlNoScheme = str_startswith_nocase(pUrl, pScheme);
		if(pUrlNoScheme)
		{
			pUrl = pUrlNoScheme;
			*pHasScheme = true;
			break;
		}
	}

	size_t HostLength = m.length();
	for(const char *pNeedle : {":", "/", "?", "#"})
	{
		const char *pHostEnd = str_utf8_find_nocase(pUrl, pNeedle);
		if(pHostEnd)
		{
			HostLength = pHostEnd - pUrl;
			break;
		}
	}

	if(HostLength == 0)
		return false;

	if(pUrl[HostLength - 1] >= '0' && pUrl[HostLength - 1] <= '9')
		return true; // IPv4 (validated by the regex)
	if(*pHasScheme)
		return true; // explicit scheme: trust it
	if(str_startswith_nocase(pUrl, "www."))
		return true; // "www." prefix: trust it

	char aBuf[URL_MAX_LENGTH];
	str_utf8_truncate(aBuf, sizeof(aBuf), pUrl, HostLength);
	const char *pDot = str_utf8_find_nocase(aBuf, ".");
	if(!pDot)
		return false;

	size_t TldLength = HostLength - (pDot - aBuf + 1);
	return is_known_tld(pDot + 1, TldLength);
}

// URL detection regex: finds anything *shaped* like a URL. Whether it really is one
// (known TLD, ...) is decided afterwards by `url_is_plausible()`.
//
//   [scheme://]  host  [:port]  [/path | ?query | #fragment]
//
// The pattern is compiled once (thread-safe since C++11); matching with a const
// std::regex is thread-safe. Case-insensitive, so [a-z] also covers A-Z.
const std::regex &url_regex()
{
	static const std::regex re(
		// 1) Optional scheme: http:// or https://
		//    (?:...) is a non-capturing group; `s?` makes the "s" of https optional.
		R"re((?:(?:https?):\/\/)?)re"

		// 2) Host: either an IPv4 address or a domain name (the `|` between the two parts below).
		//    2a) IPv4: three "octet." groups followed by a final octet.
		//        An octet is 0-255:  25[0-5]       -> 250-255
		//                            2[0-4][0-9]   -> 200-249
		//                            1?[0-9]{1,2}  -> 0-199 (optional leading 1, then 1-2 digits)
		R"re((?:(?:(?:25[0-5]|2[0-4][0-9]|1?[0-9]{1,2})\.){3}(?:25[0-5]|2[0-4][0-9]|1?[0-9]{1,2})|)re"
		//    2b) Domain name: one or more labels followed by a dot, then the TLD.
		//        A label is 1-63 chars of [a-z0-9-] that neither starts nor ends with '-':
		//          [a-z0-9] (?: [a-z0-9-]{0,61} [a-z0-9] )?
		//        The TLD is 2-63 letters. (Which TLDs are accepted without a scheme is
		//        decided later by is_plausible_url(), not here.)
		R"re((?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)+[a-z]{2,63}))re"

		// 3) Host boundary: negative lookahead. The host must not be followed by more name
		//    characters, so "1.2.3.456" and "file.txt1" give no partial match ("1.2.3.45", "file.tx").
		R"re((?![a-z0-9_-]))re"

		// 4) Optional port: ':' followed by 1-5 digits. A bare "example.com:" does not
		//    include the colon.
		R"re((?::[0-9]{1,5})?)re"

		// 5) Optional path / query / fragment. It must begin with '/', '?' or '#', and
		//    consists of two parts so that it cannot END in trailing punctuation:
		//      [\/?#]                          first char
		//      [a-z0-9_.~%:=\-?&#+@\[\]\/]*    any run of URL characters (includes . ? : etc.)
		//      [a-z0-9_~%=\-#+\/\]]            final char: no '.', '?', ':', '&', '@', ...
		//    so "example.com/page." or "(example.com/page)." yield "example.com/page".
		//    ')' ',' ';' '<' '>' quotes and whitespace are not URL characters here.
		R"re((?:[\/?#](?:[a-z0-9_.~%:=\-?&#+@\[\]\/]*[a-z0-9_~%=\-#+\/\]])?)?)re",
		std::regex::ECMAScript | std::regex::icase | std::regex::optimize);
	return re;
}

std::vector<CUrlMatch> url_find_all(std::string_view Text)
{
	std::vector<CUrlMatch> vResult;
	const std::regex &Regex = url_regex();
	const std::size_t n = Text.size();
	constexpr std::size_t MaxTokenLength = 2 * URL_MAX_LENGTH;

	std::size_t i = 0;
	while(i < n && vResult.size() < URL_MAX_RESULTS)
	{
		while(i < n && str_isspace(Text[i]))
			++i; // skip separators
		const std::size_t TokBegin = i;
		while(i < n && !str_isspace(Text[i]))
			++i; // consume one token
		const std::size_t TokEnd = i;
		if(TokEnd == TokBegin)
			break;
		if(TokEnd - TokBegin > MaxTokenLength)
			continue;

		const char *const pFirst = Text.data() + TokBegin;
		const char *const pLast = Text.data() + TokEnd;
		for(std::cregex_iterator It(pFirst, pLast, Regex), end; It != end && vResult.size() < URL_MAX_RESULTS; ++It)
		{
			const std::cmatch &m = *It;
			if(m.length() == 0)
				continue;
			const std::size_t Start = TokBegin + static_cast<std::size_t>(m.position());
			// Reject matches that start mid-token (e-mail address, "my_site.com", ...).
			if(Start > 0 && url_blocks_start(Text[Start - 1]))
				continue;
			bool HasScheme;
			if(!url_is_plausible(std::string_view(Text.data() + Start, static_cast<std::size_t>(m.length())), &HasScheme))
				continue;
			vResult.push_back(CUrlMatch{Start, static_cast<std::size_t>(m.length()), HasScheme});
		}
	}
	return vResult;
}

std::vector<CUrlMatch> url_find_all(const char *pText)
{
	return url_find_all(pText, str_length(pText));
}

std::vector<CUrlMatch> url_find_all(const char *pText, std::size_t Length)
{
	if(!pText)
		return {};
	return url_find_all(std::string_view(pText, Length));
}

bool url_is_valid_domain_name(std::string_view Name)
{
	auto is_alpha = [](char c) noexcept { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
	auto is_digit = [](char c) noexcept { return c >= '0' && c <= '9'; };

	const std::size_t DotPos = Name.rfind('.');
	if(DotPos == std::string_view::npos)
		return false; // no "<word>.<tld>" shape
	const std::string_view Word = Name.substr(0, DotPos);
	const std::string_view Tld = Name.substr(DotPos + 1);

	if(Tld.size() < 2)
		return false;
	for(char c : Tld)
		if(!is_alpha(c))
			return false;

	if(Word.empty())
		return false;

	std::size_t LabelLength = 0; // length of the label being read
	char prev = '.';
	for(char c : Word)
	{
		if(c == '.')
		{
			if(LabelLength == 0 || prev == '-')
				return false; // empty label, or label ends with '-'
			LabelLength = 0;
		}
		else if(is_alpha(c) || is_digit(c))
		{
			++LabelLength;
		}
		else if(c == '-')
		{
			if(LabelLength == 0)
				return false; // label starts with '-'
			++LabelLength;
		}
		else
		{
			return false; // space, '_', '/', ':', non-ASCII, NUL, ...
		}
		prev = c;
	}
	return LabelLength > 0 && prev != '-'; // last label non-empty, not ending in '-'
}

bool url_is_valid_domain_name(const char *pName, std::size_t Length)
{
	return pName && url_is_valid_domain_name(std::string_view(pName, Length));
}

// helper function
static inline char ascii_lower(char c) noexcept { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }

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

	for(char c : Url)
		if(static_cast<unsigned char>(c) <= 0x20 || c == 0x7f)
			return false;

	// Optional scheme "<alpha>[alnum+-.]*://", restricted to web schemes.
	bool HasScheme = false;
	if(const std::size_t Sep = Url.find("://"); Sep != NPOS && Sep > 0)
	{
		const std::string_view Scheme = Url.substr(0, Sep);
		bool WellFormed = (ascii_lower(Scheme[0]) >= 'a' && ascii_lower(Scheme[0]) <= 'z');
		for(char c : Scheme)
		{
			const char l = ascii_lower(c);
			WellFormed = WellFormed && ((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9') || l == '+' || l == '-' || l == '.');
		}
		if(WellFormed)
		{ // otherwise "://" belongs to the path/query: no scheme
			bool Web = false;
			for(std::string_view Allowed : {"http", "https"})
				Web = Web || (Scheme.size() == Allowed.size() && str_startswith_nocase(Scheme.data(), Allowed.data()));
			if(!Web)
				return false;
			Url.remove_prefix(Sep + 3);
			HasScheme = true;
		}
	}

	// Authority = everything up to the first '/', '?', '#' or '\'.
	std::string_view Host = Url.substr(0, Url.find_first_of("/?#\\"));
	if(HasScheme)
	{
		if(const std::size_t at = Host.rfind('@'); at != NPOS)
			Host.remove_prefix(at + 1); // user-info
	}
	if(const std::size_t Colon = Host.rfind(':'); Colon != NPOS)
	{ // port: digits only
		const std::string_view Port = Host.substr(Colon + 1);
		if(Port.size() > 5)
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

	// host must end with `domain`, and the match must start on a label boundary.
	if(Host.size() < Domain.size())
		return false;
	const std::size_t Offset = Host.size() - Domain.size();
	for(std::size_t i = 0; i < Domain.size(); ++i)
		if(ascii_lower(Host[Offset + i]) != ascii_lower(Domain[i]))
			return false;
	return Offset == 0 || Host[Offset - 1] == '.';
}
