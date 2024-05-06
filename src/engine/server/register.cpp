#include "register.h"

#include <base/lock.h>
#include <base/log.h>

#include <engine/console.h>
#include <engine/engine.h>
#include <engine/shared/config.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>
#include <engine/shared/masterserver.h>
#include <engine/shared/network.h>
#include <engine/shared/packer.h>
#include <engine/shared/uuid_manager.h>

class CRegister : public IRegister
{
	enum
	{
		STATUS_NONE = 0,
		STATUS_OK,
		STATUS_NEEDCHALLENGE,
		STATUS_NEEDINFO,
		STATUS_ERROR,

		PROTOCOL_TW6_IPV6 = 0,
		PROTOCOL_TW6_IPV4,
		PROTOCOL_TW7_IPV6,
		PROTOCOL_TW7_IPV4,
		NUM_PROTOCOLS,
	};

	static bool StatusFromString(int *pResult, const char *pString);
	static const char *ProtocolToScheme(int Protocol);
	static const char *ProtocolToString(int Protocol);
	static bool ProtocolFromString(int *pResult, const char *pString);
	static const char *ProtocolToSystem(int Protocol);
	static IPRESOLVE ProtocolToIpresolve(int Protocol);

	static void ConchainOnConfigChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	class CGlobal
	{
	public:
		CLock m_Lock;
		int m_InfoSerial GUARDED_BY(m_Lock) = -1;
		int m_LatestSuccessfulInfoSerial GUARDED_BY(m_Lock) = -1;
	};

	class CProtocol
	{
		class CShared
		{
		public:
			CShared(std::shared_ptr<CGlobal> pGlobal) :
				m_pGlobal(std::move(pGlobal))
			{
			}

			std::shared_ptr<CGlobal> m_pGlobal;
			CLock m_Lock;
			int m_NumTotalRequests GUARDED_BY(m_Lock) = 0;
			int m_LatestResponseStatus GUARDED_BY(m_Lock) = STATUS_NONE;
			int m_LatestResponseIndex GUARDED_BY(m_Lock) = -1;
		};

		class CJob : public IJob
		{
			int m_Protocol;
			int m_ServerPort;
			int m_Index;
			int m_InfoSerial;
			std::shared_ptr<CShared> m_pShared;
			std::shared_ptr<CHttpRequest> m_pRegister;
			IHttp *m_pHttp;
			void Run() override;

		public:
			CJob(int Protocol, int ServerPort, int Index, int InfoSerial, std::shared_ptr<CShared> pShared, std::shared_ptr<CHttpRequest> &&pRegister, IHttp *pHttp) :
				m_Protocol(Protocol),
				m_ServerPort(ServerPort),
				m_Index(Index),
				m_InfoSerial(InfoSerial),
				m_pShared(std::move(pShared)),
				m_pRegister(std::move(pRegister)),
				m_pHttp(pHttp)
			{
			}
			~CJob() override = default;
		};

		CRegister *m_pParent;
		int m_Protocol;

		std::shared_ptr<CShared> m_pShared;
		bool m_NewChallengeToken = false;
		bool m_HaveChallengeToken = false;
		char m_aChallengeToken[128] = {0};

		void CheckChallengeStatus();

	public:
		int64_t m_PrevRegister = -1;
		int64_t m_NextRegister = -1;

		CProtocol(CRegister *pParent, int Protocol);
		void OnToken(const char *pToken);
		void SendRegister();
		void SendDeleteIfRegistered(bool Shutdown);
		void Update();
	};

	CConfig *m_pConfig;
	IConsole *m_pConsole;
	IEngine *m_pEngine;
	IHttp *m_pHttp;

	// Don't start sending registers before the server has initialized
	// completely.
	bool m_GotFirstUpdateCall = false;
	int m_ServerPort;
	char m_aConnlessTokenHex[16];

	std::shared_ptr<CGlobal> m_pGlobal = std::make_shared<CGlobal>();
	bool m_aProtocolEnabled[NUM_PROTOCOLS] = {true, true, true, true};
	CProtocol m_aProtocols[NUM_PROTOCOLS];

	int m_NumExtraHeaders = 0;
	char m_aaExtraHeaders[8][128];

