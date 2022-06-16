//
// Created by Marvin on 05.06.2022.
//

#include "material.h"
#include "mapitems.h"
#include <base/math.h>

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

CMaterials::CMaterials()
{
	m_apMaterials = {
		new CMatDefault(),
		new CMatIce()};
}

CMaterials::~CMaterials()
{
	for(CMatDefault *Mat : m_apMaterials)
	{
		delete Mat;
	}
	m_apMaterials.clear();
}

CMatDefault &CMaterials::operator[](int Index)
{
	switch(Index)
	{
	case MAT_ICE: return *m_apMaterials[1];
	}
	return *m_apMaterials[MAT_DEFAULT];
}

float CMaterials::GetGroundControlSpeed(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight)
{
	return HandleMaterialInteraction(GroundedLeft, GroundedRight, At(MaterialLeft).m_GroundControlSpeed, At(MaterialRight).m_GroundControlSpeed, [](float a, float b) { return minimum(a, b); });
}

float CMaterials::GetGroundControlAccel(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight)
{
	return HandleMaterialInteraction(GroundedLeft, GroundedRight, At(MaterialLeft).m_GroundControlAccel, At(MaterialRight).m_GroundControlAccel, [](float a, float b) { return maximum(a, b); });
}

float CMaterials::GetGroundFriction(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight)
{
	// Note: Higher friction VALUE means LOWER physical friction
	return HandleMaterialInteraction(GroundedLeft, GroundedRight, At(MaterialLeft).m_GroundFriction, At(MaterialRight).m_GroundFriction, [](float a, float b) { return minimum(a, b); });
}

float CMaterials::GetGroundJumpImpulse(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight)
{
	// you sticked to something with one leg, average it?
	return HandleMaterialInteraction(GroundedLeft, GroundedRight, At(MaterialLeft).m_GroundJumpImpulse, At(MaterialRight).m_GroundJumpImpulse, [](float a, float b) { return (a + b) / 2; });
}

float CMaterials::HandleMaterialInteraction(bool GroundedLeft, bool GroundedRight, float ValueLeft, float ValueRight, const std::function<float(float, float)> &function)
{
	if(GroundedLeft && !GroundedRight) //standing on the left edge
		return ValueLeft;
	else if(GroundedRight && !GroundedLeft) //standing on the right edge
		return ValueRight;
	else
		return function(ValueLeft, ValueRight); //standing on two materials
}
