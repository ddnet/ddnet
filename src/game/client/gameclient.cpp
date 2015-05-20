/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/editor.h>
#include <engine/engine.h>
#include <engine/friends.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/demo.h>
#include <engine/map.h>
#include <engine/storage.h>
#include <engine/sound.h>
#include <engine/serverbrowser.h>
#include <engine/updater.h>
#include <engine/shared/demo.h>
#include <engine/shared/config.h>

#include <game/generated/protocol.h>
#include <game/generated/client_data.h>

#include <base/math.h>
#include <base/vmath.h>

#include <game/localization.h>
#include <game/version.h>
#include "render.h"

#include "gameclient.h"

#include "components/binds.h"
#include "components/broadcast.h"
#include "components/camera.h"
#include "components/chat.h"
#include "components/console.h"
#include "components/controls.h"
#include "components/countryflags.h"
#include "components/damageind.h"
#include "components/debughud.h"
#include "components/detailed_stats.h"
#include "components/effects.h"
#include "components/emoticon.h"
#include "components/flow.h"
#include "components/hud.h"
#include "components/items.h"
#include "components/killmessages.h"
#include "components/mapimages.h"
#include "components/maplayers.h"
#include "components/mapsounds.h"
#include "components/menus.h"
#include "components/motd.h"
#include "components/particles.h"
#include "components/players.h"
#include "components/nameplates.h"
#include "components/scoreboard.h"
#include "components/skins.h"
#include "components/sounds.h"
#include "components/spectator.h"
#include "components/voting.h"

#include <base/system.h>
#include "components/race_demo.h"
#include "components/ghost.h"
#include <base/tl/sorted_array.h>

CGameClient g_GameClient;

// instanciate all systems
static CKillMessages gs_KillMessages;
static CCamera gs_Camera;
static CChat gs_Chat;
static CMotd gs_Motd;
static CBroadcast gs_Broadcast;
static CGameConsole gs_GameConsole;
static CBinds gs_Binds;
static CParticles gs_Particles;
static CMenus gs_Menus;
static CSkins gs_Skins;
static CCountryFlags gs_CountryFlags;
static CFlow gs_Flow;
static CHud gs_Hud;
static CDebugHud gs_DebugHud;
static CControls gs_Controls;
static CEffects gs_Effects;
static CScoreboard gs_Scoreboard;
static CDetailedStats gs_DetailedStats;
static CSounds gs_Sounds;
static CEmoticon gs_Emoticon;
static CDamageInd gsDamageInd;
static CVoting gs_Voting;
static CSpectator gs_Spectator;

static CPlayers gs_Players;
static CNamePlates gs_NamePlates;
static CItems gs_Items;
static CMapImages gs_MapImages;

static CMapLayers gs_MapLayersBackGround(CMapLayers::TYPE_BACKGROUND);
static CMapLayers gs_MapLayersForeGround(CMapLayers::TYPE_FOREGROUND);

static CMapSounds gs_MapSounds;

static CRaceDemo gs_RaceDemo;
static CGhost gs_Ghost;

CGameClient::CStack::CStack() { m_Num = 0; }
void CGameClient::CStack::Add(class CComponent *pComponent) { m_paComponents[m_Num++] = pComponent; }

const char *CGameClient::Version() { return GAME_VERSION; }
const char *CGameClient::NetVersion() { return GAME_NETVERSION; }
const char *CGameClient::GetItemName(int Type) { return m_NetObjHandler.GetObjName(Type); }

void CGameClient::ResetDummyInput()
{
	m_pControls->ResetInput(!g_Config.m_ClDummy);
}

void CGameClient::OnConsoleInit()
{
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pTextRender = Kernel()->RequestInterface<ITextRender>();
	m_pSound = Kernel()->RequestInterface<ISound>();
	m_pInput = Kernel()->RequestInterface<IInput>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pDemoPlayer = Kernel()->RequestInterface<IDemoPlayer>();
	m_pServerBrowser = Kernel()->RequestInterface<IServerBrowser>();
	m_pEditor = Kernel()->RequestInterface<IEditor>();
	m_pFriends = Kernel()->RequestInterface<IFriends>();
#if defined(CONF_FAMILY_WINDOWS) || (defined(CONF_PLATFORM_LINUX) && !defined(__ANDROID__))
	m_pUpdater = Kernel()->RequestInterface<IUpdater>();
#endif

	// setup pointers
	m_pBinds = &::gs_Binds;
	m_pGameConsole = &::gs_GameConsole;
	m_pParticles = &::gs_Particles;
	m_pMenus = &::gs_Menus;
	m_pSkins = &::gs_Skins;
	m_pCountryFlags = &::gs_CountryFlags;
	m_pChat = &::gs_Chat;
	m_pFlow = &::gs_Flow;
	m_pCamera = &::gs_Camera;
	m_pControls = &::gs_Controls;
	m_pEffects = &::gs_Effects;
	m_pSounds = &::gs_Sounds;
	m_pMotd = &::gs_Motd;
	m_pDamageind = &::gsDamageInd;
	m_pMapimages = &::gs_MapImages;
	m_pVoting = &::gs_Voting;
	m_pScoreboard = &::gs_Scoreboard;
	m_pDetailedStats = &::gs_DetailedStats;
	m_pItems = &::gs_Items;
	m_pMapLayersBackGround = &::gs_MapLayersBackGround;
	m_pMapLayersForeGround = &::gs_MapLayersForeGround;

	m_pMapSounds = &::gs_MapSounds;

	m_pRaceDemo = &::gs_RaceDemo;
	m_pGhost = &::gs_Ghost;

	// make a list of all the systems, make sure to add them in the correct render order
	m_All.Add(m_pSkins);
	m_All.Add(m_pCountryFlags);
	m_All.Add(m_pMapimages);
	m_All.Add(m_pEffects); // doesn't render anything, just updates effects
	m_All.Add(m_pParticles);
	m_All.Add(m_pBinds);
	m_All.Add(m_pControls);
	m_All.Add(m_pCamera);
	m_All.Add(m_pSounds);
	m_All.Add(m_pVoting);
	m_All.Add(m_pParticles); // doesn't render anything, just updates all the particles
	m_All.Add(m_pRaceDemo);
	m_All.Add(m_pMapSounds);

	m_All.Add(&gs_MapLayersBackGround); // first to render
	m_All.Add(&m_pParticles->m_RenderTrail);
	m_All.Add(m_pItems);
	m_All.Add(&gs_Players);
	m_All.Add(m_pGhost);
	m_All.Add(&gs_MapLayersForeGround);
	m_All.Add(&m_pParticles->m_RenderExplosions);
	m_All.Add(&gs_NamePlates);
	m_All.Add(&m_pParticles->m_RenderGeneral);
	m_All.Add(m_pDamageind);
	m_All.Add(&gs_Hud);
	m_All.Add(&gs_Spectator);
	m_All.Add(&gs_Emoticon);
	m_All.Add(&gs_KillMessages);
	m_All.Add(m_pChat);
	m_All.Add(&gs_Broadcast);
	m_All.Add(&gs_DebugHud);
	m_All.Add(&gs_Scoreboard);
	m_All.Add(&gs_DetailedStats);
	m_All.Add(m_pMotd);
	m_All.Add(m_pMenus);
	m_All.Add(m_pGameConsole);

	// build the input stack
	m_Input.Add(&m_pMenus->m_Binder); // this will take over all input when we want to bind a key
	m_Input.Add(&m_pBinds->m_SpecialBinds);
	m_Input.Add(m_pGameConsole);
	m_Input.Add(m_pChat); // chat has higher prio due to tha you can quit it by pressing esc
	m_Input.Add(m_pMotd); // for pressing esc to remove it
	m_Input.Add(m_pMenus);
	m_Input.Add(&gs_Spectator);
	m_Input.Add(&gs_Emoticon);
	m_Input.Add(m_pControls);
	m_Input.Add(m_pBinds);

	// add the some console commands
	Console()->Register("team", "i", CFGFLAG_CLIENT, ConTeam, this, "Switch team");
	Console()->Register("kill", "", CFGFLAG_CLIENT, ConKill, this, "Kill yourself");

	// register server dummy commands for tab completion
	Console()->Register("tune", "si", CFGFLAG_SERVER, 0, 0, "Tune variable to value");
	Console()->Register("tune_reset", "", CFGFLAG_SERVER, 0, 0, "Reset tuning");
	Console()->Register("tune_dump", "", CFGFLAG_SERVER, 0, 0, "Dump tuning");
	Console()->Register("change_map", "?r", CFGFLAG_SERVER, 0, 0, "Change map");
	Console()->Register("restart", "?i", CFGFLAG_SERVER, 0, 0, "Restart in x seconds");
	Console()->Register("broadcast", "r", CFGFLAG_SERVER, 0, 0, "Broadcast message");
	Console()->Register("say", "r", CFGFLAG_SERVER, 0, 0, "Say in chat");
	Console()->Register("set_team", "ii?i", CFGFLAG_SERVER, 0, 0, "Set team of player to team");
	Console()->Register("set_team_all", "i", CFGFLAG_SERVER, 0, 0, "Set team of all players to team");
	Console()->Register("add_vote", "sr", CFGFLAG_SERVER, 0, 0, "Add a voting option");
	Console()->Register("remove_vote", "s", CFGFLAG_SERVER, 0, 0, "remove a voting option");
	Console()->Register("force_vote", "ss?r", CFGFLAG_SERVER, 0, 0, "Force a voting option");
	Console()->Register("clear_votes", "", CFGFLAG_SERVER, 0, 0, "Clears the voting options");
	Console()->Register("vote", "r", CFGFLAG_SERVER, 0, 0, "Force a vote to yes/no");
	Console()->Register("swap_teams", "", CFGFLAG_SERVER, 0, 0, "Swap the current teams");
	Console()->Register("shuffle_teams", "", CFGFLAG_SERVER, 0, 0, "Shuffle the current teams");


	for(int i = 0; i < m_All.m_Num; i++)
		m_All.m_paComponents[i]->m_pClient = this;

	// let all the other components register their console commands
	for(int i = 0; i < m_All.m_Num; i++)
		m_All.m_paComponents[i]->OnConsoleInit();


	//
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

	Console()->Chain("cl_dummy", ConchainSpecialDummy, this);

	//
	m_SuppressEvents = false;
}

