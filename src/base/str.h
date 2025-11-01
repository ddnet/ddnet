#ifndef BASE_STR_H
#define BASE_STR_H

#include <cstddef>
#include <cstdint>

/**
 * Copies a string to another.
 *
 * @ingroup Strings
 *
 * @param dst Pointer to a buffer that shall receive the string.
 * @param src String to be copied.
 * @param dst_size Size of the buffer dst.
 *
 * @return Length of written string, even if it has been truncated
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Guarantees that dst string will contain null-termination.
 */
int str_copy(char *dst, const char *src, int dst_size);

/**
 * Copies a string to a fixed-size array of chars.
 *
 * @ingroup Strings
 *
 * @param dst Array that shall receive the string.
 * @param src String to be copied.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Guarantees that dst string will contain null-termination.
 */
template<int N>
void str_copy(char (&dst)[N], const char *src)
{
	str_copy(dst, src, N);
}

/**
 * Appends a string to another.
 *
 * @ingroup Strings
 *
 * @param dst Pointer to a buffer that contains a string.
 * @param src String to append.
 * @param dst_size Size of the buffer of the dst string.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Guarantees that dst string will contain null-termination.
 */
void str_append(char *dst, const char *src, int dst_size);

/**
 * Appends a string to a fixed-size array of chars.
 *
 * @ingroup Strings
 *
 * @param dst Array that shall receive the string.
 * @param src String to append.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Guarantees that dst string will contain null-termination.
 */
template<int N>
void str_append(char (&dst)[N], const char *src)
{
	str_append(dst, src, N);
}

/**
 * Truncates a string to a given length.
 *
 * @ingroup Strings
 *
 * @param dst Pointer to a buffer that shall receive the string.
 * @param dst_size Size of the buffer dst.
 * @param src String to be truncated.
 * @param truncation_len Maximum length of the returned string (not
 *                       counting the null-termination).
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Guarantees that dst string will contain null-termination.
 */
void str_truncate(char *dst, int dst_size, const char *src, int truncation_len);

/**
 * Returns the length of a null-terminated string.
 *
 * @ingroup Strings
 *
 * @param str Pointer to the string.
 *
 * @return Length of string in bytes excluding the null-termination.
 */
int str_length(const char *str);

char str_uppercase(char c);

bool str_isnum(char c);

int str_isallnum(const char *str);

int str_isallnum_hex(const char *str);

/**
 * Determines whether a character is whitespace.
 *
 * @ingroup Strings
 *
 * @param c the character to check.
 *
 * @return `1` if the character is whitespace, `0` otherwise.
 *
 * @remark The following characters are considered whitespace: ` `, `\n`, `\r`, `\t`.
 */
int str_isspace(char c);

/**
 * Trims specific number of words at the start of a string.
 *
 * @ingroup Strings
 *
 * @param str String to trim the words from.
 * @param words Count of words to trim.
 *
 * @return Trimmed string
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Leading whitespace is always trimmed.
 */
const char *str_trim_words(const char *str, int words);

/**
 * Check whether string has ASCII control characters.
 *
 * @ingroup Strings
 *
 * @param str String to check.
 *
 * @return Whether the string has ASCII control characters.
 *
 * @remark The strings are treated as null-terminated strings.
 */
bool str_has_cc(const char *str);

/**
 * Replaces all characters below 32 with whitespace.
 *
 * @ingroup Strings
 *
 * @param str String to sanitize.
 *
 * @remark The strings are treated as null-terminated strings.
 */
void str_sanitize_cc(char *str);

/**
 * Replaces all characters below 32 with whitespace with
 * exception to `\t`, `\n` and `\n`.
 *
 * @ingroup Strings
 *
 * @param str String to sanitize.
 *
 * @remark The strings are treated as null-terminated strings.
 */
void str_sanitize(char *str);

/**
 * Replaces all invalid filename characters with whitespace.
 *
 * @param str String to sanitize.
 * @remark The strings are treated as null-terminated strings.
 */
void str_sanitize_filename(char *str);

/**
 * Checks if a string is a valid filename on all supported platforms.
 *
 * @param str Filename to check.
 *
 * @return `true` if the string is a valid filename, `false` otherwise.
 *
 * @remark The strings are treated as null-terminated strings.
 */
bool str_valid_filename(const char *str);

/**
 * Compares two strings case insensitive, digit chars will be compared as numbers.
 *
 * @ingroup Strings
 *
 * @param a String to compare.
 * @param b String to compare.
 *
 * @return `< 0` - String a is less than string b
 * @return `0` - String a is equal to string b
 * @return `> 0` - String a is greater than string b
 *
 * @remark The strings are treated as null-terminated strings.
 */
int str_comp_filenames(const char *a, const char *b);