	char m_aVerifyPacketPrefix[sizeof(SERVERBROWSE_CHALLENGE) + UUID_MAXSTRSIZE];
	CUuid m_Secret = RandomUuid();
	CUuid m_ChallengeSecret = RandomUuid();
	bool m_GotServerInfo = false;
	char m_aServerInfo[16384];

public:
	CRegister(CConfig *pConfig, IConsole *pConsole, IEngine *pEngine, IHttp *pHttp, int ServerPort, unsigned SixupSecurityToken);
	void Update() override;
	void OnConfigChange() override;
	bool OnPacket(const CNetChunk *pPacket) override;
	void OnNewInfo(const char *pInfo) override;
	void OnShutdown() override;
};

bool CRegister::StatusFromString(int *pResult, const char *pString)
{
	if(str_comp(pString, "success") == 0)
	{
		*pResult = STATUS_OK;
	}
	else if(str_comp(pString, "need_challenge") == 0)
	{
		*pResult = STATUS_NEEDCHALLENGE;
	}
	else if(str_comp(pString, "need_info") == 0)
	{
		*pResult = STATUS_NEEDINFO;
	}
	else if(str_comp(pString, "error") == 0)
	{
		*pResult = STATUS_ERROR;
	}
	else
	{
		*pResult = -1;
		return true;
	}
	return false;
}

const char *CRegister::ProtocolToScheme(int Protocol)
{
	switch(Protocol)
	{
	case PROTOCOL_TW6_IPV6: return "tw-0.6+udp://";
	case PROTOCOL_TW6_IPV4: return "tw-0.6+udp://";
	case PROTOCOL_TW7_IPV6: return "tw-0.7+udp://";
	case PROTOCOL_TW7_IPV4: return "tw-0.7+udp://";
	}
	dbg_assert(false, "invalid protocol");
	dbg_break();
}

const char *CRegister::ProtocolToString(int Protocol)
{
	switch(Protocol)
	{
	case PROTOCOL_TW6_IPV6: return "tw0.6/ipv6";
	case PROTOCOL_TW6_IPV4: return "tw0.6/ipv4";
	case PROTOCOL_TW7_IPV6: return "tw0.7/ipv6";
	case PROTOCOL_TW7_IPV4: return "tw0.7/ipv4";
	}
	dbg_assert(false, "invalid protocol");
	dbg_break();
}

bool CRegister::ProtocolFromString(int *pResult, const char *pString)
{
	if(str_comp(pString, "tw0.6/ipv6") == 0)
	{
		*pResult = PROTOCOL_TW6_IPV6;
	}
	else if(str_comp(pString, "tw0.6/ipv4") == 0)
	{
		*pResult = PROTOCOL_TW6_IPV4;
	}
	else if(str_comp(pString, "tw0.7/ipv6") == 0)
	{
		*pResult = PROTOCOL_TW7_IPV6;
	}
	else if(str_comp(pString, "tw0.7/ipv4") == 0)
	{
		*pResult = PROTOCOL_TW7_IPV4;
	}
	else
	{
		*pResult = -1;
		return true;
	}
	return false;
}

const char *CRegister::ProtocolToSystem(int Protocol)
{
	switch(Protocol)
	{
	case PROTOCOL_TW6_IPV6: return "register/6/ipv6";
	case PROTOCOL_TW6_IPV4: return "register/6/ipv4";
	case PROTOCOL_TW7_IPV6: return "register/7/ipv6";
	case PROTOCOL_TW7_IPV4: return "register/7/ipv4";
	}
	dbg_assert(false, "invalid protocol");
	dbg_break();
}

IPRESOLVE CRegister::ProtocolToIpresolve(int Protocol)
{
	switch(Protocol)
	{
	case PROTOCOL_TW6_IPV6: return IPRESOLVE::V6;
	case PROTOCOL_TW6_IPV4: return IPRESOLVE::V4;
	case PROTOCOL_TW7_IPV6: return IPRESOLVE::V6;
	case PROTOCOL_TW7_IPV4: return IPRESOLVE::V4;
	}
	dbg_assert(false, "invalid protocol");
	dbg_break();
}

