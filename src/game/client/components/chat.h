/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CHAT_H
#define GAME_CLIENT_COMPONENTS_CHAT_H
#include <engine/shared/config.h>
#include <engine/shared/ringbuffer.h>
#include <game/client/component.h>
#include <game/client/lineinput.h>

class CChat : public CComponent
{
	CLineInput m_Input;

	static constexpr float CHAT_WIDTH = 200.0f;
	static constexpr float CHAT_HEIGHT_FULL = 200.0f;
	static constexpr float CHAT_HEIGHT_MIN = 50.0f;

	enum
	{
		MAX_LINES = 25
	};

	struct CLine
	{
		int64 m_Time;
		float m_YOffset[2];
		int m_ClientID;
		int m_Team;
		int m_NameColor;
		char m_aName[64];
		char m_aText[512];
		bool m_Friend;
		bool m_Highlighted;

		int m_TextContainerIndex;
		int m_QuadContainerIndex;

		char m_aSkinName[sizeof(g_Config.m_ClPlayerSkin) / sizeof(g_Config.m_ClPlayerSkin[0])];
		CSkin::SSkinTextures m_RenderSkin;
		CSkin::SSkinMetrics m_RenderSkinMetrics;
		bool m_CustomColoredSkin;
		ColorRGBA m_ColorBody;
		ColorRGBA m_ColorFeet;

		bool m_HasRenderTee;
		float m_TextYOffset;

		int m_TimesRepeated;
	};

	bool m_PrevScoreBoardShowed;
	bool m_PrevShowChat;

	CLine m_aLines[MAX_LINES];
	int m_CurrentLine;

	// chat
	enum
	{
		MODE_NONE = 0,
		MODE_ALL,
		MODE_TEAM,

		CHAT_SERVER = 0,
		CHAT_HIGHLIGHT,
		CHAT_CLIENT,
		CHAT_NUM,
	};

	int m_Mode;
	bool m_Show;
	bool m_InputUpdate;
	int m_ChatStringOffset;
	int m_OldChatStringLength;
	int m_CompletionChosen;
	char m_aCompletionBuffer[256];
	int m_PlaceholderOffset;
	int m_PlaceholderLength;

	struct CCommand
	{
		const char *pName;
		const char *pParams;

		bool operator<(const CCommand &Other) const { return str_comp(pName, Other.pName) < 0; }
		bool operator<=(const CCommand &Other) const { return str_comp(pName, Other.pName) <= 0; }
		bool operator==(const CCommand &Other) const { return str_comp(pName, Other.pName) == 0; }
	};

	sorted_array<CCommand> m_Commands;
	bool m_ReverseTAB;

	struct CHistoryEntry
	{
		int m_Team;
		char m_aText[1];
	};
	CHistoryEntry *m_pHistoryEntry;
	CStaticRingBuffer<CHistoryEntry, 64 * 1024, CRingBufferBase::FLAG_RECYCLE> m_History;
	int m_PendingChatCounter;
	int64 m_LastChatSend;
	int64 m_aLastSoundPlayed[CHAT_NUM];

	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConSayTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConChat(IConsole::IResult *pResult, void *pUserData);
	static void ConShowChat(IConsole::IResult *pResult, void *pUserData);
	static void ConEcho(IConsole::IResult *pResult, void *pUserData);

	static void ConchainChatTee(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainChatBackground(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	bool LineShouldHighlight(const char *pLine, const char *pName);
	void StoreSave(const char *pText);
	void Reset();

public:
	CChat();

	static constexpr float MESSAGE_PADDING_X = 5.0f;
	static constexpr float MESSAGE_TEE_SIZE = 7.0f;
	static constexpr float MESSAGE_TEE_PADDING_RIGHT = 0.5f;
	static constexpr float FONT_SIZE = 6.0f;
	static constexpr float MESSAGE_PADDING_Y = 1.0f;
	static constexpr float MESSAGE_ROUNDING = 3.0f;
	static_assert(FONT_SIZE + MESSAGE_PADDING_Y >= MESSAGE_ROUNDING * 2.0f, "Corners for background chat are too huge for this combination of font size and message padding.");

	bool IsActive() const { return m_Mode != MODE_NONE; }
	void AddLine(int ClientID, int Team, const char *pLine);
	void EnableMode(int Team);
	void DisableMode();
	void Say(int Team, const char *pLine);
	void SayChat(const char *pLine);
	void RegisterCommand(const char *pName, const char *pParams, int flags, const char *pHelp);
	void Echo(const char *pString);

	virtual void OnWindowResize();
	virtual void OnConsoleInit();
	virtual void OnStateChange(int NewState, int OldState);
	virtual void OnRender();
	virtual void RefindSkins();
	virtual void OnPrepareLines();
	virtual void OnRelease();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual bool OnInput(IInput::CEvent Event);

	void RebuildChat();
};
#endif
