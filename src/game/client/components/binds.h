/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_BINDS_H
#define GAME_CLIENT_COMPONENTS_BINDS_H

#include <engine/console.h>
#include <engine/keys.h>

#include <game/client/component.h>

#include <vector>

class IConfigManager;

namespace KeyModifier
{
	inline constexpr int32_t NONE = 0;
	inline constexpr int32_t CTRL = 1;
	inline constexpr int32_t ALT = 2;
	inline constexpr int32_t SHIFT = 3;
	inline constexpr int32_t GUI = 4;
	inline constexpr int32_t COUNT = 5;
	inline constexpr int32_t COMBINATION_COUNT = 1 << COUNT;
};

class CBindSlot
{
public:
	int m_Key;
	int m_ModifierMask;

	constexpr CBindSlot(int Key, int ModifierMask) :
		m_Key(Key),
		m_ModifierMask(ModifierMask)
	{
	}

	constexpr bool operator==(const CBindSlot &Other) const { return m_Key == Other.m_Key && m_ModifierMask == Other.m_ModifierMask; }
	constexpr bool operator!=(const CBindSlot &Other) const { return !(*this == Other); }
};

inline constexpr CBindSlot EMPTY_BIND_SLOT = CBindSlot(KEY_UNKNOWN, KeyModifier::NONE);

class CBinds : public CComponent
{
	static void ConBind(IConsole::IResult *pResult, void *pUserData);
	static void ConBinds(IConsole::IResult *pResult, void *pUserData);
	static void ConUnbind(IConsole::IResult *pResult, void *pUserData);
	static void ConUnbindAll(IConsole::IResult *pResult, void *pUserData);

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

	CBindSlot GetBindSlot(const char *pBindString) const;

	// free buffer after use
	char *GetKeyBindCommand(int ModifierCombination, int Key) const;

public:
	CBinds();
	~CBinds() override;
	int Sizeof() const override { return sizeof(*this); }

	class CBindsSpecial : public CComponent
	{
	public:
		CBinds *m_pBinds;
		int Sizeof() const override { return sizeof(*this); }
		bool OnInput(const IInput::CEvent &Event) override;
	};

	bool m_MouseOnAction;

	CBindsSpecial m_SpecialBinds;

	void Bind(int KeyId, const char *pStr, bool FreeOnly = false, int ModifierCombination = KeyModifier::NONE);
	void SetDefaults();
	void UnbindAll();
	const char *Get(int KeyId, int ModifierCombination) const;
	const char *Get(const CBindSlot &BindSlot) const;
	void GetKey(const char *pBindStr, char *pBuf, size_t BufSize) const;
	static int GetModifierMask(IInput *pInput);
	static int GetModifierMaskOfKey(int Key);
	static const char *GetModifierName(int Modifier);
	void GetKeyBindName(int Key, int ModifierMask, char *pBuf, size_t BufSize) const;

	void OnConsoleInit() override;
	bool OnInput(const IInput::CEvent &Event) override;

	// DDRace

	void SetDDRaceBinds(bool FreeOnly);

private:
	char *m_aapKeyBindings[KeyModifier::COMBINATION_COUNT][KEY_LAST];
	std::vector<CBindSlot> m_vActiveBinds;
};
#endif
