// ChillerDragon 2020 - chillerbot ux

#include "engine/shared/protocol.h"
#include <engine/client/notifications.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/textrender.h>
#include <game/client/animstate.h>
#include <game/client/components/camera.h>
#include <game/client/components/chat.h>
#include <game/client/components/chillerbot/chathelper.h>
#include <game/client/components/chillerbot/version.h>
#include <game/client/components/controls.h>
#include <game/client/components/menus.h>
#include <game/client/components/voting.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/race.h>
#include <game/client/render.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>
#include <game/version.h>

#include "chillerbotux.h"

void CChillerBotUX::OnRender()
{
	if(time_get() % 10 == 0)
	{
		if(g_Config.m_ClSendOnlineTime)
		{
			if(m_NextHeartbeat < time_get())
			{
				m_NextHeartbeat = time_get() + time_freq() * 60;
				m_HeartbeatState = STATE_WANTREFRESH;
			}
			if(m_HeartbeatState == STATE_WANTREFRESH)
			{
				char aUrl[1024];
				char aEscaped[128];
				str_copy(aUrl, "https://chillerbot.zillyhuhn.com/api/v1/beat/", sizeof(aUrl));
				EscapeUrl(aEscaped, sizeof(aEscaped), g_Config.m_ClChillerbotId);
				str_append(aUrl, aEscaped, sizeof(aUrl));
				str_append(aUrl, "/ux-" CHILLERBOT_VERSION "/", sizeof(aUrl));
				if(g_Config.m_PlayerName[0])
					EscapeUrl(aEscaped, sizeof(aEscaped), g_Config.m_PlayerName);
				else
					str_copy(aEscaped, "nameless tee", sizeof(aEscaped));
				str_append(aUrl, aEscaped, sizeof(aUrl));

				m_pAliveGet = HttpGet(aUrl);
				// 10 seconds connection timeout, lower than 8KB/s for 10 seconds to fail.
				m_pAliveGet->Timeout(CTimeout{10000, 8000, 10});
				m_pClient->Engine()->AddJob(m_pAliveGet);
				m_HeartbeatState = STATE_REFRESHING;
			}
			else if(m_HeartbeatState == STATE_REFRESHING)
			{
				if(m_pAliveGet->State() == HTTP_QUEUED || m_pAliveGet->State() == HTTP_RUNNING)
				{
					return;
				}
				m_HeartbeatState = STATE_DONE;
				std::shared_ptr<CHttpRequest> pGetServers = nullptr;
				std::swap(m_pAliveGet, pGetServers);

				bool Success = true;
				// json_value *pJson = pGetServers->ResultJson();
				// Success = Success && pJson;
				// Success = Success && Parse(pJson);
				// json_value_free(pJson);
				if(!Success)
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", "failed to hearthbeat");
			}
		}
		CheckEmptyTick();
		// if tabbing into tw and going afk set to inactive again over time
		if(m_AfkActivity && time_get() % 100 == 0)
			m_AfkActivity--;
	}
	if(m_AfkTill)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "(%d/%d)", m_AfkActivity, 200);
		SetComponentNoteShort("afk", aBuf);
	}
	RenderSpeedHud();
	RenderEnabledComponents();
	RenderWeaponHud();
	FinishRenameTick();
	ChangeTileNotifyTick();
	TraceSpikes();
	m_ForceDir = 0;
	CampHackTick();
	if(!m_ForceDir && m_LastForceDir)
	{
		m_pClient->m_Controls.m_InputDirectionRight[g_Config.m_ClDummy] = 0;
		m_pClient->m_Controls.m_InputDirectionLeft[g_Config.m_ClDummy] = 0;
		if(m_pClient->m_VibeBot.IsVibing(g_Config.m_ClDummy))
		{
			m_pClient->m_Controls.m_InputData[g_Config.m_ClDummy].m_Direction = 0;
			m_pClient->m_VibeBot.m_InputData[g_Config.m_ClDummy].m_Direction = 0;
		}
	}
	m_LastForceDir = m_ForceDir;
}

void CChillerBotUX::CheckEmptyTick()
{
	if(!g_Config.m_ClReconnectWhenEmpty)
		return;

	static int s_LastPlayerCount = 0;
	int PlayerCount = CountOnlinePlayers();
	if(s_LastPlayerCount > PlayerCount && PlayerCount == 1)
		m_pClient->Client()->Connect(m_pClient->Client()->ConnectAddressString());
	else
		s_LastPlayerCount = CountOnlinePlayers();
}

void CChillerBotUX::ChangeTileNotifyTick()
{
	if(!g_Config.m_ClChangeTileNotification)
		return;
	static int LastTile = -1;
	float X = m_pClient->m_Snap.m_aCharacters[m_pClient->m_LocalIDs[g_Config.m_ClDummy]].m_Cur.m_X;
	float Y = m_pClient->m_Snap.m_aCharacters[m_pClient->m_LocalIDs[g_Config.m_ClDummy]].m_Cur.m_Y;
	int CurrentTile = Collision()->GetTileIndex(Collision()->GetPureMapIndex(X, Y));
	if(LastTile != CurrentTile && m_LastNotification + time_freq() * 10 < time_get())
	{
		IEngineGraphics *pGraphics = ((IEngineGraphics *)Kernel()->RequestInterface<IEngineGraphics>());
		if(pGraphics && !pGraphics->WindowActive() && Graphics())
		{
			NotificationsNotify("chillerbot-ux", "current tile changed");
			Graphics()->NotifyWindow();
		}
		m_LastNotification = time_get();
	}
	LastTile = CurrentTile;
}

