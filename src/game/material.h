//
// Created by Marvin on 05.06.2022.
//

#ifndef DDNET_MATERIAL_H
#define DDNET_MATERIAL_H

#include <base/system.h>

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

class CTuningParams
{
public:
	CTuningParams()
	{
		const float TicksPerSecond = 50.0f;
#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) m_##Name.Set((int)(Value * 100.0f));
#include "tuning.h"
#undef MACRO_TUNING_PARAM
	}

	static const char *ms_apNames[];

#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) CTuneParam m_##Name;
#include "tuning.h"
#undef MACRO_TUNING_PARAM

	static int Num()
	{
		return sizeof(CTuningParams) / sizeof(CTuneParam);
	}
	bool Set(int Index, float Value);
	bool Set(const char *pName, float Value);
	bool Get(int Index, float *pValue) const;
	bool Get(const char *pName, float *pValue) const;
};

#endif //DDNET_MATERIAL_H
