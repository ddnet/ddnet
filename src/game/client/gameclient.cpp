/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <chrono>
#include <limits>

#include <engine/client/checksum.h>
#include <engine/demo.h>
#include <engine/editor.h>
#include <engine/engine.h>
#include <engine/favorites.h>
#include <engine/friends.h>
#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/sound.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/updater.h>

#include <game/generated/client_data.h>
#include <game/generated/client_data7.h>
#include <game/generated/protocol.h>

#include <base/math.h>
#include <base/system.h>
#include <base/vmath.h>

#include "gameclient.h"
#include "lineinput.h"
#include "race.h"
#include "render.h"

#include <game/localization.h>
#include <game/mapitems.h>
#include <game/version.h>

#include "components/background.h"
#include "components/binds.h"
#include "components/broadcast.h"
#include "components/camera.h"
#include "components/chat.h"
#include "components/console.h"
#include "components/controls.h"
#include "components/countryflags.h"
#include "components/damageind.h"
#include "components/debughud.h"
#include "components/effects.h"
#include "components/emoticon.h"
#include "components/freezebars.h"
#include "components/ghost.h"
#include "components/hud.h"
#include "components/infomessages.h"
#include "components/items.h"
#include "components/mapimages.h"
#include "components/maplayers.h"
#include "components/mapsounds.h"
#include "components/menu_background.h"
#include "components/menus.h"
#include "components/motd.h"
#include "components/nameplates.h"
#include "components/particles.h"
#include "components/players.h"
#include "components/race_demo.h"
#include "components/scoreboard.h"
#include "components/skins.h"
#include "components/sounds.h"
#include "components/spectator.h"
#include "components/statboard.h"
#include "components/voting.h"
#include "prediction/entities/character.h"
#include "prediction/entities/projectile.h"

using namespace std::chrono_literals;

const char *CGameClient::Version() const { return GAME_VERSION; }
const char *CGameClient::NetVersion() const { return GAME_NETVERSION; }
int CGameClient::DDNetVersion() const { return DDNET_VERSION_NUMBER; }
const char *CGameClient::DDNetVersionStr() const { return m_aDDNetVersionStr; }
const char *CGameClient::GetItemName(int Type) const { return m_NetObjHandler.GetObjName(Type); }

void CGameClient::OnConsoleInit()
{
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pTextRender = Kernel()->RequestInterface<ITextRender>();
	m_pSound = Kernel()->RequestInterface<ISound>();
	m_pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	m_pConfig = m_pConfigManager->Values();
	m_pInput = Kernel()->RequestInterface<IInput>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pDemoPlayer = Kernel()->RequestInterface<IDemoPlayer>();
	m_pServerBrowser = Kernel()->RequestInterface<IServerBrowser>();
	m_pEditor = Kernel()->RequestInterface<IEditor>();
	m_pFavorites = Kernel()->RequestInterface<IFavorites>();
	m_pFriends = Kernel()->RequestInterface<IFriends>();
	m_pFoes = Client()->Foes();
#if defined(CONF_AUTOUPDATE)
	m_pUpdater = Kernel()->RequestInterface<IUpdater>();
#endif
	m_pHttp = Kernel()->RequestInterface<IHttp>();

	m_Menus.SetMenuBackground(&m_MenuBackground);

	m_NamePlates.SetPlayers(&m_Players);

	// make a list of all the systems, make sure to add them in the correct render order
	m_vpAll.insert(m_vpAll.end(), {&m_Skins,
					      &m_CountryFlags,
					      &m_MapImages,
					      &m_Effects, // doesn't render anything, just updates effects
					      &m_Binds,
					      &m_Binds.m_SpecialBinds,
					      &m_Controls,
					      &m_Camera,
					      &m_Sounds,
					      &m_Voting,
					      &m_Particles, // doesn't render anything, just updates all the particles
					      &m_RaceDemo,
					      &m_MapSounds,
					      &m_Background, // render instead of m_MapLayersBackground when g_Config.m_ClOverlayEntities == 100
					      &m_MapLayersBackground, // first to render
					      &m_Particles.m_RenderTrail,
					      &m_Items,
					      &m_Ghost,
					      &m_Players,
					      &m_MapLayersForeground,
					      &m_Particles.m_RenderExplosions,
					      &m_NamePlates,
					      &m_Particles.m_RenderExtra,
					      &m_Particles.m_RenderGeneral,
					      &m_FreezeBars,
					      &m_DamageInd,
					      &m_Hud,
					      &m_Spectator,
					      &m_Emoticon,
					      &m_InfoMessages,
					      &m_Chat,
					      &m_Broadcast,
					      &m_DebugHud,
					      &m_Scoreboard,
					      &m_Statboard,
					      &m_Motd,
					      &m_Menus,
					      &m_Tooltips,
					      &CMenus::m_Binder,
					      &m_GameConsole,
					      &m_MenuBackground});

	// build the input stack
	m_vpInput.insert(m_vpInput.end(), {&CMenus::m_Binder, // this will take over all input when we want to bind a key
						  &m_Binds.m_SpecialBinds,
						  &m_GameConsole,
						  &m_Chat, // chat has higher prio, due to that you can quit it by pressing esc
						  &m_Motd, // for pressing esc to remove it
						  &m_Menus,
						  &m_Spectator,
						  &m_Emoticon,
						  &m_Controls,
						  &m_Binds});

	// add the some console commands
	Console()->Register("team", "i[team-id]", CFGFLAG_CLIENT, ConTeam, this, "Switch team");
	Console()->Register("kill", "", CFGFLAG_CLIENT, ConKill, this, "Kill yourself to restart");

	// register server dummy commands for tab completion
	Console()->Register("tune", "s[tuning] ?f[value]", CFGFLAG_SERVER, 0, 0, "Tune variable to value or show current value");
	Console()->Register("tune_reset", "?s[tuning]", CFGFLAG_SERVER, 0, 0, "Reset all or one tuning variable to default");
	Console()->Register("tunes", "", CFGFLAG_SERVER, 0, 0, "List all tuning variables and their values");
	Console()->Register("change_map", "?r[map]", CFGFLAG_SERVER, 0, 0, "Change map");
	Console()->Register("restart", "?i[seconds]", CFGFLAG_SERVER, 0, 0, "Restart in x seconds");
	Console()->Register("broadcast", "r[message]", CFGFLAG_SERVER, 0, 0, "Broadcast message");
	Console()->Register("say", "r[message]", CFGFLAG_SERVER, 0, 0, "Say in chat");
	Console()->Register("set_team", "i[id] i[team-id] ?i[delay in minutes]", CFGFLAG_SERVER, 0, 0, "Set team of player to team");
	Console()->Register("set_team_all", "i[team-id]", CFGFLAG_SERVER, 0, 0, "Set team of all players to team");
	Console()->Register("add_vote", "s[name] r[command]", CFGFLAG_SERVER, 0, 0, "Add a voting option");
	Console()->Register("remove_vote", "s[name]", CFGFLAG_SERVER, 0, 0, "remove a voting option");
	Console()->Register("force_vote", "s[name] s[command] ?r[reason]", CFGFLAG_SERVER, 0, 0, "Force a voting option");
	Console()->Register("clear_votes", "", CFGFLAG_SERVER, 0, 0, "Clears the voting options");
	Console()->Register("add_map_votes", "", CFGFLAG_SERVER, 0, 0, "Automatically adds voting options for all maps");
	Console()->Register("vote", "r['yes'|'no']", CFGFLAG_SERVER, 0, 0, "Force a vote to yes/no");
	Console()->Register("swap_teams", "", CFGFLAG_SERVER, 0, 0, "Swap the current teams");
	Console()->Register("shuffle_teams", "", CFGFLAG_SERVER, 0, 0, "Shuffle the current teams");

	// register tune zone command to allow the client prediction to load tunezones from the map
	Console()->Register("tune_zone", "i[zone] s[tuning] f[value]", CFGFLAG_CLIENT | CFGFLAG_GAME, ConTuneZone, this, "Tune in zone a variable to value");

	for(auto &pComponent : m_vpAll)
		pComponent->m_pClient = this;

	// let all the other components register their console commands
	for(auto &pComponent : m_vpAll)
		pComponent->OnConsoleInit();

	Console()->Chain("cl_languagefile", ConchainLanguageUpdate, this);

	Console()->Chain("player_name", ConchainSpecialInfoupdate, this);
	Console()->Chain("player_clan", ConchainSpecialInfoupdate, this);
	Console()->Chain("player_country", ConchainSpecialInfoupdate, this);
	Console()->Chain("player_use_custom_color", ConchainSpecialInfoupdate, this);
	Console()->Chain("player_color_body", ConchainSpecialInfoupdate, this);
	Console()->Chain("player_color_feet", ConchainSpecialInfoupdate, this);
	Console()->Chain("player_skin", ConchainSpecialInfoupdate, this);

	Console()->Chain("dummy_name", ConchainSpecialDummyInfoupdate, this);
	Console()->Chain("dummy_clan", ConchainSpecialDummyInfoupdate, this);
	Console()->Chain("dummy_country", ConchainSpecialDummyInfoupdate, this);
	Console()->Chain("dummy_use_custom_color", ConchainSpecialDummyInfoupdate, this);
	Console()->Chain("dummy_color_body", ConchainSpecialDummyInfoupdate, this);
	Console()->Chain("dummy_color_feet", ConchainSpecialDummyInfoupdate, this);
	Console()->Chain("dummy_skin", ConchainSpecialDummyInfoupdate, this);

	Console()->Chain("cl_skin_download_url", ConchainRefreshSkins, this);
	Console()->Chain("cl_skin_community_download_url", ConchainRefreshSkins, this);
	Console()->Chain("cl_download_skins", ConchainRefreshSkins, this);
	Console()->Chain("cl_download_community_skins", ConchainRefreshSkins, this);
	Console()->Chain("cl_vanilla_skins_only", ConchainRefreshSkins, this);

	Console()->Chain("cl_dummy", ConchainSpecialDummy, this);
	Console()->Chain("cl_text_entities_size", ConchainClTextEntitiesSize, this);

	Console()->Chain("cl_menu_map", ConchainMenuMap, this);

	//
	m_SuppressEvents = false;
}

void CGameClient::OnInit()
{
	Client()->SetMapLoadingCBFunc([this]() {
		m_Menus.RenderLoading(DemoPlayer()->IsPlaying() ? Localize("Preparing demo playback") : Localize("Connected"), Localize("Loading map file from storage"), 0, false);
	});

	m_pGraphics = Kernel()->RequestInterface<IGraphics>();

	// propagate pointers
	m_UI.Init(Kernel());
	m_RenderTools.Init(Graphics(), TextRender());

	int64_t Start = time_get();

	if(GIT_SHORTREV_HASH)
	{
		str_format(m_aDDNetVersionStr, sizeof(m_aDDNetVersionStr), "%s %s (%s)", GAME_NAME, GAME_RELEASE_VERSION, GIT_SHORTREV_HASH);
	}
	else
	{
		str_format(m_aDDNetVersionStr, sizeof(m_aDDNetVersionStr), "%s %s", GAME_NAME, GAME_RELEASE_VERSION);
	}

	// set the language
	g_Localization.LoadIndexfile(Storage(), Console());
	if(g_Config.m_ClShowWelcome)
		g_Localization.SelectDefaultLanguage(Console(), g_Config.m_ClLanguagefile, sizeof(g_Config.m_ClLanguagefile));
	g_Localization.Load(g_Config.m_ClLanguagefile, Storage(), Console());

	// TODO: this should be different
	// setup item sizes
	for(int i = 0; i < NUM_NETOBJTYPES; i++)
		Client()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	TextRender()->LoadFonts();
	TextRender()->SetFontLanguageVariant(g_Config.m_ClLanguagefile);

	// update and swap after font loading, they are quite huge
	Client()->UpdateAndSwap();

	const char *pLoadingDDNetCaption = Localize("Loading DDNet Client");

	// init all components
	int SkippedComps = 0;
	int CompCounter = 0;
	for(int i = m_vpAll.size() - 1; i >= 0; --i)
	{
		m_vpAll[i]->OnInit();
		// try to render a frame after each component, also flushes GPU uploads
		if(m_Menus.IsInit())
		{
			char aBuff[256];
			str_format(aBuff, std::size(aBuff), "%s [%d/%d]", CompCounter == 40 ? Localize("Why are you slowmo replaying to read this?") : Localize("Initializing components"), (CompCounter + 1), (int)ComponentCount());
			m_Menus.RenderLoading(pLoadingDDNetCaption, aBuff, 1 + SkippedComps);
			SkippedComps = 0;
		}
		else
		{
			++SkippedComps;
		}
		++CompCounter;
	}

	char aBuf[256];

	m_GameSkinLoaded = false;
	m_ParticlesSkinLoaded = false;
	m_EmoticonsSkinLoaded = false;
	m_HudSkinLoaded = false;

	// setup load amount, load textures
	for(int i = 0; i < g_pData->m_NumImages; i++)
	{
		if(i == IMAGE_GAME)
			LoadGameSkin(g_Config.m_ClAssetGame);
		else if(i == IMAGE_EMOTICONS)
			LoadEmoticonsSkin(g_Config.m_ClAssetEmoticons);
		else if(i == IMAGE_PARTICLES)
			LoadParticlesSkin(g_Config.m_ClAssetParticles);
		else if(i == IMAGE_HUD)
			LoadHudSkin(g_Config.m_ClAssetHud);
		else if(i == IMAGE_EXTRAS)
			LoadExtrasSkin(g_Config.m_ClAssetExtras);
		else if(g_pData->m_aImages[i].m_pFilename[0] == '\0') // handle special null image without filename
			g_pData->m_aImages[i].m_Id = IGraphics::CTextureHandle();
		else
			g_pData->m_aImages[i].m_Id = Graphics()->LoadTexture(g_pData->m_aImages[i].m_pFilename, IStorage::TYPE_ALL);
		m_Menus.RenderLoading(pLoadingDDNetCaption, Localize("Initializing assets"), 1);
	}

	for(auto &pComponent : m_vpAll)
		pComponent->OnReset();

	m_ServerMode = SERVERMODE_PURE;

	m_aDDRaceMsgSent[0] = false;
	m_aDDRaceMsgSent[1] = false;
	m_aShowOthers[0] = SHOW_OTHERS_NOT_SET;
	m_aShowOthers[1] = SHOW_OTHERS_NOT_SET;
	m_aSwitchStateTeam[0] = -1;
	m_aSwitchStateTeam[1] = -1;

	m_LastZoom = .0;
	m_LastScreenAspect = .0;
	m_LastDummyConnected = false;

	// Set free binds to DDRace binds if it's active
	m_Binds.SetDDRaceBinds(true);

	if(g_Config.m_ClTimeoutCode[0] == '\0' || str_comp(g_Config.m_ClTimeoutCode, "hGuEYnfxicsXGwFq") == 0)
	{
		for(unsigned int i = 0; i < 16; i++)
		{
			if(rand() % 2)
				g_Config.m_ClTimeoutCode[i] = (char)((rand() % 26) + 97);
			else
				g_Config.m_ClTimeoutCode[i] = (char)((rand() % 26) + 65);
		}
	}

	if(g_Config.m_ClDummyTimeoutCode[0] == '\0' || str_comp(g_Config.m_ClDummyTimeoutCode, "hGuEYnfxicsXGwFq") == 0)
	{
		for(unsigned int i = 0; i < 16; i++)
		{
			if(rand() % 2)
				g_Config.m_ClDummyTimeoutCode[i] = (char)((rand() % 26) + 97);
			else
				g_Config.m_ClDummyTimeoutCode[i] = (char)((rand() % 26) + 65);
		}
	}

	int64_t End = time_get();
	str_format(aBuf, sizeof(aBuf), "initialisation finished after %.2fms", ((End - Start) * 1000) / (float)time_freq());
	Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "gameclient", aBuf);

	m_GameWorld.m_pCollision = Collision();
	m_GameWorld.m_pTuningList = m_aTuningList;

	m_MapImages.SetTextureScale(g_Config.m_ClTextEntitiesSize);

	// Aggressively try to grab window again since some Windows users report
	// window not being focused after starting client.
	Graphics()->SetWindowGrab(true);

	CChecksumData *pChecksum = Client()->ChecksumData();
	pChecksum->m_SizeofGameClient = sizeof(*this);
	pChecksum->m_NumComponents = m_vpAll.size();
	for(size_t i = 0; i < m_vpAll.size(); i++)
	{
		if(i >= std::size(pChecksum->m_aComponentsChecksum))
		{
			break;
		}
		int Size = m_vpAll[i]->Sizeof();
		pChecksum->m_aComponentsChecksum[i] = Size;
	}
}

void CGameClient::OnUpdate()
{
	HandleLanguageChanged();

	CUIElementBase::Init(UI()); // update static pointer because game and editor use separate UI

	// handle mouse movement
	float x = 0.0f, y = 0.0f;
	IInput::ECursorType CursorType = Input()->CursorRelative(&x, &y);
	if(CursorType != IInput::CURSOR_NONE)
	{
		for(auto &pComponent : m_vpInput)
		{
			if(pComponent->OnCursorMove(x, y, CursorType))
				break;
		}
	}

	// handle key presses
	for(size_t i = 0; i < Input()->NumEvents(); i++)
	{
		const IInput::CEvent &Event = Input()->GetEvent(i);
		if(!Input()->IsEventValid(Event))
			continue;

		for(auto &pComponent : m_vpInput)
		{
			if(pComponent->OnInput(Event))
				break;
		}
	}

	if(g_Config.m_ClSubTickAiming && m_Binds.m_MouseOnAction)
	{
		m_Controls.m_aMousePosOnAction[g_Config.m_ClDummy] = m_Controls.m_aMousePos[g_Config.m_ClDummy];
		m_Binds.m_MouseOnAction = false;
	}
}

void CGameClient::OnDummySwap()
{
	if(g_Config.m_ClDummyResetOnSwitch)
	{
		int PlayerOrDummy = (g_Config.m_ClDummyResetOnSwitch == 2) ? g_Config.m_ClDummy : (!g_Config.m_ClDummy);
		m_Controls.ResetInput(PlayerOrDummy);
		m_Controls.m_aInputData[PlayerOrDummy].m_Hook = 0;
	}
	int tmp = m_DummyInput.m_Fire;
	m_DummyInput = m_Controls.m_aInputData[!g_Config.m_ClDummy];
	m_Controls.m_aInputData[g_Config.m_ClDummy].m_Fire = tmp;
	m_IsDummySwapping = 1;
}

int CGameClient::OnSnapInput(int *pData, bool Dummy, bool Force)
{
	if(!Dummy)
	{
		return m_Controls.SnapInput(pData);
	}

	if(!g_Config.m_ClDummyHammer)
	{
		if(m_DummyFire != 0)
		{
			m_DummyInput.m_Fire = (m_HammerInput.m_Fire + 1) & ~1;
			m_DummyFire = 0;
		}

		if(!Force && (!m_DummyInput.m_Direction && !m_DummyInput.m_Jump && !m_DummyInput.m_Hook))
		{
			return 0;
		}

		mem_copy(pData, &m_DummyInput, sizeof(m_DummyInput));
		return sizeof(m_DummyInput);
	}
	else
	{
		if(m_DummyFire % 25 != 0)
		{
			m_DummyFire++;
			return 0;
		}
		m_DummyFire++;

		m_HammerInput.m_Fire = (m_HammerInput.m_Fire + 1) | 1;
		m_HammerInput.m_WantedWeapon = WEAPON_HAMMER + 1;
		if(!g_Config.m_ClDummyRestoreWeapon)
		{
			m_DummyInput.m_WantedWeapon = WEAPON_HAMMER + 1;
		}

		vec2 MainPos = m_LocalCharacterPos;
		vec2 DummyPos = m_aClients[m_aLocalIDs[!g_Config.m_ClDummy]].m_Predicted.m_Pos;
		vec2 Dir = MainPos - DummyPos;
		m_HammerInput.m_TargetX = (int)(Dir.x);
		m_HammerInput.m_TargetY = (int)(Dir.y);

		mem_copy(pData, &m_HammerInput, sizeof(m_HammerInput));
		return sizeof(m_HammerInput);
	}
}

void CGameClient::OnConnected()
{
	const char *pConnectCaption = DemoPlayer()->IsPlaying() ? Localize("Preparing demo playback") : Localize("Connected");
	const char *pLoadMapContent = Localize("Initializing map logic");
	// render loading before skip is calculated
	m_Menus.RenderLoading(pConnectCaption, pLoadMapContent, 0, false);
	m_Layers.Init(Kernel());
	m_Collision.Init(Layers());
	m_GameWorld.m_Core.InitSwitchers(m_Collision.m_HighestSwitchNumber);

	CRaceHelper::ms_aFlagIndex[0] = -1;
	CRaceHelper::ms_aFlagIndex[1] = -1;

	CTile *pGameTiles = static_cast<CTile *>(Layers()->Map()->GetData(Layers()->GameLayer()->m_Data));

	// get flag positions
	for(int i = 0; i < m_Collision.GetWidth() * m_Collision.GetHeight(); i++)
	{
		if(pGameTiles[i].m_Index - ENTITY_OFFSET == ENTITY_FLAGSTAND_RED)
			CRaceHelper::ms_aFlagIndex[TEAM_RED] = i;
		else if(pGameTiles[i].m_Index - ENTITY_OFFSET == ENTITY_FLAGSTAND_BLUE)
			CRaceHelper::ms_aFlagIndex[TEAM_BLUE] = i;
		i += pGameTiles[i].m_Skip;
	}

	// render loading before going through all components
	m_Menus.RenderLoading(pConnectCaption, pLoadMapContent, 0, false);
	for(auto &pComponent : m_vpAll)
	{
		pComponent->OnMapLoad();
		pComponent->OnReset();
	}

	Client()->SetLoadingStateDetail(IClient::LOADING_STATE_DETAIL_GETTING_READY);
	m_Menus.RenderLoading(pConnectCaption, Localize("Sending initial client info"), 0, false);

	m_ServerMode = SERVERMODE_PURE;

	// send the initial info
	SendInfo(true);
	// we should keep this in for now, because otherwise you can't spectate
	// people at start as the other info 64 packet is only sent after the first
	// snap
	Client()->Rcon("crashmeplx");

	m_GameWorld.Clear();
	m_GameWorld.m_WorldConfig.m_InfiniteAmmo = true;
	mem_zero(&m_GameInfo, sizeof(m_GameInfo));
	m_PredictedDummyID = -1;
	ConfigManager()->ResetGameSettings();
	LoadMapSettings();

	if(Client()->State() != IClient::STATE_DEMOPLAYBACK && g_Config.m_ClAutoDemoOnConnect)
		Client()->DemoRecorder_HandleAutoStart();
}

