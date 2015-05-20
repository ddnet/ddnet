#include <game/client/component.h>

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
