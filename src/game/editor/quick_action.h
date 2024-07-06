#ifndef GAME_EDITOR_EDITOR_QUICK_ACTION_H
#define GAME_EDITOR_EDITOR_QUICK_ACTION_H

#include <functional>
#include <utility>

typedef std::function<void()> FButtonClickCallback;
typedef std::function<bool()> FButtonEnabledCallback;
typedef std::function<bool()> FButtonActiveCallback;

class CQuickAction
{
public:
	const char *m_pLabel;
	const char *m_pDescription;

	// code to run when the action is triggered
	FButtonClickCallback m_pfnCallback;

	// bool that indicates if the action can be performed not or not
	FButtonEnabledCallback m_pfnEnabledCallback;

	// bool that indicates if the action is currently running
	// only applies to actions that can be turned on or off like proof borders
	FButtonActiveCallback m_pfnActiveCallback;

	CQuickAction(
		const char *pLabel,
		const char *pDescription,
		FButtonClickCallback pfnCallback) :
		m_pLabel(pLabel),
		m_pDescription(pDescription),
		m_pfnCallback(std::move(pfnCallback))
	{
		m_pfnEnabledCallback = []() { return true; };
		m_pfnActiveCallback = []() { return true; };
	}

	void Call() const
	{
		m_pfnCallback();
	}

	const char *Label() const { return m_pLabel; }
	const char *Description() const { return m_pDescription; }
};

#endif