void CGameClient::OnInit()
{
	m_pGraphics = Kernel()->RequestInterface<IGraphics>();

	// propagate pointers
	m_UI.SetGraphics(Graphics(), TextRender());
	m_RenderTools.m_pGraphics = Graphics();
	m_RenderTools.m_pUI = UI();

	int64 Start = time_get();

	// set the language
	g_Localization.Load(g_Config.m_ClLanguagefile, Storage(), Console());

	// TODO: this should be different
	// setup item sizes
	for(int i = 0; i < NUM_NETOBJTYPES; i++)
		Client()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	// load default font
	static CFont *pDefaultFont = 0;
	char aFilename[512];
	IOHANDLE File = Storage()->OpenFile("fonts/DejaVuSans.ttf", IOFLAG_READ, IStorage::TYPE_ALL, aFilename, sizeof(aFilename));
	if(File)
	{
		io_close(File);
		pDefaultFont = TextRender()->LoadFont(aFilename);
		TextRender()->SetDefaultFont(pDefaultFont);
	}
	if(!pDefaultFont)
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gameclient", "failed to load font. filename='fonts/DejaVuSans.ttf'");

	// init all components
	for(int i = m_All.m_Num-1; i >= 0; --i)
		m_All.m_paComponents[i]->OnInit();

	char aBuf[256];

	// setup load amount// load textures
	for(int i = 0; i < g_pData->m_NumImages; i++)
	{
		g_pData->m_aImages[i].m_Id = Graphics()->LoadTexture(g_pData->m_aImages[i].m_pFilename, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
		g_GameClient.m_pMenus->RenderLoading();
	}

#if defined(__ANDROID__)
	m_pMapimages->OnMapLoad(); // Reload map textures on Android
#endif

	for(int i = 0; i < m_All.m_Num; i++)
		m_All.m_paComponents[i]->OnReset();

	int64 End = time_get();
	str_format(aBuf, sizeof(aBuf), "initialisation finished after %.2fms", ((End-Start)*1000)/(float)time_freq());
	Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "gameclient", aBuf);

	m_ServerMode = SERVERMODE_PURE;

	m_DDRaceMsgSent[0] = false;
	m_DDRaceMsgSent[1] = false;
	m_ShowOthers[0] = -1;
	m_ShowOthers[1] = -1;

	// Set free binds to DDRace binds if it's active
	if(!g_Config.m_ClDDRaceBindsSet && g_Config.m_ClDDRaceBinds)
		gs_Binds.SetDDRaceBinds(true);

	if(g_Config.m_ClTimeoutCode[0] == '\0' || str_comp(g_Config.m_ClTimeoutCode, "hGuEYnfxicsXGwFq") == 0)
	{
		for(unsigned int i = 0; i < 16; i++)
		{
			if (rand() % 2)
				g_Config.m_ClTimeoutCode[i] = (rand() % 26) + 97;
			else
				g_Config.m_ClTimeoutCode[i] = (rand() % 26) + 65;
		}
	}

	if(g_Config.m_ClDummyTimeoutCode[0] == '\0' || str_comp(g_Config.m_ClDummyTimeoutCode, "hGuEYnfxicsXGwFq") == 0)
	{
		for(unsigned int i = 0; i < 16; i++)
		{
			if (rand() % 2)
				g_Config.m_ClDummyTimeoutCode[i] = (rand() % 26) + 97;
			else
				g_Config.m_ClDummyTimeoutCode[i] = (rand() % 26) + 65;
		}
	}
}

void CGameClient::DispatchInput()
{
	// handle mouse movement
	float x = 0.0f, y = 0.0f;
	Input()->MouseRelative(&x, &y);
#if !defined(__ANDROID__) // No relative mouse on Android
	if(x != 0.0f || y != 0.0f)
#endif
	{
		for(int h = 0; h < m_Input.m_Num; h++)
		{
			if(m_Input.m_paComponents[h]->OnMouseMove(x, y))
				break;
		}
	}

	// handle key presses
	for(int i = 0; i < Input()->NumEvents(); i++)
	{
		IInput::CEvent e = Input()->GetEvent(i);

		for(int h = 0; h < m_Input.m_Num; h++)
		{
			if(m_Input.m_paComponents[h]->OnInput(e))
			{
				//dbg_msg("", "%d char=%d key=%d flags=%d", h, e.ch, e.key, e.flags);
				break;
			}
		}
	}

	// clear all events for this frame
	Input()->ClearEvents();
}


int CGameClient::OnSnapInput(int *pData)
{
	return m_pControls->SnapInput(pData);
}

void CGameClient::OnConnected()
{
	m_Layers.Init(Kernel());
	m_Collision.Init(Layers());

	RenderTools()->RenderTilemapGenerateSkip(Layers());

	for(int i = 0; i < m_All.m_Num; i++)
	{
		m_All.m_paComponents[i]->OnMapLoad();
		m_All.m_paComponents[i]->OnReset();
	}

	CServerInfo CurrentServerInfo;
	Client()->GetServerInfo(&CurrentServerInfo);

	m_ServerMode = SERVERMODE_PURE;

	// send the inital info
	SendInfo(true);
	// we should keep this in for now, because otherwise you can't spectate
	// people at start as the other info 64 packet is only sent after the first
	// snap
	Client()->Rcon("crashmeplx");
}

void CGameClient::OnReset()
{
	// clear out the invalid pointers
	m_LastNewPredictedTick[0] = -1;
	m_LastNewPredictedTick[1] = -1;
	mem_zero(&g_GameClient.m_Snap, sizeof(g_GameClient.m_Snap));

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aClients[i].Reset();

	for(int i = 0; i < m_All.m_Num; i++)
		m_All.m_paComponents[i]->OnReset();

	m_DemoSpecID = SPEC_FREEVIEW;
	m_FlagDropTick[TEAM_RED] = 0;
	m_FlagDropTick[TEAM_BLUE] = 0;
	m_LastRoundStartTick = -1;
	m_LastFlagCarrierRed = -4;
	m_LastFlagCarrierBlue = -4;
	m_Tuning[g_Config.m_ClDummy] = CTuningParams();

	m_Teams.Reset();
	m_DDRaceMsgSent[0] = false;
	m_DDRaceMsgSent[1] = false;
	m_ShowOthers[0] = -1;
	m_ShowOthers[1] = -1;

	for(int i = 0; i < 150; i++)
		m_aWeaponData[i].m_Tick = -1;
}


