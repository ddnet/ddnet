#ifndef GAME_CLIENT_COMPONENTS_STATBOARD_H
#define GAME_CLIENT_COMPONENTS_STATBOARD_H

#include <game/client/component.h>

class CStatboard : public CComponent
{
private:
	bool m_Active;
	bool m_ScreenshotTaken;
	int64_t m_ScreenshotTime;
	static void ConKeyStats(IConsole::IResult *pResult, void *pUserData);
	void RenderGlobalStats();
	void AutoStatScreenshot();
	void AutoStatCSV();

	char *m_pCSVstr;
	char *ReplaceCommata(char *pStr);
	void FormatStats();

public:
	CStatboard();
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnReset() override;
	virtual void OnConsoleInit() override;
	virtual void OnRender() override;
	virtual void OnRelease() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	bool IsActive();
};

#endif // GAME_CLIENT_COMPONENTS_STATBOARD_H
