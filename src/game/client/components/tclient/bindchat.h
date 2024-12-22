
#ifndef GAME_CLIENT_COMPONENTS_BINDCHAT_H
#define GAME_CLIENT_COMPONENTS_BINDCHAT_H
#include <game/client/component.h>
class IConfigManager;

enum
{
	BINDCHAT_MAX_NAME = 64,
	BINDCHAT_MAX_CMD = 1024,
	BINDCHAT_MAX_BINDS = 256,
};

class CBindchat : public CComponent
{
	static void ConAddBindchat(IConsole::IResult *pResult, void *pUserData);
	static void ConBindchats(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveBindchat(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveBindchatAll(IConsole::IResult *pResult, void *pUserData);
	static void ConBindchatDefaults(IConsole::IResult *pResult, void *pUserData);

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

	void ExecuteBind(int Bind, const char *pArgs);

public:
	class CBind
	{
	public:
		char m_aName[BINDCHAT_MAX_NAME];
		char m_aCommand[BINDCHAT_MAX_CMD];

		bool operator==(const CBind &Other) const
		{
			return str_comp(m_aName, Other.m_aName) == 0 && str_comp(m_aCommand, Other.m_aCommand) == 0;
		}
	};

	std::vector<CBind> m_vBinds;

	CBindchat();
	virtual int Sizeof() const override { return sizeof(*this); }

	virtual void OnConsoleInit() override;

	void AddBind(const char *pName, const char *pCommand);
	void RemoveBind(const char *pName);
	void RemoveBind(int Index);
	void RemoveAllBinds();

	int GetBind(const char *pCommand);
	CBind *Get(int Index);

	bool ChatDoBinds(const char *pText);
	bool ChatDoAutocomplete(bool ShiftPressed);
};

#endif
