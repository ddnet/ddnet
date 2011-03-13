#ifndef BASE_UTF8CONVERT_H
#define BASE_UTF8CONVERT_H

#ifdef __cplusplus
extern "C" {
#endif
	
void UTF8toLatin1(char *dst, const char *src, int dst_size);
void Latin1toUTF8(char *dst, const char *src, int dst_size);

#ifdef __cplusplus
}
#endif

#endif // BASE_UTF8CONVERT_H