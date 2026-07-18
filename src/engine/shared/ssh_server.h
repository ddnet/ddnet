#ifndef ENGINE_SHARED_SSH_SERVER_H
#define ENGINE_SHARED_SSH_SERVER_H

#if defined(CONF_SSH)

#include <base/log.h>
#include <base/logger.h>
#include <base/types.h>
#include <base/vmath.h>

#include <engine/external/unicode-width/unicode_width.h>
#include <engine/shared/config.h>
#include <engine/shared/network.h>
#include <engine/shared/ringbuffer.h>
#include <engine/storage.h>

#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

#include <optional>

static constexpr int MAX_SSH_CLIENTS = 16;

class CSshServer;

// Not thread-safe!
class CSshLogger : public ILogger
{
	CSshServer *m_pSshServer;
	int m_ClientId;
	ILogger *m_pOuterLogger;

public:
	CSshLogger(CSshServer *pSshServer, int ClientId, ILogger *pOuterLogger) :
		m_pSshServer(pSshServer),
		m_ClientId(ClientId),
		m_pOuterLogger(pOuterLogger)
	{
	}
	void Log(const CLogMessage *pMessage) override;

	static int LineWrapForSsh(const char *pServerLine, char *pSshLine, size_t SshLineSize, int TerminalWidth, unicode_width_state_t *pUnicodeWidthState);
};

// supposed to hold raw data
// it does place a null byte at the end
// (after the size bound)
// so that even a partial data can be used like a valid C string
class CByteBuffer
{
	unsigned char m_aBuf[512];
	size_t m_Size = 0;

public:
	void AddByte(char Byte);
	void AddBytes(unsigned char *pBytes, size_t Size);

	// delete Amount bytes at the start of the buffer
	void TrimLeading(size_t Amount);

	// delete Amount bytes at the end of the buffer
	void TrimTrailing(size_t Amount);

	void Clear();
	size_t Size() const { return m_Size; }
	const unsigned char *Data() const { return m_aBuf; }
};

class CSshClient
{
public:
	const IConsole *Console() const;
	IConsole *Console();

	CSshClient(int ClientId, ssh_session Session)
	{
		m_ClientId = ClientId;
		m_Session = Session;
	}

	int m_ClientId;
	NETADDR m_Addr;

	// HOLY STATE HANDLING???
	// refactor this?
	bool m_Authenticated = false;
	bool m_ShellReady = false;
	bool m_Dropped = false;

	ssh_session m_Session;
	ssh_channel m_Channel = nullptr;

	int64_t m_JoinTime = 0;
	// The current ssh channel read buffer.
	// These are all the bytes the ssh client sent to use
	// that we have not processed yet
	CByteBuffer m_Buffer;

	// Users can cut and paste text with ctrl+k and ctrl+y
	char m_aYankBuffer[2048] = "";

	// The text input in the current console prompt
	char m_aInput[2048] = "";

	// TODO: there should be two variables
	//       one is the cursor position in the clients terminal
	//       and the other one is a offset index into m_aInput
	//       they are shifted by the prompt length already
	//       but once we introduce multi byte unicode that takes up only
	//       one terminal column of width everything becomes too hard to
	//       store in only one variable
	//       --
	//       Not sure but could move this one to m_Term to make it more clear
	//       that its not the input index
	//
	// position of the cursor in the m_aInput buffer
	// sent to the client and used as insert offset when typing
	ivec2 m_CursorPos = ivec2(0, 0);

	// current index in the m_aInput array
	// related to m_Term.m_CursorPos.x but not the same
	// it is not shifted by the prompt length
	// ideally this would always point to a valid utf8 codepoint
	// but for now this is also the index for the partial write
	// of a multi byte utf8 character
	int m_InputIdx = 0;

	// returns false when it did not move
	bool CursorMoveLeft();

	// returns false when it did not move
	bool CursorMoveRight();

	// returns false when it did not move
	bool CursorMoveWordLeft();

	// returns false when it did not move
	bool CursorMoveWordRight();

	// returns false when there was no input to be deleted
	bool DeleteWordAtCursor();

	// we sent a cursor pos fetch to the client
	// and expect the next input to be the response to it
	bool m_WaitingForCursorPos = false;

	// overwrites the entire m_aInput buffer
	// and sends the new line to the client over the ssh channel
	void SetInput(const char *pInput);

	static bool IsSimpleAsciiLetter(char Byte);

	// when the client sent a new byte
	// that should be added to the input buffer
	//
	// not called for escape sequences or multi byte utf8 symbols
	// returns false if the input was not changed and a size limit was reached.
	bool AddSingleAsciiLetterToInput(char Byte);