void CGameClient::OnReset()
{
	m_aLastNewPredictedTick[0] = -1;
	m_aLastNewPredictedTick[1] = -1;

	m_aLocalTuneZone[0] = 0;
	m_aLocalTuneZone[1] = 0;

	m_aExpectingTuningForZone[0] = -1;
	m_aExpectingTuningForZone[1] = -1;

	m_aReceivedTuning[0] = false;
	m_aReceivedTuning[1] = false;

	InvalidateSnapshot();

	for(auto &Client : m_aClients)
		Client.Reset();

	for(auto &pComponent : m_vpAll)
		pComponent->OnReset();

	m_DemoSpecID = SPEC_FOLLOW;
	m_aFlagDropTick[TEAM_RED] = 0;
	m_aFlagDropTick[TEAM_BLUE] = 0;
	m_LastRoundStartTick = -1;
	m_LastFlagCarrierRed = -4;
	m_LastFlagCarrierBlue = -4;
	m_aTuning[g_Config.m_ClDummy] = CTuningParams();

	m_Teams.Reset();
	m_aDDRaceMsgSent[0] = false;
	m_aDDRaceMsgSent[1] = false;
	m_aShowOthers[0] = SHOW_OTHERS_NOT_SET;
	m_aShowOthers[1] = SHOW_OTHERS_NOT_SET;

	m_LastZoom = .0;
	m_LastScreenAspect = .0;
	m_LastDummyConnected = false;

	m_ReceivedDDNetPlayer = false;

	Editor()->ResetMentions();
	Editor()->ResetIngameMoved();
}

void CGameClient::UpdatePositions()
{
	// local character position
	if(g_Config.m_ClPredict && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(!AntiPingPlayers())
		{
			if(!m_Snap.m_pLocalCharacter || (m_Snap.m_pGameInfoObj && m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
			{
				// don't use predicted
			}
			else
				m_LocalCharacterPos = mix(m_PredictedPrevChar.m_Pos, m_PredictedChar.m_Pos, Client()->PredIntraGameTick(g_Config.m_ClDummy));
		}
		else
		{
			if(!(m_Snap.m_pGameInfoObj && m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
			{
				if(m_Snap.m_pLocalCharacter)
					m_LocalCharacterPos = mix(m_PredictedPrevChar.m_Pos, m_PredictedChar.m_Pos, Client()->PredIntraGameTick(g_Config.m_ClDummy));
			}
			//		else
			//			m_LocalCharacterPos = mix(m_PredictedPrevChar.m_Pos, m_PredictedChar.m_Pos, Client()->PredIntraGameTick(g_Config.m_ClDummy));
		}
	}
	else if(m_Snap.m_pLocalCharacter && m_Snap.m_pLocalPrevCharacter)
	{
		m_LocalCharacterPos = mix(
			vec2(m_Snap.m_pLocalPrevCharacter->m_X, m_Snap.m_pLocalPrevCharacter->m_Y),
			vec2(m_Snap.m_pLocalCharacter->m_X, m_Snap.m_pLocalCharacter->m_Y), Client()->IntraGameTick(g_Config.m_ClDummy));
	}

	// spectator position
	if(m_Snap.m_SpecInfo.m_Active)
	{
		if(m_MultiViewActivated)
		{
			HandleMultiView();
		}
		else if(Client()->State() == IClient::STATE_DEMOPLAYBACK && m_DemoSpecID != SPEC_FOLLOW && m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW)
		{
			m_Snap.m_SpecInfo.m_Position = mix(
				vec2(m_Snap.m_aCharacters[m_Snap.m_SpecInfo.m_SpectatorID].m_Prev.m_X, m_Snap.m_aCharacters[m_Snap.m_SpecInfo.m_SpectatorID].m_Prev.m_Y),
				vec2(m_Snap.m_aCharacters[m_Snap.m_SpecInfo.m_SpectatorID].m_Cur.m_X, m_Snap.m_aCharacters[m_Snap.m_SpecInfo.m_SpectatorID].m_Cur.m_Y),
				Client()->IntraGameTick(g_Config.m_ClDummy));
			m_Snap.m_SpecInfo.m_UsePosition = true;
		}
		else if(m_Snap.m_pSpectatorInfo && ((Client()->State() == IClient::STATE_DEMOPLAYBACK && m_DemoSpecID == SPEC_FOLLOW) || (Client()->State() != IClient::STATE_DEMOPLAYBACK && m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW)))
		{
			if(m_Snap.m_pPrevSpectatorInfo && m_Snap.m_pPrevSpectatorInfo->m_SpectatorID == m_Snap.m_pSpectatorInfo->m_SpectatorID)
				m_Snap.m_SpecInfo.m_Position = mix(vec2(m_Snap.m_pPrevSpectatorInfo->m_X, m_Snap.m_pPrevSpectatorInfo->m_Y),
					vec2(m_Snap.m_pSpectatorInfo->m_X, m_Snap.m_pSpectatorInfo->m_Y), Client()->IntraGameTick(g_Config.m_ClDummy));
			else
				m_Snap.m_SpecInfo.m_Position = vec2(m_Snap.m_pSpectatorInfo->m_X, m_Snap.m_pSpectatorInfo->m_Y);
			m_Snap.m_SpecInfo.m_UsePosition = true;
		}
	}

	if(!m_MultiViewActivated && m_MultiView.m_IsInit)
		ResetMultiView();

	UpdateRenderedCharacters();
}

void CGameClient::OnRender()
{
	// check if multi view got activated
	if(!m_MultiView.m_IsInit && m_MultiViewActivated)
	{
		int TeamId = 0;
		if(m_Snap.m_SpecInfo.m_SpectatorID >= 0)
			TeamId = m_Teams.Team(m_Snap.m_SpecInfo.m_SpectatorID);

		if(TeamId > MAX_CLIENTS || TeamId < 0)
			TeamId = 0;

		if(!InitMultiView(TeamId))
		{
			dbg_msg("MultiView", "No players found to spectate");
			ResetMultiView();
		}
	}

	// update the local character and spectate position
	UpdatePositions();

	// display gfx & client warnings
	for(SWarning *pWarning : {Graphics()->GetCurWarning(), Client()->GetCurWarning()})
	{
		if(pWarning != nullptr && m_Menus.CanDisplayWarning())
		{
			m_Menus.PopupWarning(pWarning->m_aWarningTitle[0] == '\0' ? Localize("Warning") : pWarning->m_aWarningTitle, pWarning->m_aWarningMsg, "Ok", pWarning->m_AutoHide ? 10s : 0s);
			pWarning->m_WasShown = true;
		}
	}

	// render all systems
	for(auto &pComponent : m_vpAll)
		pComponent->OnRender();

	// clear all events/input for this frame
	Input()->Clear();

	CLineInput::RenderCandidates();

	// clear new tick flags
	m_NewTick = false;
	m_NewPredictedTick = false;

	if(g_Config.m_ClDummy && !Client()->DummyConnected())
		g_Config.m_ClDummy = 0;

	// resend player and dummy info if it was filtered by server
	if(Client()->State() == IClient::STATE_ONLINE && !m_Menus.IsActive())
	{
		if(m_aCheckInfo[0] == 0)
		{
			if(
				str_comp(m_aClients[m_aLocalIDs[0]].m_aName, Client()->PlayerName()) ||
				str_comp(m_aClients[m_aLocalIDs[0]].m_aClan, g_Config.m_PlayerClan) ||
				m_aClients[m_aLocalIDs[0]].m_Country != g_Config.m_PlayerCountry ||
				str_comp(m_aClients[m_aLocalIDs[0]].m_aSkinName, g_Config.m_ClPlayerSkin) ||
				m_aClients[m_aLocalIDs[0]].m_UseCustomColor != g_Config.m_ClPlayerUseCustomColor ||
				m_aClients[m_aLocalIDs[0]].m_ColorBody != (int)g_Config.m_ClPlayerColorBody ||
				m_aClients[m_aLocalIDs[0]].m_ColorFeet != (int)g_Config.m_ClPlayerColorFeet)
				SendInfo(false);
			else
				m_aCheckInfo[0] = -1;
		}

		if(m_aCheckInfo[0] > 0)
			m_aCheckInfo[0]--;

		if(Client()->DummyConnected())
		{
			if(m_aCheckInfo[1] == 0)
			{
				if(
					str_comp(m_aClients[m_aLocalIDs[1]].m_aName, Client()->DummyName()) ||
					str_comp(m_aClients[m_aLocalIDs[1]].m_aClan, g_Config.m_ClDummyClan) ||
					m_aClients[m_aLocalIDs[1]].m_Country != g_Config.m_ClDummyCountry ||
					str_comp(m_aClients[m_aLocalIDs[1]].m_aSkinName, g_Config.m_ClDummySkin) ||
					m_aClients[m_aLocalIDs[1]].m_UseCustomColor != g_Config.m_ClDummyUseCustomColor ||
					m_aClients[m_aLocalIDs[1]].m_ColorBody != (int)g_Config.m_ClDummyColorBody ||
					m_aClients[m_aLocalIDs[1]].m_ColorFeet != (int)g_Config.m_ClDummyColorFeet)
					SendDummyInfo(false);
				else
					m_aCheckInfo[1] = -1;
			}

			if(m_aCheckInfo[1] > 0)
				m_aCheckInfo[1]--;
		}
	}
}

void CGameClient::OnDummyDisconnect()
{
	m_aDDRaceMsgSent[1] = false;
	m_aShowOthers[1] = SHOW_OTHERS_NOT_SET;
	m_aLastNewPredictedTick[1] = -1;
	m_PredictedDummyID = -1;
}

int CGameClient::GetLastRaceTick() const
{
	return m_Ghost.GetLastRaceTick();
}

bool CGameClient::Predict() const
{
	if(!g_Config.m_ClPredict)
		return false;

	if(m_Snap.m_pGameInfoObj)
	{
		if(m_Snap.m_pGameInfoObj->m_GameStateFlags & (GAMESTATEFLAG_GAMEOVER | GAMESTATEFLAG_PAUSED))
		{
			return false;
		}
	}

	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return false;

	return !m_Snap.m_SpecInfo.m_Active && m_Snap.m_pLocalCharacter;
}

ColorRGBA CGameClient::GetDDTeamColor(int DDTeam, float Lightness) const
{
	// Use golden angle to generate unique colors with distinct adjacent colors.
	// The first DDTeam (team 1) gets angle 0°, i.e. red hue.
	const float Hue = std::fmod((DDTeam - 1) * (137.50776f / 360.0f), 1.0f);
	return color_cast<ColorRGBA>(ColorHSLA(Hue, 1.0f, Lightness));
}

void CGameClient::OnRelease()
{
	// release all systems
	for(auto &pComponent : m_vpAll)
		pComponent->OnRelease();
}

void CGameClient::OnMessage(int MsgId, CUnpacker *pUnpacker, int Conn, bool Dummy)
{
	// special messages
	if(MsgId == NETMSGTYPE_SV_TUNEPARAMS)
	{
		// unpack the new tuning
		CTuningParams NewTuning;
		int *pParams = (int *)&NewTuning;
		// No jetpack on DDNet incompatible servers:
		NewTuning.m_JetpackStrength = 0;
		for(unsigned i = 0; i < sizeof(CTuningParams) / sizeof(int); i++)
		{
			int value = pUnpacker->GetInt();

			// check for unpacking errors
			if(pUnpacker->Error())
				break;

			pParams[i] = value;
		}

		m_ServerMode = SERVERMODE_PURE;

		m_aReceivedTuning[Conn] = true;
		// apply new tuning
		m_aTuning[Conn] = NewTuning;
		return;
	}

	void *pRawMsg = m_NetObjHandler.SecureUnpackMsg(MsgId, pUnpacker);
	if(!pRawMsg)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "dropped weird message '%s' (%d), failed on '%s'", m_NetObjHandler.GetMsgName(MsgId), MsgId, m_NetObjHandler.FailedMsgOn());
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);
		return;
	}

	if(Dummy)
	{
		if(MsgId == NETMSGTYPE_SV_CHAT && m_aLocalIDs[0] >= 0 && m_aLocalIDs[1] >= 0)
		{
			CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;

			if((pMsg->m_Team == 1 && (m_aClients[m_aLocalIDs[0]].m_Team != m_aClients[m_aLocalIDs[1]].m_Team || m_Teams.Team(m_aLocalIDs[0]) != m_Teams.Team(m_aLocalIDs[1]))) || pMsg->m_Team > 1)
			{
				m_Chat.OnMessage(MsgId, pRawMsg);
			}
		}
		return; // no need of all that stuff for the dummy
	}

	// TODO: this should be done smarter
	for(auto &pComponent : m_vpAll)
		pComponent->OnMessage(MsgId, pRawMsg);

	if(MsgId == NETMSGTYPE_SV_READYTOENTER)
	{
		Client()->EnterGame(Conn);
	}
	else if(MsgId == NETMSGTYPE_SV_EMOTICON)
	{
		CNetMsg_Sv_Emoticon *pMsg = (CNetMsg_Sv_Emoticon *)pRawMsg;

		// apply
		m_aClients[pMsg->m_ClientID].m_Emoticon = pMsg->m_Emoticon;
		m_aClients[pMsg->m_ClientID].m_EmoticonStartTick = Client()->GameTick(Conn);
		m_aClients[pMsg->m_ClientID].m_EmoticonStartFraction = Client()->IntraGameTickSincePrev(Conn);
	}
	else if(MsgId == NETMSGTYPE_SV_SOUNDGLOBAL)
	{
		if(m_SuppressEvents)
			return;

		// don't enqueue pseudo-global sounds from demos (created by PlayAndRecord)
		CNetMsg_Sv_SoundGlobal *pMsg = (CNetMsg_Sv_SoundGlobal *)pRawMsg;
		if(pMsg->m_SoundID == SOUND_CTF_DROP || pMsg->m_SoundID == SOUND_CTF_RETURN ||
			pMsg->m_SoundID == SOUND_CTF_CAPTURE || pMsg->m_SoundID == SOUND_CTF_GRAB_EN ||
			pMsg->m_SoundID == SOUND_CTF_GRAB_PL)
		{
			if(g_Config.m_SndGame)
				m_Sounds.Enqueue(CSounds::CHN_GLOBAL, pMsg->m_SoundID);
		}
		else
		{
			if(g_Config.m_SndGame)
				m_Sounds.Play(CSounds::CHN_GLOBAL, pMsg->m_SoundID, 1.0f);
		}
	}
	else if(MsgId == NETMSGTYPE_SV_TEAMSSTATE || MsgId == NETMSGTYPE_SV_TEAMSSTATELEGACY)
	{
		unsigned int i;

		for(i = 0; i < MAX_CLIENTS; i++)
		{
			const int Team = pUnpacker->GetInt();
			if(!pUnpacker->Error() && Team >= TEAM_FLOCK && Team <= TEAM_SUPER)
				m_Teams.Team(i, Team);
			else
			{
				m_Teams.Team(i, 0);
				break;
			}
		}

		if(i <= 16)
			m_Teams.m_IsDDRace16 = true;

		m_Ghost.m_AllowRestart = true;
		m_RaceDemo.m_AllowRestart = true;
	}
	else if(MsgId == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;
		// reset character prediction
		if(!(m_GameWorld.m_WorldConfig.m_IsFNG && pMsg->m_Weapon == WEAPON_LASER))
		{
			m_CharOrder.GiveWeak(pMsg->m_Victim);
			if(CCharacter *pChar = m_GameWorld.GetCharacterByID(pMsg->m_Victim))
				pChar->ResetPrediction();
			m_GameWorld.ReleaseHooked(pMsg->m_Victim);
		}

		// if we are spectating a static id set (team 0) and somebody killed, and its not a guy in solo, we remove him from the list
		if(IsMultiViewIdSet() && m_MultiViewTeam == 0 && m_aMultiViewId[pMsg->m_Victim] && !m_aClients[pMsg->m_Victim].m_Spec && !m_MultiView.m_Solo)
		{
			m_aMultiViewId[pMsg->m_Victim] = false;

			// if everyone of a team killed, we have no ids to spectate anymore, so we disable multi view
			if(!IsMultiViewIdSet())
				ResetMultiView();
			else
			{
				// the "main" tee killed, search a new one
				if(m_Snap.m_SpecInfo.m_SpectatorID == pMsg->m_Victim)
				{
					int NewClientID = FindFirstMultiViewId();
					if(NewClientID < MAX_CLIENTS && NewClientID >= 0)
					{
						CleanMultiViewId(NewClientID);
						m_aMultiViewId[NewClientID] = true;
						m_Spectator.Spectate(NewClientID);
					}
				}
			}
		}
	}
	else if(MsgId == NETMSGTYPE_SV_KILLMSGTEAM)
	{
		CNetMsg_Sv_KillMsgTeam *pMsg = (CNetMsg_Sv_KillMsgTeam *)pRawMsg;

		// reset prediction
		std::vector<std::pair<int, int>> vStrongWeakSorted;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_Teams.Team(i) == pMsg->m_Team)
			{
				if(CCharacter *pChar = m_GameWorld.GetCharacterByID(i))
				{
					pChar->ResetPrediction();
					vStrongWeakSorted.emplace_back(i, pMsg->m_First == i ? MAX_CLIENTS : pChar ? pChar->GetStrongWeakID() : 0);
				}
				m_GameWorld.ReleaseHooked(i);
			}
		}
		std::stable_sort(vStrongWeakSorted.begin(), vStrongWeakSorted.end(), [](auto &Left, auto &Right) { return Left.second > Right.second; });
		for(auto ID : vStrongWeakSorted)
		{
			m_CharOrder.GiveWeak(ID.first);
		}
	}
}

void CGameClient::OnStateChange(int NewState, int OldState)
{
	// reset everything when not already connected (to keep gathered stuff)
	if(NewState < IClient::STATE_ONLINE)
		OnReset();

	// then change the state
	for(auto &pComponent : m_vpAll)
		pComponent->OnStateChange(NewState, OldState);
}

void CGameClient::OnShutdown()
{
	RenderShutdownMessage();

	for(auto &pComponent : m_vpAll)
		pComponent->OnShutdown();
}

void CGameClient::OnEnterGame()
{
	m_Effects.ResetDamageIndicator();
}

void CGameClient::OnGameOver()
{
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK && g_Config.m_ClEditor == 0)
		Client()->AutoScreenshot_Start();
}

void CGameClient::OnStartGame()
{
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK && !g_Config.m_ClAutoDemoOnConnect)
		Client()->DemoRecorder_HandleAutoStart();
	m_Statboard.OnReset();
}

void CGameClient::OnStartRound()
{
	// In GamePaused or GameOver state RoundStartTick is updated on each tick
	// hence no need to reset stats until player leaves GameOver
	// and it would be a mistake to reset stats after or during the pause
	m_Statboard.OnReset();

	// Restart automatic race demo recording
	m_RaceDemo.OnReset();
}

void CGameClient::OnFlagGrab(int TeamID)
{
	if(TeamID == TEAM_RED)
		m_aStats[m_Snap.m_pGameDataObj->m_FlagCarrierRed].m_FlagGrabs++;
	else
		m_aStats[m_Snap.m_pGameDataObj->m_FlagCarrierBlue].m_FlagGrabs++;
}

void CGameClient::OnWindowResize()
{
	for(auto &pComponent : m_vpAll)
		pComponent->OnWindowResize();

	UI()->OnWindowResize();
}

void CGameClient::OnLanguageChange()
{
	// The actual language change is delayed because it
	// might require clearing the text render font atlas,
	// which would invalidate text that is currently drawn.
	m_LanguageChanged = true;
}

void CGameClient::HandleLanguageChanged()
{
	if(!m_LanguageChanged)
		return;
	m_LanguageChanged = false;

	g_Localization.Load(g_Config.m_ClLanguagefile, Storage(), Console());
	TextRender()->SetFontLanguageVariant(g_Config.m_ClLanguagefile);

	// Clear all text containers
	Client()->OnWindowResize();
}

void CGameClient::RenderShutdownMessage()
{
	const char *pMessage = nullptr;
	if(Client()->State() == IClient::STATE_QUITTING)
		pMessage = Localize("Quitting. Please wait…");
	else if(Client()->State() == IClient::STATE_RESTARTING)
		pMessage = Localize("Restarting. Please wait…");
	else
		dbg_assert(false, "Invalid client state for quitting message");

	// This function only gets called after the render loop has already terminated, so we have to call Swap manually.
	Graphics()->Clear(0.0f, 0.0f, 0.0f);
	UI()->MapScreen();
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	UI()->DoLabel(UI()->Screen(), pMessage, 16.0f, TEXTALIGN_MC);
	Graphics()->Swap();
	Graphics()->Clear(0.0f, 0.0f, 0.0f);
}

void CGameClient::OnRconType(bool UsernameReq)
{
	m_GameConsole.RequireUsername(UsernameReq);
}

void CGameClient::OnRconLine(const char *pLine)
{
	m_GameConsole.PrintLine(CGameConsole::CONSOLETYPE_REMOTE, pLine);
}

