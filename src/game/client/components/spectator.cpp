/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "spectator.h"

#include "camera.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <limits>

bool CSpectator::CanChangeSpectatorId()
{
	// don't change SpectatorId when not spectating
	if(!GameClient()->m_Snap.m_SpecInfo.m_Active)
		return false;

	// stop follow mode from changing SpectatorId
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK && GameClient()->m_DemoSpecId == SPEC_FOLLOW)
		return false;

	return true;
}

void CSpectator::SpectateNext(bool Reverse)
{
	int CurIndex = -1;
	const CNetObj_PlayerInfo **paPlayerInfos = GameClient()->m_Snap.m_apInfoByDDTeamName;

	// m_SpectatorId may be uninitialized if m_Active is false
	if(GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(paPlayerInfos[i] && paPlayerInfos[i]->m_ClientId == GameClient()->m_Snap.m_SpecInfo.m_SpectatorId)
			{
				CurIndex = i;
				break;
			}
		}
	}

	int Start;
	if(CurIndex != -1)
	{
		if(Reverse)
			Start = CurIndex - 1;
		else
			Start = CurIndex + 1;
	}
	else
	{
		if(Reverse)
			Start = -1;
		else
			Start = 0;
	}

	int Increment = Reverse ? -1 : 1;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		int PlayerIndex = (Start + i * Increment) % MAX_CLIENTS;
		// % in C++ takes the sign of the dividend, not divisor
		if(PlayerIndex < 0)
			PlayerIndex += MAX_CLIENTS;

		const CNetObj_PlayerInfo *pPlayerInfo = paPlayerInfos[PlayerIndex];
		if(pPlayerInfo && pPlayerInfo->m_Team != TEAM_SPECTATORS)
		{
			Spectate(pPlayerInfo->m_ClientId);
			break;
		}
	}
}

void CSpectator::ConKeySpectator(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;

	if(pSelf->GameClient()->m_Scoreboard.IsActive())
		return;

	if(pSelf->GameClient()->m_Snap.m_SpecInfo.m_Active || pSelf->Client()->State() == IClient::STATE_DEMOPLAYBACK)
		pSelf->m_Active = pResult->GetInteger(0) != 0;
	else
		pSelf->m_Active = false;
}

void CSpectator::ConSpectate(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	if(!pSelf->CanChangeSpectatorId())
		return;

	pSelf->Spectate(pResult->GetInteger(0));
}

void CSpectator::ConSpectateNext(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	if(!pSelf->CanChangeSpectatorId())
		return;

	pSelf->SpectateNext(false);
}

void CSpectator::ConSpectatePrevious(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	if(!pSelf->CanChangeSpectatorId())
		return;

	pSelf->SpectateNext(true);
}

void CSpectator::ConSpectateClosest(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	pSelf->SpectateClosest();
}

void CSpectator::ConMultiView(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	int Input = pResult->GetInteger(0);

	if(Input == -1)
		std::fill(std::begin(pSelf->GameClient()->m_aMultiViewId), std::end(pSelf->GameClient()->m_aMultiViewId), false); // remove everyone from multiview
	else if(Input < MAX_CLIENTS && Input >= 0)
		pSelf->GameClient()->m_aMultiViewId[Input] = !pSelf->GameClient()->m_aMultiViewId[Input]; // activate or deactivate one player from multiview
}

CSpectator::CSpectator()
{
	m_SelectorMouse = vec2(0.0f, 0.0f);
	m_SelectorPage = 0;
	CSpectator::OnReset();
}