void CChillerBotUX::RenderWeaponHud()
{
	if(!g_Config.m_ClWeaponHud)
		return;
	if(CCharacter *pChar = m_pClient->m_GameWorld.GetCharacterByID(m_pClient->m_LocalIDs[g_Config.m_ClDummy]))
	{
		char aWeapons[1024];
		aWeapons[0] = '\0';
		if(pChar->GetWeaponGot(WEAPON_HAMMER))
			str_append(aWeapons, "hammer", sizeof(aWeapons));
		if(pChar->GetWeaponGot(WEAPON_GUN))
			str_append(aWeapons, aWeapons[0] ? ", gun" : "gun", sizeof(aWeapons));
		if(pChar->GetWeaponGot(WEAPON_SHOTGUN))
			str_append(aWeapons, aWeapons[0] ? ", shotgun" : "shotgun", sizeof(aWeapons));
		if(pChar->GetWeaponGot(WEAPON_GRENADE))
			str_append(aWeapons, aWeapons[0] ? ", grenade" : "grenade", sizeof(aWeapons));
		if(pChar->GetWeaponGot(WEAPON_LASER))
			str_append(aWeapons, aWeapons[0] ? ", rifle" : "rifle", sizeof(aWeapons));

		SetComponentNoteLong("weapon hud", aWeapons);
	}
}

void CChillerBotUX::RenderSpeedHud()
{
	if(!g_Config.m_ClShowSpeed || !m_pClient->m_Snap.m_pLocalCharacter || !m_pClient->m_Snap.m_pLocalPrevCharacter)
		return;

	float Width = 300 * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0, 0, Width, 300);
	// float Velspeed = length(vec2(m_pClient->m_Snap.m_pLocalCharacter->m_VelX / 256.0f, m_pClient->m_Snap.m_pLocalCharacter->m_VelY / 256.0f)) * 50;

	const char *paStrings[] = {"Vel X:", "Vel Y:"};
	const int Num = sizeof(paStrings) / sizeof(char *);
	const float LineHeight = 12.0f;
	const float Fontsize = 15.0f;

	static int LastVelX = 0;
	static int LastVelXChange = 0;
	int CurVelX = abs(m_pClient->m_Snap.m_pLocalCharacter->m_VelX);
	if(LastVelX < CurVelX)
		LastVelXChange = 1;
	else if(LastVelX > CurVelX)
		LastVelXChange = -1;
	LastVelX = CurVelX;

	static int LastVelY = 0;
	static int LastVelYChange = 0;
	int CurVelY = abs(m_pClient->m_Snap.m_pLocalCharacter->m_VelY);
	if(LastVelY < CurVelY)
		LastVelYChange = 1;
	else if(LastVelY > CurVelY)
		LastVelYChange = -1;
	LastVelY = CurVelY;

	float x = Width - 100.0f, y = 50.0f;
	for(int i = 0; i < Num; ++i)
		TextRender()->Text(0, x, y + i * LineHeight, Fontsize, paStrings[i], -1.0f);

	x = Width - 10.0f;
	char aBuf[128];

	if(LastVelXChange == 1)
		TextRender()->TextColor(0.0f, 1.0f, 0.0f, 1.0f);
	else if(LastVelXChange == -1)
		TextRender()->TextColor(1.0f, 0.0f, 0.0f, 1.0f);

	str_format(aBuf, sizeof(aBuf), "%.0f", m_pClient->m_Snap.m_pLocalCharacter->m_VelX / 32.f);
	float w = TextRender()->TextWidth(0, Fontsize, aBuf, -1, -1.0f);
	TextRender()->Text(0, x - w, y, Fontsize, aBuf, -1.0f);
	y += LineHeight;

	if(LastVelYChange == 1)
		TextRender()->TextColor(0.0f, 1.0f, 0.0f, 1.0f);
	else if(LastVelYChange == -1)
		TextRender()->TextColor(1.0f, 0.0f, 0.0f, 1.0f);

	str_format(aBuf, sizeof(aBuf), "%.0f", m_pClient->m_Snap.m_pLocalCharacter->m_VelY / 32.f);
	w = TextRender()->TextWidth(0, Fontsize, aBuf, -1, -1.0f);
	TextRender()->Text(0, x - w, y, Fontsize, aBuf, -1.0f);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CChillerBotUX::RenderEnabledComponents()
{
	if(m_pClient->m_Menus.IsActive())
		return;
	if(m_pClient->m_Voting.IsVoting())
		return;
	if(!g_Config.m_ClChillerbotHud)
		return;
	int offset = 0;
	if(m_IsLeftSidedBroadcast && Client()->GameTick(g_Config.m_ClDummy) < m_BroadcastTick)
		offset = 2;
	for(auto &m_aEnabledComponent : m_aEnabledComponents)
	{
		if(m_aEnabledComponent.m_aName[0] == '\0')
			continue;
		float TwName = TextRender()->TextWidth(0, 10.0f, m_aEnabledComponent.m_aName, -1, -1);
		float TwNoteShort = 2.f;
		if(m_aEnabledComponent.m_aNoteShort[0])
			TwNoteShort += TextRender()->TextWidth(0, 10.0f, m_aEnabledComponent.m_aNoteShort, -1, -1);
		Graphics()->BlendNormal();
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0, 0, 0, 0.5f);
		RenderTools()->DrawRoundRect(4.0f, 60.f + offset * 15, TwName + TwNoteShort, 14.0f, 3.0f);
		Graphics()->QuadsEnd();
		TextRender()->Text(0, 5.0f, 60.f + offset * 15, 10.0f, m_aEnabledComponent.m_aName, -1);
		TextRender()->Text(0, 5.0f + TwName + 2.f, 60.f + offset * 15, 10.0f, m_aEnabledComponent.m_aNoteShort, -1);
		TextRender()->Text(0, 5.0f, 60.f + offset * 15 + 10, 4.0f, m_aEnabledComponent.m_aNoteLong, -1);
		offset++;
	}
}

