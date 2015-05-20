#include <game/client/component.h>

enum {
	DETAILED_STATS_FRAGS=1,
	DETAILED_STATS_DEATHS=2,
	DETAILED_STATS_SUICIDES=4,
	DETAILED_STATS_RATIO=8,
	DETAILED_STATS_NET=16,
	DETAILED_STATS_FPM=32,
	DETAILED_STATS_SPREE=64,
	DETAILED_STATS_BESTSPREE=128,
	DETAILED_STATS_FLAGGRABS=256,
	DETAILED_STATS_WEAPS=512,
	DETAILED_STATS_FLAGCAPTURES=1024,
};

class CDetailedStats: public CComponent
{
private:
	bool m_Active;
	bool m_ScreenshotTaken;
	int64 m_ScreenshotTime;
	static void ConKeyStats(IConsole::IResult *pResult, void *pUserData);
	void RenderGlobalStats();
	void AutoStatScreenshot();

public:
	CDetailedStats();
	virtual void OnReset();
	virtual void OnConsoleInit();
	virtual void OnRender();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	bool IsActive();
};
