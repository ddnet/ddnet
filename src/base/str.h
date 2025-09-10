#ifndef BASE_STR_H
#define BASE_STR_H

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
 * @remark The following characters are considered whitespace: ' ', '\n', '\r', '\t'.
 */
int str_isspace(char c);

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
 * @param str String to be truncated.
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
