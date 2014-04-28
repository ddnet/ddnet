#include <base/math.h>
#include <base/system.h>
#include <game/version.h>

#if defined(CONF_FAMILY_UNIX)
	#include <sys/time.h>
	#include <unistd.h>

	/* unix net includes */
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/ioctl.h>
	#include <errno.h>
	#include <netdb.h>
	#include <netinet/in.h>
	#include <fcntl.h>
	#include <pthread.h>
	#include <arpa/inet.h>

	#include <dirent.h>

#elif defined(CONF_FAMILY_WINDOWS)
	#define WIN32_LEAN_AND_MEAN
	#define _WIN32_WINNT 0x0501 /* required for mingw to get getaddrinfo to work */
	#include <windows.h>
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <Shellapi.h>
	#include <shlobj.h>
	#include <fcntl.h>
	#include <direct.h>
	#include <errno.h>
#else
	#error NOT IMPLEMENTED
#endif
#include <string.h>
#include <stdio.h> //remove

#include <engine/shared/config.h>

#include "autoupdate.h"

static NETSOCKET invalid_socket = {NETTYPE_INVALID, -1, -1};

CAutoUpdate::CAutoUpdate()
{
	Reset();
}

void CAutoUpdate::Reset()
{
	m_NeedUpdate = false;
	m_NeedUpdateBackground = false;
	m_NeedUpdateClient = false;
	m_NeedResetClient = false;
	m_Updated = false;
	m_vFiles.clear();
}

bool CAutoUpdate::CanUpdate(const char *pFile)
{
	std::list<std::string>::iterator it = m_vFiles.begin();
	while (it != m_vFiles.end())
	{
		if ((*it).compare(pFile) == 0)
			return false;

		it++;
	}

	return true;
}

void CAutoUpdate::ExecuteExit()
{
	if (!m_NeedResetClient)
		return

	dbg_msg("autoupdate", "Executing pre-quiting...");

	if (m_NeedUpdateClient)
	{
		SelfDelete();
		#if defined(CONF_FAMILY_WINDOWS)
			ShellExecuteA(0,0,"du.bat",0,0,SW_HIDE);
		#else
			if (rename("DDNet_tmp","DDNet"))
				dbg_msg("autoupdate", "Error renaming binary file");
			if (system("chmod +x DDNet"))
				dbg_msg("autoupdate", "Error setting executable bit");
		#endif
	}

	#if defined(CONF_FAMILY_WINDOWS)
	if (!m_NeedUpdateClient)
		ShellExecuteA(0,0,"DDNet.exe",0,0,SW_SHOW);
	#else
		pid_t pid;
		pid=fork();
		if (pid==0)
		{
			char* argv[1];
			argv[0] = NULL;
			execv("DDNet", argv);
		}
		else
			return;
	#endif
}

void CAutoUpdate::CheckUpdates(CMenus *pMenus)
{
	char aReadBuf[512];
	char aBuf[512];

	dbg_msg("autoupdate", "Checking for updates");
	if (!GetFile("ddnet.upd", "ddnet.upd"))
	{
		dbg_msg("autoupdate", "Error downloading update list");
		return;
	}

	dbg_msg("autoupdate", "Processing data");

	Reset();
	IOHANDLE updFile = io_open("ddnet.upd", IOFLAG_READ);
	if (!updFile)
		return;

	//read data
	std::string ReadData;
	char last_version[15], current_version[15];
	char cmd;
	while (io_read(updFile, aReadBuf, sizeof(aReadBuf)) > 0)
	{
		for (size_t i=0; i<sizeof(aReadBuf); i++)
		{
			if (aReadBuf[i]=='\n')
			{
				if (i>0 && aReadBuf[i-1] == '\r')
					ReadData = ReadData.substr(0, -2);

				//Parse Command
				cmd = ReadData[0];
				if (cmd == '#')
				{
					if (!m_NeedUpdate)
						str_copy(last_version, ReadData.substr(1).c_str(), sizeof(last_version));
					if (ReadData.substr(1).compare(GAME_RELEASE_VERSION) != 0)
						m_NeedUpdate = true;
					else
					{
						dbg_msg("autoupdate", "Version match");
						break;
					}

					str_copy(current_version, ReadData.substr(1).c_str(), sizeof(current_version));
				}

				if (m_NeedUpdate)
				{
					if (cmd == '@')
					{
						if (!m_NeedUpdateClient && ReadData.substr(2).compare("UPDATE_CLIENT") == 0)
						{
							str_format(aBuf, sizeof(aBuf), "Updating DDNet Client to %s", last_version);
							pMenus->RenderUpdating(aBuf);

							m_NeedUpdateClient = true;
							m_NeedResetClient = true;
							dbg_msg("autoupdate", "Updating client");
							#if defined(CONF_FAMILY_WINDOWS)
							if (!GetFile("DDNet.exe", "DDNet_tmp.exe"))
							#elif defined(CONF_ARCH_AMD64)
							if (!GetFile("DDNet64", "DDNet_tmp"))
							#else
							if (!GetFile("DDNet", "DDNet_tmp"))
							#endif
								dbg_msg("autoupdate", "Error downloading new version");
						}
						else if (!m_NeedUpdateClient && ReadData.substr(2).compare("RESET_CLIENT") == 0)
							m_NeedResetClient = true;
					}
					else if (cmd == 'D')
					{
						int posDel=0;
						ReadData = ReadData.substr(2);
						posDel = ReadData.find_first_of(":");

						if (CanUpdate(ReadData.substr(posDel+1).c_str()))
						{
							str_format(aBuf, sizeof(aBuf), "Downloading '%s'", ReadData.substr(posDel+1).c_str());
							pMenus->RenderUpdating(aBuf);

							dbg_msg("autoupdate", "Updating file '%s'", ReadData.substr(posDel+1).c_str());
							if (!GetFile(ReadData.substr(0, posDel).c_str(), ReadData.substr(posDel+1).c_str()))
								dbg_msg("autoupdate", "Error downloading '%s'", ReadData.substr(0, posDel).c_str());
							else
								m_vFiles.push_back(ReadData.substr(posDel+1));
						}
					}
					else if (cmd == 'R')
					{
						if (ReadData.substr(2).c_str()[0] == 0)
							return;

						if (CanUpdate(ReadData.substr(2).c_str()))
						{
							str_format(aBuf, sizeof(aBuf), "Deleting '%s'", ReadData.substr(2).c_str());
							pMenus->RenderUpdating(aBuf);

							dbg_msg("autoupdate", "Deleting file '%s'", ReadData.substr(2).c_str());
							remove(ReadData.substr(2).c_str());

							m_vFiles.push_back(ReadData.substr(2));
						}
					}
				}

				ReadData.clear();
				continue;
			}

			ReadData+=aReadBuf[i];
		}

		if (!m_NeedUpdate)
			break;
	}
	
	if (m_NeedUpdate)
	{
		m_Updated = true;
		m_NeedUpdate = false;

		if (!m_NeedUpdateClient)
			dbg_msg("autoupdate", "Updated successfully");
		else
			dbg_msg("autoupdate", "Restart necessary");
	}
	else
		dbg_msg("autoupdate", "No updates available");

	io_close(updFile);
	remove("ddnet.upd");
}

