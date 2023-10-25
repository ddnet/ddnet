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
	static void ConChord(IConsole::IResult *pResult, void *pUserData);
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
		virtual bool OnInput(const IInput::CEvent &Event) override;
	};

	class CBindsChord : public CComponent
	{
	public:
		virtual int Sizeof() const override { return sizeof(*this); }
		virtual bool OnInput(const IInput::CEvent &Event) override;
		int m_keyBindingsLength = 0;
		void FreeKeyBindings() {
			if (m_keyBindingsLength == 0) return;
			for (int i = 0; i < m_keyBindingsLength; i++)
				free((void*)m_keyBindings[i].Command);
			free((void*)m_keyBindings);
			m_keyBindingsLength = 0;
			m_keyBindings = NULL;
		}
		typedef struct
		{
			int Key;
			const char *Command;
		} keyBinding;
		keyBinding *m_keyBindings;
	};

	bool m_MouseOnAction;

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

	CBindsChord m_ChordBinds;

	void Bind(int KeyID, const char *pStr, bool FreeOnly = false, int ModifierCombination = MODIFIER_NONE);
	void SetDefaults();
	void UnbindAll();
	const char *Get(int KeyID, int ModifierCombination);
	void GetKey(const char *pBindStr, char *pBuf, size_t BufSize);
	int GetBindSlot(const char *pBindString, int *pModifierCombination);
	static int GetModifierMask(IInput *pInput);
	static int GetModifierMaskOfKey(int Key);
	static const char *GetModifierName(int Modifier);
	static const char *GetKeyBindModifiersName(int ModifierCombination);

	virtual void OnConsoleInit() override;
	virtual bool OnInput(const IInput::CEvent &Event) override;

	// DDRace

	void SetDDRaceBinds(bool FreeOnly);

private:
	char *m_aapKeyBindings[MODIFIER_COMBINATION_COUNT][KEY_LAST];
};
#endif