void CGameClient::ProcessEvents()
{
	if(m_SuppressEvents)
		return;

	int SnapType = IClient::SNAP_CURRENT;
	int Num = Client()->SnapNumItems(SnapType);
	for(int Index = 0; Index < Num; Index++)
	{
		IClient::CSnapItem Item;
		const void *pData = Client()->SnapGetItem(SnapType, Index, &Item);

		// We don't have enough info about us, others, to know a correct alpha value.
		float Alpha = 1.0f;

		if(Item.m_Type == NETEVENTTYPE_DAMAGEIND)
		{
			CNetEvent_DamageInd *pEvent = (CNetEvent_DamageInd *)pData;
			m_Effects.DamageIndicator(vec2(pEvent->m_X, pEvent->m_Y), direction(pEvent->m_Angle / 256.0f), Alpha);
		}
		else if(Item.m_Type == NETEVENTTYPE_EXPLOSION)
		{
			CNetEvent_Explosion *pEvent = (CNetEvent_Explosion *)pData;
			m_Effects.Explosion(vec2(pEvent->m_X, pEvent->m_Y), Alpha);
		}
		else if(Item.m_Type == NETEVENTTYPE_HAMMERHIT)
		{
			CNetEvent_HammerHit *pEvent = (CNetEvent_HammerHit *)pData;
			m_Effects.HammerHit(vec2(pEvent->m_X, pEvent->m_Y), Alpha);
		}
		else if(Item.m_Type == NETEVENTTYPE_SPAWN)
		{
			CNetEvent_Spawn *pEvent = (CNetEvent_Spawn *)pData;
			m_Effects.PlayerSpawn(vec2(pEvent->m_X, pEvent->m_Y), Alpha);
		}
		else if(Item.m_Type == NETEVENTTYPE_DEATH)
		{
			CNetEvent_Death *pEvent = (CNetEvent_Death *)pData;
			m_Effects.PlayerDeath(vec2(pEvent->m_X, pEvent->m_Y), pEvent->m_ClientID, Alpha);
		}
		else if(Item.m_Type == NETEVENTTYPE_SOUNDWORLD)
		{
			CNetEvent_SoundWorld *pEvent = (CNetEvent_SoundWorld *)pData;
			if(!Config()->m_SndGame)
				continue;

			if(m_GameInfo.m_RaceSounds && ((pEvent->m_SoundID == SOUND_GUN_FIRE && !g_Config.m_SndGun) || (pEvent->m_SoundID == SOUND_PLAYER_PAIN_LONG && !g_Config.m_SndLongPain)))
				continue;

			m_Sounds.PlayAt(CSounds::CHN_WORLD, pEvent->m_SoundID, 1.0f, vec2(pEvent->m_X, pEvent->m_Y));
		}
	}
}

static CGameInfo GetGameInfo(const CNetObj_GameInfoEx *pInfoEx, int InfoExSize, CServerInfo *pFallbackServerInfo)
{
	int Version = -1;
	if(InfoExSize >= 12)
	{
		Version = pInfoEx->m_Version;
	}
	else if(InfoExSize >= 8)
	{
		Version = minimum(pInfoEx->m_Version, 4);
	}
	else if(InfoExSize >= 4)
	{
		Version = 0;
	}
	int Flags = 0;
	if(Version >= 0)
	{
		Flags = pInfoEx->m_Flags;
	}
	int Flags2 = 0;
	if(Version >= 5)
	{
		Flags2 = pInfoEx->m_Flags2;
	}
	bool Race;
	bool FastCap;
	bool FNG;
	bool DDRace;
	bool DDNet;
	bool BlockWorlds;
	bool City;
	bool Vanilla;
	bool Plus;
	bool FDDrace;
	if(Version < 1)
	{
		const char *pGameType = pFallbackServerInfo->m_aGameType;
		Race = str_find_nocase(pGameType, "race") || str_find_nocase(pGameType, "fastcap");
		FastCap = str_find_nocase(pGameType, "fastcap");
		FNG = str_find_nocase(pGameType, "fng");
		DDRace = str_find_nocase(pGameType, "ddrace") || str_find_nocase(pGameType, "mkrace");
		DDNet = str_find_nocase(pGameType, "ddracenet") || str_find_nocase(pGameType, "ddnet");
		BlockWorlds = str_startswith(pGameType, "bw  ") || str_comp_nocase(pGameType, "bw") == 0;
		City = str_find_nocase(pGameType, "city");
		Vanilla = str_comp(pGameType, "DM") == 0 || str_comp(pGameType, "TDM") == 0 || str_comp(pGameType, "CTF") == 0;
		Plus = str_find(pGameType, "+");
		FDDrace = false;
	}
	else
	{
		Race = Flags & GAMEINFOFLAG_GAMETYPE_RACE;
		FastCap = Flags & GAMEINFOFLAG_GAMETYPE_FASTCAP;
		FNG = Flags & GAMEINFOFLAG_GAMETYPE_FNG;
		DDRace = Flags & GAMEINFOFLAG_GAMETYPE_DDRACE;
		DDNet = Flags & GAMEINFOFLAG_GAMETYPE_DDNET;
		BlockWorlds = Flags & GAMEINFOFLAG_GAMETYPE_BLOCK_WORLDS;
		Vanilla = Flags & GAMEINFOFLAG_GAMETYPE_VANILLA;
		Plus = Flags & GAMEINFOFLAG_GAMETYPE_PLUS;
		City = Version >= 5 && Flags2 & GAMEINFOFLAG2_GAMETYPE_CITY;
		FDDrace = Version >= 6 && Flags2 & GAMEINFOFLAG2_GAMETYPE_FDDRACE;

		// Ensure invariants upheld by the server info parsing business.
		DDRace = DDRace || DDNet || FDDrace;
		Race = Race || FastCap || DDRace;
	}

	CGameInfo Info;
	Info.m_FlagStartsRace = FastCap;
	Info.m_TimeScore = Race;
	Info.m_UnlimitedAmmo = Race;
	Info.m_DDRaceRecordMessage = DDRace && !DDNet;
	Info.m_RaceRecordMessage = DDNet || (Race && !DDRace);
	Info.m_RaceSounds = DDRace || FNG || BlockWorlds;
	Info.m_AllowEyeWheel = DDRace || BlockWorlds || City || Plus;
	Info.m_AllowHookColl = DDRace;
	Info.m_AllowZoom = Race || BlockWorlds || City;
	Info.m_BugDDRaceGhost = DDRace;
	Info.m_BugDDRaceInput = DDRace;
	Info.m_BugFNGLaserRange = FNG;
	Info.m_BugVanillaBounce = Vanilla;
	Info.m_PredictFNG = FNG;
	Info.m_PredictDDRace = DDRace;
	Info.m_PredictDDRaceTiles = DDRace && !BlockWorlds;
	Info.m_PredictVanilla = Vanilla || FastCap;
	Info.m_EntitiesDDNet = DDNet;
	Info.m_EntitiesDDRace = DDRace;
	Info.m_EntitiesRace = Race;
	Info.m_EntitiesFNG = FNG;
	Info.m_EntitiesVanilla = Vanilla;
	Info.m_EntitiesBW = BlockWorlds;
	Info.m_Race = Race;
	Info.m_DontMaskEntities = !DDNet;
	Info.m_AllowXSkins = false;
	Info.m_EntitiesFDDrace = FDDrace;
	Info.m_HudHealthArmor = true;
	Info.m_HudAmmo = true;
	Info.m_HudDDRace = false;
	Info.m_NoWeakHookAndBounce = false;
	Info.m_NoSkinChangeForFrozen = false;

	if(Version >= 0)
	{
		Info.m_TimeScore = Flags & GAMEINFOFLAG_TIMESCORE;
	}
	if(Version >= 2)
	{
		Info.m_FlagStartsRace = Flags & GAMEINFOFLAG_FLAG_STARTS_RACE;
		Info.m_UnlimitedAmmo = Flags & GAMEINFOFLAG_UNLIMITED_AMMO;
		Info.m_DDRaceRecordMessage = Flags & GAMEINFOFLAG_DDRACE_RECORD_MESSAGE;
		Info.m_RaceRecordMessage = Flags & GAMEINFOFLAG_RACE_RECORD_MESSAGE;
		Info.m_AllowEyeWheel = Flags & GAMEINFOFLAG_ALLOW_EYE_WHEEL;
		Info.m_AllowHookColl = Flags & GAMEINFOFLAG_ALLOW_HOOK_COLL;
		Info.m_AllowZoom = Flags & GAMEINFOFLAG_ALLOW_ZOOM;
		Info.m_BugDDRaceGhost = Flags & GAMEINFOFLAG_BUG_DDRACE_GHOST;
		Info.m_BugDDRaceInput = Flags & GAMEINFOFLAG_BUG_DDRACE_INPUT;
		Info.m_BugFNGLaserRange = Flags & GAMEINFOFLAG_BUG_FNG_LASER_RANGE;
		Info.m_BugVanillaBounce = Flags & GAMEINFOFLAG_BUG_VANILLA_BOUNCE;
		Info.m_PredictFNG = Flags & GAMEINFOFLAG_PREDICT_FNG;
		Info.m_PredictDDRace = Flags & GAMEINFOFLAG_PREDICT_DDRACE;
		Info.m_PredictDDRaceTiles = Flags & GAMEINFOFLAG_PREDICT_DDRACE_TILES;
		Info.m_PredictVanilla = Flags & GAMEINFOFLAG_PREDICT_VANILLA;
		Info.m_EntitiesDDNet = Flags & GAMEINFOFLAG_ENTITIES_DDNET;
		Info.m_EntitiesDDRace = Flags & GAMEINFOFLAG_ENTITIES_DDRACE;
		Info.m_EntitiesRace = Flags & GAMEINFOFLAG_ENTITIES_RACE;
		Info.m_EntitiesFNG = Flags & GAMEINFOFLAG_ENTITIES_FNG;
		Info.m_EntitiesVanilla = Flags & GAMEINFOFLAG_ENTITIES_VANILLA;
	}
	if(Version >= 3)
	{
		Info.m_Race = Flags & GAMEINFOFLAG_RACE;
		Info.m_DontMaskEntities = Flags & GAMEINFOFLAG_DONT_MASK_ENTITIES;
	}
	if(Version >= 4)
	{
		Info.m_EntitiesBW = Flags & GAMEINFOFLAG_ENTITIES_BW;
	}
	if(Version >= 5)
	{
		Info.m_AllowXSkins = Flags2 & GAMEINFOFLAG2_ALLOW_X_SKINS;
	}
	if(Version >= 6)
	{
		Info.m_EntitiesFDDrace = Flags2 & GAMEINFOFLAG2_ENTITIES_FDDRACE;
	}
	if(Version >= 7)
	{
		Info.m_HudHealthArmor = Flags2 & GAMEINFOFLAG2_HUD_HEALTH_ARMOR;
		Info.m_HudAmmo = Flags2 & GAMEINFOFLAG2_HUD_AMMO;
		Info.m_HudDDRace = Flags2 & GAMEINFOFLAG2_HUD_DDRACE;
	}
	if(Version >= 8)
	{
		Info.m_NoWeakHookAndBounce = Flags2 & GAMEINFOFLAG2_NO_WEAK_HOOK;
	}
	if(Version >= 9)
	{
		Info.m_NoSkinChangeForFrozen = Flags2 & GAMEINFOFLAG2_NO_SKIN_CHANGE_FOR_FROZEN;
	}

	return Info;
}

void CGameClient::InvalidateSnapshot()
{
	// clear all pointers
	mem_zero(&m_Snap, sizeof(m_Snap));
	m_Snap.m_LocalClientID = -1;
	SnapCollectEntities();
}

void CGameClient::OnNewSnapshot()
{
	auto &&Evolve = [this](CNetObj_Character *pCharacter, int Tick) {
		CWorldCore TempWorld;
		CCharacterCore TempCore = CCharacterCore();
		CTeamsCore TempTeams = CTeamsCore();
		TempCore.Init(&TempWorld, Collision(), &TempTeams);
		TempCore.Read(pCharacter);
		TempCore.m_ActiveWeapon = pCharacter->m_Weapon;

		while(pCharacter->m_Tick < Tick)
		{
			pCharacter->m_Tick++;
			TempCore.Tick(false);
			TempCore.Move();
			TempCore.Quantize();
		}

		TempCore.Write(pCharacter);
	};

	InvalidateSnapshot();

	m_NewTick = true;

	ProcessEvents();

#ifdef CONF_DEBUG
	if(g_Config.m_DbgStress)
	{
		if((Client()->GameTick(g_Config.m_ClDummy) % 100) == 0)
		{
			char aMessage[64];
			int MsgLen = rand() % (sizeof(aMessage) - 1);
			for(int i = 0; i < MsgLen; i++)
				aMessage[i] = (char)('a' + (rand() % ('z' - 'a')));
			aMessage[MsgLen] = 0;

			CNetMsg_Cl_Say Msg;
			Msg.m_Team = rand() & 1;
			Msg.m_pMessage = aMessage;
			Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);
		}
	}
