#ifndef BASE_URL_H
#define BASE_URL_H

/**
 * Url utilities.
 *
 * @defgroup Url Url
 */

#include <cctype>
#include <cstddef>
#include <cstring>
#include <optional>
#include <regex>
#include <string_view>
#include <vector>

// Max url length
constexpr std::size_t URL_MAX_LENGTH = 2048;
// Will there be any text containing more than 32 urls?
constexpr std::size_t URL_MAX_RESULTS = 32;
// Max amount of allowed domains
constexpr std::size_t URL_ALLOWLIST_MAX_ENTRIES = 128;

class CUrlMatch
{
public:
	std::size_t m_Offset; // index of the first character in the input
	std::size_t m_Length; // number of characters
	bool m_HasScheme; // has protocol scheme
	char m_aText[URL_MAX_LENGTH]; // Raw text
	char m_aDomainName[URL_MAX_LENGTH]; // domain name

	CUrlMatch() = default;
	CUrlMatch(size_t Offset, size_t Length, bool HasScheme, const char *pText, std::string_view DomainName);
};

// Domain name representation storing the data & its length
class CUrlDomainName
{
public:
	char m_aName[URL_MAX_LENGTH];
	size_t m_Length;

	CUrlDomainName() :
		m_aName(), m_Length(0) {}
};

/**
 * Finds all url in a given text.
 *
 * @ingroup Url
 *
 * @param Text The text to parse
 *
 * @return A vector of @link CUrlMatch @endlink containing metadata for all found matches
 */
std::vector<CUrlMatch> url_find_all(std::string_view Text);

/**
 * Finds all url in a given text.
 *
 * @ingroup Url
 *
 * @param pText The text to parse
 *
 * @return A vector of @link CUrlMatch @endlink containing metadata for all found matches
 */
std::vector<CUrlMatch> url_find_all(const char *pText);

/**
 * Finds all url in a given text.
 *
 * @ingroup Url
 *
 * @param pText The text to parse
 * @param Length The effective length of the text
 *
 * @return A vector of @link CUrlMatch @endlink containing metadata for all found matches
 */
std::vector<CUrlMatch> url_find_all(const char *pText, std::size_t Length);

/**
 * Checks for a basic domain name.
 * Should answer the question: is `name` exactly `<word>.<tld>`?
 *   `<tld>`  : the part after the LAST dot; 2 or more ASCII letters (no digits, no hyphens)
 *   `<word>` : everything before it; one or more dot-separated labels made of letters, digits
 *            and '-', none empty, none starting or ending with '-' ("example", "sub.example")
 * Case-insensitive. Anything else (spaces, '_', ':', '/', '@', non-ASCII, trailing dot, ...)
 * makes it invalid. The TLD is NOT looked up in kTlds, so "hf.cfg" is well-formed.
 *
 * @ingroup Url
 *
 * @param Name The name to check
 *
 * @return True if `Name` answers the above question
 */
bool url_is_valid_domain_name(std::string_view Name);

/**
 * Checks for a basic domain name.
 * Should answer the question: is `name` exactly `<word>.<tld>`?
 *   `<tld>`  : the part after the LAST dot; 2 or more ASCII letters (no digits, no hyphens)
 *   `<word>` : everything before it; one or more dot-separated labels made of letters, digits
 *            and '-', none empty, none starting or ending with '-' ("example", "sub.example")
 * Case-insensitive. Anything else (spaces, '_', ':', '/', '@', non-ASCII, trailing dot, ...)
 * makes it invalid. The TLD is NOT looked up in kTlds, so "hf.cfg" is well-formed.
 *
 * @ingroup Url
 *
 * @param pName The name to check
 * @param Length The effective length of `pName`
 *
 * @return True if `pName` answers the above question
 */
bool url_is_valid_domain_name(const char *pName, std::size_t Length);

/**
 * Checks if the host of the full URL `pUrl` is `pDomainName` or one of its subdomains.
 *   url_check_domain_name("https://a.b.c.site.com", "site.com")                          -> true
 *   url_check_domain_name("https://www.youtube.com/watch?v=abc&t=12#a=b", "youtube.com") -> true
 *   url_check_domain_name("https://evilsite.com", "site.com")                            -> false (label boundary)
 *   url_check_domain_name("https://site.com@evil.com", "site.com")                       -> false (host is evil.com)
 *
 * Details:
 *   - case-insensitive; one trailing dot ("site.com.") is ignored on both arguments
 *   - pDomainName must itself be a valid "<word>.<tld>" name (see url_is_valid_domainname),
 *     so "com" or "" can never match everything
 *   - the scheme is optional; if present it must be http, https, ftp, ftps, ws or wss
 *     (so "javascript://site.com/..." is rejected); user-info ("user:pw@") is only skipped
 *     when a scheme is present, so e-mail addresses ("x@site.com") do not match
 *   - the host ends at the first '/', '?', '#' or '\' (browsers treat '\' like '/'),
 *     an optional ":port" is allowed, and the host itself must be a valid domain name
 *     (no '%', '_', spaces, non-ASCII: pass IDNs in punycode "xn--" form)
 *   - whitespace or control characters anywhere in pUrl -> false; null pointers -> false
 *
 * @ingroup Url
 *
 * @param pUrl The full url to check
 * @param pDomainName The domain name to compare against
 *
 * @return True if the host of the full URL `pUrl` is `pDomainName` or one of its subdomains.
 */
bool url_check_domain_name(const char *pUrl, const char *pDomainName);

#endif
