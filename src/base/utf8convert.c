#include "utf8convert.h"
#include "system.h"
#include <string.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* 
 In the original code "const char* latin" was a allocated variable in the method as string and the reserverd length was
 strlen(utf8) + stren(utf8)/10, don't know why, but keep that in mind 
*/
void UTF8toLatin1(char *dst, const char *src, int dst_size)
{
	
	const unsigned char* pSource = (const unsigned char*)src;
	
	unsigned long ucs4;
	int c, state, octets, i;
	
	ucs4 = 0;
	state = 0;
	octets = 0;
	i = 0;
	
	c = *(pSource);
	
	while (c && i<dst_size-1) 
	{
		switch (state) 
		{
			case 0:	/* start of utf8 char */
				ucs4 = 0;	/* reset ucs4 char */
				if ((c & 0xfe) == 0xfc) 
				{		/* 6 octets */
					ucs4 = (c & 0x01) << 30;
					octets = 6;
					state = 5;	/* look for 5 more */
				} 
				else if ((c & 0xfc) == 0xf8) 
				{	/* 5 octets */
					ucs4 = (c & 0x03) << 24;
					octets = 5;
					state = 4;
				} 
				else if ((c & 0xf8) == 0xf0) 
				{	/* 4 octets */
					ucs4 = (c & 0x07) << 18;
					octets = 4;
					state = 3;
				} 
				else if ((c & 0xf0) == 0xe0) 
				{	/* 3 octets */
					ucs4 = (c & 0x0f) << 12;
					octets = 3;
					state = 2;
				} 
				else if ((c & 0xe0) == 0xc0) 
				{	/* 2 octets */
					ucs4 = (c & 0x1f) << 6;
					octets = 2;
					state = 1;	/* look for 1 more */
				} 
				else if ((c & 0x80) == 0x00) 
				{	/* 1 octet */
					ucs4 = (c & 0x7f);
					octets = 1;
					state = 0;	/* we have a result */
				} 
				else {				/* error */
					;
				}
				break;
			case 1:
				if ((c & 0xc0) == 0x80) 
				{
					ucs4 = ucs4 | (c & 0x3f);
					if (ucs4 < 0x80 || ucs4 > 0x7ff) 
					{
						ucs4 = 0xffffffff;
					}
				} 
				else 
				{
					ucs4 = 0xffffffff;
				}
				state = 0;	/* we're done and have a result */
				break;
			case 2:
				if ((c & 0xc0) == 0x80) 
				{
					ucs4 = ucs4 | ((c & 0x3f) << 6);
					state = 1;
				} 
				else 
				{
					ucs4 = 0xffffffff;
					state = 0;
				}
				break;
			case 3:
				if ((c & 0xc0) == 0x80) 
				{
					ucs4 = ucs4 | ((c & 0x3f) << 12);
					state = 2;
				} 				
				else 
				{
					ucs4 = 0xffffffff;
					state = 0;
				}
				break;
			case 4:
				if ((c & 0xc0) == 0x80) 
				{
					ucs4 = ucs4 | ((c & 0x3f) << 18);
					state = 3;
				} 
				else 
				{
					ucs4 = 0xffffffff;
					state = 0;
				}
				break;
			case 5:
				if ((c & 0xc0) == 0x80) 
				{
					ucs4 = ucs4 | ((c & 0x3f) << 24);
					state = 4;
				} 
				else 
				{
					ucs4 = 0xffffffff;
					state = 0;
				}
				break;
			default:	/* error, can't happen */
				ucs4 = 0xffffffff;
				state = 0;
				break;
		}
		if (state == 0) 
		{
			switch (octets) 
			{
				case 1:
					if (ucs4 < 0x0 || ucs4 > 0x7f)
						ucs4 = 0xffffffff;
					break;
				case 2:
					if (ucs4 < 0x80 || ucs4 > 0x7ff)
						ucs4 = 0xffffffff;
					break;
				case 3:
					if (ucs4 < 0x800 || ucs4 > 0xffff)
						ucs4 = 0xffffffff;
					break;
				case 4:
					if (ucs4 < 0x10000 || ucs4 > 0x1fffff)
						ucs4 = 0xffffffff;
					break;
				case 5:
					if (ucs4 < 0x200000 || ucs4 > 0x3ffffff)
						ucs4 = 0xffffffff;
					break;
				case 6:
					if (ucs4 < 0x4000000 || ucs4 > 0x7fffffff)
						ucs4 = 0xffffffff;
					break;
				default:
					ucs4 = 0xffffffff;
					break;
			}
			if (ucs4 != 0xffffffff) 
			{
				dst[i++] = (char)ucs4;
			}
		}
		c = *(++pSource);
	}
	dst[i] = '\0';
}
	
void Latin1toUTF8(char *dst, const char *src, int dst_size)
{
	const unsigned char* pSource = (const unsigned char*)src;
	int i = 0;
	int cSource = *(pSource);
	while ( cSource && i < dst_size-1)
	{
		if ( cSource >= 128 )
		{
			dst[i++] = (char)(((cSource&0x7c0)>>6) | 0xc0);
			if(i < dst_size-1)
				dst[i++] = (char)((cSource&0x3f) | 0x80);
		}
		else
			dst[i++] = cSource;
		cSource = *(++pSource);
	}
	dst[i] = '\0';

}
	
#if defined(__cplusplus)
}
#endif