void CChillerBotUX::EnableComponent(const char *pComponent, const char *pNoteShort, const char *pNoteLong)
{
	// update
	for(auto &Component : m_aEnabledComponents)
	{
		if(str_comp(Component.m_aName, pComponent))
			continue;
		str_copy(Component.m_aName, pComponent, sizeof(Component.m_aName));
		if(pNoteShort)
			str_copy(Component.m_aNoteShort, pNoteShort, sizeof(Component.m_aNoteShort));
		if(pNoteLong)
			str_copy(Component.m_aNoteLong, pNoteLong, sizeof(Component.m_aNoteLong));
		return;
	}
	// insert
	for(auto &Component : m_aEnabledComponents)
	{
		if(Component.m_aName[0] != '\0')
			continue;
		str_copy(Component.m_aName, pComponent, sizeof(Component.m_aName));
		Component.m_aNoteShort[0] = '\0';
		Component.m_aNoteLong[0] = '\0';
		if(pNoteShort)
			str_copy(Component.m_aNoteShort, pNoteShort, sizeof(Component.m_aNoteShort));
		if(pNoteLong)
			str_copy(Component.m_aNoteLong, pNoteLong, sizeof(Component.m_aNoteLong));
		return;
	}
}

void CChillerBotUX::DisableComponent(const char *pComponent)
{
	// update
	for(auto &Component : m_aEnabledComponents)
	{
		if(str_comp(Component.m_aName, pComponent))
			continue;
		Component.m_aName[0] = '\0';
		return;
	}
}

bool CChillerBotUX::SetComponentNoteShort(const char *pComponent, const char *pNote)
{
	if(!pNote)
		return false;
	for(auto &Component : m_aEnabledComponents)
	{
		if(str_comp(Component.m_aName, pComponent))
			continue;
		str_copy(Component.m_aNoteShort, pNote, sizeof(Component.m_aNoteShort));
		return true;
	}
	return false;
}

bool CChillerBotUX::SetComponentNoteLong(const char *pComponent, const char *pNote)
{
	if(!pNote)
		return false;
	for(auto &Component : m_aEnabledComponents)
	{
		if(str_comp(Component.m_aName, pComponent))
			continue;
		str_copy(Component.m_aNoteLong, pNote, sizeof(Component.m_aNoteLong));
		return true;
	}
	return false;
}

void CChillerBotUX::MapScreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup, float Zoom)
{
	float Points[4];
	RenderTools()->MapScreenToWorld(CenterX, CenterY, pGroup->m_ParallaxX, pGroup->m_ParallaxY,
		pGroup->m_OffsetX, pGroup->m_OffsetY, Graphics()->ScreenAspect(), Zoom, Points);
	Graphics()->MapScreen(Points[0], Points[1], Points[2], Points[3]);
}

void CChillerBotUX::CampHackTick()
{
	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return;
	if(!g_Config.m_ClCampHack)
		return;
	if(Layers()->GameGroup())
	{
		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
		MapScreenToGroup(m_pClient->m_Camera.m_Center.x, m_pClient->m_Camera.m_Center.y, Layers()->GameGroup(), m_pClient->m_Camera.m_Zoom);
		Graphics()->BlendNormal();
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0, 0, 0, 0.4f);
		RenderTools()->DrawRoundRect(m_CampHackX1, m_CampHackY1, 20.0f, 20.0f, 3.0f);
		RenderTools()->DrawRoundRect(m_CampHackX2, m_CampHackY2, 20.0f, 20.0f, 3.0f);
		if(m_CampHackX1 && m_CampHackX2 && m_CampHackY1 && m_CampHackY2)
		{
			if(m_CampHackX1 < m_CampHackX2)
				Graphics()->SetColor(0, 1, 0, 0.2f);
			else
				Graphics()->SetColor(1, 0, 0, 0.2f);
			RenderTools()->DrawRoundRect(m_CampHackX1, m_CampHackY1, m_CampHackX2 - m_CampHackX1, m_CampHackY2 - m_CampHackY1, 3.0f);
		}
		Graphics()->QuadsEnd();
		TextRender()->Text(0, m_CampHackX1, m_CampHackY1, 10.0f, "1", -1);
		TextRender()->Text(0, m_CampHackX2, m_CampHackY2, 10.0f, "2", -1);
		Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
	}
	if(!m_CampHackX1 || !m_CampHackX2 || !m_CampHackY1 || !m_CampHackY2)
		return;
	if(g_Config.m_ClCampHack < 2 || GameClient()->m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_HAMMER)
		return;
	if(m_CampHackX1 > GameClient()->m_Snap.m_pLocalCharacter->m_X)
	{
		m_pClient->m_Controls.m_InputDirectionRight[g_Config.m_ClDummy] = 1;
		m_pClient->m_Controls.m_InputDirectionLeft[g_Config.m_ClDummy] = 0;
		m_ForceDir = 1;
		if(m_pClient->m_VibeBot.IsVibing(g_Config.m_ClDummy))
		{
			m_pClient->m_Controls.m_InputData[g_Config.m_ClDummy].m_Direction = 1;
			m_pClient->m_VibeBot.m_InputData[g_Config.m_ClDummy].m_Direction = 1;
		}
	}
	else if(m_CampHackX2 < GameClient()->m_Snap.m_pLocalCharacter->m_X)
	{
		m_pClient->m_Controls.m_InputDirectionRight[g_Config.m_ClDummy] = 0;
		m_pClient->m_Controls.m_InputDirectionLeft[g_Config.m_ClDummy] = 1;
		m_ForceDir = -1;
		if(m_pClient->m_VibeBot.IsVibing(g_Config.m_ClDummy))
		{
			m_pClient->m_Controls.m_InputData[g_Config.m_ClDummy].m_Direction = -1;
			m_pClient->m_VibeBot.m_InputData[g_Config.m_ClDummy].m_Direction = -1;
		}
	}
}