void CSpectator::OnConsoleInit()
{
	Console()->Register("+spectate", "", CFGFLAG_CLIENT, ConKeySpectator, this, "Open spectator mode selector");
	Console()->Register("spectate", "i[spectator-id]", CFGFLAG_CLIENT, ConSpectate, this, "Switch spectator mode");
	Console()->Register("spectate_next", "", CFGFLAG_CLIENT, ConSpectateNext, this, "Spectate the next player");
	Console()->Register("spectate_previous", "", CFGFLAG_CLIENT, ConSpectatePrevious, this, "Spectate the previous player");
	Console()->Register("spectate_closest", "", CFGFLAG_CLIENT, ConSpectateClosest, this, "Spectate the closest player");
	Console()->Register("spectate_multiview", "i[id]", CFGFLAG_CLIENT, ConMultiView, this, "Add/remove Client-IDs to spectate them exclusively (-1 to reset)");
}

bool CSpectator::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Active)
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	m_SelectorMouse += vec2(x, y);
	return true;
}

bool CSpectator::OnInput(const IInput::CEvent &Event)
{
	if(IsActive() && Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		OnRelease();
		return true;
	}

	if(g_Config.m_ClSpectatorMouseclicks)
	{
		if(GameClient()->m_Snap.m_SpecInfo.m_Active && !IsActive() && !GameClient()->m_MultiViewActivated &&
			!Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive() && !GameClient()->m_Menus.IsActive())
		{
			if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_MOUSE_1)
			{
				if(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW)
					Spectate(SPEC_FREEVIEW);
				else
					SpectateClosest();
				return true;
			}
		}
	}

	if(GameClient()->m_Camera.SpectatingPlayer() && GameClient()->m_Camera.CanUseAutoSpecCamera())
	{
		if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_MOUSE_2)
		{
			GameClient()->m_Camera.ResetAutoSpecCamera();
			return true;
		}
	}

	return false;
}

void CSpectator::OnRelease()
{
	OnReset();
}

