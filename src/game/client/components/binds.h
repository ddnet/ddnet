/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_BINDS_H
#define GAME_CLIENT_COMPONENTS_BINDS_H

#include <engine/console.h>
#include <engine/keys.h>

#include <game/client/component.h>

#include <map>
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

public:
	CBinds();
	~CBinds();
	virtual int Sizeof() const override { return sizeof(*this); }

	class CBindSlot
	{
	public:
		int m_Key;
		int m_ModifierMask;
		CBindSlot(int KeyId, int ModifierMask);

		bool operator==(const CBindSlot &other) const
		{
			return m_Key == other.m_Key && m_ModifierMask == other.m_ModifierMask;
		}
		bool operator<(const CBindSlot &other) const
		{
			if(m_ModifierMask != other.m_ModifierMask)
				return m_ModifierMask < other.m_ModifierMask;
			return m_Key < other.m_Key;
		}
	};

	class CBindsSpecial : public CComponent
	{
	public:
		CBinds *m_pBinds;
		virtual int Sizeof() const override { return sizeof(*this); }
		virtual bool OnInput(const IInput::CEvent &Event) override;
	};

	CBindSlot GetBindSlot(const char *pBindString) const;

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
	char *Get(CBindSlot BindSlot);
	inline char *Get(int KeyId, int ModifierMask) { return Get(CBindSlot(KeyId, ModifierMask)); }
	void GetKey(const char *pBindStr, char *pBuf, size_t BufSize);
	size_t GetName(int KeyId, int ModifierMask, char *pBuf, size_t BufSize);
	static int GetModifierMask(IInput *pInput);
	static int GetModifierMaskOfKey(int Key);
	static const char *GetModifierName(int Modifier);
	static size_t GetKeyBindModifiersName(int ModifierCombination, char *pBuf, size_t BufSize);

	virtual void OnConsoleInit() override;
	virtual bool OnInput(const IInput::CEvent &Event) override;

	// DDRace
	void SetDDRaceBinds(bool FreeOnly);

private:
	// struct CBindSlotHash // for unordered_map
	// {
	// 	std::size_t operator()(const CBindSlot &bindSlot) const
	// 	{
	// 		return std::hash<int>()(bindSlot.m_Key) ^ (std::hash<int>()(bindSlot.m_ModifierMask) << 1);
	// 	}
	// };
	std::map<CBindSlot, char *> m_Binds;
	std::vector<CBindSlot> m_vActiveBinds;
};
#endif