bool CChillerBotUX::OnMouseMove(float x, float y)
{
	if(time_get() % 10 == 0)
		ReturnFromAfk();
	return false;
}

bool CChillerBotUX::OnInput(IInput::CEvent e)
{
	ReturnFromAfk();
	SelectCampArea(e.m_Key);
	return false;
}

void CChillerBotUX::SelectCampArea(int Key)
{
	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return;
	if(g_Config.m_ClCampHack != 1)
		return;
	if(Key != KEY_MOUSE_1)
		return;
	if(GameClient()->m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_GUN)
		return;
	m_CampClick++;
	if(m_CampClick % 2 == 0)
	{
		// UNSET IF CLOSE
		vec2 Current = vec2(GameClient()->m_Snap.m_pLocalCharacter->m_X, GameClient()->m_Snap.m_pLocalCharacter->m_Y);
		vec2 CrackPos1 = vec2(m_CampHackX1, m_CampHackY1);
		float dist = distance(CrackPos1, Current);
		if(dist < 100.0f)
		{
			m_CampHackX1 = 0;
			m_CampHackY1 = 0;
			GameClient()->m_Chat.AddLine(-2, 0, "Unset camp[1]");
			return;
		}
		vec2 CrackPos2 = vec2(m_CampHackX2, m_CampHackY2);
		dist = distance(CrackPos2, Current);
		if(dist < 100.0f)
		{
			m_CampHackX2 = 0;
			m_CampHackY2 = 0;
			GameClient()->m_Chat.AddLine(-2, 0, "Unset camp[2]");
			return;
		}
		// SET OTHERWISE
		if(m_CampClick == 2)
		{
			m_CampHackX1 = GameClient()->m_Snap.m_pLocalCharacter->m_X;
			m_CampHackY1 = GameClient()->m_Snap.m_pLocalCharacter->m_Y;
		}
		else
		{
			m_CampHackX2 = GameClient()->m_Snap.m_pLocalCharacter->m_X;
			m_CampHackY2 = GameClient()->m_Snap.m_pLocalCharacter->m_Y;
		}
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf),
			"Set camp[%d] %d",
			m_CampClick == 2 ? 1 : 2,
			GameClient()->m_Snap.m_pLocalCharacter->m_X / 32);
		GameClient()->m_Chat.AddLine(-2, 0, aBuf);
	}
	if(m_CampClick > 3)
		m_CampClick = 0;
}

void CChillerBotUX::FinishRenameTick()
{
	if(!m_pClient->m_Snap.m_pLocalCharacter)
		return;
	if(!g_Config.m_ClFinishRename)
		return;
	vec2 Pos = m_pClient->m_PredictedChar.m_Pos;
	if(CRaceHelper::IsNearFinish(m_pClient, Pos))
	{
		if(Client()->State() == IClient::STATE_ONLINE && !m_pClient->m_Menus.IsActive() && g_Config.m_ClEditor == 0)
		{
			Graphics()->BlendNormal();
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0, 0, 0, 0.5f);
			RenderTools()->DrawRoundRect(10.0f, 30.0f, 150.0f, 50.0f, 10.0f);
			Graphics()->QuadsEnd();
			TextRender()->Text(0, 20.0f, 30.f, 20.0f, "chillerbot-ux", -1);
			TextRender()->Text(0, 50.0f, 60.f, 10.0f, "finish rename", -1);
		}
		if(!m_IsNearFinish)
		{
			m_IsNearFinish = true;
			m_pClient->SendFinishName();
		}
	}
	else
	{
		m_IsNearFinish = false;
	}
}

void CChillerBotUX::OnInit()
{
	m_pChatHelper = &m_pClient->m_ChatHelper;

	m_AfkTill = 0;
	m_AfkActivity = 0;
	m_aAfkMessage[0] = '\0';

	for(auto &c : m_aEnabledComponents)
	{
		c.m_aName[0] = '\0';
		c.m_aNoteShort[0] = '\0';
		c.m_aNoteLong[0] = '\0';
	}
	UpdateComponents();
	m_GotoSwitchOffset = 0;
	m_GotoSwitchLastX = -1;
	m_GotoSwitchLastY = -1;
	m_GotoTeleOffset = 0;
	m_GotoTeleLastX = -1;
	m_GotoTeleLastY = -1;
	m_aLastKiller[0][0] = '\0';
	m_aLastKiller[1][0] = '\0';
	m_aLastKillerTime[0][0] = '\0';
	m_aLastKillerTime[1][0] = '\0';
	m_BroadcastTick = 0;
	m_IsLeftSidedBroadcast = false;
	m_HeartbeatState = STATE_WANTREFRESH;
	m_NextHeartbeat = 0;
	// TODO: replace this with priv pub key pairs otherwise account ownership claims are trash
	if(!g_Config.m_ClChillerbotId[0])
		secure_random_password(g_Config.m_ClChillerbotId, sizeof(g_Config.m_ClChillerbotId), 16);
}

void CChillerBotUX::UpdateComponents()
{
	// TODO: make this auto or nicer
	if(g_Config.m_ClChillerbotHud)
		EnableComponent("chillerbot hud");
	else
		DisableComponent("chillerbot hud");
	if(g_Config.m_ClCampHack)
		EnableComponent("camp hack");
	else
		DisableComponent("camp hack");
	if(g_Config.m_ClAutoReply)
		EnableComponent("auto reply");
	else
		DisableComponent("auto reply");
	if(g_Config.m_ClFinishRename)
		EnableComponent("finish rename");
	else
		DisableComponent("finish rename");
	if(g_Config.m_ClWarList)
		EnableComponent("war list");
	else
		DisableComponent("war list");
	if(g_Config.m_ClShowLastKiller)
		EnableComponent("last killer");
	else
		DisableComponent("last killer");
	if(g_Config.m_ClShowLastPing)
		EnableComponent("last ping");
	else
		DisableComponent("last ping");
	if(g_Config.m_ClShowWallet)
		EnableComponent("money");
	else
		DisableComponent("money");
	if(g_Config.m_ClWeaponHud)
		EnableComponent("weapon hud");
	else
		DisableComponent("weapon hud");
}

