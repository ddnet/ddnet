#ifndef GAME_ENVELOPE_TRIGGER_H
#define GAME_ENVELOPE_TRIGGER_H

#include <vector>

enum EEnvelopeTriggerType
{
	TRIGGER_TYPE_DEFAULT = 0,
	TRIGGER_TYPE_PAUSE,
	TRIGGER_TYPE_START_ONCE,
	TRIGGER_TYPE_START_LOOP,
	TRIGGER_TYPE_STOP,
	TRIGGER_TYPE_RESET_START_ONCE,
	TRIGGER_TYPE_RESET_START_LOOP,
	NUM_ENVELOPE_TRIGGERS,
};

enum EEnvelopeTriggerBitFlags
{
	TRIGGER_FLAG_NONE = 0,
	TRIGGER_FLAG_TEAM = 1,
};

static constexpr int ENVELOPE_NONE = -1;
static constexpr int ENVELOPE_RESET = -2;

static inline bool IsEnvelopeTriggerPlaying(EEnvelopeTriggerType Type)
{
	return Type != EEnvelopeTriggerType::TRIGGER_TYPE_DEFAULT && Type != EEnvelopeTriggerType::TRIGGER_TYPE_PAUSE && Type != EEnvelopeTriggerType::TRIGGER_TYPE_STOP;
}

class CEnvelopeTrigger
{
public:
	int m_EnvelopeId;
	EEnvelopeTriggerType m_State;
	static const char *ConsoleName(EEnvelopeTriggerType Trigger);
	static EEnvelopeTriggerType FromName(const char *pTriggerName);
};

class CEnvelopeTriggerZone
{
public:
	std::vector<CEnvelopeTrigger> m_vEnvelopeTriggers;
};

#endif
