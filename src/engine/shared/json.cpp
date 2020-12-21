#include <base/system.h>
#include <engine/shared/json.h>

const struct _json_value *json_object_get(const json_value *object, const char *index)
{
	unsigned int i;

	if(object->type != json_object)
		return &json_value_none;

	for(i = 0; i < object->u.object.length; ++i)
		if(!str_comp(object->u.object.values[i].name, index))
			return object->u.object.values[i].value;

	return &json_value_none;
}

const struct _json_value *json_array_get(const json_value *array, int index)
{
	if(array->type != json_array || index >= (int)array->u.array.length)
		return &json_value_none;

	return array->u.array.values[index];
}

int json_array_length(const json_value *array)
{
	return array->u.array.length;
}

const char *json_string_get(const json_value *string)
{
	return string->u.string.ptr;
}

int json_int_get(const json_value *integer)
{
	return integer->u.integer;
}

int json_boolean_get(const json_value *boolean)
{
	return boolean->u.boolean != 0;
}

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