#endif

	bool FoundGameInfoEx = false;
	bool GotSwitchStateTeam = false;
	m_aSwitchStateTeam[g_Config.m_ClDummy] = -1;

	for(auto &Client : m_aClients)
	{
		Client.m_SpecCharPresent = false;
	}

	// go through all the items in the snapshot and gather the info we want
	{
		m_Snap.m_aTeamSize[TEAM_RED] = m_Snap.m_aTeamSize[TEAM_BLUE] = 0;

		int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);
		for(int i = 0; i < Num; i++)
		{
			IClient::CSnapItem Item;
			const void *pData = Client()->SnapGetItem(IClient::SNAP_CURRENT, i, &Item);

			if(Item.m_Type == NETOBJTYPE_CLIENTINFO)
			{
				const CNetObj_ClientInfo *pInfo = (const CNetObj_ClientInfo *)pData;
				int ClientID = Item.m_ID;
				if(ClientID < MAX_CLIENTS)
				{
					CClientData *pClient = &m_aClients[ClientID];

					IntsToStr(&pInfo->m_Name0, 4, pClient->m_aName);
					IntsToStr(&pInfo->m_Clan0, 3, pClient->m_aClan);
					pClient->m_Country = pInfo->m_Country;
					IntsToStr(&pInfo->m_Skin0, 6, pClient->m_aSkinName);

					pClient->m_UseCustomColor = pInfo->m_UseCustomColor;
					pClient->m_ColorBody = pInfo->m_ColorBody;
					pClient->m_ColorFeet = pInfo->m_ColorFeet;

					// prepare the info
					if(!m_GameInfo.m_AllowXSkins && (pClient->m_aSkinName[0] == 'x' && pClient->m_aSkinName[1] == '_'))
						str_copy(pClient->m_aSkinName, "default");

					pClient->m_SkinInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(pClient->m_ColorBody).UnclampLighting());
					pClient->m_SkinInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(pClient->m_ColorFeet).UnclampLighting());
					pClient->m_SkinInfo.m_Size = 64;

					// find new skin
					const CSkin *pSkin = m_Skins.Find(pClient->m_aSkinName);
					pClient->m_SkinInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
					pClient->m_SkinInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
					pClient->m_SkinInfo.m_SkinMetrics = pSkin->m_Metrics;
					pClient->m_SkinInfo.m_BloodColor = pSkin->m_BloodColor;
					pClient->m_SkinInfo.m_CustomColoredSkin = pClient->m_UseCustomColor;

					if(!pClient->m_UseCustomColor)
					{
						pClient->m_SkinInfo.m_ColorBody = ColorRGBA(1, 1, 1);
						pClient->m_SkinInfo.m_ColorFeet = ColorRGBA(1, 1, 1);
					}

					pClient->UpdateRenderInfo(IsTeamPlay());
				}
			}
			else if(Item.m_Type == NETOBJTYPE_PLAYERINFO)
			{
				const CNetObj_PlayerInfo *pInfo = (const CNetObj_PlayerInfo *)pData;

				if(pInfo->m_ClientID < MAX_CLIENTS && pInfo->m_ClientID == Item.m_ID)
				{
					m_aClients[pInfo->m_ClientID].m_Team = pInfo->m_Team;
					m_aClients[pInfo->m_ClientID].m_Active = true;
					m_Snap.m_apPlayerInfos[pInfo->m_ClientID] = pInfo;
					m_Snap.m_NumPlayers++;

					if(pInfo->m_Local)
					{
						m_Snap.m_LocalClientID = pInfo->m_ClientID;
						m_Snap.m_pLocalInfo = pInfo;

						if(pInfo->m_Team == TEAM_SPECTATORS)
						{
							m_Snap.m_SpecInfo.m_Active = true;
						}
					}

					// calculate team-balance
					if(pInfo->m_Team != TEAM_SPECTATORS)
					{
						m_Snap.m_aTeamSize[pInfo->m_Team]++;
						if(!m_aStats[pInfo->m_ClientID].IsActive())
							m_aStats[pInfo->m_ClientID].JoinGame(Client()->GameTick(g_Config.m_ClDummy));
					}
					else if(m_aStats[pInfo->m_ClientID].IsActive())
						m_aStats[pInfo->m_ClientID].JoinSpec(Client()->GameTick(g_Config.m_ClDummy));
				}
			}
			else if(Item.m_Type == NETOBJTYPE_DDNETPLAYER)
			{
				m_ReceivedDDNetPlayer = true;
				const CNetObj_DDNetPlayer *pInfo = (const CNetObj_DDNetPlayer *)pData;
				if(Item.m_ID < MAX_CLIENTS)
				{
					m_aClients[Item.m_ID].m_AuthLevel = pInfo->m_AuthLevel;
					m_aClients[Item.m_ID].m_Afk = pInfo->m_Flags & EXPLAYERFLAG_AFK;
					m_aClients[Item.m_ID].m_Paused = pInfo->m_Flags & EXPLAYERFLAG_PAUSED;
					m_aClients[Item.m_ID].m_Spec = pInfo->m_Flags & EXPLAYERFLAG_SPEC;

					if(Item.m_ID == m_Snap.m_LocalClientID && (m_aClients[Item.m_ID].m_Paused || m_aClients[Item.m_ID].m_Spec))
					{
						m_Snap.m_SpecInfo.m_Active = true;
					}
				}
			}
			else if(Item.m_Type == NETOBJTYPE_CHARACTER)
			{
				if(Item.m_ID < MAX_CLIENTS)
				{
					const void *pOld = Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_CHARACTER, Item.m_ID);
					m_Snap.m_aCharacters[Item.m_ID].m_Cur = *((const CNetObj_Character *)pData);
					if(pOld)
					{
						m_Snap.m_aCharacters[Item.m_ID].m_Active = true;
						m_Snap.m_aCharacters[Item.m_ID].m_Prev = *((const CNetObj_Character *)pOld);

						// limit evolving to 3 seconds
						bool EvolvePrev = Client()->PrevGameTick(g_Config.m_ClDummy) - m_Snap.m_aCharacters[Item.m_ID].m_Prev.m_Tick <= 3 * Client()->GameTickSpeed();
						bool EvolveCur = Client()->GameTick(g_Config.m_ClDummy) - m_Snap.m_aCharacters[Item.m_ID].m_Cur.m_Tick <= 3 * Client()->GameTickSpeed();

						// reuse the result from the previous evolve if the snapped character didn't change since the previous snapshot
						if(EvolveCur && m_aClients[Item.m_ID].m_Evolved.m_Tick == Client()->PrevGameTick(g_Config.m_ClDummy))
						{
							if(mem_comp(&m_Snap.m_aCharacters[Item.m_ID].m_Prev, &m_aClients[Item.m_ID].m_Snapped, sizeof(CNetObj_Character)) == 0)
								m_Snap.m_aCharacters[Item.m_ID].m_Prev = m_aClients[Item.m_ID].m_Evolved;
							if(mem_comp(&m_Snap.m_aCharacters[Item.m_ID].m_Cur, &m_aClients[Item.m_ID].m_Snapped, sizeof(CNetObj_Character)) == 0)
								m_Snap.m_aCharacters[Item.m_ID].m_Cur = m_aClients[Item.m_ID].m_Evolved;
						}

						if(EvolvePrev && m_Snap.m_aCharacters[Item.m_ID].m_Prev.m_Tick)
							Evolve(&m_Snap.m_aCharacters[Item.m_ID].m_Prev, Client()->PrevGameTick(g_Config.m_ClDummy));
						if(EvolveCur && m_Snap.m_aCharacters[Item.m_ID].m_Cur.m_Tick)
							Evolve(&m_Snap.m_aCharacters[Item.m_ID].m_Cur, Client()->GameTick(g_Config.m_ClDummy));

						m_aClients[Item.m_ID].m_Snapped = *((const CNetObj_Character *)pData);
						m_aClients[Item.m_ID].m_Evolved = m_Snap.m_aCharacters[Item.m_ID].m_Cur;
					}
					else
					{
						m_aClients[Item.m_ID].m_Evolved.m_Tick = -1;
					}
				}
			}
			else if(Item.m_Type == NETOBJTYPE_DDNETCHARACTER)
			{
				const CNetObj_DDNetCharacter *pCharacterData = (const CNetObj_DDNetCharacter *)pData;

				if(Item.m_ID < MAX_CLIENTS)
				{
					m_Snap.m_aCharacters[Item.m_ID].m_ExtendedData = *pCharacterData;
					m_Snap.m_aCharacters[Item.m_ID].m_PrevExtendedData = (const CNetObj_DDNetCharacter *)Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_DDNETCHARACTER, Item.m_ID);
					m_Snap.m_aCharacters[Item.m_ID].m_HasExtendedData = true;
					m_Snap.m_aCharacters[Item.m_ID].m_HasExtendedDisplayInfo = false;
					if(pCharacterData->m_JumpedTotal != -1)
					{
						m_Snap.m_aCharacters[Item.m_ID].m_HasExtendedDisplayInfo = true;
					}
					CClientData *pClient = &m_aClients[Item.m_ID];
					// Collision
					pClient->m_Solo = pCharacterData->m_Flags & CHARACTERFLAG_SOLO;
					pClient->m_Jetpack = pCharacterData->m_Flags & CHARACTERFLAG_JETPACK;
					pClient->m_CollisionDisabled = pCharacterData->m_Flags & CHARACTERFLAG_COLLISION_DISABLED;
					pClient->m_HammerHitDisabled = pCharacterData->m_Flags & CHARACTERFLAG_HAMMER_HIT_DISABLED;
					pClient->m_GrenadeHitDisabled = pCharacterData->m_Flags & CHARACTERFLAG_GRENADE_HIT_DISABLED;
					pClient->m_LaserHitDisabled = pCharacterData->m_Flags & CHARACTERFLAG_LASER_HIT_DISABLED;
					pClient->m_ShotgunHitDisabled = pCharacterData->m_Flags & CHARACTERFLAG_SHOTGUN_HIT_DISABLED;
					pClient->m_HookHitDisabled = pCharacterData->m_Flags & CHARACTERFLAG_HOOK_HIT_DISABLED;
					pClient->m_Super = pCharacterData->m_Flags & CHARACTERFLAG_SUPER;

					// Endless
					pClient->m_EndlessHook = pCharacterData->m_Flags & CHARACTERFLAG_ENDLESS_HOOK;
					pClient->m_EndlessJump = pCharacterData->m_Flags & CHARACTERFLAG_ENDLESS_JUMP;

					// Freeze
					pClient->m_FreezeEnd = pCharacterData->m_FreezeEnd;
					pClient->m_DeepFrozen = pCharacterData->m_FreezeEnd == -1;
					pClient->m_LiveFrozen = (pCharacterData->m_Flags & CHARACTERFLAG_MOVEMENTS_DISABLED) != 0;

					// Telegun
					pClient->m_HasTelegunGrenade = pCharacterData->m_Flags & CHARACTERFLAG_TELEGUN_GRENADE;
					pClient->m_HasTelegunGun = pCharacterData->m_Flags & CHARACTERFLAG_TELEGUN_GUN;
					pClient->m_HasTelegunLaser = pCharacterData->m_Flags & CHARACTERFLAG_TELEGUN_LASER;

					pClient->m_Predicted.ReadDDNet(pCharacterData);

					m_Teams.SetSolo(Item.m_ID, pClient->m_Solo);
				}
			}
			else if(Item.m_Type == NETOBJTYPE_SPECCHAR)
			{
				const CNetObj_SpecChar *pSpecCharData = (const CNetObj_SpecChar *)pData;

				if(Item.m_ID < MAX_CLIENTS)
				{
					CClientData *pClient = &m_aClients[Item.m_ID];
					pClient->m_SpecCharPresent = true;
					pClient->m_SpecChar.x = pSpecCharData->m_X;
					pClient->m_SpecChar.y = pSpecCharData->m_Y;
				}
			}
			else if(Item.m_Type == NETOBJTYPE_SPECTATORINFO)
			{
				m_Snap.m_pSpectatorInfo = (const CNetObj_SpectatorInfo *)pData;
				m_Snap.m_pPrevSpectatorInfo = (const CNetObj_SpectatorInfo *)Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_SPECTATORINFO, Item.m_ID);

				m_Snap.m_SpecInfo.m_SpectatorID = m_Snap.m_pSpectatorInfo->m_SpectatorID;
			}
			else if(Item.m_Type == NETOBJTYPE_GAMEINFO)
			{
				m_Snap.m_pGameInfoObj = (const CNetObj_GameInfo *)pData;
				bool CurrentTickGameOver = (bool)(m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER);
				if(!m_GameOver && CurrentTickGameOver)
					OnGameOver();
				else if(m_GameOver && !CurrentTickGameOver)
					OnStartGame();
				// Handle case that a new round is started (RoundStartTick changed)
				// New round is usually started after `restart` on server
				if(m_Snap.m_pGameInfoObj->m_RoundStartTick != m_LastRoundStartTick && !(CurrentTickGameOver || m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED || m_GamePaused))
					OnStartRound();
				m_LastRoundStartTick = m_Snap.m_pGameInfoObj->m_RoundStartTick;
				m_GameOver = CurrentTickGameOver;
				m_GamePaused = (bool)(m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED);
			}
			else if(Item.m_Type == NETOBJTYPE_GAMEINFOEX)
			{
				if(FoundGameInfoEx)
				{
					continue;
				}
				FoundGameInfoEx = true;
				CServerInfo ServerInfo;
				Client()->GetServerInfo(&ServerInfo);
				m_GameInfo = GetGameInfo((const CNetObj_GameInfoEx *)pData, Client()->SnapItemSize(IClient::SNAP_CURRENT, i), &ServerInfo);
			}
			else if(Item.m_Type == NETOBJTYPE_GAMEDATA)
			{
				m_Snap.m_pGameDataObj = (const CNetObj_GameData *)pData;
				m_Snap.m_GameDataSnapID = Item.m_ID;
				if(m_Snap.m_pGameDataObj->m_FlagCarrierRed == FLAG_TAKEN)
				{
					if(m_aFlagDropTick[TEAM_RED] == 0)
						m_aFlagDropTick[TEAM_RED] = Client()->GameTick(g_Config.m_ClDummy);
				}
				else
					m_aFlagDropTick[TEAM_RED] = 0;
				if(m_Snap.m_pGameDataObj->m_FlagCarrierBlue == FLAG_TAKEN)
				{
					if(m_aFlagDropTick[TEAM_BLUE] == 0)
						m_aFlagDropTick[TEAM_BLUE] = Client()->GameTick(g_Config.m_ClDummy);
				}
				else
					m_aFlagDropTick[TEAM_BLUE] = 0;
				if(m_LastFlagCarrierRed == FLAG_ATSTAND && m_Snap.m_pGameDataObj->m_FlagCarrierRed >= 0)
					OnFlagGrab(TEAM_RED);
				else if(m_LastFlagCarrierBlue == FLAG_ATSTAND && m_Snap.m_pGameDataObj->m_FlagCarrierBlue >= 0)
					OnFlagGrab(TEAM_BLUE);

				m_LastFlagCarrierRed = m_Snap.m_pGameDataObj->m_FlagCarrierRed;
				m_LastFlagCarrierBlue = m_Snap.m_pGameDataObj->m_FlagCarrierBlue;
			}
			else if(Item.m_Type == NETOBJTYPE_FLAG)
				m_Snap.m_apFlags[Item.m_ID % 2] = (const CNetObj_Flag *)pData;
			else if(Item.m_Type == NETOBJTYPE_SWITCHSTATE)
			{
				if(Item.m_DataSize < 36)
				{
					continue;
				}
				const CNetObj_SwitchState *pSwitchStateData = (const CNetObj_SwitchState *)pData;
				int Team = clamp(Item.m_ID, (int)TEAM_FLOCK, (int)TEAM_SUPER - 1);

				int HighestSwitchNumber = clamp(pSwitchStateData->m_HighestSwitchNumber, 0, 255);
				if(HighestSwitchNumber != maximum(0, (int)Switchers().size() - 1))
				{
					m_GameWorld.m_Core.InitSwitchers(HighestSwitchNumber);
					Collision()->m_HighestSwitchNumber = HighestSwitchNumber;
				}

				for(int j = 0; j < (int)Switchers().size(); j++)
				{
					Switchers()[j].m_aStatus[Team] = (pSwitchStateData->m_aStatus[j / 32] >> (j % 32)) & 1;
				}

				if(Item.m_DataSize >= 68)
				{
					// update the endtick of up to four timed switchers
					for(int j = 0; j < (int)std::size(pSwitchStateData->m_aEndTicks); j++)
					{
						int SwitchNumber = pSwitchStateData->m_aSwitchNumbers[j];
						int EndTick = pSwitchStateData->m_aEndTicks[j];
						if(EndTick > 0 && in_range(SwitchNumber, 0, (int)Switchers().size()))
						{
							Switchers()[SwitchNumber].m_aEndTick[Team] = EndTick;
						}
					}
				}

				// update switch types
				for(auto &Switcher : Switchers())
				{
					if(Switcher.m_aStatus[Team])
						Switcher.m_aType[Team] = Switcher.m_aEndTick[Team] ? TILE_SWITCHTIMEDOPEN : TILE_SWITCHOPEN;
					else
						Switcher.m_aType[Team] = Switcher.m_aEndTick[Team] ? TILE_SWITCHTIMEDCLOSE : TILE_SWITCHCLOSE;
				}

				if(!GotSwitchStateTeam)
					m_aSwitchStateTeam[g_Config.m_ClDummy] = Team;
				else
					m_aSwitchStateTeam[g_Config.m_ClDummy] = -1;
				GotSwitchStateTeam = true;
			}
		}
	}

	if(!FoundGameInfoEx)
	{
		CServerInfo ServerInfo;
		Client()->GetServerInfo(&ServerInfo);
		m_GameInfo = GetGameInfo(0, 0, &ServerInfo);
	}

	// setup local pointers
	if(m_Snap.m_LocalClientID >= 0)
	{
		m_aLocalIDs[g_Config.m_ClDummy] = m_Snap.m_LocalClientID;

		CSnapState::CCharacterInfo *pChr = &m_Snap.m_aCharacters[m_Snap.m_LocalClientID];
		if(pChr->m_Active)
		{
			if(!m_Snap.m_SpecInfo.m_Active)
			{
				m_Snap.m_pLocalCharacter = &pChr->m_Cur;
				m_Snap.m_pLocalPrevCharacter = &pChr->m_Prev;
				m_LocalCharacterPos = vec2(m_Snap.m_pLocalCharacter->m_X, m_Snap.m_pLocalCharacter->m_Y);
			}
		}
		else if(Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_CHARACTER, m_Snap.m_LocalClientID))
		{
			// player died
			m_Controls.OnPlayerDeath();
		}
	}
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		if(m_Snap.m_LocalClientID == -1 && m_DemoSpecID == SPEC_FOLLOW)
			m_DemoSpecID = SPEC_FREEVIEW;
		if(m_DemoSpecID != SPEC_FOLLOW)
		{
			m_Snap.m_SpecInfo.m_Active = true;
			if(m_DemoSpecID > SPEC_FREEVIEW && m_Snap.m_aCharacters[m_DemoSpecID].m_Active)
				m_Snap.m_SpecInfo.m_SpectatorID = m_DemoSpecID;
			else
				m_Snap.m_SpecInfo.m_SpectatorID = SPEC_FREEVIEW;
		}
	}

	// clear out unneeded client data
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(!m_Snap.m_apPlayerInfos[i] && m_aClients[i].m_Active)
		{
			m_aClients[i].Reset();
			m_aStats[i].Reset();
		}
	}

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		// update friend state
		m_aClients[i].m_Friend = !(i == m_Snap.m_LocalClientID || !m_Snap.m_apPlayerInfos[i] || !Friends()->IsFriend(m_aClients[i].m_aName, m_aClients[i].m_aClan, true));

		// update foe state
		m_aClients[i].m_Foe = !(i == m_Snap.m_LocalClientID || !m_Snap.m_apPlayerInfos[i] || !Foes()->IsFriend(m_aClients[i].m_aName, m_aClients[i].m_aClan, true));
	}

	// sort player infos by name
	mem_copy(m_Snap.m_apInfoByName, m_Snap.m_apPlayerInfos, sizeof(m_Snap.m_apInfoByName));
	std::stable_sort(m_Snap.m_apInfoByName, m_Snap.m_apInfoByName + MAX_CLIENTS,
		[this](const CNetObj_PlayerInfo *p1, const CNetObj_PlayerInfo *p2) -> bool {
			if(!p2)
				return static_cast<bool>(p1);
			if(!p1)
				return false;
			return str_comp_nocase(m_aClients[p1->m_ClientID].m_aName, m_aClients[p2->m_ClientID].m_aName) < 0;
		});

	bool TimeScore = m_GameInfo.m_TimeScore;

	// sort player infos by score
	mem_copy(m_Snap.m_apInfoByScore, m_Snap.m_apInfoByName, sizeof(m_Snap.m_apInfoByScore));
	std::stable_sort(m_Snap.m_apInfoByScore, m_Snap.m_apInfoByScore + MAX_CLIENTS,
		[TimeScore](const CNetObj_PlayerInfo *p1, const CNetObj_PlayerInfo *p2) -> bool {
			if(!p2)
				return static_cast<bool>(p1);
			if(!p1)
				return false;
			return (((TimeScore && p1->m_Score == -9999) ? std::numeric_limits<int>::min() : p1->m_Score) >
				((TimeScore && p2->m_Score == -9999) ? std::numeric_limits<int>::min() : p2->m_Score));
		});

	// sort player infos by DDRace Team (and score between)
	int Index = 0;
	for(int Team = TEAM_FLOCK; Team <= TEAM_SUPER; ++Team)
	{
		for(int i = 0; i < MAX_CLIENTS && Index < MAX_CLIENTS; ++i)
		{
			if(m_Snap.m_apInfoByScore[i] && m_Teams.Team(m_Snap.m_apInfoByScore[i]->m_ClientID) == Team)
				m_Snap.m_apInfoByDDTeamScore[Index++] = m_Snap.m_apInfoByScore[i];
		}
	}

	// sort player infos by DDRace Team (and name between)
	Index = 0;
	for(int Team = TEAM_FLOCK; Team <= TEAM_SUPER; ++Team)
	{
		for(int i = 0; i < MAX_CLIENTS && Index < MAX_CLIENTS; ++i)
		{
			if(m_Snap.m_apInfoByName[i] && m_Teams.Team(m_Snap.m_apInfoByName[i]->m_ClientID) == Team)
				m_Snap.m_apInfoByDDTeamName[Index++] = m_Snap.m_apInfoByName[i];
		}
	}

	CServerInfo CurrentServerInfo;
	Client()->GetServerInfo(&CurrentServerInfo);
	CTuningParams StandardTuning;
	if(CurrentServerInfo.m_aGameType[0] != '0')
	{
		if(str_comp(CurrentServerInfo.m_aGameType, "DM") != 0 && str_comp(CurrentServerInfo.m_aGameType, "TDM") != 0 && str_comp(CurrentServerInfo.m_aGameType, "CTF") != 0)
			m_ServerMode = SERVERMODE_MOD;
		else if(mem_comp(&StandardTuning, &m_aTuning[g_Config.m_ClDummy], 33) == 0)
			m_ServerMode = SERVERMODE_PURE;
		else
			m_ServerMode = SERVERMODE_PUREMOD;
	}

	// add tuning to demo
	bool AnyRecording = false;
	for(int i = 0; i < RECORDER_MAX; i++)
		if(DemoRecorder(i)->IsRecording())
		{
			AnyRecording = true;
			break;
		}
	if(AnyRecording && mem_comp(&StandardTuning, &m_aTuning[g_Config.m_ClDummy], sizeof(CTuningParams)) != 0)
	{
		CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
		int *pParams = (int *)&m_aTuning[g_Config.m_ClDummy];
		for(unsigned i = 0; i < sizeof(m_aTuning[0]) / sizeof(int); i++)
			Msg.AddInt(pParams[i]);
		Client()->SendMsgActive(&Msg, MSGFLAG_RECORD | MSGFLAG_NOSEND);
	}

	for(int i = 0; i < 2; i++)
	{
		if(m_aDDRaceMsgSent[i] || !m_Snap.m_pLocalInfo)
		{
			continue;
		}
		if(i == IClient::CONN_DUMMY && !Client()->DummyConnected())
		{
			continue;
		}
		CMsgPacker Msg(NETMSGTYPE_CL_ISDDNETLEGACY, false);
		Msg.AddInt(DDNetVersion());
		Client()->SendMsg(i, &Msg, MSGFLAG_VITAL);
		m_aDDRaceMsgSent[i] = true;
	}

	if(m_Snap.m_SpecInfo.m_Active && m_MultiViewActivated)
	{
		// dont show other teams while spectating in multi view
		CNetMsg_Cl_ShowOthers Msg;
		Msg.m_Show = SHOW_OTHERS_ONLY_TEAM;
		Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);

		// update state
		m_aShowOthers[g_Config.m_ClDummy] = SHOW_OTHERS_ONLY_TEAM;
	}
	else if(m_aShowOthers[g_Config.m_ClDummy] == SHOW_OTHERS_NOT_SET || m_aShowOthers[g_Config.m_ClDummy] != g_Config.m_ClShowOthers)
	{
		{
			CNetMsg_Cl_ShowOthers Msg;
			Msg.m_Show = g_Config.m_ClShowOthers;
			Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);
		}

		// update state
		m_aShowOthers[g_Config.m_ClDummy] = g_Config.m_ClShowOthers;
	}

	float ZoomToSend = m_Camera.m_Zoom;
	if(m_Camera.m_Zooming)
	{
		if(m_Camera.m_ZoomSmoothingTarget > m_Camera.m_Zoom) // Zooming out
			ZoomToSend = m_Camera.m_ZoomSmoothingTarget;
		else if(m_Camera.m_ZoomSmoothingTarget < m_Camera.m_Zoom && m_LastZoom > 0) // Zooming in
			ZoomToSend = m_LastZoom;
	}

	if(ZoomToSend != m_LastZoom || Graphics()->ScreenAspect() != m_LastScreenAspect || (Client()->DummyConnected() && !m_LastDummyConnected))
	{
		CNetMsg_Cl_ShowDistance Msg;
		float x, y;
		RenderTools()->CalcScreenParams(Graphics()->ScreenAspect(), ZoomToSend, &x, &y);
		Msg.m_X = x;
		Msg.m_Y = y;
		Client()->ChecksumData()->m_Zoom = ZoomToSend;
		CMsgPacker Packer(&Msg);
		Msg.Pack(&Packer);
		if(ZoomToSend != m_LastZoom)
			Client()->SendMsg(IClient::CONN_MAIN, &Packer, MSGFLAG_VITAL);
		if(Client()->DummyConnected())
			Client()->SendMsg(IClient::CONN_DUMMY, &Packer, MSGFLAG_VITAL);
		m_LastZoom = ZoomToSend;
		m_LastScreenAspect = Graphics()->ScreenAspect();
	}
	m_LastDummyConnected = Client()->DummyConnected();

	for(auto &pComponent : m_vpAll)
		pComponent->OnNewSnapshot();

	// notify editor when local character moved
	UpdateEditorIngameMoved();

	// detect air jump for other players
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(m_Snap.m_aCharacters[i].m_Active && (m_Snap.m_aCharacters[i].m_Cur.m_Jumped & 2) && !(m_Snap.m_aCharacters[i].m_Prev.m_Jumped & 2))
			if(!Predict() || (i != m_Snap.m_LocalClientID && (!AntiPingPlayers() || i != m_PredictedDummyID)))
			{
				vec2 Pos = mix(vec2(m_Snap.m_aCharacters[i].m_Prev.m_X, m_Snap.m_aCharacters[i].m_Prev.m_Y),
					vec2(m_Snap.m_aCharacters[i].m_Cur.m_X, m_Snap.m_aCharacters[i].m_Cur.m_Y),
					Client()->IntraGameTick(g_Config.m_ClDummy));
				float Alpha = 1.0f;
				bool SameTeam = m_Teams.SameTeam(m_Snap.m_LocalClientID, i);
				if(!SameTeam || m_aClients[i].m_Solo || m_aClients[m_Snap.m_LocalClientID].m_Solo)
					Alpha = g_Config.m_ClShowOthersAlpha / 100.0f;
				m_Effects.AirJump(Pos, Alpha);
			}

	if(m_Snap.m_LocalClientID != m_PrevLocalID)
		m_PredictedDummyID = m_PrevLocalID;
	m_PrevLocalID = m_Snap.m_LocalClientID;
	m_IsDummySwapping = 0;

	SnapCollectEntities(); // creates a collection that associates EntityEx snap items with the entities they belong to

	// update prediction data
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
		UpdatePrediction();
}

void CGameClient::UpdateEditorIngameMoved()
{
	const bool LocalCharacterMoved = m_Snap.m_pLocalCharacter && m_Snap.m_pLocalPrevCharacter && (m_Snap.m_pLocalCharacter->m_X != m_Snap.m_pLocalPrevCharacter->m_X || m_Snap.m_pLocalCharacter->m_Y != m_Snap.m_pLocalPrevCharacter->m_Y);
	if(!g_Config.m_ClEditor)
	{
		m_EditorMovementDelay = 5;
	}
	else if(m_EditorMovementDelay > 0 && !LocalCharacterMoved)
	{
		--m_EditorMovementDelay;
	}
	if(m_EditorMovementDelay == 0 && LocalCharacterMoved)
	{
		Editor()->OnIngameMoved();
	}
}