void CRegister::ConchainOnConfigChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		((CRegister *)pUserData)->OnConfigChange();
	}
}

void CRegister::CProtocol::SendRegister()
{
	int64_t Now = time_get();
	int64_t Freq = time_freq();

	char aAddress[64];
	str_format(aAddress, sizeof(aAddress), "%sconnecting-address.invalid:%d", ProtocolToScheme(m_Protocol), m_pParent->m_ServerPort);

	char aSecret[UUID_MAXSTRSIZE];
	FormatUuid(m_pParent->m_Secret, aSecret, sizeof(aSecret));

	char aChallengeUuid[UUID_MAXSTRSIZE];
	FormatUuid(m_pParent->m_ChallengeSecret, aChallengeUuid, sizeof(aChallengeUuid));
	char aChallengeSecret[64];
	str_format(aChallengeSecret, sizeof(aChallengeSecret), "%s:%s", aChallengeUuid, ProtocolToString(m_Protocol));
	int InfoSerial;
	bool SendInfo;

	{
		CLockScope ls(m_pShared->m_pGlobal->m_Lock);
		InfoSerial = m_pShared->m_pGlobal->m_InfoSerial;
		SendInfo = InfoSerial > m_pShared->m_pGlobal->m_LatestSuccessfulInfoSerial;
	}

	std::unique_ptr<CHttpRequest> pRegister;
	if(SendInfo)
	{
		pRegister = HttpPostJson(m_pParent->m_pConfig->m_SvRegisterUrl, m_pParent->m_aServerInfo);
	}
	else
	{
		pRegister = HttpPost(m_pParent->m_pConfig->m_SvRegisterUrl, (unsigned char *)"", 0);
	}
	pRegister->HeaderString("Address", aAddress);
	pRegister->HeaderString("Secret", aSecret);
	if(m_Protocol == PROTOCOL_TW7_IPV6 || m_Protocol == PROTOCOL_TW7_IPV4)
	{
		pRegister->HeaderString("Connless-Token", m_pParent->m_aConnlessTokenHex);
	}
	pRegister->HeaderString("Challenge-Secret", aChallengeSecret);
	if(m_HaveChallengeToken)
	{
		pRegister->HeaderString("Challenge-Token", m_aChallengeToken);
	}
	pRegister->HeaderInt("Info-Serial", InfoSerial);
	for(int i = 0; i < m_pParent->m_NumExtraHeaders; i++)
	{
		pRegister->Header(m_pParent->m_aaExtraHeaders[i]);
	}
	pRegister->LogProgress(HTTPLOG::FAILURE);
	pRegister->IpResolve(ProtocolToIpresolve(m_Protocol));
	pRegister->FailOnErrorStatus(false);

	int RequestIndex;
	{
		CLockScope ls(m_pShared->m_Lock);
		if(m_pShared->m_LatestResponseStatus != STATUS_OK)
		{
			log_info(ProtocolToSystem(m_Protocol), "registering...");
		}
		RequestIndex = m_pShared->m_NumTotalRequests;
		m_pShared->m_NumTotalRequests += 1;
	}
	m_pParent->m_pEngine->AddJob(std::make_shared<CJob>(m_Protocol, m_pParent->m_ServerPort, RequestIndex, InfoSerial, m_pShared, std::move(pRegister), m_pParent->m_pHttp));
	m_NewChallengeToken = false;

	m_PrevRegister = Now;
	m_NextRegister = Now + 15 * Freq;
}