/**
 * Removes leading and trailing spaces and limits the use of multiple spaces.
 *
 * @ingroup Strings
 *
 * @param str String to clean up.
 *
 * @remark The strings are treated as null-terminated strings.
 */
void str_clean_whitespaces(char *str);

/**
 * Skips leading non-whitespace characters.
 *
 * @ingroup Strings
 *
 * @param str Pointer to the string.
 *
 * @return Pointer to the first whitespace character found
 *		   within the string.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Whitespace is defined according to str_isspace.
 */
char *str_skip_to_whitespace(char *str);

/**
 * @ingroup Strings
 *
 * @see str_skip_to_whitespace
 */
const char *str_skip_to_whitespace_const(const char *str);

/**
 * Skips leading whitespace characters.
 *
 * @ingroup Strings
 *
 * @param str Pointer to the string.
 *
 * @return Pointer to the first non-whitespace character found
 *         within the string.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Whitespace is defined according to str_isspace.
 */
char *str_skip_whitespaces(char *str);

/**
 * @ingroup Strings
 *
 * @see str_skip_whitespaces
 */
const char *str_skip_whitespaces_const(const char *str);

/**
 * Compares to strings case insensitively.
 *
 * @ingroup Strings
 *
 * @param a String to compare.
 * @param b String to compare.
 *
 * @return `< 0` if string a is less than string b.
 * @return `0` if string a is equal to string b.
 * @return `> 0` if string a is greater than string b.
 *
 * @remark Only guaranteed to work with a-z/A-Z.
 * @remark The strings are treated as null-terminated strings.
 */
int str_comp_nocase(const char *a, const char *b);

/**
 * Compares up to `num` characters of two strings case insensitively.
 *
 * @ingroup Strings
 *
 * @param a String to compare.
 * @param b String to compare.
 * @param num Maximum characters to compare.
 *
 * @return `< 0` if string a is less than string b.
 * @return `0` if string a is equal to string b.
 * @return `> 0` if string a is greater than string b.
 *
 * @remark Only guaranteed to work with a-z/A-Z.
 * @remark Use `str_utf8_comp_nocase_num` for unicode support.
 * @remark The strings are treated as null-terminated strings.
 */
int str_comp_nocase_num(const char *a, const char *b, int num);

/**
 * Compares two strings case sensitive.
 *
 * @ingroup Strings
 *
 * @param a String to compare.
 * @param b String to compare.
 *
 * @return `< 0` if string a is less than string b.
 * @return `0` if string a is equal to string b.
 * @return `> 0` if string a is greater than string b.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int str_comp(const char *a, const char *b);

/**
 * Compares up to `num` characters of two strings case sensitive.
 *
 * @ingroup Strings
 *
 * @param a String to compare.
 * @param b String to compare.
 * @param num Maximum characters to compare.
 *
 * @return `< 0` if string a is less than string b.
 * @return `0` if string a is equal to string b.
 * @return `> 0` if string a is greater than string b.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int str_comp_num(const char *a, const char *b, int num);

/**
 * Checks case insensitive whether the string begins with a certain prefix.
 *
 * @ingroup Strings
 *
 * @param str String to check.
 * @param prefix Prefix to look for.
 *
 * @return A pointer to the string `str` after the string prefix, or `nullptr` if
 *         the string prefix isn't a prefix of the string `str`.
 *
 * @remark The strings are treated as null-terminated strings.
 */
const char *str_startswith_nocase(const char *str, const char *prefix);

/**
 * Checks case sensitive whether the string begins with a certain prefix.
 *
 * @ingroup Strings
 *
 * @param str String to check.
 * @param prefix Prefix to look for.
 *
 * @return A pointer to the string `str` after the string prefix, or `nullptr` if
 *         the string prefix isn't a prefix of the string `str`.
 *
 * @remark The strings are treated as null-terminated strings.
 */
const char *str_startswith(const char *str, const char *prefix);

/**
 * Checks case insensitive whether the string ends with a certain suffix.
 *
 * @ingroup Strings
 *
 * @param str String to check.
 * @param suffix Suffix to look.
 *
 * @return A pointer to the beginning of the suffix in the string `str`.
 * @return `nullptr` if the string suffix isn't a suffix of the string `str`.
 *
 * @remark The strings are treated as null-terminated strings.
 */
const char *str_endswith_nocase(const char *str, const char *suffix);

/**
 * Checks case sensitive whether the string ends with a certain suffix.
 *
 * @param str String to check.
 * @param suffix Suffix to look for.
 *
 * @return A pointer to the beginning of the suffix in the string `str`.
 * @return `nullptr` if the string suffix isn't a suffix of the string `str`.
 *
 * @remark The strings are treated as null-terminated strings.
 */
const char *str_endswith(const char *str, const char *suffix);

