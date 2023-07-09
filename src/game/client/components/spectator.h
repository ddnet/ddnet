/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SPECTATOR_H
#define GAME_CLIENT_COMPONENTS_SPECTATOR_H
#include <base/vmath.h>

#include <game/client/component.h>

class CSpectator : public CComponent
{
	enum
	{
		MULTI_VIEW = -4,
		NO_SELECTION = -3,
	};

	bool m_Active;
	bool m_WasActive;
	bool m_Clicked;

	int m_SelectedSpectatorID;
	vec2 m_SelectorMouse;

	float m_OldMouseX;
	float m_OldMouseY;

	float m_MultiViewActivateDelay;

	bool CanChangeSpectator();
	void SpectateNext(bool Reverse);

	static void ConKeySpectator(IConsole::IResult *pResult, void *pUserData);
	static void ConSpectate(IConsole::IResult *pResult, void *pUserData);
	static void ConSpectateNext(IConsole::IResult *pResult, void *pUserData);
	static void ConSpectatePrevious(IConsole::IResult *pResult, void *pUserData);
	static void ConSpectateClosest(IConsole::IResult *pResult, void *pUserData);
	static void ConMultiView(IConsole::IResult *pResult, void *pUserData);

public:
	CSpectator();
	virtual int Sizeof() const override { return sizeof(*this); }

	virtual void OnConsoleInit() override;
	virtual bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	virtual void OnRender() override;
	virtual void OnRelease() override;
	virtual void OnReset() override;
	virtual bool OnInput(const IInput::CEvent &Event) override;

	void Spectate(int SpectatorID);
	void SpectateClosest();
};

#endif
