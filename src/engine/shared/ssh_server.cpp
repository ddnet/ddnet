#if defined(CONF_SSH)

#include "ssh_server.h"

#include <base/dbg.h>
#include <base/io.h>
#include <base/log.h>
#include <base/logger.h>
#include <base/mem.h>
#include <base/net.h>
#include <base/str.h>
#include <base/time.h>
#include <base/types.h>

#include <engine/console.h>
#include <engine/external/unicode-width/unicode_width.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>

// TODO: have a shortcut like / to fuzzy search in command names and their help text

// TODO: somewhere the command help text has to be previewed!
//       right now users have to manually use "help sv_name" to get that which is worse
//       ux than the remote console in the client! Uacceptable!

// TODO: add some delay in the password check to combat bruteforcing
//       if there are too many requests from different ips slow down every check
//       after every failed attempt slow down that ip
//       --
//       or does libssh does this a bit already? or offer something?

// TODO: experiment with some fancy features
//       in theory this thing could be a full TUI
//       for example typing in the "kick" command could
//       and pressing tab there could be a player selection menu
//       for the id argument with a list of names
//       and the server will verify the selected name did not get invalidated
//       by disconnect once the command is being executed
//       ---
//       can also extend the console scripting language with additional syntax
//       like loops, sleep statement, variables, pipes/filters and functions that get parsed and interpreted before being passed
//       to console execute line
//       just imagine "kick 0 $reason"
//       or "status | grep xx"
//       or "logs" which opens a less/more like curses app
//       or "dump_xx | less"

// TODO: add an option to edit config variables not just override them.
//       so for example "edit sv_name" or "e sv_name" fills the prompt with the
//       current value and lets the user edit it in place
//       for this we need a new prompt mode similar to the sarch mode to show that the user what is being edited

// TODO: reaching terminal width length with the input is not supported yet
//       things get weird
//       i mean ideally it would line wrap like a terminal does but thats kinda tricky
//       for now i would like to just cap it at terminal width and have limited but
//       clear and bug free behavior
//       ---
//       of course that gets tricky when shrinking the terminal window
//       maybe the only really stable way to go is to implement proper wrapping
//       or at least rewrite the entire input on resize to allow users to get out of
//       the bad state by just expanding the terminal again without having to rewrite input

// TODO: the default file location in the current dir is not ideal
//       this should be in the storage save location
//       or maybe even use the system wide location that also the regular host
//       ssh server uses so we do not need to generate the key
#define HOSTKEY_FILE "ssh_host_rsa_key"

// the KEY_ prefix is already used by sdl
// also it does not seem to perfectly fit
// but idk what the better choice would be

#define KEY_ENTER 13
#define KEY_CTRL_U 21
#define KEY_CTRL_K 11
#define KEY_CTRL_Y 25
#define KEY_CTRL_C 3
#define KEY_CTRL_D 4
#define KEY_CTRL_L 12
#define KEY_CTRL_A 1
#define KEY_CTRL_E 5
#define KEY_CTRL_R 18
#define KEY_TAB 9
#define KEY_DEL 127
#define KEY_BACKSPACE '\b'

// TODO: there is sockaddr_to_netaddr() in base/net.cpp but its static too there
//       moving it in to the header creates a diff to ddnet that is hard to maintain
//       also windows build is annoying because the signature requires includes
static void SockaddrToNetaddr(const sockaddr *pSrc, socklen_t SrcLen, NETADDR *pDst)
{
	*pDst = NETADDR_ZEROED;
	if(pSrc->sa_family == AF_INET && SrcLen >= (socklen_t)sizeof(sockaddr_in))
	{
		const sockaddr_in *pSrcIn = (const sockaddr_in *)pSrc;
		pDst->type = NETTYPE_IPV4;
		pDst->port = htons(pSrcIn->sin_port);
		static_assert(sizeof(pDst->ip) >= sizeof(pSrcIn->sin_addr.s_addr));
		mem_copy(pDst->ip, &pSrcIn->sin_addr.s_addr, sizeof(pSrcIn->sin_addr.s_addr));
	}
	else if(pSrc->sa_family == AF_INET6 && SrcLen >= (socklen_t)sizeof(sockaddr_in6))
	{
		const sockaddr_in6 *pSrcIn6 = (const sockaddr_in6 *)pSrc;
		pDst->type = NETTYPE_IPV6;
		pDst->port = htons(pSrcIn6->sin6_port);
		static_assert(sizeof(pDst->ip) >= sizeof(pSrcIn6->sin6_addr.s6_addr));
		mem_copy(pDst->ip, &pSrcIn6->sin6_addr.s6_addr, sizeof(pSrcIn6->sin6_addr.s6_addr));
	}
	else
	{
		log_warn("net", "Cannot convert sockaddr of family %d", pSrc->sa_family);
	}
}

static bool FuzzyMatch(const char *pHaystack, const char *pNeedle)
{
	if(!pNeedle || !pNeedle[0])
		return false;
	char aBuf[2] = {0};
	const char *pHit = pHaystack;
	int NeedleLen = str_length(pNeedle);
	for(int i = 0; i < NeedleLen; i++)
	{
		if(!pHit)
			return false;
		aBuf[0] = pNeedle[i];
		pHit = str_find_nocase(pHit, aBuf);
		if(pHit)
			pHit++;
	}
	return pHit;
}

// Get the amount of terminal columns this string will take up.
// It supports multi byte utf-8 characters and also wide characters
static int StringTerminalWidth(const char *pStr, unicode_width_state_t *pUnicodeWidthState)
{
	int Width = 0;
	while(*pStr)
	{
		int CodePoint = str_utf8_decode(&pStr);
		if(CodePoint == 0)
		{
			return Width;
		}
		if(CodePoint == -1)
		{
			// meh
			return Width;
		}
		Width += unicode_width_process(pUnicodeWidthState, CodePoint);
	}
	return Width;
}

// We keep track of the client's ssh session cursor position on the server side.
// For that we need to update the y position on every new log line.
// This gets complicated as soon as the log line contains \n newline escape sequences
// and the line is longer than the terminal width because then we need to increase the y
// pos more than once.
//
// Another problem is that we need to send \r\n over the ssh connection and the logger just needs \n
// for a proper line break. So to visually represent everything correct on the client side we also
// need to edit the log line to replace all \n with \r\n
//
// This method should be called for every log line the server printed and that we send to the ssh client.
// It returns the amount of rows written (so by how much the cursor pos y should be incremented)
// And it writes the reformartted log line to the output buffer pSshLine
int CSshLogger::LineWrapForSsh(const char *pServerLine, char *pSshLine, size_t SshLineSize, int TerminalWidth, unicode_width_state_t *pUnicodeWidthState)
{
	size_t InIdx = 0;
	size_t OutIdx = 0;
	int NumLines = 1;
	int SubLineLen = 0;

	auto WriteOutputByte = [&OutIdx, SshLineSize, &pSshLine](char Byte) {
		if(OutIdx + 1 > SshLineSize - 2)
			return false;

		pSshLine[OutIdx++] = Byte;
		return true;
	};

	while(pServerLine[InIdx])
	{
		if(pServerLine[InIdx] == '\n')
		{
			if(!WriteOutputByte('\r'))
				break;
			NumLines++;
			SubLineLen = 0;
		}
		const char *pStr = pServerLine + InIdx;
		int CodePoint = str_utf8_decode(&pStr);

		// neither end of string not invalid utf8
		// are expected here
		// this should be an assert but we are in the log callback
		// we cant log on log so the assert error message would always be hidden
		// and we crash the server for nothing
		// so instead we just do some safe fallback which will mess up the cursor position potentially
		if(CodePoint == 0 || CodePoint == -1)
		{
			str_copy(pSshLine, pServerLine, SshLineSize);
			return 1;
		}

		int Bytes = pStr - (pServerLine + InIdx);
		for(int i = 0; i < Bytes; i++)
		{
			// this only breaks the inner loop but thats fine
			// on the next write it will break the outer loop
			// and it anyways never will write if full
			if(!WriteOutputByte(pServerLine[InIdx++]))
				break;
		}
		int Width = unicode_width_process(pUnicodeWidthState, CodePoint);
		SubLineLen += Width;
		if(SubLineLen > TerminalWidth)
		{
			NumLines++;
			SubLineLen = 0;
		}
	};
	pSshLine[OutIdx] = '\0';

	return NumLines;
}

