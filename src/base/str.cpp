#include <base/str.h>
#include <base/system.h>

#include <cstring>

int str_copy(char *dst, const char *src, int dst_size)
{
	dst[0] = '\0';
	strncat(dst, src, dst_size - 1);
	return str_utf8_fix_truncation(dst);
}

void str_append(char *dst, const char *src, int dst_size)
{
	int s = str_length(dst);
	int i = 0;
	while(s < dst_size)
	{
		dst[s] = src[i];
		if(!src[i]) /* check for null termination */
			break;
		s++;
		i++;
	}

	dst[dst_size - 1] = 0; /* assure null termination */
	str_utf8_fix_truncation(dst);
}

void str_truncate(char *dst, int dst_size, const char *src, int truncation_len)
{
	int size = dst_size;
	if(truncation_len < size)
	{
		size = truncation_len + 1;
	}
	str_copy(dst, src, size);
}

int str_length(const char *str)
{
	return (int)strlen(str);
}

char str_uppercase(char c)
{
	if(c >= 'a' && c <= 'z')
		return 'A' + (c - 'a');
	return c;
}

bool str_isnum(char c)
{
	return c >= '0' && c <= '9';
}

int str_isallnum(const char *str)
{
	while(*str)
	{
		if(!str_isnum(*str))
			return 0;
		str++;
	}
	return 1;
}

int str_isallnum_hex(const char *str)
{
	while(*str)
	{
		if(!str_isnum(*str) && !(*str >= 'a' && *str <= 'f') && !(*str >= 'A' && *str <= 'F'))
			return 0;
		str++;
	}
	return 1;
}