void CRegister::CProtocol::SendDeleteIfRegistered(bool Shutdown)
{
	{
		const CLockScope LockScope(m_pShared->m_Lock);
		const bool ShouldSendDelete = m_pShared->m_LatestResponseStatus == STATUS_OK;
		m_pShared->m_LatestResponseStatus = STATUS_NONE;
		if(!ShouldSendDelete)
			return;
	}

	char aAddress[64];
	str_format(aAddress, sizeof(aAddress), "%sconnecting-address.invalid:%d", ProtocolToScheme(m_Protocol), m_pParent->m_ServerPort);

	char aSecret[UUID_MAXSTRSIZE];
	FormatUuid(m_pParent->m_Secret, aSecret, sizeof(aSecret));

	std::shared_ptr<CHttpRequest> pDelete = HttpPost(m_pParent->m_pConfig->m_SvRegisterUrl, (const unsigned char *)"", 0);
	pDelete->HeaderString("Action", "delete");
	pDelete->HeaderString("Address", aAddress);
	pDelete->HeaderString("Secret", aSecret);
	for(int i = 0; i < m_pParent->m_NumExtraHeaders; i++)
	{
		pDelete->Header(m_pParent->m_aaExtraHeaders[i]);
	}
	pDelete->LogProgress(HTTPLOG::FAILURE);
	pDelete->IpResolve(ProtocolToIpresolve(m_Protocol));
	if(Shutdown)
	{
		// On shutdown, wait at most 1 second for the delete requests.
		pDelete->Timeout(CTimeout{1000, 1000, 0, 0});
	}
	log_info(ProtocolToSystem(m_Protocol), "deleting...");
	m_pParent->m_pHttp->Run(pDelete);
}

CRegister::CProtocol::CProtocol(CRegister *pParent, int Protocol) :
	m_pParent(pParent),
	m_Protocol(Protocol),
	m_pShared(std::make_shared<CShared>(pParent->m_pGlobal))
{
}

void CRegister::CProtocol::CheckChallengeStatus()
{
	CLockScope ls(m_pShared->m_Lock);
	// No requests in flight?
	if(m_pShared->m_LatestResponseIndex == m_pShared->m_NumTotalRequests - 1)
	{
		switch(m_pShared->m_LatestResponseStatus)
		{
		case STATUS_NEEDCHALLENGE:
			if(m_NewChallengeToken)
			{
				// Immediately resend if we got the token.
				m_NextRegister = time_get();
			}
			break;
		case STATUS_NEEDINFO:
			// Act immediately if the master requests more info.
			m_NextRegister = time_get();
			break;
		}
	}
}

void CRegister::CProtocol::Update()
{
	CheckChallengeStatus();
	if(time_get() >= m_NextRegister)
	{
		SendRegister();
	}
}

void CRegister::CProtocol::OnToken(const char *pToken)
{
	m_NewChallengeToken = true;
	m_HaveChallengeToken = true;
	str_copy(m_aChallengeToken, pToken);

	CheckChallengeStatus();
	if(time_get() >= m_NextRegister)
	{
		SendRegister();
	}
}

void CRegister::CProtocol::CJob::Run()
{
	m_pHttp->Run(m_pRegister);
	m_pRegister->Wait();
	if(m_pRegister->State() != EHttpState::DONE)
	{
		// TODO: exponential backoff
		log_error(ProtocolToSystem(m_Protocol), "error sending request to master");
		return;
	}
	json_value *pJson = m_pRegister->ResultJson();
	if(!pJson)
	{
		log_error(ProtocolToSystem(m_Protocol), "non-JSON response from master");
		return;
	}
	const json_value &Json = *pJson;
	const json_value &StatusString = Json["status"];
	if(StatusString.type != json_string)
	{
		json_value_free(pJson);
		log_error(ProtocolToSystem(m_Protocol), "invalid JSON response from master");
		return;
	}
	int Status;
	if(StatusFromString(&Status, StatusString))
	{
		log_error(ProtocolToSystem(m_Protocol), "invalid status from master: %s", (const char *)StatusString);
		json_value_free(pJson);
		return;
	}
	if(Status == STATUS_ERROR)
	{
		const json_value &Message = Json["message"];
		if(Message.type != json_string)
		{
			json_value_free(pJson);
			log_error(ProtocolToSystem(m_Protocol), "invalid JSON error response from master");
			return;
		}
		log_error(ProtocolToSystem(m_Protocol), "error response from master: %d: %s", m_pRegister->StatusCode(), (const char *)Message);
		json_value_free(pJson);
		return;
	}
	if(m_pRegister->StatusCode() >= 400)
	{
		log_error(ProtocolToSystem(m_Protocol), "non-success status code %d from master without error code", m_pRegister->StatusCode());
		json_value_free(pJson);
		return;
	}
	{
		CLockScope ls(m_pShared->m_Lock);
		if(Status != STATUS_OK || Status != m_pShared->m_LatestResponseStatus)
		{
			log_debug(ProtocolToSystem(m_Protocol), "status: %s", (const char *)StatusString);
		}
		if(Status == m_pShared->m_LatestResponseStatus && Status == STATUS_NEEDCHALLENGE)
		{
			log_error(ProtocolToSystem(m_Protocol), "ERROR: the master server reports that clients can not connect to this server.");
			log_error(ProtocolToSystem(m_Protocol), "ERROR: configure your firewall/nat to let through udp on port %d.", m_ServerPort);
		}
		json_value_free(pJson);
		if(m_Index > m_pShared->m_LatestResponseIndex)
		{
			m_pShared->m_LatestResponseIndex = m_Index;
			m_pShared->m_LatestResponseStatus = Status;
		}
	}
	if(Status == STATUS_OK)
	{
		CLockScope ls(m_pShared->m_pGlobal->m_Lock);
		if(m_InfoSerial > m_pShared->m_pGlobal->m_LatestSuccessfulInfoSerial)
		{
			m_pShared->m_pGlobal->m_LatestSuccessfulInfoSerial = m_InfoSerial;
		}
	}
	else if(Status == STATUS_NEEDINFO)
	{
		CLockScope ls(m_pShared->m_pGlobal->m_Lock);
		if(m_InfoSerial == m_pShared->m_pGlobal->m_LatestSuccessfulInfoSerial)
		{
			// Tell other requests that they need to send the info again.
			m_pShared->m_pGlobal->m_LatestSuccessfulInfoSerial -= 1;
		}
	}
}