void CSshLogger::Log(const CLogMessage *pMessage)
{
	CSshClient *pClient = m_pSshServer->m_apClients[m_ClientId];
	if(!pClient)
	{
		m_pOuterLogger->Log(pMessage);
		return;
	}

	// We intentionally only print the Message() here not the m_aLine
	// the timestamp is quite long and usually not interesting for a prompt
	// the system is often "chatresp" which just looks ugly and also makes
	// the output harder to read.
	// Just showing the message might in some rare cases cause confusion where
	// this log line comes from but in the majority of cases makes the console
	// way more clean which is the main selling point of this entire thing.
	// For now I don't have a use case for it but in the future there could be an option
	// to enable printing more than just the message.

	if(pClient->m_Channel)
	{
		bool NeedColorReset = true;
		// I tested it and coloring does work
		// but had to patch the server code for it
		// because as far as I can tell no server side log uses colors for now
		if(pMessage->m_HaveColor)
		{
			pClient->SendColor(pMessage->m_Color);
		}
		else if(pMessage->m_Level == LEVEL_ERROR)
		{
			pClient->SendColor({.r = 220, .g = 53, .b = 69});
		}
		else if(pMessage->m_Level == LEVEL_WARN)
		{
			pClient->SendColor({.r = 225, .g = 193, .b = 7});
		}
		else
		{
			NeedColorReset = false;
		}
		char aSshLine[8192];
		int NumLines = LineWrapForSsh(pMessage->Message(), aSshLine, sizeof(aSshLine), pClient->m_Term.m_Width, &m_pSshServer->m_UnicodeWidthState);
		pClient->m_CursorPos.y += NumLines;
		ssh_channel_write(pClient->m_Channel, "\r\n", 2);
		ssh_channel_write(pClient->m_Channel, aSshLine, str_length(aSshLine));
		if(NeedColorReset)
		{
			pClient->ResetColor();
		}
	}

	// just mirror everything to regular log because the hiding is stupid
	// https://github.com/ddnet/ddnet/issues/11095
	m_pOuterLogger->Log(pMessage);
}

void CByteBuffer::AddByte(char Byte)
{
	dbg_assert(m_Size + 1 < sizeof(m_aBuf) - 2, "byte buffer full");
	m_aBuf[m_Size++] = Byte;
	m_aBuf[m_Size] = 0x00;
}

void CByteBuffer::AddBytes(unsigned char *pBytes, size_t Size)
{
	if(Size == 0)
		return;

	dbg_assert(m_Size + Size < sizeof(m_aBuf) - 2, "byte buffer full");
	mem_copy(m_aBuf + m_Size, pBytes, Size);
	m_Size += Size;
	m_aBuf[m_Size] = 0x00;
}

void CByteBuffer::TrimLeading(size_t Amount)
{
	if(Amount >= m_Size)
	{
		m_Size = 0;
		m_aBuf[m_Size] = 0x00;
		return;
	}
	if(!Amount)
		return;
	unsigned char *aTmp[sizeof(m_aBuf)];
	m_Size -= Amount;
	mem_copy(aTmp, m_aBuf + Amount, m_Size);
	mem_copy(m_aBuf, aTmp, m_Size);
	m_aBuf[m_Size] = 0x00;
}

void CByteBuffer::TrimTrailing(size_t Amount)
{
	m_Size = std::max((size_t)0, m_Size - Amount);
	m_aBuf[m_Size] = 0x00;
}

void CByteBuffer::Clear()
{
	m_Size = 0;
}

const IConsole *CSshClient::Console() const
{
	return m_CallbackCtx.m_pServer->Console();
}

IConsole *CSshClient::Console()
{
	return m_CallbackCtx.m_pServer->Console();
}

bool CSshClient::CursorMoveLeft()
{
	if(m_InputIdx == 0)
		return false;

	const int Min = PromptLength();
	m_InputIdx = str_utf8_rewind(m_aInput, m_InputIdx);
	int Width = 1;
	const char *pStr = m_aInput + m_InputIdx;
	int CodePoint = str_utf8_decode(&pStr);
	if(CodePoint == 0 || CodePoint == -1)
	{
		log_warn("ssh", "Move cursor left failed. Invalid CodePoint: %d", CodePoint);
	}
	else
	{
		Width = unicode_width_process(&m_CallbackCtx.m_pServer->m_UnicodeWidthState, CodePoint);
	}
	m_CursorPos.x = std::max(m_CursorPos.x - Width, Min);
	return true;
}

bool CSshClient::CursorMoveRight()
{
	const char *pStr = m_aInput + m_InputIdx;
	int CodePoint = str_utf8_decode(&pStr);
	int Size = pStr - (m_aInput + m_InputIdx);
	int Width = 1;
	if(CodePoint == 0)
		return false;
	if(CodePoint == -1)
	{
		log_warn("ssh", "Move cursor right failed. Invalid CodePoint: %d", CodePoint);
	}
	else
	{
		Width = unicode_width_process(&m_CallbackCtx.m_pServer->m_UnicodeWidthState, CodePoint);
	}

	if(m_CursorPos.x + Width >= m_Term.m_Width)
		return false;
	if(m_InputIdx + Size >= (int)sizeof(m_aInput) - 1)
		return false;
	if(m_InputIdx + Size > str_length(m_aInput))
		return false;

	m_InputIdx += Size;
	m_CursorPos.x += Width;
	return true;
}

bool CSshClient::CursorMoveWordLeft()
{
	// skip to first word
	while(m_aInput[m_InputIdx] == ' ')
	{
		if(m_InputIdx == 0)
			return false;

		m_InputIdx--;
		m_CursorPos.x--;
	}

	while(true)
	{
		if(!CursorMoveLeft())
			return false;

		if(m_aInput[m_InputIdx] == ' ')
			return true;
	}
	return false;
}

bool CSshClient::CursorMoveWordRight()
{
	bool Moved = false;

	// skip to first word
	while(m_aInput[m_InputIdx] == ' ')
	{
		m_InputIdx++;
		m_CursorPos.x++;
	}

	while(true)
	{
		if(!CursorMoveRight())
			return Moved;

		Moved = true;
		if(m_aInput[m_InputIdx] == ' ')
			return true;
	}
	return Moved;
}

bool CSshClient::DeleteWordAtCursor()
{
	ivec2 OldCursorPos = m_CursorPos;
	int BeginIdx = m_InputIdx;
	if(!CursorMoveWordRight())
		return false;
	int EndIdx = m_InputIdx;
	m_InputIdx = BeginIdx;
	m_CursorPos = OldCursorPos;
	if(BeginIdx >= EndIdx)
		return false;

	char aRight[sizeof(m_aInput)];
	str_copy(aRight, m_aInput + EndIdx);
	str_copy(m_aYankBuffer, m_aInput + BeginIdx, (EndIdx - BeginIdx) + 1);
	m_aInput[BeginIdx] = '\0';
	str_append(m_aInput, aRight);

	ssh_channel_write(m_Channel, "\033[K", str_length("\033[K"));
	ssh_channel_write(m_Channel, aRight, str_length(aRight));

	return false;
}

void CSshClient::SetInput(const char *pInput)
{
	str_copy(m_aInput, pInput);

	if(m_Channel)
	{
		ClearPrompt();
		ssh_channel_write(m_Channel, pInput, str_length(pInput));
	}
	m_CursorPos.x = PromptLength() + StringTerminalWidth(pInput, &m_CallbackCtx.m_pServer->m_UnicodeWidthState);
	m_InputIdx = str_length(m_aInput);
}

bool CSshClient::IsSimpleAsciiLetter(char Byte)
{
	if(Byte >= ' ' && Byte <= '~')
		return true;
	return false;
}

bool CSshClient::AddSingleAsciiLetterToInput(char Byte)
{
	{
		const int InputSize = str_length(m_aInput);
		const int InputWidth = StringTerminalWidth(m_aInput, &m_CallbackCtx.m_pServer->m_UnicodeWidthState);

		if(InputSize + 1 >= (int)sizeof(m_aInput) - 1)
		{
			return false;
		}
		// Ideally this would be PromptWidth() not PromptLength() but
		// lets keep things simple for now as the prompt is hardcodet ascii
		if(InputWidth + 1 + PromptLength() >= m_Term.m_Width)
		{
			return false;
		}
	}

	char aByteBuf[2] = {Byte, 0x00};
	ssh_channel Channel = m_Channel;
	bool CursorAtEnd = m_aInput[m_InputIdx] == '\0';
	if(CursorAtEnd)
	{
		// if we override the nullterm make sure to move it
		// so the string stays terminated
		// but not if we are in the middle of the input
		m_aInput[m_InputIdx++] = Byte;
		m_aInput[m_InputIdx] = '\0';
		m_CursorPos.x++;
		ssh_channel_write(Channel, aByteBuf, 1);
	}
	else
	{
		// if we are in the middle of the string
		// we need to shift all the input

		// clear everything from the cursor till the end
		ssh_channel_write(Channel, "\33[0K", str_length("\33[0K"));
		ssh_channel_write(Channel, aByteBuf, 1);

		char aRight[2048];
		str_copy(aRight, m_aInput + m_InputIdx);

		ssh_channel_write(Channel, aRight, str_length(aRight));

		m_aInput[m_InputIdx++] = Byte;
		m_aInput[m_InputIdx] = '\0';

		str_append(m_aInput, aRight);

		m_CursorPos.x++;

		// move cursor back into the input after rewriting the line
		SendCursorPos(m_CursorPos);
	}

	if(CursorAtEnd)
	{
		m_pCompletionPreview = nullptr;
		Console()->PossibleCommands(m_aInput, CFGFLAG_SERVER, false, CompletionPreviewCallback, &m_CallbackCtx);
		if(!m_pCompletionPreview)
			ClearCompletionPreview();
	}
	return true;
}

