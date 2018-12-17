#include <base/system.h>
#include <engine/shared/json.h>

static char EscapeJsonChar(char c)
{
	switch(c)
	{
	case '\"': return '\"';
	case '\\': return '\\';
	case '\b': return 'b';
	case '\n': return 'n';
	case '\r': return 'r';
	case '\t': return 't';
	// Don't escape '\f', who uses that. :)
	default: return 0;
	}
}

char *EscapeJson(char *pBuffer, int BufferSize, const char *pString)
{
	dbg_assert(BufferSize > 0, "can't null-terminate the string");
	// Subtract the space for null termination early.
	BufferSize--;

	char *pResult = pBuffer;
	while(BufferSize && *pString)
	{
		char c = *pString;
		pString++;
		char Escaped = EscapeJsonChar(c);
		if(Escaped)
		{
			if(BufferSize < 2)
			{
				break;
			}
			*pBuffer++ = '\\';
			*pBuffer++ = Escaped;
			BufferSize -= 2;
		}
		// Assuming ASCII/UTF-8, "if control character".
		else if((unsigned char)c < 0x20)
		{
			// \uXXXX
			if(BufferSize < 6)
			{
				break;
			}
			str_format(pBuffer, BufferSize, "\\u%04x", c);
			pBuffer += 6;
			BufferSize -= 6;
		}
		else
		{
			*pBuffer++ = c;
			BufferSize--;
		}
	}
	*pBuffer = 0;
	return pResult;
}

const char *JsonBool(bool Bool)
{
	if(Bool)
	{
		return "true";
	}
	else
	{
		return "false";
	}
}
