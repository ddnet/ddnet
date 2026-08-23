/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_IMPORTANT_ALERT_H
#define GAME_CLIENT_COMPONENTS_IMPORTANT_ALERT_H

#include <engine/textrender.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

#include <chrono>
#include <optional>

class CImportantAlert : public CComponent
{
	bool m_Active = false;
	float m_ActiveSince;
	std::chrono::nanoseconds m_ActiveSinceNanos{0};
	float m_FadeOutSince;
	char m_aTitleText[128];
	char m_aMessageText[1024];
	STextContainerIndex m_TitleTextContainerIndex;
	STextContainerIndex m_MessageTextContainerIndex;
	STextContainerIndex m_CloseHintTextContainerIndex;
	bool m_CloseHintShownForTouch = false; // whether the close hint text was created for touch controls
	std::optional<CUIRect> m_DismissTouchRect; // in normalized screen coordinates like the touch finger positions
	std::optional<IInput::CTouchFinger> m_DismissTouchFinger; // the finger that dismissed the alert, ignored until released

	void DeleteTextContainers();
	void RenderImportantAlert();
	void DoImportantAlert(const char *pTitle, const char *pLogGroup, const char *pMessage);
	float SecondsActive() const;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnWindowResize() override;
	void OnRender() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	bool OnInput(const IInput::CEvent &Event) override;
	bool OnTouchState(std::vector<IInput::CTouchFingerState> &vTouchFingerStates) override;

	bool IsActive() const;
};

#endif