// InsertToInputAtCursor() could be used to replace AddSingleAsciiLetterToInput()
// and also the string could be buffered and inserted at once and not inserted for every code point
// but that makes the code a bit more complex because we will need to flush the buffered strings correctly
// if there are escape sequences mixed in.
// InsertToInputAtCursor() could be used right now to replace AddSingleAsciiLetterToInput() but it would
// be a bit less performant. So lets keep the common ascii case light weight.

bool CSshClient::InsertToInputAtCursor(const char *pText)
{
	ssh_channel Channel = m_Channel;
	bool CursorAtEnd = m_aInput[m_InputIdx] == '\0';
	const int TextSize = str_length(pText);
	const int TextWidth = StringTerminalWidth(pText, &m_CallbackCtx.m_pServer->m_UnicodeWidthState);

	{
		const int InputSize = str_length(m_aInput);
		const int InputWidth = StringTerminalWidth(m_aInput, &m_CallbackCtx.m_pServer->m_UnicodeWidthState);
		if(InputSize + TextSize >= (int)sizeof(m_aInput) - 1)
		{
			return false;
		}
		// Ideally this would be PromptWidth() not PromptLength() but
		// lets keep things simple for now as the prompt is hardcodet ascii
		if(TextWidth + InputWidth + PromptLength() >= m_Term.m_Width)
		{
			return false;
		}
	}

	if(CursorAtEnd)
	{
		str_append(m_aInput, pText);
		ssh_channel_write(Channel, pText, TextSize);
	}
	else
	{
		// if we are in the middle of the string
		// we need to shift all the input

		char aRight[2048];
		str_copy(aRight, m_aInput + m_InputIdx);

		// clear everything from the cursor till the end
		ssh_channel_write(Channel, "\33[0K", str_length("\33[0K"));
		ssh_channel_write(Channel, pText, TextSize);
		ssh_channel_write(Channel, aRight, str_length(aRight));

		m_aInput[m_InputIdx] = '\0';
		str_append(m_aInput, pText);
		str_append(m_aInput, aRight);
	}
	m_InputIdx += TextSize;
	m_CursorPos.x += TextWidth;

	if(!CursorAtEnd)
	{
		// if we had to clear and rewrite the right side
		// of the cursor that will shift the cursor implicitly client sided
		// so we need to resend the pos to be still in the middle of the input
		SendCursorPos(m_CursorPos);
	}

	return true;
}

void CSshClient::ClearPrompt()
{
	if(!m_Channel)
		return;

	ssh_channel_write(m_Channel, "\r\033[2K", 6);
	ssh_channel_write(m_Channel, PromptStr(), str_length(PromptStr()));
	SetCursorPosToPromptStart();
}

void CSshClient::NewPrompt()
{
	if(!m_Channel)
		return;

	ssh_channel_write(m_Channel, "\r\n", 2);
	ssh_channel_write(m_Channel, PromptStr(), str_length(PromptStr()));
	m_CursorPos.y++;
	SetCursorPosToPromptStart();
}

void CSshClient::ResendPrompt()
{
	if(!m_Channel)
		return;

	ssh_channel_write(m_Channel, "\r\033[2K", 6);
	ssh_channel_write(m_Channel, PromptStr(), str_length(PromptStr()));
	ssh_channel_write(m_Channel, m_aInput, str_length(m_aInput));
}

const char *CSshClient::PromptStr()
{
	if(m_Mode == EClientMode::HISTORY_SEARCH)
	{
		return "search: ";
	}
	return "> ";
}

int CSshClient::PromptLength()
{
	// TODO: off by one -,- ?
	return str_length(PromptStr()) + 1;
}

int CSshClient::PromptHeight()
{
	return 1;
}

void CSshClient::SetCursorPosToPromptStart()
{
	// not the most ideal method name but eh idk
	m_CursorPos.x = PromptLength();
	m_InputIdx = 0;
}

void CSshClient::ClearCompletionPreview()
{
	bool CursorAtEnd = m_aInput[m_InputIdx] == '\0';
	// there can only be a completion string if our cursor is
	// at the end of the input
	if(!CursorAtEnd)
		return;
	if(!m_pCompletionPreview)
		return;

	// clear everything from the cursor till the end
	ssh_channel_write(m_Channel, "\33[0K", str_length("\33[0K"));
	m_pCompletionPreview = nullptr;
}

void CSshClient::EnableAltBuf()
{
	if(!m_Channel)
		return;

	ssh_channel_write(m_Channel, "\033[?1049h", 9);
	m_CurorPosMainBuf = m_CursorPos;
}

void CSshClient::DisableAltBuf()
{
	if(!m_Channel)
		return;

	ssh_channel_write(m_Channel, "\033[?1049l", 9);
	m_CursorPos = m_CurorPosMainBuf;
}

void CSshClient::SendBell() const
{
	if(!m_Channel)
		return;

	ssh_channel_write(m_Channel, "\a", 1);
}

// TODO: never use this except ctrl+l
//       when redrawing frames do not clear the screen
//       this will cause visual glitches
//       ----
//       redraw by overriding and hide the cursor while doing so
//       keep a buffer representing the screen on the server side and
//       only send the diff
//       --
//       also batch the writes
//       dont do many ssh_channel_write calls if it in the end should be one frame
void CSshClient::SendClearScreen() const
{
	if(!m_Channel)
		return;

	ssh_channel_write(m_Channel, "\033[2J\r", 6);
}

void CSshClient::SendColor(LOG_COLOR Color) const
{
	if(!m_Channel)
		return;

	char aAnsi[32];
	str_format(aAnsi, sizeof(aAnsi),
		"\x1b[38;2;%d;%d;%dm",
		Color.r,
		Color.g,
		Color.b);
	ssh_channel_write(m_Channel, aAnsi, str_length(aAnsi));
}

void CSshClient::SendBackgroundColor(LOG_COLOR Color) const
{
	if(!m_Channel)
		return;

	char aAnsi[32];
	str_format(aAnsi, sizeof(aAnsi),
		"\033[48;2;%d;%d;%dm",
		Color.r,
		Color.g,
		Color.b);
	ssh_channel_write(m_Channel, aAnsi, str_length(aAnsi));
}

void CSshClient::ResetColor() const
{
	if(!m_Channel)
		return;

	const char aResetColor[] = "\x1b[0m";
	ssh_channel_write(m_Channel, aResetColor, str_length(aResetColor));
}

void CSshClient::SendCursorPos(ivec2 Pos) const
{
	// log_info("ssh", "sending pos x=%d y=%d", Pos.x, Pos.y);

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "\x1B[%d;%dH", Pos.y, Pos.x);
	ssh_channel_write(m_Channel, aBuf, str_length(aBuf));

	// there would also be relative move left and right
	// maybe that is more portable than sending absolute pos
	// Action	Escape Sequence (Text)
	// Move Up	\x1B[A
	// Move Down	\x1B[B
	// Move Right	\x1B[C
	// Move Left	\x1B[D
	// Move to (X,Y)	\x1B[%d;%dH
}

void CSshClient::RequestCursorPos()
{
	if(!m_Channel)
		return;

	ssh_channel_write(m_Channel, "\x1B[6n", str_length("\x1B[6n"));
	m_WaitingForCursorPos = true;
}

void CSshClient::ResetCompletion()
{
	ClearCompletionPreview();
	m_aCompletionBuffer[0] = '\0';
	m_CompletionIndex = -1;
	m_CompletionEnumerationCount = -1;
}

void CSshClient::AbortHistorySearch()
{
	if(m_Mode != EClientMode::HISTORY_SEARCH)
		return;

	DisableAltBuf();
	m_Mode = EClientMode::PROMPT;
	SetInput(m_aPromptInput);
}

void CSshClient::RenderHistorySearch()
{
	// TODO: this causes a bit of graphical glitching
	//       find something smoother
	//       maybe do less updates if the input did not affect the search
	//       or just override the text instead of clearing all
	SendClearScreen();

	const char *apMatches[MAX_TERMINAL_HEIGHT + 1] = {nullptr};
	m_pHistorySearchMatch = nullptr;

	int MaxLines = std::min(m_Term.m_Height - PromptHeight(), (int)MAX_TERMINAL_HEIGHT);
	int NumLines = 0;
	for(auto &Entry : m_InputHistory)
	{
		if(m_aInput[0])
			if(!FuzzyMatch(Entry.data(), m_aInput))
				continue;

		if(NumLines >= MaxLines)
			break;

		apMatches[NumLines++] = Entry.data();
	}
	int NumPaddings = MaxLines - NumLines;
	// for(int Pad = 0; Pad < NumPaddings; Pad++)
	// {
	// 	ssh_channel_write(m_Channel, "padding\r\n", 10);
	// }
	SendCursorPos({0, NumPaddings + 1});

	if(NumLines >= 1)
	{
		m_HistorySearchScroll = std::clamp(m_HistorySearchScroll, 0, NumLines - 1);

		// crazy invert wtf
		int InvertedScroll = NumLines - m_HistorySearchScroll;

		int i = 0;
		for(const char *pMatch : apMatches)
		{
			if(!pMatch)
				break;
			bool IsCandidate = ++i == InvertedScroll;
			if(IsCandidate)
			{
				SendBackgroundColor({.r = 55, .g = 55, .b = 55});
				m_pHistorySearchMatch = pMatch;

				// indent the candidate
				ssh_channel_write(m_Channel, " ", 1);
			}

			ssh_channel_write(m_Channel, pMatch, str_length(pMatch));
			ssh_channel_write(m_Channel, "\r\n", 2);

			if(IsCandidate)
			{
				ResetColor();
			}
		}
	}
	// could render a "no matches here"
	// we need to if statement so the clamp() does not explode
	// if we never render "no matches" we could also get rid of the indent
	// by doing a safer clamp

	ResendPrompt();
	SendCursorPos(m_CursorPos);
}