/**
 * Finds a string inside another string case insensitively.
 *
 * @ingroup Strings
 *
 * @param haystack String to search in.
 * @param needle String to search for.
 *
 * @return A pointer into `haystack` where the needle was found.
 * @return Returns `nullptr` if `needle` could not be found.
 *
 * @remark Only guaranteed to work with a-z/A-Z.
 * @remark Use str_utf8_find_nocase for unicode support.
 * @remark The strings are treated as null-terminated strings.
 */
const char *str_find_nocase(const char *haystack, const char *needle);

/**
 * Finds a string inside another string case sensitive.
 *
 * @ingroup Strings
 *
 * @param haystack String to search in.
 * @param needle String to search for.
 *
 * @return A pointer into `haystack` where the needle was found.
 * @return Returns `nullptr` if `needle` could not be found.
 *
 * @remark The strings are treated as null-terminated strings.
 */
const char *str_find(const char *haystack, const char *needle);

/**
 * Writes the next token after str into buf, returns the rest of the string.
 *
 * @ingroup Strings
 *
 * @param str Pointer to string.
 * @param delim Delimiter for tokenization.
 * @param buffer Buffer to store token in.
 * @param buffer_size Size of the buffer.
 *
 * @return Pointer to rest of the string.
 *
 * @remark The token is always null-terminated.
 */
const char *str_next_token(const char *str, const char *delim, char *buffer, int buffer_size);

/**
 * Checks if needle is in list delimited by delim.
 *
 * @param list List.
 * @param delim List delimiter.
 * @param needle Item that is being looked for.
 *
 * @return `1` - Item is in list.
 * @return `0` - Item isn't in list.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int str_in_list(const char *list, const char *delim, const char *needle);

/**
 * @ingroup Strings
 *
 * @param haystack String to search in.
 * @param delim String to search for.
 * @param offset Number of characters into `haystack`.
 * @param start Will be set to the first delimiter on the left side of the offset (or `haystack` start).
 * @param end Will be set to the first delimiter on the right side of the offset (or `haystack` end).
 *
 * @return `true` if both delimiters were found.
 * @return `false` if a delimiter is missing (it uses `haystack` start and end as fallback).
 *
 * @remark The strings are treated as null-terminated strings.
 */
bool str_delimiters_around_offset(const char *haystack, const char *delim, int offset, int *start, int *end);

/**
 * Finds the last occurrence of a character
 *
 * @ingroup Strings
 *
 * @param haystack String to search in.
 * @param needle Character to search for.

 * @return A pointer into haystack where the needle was found.
 * @return Returns `nullptr` if needle could not be found.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark The zero-terminator character can also be found with this function.
 */
const char *str_rchr(const char *haystack, char needle);

/**
 * Counts the number of occurrences of a character in a string.
 *
 * @ingroup Strings
 *
 * @param haystack String to count in.
 * @param needle Character to count.

 * @return The number of characters in the haystack string matching
 *         the needle character.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark The number of zero-terminator characters cannot be counted.
 */
int str_countchr(const char *haystack, char needle);

/**
 * Takes a datablock and generates a hex string of it, with spaces between bytes.
 *
 * @ingroup Strings
 *
 * @param dst Buffer to fill with hex data.
 * @param dst_size Size of the buffer (at least 3 * data_size + 1 to contain all data).
 * @param data Data to turn into hex.
 * @param data_size Size of the data.
 *
 * @remark The destination buffer will be null-terminated.
 */
void str_hex(char *dst, int dst_size, const void *data, int data_size);

/**
 * Takes a datablock and generates a hex string of it, in the C style array format,
 * i.e. with bytes formatted in 0x00-0xFF notation and commas with spaces between the bytes.
 * The output can be split over multiple lines by specifying the maximum number of bytes
 * that should be printed per line.
 *
 * @ingroup Strings
 *
 * @param dst Buffer to fill with hex data.
 * @param dst_size Size of the buffer (at least `6 * data_size + 1` to contain all data).
 * @param data Data to turn into hex.
 * @param data_size Size of the data.
 * @param bytes_per_line After this many printed bytes a newline will be printed.
 *
 * @remark The destination buffer will be null-terminated.
 */
void str_hex_cstyle(char *dst, int dst_size, const void *data, int data_size, int bytes_per_line = 12);

/**
 * Takes a hex string *without spaces between bytes* and returns a byte array.
 *
 * @ingroup Strings
 *
 * @param dst Buffer for the byte array.
 * @param dst_size size of the buffer.
 * @param src String to decode.
 *
 * @return `2` if string doesn't exactly fit the buffer.
 * @return `1` if invalid character in string.
 * @return `0` if success.
 *
 * @remark The contents of the buffer is only valid on success.
 */
int str_hex_decode(void *dst, int dst_size, const char *src);