void CChillerBotUX::OnConsoleInit()
{
	Console()->Register("afk", "?i[minutes]?r[message]", CFGFLAG_CLIENT, ConAfk, this, "Activate afk mode (auto chat respond)");
	Console()->Register("camp", "?i[left]i[right]?s[tile|raw]", CFGFLAG_CLIENT, ConCampHack, this, "Activate camp mode relative to tee");
	Console()->Register("camp_abs", "i[x1]i[y1]i[x2]i[y2]?s[tile|raw]", CFGFLAG_CLIENT, ConCampHackAbs, this, "Activate camp mode absolute in the map");
	Console()->Register("uncamp", "", CFGFLAG_CLIENT, ConUnCampHack, this, "Same as cl_camp_hack 0 but resets walk input");
	Console()->Register("goto_switch", "i[number]?i[offset]", CFGFLAG_CLIENT, ConGotoSwitch, this, "Pause switch found (at offset) with given number");
	Console()->Register("goto_tele", "i[number]?i[offset]", CFGFLAG_CLIENT, ConGotoTele, this, "Pause tele found (at offset) with given number");
	Console()->Register("load_map", "s[file]", CFGFLAG_CLIENT, ConLoadMap, this, "Load mapfile");
	Console()->Register("dump_players", "?s[search]", CFGFLAG_CLIENT, ConDumpPlayers, this, "Prints players to console");
	Console()->Register("force_quit", "", CFGFLAG_CLIENT, ConForceQuit, this, "Forces a dirty client quit all data will be lost");
	// Console()->Register("set_gametile", "i[x]i[y]i[index]", CFGFLAG_CLIENT, ConSetGametile, this, "Some ugly wip map edit hack");

	Console()->Chain("cl_camp_hack", ConchainCampHack, this);
	Console()->Chain("cl_chillerbot_hud", ConchainChillerbotHud, this);
	Console()->Chain("cl_weapon_hud", ConchainWeaponHud, this);
	Console()->Chain("cl_auto_reply", ConchainAutoReply, this);
	Console()->Chain("cl_finish_rename", ConchainFinishRename, this);
	Console()->Chain("cl_show_last_killer", ConchainShowLastKiller, this);
	Console()->Chain("cl_show_last_ping", ConchainShowLastPing, this);
}

void CChillerBotUX::ConSetGametile(IConsole::IResult *pResult, void *pUserData)
{
	// CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	// char aBuf[IO_MAX_PATH_LENGTH];
	// TODO: call some twmap py script in other thread let it edit loaded map and reload it using ConLoadMap
	// str_format(aBuf, sizeof(aBuf), "maps/tmp/chillerbot-ux-%d.map.tmp", pid());
	// pSelf->m_pClient->Client()->ChillerBotLoadMap(aBuf);
}

void CChillerBotUX::ConForceQuit(IConsole::IResult *pResult, void *pUserData)
{
	exit(0);
}

void CChillerBotUX::ConDumpPlayers(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pSelf->DumpPlayers(pResult->GetString(0));
}

void CChillerBotUX::ConchainCampHack(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
		pSelf->EnableComponent("camp hack");
	else
		pSelf->DisableComponent("camp hack");
}

void CChillerBotUX::ConchainChillerbotHud(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
		pSelf->EnableComponent("chillerbot hud");
	else
		pSelf->DisableComponent("chillerbot hud");
	pSelf->UpdateComponents();
}

void CChillerBotUX::ConchainWeaponHud(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
		pSelf->EnableComponent("weapon hud");
	else
		pSelf->DisableComponent("weapon hud");
	pSelf->UpdateComponents();
}

void CChillerBotUX::ConchainAutoReply(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
		pSelf->EnableComponent("auto reply");
	else
		pSelf->DisableComponent("auto reply");
}

void CChillerBotUX::ConchainFinishRename(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
		pSelf->EnableComponent("finish rename");
	else
		pSelf->DisableComponent("finish rename");
}

void CChillerBotUX::ConchainShowLastKiller(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
		pSelf->EnableComponent("last killer");
	else
		pSelf->DisableComponent("last killer");
}

void CChillerBotUX::ConchainShowLastPing(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
		pSelf->EnableComponent("last ping");
	else
		pSelf->DisableComponent("last ping");
}

void CChillerBotUX::ConAfk(IConsole::IResult *pResult, void *pUserData)
{
	((CChillerBotUX *)pUserData)->GoAfk(pResult->NumArguments() ? pResult->GetInteger(0) : -1, pResult->GetString(1));
}

void CChillerBotUX::ConCampHackAbs(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	int Tile = 32;
	if(!str_comp(pResult->GetString(0), "raw"))
		Tile = 1;
	g_Config.m_ClCampHack = 2;
	pSelf->EnableComponent("camp hack");
	// absolute all coords
	if(pResult->NumArguments() > 1)
	{
		if(pSelf->GameClient()->m_Snap.m_pLocalCharacter)
		{
			pSelf->m_CampHackX1 = Tile * pResult->GetInteger(0);
			pSelf->m_CampHackY1 = Tile * pResult->GetInteger(1);
			pSelf->m_CampHackX2 = Tile * pResult->GetInteger(2);
			pSelf->m_CampHackY2 = Tile * pResult->GetInteger(3);
		}
		return;
	}
}

