#include "str.h"

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
