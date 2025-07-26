/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_BINDS_H
#define GAME_CLIENT_COMPONENTS_BINDS_H

#include <engine/console.h>
#include <engine/keys.h>

#include <game/client/component.h>

#include <vector>

class IConfigManager;

class CBinds : public CComponent
{
	static void ConBind(IConsole::IResult *pResult, void *pUserData);
	static void ConBinds(IConsole::IResult *pResult, void *pUserData);
	static void ConUnbind(IConsole::IResult *pResult, void *pUserData);
	static void ConUnbindAll(IConsole::IResult *pResult, void *pUserData);
	class IConsole *GetConsole() const { return Console(); }

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

	class CBindSlot
	{
	public:
		int m_Key;
		int m_ModifierMask;

		CBindSlot(int Key, int ModifierMask) :
			m_Key(Key),
			m_ModifierMask(ModifierMask)
		{
		}
	};
	CBindSlot GetBindSlot(const char *pBindString) const;

	// free buffer after use
	char *GetKeyBindCommand(int ModifierCombination, int Key) const;

public:
	CBinds();
	~CBinds();
	int Sizeof() const override { return sizeof(*this); }

	class CBindsSpecial : public CComponent
	{
	public:
		CBinds *m_pBinds;
		int Sizeof() const override { return sizeof(*this); }
		bool OnInput(const IInput::CEvent &Event) override;
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

	void Bind(int KeyId, const char *pStr, bool FreeOnly = false, int ModifierCombination = MODIFIER_NONE);
	void SetDefaults();
	void UnbindAll();
	const char *Get(int KeyId, int ModifierCombination);
	void GetKey(const char *pBindStr, char *pBuf, size_t BufSize);
	static int GetModifierMask(IInput *pInput);
	static int GetModifierMaskOfKey(int Key);
	static const char *GetModifierName(int Modifier);
	static void GetKeyBindModifiersName(int ModifierCombination, char *pBuf, size_t BufSize);

	void OnConsoleInit() override;
	bool OnInput(const IInput::CEvent &Event) override;

	// DDRace

	void SetDDRaceBinds(bool FreeOnly);

private:
	char *m_aapKeyBindings[MODIFIER_COMBINATION_COUNT][KEY_LAST];
	std::vector<CBindSlot> m_vActiveBinds;
};
#endif