int str_isspace(char c)
{
	return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

const char *str_trim_words(const char *str, int words)
{
	while(*str && str_isspace(*str))
		str++;
	while(words && *str)
	{
		if(str_isspace(*str) && !str_isspace(*(str + 1)))
			words--;
		str++;
	}
	return str;
}

bool str_has_cc(const char *str)
{
	unsigned char *s = (unsigned char *)str;
	while(*s)
	{
		if(*s < 32)
		{
			return true;
		}
		s++;
	}
	return false;
}

/* makes sure that the string only contains the characters between 32 and 255 */
void str_sanitize_cc(char *str_in)
{
	unsigned char *str = (unsigned char *)str_in;
	while(*str)
	{
		if(*str < 32)
			*str = ' ';
		str++;
	}
}

/* makes sure that the string only contains the characters between 32 and 255 + \r\n\t */
void str_sanitize(char *str_in)
{
	unsigned char *str = (unsigned char *)str_in;
	while(*str)
	{
		if(*str < 32 && !(*str == '\r') && !(*str == '\n') && !(*str == '\t'))
			*str = ' ';
		str++;
	}
}

void str_sanitize_filename(char *str_in)
{
	unsigned char *str = (unsigned char *)str_in;
	while(*str)
	{
		if(*str <= 0x1F || *str == 0x7F || *str == '\\' || *str == '/' || *str == '|' || *str == ':' ||
			*str == '*' || *str == '?' || *str == '<' || *str == '>' || *str == '"')
		{
			*str = ' ';
		}
		str++;
	}
}

bool str_valid_filename(const char *str)
{
	// References:
	// - https://en.wikipedia.org/w/index.php?title=Filename&oldid=1281340521#Comparison_of_filename_limitations
	// - https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file (last update 2024-08-28)
	if(str[0] == '\0')
	{
		return false; // empty name not allowed
	}

	bool prev_space = false;
	bool prev_period = false;
	bool first_space_checked = false;
	const char *iterator = str;
	while(*iterator)
	{
		const int code = str_utf8_decode(&iterator);
		if(code <= 0x1F || code == 0x7F || code == '\\' || code == '/' || code == '|' || code == ':' ||
			code == '*' || code == '?' || code == '<' || code == '>' || code == '"')
		{
			return false; // disallowed characters, mostly for Windows
		}
		else if(str_utf8_isspace(code) && code != ' ')
		{
			return false; // we only allow regular space characters
		}
		if(code == ' ')
		{
			if(!first_space_checked)
			{
				return false; // leading spaces not allowed
			}
			if(prev_space)
			{
				return false; // multiple consecutive spaces not allowed
			}
			prev_space = true;
			prev_period = false;
		}
		else
		{
			prev_space = false;
			prev_period = code == '.';
			first_space_checked = true;
		}
	}
	if(prev_space || prev_period)
	{
		return false; // trailing spaces and periods not allowed
	}

	static constexpr const char *RESERVED_NAMES[] = {
		"CON", "PRN", "AUX", "NUL",
		"COM0", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", "COM¹", "COM²", "COM³",
		"LPT0", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9", "LPT¹", "LPT²", "LPT³"};
	for(const char *reserved_name : RESERVED_NAMES)
	{
		const char *prefix = str_startswith_nocase(str, reserved_name);
		if(prefix != nullptr && (prefix[0] == '\0' || prefix[0] == '.'))
		{
			return false; // reserved name not allowed when it makes up the entire filename or when followed by period
		}
	}

	return true;
}

int str_comp_filenames(const char *a, const char *b)
{
	int result;

	for(; *a && *b; ++a, ++b)
	{
		if(str_isnum(*a) && str_isnum(*b))
		{
			result = 0;
			do
			{
				if(!result)
					result = *a - *b;
				++a;
				++b;
			} while(str_isnum(*a) && str_isnum(*b));

			if(str_isnum(*a))
				return 1;
			else if(str_isnum(*b))
				return -1;
			else if(result || *a == '\0' || *b == '\0')
				return result;
		}

		result = tolower(*a) - tolower(*b);
		if(result)
			return result;
	}
	return *a - *b;
}

void str_clean_whitespaces(char *str)
{
	char *read = str;
	char *write = str;

	/* skip initial whitespace */
	while(*read == ' ')
		read++;

	/* end of read string is detected in the loop */
	while(true)
	{
		/* skip whitespace */
		int found_whitespace = 0;
		for(; *read == ' '; read++)
			found_whitespace = 1;
		/* if not at the end of the string, put a found whitespace here */
		if(*read)
		{
			if(found_whitespace)
				*write++ = ' ';
			*write++ = *read++;
		}
		else
		{
			*write = 0;
			break;
		}
	}
}

char *str_skip_to_whitespace(char *str)
{
	while(*str && !str_isspace(*str))
		str++;
	return str;
}

const char *str_skip_to_whitespace_const(const char *str)
{
	while(*str && !str_isspace(*str))
		str++;
	return str;
}

char *str_skip_whitespaces(char *str)
{
	while(*str && str_isspace(*str))
		str++;
	return str;
}

const char *str_skip_whitespaces_const(const char *str)
{
	while(*str && str_isspace(*str))
		str++;
	return str;
}

/* case */
int str_comp_nocase(const char *a, const char *b)
{
#if defined(CONF_FAMILY_WINDOWS)
	return _stricmp(a, b);
#else
	return strcasecmp(a, b);
#endif
}

int str_comp_nocase_num(const char *a, const char *b, int num)
{
#if defined(CONF_FAMILY_WINDOWS)
	return _strnicmp(a, b, num);
#else
	return strncasecmp(a, b, num);
#endif
}

int str_comp(const char *a, const char *b)
{
	return strcmp(a, b);
}

int str_comp_num(const char *a, const char *b, int num)
{
	return strncmp(a, b, num);
}

const char *str_startswith_nocase(const char *str, const char *prefix)
{
	int prefixl = str_length(prefix);
	if(str_comp_nocase_num(str, prefix, prefixl) == 0)
	{
		return str + prefixl;
	}
	else
	{
		return nullptr;
	}
}

const char *str_startswith(const char *str, const char *prefix)
{
	int prefixl = str_length(prefix);
	if(str_comp_num(str, prefix, prefixl) == 0)
	{
		return str + prefixl;
	}
	else
	{
		return nullptr;
	}
}

const char *str_endswith_nocase(const char *str, const char *suffix)
{
	int strl = str_length(str);
	int suffixl = str_length(suffix);
	const char *strsuffix;
	if(strl < suffixl)
	{
		return nullptr;
	}
	strsuffix = str + strl - suffixl;
	if(str_comp_nocase(strsuffix, suffix) == 0)
	{
		return strsuffix;
	}
	else
	{
		return nullptr;
	}
}

const char *str_endswith(const char *str, const char *suffix)
{
	int strl = str_length(str);
	int suffixl = str_length(suffix);
	const char *strsuffix;
	if(strl < suffixl)
	{
		return nullptr;
	}
	strsuffix = str + strl - suffixl;
	if(str_comp(strsuffix, suffix) == 0)
	{
		return strsuffix;
	}
	else
	{
		return nullptr;
	}
}

const char *str_find_nocase(const char *haystack, const char *needle)
{
	while(*haystack) /* native implementation */
	{
		const char *a = haystack;
		const char *b = needle;
		while(*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b))
		{
			a++;
			b++;
		}
		if(!(*b))
			return haystack;
		haystack++;
	}

	return nullptr;
}

const char *str_find(const char *haystack, const char *needle)
{
	while(*haystack) /* native implementation */
	{
		const char *a = haystack;
		const char *b = needle;
		while(*a && *b && *a == *b)
		{
			a++;
			b++;
		}
		if(!(*b))
			return haystack;
		haystack++;
	}

	return nullptr;
}

static const char *str_token_get(const char *str, const char *delim, int *length)
{
	size_t len = strspn(str, delim);
	if(len > 1)
		str++;
	else
		str += len;
	if(!*str)
		return nullptr;

	*length = strcspn(str, delim);
	return str;
}

const char *str_next_token(const char *str, const char *delim, char *buffer, int buffer_size)
{
	int len = 0;
	const char *tok = str_token_get(str, delim, &len);
	if(len < 0 || tok == nullptr)
	{
		buffer[0] = '\0';
		return nullptr;
	}

	len = buffer_size > len ? len : buffer_size - 1;
	mem_copy(buffer, tok, len);
	buffer[len] = '\0';

	return tok + len;
}

int str_in_list(const char *list, const char *delim, const char *needle)
{
	const char *tok = list;
	int len = 0, notfound = 1, needlelen = str_length(needle);

	while(notfound && (tok = str_token_get(tok, delim, &len)))
	{
		notfound = needlelen != len || str_comp_num(tok, needle, len);
		tok = tok + len;
	}

	return !notfound;
}

bool str_delimiters_around_offset(const char *haystack, const char *delim, int offset, int *start, int *end)
{
	bool found = true;
	const char *search = haystack;
	const int delim_len = str_length(delim);
	*start = 0;
	while(str_find(search, delim))
	{
		const char *test = str_find(search, delim) + delim_len;
		int distance = test - haystack;
		if(distance > offset)
			break;

		*start = distance;
		search = test;
	}
	if(search == haystack)
		found = false;

	if(str_find(search, delim))
	{
		*end = str_find(search, delim) - haystack;
	}
	else
	{
		*end = str_length(haystack);
		found = false;
	}

	return found;
}

const char *str_rchr(const char *haystack, char needle)
{
	return strrchr(haystack, needle);
}

int str_countchr(const char *haystack, char needle)
{
	int count = 0;
	while(*haystack)
	{
		if(*haystack == needle)
			count++;
		haystack++;
	}
	return count;
}

void str_hex(char *dst, int dst_size, const void *data, int data_size)
{
	static const char hex[] = "0123456789ABCDEF";
	int data_index;
	int dst_index;
	for(data_index = 0, dst_index = 0; data_index < data_size && dst_index < dst_size - 3; data_index++)
	{
		dst[data_index * 3] = hex[((const unsigned char *)data)[data_index] >> 4];
		dst[data_index * 3 + 1] = hex[((const unsigned char *)data)[data_index] & 0xf];
		dst[data_index * 3 + 2] = ' ';
		dst_index += 3;
	}
	dst[dst_index] = '\0';
}

void str_hex_cstyle(char *dst, int dst_size, const void *data, int data_size, int bytes_per_line)
{
	static const char hex[] = "0123456789ABCDEF";
	int data_index;
	int dst_index;
	int remaining_bytes_per_line = bytes_per_line;
	for(data_index = 0, dst_index = 0; data_index < data_size && dst_index < dst_size - 6; data_index++)
	{
		--remaining_bytes_per_line;
		dst[data_index * 6] = '0';
		dst[data_index * 6 + 1] = 'x';
		dst[data_index * 6 + 2] = hex[((const unsigned char *)data)[data_index] >> 4];
		dst[data_index * 6 + 3] = hex[((const unsigned char *)data)[data_index] & 0xf];
		dst[data_index * 6 + 4] = ',';
		if(remaining_bytes_per_line == 0)
		{
			dst[data_index * 6 + 5] = '\n';
			remaining_bytes_per_line = bytes_per_line;
		}
		else
		{
			dst[data_index * 6 + 5] = ' ';
		}
		dst_index += 6;
	}
	dst[dst_index] = '\0';
	// Remove trailing comma and space/newline
	if(dst_index >= 1)
		dst[dst_index - 1] = '\0';
	if(dst_index >= 2)
		dst[dst_index - 2] = '\0';
}

static int hexval(char x)
{
	switch(x)
	{
	case '0': return 0;
	case '1': return 1;
	case '2': return 2;
	case '3': return 3;
	case '4': return 4;
	case '5': return 5;
	case '6': return 6;
	case '7': return 7;
	case '8': return 8;
	case '9': return 9;
	case 'a':
	case 'A': return 10;
	case 'b':
	case 'B': return 11;
	case 'c':
	case 'C': return 12;
	case 'd':
	case 'D': return 13;
	case 'e':
	case 'E': return 14;
	case 'f':
	case 'F': return 15;
	default: return -1;
	}
}

static int byteval(const char *hex, unsigned char *dst)
{
	int v1 = hexval(hex[0]);
	int v2 = hexval(hex[1]);

	if(v1 < 0 || v2 < 0)
		return 1;

	*dst = v1 * 16 + v2;
	return 0;
}

int str_hex_decode(void *dst, int dst_size, const char *src)
{
	unsigned char *cdst = (unsigned char *)dst;
	int slen = str_length(src);
	int len = slen / 2;
	int i;
	if(slen != dst_size * 2)
		return 2;

	for(i = 0; i < len && dst_size; i++, dst_size--)
	{
		if(byteval(src + i * 2, cdst++))
			return 1;
	}
	return 0;
}

void str_base64(char *dst, int dst_size, const void *data_raw, int data_size)
{
	static const char DIGITS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	const unsigned char *data = (const unsigned char *)data_raw;
	unsigned value = 0;
	int num_bits = 0;
	int i = 0;
	int o = 0;

	dst_size -= 1;
	dst[dst_size] = 0;
	while(true)
	{
		if(num_bits < 6 && i < data_size)
		{
			value = (value << 8) | data[i];
			num_bits += 8;
			i += 1;
		}
		if(o == dst_size)
		{
			return;
		}
		if(num_bits > 0)
		{
			unsigned padded;
			if(num_bits >= 6)
			{
				padded = (value >> (num_bits - 6)) & 0x3f;
			}
			else
			{
				padded = (value << (6 - num_bits)) & 0x3f;
			}
			dst[o] = DIGITS[padded];
			num_bits -= 6;
			o += 1;
		}
		else if(o % 4 != 0)
		{
			dst[o] = '=';
			o += 1;
		}
		else
		{
			dst[o] = 0;
			return;
		}
	}
}

static int base64_digit_value(char digit)
{
	if('A' <= digit && digit <= 'Z')
	{
		return digit - 'A';
	}
	else if('a' <= digit && digit <= 'z')
	{
		return digit - 'a' + 26;
	}
	else if('0' <= digit && digit <= '9')
	{
		return digit - '0' + 52;
	}
	else if(digit == '+')
	{
		return 62;
	}
	else if(digit == '/')
	{
		return 63;
	}
	return -1;
}

int str_base64_decode(void *dst_raw, int dst_size, const char *data)
{
	unsigned char *dst = (unsigned char *)dst_raw;
	int data_len = str_length(data);

	int i;
	int o = 0;

	if(data_len % 4 != 0)
	{
		return -3;
	}
	if(data_len / 4 * 3 > dst_size)
	{
		// Output buffer too small.
		return -2;
	}
	for(i = 0; i < data_len; i += 4)
	{
		int num_output_bytes = 3;
		char copy[4];
		int d[4];
		int value;
		int b;
		mem_copy(copy, data + i, sizeof(copy));
		if(i == data_len - 4)
		{
			if(copy[3] == '=')
			{
				copy[3] = 'A';
				num_output_bytes = 2;
				if(copy[2] == '=')
				{
					copy[2] = 'A';
					num_output_bytes = 1;
				}
			}
		}
		d[0] = base64_digit_value(copy[0]);
		d[1] = base64_digit_value(copy[1]);
		d[2] = base64_digit_value(copy[2]);
		d[3] = base64_digit_value(copy[3]);
		if(d[0] == -1 || d[1] == -1 || d[2] == -1 || d[3] == -1)
		{
			// Invalid digit.
			return -1;
		}
		value = (d[0] << 18) | (d[1] << 12) | (d[2] << 6) | d[3];
		for(b = 0; b < 3; b++)
		{
			unsigned char byte_value = (value >> (16 - 8 * b)) & 0xff;
			if(b < num_output_bytes)
			{
				dst[o] = byte_value;
				o += 1;
			}
			else
			{
				if(byte_value != 0)
				{
					// Padding not zeroed.
					return -2;
				}
			}
		}
	}
	return o;
}

void str_escape(char **dst, const char *src, const char *end)
{
	while(*src && *dst + 1 < end)
	{
		if(*src == '"' || *src == '\\') // escape \ and "
		{
			if(*dst + 2 < end)
				*(*dst)++ = '\\';
			else
				break;
		}
		*(*dst)++ = *src++;
	}
	**dst = 0;
}

int str_toint(const char *str)
{
	return str_toint_base(str, 10);
}

bool str_toint(const char *str, int *out)
{
	// returns true if conversion was successful
	char *end;
	int value = strtol(str, &end, 10);
	if(*end != '\0')
		return false;
	if(out != nullptr)
		*out = value;
	return true;
}

int str_toint_base(const char *str, int base)
{
	return strtol(str, nullptr, base);
}

unsigned long str_toulong_base(const char *str, int base)
{
	return strtoul(str, nullptr, base);
}

int64_t str_toint64_base(const char *str, int base)
{
	return strtoll(str, nullptr, base);
}

float str_tofloat(const char *str)
{
	return strtod(str, nullptr);
}

bool str_tofloat(const char *str, float *out)
{
	// returns true if conversion was successful
	char *end;
	float value = strtod(str, &end);
	if(*end != '\0')
		return false;
	if(out != nullptr)
		*out = value;
	return true;
}

unsigned str_quickhash(const char *str)
{
	unsigned hash = 5381;
	for(; *str; str++)
		hash = ((hash << 5) + hash) + (*str); /* hash * 33 + c */
	return hash;
}

int str_utf8_encode(char *ptr, int chr)
{
	/* encode */
	if(chr <= 0x7F)
	{
		ptr[0] = (char)chr;
		return 1;
	}
	else if(chr <= 0x7FF)
	{
		ptr[0] = 0xC0 | ((chr >> 6) & 0x1F);
		ptr[1] = 0x80 | (chr & 0x3F);
		return 2;
	}
	else if(chr <= 0xFFFF)
	{
		ptr[0] = 0xE0 | ((chr >> 12) & 0x0F);
		ptr[1] = 0x80 | ((chr >> 6) & 0x3F);
		ptr[2] = 0x80 | (chr & 0x3F);
		return 3;
	}
	else if(chr <= 0x10FFFF)
	{
		ptr[0] = 0xF0 | ((chr >> 18) & 0x07);
		ptr[1] = 0x80 | ((chr >> 12) & 0x3F);
		ptr[2] = 0x80 | ((chr >> 6) & 0x3F);
		ptr[3] = 0x80 | (chr & 0x3F);
		return 4;
	}

	return 0;
}

static unsigned char str_byte_next(const char **ptr)
{
	unsigned char byte_value = **ptr;
	(*ptr)++;
	return byte_value;
}

static void str_byte_rewind(const char **ptr)
{
	(*ptr)--;
}

int str_utf8_decode(const char **ptr)
{
	// As per https://encoding.spec.whatwg.org/#utf-8-decoder.
	unsigned char utf8_lower_boundary = 0x80;
	unsigned char utf8_upper_boundary = 0xBF;
	int utf8_code_point = 0;
	int utf8_bytes_seen = 0;
	int utf8_bytes_needed = 0;
	while(true)
	{
		unsigned char byte_value = str_byte_next(ptr);
		if(utf8_bytes_needed == 0)
		{
			if(byte_value <= 0x7F)
			{
				return byte_value;
			}
			else if(0xC2 <= byte_value && byte_value <= 0xDF)
			{
				utf8_bytes_needed = 1;
				utf8_code_point = byte_value - 0xC0;
			}
			else if(0xE0 <= byte_value && byte_value <= 0xEF)
			{
				if(byte_value == 0xE0)
					utf8_lower_boundary = 0xA0;
				if(byte_value == 0xED)
					utf8_upper_boundary = 0x9F;
				utf8_bytes_needed = 2;
				utf8_code_point = byte_value - 0xE0;
			}
			else if(0xF0 <= byte_value && byte_value <= 0xF4)
			{
				if(byte_value == 0xF0)
					utf8_lower_boundary = 0x90;
				if(byte_value == 0xF4)
					utf8_upper_boundary = 0x8F;
				utf8_bytes_needed = 3;
				utf8_code_point = byte_value - 0xF0;
			}
			else
			{
				return -1; // Error.
			}
			utf8_code_point = utf8_code_point << (6 * utf8_bytes_needed);
			continue;
		}
		if(!(utf8_lower_boundary <= byte_value && byte_value <= utf8_upper_boundary))
		{
			// Resetting variables not necessary, will be done when
			// the function is called again.
			str_byte_rewind(ptr);
			return -1;
		}
		utf8_lower_boundary = 0x80;
		utf8_upper_boundary = 0xBF;
		utf8_bytes_seen += 1;
		utf8_code_point = utf8_code_point + ((byte_value - 0x80) << (6 * (utf8_bytes_needed - utf8_bytes_seen)));
		if(utf8_bytes_seen != utf8_bytes_needed)
		{
			continue;
		}
		// Resetting variables not necessary, see above.
		return utf8_code_point;
	}
}

void str_utf8_truncate(char *dst, int dst_size, const char *src, int truncation_len)
{
	int size = -1;
	const char *cursor = src;
	int pos = 0;
	while(pos <= truncation_len && cursor - src < dst_size && size != cursor - src)
	{
		size = cursor - src;
		if(str_utf8_decode(&cursor) == 0)
		{
			break;
		}
		pos++;
	}
	str_copy(dst, src, size + 1);
}

int str_utf8_fix_truncation(char *str)
{
	int len = str_length(str);
	if(len > 0)
	{
		int last_char_index = str_utf8_rewind(str, len);
		const char *last_char = str + last_char_index;
		// Fix truncated UTF-8.
		if(str_utf8_decode(&last_char) == -1)
		{
			str[last_char_index] = 0;
			return last_char_index;
		}
	}
	return len;
}

void str_utf8_trim_right(char *param)
{
	const char *str = param;
	char *end = nullptr;
	while(*str)
	{
		char *str_old = (char *)str;
		int code = str_utf8_decode(&str);

		// check if unicode is not empty
		if(!str_utf8_isspace(code))
		{
			end = nullptr;
		}
		else if(!end)
		{
			end = str_old;
		}
	}
	if(end)
	{
		*end = 0;
	}
}

void str_utf8_tolower(const char *input, char *output, size_t size)
{
	size_t out_pos = 0;
	while(*input)
	{
		const int code = str_utf8_tolower_codepoint(str_utf8_decode(&input));
		char encoded_code[4];
		const int code_size = str_utf8_encode(encoded_code, code);
		if(out_pos + code_size + 1 > size) // +1 for null termination
		{
			break;
		}
		mem_copy(&output[out_pos], encoded_code, code_size);
		out_pos += code_size;
	}
	output[out_pos] = '\0';
}

int str_utf8_isspace(int code)
{
	return code <= 0x0020 || code == 0x0085 || code == 0x00A0 || code == 0x034F ||
	       code == 0x115F || code == 0x1160 || code == 0x1680 || code == 0x180E ||
	       (code >= 0x2000 && code <= 0x200F) || (code >= 0x2028 && code <= 0x202F) ||
	       (code >= 0x205F && code <= 0x2064) || (code >= 0x206A && code <= 0x206F) ||
	       code == 0x2800 || code == 0x3000 || code == 0x3164 ||
	       (code >= 0xFE00 && code <= 0xFE0F) || code == 0xFEFF || code == 0xFFA0 ||
	       (code >= 0xFFF9 && code <= 0xFFFC);
}

int str_utf8_isstart(char c)
{
	if((c & 0xC0) == 0x80) /* 10xxxxxx */
		return 0;
	return 1;
}

int str_utf8_rewind(const char *str, int cursor)
{
	while(cursor)
	{
		cursor--;
		if(str_utf8_isstart(*(str + cursor)))
			break;
	}
	return cursor;
}

const char *str_utf8_find_nocase(const char *haystack, const char *needle, const char **end)
{
	while(*haystack) /* native implementation */
	{
		const char *a = haystack;
		const char *b = needle;
		const char *a_next = a;
		const char *b_next = b;
		while(*a && *b && str_utf8_tolower_codepoint(str_utf8_decode(&a_next)) == str_utf8_tolower_codepoint(str_utf8_decode(&b_next)))
		{
			a = a_next;
			b = b_next;
		}
		if(!(*b))
		{
			if(end != nullptr)
				*end = a_next;
			return haystack;
		}
		str_utf8_decode(&haystack);
	}

	if(end != nullptr)
		*end = nullptr;
	return nullptr;
}

int str_utf8_comp_nocase(const char *a, const char *b)
{
	int code_a;
	int code_b;

	while(*a && *b)
	{
		code_a = str_utf8_tolower_codepoint(str_utf8_decode(&a));
		code_b = str_utf8_tolower_codepoint(str_utf8_decode(&b));

		if(code_a != code_b)
			return code_a - code_b;
	}
	return (unsigned char)*a - (unsigned char)*b;
}

int str_utf8_comp_nocase_num(const char *a, const char *b, int num)
{
	int code_a;
	int code_b;
	const char *old_a = a;

	if(num <= 0)
		return 0;

	while(*a && *b)
	{
		code_a = str_utf8_tolower_codepoint(str_utf8_decode(&a));
		code_b = str_utf8_tolower_codepoint(str_utf8_decode(&b));

		if(code_a != code_b)
			return code_a - code_b;

		if(a - old_a >= num)
			return 0;
	}

	return (unsigned char)*a - (unsigned char)*b;
}

const char *str_utf8_skip_whitespaces(const char *str)
{
	const char *str_old;
	int code;

	while(*str)
	{
		str_old = str;
		code = str_utf8_decode(&str);

		// check if unicode is not empty
		if(!str_utf8_isspace(code))
		{
			return str_old;
		}
	}

	return str;
}

int str_utf8_forward(const char *str, int cursor)
{
	const char *ptr = str + cursor;
	if(str_utf8_decode(&ptr) == 0)
	{
		return cursor;
	}
	return ptr - str;
}

int str_utf8_check(const char *str)
{
	int codepoint;
	while((codepoint = str_utf8_decode(&str)))
	{
		if(codepoint == -1)
		{
			return 0;
		}
	}
	return 1;
}

void str_utf8_copy_num(char *dst, const char *src, int dst_size, int num)
{
	int new_cursor;
	int cursor = 0;

	while(src[cursor] && num > 0)
	{
		new_cursor = str_utf8_forward(src, cursor);
		if(new_cursor >= dst_size) // reserve 1 byte for the null termination
			break;
		else
			cursor = new_cursor;
		--num;
	}

	str_copy(dst, src, cursor < dst_size ? cursor + 1 : dst_size);
}

void str_utf8_stats(const char *str, size_t max_size, size_t max_count, size_t *size, size_t *count)
{
	const char *cursor = str;
	*size = 0;
	*count = 0;
	while(*size < max_size && *count < max_count)
	{
		if(str_utf8_decode(&cursor) == 0)
		{
			break;
		}
		if((size_t)(cursor - str) >= max_size)
		{
			break;
		}
		*size = cursor - str;
		++(*count);
	}
}

size_t str_utf8_offset_bytes_to_chars(const char *str, size_t byte_offset)
{
	size_t char_offset = 0;
	size_t current_offset = 0;
	while(current_offset < byte_offset)
	{
		const size_t prev_byte_offset = current_offset;
		current_offset = str_utf8_forward(str, current_offset);
		if(current_offset == prev_byte_offset)
			break;
		char_offset++;
	}
	return char_offset;
}

size_t str_utf8_offset_chars_to_bytes(const char *str, size_t char_offset)
{
	size_t byte_offset = 0;
	for(size_t i = 0; i < char_offset; i++)
	{
		const size_t prev_byte_offset = byte_offset;
		byte_offset = str_utf8_forward(str, byte_offset);
		if(byte_offset == prev_byte_offset)
			break;
	}
	return byte_offset;
}
