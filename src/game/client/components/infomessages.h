/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_INFOMESSAGES_H
#define GAME_CLIENT_COMPONENTS_INFOMESSAGES_H
#include <engine/textrender.h>
#include <game/client/component.h>

#include <game/client/render.h>
class CInfoMessages : public CComponent
{
	int m_SpriteQuadContainerIndex = -1;
	int m_QuadOffsetRaceFlag = -1;

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

	struct CInfoMsg
	{
		EType m_Type;
		int m_Tick;

		int m_aVictimIds[MAX_KILLMSG_TEAM_MEMBERS];
		int m_VictimDDTeam;
		char m_aVictimName[64];
		STextContainerIndex m_VictimTextContainerIndex;
		CTeeRenderInfo m_aVictimRenderInfo[MAX_KILLMSG_TEAM_MEMBERS];
		int m_KillerId;
		char m_aKillerName[64];
		STextContainerIndex m_KillerTextContainerIndex;
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
		bool m_RecordPersonal;
	};

	CInfoMsg m_aInfoMsgs[MAX_INFOMSGS];
	int m_InfoMsgCurrent;

	CInfoMsg CreateInfoMsg(EType Type);
	void AddInfoMsg(const CInfoMsg &InfoMsg);
	void RenderKillMsg(const CInfoMsg &InfoMsg, float x, float y);
	void RenderFinishMsg(const CInfoMsg &InfoMsg, float x, float y);

	void OnTeamKillMessage(const struct CNetMsg_Sv_KillMsgTeam *pMsg);
	void OnKillMessage(const struct CNetMsg_Sv_KillMsg *pMsg);
	void OnRaceFinishMessage(const struct CNetMsg_Sv_RaceFinish *pMsg);

	void CreateTextContainersIfNotCreated(CInfoMsg &InfoMsg);
	void DeleteTextContainers(CInfoMsg &InfoMsg);

public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnWindowResize() override;
	virtual void OnRefreshSkins() override;
	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual void OnInit() override;
};

#endif