void CGameClient::OnPredict()
{
	// store the previous values so we can detect prediction errors
	CCharacterCore BeforePrevChar = m_PredictedPrevChar;
	CCharacterCore BeforeChar = m_PredictedChar;

	// we can't predict without our own id or own character
	if(m_Snap.m_LocalClientID == -1 || !m_Snap.m_aCharacters[m_Snap.m_LocalClientID].m_Active)
		return;

	// don't predict anything if we are paused
	if(m_Snap.m_pGameInfoObj && m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED)
	{
		if(m_Snap.m_pLocalCharacter)
		{
			m_PredictedChar.Read(m_Snap.m_pLocalCharacter);
			m_PredictedChar.m_ActiveWeapon = m_Snap.m_pLocalCharacter->m_Weapon;
		}
		if(m_Snap.m_pLocalPrevCharacter)
		{
			m_PredictedPrevChar.Read(m_Snap.m_pLocalPrevCharacter);
			m_PredictedPrevChar.m_ActiveWeapon = m_Snap.m_pLocalPrevCharacter->m_Weapon;
		}
		return;
	}

	vec2 aBeforeRender[MAX_CLIENTS];
	for(int i = 0; i < MAX_CLIENTS; i++)
		aBeforeRender[i] = GetSmoothPos(i);

	// init
	bool Dummy = g_Config.m_ClDummy ^ m_IsDummySwapping;
	m_PredictedWorld.CopyWorld(&m_GameWorld);

	// don't predict inactive players, or entities from other teams
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(CCharacter *pChar = m_PredictedWorld.GetCharacterByID(i))
			if((!m_Snap.m_aCharacters[i].m_Active && pChar->m_SnapTicks > 10) || IsOtherTeam(i))
				pChar->Destroy();

	CProjectile *pProjNext = 0;
	for(CProjectile *pProj = (CProjectile *)m_PredictedWorld.FindFirst(CGameWorld::ENTTYPE_PROJECTILE); pProj; pProj = pProjNext)
	{
		pProjNext = (CProjectile *)pProj->TypeNext();
		if(IsOtherTeam(pProj->GetOwner()))
		{
			pProj->Destroy();
		}
	}

	CCharacter *pLocalChar = m_PredictedWorld.GetCharacterByID(m_Snap.m_LocalClientID);
	if(!pLocalChar)
		return;
	CCharacter *pDummyChar = 0;
	if(PredictDummy())
		pDummyChar = m_PredictedWorld.GetCharacterByID(m_PredictedDummyID);

	// predict
	for(int Tick = Client()->GameTick(g_Config.m_ClDummy) + 1; Tick <= Client()->PredGameTick(g_Config.m_ClDummy); Tick++)
	{
		// fetch the previous characters
		if(Tick == Client()->PredGameTick(g_Config.m_ClDummy))
		{
			m_PrevPredictedWorld.CopyWorld(&m_PredictedWorld);
			m_PredictedPrevChar = pLocalChar->GetCore();
			for(int i = 0; i < MAX_CLIENTS; i++)
				if(CCharacter *pChar = m_PredictedWorld.GetCharacterByID(i))
					m_aClients[i].m_PrevPredicted = pChar->GetCore();
		}

		// optionally allow some movement in freeze by not predicting freeze the last one to two ticks
		if(g_Config.m_ClPredictFreeze == 2 && Client()->PredGameTick(g_Config.m_ClDummy) - 1 - Client()->PredGameTick(g_Config.m_ClDummy) % 2 <= Tick)
			pLocalChar->m_CanMoveInFreeze = true;

		// apply inputs and tick
		CNetObj_PlayerInput *pInputData = (CNetObj_PlayerInput *)Client()->GetInput(Tick, m_IsDummySwapping);
		CNetObj_PlayerInput *pDummyInputData = !pDummyChar ? 0 : (CNetObj_PlayerInput *)Client()->GetInput(Tick, m_IsDummySwapping ^ 1);
		bool DummyFirst = pInputData && pDummyInputData && pDummyChar->GetCID() < pLocalChar->GetCID();

		if(DummyFirst)
			pDummyChar->OnDirectInput(pDummyInputData);
		if(pInputData)
			pLocalChar->OnDirectInput(pInputData);
		if(pDummyInputData && !DummyFirst)
			pDummyChar->OnDirectInput(pDummyInputData);
		m_PredictedWorld.m_GameTick = Tick;
		if(pInputData)
			pLocalChar->OnPredictedInput(pInputData);
		if(pDummyInputData)
			pDummyChar->OnPredictedInput(pDummyInputData);
		m_PredictedWorld.Tick();

		// fetch the current characters
		if(Tick == Client()->PredGameTick(g_Config.m_ClDummy))
		{
			m_PredictedChar = pLocalChar->GetCore();
			for(int i = 0; i < MAX_CLIENTS; i++)
				if(CCharacter *pChar = m_PredictedWorld.GetCharacterByID(i))
					m_aClients[i].m_Predicted = pChar->GetCore();
		}

		for(int i = 0; i < MAX_CLIENTS; i++)
			if(CCharacter *pChar = m_PredictedWorld.GetCharacterByID(i))
			{
				m_aClients[i].m_aPredPos[Tick % 200] = pChar->Core()->m_Pos;
				m_aClients[i].m_aPredTick[Tick % 200] = Tick;
			}

		// check if we want to trigger effects
		if(Tick > m_aLastNewPredictedTick[Dummy])
		{
			m_aLastNewPredictedTick[Dummy] = Tick;
			m_NewPredictedTick = true;
			vec2 Pos = pLocalChar->Core()->m_Pos;
			int Events = pLocalChar->Core()->m_TriggeredEvents;
			if(g_Config.m_ClPredict && !m_SuppressEvents)
				if(Events & COREEVENT_AIR_JUMP)
					m_Effects.AirJump(Pos, 1.0f);
			if(g_Config.m_SndGame && !m_SuppressEvents)
			{
				if(Events & COREEVENT_GROUND_JUMP)
					m_Sounds.PlayAndRecord(CSounds::CHN_WORLD, SOUND_PLAYER_JUMP, 1.0f, Pos);
				if(Events & COREEVENT_HOOK_ATTACH_GROUND)
					m_Sounds.PlayAndRecord(CSounds::CHN_WORLD, SOUND_HOOK_ATTACH_GROUND, 1.0f, Pos);
				if(Events & COREEVENT_HOOK_HIT_NOHOOK)
					m_Sounds.PlayAndRecord(CSounds::CHN_WORLD, SOUND_HOOK_NOATTACH, 1.0f, Pos);
			}
		}

		// check if we want to trigger predicted airjump for dummy
		if(AntiPingPlayers() && pDummyChar && Tick > m_aLastNewPredictedTick[!Dummy])
		{
			m_aLastNewPredictedTick[!Dummy] = Tick;
			vec2 Pos = pDummyChar->Core()->m_Pos;
			int Events = pDummyChar->Core()->m_TriggeredEvents;
			if(g_Config.m_ClPredict && !m_SuppressEvents)
				if(Events & COREEVENT_AIR_JUMP)
					m_Effects.AirJump(Pos, 1.0f);
		}
	}

	// detect mispredictions of other players and make corrections smoother when possible
	if(g_Config.m_ClAntiPingSmooth && Predict() && AntiPingPlayers() && m_NewTick && absolute(m_PredictedTick - Client()->PredGameTick(g_Config.m_ClDummy)) <= 1 && absolute(Client()->GameTick(g_Config.m_ClDummy) - Client()->PrevGameTick(g_Config.m_ClDummy)) <= 2)
	{
		int PredTime = clamp(Client()->GetPredictionTime(), 0, 800);
		float SmoothPace = 4 - 1.5f * PredTime / 800.f; // smoothing pace (a lower value will make the smoothing quicker)
		int64_t Len = 1000 * PredTime * SmoothPace;

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!m_Snap.m_aCharacters[i].m_Active || i == m_Snap.m_LocalClientID || !m_aLastActive[i])
				continue;
			vec2 NewPos = (m_PredictedTick == Client()->PredGameTick(g_Config.m_ClDummy)) ? m_aClients[i].m_Predicted.m_Pos : m_aClients[i].m_PrevPredicted.m_Pos;
			vec2 PredErr = (m_aLastPos[i] - NewPos) / (float)minimum(Client()->GetPredictionTime(), 200);
			if(in_range(length(PredErr), 0.05f, 5.f))
			{
				vec2 PredPos = mix(m_aClients[i].m_PrevPredicted.m_Pos, m_aClients[i].m_Predicted.m_Pos, Client()->PredIntraGameTick(g_Config.m_ClDummy));
				vec2 CurPos = mix(
					vec2(m_Snap.m_aCharacters[i].m_Prev.m_X, m_Snap.m_aCharacters[i].m_Prev.m_Y),
					vec2(m_Snap.m_aCharacters[i].m_Cur.m_X, m_Snap.m_aCharacters[i].m_Cur.m_Y),
					Client()->IntraGameTick(g_Config.m_ClDummy));
				vec2 RenderDiff = PredPos - aBeforeRender[i];
				vec2 PredDiff = PredPos - CurPos;

				float aMixAmount[2];
				for(int j = 0; j < 2; j++)
				{
					aMixAmount[j] = 1.0f;
					if(absolute(PredErr[j]) > 0.05f)
					{
						aMixAmount[j] = 0.0f;
						if(absolute(RenderDiff[j]) > 0.01f)
						{
							aMixAmount[j] = 1.f - clamp(RenderDiff[j] / PredDiff[j], 0.f, 1.f);
							aMixAmount[j] = 1.f - std::pow(1.f - aMixAmount[j], 1 / 1.2f);
						}
					}
					int64_t TimePassed = time_get() - m_aClients[i].m_aSmoothStart[j];
					if(in_range(TimePassed, (int64_t)0, Len - 1))
						aMixAmount[j] = minimum(aMixAmount[j], (float)(TimePassed / (double)Len));
				}
				for(int j = 0; j < 2; j++)
					if(absolute(RenderDiff[j]) < 0.01f && absolute(PredDiff[j]) < 0.01f && absolute(m_aClients[i].m_PrevPredicted.m_Pos[j] - m_aClients[i].m_Predicted.m_Pos[j]) < 0.01f && aMixAmount[j] > aMixAmount[j ^ 1])
						aMixAmount[j] = aMixAmount[j ^ 1];
				for(int j = 0; j < 2; j++)
				{
					int64_t Remaining = minimum((1.f - aMixAmount[j]) * Len, minimum(time_freq() * 0.700f, (1.f - aMixAmount[j ^ 1]) * Len + time_freq() * 0.300f)); // don't smooth for longer than 700ms, or more than 300ms longer along one axis than the other axis
					int64_t Start = time_get() - (Len - Remaining);
					if(!in_range(Start + Len, m_aClients[i].m_aSmoothStart[j], m_aClients[i].m_aSmoothStart[j] + Len))
					{
						m_aClients[i].m_aSmoothStart[j] = Start;
						m_aClients[i].m_aSmoothLen[j] = Len;
					}
				}
			}
		}
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_Snap.m_aCharacters[i].m_Active)
		{
			m_aLastPos[i] = m_aClients[i].m_Predicted.m_Pos;
			m_aLastActive[i] = true;
		}
		else
			m_aLastActive[i] = false;
	}

	if(g_Config.m_Debug && g_Config.m_ClPredict && m_PredictedTick == Client()->PredGameTick(g_Config.m_ClDummy))
	{
		CNetObj_CharacterCore Before = {0}, Now = {0}, BeforePrev = {0}, NowPrev = {0};
		BeforeChar.Write(&Before);
		BeforePrevChar.Write(&BeforePrev);
		m_PredictedChar.Write(&Now);
		m_PredictedPrevChar.Write(&NowPrev);

		if(mem_comp(&Before, &Now, sizeof(CNetObj_CharacterCore)) != 0)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", "prediction error");
			for(unsigned i = 0; i < sizeof(CNetObj_CharacterCore) / sizeof(int); i++)
				if(((int *)&Before)[i] != ((int *)&Now)[i])
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "	%d %d %d (%d %d)", i, ((int *)&Before)[i], ((int *)&Now)[i], ((int *)&BeforePrev)[i], ((int *)&NowPrev)[i]);
					Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
				}
		}
	}

	m_PredictedTick = Client()->PredGameTick(g_Config.m_ClDummy);

	if(m_NewPredictedTick)
		m_Ghost.OnNewPredictedSnapshot();
}

void CGameClient::OnActivateEditor()
{
	OnRelease();
}

CGameClient::CClientStats::CClientStats()
{
	Reset();
}

void CGameClient::CClientStats::Reset()
{
	m_JoinTick = 0;
	m_IngameTicks = 0;
	m_Active = false;
	m_Frags = 0;
	m_Deaths = 0;
	m_Suicides = 0;
	m_BestSpree = 0;
	m_CurrentSpree = 0;
	for(int j = 0; j < NUM_WEAPONS; j++)
	{
		m_aFragsWith[j] = 0;
		m_aDeathsFrom[j] = 0;
	}
	m_FlagGrabs = 0;
	m_FlagCaptures = 0;
}

void CGameClient::CClientData::UpdateRenderInfo(bool IsTeamPlay)
{
	m_RenderInfo = m_SkinInfo;

	// force team colors
	if(IsTeamPlay)
	{
		m_RenderInfo.m_CustomColoredSkin = true;
		const int aTeamColors[2] = {65461, 10223541};
		if(m_Team >= TEAM_RED && m_Team <= TEAM_BLUE)
		{
			m_RenderInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(aTeamColors[m_Team]));
			m_RenderInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(aTeamColors[m_Team]));
		}
		else
		{
			m_RenderInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(12829350));
			m_RenderInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(12829350));
		}
	}
}

void CGameClient::CClientData::Reset()
{
	m_aName[0] = 0;
	m_aClan[0] = 0;
	m_Country = -1;
	m_aSkinName[0] = '\0';
	m_SkinColor = 0;
	m_Team = 0;
	m_Angle = 0;
	m_Emoticon = 0;
	m_EmoticonStartTick = -1;
	m_EmoticonStartFraction = 0;
	m_Active = false;
	m_ChatIgnore = false;
	m_EmoticonIgnore = false;
	m_Friend = false;
	m_Foe = false;
	m_AuthLevel = AUTHED_NO;
	m_Afk = false;
	m_Paused = false;
	m_Spec = false;
	m_SkinInfo.m_BloodColor = ColorRGBA(1, 1, 1);
	m_SkinInfo.m_ColorableRenderSkin.Reset();
	m_SkinInfo.m_OriginalRenderSkin.Reset();
	m_SkinInfo.m_CustomColoredSkin = false;
	m_SkinInfo.m_ColorBody = ColorRGBA(1, 1, 1);
	m_SkinInfo.m_ColorFeet = ColorRGBA(1, 1, 1);
	m_SkinInfo.m_SkinMetrics.Reset();

	m_Solo = false;
	m_Jetpack = false;
	m_CollisionDisabled = false;
	m_EndlessHook = false;
	m_EndlessJump = false;
	m_HammerHitDisabled = false;
	m_GrenadeHitDisabled = false;
	m_LaserHitDisabled = false;
	m_ShotgunHitDisabled = false;
	m_HookHitDisabled = false;
	m_Super = false;
	m_HasTelegunGun = false;
	m_HasTelegunGrenade = false;
	m_HasTelegunLaser = false;
	m_FreezeEnd = 0;
	m_DeepFrozen = false;
	m_LiveFrozen = false;

	m_Evolved.m_Tick = -1;

	m_SpecChar = vec2(0, 0);
	m_SpecCharPresent = false;

	mem_zero(m_aSwitchStates, sizeof(m_aSwitchStates));

	UpdateRenderInfo(false);
}

void CGameClient::SendSwitchTeam(int Team)
{
	CNetMsg_Cl_SetTeam Msg;
	Msg.m_Team = Team;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);

	if(Team != TEAM_SPECTATORS)
		m_Camera.OnReset();
}

void CGameClient::SendInfo(bool Start)
{
	if(Start)
	{
		CNetMsg_Cl_StartInfo Msg;
		Msg.m_pName = Client()->PlayerName();
		Msg.m_pClan = g_Config.m_PlayerClan;
		Msg.m_Country = g_Config.m_PlayerCountry;
		Msg.m_pSkin = g_Config.m_ClPlayerSkin;
		Msg.m_UseCustomColor = g_Config.m_ClPlayerUseCustomColor;
		Msg.m_ColorBody = g_Config.m_ClPlayerColorBody;
		Msg.m_ColorFeet = g_Config.m_ClPlayerColorFeet;
		CMsgPacker Packer(&Msg);
		Msg.Pack(&Packer);
		Client()->SendMsg(IClient::CONN_MAIN, &Packer, MSGFLAG_VITAL | MSGFLAG_FLUSH);
		m_aCheckInfo[0] = -1;
	}
	else
	{
		CNetMsg_Cl_ChangeInfo Msg;
		Msg.m_pName = Client()->PlayerName();
		Msg.m_pClan = g_Config.m_PlayerClan;
		Msg.m_Country = g_Config.m_PlayerCountry;
		Msg.m_pSkin = g_Config.m_ClPlayerSkin;
		Msg.m_UseCustomColor = g_Config.m_ClPlayerUseCustomColor;
		Msg.m_ColorBody = g_Config.m_ClPlayerColorBody;
		Msg.m_ColorFeet = g_Config.m_ClPlayerColorFeet;
		CMsgPacker Packer(&Msg);
		Msg.Pack(&Packer);
		Client()->SendMsg(IClient::CONN_MAIN, &Packer, MSGFLAG_VITAL);
		m_aCheckInfo[0] = Client()->GameTickSpeed();
	}
}

void CGameClient::SendDummyInfo(bool Start)
{
	if(Start)
	{
		CNetMsg_Cl_StartInfo Msg;
		Msg.m_pName = Client()->DummyName();
		Msg.m_pClan = g_Config.m_ClDummyClan;
		Msg.m_Country = g_Config.m_ClDummyCountry;
		Msg.m_pSkin = g_Config.m_ClDummySkin;
		Msg.m_UseCustomColor = g_Config.m_ClDummyUseCustomColor;
		Msg.m_ColorBody = g_Config.m_ClDummyColorBody;
		Msg.m_ColorFeet = g_Config.m_ClDummyColorFeet;
		CMsgPacker Packer(&Msg);
		Msg.Pack(&Packer);
		Client()->SendMsg(IClient::CONN_DUMMY, &Packer, MSGFLAG_VITAL);
		m_aCheckInfo[1] = -1;
	}
	else
	{
		CNetMsg_Cl_ChangeInfo Msg;
		Msg.m_pName = Client()->DummyName();
		Msg.m_pClan = g_Config.m_ClDummyClan;
		Msg.m_Country = g_Config.m_ClDummyCountry;
		Msg.m_pSkin = g_Config.m_ClDummySkin;
		Msg.m_UseCustomColor = g_Config.m_ClDummyUseCustomColor;
		Msg.m_ColorBody = g_Config.m_ClDummyColorBody;
		Msg.m_ColorFeet = g_Config.m_ClDummyColorFeet;
		CMsgPacker Packer(&Msg);
		Msg.Pack(&Packer);
		Client()->SendMsg(IClient::CONN_DUMMY, &Packer, MSGFLAG_VITAL);
		m_aCheckInfo[1] = Client()->GameTickSpeed();
	}
}

void CGameClient::SendKill(int ClientID) const
{
	CNetMsg_Cl_Kill Msg;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);

	if(g_Config.m_ClDummyCopyMoves)
	{
		CMsgPacker MsgP(NETMSGTYPE_CL_KILL, false);
		Client()->SendMsg(!g_Config.m_ClDummy, &MsgP, MSGFLAG_VITAL);
	}
}

void CGameClient::ConTeam(IConsole::IResult *pResult, void *pUserData)
{
	((CGameClient *)pUserData)->SendSwitchTeam(pResult->GetInteger(0));
}

void CGameClient::ConKill(IConsole::IResult *pResult, void *pUserData)
{
	((CGameClient *)pUserData)->SendKill(-1);
}

void CGameClient::ConchainLanguageUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		((CGameClient *)pUserData)->OnLanguageChange();
}

void CGameClient::ConchainSpecialInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		((CGameClient *)pUserData)->SendInfo(false);
}

void CGameClient::ConchainSpecialDummyInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		((CGameClient *)pUserData)->SendDummyInfo(false);
}

void CGameClient::ConchainSpecialDummy(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		if(g_Config.m_ClDummy && !((CGameClient *)pUserData)->Client()->DummyConnected())
			g_Config.m_ClDummy = 0;
}

void CGameClient::ConchainClTextEntitiesSize(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);

	if(pResult->NumArguments())
	{
		CGameClient *pGameClient = (CGameClient *)pUserData;
		pGameClient->m_MapImages.SetTextureScale(g_Config.m_ClTextEntitiesSize);
	}
}

IGameClient *CreateGameClient()
{
	return new CGameClient();
}

int CGameClient::IntersectCharacter(vec2 HookPos, vec2 NewPos, vec2 &NewPos2, int ownID)
{
	float Distance = 0.0f;
	int ClosestID = -1;

	const CClientData &OwnClientData = m_aClients[ownID];

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == ownID)
			continue;

		const CClientData &cData = m_aClients[i];

		if(!cData.m_Active)
			continue;

		CNetObj_Character Prev = m_Snap.m_aCharacters[i].m_Prev;
		CNetObj_Character Player = m_Snap.m_aCharacters[i].m_Cur;

		vec2 Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), Client()->IntraGameTick(g_Config.m_ClDummy));

		bool IsOneSuper = cData.m_Super || OwnClientData.m_Super;
		bool IsOneSolo = cData.m_Solo || OwnClientData.m_Solo;

		if(!IsOneSuper && (!m_Teams.SameTeam(i, ownID) || IsOneSolo || OwnClientData.m_HookHitDisabled))
			continue;

		vec2 ClosestPoint;
		if(closest_point_on_line(HookPos, NewPos, Position, ClosestPoint))
		{
			if(distance(Position, ClosestPoint) < CCharacterCore::PhysicalSize() + 2.0f)
			{
				if(ClosestID == -1 || distance(HookPos, Position) < Distance)
				{
					NewPos2 = ClosestPoint;
					ClosestID = i;
					Distance = distance(HookPos, Position);
				}
			}
		}
	}

	return ClosestID;
}

ColorRGBA CalculateNameColor(ColorHSLA TextColorHSL)
{
	return color_cast<ColorRGBA>(ColorHSLA(TextColorHSL.h, TextColorHSL.s * 0.68f, TextColorHSL.l * 0.81f));
}

