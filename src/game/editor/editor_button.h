#ifndef GAME_EDITOR_EDITOR_BUTTON_H
#define GAME_EDITOR_EDITOR_BUTTON_H

#include <functional>
#include <utility>

typedef std::function<void()> FButtonClickCallback;
typedef std::function<bool()> FButtonEnabledCallback;

class CEditorButton
{
public:
	const char *m_pText;
	FButtonClickCallback m_pfnCallback;
	FButtonClickCallback m_pfnEnabledCallback;

	CEditorButton(
		const char *pText,
		FButtonClickCallback pfnCallback
		) :
		m_pText(pText),
		m_pfnCallback(std::move(pfnCallback))
	{
		m_pfnEnabledCallback = []() { return true; };
	}

	void Call() const
	{
		m_pfnCallback();
	}
};

#endif