/**
 * Takes a datablock and generates the base64 encoding of it.
 *
 * @ingroup Strings
 *
 * @param dst Buffer to fill with base64 data.
 * @param dst_size Size of the buffer.
 * @param data Data to turn into base64.
 * @param data_size Size of the data.
 *
 * @remark The destination buffer will be null-terminated
 */
void str_base64(char *dst, int dst_size, const void *data, int data_size);

/**
 * Takes a base64 string without any whitespace and correct
 * padding and returns a byte array.
 *
 * @ingroup Strings
 *
 * @param dst Buffer for the byte array.
 * @param dst_size Size of the buffer.
 * @param data String to decode.
 *
 * @return `< 0` - Error.
 * @return `<= 0` - Success, length of the resulting byte buffer.
 *
 * @remark The contents of the buffer is only valid on success.
 */
int str_base64_decode(void *dst, int dst_size, const char *data);

/**
 * Escapes \ and " characters in a string.
 *
 * @param dst Destination array pointer, gets increased, will point to the terminating null.
 * @param src Source array.
 * @param end End of destination array.
 */
void str_escape(char **dst, const char *src, const char *end);

int str_toint(const char *str);
bool str_toint(const char *str, int *out);
int str_toint_base(const char *str, int base);
unsigned long str_toulong_base(const char *str, int base);
int64_t str_toint64_base(const char *str, int base = 10);
float str_tofloat(const char *str);
bool str_tofloat(const char *str, float *out);

unsigned str_quickhash(const char *str);

/**
 * Encode a UTF-8 character.
 *
 * @ingroup Strings
 *
 * @param ptr Pointer to a buffer that should receive the data. Should be able to hold at least 4 bytes.
 * @param chr Unicode codepoint to encode.
 *
 * @return Number of bytes put into the buffer.
 *
 * @remark Does not do null-termination of the string.
 */
int str_utf8_encode(char *ptr, int chr);

/**
 * Decodes a UTF-8 codepoint.
 *
 * @ingroup Strings
 *
 * @param ptr Pointer to a UTF-8 string. This pointer will be moved forward.
 *
 * @return The Unicode codepoint. `-1` for invalid input and 0 for end of string.
 *
 * @remark This function will also move the pointer forward.
 * @remark You may call this function again after an error occurred.
 * @remark The strings are treated as null-terminated.
 */
int str_utf8_decode(const char **ptr);

/**
 * Truncates a UTF-8 encoded string to a given length.
 *
 * @ingroup Strings
 *
 * @param dst Pointer to a buffer that shall receive the string.
 * @param dst_size Size of the buffer dst.
 * @param src String to be truncated.
 * @param truncation_len Maximum codepoints in the returned string.
 *
 * @remark The strings are treated as utf8-encoded null-terminated strings.
 * @remark Guarantees that dst string will contain null-termination.
 */
void str_utf8_truncate(char *dst, int dst_size, const char *src, int truncation_len);

/**
 * Fixes truncation of a Unicode character at the end of a UTF-8 string.
 *
 * @ingroup Strings
 *
 * @param str UTF-8 string.
 *
 * @return The new string length.
 *
 * @remark The strings are treated as null-terminated.
 */
int str_utf8_fix_truncation(char *str);

/**
 * Removes trailing characters that render as spaces by modifying the string in-place.
 *
 * @ingroup Strings
 *
 * @param param Input string.
 *
 * @remark The string is modified in-place.
 * @remark The strings are treated as null-terminated.
 */
void str_utf8_trim_right(char *param);

/**
 * Converts the given UTF-8 string to lowercase (locale insensitive).
 *
 * @ingroup Strings
 *
 * @param input String to convert to lowercase.
 * @param output Buffer that will receive the lowercase string.
 * @param size Size of the output buffer.
 *
 * @remark The strings are treated as zero-terminated strings.
 * @remark This function does not work in-place as converting a UTF-8 string to
 *         lowercase may increase its length.
 */
void str_utf8_tolower(const char *input, char *output, size_t size);

/**
 * Checks whether the given Unicode codepoint renders as space.
 *
 * @ingroup Strings
 *
 * @param code Unicode codepoint to check.
 *
 * @return Whether the codepoint is a space.
 */
int str_utf8_isspace(int code);

/**
 * Checks whether a given byte is the start of a UTF-8 character.
 *
 * @ingroup Strings
 *
 * @param c Byte to check.
 *
 * @return Whether the char starts a UTF-8 character.
 */
int str_utf8_isstart(char c);

/**
 * Moves a cursor backwards in an UTF-8 string,
 *
 * @ingroup Strings
 *
 * @param str UTF-8 string.
 * @param cursor Position in the string.
 *
 * @return New cursor position.
 *
 * @remark Won't move the cursor less then 0.
 * @remark The strings are treated as null-terminated.
 */
int str_utf8_rewind(const char *str, int cursor);

#endif
