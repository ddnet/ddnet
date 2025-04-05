/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SPECTATOR_H
#define GAME_CLIENT_COMPONENTS_SPECTATOR_H
#include <base/vmath.h>
#include <engine/console.h>

#include <game/client/component.h>
#include <game/client/ui.h>

class CSpectator : public CComponent
{
	enum
	{
		MULTI_VIEW = -4,
		NO_SELECTION = -3,
	};

	bool m_Active;
	bool m_WasActive;

	int m_SelectedSpectatorId;
	vec2 m_SelectorMouse;

	CUi::CTouchState m_TouchState;

	float m_MultiViewActivateDelay;

	bool CanChangeSpectatorId();
	void SpectateNext(bool Reverse);

	static void ConKeySpectator(IConsole::IResult *pResult, void *pUserData);
	static void ConSpectate(IConsole::IResult *pResult, void *pUserData);
	static void ConSpectateNext(IConsole::IResult *pResult, void *pUserData);
	static void ConSpectatePrevious(IConsole::IResult *pResult, void *pUserData);
	static void ConSpectateClosest(IConsole::IResult *pResult, void *pUserData);
	static void ConMultiView(IConsole::IResult *pResult, void *pUserData);

public:
	CSpectator();
	int Sizeof() const override { return sizeof(*this); }

	void OnConsoleInit() override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	bool OnInput(const IInput::CEvent &Event) override;
	void OnRender() override;
	void OnRelease() override;
	void OnReset() override;

	void Spectate(int SpectatorId);
	void SpectateClosest();

	bool IsActive() const { return m_Active; }
};

#endif
