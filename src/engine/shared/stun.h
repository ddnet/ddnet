#ifndef ENGINE_SHARED_STUN_H
#define ENGINE_SHARED_STUN_H
#include <cstddef>

struct NETADDR;

class CStunData
{
public:
	unsigned char m_aSecret[12];
};

size_t StunMessagePrepare(unsigned char *pBuffer, size_t BufferSize, CStunData *pData);
bool StunMessageParse(const unsigned char *pMessage, size_t MessageSize, const CStunData *pData, bool *pSuccess, NETADDR *pAddr);
#endif // ENGINE_SHARED_STUN_H
