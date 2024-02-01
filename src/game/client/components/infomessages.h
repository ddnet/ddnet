/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_INFOMESSAGES_H
#define GAME_CLIENT_COMPONENTS_INFOMESSAGES_H
#include <game/client/component.h>

#include <game/client/render.h>
class CInfoMessages : public CComponent
{
	int m_SpriteQuadContainerIndex;
	enum
	{
		MAX_INFOMSGS = 5,
		MAX_KILLMSG_TEAM_MEMBERS = 4,
	};

	enum EType
	{
		TYPE_KILL,
		TYPE_FINISH,
	};

public:
	// info messages
	struct CInfoMsg
	{
		EType m_Type;
		int m_Tick;

		int m_aVictimIds[MAX_KILLMSG_TEAM_MEMBERS];
		int m_VictimDDTeam;
		char m_aVictimName[64];
		STextContainerIndex m_VictimTextContainerIndex;
		float m_VictimTextWidth;
		CTeeRenderInfo m_aVictimRenderInfo[MAX_KILLMSG_TEAM_MEMBERS];
		int m_KillerID;
		char m_aKillerName[64];
		STextContainerIndex m_KillerTextContainerIndex;
		float m_KillerTextWidth;
		CTeeRenderInfo m_KillerRenderInfo;

		// kill msg
		int m_Weapon;
		int m_ModeSpecial; // for CTF, if the guy is carrying a flag for example
		int m_FlagCarrierBlue;
		int m_TeamSize;

		// finish msg
		int m_Diff;
		char m_aTimeText[32];
		char m_aDiffText[32];
		STextContainerIndex m_TimeTextContainerIndex;
		STextContainerIndex m_DiffTextContainerIndex;
		float m_TimeTextWidth;
		float m_DiffTextWidth;
		bool m_RecordPersonal;
	};

private:
	void AddInfoMsg(EType Type, CInfoMsg NewMsg);
	void RenderKillMsg(CInfoMsg *pInfoMsg, float x, float y);
	void RenderFinishMsg(CInfoMsg *pInfoMsg, float x, float y);

	void CreateNamesIfNotCreated(CInfoMsg *pInfoMsg);
	void CreateFinishTextContainersIfNotCreated(CInfoMsg *pInfoMsg);

	void DeleteTextContainers(CInfoMsg *pInfoMsg);

public:
	CInfoMsg m_aInfoMsgs[MAX_INFOMSGS];
	int m_InfoMsgCurrent;

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnWindowResize() override;
	virtual void OnRefreshSkins() override;
	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual void OnInit() override;
};

#endif
