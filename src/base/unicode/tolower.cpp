#include <cstdlib>

#include "tolower.h"

static int CompUl(const void *pA, const void *pB)
{
	CUpperToLower *ul_a = (CUpperToLower *)pA;
	CUpperToLower *ul_b = (CUpperToLower *)pB;
	return ul_a->m_Upper - ul_b->m_Upper;
}

int str_utf8_to_lower(int code)
{
	CUpperToLower Key;
	CUpperToLower *pRes;
	Key.m_Upper = code;
	pRes = (CUpperToLower *)bsearch(&Key, ToLowerMap, ToLowerNum, sizeof(CUpperToLower), CompUl);

	if(pRes == nullptr)
		return code;
	return pRes->m_Lower;
}

#define TOLOWER_DATA
#include "tolower_data.h"
#undef TOLOWER_DATA
