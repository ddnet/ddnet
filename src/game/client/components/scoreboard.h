/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SCOREBOARD_H
#define GAME_CLIENT_COMPONENTS_SCOREBOARD_H

#include <engine/console.h>
#include <engine/graphics.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

class CScoreboard : public CComponent
{
	struct CScoreboardRenderState
	{
		float m_TeamStartX;
		float m_TeamStartY;
		int m_CurrentDDTeamSize;

		CScoreboardRenderState() :
			m_TeamStartX(0), m_TeamStartY(0), m_CurrentDDTeamSize(0) {}
	};

	void RenderTitle(CUIRect TitleBar, int Team, const char *pTitle);
	void RenderGoals(CUIRect Goals);
	void RenderSpectators(CUIRect Spectators);
	void RenderScoreboard(CUIRect Scoreboard, int Team, int CountStart, int CountEnd, CScoreboardRenderState &State);
	void RenderRecordingNotification(float x);

	static void ConKeyScoreboard(IConsole::IResult *pResult, void *pUserData);
	const char *GetTeamName(int Team) const;

	bool m_Active;
	float m_ServerRecord;

	IGraphics::CTextureHandle m_DeadTeeTexture;

public:
	CScoreboard();
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnConsoleInit() override;
	virtual void OnInit() override;
	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual void OnRelease() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;

	bool Active() const;
};

#endif