bool CAutoUpdate::GetFile(const char *pFile, const char *dst)
{
	NETSOCKET Socket = invalid_socket;
	NETADDR HostAddress;
	char aNetBuff[1024];

	//Lookup
	if(net_host_lookup(g_Config.m_ClDDNetUpdateServer, &HostAddress, NETTYPE_IPV4) != 0)
	{
		dbg_msg("autoupdate", "Error running host lookup");
		return false;
	}

	char aAddrStr[NETADDR_MAXSTRSIZE];
	net_addr_str(&HostAddress, aAddrStr, sizeof(aAddrStr), 80);

	//Connect
	int socketID = socket(AF_INET, SOCK_STREAM, 0);
	if(socketID < 0)
	{
		dbg_msg("autoupdate", "Error creating socket");
		return false;
	}

	Socket.type = NETTYPE_IPV4;
	Socket.ipv4sock = socketID;
	HostAddress.port = 80;

	if(net_tcp_connect(Socket, &HostAddress) != 0)
	{
		net_tcp_close(Socket);
		dbg_msg("autoupdate","Error connecting to host");
		return false;
	}

	//Send request
	str_format(aNetBuff, sizeof(aNetBuff), "GET /%s HTTP/1.0\nHOST: %s\n\n", pFile, g_Config.m_ClDDNetUpdateServer);
	net_tcp_send(Socket, aNetBuff, strlen(aNetBuff));

	//read data
	IOHANDLE dstFile = io_open(dst, IOFLAG_WRITE);
	if (!dstFile)
	{
		net_tcp_close(Socket);
		dbg_msg("autoupdate","Error writing to disk");
		return false;
	}

	std::string NetData;
	int TotalRecv = 0;
	int TotalBytes = 0;
	int CurrentRecv = 0;
	bool isHead = true;
	int enterCtrl = 0;
	while ((CurrentRecv = net_tcp_recv(Socket, aNetBuff, sizeof(aNetBuff))) > 0)
	{
		for (int i=0; i<CurrentRecv ; i++)
		{
			if (isHead)
			{
				if (aNetBuff[i]=='\n')
				{
					enterCtrl++;
					if (isHead && enterCtrl == 2)
					{
						isHead = false;
						NetData.clear();
						continue;
					}

					int posDel = NetData.find_first_of(":");
					if (posDel > 0 && str_comp_nocase(NetData.substr(0, posDel).c_str(),"content-length") == 0)
						TotalBytes = atoi(NetData.substr(posDel+2).c_str());

					NetData.clear();
					continue;
				}
				else if (aNetBuff[i]!='\r')
					enterCtrl=0;

				NetData+=aNetBuff[i];
			}
			else
			{
				if (TotalBytes == 0)
				{
					io_close(dstFile);
					net_tcp_close(Socket);
					dbg_msg("autoupdate","Error receiving file");
					return false;
				}

				io_write(dstFile, &aNetBuff[i], 1);

				TotalRecv++;
				if (TotalRecv == TotalBytes)
					break;
			}
		}
	}

	//Finish
	io_close(dstFile);
	net_tcp_close(Socket);

	return true;
}

bool CAutoUpdate::SelfDelete()
{
	#ifdef CONF_FAMILY_WINDOWS
	IOHANDLE bhFile = io_open("du.bat", IOFLAG_WRITE);
	if (!bhFile)
		return false;

	char aFileData[512];
	str_format(aFileData, sizeof(aFileData), ":_R\r\ndel \"DDNet.exe\"\r\nif exist \"DDNet.exe\" goto _R\r\nrename \"DDNet_tmp.exe\" \"DDNet.exe\"\r\n:_T\r\nif not exist \"DDNet.exe\" goto _T\r\nstart DDNet.exe\r\ndel \"du.bat\"\r\n");
	io_write(bhFile, aFileData, str_length(aFileData));
	io_close(bhFile);
	#else
		remove("DDNet");
		return true;
	#endif

	return false;
}
