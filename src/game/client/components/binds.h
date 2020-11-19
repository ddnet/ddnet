/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_BINDS_H
#define GAME_CLIENT_COMPONENTS_BINDS_H
#include <engine/keys.h>
#include <game/client/component.h>

class CBinds : public CComponent
{
	int GetKeyID(const char *pKeyName);

	static void ConBind(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpBinds(IConsole::IResult *pResult, void *pUserData);
	static void ConUnbind(IConsole::IResult *pResult, void *pUserData);
	static void ConUnbindAll(IConsole::IResult *pResult, void *pUserData);
	class IConsole *GetConsole() const { return Console(); }

	static void ConfigSaveCallback(class IConfig *pConfig, void *pUserData);

public:
	CBinds();
	~CBinds();

	class CBindsSpecial : public CComponent
	{
	public:
		CBinds *m_pBinds;
		virtual bool OnInput(IInput::CEvent Event);
	};

	enum
	{
		MODIFIER_NONE = 0,
		MODIFIER_SHIFT,
		MODIFIER_CTRL,
		MODIFIER_ALT,
		MODIFIER_COUNT,
		MODIFIER_COMBINATION_COUNT = 1 << MODIFIER_COUNT
	};

	CBindsSpecial m_SpecialBinds;

	void Bind(int KeyID, const char *pStr, bool FreeOnly = false, int Modifier = MODIFIER_NONE);
	void SetDefaults();
	void UnbindAll();
	const char *Get(int KeyID, int Modifier);
	void GetKey(const char *pBindStr, char *aBuf, unsigned BufSize);
	int GetBindSlot(const char *pBindString, int *Modifier);
	static int GetModifierMask(IInput *i);
	static int GetModifierMaskOfKey(int Key);
	static bool ModifierMatchesKey(int Modifier, int Key);
	static const char *GetModifierName(int Modifier);
	static const char *GetKeyBindModifiersName(int Modifier);

	virtual void OnConsoleInit();
	virtual bool OnInput(IInput::CEvent Event);

	// DDRace

	void SetDDRaceBinds(bool FreeOnly);

private:
	char *m_aapKeyBindings[MODIFIER_COMBINATION_COUNT][KEY_LAST];
};
#endif
