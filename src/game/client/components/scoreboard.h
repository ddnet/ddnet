/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SCOREBOARD_H
#define GAME_CLIENT_COMPONENTS_SCOREBOARD_H

#include <engine/console.h>
#include <engine/graphics.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

class CGameClient;

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
	bool RenderScoreboard(CUIRect Scoreboard, int Team, int CountStart, int CountEnd, CScoreboardRenderState &State, bool playerHovered = false);
	void RenderRecordingNotification(float x);

	// popup
	float CalculatePopupHeight();
	void RenderPlayerPopUp();
	void RenderQuickActions(CUIRect *pBase);
	void RenderGeneralActions(CUIRect *pBase);
	void RenderTeamActions(CUIRect *pBase);

	static void ConKeyScoreboard(IConsole::IResult *pResult, void *pUserData);
	const char *GetTeamName(int Team) const;

	bool m_Active;
	float m_ServerRecord;

	IGraphics::CTextureHandle m_DeadTeeTexture;

	bool DoButtonLogic(const CUIRect *pRect) const
	{
		return Hovered(pRect) && m_Mouse.m_Clicked;
	}
	bool Hovered(const CUIRect *pRect) const
	{
		return m_Mouse.m_Unlocked && pRect->Inside(m_Mouse.m_Position);
	}

	void DoIconLabeledButton(CUIRect *pRect, const char *pTitle, const char *pIcon, float TextSize, float Height, ColorRGBA IconColor) const;
	void DoIconButton(CUIRect *pRect, const char *pIcon, float TextSize, ColorRGBA IconColor) const;

	struct SMouseState
	{
		bool m_Unlocked = false;
		bool m_Clicked = false;
		bool m_LastMouseInput = false;
		bool m_MouseInput = false;
		vec2 m_Position{0, 0};

		void reset()
		{
			m_Unlocked = false;
			m_Clicked = false;
			m_LastMouseInput = false;
			m_MouseInput = false;
		}

		void clampPosition(float ScreenWidth, float ScreenHeight)
		{
			m_Position.x = clamp(m_Position.x, 0.0f, ScreenWidth - 1.0f);
			m_Position.y = clamp(m_Position.y, 0.0f, ScreenHeight - 1.0f);
		}
	} m_Mouse;

	struct SPlayerPopup
	{
		bool m_Visible = false;
		CUIRect m_Rect;
		vec2 m_Position{0, 0};
		int m_PlayerId = -1;
		float m_LastButtonPressTime = 0.0f;

		void reset()
		{
			m_Visible = false;
			m_Position = {0, 0};
			m_PlayerId = -1;
			m_LastButtonPressTime = 0.0f;
		}

		void toggle(bool Show, vec2 Pos = {0, 0}, int Id = -1);
		bool shouldHide(const SMouseState &Mouse, bool PlayerHovered) const;
	} m_Popup;

public:
	CScoreboard();
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnConsoleInit() override;
	virtual void OnInit() override;
	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual void OnRelease() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	virtual bool OnInput(const IInput::CEvent &Event) override;

	bool IsActive() const;
};

#endif
