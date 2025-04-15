/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SCOREBOARD_H
#define GAME_CLIENT_COMPONENTS_SCOREBOARD_H

#include "engine/client.h"

#include <engine/console.h>
#include <engine/graphics.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

class CGameClient;
class CScoreboard : public CComponent
{

	struct SPopupProperties
	{
		static constexpr float ms_Width = 220.0f;

		static constexpr float ms_HeadlineFontSize = 24.0f;
		static constexpr float ms_FontSize = 24.0f;
		static constexpr float ms_IconFontSize = 33.0f;
		static constexpr float ms_Padding = 20.0f;
		static constexpr float ms_Rounding = 10.0f;

		static constexpr float ms_ItemSpacing = 5.0f;
		static constexpr float ms_GroupSpacing = 15.0f;

		static constexpr float ms_QuickActionsHeight = 50.0f;
		static constexpr float ms_ButtonHeight = 35.0f;

		static ColorRGBA WindowColor() { return ColorRGBA(0.451f, 0.451f, 0.451f, 0.9f); };
		static ColorRGBA GeneralButtonColor() { return ColorRGBA(0.541f, 0.561f, 0.48f, 0.8f); };
		static ColorRGBA GeneralActiveButtonColor() { return ColorRGBA(0.53f, 0.78f, 0.53f, 0.8f); };

		static ColorRGBA ActionGeneralButtonColor() { return ColorRGBA(0.541f, 0.561f, 0.48f, 0.8f); };
		static ColorRGBA ActionActiveButtonColor() { return ColorRGBA(0.53f, 0.78f, 0.53f, 0.8f); };
		static ColorRGBA ActionAltActiveButtonColor() { return ColorRGBA(1.0f, 0.42f, 0.42f, 0.8f); };

		static ColorRGBA TeamsGeneralButtonColor() { return ColorRGBA(0.32f, 0.32f, 0.72f, 0.8f); };
		static ColorRGBA TeamsActiveButtonColor() { return ColorRGBA(0.31f, 0.52f, 0.78f, 0.8f); };

		static ColorRGBA ActionSpecButtonColor() { return ColorRGBA(0.2f, 1.0f, 0.2f, 0.8f); }; // Bright green color for spec

	};

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
	void RenderTooltip(const char *pText, float x, float y);
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

	struct STooltip
	{
		const char *m_pText = nullptr;
		vec2 m_Pos = vec2(0.0f, 0.0f);

		void Clear()
		{
			m_pText = nullptr;
		}
	};

	STooltip m_Tooltip;

	struct SMouseState
	{
		bool m_Unlocked = false;
		bool m_Clicked = false;
		bool m_LastMouseInput = false;
		bool m_MouseInput = false;
		vec2 m_Position{0, 0};
		float m_LastClickTime = 0.0f;
		float m_ClickCooldown = 0.1f; // 100ms
		bool m_IsDragging = false;
		vec2 m_DragStart{0, 0};

		void reset()
		{
			m_Unlocked = false;
			m_Clicked = false;
			m_LastMouseInput = false;
			m_MouseInput = false;
			m_IsDragging = false;
			m_LastClickTime = 0.0f;
		}

		void clampPosition(float ScreenWidth, float ScreenHeight)
		{
			m_Position.x = clamp(m_Position.x, 0.0f, ScreenWidth - 1.0f);
			m_Position.y = clamp(m_Position.y, 0.0f, ScreenHeight - 1.0f);
		}

		bool canClick(IClient *pClient) const
		{
			return m_Unlocked && !m_IsDragging && (pClient->LocalTime() - m_LastClickTime) > m_ClickCooldown;
		}
	} m_Mouse;

	struct SPlayerPopup
	{
		bool m_Visible = false;
		CUIRect m_Rect;
		vec2 m_Position{0, 0};
		int m_PlayerId = -1;
		float m_LastButtonPressTime = 0.0f;
		float m_ButtonCooldown = 0.2f; // 200ms cooldown between button presses
		bool m_IsInteracting = false;

		void reset()
		{
			m_Visible = false;
			m_Position = {0, 0};
			m_PlayerId = -1;
			m_LastButtonPressTime = 0.0f;
			m_IsInteracting = false;
		}

		void toggle(bool Show, vec2 Pos = {0, 0}, int Id = -1)
		{
			m_Visible = Show;
			if(Show)
			{
				m_Position = Pos;
				m_PlayerId = Id;
				m_IsInteracting = false;
			}
		}

		bool shouldHide(const SMouseState &Mouse, bool PlayerHovered) const
		{
			return (!PlayerHovered && Mouse.m_Clicked && !m_IsInteracting) ||
			       !Mouse.m_Unlocked;
		}

		bool canInteract(IClient *pClient) const
		{
			return m_Visible && (pClient->LocalTime() - m_LastButtonPressTime) > m_ButtonCooldown;
		}
	} m_Popup;

	bool DoButtonLogic(const CUIRect *pRect)
	{
		if(!m_Mouse.canClick(Client()))
			return false;

		bool Hovered = pRect->Inside(m_Mouse.m_Position);
		bool Clicked = Hovered && m_Mouse.m_Clicked;

		if(Clicked)
		{
			m_Mouse.m_LastClickTime = Client()->LocalTime();
			m_Popup.m_LastButtonPressTime = Client()->LocalTime();
			m_Popup.m_IsInteracting = true;
		}

		return Clicked;
	}

	bool Hovered(const CUIRect *pRect) const
	{
		return m_Mouse.m_Unlocked && pRect->Inside(m_Mouse.m_Position);
	}

	void DoIconLabeledButton(CUIRect *pRect, const char *pTitle, const char *pIcon, float TextSize, float Height, ColorRGBA IconColor) const;
	void DoIconButton(CUIRect *pRect, const char *pIcon, float TextSize, ColorRGBA IconColor) const;

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
	virtual bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	virtual bool OnInput(const IInput::CEvent &Event) override;

	bool IsActive() const;
};

#endif
