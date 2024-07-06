#ifndef GAME_EDITOR_EDITOR_QUICK_ACTION_H
#define GAME_EDITOR_EDITOR_QUICK_ACTION_H

#include <functional>
#include <utility>

typedef std::function<void()> FButtonClickCallback;
typedef std::function<bool()> FButtonDisabledCallback;
typedef std::function<bool()> FButtonActiveCallback;

class CQuickAction
{
private:
	const char *m_pLabel;
	const char *m_pDescription;

	FButtonClickCallback m_pfnCallback;
	FButtonDisabledCallback m_pfnDisabledCallback;
	FButtonActiveCallback m_pfnActiveCallback;

public:
	CQuickAction(
		const char *pLabel,
		const char *pDescription,
		FButtonClickCallback pfnCallback,
		FButtonDisabledCallback pfnDisabledCallback,
		FButtonActiveCallback pfnActiveCallback) :
		m_pLabel(pLabel),
		m_pDescription(pDescription),
		m_pfnCallback(std::move(pfnCallback)),
		m_pfnDisabledCallback(std::move(pfnDisabledCallback)),
		m_pfnActiveCallback(std::move(pfnActiveCallback))
	{
	}

	// code to run when the action is triggered
	void Call() const { m_pfnCallback(); }

	// bool that indicates if the action can be performed not or not
	bool Disabled() { return m_pfnDisabledCallback(); }

	// bool that indicates if the action is currently running
	// only applies to actions that can be turned on or off like proof borders
	bool Active() { return m_pfnActiveCallback(); }

	const char *Label() const { return m_pLabel; }
	const char *Description() const { return m_pDescription; }
};

#endif
