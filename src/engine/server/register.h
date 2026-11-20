#ifndef ENGINE_SERVER_REGISTER_H
#define ENGINE_SERVER_REGISTER_H

class CConfig;
class IConsole;
class IEngine;
class IHttp;
struct CNetChunk;

class IRegister
{
public:
	virtual ~IRegister() = default;

	virtual void Update() = 0;
	// Call `OnConfigChange` if you change relevant config variables
	// without going through the console.
	virtual void OnConfigChange() = 0;
	// Set the port discovered via STUN for the given net type (`NETTYPE_IPV4`
	// or `NETTYPE_IPV6`). It is only used as a fallback, replacing the port we
	// are registered with if the master server can not reach the latter.
	virtual void SetStunPort(int NetType, int Port) = 0;
	// Returns whether at least one protocol with the given net type
	// (`NETTYPE_IPV4` or `NETTYPE_IPV6`) is enabled for registration.
	virtual bool IsNetTypeEnabled(int NetType) const = 0;
	// Returns whether the STUN port discovery should keep running, i.e. whether
	// any enabled protocol is, or is about to be, registered with a port
	// discovered via STUN.
	virtual bool NeedsStunPort() const = 0;
	// Returns `true` if the packet was a packet related to registering
	// code and doesn't have to processed furtherly.
	virtual bool OnPacket(const CNetChunk *pPacket) = 0;
	// `pInfo` must be an encoded JSON object.
	virtual void OnNewInfo(const char *pInfo) = 0;
	virtual void OnShutdown() = 0;
};

IRegister *CreateRegister(CConfig *pConfig, IConsole *pConsole, IEngine *pEngine, IHttp *pHttp, int ServerPort, unsigned SixupSecurityToken);

#endif
