// do we need some gctf system.h?

#include "instagib.h"

const char *str_find_digit(const char *haystack)
{
	char aaDigits[][2] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

	const char *first = NULL;
	for(const char *aDigit : aaDigits)
	{
		const char *s = str_find(haystack, aDigit);
		if(s && (!first || first > s))
			first = s;
	}
	return first;
}

bool str_contains_ip(const char *pStr)
{
	const char *s = str_find_digit(pStr);
	if(!s)
		return false;
	// dbg_msg("str_contains_ip", "s=%s", s);
	for(int byte = 0; byte < 4; byte++)
	{
		int i;
		for(i = 0; i < 3; i++)
		{
			// dbg_msg("str_contains_ip", "b=%d s=%c", byte, s[0]);
			if(!s && !s[0])
			{
				// dbg_msg("str_contains_ip", "EOL");
				return false;
			}
			if(i > 0 && s[0] == '.')
			{
				// dbg_msg("str_contains_ip", "break in byte %d at i=%d because got dot", byte, i);
				break;
			}
			if(!isdigit(s[0]))
			{
				if(i > 0)
				{
					// dbg_msg("str_contains_ip", "we got ip with less than 3 digits in the end");
					return true;
				}
				// dbg_msg("str_contains_ip", "non digit char");
				return false;
			}
			s++;
		}
		if(byte == 3 && i > 1)
		{
			// dbg_msg("str_contains_ip", "we got ip with 3 digits in the end");
			return true;
		}
		if(s[0] == '.')
			s++;
		else
		{
			// dbg_msg("str_contains_ip", "expected dot got no dot!!");
			return false;
		}
	}
	return false;
}

// int test_thing()
// {
// 	char aMsg[512];
// 	strncpy(aMsg, "join  my server 127.0.0.1 omg it best", sizeof(aMsg));

//     dbg_msg("test", "contains %d", str_contains_ip(aMsg));

// 	return 0;
// }
