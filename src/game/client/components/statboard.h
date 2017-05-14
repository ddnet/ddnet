#include <game/client/component.h>

class CStatboard: public CComponent
{
	private:
		bool m_Active;
		bool m_ScreenshotTaken;
		int64 m_ScreenshotTime;
		static void ConKeyStats(IConsole::IResult *pResult, void *pUserData);
		void RenderGlobalStats();
		void AutoStatScreenshot();
		void AutoStatCSV();

		char* m_pCSVstr;
		char* ReplaceCommata(char* pStr);
		void FormatStats();

	public:
		CStatboard();
		virtual void OnReset();
		virtual void OnConsoleInit();
		virtual void OnRender();
		virtual void OnRelease();
		virtual void OnMessage(int MsgType, void *pRawMsg);
		bool IsActive();
};