CRegister::CRegister(CConfig *pConfig, IConsole *pConsole, IEngine *pEngine, IHttp *pHttp, int ServerPort, unsigned SixupSecurityToken) :
	m_pConfig(pConfig),
	m_pConsole(pConsole),
	m_pEngine(pEngine),
	m_pHttp(pHttp),
	m_ServerPort(ServerPort),
	m_aProtocols{
		CProtocol(this, PROTOCOL_TW6_IPV6),
		CProtocol(this, PROTOCOL_TW6_IPV4),
		CProtocol(this, PROTOCOL_TW7_IPV6),
		CProtocol(this, PROTOCOL_TW7_IPV4),
	}
{
	const int HEADER_LEN = sizeof(SERVERBROWSE_CHALLENGE);
	mem_copy(m_aVerifyPacketPrefix, SERVERBROWSE_CHALLENGE, HEADER_LEN);
	FormatUuid(m_ChallengeSecret, m_aVerifyPacketPrefix + HEADER_LEN, sizeof(m_aVerifyPacketPrefix) - HEADER_LEN);
	m_aVerifyPacketPrefix[HEADER_LEN + UUID_MAXSTRSIZE - 1] = ':';

	// The DDNet code uses the `unsigned` security token in big-endian byte order.
	str_format(m_aConnlessTokenHex, sizeof(m_aConnlessTokenHex), "%08x", SixupSecurityToken);

	m_pConsole->Chain("sv_register", ConchainOnConfigChange, this);
	m_pConsole->Chain("sv_register_extra", ConchainOnConfigChange, this);
	m_pConsole->Chain("sv_register_url", ConchainOnConfigChange, this);
	m_pConsole->Chain("sv_sixup", ConchainOnConfigChange, this);
	m_pConsole->Chain("sv_ipv4only", ConchainOnConfigChange, this);
}

void CRegister::Update()
{
	if(!m_GotFirstUpdateCall)
	{
		bool Ipv6 = m_aProtocolEnabled[PROTOCOL_TW6_IPV6] || m_aProtocolEnabled[PROTOCOL_TW7_IPV6];
		bool Ipv4 = m_aProtocolEnabled[PROTOCOL_TW6_IPV4] || m_aProtocolEnabled[PROTOCOL_TW7_IPV4];
		if(Ipv6 && Ipv4)
		{
			dbg_assert(!HttpHasIpresolveBug(), "curl version < 7.77.0 does not support registering via both IPv4 and IPv6, set `sv_register ipv6` or `sv_register ipv4`");
		}
		m_GotFirstUpdateCall = true;
	}
	if(!m_GotServerInfo)
	{
		return;
	}
	for(int i = 0; i < NUM_PROTOCOLS; i++)
	{
		if(!m_aProtocolEnabled[i])
		{
			continue;
		}
		m_aProtocols[i].Update();
	}
}

