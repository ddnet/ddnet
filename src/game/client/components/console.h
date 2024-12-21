/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CONSOLE_H
#define GAME_CLIENT_COMPONENTS_CONSOLE_H

#include <base/lock.h>

#include <engine/console.h>
#include <engine/shared/ringbuffer.h>

#include <game/client/component.h>
#include <game/client/lineinput.h>
#include <game/client/ui.h>

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
			int m_LineCount;
			ColorRGBA m_PrintColor;
			size_t m_Length;
			char m_aText[1];
		};
		CStaticRingBuffer<CBacklogEntry, 1024 * 1024, CRingBufferBase::FLAG_RECYCLE> m_Backlog;
		CLock m_BacklogPendingLock;
		CStaticRingBuffer<CBacklogEntry, 1024 * 1024, CRingBufferBase::FLAG_RECYCLE> m_BacklogPending GUARDED_BY(m_BacklogPendingLock);
		CStaticRingBuffer<char, 64 * 1024, CRingBufferBase::FLAG_RECYCLE> m_History;
		char *m_pHistoryEntry;

		CLineInputBuffered<IConsole::CMDLINE_LENGTH> m_Input;
		const char *m_pName;
		int m_Type;
		int m_BacklogCurLine;
		int m_BacklogLastActiveLine = -1;
		int m_LinesRendered;

		STextBoundingBox m_BoundingBox = {0.0f, 0.0f, 0.0f, 0.0f};
		float m_LastInputHeight = 0.0f;

		bool m_MouseIsPress = false;
		vec2 m_MousePress = vec2(0.0f, 0.0f);
		vec2 m_MouseRelease = vec2(0.0f, 0.0f);
		int m_CurSelStart = 0;
		int m_CurSelEnd = 0;
		bool m_HasSelection = false;
		int m_NewLineCounter = 0;

		CGameConsole *m_pGameConsole;

		char m_aCompletionBuffer[IConsole::CMDLINE_LENGTH];
		int m_CompletionChosen;
		char m_aCompletionBufferArgument[IConsole::CMDLINE_LENGTH];
		int m_CompletionChosenArgument;
		int m_CompletionFlagmask;
		float m_CompletionRenderOffset;
		float m_CompletionRenderOffsetChange;
		int m_CompletionArgumentPosition;
		int m_CompletionCommandStart = 0;
		int m_CompletionCommandEnd = 0;

		char m_aUser[32];
		bool m_UserGot;
		bool m_UsernameReq;

		bool m_IsCommand;
		const char *m_pCommandName;
		const char *m_pCommandHelp;
		const char *m_pCommandParams;

		bool m_Searching = false;
		struct SSearchMatch
		{
			int m_Pos;
			int m_StartLine;
			int m_EndLine;
			int m_EntryLine;

			SSearchMatch(int Pos, int StartLine, int EndLine, int EntryLine) :
				m_Pos(Pos), m_StartLine(StartLine), m_EndLine(EndLine), m_EntryLine(EntryLine) {}
		};
		int m_CurrentMatchIndex;
		char m_aCurrentSearchString[IConsole::CMDLINE_LENGTH];
		std::vector<SSearchMatch> m_vSearchMatches;

		CInstance(int t);
		void Init(CGameConsole *pGameConsole);

		void ClearBacklog() REQUIRES(!m_BacklogPendingLock);
		void UpdateBacklogTextAttributes();
		void PumpBacklogPending() REQUIRES(!m_BacklogPendingLock);
		void ClearHistory();
		void Reset();

		void ExecuteLine(const char *pLine);

		bool OnInput(const IInput::CEvent &Event);
		void PrintLine(const char *pLine, int Len, ColorRGBA PrintColor) REQUIRES(!m_BacklogPendingLock);
		int GetLinesToScroll(int Direction, int LinesToScroll);
		void ScrollToCenter(int StartLine, int EndLine);
		void Dump() REQUIRES(!m_BacklogPendingLock);

		const char *GetString() const { return m_Input.GetString(); }
		/**
		 * Gets the command at the current cursor including surrounding spaces.
		 * Commands are split by semicolons.
		 *
		 * So if the current console input is for example "hello; world ;foo"
		 *                                                        ^
		 *                   and the cursor is here  -------------/
		 * The result would be " world "
		 *
		 * @param pInput the console input line
		 * @param aCmd the command the cursor is at
		 */
		void GetCommand(const char *pInput, char (&aCmd)[IConsole::CMDLINE_LENGTH]);
		static void PossibleCommandsCompleteCallback(int Index, const char *pStr, void *pUser);
		static void PossibleArgumentsCompleteCallback(int Index, const char *pStr, void *pUser);

		void UpdateEntryTextAttributes(CBacklogEntry *pEntry) const;

	private:
		void SetSearching(bool Searching);
		void ClearSearch();
		void UpdateSearch();

		friend class CGameConsole;
	};

	class IConsole *m_pConsole;
	CConsoleLogger *m_pConsoleLogger = nullptr;

	CInstance m_LocalConsole;
	CInstance m_RemoteConsole;

	CInstance *ConsoleForType(int ConsoleType);
	CInstance *CurrentConsole();

	int m_ConsoleType;
	int m_ConsoleState;
	float m_StateChangeEnd;
	float m_StateChangeDuration;

	bool m_WantsSelectionCopy = false;
	CUi::CTouchState m_TouchState;

	static const ColorRGBA ms_SearchHighlightColor;
	static const ColorRGBA ms_SearchSelectedColor;

	static void PossibleCommandsRenderCallback(int Index, const char *pStr, void *pUser);
	static void ConToggleLocalConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConToggleRemoteConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConClearLocalConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConClearRemoteConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpLocalConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpRemoteConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConConsolePageUp(IConsole::IResult *pResult, void *pUserData);
	static void ConConsolePageDown(IConsole::IResult *pResult, void *pUserData);
	static void ConchainConsoleOutputLevel(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

public:
	enum
	{
		CONSOLETYPE_LOCAL = 0,
		CONSOLETYPE_REMOTE,
		NUM_CONSOLETYPES
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
	virtual bool OnInput(const IInput::CEvent &Event) override;
	void Prompt(char (&aPrompt)[32]);

	void Toggle(int Type);
	bool IsClosed() { return m_ConsoleState == CONSOLE_CLOSED; }
};
#endif
