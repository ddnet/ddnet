/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CONSOLE_H
#define GAME_CLIENT_COMPONENTS_CONSOLE_H
#include <engine/shared/ringbuffer.h>
#include <game/client/component.h>
#include <game/client/lineinput.h>

#include <engine/console.h>

#include <mutex>

enum
{
	CONSOLE_CLOSED,
	CONSOLE_OPENING,
	CONSOLE_OPEN,
	CONSOLE_CLOSING,
};

class CConsoleLogger;

class CGameConsole : public CComponent
{
	friend class CConsoleLogger;
	class CInstance
	{
	public:
		struct CBacklogEntry
		{
			float m_YOffset;
			ColorRGBA m_PrintColor;
			char m_aText[1];
		};
		std::mutex m_BacklogLock;
		CStaticRingBuffer<CBacklogEntry, 1024 * 1024, CRingBufferBase::FLAG_RECYCLE> m_Backlog;
		CStaticRingBuffer<char, 64 * 1024, CRingBufferBase::FLAG_RECYCLE> m_History;
		char *m_pHistoryEntry;

		CLineInput m_Input;
		const char *m_pName;
		int m_Type;
		int m_BacklogCurPage;

		CGameConsole *m_pGameConsole;

		char m_aCompletionBuffer[128];
		int m_CompletionChosen;
		char m_aCompletionBufferArgument[128];
		int m_CompletionChosenArgument;
		int m_CompletionFlagmask;
		float m_CompletionRenderOffset;
		float m_CompletionRenderOffsetChange;

		char m_aUser[32];
		bool m_UserGot;
		bool m_UsernameReq;

		bool m_IsCommand;
		char m_aCommandName[IConsole::TEMPCMD_NAME_LENGTH];
		char m_aCommandHelp[IConsole::TEMPCMD_HELP_LENGTH];
		char m_aCommandParams[IConsole::TEMPCMD_PARAMS_LENGTH];

		CInstance(int t);
		void Init(CGameConsole *pGameConsole);

		void ClearBacklog();
		void ClearBacklogYOffsets();
		void ClearHistory();
		void Reset();

		void ExecuteLine(const char *pLine);

		void OnInput(IInput::CEvent Event);
		void PrintLine(const char *pLine, int Len, ColorRGBA PrintColor);

		const char *GetString() const { return m_Input.GetString(); }
		static void PossibleCommandsCompleteCallback(int Index, const char *pStr, void *pUser);
		static void PossibleArgumentsCompleteCallback(int Index, const char *pStr, void *pUser);
	};

	class IConsole *m_pConsole;
	CConsoleLogger *m_pConsoleLogger = nullptr;

	CInstance m_LocalConsole;
	CInstance m_RemoteConsole;

	CInstance *CurrentConsole();
	float TimeNow();

	int m_ConsoleType;
	int m_ConsoleState;
	float m_StateChangeEnd;
	float m_StateChangeDuration;

	bool m_MouseIsPress = false;
	int m_MousePressX = 0;
	int m_MousePressY = 0;
	int m_MouseCurX = 0;
	int m_MouseCurY = 0;
	int m_CurSelStart = 0;
	int m_CurSelEnd = 0;
	bool m_HasSelection = false;
	int m_NewLineCounter = 0;

	int m_LastInputLineCount = 0;

	void Toggle(int Type);
	void Dump(int Type);

	static void PossibleCommandsRenderCallback(int Index, const char *pStr, void *pUser);
	static void ConToggleLocalConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConToggleRemoteConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConClearLocalConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConClearRemoteConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpLocalConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpRemoteConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConConsolePageUp(IConsole::IResult *pResult, void *pUserData);
	static void ConConsolePageDown(IConsole::IResult *pResult, void *pUserData);

public:
	enum
	{
		CONSOLETYPE_LOCAL = 0,
		CONSOLETYPE_REMOTE,
	};

	CGameConsole();
	~CGameConsole();
	virtual int Sizeof() const override { return sizeof(*this); }

	void PrintLine(int Type, const char *pLine);
	void RequireUsername(bool UsernameReq);

	virtual void OnStateChange(int NewState, int OldState) override;
	virtual void OnConsoleInit() override;
	virtual void OnInit() override;
	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual bool OnInput(IInput::CEvent Events) override;

	bool IsClosed() { return m_ConsoleState == CONSOLE_CLOSED; }
};
#endif