void CSpectator::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!GameClient()->m_MultiViewActivated && m_MultiViewActivateDelay != 0.0f)
	{
		if(m_MultiViewActivateDelay <= Client()->LocalTime())
		{
			m_MultiViewActivateDelay = 0.0f;
			GameClient()->m_MultiViewActivated = true;
		}
	}

	if(!m_Active)
	{
		// closing the spectator menu
		if(m_WasActive)
		{
			// apply page change if the page button was selected
			if(m_SelectedSpectatorId == PAGE_CYCLE)
			{
				int TotalPlayers = 0;
				for(const auto &pInfo : GameClient()->m_Snap.m_apInfoByDDTeamName)
				{
					if(pInfo && pInfo->m_Team != TEAM_SPECTATORS)
						++TotalPlayers;
				}
				int NumPages = std::max(1, (TotalPlayers + 127) / 128);
				m_SelectorPage = (m_SelectorPage + 1) % NumPages;
			}
			else if(m_SelectedSpectatorId != NO_SELECTION)
			{
				if(m_SelectedSpectatorId == MULTI_VIEW)
					GameClient()->m_MultiViewActivated = true;
				else if(m_SelectedSpectatorId == SPEC_FREEVIEW || m_SelectedSpectatorId == SPEC_FOLLOW)
					GameClient()->m_MultiViewActivated = false;

				if(!GameClient()->m_MultiViewActivated)
					Spectate(m_SelectedSpectatorId);

				if(GameClient()->m_MultiViewActivated && m_SelectedSpectatorId != MULTI_VIEW && GameClient()->m_Teams.Team(m_SelectedSpectatorId) != GameClient()->m_MultiViewTeam)
				{
					GameClient()->ResetMultiView();
					Spectate(m_SelectedSpectatorId);
					m_MultiViewActivateDelay = Client()->LocalTime() + 0.3f;
				}
			}
			m_WasActive = false;
		}
		return;
	}

	if(!GameClient()->m_Snap.m_SpecInfo.m_Active && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		m_Active = false;
		m_WasActive = false;
		return;
	}

	m_WasActive = true;
	m_SelectedSpectatorId = NO_SELECTION;

	// draw background
	float Width = 400 * 3.0f * Graphics()->ScreenAspect();
	float Height = 400 * 3.0f;
	float ObjWidth = 300.0f;
	float FontSize = 20.0f;
	float BigFontSize = 20.0f;
	float StartY = -190.0f;
	float LineHeight = 60.0f;
	float TeeSizeMod = 1.0f;
	float RoundRadius = 30.0f;
	int TotalPlayers = 0;
	int PerLine = 8;
	float BoxMove = -10.0f;
	float BoxOffset = 0.0f;

	// Count total players (non-spectators)
	for(const auto &pInfo : GameClient()->m_Snap.m_apInfoByDDTeamName)
	{
		if(!pInfo || pInfo->m_Team == TEAM_SPECTATORS)
			continue;

		++TotalPlayers;
	}

	// Page calculation
	const int PlayersPerPage = 128;
	int NumPages = std::max(1, (TotalPlayers + PlayersPerPage - 1) / PlayersPerPage);
	if(NumPages <= 1)
	{
		m_SelectorPage = 0;
	}
	else
	{
		m_SelectorPage = std::clamp(m_SelectorPage, 0, NumPages - 1);
		// Explicitly keep width on other pages, akin to scoreboard. This makes UX nicer, as the window doesn't resize while cycling pages
		ObjWidth = 600.f;
	}

	int PageStart = m_SelectorPage * PlayersPerPage;
	int PageEnd = std::min(PageStart + PlayersPerPage, TotalPlayers);
	int PlayersOnPage = PageEnd - PageStart;

	// Layout based only on players on the current page
	if(PlayersOnPage > 96)
	{
		FontSize = 15.0f;
		LineHeight = 15.0f;
		TeeSizeMod = 0.3f;
		PerLine = 32;
		RoundRadius = 5.0f;
		BoxMove = 3.0f;
		BoxOffset = 6.0f;
	}
	else if(PlayersOnPage > 64)
	{
		FontSize = 16.0f;
		LineHeight = 19.0f;
		TeeSizeMod = 0.45f;
		PerLine = 24;
		RoundRadius = 6.0f;
		BoxMove = 3.0f;
		BoxOffset = 6.0f;
	}
	else if(PlayersOnPage > 32)
	{
		FontSize = 18.0f;
		LineHeight = 30.0f;
		TeeSizeMod = 0.7f;
		PerLine = 16;
		RoundRadius = 10.0f;
		BoxMove = 3.0f;
		BoxOffset = 6.0f;
	}
	if(PlayersOnPage > 16)
	{
		ObjWidth = 600.0f;
	}

	const vec2 ScreenSize = vec2(Width, Height);
	const vec2 ScreenCenter = ScreenSize / 2.0f;
	CUIRect SpectatorRect = {Width / 2.0f - ObjWidth, Height / 2.0f - 300.0f, ObjWidth * 2.0f, 600.0f};
	CUIRect SpectatorMouseRect;
	SpectatorRect.Margin(20.0f, &SpectatorMouseRect);

	const bool WasTouchPressed = m_TouchState.m_AnyPressed;
	Ui()->UpdateTouchState(m_TouchState);
	if(m_TouchState.m_AnyPressed)
	{
		const vec2 TouchPos = (m_TouchState.m_PrimaryPosition - vec2(0.5f, 0.5f)) * ScreenSize;
		if(SpectatorMouseRect.Inside(ScreenCenter + TouchPos))
		{
			m_SelectorMouse = TouchPos;
		}
	}
	else if(WasTouchPressed)
	{
		const vec2 TouchPos = (m_TouchState.m_PrimaryPosition - vec2(0.5f, 0.5f)) * ScreenSize;
		if(!SpectatorRect.Inside(ScreenCenter + TouchPos))
		{
			OnRelease();
			return;
		}
	}

	Graphics()->MapScreenToSize(Width, Height);

	SpectatorRect.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.3f), IGraphics::CORNER_ALL, 20.0f);

	// clamp mouse position to selector area
	m_SelectorMouse.x = std::clamp(m_SelectorMouse.x, -(ObjWidth - 20.0f), ObjWidth - 20.0f);
	m_SelectorMouse.y = std::clamp(m_SelectorMouse.y, -280.0f, 280.0f);

	const bool MousePressed = Input()->KeyPress(KEY_MOUSE_1) || m_TouchState.m_PrimaryPressed;

	// ---- Top bar buttons ----
	const float TopBarY = -280.0f;
	const float TopBarHeight = 60.0f;
	const float TopBarStartX = -(ObjWidth - 20.0f);
	const float TopBarTotalWidth = (ObjWidth * 2.0f) - 40.0f;
	const float Gap = 40.0f;

	const bool ShowFollow = Client()->State() == IClient::STATE_DEMOPLAYBACK && GameClient()->m_Snap.m_LocalClientId >= 0;
	const bool ShowPage = NumPages > 1;

	// Prepare page button label and width
	char aPageLabel[64];
	float PageButtonWidth = 0.0f;
	if(ShowPage)
	{
		if(m_SelectorPage == 0)
			str_format(aPageLabel, sizeof(aPageLabel), "⋅ %d more", std::max(0, TotalPlayers - PlayersPerPage));
		else if(m_SelectorPage == NumPages - 1 && NumPages - 1 == 1)
			str_copy(aPageLabel, "⋅ Back", sizeof(aPageLabel));
		else
		{
			str_format(aPageLabel, sizeof(aPageLabel), "⋅ %d/%d", m_SelectorPage, NumPages - 1);
			if(m_SelectorPage == NumPages - 1)
				str_append(aPageLabel, " | Back", sizeof(aPageLabel));
		}

		PageButtonWidth = TextRender()->TextWidth(BigFontSize, aPageLabel) + 40.0f;
		PageButtonWidth = std::min(PageButtonWidth, 200.0f); // limit maximum width
	}

	// Natural width for base buttons: text width + padding
	const float ButtonPadding = 40.0f;
	const char *pFreeViewText = Localize("Free-View");
	const char *pMultiViewText = Localize("Multi-View");
	const char *pFollowText = Localize("Follow");

	float FreeViewWidth = TextRender()->TextWidth(BigFontSize, pFreeViewText) + ButtonPadding;
	float MultiViewWidth = TextRender()->TextWidth(BigFontSize, pMultiViewText) + ButtonPadding;
	float FollowWidth = ShowFollow ? TextRender()->TextWidth(BigFontSize, pFollowText) + ButtonPadding : 0.0f;

	// Scale base buttons to fit available space, allowing moderate growth but also shrinking if needed
	const int BaseButtons = 2 + (ShowFollow ? 1 : 0);
	const float NaturalBaseWidth = FreeViewWidth + MultiViewWidth + FollowWidth;
	const float GapsBetweenBase = (BaseButtons - 1) * Gap;
	const float AvailableBaseWidth = TopBarTotalWidth - (ShowPage ? PageButtonWidth + Gap + 1.0f : 0.0f);

	const float MaxButtonScale = 1.3f; // adjust this to allow more growth (e.g. 1.5f)
	const float TotalBaseWidthNatural = NaturalBaseWidth + GapsBetweenBase;
	float Scale = std::min(MaxButtonScale, AvailableBaseWidth / TotalBaseWidthNatural);

	FreeViewWidth *= Scale;
	MultiViewWidth *= Scale;
	FollowWidth *= Scale;

	// Place base buttons left‑aligned
	CUIRect FreeViewRect = {Width / 2.0f + TopBarStartX, Height / 2.0f + TopBarY, FreeViewWidth, TopBarHeight};
	float CurX = TopBarStartX + FreeViewWidth + Gap;

	CUIRect MultiViewRect = {Width / 2.0f + CurX, Height / 2.0f + TopBarY, MultiViewWidth, TopBarHeight};
	CurX += MultiViewWidth + Gap;

	CUIRect FollowRect;
	if(ShowFollow)
	{
		FollowRect = {Width / 2.0f + CurX, Height / 2.0f + TopBarY, FollowWidth, TopBarHeight};
		CurX += FollowWidth + Gap;
	}

	// Page button at far right (shifted 1px left for reliable hover)
	CUIRect PageRect;
	if(ShowPage)
	{
		PageRect = {Width / 2.0f + TopBarStartX + TopBarTotalWidth - PageButtonWidth, Height / 2.0f + TopBarY, PageButtonWidth + 1.0f, TopBarHeight};
	}

	bool FreeViewSelected = false;
	bool MultiViewSelected = false;
	bool FollowSelected = false;
	bool PageSelected = false;

	if(FreeViewRect.Inside(ScreenCenter + m_SelectorMouse))
	{
		FreeViewSelected = true;
		m_SelectedSpectatorId = SPEC_FREEVIEW;
		if(MousePressed)
		{
			GameClient()->m_MultiViewActivated = false;
			Spectate(m_SelectedSpectatorId);
		}
	}

	if(MultiViewRect.Inside(ScreenCenter + m_SelectorMouse))
	{
		MultiViewSelected = true;
		m_SelectedSpectatorId = MULTI_VIEW;
		if(MousePressed)
		{
			GameClient()->m_MultiViewActivated = true;
		}
	}

	if(ShowFollow && FollowRect.Inside(ScreenCenter + m_SelectorMouse))
	{
		FollowSelected = true;
		m_SelectedSpectatorId = SPEC_FOLLOW;
		if(MousePressed)
		{
			GameClient()->m_MultiViewActivated = false;
			Spectate(m_SelectedSpectatorId);
		}
	}

	if(ShowPage && PageRect.Inside(ScreenCenter + m_SelectorMouse))
	{
		PageSelected = true;
		m_SelectedSpectatorId = PAGE_CYCLE;
		if(MousePressed)
		{
			// Immediately advance page; no double action on close
			m_SelectorPage = (m_SelectorPage + 1) % NumPages;
			m_SelectedSpectatorId = NO_SELECTION;
		}
	}

	bool FreeViewActive = (Client()->State() == IClient::STATE_DEMOPLAYBACK && GameClient()->m_DemoSpecId == SPEC_FREEVIEW) ||
			      (Client()->State() != IClient::STATE_DEMOPLAYBACK && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SPEC_FREEVIEW);
	bool MultiViewActive = GameClient()->m_MultiViewActivated;
	bool FollowActive = ShowFollow && Client()->State() == IClient::STATE_DEMOPLAYBACK && GameClient()->m_DemoSpecId == SPEC_FOLLOW;

	// Draw active highlights (original behavior)
	if(FreeViewActive)
		FreeViewRect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 20.0f);

	if(MultiViewActive)
		MultiViewRect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 20.0f);

	if(ShowFollow && FollowActive)
		FollowRect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 20.0f);

	// Draw page hover highlight
	if(ShowPage && PageSelected)
		PageRect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 20.0f);

	// Render text (no wrapping)
	auto RenderTopButtonText = [&](CUIRect Rect, const char *pText, bool Selected) {
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, Selected ? 1.0f : 0.5f);
		float TextWidth = TextRender()->TextWidth(BigFontSize, pText);
		const float NaturalWidth = TextWidth + 40.0f; // same padding as ButtonPadding
		float TextX;
		if(Rect.w >= NaturalWidth)
		{
			// enough space: left align with 20px margin
			TextX = Rect.x + 20.0f;
		}
		else
		{
			// not enough space: center text
			TextX = Rect.x + (Rect.w - TextWidth) / 2.0f;
		}
		float TextY = Rect.y + (Rect.h - BigFontSize) / 2.0f;
		TextRender()->Text(TextX, TextY, BigFontSize, pText, -1.0f);
	};

	RenderTopButtonText(FreeViewRect, pFreeViewText, FreeViewSelected);
	RenderTopButtonText(MultiViewRect, pMultiViewText, MultiViewSelected);

	if(ShowFollow)
		RenderTopButtonText(FollowRect, pFollowText, FollowSelected);

	if(ShowPage)
		RenderTopButtonText(PageRect, aPageLabel, PageSelected);

	// ---- Player grid ----
	float x = -(ObjWidth - 35.0f), y = StartY;

	int OldDDTeam = -1;
	int PlayersSkipped = 0;
	int Count = 0;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		const CNetObj_PlayerInfo *pInfo = GameClient()->m_Snap.m_apInfoByDDTeamName[i];

		if(!pInfo || pInfo->m_Team == TEAM_SPECTATORS)
			continue;

		if(PlayersSkipped < PageStart)
		{
			PlayersSkipped++;
			continue;
		}
		if(PlayersSkipped >= PageEnd)
			break;

		PlayersSkipped++;
		Count++;

		if(Count == PerLine + 1 || (Count > PerLine + 1 && (Count - 1) % PerLine == 0))
		{
			x += 290.0f;
			y = StartY;
		}

		int DDTeam = GameClient()->m_Teams.Team(pInfo->m_ClientId);
		int NextDDTeam = 0;

		for(int j = i + 1; j < MAX_CLIENTS; j++)
		{
			const CNetObj_PlayerInfo *pInfo2 = GameClient()->m_Snap.m_apInfoByDDTeamName[j];

			if(!pInfo2 || pInfo2->m_Team == TEAM_SPECTATORS)
				continue;

			NextDDTeam = GameClient()->m_Teams.Team(pInfo2->m_ClientId);
			break;
		}

		if(OldDDTeam == -1)
		{
			for(int j = i - 1; j >= 0; j--)
			{
				const CNetObj_PlayerInfo *pInfo2 = GameClient()->m_Snap.m_apInfoByDDTeamName[j];

				if(!pInfo2 || pInfo2->m_Team == TEAM_SPECTATORS)
					continue;

				OldDDTeam = GameClient()->m_Teams.Team(pInfo2->m_ClientId);
				break;
			}
		}

		if(DDTeam != TEAM_FLOCK)
		{
			const ColorRGBA Color = GameClient()->GetDDTeamColor(DDTeam).WithAlpha(0.5f);
			int Corners = 0;
			if(OldDDTeam != DDTeam)
				Corners |= IGraphics::CORNER_TL | IGraphics::CORNER_TR;
			if(NextDDTeam != DDTeam)
				Corners |= IGraphics::CORNER_BL | IGraphics::CORNER_BR;
			Graphics()->DrawRect(Width / 2.0f + x - 10.0f + BoxOffset, Height / 2.0f + y + BoxMove, 270.0f - BoxOffset, LineHeight, Color, Corners, RoundRadius);
		}

		OldDDTeam = DDTeam;

		if((Client()->State() == IClient::STATE_DEMOPLAYBACK && GameClient()->m_DemoSpecId == pInfo->m_ClientId) || (Client()->State() != IClient::STATE_DEMOPLAYBACK && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == pInfo->m_ClientId))
		{
			Graphics()->DrawRect(Width / 2.0f + x - 10.0f + BoxOffset, Height / 2.0f + y + BoxMove, 270.0f - BoxOffset, LineHeight, ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, RoundRadius);
		}

		bool PlayerSelected = false;
		if(m_SelectorMouse.x >= x - 10.0f && m_SelectorMouse.x < x + 260.0f &&
			m_SelectorMouse.y >= y - (LineHeight / 6.0f) && m_SelectorMouse.y < y + (LineHeight * 5.0f / 6.0f))
		{
			m_SelectedSpectatorId = pInfo->m_ClientId;
			PlayerSelected = true;
			if(MousePressed)
			{
				if(GameClient()->m_MultiViewActivated)
				{
					if(GameClient()->m_MultiViewTeam == DDTeam)
					{
						GameClient()->m_aMultiViewId[m_SelectedSpectatorId] = !GameClient()->m_aMultiViewId[m_SelectedSpectatorId];
						if(!GameClient()->m_aMultiViewId[GameClient()->m_Snap.m_SpecInfo.m_SpectatorId])
						{
							int NewClientId = GameClient()->FindFirstMultiViewId();
							if(NewClientId < MAX_CLIENTS && NewClientId >= 0)
							{
								GameClient()->CleanMultiViewId(NewClientId);
								GameClient()->m_aMultiViewId[NewClientId] = true;
								Spectate(NewClientId);
							}
						}
					}
					else
					{
						GameClient()->ResetMultiView();
						Spectate(m_SelectedSpectatorId);
						m_MultiViewActivateDelay = Client()->LocalTime() + 0.3f;
					}
				}
				else
				{
					Spectate(m_SelectedSpectatorId);
				}
			}
		}
		float TeeAlpha;
		if(Client()->State() == IClient::STATE_DEMOPLAYBACK &&
			!GameClient()->m_Snap.m_aCharacters[pInfo->m_ClientId].m_Active)
		{
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.25f);
			TeeAlpha = 0.5f;
		}
		else
		{
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, PlayerSelected ? 1.0f : 0.5f);
			TeeAlpha = 1.0f;
		}
		CTextCursor NameCursor;
		NameCursor.SetPosition(vec2(Width / 2.0f + x + 50.0f, Height / 2.0f + y + BoxMove + (LineHeight - FontSize) / 2.f));
		NameCursor.m_FontSize = FontSize;
		NameCursor.m_Flags |= TEXTFLAG_ELLIPSIS_AT_END;
		NameCursor.m_LineWidth = 180.0f;
		if(g_Config.m_ClShowIds)
		{
			char aClientId[16];
			GameClient()->FormatClientId(pInfo->m_ClientId, aClientId, EClientIdFormat::INDENT_AUTO);
			TextRender()->TextEx(&NameCursor, aClientId);
		}
		TextRender()->TextEx(&NameCursor, GameClient()->m_aClients[pInfo->m_ClientId].m_aName);

		if(GameClient()->m_MultiViewActivated)
		{
			if(GameClient()->m_aMultiViewId[pInfo->m_ClientId])
			{
				TextRender()->TextColor(0.1f, 1.0f, 0.1f, PlayerSelected ? 1.0f : 0.5f);
				TextRender()->Text(Width / 2.0f + x + 50.0f + 180.0f, Height / 2.0f + y + BoxMove + (LineHeight - FontSize) / 2.f, FontSize - 3, "⬤", 220.0f);
			}
			else if(GameClient()->m_MultiViewTeam == DDTeam)
			{
				TextRender()->TextColor(1.0f, 0.1f, 0.1f, PlayerSelected ? 1.0f : 0.5f);
				TextRender()->Text(Width / 2.0f + x + 50.0f + 180.0f, Height / 2.0f + y + BoxMove + (LineHeight - FontSize) / 2.f, FontSize - 3, "◯", 220.0f);
			}
		}

		// flag
		if(GameClient()->m_Snap.m_pGameInfoObj && (GameClient()->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS) &&
			GameClient()->m_Snap.m_pGameDataObj && (GameClient()->m_Snap.m_pGameDataObj->m_FlagCarrierRed == pInfo->m_ClientId || GameClient()->m_Snap.m_pGameDataObj->m_FlagCarrierBlue == pInfo->m_ClientId))
		{
			if(GameClient()->m_Snap.m_pGameDataObj->m_FlagCarrierBlue == pInfo->m_ClientId)
				Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagBlue);
			else
				Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagRed);

			Graphics()->QuadsBegin();
			Graphics()->QuadsSetSubset(1, 0, 0, 1);

			float Size = LineHeight;
			IGraphics::CQuadItem QuadItem(Width / 2.0f + x - LineHeight / 5.0f, Height / 2.0f + y - LineHeight / 3.0f, Size / 2.0f, Size);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		CTeeRenderInfo TeeInfo = GameClient()->m_aClients[pInfo->m_ClientId].m_RenderInfo;
		TeeInfo.m_Size *= TeeSizeMod;

		const CAnimState *pIdleState = CAnimState::GetIdle();
		vec2 OffsetToMid;
		CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
		vec2 TeeRenderPos(Width / 2.0f + x + 20.0f, Height / 2.0f + y + BoxMove + LineHeight / 2.0f + OffsetToMid.y);

		RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos, TeeAlpha);

		if(GameClient()->m_aClients[pInfo->m_ClientId].m_Friend)
		{
			TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageFriendColor)));
			TextRender()->Text(Width / 2.0f + x - TeeInfo.m_Size / 2.0f, Height / 2.0f + y + BoxMove + (LineHeight - FontSize) / 2.f, FontSize, "♥", 220.0f);
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
		}

		y += LineHeight;
	}
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

	RenderTools()->RenderCursor(ScreenCenter + m_SelectorMouse, 48.0f);
}