void CGameClient::UpdatePositions()
{
	// local character position
	if(g_Config.m_ClPredict && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if (!g_Config.m_ClAntiPingPlayers)
		{
			if(!m_Snap.m_pLocalCharacter || (m_Snap.m_pGameInfoObj && m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
			{
				// don't use predicted
			}
			else
				m_LocalCharacterPos = mix(m_PredictedPrevChar.m_Pos, m_PredictedChar.m_Pos, Client()->PredIntraGameTick());
		}
		else
		{
			if(!(m_Snap.m_pGameInfoObj && m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
			{
				if (m_Snap.m_pLocalCharacter)
					m_LocalCharacterPos = mix(m_PredictedPrevChar.m_Pos, m_PredictedChar.m_Pos, Client()->PredIntraGameTick());
			}
	//		else
	//			m_LocalCharacterPos = mix(m_PredictedPrevChar.m_Pos, m_PredictedChar.m_Pos, Client()->PredIntraGameTick());
		}
	}
	else if(m_Snap.m_pLocalCharacter && m_Snap.m_pLocalPrevCharacter)
	{
		m_LocalCharacterPos = mix(
			vec2(m_Snap.m_pLocalPrevCharacter->m_X, m_Snap.m_pLocalPrevCharacter->m_Y),
			vec2(m_Snap.m_pLocalCharacter->m_X, m_Snap.m_pLocalCharacter->m_Y), Client()->IntraGameTick());
	}

	if (g_Config.m_ClAntiPingPlayers)
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (!m_Snap.m_aCharacters[i].m_Active)
				continue;

			if (m_Snap.m_pLocalCharacter && m_Snap.m_pLocalPrevCharacter && g_Config.m_ClPredict /* && g_Config.m_AntiPing */ && !(m_Snap.m_LocalClientID == -1 || !m_Snap.m_aCharacters[m_Snap.m_LocalClientID].m_Active))
				m_Snap.m_aCharacters[i].m_Position = mix(m_aClients[i].m_PrevPredicted.m_Pos, m_aClients[i].m_Predicted.m_Pos, Client()->PredIntraGameTick());
			else
				m_Snap.m_aCharacters[i].m_Position = mix(vec2(m_Snap.m_aCharacters[i].m_Prev.m_X, m_Snap.m_aCharacters[i].m_Prev.m_Y), vec2(m_Snap.m_aCharacters[i].m_Cur.m_X, m_Snap.m_aCharacters[i].m_Cur.m_Y), Client()->IntraGameTick());
		}
	}

	// spectator position
	if(m_Snap.m_SpecInfo.m_Active)
	{
		if(Client()->State() == IClient::STATE_DEMOPLAYBACK && DemoPlayer()->GetDemoType() == IDemoPlayer::DEMOTYPE_SERVER &&
			m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW)
		{
			m_Snap.m_SpecInfo.m_Position = mix(
				vec2(m_Snap.m_aCharacters[m_Snap.m_SpecInfo.m_SpectatorID].m_Prev.m_X, m_Snap.m_aCharacters[m_Snap.m_SpecInfo.m_SpectatorID].m_Prev.m_Y),
				vec2(m_Snap.m_aCharacters[m_Snap.m_SpecInfo.m_SpectatorID].m_Cur.m_X, m_Snap.m_aCharacters[m_Snap.m_SpecInfo.m_SpectatorID].m_Cur.m_Y),
				Client()->IntraGameTick());
			m_Snap.m_SpecInfo.m_UsePosition = true;
		}
		else if(m_Snap.m_pSpectatorInfo && (Client()->State() == IClient::STATE_DEMOPLAYBACK || m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW))
		{
			if(m_Snap.m_pPrevSpectatorInfo)
				m_Snap.m_SpecInfo.m_Position = mix(vec2(m_Snap.m_pPrevSpectatorInfo->m_X, m_Snap.m_pPrevSpectatorInfo->m_Y),
													vec2(m_Snap.m_pSpectatorInfo->m_X, m_Snap.m_pSpectatorInfo->m_Y), Client()->IntraGameTick());
			else
				m_Snap.m_SpecInfo.m_Position = vec2(m_Snap.m_pSpectatorInfo->m_X, m_Snap.m_pSpectatorInfo->m_Y);
			m_Snap.m_SpecInfo.m_UsePosition = true;
		}
	}
}


static void Evolve(CNetObj_Character *pCharacter, int Tick)
{
	CWorldCore TempWorld;
	CCharacterCore TempCore;
	CTeamsCore TempTeams;
	mem_zero(&TempCore, sizeof(TempCore));
	mem_zero(&TempTeams, sizeof(TempTeams));
	TempCore.Init(&TempWorld, g_GameClient.Collision(), &TempTeams);
	TempCore.Read(pCharacter);
	TempCore.m_ActiveWeapon = pCharacter->m_Weapon;

	while(pCharacter->m_Tick < Tick)
	{
		pCharacter->m_Tick++;
		TempCore.Tick(false, true);
		TempCore.Move();
		TempCore.Quantize();
	}

	TempCore.Write(pCharacter);
}

void CGameClient::OnRender()
{
	/*Graphics()->Clear(1,0,0);

	menus->render_background();
	return;*/
	/*
	Graphics()->Clear(1,0,0);
	Graphics()->MapScreen(0,0,100,100);

	Graphics()->QuadsBegin();
		Graphics()->SetColor(1,1,1,1);
		Graphics()->QuadsDraw(50, 50, 30, 30);
	Graphics()->QuadsEnd();

	return;*/

	// update the local character and spectate position
	UpdatePositions();

	// dispatch all input to systems
	DispatchInput();

	// render all systems
	for(int i = 0; i < m_All.m_Num; i++)
		m_All.m_paComponents[i]->OnRender();

	// clear new tick flags
	m_NewTick = false;
	m_NewPredictedTick = false;

	if(g_Config.m_ClAntiPing != m_CurrentAntiPing)
	{
		g_Config.m_ClAntiPingPlayers = g_Config.m_ClAntiPing;
		g_Config.m_ClAntiPingGrenade = g_Config.m_ClAntiPing;
		g_Config.m_ClAntiPingWeapons = g_Config.m_ClAntiPing;
		m_CurrentAntiPing = g_Config.m_ClAntiPing;
	}

	if(g_Config.m_ClDummy && !Client()->DummyConnected())
		g_Config.m_ClDummy = 0;

	// resend player and dummy info if it was filtered by server
	if(Client()->State() == IClient::STATE_ONLINE && !m_pMenus->IsActive()) {
		if(m_CheckInfo[0] == 0) {
			if(
			str_comp(m_aClients[Client()->m_LocalIDs[0]].m_aName, g_Config.m_PlayerName) ||
			str_comp(m_aClients[Client()->m_LocalIDs[0]].m_aClan, g_Config.m_PlayerClan) ||
			m_aClients[Client()->m_LocalIDs[0]].m_Country != g_Config.m_PlayerCountry ||
			str_comp(m_aClients[Client()->m_LocalIDs[0]].m_aSkinName, g_Config.m_PlayerSkin) ||
			m_aClients[Client()->m_LocalIDs[0]].m_UseCustomColor != g_Config.m_PlayerUseCustomColor ||
			m_aClients[Client()->m_LocalIDs[0]].m_ColorBody != g_Config.m_PlayerColorBody ||
			m_aClients[Client()->m_LocalIDs[0]].m_ColorFeet != g_Config.m_PlayerColorFeet
			)
				SendInfo(false);
			else
				m_CheckInfo[0] = -1;
		}

		if(m_CheckInfo[0] > 0)
			m_CheckInfo[0]--;

		if(Client()->DummyConnected()) {
			if(m_CheckInfo[1] == 0) {
				if(
				str_comp(m_aClients[Client()->m_LocalIDs[1]].m_aName, g_Config.m_DummyName) ||
				str_comp(m_aClients[Client()->m_LocalIDs[1]].m_aClan, g_Config.m_DummyClan) ||
				m_aClients[Client()->m_LocalIDs[1]].m_Country != g_Config.m_DummyCountry ||
				str_comp(m_aClients[Client()->m_LocalIDs[1]].m_aSkinName, g_Config.m_DummySkin) ||
				m_aClients[Client()->m_LocalIDs[1]].m_UseCustomColor != g_Config.m_DummyUseCustomColor ||
				m_aClients[Client()->m_LocalIDs[1]].m_ColorBody != g_Config.m_DummyColorBody ||
				m_aClients[Client()->m_LocalIDs[1]].m_ColorFeet != g_Config.m_DummyColorFeet
				)
					SendDummyInfo(false);
				else
					m_CheckInfo[1] = -1;
			}

			if(m_CheckInfo[1] > 0)
				m_CheckInfo[1]--;
		}
	}
}

void CGameClient::OnDummyDisconnect()
{
	m_DDRaceMsgSent[1] = false;
	m_ShowOthers[1] = -1;
	m_LastNewPredictedTick[1] = -1;
}

void CGameClient::OnRelease()
{
	// release all systems
	for(int i = 0; i < m_All.m_Num; i++)
		m_All.m_paComponents[i]->OnRelease();
}

void CGameClient::OnMessage(int MsgId, CUnpacker *pUnpacker, bool IsDummy)
{
	// special messages
	if(MsgId == NETMSGTYPE_SV_EXTRAPROJECTILE && !IsDummy)
	{
		int Num = pUnpacker->GetInt();

		for(int k = 0; k < Num; k++)
		{
			CNetObj_Projectile Proj;
			for(unsigned i = 0; i < sizeof(CNetObj_Projectile)/sizeof(int); i++)
				((int *)&Proj)[i] = pUnpacker->GetInt();

			if(pUnpacker->Error())
				return;

			g_GameClient.m_pItems->AddExtraProjectile(&Proj);

			if(g_Config.m_ClAntiPingWeapons && Proj.m_Type == WEAPON_GRENADE && !UseExtraInfo(&Proj))
			{
				vec2 StartPos;
				vec2 Direction;
				ExtractInfo(&Proj, &StartPos, &Direction, 1);
				if(CWeaponData *pCurrentData = GetWeaponData(Proj.m_StartTick))
				{
					if(CWeaponData *pMatchingData = FindWeaponData(Proj.m_StartTick))
					{
						if(distance(pMatchingData->m_Direction, Direction) < 0.015)
							Direction = pMatchingData->m_Direction;
						else if(int *pData = Client()->GetInput(Proj.m_StartTick+2))
						{
							CNetObj_PlayerInput *pNextInput = (CNetObj_PlayerInput*) pData;
							vec2 NextDirection = normalize(vec2(pNextInput->m_TargetX, pNextInput->m_TargetY));
							if(distance(NextDirection, Direction) < 0.015)
								Direction = NextDirection;
						}
						if(distance(pMatchingData->StartPos(), StartPos) < 1)
							StartPos = pMatchingData->StartPos();
					}
					pCurrentData->m_Tick = Proj.m_StartTick;
					pCurrentData->m_Direction = Direction;
					pCurrentData->m_Pos = StartPos - Direction * 28.0f * 0.75f;
				}
			}
		}

		return;
	}
	else if(MsgId == NETMSGTYPE_SV_TUNEPARAMS)
	{
		// unpack the new tuning
		CTuningParams NewTuning;
		int *pParams = (int *)&NewTuning;
		// No jetpack on DDNet incompatible servers:
		NewTuning.m_JetpackStrength = 0;
		for(unsigned i = 0; i < sizeof(CTuningParams)/sizeof(int); i++)
		{
			int value = pUnpacker->GetInt();

			// check for unpacking errors
			if(pUnpacker->Error())
				break;

			pParams[i] = value;
		}

		m_ServerMode = SERVERMODE_PURE;

		// apply new tuning
		m_Tuning[IsDummy ? !g_Config.m_ClDummy : g_Config.m_ClDummy] = NewTuning;
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

	if(IsDummy)
	{
		if(MsgId == NETMSGTYPE_SV_CHAT
			&& Client()->m_LocalIDs[0] >= 0
			&& Client()->m_LocalIDs[1] >= 0)
		{
			CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;

			if((pMsg->m_Team == 1
					&& (m_aClients[Client()->m_LocalIDs[0]].m_Team != m_aClients[Client()->m_LocalIDs[1]].m_Team
						|| m_Teams.Team(Client()->m_LocalIDs[0]) != m_Teams.Team(Client()->m_LocalIDs[1])))
				|| pMsg->m_Team > 1)
			{
				m_pChat->OnMessage(MsgId, pRawMsg);
			}
		}
		return; // no need of all that stuff for the dummy
	}

	// TODO: this should be done smarter
	for(int i = 0; i < m_All.m_Num; i++)
		m_All.m_paComponents[i]->OnMessage(MsgId, pRawMsg);

	if(MsgId == NETMSGTYPE_SV_READYTOENTER)
	{
		Client()->EnterGame();
	}
	else if (MsgId == NETMSGTYPE_SV_EMOTICON)
	{
		CNetMsg_Sv_Emoticon *pMsg = (CNetMsg_Sv_Emoticon *)pRawMsg;

		// apply
		m_aClients[pMsg->m_ClientID].m_Emoticon = pMsg->m_Emoticon;
		m_aClients[pMsg->m_ClientID].m_EmoticonStart = Client()->GameTick();
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
				g_GameClient.m_pSounds->Enqueue(CSounds::CHN_GLOBAL, pMsg->m_SoundID);
		}
		else
		{
			if(g_Config.m_SndGame)
				g_GameClient.m_pSounds->Play(CSounds::CHN_GLOBAL, pMsg->m_SoundID, 1.0f);
		}
	}
	else if(MsgId == NETMSGTYPE_SV_TEAMSSTATE)
	{
		unsigned int i;

		for(i = 0; i < MAX_CLIENTS; i++)
		{
			int Team = pUnpacker->GetInt();
			bool WentWrong = false;

			if(pUnpacker->Error())
				WentWrong = true;

			if(!WentWrong && Team >= 0 && Team < MAX_CLIENTS)
				m_Teams.Team(i, Team);
			else if (Team != MAX_CLIENTS)
				WentWrong = true;

			if(WentWrong)
			{
				m_Teams.Team(i, 0);
				break;
			}
		}

		if (i <= 16)
			m_Teams.m_IsDDRace16 = true;
	}
	else if(MsgId == NETMSGTYPE_SV_PLAYERTIME)
	{
		CNetMsg_Sv_PlayerTime *pMsg = (CNetMsg_Sv_PlayerTime *)pRawMsg;
		m_aClients[pMsg->m_ClientID].m_Score = pMsg->m_Time;
	}
}

void CGameClient::OnStateChange(int NewState, int OldState)
{
	// reset everything when not already connected (to keep gathered stuff)
	if(NewState < IClient::STATE_ONLINE)
		OnReset();

	// then change the state
	for(int i = 0; i < m_All.m_Num; i++)
		m_All.m_paComponents[i]->OnStateChange(NewState, OldState);
}

void CGameClient::OnShutdown()
{
	m_pRaceDemo->OnShutdown();
}

void CGameClient::OnEnterGame()
{
	g_GameClient.m_pEffects->ResetDamageIndicator();
}

void CGameClient::OnGameOver()
{
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK && g_Config.m_ClEditor == 0)
		Client()->AutoScreenshot_Start();
}

void CGameClient::OnStartGame()
{
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
		Client()->DemoRecorder_HandleAutoStart();
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aStats[i].Reset();
}

void CGameClient::OnFlagGrab(int TeamID)
{
	if(TeamID == TEAM_RED)
		m_aStats[m_Snap.m_pGameDataObj->m_FlagCarrierRed].m_FlagGrabs++;
	else
		m_aStats[m_Snap.m_pGameDataObj->m_FlagCarrierBlue].m_FlagGrabs++;
}

void CGameClient::OnRconLine(const char *pLine)
{
	m_pGameConsole->PrintLine(CGameConsole::CONSOLETYPE_REMOTE, pLine);
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

		if(Item.m_Type == NETEVENTTYPE_DAMAGEIND)
		{
			CNetEvent_DamageInd *ev = (CNetEvent_DamageInd *)pData;
			g_GameClient.m_pEffects->DamageIndicator(vec2(ev->m_X, ev->m_Y), GetDirection(ev->m_Angle));
		}
		else if(Item.m_Type == NETEVENTTYPE_EXPLOSION)
		{
			CNetEvent_Explosion *ev = (CNetEvent_Explosion *)pData;
			g_GameClient.m_pEffects->Explosion(vec2(ev->m_X, ev->m_Y));
		}
		else if(Item.m_Type == NETEVENTTYPE_HAMMERHIT)
		{
			CNetEvent_HammerHit *ev = (CNetEvent_HammerHit *)pData;
			g_GameClient.m_pEffects->HammerHit(vec2(ev->m_X, ev->m_Y));
		}
		else if(Item.m_Type == NETEVENTTYPE_SPAWN)
		{
			CNetEvent_Spawn *ev = (CNetEvent_Spawn *)pData;
			g_GameClient.m_pEffects->PlayerSpawn(vec2(ev->m_X, ev->m_Y));
		}
		else if(Item.m_Type == NETEVENTTYPE_DEATH)
		{
			CNetEvent_Death *ev = (CNetEvent_Death *)pData;
			g_GameClient.m_pEffects->PlayerDeath(vec2(ev->m_X, ev->m_Y), ev->m_ClientID);
		}
		else if(Item.m_Type == NETEVENTTYPE_SOUNDWORLD)
		{
			CNetEvent_SoundWorld *ev = (CNetEvent_SoundWorld *)pData;
			if(g_Config.m_SndGame && (ev->m_SoundID != SOUND_GUN_FIRE || g_Config.m_SndGun))
				g_GameClient.m_pSounds->PlayAt(CSounds::CHN_WORLD, ev->m_SoundID, 1.0f, vec2(ev->m_X, ev->m_Y));
		}
	}
}

