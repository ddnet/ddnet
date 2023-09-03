/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CHAT_H
#define GAME_CLIENT_COMPONENTS_CHAT_H
#include <vector>

#include <engine/console.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/shared/ringbuffer.h>

#include <game/client/component.h>
#include <game/client/lineinput.h>
#include <game/client/skin.h>

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
		int64_t m_Time;
		float m_aYOffset[2];
		int m_ClientID;
		int m_TeamNumber;
		bool m_Team;
		bool m_Whisper;
		int m_NameColor;
		char m_aName[64];
		char m_aText[512];
		bool m_Friend;
		bool m_Highlighted;

		int m_TextContainerIndex;
		int m_QuadContainerIndex;

		char m_aSkinName[std::size(g_Config.m_ClPlayerSkin)];
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
	bool m_CompletionUsed;
	int m_CompletionChosen;
	char m_aCompletionBuffer[256];
	int m_PlaceholderOffset;
	int m_PlaceholderLength;
	struct CRateablePlayer
	{
		int ClientID;
		int Score;
	};
	CRateablePlayer m_aPlayerCompletionList[MAX_CLIENTS];
	int m_PlayerCompletionListLength;

	struct CCommand
	{
		const char *m_pName;
		const char *m_pParams;

		CCommand() = default;
		CCommand(const char *pName, const char *pParams) :
			m_pName(pName), m_pParams(pParams)
		{
		}

		bool operator<(const CCommand &Other) const { return str_comp(m_pName, Other.m_pName) < 0; }
		bool operator<=(const CCommand &Other) const { return str_comp(m_pName, Other.m_pName) <= 0; }
		bool operator==(const CCommand &Other) const { return str_comp(m_pName, Other.m_pName) == 0; }
	};

	std::vector<CCommand> m_vCommands;
	bool m_ReverseTAB;

	struct CHistoryEntry
	{
		int m_Team;
		char m_aText[1];
	};
	CHistoryEntry *m_pHistoryEntry;
	CStaticRingBuffer<CHistoryEntry, 64 * 1024, CRingBufferBase::FLAG_RECYCLE> m_History;
	int m_PendingChatCounter;
	int64_t m_LastChatSend;
	int64_t m_aLastSoundPlayed[CHAT_NUM];

	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConSayTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConChat(IConsole::IResult *pResult, void *pUserData);
	static void ConShowChat(IConsole::IResult *pResult, void *pUserData);
	static void ConEcho(IConsole::IResult *pResult, void *pUserData);

	static void ConchainChatOld(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	bool LineShouldHighlight(const char *pLine, const char *pName);
	void StoreSave(const char *pText);

public:
	CChat();
	int Sizeof() const override { return sizeof(*this); }

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

	void OnWindowResize() override;
	void OnConsoleInit() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnRender() override;
	void RefindSkins();
	void OnPrepareLines();
	void Reset();
	void OnRelease() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	bool OnInput(IInput::CEvent Event) override;
	void OnInit() override;

	void RebuildChat();
};
#endif