void CSpectator::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	m_SelectedSpectatorId = NO_SELECTION;
	m_SelectorPage = 0;
}

void CSpectator::Spectate(int SpectatorId)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		GameClient()->m_DemoSpecId = std::clamp(SpectatorId, (int)SPEC_FOLLOW, MAX_CLIENTS - 1);
		// The tick must be rendered for the spectator mode to be updated, so we do it manually when demo playback is paused
		// TODO: https://github.com/ddnet/ddnet/issues/11681
		if(DemoPlayer()->BaseInfo()->m_Paused)
			GameClient()->m_Menus.DemoSeekTick(IDemoPlayer::TICK_CURRENT);
		return;
	}

	if(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SpectatorId)
		return;

	if(Client()->IsSixup())
	{
		protocol7::CNetMsg_Cl_SetSpectatorMode Msg;
		if(SpectatorId == SPEC_FREEVIEW)
		{
			Msg.m_SpecMode = protocol7::SPEC_FREEVIEW;
			Msg.m_SpectatorId = -1;
		}
		else
		{
			Msg.m_SpecMode = protocol7::SPEC_PLAYER;
			Msg.m_SpectatorId = SpectatorId;
		}
		Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL, true);
		return;
	}
	CNetMsg_Cl_SetSpectatorMode Msg;
	Msg.m_SpectatorId = SpectatorId;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);
}