void CSshClient::CompleteCommands(bool IsReverse)
{
	if(m_CompletionEnumerationCount == -1)
		str_copy(m_aCompletionBuffer, m_aInput);

	m_CompletionEnumerationCount = 0;

	if(IsReverse)
		m_CompletionIndex--;
	else
		m_CompletionIndex++;

	Console()->PossibleCommands(m_aCompletionBuffer, CFGFLAG_SERVER, false, CompletionCallback, &m_CallbackCtx);

	// handle wrapping
	if(m_CompletionEnumerationCount && (m_CompletionIndex >= m_CompletionEnumerationCount || m_CompletionIndex < 0))
	{
		m_CompletionIndex = (m_CompletionIndex + m_CompletionEnumerationCount) % m_CompletionEnumerationCount;
		m_CompletionEnumerationCount = 0;
		Console()->PossibleCommands(m_aCompletionBuffer, CFGFLAG_SERVER, false, CompletionCallback, &m_CallbackCtx);
	}
}

void CSshClient::CompletionCallback(int Index, const char *pCmd, void *pUser)
{
	CCallbackCtx *pCtx = static_cast<CCallbackCtx *>(pUser);
	CSshClient *pClient = pCtx->m_pClient;

	if(pClient->m_CompletionIndex == pClient->m_CompletionEnumerationCount)
	{
		pClient->SetInput(pCmd);
	}

	pClient->m_CompletionEnumerationCount++;
}

void CSshClient::CompletionPreviewCallback(int Index, const char *pCmd, void *pUser)
{
	CCallbackCtx *pCtx = static_cast<CCallbackCtx *>(pUser);
	CSshClient *pClient = pCtx->m_pClient;

	if(Index == 0 && pClient->m_CompletionEnumerationCount == -1)
	{
		// the console completion uses string find and not str starts with
		// unlike for example the bash shell
		// so previewing the possible completion in line
		// gets quite complicated for a match in the middle of the command
		// so we only preview the simple case
		const char *pPreview = str_startswith(pCmd, pClient->m_aInput);
		if(pPreview)
		{
			// if there previously was a longer completion preview
			// we need to clear it out because we will only partially override it
			// could also check the string length of m_pCompletionPreview compared to pPreview
			// and only then clear but eh idk seems cheap enough to just always clear
			ssh_channel_write(pClient->m_Channel, "\33[0K", str_length("\33[0K"));

			ssh_channel_write(pClient->m_Channel, "\033[36m", str_length("\033[36m"));
			ssh_channel_write(pClient->m_Channel, pPreview, str_length(pPreview));
			ssh_channel_write(pClient->m_Channel, "\033[0m", str_length("\033[0m"));
			pClient->SendCursorPos(pClient->m_CursorPos);
			pClient->m_pCompletionPreview = pPreview;
		}
	}
}

void CSshClient::AddToInputHistory(const char *pInput)
{
	// check duplicates
	// for now only filter out consecutive duplicates
	if(!m_InputHistory.empty())
	{
		auto &Prev = m_InputHistory.back();
		if(!str_comp(Prev.data(), pInput))
		{
			return;
		}
	}

	auto &Entry = m_InputHistory.emplace_back();
	str_copy(Entry.data(), pInput, Entry.size());
	if(m_InputHistory.size() > MAX_HISTORY_ENTRIES)
	{
		m_InputHistory.pop_front();
	}

	// WARNING: here we go intentionally out of bounds by one
	m_HistoryIdx = m_InputHistory.size();
}

const char *CSshClient::PrevInputFromHistory()
{
	if(m_InputHistory.empty())
		return "";

	if(m_HistoryIdx > 0)
		m_HistoryIdx--;
	auto &Entry = m_InputHistory.at(m_HistoryIdx);
	return Entry.data();
}

const char *CSshClient::NextInputFromHistory()
{
	if(m_InputHistory.empty())
		return "";
	// WARNING: here we go intentionally out of bounds by one
	if(m_HistoryIdx < m_InputHistory.size())
		m_HistoryIdx++;
	if(m_HistoryIdx >= m_InputHistory.size())
		return "";

	auto &Entry = m_InputHistory.at(m_HistoryIdx);
	return Entry.data();
}

void CSshServer::ProcessMessage(CSshClient *pClient)
{
	// we aren't interested in any specific message yet
	// all of the messages are handled in callbacks
	// but if we do not call ssh_message_get() things get stuck
	ssh_session Session = pClient->m_Session;
	ssh_message Message = ssh_message_get(Session);
	if(Message == nullptr)
		return;

	// WARNING: if you check messages here make sure to check pClient->m_Authenticated first if needed

	ssh_message_free(Message);
}

void CSshServer::ListConnections()
{
	for(CSshClient *pClient : m_apClients)
	{
		if(!pClient)
			continue;

		char aAddr[NETADDR_MAXSTRSIZE];
		net_addr_str(&pClient->m_Addr, aAddr, sizeof(aAddr), true);
		int64_t Seconds = (time_get() - pClient->m_JoinTime) / time_freq();
		log_info(
			"ssh",
			"id=%d addr=<{%s}> authed=%d online since %" PRId64 " seconds",
			pClient->m_ClientId,
			aAddr,
			pClient->m_Authenticated,
			Seconds);
	}
}

void CSshServer::MergeInputHistory(CSshClient *pClient)
{
	m_InputHistory.insert(m_InputHistory.end(),
		pClient->m_InputHistory.begin(),
		pClient->m_InputHistory.end());

	while(m_InputHistory.size() > MAX_HISTORY_ENTRIES)
	{
		m_InputHistory.pop_front();
	}
}

void CSshServer::ExecuteRconLine(CSshClient *pClient, const char *pLine)
{
	Console()->ExecuteLine(pLine, IConsole::CLIENT_ID_UNSPECIFIED, true);
}

int CSshServer::TryProcessEscapeSequence(CSshClient *pClient, const char *pBuf, size_t BufSize)
{
	if(BufSize < 1 || pBuf[0] != 27)
		return 0;

	// KEY_ESCAPE
	if(BufSize == 1)
	{
		// TODO: check if there was an alt screen active and only then disable it
		//       we need some kind of popup and screen system
		//       but lets hack together some kind of view first so we can think about
		//       how to structure the code
		if(pClient->m_Mode == EClientMode::HISTORY_SEARCH)
		{
			pClient->AbortHistorySearch();
		}
		return 1;
	}

	if(BufSize >= 6 && pBuf[1] == 91)
	{
		bool CtrlLeft =
			pBuf[1] == 91 &&
			pBuf[2] == 49 &&
			pBuf[3] == 59 &&
			pBuf[4] == 53 &&
			pBuf[5] == 68;
		bool CtrlRight =
			pBuf[1] == 91 &&
			pBuf[2] == 49 &&
			pBuf[3] == 59 &&
			pBuf[4] == 53 &&
			pBuf[5] == 67;

		if(CtrlLeft)
		{
			if(!pClient->CursorMoveWordLeft())
				pClient->SendBell();
			pClient->SendCursorPos(pClient->m_CursorPos);

			// skip the sequence
			return 5;
		}
		else if(CtrlRight)
		{
			if(!pClient->CursorMoveWordRight())
				pClient->SendBell();
			pClient->SendCursorPos(pClient->m_CursorPos);

			// skip the sequence
			return 5;
		}

		if(pClient->m_WaitingForCursorPos)
		{
			int Row;
			int Column;
			int BytesScanned;
			if(sscanf(pBuf, "\x1B[%d;%dR%n", &Row, &Column, &BytesScanned) != 2)
			{
				log_error("ssh", "failed to read cursor input, invalid escape sequence");
				return BufSize - 1;
			}

			pClient->m_CursorPos.x = Column;
			pClient->m_CursorPos.y = Row;
			pClient->m_WaitingForCursorPos = false;
			log_info("ssh", "got cursor pos x=%d y=%d", Column, Row);

			// yes we ask the client for the cursor position
			// then we force the clients position to that position
			// this is to double ensure we are synced
			// because by the time we exchanged the position the client
			// could have drifted again
			pClient->SendCursorPos(pClient->m_CursorPos);

			// skip the sequence
			return BytesScanned - 1;
		}
	}
	if(BufSize >= 3 && pBuf[1] == 91)
	{
		if(pBuf[2] == 65) // arrow key up
		{
			if(pClient->m_Mode == EClientMode::PROMPT)
			{
				pClient->ResetCompletion();
				pClient->SetInput(pClient->PrevInputFromHistory());
			}
			else if(pClient->m_Mode == EClientMode::HISTORY_SEARCH)
			{
				pClient->m_HistorySearchScroll++;
			}

			// skip the sequence
			return 2;
		}
		else if(pBuf[2] == 66) // arrow key down
		{
			if(pClient->m_Mode == EClientMode::PROMPT)
			{
				pClient->ResetCompletion();
				pClient->SetInput(pClient->NextInputFromHistory());
			}
			else if(pClient->m_Mode == EClientMode::HISTORY_SEARCH)
			{
				pClient->m_HistorySearchScroll--;
			}

			// skip the sequence
			return 2;
		}
		else if(pBuf[2] == 68) // arrow key left
		{
			if(!pClient->CursorMoveLeft())
				pClient->SendBell();
			pClient->SendCursorPos(pClient->m_CursorPos);

			// skip the sequence
			return 2;
		}
		else if(pBuf[2] == 67) // arrow key right
		{
			if(!pClient->CursorMoveRight())
				pClient->SendBell();
			pClient->SendCursorPos(pClient->m_CursorPos);

			// skip the sequence
			return 2;
		}
		else if(pBuf[2] == 90) // shift+tab
		{
			pClient->CompleteCommands(true);

			// skip the sequence
			return 2;
		}
	}
	if(BufSize >= 1)
	{
		// alt+d
		if(pBuf[1] == 100)
		{
			if(!pClient->DeleteWordAtCursor())
				pClient->SendBell();
			pClient->SendCursorPos(pClient->m_CursorPos);

			return 1;
		}
	}

	// TODO: partially skipping unknown escape sequences will result into
	//       parsing the remaining bytes as something else
	//       which can cause some weird bugs
	return 0;
}

