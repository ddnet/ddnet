#include "envelope_trigger.h"

#include <base/dbg.h>
#include <base/str.h>

static constexpr const char *s_apEnvelopeTriggerNames[NUM_ENVELOPE_TRIGGERS] = {
	"default",
	"pause",
	"start_once",
	"start_loop",
	"stop",
	"reset_start_once",
	"reset_start_loop",
};

const char *CEnvelopeTrigger::ConsoleName(EEnvelopeTriggerType Trigger)
{
	dbg_assert(Trigger >= 0 && Trigger < NUM_ENVELOPE_TRIGGERS, "unknown envelope trigger type");
	return s_apEnvelopeTriggerNames[static_cast<int>(Trigger)];
}

EEnvelopeTriggerType CEnvelopeTrigger::FromName(const char *pTriggerName)
{
	for(int i = 0; i < NUM_ENVELOPE_TRIGGERS; i++)
	{
		if(str_comp(pTriggerName, s_apEnvelopeTriggerNames[i]) == 0)
			return static_cast<EEnvelopeTriggerType>(i);
	}
	return EEnvelopeTriggerType::TRIGGER_TYPE_DEFAULT;
}
