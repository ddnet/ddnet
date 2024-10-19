// ChillerDragon 2021 - chillerbot ux

#include <base/vmath.h>
#include <engine/config.h>
#include <engine/shared/config.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>

#include "chillerbotux.h"

#include "vibebot.h"

CNetObj_Character *CVibeBot::GetCharacter() const
{
	return &m_pClient->m_Snap.m_aCharacters[m_pClient->m_aLocalIds[m_MoveId]].m_Cur;
}

void CVibeBot::Reset()
{
	m_SendData[0] = false;
	m_SendData[1] = false;
}

void CVibeBot::OnInit()
{
	Reset();
	m_Mode[0] = VB_OFF;
	m_Mode[1] = VB_OFF;
	m_EmoteBot[0] = EB_OFF;
	m_EmoteBot[1] = EB_OFF;
	m_EmoteBotDelay[0] = 0;
	m_EmoteBotDelay[1] = 0;
	m_MoveId = 0;
	m_NextEmote[0] = 0;
	m_NextEmote[1] = 0;
	m_NextAim[0] = 0;
	m_NextAim[1] = 0;
	m_NextAimMove[0] = 0;
	m_NextAimMove[1] = 0;
	m_CurrentAim[0] = vec2(100, 100);
	m_CurrentAim[1] = vec2(100, 100);
	m_WantedAim[0] = vec2(100, 100);
	m_WantedAim[1] = vec2(100, 100);
}

void CVibeBot::SetMode(int Mode, int ClientId)
{
	int Id = 0;
	if(Mode == VB_OFF)
	{
		Id = ClientId ? !g_Config.m_ClDummy : g_Config.m_ClDummy;
		m_pClient->m_Controls.m_aInputData[Id].m_Fire = m_aInputData[ClientId].m_Fire;
		m_pClient->m_Controls.m_aInputData[Id].m_WantedWeapon = m_aInputData[ClientId].m_WantedWeapon;
	}
	else
	{
		Id = g_Config.m_ClDummy ? !ClientId : ClientId;
		m_aInputData[ClientId].m_Fire = m_pClient->m_Controls.m_aInputData[Id].m_Fire;
		m_aInputData[ClientId].m_WantedWeapon = m_pClient->m_Controls.m_aInputData[Id].m_WantedWeapon;
	}
	m_Mode[ClientId] = Mode;
	UpdateComponents();
}

void CVibeBot::SetEmoteBot(int Mode, int Delay, int ClientId)
{
	int Id = 0;
	if(Mode == EB_OFF)
	{
		Id = ClientId ? !g_Config.m_ClDummy : g_Config.m_ClDummy;
		m_pClient->m_Controls.m_aInputData[Id].m_Fire = m_aInputData[ClientId].m_Fire;
		m_pClient->m_Controls.m_aInputData[Id].m_WantedWeapon = m_aInputData[ClientId].m_WantedWeapon;
	}
	else
	{
		Id = g_Config.m_ClDummy ? !ClientId : ClientId;
		m_aInputData[ClientId].m_Fire = m_pClient->m_Controls.m_aInputData[Id].m_Fire;
		m_aInputData[ClientId].m_WantedWeapon = m_pClient->m_Controls.m_aInputData[Id].m_WantedWeapon;
	}
	m_EmoteBot[ClientId] = Mode;
	m_EmoteBotDelay[ClientId] = Delay;
	UpdateComponents();
}

void CVibeBot::UpdateComponents()
{
	if(m_Mode[0] == VB_OFF && m_Mode[1] == VB_OFF)
	{
		m_pClient->m_ChillerBotUX.DisableComponent("vibebot");
		return;
	}
	char aBuf2[128];
	char aBuf[128];
	aBuf[0] = '\0';
	if(m_Mode[0] != VB_OFF)
		str_format(aBuf, sizeof(aBuf), "main: %d ", m_Mode[0]);
	if(m_Mode[1] != VB_OFF)
	{
		str_format(aBuf2, sizeof(aBuf2), "dummy: %d", m_Mode[1]);
		str_append(aBuf, aBuf2, sizeof(aBuf));
	}
	m_pClient->m_ChillerBotUX.EnableComponent("vibebot", "", aBuf);
}

void CVibeBot::OnConsoleInit()
{
	// Console()->Register("vibe", "s[sleepy|wtf|happy|music]?i[dummy]", CFGFLAG_CLIENT, ConVibe, this, "Set vibebot mode ('vibebots' for list)");
	// Console()->Register("vibes", "", CFGFLAG_CLIENT, ConVibes, this, "Shows all vibebots (set via vibebot <name>)");
	// Console()->Register("unvibe", "", CFGFLAG_CLIENT, ConUnVibe, this, "Turn off vibebot");
	Console()->Register("emotebot", "s[sleepy|wtf|happy|music]?i[delay]?i[dummy]", CFGFLAG_CLIENT, ConEmoteBot, this, "Automatically send emotes");
}

void CVibeBot::ConEmoteBot(IConsole::IResult *pResult, void *pUserData)
{
	CVibeBot *pSelf = (CVibeBot *)pUserData;
	int Mode = EB_OFF;
	if(!str_comp(pResult->GetString(0), "happy"))
		Mode = E_HAPPY;
	else if(!str_comp(pResult->GetString(0), "sleepy"))
		Mode = E_SLEEPY;
	else if(!str_comp(pResult->GetString(0), "wtf"))
		Mode = E_WTF;
	else if(!str_comp(pResult->GetString(0), "music"))
		Mode = E_MUSIC;
	else if(str_comp(pResult->GetString(0), "off"))
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "emotebot", "unkown emote.");
	}
	pSelf->SetEmoteBot(
		Mode,
		pResult->NumArguments() > 1 ? pResult->GetInteger(1) : 4,
		pResult->NumArguments() > 2 ? pResult->GetInteger(2) : g_Config.m_ClDummy);
}