void CGameClient::OnNewSnapshot()
{
	m_NewTick = true;

	// clear out the invalid pointers
	mem_zero(&g_GameClient.m_Snap, sizeof(g_GameClient.m_Snap));
	m_Snap.m_LocalClientID = -1;

	// secure snapshot
	{
		int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);
		for(int Index = 0; Index < Num; Index++)
		{
			IClient::CSnapItem Item;
			void *pData = Client()->SnapGetItem(IClient::SNAP_CURRENT, Index, &Item);
			if(m_NetObjHandler.ValidateObj(Item.m_Type, pData, Item.m_DataSize) != 0)
			{
				if(g_Config.m_Debug)
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "invalidated index=%d type=%d (%s) size=%d id=%d", Index, Item.m_Type, m_NetObjHandler.GetObjName(Item.m_Type), Item.m_DataSize, Item.m_ID);
					Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
				}
				Client()->SnapInvalidateItem(IClient::SNAP_CURRENT, Index);
			}
		}
	}

	ProcessEvents();

	if(g_Config.m_DbgStress)
	{
		if((Client()->GameTick()%100) == 0)
		{
			char aMessage[64];
			int MsgLen = rand()%(sizeof(aMessage)-1);
			for(int i = 0; i < MsgLen; i++)
				aMessage[i] = 'a'+(rand()%('z'-'a'));
			aMessage[MsgLen] = 0;

			CNetMsg_Cl_Say Msg;
			Msg.m_Team = rand()&1;
			Msg.m_pMessage = aMessage;
			Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
		}
	}

	// go trough all the items in the snapshot and gather the info we want
	{
		m_Snap.m_aTeamSize[TEAM_RED] = m_Snap.m_aTeamSize[TEAM_BLUE] = 0;

		for(int i = 0; i < MAX_CLIENTS; i++)
			m_aStats[i].m_Active = false;

		int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);
		for(int i = 0; i < Num; i++)
		{
			IClient::CSnapItem Item;
			const void *pData = Client()->SnapGetItem(IClient::SNAP_CURRENT, i, &Item);

			if(Item.m_Type == NETOBJTYPE_CLIENTINFO)
			{
				const CNetObj_ClientInfo *pInfo = (const CNetObj_ClientInfo *)pData;
				int ClientID = Item.m_ID;
				IntsToStr(&pInfo->m_Name0, 4, m_aClients[ClientID].m_aName);
				IntsToStr(&pInfo->m_Clan0, 3, m_aClients[ClientID].m_aClan);
				m_aClients[ClientID].m_Country = pInfo->m_Country;
				IntsToStr(&pInfo->m_Skin0, 6, m_aClients[ClientID].m_aSkinName);

				m_aClients[ClientID].m_UseCustomColor = pInfo->m_UseCustomColor;
				m_aClients[ClientID].m_ColorBody = pInfo->m_ColorBody;
				m_aClients[ClientID].m_ColorFeet = pInfo->m_ColorFeet;

				// prepare the info
				if(m_aClients[ClientID].m_aSkinName[0] == 'x' || m_aClients[ClientID].m_aSkinName[1] == '_')
					str_copy(m_aClients[ClientID].m_aSkinName, "default", 64);

				m_aClients[ClientID].m_SkinInfo.m_ColorBody = m_pSkins->GetColorV4(m_aClients[ClientID].m_ColorBody);
				m_aClients[ClientID].m_SkinInfo.m_ColorFeet = m_pSkins->GetColorV4(m_aClients[ClientID].m_ColorFeet);
				m_aClients[ClientID].m_SkinInfo.m_Size = 64;

				// find new skin
				m_aClients[ClientID].m_SkinID = g_GameClient.m_pSkins->Find(m_aClients[ClientID].m_aSkinName);
				if(m_aClients[ClientID].m_SkinID < 0)
				{
					m_aClients[ClientID].m_SkinID = g_GameClient.m_pSkins->Find("default");
					if(m_aClients[ClientID].m_SkinID < 0)
						m_aClients[ClientID].m_SkinID = 0;
				}

				if(m_aClients[ClientID].m_UseCustomColor)
					m_aClients[ClientID].m_SkinInfo.m_Texture = g_GameClient.m_pSkins->Get(m_aClients[ClientID].m_SkinID)->m_ColorTexture;
				else
				{
					m_aClients[ClientID].m_SkinInfo.m_Texture = g_GameClient.m_pSkins->Get(m_aClients[ClientID].m_SkinID)->m_OrgTexture;
					m_aClients[ClientID].m_SkinInfo.m_ColorBody = vec4(1,1,1,1);
					m_aClients[ClientID].m_SkinInfo.m_ColorFeet = vec4(1,1,1,1);
				}

				m_aClients[ClientID].UpdateRenderInfo();

			}
			else if(Item.m_Type == NETOBJTYPE_PLAYERINFO)
			{
				const CNetObj_PlayerInfo *pInfo = (const CNetObj_PlayerInfo *)pData;

				m_aClients[pInfo->m_ClientID].m_Team = pInfo->m_Team;
				m_aClients[pInfo->m_ClientID].m_Active = true;
				m_Snap.m_paPlayerInfos[pInfo->m_ClientID] = pInfo;
				m_Snap.m_NumPlayers++;

				if(pInfo->m_Local)
				{
					m_Snap.m_LocalClientID = Item.m_ID;
					m_Snap.m_pLocalInfo = pInfo;

					if(pInfo->m_Team == TEAM_SPECTATORS)
					{
						m_Snap.m_SpecInfo.m_Active = true;
						m_Snap.m_SpecInfo.m_SpectatorID = SPEC_FREEVIEW;
					}
				}

				// calculate team-balance
				if(pInfo->m_Team != TEAM_SPECTATORS)
				{
					m_Snap.m_aTeamSize[pInfo->m_Team]++;
					m_aStats[pInfo->m_ClientID].m_Active = true;
				}

			}
			else if(Item.m_Type == NETOBJTYPE_CHARACTER)
			{
				const void *pOld = Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_CHARACTER, Item.m_ID);
				m_Snap.m_aCharacters[Item.m_ID].m_Cur = *((const CNetObj_Character *)pData);
				if(pOld)
				{
					m_Snap.m_aCharacters[Item.m_ID].m_Active = true;
					m_Snap.m_aCharacters[Item.m_ID].m_Prev = *((const CNetObj_Character *)pOld);

					if(m_Snap.m_aCharacters[Item.m_ID].m_Prev.m_Tick)
						Evolve(&m_Snap.m_aCharacters[Item.m_ID].m_Prev, Client()->PrevGameTick());
					if(m_Snap.m_aCharacters[Item.m_ID].m_Cur.m_Tick)
						Evolve(&m_Snap.m_aCharacters[Item.m_ID].m_Cur, Client()->GameTick());
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
				static bool s_GameOver = 0;
				m_Snap.m_pGameInfoObj = (const CNetObj_GameInfo *)pData;
				if(!s_GameOver && m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER)
					OnGameOver();
				// this check includes the case when game is restarted -- game is not
				// over but new round is started
				else if(!(m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER)
						&& m_LastRoundStartTick != m_Snap.m_pGameInfoObj->m_RoundStartTick)
				{
					m_LastRoundStartTick = m_Snap.m_pGameInfoObj->m_RoundStartTick;
					OnStartGame();
				}
				s_GameOver = m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER;
			}
			else if(Item.m_Type == NETOBJTYPE_GAMEDATA)
			{
				m_Snap.m_pGameDataObj = (const CNetObj_GameData *)pData;
				m_Snap.m_GameDataSnapID = Item.m_ID;
				if(m_Snap.m_pGameDataObj->m_FlagCarrierRed == FLAG_TAKEN)
				{
					if(m_FlagDropTick[TEAM_RED] == 0)
						m_FlagDropTick[TEAM_RED] = Client()->GameTick();
				}
				else if(m_FlagDropTick[TEAM_RED] != 0)
						m_FlagDropTick[TEAM_RED] = 0;
				if(m_Snap.m_pGameDataObj->m_FlagCarrierBlue == FLAG_TAKEN)
				{
					if(m_FlagDropTick[TEAM_BLUE] == 0)
						m_FlagDropTick[TEAM_BLUE] = Client()->GameTick();
				}
				else if(m_FlagDropTick[TEAM_BLUE] != 0)
						m_FlagDropTick[TEAM_BLUE] = 0;
				if(m_LastFlagCarrierRed == FLAG_ATSTAND && m_Snap.m_pGameDataObj->m_FlagCarrierRed >= 0)
					OnFlagGrab(TEAM_RED);
				else if(m_LastFlagCarrierBlue == FLAG_ATSTAND && m_Snap.m_pGameDataObj->m_FlagCarrierBlue >= 0)
					OnFlagGrab(TEAM_BLUE);

				m_LastFlagCarrierRed = m_Snap.m_pGameDataObj->m_FlagCarrierRed;
				m_LastFlagCarrierBlue = m_Snap.m_pGameDataObj->m_FlagCarrierBlue;
			}
			else if(Item.m_Type == NETOBJTYPE_FLAG)
				m_Snap.m_paFlags[Item.m_ID%2] = (const CNetObj_Flag *)pData;
		}

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_aStats[i].m_Active && !m_aStats[i].m_WasActive)
			{
				m_aStats[i].Reset(); // Client connected, reset stats.
				m_aStats[i].m_Active = true;
				m_aStats[i].m_JoinDate = Client()->GameTick();
			}
			m_aStats[i].m_WasActive = m_aStats[i].m_Active;
		}
	}

	// setup local pointers
	if(m_Snap.m_LocalClientID >= 0)
	{
		CSnapState::CCharacterInfo *c = &m_Snap.m_aCharacters[m_Snap.m_LocalClientID];
		if(c->m_Active)
		{
			m_Snap.m_pLocalCharacter = &c->m_Cur;
			m_Snap.m_pLocalPrevCharacter = &c->m_Prev;
			m_LocalCharacterPos = vec2(m_Snap.m_pLocalCharacter->m_X, m_Snap.m_pLocalCharacter->m_Y);
		}
		else if(Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_CHARACTER, m_Snap.m_LocalClientID))
		{
			// player died
			m_pControls->OnPlayerDeath();
		}
	}
	else
	{
		m_Snap.m_SpecInfo.m_Active = true;
		if(Client()->State() == IClient::STATE_DEMOPLAYBACK && DemoPlayer()->GetDemoType() == IDemoPlayer::DEMOTYPE_SERVER &&
			m_DemoSpecID != SPEC_FREEVIEW && m_Snap.m_aCharacters[m_DemoSpecID].m_Active)
			m_Snap.m_SpecInfo.m_SpectatorID = m_DemoSpecID;
		else
			m_Snap.m_SpecInfo.m_SpectatorID = SPEC_FREEVIEW;
	}

	// clear out unneeded client data
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(!m_Snap.m_paPlayerInfos[i] && m_aClients[i].m_Active)
			m_aClients[i].Reset();
	}

	// update friend state
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i == m_Snap.m_LocalClientID || !m_Snap.m_paPlayerInfos[i] || !Friends()->IsFriend(m_aClients[i].m_aName, m_aClients[i].m_aClan, true))
			m_aClients[i].m_Friend = false;
		else
			m_aClients[i].m_Friend = true;
	}

	// sort player infos by name
	mem_copy(m_Snap.m_paInfoByName, m_Snap.m_paPlayerInfos, sizeof(m_Snap.m_paInfoByName));
	for(int k = 0; k < MAX_CLIENTS-1; k++) // ffs, bubblesort
	{
		for(int i = 0; i < MAX_CLIENTS-k-1; i++)
		{
			if(m_Snap.m_paInfoByName[i+1] && (!m_Snap.m_paInfoByName[i] || str_comp_nocase(m_aClients[m_Snap.m_paInfoByName[i]->m_ClientID].m_aName, m_aClients[m_Snap.m_paInfoByName[i+1]->m_ClientID].m_aName) > 0))
			{
				const CNetObj_PlayerInfo *pTmp = m_Snap.m_paInfoByName[i];
				m_Snap.m_paInfoByName[i] = m_Snap.m_paInfoByName[i+1];
				m_Snap.m_paInfoByName[i+1] = pTmp;
			}
		}
	}

	// sort player infos by score
	mem_copy(m_Snap.m_paInfoByScore, m_Snap.m_paInfoByName, sizeof(m_Snap.m_paInfoByScore));
	for(int k = 0; k < MAX_CLIENTS-1; k++) // ffs, bubblesort
	{
		for(int i = 0; i < MAX_CLIENTS-k-1; i++)
		{
			if(m_Snap.m_paInfoByScore[i+1] && (!m_Snap.m_paInfoByScore[i] || m_Snap.m_paInfoByScore[i]->m_Score < m_Snap.m_paInfoByScore[i+1]->m_Score))
			{
				const CNetObj_PlayerInfo *pTmp = m_Snap.m_paInfoByScore[i];
				m_Snap.m_paInfoByScore[i] = m_Snap.m_paInfoByScore[i+1];
				m_Snap.m_paInfoByScore[i+1] = pTmp;
			}
		}
	}

	// sort player infos by team
	//int Teams[3] = { TEAM_RED, TEAM_BLUE, TEAM_SPECTATORS };
	int Index = 0;
	//for(int Team = 0; Team < 3; ++Team)
	//{
	//	for(int i = 0; i < MAX_CLIENTS && Index < MAX_CLIENTS; ++i)
	//	{
	//		if(m_Snap.m_paPlayerInfos[i] && m_Snap.m_paPlayerInfos[i]->m_Team == Teams[Team])
	//			m_Snap.m_paInfoByTeam[Index++] = m_Snap.m_paPlayerInfos[i];
	//	}
	//}

	// sort player infos by DDRace Team (and score inbetween)
	Index = 0;
	for(int Team = 0; Team <= MAX_CLIENTS; ++Team)
	{
		for(int i = 0; i < MAX_CLIENTS && Index < MAX_CLIENTS; ++i)
		{
			if(m_Snap.m_paInfoByScore[i] && m_Teams.Team(m_Snap.m_paInfoByScore[i]->m_ClientID) == Team)
				m_Snap.m_paInfoByDDTeam[Index++] = m_Snap.m_paInfoByScore[i];
		}
	}

	CTuningParams StandardTuning;
	CServerInfo CurrentServerInfo;
	Client()->GetServerInfo(&CurrentServerInfo);
	if(CurrentServerInfo.m_aGameType[0] != '0')
	{
		if(str_comp(CurrentServerInfo.m_aGameType, "DM") != 0 && str_comp(CurrentServerInfo.m_aGameType, "TDM") != 0 && str_comp(CurrentServerInfo.m_aGameType, "CTF") != 0)
			m_ServerMode = SERVERMODE_MOD;
		else if(mem_comp(&StandardTuning, &m_Tuning[g_Config.m_ClDummy], 33) == 0)
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
	if(AnyRecording && mem_comp(&StandardTuning, &m_Tuning[g_Config.m_ClDummy], sizeof(CTuningParams)) != 0)
	{
		CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
		int *pParams = (int *)&m_Tuning[g_Config.m_ClDummy];
		for(unsigned i = 0; i < sizeof(m_Tuning[0])/sizeof(int); i++)
			Msg.AddInt(pParams[i]);
		Client()->SendMsg(&Msg, MSGFLAG_RECORD|MSGFLAG_NOSEND);
	}

	if(!m_DDRaceMsgSent[0] && m_Snap.m_pLocalInfo)
	{
		CMsgPacker Msg(NETMSGTYPE_CL_ISDDNET);
		Msg.AddInt(CLIENT_VERSIONNR);
		Client()->SendMsgExY(&Msg, MSGFLAG_VITAL,false, 0);
		m_DDRaceMsgSent[0] = true;
	}

	if(!m_DDRaceMsgSent[1] && m_Snap.m_pLocalInfo && Client()->DummyConnected())
	{
		CMsgPacker Msg(NETMSGTYPE_CL_ISDDNET);
		Msg.AddInt(CLIENT_VERSIONNR);
		Client()->SendMsgExY(&Msg, MSGFLAG_VITAL,false, 1);
		m_DDRaceMsgSent[1] = true;
	}

	if(m_ShowOthers[g_Config.m_ClDummy] == -1 || (m_ShowOthers[g_Config.m_ClDummy] != -1 && m_ShowOthers[g_Config.m_ClDummy] != g_Config.m_ClShowOthers))
	{
		// no need to send, default settings
		//if(!(m_ShowOthers == -1 && g_Config.m_ClShowOthers))
		{
			CNetMsg_Cl_ShowOthers Msg;
			Msg.m_Show = g_Config.m_ClShowOthers;
			Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
		}

		// update state
		m_ShowOthers[g_Config.m_ClDummy] = g_Config.m_ClShowOthers;
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
	if(m_Snap.m_pGameInfoObj && m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED)
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

	static bool IsWeaker[2][MAX_CLIENTS] = {{0}};
	if(g_Config.m_ClAntiPingPlayers)
		FindWeaker(IsWeaker);

	// repredict character
	CWorldCore World;
	World.m_Tuning[g_Config.m_ClDummy] = m_Tuning[g_Config.m_ClDummy];

	// search for players
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_Snap.m_aCharacters[i].m_Active || !m_Snap.m_paPlayerInfos[i])
			continue;

		g_GameClient.m_aClients[i].m_Predicted.Init(&World, Collision(), &m_Teams);
		World.m_apCharacters[i] = &g_GameClient.m_aClients[i].m_Predicted;
		World.m_apCharacters[i]->m_Id = m_Snap.m_paPlayerInfos[i]->m_ClientID;
		g_GameClient.m_aClients[i].m_Predicted.Read(&m_Snap.m_aCharacters[i].m_Cur);
		g_GameClient.m_aClients[i].m_Predicted.m_ActiveWeapon = m_Snap.m_aCharacters[i].m_Cur.m_Weapon;
	}

	CServerInfo Info;
	Client()->GetServerInfo(&Info);
	const int MaxProjectiles = 128;
	class CLocalProjectile PredictedProjectiles[MaxProjectiles];
	int NumProjectiles = 0;
	int ReloadTimer = 0;
	vec2 PrevPos;

	if(g_Config.m_ClAntiPingWeapons)
	{
		for(int Index = 0; Index < MaxProjectiles; Index++)
			PredictedProjectiles[Index].Deactivate();

		int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);
		for(int Index = 0; Index < Num && NumProjectiles < MaxProjectiles; Index++)
		{
			IClient::CSnapItem Item;
			const void *pData = Client()->SnapGetItem(IClient::SNAP_CURRENT, Index, &Item);
			if(Item.m_Type == NETOBJTYPE_PROJECTILE)
			{
				CNetObj_Projectile* pProj = (CNetObj_Projectile*) pData;
				if(pProj->m_Type == WEAPON_GRENADE || (pProj->m_Type == WEAPON_SHOTGUN && UseExtraInfo(pProj)))
				{
					CLocalProjectile NewProj;
					NewProj.Init(this, &World, Collision(), pProj);
					if(fabs(1.0f - length(NewProj.m_Direction)) < 0.015)
					{
						if(!NewProj.m_ExtraInfo)
						{
							if(CWeaponData *pData = FindWeaponData(NewProj.m_StartTick))
							{
								NewProj.m_Pos = pData->StartPos();
								NewProj.m_Direction = pData->m_Direction;
								NewProj.m_Owner = m_Snap.m_LocalClientID;
							}
						}
						PredictedProjectiles[NumProjectiles] = NewProj;
						NumProjectiles++;
					}
				}
			}
		}

		int AttackTick = m_Snap.m_aCharacters[m_Snap.m_LocalClientID].m_Cur.m_AttackTick;
		if(World.m_apCharacters[m_Snap.m_LocalClientID]->m_ActiveWeapon == WEAPON_HAMMER)
		{
			CWeaponData *pWeaponData = GetWeaponData(AttackTick);
			if(pWeaponData && pWeaponData->m_Tick == AttackTick)
				ReloadTimer = SERVER_TICK_SPEED / 3 - (Client()->GameTick() - AttackTick);
			else
				ReloadTimer = 0;
		}
		else
			ReloadTimer = g_pData->m_Weapons.m_aId[World.m_apCharacters[m_Snap.m_LocalClientID]->m_ActiveWeapon].m_Firedelay * SERVER_TICK_SPEED / 1000 - (Client()->GameTick() - AttackTick);
		ReloadTimer = max(ReloadTimer, 0);
	}

	// predict
	for(int Tick = Client()->GameTick()+1; Tick <= Client()->PredGameTick(); Tick++)
	{
		// fetch the local
		if(Tick == Client()->PredGameTick() && World.m_apCharacters[m_Snap.m_LocalClientID])
			m_PredictedPrevChar = *World.m_apCharacters[m_Snap.m_LocalClientID];

		for(int c = 0; c < MAX_CLIENTS; c++)
		{
			if(!World.m_apCharacters[c])
				continue;

			if(g_Config.m_ClAntiPingPlayers && Tick == Client()->PredGameTick())
				g_GameClient.m_aClients[c].m_PrevPredicted = *World.m_apCharacters[c];
		}

		// input
		for(int c = 0; c < MAX_CLIENTS; c++)
		{
			if(!World.m_apCharacters[c])
				continue;

			mem_zero(&World.m_apCharacters[c]->m_Input, sizeof(World.m_apCharacters[c]->m_Input));
			if(m_Snap.m_LocalClientID == c)
			{
				// apply player input
				int *pInput = Client()->GetInput(Tick);
				if(pInput)
					World.m_apCharacters[c]->m_Input = *((CNetObj_PlayerInput*)pInput);
			}
		}

		if(g_Config.m_ClAntiPingWeapons)
		{
			const float ProximityRadius = 28.0f;
			CNetObj_PlayerInput Input;
			CNetObj_PlayerInput PrevInput;
			mem_zero(&Input, sizeof(Input));
			mem_zero(&PrevInput, sizeof(PrevInput));
			int *pInput = Client()->GetInput(Tick);
			if(pInput)
				Input = *((CNetObj_PlayerInput*)pInput);
			int *pPrevInput = Client()->GetInput(Tick-1);
			if(pPrevInput)
				PrevInput = *((CNetObj_PlayerInput*)pPrevInput);

			CCharacterCore *Local = World.m_apCharacters[m_Snap.m_LocalClientID];
			vec2 Direction = normalize(vec2(Input.m_TargetX, Input.m_TargetY));
			vec2 Pos = Local->m_Pos;
			vec2 ProjStartPos = Pos + Direction * ProximityRadius * 0.75f;

			bool WeaponFired = false;
			bool NewPresses = false;
			// handle weapons
			do
			{
				if(ReloadTimer)
					break;
				if(!World.m_apCharacters[m_Snap.m_LocalClientID])
					break;
				if(!pInput || !pPrevInput)
					break;

				bool FullAuto = false;
				if(Local->m_ActiveWeapon == WEAPON_GRENADE || Local->m_ActiveWeapon == WEAPON_SHOTGUN || Local->m_ActiveWeapon == WEAPON_RIFLE)
					FullAuto = true;

				bool WillFire = false;

				if(CountInput(PrevInput.m_Fire, Input.m_Fire).m_Presses)
				{
					WillFire = true;
					NewPresses = true;
				}
				if(FullAuto && (Input.m_Fire&1))
					WillFire = true;
				if(!WillFire)
					break;
				if(!IsRace(&Info) && !m_Snap.m_pLocalCharacter->m_AmmoCount && Local->m_ActiveWeapon != WEAPON_HAMMER)
					break;

				int ExpectedStartTick = Tick-1;
				ReloadTimer = g_pData->m_Weapons.m_aId[Local->m_ActiveWeapon].m_Firedelay * SERVER_TICK_SPEED / 1000;

				bool DirectInput = Client()->InputExists(Tick);
				if(!DirectInput)
				{
					ReloadTimer++;
					ExpectedStartTick++;
				}

				switch(Local->m_ActiveWeapon)
				{
					case WEAPON_RIFLE:
					case WEAPON_SHOTGUN:
					case WEAPON_GUN:
						{
							WeaponFired = true;
						} break;
					case WEAPON_GRENADE:
						{
							if(NumProjectiles >= MaxProjectiles)
								break;
							PredictedProjectiles[NumProjectiles].Init(
									this, &World, Collision(),
									Direction, //StartDir
									ProjStartPos, //StartPos
									ExpectedStartTick, //StartTick
									WEAPON_GRENADE, //Type
									m_Snap.m_LocalClientID, //Owner
									WEAPON_GRENADE, //Weapon
									1, 0, 0, 1); //Explosive, Bouncing, Freeze, ExtraInfo
							NumProjectiles++;
							WeaponFired = true;
						} break;
					case WEAPON_HAMMER:
						{
							vec2 ProjPos = ProjStartPos;
							float Radius = ProximityRadius*0.5f;

							int Hits = 0;
							bool OwnerCanProbablyHitOthers = (m_Tuning[g_Config.m_ClDummy].m_PlayerCollision || m_Tuning[g_Config.m_ClDummy].m_PlayerHooking);
							if(!OwnerCanProbablyHitOthers)
								break;
							for(int i = 0; i < MAX_CLIENTS; i++)
							{
								if(!World.m_apCharacters[i])
									continue;
								if(i == m_Snap.m_LocalClientID)
									continue;
								if(!(distance(World.m_apCharacters[i]->m_Pos, ProjPos) < Radius+ProximityRadius))
									continue;;

								CCharacterCore *pTarget = World.m_apCharacters[i];

								if(m_aClients[i].m_Active && !m_Teams.CanCollide(i, m_Snap.m_LocalClientID))
									continue;

								vec2 Dir;
								if (length(pTarget->m_Pos - Pos) > 0.0f)
									Dir = normalize(pTarget->m_Pos - Pos);
								else
									Dir = vec2(0.f, -1.f);

								float Strength;
								Strength = World.m_Tuning[g_Config.m_ClDummy].m_HammerStrength;

								vec2 Temp = pTarget->m_Vel + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f;

								pTarget->LimitForce(&Temp);

								Temp -= pTarget->m_Vel;
								pTarget->ApplyForce((vec2(0.f, -1.0f) + Temp) * Strength);
								Hits++;
							}
							// if we Hit anything, we have to wait for the reload
							if(Hits)
							{
								ReloadTimer = SERVER_TICK_SPEED/3;
								WeaponFired = true;
							}
						} break;
				}
				if(!ReloadTimer)
				{
					ReloadTimer = g_pData->m_Weapons.m_aId[Local->m_ActiveWeapon].m_Firedelay * SERVER_TICK_SPEED / 1000;
					if(!DirectInput)
						ReloadTimer++;
				}
			} while(false);

			if(ReloadTimer)
				ReloadTimer--;

			if(Tick > Client()->GameTick()+1)
				if(CWeaponData *pWeaponData = GetWeaponData(Tick-1))
					pWeaponData->m_Pos = Pos;
			if(WeaponFired)
			{
				if(CWeaponData *pWeaponData = GetWeaponData(Tick-1))
				{
					pWeaponData->m_Direction = Direction;
					pWeaponData->m_Tick = Tick-1;
				}
				if(NewPresses)
				{
					if(CWeaponData *pWeaponData = GetWeaponData(Tick-2))
					{
						pWeaponData->m_Direction = Direction;
						pWeaponData->m_Tick = Tick-2;
					}
					if(CWeaponData *pWeaponData = GetWeaponData(Tick))
					{
						pWeaponData->m_Direction = Direction;
						pWeaponData->m_Tick = Tick;
					}
				}
			}

			// projectiles
			for(int g = 0; g < MaxProjectiles; g++)
				if(PredictedProjectiles[g].m_Active)
					PredictedProjectiles[g].Tick(Tick, Client()->GameTickSpeed(), m_Snap.m_LocalClientID);
		}

		// calculate where everyone should move
		if(g_Config.m_ClAntiPingPlayers)
		{
			//first apply Tick to weaker players (players that the local client has strong hook against), then local, then stronger players
			for(int h = 0; h < 3; h++)
			{
				if(h == 1)
				{
					if(World.m_apCharacters[m_Snap.m_LocalClientID])
						World.m_apCharacters[m_Snap.m_LocalClientID]->Tick(true, true);
				}
				else
					for(int c = 0; c < MAX_CLIENTS; c++)
						if(c != m_Snap.m_LocalClientID && World.m_apCharacters[c] && ((h == 0 && IsWeaker[g_Config.m_ClDummy][c]) || (h == 2 && !IsWeaker[g_Config.m_ClDummy][c])))
							World.m_apCharacters[c]->Tick(false, true);
			}
		}
		else
		{
			for(int c = 0; c < MAX_CLIENTS; c++)
			{
				if(!World.m_apCharacters[c])
					continue;
				if(m_Snap.m_LocalClientID == c)
					World.m_apCharacters[c]->Tick(true, true);
				else
					World.m_apCharacters[c]->Tick(false, true);
			}
		}

		// move all players and quantize their data
		if(g_Config.m_ClAntiPingPlayers)
		{
			// Everyone with weaker hook
			for(int c = 0; c < MAX_CLIENTS; c++)
			{
				if(c != m_Snap.m_LocalClientID && World.m_apCharacters[c] && IsWeaker[g_Config.m_ClDummy][c])
				{
					World.m_apCharacters[c]->Move();
					World.m_apCharacters[c]->Quantize();
				}
			}

			// Us
			if(World.m_apCharacters[m_Snap.m_LocalClientID])
			{
				World.m_apCharacters[m_Snap.m_LocalClientID]->Move();
				World.m_apCharacters[m_Snap.m_LocalClientID]->Quantize();
			}

			// Everyone with stronger hook
			for(int c = 0; c < MAX_CLIENTS; c++)
			{
				if(c != m_Snap.m_LocalClientID && World.m_apCharacters[c] && !IsWeaker[g_Config.m_ClDummy][c])
				{
					World.m_apCharacters[c]->Move();
					World.m_apCharacters[c]->Quantize();
				}
			}
		}
		else
		{
			for(int c = 0; c < MAX_CLIENTS; c++)
			{
				if(!World.m_apCharacters[c])
					continue;
				World.m_apCharacters[c]->Move();
				World.m_apCharacters[c]->Quantize();
			}
		}

		// check if we want to trigger effects
		if(Tick > m_LastNewPredictedTick[g_Config.m_ClDummy])
		{
			m_LastNewPredictedTick[g_Config.m_ClDummy] = Tick;
			m_NewPredictedTick = true;

			if(m_Snap.m_LocalClientID != -1 && World.m_apCharacters[m_Snap.m_LocalClientID])
			{
				vec2 Pos = World.m_apCharacters[m_Snap.m_LocalClientID]->m_Pos;
				int Events = World.m_apCharacters[m_Snap.m_LocalClientID]->m_TriggeredEvents;
				if(Events&COREEVENT_GROUND_JUMP)
					if(g_Config.m_SndGame)
						g_GameClient.m_pSounds->PlayAndRecord(CSounds::CHN_WORLD, SOUND_PLAYER_JUMP, 1.0f, Pos);

				/*if(events&COREEVENT_AIR_JUMP)
				{
					GameClient.effects->air_jump(pos);
					GameClient.sounds->play_and_record(SOUNDS::CHN_WORLD, SOUND_PLAYER_AIRJUMP, 1.0f, pos);
				}*/

				//if(events&COREEVENT_HOOK_LAUNCH) snd_play_random(CHN_WORLD, SOUND_HOOK_LOOP, 1.0f, pos);
				//if(events&COREEVENT_HOOK_ATTACH_PLAYER) snd_play_random(CHN_WORLD, SOUND_HOOK_ATTACH_PLAYER, 1.0f, pos);
				if(Events&COREEVENT_HOOK_ATTACH_GROUND)
					if(g_Config.m_SndGame)
						g_GameClient.m_pSounds->PlayAndRecord(CSounds::CHN_WORLD, SOUND_HOOK_ATTACH_GROUND, 1.0f, Pos);
				if(Events&COREEVENT_HOOK_HIT_NOHOOK)
					if(g_Config.m_SndGame)
						g_GameClient.m_pSounds->PlayAndRecord(CSounds::CHN_WORLD, SOUND_HOOK_NOATTACH, 1.0f, Pos);
				//if(events&COREEVENT_HOOK_RETRACT) snd_play_random(CHN_WORLD, SOUND_PLAYER_JUMP, 1.0f, pos);
			}
		}


		if(Tick == Client()->PredGameTick() && World.m_apCharacters[m_Snap.m_LocalClientID])
		{
			m_PredictedChar = *World.m_apCharacters[m_Snap.m_LocalClientID];

			if (g_Config.m_ClAntiPingPlayers)
			{
				for (int c = 0; c < MAX_CLIENTS; c++)
				{
					if(!World.m_apCharacters[c])
						continue;

					g_GameClient.m_aClients[c].m_Predicted = *World.m_apCharacters[c];
				}
			}
		}
	}

	if(g_Config.m_Debug && g_Config.m_ClPredict && m_PredictedTick == Client()->PredGameTick())
	{
		CNetObj_CharacterCore Before = {0}, Now = {0}, BeforePrev = {0}, NowPrev = {0};
		BeforeChar.Write(&Before);
		BeforePrevChar.Write(&BeforePrev);
		m_PredictedChar.Write(&Now);
		m_PredictedPrevChar.Write(&NowPrev);

		if(mem_comp(&Before, &Now, sizeof(CNetObj_CharacterCore)) != 0)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", "prediction error");
			for(unsigned i = 0; i < sizeof(CNetObj_CharacterCore)/sizeof(int); i++)
				if(((int *)&Before)[i] != ((int *)&Now)[i])
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "	%d %d %d (%d %d)", i, ((int *)&Before)[i], ((int *)&Now)[i], ((int *)&BeforePrev)[i], ((int *)&NowPrev)[i]);
					Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
				}
		}
	}

	m_PredictedTick = Client()->PredGameTick();
}

