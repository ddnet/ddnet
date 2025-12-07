/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "mem.h"

void mem_copy(void *dest, const void *source, size_t size)
{
	memcpy(dest, source, size);
}

void mem_move(void *dest, const void *source, size_t size)
{
	memmove(dest, source, size);
}

int mem_comp(const void *a, const void *b, size_t size)
{
	return memcmp(a, b, size);
}

bool mem_has_null(const void *block, size_t size)
{
	const unsigned char *bytes = (const unsigned char *)block;
	for(size_t i = 0; i < size; i++)
	{
		if(bytes[i] == 0)
		{
			return true;
		}
	}
	return false;
}