void CRegister::OnConfigChange()
{
	bool aOldProtocolEnabled[NUM_PROTOCOLS];
	for(int i = 0; i < NUM_PROTOCOLS; i++)
	{
		aOldProtocolEnabled[i] = m_aProtocolEnabled[i];
	}
	const char *pProtocols = m_pConfig->m_SvRegister;
	if(str_comp(pProtocols, "1") == 0)
	{
		for(auto &Enabled : m_aProtocolEnabled)
		{
			Enabled = true;
		}
	}
	else if(str_comp(pProtocols, "0") == 0)
	{
		for(auto &Enabled : m_aProtocolEnabled)
		{
			Enabled = false;
		}
	}
	else
	{
		for(auto &Enabled : m_aProtocolEnabled)
		{
			Enabled = false;
		}
		char aBuf[16];
		while((pProtocols = str_next_token(pProtocols, ",", aBuf, sizeof(aBuf))))
		{
			int Protocol;
			if(str_comp(aBuf, "ipv6") == 0)
			{
				m_aProtocolEnabled[PROTOCOL_TW6_IPV6] = true;
				m_aProtocolEnabled[PROTOCOL_TW7_IPV6] = true;
			}
			else if(str_comp(aBuf, "ipv4") == 0)
			{
				m_aProtocolEnabled[PROTOCOL_TW6_IPV4] = true;
				m_aProtocolEnabled[PROTOCOL_TW7_IPV4] = true;
			}
			else if(str_comp(aBuf, "tw0.6") == 0)
			{
				m_aProtocolEnabled[PROTOCOL_TW6_IPV6] = true;
				m_aProtocolEnabled[PROTOCOL_TW6_IPV4] = true;
			}
			else if(str_comp(aBuf, "tw0.7") == 0)
			{
				m_aProtocolEnabled[PROTOCOL_TW7_IPV6] = true;
				m_aProtocolEnabled[PROTOCOL_TW7_IPV4] = true;
			}
			else if(!ProtocolFromString(&Protocol, aBuf))
			{
				m_aProtocolEnabled[Protocol] = true;
			}
			else
			{
				log_warn("register", "unknown protocol '%s'", aBuf);
				continue;
			}
		}
	}
	if(!m_pConfig->m_SvSixup)
	{
		m_aProtocolEnabled[PROTOCOL_TW7_IPV6] = false;
		m_aProtocolEnabled[PROTOCOL_TW7_IPV4] = false;
	}
	if(m_pConfig->m_SvIpv4Only)
	{
		m_aProtocolEnabled[PROTOCOL_TW6_IPV6] = false;
		m_aProtocolEnabled[PROTOCOL_TW7_IPV6] = false;
	}
	m_NumExtraHeaders = 0;
	const char *pRegisterExtra = m_pConfig->m_SvRegisterExtra;
	char aHeader[128];
	while((pRegisterExtra = str_next_token(pRegisterExtra, ",", aHeader, sizeof(aHeader))))
	{
		if(m_NumExtraHeaders == (int)std::size(m_aaExtraHeaders))
		{
			log_warn("register", "reached maximum of %d extra headers, dropping '%s' and all further headers", m_NumExtraHeaders, aHeader);
			break;
		}
		if(!str_find(aHeader, ": "))
		{
			log_warn("register", "header '%s' doesn't contain mandatory ': ', ignoring", aHeader);
			continue;
		}
		str_copy(m_aaExtraHeaders[m_NumExtraHeaders], aHeader);
		m_NumExtraHeaders += 1;
	}
	// Don't start registering before the first `CRegister::Update` call.
	if(!m_GotFirstUpdateCall)
	{
		return;
	}
	for(int i = 0; i < NUM_PROTOCOLS; i++)
	{
		if(aOldProtocolEnabled[i] == m_aProtocolEnabled[i])
		{
			continue;
		}
		if(m_aProtocolEnabled[i])
		{
			m_aProtocols[i].SendRegister();
		}
		else
		{
			m_aProtocols[i].SendDeleteIfRegistered(false);
		}
	}
}

