#include "tuning.h"

#include <base/dbg.h>
#include <base/str.h>

#include <generated/protocol.h>

const char *CTuningParams::ms_apNames[] =
	{
#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) #ScriptName,
#include "tuning_params.h"
#undef MACRO_TUNING_PARAM
};

bool CTuningParams::Set(int Index, float Value)
{
	if(Index < 0 || Index >= Num())
		return false;
	((CTuneParam *)this)[Index] = Value;
	return true;
}

bool CTuningParams::Get(int Index, float *pValue) const
{
	if(Index < 0 || Index >= Num())
		return false;
	*pValue = (float)((CTuneParam *)this)[Index];
	return true;
}

bool CTuningParams::Set(const char *pName, float Value)
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, Name(i)) == 0)
			return Set(i, Value);
	return false;
}

bool CTuningParams::Get(const char *pName, float *pValue) const
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, Name(i)) == 0)
			return Get(i, pValue);

	return false;
}

float CTuningParams::GetWeaponFireDelay(int Weapon) const
{
	switch(Weapon)
	{
	case WEAPON_HAMMER: return (float)m_HammerFireDelay / 1000.0f;
	case WEAPON_GUN: return (float)m_GunFireDelay / 1000.0f;
	case WEAPON_SHOTGUN: return (float)m_ShotgunFireDelay / 1000.0f;
	case WEAPON_GRENADE: return (float)m_GrenadeFireDelay / 1000.0f;
	case WEAPON_LASER: return (float)m_LaserFireDelay / 1000.0f;
	case WEAPON_NINJA: return (float)m_NinjaFireDelay / 1000.0f;
	default: dbg_assert_failed("invalid weapon");
	}
}

int CTuningParams::GetIndex(const char *pName)
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, Name(i)) == 0)
			return i;
	return -1;
}

CTuningParams *CTuningParams::ApplyLockedTunings(CTuningParams *pTuning, LOCKED_TUNES &LockedTunings)
{
	static CTuningParams s_Tuning;
	s_Tuning = *pTuning;
	for(auto &LockedTune : LockedTunings)
		s_Tuning.Set(LockedTune.m_ParamIndex, (float)LockedTune.m_Value);
	return &s_Tuning;
}

int SetLockedTune(CTuningParams *pGlobalTuning, LOCKED_TUNES *pLockedTunings, CLockedTune &Tune, bool AllowGlobalValues)
{
	float GlobalValue;
	if(!pGlobalTuning->Get(Tune.m_ParamIndex, &GlobalValue))
		return 0;

	bool IsGlobalValue = Tune.m_Value.Get() == (int)(GlobalValue * 100.f);
	for(unsigned int i = 0; i < pLockedTunings->size(); i++)
	{
		if(pLockedTunings->at(i).m_ParamIndex == Tune.m_ParamIndex)
		{
			if(IsGlobalValue)
			{
				pLockedTunings->erase(pLockedTunings->begin() + i);
				return 3;
			}
			pLockedTunings->at(i).m_Value = Tune.m_Value;
			return 2;
		}
	}

	if(IsGlobalValue && !AllowGlobalValues)
		return 0;

	pLockedTunings->push_back(Tune);
	return 1;
}

void ApplyTuneLock(CTuningParams *pGlobalTuning, LOCKED_TUNES *pLockedTunings, LOCKED_TUNES *pLockedTuningList)
{
	if(pLockedTuningList == nullptr)
	{
		pLockedTunings->clear();
		return;
	}

	for(auto &LockedTune : *pLockedTuningList)
		SetLockedTune(pGlobalTuning, pLockedTunings, LockedTune);
}
