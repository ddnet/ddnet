/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_IMPORTANT_ALERT_H
#define GAME_CLIENT_COMPONENTS_IMPORTANT_ALERT_H

#include <engine/textrender.h>

#include <game/client/component.h>

class CImportantAlert : public CComponent
{
	float m_ShowUntilTick;
	char m_aTitleText[128];
	char m_aMessageText[1024];
	STextContainerIndex m_TitleTextContainerIndex;
	STextContainerIndex m_MessageTextContainerIndex;

	void DeleteTextContainers();
	void RenderImportantAlert();
	void DoImportantAlert(int Type, const char *pMessage);
	float SecondsRemaining() const;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnWindowResize() override;
	void OnRender() override;
	void OnMessage(int MsgType, void *pRawMsg) override;

	bool IsActive() const;
};

#endif