void CChillerBotUX::DumpPlayers(const char *pSearch)
{
	char aBuf[128];
	char aTime[128];
	char aLine[512];
	int OldDDTeam = -1;
	dbg_msg("dump_players", "+----------+--+----------------+----------------+---+-------+");
	dbg_msg("dump_players", "|score     |id|name            |clan            |lat|team   |");
	dbg_msg("dump_players", "+----------+--+----------------+----------------+---+-------+");
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paInfoByDDTeamScore[i];
		if(!pInfo)
			continue;

		bool IsMatch = !(pSearch && pSearch[0] != 0);
		aLine[0] = '\0';
		// score
		if(m_pClient->m_GameInfo.m_TimeScore && g_Config.m_ClDDRaceScoreBoard)
		{
			if(pInfo->m_Score == -9999)
				str_format(aBuf, sizeof(aBuf), "|%10s|", " ");
			else
			{
				str_time((int64_t)abs(pInfo->m_Score) * 100, TIME_HOURS, aTime, sizeof(aTime));
				str_format(aBuf, sizeof(aBuf), "|%10s|", aTime);
			}
		}
		else
			str_format(aBuf, sizeof(aBuf), "|%10d|", clamp(pInfo->m_Score, -999, 99999));
		str_append(aLine, aBuf, sizeof(aLine));

		// id | name
		if(pSearch && pSearch[0] != 0)
			if(str_find_nocase(m_pClient->m_aClients[pInfo->m_ClientID].m_aName, pSearch))
				IsMatch = true;
		str_format(aBuf, sizeof(aBuf), "%2d|%16s|", pInfo->m_ClientID, m_pClient->m_aClients[pInfo->m_ClientID].m_aName);
		str_append(aLine, aBuf, sizeof(aLine));

		// clan
		if(pSearch && pSearch[0] != 0)
			if(str_find_nocase(m_pClient->m_aClients[pInfo->m_ClientID].m_aClan, pSearch))
				IsMatch = true;
		str_format(aBuf, sizeof(aBuf), "%16s|", m_pClient->m_aClients[pInfo->m_ClientID].m_aClan);
		str_append(aLine, aBuf, sizeof(aLine));

		// ping
		str_format(aBuf, sizeof(aBuf), "%3d|", clamp(pInfo->m_Latency, 0, 999));
		str_append(aLine, aBuf, sizeof(aLine));

		// team
		int DDTeam = m_pClient->m_Teams.Team(pInfo->m_ClientID);
		int NextDDTeam = 0;

		for(int j = i + 1; j < MAX_CLIENTS; j++)
		{
			const CNetObj_PlayerInfo *pInfo2 = m_pClient->m_Snap.m_paInfoByDDTeamScore[j];

			if(!pInfo2)
				continue;

			NextDDTeam = m_pClient->m_Teams.Team(pInfo2->m_ClientID);
			break;
		}

		if(OldDDTeam == -1)
		{
			for(int j = i - 1; j >= 0; j--)
			{
				const CNetObj_PlayerInfo *pInfo2 = m_pClient->m_Snap.m_paInfoByDDTeamScore[j];

				if(!pInfo2)
					continue;

				OldDDTeam = m_pClient->m_Teams.Team(pInfo2->m_ClientID);
				break;
			}
		}

		if(DDTeam != TEAM_FLOCK)
		{
			if(NextDDTeam != DDTeam)
			{
				if(m_pClient->m_Snap.m_aTeamSize[0] > 8)
					str_format(aBuf, sizeof(aBuf), "%7d|", DDTeam);
				else
					str_format(aBuf, sizeof(aBuf), "Team %2d|", DDTeam);
			}
			else
				str_format(aBuf, sizeof(aBuf), "%7s|", " ");
			str_append(aLine, aBuf, sizeof(aLine));
			if(OldDDTeam != DDTeam)
				dbg_msg("dump_players", "+----------+--+----------------+----------------+---+-------/");
			if(NextDDTeam != DDTeam)
				dbg_msg("dump_players", "+----------+--+----------------+----------------+---+-------\\");
		}

		OldDDTeam = DDTeam;
		if(IsMatch)
			dbg_msg("dump_players", "%s", aLine);
	}
	dbg_msg("dump_players", "+----------+--+----------------+----------------+---+-------+");
}

void CChillerBotUX::ConCampHack(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	int Tile = 32;
	if(!str_comp(pResult->GetString(0), "raw"))
		Tile = 1;
	g_Config.m_ClCampHack = 2;
	pSelf->EnableComponent("camp hack");
	if(!pResult->NumArguments())
	{
		if(pSelf->GameClient()->m_Snap.m_pLocalCharacter)
		{
			pSelf->m_CampHackX1 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_X - 32 * 3;
			pSelf->m_CampHackY1 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_Y;
			pSelf->m_CampHackX2 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_X + 32 * 3;
			pSelf->m_CampHackY2 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_Y;
		}
		return;
	}
	// relative left and right
	if(pResult->NumArguments() > 1)
	{
		if(pSelf->GameClient()->m_Snap.m_pLocalCharacter)
		{
			pSelf->m_CampHackX1 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_X - Tile * pResult->GetInteger(0);
			pSelf->m_CampHackY1 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_Y;
			pSelf->m_CampHackX2 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_X + Tile * pResult->GetInteger(1);
			pSelf->m_CampHackY2 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_Y;
		}
		return;
	}
}

void CChillerBotUX::ConUnCampHack(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	g_Config.m_ClCampHack = 0;
	pSelf->DisableComponent("camp hack");
	pSelf->m_pClient->m_Controls.m_InputDirectionRight[g_Config.m_ClDummy] = 0;
	pSelf->m_pClient->m_Controls.m_InputDirectionLeft[g_Config.m_ClDummy] = 0;
}

