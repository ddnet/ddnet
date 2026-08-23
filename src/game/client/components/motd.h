/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MOTD_H
#define GAME_CLIENT_COMPONENTS_MOTD_H

#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

#include <chrono>
#include <optional>

class CMotd : public CComponent
{
	char m_aServerMotd[std::size(g_Config.m_SvMotd)];
	int64_t m_ServerMotdTime;
	int64_t m_ServerMotdUpdateTime;
	std::chrono::nanoseconds m_ShownSince{0}; // when the MOTD last became visible
	int m_RectQuadContainer = -1;
	STextContainerIndex m_TextContainerIndex;
	std::optional<CUIRect> m_TouchRect; // in normalized screen coordinates like the touch finger positions
	std::optional<IInput::CTouchFinger> m_DismissTouchFinger; // the finger that dismissed the MOTD, ignored until released

public:
	CMotd();
	int Sizeof() const override { return sizeof(*this); }

	const char *ServerMotd() const { return m_aServerMotd; }
	int64_t ServerMotdUpdateTime() const { return m_ServerMotdUpdateTime; }
	void Clear();
	bool IsActive() const;

	void OnRender() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnWindowResize() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	bool OnInput(const IInput::CEvent &Event) override;
	bool OnTouchState(std::vector<IInput::CTouchFingerState> &vTouchFingerStates) override;
};

#endif