void CGameClient::OnActivateEditor()
{
	OnRelease();
}

CGameClient::CClientStats::CClientStats()
{
	m_JoinDate = 0;
	m_Active = false;
	m_WasActive = false;
	m_Frags = 0;
	m_Deaths = 0;
	m_Suicides = 0;
	for(int j = 0; j < NUM_WEAPONS; j++)
	{
		m_aFragsWith[j] = 0;
		m_aDeathsFrom[j] = 0;
	}
	m_FlagGrabs = 0;
	m_FlagCaptures = 0;
	m_CarriersKilled = 0;
	m_KillsCarrying = 0;
	m_DeathsCarrying = 0;
}

void CGameClient::CClientStats::Reset()
{
	m_JoinDate = 0;
	m_Active = false;
	m_WasActive = false;
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
	m_CarriersKilled = 0;
	m_KillsCarrying = 0;
	m_DeathsCarrying = 0;
}

void CGameClient::CClientData::UpdateRenderInfo()
{
	m_RenderInfo = m_SkinInfo;

	// force team colors
	if(g_GameClient.m_Snap.m_pGameInfoObj && g_GameClient.m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS)
	{
		m_RenderInfo.m_Texture = g_GameClient.m_pSkins->Get(m_SkinID)->m_ColorTexture;
		const int TeamColors[2] = {65387, 10223467};
		if(m_Team >= TEAM_RED && m_Team <= TEAM_BLUE)
		{
			m_RenderInfo.m_ColorBody = g_GameClient.m_pSkins->GetColorV4(TeamColors[m_Team]);
			m_RenderInfo.m_ColorFeet = g_GameClient.m_pSkins->GetColorV4(TeamColors[m_Team]);
		}
		else
		{
			m_RenderInfo.m_ColorBody = g_GameClient.m_pSkins->GetColorV4(12895054);
			m_RenderInfo.m_ColorFeet = g_GameClient.m_pSkins->GetColorV4(12895054);
		}
	}
}

