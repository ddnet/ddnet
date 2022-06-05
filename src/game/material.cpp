//
// Created by Marvin on 05.06.2022.
//

#include "material.h"

const char *CMatDefault::ms_apNames[] =
	{
#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) #ScriptName,
#include "tuning.h"
#undef MACRO_TUNING_PARAM
	};

bool CMatDefault::Set(int Index, float Value)
{
	if(Index < 0 || Index >= Num())
		return false;
	((CTuneParam *)this)[Index] = Value;
	return true;
}

bool CMatDefault::Get(int Index, float *pValue) const
{
	if(Index < 0 || Index >= Num())
		return false;
	*pValue = (float)((CTuneParam *)this)[Index];
	return true;
}

bool CMatDefault::Set(const char *pName, float Value)
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, ms_apNames[i]) == 0)
			return Set(i, Value);
	return false;
}

bool CMatDefault::Get(const char *pName, float *pValue) const
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, ms_apNames[i]) == 0)
			return Get(i, pValue);

	return false;
}