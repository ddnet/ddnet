/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_KEY_BINDER_H
#define GAME_CLIENT_COMPONENTS_KEY_BINDER_H

#include <game/client/component.h>
#include <game/client/ui_rect.h>

// component to fetch keypresses, override all other input
class CKeyBinder : public CComponent
{
public:
	CKeyBinder();
	int Sizeof() const override { return sizeof(*this); }
	bool OnInput(const IInput::CEvent &Event) override;

	int DoKeyReader(const void *pId, const CUIRect *pRect, int Key, int ModifierCombination, int *pNewModifierCombination);
	bool IsActive() const;

private:
	const void *m_pKeyReaderId;
	bool m_TakeKey;
	bool m_GotKey;
	IInput::CEvent m_Key;
	int m_ModifierCombination;
};

#endif
