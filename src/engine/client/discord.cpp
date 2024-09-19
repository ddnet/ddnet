#include <base/system.h>
#include <engine/discord.h>

#if defined(CONF_DISCORD)
#include <discord_game_sdk.h>

typedef enum EDiscordResult DISCORD_API (*FDiscordCreate)(DiscordVersion, struct DiscordCreateParams *, struct IDiscordCore **);

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
	IDiscordCore *m_pCore;
	IDiscordActivityEvents m_ActivityEvents;
	IDiscordActivityManager *m_pActivityManager;

public:
	bool Init(FDiscordCreate pfnDiscordCreate)
	{
		m_pCore = 0;
		mem_zero(&m_ActivityEvents, sizeof(m_ActivityEvents));
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
		m_pActivityManager->update_activity(m_pActivityManager, &Activity, 0, 0);
	}
	void SetGameInfo(const NETADDR &ServerAddr, const char *pMapName, bool AnnounceAddr) override
	{
		DiscordActivity Activity;
		mem_zero(&Activity, sizeof(DiscordActivity));
		str_copy(Activity.assets.large_image, "ddnet_logo", sizeof(Activity.assets.large_image));
		str_copy(Activity.assets.large_text, "DDNet logo", sizeof(Activity.assets.large_text));
		Activity.timestamps.start = time_timestamp();
		str_copy(Activity.details, "Online", sizeof(Activity.details));
		str_copy(Activity.state, pMapName, sizeof(Activity.state));
		m_pActivityManager->update_activity(m_pActivityManager, &Activity, 0, 0);
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
	return 0;
}
#endif

class CDiscordStub : public IDiscord
{
	void Update() override {}
	void ClearGameInfo() override {}
	void SetGameInfo(const NETADDR &ServerAddr, const char *pMapName, bool AnnounceAddr) override {}
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