void CChillerBotUX::ConGotoSwitch(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pSelf->GotoSwitch(pResult->GetInteger(0), pResult->NumArguments() > 1 ? pResult->GetInteger(1) : -1);
}

void CChillerBotUX::ConGotoTele(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pSelf->GotoTele(pResult->GetInteger(0), pResult->NumArguments() > 1 ? pResult->GetInteger(1) : -1);
}

void CChillerBotUX::ConLoadMap(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pSelf->m_pClient->Client()->ChillerBotLoadMap(pResult->GetString(0));
}

void CChillerBotUX::GotoSwitch(int Number, int Offset)
{
	int Match = -1;
	int MatchX = -1;
	int MatchY = -1;
	for(int x = 0; x < Collision()->GetWidth(); x++)
	{
		for(int y = 0; y < Collision()->GetHeight(); y++)
		{
			int i = y * Collision()->GetWidth() + x;
			if(Number == Collision()->GetSwitchNumber(i))
			{
				Match++;
				if(Offset != -1)
				{
					if(Match == Offset)
					{
						MatchX = x;
						MatchY = y;
						m_GotoSwitchOffset = Match;
						goto set_view;
					}
					continue;
				}
				MatchX = x;
				MatchY = y;
				if(Match == m_GotoSwitchOffset)
					goto set_view;
			}
		}
	}
set_view:
	if(MatchX == -1 || MatchY == -1)
		return;
	if(Match < m_GotoSwitchOffset)
		m_GotoSwitchOffset = -1;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "set_view %d %d", MatchX, MatchY);
	Console()->ExecuteLine(aBuf);
	m_GotoSwitchOffset++;
}

void CChillerBotUX::TraceSpikes()
{
	if(!g_Config.m_ClSpikeTracer)
		return;
	if(!m_pClient->m_Snap.m_pLocalCharacter)
		return;

	// int CurrentX = (int)(m_pClient->m_Snap.m_aCharacters[m_pClient->m_LocalIDs[0]].m_Cur.m_X / 32);
	// int CurrentY = (int)(m_pClient->m_Snap.m_aCharacters[m_pClient->m_LocalIDs[0]].m_Cur.m_Y / 32);
	int CurrentX = (int)(m_pClient->m_Snap.m_pLocalCharacter->m_X / 32);
	int CurrentY = (int)(m_pClient->m_Snap.m_pLocalCharacter->m_Y / 32);
	int FromX = maximum(0, CurrentX - g_Config.m_ClSpikeTracer);
	int ToX = minimum(Collision()->GetWidth(), CurrentX + g_Config.m_ClSpikeTracer);
	int FromY = maximum(0, CurrentY - g_Config.m_ClSpikeTracer);
	int ToY = minimum(Collision()->GetHeight(), CurrentY + g_Config.m_ClSpikeTracer);
	static float m_ScreenX0;
	static float m_ScreenX1;
	static float m_ScreenY0;
	static float m_ScreenY1;
	Graphics()->GetScreen(&m_ScreenX0, &m_ScreenY0, &m_ScreenX1, &m_ScreenY1);
	MapScreenToGroup(m_pClient->m_Camera.m_Center.x, m_pClient->m_Camera.m_Center.y, Layers()->GameGroup(), m_pClient->m_Camera.m_Zoom);
	for(int x = FromX; x < ToX; x++)
	{
		for(int y = FromY; y < ToY; y++)
		{
			int Tile = Collision()->GetIndex(x, y);
			if(Tile == TILE_DEATH)
			{
				Graphics()->TextureClear();
				bool IsIntersect = false;
				if(Collision()->IntersectLine(vec2(x * 32, y * 32), vec2(CurrentX * 32, CurrentY * 32), 0, 0))
				{
					IsIntersect = true;
					if(!g_Config.m_ClSpikeTracerWalls)
						continue;
				}
				Graphics()->LinesBegin();
				if(IsIntersect)
					Graphics()->SetColor(1.f, 1.f, 1.f, 0.45f);
				else
					Graphics()->SetColor(1.f, 0.f, 0.f, 0.75f);
				IGraphics::CLineItem LineItem(x * 32, y * 32, CurrentX * 32, CurrentY * 32);
				Graphics()->LinesDraw(&LineItem, 1);
				Graphics()->LinesEnd();
			}
		}
	}
	Graphics()->MapScreen(m_ScreenX0, m_ScreenY0, m_ScreenX1, m_ScreenY1);
}

void CChillerBotUX::GotoTele(int Number, int Offset)
{
	int Match = -1;
	int MatchX = -1;
	int MatchY = -1;
	for(int x = 0; x < Collision()->GetWidth(); x++)
	{
		for(int y = 0; y < Collision()->GetHeight(); y++)
		{
			int i = y * Collision()->GetWidth() + x;
			int Tele = Collision()->TeleLayer()[i].m_Number;
			if(Number == Tele)
			{
				Match++;
				if(Offset != -1)
				{
					if(Match == Offset)
					{
						MatchX = x;
						MatchY = y;
						m_GotoTeleOffset = Match;
						goto set_view;
					}
					continue;
				}
				MatchX = x;
				MatchY = y;
				if(m_GotoTeleLastX != -1 && m_GotoTeleLastY != -1)
				{
					if(distance(vec2(m_GotoTeleLastX, m_GotoTeleLastY), vec2(x, y)) < 10.0f)
					{
						m_GotoTeleOffset++;
						continue;
					}
				}
				m_GotoTeleLastX = x;
				m_GotoTeleLastY = y;
				if(Match == m_GotoTeleOffset)
					goto set_view;
			}
		}
	}
set_view:
	if(MatchX == -1 || MatchY == -1)
		return;
	if(Match < m_GotoTeleOffset)
		m_GotoTeleOffset = -1;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "set_view %d %d", MatchX, MatchY);
	Console()->ExecuteLine(aBuf);
	m_GotoTeleOffset++;
}

