// ChillerDragon 2023 - chillerbot

#if defined(CONF_CURSES_CLIENT)

#include <engine/client/serverbrowser.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>

#include <base/chillerbot/curses_colors.h>
#include <base/terminalui.h>

#include "terminalui.h"

bool CTerminalUI::SaveInputToHistoryFile(int Type, const char *pInput)
{
	char aType[128];
	str_copy(aType, GetInputModeSlug(Type), sizeof(aType));

	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/term_hist/%s.txt", aType);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
	{
		dbg_msg("term-ux", "failed to open history file '%s'", aFilename);
		return false;
	}

	io_write(File, pInput, str_length(pInput));
	io_write_newline(File);
	io_close(File);
	return true;
}

bool CTerminalUI::SaveCurrentHistoryBufferToDisk(int Type)
{
	char aType[128];
	str_copy(aType, GetInputModeSlug(Type), sizeof(aType));

	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/term_hist/%s.txt", aType);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
	{
		dbg_msg("term-ux", "failed to open history file '%s'", aFilename);
		return false;
	}

	for(int i = (INPUT_HISTORY_MAX_LEN - 1); i >= 0; i--)
	{
		if(m_aaInputHistory[Type][i][0] == '\0')
			continue;

		io_write(File, m_aaInputHistory[Type][i], str_length(m_aaInputHistory[Type][i]));
		io_write_newline(File);
	}
	io_close(File);
	return true;
}

bool CTerminalUI::LoadInputHistoryFile(int Type)
{
	char aType[128];
	str_copy(aType, GetInputModeSlug(Type), sizeof(aType));

	char aFilename[1024];
	str_format(aFilename, sizeof(aFilename), "chillerbot/term_hist/%s.txt", aType);

	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
	{
		// do not bother the user if the file does not exist yet
		// dbg_msg("term-ux", "failed to read history file '%s'", aFilename);
		return false;
	}

	const char *pLine;
	int i = (INPUT_HISTORY_MAX_LEN - 1);
	CLineReader LineReader = CLineReader();

	if(!LineReader.OpenFile(File))
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "failed to open '%s'", g_Config.m_ClPasswordFile);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", aBuf);
		return false;
	}

	while((pLine = LineReader.Get()))
	{
		if(pLine[0] == '\0')
			continue;

		if(i <= 0)
			break;

		dbg_msg("term-ux", "hist type=%s i=%d max=%d line=%s", aType, i, INPUT_HISTORY_MAX_LEN, pLine);
		str_copy(m_aaInputHistory[Type][i], pLine, sizeof(m_aaInputHistory[Type][i]));
		i--;
	}

	// less than INPUT_HISTORY_MAX_LEN entries
	// keeps the latest element still empty
	// which makes the history search think its empty
	if(i > 0)
		ShiftHistoryFromHighToLow(Type);
	return true;
}

void CTerminalUI::ShiftHistoryFromHighToLow(int Type)
{
	// if there is no element at the top we wont shift
	// to avoid infinite loops
	// if your hist is starting at offset 2 you did somehting wrong anyways
	if(m_aaInputHistory[Type][(INPUT_HISTORY_MAX_LEN - 1)][0] == '\0')
		return;
	while(m_aaInputHistory[Type][0][0] == '\0')
		for(int i = 0; i < INPUT_HISTORY_MAX_LEN; i++)
			str_copy(m_aaInputHistory[Type][i], m_aaInputHistory[Type][i + 1], sizeof(m_aaInputHistory[Type][i]));
}

#endif