void CGameClient::UpdatePrediction()
{
	m_GameWorld.m_WorldConfig.m_IsVanilla = m_GameInfo.m_PredictVanilla;
	m_GameWorld.m_WorldConfig.m_IsDDRace = m_GameInfo.m_PredictDDRace;
	m_GameWorld.m_WorldConfig.m_IsFNG = m_GameInfo.m_PredictFNG;
	m_GameWorld.m_WorldConfig.m_PredictDDRace = m_GameInfo.m_PredictDDRace;
	m_GameWorld.m_WorldConfig.m_PredictTiles = m_GameInfo.m_PredictDDRace && m_GameInfo.m_PredictDDRaceTiles;
	m_GameWorld.m_WorldConfig.m_UseTuneZones = m_GameInfo.m_PredictDDRaceTiles;
	m_GameWorld.m_WorldConfig.m_PredictFreeze = g_Config.m_ClPredictFreeze;
	m_GameWorld.m_WorldConfig.m_PredictWeapons = AntiPingWeapons();
	m_GameWorld.m_WorldConfig.m_BugDDRaceInput = m_GameInfo.m_BugDDRaceInput;
	m_GameWorld.m_WorldConfig.m_NoWeakHookAndBounce = m_GameInfo.m_NoWeakHookAndBounce;

	// always update default tune zone, even without character
	if(!m_GameWorld.m_WorldConfig.m_UseTuneZones)
		m_GameWorld.TuningList()[0] = m_aTuning[g_Config.m_ClDummy];

	if(!m_Snap.m_pLocalCharacter)
	{
		if(CCharacter *pLocalChar = m_GameWorld.GetCharacterByID(m_Snap.m_LocalClientID))
			pLocalChar->Destroy();
		return;
	}

	if(m_Snap.m_pLocalCharacter->m_AmmoCount > 0 && m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_NINJA)
		m_GameWorld.m_WorldConfig.m_InfiniteAmmo = false;
	m_GameWorld.m_WorldConfig.m_IsSolo = !m_Snap.m_aCharacters[m_Snap.m_LocalClientID].m_HasExtendedData && !m_aTuning[g_Config.m_ClDummy].m_PlayerCollision && !m_aTuning[g_Config.m_ClDummy].m_PlayerHooking;

	// update the tuning/tunezone at the local character position with the latest tunings received before the new snapshot
	vec2 LocalCharPos = vec2(m_Snap.m_pLocalCharacter->m_X, m_Snap.m_pLocalCharacter->m_Y);
	m_GameWorld.m_Core.m_aTuning[g_Config.m_ClDummy] = m_aTuning[g_Config.m_ClDummy];

	if(m_GameWorld.m_WorldConfig.m_UseTuneZones)
	{
		int TuneZone = Collision()->IsTune(Collision()->GetMapIndex(LocalCharPos));

		if(TuneZone != m_aLocalTuneZone[g_Config.m_ClDummy])
		{
			// our tunezone changed, expecting tuning message
			m_aLocalTuneZone[g_Config.m_ClDummy] = m_aExpectingTuningForZone[g_Config.m_ClDummy] = TuneZone;
			m_aExpectingTuningSince[g_Config.m_ClDummy] = 0;
		}

		if(m_aExpectingTuningForZone[g_Config.m_ClDummy] >= 0)
		{
			if(m_aReceivedTuning[g_Config.m_ClDummy])
			{
				m_GameWorld.TuningList()[m_aExpectingTuningForZone[g_Config.m_ClDummy]] = m_aTuning[g_Config.m_ClDummy];
				m_aReceivedTuning[g_Config.m_ClDummy] = false;
				m_aExpectingTuningForZone[g_Config.m_ClDummy] = -1;
			}
			else if(m_aExpectingTuningSince[g_Config.m_ClDummy] >= 5)
			{
				// if we are expecting tuning for more than 10 snaps (less than a quarter of a second)
				// it is probably dropped or it was received out of order
				// or applied to another tunezone.
				// we need to fallback to current tuning to fix ourselves.
				m_aExpectingTuningForZone[g_Config.m_ClDummy] = -1;
				m_aExpectingTuningSince[g_Config.m_ClDummy] = 0;
				m_aReceivedTuning[g_Config.m_ClDummy] = false;
				dbg_msg("tunezone", "the tuning was missed");
			}
			else
			{
				// if we are expecting tuning and have not received one yet.
				// do not update any tuning, so we don't apply it to the wrong tunezone.
				dbg_msg("tunezone", "waiting for tuning for zone %d", m_aExpectingTuningForZone[g_Config.m_ClDummy]);
				m_aExpectingTuningSince[g_Config.m_ClDummy]++;
			}
		}
		else
		{
			// if we have processed what we need, and the tuning is still wrong due to out of order messege
			// fix our tuning by using the current one
			m_GameWorld.TuningList()[TuneZone] = m_aTuning[g_Config.m_ClDummy];
			m_aExpectingTuningSince[g_Config.m_ClDummy] = 0;
			m_aReceivedTuning[g_Config.m_ClDummy] = false;
		}
	}

	// if ddnetcharacter is available, ignore server-wide tunings for hook and collision
	if(m_Snap.m_aCharacters[m_Snap.m_LocalClientID].m_HasExtendedData)
	{
		m_GameWorld.m_Core.m_aTuning[g_Config.m_ClDummy].m_PlayerCollision = 1;
		m_GameWorld.m_Core.m_aTuning[g_Config.m_ClDummy].m_PlayerHooking = 1;
	}

	CCharacter *pLocalChar = m_GameWorld.GetCharacterByID(m_Snap.m_LocalClientID);
	CCharacter *pDummyChar = 0;
	if(PredictDummy())
		pDummyChar = m_GameWorld.GetCharacterByID(m_PredictedDummyID);

	// update strong and weak hook
	if(pLocalChar && !m_Snap.m_SpecInfo.m_Active && Client()->State() != IClient::STATE_DEMOPLAYBACK && (m_aTuning[g_Config.m_ClDummy].m_PlayerCollision || m_aTuning[g_Config.m_ClDummy].m_PlayerHooking))
	{
		if(m_Snap.m_aCharacters[m_Snap.m_LocalClientID].m_HasExtendedData)
		{
			int aIDs[MAX_CLIENTS];
			for(int &ID : aIDs)
				ID = -1;
			for(int i = 0; i < MAX_CLIENTS; i++)
				if(CCharacter *pChar = m_GameWorld.GetCharacterByID(i))
					aIDs[pChar->GetStrongWeakID()] = i;
			for(int ID : aIDs)
				if(ID >= 0)
					m_CharOrder.GiveStrong(ID);
		}
		else
		{
			// manual detection
			DetectStrongHook();
		}
		for(int i : m_CharOrder.m_IDs)
		{
			if(CCharacter *pChar = m_GameWorld.GetCharacterByID(i))
			{
				m_GameWorld.RemoveEntity(pChar);
				m_GameWorld.InsertEntity(pChar);
			}
		}
	}

	// advance the gameworld to the current gametick
	if(pLocalChar && absolute(m_GameWorld.GameTick() - Client()->GameTick(g_Config.m_ClDummy)) < Client()->GameTickSpeed())
	{
		for(int Tick = m_GameWorld.GameTick() + 1; Tick <= Client()->GameTick(g_Config.m_ClDummy); Tick++)
		{
			CNetObj_PlayerInput *pInput = (CNetObj_PlayerInput *)Client()->GetInput(Tick);
			CNetObj_PlayerInput *pDummyInput = 0;
			if(pDummyChar)
				pDummyInput = (CNetObj_PlayerInput *)Client()->GetInput(Tick, 1);
			if(pInput)
				pLocalChar->OnDirectInput(pInput);
			if(pDummyInput)
				pDummyChar->OnDirectInput(pDummyInput);
			m_GameWorld.m_GameTick = Tick;
			if(pInput)
				pLocalChar->OnPredictedInput(pInput);
			if(pDummyInput)
				pDummyChar->OnPredictedInput(pDummyInput);
			m_GameWorld.Tick();

			for(int i = 0; i < MAX_CLIENTS; i++)
				if(CCharacter *pChar = m_GameWorld.GetCharacterByID(i))
				{
					m_aClients[i].m_aPredPos[Tick % 200] = pChar->Core()->m_Pos;
					m_aClients[i].m_aPredTick[Tick % 200] = Tick;
				}
		}
	}
	else
	{
		// skip to current gametick
		m_GameWorld.m_GameTick = Client()->GameTick(g_Config.m_ClDummy);
		if(pLocalChar)
			if(CNetObj_PlayerInput *pInput = (CNetObj_PlayerInput *)Client()->GetInput(Client()->GameTick(g_Config.m_ClDummy)))
				pLocalChar->SetInput(pInput);
		if(pDummyChar)
			if(CNetObj_PlayerInput *pInput = (CNetObj_PlayerInput *)Client()->GetInput(Client()->GameTick(g_Config.m_ClDummy), 1))
				pDummyChar->SetInput(pInput);
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
		if(CCharacter *pChar = m_GameWorld.GetCharacterByID(i))
		{
			m_aClients[i].m_aPredPos[Client()->GameTick(g_Config.m_ClDummy) % 200] = pChar->Core()->m_Pos;
			m_aClients[i].m_aPredTick[Client()->GameTick(g_Config.m_ClDummy) % 200] = Client()->GameTick(g_Config.m_ClDummy);
		}

	// update the local gameworld with the new snapshot
	m_GameWorld.NetObjBegin(m_Teams, m_Snap.m_LocalClientID);

	for(int i = 0; i < MAX_CLIENTS; i++)
		if(m_Snap.m_aCharacters[i].m_Active)
		{
			bool IsLocal = (i == m_Snap.m_LocalClientID || (PredictDummy() && i == m_PredictedDummyID));
			int GameTeam = (m_Snap.m_pGameInfoObj && (m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS)) ? m_aClients[i].m_Team : i;
			m_GameWorld.NetCharAdd(i, &m_Snap.m_aCharacters[i].m_Cur,
				m_Snap.m_aCharacters[i].m_HasExtendedData ? &m_Snap.m_aCharacters[i].m_ExtendedData : 0,
				GameTeam, IsLocal);
		}

	for(const CSnapEntities &EntData : SnapEntities())
		m_GameWorld.NetObjAdd(EntData.m_Item.m_ID, EntData.m_Item.m_Type, EntData.m_pData, EntData.m_pDataEx);

	m_GameWorld.NetObjEnd();
}

void CGameClient::UpdateRenderedCharacters()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_Snap.m_aCharacters[i].m_Active)
			continue;
		m_aClients[i].m_RenderCur = m_Snap.m_aCharacters[i].m_Cur;
		m_aClients[i].m_RenderPrev = m_Snap.m_aCharacters[i].m_Prev;
		m_aClients[i].m_IsPredicted = false;
		m_aClients[i].m_IsPredictedLocal = false;
		vec2 UnpredPos = mix(
			vec2(m_Snap.m_aCharacters[i].m_Prev.m_X, m_Snap.m_aCharacters[i].m_Prev.m_Y),
			vec2(m_Snap.m_aCharacters[i].m_Cur.m_X, m_Snap.m_aCharacters[i].m_Cur.m_Y),
			Client()->IntraGameTick(g_Config.m_ClDummy));
		vec2 Pos = UnpredPos;

		CCharacter *pChar = m_PredictedWorld.GetCharacterByID(i);
		if(Predict() && (i == m_Snap.m_LocalClientID || (AntiPingPlayers() && !IsOtherTeam(i))) && pChar)
		{
			m_aClients[i].m_Predicted.Write(&m_aClients[i].m_RenderCur);
			m_aClients[i].m_PrevPredicted.Write(&m_aClients[i].m_RenderPrev);

			m_aClients[i].m_IsPredicted = true;

			Pos = mix(
				vec2(m_aClients[i].m_RenderPrev.m_X, m_aClients[i].m_RenderPrev.m_Y),
				vec2(m_aClients[i].m_RenderCur.m_X, m_aClients[i].m_RenderCur.m_Y),
				m_aClients[i].m_IsPredicted ? Client()->PredIntraGameTick(g_Config.m_ClDummy) : Client()->IntraGameTick(g_Config.m_ClDummy));

			if(i == m_Snap.m_LocalClientID)
			{
				m_aClients[i].m_IsPredictedLocal = true;
				if(AntiPingGunfire() && ((pChar->m_NinjaJetpack && pChar->m_FreezeTime == 0) || m_Snap.m_aCharacters[i].m_Cur.m_Weapon != WEAPON_NINJA || m_Snap.m_aCharacters[i].m_Cur.m_Weapon == m_aClients[i].m_Predicted.m_ActiveWeapon))
				{
					m_aClients[i].m_RenderCur.m_AttackTick = pChar->GetAttackTick();
					if(m_Snap.m_aCharacters[i].m_Cur.m_Weapon != WEAPON_NINJA && !(pChar->m_NinjaJetpack && pChar->Core()->m_ActiveWeapon == WEAPON_GUN))
						m_aClients[i].m_RenderCur.m_Weapon = m_aClients[i].m_Predicted.m_ActiveWeapon;
				}
			}
			else
			{
				// use unpredicted values for other players
				m_aClients[i].m_RenderPrev.m_Angle = m_Snap.m_aCharacters[i].m_Prev.m_Angle;
				m_aClients[i].m_RenderCur.m_Angle = m_Snap.m_aCharacters[i].m_Cur.m_Angle;

				if(g_Config.m_ClAntiPingSmooth)
					Pos = GetSmoothPos(i);
			}
		}
		m_Snap.m_aCharacters[i].m_Position = Pos;
		m_aClients[i].m_RenderPos = Pos;
		if(Predict() && i == m_Snap.m_LocalClientID)
			m_LocalCharacterPos = Pos;
	}
}

void CGameClient::DetectStrongHook()
{
	// attempt to detect strong/weak between players
	for(int FromPlayer = 0; FromPlayer < MAX_CLIENTS; FromPlayer++)
	{
		if(!m_Snap.m_aCharacters[FromPlayer].m_Active)
			continue;
		int ToPlayer = m_Snap.m_aCharacters[FromPlayer].m_Prev.m_HookedPlayer;
		if(ToPlayer < 0 || ToPlayer >= MAX_CLIENTS || !m_Snap.m_aCharacters[ToPlayer].m_Active || ToPlayer != m_Snap.m_aCharacters[FromPlayer].m_Cur.m_HookedPlayer)
			continue;
		if(absolute(minimum(m_aLastUpdateTick[ToPlayer], m_aLastUpdateTick[FromPlayer]) - Client()->GameTick(g_Config.m_ClDummy)) < Client()->GameTickSpeed() / 4)
			continue;
		if(m_Snap.m_aCharacters[FromPlayer].m_Prev.m_Direction != m_Snap.m_aCharacters[FromPlayer].m_Cur.m_Direction || m_Snap.m_aCharacters[ToPlayer].m_Prev.m_Direction != m_Snap.m_aCharacters[ToPlayer].m_Cur.m_Direction)
			continue;

		CCharacter *pFromCharWorld = m_GameWorld.GetCharacterByID(FromPlayer);
		CCharacter *pToCharWorld = m_GameWorld.GetCharacterByID(ToPlayer);
		if(!pFromCharWorld || !pToCharWorld)
			continue;

		m_aLastUpdateTick[ToPlayer] = m_aLastUpdateTick[FromPlayer] = Client()->GameTick(g_Config.m_ClDummy);

		float aPredictErr[2];
		CCharacterCore ToCharCur;
		ToCharCur.Read(&m_Snap.m_aCharacters[ToPlayer].m_Cur);

		CWorldCore World;
		World.m_aTuning[g_Config.m_ClDummy] = m_aTuning[g_Config.m_ClDummy];

		for(int dir = 0; dir < 2; dir++)
		{
			CCharacterCore ToChar = pFromCharWorld->GetCore();
			ToChar.Init(&World, Collision(), &m_Teams);
			World.m_apCharacters[ToPlayer] = &ToChar;
			ToChar.Read(&m_Snap.m_aCharacters[ToPlayer].m_Prev);

			CCharacterCore FromChar = pFromCharWorld->GetCore();
			FromChar.Init(&World, Collision(), &m_Teams);
			World.m_apCharacters[FromPlayer] = &FromChar;
			FromChar.Read(&m_Snap.m_aCharacters[FromPlayer].m_Prev);

			for(int Tick = Client()->PrevGameTick(g_Config.m_ClDummy); Tick < Client()->GameTick(g_Config.m_ClDummy); Tick++)
			{
				if(dir == 0)
				{
					FromChar.Tick(false);
					ToChar.Tick(false);
				}
				else
				{
					ToChar.Tick(false);
					FromChar.Tick(false);
				}
				FromChar.Move();
				FromChar.Quantize();
				ToChar.Move();
				ToChar.Quantize();
			}
			aPredictErr[dir] = distance(ToChar.m_Vel, ToCharCur.m_Vel);
		}
		const float LOW = 0.0001f;
		const float HIGH = 0.07f;
		if(aPredictErr[1] < LOW && aPredictErr[0] > HIGH)
		{
			if(m_CharOrder.HasStrongAgainst(ToPlayer, FromPlayer))
			{
				if(ToPlayer != m_Snap.m_LocalClientID)
					m_CharOrder.GiveWeak(ToPlayer);
				else
					m_CharOrder.GiveStrong(FromPlayer);
			}
		}
		else if(aPredictErr[0] < LOW && aPredictErr[1] > HIGH)
		{
			if(m_CharOrder.HasStrongAgainst(FromPlayer, ToPlayer))
			{
				if(ToPlayer != m_Snap.m_LocalClientID)
					m_CharOrder.GiveStrong(ToPlayer);
				else
					m_CharOrder.GiveWeak(FromPlayer);
			}
		}
	}
}

vec2 CGameClient::GetSmoothPos(int ClientID)
{
	vec2 Pos = mix(m_aClients[ClientID].m_PrevPredicted.m_Pos, m_aClients[ClientID].m_Predicted.m_Pos, Client()->PredIntraGameTick(g_Config.m_ClDummy));
	int64_t Now = time_get();
	for(int i = 0; i < 2; i++)
	{
		int64_t Len = clamp(m_aClients[ClientID].m_aSmoothLen[i], (int64_t)1, time_freq());
		int64_t TimePassed = Now - m_aClients[ClientID].m_aSmoothStart[i];
		if(in_range(TimePassed, (int64_t)0, Len - 1))
		{
			float MixAmount = 1.f - std::pow(1.f - TimePassed / (float)Len, 1.2f);
			int SmoothTick;
			float SmoothIntra;
			Client()->GetSmoothTick(&SmoothTick, &SmoothIntra, MixAmount);
			if(SmoothTick > 0 && m_aClients[ClientID].m_aPredTick[(SmoothTick - 1) % 200] >= Client()->PrevGameTick(g_Config.m_ClDummy) && m_aClients[ClientID].m_aPredTick[SmoothTick % 200] <= Client()->PredGameTick(g_Config.m_ClDummy))
				Pos[i] = mix(m_aClients[ClientID].m_aPredPos[(SmoothTick - 1) % 200][i], m_aClients[ClientID].m_aPredPos[SmoothTick % 200][i], SmoothIntra);
		}
	}
	return Pos;
}

void CGameClient::Echo(const char *pString)
{
	m_Chat.Echo(pString);
}

bool CGameClient::IsOtherTeam(int ClientID) const
{
	bool Local = m_Snap.m_LocalClientID == ClientID;

	if(m_Snap.m_LocalClientID < 0)
		return false;
	else if((m_Snap.m_SpecInfo.m_Active && m_Snap.m_SpecInfo.m_SpectatorID == SPEC_FREEVIEW) || ClientID < 0)
		return false;
	else if(m_Snap.m_SpecInfo.m_Active && m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW)
	{
		if(m_Teams.Team(ClientID) == TEAM_SUPER || m_Teams.Team(m_Snap.m_SpecInfo.m_SpectatorID) == TEAM_SUPER)
			return false;
		return m_Teams.Team(ClientID) != m_Teams.Team(m_Snap.m_SpecInfo.m_SpectatorID);
	}
	else if((m_aClients[m_Snap.m_LocalClientID].m_Solo || m_aClients[ClientID].m_Solo) && !Local)
		return true;

	if(m_Teams.Team(ClientID) == TEAM_SUPER || m_Teams.Team(m_Snap.m_LocalClientID) == TEAM_SUPER)
		return false;

	return m_Teams.Team(ClientID) != m_Teams.Team(m_Snap.m_LocalClientID);
}

int CGameClient::SwitchStateTeam() const
{
	if(m_aSwitchStateTeam[g_Config.m_ClDummy] >= 0)
		return m_aSwitchStateTeam[g_Config.m_ClDummy];
	else if(m_Snap.m_LocalClientID < 0)
		return 0;
	else if(m_Snap.m_SpecInfo.m_Active && m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW)
		return m_Teams.Team(m_Snap.m_SpecInfo.m_SpectatorID);
	return m_Teams.Team(m_Snap.m_LocalClientID);
}

bool CGameClient::IsLocalCharSuper() const
{
	if(m_Snap.m_LocalClientID < 0)
		return false;
	return m_aClients[m_Snap.m_LocalClientID].m_Super;
}

