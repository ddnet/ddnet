#include <base/log.h>
#include <base/system.h>
#include <engine/client.h>
#include <engine/discord.h>

#if defined(CONF_DISCORD)
#include <discord_game_sdk.h>

typedef enum EDiscordResult(DISCORD_API *FDiscordCreate)(DiscordVersion, struct DiscordCreateParams *, struct IDiscordCore **);

#if defined(CONF_DISCORD_DYNAMIC)
#include <dlfcn.h>
FDiscordCreate GetDiscordCreate()
{
	void *pSdk = dlopen("discord_game_sdk.so", RTLD_NOW);
	if(!pSdk)
	{
		return nullptr;
	}
	return (FDiscordCreate)dlsym(pSdk, "DiscordCreate");
}
#else
FDiscordCreate GetDiscordCreate()
{
	return DiscordCreate;
}
#endif

class CDiscord : public IDiscord
{
	DiscordActivity m_Activity;

	IDiscordCore *m_pCore;
	IDiscordActivityEvents m_ActivityEvents;
	IDiscordActivityManager *m_pActivityManager;

public:
	bool Init(FDiscordCreate pfnDiscordCreate)
	{
		m_pCore = 0;
		mem_zero(&m_ActivityEvents, sizeof(m_ActivityEvents));

		m_ActivityEvents.on_activity_join = &CDiscord::OnActivityJoin;
		m_pActivityManager = 0;

		DiscordCreateParams Params;
		DiscordCreateParamsSetDefault(&Params);

		Params.client_id = 752165779117441075; // DDNet
		Params.flags = EDiscordCreateFlags::DiscordCreateFlags_NoRequireDiscord;
		Params.event_data = this;
		Params.activity_events = &m_ActivityEvents;

		int Error = pfnDiscordCreate(DISCORD_VERSION, &Params, &m_pCore);
		if(Error != DiscordResult_Ok)
		{
			dbg_msg("discord", "error initializing discord instance, error=%d", Error);
			return true;
		}

		m_pActivityManager = m_pCore->get_activity_manager(m_pCore);

		m_pActivityManager->register_command(m_pActivityManager, CONNECTLINK_DOUBLE_SLASH);
		m_pActivityManager->register_steam(m_pActivityManager, 412220);

		ClearGameInfo();

		return false;
	}

	void Update() override
	{
		m_pCore->run_callbacks(m_pCore);
	}

	void ClearGameInfo() override
	{
		DiscordActivity Activity;
		mem_zero(&Activity, sizeof(DiscordActivity));
		str_copy(Activity.assets.large_image, "ddnet_logo", sizeof(Activity.assets.large_image));
		str_copy(Activity.assets.large_text, "DDNet logo", sizeof(Activity.assets.large_text));
		Activity.timestamps.start = time_timestamp();
		str_copy(Activity.details, "Offline", sizeof(Activity.details));
		Activity.instance = false;
		m_Activity = Activity;
		m_pActivityManager->update_activity(m_pActivityManager, &Activity, 0, 0);
	}

	void SetGameInfo(const NETADDR &ServerAddr, const CServerInfo &ServerInfo, bool Registered) override
	{
		DiscordActivity Activity;
		mem_zero(&Activity, sizeof(DiscordActivity));
		str_copy(Activity.assets.large_image, "ddnet_logo", sizeof(Activity.assets.large_image));
		str_copy(Activity.assets.large_text, "DDNet logo", sizeof(Activity.assets.large_text));
		Activity.timestamps.start = time_timestamp();
		str_copy(Activity.name, "Online", sizeof(Activity.name));
		str_copy(Activity.details, ServerInfo.m_aName, sizeof(Activity.details));
		str_copy(Activity.state, ServerInfo.m_aMap, sizeof(Activity.state));

		char aAddr[NETADDR_MAXSTRSIZE];
		net_addr_str(&ServerAddr, aAddr, sizeof(aAddr), true);

		char aBuf[128];
		if(Registered)
			str_base64(aBuf, sizeof(aBuf), aAddr, str_length(aAddr));
		else
			secure_random_password(aBuf, sizeof(aBuf), 16);

		str_copy(Activity.party.id, aBuf, sizeof(Activity.party.id));
		Activity.party.size.current_size = ServerInfo.m_NumClients;
		Activity.party.size.max_size = ServerInfo.m_MaxClients;
		Activity.party.privacy = Registered ? DiscordActivityPartyPrivacy_Public : DiscordActivityPartyPrivacy_Private;
		Activity.instance = true;

		str_copy(Activity.secrets.join, aAddr, sizeof(Activity.secrets.join));
		m_Activity = Activity;
		m_pActivityManager->update_activity(m_pActivityManager, &Activity, 0, 0);
	}

	void UpdatePlayerCount(int Count)
	{
		DiscordActivity Activity;
		mem_zero(&Activity, sizeof(DiscordActivity));
		Activity = m_Activity;
		if(!Activity.instance)
			return;

		Activity.party.size.current_size = Count;
		m_pActivityManager->update_activity(m_pActivityManager, &Activity, 0, 0);
	}

	void UpdateServerInfo(const CServerInfo &ServerInfo)
	{
		DiscordActivity Activity;
		mem_zero(&Activity, sizeof(DiscordActivity));
		Activity = m_Activity;
		if(!Activity.instance)
			return;

		str_copy(Activity.details, ServerInfo.m_aName, sizeof(Activity.details));
		str_copy(Activity.state, ServerInfo.m_aMap, sizeof(Activity.state));
		Activity.party.size.max_size = ServerInfo.m_MaxClients;
		m_pActivityManager->update_activity(m_pActivityManager, &Activity, 0, 0);
	}

	static void OnActivityJoin(void *pEventData, const char *Secret)
	{
		log_debug("discord", "on activity join with secret %s", Secret);
		CDiscord *pSelf = (CDiscord *)pEventData;
		IClient *m_pClient = pSelf->Kernel()->RequestInterface<IClient>();
		m_pClient->Connect(Secret);
	}
};

IDiscord *CreateDiscordImpl()
{
	FDiscordCreate pfnDiscordCreate = GetDiscordCreate();
	if(!pfnDiscordCreate)
	{
		return 0;
	}
	CDiscord *pDiscord = new CDiscord();
	if(pDiscord->Init(pfnDiscordCreate))
	{
		delete pDiscord;
		return 0;
	}
	return pDiscord;
}
#else
IDiscord *CreateDiscordImpl()
{
	return nullptr;
}
#endif

class CDiscordStub : public IDiscord
{
	void Update() override {}
	void ClearGameInfo() override {}
	void SetGameInfo(const NETADDR &ServerAddr, const CServerInfo &ServerInfo, bool Registered) override {}
	void UpdatePlayerCount(int Count) override {}
	void UpdateServerInfo(const CServerInfo &ServerInfo) override {}
};

IDiscord *CreateDiscord()
{
	IDiscord *pDiscord = CreateDiscordImpl();
	if(pDiscord)
	{
		return pDiscord;
	}
	return new CDiscordStub();
}