void CGameClient::CClientData::Reset()
{
	m_aName[0] = 0;
	m_aClan[0] = 0;
	m_Country = -1;
	m_SkinID = 0;
	m_Team = 0;
	m_Angle = 0;
	m_Emoticon = 0;
	m_EmoticonStart = -1;
	m_Active = false;
	m_ChatIgnore = false;
	m_SkinInfo.m_Texture = g_GameClient.m_pSkins->Get(0)->m_ColorTexture;
	m_SkinInfo.m_ColorBody = vec4(1,1,1,1);
	m_SkinInfo.m_ColorFeet = vec4(1,1,1,1);
	UpdateRenderInfo();
}

void CGameClient::SendSwitchTeam(int Team)
{
	CNetMsg_Cl_SetTeam Msg;
	Msg.m_Team = Team;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);

	if (Team != TEAM_SPECTATORS)
		m_pCamera->OnReset();
}

void CGameClient::SendInfo(bool Start)
{
	if(Start)
	{
		CNetMsg_Cl_StartInfo Msg;
		Msg.m_pName = g_Config.m_PlayerName;
		Msg.m_pClan = g_Config.m_PlayerClan;
		Msg.m_Country = g_Config.m_PlayerCountry;
		Msg.m_pSkin = g_Config.m_PlayerSkin;
		Msg.m_UseCustomColor = g_Config.m_PlayerUseCustomColor;
		Msg.m_ColorBody = g_Config.m_PlayerColorBody;
		Msg.m_ColorFeet = g_Config.m_PlayerColorFeet;
		CMsgPacker Packer(Msg.MsgID());
		Msg.Pack(&Packer);
		Client()->SendMsgExY(&Packer, MSGFLAG_VITAL, false, 0);
		m_CheckInfo[0] = -1;
	}
	else
	{
		CNetMsg_Cl_ChangeInfo Msg;
		Msg.m_pName = g_Config.m_PlayerName;
		Msg.m_pClan = g_Config.m_PlayerClan;
		Msg.m_Country = g_Config.m_PlayerCountry;
		Msg.m_pSkin = g_Config.m_PlayerSkin;
		Msg.m_UseCustomColor = g_Config.m_PlayerUseCustomColor;
		Msg.m_ColorBody = g_Config.m_PlayerColorBody;
		Msg.m_ColorFeet = g_Config.m_PlayerColorFeet;
		CMsgPacker Packer(Msg.MsgID());
		Msg.Pack(&Packer);
		Client()->SendMsgExY(&Packer, MSGFLAG_VITAL, false, 0);
		m_CheckInfo[0] = Client()->GameTickSpeed();
	}
}