void CSpectator::SpectateClosest()
{
	if(!CanChangeSpectatorId())
		return;

	const CGameClient::CSnapState &Snap = GameClient()->m_Snap;
	int SpectatorId = Snap.m_SpecInfo.m_SpectatorId;

	int NewSpectatorId = -1;

	vec2 CurPosition = GameClient()->m_Camera.m_Center;
	if(SpectatorId != SPEC_FREEVIEW)
	{
		const CNetObj_Character &CurCharacter = Snap.m_aCharacters[SpectatorId].m_Cur;
		CurPosition = vec2(CurCharacter.m_X, CurCharacter.m_Y);
	}

	int ClosestDistance = std::numeric_limits<int>::max();
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(ClientId == SpectatorId || !Snap.m_aCharacters[ClientId].m_Active || !Snap.m_apPlayerInfos[ClientId] || Snap.m_apPlayerInfos[ClientId]->m_Team == TEAM_SPECTATORS)
			continue;

		if(Client()->State() != IClient::STATE_DEMOPLAYBACK && ClientId == Snap.m_LocalClientId)
			continue;

		const CNetObj_Character &MaybeClosestCharacter = Snap.m_aCharacters[ClientId].m_Cur;
		int Distance = distance(CurPosition, vec2(MaybeClosestCharacter.m_X, MaybeClosestCharacter.m_Y));
		if(NewSpectatorId == -1 || Distance < ClosestDistance)
		{
			NewSpectatorId = ClientId;
			ClosestDistance = Distance;
		}
	}
	if(NewSpectatorId > -1)
		Spectate(NewSpectatorId);
}