bool CRegister::OnPacket(const CNetChunk *pPacket)
{
	if((pPacket->m_Flags & NETSENDFLAG_CONNLESS) == 0)
	{
		return false;
	}
	if(pPacket->m_DataSize >= (int)sizeof(m_aVerifyPacketPrefix) &&
		mem_comp(pPacket->m_pData, m_aVerifyPacketPrefix, sizeof(m_aVerifyPacketPrefix)) == 0)
	{
		CUnpacker Unpacker;
		Unpacker.Reset(pPacket->m_pData, pPacket->m_DataSize);
		Unpacker.GetRaw(sizeof(m_aVerifyPacketPrefix));
		const char *pProtocol = Unpacker.GetString(0);
		const char *pToken = Unpacker.GetString(0);
		if(Unpacker.Error())
		{
			log_error("register", "got erroneous challenge packet from master");
			return true;
		}

		log_debug("register", "got challenge token, protocol='%s' token='%s'", pProtocol, pToken);
		int Protocol;
		if(ProtocolFromString(&Protocol, pProtocol))
		{
			log_error("register", "got challenge packet with unknown protocol");
			return true;
		}
		m_aProtocols[Protocol].OnToken(pToken);
		return true;
	}
	return false;
}

void CRegister::OnNewInfo(const char *pInfo)
{
	log_trace("register", "info: %s", pInfo);
	if(m_GotServerInfo && str_comp(m_aServerInfo, pInfo) == 0)
	{
		return;
	}

	m_GotServerInfo = true;
	str_copy(m_aServerInfo, pInfo);
	{
		CLockScope ls(m_pGlobal->m_Lock);
		m_pGlobal->m_InfoSerial += 1;
	}

	// Don't start registering before the first `CRegister::Update` call.
	if(!m_GotFirstUpdateCall)
	{
		return;
	}

	// Immediately send new info if it changes, but at most once per second.
	int64_t Now = time_get();
	int64_t Freq = time_freq();
	int64_t MaximumPrevRegister = -1;
	int64_t MinimumNextRegister = -1;
	int MinimumNextRegisterProtocol = -1;
	for(int i = 0; i < NUM_PROTOCOLS; i++)
	{
		if(!m_aProtocolEnabled[i])
		{
			continue;
		}
		if(m_aProtocols[i].m_NextRegister == -1)
		{
			m_aProtocols[i].m_NextRegister = Now;
			continue;
		}
		if(m_aProtocols[i].m_PrevRegister > MaximumPrevRegister)
		{
			MaximumPrevRegister = m_aProtocols[i].m_PrevRegister;
		}
		if(MinimumNextRegisterProtocol == -1 || m_aProtocols[i].m_NextRegister < MinimumNextRegister)
		{
			MinimumNextRegisterProtocol = i;
			MinimumNextRegister = m_aProtocols[i].m_NextRegister;
		}
	}
	for(int i = 0; i < NUM_PROTOCOLS; i++)
	{
		if(!m_aProtocolEnabled[i])
		{
			continue;
		}
		if(i == MinimumNextRegisterProtocol)
		{
			m_aProtocols[i].m_NextRegister = std::min(m_aProtocols[i].m_NextRegister, MaximumPrevRegister + Freq);
		}
		if(Now >= m_aProtocols[i].m_NextRegister)
		{
			m_aProtocols[i].SendRegister();
		}
	}
}

void CRegister::OnShutdown()
{
	for(int i = 0; i < NUM_PROTOCOLS; i++)
	{
		if(!m_aProtocolEnabled[i])
		{
			continue;
		}
		m_aProtocols[i].SendDeleteIfRegistered(true);
	}
}

IRegister *CreateRegister(CConfig *pConfig, IConsole *pConsole, IEngine *pEngine, IHttp *pHttp, int ServerPort, unsigned SixupSecurityToken)
{
	return new CRegister(pConfig, pConsole, pEngine, pHttp, ServerPort, SixupSecurityToken);
}
