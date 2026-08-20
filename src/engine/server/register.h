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
	// Returns `true` if the packet was a packet related to registering
	// code and doesn't have to processed furtherly.
	virtual bool OnPacket(const CNetChunk *pPacket) = 0;
	// `pInfo` must be an encoded JSON object.
	virtual void OnNewInfo(const char *pInfo) = 0;
	virtual void OnShutdown() = 0;
};

// `NetTypes` are the `NETTYPE_*` flags of the network types the server's
// listen socket is actually bound to, together with `NETTYPE_WEBSOCKET_TLS`
// if websockets are served with TLS (wss). They determine which websocket
// protocols are registered and under which scheme.
IRegister *CreateRegister(CConfig *pConfig, IConsole *pConsole, IEngine *pEngine, IHttp *pHttp, int NetTypes, int ServerPort, unsigned SixupSecurityToken);

#endif
