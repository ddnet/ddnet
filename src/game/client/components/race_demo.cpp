/* (c) Redix and Sushi */

#include <base/system.h>
#include <engine/shared/config.h>
#include <engine/serverbrowser.h>
#include <engine/storage.h>

#include "menus.h"
#include "race.h"
#include "race_demo.h"

CRaceDemo::CRaceDemo() : m_RaceState(RACE_NONE), m_RecordStartTick(-1), m_RecordStopTick(-1), m_Time(0) {}

void CRaceDemo::OnStateChange(int NewState, int OldState)
{
	if(OldState == IClient::STATE_ONLINE)
		StopRecord();
}

void CRaceDemo::OnRender()
{
	if(!g_Config.m_ClAutoRaceRecord || !m_pClient->m_Snap.m_pLocalCharacter)
		return;

	// only for race
	CServerInfo ServerInfo;
	Client()->GetServerInfo(&ServerInfo);
	if(!IsRace(&ServerInfo) || !m_pClient->m_NewTick)
		return;

	// start the demo
	if((m_RaceState == RACE_NONE || !m_IsSolo) && m_RecordStartTick + Client()->GameTickSpeed() < Client()->GameTick())
	{
		vec2 PrevPos = vec2(m_pClient->m_Snap.m_pLocalPrevCharacter->m_X, m_pClient->m_Snap.m_pLocalPrevCharacter->m_Y);
		vec2 Pos = vec2(m_pClient->m_Snap.m_pLocalCharacter->m_X, m_pClient->m_Snap.m_pLocalCharacter->m_Y);

		if(CRaceHelper::IsStart(m_pClient, m_pClient->m_PredictedPrevChar.m_Pos, m_pClient->m_LocalCharacterPos))
		{
			if(m_RaceState == RACE_STARTED)
				OnReset();
			Client()->RaceRecord_Start();
			m_RecordStartTick = Client()->GameTick();
			m_RaceState = RACE_STARTED;
		}
	}

	// stop the demo
	if(m_RaceState == RACE_FINISHED && m_RecordStopTick <= Client()->GameTick())
		StopRecord(m_Time);
}

void CRaceDemo::OnReset()
{
	StopRecord();
}

void CRaceDemo::OnMessage(int MsgType, void *pRawMsg)
{
	if(!g_Config.m_ClAutoRaceRecord || m_pClient->m_Snap.m_SpecInfo.m_Active)
		return;

	// only for race
	CServerInfo ServerInfo;
	Client()->GetServerInfo(&ServerInfo);
	if(!IsRace(&ServerInfo))
		return;

	// check for messages from server
	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;
		if(pMsg->m_Victim == m_pClient->m_Snap.m_LocalClientID)
			StopRecord(m_Time);
	}
	else if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		if(pMsg->m_ClientID == -1 && m_RaceState == RACE_STARTED)
		{
			char aName[MAX_NAME_LENGTH];
			int Time = CRaceHelper::TimeFromFinishMessage(pMsg->m_pMessage, aName, sizeof(aName));
			if(Time > 0 && str_comp(aName, m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_aName) == 0)
			{
				m_RaceState = RACE_FINISHED;
				m_RecordStopTick = Client()->GameTick() + Client()->GameTickSpeed();
				m_Time = Time;
			}
		}
	}
}

void CRaceDemo::OnMapLoad()
{
	m_IsSolo = true;
}

void CRaceDemo::StopRecord(int Time)
{
	if(Client()->RaceRecord_IsRecording())
		Client()->RaceRecord_Stop();

	char aDemoName[128];
	char aTmpFilename[512];
	Client()->RaceRecord_GetName(aDemoName, sizeof(aDemoName));
	str_format(aTmpFilename, sizeof(aTmpFilename), "demos/%s.demo", aDemoName);

	if(Time > 0 && CheckDemo(Time))
	{
		// save file
		char aNewFilename[512];
		Client()->RaceRecord_GetName(aDemoName, sizeof(aDemoName), m_Time);
		str_format(aNewFilename, sizeof(aNewFilename), "demos/%s.demo", aDemoName);

		Storage()->RenameFile(aTmpFilename, aNewFilename, IStorage::TYPE_SAVE);
	}
	else // no new record
		Storage()->RemoveFile(aTmpFilename, IStorage::TYPE_SAVE);

	m_Time = 0;
	m_RaceState = RACE_NONE;
	m_RecordStartTick = -1;
	m_RecordStopTick = -1;
}

bool CRaceDemo::CheckDemo(int Time) const
{
	if(str_comp(m_pClient->m_pMenus->GetCurrentDemoFolder(), "demos") != 0)
		return true;

	char aTmpDemoName[128];
	Client()->RaceRecord_GetName(aTmpDemoName, sizeof(aTmpDemoName));

	// loop through demo files
	m_pClient->m_pMenus->DemolistPopulate();
	for(int i = 0; i < m_pClient->m_pMenus->m_lDemos.size(); i++)
	{
		// skip temp file
		const char *pDemoName = m_pClient->m_pMenus->m_lDemos[i].m_aName;
		if(str_comp(pDemoName, aTmpDemoName) == 0)
			continue;

		int MapLen = str_length(Client()->GetCurrentMap());
		if(str_comp_num(pDemoName, Client()->GetCurrentMap(), MapLen) != 0 || pDemoName[MapLen] != '_')
			continue;

		// set cursor
		pDemoName += MapLen + 1;

		if(g_Config.m_ClDemoName)
		{
			char aPlayerName[MAX_NAME_LENGTH];
			str_copy(aPlayerName, g_Config.m_PlayerName, sizeof(aPlayerName));
			char *pStr = aPlayerName;

			while(*pStr)
			{
				if(*pStr == '\\' || *pStr == '/' || *pStr == '|' || *pStr == ':' || *pStr == '*' || *pStr == '?' || *pStr == '<' || *pStr == '>' || *pStr == '"')
					*pStr = '%';
				pStr++;
			}

			if(!str_find(pDemoName, aPlayerName))
				continue;
		}

		int DemoTime = CRaceHelper::TimeFromSecondsStr(pDemoName);
		if(DemoTime > 0)
		{
			if(Time >= DemoTime) // found a better demo
				return false;

			// delete old demo
			char aFilename[512];
			str_format(aFilename, sizeof(aFilename), "demos/%s", m_pClient->m_pMenus->m_lDemos[i].m_aFilename);
			Storage()->RemoveFile(aFilename, IStorage::TYPE_SAVE);
		}
	}

	return true;
}