void CSshServer::TryProcessCurrentInput(CSshClient *pClient)
{
	if(!pClient->m_Buffer.Size())
		return;

	// TODO: it is not ideal that every return statement needs a explicit buffer clear
	//       it is easy to introduce read twice errors with this design
	//       is there something that can be done about that?
	//       give the function a return type that is how many bytes were consumed
	//       or bool clear maybe and then do it in the outer scope
	//       and the compiler will help us on the return statement to do things correctly
	//       and think about every case

	// TODO: none of the buffering and partial reads are implemented yet
	//       ideally only simple single byte printable ascii characters should be handled instantly
	//       and all the multi byte utf and escape sequences should properly be buf size checked
	//       and potentially not clear the buffer if incomplete and wait for more data
	//       if the data is too slow to arrive throw a timeout error on a partial read of
	//       an expectected multi byte sequence

	const char *pBuf = (const char *)pClient->m_Buffer.Data();
	size_t BufSize = pClient->m_Buffer.Size();
	ssh_channel Channel = pClient->m_Channel;

	// catch reaching the buffer size limit early
	// and drop all input except it is a delete instruction
	if(str_length(pClient->m_aInput) + BufSize > (int)sizeof(pClient->m_aInput) - 10)
	{
		// do not allow multiple characters at once to keep things simple
		if(BufSize > 1)
		{
			pClient->SendBell();
			pClient->m_Buffer.Clear();
			return;
		}

		// allow operations that clear the input
		char Chr = pBuf[0];
		bool Whitelisted =
			Chr == KEY_ENTER ||
			Chr == KEY_CTRL_U ||
			Chr == KEY_CTRL_K ||
			Chr == KEY_CTRL_C ||
			Chr == KEY_CTRL_D ||
			Chr == KEY_DEL ||
			Chr == KEY_BACKSPACE;
		if(!Whitelisted)
		{
			pClient->SendBell();
			pClient->m_Buffer.Clear();
			return;
		}
	}

	// TODO: for waiting on more data this loop is not ideal
	//       because when we do not call m_Buffer.Clear() we did potentially
	//       already handle previous bytes in the loop

	for(size_t i = 0; i < BufSize; i++)
	{
		char Byte = pBuf[i];
		if(Byte == KEY_ENTER)
		{
			// this if statement is a bit ugly move the mode somewhere else
			if(pClient->m_Mode == EClientMode::HISTORY_SEARCH)
			{
				pClient->DisableAltBuf();
				pClient->m_Mode = EClientMode::PROMPT;
				pClient->SendCursorPos(pClient->m_CursorPos);
				if(pClient->m_pHistorySearchMatch)
				{
					pClient->SetInput(pClient->m_pHistorySearchMatch);
				}
				else
				{
					pClient->ResendPrompt();
					pClient->SendCursorPos(pClient->m_CursorPos);
				}

				continue;
			}

			pClient->ResetCompletion();

			const char *pCmd = pClient->m_aInput;
			if(pCmd[0])
			{
				if(!str_utf8_check(pCmd))
				{
					log_warn("ssh", "cid=%d sent invalid utf-8 command", pClient->m_ClientId);
					ssh_channel_write(Channel, "\r\ninvalid utf-8", 16);
					pClient->m_CursorPos.y++;
					pClient->m_aInput[0] = '\0';
					pClient->NewPrompt();
					continue;
				}

				log_info("ssh", "cid=%d cmd='%s'", pClient->m_ClientId, pCmd);
				pClient->AddToInputHistory(pCmd);

				// TODO: how bad is it that these are fake commands?
				//       meaning they have to be a full match
				//       spaces, semicolons and arguments are not supported
				//       alternatively we could also register them as actual commands in the console
				//       and run this code in the callback
				if(!str_comp(pCmd, "logout") || !str_comp(pCmd, "exit"))
				{
					OnClientDisconnect(pClient->m_ClientId, "logout");
					pClient->m_Buffer.Clear();
					return;
				}
				else if(!str_comp(pCmd, "w") || !str_comp(pCmd, "who"))
				{
					CSshLogger Logger(this, pClient->m_ClientId, log_get_scope_logger());
					CLogScope Scope(&Logger);
					ListConnections();
				}
				else if(!str_comp(pCmd, "x")) // FIXME: remove debug
				{
					CSshLogger Logger(this, pClient->m_ClientId, log_get_scope_logger());
					CLogScope Scope(&Logger);
					// log_info("ssh", "debug command aaaaaaaaaaaaa aaaxxxxxxxxxxxT");
					// log_info("ssh", "new\nline that would be long\nbuline breakslolxx");
					// log_info("ssh", "hello world");
					log_info("ssh", "✅✅✅✅✅✅✅✅✅✅✅✅✅✅✅");
					pClient->RequestCursorPos();
				}
				else
				{
					CSshLogger Logger(this, pClient->m_ClientId, log_get_scope_logger());
					CLogScope Scope(&Logger);
					ExecuteRconLine(pClient, pCmd);
				}
			}

			pClient->m_aInput[0] = '\0';
			pClient->NewPrompt();
			continue;
		}
		else if(Byte == KEY_CTRL_A)
		{
			pClient->ClearCompletionPreview();
			pClient->SetCursorPosToPromptStart();
			pClient->SendCursorPos(pClient->m_CursorPos);
			continue;
		}
		else if(Byte == KEY_CTRL_E)
		{
			pClient->ClearCompletionPreview();
			pClient->m_CursorPos.x = pClient->PromptLength() + StringTerminalWidth(pClient->m_aInput, &m_UnicodeWidthState);
			pClient->m_InputIdx = str_length(pClient->m_aInput);
			pClient->SendCursorPos(pClient->m_CursorPos);
			continue;
		}
		else if(Byte == KEY_CTRL_R)
		{
			if(pClient->m_Mode == EClientMode::PROMPT)
			{
				pClient->m_Mode = EClientMode::HISTORY_SEARCH;
				pClient->EnableAltBuf();
				str_copy(pClient->m_aPromptInput, pClient->m_aInput);
				pClient->m_aInput[0] = '\0';
				pClient->ClearPrompt();
				pClient->m_CursorPos.y = pClient->m_Term.m_Height - (pClient->PromptHeight() - 1);
				pClient->m_HistorySearchScroll = 0;
				pClient->RenderHistorySearch();
			}
			else
			{
				pClient->SendBell();
			}
			continue;
		}
		else if(Byte == KEY_CTRL_K)
		{
			pClient->ResetCompletion();
			bool CursorAtStart = pClient->m_InputIdx == 0;
			if(CursorAtStart)
			{
				str_copy(pClient->m_aYankBuffer, pClient->m_aInput);
				pClient->m_aInput[0] = '\0';
				pClient->ClearPrompt();
			}
			else
			{
				str_copy(pClient->m_aYankBuffer, pClient->m_aInput + pClient->m_InputIdx);
				pClient->m_aInput[pClient->m_InputIdx] = '\0';
				ssh_channel_write(Channel, "\033[K", str_length("\033[K"));
			}
			continue;
		}
		else if(Byte == KEY_CTRL_U)
		{
			pClient->ResetCompletion();
			bool CursorAtEnd = pClient->m_aInput[pClient->m_InputIdx] == '\0';
			if(CursorAtEnd)
			{
				str_copy(pClient->m_aYankBuffer, pClient->m_aInput);
				pClient->m_aInput[0] = '\0';
				pClient->ClearPrompt();
			}
			else
			{
				char aRight[sizeof(CSshClient::m_aInput)];
				str_copy(aRight, pClient->m_aInput + pClient->m_InputIdx);
				str_copy(pClient->m_aYankBuffer, pClient->m_aInput, pClient->m_InputIdx + 1);
				pClient->ClearPrompt();
				ssh_channel_write(Channel, aRight, str_length(aRight));
				str_copy(pClient->m_aInput, aRight);
				pClient->SendCursorPos(pClient->m_CursorPos);
			}
			continue;
		}
		else if(Byte == KEY_CTRL_Y)
		{
			pClient->ResetCompletion();
			if(!pClient->InsertToInputAtCursor(pClient->m_aYankBuffer))
				pClient->SendBell();
			continue;
		}
		else if(Byte == KEY_CTRL_C)
		{
			if(pClient->m_Mode == EClientMode::HISTORY_SEARCH)
			{
				pClient->AbortHistorySearch();
			}
			else
			{
				pClient->ResetCompletion();

				ssh_channel_write(Channel, "^C", 2);

				if(pClient->m_aInput[0] == '\0')
				{
					const char *pMsg = "\r\nUse ctrl+d or 'exit' to quit";
					ssh_channel_write(Channel, pMsg, str_length(pMsg));
					pClient->m_CursorPos.y++;
				}

				pClient->m_aInput[0] = '\0';
				pClient->NewPrompt();
			}
			continue;
		}
		else if(Byte == KEY_CTRL_D)
		{
			// silently ignore ctrl+d if there is still input
			if(pClient->m_aInput[0])
				continue;

			if(pClient->m_Mode == EClientMode::HISTORY_SEARCH)
			{
				pClient->AbortHistorySearch();
			}
			else
			{
				OnClientDisconnect(pClient->m_ClientId, "logout");
				pClient->m_Buffer.Clear();
				return;
			}
		}
		else if(Byte == KEY_CTRL_L)
		{
			// silently ignore ctrl+l if there is still input
			if(pClient->m_aInput[0])
				continue;

			pClient->ClearCompletionPreview();
			pClient->SendClearScreen();
			pClient->m_CursorPos.y = 1;
			pClient->SendCursorPos(pClient->m_CursorPos);
			pClient->ClearPrompt();
			continue;
		}
		else if(Byte == KEY_BACKSPACE || Byte == KEY_DEL)
		{
			pClient->ResetCompletion();
			int Idx = pClient->m_InputIdx;
			if(Idx == 0)
			{
				ssh_channel_write(pClient->m_Channel, "\a", 1);
				continue;
			}
			pClient->CursorMoveLeft();
			// delete at the end of the input
			if(pClient->m_aInput[Idx] == '\0')
			{
				if(pClient->m_aInput[0])
				{
					ssh_channel_write(pClient->m_Channel, "\b \b", 3);
					pClient->SendCursorPos(pClient->m_CursorPos);
				}
				else
					pClient->SendBell();
				pClient->m_aInput[pClient->m_InputIdx] = '\0';
			}
			else
			{
				// delete with cursor inside of the input

				char aRight[2048];
				// TODO: bound check this
				str_copy(aRight, pClient->m_aInput + Idx);

				int Size = Idx - pClient->m_InputIdx;

				// server side we need to shift the entire input
				// TODO: bound check this
				str_copy(pClient->m_aInput + Idx - Size, aRight, sizeof(pClient->m_aInput) - Idx);

				// simple ascii
				if(Size == 1)
				{
					// client side del is just sending a backspace
					ssh_channel_write(pClient->m_Channel, "\b \b", 3);

					// clear everything from the cursor till the end
					ssh_channel_write(Channel, "\33[0K", str_length("\33[0K"));

					// rewrite shifted line
					ssh_channel_write(Channel, aRight, str_length(aRight));
				}
				else
				{
					// with multi byte and multi span col width utf8 things can get messy
					// to avoid any client desync we just rewrite the entire line
					// might not be as performant but it only affects inline utf8 editing so its fine

					ssh_channel_write(Channel, "\r\033[2K", 6);
					ssh_channel_write(Channel, pClient->PromptStr(), str_length(pClient->PromptStr()));
					ssh_channel_write(Channel, pClient->m_aInput, str_length(pClient->m_aInput));
				}

				// move cursor back into the input after rewriting the line
				pClient->SendCursorPos(pClient->m_CursorPos);
			}
			continue;
		}
		else if(Byte == KEY_TAB)
		{
			pClient->ClearCompletionPreview();
			pClient->CompleteCommands(false);
			continue;
		}
		else if(Byte == 27) // escape sequence
		{
			pClient->ClearCompletionPreview();
			int SkipBytes = TryProcessEscapeSequence(pClient, pBuf + i, BufSize - i);
			i += SkipBytes;
			continue;
		}
		else if(CSshClient::IsSimpleAsciiLetter(Byte))
		{
			pClient->ResetCompletion();
			if(!pClient->AddSingleAsciiLetterToInput(Byte))
				pClient->SendBell();

			if(pClient->m_Mode == EClientMode::HISTORY_SEARCH)
			{
				pClient->m_HistorySearchScroll = 0;
			}
			continue;
		}
		else // assume multi byte unicode
		{
			pClient->ResetCompletion();

			const char *pStr = pBuf + i;
			int CodePoint = str_utf8_decode(&pStr);
			if(CodePoint == 0)
			{
				// the client sending a null byte would be weird huh?
				// lets silently ignore it for now
				continue;
			}
			// the read buffer is null terminated
			// str_utf8_decode() might read beyond the buffer size
			// but then it will hit the null term and the utf8 decode will
			// fail if another byte was expected
			// in that case we have possibly partial utf8 that is yet
			// to be fully sent over the network or another error
			if(CodePoint == -1)
			{
				// TODO: test this branch, this might require some hacking to actually hit it and send partial utf8

				log_error("ssh", "cid=%d sent invalid utf-8 which might be partial but that is not supported yet", pClient->m_ClientId);
				log_error("ssh", "       or it might be a unsupported escape sequence");
				log_error("ssh", "       amount of bytes in recv buffer: %" PRIzu, BufSize);
				log_error("ssh", "       invalid utf-8 at index: %" PRIzu, i);
				log_error("ssh", "       full buffer:");
				for(size_t DbgBufIdx = 0; DbgBufIdx < BufSize; DbgBufIdx++)
				{
					const char *pNote = "";
					if(DbgBufIdx == i)
						pNote = " <-- invalid utf-8 starts here";
					log_error("ssh", "         buf[%" PRIzu "] = %d%s", DbgBufIdx, (unsigned char)pBuf[DbgBufIdx], pNote);
				}

				// clear out the previous bytes we successfully handled already
				// so they do not get processed again
				pClient->m_Buffer.TrimLeading(i);

				// and then we return without calling Clear() on the buffer so the partial utf8
				// stays for the next tick

				// TODO: here we need to add a timeout because we can not wait forever for partial utf8

				return;
			}

			int LengthInBytes = pStr - (pBuf + i);
			char aUtf8Str[32] = {0};
			str_copy(aUtf8Str, pBuf + i);
			aUtf8Str[LengthInBytes] = '\0';
			if(!pClient->InsertToInputAtCursor(aUtf8Str))
				pClient->SendBell();

			// skip n bytes in loop
			// but -1 because the for loop already does i++
			i += LengthInBytes - 1;
		}
	}

	// TODO: this should not be here
	//       we also need to blacklist some commands during special modes

	if(pClient->m_Mode == EClientMode::HISTORY_SEARCH)
		pClient->RenderHistorySearch();

	// TODO: only clear if we actually read the data
	pClient->m_Buffer.Clear();
}