void CChillerBotUX::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;

		// unpack messages
		CKillMsg Kill;
		Kill.m_aVictimName[0] = '\0';
		Kill.m_aKillerName[0] = '\0';

		Kill.m_VictimID = pMsg->m_Victim;
		if(Kill.m_VictimID >= 0 && Kill.m_VictimID < MAX_CLIENTS)
		{
			Kill.m_VictimTeam = m_pClient->m_aClients[Kill.m_VictimID].m_Team;
			Kill.m_VictimDDTeam = m_pClient->m_Teams.Team(Kill.m_VictimID);
			str_copy(Kill.m_aVictimName, m_pClient->m_aClients[Kill.m_VictimID].m_aName, sizeof(Kill.m_aVictimName));
			Kill.m_VictimRenderInfo = m_pClient->m_aClients[Kill.m_VictimID].m_RenderInfo;
		}

		Kill.m_KillerID = pMsg->m_Killer;
		if(Kill.m_KillerID >= 0 && Kill.m_KillerID < MAX_CLIENTS)
		{
			Kill.m_KillerTeam = m_pClient->m_aClients[Kill.m_KillerID].m_Team;
			str_copy(Kill.m_aKillerName, m_pClient->m_aClients[Kill.m_KillerID].m_aName, sizeof(Kill.m_aKillerName));
			Kill.m_KillerRenderInfo = m_pClient->m_aClients[Kill.m_KillerID].m_RenderInfo;
		}

		Kill.m_Weapon = pMsg->m_Weapon;
		Kill.m_ModeSpecial = pMsg->m_ModeSpecial;
		Kill.m_Tick = Client()->GameTick(g_Config.m_ClDummy);

		Kill.m_FlagCarrierBlue = m_pClient->m_Snap.m_pGameDataObj ? m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue : -1;

		bool KillMsgValid = (Kill.m_VictimRenderInfo.m_CustomColoredSkin && Kill.m_VictimRenderInfo.m_ColorableRenderSkin.m_Body.IsValid()) || (!Kill.m_VictimRenderInfo.m_CustomColoredSkin && Kill.m_VictimRenderInfo.m_OriginalRenderSkin.m_Body.IsValid());
		// if killer != victim, killer must be valid too
		KillMsgValid &= Kill.m_KillerID == Kill.m_VictimID || ((Kill.m_KillerRenderInfo.m_CustomColoredSkin && Kill.m_KillerRenderInfo.m_ColorableRenderSkin.m_Body.IsValid()) || (!Kill.m_KillerRenderInfo.m_CustomColoredSkin && Kill.m_KillerRenderInfo.m_OriginalRenderSkin.m_Body.IsValid()));
		if(KillMsgValid && Kill.m_KillerID != Kill.m_VictimID)
		{
			for(int i = 0; i < 2; i++)
			{
				if(m_pClient->m_LocalIDs[i] != Kill.m_VictimID)
					continue;

				str_copy(m_aLastKiller[i], Kill.m_aKillerName, sizeof(m_aLastKiller[i]));
				char aBuf[512];
				str_timestamp_format(m_aLastKillerTime[i], sizeof(m_aLastKillerTime[i]), "%H:%M");
				str_format(
					aBuf,
					sizeof(aBuf),
					"[%s]main: %s [%s]dummy: %s",
					m_aLastKillerTime[0],
					m_aLastKiller[0],
					m_aLastKillerTime[1],
					m_aLastKiller[1]);
				SetComponentNoteLong("last killer", aBuf);
			}
		}
	}
	else if(MsgType == NETMSGTYPE_SV_BROADCAST)
	{
		CNetMsg_Sv_Broadcast *pMsg = (CNetMsg_Sv_Broadcast *)pRawMsg;
		str_copy(m_aBroadcastText, pMsg->m_pMessage, sizeof(m_aBroadcastText));
		m_BroadcastTick = Client()->GameTick(g_Config.m_ClDummy) + Client()->GameTickSpeed() * 10;
		m_IsLeftSidedBroadcast = str_find(m_aBroadcastText, "                                ") != NULL;
	}
}

void CChillerBotUX::GoAfk(int Minutes, const char *pMsg)
{
	if(pMsg)
	{
		str_copy(m_aAfkMessage, pMsg, sizeof(m_aAfkMessage));
		if((unsigned long)str_length(pMsg) > sizeof(m_aAfkMessage))
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "error: afk message too long %d/%lu", str_length(pMsg), sizeof(m_aAfkMessage));
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
			return;
		}
	}
	m_AfkTill = time_get() + time_freq() * 60 * Minutes;
	m_AfkActivity = 0;
	m_pChatHelper->ClearLastAfkPingMessage();
	EnableComponent("afk");
	EnableComponent("last killer");
	g_Config.m_ClShowLastKiller = 1;
}

void CChillerBotUX::ReturnFromAfk(const char *pChatMessage)
{
	if(!m_AfkTill)
		return;
	if(pChatMessage && pChatMessage[0] != '/')
	{
		if(m_IgnoreChatAfk > 0)
			m_IgnoreChatAfk--;
		else
			m_AfkActivity += 400;
	}
	m_AfkActivity++;
	if(m_AfkActivity < 200)
		return;
	m_pClient->m_Chat.AddLine(-2, 0, "[chillerbot-ux] welcome back :)");
	m_AfkTill = 0;
	DisableComponent("afk");
}

int CChillerBotUX::CountOnlinePlayers()
{
	// Code from scoreboard. There is probably a better way to do this
	int Num = 0;
	for(const auto *pInfo : m_pClient->m_Snap.m_paInfoByDDTeamScore)
		if(pInfo)
			Num++;
	return Num;
}
