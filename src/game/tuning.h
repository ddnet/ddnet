#ifndef GAME_TUNING_H
#define GAME_TUNING_H

#include <engine/shared/protocol.h>

#include <algorithm>
#include <vector>

class CTuneParam
{
	int m_Value;

public:
	void Set(int v) { m_Value = v; }
	int Get() const { return m_Value; }
	CTuneParam &operator=(int v)
	{
		m_Value = (int)(v * 100.0f);
		return *this;
	}
	CTuneParam &operator=(float v)
	{
		m_Value = (int)(v * 100.0f);
		return *this;
	}
	operator float() const { return m_Value / 100.0f; }
};

class CLockedTune
{
public:
	int m_ParamIndex;
	CTuneParam m_Value;

	CLockedTune(int ParamIndex, float Value)
	{
		m_ParamIndex = ParamIndex;
		m_Value = Value;
	}

	bool operator==(const CLockedTune &Tune) const
	{
		return m_ParamIndex == Tune.m_ParamIndex && m_Value.Get() == Tune.m_Value.Get();
	}
};
typedef std::vector<CLockedTune> LOCKED_TUNES;

inline bool IsTuneInList(const LOCKED_TUNES *pLockedTunings, int ParamIndex)
{
	return std::any_of(pLockedTunings->begin(), pLockedTunings->end(),
		[ParamIndex](const CLockedTune &Tune) {
			return Tune.m_ParamIndex == ParamIndex;
		});
}

class CTuningParams
{
	static const char *ms_apNames[];

public:
	CTuningParams()
	{
#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) m_##Name.Set((int)((Value) * 100.0f));
#include "tuning_params.h"
#undef MACRO_TUNING_PARAM
	}

#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) CTuneParam m_##Name;
#include "tuning_params.h"
#undef MACRO_TUNING_PARAM

	static int Num()
	{
		return sizeof(CTuningParams) / sizeof(int);
	}
	int *NetworkArray() { return (int *)this; }
	const int *NetworkArray() const { return (const int *)this; }
	bool Set(int Index, float Value);
	bool Set(const char *pName, float Value);
	bool Get(int Index, float *pValue) const;
	bool Get(const char *pName, float *pValue) const;
	static const char *Name(int Index) { return ms_apNames[Index]; }
	float GetWeaponFireDelay(int Weapon) const;
	static int GetIndex(const char *pName);
	static CTuningParams *ApplyLockedTunings(CTuningParams *pTuning, LOCKED_TUNES &LockedTunings);

	static const CTuningParams DEFAULT;
};

int SetLockedTune(CTuningParams *pGlobalTuning, LOCKED_TUNES *pLockedTunings, CLockedTune &Tune, bool AllowGlobalValues = false);
void ApplyTuneLock(CTuningParams *pGlobalTuning, LOCKED_TUNES *pLockedTunings, LOCKED_TUNES *pLockedTuningList);

#endif
