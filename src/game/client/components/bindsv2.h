/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_BINDSV2_H
#define GAME_CLIENT_COMPONENTS_BINDSV2_H

#include <engine/console.h>
#include <engine/keys.h>

#include <game/client/component.h>

enum
{
	MAX_GROUP_NAME_LENGTH = 16
};

class CBindsV2 : public CComponent
{
	int GetKeyID(const char *pKeyName);

	static void ConBind(const char *pBindStr, const char *pCommand, const std::shared_ptr<CBindsV2> &pBinds);
	static void ConBinds(const char *pKey, const std::shared_ptr<CBindsV2> &pBinds);
	static void ConUnbind(const char *pKey, const std::shared_ptr<CBindsV2> &pBinds);
	static void ConUnbindAll(const std::shared_ptr<CBindsV2> &pBinds);
	class IConsole *GetConsole() const { return Console(); }

public:
	CBindsV2(const char *pGroupName);
	~CBindsV2();
	virtual int Sizeof() const override { return sizeof(*this); }

	class CBindsV2Special : public CComponent
	{
	public:
		CBindsV2 *m_pBinds;
		virtual int Sizeof() const override { return sizeof(*this); }
		virtual bool OnInput(const IInput::CEvent &Event) override;

		friend class CBindsManager;
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

	CBindsV2Special m_SpecialBinds;

	void Bind(int KeyID, const char *pStr, bool FreeOnly = false, int ModifierCombination = MODIFIER_NONE);
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
	const char *GroupName() const { return m_aGroupName; }

private:
	char *m_aapKeyBindings[MODIFIER_COMBINATION_COUNT][KEY_LAST];
	char m_aGroupName[MAX_GROUP_NAME_LENGTH];

	void SaveBinds();

	friend class CBindsManager;
};
#endif
