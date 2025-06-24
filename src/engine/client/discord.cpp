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
	bool m_UpdateActivity = false;
	int64_t m_LastActivityUpdate = 0;

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

		// which application to launch when joining activity
		m_pActivityManager->register_command(m_pActivityManager, CONNECTLINK_DOUBLE_SLASH);
		m_pActivityManager->register_steam(m_pActivityManager, 412220); // steam id

		ClearGameInfo();

		return false;
	}

	void Update() override
	{
		// update every 5 seconds, rate limit is 5 updates per 20 seconds
		if(m_UpdateActivity && time_get() > m_LastActivityUpdate + time_freq() * 5)
		{
			m_UpdateActivity = false;
			m_LastActivityUpdate = time_get();

			m_pActivityManager->update_activity(m_pActivityManager, &m_Activity, 0, 0);
		}

		m_pCore->run_callbacks(m_pCore);
	}

	void ClearGameInfo() override
	{
		mem_zero(&m_Activity, sizeof(DiscordActivity));

		str_copy(m_Activity.assets.large_image, "ddnet_logo", sizeof(m_Activity.assets.large_image));
		str_copy(m_Activity.assets.large_text, "DDNet logo", sizeof(m_Activity.assets.large_text));
		m_Activity.timestamps.start = time_timestamp();
		str_copy(m_Activity.details, "Offline", sizeof(m_Activity.details));
		m_Activity.instance = false;

		m_UpdateActivity = true;
	}

	void SetGameInfo(const CServerInfo &ServerInfo, const char *pMapName, bool Registered) override
	{
		mem_zero(&m_Activity, sizeof(DiscordActivity));

		str_copy(m_Activity.assets.large_image, "ddnet_logo", sizeof(m_Activity.assets.large_image));
		str_copy(m_Activity.assets.large_text, "DDNet logo", sizeof(m_Activity.assets.large_text));
		m_Activity.timestamps.start = time_timestamp();
		str_copy(m_Activity.name, "Online", sizeof(m_Activity.name));
		m_Activity.instance = true;

		str_copy(m_Activity.details, ServerInfo.m_aName, sizeof(m_Activity.details));
		str_copy(m_Activity.state, pMapName, sizeof(m_Activity.state));
		m_Activity.party.size.current_size = ServerInfo.m_NumClients;
		m_Activity.party.size.max_size = ServerInfo.m_MaxClients;
		// private makes it so the game isn't public to join, but there's 'Ask to Join' button instead
		m_Activity.party.privacy = Registered ? DiscordActivityPartyPrivacy_Public : DiscordActivityPartyPrivacy_Private;

		if(!Registered)
		{
			// private parties have random id to not leak the server ip
			char aPartyId[sizeof(m_Activity.party.id)];
			secure_random_password(aPartyId, sizeof(aPartyId), 64);
			str_copy(m_Activity.party.id, aPartyId, sizeof(m_Activity.party.id));
		}
		UpdateServerIp(ServerInfo);

		m_UpdateActivity = true;
	}

	void UpdateServerInfo(const CServerInfo &ServerInfo, const char *pMapName) override
	{
		if(!m_Activity.instance)
			return;

		UpdateServerIp(ServerInfo);

		str_copy(m_Activity.details, ServerInfo.m_aName, sizeof(m_Activity.details));
		str_copy(m_Activity.state, pMapName, sizeof(m_Activity.state));
		m_Activity.party.size.max_size = ServerInfo.m_MaxClients;
		m_UpdateActivity = true;
	}

	void UpdatePlayerCount(int Count) override
	{
		if(!m_Activity.instance)
			return;

		if(m_Activity.party.size.current_size == Count)
			return;

		m_Activity.party.size.current_size = Count;
		m_UpdateActivity = true;
	}

	void UpdateServerIp(const CServerInfo &ServerInfo)
	{
		if(!m_Activity.instance)
			return;

		// secret is only shared when player is joining the game, or when he's invited for private games
		if(str_length(ServerInfo.m_aAddress) < (int)sizeof(m_Activity.secrets.join))
		{
			str_copy(m_Activity.secrets.join, ServerInfo.m_aAddress, sizeof(m_Activity.secrets.join));
		}
		else
		{
			char aAddr[NETADDR_MAXSTRSIZE];
			net_addr_str(&ServerInfo.m_aAddresses[0], aAddr, sizeof(aAddr), true);
			str_copy(m_Activity.secrets.join, aAddr, sizeof(m_Activity.secrets.join));
		}

		if(m_Activity.party.privacy == DiscordActivityPartyPrivacy_Public)
		{
			// id is sha256, because it didn't work with the ':' character
			char aPartyId[SHA256_MAXSTRSIZE];
			SHA256_DIGEST PartyIdSha256 = sha256(m_Activity.secrets.join, str_length(m_Activity.secrets.join));
			sha256_str(PartyIdSha256, aPartyId, sizeof(aPartyId));
			str_copy(m_Activity.party.id, aPartyId, sizeof(m_Activity.party.id));
		}
	}

	static void DISCORD_CALLBACK OnActivityJoin(void *pEventData, const char *pSecret)
	{
		CDiscord *pSelf = static_cast<CDiscord *>(pEventData);
		IClient *m_pClient = pSelf->Kernel()->RequestInterface<IClient>();
		m_pClient->Connect(pSecret);
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
	void SetGameInfo(const CServerInfo &ServerInfo, const char *pMapName, bool Registered) override {}
	void UpdateServerInfo(const CServerInfo &ServerInfo, const char *pMapName) override {}
	void UpdatePlayerCount(int Count) override {}
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