void CGameClient::SendDummyInfo(bool Start)
{
	if(Start)
	{
		CNetMsg_Cl_StartInfo Msg;
		Msg.m_pName = g_Config.m_DummyName;
		Msg.m_pClan = g_Config.m_DummyClan;
		Msg.m_Country = g_Config.m_DummyCountry;
		Msg.m_pSkin = g_Config.m_DummySkin;
		Msg.m_UseCustomColor = g_Config.m_DummyUseCustomColor;
		Msg.m_ColorBody = g_Config.m_DummyColorBody;
		Msg.m_ColorFeet = g_Config.m_DummyColorFeet;
		CMsgPacker Packer(Msg.MsgID());
		Msg.Pack(&Packer);
		Client()->SendMsgExY(&Packer, MSGFLAG_VITAL, false, 1);
		m_CheckInfo[1] = -1;
	}
	else
	{
		CNetMsg_Cl_ChangeInfo Msg;
		Msg.m_pName = g_Config.m_DummyName;
		Msg.m_pClan = g_Config.m_DummyClan;
		Msg.m_Country = g_Config.m_DummyCountry;
		Msg.m_pSkin = g_Config.m_DummySkin;
		Msg.m_UseCustomColor = g_Config.m_DummyUseCustomColor;
		Msg.m_ColorBody = g_Config.m_DummyColorBody;
		Msg.m_ColorFeet = g_Config.m_DummyColorFeet;
		CMsgPacker Packer(Msg.MsgID());
		Msg.Pack(&Packer);
		Client()->SendMsgExY(&Packer, MSGFLAG_VITAL,false, 1);
		m_CheckInfo[1] = Client()->GameTickSpeed();
	}
}

void CGameClient::SendKill(int ClientID)
{
	CNetMsg_Cl_Kill Msg;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);

	if(g_Config.m_ClDummyCopyMoves)
	{
		CMsgPacker Msg(NETMSGTYPE_CL_KILL);
		Client()->SendMsgExY(&Msg, MSGFLAG_VITAL, false, !g_Config.m_ClDummy);
	}
}

void CGameClient::ConTeam(IConsole::IResult *pResult, void *pUserData)
{
	((CGameClient*)pUserData)->SendSwitchTeam(pResult->GetInteger(0));
}

void CGameClient::ConKill(IConsole::IResult *pResult, void *pUserData)
{
	((CGameClient*)pUserData)->SendKill(-1);
}

void CGameClient::ConchainSpecialInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		((CGameClient*)pUserData)->SendInfo(false);
}

void CGameClient::ConchainSpecialDummyInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		((CGameClient*)pUserData)->SendDummyInfo(false);
}

void CGameClient::ConchainSpecialDummy(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		if(g_Config.m_ClDummy && !((CGameClient*)pUserData)->Client()->DummyConnected())
			g_Config.m_ClDummy = 0;
}

IGameClient *CreateGameClient()
{
	return &g_GameClient;
}

int CGameClient::IntersectCharacter(vec2 HookPos, vec2 NewPos, vec2& NewPos2, int ownID)
{
	float PhysSize = 28.0f;
	float Distance = 0.0f;
	int ClosestID = -1;

	if (!m_Tuning[g_Config.m_ClDummy].m_PlayerHooking)
		return ClosestID;

	for (int i=0; i<MAX_CLIENTS; i++)
	{
		CClientData cData = m_aClients[i];
		CNetObj_Character Prev = m_Snap.m_aCharacters[i].m_Prev;
		CNetObj_Character Player = m_Snap.m_aCharacters[i].m_Cur;

		vec2 Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), Client()->IntraGameTick());

		if (!cData.m_Active || i == ownID || !m_Teams.SameTeam(i, ownID))
			continue;

		vec2 ClosestPoint = closest_point_on_line(HookPos, NewPos, Position);
		if(distance(Position, ClosestPoint) < PhysSize+2.0f)
		{
			if(ClosestID == -1 || distance(HookPos, Position) < Distance)
			{
				NewPos2 = ClosestPoint;
				ClosestID = i;
				Distance = distance(HookPos, Position);
			}
		}
	}

	return ClosestID;
}


int CGameClient::IntersectCharacter(vec2 OldPos, vec2 NewPos, float Radius, vec2 *NewPos2, int ownID, CWorldCore *World)
{
	float PhysSize = 28.0f;
	float Distance = 0.0f;
	int ClosestID = -1;

	if(!World)
		return ClosestID;

	for (int i=0; i<MAX_CLIENTS; i++)
	{
		if(!World->m_apCharacters[i])
			continue;
		CClientData cData = m_aClients[i];

		if(!cData.m_Active || i == ownID || !m_Teams.CanCollide(i, ownID))
			continue;
		vec2 Position = World->m_apCharacters[i]->m_Pos;
		vec2 ClosestPoint = closest_point_on_line(OldPos, NewPos, Position);
		if(distance(Position, ClosestPoint) < PhysSize+Radius)
		{
			if(ClosestID == -1 || distance(OldPos, Position) < Distance)
			{
				*NewPos2 = ClosestPoint;
				ClosestID = i;
				Distance = distance(OldPos, Position);
			}
		}
	}
	return ClosestID;
}