void CGameClient::LoadGameSkin(const char *pPath, bool AsDir)
{
	if(m_GameSkinLoaded)
	{
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteHealthFull);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteHealthEmpty);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteArmorFull);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteArmorEmpty);

		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponHammerCursor);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponGunCursor);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponShotgunCursor);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponGrenadeCursor);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponNinjaCursor);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponLaserCursor);

		for(auto &SpriteWeaponCursor : m_GameSkin.m_aSpriteWeaponCursors)
		{
			SpriteWeaponCursor = IGraphics::CTextureHandle();
		}

		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteHookChain);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteHookHead);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponHammer);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponGun);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponShotgun);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponGrenade);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponNinja);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponLaser);

		for(auto &SpriteWeapon : m_GameSkin.m_aSpriteWeapons)
		{
			SpriteWeapon = IGraphics::CTextureHandle();
		}

		for(auto &SpriteParticle : m_GameSkin.m_aSpriteParticles)
		{
			Graphics()->UnloadTexture(&SpriteParticle);
		}

		for(auto &SpriteStar : m_GameSkin.m_aSpriteStars)
		{
			Graphics()->UnloadTexture(&SpriteStar);
		}

		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponGunProjectile);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponShotgunProjectile);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponGrenadeProjectile);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponHammerProjectile);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponNinjaProjectile);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteWeaponLaserProjectile);

		for(auto &SpriteWeaponProjectile : m_GameSkin.m_aSpriteWeaponProjectiles)
		{
			SpriteWeaponProjectile = IGraphics::CTextureHandle();
		}

		for(int i = 0; i < 3; ++i)
		{
			Graphics()->UnloadTexture(&m_GameSkin.m_aSpriteWeaponGunMuzzles[i]);
			Graphics()->UnloadTexture(&m_GameSkin.m_aSpriteWeaponShotgunMuzzles[i]);
			Graphics()->UnloadTexture(&m_GameSkin.m_aaSpriteWeaponNinjaMuzzles[i]);

			for(auto &SpriteWeaponsMuzzle : m_GameSkin.m_aaSpriteWeaponsMuzzles)
			{
				SpriteWeaponsMuzzle[i] = IGraphics::CTextureHandle();
			}
		}

		Graphics()->UnloadTexture(&m_GameSkin.m_SpritePickupHealth);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpritePickupArmor);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpritePickupArmorShotgun);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpritePickupArmorGrenade);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpritePickupArmorLaser);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpritePickupArmorNinja);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpritePickupGrenade);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpritePickupShotgun);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpritePickupLaser);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpritePickupNinja);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpritePickupGun);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpritePickupHammer);

		for(auto &SpritePickupWeapon : m_GameSkin.m_aSpritePickupWeapons)
		{
			SpritePickupWeapon = IGraphics::CTextureHandle();
		}

		for(auto &SpritePickupWeaponArmor : m_GameSkin.m_aSpritePickupWeaponArmor)
		{
			SpritePickupWeaponArmor = IGraphics::CTextureHandle();
		}

		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteFlagBlue);
		Graphics()->UnloadTexture(&m_GameSkin.m_SpriteFlagRed);

		if(m_GameSkin.IsSixup())
		{
			Graphics()->UnloadTexture(&m_GameSkin.m_SpriteNinjaBarFullLeft);
			Graphics()->UnloadTexture(&m_GameSkin.m_SpriteNinjaBarFull);
			Graphics()->UnloadTexture(&m_GameSkin.m_SpriteNinjaBarEmpty);
			Graphics()->UnloadTexture(&m_GameSkin.m_SpriteNinjaBarEmptyRight);
		}

		m_GameSkinLoaded = false;
	}

	char aPath[IO_MAX_PATH_LENGTH];
	bool IsDefault = false;
	if(str_comp(pPath, "default") == 0)
	{
		str_copy(aPath, g_pData->m_aImages[IMAGE_GAME].m_pFilename);
		IsDefault = true;
	}
	else
	{
		if(AsDir)
			str_format(aPath, sizeof(aPath), "assets/game/%s/%s", pPath, g_pData->m_aImages[IMAGE_GAME].m_pFilename);
		else
			str_format(aPath, sizeof(aPath), "assets/game/%s.png", pPath);
	}

	CImageInfo ImgInfo;
	bool PngLoaded = Graphics()->LoadPNG(&ImgInfo, aPath, IStorage::TYPE_ALL);
	if(!PngLoaded && !IsDefault)
	{
		if(AsDir)
			LoadGameSkin("default");
		else
			LoadGameSkin(pPath, true);
	}
	else if(PngLoaded && Graphics()->CheckImageDivisibility(aPath, ImgInfo, g_pData->m_aSprites[SPRITE_HEALTH_FULL].m_pSet->m_Gridx, g_pData->m_aSprites[SPRITE_HEALTH_FULL].m_pSet->m_Gridy, true) && Graphics()->IsImageFormatRGBA(aPath, ImgInfo))
	{
		m_GameSkin.m_SpriteHealthFull = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HEALTH_FULL]);
		m_GameSkin.m_SpriteHealthEmpty = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HEALTH_EMPTY]);
		m_GameSkin.m_SpriteArmorFull = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_ARMOR_FULL]);
		m_GameSkin.m_SpriteArmorEmpty = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_ARMOR_EMPTY]);

		m_GameSkin.m_SpriteWeaponHammerCursor = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_HAMMER_CURSOR]);
		m_GameSkin.m_SpriteWeaponGunCursor = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_GUN_CURSOR]);
		m_GameSkin.m_SpriteWeaponShotgunCursor = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_SHOTGUN_CURSOR]);
		m_GameSkin.m_SpriteWeaponGrenadeCursor = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_GRENADE_CURSOR]);
		m_GameSkin.m_SpriteWeaponNinjaCursor = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_NINJA_CURSOR]);
		m_GameSkin.m_SpriteWeaponLaserCursor = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_LASER_CURSOR]);

		m_GameSkin.m_aSpriteWeaponCursors[0] = m_GameSkin.m_SpriteWeaponHammerCursor;
		m_GameSkin.m_aSpriteWeaponCursors[1] = m_GameSkin.m_SpriteWeaponGunCursor;
		m_GameSkin.m_aSpriteWeaponCursors[2] = m_GameSkin.m_SpriteWeaponShotgunCursor;
		m_GameSkin.m_aSpriteWeaponCursors[3] = m_GameSkin.m_SpriteWeaponGrenadeCursor;
		m_GameSkin.m_aSpriteWeaponCursors[4] = m_GameSkin.m_SpriteWeaponLaserCursor;
		m_GameSkin.m_aSpriteWeaponCursors[5] = m_GameSkin.m_SpriteWeaponNinjaCursor;

		// weapons and hook
		m_GameSkin.m_SpriteHookChain = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HOOK_CHAIN]);
		m_GameSkin.m_SpriteHookHead = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HOOK_HEAD]);
		m_GameSkin.m_SpriteWeaponHammer = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_HAMMER_BODY]);
		m_GameSkin.m_SpriteWeaponGun = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_GUN_BODY]);
		m_GameSkin.m_SpriteWeaponShotgun = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_SHOTGUN_BODY]);
		m_GameSkin.m_SpriteWeaponGrenade = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_GRENADE_BODY]);
		m_GameSkin.m_SpriteWeaponNinja = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_NINJA_BODY]);
		m_GameSkin.m_SpriteWeaponLaser = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_LASER_BODY]);

		m_GameSkin.m_aSpriteWeapons[0] = m_GameSkin.m_SpriteWeaponHammer;
		m_GameSkin.m_aSpriteWeapons[1] = m_GameSkin.m_SpriteWeaponGun;
		m_GameSkin.m_aSpriteWeapons[2] = m_GameSkin.m_SpriteWeaponShotgun;
		m_GameSkin.m_aSpriteWeapons[3] = m_GameSkin.m_SpriteWeaponGrenade;
		m_GameSkin.m_aSpriteWeapons[4] = m_GameSkin.m_SpriteWeaponLaser;
		m_GameSkin.m_aSpriteWeapons[5] = m_GameSkin.m_SpriteWeaponNinja;

		// particles
		for(int i = 0; i < 9; ++i)
		{
			m_GameSkin.m_aSpriteParticles[i] = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PART1 + i]);
		}

		// stars
		for(int i = 0; i < 3; ++i)
		{
			m_GameSkin.m_aSpriteStars[i] = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_STAR1 + i]);
		}

		// projectiles
		m_GameSkin.m_SpriteWeaponGunProjectile = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_GUN_PROJ]);
		m_GameSkin.m_SpriteWeaponShotgunProjectile = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_SHOTGUN_PROJ]);
		m_GameSkin.m_SpriteWeaponGrenadeProjectile = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_GRENADE_PROJ]);

		// these weapons have no projectiles
		m_GameSkin.m_SpriteWeaponHammerProjectile = IGraphics::CTextureHandle();
		m_GameSkin.m_SpriteWeaponNinjaProjectile = IGraphics::CTextureHandle();

		m_GameSkin.m_SpriteWeaponLaserProjectile = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_LASER_PROJ]);

		m_GameSkin.m_aSpriteWeaponProjectiles[0] = m_GameSkin.m_SpriteWeaponHammerProjectile;
		m_GameSkin.m_aSpriteWeaponProjectiles[1] = m_GameSkin.m_SpriteWeaponGunProjectile;
		m_GameSkin.m_aSpriteWeaponProjectiles[2] = m_GameSkin.m_SpriteWeaponShotgunProjectile;
		m_GameSkin.m_aSpriteWeaponProjectiles[3] = m_GameSkin.m_SpriteWeaponGrenadeProjectile;
		m_GameSkin.m_aSpriteWeaponProjectiles[4] = m_GameSkin.m_SpriteWeaponLaserProjectile;
		m_GameSkin.m_aSpriteWeaponProjectiles[5] = m_GameSkin.m_SpriteWeaponNinjaProjectile;

		// muzzles
		for(int i = 0; i < 3; ++i)
		{
			m_GameSkin.m_aSpriteWeaponGunMuzzles[i] = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_GUN_MUZZLE1 + i]);
			m_GameSkin.m_aSpriteWeaponShotgunMuzzles[i] = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_SHOTGUN_MUZZLE1 + i]);
			m_GameSkin.m_aaSpriteWeaponNinjaMuzzles[i] = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_NINJA_MUZZLE1 + i]);

			m_GameSkin.m_aaSpriteWeaponsMuzzles[1][i] = m_GameSkin.m_aSpriteWeaponGunMuzzles[i];
			m_GameSkin.m_aaSpriteWeaponsMuzzles[2][i] = m_GameSkin.m_aSpriteWeaponShotgunMuzzles[i];
			m_GameSkin.m_aaSpriteWeaponsMuzzles[5][i] = m_GameSkin.m_aaSpriteWeaponNinjaMuzzles[i];
		}

		// pickups
		m_GameSkin.m_SpritePickupHealth = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_HEALTH]);
		m_GameSkin.m_SpritePickupArmor = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_ARMOR]);
		m_GameSkin.m_SpritePickupHammer = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_HAMMER]);
		m_GameSkin.m_SpritePickupGun = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_GUN]);
		m_GameSkin.m_SpritePickupShotgun = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_SHOTGUN]);
		m_GameSkin.m_SpritePickupGrenade = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_GRENADE]);
		m_GameSkin.m_SpritePickupLaser = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_LASER]);
		m_GameSkin.m_SpritePickupNinja = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_NINJA]);
		m_GameSkin.m_SpritePickupArmorShotgun = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_ARMOR_SHOTGUN]);
		m_GameSkin.m_SpritePickupArmorGrenade = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_ARMOR_GRENADE]);
		m_GameSkin.m_SpritePickupArmorNinja = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_ARMOR_NINJA]);
		m_GameSkin.m_SpritePickupArmorLaser = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_ARMOR_LASER]);

		m_GameSkin.m_aSpritePickupWeapons[0] = m_GameSkin.m_SpritePickupHammer;
		m_GameSkin.m_aSpritePickupWeapons[1] = m_GameSkin.m_SpritePickupGun;
		m_GameSkin.m_aSpritePickupWeapons[2] = m_GameSkin.m_SpritePickupShotgun;
		m_GameSkin.m_aSpritePickupWeapons[3] = m_GameSkin.m_SpritePickupGrenade;
		m_GameSkin.m_aSpritePickupWeapons[4] = m_GameSkin.m_SpritePickupLaser;
		m_GameSkin.m_aSpritePickupWeapons[5] = m_GameSkin.m_SpritePickupNinja;

		m_GameSkin.m_aSpritePickupWeaponArmor[0] = m_GameSkin.m_SpritePickupArmorShotgun;
		m_GameSkin.m_aSpritePickupWeaponArmor[1] = m_GameSkin.m_SpritePickupArmorGrenade;
		m_GameSkin.m_aSpritePickupWeaponArmor[2] = m_GameSkin.m_SpritePickupArmorNinja;
		m_GameSkin.m_aSpritePickupWeaponArmor[3] = m_GameSkin.m_SpritePickupArmorLaser;

		// flags
		m_GameSkin.m_SpriteFlagBlue = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_FLAG_BLUE]);
		m_GameSkin.m_SpriteFlagRed = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_FLAG_RED]);

		// ninja bar (0.7)
		if(!Graphics()->IsSpriteTextureFullyTransparent(ImgInfo, &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_FULL_LEFT]) ||
			!Graphics()->IsSpriteTextureFullyTransparent(ImgInfo, &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_FULL]) ||
			!Graphics()->IsSpriteTextureFullyTransparent(ImgInfo, &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_EMPTY]) ||
			!Graphics()->IsSpriteTextureFullyTransparent(ImgInfo, &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_EMPTY_RIGHT]))
		{
			m_GameSkin.m_SpriteNinjaBarFullLeft = Graphics()->LoadSpriteTexture(ImgInfo, &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_FULL_LEFT]);
			m_GameSkin.m_SpriteNinjaBarFull = Graphics()->LoadSpriteTexture(ImgInfo, &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_FULL]);
			m_GameSkin.m_SpriteNinjaBarEmpty = Graphics()->LoadSpriteTexture(ImgInfo, &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_EMPTY]);
			m_GameSkin.m_SpriteNinjaBarEmptyRight = Graphics()->LoadSpriteTexture(ImgInfo, &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_EMPTY_RIGHT]);
		}

		m_GameSkinLoaded = true;

		Graphics()->FreePNG(&ImgInfo);
	}
}

void CGameClient::LoadEmoticonsSkin(const char *pPath, bool AsDir)
{
	if(m_EmoticonsSkinLoaded)
	{
		for(auto &SpriteEmoticon : m_EmoticonsSkin.m_aSpriteEmoticons)
			Graphics()->UnloadTexture(&SpriteEmoticon);

		m_EmoticonsSkinLoaded = false;
	}

	char aPath[IO_MAX_PATH_LENGTH];
	bool IsDefault = false;
	if(str_comp(pPath, "default") == 0)
	{
		str_copy(aPath, g_pData->m_aImages[IMAGE_EMOTICONS].m_pFilename);
		IsDefault = true;
	}
	else
	{
		if(AsDir)
			str_format(aPath, sizeof(aPath), "assets/emoticons/%s/%s", pPath, g_pData->m_aImages[IMAGE_EMOTICONS].m_pFilename);
		else
			str_format(aPath, sizeof(aPath), "assets/emoticons/%s.png", pPath);
	}

	CImageInfo ImgInfo;
	bool PngLoaded = Graphics()->LoadPNG(&ImgInfo, aPath, IStorage::TYPE_ALL);
	if(!PngLoaded && !IsDefault)
	{
		if(AsDir)
			LoadEmoticonsSkin("default");
		else
			LoadEmoticonsSkin(pPath, true);
	}
	else if(PngLoaded && Graphics()->CheckImageDivisibility(aPath, ImgInfo, g_pData->m_aSprites[SPRITE_OOP].m_pSet->m_Gridx, g_pData->m_aSprites[SPRITE_OOP].m_pSet->m_Gridy, true) && Graphics()->IsImageFormatRGBA(aPath, ImgInfo))
	{
		for(int i = 0; i < 16; ++i)
			m_EmoticonsSkin.m_aSpriteEmoticons[i] = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_OOP + i]);

		m_EmoticonsSkinLoaded = true;
		Graphics()->FreePNG(&ImgInfo);
	}
}

void CGameClient::LoadParticlesSkin(const char *pPath, bool AsDir)
{
	if(m_ParticlesSkinLoaded)
	{
		Graphics()->UnloadTexture(&m_ParticlesSkin.m_SpriteParticleSlice);
		Graphics()->UnloadTexture(&m_ParticlesSkin.m_SpriteParticleBall);
		for(auto &SpriteParticleSplat : m_ParticlesSkin.m_aSpriteParticleSplat)
			Graphics()->UnloadTexture(&SpriteParticleSplat);
		Graphics()->UnloadTexture(&m_ParticlesSkin.m_SpriteParticleSmoke);
		Graphics()->UnloadTexture(&m_ParticlesSkin.m_SpriteParticleShell);
		Graphics()->UnloadTexture(&m_ParticlesSkin.m_SpriteParticleExpl);
		Graphics()->UnloadTexture(&m_ParticlesSkin.m_SpriteParticleAirJump);
		Graphics()->UnloadTexture(&m_ParticlesSkin.m_SpriteParticleHit);

		for(auto &SpriteParticle : m_ParticlesSkin.m_aSpriteParticles)
			SpriteParticle = IGraphics::CTextureHandle();

		m_ParticlesSkinLoaded = false;
	}

	char aPath[IO_MAX_PATH_LENGTH];
	bool IsDefault = false;
	if(str_comp(pPath, "default") == 0)
	{
		str_copy(aPath, g_pData->m_aImages[IMAGE_PARTICLES].m_pFilename);
		IsDefault = true;
	}
	else
	{
		if(AsDir)
			str_format(aPath, sizeof(aPath), "assets/particles/%s/%s", pPath, g_pData->m_aImages[IMAGE_PARTICLES].m_pFilename);
		else
			str_format(aPath, sizeof(aPath), "assets/particles/%s.png", pPath);
	}

	CImageInfo ImgInfo;
	bool PngLoaded = Graphics()->LoadPNG(&ImgInfo, aPath, IStorage::TYPE_ALL);
	if(!PngLoaded && !IsDefault)
	{
		if(AsDir)
			LoadParticlesSkin("default");
		else
			LoadParticlesSkin(pPath, true);
	}
	else if(PngLoaded && Graphics()->CheckImageDivisibility(aPath, ImgInfo, g_pData->m_aSprites[SPRITE_PART_SLICE].m_pSet->m_Gridx, g_pData->m_aSprites[SPRITE_PART_SLICE].m_pSet->m_Gridy, true) && Graphics()->IsImageFormatRGBA(aPath, ImgInfo))
	{
		m_ParticlesSkin.m_SpriteParticleSlice = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PART_SLICE]);
		m_ParticlesSkin.m_SpriteParticleBall = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PART_BALL]);
		for(int i = 0; i < 3; ++i)
			m_ParticlesSkin.m_aSpriteParticleSplat[i] = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PART_SPLAT01 + i]);
		m_ParticlesSkin.m_SpriteParticleSmoke = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PART_SMOKE]);
		m_ParticlesSkin.m_SpriteParticleShell = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PART_SHELL]);
		m_ParticlesSkin.m_SpriteParticleExpl = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PART_EXPL01]);
		m_ParticlesSkin.m_SpriteParticleAirJump = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PART_AIRJUMP]);
		m_ParticlesSkin.m_SpriteParticleHit = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PART_HIT01]);

		m_ParticlesSkin.m_aSpriteParticles[0] = m_ParticlesSkin.m_SpriteParticleSlice;
		m_ParticlesSkin.m_aSpriteParticles[1] = m_ParticlesSkin.m_SpriteParticleBall;
		for(int i = 0; i < 3; ++i)
			m_ParticlesSkin.m_aSpriteParticles[2 + i] = m_ParticlesSkin.m_aSpriteParticleSplat[i];
		m_ParticlesSkin.m_aSpriteParticles[5] = m_ParticlesSkin.m_SpriteParticleSmoke;
		m_ParticlesSkin.m_aSpriteParticles[6] = m_ParticlesSkin.m_SpriteParticleShell;
		m_ParticlesSkin.m_aSpriteParticles[7] = m_ParticlesSkin.m_SpriteParticleExpl;
		m_ParticlesSkin.m_aSpriteParticles[8] = m_ParticlesSkin.m_SpriteParticleAirJump;
		m_ParticlesSkin.m_aSpriteParticles[9] = m_ParticlesSkin.m_SpriteParticleHit;

		m_ParticlesSkinLoaded = true;
		free(ImgInfo.m_pData);
	}
}

void CGameClient::LoadHudSkin(const char *pPath, bool AsDir)
{
	if(m_HudSkinLoaded)
	{
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudAirjump);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudAirjumpEmpty);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudSolo);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudCollisionDisabled);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudEndlessJump);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudEndlessHook);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudJetpack);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudFreezeBarFullLeft);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudFreezeBarFull);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudFreezeBarEmpty);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudFreezeBarEmptyRight);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudNinjaBarFullLeft);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudNinjaBarFull);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudNinjaBarEmpty);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudNinjaBarEmptyRight);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudHookHitDisabled);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudHammerHitDisabled);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudShotgunHitDisabled);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudGrenadeHitDisabled);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudLaserHitDisabled);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudGunHitDisabled);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudDeepFrozen);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudLiveFrozen);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudTeleportGrenade);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudTeleportGun);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudTeleportLaser);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudPracticeMode);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudLockMode);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudDummyHammer);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudDummyCopy);
		m_HudSkinLoaded = false;
	}

	char aPath[IO_MAX_PATH_LENGTH];
	bool IsDefault = false;
	if(str_comp(pPath, "default") == 0)
	{
		str_copy(aPath, g_pData->m_aImages[IMAGE_HUD].m_pFilename);
		IsDefault = true;
	}
	else
	{
		if(AsDir)
			str_format(aPath, sizeof(aPath), "assets/hud/%s/%s", pPath, g_pData->m_aImages[IMAGE_HUD].m_pFilename);
		else
			str_format(aPath, sizeof(aPath), "assets/hud/%s.png", pPath);
	}

	CImageInfo ImgInfo;
	bool PngLoaded = Graphics()->LoadPNG(&ImgInfo, aPath, IStorage::TYPE_ALL);
	if(!PngLoaded && !IsDefault)
	{
		if(AsDir)
			LoadHudSkin("default");
		else
			LoadHudSkin(pPath, true);
	}
	else if(PngLoaded && Graphics()->CheckImageDivisibility(aPath, ImgInfo, g_pData->m_aSprites[SPRITE_HUD_AIRJUMP].m_pSet->m_Gridx, g_pData->m_aSprites[SPRITE_HUD_AIRJUMP].m_pSet->m_Gridy, true) && Graphics()->IsImageFormatRGBA(aPath, ImgInfo))
	{
		m_HudSkin.m_SpriteHudAirjump = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_AIRJUMP]);
		m_HudSkin.m_SpriteHudAirjumpEmpty = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_AIRJUMP_EMPTY]);
		m_HudSkin.m_SpriteHudSolo = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_SOLO]);
		m_HudSkin.m_SpriteHudCollisionDisabled = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_COLLISION_DISABLED]);
		m_HudSkin.m_SpriteHudEndlessJump = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_ENDLESS_JUMP]);
		m_HudSkin.m_SpriteHudEndlessHook = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_ENDLESS_HOOK]);
		m_HudSkin.m_SpriteHudJetpack = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_JETPACK]);
		m_HudSkin.m_SpriteHudFreezeBarFullLeft = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_FREEZE_BAR_FULL_LEFT]);
		m_HudSkin.m_SpriteHudFreezeBarFull = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_FREEZE_BAR_FULL]);
		m_HudSkin.m_SpriteHudFreezeBarEmpty = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_FREEZE_BAR_EMPTY]);
		m_HudSkin.m_SpriteHudFreezeBarEmptyRight = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_FREEZE_BAR_EMPTY_RIGHT]);
		m_HudSkin.m_SpriteHudNinjaBarFullLeft = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_NINJA_BAR_FULL_LEFT]);
		m_HudSkin.m_SpriteHudNinjaBarFull = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_NINJA_BAR_FULL]);
		m_HudSkin.m_SpriteHudNinjaBarEmpty = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_NINJA_BAR_EMPTY]);
		m_HudSkin.m_SpriteHudNinjaBarEmptyRight = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_NINJA_BAR_EMPTY_RIGHT]);
		m_HudSkin.m_SpriteHudHookHitDisabled = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_HOOK_HIT_DISABLED]);
		m_HudSkin.m_SpriteHudHammerHitDisabled = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_HAMMER_HIT_DISABLED]);
		m_HudSkin.m_SpriteHudShotgunHitDisabled = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_SHOTGUN_HIT_DISABLED]);
		m_HudSkin.m_SpriteHudGrenadeHitDisabled = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_GRENADE_HIT_DISABLED]);
		m_HudSkin.m_SpriteHudLaserHitDisabled = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_LASER_HIT_DISABLED]);
		m_HudSkin.m_SpriteHudGunHitDisabled = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_GUN_HIT_DISABLED]);
		m_HudSkin.m_SpriteHudDeepFrozen = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_DEEP_FROZEN]);
		m_HudSkin.m_SpriteHudLiveFrozen = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_LIVE_FROZEN]);
		m_HudSkin.m_SpriteHudTeleportGrenade = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_TELEPORT_GRENADE]);
		m_HudSkin.m_SpriteHudTeleportGun = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_TELEPORT_GUN]);
		m_HudSkin.m_SpriteHudTeleportLaser = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_TELEPORT_LASER]);
		m_HudSkin.m_SpriteHudPracticeMode = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_PRACTICE_MODE]);
		m_HudSkin.m_SpriteHudLockMode = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_LOCK_MODE]);
		m_HudSkin.m_SpriteHudDummyHammer = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_DUMMY_HAMMER]);
		m_HudSkin.m_SpriteHudDummyCopy = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_HUD_DUMMY_COPY]);

		m_HudSkinLoaded = true;
		free(ImgInfo.m_pData);
	}
}