void CSshServer::ReadNewInput(CSshClient *pClient)
{
	ssh_channel Channel = pClient->m_Channel;
	unsigned char aBuf[256] = {0};

	// TODO: so far in all my tests the non blocking read gave me all escape sequences
	//       as a whole chunk
	//       and also all utf8 characters
	//       but i think this is not guaranteed
	//       which makes everything just so much more complicated
	//       in theory all reads should be collected in a buffer and then
	//       parsed there
	//       the question is just when do we start parsing
	//       ---
	//       UPDATE: there is the buffer now but still no flushing or timeouts

	int n = ssh_channel_read_nonblocking(Channel, aBuf, sizeof(aBuf), 0);
	if(n == SSH_EOF)
	{
		OnClientDisconnect(pClient->m_ClientId, "eof");
		return;
	}
	if(n == SSH_ERROR)
	{
		OnClientDisconnect(pClient->m_ClientId, "channel read error");
		return;
	}
	if(n == SSH_AGAIN || n == 0)
	{
		return;
	}
	pClient->m_Buffer.AddBytes(aBuf, n);
	TryProcessCurrentInput(pClient);

	if(!std::isprint(aBuf[0]))
	{
		log_info("ssh", "-----");
		for(int i = 0; i < n; i++)
		{
			log_info("ssh", "debug input buf[%d/%d] = %d", i, n, aBuf[i]);
		}
		log_info("ssh", "cursor x=%d y=%d term w=%d h=%d", pClient->m_CursorPos.x, pClient->m_CursorPos.y, pClient->m_Term.m_Width, pClient->m_Term.m_Height);
		log_info("ssh", "input bufidx=%d '%s'", pClient->m_InputIdx, pClient->m_aInput);
	}
}

