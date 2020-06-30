#include "score.h"

CScorePlayerResult::CScorePlayerResult() :
	m_Done(false)
{
	SetVariant(Variant::DIRECT);
}

void CScorePlayerResult::SetVariant(Variant v)
{
	m_MessageKind = v;
	switch(v)
	{
	case DIRECT:
	case ALL:
		for(int i = 0; i < MAX_MESSAGES; i++)
			m_Data.m_aaMessages[i][0] = 0;
		break;
	case BROADCAST:
		m_Data.m_Broadcast[0] = 0;
		break;
	case MAP_VOTE:
		m_Data.m_MapVote.m_Map[0] = '\0';
		m_Data.m_MapVote.m_Reason[0] = '\0';
		m_Data.m_MapVote.m_Server[0] = '\0';
		break;
	case PLAYER_INFO:
		m_Data.m_Info.m_Score = -9999;
		m_Data.m_Info.m_Birthday = 0;
		m_Data.m_Info.m_HasFinishScore = false;
		m_Data.m_Info.m_Time = 0;
		for(int i = 0; i < NUM_CHECKPOINTS; i++)
			m_Data.m_Info.m_CpTime[i] = 0;
	}
}
