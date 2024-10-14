// ChillerDragon 2021 - chillerbot ux

#include <engine/config.h>
#include <engine/shared/linereader.h>
#include <engine/textrender.h>
#include <game/client/gameclient.h>

#include <base/system.h>

#include "warlist.h"

bool CWarList::OnChatCmdAdvanced(char Prefix, int ClientId, int Team, const char *pCmd, int NumArgs, const char **ppArgs, const char *pRawArgLine)
{
	char aBuf[512];
	if(!str_comp(pCmd, "search")) // "search <name can contain spaces>"
	{
		if(NumArgs != 1)
		{
			str_format(aBuf, sizeof(aBuf), "Error: expected 1 argument but got %d", NumArgs);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
			return true;
		}
		m_pClient->m_Chat.AddLine(-2, 0, "[search] fullmatch:");
		if(!SearchName(ppArgs[0], false))
		{
			m_pClient->m_Chat.AddLine(-2, 0, "[search] partial:");
			SearchName(ppArgs[0], true);
		}
	}
	else if(!str_comp(pCmd, "help"))
	{
		m_pClient->m_Chat.AddLine(-2, 0, "=== Aiodob warlist ===");
		m_pClient->m_Chat.AddLine(-2, 0, "!addwar <folder> <name>");
		m_pClient->m_Chat.AddLine(-2, 0, "!addteam <folder> <name>");
		m_pClient->m_Chat.AddLine(-2, 0, "!peace <folder>");
		m_pClient->m_Chat.AddLine(-2, 0, "!war <folder>");
		m_pClient->m_Chat.AddLine(-2, 0, "!team <folder>");
		m_pClient->m_Chat.AddLine(-2, 0, "!unfriend <folder>"); // aliases delteam, unteam
		m_pClient->m_Chat.AddLine(-2, 0, "!addreason <folder> [--force] <reason>");
		m_pClient->m_Chat.AddLine(-2, 0, "!search <name>");
		m_pClient->m_Chat.AddLine(-2, 0, "!create <war|team|neutral|traitor> <folder> [name]");
	}
	else if(!str_comp(pCmd, "create")) // "create <war|team|neutral|traitor> <folder> [name]"
	{
		if(NumArgs < 1)
		{
			m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <type>");
			return true;
		}
		if(NumArgs < 2)
		{
			m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <folder>");
			return true;
		}
		char aType[512];
		char aFolder[512];
		str_copy(aType, ppArgs[0], sizeof(aType));
		str_copy(aFolder, ppArgs[1], sizeof(aFolder));
		char aName[64];
		aName[0] = '\0';
		if(NumArgs > 2)
			str_copy(aName, ppArgs[2], sizeof(aName));
		if(str_comp(aType, "war") &&
			str_comp(aType, "team") &&
			str_comp(aType, "neutral") &&
			str_comp(aType, "traitor"))
		{
			m_pClient->m_Chat.AddLine(-2, 0, "Error type has to be one of those: <war|team|neutral|traitor>");
			return true;
		}
		const char *aTypes[] = {"war", "team", "neutral", "traitor"};
		for(const auto *CheckType : aTypes)
		{
			str_format(aBuf, sizeof(aBuf), "chillerbot/warlist/%s/%s", CheckType, aFolder);
			if(Storage()->FolderExists(aBuf, IStorage::TYPE_SAVE))
			{
				char aError[256];
				str_format(aError, sizeof(aError), "Error folder already exists: %s", aBuf);
				m_pClient->m_Chat.AddLine(-2, 0, aError);
				return true;
			}
		}
		str_format(aBuf, sizeof(aBuf), "chillerbot/warlist/%s", aType);
		if(!Storage()->CreateFolder(aBuf, IStorage::TYPE_SAVE))
		{
			str_format(aBuf, sizeof(aBuf), "Failed to create folder %s/%s", aType, aFolder);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
			return true;
		}
		str_format(aBuf, sizeof(aBuf), "chillerbot/warlist/%s/%s", aType, aFolder);
		if(!Storage()->CreateFolder(aBuf, IStorage::TYPE_SAVE))
		{
			str_format(aBuf, sizeof(aBuf), "Failed to create folder %s/%s", aType, aFolder);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
			return true;
		}
		if(!aName[0])
		{
			str_format(aBuf, sizeof(aBuf), "Created folder %s/%s", aType, aFolder);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
			return true;
		}

		if(SearchName(aName, false, true))
		{
			str_format(aBuf, sizeof(aBuf), "Error: name '%s' is already used in different folder", aName);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
			return true;
		}
		str_format(aBuf, sizeof(aBuf), "Created folder %s/%s and add name '%s'", aType, aFolder, aName);
		m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		if(!str_comp(aType, "war"))
			AddWar(aFolder, aName);
		else if(!str_comp(aType, "team"))
			AddTeam(aFolder, aName);
		else
		{
			str_format(aBuf, sizeof(aBuf), "Error: failed to add name '%s' to '%s' list (list type not supported)", aName, aType);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
		}
	}
	else if(!str_comp(pCmd, "addwar")) // "addwar <folder> <name can contain spaces>"
	{
		if(NumArgs < 1)
		{
			m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <folder>");
			return true;
		}
		if(NumArgs < 2)
		{
			m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <name>");
			return true;
		}
		char aFolder[512];
		char aName[512];
		str_copy(aFolder, ppArgs[0], sizeof(aFolder));
		str_copy(aName, ppArgs[1], sizeof(aName));
		AddWar(aFolder, aName);
	}
	else if(!str_comp(pCmd, "addteam")) // "addteam <folder> <name can contain spaces>"
	{
		if(NumArgs < 1)
		{
			m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <folder>");
			return true;
		}
		if(NumArgs < 2)
		{
			m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <name>");
			return true;
		}
		char aFolder[512];
		char aName[512];
		str_copy(aFolder, ppArgs[0], sizeof(aFolder));
		str_copy(aName, ppArgs[1], sizeof(aName));
		AddTeam(aFolder, aName);
	}
	else if(!str_comp(pCmd, "addtraitor")) // "addtraitor <folder> <name can contain spaces>"
	{
		if(NumArgs < 1)
		{
			m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <folder>");
			return true;
		}
		if(NumArgs < 2)
		{
			m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <name>");
			return true;
		}
		char aFolder[512];
		char aName[512];
		str_copy(aFolder, ppArgs[0], sizeof(aFolder));
		str_copy(aName, ppArgs[1], sizeof(aName));
		char aFilename[1024];
		str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/traitor/%s/names.txt", aFolder);
		IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_APPEND, IStorage::TYPE_SAVE);
		if(!File)
		{
			str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aFilename);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
			return true;
		}

		io_write(File, aName, str_length(aName));
		io_write_newline(File);
		io_close(File);

		str_format(aBuf, sizeof(aBuf), "Added '%s' to the folder %s", aName, aFolder);
		ReloadList();
		m_pClient->m_Chat.AddLine(-2, 0, aBuf);
	}
	else if(!str_comp(pCmd, "peace")) // "peace <folder>"
	{
		if(NumArgs < 1)
		{
			m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <folder>");
			return true;
		}
		char aFolder[512];
		str_copy(aFolder, ppArgs[0], sizeof(aFolder));
		char aPath[1024];
		char aPeacePath[1024];
		str_format(aPath, sizeof(aPath), "chillerbot/warlist/war/%s/names.txt", aFolder);
		str_format(aPeacePath, sizeof(aPeacePath), "chillerbot/warlist/neutral/%s/names.txt", aFolder);
		IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
		if(!File)
		{
			str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aPath);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
			return true;
		}
		io_close(File);
		File = Storage()->OpenFile(aPeacePath, IOFLAG_READ, IStorage::TYPE_SAVE);
		if(File)
		{
			str_format(aBuf, sizeof(aBuf), "Peace entry already exists '%s'", aPeacePath);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
			io_close(File);
			return true;
		}

		str_format(aPath, sizeof(aPath), "chillerbot/warlist/war/%s", aFolder);
		str_format(aPeacePath, sizeof(aPeacePath), "chillerbot/warlist/neutral/%s", aFolder);
		Storage()->RenameFile(aPath, aPeacePath, IStorage::TYPE_SAVE);

		str_format(aBuf, sizeof(aBuf), "Moved folder %s from war/ to neutral/", aFolder);
		ReloadList();
		m_pClient->m_Chat.AddLine(-2, 0, aBuf);
	}
	else if(!str_comp(pCmd, "unfriend") || !str_comp(pCmd, "unteam") || !str_comp(pCmd, "delteam")) // "unfriend <folder>"
	{
		if(NumArgs < 1)
		{
			m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <folder>");
			return true;
		}
		char aFolder[512];
		str_copy(aFolder, ppArgs[0], sizeof(aFolder));
		char aPath[1024];
		char aNeutralPath[1024];
		str_format(aPath, sizeof(aPath), "chillerbot/warlist/team/%s/names.txt", aFolder);
		str_format(aNeutralPath, sizeof(aNeutralPath), "chillerbot/warlist/neutral/%s/names.txt", aFolder);
		IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
		if(!File)
		{
			str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aPath);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
			return true;
		}
		io_close(File);
		File = Storage()->OpenFile(aNeutralPath, IOFLAG_READ, IStorage::TYPE_SAVE);
		if(File)
		{
			str_format(aBuf, sizeof(aBuf), "Neutral entry already exists '%s'", aNeutralPath);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
			io_close(File);
			return true;
		}

		str_format(aPath, sizeof(aPath), "chillerbot/warlist/team/%s", aFolder);
		str_format(aNeutralPath, sizeof(aNeutralPath), "chillerbot/warlist/neutral/%s", aFolder);
		Storage()->RenameFile(aPath, aNeutralPath, IStorage::TYPE_SAVE);

		str_format(aBuf, sizeof(aBuf), "Moved folder %s from team/ to neutral/", aFolder);
		ReloadList();
		m_pClient->m_Chat.AddLine(-2, 0, aBuf);
	}
	else if(!str_comp(pCmd, "team")) // "team <folder>"
	{
		if(NumArgs < 1)
		{
			m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <folder>");
			return true;
		}
		char aFolder[512];
		str_copy(aFolder, ppArgs[0], sizeof(aFolder));
		char aPath[1024];
		char aTeamPath[1024];
		str_format(aPath, sizeof(aPath), "chillerbot/warlist/neutral/%s/names.txt", aFolder);
		str_format(aTeamPath, sizeof(aTeamPath), "chillerbot/warlist/team/%s/names.txt", aFolder);
		IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
		if(!File)
		{
			str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aPath);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
			return true;
		}
		io_close(File);
		File = Storage()->OpenFile(aTeamPath, IOFLAG_READ, IStorage::TYPE_SAVE);
		if(File)
		{
			str_format(aBuf, sizeof(aBuf), "Peace entry already exists '%s'", aTeamPath);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
			io_close(File);
			return true;
		}

		str_format(aPath, sizeof(aPath), "chillerbot/warlist/neutral/%s", aFolder);
		str_format(aTeamPath, sizeof(aTeamPath), "chillerbot/warlist/team/%s", aFolder);
		Storage()->RenameFile(aPath, aTeamPath, IStorage::TYPE_SAVE);

		str_format(aBuf, sizeof(aBuf), "Moved folder %s from neutral/ to team/", aFolder);
		ReloadList();
		m_pClient->m_Chat.AddLine(-2, 0, aBuf);
	}
	else if(!str_comp(pCmd, "war")) // "war <folder>"
	{
		if(NumArgs < 1)
		{
			m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <folder>");
			return true;
		}
		char aFolder[512];
		str_copy(aFolder, ppArgs[0], sizeof(aFolder));
		char aPath[1024];
		char aWarPath[1024];
		str_format(aPath, sizeof(aPath), "chillerbot/warlist/neutral/%s/names.txt", aFolder);
		str_format(aWarPath, sizeof(aWarPath), "chillerbot/warlist/war/%s/names.txt", aFolder);
		IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
		if(!File)
		{
			str_format(aBuf, sizeof(aBuf), "failed to open war list file '%s'", aPath);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
			return true;
		}
		io_close(File);
		File = Storage()->OpenFile(aWarPath, IOFLAG_READ, IStorage::TYPE_SAVE);
		if(File)
		{
			str_format(aBuf, sizeof(aBuf), "War entry already exists '%s'", aWarPath);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
			io_close(File);
			return true;
		}

		str_format(aPath, sizeof(aPath), "chillerbot/warlist/neutral/%s", aFolder);
		str_format(aWarPath, sizeof(aWarPath), "chillerbot/warlist/war/%s", aFolder);
		Storage()->RenameFile(aPath, aWarPath, IStorage::TYPE_SAVE);

		str_format(aBuf, sizeof(aBuf), "Moved folder %s from neutral/ to war/", aFolder);
		ReloadList();
		m_pClient->m_Chat.AddLine(-2, 0, aBuf);
	}
	else if(!str_comp(pCmd, "addreason")) // "addreason <folder> <reason can contain spaces>"
	{
		if(NumArgs < 1)
		{
			m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <folder>");
			return true;
		}
		if(NumArgs < 2)
		{
			m_pClient->m_Chat.AddLine(-2, 0, "Error: missing argument <reason>");
			return true;
		}
		char aFolder[512];
		char aReason[512];
		str_copy(aFolder, ppArgs[0], sizeof(aFolder));
		str_copy(aReason, ppArgs[1], sizeof(aReason));
		const char *pReason = aReason;
		bool Force = true;
		if(str_startswith(aReason, "-f "))
			pReason += str_length("-f ");
		else if(str_startswith(aReason, "--force "))
			pReason += str_length("--force ");
		else
			Force = false;
		char aFilename[1024];
		str_format(aFilename, sizeof(aFilename), "chillerbot/warlist/war/%s/reason.txt", aFolder);
		IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_READ, IStorage::TYPE_SAVE);
		if(File)
		{
			const char *pLine;
			CLineReader Reader;

			if(!Reader.OpenFile(File))
			{
				str_format(aBuf, sizeof(aBuf), "failed to open '%s'", aFilename);
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
				return true;
			}

			// read one line only
			pLine = Reader.Get();
			if(pLine && pLine[0] != '\0' && !Force)
			{
				str_format(aBuf, sizeof(aBuf), "Folder %s already has a reason. Use -f to overwrite reason: %s", aFolder, pLine);
				m_pClient->m_Chat.AddLine(-2, 0, aBuf);
				return true;
			}
		}
		File = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(!File)
		{
			str_format(aBuf, sizeof(aBuf), "Failed to open file for writing chillerbot/warlist/war/%s/reason.txt", aFolder);
			m_pClient->m_Chat.AddLine(-2, 0, aBuf);
			return true;
		}
		str_format(aBuf, sizeof(aBuf), "Adding reason to folder='%s' reason='%s'", aFolder, pReason);
		io_write(File, pReason, str_length(pReason));
		char aDate[32];
		str_timestamp(aDate, sizeof(aDate));
		str_format(aBuf, sizeof(aBuf), " (%s)", aDate);
		io_write(File, aBuf, str_length(aBuf));
		io_write_newline(File);
		io_close(File);
		ReloadList();
		m_pClient->m_Chat.AddLine(-1, 0, aBuf);
	}
	else
	{
		return false;
	}
	return true;
}


