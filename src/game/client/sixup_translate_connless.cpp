#include <base/mem.h>

#include <engine/shared/masterserver.h>

#include <game/client/gameclient.h>

void CClient::PreprocessConnlessPacket7(CNetChunk *pPacket)
{
	if(mem_comp(pPacket->m_pData, SERVERBROWSE_INFO, sizeof(SERVERBROWSE_INFO)) == 0)
	{
		CUnpacker Up;
		Up.Reset((unsigned char *)pPacket->m_pData + sizeof(SERVERBROWSE_INFO), pPacket->m_DataSize - sizeof(SERVERBROWSE_INFO));
		CServerInfo Info = {};

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

		Info.m_vClients.resize(std::clamp(Info.m_NumClients, 0, (int)SERVERINFO_MAX_CLIENTS));
		for(auto &Client : Info.m_vClients)
		{
			GetString(Client.m_aName);
			GetString(Client.m_aClan);
			Client.m_Country = Up.GetInt();
			if(!in_range(Client.m_Country, CountryCode::MINIMUM, CountryCode::MAXIMUM))
			{
				Client.m_Country = CountryCode::DEFAULT;
			}
			Client.m_Score = Up.GetInt();
			Client.m_Player = !(Up.GetInt() & 1);
		}

		const bool IsNotVanilla = Info.m_MaxPlayers > VANILLA_MAX_CLIENTS || Info.m_MaxClients > VANILLA_MAX_CLIENTS;
		CPacker Packer;
		Packer.Reset();

		auto PutInt = [&Packer](int Num) {
			char aBuf[16];
			str_format(aBuf, sizeof(aBuf), "%d", Num);
			Packer.AddString(aBuf);
		};

		if(!IsNotVanilla)
		{
			Token = Token & 0xff;
		}
		PutInt(Token);
		Packer.AddString(Info.m_aVersion);
		Packer.AddString(Info.m_aName);
		Packer.AddString(Info.m_aMap);

		if(IsNotVanilla)
		{
			PutInt(0); // map crc
			PutInt(0); // map size
		}

		Packer.AddString(Info.m_aGameType);

		PutInt(Info.m_Flags);
		PutInt(Info.m_NumPlayers);
		PutInt(Info.m_MaxPlayers);
		PutInt(Info.m_NumClients);
		PutInt(Info.m_MaxClients);

		if(IsNotVanilla)
		{
			Packer.AddString(""); // extra info, reserved
		}

		for(const auto &Client : Info.m_vClients)
		{
			Packer.AddString(Client.m_aName);
			Packer.AddString(Client.m_aClan);

			PutInt(Client.m_Country);
			PutInt(Client.m_Score);
			PutInt(Client.m_Player);

			if(IsNotVanilla)
			{
				Packer.AddString(""); // extra info, reserved
			}
		}

		if(Packer.Error() || SERVERBROWSE_SIZE + Packer.Size() > NET_MAX_PAYLOAD)
		{
			pPacket->m_DataSize = 0;
			return;
		}

		if(IsNotVanilla)
		{
			mem_copy((unsigned char *)pPacket->m_pData, SERVERBROWSE_INFO_EXTENDED, sizeof(SERVERBROWSE_INFO_EXTENDED));
		}
		mem_copy((unsigned char *)pPacket->m_pData + SERVERBROWSE_SIZE, Packer.Data(), Packer.Size());
		pPacket->m_DataSize = SERVERBROWSE_SIZE + Packer.Size();
	}
}
