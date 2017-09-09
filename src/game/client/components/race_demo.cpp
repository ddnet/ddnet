/* (c) Redix and Sushi */

#include <base/system.h>
#include <engine/shared/config.h>
#include <engine/serverbrowser.h>
#include <engine/storage.h>

#include "menus.h"
#include "race.h"
#include "race_demo.h"

CRaceDemo::CRaceDemo()
{
	m_RaceState = RACE_NONE;
	m_RecordStopTime = 0;
	m_Time = 0;
	m_DemoStartTick = 0;
}

void CRaceDemo::Stop()
{
	if(Client()->RaceRecordIsRecording())
		Client()->RaceRecordStop();

	char aFilename[512];
	str_format(aFilename, sizeof(aFilename), "demos/%s_tmp_%d.demo", m_pMap, pid());
	Storage()->RemoveFile(aFilename, IStorage::TYPE_SAVE);

	m_Time = 0;
	m_RaceState = RACE_NONE;
	m_RecordStopTime = 0;
	m_DemoStartTick = 0;
}

void CRaceDemo::OnStateChange(int NewState, int OldState)
{
	if(OldState == IClient::STATE_ONLINE)
		Stop();
}

void CRaceDemo::OnRender()
{
	if(!g_Config.m_ClAutoRaceRecord || !m_pClient->m_Snap.m_pGameInfoObj || m_pClient->m_Snap.m_SpecInfo.m_Active || Client()->State() != IClient::STATE_ONLINE)
		return;

	// start the demo
	if(m_DemoStartTick < Client()->GameTick() && CRaceHelper::IsStart(m_pClient, m_pClient->m_PredictedPrevChar.m_Pos, m_pClient->m_LocalCharacterPos))
	{
		OnReset();
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "tmp_%d", pid());
		m_pMap = Client()->RaceRecordStart(aBuf);
		m_DemoStartTick = Client()->GameTick() + Client()->GameTickSpeed();
		m_RaceState = RACE_STARTED;
	}

	// stop the demo
	if(m_RaceState == RACE_FINISHED && m_RecordStopTime < Client()->GameTick() && m_Time > 0)
	{
		CheckDemo();
		OnReset();
	}
}

void CRaceDemo::OnReset()
{
	if(Client()->State() == IClient::STATE_ONLINE)
		Stop();
}

void CRaceDemo::OnShutdown()
{
	Stop();
}

void CRaceDemo::OnMessage(int MsgType, void *pRawMsg)
{
	if(!g_Config.m_ClAutoRaceRecord || Client()->State() != IClient::STATE_ONLINE || m_pClient->m_Snap.m_SpecInfo.m_Active)
		return;

	// check for messages from server
	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;
		if(pMsg->m_Victim == m_pClient->m_Snap.m_LocalClientID && m_RaceState == RACE_FINISHED)
		{
			// check for new record
			CheckDemo();
			OnReset();
		}
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
				m_RecordStopTime = Client()->GameTick() + Client()->GameTickSpeed();
				m_Time = Time;
			}
		}
	}
}

void CRaceDemo::CheckDemo()
{
	// stop the demo recording
	Client()->RaceRecordStop();

	char aTmpDemoName[128];
	str_format(aTmpDemoName, sizeof(aTmpDemoName), "%s_tmp_%d", m_pMap, pid());

	// loop through demo files
	m_pClient->m_pMenus->DemolistPopulate();
	for(int i = 0; i < m_pClient->m_pMenus->m_lDemos.size(); i++)
	{
		const char *pDemo = m_pClient->m_pMenus->m_lDemos[i].m_aName;
		if(str_comp(pDemo, aTmpDemoName) == 0)
			continue;

		int MapLen = str_length(m_pMap);
		if(str_comp_num(pDemo, m_pMap, MapLen) == 0 && pDemo[MapLen] == '_')
		{
			// set cursor
			pDemo += MapLen + 1;
			int Time = CRaceHelper::TimeFromSecondsStr(pDemo);
			if(Time > 0 && m_Time < Time)
			{
				// save new record
				SaveDemo(m_pMap);

				// delete old demo
				char aFilename[512];
				str_format(aFilename, sizeof(aFilename), "demos/%s.demo", m_pClient->m_pMenus->m_lDemos[i].m_aName);
				Storage()->RemoveFile(aFilename, IStorage::TYPE_SAVE);
			}

			m_Time = 0;

			return;
		}
	}

	// save demo if there is none
	SaveDemo(m_pMap);

	m_Time = 0;
}

void CRaceDemo::SaveDemo(const char* pDemo)
{
	char aNewFilename[512];
	char aOldFilename[512];
	if(g_Config.m_ClDemoName)
	{
		char aPlayerName[MAX_NAME_LENGTH];
		str_copy(aPlayerName, m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_aName, sizeof(aPlayerName));

		// check the player name
		for(int i = 0; i < MAX_NAME_LENGTH; i++)
		{
			if(!aPlayerName[i])
				break;

			if(aPlayerName[i] == '\\' || aPlayerName[i] == '/' || aPlayerName[i] == '|' || aPlayerName[i] == ':' || aPlayerName[i] == '*' || aPlayerName[i] == '?' || aPlayerName[i] == '<' || aPlayerName[i] == '>' || aPlayerName[i] == '"')
				aPlayerName[i] = '%';

			str_format(aNewFilename, sizeof(aNewFilename), "demos/%s_%d.%03d_%s.demo", pDemo, m_Time / 1000, m_Time % 1000, aPlayerName);
		}
	}
	else
		str_format(aNewFilename, sizeof(aNewFilename), "demos/%s_%d.%03d.demo", pDemo, m_Time / 1000, m_Time % 1000);

	str_format(aOldFilename, sizeof(aOldFilename), "demos/%s_tmp_%d.demo", m_pMap, pid());

	Storage()->RenameFile(aOldFilename, aNewFilename, IStorage::TYPE_SAVE);

	dbg_msg("racedemo", "saved better demo");
}
