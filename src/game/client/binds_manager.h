#ifndef GAME_CLIENT_BINDS_MANAGER_H
#define GAME_CLIENT_BINDS_MANAGER_H

#include <game/client/components/bindsv2.h>

class CBindsManager : public CComponent
{
	static void ConBind(IConsole::IResult *pResult, void *pUserData);
	static void ConBinds(IConsole::IResult *pResult, void *pUserData);
	static void ConUnbind(IConsole::IResult *pResult, void *pUserData);
	static void ConUnbindAll(IConsole::IResult *pResult, void *pUserData);
	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

public:
	CBindsManager();
	virtual int Sizeof() const override { return sizeof(*this); }

	class CBindsSpecial : public CComponent
	{
	public:
		CBindsManager *m_pBindsManager;
		virtual int Sizeof() const override { return sizeof(*this); }
		virtual bool OnInput(const IInput::CEvent &Event) override;
	};

	CBindsSpecial m_SpecialBinds;

	void OnInit() override;
	void OnConsoleInit() override;
	bool OnInput(const IInput::CEvent &Event) override;

	void SetDefaults();
	void RegisterBindGroup(const char *pName);
	void SetActiveBindGroup(const char *pName);
	std::shared_ptr<CBindsV2> Binds(const char *pGroupName);
	const std::vector<std::shared_ptr<CBindsV2>> &Groups() const { return m_vpBinds; }

	static constexpr const char *GROUP_INGAME = "ingame";
	static constexpr const char *GROUP_MENUS = "menus";
	static constexpr const char *GROUP_DEMO_PLAYER = "demoplayer";
	static constexpr const char *GROUP_EDITOR = "editor";

private:
	std::vector<std::shared_ptr<CBindsV2>> m_vpBinds;
	char m_aActiveBindGroup[MAX_GROUP_NAME_LENGTH];

	friend class CGameClient;

	std::shared_ptr<CBindsV2> CheckGroup(const char *pGroupName);

	void RegisterBindGroups();
};

#endif