void CVibeBot::ConVibe(IConsole::IResult *pResult, void *pUserData)
{
	CVibeBot *pSelf = (CVibeBot *)pUserData;
	int Mode = VB_OFF;
	if(!str_comp(pResult->GetString(0), "happy"))
		Mode = VB_HAPPY;
	else if(!str_comp(pResult->GetString(0), "sleepy"))
		Mode = VB_SLEEPY;
	else if(!str_comp(pResult->GetString(0), "wtf"))
		Mode = VB_WTF;
	else if(!str_comp(pResult->GetString(0), "music"))
		Mode = VB_MUSIC;
	else if(str_comp(pResult->GetString(0), "off"))
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "vibebot", "unkown mode use 'vibes' to list all modes.");
	}
	pSelf->SetMode(Mode, pResult->NumArguments() > 1 ? pResult->GetInteger(1) : g_Config.m_ClDummy);
}

void CVibeBot::ConUnVibe(IConsole::IResult *pResult, void *pUserData)
{
	CVibeBot *pSelf = (CVibeBot *)pUserData;
	pSelf->SetMode(CVibeBot::VB_OFF, 0);
	pSelf->SetMode(CVibeBot::VB_OFF, 1);
	pSelf->UpdateComponents();
}

void CVibeBot::ConVibes(IConsole::IResult *pResult, void *pUserData)
{
	CVibeBot *pSelf = (CVibeBot *)pUserData;
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "vibebot", "--- vibebot ---");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "vibebot", "off: off");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "vibebot", "happy: happy emote and casual eye move");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "vibebot", "music: music emote and casual eye move");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "vibebot", "sleepy: zzZ emote and casual eye move");
}

void CVibeBot::EmoteBotTick()
{
	if(m_EmoteBot[MoveId()] == EB_OFF)
		return;

	if(time_get() > m_NextEmoteBot[MoveId()])
	{
		m_NextEmoteBot[MoveId()] = time_get() + time_freq() * m_EmoteBotDelay[MoveId()];
		Emote(m_EmoteBot[MoveId()]);
	}
}

void CVibeBot::VibeEmote(int Emoticon)
{
	if(time_get() > m_NextEmote[MoveId()])
	{
		m_NextEmote[MoveId()] = time_get() + time_freq() * (rand() % 10) + 5;
		Emote(Emoticon);
	}
	if(time_get() > m_NextAim[MoveId()])
	{
		m_NextAim[MoveId()] = time_get() + time_freq() * (rand() % 5) + 5;
		Aim(rand() % 200 - 100, rand() % 200 - 100);
	}
}

void CVibeBot::Emote(int Emoticon)
{
	CMsgPacker Msg(NETMSGTYPE_CL_EMOTICON, false);
	Msg.AddInt(Emoticon);
	Client()->SendMsg(m_MoveId, &Msg, MSGFLAG_VITAL);
}

void CVibeBot::Aim(int TargetX, int TargetY)
{
	m_WantedAim[MoveId()].x = TargetX;
	m_WantedAim[MoveId()].y = TargetY;
}

void CVibeBot::AimTick()
{
	vec2 ControlsAim = vec2(
		m_pClient->m_Controls.m_aInputData[MoveId()].m_TargetX,
		m_pClient->m_Controls.m_aInputData[MoveId()].m_TargetY);
	if(distance(m_CurrentAim[MoveId()], m_WantedAim[MoveId()]) > 60.0f)
	{
		m_SendData[MoveId()] = true;
		if(time_get() > m_NextAimMove[MoveId()])
		{
			m_NextAimMove[MoveId()] = time_get() + (time_freq() / 10);
			if(m_CurrentAim[MoveId()].x < m_WantedAim[MoveId()].x)
				m_CurrentAim[MoveId()].x++;
			else
				m_CurrentAim[MoveId()].x--;
			if(m_CurrentAim[MoveId()].y < m_WantedAim[MoveId()].y)
				m_CurrentAim[MoveId()].y++;
			else
				m_CurrentAim[MoveId()].y--;
		}
	}
	else if(distance(ControlsAim, m_CurrentAim[MoveId()]) > 5.0f)
	{
		m_SendData[MoveId()] = true;
		m_WantedAim[MoveId()].x = m_pClient->m_Controls.m_aInputData[MoveId()].m_TargetX;
		m_WantedAim[MoveId()].y = m_pClient->m_Controls.m_aInputData[MoveId()].m_TargetY;
	}
	m_aInputData[MoveId()].m_TargetX = m_CurrentAim[MoveId()].x;
	m_aInputData[MoveId()].m_TargetY = m_CurrentAim[MoveId()].y;
}

void CVibeBot::OnRender()
{
	Reset();
	for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
	{
		EmoteBotTick();
		if(m_Mode[Dummy] == VB_OFF)
			continue;
		m_MoveId = Dummy;
		if(!GetCharacter())
			continue;
		m_aInputData[MoveId()] = m_pClient->m_Controls.m_aInputData[MoveId()];
		AimTick();
		if(m_Mode[Dummy] == VB_HAPPY)
			VibeEmote(E_HAPPY);
		else if(m_Mode[Dummy] == VB_SLEEPY)
			VibeEmote(E_SLEEPY);
		else if(m_Mode[Dummy] == VB_WTF)
			VibeEmote(E_WTF);
		else if(m_Mode[Dummy] == VB_MUSIC)
			VibeEmote(E_MUSIC);
	}
}