void CLocalProjectile::Init(CGameClient *pGameClient, CWorldCore *pWorld, CCollision *pCollision, const CNetObj_Projectile *pProj)
{
	m_Active = 1;
	m_pGameClient = pGameClient;
	m_pWorld = pWorld;
	m_pCollision = pCollision;
	m_StartTick = pProj->m_StartTick;
	m_Type = pProj->m_Type;
	m_Weapon = m_Type;

	ExtractInfo(pProj, &m_Pos, &m_Direction, 1);

	if(UseExtraInfo(pProj))
	{
		ExtractExtraInfo(pProj, &m_Owner, &m_Explosive, &m_Bouncing, &m_Freeze);
		m_ExtraInfo = true;
	}
	else
	{
		bool StandardVel = (fabs(1.0f - length(m_Direction)) < 0.015);
		m_Owner = -1;
		m_Explosive = ((m_Type == WEAPON_GRENADE && StandardVel) ? true : false);
		m_Bouncing = 0;
		m_Freeze = 0;
		m_ExtraInfo = false;
	}
}

void CLocalProjectile::Init(CGameClient *pGameClient, CWorldCore *pWorld, CCollision *pCollision, vec2 Direction, vec2 Pos, int StartTick, int Type, int Owner, int Weapon, bool Explosive, int Bouncing, bool Freeze, bool ExtraInfo)
{
	m_Active = 1;
	m_pGameClient = pGameClient;
	m_pWorld = pWorld;
	m_pCollision = pCollision;
	m_Direction = Direction;
	m_Pos = Pos;
	m_StartTick = StartTick;
	m_Type = Type;
	m_Weapon = Weapon;
	m_Owner = Owner;
	m_Explosive = Explosive;
	m_Bouncing = Bouncing;
	m_Freeze = Freeze;
	m_ExtraInfo = ExtraInfo;
}

vec2 CLocalProjectile::GetPos(float Time)
{
	float Curvature = 0;
	float Speed = 0;

	switch(m_Type)
	{
		case WEAPON_GRENADE:
			Curvature = m_pWorld->m_Tuning[g_Config.m_ClDummy].m_GrenadeCurvature;
			Speed = m_pWorld->m_Tuning[g_Config.m_ClDummy].m_GrenadeSpeed;
			break;

		case WEAPON_SHOTGUN:
			Curvature = m_pWorld->m_Tuning[g_Config.m_ClDummy].m_ShotgunCurvature;
			Speed = m_pWorld->m_Tuning[g_Config.m_ClDummy].m_ShotgunSpeed;
			break;

		case WEAPON_GUN:
			Curvature = m_pWorld->m_Tuning[g_Config.m_ClDummy].m_GunCurvature;
			Speed = m_pWorld->m_Tuning[g_Config.m_ClDummy].m_GunSpeed;
			break;
	}
	return CalcPos(m_Pos, m_Direction, Curvature, Speed, Time);
}

bool CLocalProjectile::GameLayerClipped(vec2 CheckPos)
{
	return round_to_int(CheckPos.x)/32 < -200 || round_to_int(CheckPos.x)/32 > m_pCollision->GetWidth()+200 ||
		round_to_int(CheckPos.y)/32 < -200 || round_to_int(CheckPos.y)/32 > m_pCollision->GetHeight()+200 ? true : false;
}

void CLocalProjectile::Tick(int CurrentTick, int GameTickSpeed, int LocalClientID)
{
	if(!m_pWorld)
		return;
	if(CurrentTick <= m_StartTick)
		return;
	float Pt = (CurrentTick-m_StartTick-1)/(float)GameTickSpeed;
	float Ct = (CurrentTick-m_StartTick)/(float)GameTickSpeed;

	vec2 PrevPos = GetPos(Pt);
	vec2 CurPos = GetPos(Ct);
	vec2 ColPos;
	vec2 NewPos;
	int Collide = 0;
	if(m_pCollision)
		Collide = m_pCollision->IntersectLine(PrevPos, CurPos, &ColPos, &NewPos, false);
	int Target = m_pGameClient->IntersectCharacter(PrevPos, ColPos, m_Freeze ? 1.0f : 6.0f, &ColPos, m_Owner, m_pWorld);

	bool isWeaponCollide = false;
	if(m_Owner >= 0 && Target >= 0 && m_pGameClient->m_aClients[m_Owner].m_Active && m_pGameClient->m_aClients[Target].m_Active && !m_pGameClient->m_Teams.CanCollide(m_Owner, Target))
		isWeaponCollide = true;

	bool OwnerCanProbablyHitOthers = (m_pWorld->m_Tuning[g_Config.m_ClDummy].m_PlayerCollision || m_pWorld->m_Tuning[g_Config.m_ClDummy].m_PlayerHooking);

	if(((Target >= 0 && (m_Owner >= 0 ? OwnerCanProbablyHitOthers : 1 || Target == m_Owner)) || Collide || GameLayerClipped(CurPos)) && !isWeaponCollide)
	{
		if(m_Explosive && (Target < 0 || (Target >= 0 && (!m_Freeze || (m_Weapon == WEAPON_SHOTGUN && Collide)))))
			CreateExplosion(ColPos, m_Owner);
		if(Collide && m_Bouncing != 0)
		{
			m_StartTick = CurrentTick;
			m_Pos = NewPos+(-(m_Direction*4));
			if (m_Bouncing == 1)
				m_Direction.x = -m_Direction.x;
			else if(m_Bouncing == 2)
				m_Direction.y = -m_Direction.y;
			if (fabs(m_Direction.x) < 1e-6)
				m_Direction.x = 0;
			if (fabs(m_Direction.y) < 1e-6)
				m_Direction.y = 0;
			m_Pos += m_Direction;
		}
		else if(!m_Bouncing)
			Deactivate();
	}

	if(!m_Bouncing)
	{
		int Lifetime = 0;
		if(m_Weapon == WEAPON_GRENADE)
			Lifetime = m_pGameClient->m_Tuning[g_Config.m_ClDummy].m_GrenadeLifetime * SERVER_TICK_SPEED;
		else if(m_Weapon == WEAPON_GUN)
			Lifetime = m_pGameClient->m_Tuning[g_Config.m_ClDummy].m_GrenadeLifetime * SERVER_TICK_SPEED;
		else if(m_Weapon == WEAPON_SHOTGUN)
			Lifetime = m_pGameClient->m_Tuning[g_Config.m_ClDummy].m_ShotgunLifetime * SERVER_TICK_SPEED;
		int LifeSpan = Lifetime - (CurrentTick - m_StartTick);
		if(LifeSpan == -1)
		{
			if(m_Explosive)
				CreateExplosion(ColPos, LocalClientID);
			Deactivate();
		}
	}
}

void CLocalProjectile::CreateExplosion(vec2 Pos, int LocalClientID)
{
	if(!m_pWorld)
		return;
	float Radius = 135.0f;
	float InnerRadius = 48.0f;

	bool OwnerCanProbablyHitOthers = (m_pWorld->m_Tuning[g_Config.m_ClDummy].m_PlayerCollision || m_pWorld->m_Tuning[g_Config.m_ClDummy].m_PlayerHooking);

	for(int c = 0; c < MAX_CLIENTS; c++)
	{
		if(!m_pWorld->m_apCharacters[c])
			continue;
		if(m_Owner >= 0 && c >= 0)
			if(m_pGameClient->m_aClients[c].m_Active && !m_pGameClient->m_Teams.CanCollide(c, m_Owner))
				continue;
		if(c != LocalClientID && !OwnerCanProbablyHitOthers)
			continue;
		vec2 Diff = m_pWorld->m_apCharacters[c]->m_Pos - Pos;
		vec2 ForceDir(0,1);
		float l = length(Diff);
		if(l)
			ForceDir = normalize(Diff);
		l = 1-clamp((l-InnerRadius)/(Radius-InnerRadius), 0.0f, 1.0f);

		float Strength = m_pWorld->m_Tuning[g_Config.m_ClDummy].m_ExplosionStrength;
		float Dmg = Strength * l;

		if((int)Dmg)
			m_pWorld->m_apCharacters[c]->ApplyForce(ForceDir*Dmg*2);
	}
}

CWeaponData *CGameClient::FindWeaponData(int TargetTick)
{
	CWeaponData *pData;
	int TickDiff[3] = {0, -1, 1};
	for(unsigned int i = 0; i < sizeof(TickDiff)/sizeof(int); i++)
		if((pData = GetWeaponData(TargetTick + TickDiff[i])))
			if(pData->m_Tick == TargetTick + TickDiff[i])
				return GetWeaponData(TargetTick + TickDiff[i]);
	return NULL;
}

void CGameClient::FindWeaker(bool IsWeaker[2][MAX_CLIENTS])
{
	// attempts to detect strong/weak against the player we are hooking
	static int DirAccumulated[2][MAX_CLIENTS] = {{0}};
	if(!m_Snap.m_aCharacters[m_Snap.m_LocalClientID].m_Active || !m_Snap.m_paPlayerInfos[m_Snap.m_LocalClientID])
		return;
	int HookedPlayer = m_Snap.m_aCharacters[m_Snap.m_LocalClientID].m_Prev.m_HookedPlayer;
	if(HookedPlayer >= 0 && m_Snap.m_aCharacters[HookedPlayer].m_Active && m_Snap.m_paPlayerInfos[HookedPlayer])
	{
		CCharacterCore OtherCharCur;
		OtherCharCur.Read(&m_Snap.m_aCharacters[HookedPlayer].m_Cur);
		float PredictErr[2];
		for(int dir = 0; dir < 2; dir++)
		{
			CWorldCore World;
			World.m_Tuning[g_Config.m_ClDummy] = m_Tuning[g_Config.m_ClDummy];

			CCharacterCore OtherChar;
			OtherChar.Init(&World, Collision(), &m_Teams);
			World.m_apCharacters[HookedPlayer] = &OtherChar;
			OtherChar.Read(&m_Snap.m_aCharacters[HookedPlayer].m_Prev);

			CCharacterCore LocalChar;
			LocalChar.Init(&World, Collision(), &m_Teams);
			World.m_apCharacters[m_Snap.m_LocalClientID] = &LocalChar;
			LocalChar.Read(&m_Snap.m_aCharacters[m_Snap.m_LocalClientID].m_Prev);

			for(int Tick = Client()->PrevGameTick(); Tick < Client()->GameTick(); Tick++)
			{
				if(dir == 0)
				{
					LocalChar.Tick(false, true);
					OtherChar.Tick(false, true);
				}
				else
				{
					OtherChar.Tick(false, true);
					LocalChar.Tick(false, true);
				}
				LocalChar.Move();
				LocalChar.Quantize();
				OtherChar.Move();
				OtherChar.Quantize();
			}
			PredictErr[dir] = distance(OtherChar.m_Vel, OtherCharCur.m_Vel);
		}
		const float Low = 0.0001, High = 0.07;
		if(PredictErr[1] < Low && PredictErr[0] > High)
			DirAccumulated[g_Config.m_ClDummy][HookedPlayer] = SaturatedAdd(-1, 2, DirAccumulated[g_Config.m_ClDummy][HookedPlayer], 1);
		else if(PredictErr[0] < Low && PredictErr[1] > High)
			DirAccumulated[g_Config.m_ClDummy][HookedPlayer] = SaturatedAdd(-1, 2, DirAccumulated[g_Config.m_ClDummy][HookedPlayer], -1);
		IsWeaker[g_Config.m_ClDummy][HookedPlayer] = (DirAccumulated[g_Config.m_ClDummy][HookedPlayer] > 0);
	}
}