void CSshServer::GenerateHostKeyIfMissing()
{
#ifdef CONF_PLATFORM_LINUX
	// TODO: make this cross platform and use openssh C++ code
	int Ret = system("bash -c \"[[ -f ssh_host_rsa_key ]] || ssh-keygen -t rsa -b 4096 -f ssh_host_rsa_key -N ''\"");
	if(Ret != 0)
	{
		str_copy(m_aError, "host key generation failed");
		log_error("ssh", "failed to generate host key");
	}
#endif
}

void CSshServer::Init(CConfig *pConfig, IConsole *pConsole, IStorage *pStorage)
{
	m_pConfig = pConfig;
	m_pConsole = pConsole;
	m_pStorage = pStorage;

	if(!g_Config.m_SvSsh)
		return;

	unicode_width_init(&m_UnicodeWidthState);

	log_info("ssh", "libssh %s", ssh_version(0));

	m_Bind = ssh_bind_new();
	if(m_Bind == nullptr)
	{
		str_copy(m_aError, "failed to bind");
		log_error("ssh", "failed to create ssh_bind");
		return;
	}

	GenerateHostKeyIfMissing();

	char aPort[32];
	str_format(aPort, sizeof(aPort), "%d", g_Config.m_SvSshPort);

	ssh_bind_options_set(m_Bind, SSH_BIND_OPTIONS_BINDADDR, "0.0.0.0");
	ssh_bind_options_set(m_Bind, SSH_BIND_OPTIONS_BINDPORT_STR, aPort);
	ssh_bind_options_set(m_Bind, SSH_BIND_OPTIONS_RSAKEY, HOSTKEY_FILE);

	int Ok = ssh_bind_listen(m_Bind);
	if(Ok < 0)
	{
		str_copy(m_aError, "listen error");
		log_error("ssh", "listen error: %s", ssh_get_error(m_Bind));
		ssh_bind_free(m_Bind);
		return;
	}

	ssh_bind_set_blocking(m_Bind, 0);

	int RawFd = ssh_bind_get_fd(m_Bind);
	fcntl(RawFd, F_SETFL, O_NONBLOCK);

	log_info("ssh", "listening on 0.0.0.0:%s", aPort);

	// placeholder history for testing the search feature
	// in the future we can load it from a file here
	// so the history survives server restarts
	// which is super handy for local development where
	// the server restarts all the time
	const char *apHistory[] = {
		"sv_shutdown_when_empty 0",
		"say foo bar baz",
		"echo hello world",
		"say foo;say bar;say baz"};
	for(const char *pLine : apHistory)
	{
		auto &Entry = m_InputHistory.emplace_back();
		str_copy(Entry.data(), pLine, Entry.size());
	}
}

std::optional<int> CSshServer::FindFreeSlot()
{
	for(int i = 0; i < MAX_SSH_CLIENTS; i++)
		if(m_apClients[i] == nullptr)
			return i;
	return std::nullopt;
}

int CSshServer::AuthPasswordCallback(ssh_session Session, const char *pUsername, const char *pPassword, void *pUserData)
{
	CSshClient::CCallbackCtx *pCtx = static_cast<CSshClient::CCallbackCtx *>(pUserData);

	if(g_Config.m_SvRconPassword[0] == '\0')
		return SSH_AUTH_DENIED;
	if(!g_Config.m_SvSshPasswordAuthentication)
		return SSH_AUTH_DENIED;

	bool AdminUsername = str_comp(pUsername, "root") == 0 || str_comp(pUsername, "admin") == 0 || str_comp(pUsername, "default_admin") == 0;
	if(AdminUsername && str_comp(pPassword, g_Config.m_SvRconPassword) == 0)
	{
		pCtx->m_pClient->m_Authenticated = true;
		return SSH_AUTH_SUCCESS;
	}

	return SSH_AUTH_DENIED;
}

int CSshServer::AuthPubkeyCallback(ssh_session Session, const char *pUsername, struct ssh_key_struct *pClientPubKey, char SignatureState, void *pUserData)
{
	CSshClient::CCallbackCtx *pCtx = static_cast<CSshClient::CCallbackCtx *>(pUserData);
	IStorage *pStorage = pCtx->m_pServer->Storage();

	// TODO: the pUsername is not checked so a valid pub key can log into any name.
	//       is that convenient for the admin or insecure?
	//       right now there is only admin rank anyways

	const char *pAuthorizedKeysFile = "authorized_keys";

	CLineReader LineReader;
	if(!LineReader.OpenFile(pStorage->OpenFile(pAuthorizedKeysFile, IOFLAG_READ, IStorage::TYPE_SAVE)))
	{
		// no authorized_keys file
		return SSH_AUTH_DENIED;
	}

	bool Match = false;

	while(const char *pLine = LineReader.Get())
	{
		const char *pStr = str_skip_whitespaces_const(pLine);
		if(pStr[0] == '#')
			continue;

		char aKeyType[512] = {0};
		bool InvalidKey = false;
		size_t i;
		for(i = 0; i < sizeof(aKeyType); i++)
		{
			if(pStr[i] == ' ')
				break;
			if(pStr[i] == '\0')
			{
				InvalidKey = true;
				break;
			}

			aKeyType[i] = pStr[i];
		}
		if(InvalidKey)
			continue;
		pStr = str_skip_whitespaces_const(pStr + i);
		char aKeyBase64[2048];
		str_copy(aKeyBase64, pStr);
		char *pKeyEnd = str_skip_to_whitespace(aKeyBase64);
		if(pKeyEnd)
			*pKeyEnd = '\0';

		enum ssh_keytypes_e KeyType = ssh_key_type_from_name(aKeyType);
		if(KeyType == SSH_KEYTYPE_UNKNOWN)
		{
			continue;
		}

		ssh_key FileKey = nullptr;
		// Import public key from base64 string
		int Rc = ssh_pki_import_pubkey_base64(aKeyBase64, KeyType, &FileKey);
		if(Rc == SSH_OK && FileKey != nullptr)
		{
			// Compare the client's key with the key from the file
			if(ssh_key_cmp(pClientPubKey, FileKey, SSH_KEY_CMP_PUBLIC) == 0)
			{
				Match = true;
				ssh_key_free(FileKey);
				break;
			}
			ssh_key_free(FileKey);
		}
		else
		{
			log_error("ssh", "failed to read public key!");
			log_error("ssh", " return_code=%d", Rc);
			log_error("ssh", " key_type=%d", KeyType);
			log_error("ssh", " key='%s'", aKeyBase64);
		}
	}

	if(Match)
	{
		if(SignatureState == SSH_PUBLICKEY_STATE_NONE)
		{
			// non verified probe
			return SSH_AUTH_SUCCESS;
		}
		else if(SignatureState == SSH_PUBLICKEY_STATE_VALID)
		{
			// verified ownership -> authenticate
			pCtx->m_pClient->m_Authenticated = true;
			return SSH_AUTH_SUCCESS;
		}
	}

	return SSH_AUTH_DENIED;
}

ssh_channel CSshServer::ChannelOpenRequestSessionCallback(ssh_session Session, void *pUserData)
{
	CSshClient::CCallbackCtx *pCtx = static_cast<CSshClient::CCallbackCtx *>(pUserData);

	if(!pCtx->m_pClient->m_Authenticated)
	{
		return nullptr;
	}

	ssh_channel Channel = ssh_channel_new(Session);
	if(Channel == nullptr)
	{
		log_error("ssh", "failed to create new channel");
		return nullptr;
	}

	pCtx->m_pClient->m_Channel = Channel;

	pCtx->m_pClient->m_ChannelCallback = {
		.userdata = pCtx,
		.channel_pty_request_function = ChannelPtyRequestCallback,
		.channel_shell_request_function = ChannelShellRequestCallback,
		.channel_pty_window_change_function = ChannelPtyWindowChangeCallback,
		.channel_exec_request_function = ChannelExecRequestCallback,
	};
	ssh_callbacks_init(&pCtx->m_pClient->m_ChannelCallback);
	ssh_set_channel_callbacks(Channel, &pCtx->m_pClient->m_ChannelCallback);

	return Channel;
}

