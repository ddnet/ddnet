#include <string>
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
		std::string FormatStats();
		std::string ReplaceCommata(std::string str);

	public:
		CStatboard();
		virtual void OnReset();
		virtual void OnConsoleInit();
		virtual void OnRender();
		virtual void OnRelease();
		virtual void OnMessage(int MsgType, void *pRawMsg);
		bool IsActive();
};
