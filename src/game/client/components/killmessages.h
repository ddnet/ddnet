/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_KILLMESSAGES_H
#define GAME_CLIENT_COMPONENTS_KILLMESSAGES_H
#include <game/client/component.h>

#include <game/client/render.h>

class CKillMessages : public CComponent
{
	int m_SpriteQuadContainerIndex = 0;

public:
	// kill messages
	struct CKillMsg
	{
		CKillMsg()
		{
			m_KillerTextContainerIndex = m_VictimTextContainerIndex = -1;
		}

		int m_Weapon = 0;

		int m_VictimID = 0;
		int m_VictimTeam = 0;
		int m_VictimDDTeam = 0;
		char m_aVictimName[64] = {0};
		int m_VictimTextContainerIndex = 0;
		float m_VitctimTextWidth = 0;
		CTeeRenderInfo m_VictimRenderInfo;

		int m_KillerID = 0;
		int m_KillerTeam = 0;
		char m_aKillerName[64] = {0};
		int m_KillerTextContainerIndex = 0;
		float m_KillerTextWidth = 0;
		CTeeRenderInfo m_KillerRenderInfo;

		int m_ModeSpecial = 0; // for CTF, if the guy is carrying a flag for example
		int m_Tick = 0;
		int m_FlagCarrierBlue = 0;
	};

	enum
	{
		MAX_KILLMSGS = 5,
	};

private:
	void CreateKillmessageNamesIfNotCreated(CKillMsg &Kill);

public:
	CKillMsg m_aKillmsgs[MAX_KILLMSGS];
	int m_KillmsgCurrent = 0;

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnWindowResize() override;
	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual void OnInit() override;

	void RefindSkins();
};

#endif
