#ifndef GAME_EDITOR_EDITOR_QUICK_ACTION_H
#define GAME_EDITOR_EDITOR_QUICK_ACTION_H

#include <functional>
#include <utility>

typedef std::function<void()> FButtonClickCallback;
typedef std::function<bool()> FButtonEnabledCallback;

class CQuickAction
{
public:
	const char *m_pLabel;
	const char *m_pDescription;
	FButtonClickCallback m_pfnCallback;
	FButtonClickCallback m_pfnEnabledCallback;

	CQuickAction(
		const char *pLabel,
		const char *pDescription,
		FButtonClickCallback pfnCallback) :
		m_pLabel(pLabel),
		m_pDescription(pDescription),
		m_pfnCallback(std::move(pfnCallback))
	{
		m_pfnEnabledCallback = []() { return true; };
	}

	void Call() const
	{
		m_pfnCallback();
	}

	const char *Label() const { return m_pLabel; }
	const char *Description() const { return m_pDescription; }
};

#endif