void CGameClient::LoadExtrasSkin(const char *pPath, bool AsDir)
{
	if(m_ExtrasSkinLoaded)
	{
		Graphics()->UnloadTexture(&m_ExtrasSkin.m_SpriteParticleSnowflake);

		for(auto &SpriteParticle : m_ExtrasSkin.m_aSpriteParticles)
			SpriteParticle = IGraphics::CTextureHandle();

		m_ExtrasSkinLoaded = false;
	}

	char aPath[IO_MAX_PATH_LENGTH];
	bool IsDefault = false;
	if(str_comp(pPath, "default") == 0)
	{
		str_copy(aPath, g_pData->m_aImages[IMAGE_EXTRAS].m_pFilename);
		IsDefault = true;
	}
	else
	{
		if(AsDir)
			str_format(aPath, sizeof(aPath), "assets/extras/%s/%s", pPath, g_pData->m_aImages[IMAGE_EXTRAS].m_pFilename);
		else
			str_format(aPath, sizeof(aPath), "assets/extras/%s.png", pPath);
	}

	CImageInfo ImgInfo;
	bool PngLoaded = Graphics()->LoadPNG(&ImgInfo, aPath, IStorage::TYPE_ALL);
	if(!PngLoaded && !IsDefault)
	{
		if(AsDir)
			LoadExtrasSkin("default");
		else
			LoadExtrasSkin(pPath, true);
	}
	else if(PngLoaded && Graphics()->CheckImageDivisibility(aPath, ImgInfo, g_pData->m_aSprites[SPRITE_PART_SNOWFLAKE].m_pSet->m_Gridx, g_pData->m_aSprites[SPRITE_PART_SNOWFLAKE].m_pSet->m_Gridy, true) && Graphics()->IsImageFormatRGBA(aPath, ImgInfo))
	{
		m_ExtrasSkin.m_SpriteParticleSnowflake = Graphics()->LoadSpriteTexture(ImgInfo, &g_pData->m_aSprites[SPRITE_PART_SNOWFLAKE]);
		m_ExtrasSkin.m_aSpriteParticles[0] = m_ExtrasSkin.m_SpriteParticleSnowflake;
		m_ExtrasSkinLoaded = true;
		free(ImgInfo.m_pData);
	}
}

void CGameClient::RefreshSkins()
{
	const auto SkinStartLoadTime = time_get_nanoseconds();
	m_Skins.Refresh([&](int) {
		// if skin refreshing takes to long, swap to a loading screen
		if(time_get_nanoseconds() - SkinStartLoadTime > 500ms)
		{
			m_Menus.RenderLoading(Localize("Loading skin files"), "", 0, false);
		}
	});

	for(auto &Client : m_aClients)
	{
		Client.m_SkinInfo.m_OriginalRenderSkin.Reset();
		Client.m_SkinInfo.m_ColorableRenderSkin.Reset();
		if(Client.m_aSkinName[0] != '\0')
		{
			const CSkin *pSkin = m_Skins.Find(Client.m_aSkinName);
			Client.m_SkinInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
			Client.m_SkinInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
		}
		else
		{
			Client.m_SkinInfo.m_OriginalRenderSkin.Reset();
			Client.m_SkinInfo.m_ColorableRenderSkin.Reset();
		}
		Client.UpdateRenderInfo(IsTeamPlay());
	}

	for(auto &pComponent : m_vpAll)
		pComponent->OnRefreshSkins();
}

void CGameClient::ConchainRefreshSkins(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CGameClient *pThis = static_cast<CGameClient *>(pUserData);
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() && pThis->m_Menus.IsInit())
	{
		pThis->RefreshSkins();
	}
}

static bool UnknownMapSettingCallback(const char *pCommand, void *pUser)
{
	return true;
}

void CGameClient::LoadMapSettings()
{
	// Reset Tunezones
	CTuningParams TuningParams;
	for(int i = 0; i < NUM_TUNEZONES; i++)
	{
		TuningList()[i] = TuningParams;
		TuningList()[i].Set("gun_curvature", 0);
		TuningList()[i].Set("gun_speed", 1400);
		TuningList()[i].Set("shotgun_curvature", 0);
		TuningList()[i].Set("shotgun_speed", 500);
		TuningList()[i].Set("shotgun_speeddiff", 0);
	}

	// Load map tunings
	IMap *pMap = Kernel()->RequestInterface<IMap>();
	int Start, Num;
	pMap->GetType(MAPITEMTYPE_INFO, &Start, &Num);
	for(int i = Start; i < Start + Num; i++)
	{
		int ItemID;
		CMapItemInfoSettings *pItem = (CMapItemInfoSettings *)pMap->GetItem(i, nullptr, &ItemID);
		int ItemSize = pMap->GetItemSize(i);
		if(!pItem || ItemID != 0)
			continue;

		if(ItemSize < (int)sizeof(CMapItemInfoSettings))
			break;
		if(!(pItem->m_Settings > -1))
			break;

		int Size = pMap->GetDataSize(pItem->m_Settings);
		char *pSettings = (char *)pMap->GetData(pItem->m_Settings);
		char *pNext = pSettings;
		Console()->SetUnknownCommandCallback(UnknownMapSettingCallback, nullptr);
		while(pNext < pSettings + Size)
		{
			int StrSize = str_length(pNext) + 1;
			Console()->ExecuteLine(pNext, IConsole::CLIENT_ID_GAME);
			pNext += StrSize;
		}
		Console()->SetUnknownCommandCallback(IConsole::EmptyUnknownCommandCallback, nullptr);
		pMap->UnloadData(pItem->m_Settings);
		break;
	}
}

void CGameClient::ConTuneZone(IConsole::IResult *pResult, void *pUserData)
{
	CGameClient *pSelf = (CGameClient *)pUserData;
	int List = pResult->GetInteger(0);
	const char *pParamName = pResult->GetString(1);
	float NewValue = pResult->GetFloat(2);

	if(List >= 0 && List < NUM_TUNEZONES)
		pSelf->TuningList()[List].Set(pParamName, NewValue);
}

void CGameClient::ConchainMenuMap(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CGameClient *pSelf = (CGameClient *)pUserData;
	if(pResult->NumArguments())
	{
		if(str_comp(g_Config.m_ClMenuMap, pResult->GetString(0)) != 0)
		{
			str_copy(g_Config.m_ClMenuMap, pResult->GetString(0));
			pSelf->m_MenuBackground.LoadMenuBackground();
		}
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

void CGameClient::DummyResetInput()
{
	if(!Client()->DummyConnected())
		return;

	if((m_DummyInput.m_Fire & 1) != 0)
		m_DummyInput.m_Fire++;

	m_Controls.ResetInput(!g_Config.m_ClDummy);
	m_Controls.m_aInputData[!g_Config.m_ClDummy].m_Hook = 0;
	m_Controls.m_aInputData[!g_Config.m_ClDummy].m_Fire = m_DummyInput.m_Fire;

	m_DummyInput = m_Controls.m_aInputData[!g_Config.m_ClDummy];
}

bool CGameClient::CanDisplayWarning() const
{
	return m_Menus.CanDisplayWarning();
}

bool CGameClient::IsDisplayingWarning() const
{
	return m_Menus.GetCurPopup() == CMenus::POPUP_WARNING;
}

CNetObjHandler *CGameClient::GetNetObjHandler()
{
	return &m_NetObjHandler;
}

void CGameClient::SnapCollectEntities()
{
	int NumSnapItems = Client()->SnapNumItems(IClient::SNAP_CURRENT);

	std::vector<CSnapEntities> vItemData;
	std::vector<CSnapEntities> vItemEx;

	for(int Index = 0; Index < NumSnapItems; Index++)
	{
		IClient::CSnapItem Item;
		const void *pData = Client()->SnapGetItem(IClient::SNAP_CURRENT, Index, &Item);
		if(Item.m_Type == NETOBJTYPE_ENTITYEX)
			vItemEx.push_back({Item, pData, 0});
		else if(Item.m_Type == NETOBJTYPE_PICKUP || Item.m_Type == NETOBJTYPE_DDNETPICKUP || Item.m_Type == NETOBJTYPE_LASER || Item.m_Type == NETOBJTYPE_DDNETLASER || Item.m_Type == NETOBJTYPE_PROJECTILE || Item.m_Type == NETOBJTYPE_DDRACEPROJECTILE || Item.m_Type == NETOBJTYPE_DDNETPROJECTILE)
			vItemData.push_back({Item, pData, 0});
	}

	// sort by id
	class CEntComparer
	{
	public:
		bool operator()(const CSnapEntities &lhs, const CSnapEntities &rhs) const
		{
			return lhs.m_Item.m_ID < rhs.m_Item.m_ID;
		}
	};

	std::sort(vItemData.begin(), vItemData.end(), CEntComparer());
	std::sort(vItemEx.begin(), vItemEx.end(), CEntComparer());

	// merge extended items with items they belong to
	m_vSnapEntities.clear();

	size_t IndexEx = 0;
	for(const CSnapEntities &Ent : vItemData)
	{
		const CNetObj_EntityEx *pDataEx = 0;
		while(IndexEx < vItemEx.size() && vItemEx[IndexEx].m_Item.m_ID < Ent.m_Item.m_ID)
			IndexEx++;
		if(IndexEx < vItemEx.size() && vItemEx[IndexEx].m_Item.m_ID == Ent.m_Item.m_ID)
			pDataEx = (const CNetObj_EntityEx *)vItemEx[IndexEx].m_pData;

		m_vSnapEntities.push_back({Ent.m_Item, Ent.m_pData, pDataEx});
	}
}

void CGameClient::HandleMultiView()
{
	bool IsTeamZero = IsMultiViewIdSet();
	bool Init = false;
	int AmountPlayers = 0;
	vec2 Minpos, Maxpos;
	float TmpVel = 0.0f;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// look at players who are vanished
		if(m_MultiView.m_aVanish[i])
		{
			// not in freeze anymore and the delay is over
			if(m_MultiView.m_aLastFreeze[i] + 6.0f <= Client()->LocalTime() && m_aClients[i].m_FreezeEnd == 0)
			{
				m_MultiView.m_aVanish[i] = false;
				m_MultiView.m_aLastFreeze[i] = 0.0f;
			}
		}

		// we look at team 0 and the player is not in the spec list
		if(IsTeamZero && !m_aMultiViewId[i])
			continue;

		// player is vanished
		if(m_MultiView.m_aVanish[i])
			continue;

		// the player is not in the team we are spectating
		if(m_Teams.Team(i) != m_MultiViewTeam)
			continue;

		vec2 PlayerPos;
		if(m_Snap.m_aCharacters[i].m_Active)
			PlayerPos = vec2(m_aClients[i].m_RenderPos.x, m_aClients[i].m_RenderPos.y);
		else if(m_aClients[i].m_Spec) // tee is in spec
			PlayerPos = m_aClients[i].m_SpecChar;
		else
			continue;

		// player is far away and frozen
		if(distance(m_MultiView.m_OldPos, PlayerPos) > 1100 && m_aClients[i].m_FreezeEnd != 0)
		{
			// check if the player is frozen for more than 3 seconds, if so vanish him
			if(m_MultiView.m_aLastFreeze[i] == 0.0f)
				m_MultiView.m_aLastFreeze[i] = Client()->LocalTime();
			else if(m_MultiView.m_aLastFreeze[i] + 3.0f <= Client()->LocalTime())
			{
				m_MultiView.m_aVanish[i] = true;
				// player we want to be vanished is our "main" tee, so lets switch the tee
				if(i == m_Snap.m_SpecInfo.m_SpectatorID)
					m_Spectator.Spectate(FindFirstMultiViewId());
			}
		}
		else if(m_MultiView.m_aLastFreeze[i] != 0)
			m_MultiView.m_aLastFreeze[i] = 0;

		// set the minimum and maximum position
		if(!Init)
		{
			Minpos = PlayerPos;
			Maxpos = PlayerPos;
			Init = true;
		}
		else
		{
			Minpos.x = std::min(Minpos.x, PlayerPos.x);
			Maxpos.x = std::max(Maxpos.x, PlayerPos.x);
			Minpos.y = std::min(Minpos.y, PlayerPos.y);
			Maxpos.y = std::max(Maxpos.y, PlayerPos.y);
		}

		// sum up the velocity of all players we are spectating
		const CNetObj_Character &CurrentCharacter = m_Snap.m_aCharacters[i].m_Cur;
		TmpVel += (length(vec2(CurrentCharacter.m_VelX / 256.0f, CurrentCharacter.m_VelY / 256.0f)) * 50) / 32.0f;
		AmountPlayers++;
	}

	// if we have found no players, we disable multi view
	if(AmountPlayers == 0)
	{
		if(m_MultiView.m_SecondChance == 0.0f)
			m_MultiView.m_SecondChance = Client()->LocalTime() + 0.3f;
		else if(m_MultiView.m_SecondChance < Client()->LocalTime())
		{
			ResetMultiView();
			return;
		}
		return;
	}
	else if(m_MultiView.m_SecondChance != 0.0f)
		m_MultiView.m_SecondChance = 0.0f;

	// if we only have one tee that's in the list, we activate solo-mode
	m_MultiView.m_Solo = std::count(std::begin(m_aMultiViewId), std::end(m_aMultiViewId), true) == 1;

	vec2 TargetPos = vec2((Minpos.x + Maxpos.x) / 2.0f, (Minpos.y + Maxpos.y) / 2.0f);
	// dont hide the position hud if its only one player
	m_MultiViewShowHud = AmountPlayers == 1;
	// get the average velocity
	float AvgVel = clamp(TmpVel / AmountPlayers ? TmpVel / (float)AmountPlayers : 0.0f, 0.0f, 1000.0f);

	if(m_MultiView.m_OldPersonalZoom == m_MultiViewPersonalZoom)
		m_Camera.SetZoom(CalculateMultiViewZoom(Minpos, Maxpos, AvgVel), g_Config.m_ClMultiViewZoomSmoothness);
	else
		m_Camera.SetZoom(CalculateMultiViewZoom(Minpos, Maxpos, AvgVel), 50);

	m_Snap.m_SpecInfo.m_Position = m_MultiView.m_OldPos + ((TargetPos - m_MultiView.m_OldPos) * CalculateMultiViewMultiplier(TargetPos));
	m_MultiView.m_OldPos = m_Snap.m_SpecInfo.m_Position;
	m_Snap.m_SpecInfo.m_UsePosition = true;
}

bool CGameClient::InitMultiView(int Team)
{
	float Width, Height;
	CleanMultiViewIds();
	m_MultiView.m_IsInit = true;

	// get the current view coordinates
	RenderTools()->CalcScreenParams(Graphics()->ScreenAspect(), m_Camera.m_Zoom, &Width, &Height);
	vec2 AxisX = vec2(m_Camera.m_Center.x - (Width / 2), m_Camera.m_Center.x + (Width / 2));
	vec2 AxisY = vec2(m_Camera.m_Center.y - (Height / 2), m_Camera.m_Center.y + (Height / 2));

	if(Team > 0)
	{
		m_MultiViewTeam = Team;
		for(int i = 0; i < MAX_CLIENTS; i++)
			m_aMultiViewId[i] = m_Teams.Team(i) == Team;
	}
	else
	{
		// we want to allow spectating players in teams directly if there is no other team on screen
		// to do that, -1 is used temporarily for "we don't know which team to spectate yet"
		m_MultiViewTeam = -1;

		int Count = 0;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			vec2 PlayerPos;

			// get the position of the player
			if(m_Snap.m_aCharacters[i].m_Active)
				PlayerPos = vec2(m_Snap.m_aCharacters[i].m_Cur.m_X, m_Snap.m_aCharacters[i].m_Cur.m_Y);
			else if(m_aClients[i].m_Spec)
				PlayerPos = m_aClients[i].m_SpecChar;
			else
				continue;

			if(PlayerPos.x == 0 || PlayerPos.y == 0)
				continue;

			// skip players that aren't in view
			if(PlayerPos.x <= AxisX.x || PlayerPos.x >= AxisX.y || PlayerPos.y <= AxisY.x || PlayerPos.y >= AxisY.y)
				continue;

			if(m_MultiViewTeam == -1)
			{
				// use the current player's team for now, but it might switch to team 0 if any other team is found
				m_MultiViewTeam = m_Teams.Team(i);
			}
			else if(m_MultiViewTeam != 0 && m_Teams.Team(i) != m_MultiViewTeam)
			{
				// mismatched teams; remove all previously added players again and switch to team 0 instead
				std::fill_n(m_aMultiViewId, i, false);
				m_MultiViewTeam = 0;
			}

			m_aMultiViewId[i] = true;
			Count++;
		}

		// might still be -1 if not a single player was in view; fallback to team 0 in that case
		if(m_MultiViewTeam == -1)
			m_MultiViewTeam = 0;

		// we are spectating only one player
		m_MultiView.m_Solo = Count == 1;
	}

	if(IsMultiViewIdSet())
	{
		int SpectatorID = m_Snap.m_SpecInfo.m_SpectatorID;
		int NewSpectatorID = -1;

		vec2 CurPosition(m_Camera.m_Center);
		if(SpectatorID != SPEC_FREEVIEW)
		{
			const CNetObj_Character &CurCharacter = m_Snap.m_aCharacters[SpectatorID].m_Cur;
			CurPosition.x = CurCharacter.m_X;
			CurPosition.y = CurCharacter.m_Y;
		}

		int ClosestDistance = std::numeric_limits<int>::max();
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!m_Snap.m_apPlayerInfos[i] || m_Snap.m_apPlayerInfos[i]->m_Team == TEAM_SPECTATORS || m_Teams.Team(i) != m_MultiViewTeam)
				continue;

			vec2 PlayerPos;
			if(m_Snap.m_aCharacters[i].m_Active)
				PlayerPos = vec2(m_aClients[i].m_RenderPos.x, m_aClients[i].m_RenderPos.y);
			else if(m_aClients[i].m_Spec) // tee is in spec
				PlayerPos = m_aClients[i].m_SpecChar;
			else
				continue;

			int Distance = distance(CurPosition, PlayerPos);
			if(NewSpectatorID == -1 || Distance < ClosestDistance)
			{
				NewSpectatorID = i;
				ClosestDistance = Distance;
			}
		}

		if(NewSpectatorID > -1)
			m_Spectator.Spectate(NewSpectatorID);
	}

	return IsMultiViewIdSet();
}

float CGameClient::CalculateMultiViewMultiplier(vec2 TargetPos)
{
	float MaxCameraDist = 200.0f;
	float MinCameraDist = 20.0f;
	float MaxVel = g_Config.m_ClMultiViewSensitivity / 150.0f;
	float MinVel = 0.007f;
	float CurrentCameraDistance = distance(m_MultiView.m_OldPos, TargetPos);
	float UpperLimit = 1.0f;

	if(m_MultiView.m_Teleported && CurrentCameraDistance <= 100)
		m_MultiView.m_Teleported = false;

	// somebody got teleported very likely
	if((m_MultiView.m_Teleported || CurrentCameraDistance - m_MultiView.m_OldCameraDistance > 100) && m_MultiView.m_OldCameraDistance != 0.0f)
	{
		UpperLimit = 0.1f; // dont try to compensate it by flickering
		m_MultiView.m_Teleported = true;
	}
	m_MultiView.m_OldCameraDistance = CurrentCameraDistance;

	return clamp(MapValue(MaxCameraDist, MinCameraDist, MaxVel, MinVel, CurrentCameraDistance), MinVel, UpperLimit);
}

float CGameClient::CalculateMultiViewZoom(vec2 MinPos, vec2 MaxPos, float Vel)
{
	float Ratio = Graphics()->ScreenAspect();
	float ZoomX = 0.0f, ZoomY;

	// only calc two axis if the aspect ratio is not 1:1
	if(Ratio != 1.0f)
		ZoomX = (0.001309f - 0.000328 * Ratio) * (MaxPos.x - MinPos.x) + (0.741413f - 0.032959 * Ratio);

	// calculate the according zoom with linear function
	ZoomY = 0.001309f * (MaxPos.y - MinPos.y) + 0.741413f;
	// choose the highest zoom
	float Zoom = std::max(ZoomX, ZoomY);
	// zoom out to maximum 10 percent of the current zoom for 70 velocity
	float Diff = clamp(MapValue(70.0f, 15.0f, Zoom * 0.10f, 0.0f, Vel), 0.0f, Zoom * 0.10f);
	// zoom should stay between 1.1 and 20.0
	Zoom = clamp(Zoom + Diff, 1.1f, 20.0f);
	// dont go below default zoom
	Zoom = std::max(float(std::pow(CCamera::ZOOM_STEP, g_Config.m_ClDefaultZoom - 10)), Zoom);
	// add the user preference
	Zoom -= (Zoom * 0.1f) * m_MultiViewPersonalZoom;
	m_MultiView.m_OldPersonalZoom = m_MultiViewPersonalZoom;

	return Zoom;
}

float CGameClient::MapValue(float MaxValue, float MinValue, float MaxRange, float MinRange, float Value)
{
	return (MaxRange - MinRange) / (MaxValue - MinValue) * (Value - MinValue) + MinRange;
}

void CGameClient::ResetMultiView()
{
	m_Camera.SetZoom(std::pow(CCamera::ZOOM_STEP, g_Config.m_ClDefaultZoom - 10), g_Config.m_ClSmoothZoomTime);
	m_MultiViewPersonalZoom = 0;
	m_MultiViewActivated = false;
	m_MultiView.m_Solo = false;
	m_MultiView.m_IsInit = false;
	m_MultiView.m_Teleported = false;
	m_MultiView.m_OldCameraDistance = 0.0f;
}

void CGameClient::CleanMultiViewIds()
{
	std::fill(std::begin(m_aMultiViewId), std::end(m_aMultiViewId), false);
	std::fill(std::begin(m_MultiView.m_aLastFreeze), std::end(m_MultiView.m_aLastFreeze), 0.0f);
	std::fill(std::begin(m_MultiView.m_aVanish), std::end(m_MultiView.m_aVanish), false);
}

void CGameClient::CleanMultiViewId(int ClientID)
{
	if(ClientID >= MAX_CLIENTS || ClientID < 0)
		return;

	m_aMultiViewId[ClientID] = false;
	m_MultiView.m_aLastFreeze[ClientID] = 0.0f;
	m_MultiView.m_aVanish[ClientID] = false;
}

bool CGameClient::IsMultiViewIdSet()
{
	return std::any_of(std::begin(m_aMultiViewId), std::end(m_aMultiViewId), [](bool IsSet) { return IsSet; });
}

int CGameClient::FindFirstMultiViewId()
{
	int ClientID = -1;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aMultiViewId[i] && !m_MultiView.m_aVanish[i])
			return i;
	}
	return ClientID;
}
