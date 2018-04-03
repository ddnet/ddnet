/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_KILLMESSAGES_H
#define GAME_CLIENT_COMPONENTS_KILLMESSAGES_H
#include <game/client/component.h>

class CKillMessages : public CComponent
{
	int m_SpriteQuadContainerIndex;
public:
	// kill messages
	struct CKillMsg
	{
		CKillMsg()
		{
			m_KillerTextContainerIndex = m_VictimTextContainerIndex = -1;
		}

		int m_Weapon;

		int m_VictimID;
		int m_VictimTeam;
		int m_VictimDDTeam;
		char m_aVictimName[64];
		int m_VictimTextContainerIndex;
		float m_VitctimTextWidth;
		CTeeRenderInfo m_VictimRenderInfo;

		int m_KillerID;
		int m_KillerTeam;
		char m_aKillerName[64];
		int m_KillerTextContainerIndex;
		float m_KillerTextWidth;
		CTeeRenderInfo m_KillerRenderInfo;

		int m_ModeSpecial; // for CTF, if the guy is carrying a flag for example
		int m_Tick;
	};

	enum
	{
		MAX_KILLMSGS = 5,
	};

	CKillMsg m_aKillmsgs[MAX_KILLMSGS];
	int m_KillmsgCurrent;

	virtual void OnWindowResize();
	virtual void OnReset();
	virtual void OnRender();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual void OnInit();
};

#endif
