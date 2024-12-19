#include <engine/shared/masterserver.h>
#include <game/client/gameclient.h>

void CClient::PreprocessConnlessPacket7(CNetChunk *pPacket)
{
	if(mem_comp(pPacket->m_pData, SERVERBROWSE_INFO, sizeof(SERVERBROWSE_INFO)) == 0)
	{
		CUnpacker Up;
		Up.Reset((unsigned char *)pPacket->m_pData + sizeof(SERVERBROWSE_INFO), pPacket->m_DataSize - sizeof(SERVERBROWSE_INFO));
		CServerInfo Info;
		mem_zero(&Info, sizeof(CServerInfo));

		auto GetString = [&Up](auto &Buf) {
			str_copy(Buf, Up.GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES));
		};

		TOKEN Token = Up.GetInt();
		GetString(Info.m_aVersion);
		GetString(Info.m_aName);
		str_clean_whitespaces(Info.m_aName);

		Up.GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES); // Hostname
		GetString(Info.m_aMap);
		GetString(Info.m_aGameType);
		Info.m_Flags = Up.GetInt();
		Up.GetInt(); // Server level
		Info.m_NumPlayers = Up.GetInt();
		Info.m_MaxPlayers = Up.GetInt();
		Info.m_NumClients = Up.GetInt();
		Info.m_MaxClients = Up.GetInt();

		for(int i = 0; i < Info.m_NumClients; i++)
		{
			GetString(Info.m_aClients[i].m_aName);
			GetString(Info.m_aClients[i].m_aClan);
			Info.m_aClients[i].m_Country = Up.GetInt();
			Info.m_aClients[i].m_Score = Up.GetInt();
			Info.m_aClients[i].m_Player = !(Up.GetInt() & 1);
		}

		CPacker Packer;
		Packer.Reset();

		auto PutInt = [&Packer](int Num) {
			char aBuf[16];
			str_format(aBuf, sizeof(aBuf), "%d", Num);
			Packer.AddString(aBuf);
		};

		PutInt(Token & 0xff);
		Packer.AddString(Info.m_aVersion);
		Packer.AddString(Info.m_aName);
		Packer.AddString(Info.m_aMap);
		Packer.AddString(Info.m_aGameType);

		PutInt(Info.m_Flags);
		PutInt(Info.m_NumPlayers);
		PutInt(Info.m_MaxPlayers);
		PutInt(Info.m_NumClients);
		PutInt(Info.m_MaxClients);

		for(int i = 0; i < Info.m_NumClients; i++)
		{
			Packer.AddString(Info.m_aClients[i].m_aName);
			Packer.AddString(Info.m_aClients[i].m_aClan);

			PutInt(Info.m_aClients[i].m_Country);
			PutInt(Info.m_aClients[i].m_Score);
			PutInt(Info.m_aClients[i].m_Player);
		}

		mem_copy((unsigned char *)pPacket->m_pData + sizeof(SERVERBROWSE_INFO), Packer.Data(), Packer.Size());
		pPacket->m_DataSize = sizeof(SERVERBROWSE_INFO) + Packer.Size();
	}
}