	// Similar to SetInput() but does not override but insert.
	// Similar to AddSingleAsciiLetterToInput() and AddSingleUtf8CodePointToInput()
	// but is not limited in length.
	// Utf-8 is supported.
	// It does also move the cursor client and server side.
	// returns false if the input was not changed and a size limit was reached.
	bool InsertToInputAtCursor(const char *pText);

	// clears the current line and places an empty prompt
	void ClearPrompt();

	// sends new line and prompt
	void NewPrompt();

	const char *PromptStr();
	int PromptLength();
	void SetCursorPosToPromptStart();

	void ClearCompletionPreview();
	void SendBell() const;
	void SendColor(LOG_COLOR Color) const;
	void ResetColor() const;
	void SendCursorPos(ivec2 Pos) const;
	void RequestCursorPos();

	void ResetCompletion();
	void CompleteCommands(bool IsReverse);

	char m_aCompletionBuffer[2048] = "";
	int m_CompletionIndex = -1;
	int m_CompletionEnumerationCount = -1;
	const char *m_pCompletionPreview = nullptr;

	static void CompletionCallback(int Index, const char *pCmd, void *pUser);
	static void CompletionPreviewCallback(int Index, const char *pCmd, void *pUser);

	class CTerminal
	{
	public:
		int m_Width = 10;
		int m_Height = 10;

		// cursor position on the clients screen
		// this value is mostly written by the server
		// but sometimes also fetched from the client
		//
		// Be careful m_CursorPos.x can NOT be used as an index to m_aInput
		// because the value is offset by the prompt length
		// and also by utf8 characters that span only one column but have multiple bytes
		ivec2 m_CursorPos = ivec2(0, 0);
	};
	CTerminal m_Term;

	CStaticRingBuffer<char, 64 * 1024, CRingBufferBase::FLAG_RECYCLE> m_History;
	char *m_pHistoryEntry = nullptr;

	void AddToInputHistory(const char *pInput);

	// We need to manage the memory for this struct
	// because libssh does not copy it
	struct ssh_server_callbacks_struct m_ServerCallback = {};
	struct ssh_channel_callbacks_struct m_ChannelCallback = {};

	// TODO: I wonder if just passing *this* would be enough
	//       i think it works for now but then later we might need the entire server in scope
	//       to ban on too many login attempts or something like that
	class CCallbackCtx
	{
	public:
		CSshClient *m_pClient = nullptr;
		CSshServer *m_pServer = nullptr;
	};
	CCallbackCtx m_CallbackCtx;
};

class CSshServer
{
	CConfig *m_pConfig = nullptr;
	IConsole *m_pConsole = nullptr;
	IStorage *m_pStorage = nullptr;

	const IStorage *Storage() const { return m_pStorage; }
	IStorage *Storage() { return m_pStorage; }

	ssh_bind m_Bind = nullptr;

	char m_aError[512] = "";

	void GenerateHostKeyIfMissing();
	void ProcessMessage(CSshClient *pClient);

	std::optional<int> FindFreeSlot();

	// libssh callbacks
	static int AuthPasswordCallback(ssh_session Session, const char *pUsername, const char *pPassword, void *pUserData);
	static int AuthPubkeyCallback(ssh_session Session, const char *pUsername, struct ssh_key_struct *pClientPubKey, char SignatureState, void *pUserData);
	static ssh_channel ChannelOpenRequestSessionCallback(ssh_session Session, void *pUserData);
	static int ChannelPtyRequestCallback(ssh_session Session, ssh_channel Channel, const char *pTerm, int Width, int Height, int PxWidth, int PwHeight, void *pUserData);
	static int ChannelPtyWindowChangeCallback(ssh_session Session, ssh_channel Channel, int Width, int Height, int PxWidth, int PwHeight, void *pUserData);
	static int ChannelExecRequestCallback(ssh_session Session, ssh_channel Channel, const char *pCommand, void *pUserData);
	static int ChannelShellRequestCallback(ssh_session Session, ssh_channel Channel, void *pUserData);

	void OnClientConnect(int ClientId, ssh_session Session);
	void OnClientDisconnect(int ClientId, const char *pReason = "");

	void AcceptNewConnections();
	void ListConnections();
	void ExecuteRconLine(CSshClient *pClient, const char *pLine);
	void TryProcessCurrentInput(CSshClient *pClient);
	void ReadNewInput(CSshClient *pClient);

public:
	CSshClient *m_apClients[MAX_SSH_CLIENTS] = {};
	unicode_width_state_t m_UnicodeWidthState;

	IConsole *Console() { return m_pConsole; }

	void Init(CConfig *pConfig, IConsole *pConsole, IStorage *pStorage);
	void Update();
	void Shutdown();
	bool GotActiveConnections();
};

#endif

#endif