int CSshServer::ChannelPtyRequestCallback(ssh_session Session, ssh_channel Channel, const char *pTerm, int Width, int Height, int PxWidth, int PwHeight, void *pUserData)
{
	CSshClient::CCallbackCtx *pCtx = static_cast<CSshClient::CCallbackCtx *>(pUserData);

	if(!pCtx->m_pClient->m_Authenticated)
	{
		return SSH_ERROR;
	}

	pCtx->m_pClient->m_Term.m_Width = Width;
	pCtx->m_pClient->m_Term.m_Height = Height;

	// we don't support pty yet, only shell for now
	// but we need to return OK here otherwise the client shows an error

	// update: actually I feel like we require a pty by now :D
	//         because we use height and width and ansi escape sequences
	//         i feel like that is exactly what a pty is, but not sure
	//         maybe we should drop the connection if there was no pty request
	//         or have a fallback to a less fancy shell

	return SSH_OK;
}

int CSshServer::ChannelPtyWindowChangeCallback(ssh_session Session, ssh_channel Channel, int Width, int Height, int PxWidth, int PwHeight, void *pUserData)
{
	CSshClient::CCallbackCtx *pCtx = static_cast<CSshClient::CCallbackCtx *>(pUserData);
	pCtx->m_pClient->m_Term.m_Width = Width;
	pCtx->m_pClient->m_Term.m_Height = Height;
	return SSH_OK;
}

int CSshServer::ChannelExecRequestCallback(ssh_session Session, ssh_channel Channel, const char *pCommand, void *pUserData)
{
	CSshClient::CCallbackCtx *pCtx = static_cast<CSshClient::CCallbackCtx *>(pUserData);

	if(!pCtx->m_pClient->m_Authenticated)
	{
		return SSH_ERROR;
	}

	CSshLogger Logger(pCtx->m_pServer, pCtx->m_pClient->m_ClientId, log_get_scope_logger());
	CLogScope Scope(&Logger);
	pCtx->m_pServer->ExecuteRconLine(pCtx->m_pClient, pCommand);
	ssh_channel_write(pCtx->m_pClient->m_Channel, "\r\n", 2);

	pCtx->m_pClient->m_Dropped = true;

	return SSH_OK;
}

int CSshServer::ChannelShellRequestCallback(ssh_session Session, ssh_channel Channel, void *pUserData)
{
	CSshClient::CCallbackCtx *pCtx = static_cast<CSshClient::CCallbackCtx *>(pUserData);

	if(!pCtx->m_pClient->m_Authenticated)
	{
		return SSH_ERROR;
	}

	pCtx->m_pClient->m_Channel = Channel;
	pCtx->m_pClient->m_ShellReady = true;

	const char *pBanner =
		"##################################\r\n"
		"#                                #\r\n"
		"#  welcome to the rcon console!  #\r\n"
		"#                                #\r\n"
		"##################################\r\n";
	ssh_channel_write(Channel, pBanner, str_length(pBanner));
	pCtx->m_pClient->NewPrompt();
	pCtx->m_pClient->RequestCursorPos();

	return SSH_OK;
}

void CSshServer::OnClientConnect(int ClientId, ssh_session Session)
{
	dbg_assert(m_apClients[ClientId] == nullptr, "ssh_server connect failed ClientId %d reused", ClientId);

	socket_t Socket = ssh_get_fd(Session);
	struct sockaddr_storage SockAddr;
	socklen_t SockAddrLen = sizeof(SockAddr);
	if(getpeername(Socket, (struct sockaddr *)&SockAddr, &SockAddrLen))
	{
		dbg_assert_failed("failed to get ssh connection address");
	}

	CSshClient *pClient = new CSshClient(ClientId, Session);
	pClient->m_JoinTime = time_get();

	SockaddrToNetaddr((sockaddr *)&SockAddr, SockAddrLen, &pClient->m_Addr);
	if(net_addr_comp(&pClient->m_Addr, &NETADDR_ZEROED) == 0)
	{
		dbg_assert_failed("failed to convert ssh client address");
	}

	char aAddr[NETADDR_MAXSTRSIZE];
	net_addr_str(&pClient->m_Addr, aAddr, sizeof(aAddr), true);

	// TODO: this prints for every connection attempt so attackers without the password can spam the log
	//       and occupy client ids
	//       so ideally there would be a different connection pool just for the before auth state
	log_info("ssh", "new connection cid=%d addr=<{%s}>", ClientId, aAddr);

	pClient->m_InputHistory = m_InputHistory;
	pClient->m_HistoryIdx = pClient->m_InputHistory.size();

	pClient->m_CallbackCtx = {
		.m_pClient = pClient,
		.m_pServer = this};

	pClient->m_ServerCallback = {
		.userdata = &pClient->m_CallbackCtx,
		.auth_password_function = AuthPasswordCallback,
		.auth_pubkey_function = AuthPubkeyCallback,
		.channel_open_request_session_function = ChannelOpenRequestSessionCallback,
	};
	ssh_callbacks_init(&pClient->m_ServerCallback);

	int AuthMethods = SSH_AUTH_METHOD_PUBLICKEY;
	if(g_Config.m_SvSshPasswordAuthentication)
		AuthMethods |= SSH_AUTH_METHOD_PASSWORD;
	ssh_set_auth_methods(Session, AuthMethods);

	ssh_set_server_callbacks(Session, &pClient->m_ServerCallback);

	m_apClients[ClientId] = pClient;
}

void CSshServer::OnClientDisconnect(int ClientId, const char *pReason)
{
	CSshClient *pClient = m_apClients[ClientId];
	if(!pClient)
		return;

	log_info("ssh", "disconnect cid=%d reason='%s'", ClientId, pReason);
	MergeInputHistory(pClient);

	ssh_channel Channel = pClient->m_Channel;
	if(Channel)
	{
		ssh_channel_free(Channel);
	}
	ssh_session Session = m_apClients[ClientId]->m_Session;
	if(Session)
	{
		ssh_disconnect(Session);
		ssh_free(Session);
	}

	delete m_apClients[ClientId];
	m_apClients[ClientId] = nullptr;
}

void CSshServer::AcceptNewConnections()
{
	ssh_session NewSession = ssh_new();

	int Rc = ssh_bind_accept(m_Bind, NewSession);
	if(Rc == SSH_ERROR)
	{
		ssh_free(NewSession);
		return;
	}

	auto Slot = FindFreeSlot();
	if(!Slot.has_value())
	{
		ssh_disconnect(NewSession);
		ssh_free(NewSession);
		return;
	}
	int ClientId = Slot.value();

	ssh_set_blocking(NewSession, 0);

	if(ssh_handle_key_exchange(NewSession) == SSH_AGAIN ||
		ssh_handle_key_exchange(NewSession) == SSH_OK)
	{
		OnClientConnect(ClientId, NewSession);
	}
	else
	{
		ssh_free(NewSession);
	}
}

void CSshServer::Update()
{
	if(m_aError[0])
		return;
	if(m_Bind == nullptr)
		return;
	if(g_Config.m_SvSsh == 0)
		return;

	AcceptNewConnections();

	for(CSshClient *pClient : m_apClients)
	{
		if(!pClient)
			continue;

		ProcessMessage(pClient);

		if(pClient->m_Dropped)
		{
			OnClientDisconnect(pClient->m_ClientId);
			continue;
		}

		// TODO: should we also timeout sessions without keepalive?
		// TODO: this is not optimal since during the password prompt we can not really send a message
		//       there is no channel yet so the user can still be typing a password after already being disconnected
		//       just to see "bye bye" in the end
		//       but we also can not block a slot for everyone that is in the password state
		//       i wonder how econ does it
		//       can econ be denial of serviced with a bunch of clients in the asking for password state?
		if(!pClient->m_ShellReady)
		{
			int64_t ConnectedSinceSeconds = (time_get() - pClient->m_JoinTime) / time_freq();
			if(ConnectedSinceSeconds > 10)
			{
				log_info("ssh", "cid=%d did not get shell ready fast enough and timed out", pClient->m_ClientId);
				OnClientDisconnect(pClient->m_ClientId, "timeout");
			}
			continue;
		}

		if(!ssh_channel_is_open(pClient->m_Channel) || ssh_channel_is_eof(pClient->m_Channel))
		{
			ssh_channel_send_eof(pClient->m_Channel);
			ssh_channel_close(pClient->m_Channel);
			OnClientDisconnect(pClient->m_ClientId);
		}

		if(!pClient->m_ShellReady)
			continue;
		if(!pClient->m_Channel)
			continue;
		if(!pClient->m_Authenticated)
			continue;

		ReadNewInput(pClient);
	}
}

void CSshServer::Shutdown()
{
	if(m_Bind != nullptr && m_aError[0] == '\0')
	{
		ssh_bind_free(m_Bind);
		m_Bind = nullptr;
	}
}

bool CSshServer::GotActiveConnections()
{
	return std::ranges::any_of(m_apClients, [](auto *pClient) { return pClient != nullptr; });
}

#endif
