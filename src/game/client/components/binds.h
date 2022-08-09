/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_BINDS_H
#define GAME_CLIENT_COMPONENTS_BINDS_H

#include <engine/console.h>
#include <engine/keys.h>

#include <game/client/component.h>

class IConfigManager;

class CBinds : public CComponent
{
	int GetKeyID(const char *pKeyName);

	static void ConBind(IConsole::IResult *pResult, void *pUserData);
	static void ConBinds(IConsole::IResult *pResult, void *pUserData);
	static void ConUnbind(IConsole::IResult *pResult, void *pUserData);
	static void ConUnbindAll(IConsole::IResult *pResult, void *pUserData);
	class IConsole *GetConsole() const { return Console(); }

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

public:
	CBinds();
	~CBinds();
	virtual int Sizeof() const override { return sizeof(*this); }

	class CBindsSpecial : public CComponent
	{
	public:
		CBinds *m_pBinds;
		virtual int Sizeof() const override { return sizeof(*this); }
		virtual bool OnInput(IInput::CEvent Event) override;
	};

	enum
	{
		MODIFIER_NONE = 0,
		MODIFIER_CTRL,
		MODIFIER_ALT,
		MODIFIER_SHIFT,
		MODIFIER_GUI,
		MODIFIER_COUNT,
		MODIFIER_COMBINATION_COUNT = 1 << MODIFIER_COUNT
	};

	CBindsSpecial m_SpecialBinds;

	void Bind(int KeyID, const char *pStr, bool FreeOnly = false, int ModifierCombination = MODIFIER_NONE);
	void SetDefaults();
	void UnbindAll();
	const char *Get(int KeyID, int ModifierCombination);
	void GetKey(const char *pBindStr, char *aBuf, unsigned BufSize);
	int GetBindSlot(const char *pBindString, int *pModifierCombination);
	static int GetModifierMask(IInput *pInput);
	static int GetModifierMaskOfKey(int Key);
	static const char *GetModifierName(int Modifier);
	static const char *GetKeyBindModifiersName(int ModifierCombination);

	virtual void OnConsoleInit() override;
	virtual bool OnInput(IInput::CEvent Event) override;

	// DDRace

	void SetDDRaceBinds(bool FreeOnly);

private:
	char *m_aapKeyBindings[MODIFIER_COMBINATION_COUNT][KEY_LAST];
};
#endif
